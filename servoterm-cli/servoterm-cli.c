/*
 * This file is part of the stmbl project.
 *
 * servoterm-cli: a headless command-line client for a real stmbl drive,
 * modelled on QtServoterm (https://github.com/STMBL/QtServoterm) but with no
 * GUI. It is derived from host/servoterm.c, which drives the x86 simulator;
 * this variant opens the drive's USB CDC serial port (e.g. /dev/ttyACM0)
 * instead of spawning stmbl_host.
 *
 * Wire protocol (USB serial link, see shared/comps/term.c):
 *   host -> drive : command lines terminated with '\n'
 *   drive -> host : text responses, interleaved with scope frames:
 *                     0xFF, ch0, ch1, ... ch7   (each channel byte in 1..254)
 *                     0xFE                       = scope reset
 *                   channel value = (byte - 128) / 128.0
 *
 * Usage:
 *   servoterm-cli [-d DEV] [-s FILE] [-t SEC] [-w MS]
 *                 [--dump FILE] [--load FILE [--no-backup]] [-c CMD]... [SCRIPT]
 *     -d DEV    serial device (default /dev/ttyACM0)
 *     -s FILE   write decoded scope samples to FILE as CSV
 *     -t SEC    after input is exhausted, keep reading until the drive has
 *               been quiet (no text) for SEC seconds, then exit (default 0.5)
 *     -w MS     delay between sent command lines in ms (default 50)
 *     -c CMD    send CMD (may be repeated); takes precedence over SCRIPT/stdin
 *     --dump FILE  save the drive's flash config to FILE ('-' = stdout)
 *     --load FILE  write FILE to the drive's flash config and verify it; the
 *                  previous config is saved to conf-backup-<time>.txt first
 *                  unless --no-backup. Runs before --dump and -c.
 *     --capture FILE  the drive's capture scope (scope0, shared/comps/scope.c):
 *                  after the -c commands (which set it up), arm it (unless
 *                  --no-arm), wait for the capture (--capture-timeout SEC,
 *                  default 60), download it (scopedump: CRC per line), write
 *                  CSV: t (0 at the trigger) and a column per channel
 *     --update IMAGE  in-band firmware update (shared/comps/fwup.c, the drive
 *                  disabled): stage the application image (.bin) in the
 *                  drive's flash, line by line with a CRC each (resumes an
 *                  interrupted transfer), check its CRC32, then fwapply: the
 *                  drive resets and its bootloader copies the image. Over
 *                  USB in seconds; over the smart-serial console (sserialterm)
 *                  at ~1 kB/s. --no-apply stops before fwapply.
 *     --wizard  the setup wizard (setup/, docs/setup_wizard.md): sets up the
 *               drive and its motor step by step. --session FILE keeps
 *               its progress (default stmbl-setup-session.txt; run again
 *               to resume), --answers FILE answers from a file, --redo
 *               STEP repeats a step and what follows, --wizard-step STEP
 *               runs one step, --out DIR for backups / config / report,
 *               --time-scale N for the simulator's --speed N, -v shows
 *               the drive's text
 *     SCRIPT    file of commands to send; if omitted (and no --dump/--load),
 *               reads stdin
 *
 * Lines starting with '#' and empty lines in SCRIPT/stdin are not sent.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <math.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "setup/su_cli_ui.h"
#include "setup/su_steps.h"
#include "setup_api.h"

#ifndef SETUP_GIT
#define SETUP_GIT ""
#endif

#define SCOPE_CHANNELS 8
#define SCOPE_MARKER 0xFF
#define SCOPE_RESET 0xFE
#define MAX_CMDS 64

static FILE *scope_fp   = NULL;
static long scope_frame = 0;
static int frame_active = 0;
static int frame_have   = 0;
static int discard_text = 0;  // drop text while syncing to the stream

// When capturing, text goes into cap_buf instead of stdout.
static int capturing  = 0;
static char *cap_buf  = NULL;
static size_t cap_len = 0;
static size_t cap_size = 0;
static unsigned char frame[SCOPE_CHANNELS];

// Scope channel setup from -C: name and gain per channel. The drive sends
// byte = (value + offset) * gain + 128, clamped to 1..254 (term.c), so gain is
// counts per unit and full scale is +-127/gain. With a gain set, samples are
// written in the pin's own units: (byte - 128) / gain.
static const char *scope_name[SCOPE_CHANNELS];
static double scope_gain[SCOPE_CHANNELS];
static double scope_dt = 0.0;  // s per frame (from --step), adds a "t" column

static volatile sig_atomic_t stop_req = 0;
static void on_signal(int sig) {
  (void)sig;
  stop_req = 1;
}

// --log mode: prefix each line of drive text with the host time
static int stamp_text     = 0;
static int at_line_start  = 1;

static void scope_emit(void) {
  if(scope_fp) {
    fprintf(scope_fp, "%ld", scope_frame);
    if(scope_dt > 0.0) {
      fprintf(scope_fp, ",%.4f", scope_frame * scope_dt);
    }
    for(int i = 0; i < SCOPE_CHANNELS; i++) {
      if(scope_gain[i] != 0.0) {
        fprintf(scope_fp, ",%.6g", (frame[i] - 128) / scope_gain[i]);
      } else {
        fprintf(scope_fp, ",%.5f", (frame[i] - 128) / 128.0);
      }
    }
    fprintf(scope_fp, "\n");
  }
  scope_frame++;
}

static void print_stamp(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  struct tm tm;
  localtime_r(&tv.tv_sec, &tm);
  printf("[%02d:%02d:%02d.%03ld] ", tm.tm_hour, tm.tm_min, tm.tm_sec, (long)(tv.tv_usec / 1000));
}

// Split the drive's byte stream into text (printed) and scope frames (to CSV).
// Returns the number of text bytes seen, so the caller can detect quiet time.
static int demux(const unsigned char *buf, int n) {
  int text = 0;
  for(int i = 0; i < n; i++) {
    unsigned char b = buf[i];
    if(frame_active) {
      frame[frame_have++] = b;
      if(frame_have == SCOPE_CHANNELS) {
        frame_active = 0;
        frame_have   = 0;
        scope_emit();
      }
    } else if(b == SCOPE_MARKER) {
      frame_active = 1;
      frame_have   = 0;
    } else if(b == SCOPE_RESET) {
      if(scope_fp) {
        fprintf(scope_fp, "# reset\n");
      }
    } else if(!discard_text) {
      if(capturing) {
        if(cap_len + 2 > cap_size) {
          cap_size = cap_size ? cap_size * 2 : 4096;
          cap_buf  = realloc(cap_buf, cap_size);
          if(!cap_buf) {
            fprintf(stderr, "servoterm-cli: out of memory\n");
            exit(1);
          }
        }
        cap_buf[cap_len++] = b;
        cap_buf[cap_len]   = '\0';
      } else {
        if(stamp_text && at_line_start && b != '\n' && b != '\r') {
          print_stamp();
          at_line_start = 0;
        }
        putchar(b);
        if(b == '\n') {
          at_line_start = 1;
        }
      }
      text++;
    }
  }
  fflush(stdout);
  return text;
}

static double now(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + tv.tv_usec / 1e6;
}

static int open_port(const char *dev) {
  int fd = open(dev, O_RDWR | O_NOCTTY);
  if(fd < 0) {
    fprintf(stderr, "servoterm-cli: cannot open %s: %s\n", dev, strerror(errno));
    return -1;
  }
  struct termios tio;
  if(tcgetattr(fd, &tio) < 0) {
    perror("tcgetattr");
    close(fd);
    return -1;
  }
  // Raw 8-bit: scope bytes 0xFE/0xFF and CR/LF must pass through untouched.
  // The baud rate is irrelevant for USB CDC.
  cfmakeraw(&tio);
  tio.c_cflag |= CLOCAL | CREAD;
  tio.c_cc[VMIN]  = 0;
  tio.c_cc[VTIME] = 0;
  cfsetispeed(&tio, B115200);
  cfsetospeed(&tio, B115200);
  if(tcsetattr(fd, TCSANOW, &tio) < 0) {
    perror("tcsetattr");
    close(fd);
    return -1;
  }
  tcflush(fd, TCIOFLUSH);
  return fd;
}

static int write_all(int fd, const char *buf, size_t n) {
  size_t off = 0;
  while(off < n) {
    ssize_t w = write(fd, buf + off, n - off);
    if(w < 0) {
      if(errno == EINTR || errno == EAGAIN) {
        continue;
      }
      return -1;
    }
    off += w;
  }
  return 0;
}

// Send one command line as a single write, so it reaches the drive in one
// USB packet. The firmware's input ring buffer (shared/ringbuf.c) is written
// from the USB interrupt and read from the main loop without locking; when a
// line arrives split across packets, the main loop reads the partial line and
// rewinds it (rb_undo) while the interrupt may be adding the rest, and bytes
// get lost. Lines over 63 chars would also jam the drive's 64-byte line
// buffer, so they are refused.
#define MAX_CMD_LINE 63
static int write_line(int fd, const char *text, size_t len) {
  char buf[MAX_CMD_LINE + 1];
  if(len > MAX_CMD_LINE) {
    fprintf(stderr, "servoterm-cli: command too long (%zu > %d chars), not sent: %.*s\n", len,
            MAX_CMD_LINE, (int)len, text);
    return -2;
  }
  memcpy(buf, text, len);
  buf[len] = '\n';
  if(write_all(fd, buf, len + 1) < 0) {
    perror("servoterm-cli: write");
    return -1;
  }
  return 0;
}

// Read from the drive for up to 'sec' seconds; returns text bytes seen, -1 on
// error/disconnect.
static int pump(int fd, double sec) {
  int text   = 0;
  double end = now() + sec;
  for(;;) {
    double left = end - now();
    if(left <= 0 || stop_req) {
      return text;
    }
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    struct timeval tv = {(long)left, (long)((left - (long)left) * 1e6)};
    int r             = select(fd + 1, &rfds, NULL, NULL, &tv);
    if(r < 0) {
      if(errno == EINTR) {
        continue;
      }
      return -1;
    }
    if(r == 0) {
      return text;
    }
    unsigned char buf[1024];
    ssize_t n = read(fd, buf, sizeof(buf));
    if(n < 0) {
      if(errno == EINTR || errno == EAGAIN) {
        continue;
      }
      return -1;
    }
    if(n == 0) {
      return -1;  // device gone
    }
    text += demux(buf, (int)n);
  }
}

static int send_line(int fd, const char *line, int wait_ms) {
  size_t len = strlen(line);
  while(len && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
    len--;
  }
  const char *p = line;
  while(*p == ' ' || *p == '\t') {
    p++;
  }
  if(*p == '\0' || *p == '\n' || *p == '\r' || *p == '#') {
    return 0;
  }
  if(write_line(fd, line, len) < 0) {
    return -1;
  }
  return pump(fd, wait_ms / 1000.0) < 0 ? -1 : 0;
}

/*
 * Config dump/load.
 *
 * The drive keeps its config as text: a RAM buffer (edited with deleteconf /
 * appendconf) and a flash copy (written with flashsaveconf, printed with
 * showconf, parsed at boot). Loading therefore goes deleteconf -> appendconf
 * per line -> flashsaveconf -> showconf to verify. The running drive is not
 * affected until it is reset.
 *
 * Limits from the firmware:
 *  - term reads lines into a 64 byte buffer; a longer line is never consumed
 *    and blocks all further input, so "appendconf " + line must fit in 63.
 *  - call_cmd() takes the argument with sscanf(" %[ -~]"): leading blanks are
 *    dropped and the argument ends at the first non-printable character.
 *  - config buffer is 15 KiB including the terminating NUL.
 */
#define CONF_MAX_LINE 63
#define CONF_PREFIX "appendconf "
#define CONF_MAX_SIZE (15 * 1024 - 1)

// Send a command and collect its text reply in cap_buf. Returns once the
// drive has been quiet for 'quiet' s, or 'done' (if given) appears, or
// 'timeout' s passed. Returns -1 on I/O error.
static int capture_cmd(int fd, const char *cmd, double quiet, const char *done, double timeout) {
  capturing = 1;
  cap_len   = 0;
  if(cap_buf) {
    cap_buf[0] = '\0';
  }
  if(write_line(fd, cmd, strlen(cmd)) < 0) {
    capturing = 0;
    return -1;
  }
  double end = now() + timeout;
  int got    = 0;
  while(now() < end) {
    int t = pump(fd, done ? 0.05 : quiet);
    if(t < 0) {
      capturing = 0;
      return -1;
    }
    got += t;
    if(done && cap_buf && strstr(cap_buf, done)) {
      break;
    }
    if(!done && got && t == 0) {
      break;
    }
  }
  capturing = 0;
  return 0;
}

// Normalize config text the way the drive stores it: CR removed, tabs turned
// into spaces, leading blanks dropped, trailing blanks dropped, trailing
// empty lines dropped. Result is malloc'd.
static char *conf_normalize(const char *in) {
  size_t n  = strlen(in);
  char *out = malloc(n + 2);
  size_t o  = 0;
  const char *p = in;
  while(*p) {
    const char *e = strchr(p, '\n');
    size_t len    = e ? (size_t)(e - p) : strlen(p);
    size_t s      = 0;
    while(s < len && (p[s] == ' ' || p[s] == '\t' || p[s] == '\r')) {
      s++;
    }
    size_t t = len;
    while(t > s && (p[t - 1] == ' ' || p[t - 1] == '\t' || p[t - 1] == '\r')) {
      t--;
    }
    for(size_t i = s; i < t; i++) {
      out[o++] = p[i] == '\t' ? ' ' : p[i];
    }
    out[o++] = '\n';
    if(!e) {
      break;
    }
    p = e + 1;
  }
  while(o && out[o - 1] == '\n') {
    o--;
  }
  out[o++] = '\n';
  out[o]   = '\0';
  return out;
}

static char *read_file(const char *path) {
  FILE *f = strcmp(path, "-") == 0 ? stdin : fopen(path, "r");
  if(!f) {
    fprintf(stderr, "servoterm-cli: cannot open %s: %s\n", path, strerror(errno));
    return NULL;
  }
  size_t size = 0, cap = 4096;
  char *buf   = malloc(cap);
  size_t r;
  while((r = fread(buf + size, 1, cap - size - 1, f)) > 0) {
    size += r;
    if(cap - size < 2) {
      cap *= 2;
      buf = realloc(buf, cap);
    }
  }
  buf[size] = '\0';
  if(f != stdin) {
    fclose(f);
  }
  return buf;
}

static int write_file(const char *path, const char *data) {
  FILE *f = strcmp(path, "-") == 0 ? stdout : fopen(path, "w");
  if(!f) {
    fprintf(stderr, "servoterm-cli: cannot write %s: %s\n", path, strerror(errno));
    return -1;
  }
  fputs(data, f);
  if(f != stdout) {
    fclose(f);
  } else {
    fflush(f);
  }
  return 0;
}

// Read the flash config with showconf; returns normalized text (malloc'd).
static char *conf_read(int fd) {
  if(capture_cmd(fd, "showconf", 0.5, NULL, 10.0) < 0) {
    return NULL;
  }
  return conf_normalize(cap_buf ? cap_buf : "");
}

static int conf_dump(int fd, const char *path) {
  char *conf = conf_read(fd);
  if(!conf) {
    return -1;
  }
  int rc = write_file(path, conf);
  if(rc == 0 && strcmp(path, "-") != 0) {
    fprintf(stderr, "servoterm-cli: config (%zu bytes) written to %s\n", strlen(conf), path);
  }
  free(conf);
  return rc;
}

// Check every line before touching the drive. Returns number of errors.
static int conf_validate(const char *conf, const char *path) {
  int errors = 0, lineno = 0;
  const char *p = conf;
  while(*p) {
    const char *e = strchr(p, '\n');
    size_t len    = e ? (size_t)(e - p) : strlen(p);
    lineno++;
    if(strlen(CONF_PREFIX) + len > CONF_MAX_LINE) {
      fprintf(stderr, "%s:%d: line is %zu chars, max %zu: %.*s\n", path, lineno, len,
              (size_t)(CONF_MAX_LINE - strlen(CONF_PREFIX)), (int)len, p);
      errors++;
    }
    for(size_t i = 0; i < len; i++) {
      unsigned char c = p[i];
      if(c < ' ' || c > '~') {
        fprintf(stderr, "%s:%d: non-printable character 0x%02x at column %zu\n", path, lineno, c,
                i + 1);
        errors++;
        break;
      }
    }
    if(!e) {
      break;
    }
    p = e + 1;
  }
  if(strlen(conf) > CONF_MAX_SIZE) {
    fprintf(stderr, "%s: config is %zu bytes, max %d\n", path, strlen(conf), CONF_MAX_SIZE);
    errors++;
  }
  return errors;
}

static int conf_load(int fd, const char *path, int backup) {
  char *raw = read_file(path);
  if(!raw) {
    return -1;
  }
  char *conf = conf_normalize(raw);
  free(raw);
  if(conf_validate(conf, path)) {
    fprintf(stderr, "servoterm-cli: %s rejected, drive not touched\n", path);
    free(conf);
    return -1;
  }

  // Save what is in flash now, so a bad load can be undone.
  char *old = conf_read(fd);
  if(!old) {
    free(conf);
    return -1;
  }
  if(strcmp(old, conf) == 0) {
    fprintf(stderr, "servoterm-cli: drive config already matches %s, nothing to do\n", path);
    free(old);
    free(conf);
    return 0;
  }
  if(backup) {
    char bak[64];
    time_t t = time(NULL);
    strftime(bak, sizeof(bak), "conf-backup-%Y%m%d-%H%M%S.txt", localtime(&t));
    if(write_file(bak, old) < 0) {
      free(old);
      free(conf);
      return -1;
    }
    fprintf(stderr, "servoterm-cli: previous config saved to %s\n", bak);
  }
  free(old);

  if(capture_cmd(fd, "deleteconf", 0.1, NULL, 1.0) < 0) {
    free(conf);
    return -1;
  }

  // Append line by line, waiting for each "adding ..." echo before the next
  // one so the drive's 64 byte line buffer never overflows.
  int lines = 0, rc = 0;
  char cmd[CONF_MAX_LINE + 1];
  const char *p = conf;
  while(*p) {
    const char *e = strchr(p, '\n');
    size_t len    = e ? (size_t)(e - p) : strlen(p);
    snprintf(cmd, sizeof(cmd), CONF_PREFIX "%.*s", (int)len, p);
    if(capture_cmd(fd, cmd, 0.1, "\n", 2.0) < 0) {
      rc = -1;
      break;
    }
    // The drive answers "adding <arg>\n"; anything else (a merged line, a
    // missing reply) means the drive did not get exactly this line.
    char want[CONF_MAX_LINE + 16];
    snprintf(want, sizeof(want), "adding %.*s\n", (int)len, p);
    if(!cap_buf || strcmp(cap_buf, want) != 0) {
      fprintf(stderr, "servoterm-cli: no ack for line %d (%s), got: %s\n", lines + 1, cmd,
              cap_buf ? cap_buf : "");
      rc = -1;
      break;
    }
    lines++;
    if(!e) {
      break;
    }
    p = e + 1;
  }
  if(rc < 0) {
    fprintf(stderr, "servoterm-cli: load aborted, flash NOT written\n");
    free(conf);
    return -1;
  }

  // Flash sector erase takes a while; wait for the final "OK"/"error".
  if(capture_cmd(fd, "flashsaveconf", 0.1, "bytes written", 10.0) < 0) {
    free(conf);
    return -1;
  }
  if(!cap_buf || !strstr(cap_buf, "bytes written") || strstr(cap_buf, "error")) {
    fprintf(stderr, "servoterm-cli: flashsaveconf failed: %s\n", cap_buf ? cap_buf : "");
    free(conf);
    return -1;
  }

  char *now_conf = conf_read(fd);
  if(!now_conf || strcmp(now_conf, conf) != 0) {
    fprintf(stderr, "servoterm-cli: verify FAILED, flash differs from %s\n", path);
    free(now_conf);
    free(conf);
    return -1;
  }
  fprintf(stderr, "servoterm-cli: %d lines (%zu bytes) written to flash and verified.\n", lines,
          strlen(conf));
  fprintf(stderr, "servoterm-cli: takes effect after a drive reset.\n");
  free(now_conf);
  free(conf);
  return 0;
}

// ---- autotune support ----------------------------------------------------

// Write a pre-built normalized config string to flash (no file involved).
// label is shown in error messages, e.g. "(autotune)".
static int conf_write_text(int fd, char *conf, const char *label, int backup) {
  if(conf_validate(conf, label)) {
    fprintf(stderr, "servoterm-cli: %s rejected, drive not touched\n", label);
    return -1;
  }
  char *old = conf_read(fd);
  if(!old) {
    return -1;
  }
  if(strcmp(old, conf) == 0) {
    fprintf(stderr, "servoterm-cli: config unchanged, nothing to write\n");
    free(old);
    return 0;
  }
  if(backup) {
    char bak[64];
    time_t t = time(NULL);
    strftime(bak, sizeof(bak), "conf-backup-%Y%m%d-%H%M%S.txt", localtime(&t));
    if(write_file(bak, old) < 0) {
      free(old);
      return -1;
    }
    fprintf(stderr, "servoterm-cli: previous config saved to %s\n", bak);
  }
  free(old);
  if(capture_cmd(fd, "deleteconf", 0.1, NULL, 1.0) < 0) {
    return -1;
  }
  int lines = 0, rc = 0;
  char cmd[CONF_MAX_LINE + 1];
  const char *p = conf;
  while(*p) {
    const char *e = strchr(p, '\n');
    size_t len    = e ? (size_t)(e - p) : strlen(p);
    snprintf(cmd, sizeof(cmd), CONF_PREFIX "%.*s", (int)len, p);
    if(capture_cmd(fd, cmd, 0.1, "\n", 2.0) < 0) { rc = -1; break; }
    char want[CONF_MAX_LINE + 16];
    snprintf(want, sizeof(want), "adding %.*s\n", (int)len, p);
    if(!cap_buf || strcmp(cap_buf, want) != 0) {
      fprintf(stderr, "servoterm-cli: no ack for line %d (%s), got: %s\n",
              lines + 1, cmd, cap_buf ? cap_buf : "");
      rc = -1;
      break;
    }
    lines++;
    if(!e) break;
    p = e + 1;
  }
  if(rc < 0) {
    fprintf(stderr, "servoterm-cli: write aborted, flash NOT written\n");
    return -1;
  }
  if(capture_cmd(fd, "flashsaveconf", 0.1, "bytes written", 10.0) < 0) return -1;
  if(!cap_buf || !strstr(cap_buf, "bytes written") || strstr(cap_buf, "error")) {
    fprintf(stderr, "servoterm-cli: flashsaveconf failed: %s\n", cap_buf ? cap_buf : "");
    return -1;
  }
  char *now_conf = conf_read(fd);
  if(!now_conf || strcmp(now_conf, conf) != 0) {
    fprintf(stderr, "servoterm-cli: verify FAILED\n");
    free(now_conf);
    return -1;
  }
  fprintf(stderr, "servoterm-cli: %d lines (%zu bytes) written to flash and verified.\n",
          lines, strlen(conf));
  fprintf(stderr, "servoterm-cli: takes effect after a drive reset.\n");
  free(now_conf);
  return 0;
}

// Parse the last occurrence of "<pin> = <float>" from the capture buffer.
// Returns the value, or -1e38 as a sentinel if not found.
static double parse_pin_val(const char *buf, const char *pin) {
  if(!buf) return -1e38;
  char pat[128];
  snprintf(pat, sizeof(pat), "%s = ", pin);
  const char *p = NULL, *q = strstr(buf, pat);
  while(q) {
    p = q;
    q = strstr(q + 1, pat);
  }
  if(!p) return -1e38;
  double v = -1e38;
  sscanf(p + strlen(pat), "%lf", &v);
  return v;
}

// Set "conf0.<key> = <val>" in a config string (freed; result malloc'd).
// The last line for the key is the one loadconf applies, so the new line goes
// there and earlier duplicates are dropped; if there is none it is appended.
static char *conf_patch(char *conf, const char *key, const char *val, const char *tag) {
  char needle[64];
  snprintf(needle, sizeof(needle), "conf0.%s = ", key);
  size_t nl = strlen(needle);
  char line[256];  // longer than CONF_MAX_LINE so conf_validate rejects, not truncates
  snprintf(line, sizeof(line), "conf0.%s = %s # %s\n", key, val, tag);

  const char *last = NULL;
  for(const char *p = conf; *p;) {
    if(strncmp(p, needle, nl) == 0) last = p;
    const char *e = strchr(p, '\n');
    if(!e) break;
    p = e + 1;
  }

  char *out = malloc(strlen(conf) + strlen(line) + 2);
  size_t o  = 0;
  for(const char *p = conf; *p;) {
    const char *e = strchr(p, '\n');
    size_t len    = e ? (size_t)(e - p + 1) : strlen(p);
    if(p == last) {
      memcpy(out + o, line, strlen(line));
      o += strlen(line);
    } else if(strncmp(p, needle, nl) != 0) {
      memcpy(out + o, p, len);
      o += len;
    }
    p += len;
  }
  if(!last) {
    if(o && out[o - 1] != '\n') out[o++] = '\n';
    memcpy(out + o, line, strlen(line));
    o += strlen(line);
  }
  out[o] = '\0';
  free(conf);
  return out;
}

static const char *autotune_state_label(int s) {
  switch(s) {
  case 0: return "idle";
  case 1: return "R measurement";
  case 2: return "transition";
  case 3: return "polecount";
  case 4: return "psi / l / J (drive and coast)";
  case 5: return "friction / damping / load";
  case 6: return "done";
  case 7: return "ABORTED";
  default: return "?";
  }
}

static int autotune_run(int fd, const char *comp, double timeout, int backup) {
  char cmd[128];

  // Enable the identification state machine.
  snprintf(cmd, sizeof(cmd), "%s.en = 1", comp);
  if(send_line(fd, cmd, 50) < 0) return 1;

  fprintf(stderr, "\nservoterm-cli: autotune started  (component: %s, timeout: %.0fs)\n",
          comp, timeout);
  fprintf(stderr, "servoterm-cli: Ctrl-C to abort\n\n");

  double end     = now() + timeout;
  int last_state = -1;
  int done       = 0;

  while(now() < end && !stop_req) {
    snprintf(cmd, sizeof(cmd), "%s.state", comp);
    if(capture_cmd(fd, cmd, 0.3, NULL, 2.0) < 0) {
      fprintf(stderr, "servoterm-cli: read error during autotune\n");
      return 1;
    }
    double sv  = parse_pin_val(cap_buf, cmd);
    int state  = (int)(sv + 0.5);
    if(state != last_state) {
      fprintf(stderr, "  state %d — %s\n", state, autotune_state_label(state));
      last_state = state;
    }
    if(state >= 6) { done = 1; break; }
    pump(fd, 0.5);
  }

  if(!done) {
    fprintf(stderr, "\nservoterm-cli: autotune timed out (state %d)\n", last_state);
    return 1;
  }
  if(last_state == 7) {
    fprintf(stderr, "\nservoterm-cli: autotune aborted: motor exceeded 2 x %s.max_vel, or\n"
                    "  psi / j could not be measured. Output was switched off and results\n"
                    "  are invalid; nothing written. Check commutation and feedback\n"
                    "  direction (rlpsij has no fb_rev), and test_cur.\n",
            comp);
    return 1;
  }

  // Read the identified parameters.
  struct { const char *pin; const char *conf_key; double val; } params[] = {
    { "r",        "r"         },
    { "psi",      "psi"       },
    { "polecount","polecount" },
    { "j",        "j"         },
    { "friction", "f"         },
    { "damping",  "d"         },
    { "l",        "l"         },
  };
  int nparams = (int)(sizeof(params) / sizeof(params[0]));
  int read_ok = 1;

  for(int i = 0; i < nparams; i++) {
    snprintf(cmd, sizeof(cmd), "%s.%s", comp, params[i].pin);
    if(capture_cmd(fd, cmd, 0.3, NULL, 2.0) < 0) {
      fprintf(stderr, "servoterm-cli: failed to read %s\n", cmd);
      read_ok = 0;
      continue;
    }
    params[i].val = parse_pin_val(cap_buf, cmd);
  }

  if(!read_ok) return 1;

  int polecount_int = (int)(params[2].val + 0.5);

  // Physically impossible values are reported but never written, and so are
  // values the console's 6 decimals cannot resolve to 5% (e.g. j < 1e-5).
  int ok[7];
  ok[0] = params[0].val > 0.0;
  ok[1] = params[1].val > 0.0;
  ok[2] = polecount_int >= 1;
  ok[3] = params[3].val > 0.0;
  ok[4] = params[4].val >= 0.0;
  ok[5] = params[5].val >= 0.0;
  ok[6] = params[6].val > 0.0;
  const char *units[] = {"ohm", "Wb", "", "kg*m^2", "Nm", "Nm*s/rad", "H"};
  int nok = 0;

  fprintf(stderr, "\n--- Autotune results ---\n");
  for(int i = 0; i < nparams; i++) {
    const char *why = ok[i] ? "" : "   IMPLAUSIBLE, will not be written";
    if(ok[i] && i != 2 && params[i].val != 0.0 && fabs(params[i].val) < 1e-5) {
      ok[i] = 0;
      why   = "   LOW RESOLUTION (console prints 6 decimals), will not be written";
    }
    if(i == 2)
      fprintf(stderr, "  %-9s = %d", params[i].pin, polecount_int);
    else
      fprintf(stderr, "  %-9s = %.6g %s", params[i].pin, params[i].val, units[i]);
    fprintf(stderr, "%s\n", why);
    nok += ok[i];
  }
  fprintf(stderr, "------------------------\n\n");
  fprintf(stderr, "  Note: damping is only resolved when it is a sizeable part of the\n"
                  "  torque at test_vel; l is unreliable when l / r is below ~0.25 ms.\n\n");
  if(!nok) {
    fprintf(stderr, "servoterm-cli: no plausible results, nothing to write\n");
    return 1;
  }

  // Prompt (skip in non-interactive / headless mode).
  int write_it = 0;
  if(isatty(STDIN_FILENO)) {
    fprintf(stderr, "Write to flash config? [w = write / s = skip / q = quit]: ");
    fflush(stderr);
    char answer[8] = {0};
    if(fgets(answer, sizeof(answer), stdin)) {
      write_it = (answer[0] == 'w' || answer[0] == 'W');
      if(answer[0] == 'q' || answer[0] == 'Q') return 1;
    }
  } else {
    fprintf(stderr, "servoterm-cli: non-interactive, skipping flash write (use -c to apply)\n");
  }

  if(!write_it) {
    fprintf(stderr, "servoterm-cli: results not written\n");
    // Print the commands that would apply them, for reference.
    fprintf(stderr, "  To apply manually:\n");
    for(int i = 0; i < nparams; i++) {
      if(!ok[i]) continue;
      if(i == 2) // polecount: integer
        fprintf(stderr, "    conf0.%s = %d\n", params[i].conf_key, polecount_int);
      else
        fprintf(stderr, "    conf0.%s = %.6g\n", params[i].conf_key, params[i].val);
    }
    return 0;
  }

  // Patch the live config and write to flash.
  char *conf = conf_read(fd);
  if(!conf) return 1;

  char tag[32];
  time_t t = time(NULL);
  strftime(tag, sizeof(tag), "autotune %Y-%m-%d", localtime(&t));
  char valstr[32];
  for(int i = 0; i < nparams; i++) {
    if(!ok[i]) continue;
    if(i == 2) // polecount: integer
      snprintf(valstr, sizeof(valstr), "%d", polecount_int);
    else
      snprintf(valstr, sizeof(valstr), "%.6g", params[i].val);
    conf = conf_patch(conf, params[i].conf_key, valstr, tag);
  }

  int rc = conf_write_text(fd, conf, "(autotune)", backup);
  free(conf);
  return rc < 0 ? 1 : 0;
}

// ---- capture scope (shared/comps/scope.c) --------------------------------

static int b64val(char c) {
  if(c >= 'A' && c <= 'Z') return c - 'A';
  if(c >= 'a' && c <= 'z') return c - 'a' + 26;
  if(c >= '0' && c <= '9') return c - '0' + 52;
  if(c == '+') return 62;
  if(c == '/') return 63;
  return -1;
}

static int b64dec(const char *in, unsigned char *out, int max) {
  int n = 0, bits = 0;
  unsigned v = 0;
  for(; *in && *in != ' '; in++) {
    int d = b64val(*in);
    if(d < 0) {  // '=' padding
      continue;
    }
    v = (v << 6) | (unsigned)d;
    bits += 6;
    if(bits >= 8) {
      bits -= 8;
      if(n < max) {
        out[n++] = (unsigned char)(v >> bits);
      }
    }
  }
  return n;
}

static unsigned crc16(const unsigned char *d, int n) {  // CRC-16/CCITT-FALSE, as the drive's
  unsigned c = 0xffff;
  for(int i = 0; i < n; i++) {
    c ^= (unsigned)d[i] << 8;
    for(int b = 0; b < 8; b++) {
      c = c & 0x8000 ? ((c << 1) ^ 0x1021) & 0xffff : (c << 1) & 0xffff;
    }
  }
  return c;
}

// Arm (unless !arm), wait for the capture, download it (scopedump), check
// it, write CSV: t (s, 0 at the trigger) and a column per channel.
static int scope_capture(int fd, const char *path, int arm, double timeout) {
  double end = now() + timeout;
  if(arm && capture_cmd(fd, "scopearm", 0.3, "scope: armed", 5.0) < 0) {
    return -1;
  }
  double st = -1;
  while(now() < end && !stop_req) {
    if(capture_cmd(fd, "scope0.state", 0.3, "scope0.state = ", 5.0) < 0) {
      return -1;
    }
    st = parse_pin_val(cap_buf, "scope0.state");
    if(st == 3.0) {
      break;
    }
    usleep(200000);
  }
  if(st != 3.0) {
    fprintf(stderr, "servoterm-cli: no capture within %g s (scope0.state %g)\n", timeout, st);
    return -1;
  }
  double t0 = now();
  if(capture_cmd(fd, "scopedump", 2.0, "scope end", end - now() > 30 ? end - now() : 30) < 0) {
    return -1;
  }
  const char *b = cap_buf ? strstr(cap_buf, "scope begin") : NULL;
  int nch = 0, n = 0, pre = 0;
  double dt = 0;
  if(!b || sscanf(b, "scope begin nch=%d n=%d dt=%lf pre=%d", &nch, &n, &dt, &pre) != 4 || nch < 1 || nch > 8 ||
     n < 1 || !strstr(b, "scope end")) {
    fprintf(stderr, "servoterm-cli: bad or incomplete scopedump\n");
    return -1;
  }
  char names[8][64];
  double mn[8], mx[8];
  for(int c = 0; c < nch; c++) {
    char key[24];
    snprintf(key, sizeof(key), "scope ch%d ", c);
    const char *l = strstr(b, key);
    if(!l || sscanf(l + strlen(key), "%63s min=%lf max=%lf", names[c], &mn[c], &mx[c]) != 3) {
      fprintf(stderr, "servoterm-cli: scopedump lacks channel %d\n", c);
      return -1;
    }
  }
  size_t total = (size_t)n * nch;
  unsigned short *q = calloc(total, sizeof(*q));
  size_t got = 0;
  int bad = 0, lines = 0;
  for(const char *l = strstr(b, "scope data "); l; l = strstr(l + 1, "scope data ")) {
    unsigned char raw[64];
    int k = b64dec(l + 11, raw, sizeof(raw));
    const char *sp = strchr(l + 11, ' ');
    unsigned crc = 0;
    if(!sp || sscanf(sp + 1, "%x", &crc) != 1 || crc != crc16(raw, k)) {
      bad++;
    }
    for(int i = 0; i + 1 < k && got < total; i += 2) {
      q[got++] = (unsigned short)(raw[i] | raw[i + 1] << 8);
    }
    lines++;
  }
  if(bad || got != total) {
    fprintf(stderr, "servoterm-cli: scopedump: %d bad lines, %zu of %zu samples\n", bad, got, total);
    free(q);
    return -1;
  }
  FILE *f = strcmp(path, "-") ? fopen(path, "w") : stdout;
  if(!f) {
    perror(path);
    free(q);
    return -1;
  }
  fprintf(f, "t");
  for(int c = 0; c < nch; c++) {
    fprintf(f, ",%s", names[c]);
  }
  fprintf(f, "\n");
  for(int i = 0; i < n; i++) {
    fprintf(f, "%.6g", (i - pre) * dt);
    for(int c = 0; c < nch; c++) {
      fprintf(f, ",%.6g", mn[c] + q[i * nch + c] / 65535.0 * (mx[c] - mn[c]));
    }
    fprintf(f, "\n");
  }
  if(f != stdout) {
    fclose(f);
    fprintf(stderr, "servoterm-cli: capture %d x %d samples (dt %g s) in %.1f s, %d lines, CRCs ok -> %s\n", nch,
            n, dt, now() - t0, lines, path);
  }
  free(q);
  return 0;
}

// ---- in-band firmware update (shared/comps/fwup.c, shared/fwstage.h) ----

#define FW_LINE 30   // bytes per fwdata line: the console's 63 characters
#define FW_WINDOW 3  // lines in flight: the smart-serial console buffers 256 bytes

static unsigned long fw_crc32(const unsigned char *d, size_t n) {
  unsigned long c = 0xffffffffUL;
  for(size_t i = 0; i < n; i++) {
    c ^= d[i];
    for(int b = 0; b < 8; b++) {
      c = c & 1 ? (c >> 1) ^ 0xEDB88320UL : c >> 1;
    }
  }
  return c ^ 0xffffffffUL;
}

// send cmd, wait for its "fw ..." reply line; NULL on timeout
static const char *fw_cmd(int fd, const char *cmd, double timeout) {
  if(capture_cmd(fd, cmd, 0.1, "\n", timeout) < 0 || !cap_buf) {
    return NULL;
  }
  const char *r = strstr(cap_buf, "fw ");
  return r && strchr(r, '\n') ? r : NULL;
}

static int fw_update(int fd, const char *path, int apply) {
  FILE *f = fopen(path, "rb");
  if(!f) {
    fprintf(stderr, "servoterm-cli: %s: %s\n", path, strerror(errno));
    return -1;
  }
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);
  unsigned char *img = malloc(size > 0 ? size : 1);
  if(size <= 0 || fread(img, 1, size, f) != (size_t)size) {
    fprintf(stderr, "servoterm-cli: %s: cannot read\n", path);
    fclose(f);
    free(img);
    return -1;
  }
  fclose(f);
  unsigned long crc = fw_crc32(img, size);
  char cmd[128];
  snprintf(cmd, sizeof(cmd), "fwbegin %ld %08lx", size, crc);
  const char *r = fw_cmd(fd, cmd, 5.0);
  long off = 0;
  if(!r || strncmp(r, "fw ok", 5)) {
    fprintf(stderr, "servoterm-cli: fwbegin: %.*s\n", r ? (int)strcspn(r, "\n") : 7, r ? r : "no reply");
    free(img);
    return -1;
  }
  if(sscanf(r, "fw ok resume %ld", &off) == 1) {
    fprintf(stderr, "servoterm-cli: resuming at %ld of %ld bytes\n", off, size);
  }
  // a window of lines in flight (the drive's console buffers a few); every
  // reply carries the drive's position, the last one says where to go on
  double t0 = now();
  int retries = 0, busy = 0, last_pct = -1;
  while(off < size && !stop_req) {
    int k = 0;
    capturing = 1;
    cap_len   = 0;
    if(cap_buf) {
      cap_buf[0] = '\0';
    }
    for(long o2 = off; o2 < size && k < FW_WINDOW; o2 += FW_LINE, k++) {
      int n = size - o2 < FW_LINE ? (int)(size - o2) : FW_LINE;
      static const char t[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
      char b64[64];
      int o = 0;
      for(int i = 0; i < n; i += 3) {
        const unsigned char *p = img + o2 + i;
        unsigned v = (unsigned)p[0] << 16 | (i + 1 < n ? (unsigned)p[1] << 8 : 0) | (i + 2 < n ? p[2] : 0);
        b64[o++]   = t[(v >> 18) & 63];
        b64[o++]   = t[(v >> 12) & 63];
        b64[o++]   = i + 1 < n ? t[(v >> 6) & 63] : '=';
        b64[o++]   = i + 2 < n ? t[v & 63] : '=';
      }
      b64[o] = 0;
      snprintf(cmd, sizeof(cmd), "fwdata %ld %s %04x", o2, b64, crc16(img + o2, n));
      if(write_line(fd, cmd, strlen(cmd)) < 0) {
        capturing = 0;
        free(img);
        return -1;
      }
    }
    // the replies: one line each, "fw ok GOT" / "fw busy" / "fw err E at GOT"
    double end = now() + 3.0;
    int replies = 0;
    while(replies < k && now() < end) {
      if(pump(fd, 0.02) < 0) {
        capturing = 0;
        free(img);
        return -1;
      }
      replies = 0;
      for(const char *q = cap_buf; q && (q = strstr(q, "fw ")); q++) {
        replies += strchr(q, '\n') != NULL;
      }
    }
    capturing = 0;
    long pos = -1, got;
    int bad = 0;
    for(const char *q = cap_buf; q && (q = strstr(q, "fw ")); q++) {
      if(sscanf(q, "fw ok %ld", &got) == 1 || sscanf(q, "fw err %*d at %ld", &got) == 1) {
        pos = got;
      }
      if(!strncmp(q, "fw busy", 7)) {
        busy++;
      }
      bad += !strncmp(q, "fw err", 6) && strncmp(q, "fw err -3", 9);  // a gap after a busy line is expected
    }
    if(pos > off && pos <= size && pos % FW_LINE == 0) {
      off     = pos;
      retries = 0;
    } else if(pos >= 0 && pos <= size && pos % FW_LINE == 0 && bad == 0) {
      off = pos;  // no progress (busy): again
      pump(fd, 0.01);
    } else if(pos == size) {
      off = size;
    } else if(++retries > 10) {
      fprintf(stderr, "servoterm-cli: fwdata at %ld: no progress (%s)\n", off, cap_buf ? cap_buf : "no reply");
      free(img);
      return -1;
    }
    int pct = (int)(100 * off / size);
    if(pct / 10 != last_pct / 10) {
      fprintf(stderr, "servoterm-cli: %3d%% (%ld bytes, %.0f s)\n", pct, off, now() - t0);
      last_pct = pct;
    }
  }
  free(img);
  if(stop_req) {
    return -1;  // fwbegin with the same image resumes
  }
  for(int i = 0; i < 50; i++) {
    r = fw_cmd(fd, "fwend", 3.0);
    if(!r || strncmp(r, "fw busy", 7)) {
      break;
    }
    pump(fd, 0.05);
  }
  if(!r || strncmp(r, "fw ok staged", 12)) {
    fprintf(stderr, "servoterm-cli: fwend: %.*s\n", r ? (int)strcspn(r, "\n") : 8, r ? r : "no reply");
    return -1;
  }
  fprintf(stderr, "servoterm-cli: staged %ld bytes, crc32 %08lx, in %.1f s (%d busy)\n", size, crc, now() - t0, busy);
  if(!apply) {
    return 0;
  }
  r = fw_cmd(fd, "fwapply", 3.0);
  if(!r || strncmp(r, "fw ok", 5)) {
    fprintf(stderr, "servoterm-cli: fwapply: %.*s\n", r ? (int)strcspn(r, "\n") : 8, r ? r : "no reply");
    return -1;
  }
  fprintf(stderr, "servoterm-cli: applied: the drive resets, its bootloader copies the image\n");
  return 0;
}

// --wizard: the setup wizard, on its own link (setup/su_link.c)
static int wizard_main(const char *dev, const char *session, const char *answers, const char *redo,
                       const char *one, const char *out_dir, double time_scale, int verbose) {
  su_cli_t *cli = su_cli_new(answers, verbose);
  if(!cli) {
    fprintf(stderr, "servoterm-cli: cannot read answers file %s\n", answers);
    return 1;
  }
  char err[256];
  su_link_t *link = su_link_open(dev, err, sizeof(err));
  if(!link) {
    fprintf(stderr, "servoterm-cli: %s\n", err);
    su_cli_free(cli);
    return 1;
  }
  su_state_t *st = su_state_new();
  if(su_state_load(st, session) == 0) {
    fprintf(stderr, "servoterm-cli: resuming the session in %s\n", session);
  }
  su_wizard_t w = {link, su_cli_ui(cli), st, session, out_dir, STMBL_SETUP_API, SETUP_GIT, time_scale};
  if(redo) {
    su_wizard_redo(&w, redo);
  }
  su_result_t r = one ? su_wizard_step(&w, one) : su_wizard_run(&w);
  if(r == SU_ABORTED) {
    fprintf(stderr, "\nStopped. The session is in %s: run the wizard again to go on.\n", session);
  } else if(r == SU_FAIL) {
    fprintf(stderr, "\nThe step failed (above). Fix it and run the wizard again: it repeats that step.\n");
  }
  su_state_free(st);
  su_link_close(link);
  su_cli_free(cli);
  return r == SU_OK ? 0 : 1;
}

static void usage(void) {
  fprintf(stderr,
          "usage: servoterm-cli [-d DEV] [-s FILE] [-t SEC] [-w MS]\n"
          "                     [--dump FILE] [--load FILE [--no-backup]]\n"
          "                     [-C pin:gain,...] [--step N] [--log SEC]\n"
          "                     [--autotune [--autotune-comp COMP] [--autotune-timeout SEC]]\n"
          "                     [--capture FILE [--no-arm] [--capture-timeout SEC]]\n"
          "                     [--update IMAGE [--no-apply]]\n"
          "                     [--wizard [--session FILE] [--answers FILE] [--redo STEP]\n"
          "                      [--wizard-step STEP] [--out DIR] [--time-scale N] [-v]]\n"
          "                     [-c CMD]... [SCRIPT]\n");
}

int main(int argc, char **argv) {
  const char *dev        = "/dev/ttyACM0";
  const char *scope_path = NULL;
  const char *script     = NULL;
  const char *cmds[MAX_CMDS];
  int ncmds             = 0;
  double settle         = 0.5;
  int wait_ms           = 50;
  const char *dump_path      = NULL;
  const char *load_path      = NULL;
  int backup                 = 1;
  const char *scope_cfg      = NULL;
  int send_step              = 0;
  double log_sec             = -1.0;  // < 0: no log mode
  int do_autotune            = 0;
  const char *autotune_comp  = "rlpsij0";
  double autotune_timeout    = 120.0;
  const char *capture_path   = NULL;  // --capture: the capture scope's samples as CSV
  int capture_arm            = 1;
  double capture_timeout     = 60.0;
  const char *update_path    = NULL;  // --update: in-band firmware update
  int update_apply           = 1;
  int wizard                 = 0;  // --wizard: the setup wizard
  const char *wiz_session    = "stmbl-setup-session.txt";
  const char *wiz_answers    = NULL;
  const char *wiz_redo       = NULL;
  const char *wiz_step       = NULL;
  const char *wiz_out        = ".";
  double wiz_scale           = 1.0;
  int verbose                = 0;

  for(int i = 1; i < argc; i++) {
    if(strcmp(argv[i], "--wizard") == 0) {
      wizard = 1;
    } else if(strcmp(argv[i], "--session") == 0 && i + 1 < argc) {
      wiz_session = argv[++i];
    } else if(strcmp(argv[i], "--answers") == 0 && i + 1 < argc) {
      wiz_answers = argv[++i];
    } else if(strcmp(argv[i], "--redo") == 0 && i + 1 < argc) {
      wiz_redo = argv[++i];
    } else if(strcmp(argv[i], "--wizard-step") == 0 && i + 1 < argc) {
      wiz_step = argv[++i];
    } else if(strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
      wiz_out = argv[++i];
    } else if(strcmp(argv[i], "--time-scale") == 0 && i + 1 < argc) {
      wiz_scale = atof(argv[++i]);
    } else if(strcmp(argv[i], "-v") == 0) {
      verbose = 1;
    } else if(strcmp(argv[i], "-C") == 0 && i + 1 < argc) {
      scope_cfg = argv[++i];
    } else if(strcmp(argv[i], "--step") == 0 && i + 1 < argc) {
      send_step = atoi(argv[++i]);
    } else if(strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
      log_sec = atof(argv[++i]);
    } else if(strcmp(argv[i], "--autotune") == 0) {
      do_autotune = 1;
    } else if(strcmp(argv[i], "--autotune-comp") == 0 && i + 1 < argc) {
      autotune_comp = argv[++i];
    } else if(strcmp(argv[i], "--autotune-timeout") == 0 && i + 1 < argc) {
      autotune_timeout = atof(argv[++i]);
    } else if(strcmp(argv[i], "--capture") == 0 && i + 1 < argc) {
      capture_path = argv[++i];
    } else if(strcmp(argv[i], "--update") == 0 && i + 1 < argc) {
      update_path = argv[++i];
    } else if(strcmp(argv[i], "--no-apply") == 0) {
      update_apply = 0;
    } else if(strcmp(argv[i], "--no-arm") == 0) {
      capture_arm = 0;
    } else if(strcmp(argv[i], "--capture-timeout") == 0 && i + 1 < argc) {
      capture_timeout = atof(argv[++i]);
    } else if(strcmp(argv[i], "--dump") == 0 && i + 1 < argc) {
      dump_path = argv[++i];
    } else if(strcmp(argv[i], "--load") == 0 && i + 1 < argc) {
      load_path = argv[++i];
    } else if(strcmp(argv[i], "--no-backup") == 0) {
      backup = 0;
    } else if(strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
      dev = argv[++i];
    } else if(strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
      scope_path = argv[++i];
    } else if(strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
      settle = atof(argv[++i]);
    } else if(strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
      wait_ms = atoi(argv[++i]);
    } else if(strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
      if(ncmds < MAX_CMDS) {
        cmds[ncmds++] = argv[++i];
      }
    } else if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      usage();
      return 0;
    } else if(argv[i][0] == '-' && argv[i][1] != '\0') {
      usage();
      return 1;
    } else {
      script = argv[i];
    }
  }

  if(wizard) {
    return wizard_main(dev, wiz_session, wiz_answers, wiz_redo, wiz_step, wiz_out, wiz_scale, verbose);
  }

  // -C "pin:gain,pin:gain,...": up to 8 channels, in order wave0..wave7.
  // An empty entry leaves that channel as it is on the drive.
  char *cfg_copy = scope_cfg ? strdup(scope_cfg) : NULL;
  if(cfg_copy) {
    char *save = NULL;
    char *tok  = strtok_r(cfg_copy, ",", &save);
    for(int i = 0; tok && i < SCOPE_CHANNELS; i++) {
      char *colon = strchr(tok, ':');
      if(!colon || colon == tok || atof(colon + 1) == 0.0) {
        fprintf(stderr, "servoterm-cli: -C entry %d must be pin:gain (non-zero gain): %s\n", i, tok);
        return 1;
      }
      *colon        = '\0';
      scope_name[i] = tok;
      scope_gain[i] = atof(colon + 1);
      tok           = strtok_r(NULL, ",", &save);
    }
  }
  if(send_step > 0) {
    scope_dt = send_step / 5000.0;  // rt runs at 5 kHz
  }

  if(scope_path) {
    scope_fp = fopen(scope_path, "w");
    if(!scope_fp) {
      fprintf(stderr, "servoterm-cli: cannot open scope file %s\n", scope_path);
      return 1;
    }
    fprintf(scope_fp, "frame");
    if(scope_dt > 0.0) {
      fprintf(scope_fp, ",t");
    }
    for(int i = 0; i < SCOPE_CHANNELS; i++) {
      if(scope_name[i]) {
        fprintf(scope_fp, ",%s", scope_name[i]);
      } else {
        fprintf(scope_fp, ",ch%d", i);
      }
    }
    fprintf(scope_fp, "\n");
  }

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = on_signal;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGHUP, &sa, NULL);

  int fd = open_port(dev);
  if(fd < 0) {
    return 1;
  }
  // Give the drive time to see the connection (DTR), and drop whatever it had
  // queued before, including a partial scope frame at the start of the stream.
  discard_text = 1;
  pump(fd, 0.3);
  // A lone newline ends any half-line left in the drive's input buffer by an
  // earlier session (an empty line is a no-op on the drive).
  if(write_all(fd, "\n", 1) < 0) {
    perror("servoterm-cli: write");
    close(fd);
    return 1;
  }
  pump(fd, 0.2);
  discard_text = 0;

  int rc = 0;
  // Scope setup (RAM only, not saved to flash).
  for(int i = 0; i < SCOPE_CHANNELS && rc == 0; i++) {
    if(scope_name[i]) {
      char line[MAX_CMD_LINE + 1];
      snprintf(line, sizeof(line), "term0.wave%d = %s", i, scope_name[i]);
      rc = send_line(fd, line, wait_ms);
      snprintf(line, sizeof(line), "term0.gain%d = %g", i, scope_gain[i]);
      rc = rc ? rc : send_line(fd, line, wait_ms);
      snprintf(line, sizeof(line), "term0.offset%d = 0", i);
      rc = rc ? rc : send_line(fd, line, wait_ms);
    }
  }
  if(rc == 0 && send_step > 0) {
    char line[MAX_CMD_LINE + 1];
    snprintf(line, sizeof(line), "term0.send_step = %d", send_step);
    rc = send_line(fd, line, wait_ms);
  }
  // Config operations run first, so "--load f -c reset" applies the new config.
  if(load_path) {
    rc = conf_load(fd, load_path, backup) < 0;
  }
  if(rc == 0 && dump_path) {
    rc = conf_dump(fd, dump_path) < 0;
  }
  if(rc == 0 && do_autotune) {
    rc = autotune_run(fd, autotune_comp, autotune_timeout, backup);
  }
  if(rc) {
    // don't send further commands after a failed config operation
  } else if(ncmds) {
    for(int i = 0; i < ncmds && rc == 0; i++) {
      rc = send_line(fd, cmds[i], wait_ms);
    }
  } else if(script || !(dump_path || load_path || log_sec >= 0.0 || scope_cfg || capture_path || update_path)) {
    FILE *in = stdin;
    if(script) {
      in = fopen(script, "r");
      if(!in) {
        fprintf(stderr, "servoterm-cli: cannot open script %s\n", script);
        close(fd);
        return 1;
      }
    }
    if(in == stdin && isatty(STDIN_FILENO)) {
      // Interactive: multiplex keyboard lines and drive output.
      char line[256];
      for(;;) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        FD_SET(STDIN_FILENO, &rfds);
        if(select(fd + 1, &rfds, NULL, NULL, NULL) < 0) {
          if(errno == EINTR) {
            continue;
          }
          break;
        }
        if(FD_ISSET(fd, &rfds) && pump(fd, 0) < 0) {
          fprintf(stderr, "servoterm-cli: device disconnected\n");
          rc = 1;
          break;
        }
        if(FD_ISSET(STDIN_FILENO, &rfds)) {
          if(!fgets(line, sizeof(line), stdin)) {
            break;
          }
          if(send_line(fd, line, 0) < 0) {
            rc = 1;
            break;
          }
        }
      }
    } else {
      char line[256];
      while(rc == 0 && fgets(line, sizeof(line), in)) {
        rc = send_line(fd, line, wait_ms);
      }
    }
    if(in != stdin) {
      fclose(in);
    }
  }

  if(rc == 0 && log_sec >= 0.0) {
    // Record until log_sec passed (0 = until SIGINT/SIGTERM), flushing the
    // CSV every second so a crash or kill loses at most a second.
    fprintf(stderr, "servoterm-cli: logging%s, stop with Ctrl-C or kill %d\n",
            log_sec > 0.0 ? "" : " until stopped", (int)getpid());
    stamp_text = 1;
    double end = now() + log_sec;
    while(!stop_req && (log_sec == 0.0 || now() < end)) {
      if(pump(fd, 1.0) < 0) {
        fprintf(stderr, "servoterm-cli: device disconnected\n");
        rc = 1;
        break;
      }
      fflush(stdout);
      if(scope_fp) {
        fflush(scope_fp);
      }
    }
    stamp_text = 0;
    settle     = 0.0;
  }

  // The capture scope: after the -c commands, which set it up
  if(rc == 0 && capture_path) {
    pump(fd, 0.3);
    rc = scope_capture(fd, capture_path, capture_arm, capture_timeout) < 0;
  }

  if(rc == 0 && update_path) {
    pump(fd, 0.3);
    rc = fw_update(fd, update_path, update_apply) < 0;
    if(rc == 0 && update_apply) {
      settle = 0.0;  // the drive resets now: its port goes away, which is no error
    }
  }

  // Collect trailing output until the drive goes quiet. Scope frames alone do
  // not count as activity, since they stream continuously.
  while(rc == 0 && settle > 0.0 && !stop_req) {
    int t = pump(fd, settle);
    if(t < 0) {
      rc = 1;
    }
    if(t <= 0) {
      break;
    }
  }

  close(fd);
  if(scope_fp) {
    fprintf(stderr, "servoterm-cli: %ld scope frames written to %s\n", scope_frame, scope_path);
    fclose(scope_fp);
  }
  return rc;
}
