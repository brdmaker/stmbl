/*
 * This file is part of the stmbl project.
 *
 * servoterm: a headless command-line client for the stmbl core, modelled on
 * QtServoterm (https://github.com/STMBL/QtServoterm) but with no GUI. It is
 * meant for building automated test assets: it spawns the x86 host simulator
 * (stmbl_host --serve), forwards commands to it, and demultiplexes the reply
 * stream the same way QtServoterm does -- plain text is printed, while binary
 * oscilloscope frames (0xFF marker + 8 channel bytes) are decoded to a CSV.
 *
 * Wire protocol (identical to the device's USB serial link):
 *   host -> core : command lines terminated with '\n'
 *   core -> host : text responses, interleaved with scope frames:
 *                    0xFF, ch0, ch1, ... ch7   (each channel byte in 1..254)
 *                    0xFE                       = scope reset
 *                  channel value = (byte - 128) / 128.0
 *
 * Usage:
 *   servoterm [--sim PATH] [--scope FILE] [SCRIPT]
 *     --sim PATH    path to the host simulator (default ./stmbl_host)
 *     --scope FILE  write decoded scope samples to FILE as CSV
 *     SCRIPT        file of commands to send; if omitted, reads stdin
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>

#define SCOPE_CHANNELS 8
#define SCOPE_MARKER 0xFF
#define SCOPE_RESET 0xFE

static FILE *scope_fp   = NULL;
static long scope_frame = 0;
static int frame_active = 0;
static int frame_have   = 0;
static unsigned char frame[SCOPE_CHANNELS];

static void scope_emit(void) {
  if(scope_fp) {
    fprintf(scope_fp, "%ld", scope_frame);
    for(int i = 0; i < SCOPE_CHANNELS; i++) {
      fprintf(scope_fp, ",%.5f", (frame[i] - 128) / 128.0);
    }
    fprintf(scope_fp, "\n");
  }
  scope_frame++;
}

// Split the core's byte stream into text (printed) and scope frames (to CSV).
static void demux(const unsigned char *buf, int n) {
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
    } else {
      putchar(b);
    }
  }
  fflush(stdout);
}

int main(int argc, char **argv) {
  const char *sim_path   = "./stmbl_host";
  const char *scope_path = NULL;
  const char *script     = NULL;

  for(int i = 1; i < argc; i++) {
    if(strcmp(argv[i], "--sim") == 0 && i + 1 < argc) {
      sim_path = argv[++i];
    } else if(strcmp(argv[i], "--scope") == 0 && i + 1 < argc) {
      scope_path = argv[++i];
    } else {
      script = argv[i];
    }
  }

  if(scope_path) {
    scope_fp = fopen(scope_path, "w");
    if(!scope_fp) {
      fprintf(stderr, "servoterm: cannot open scope file %s\n", scope_path);
      return 1;
    }
    fprintf(scope_fp, "frame");
    for(int i = 0; i < SCOPE_CHANNELS; i++) {
      fprintf(scope_fp, ",ch%d", i);
    }
    fprintf(scope_fp, "\n");
  }

  // Command input: a script file, or our own stdin for interactive use.
  int in_fd       = STDIN_FILENO;
  FILE *script_fp = NULL;
  if(script) {
    script_fp = fopen(script, "r");
    if(!script_fp) {
      fprintf(stderr, "servoterm: cannot open script %s\n", script);
      return 1;
    }
    in_fd = fileno(script_fp);
  }

  // Pipes: to_sim (we write, sim reads as stdin); from_sim (sim writes stdout).
  int to_sim[2], from_sim[2];
  if(pipe(to_sim) < 0 || pipe(from_sim) < 0) {
    perror("pipe");
    return 1;
  }

  pid_t pid = fork();
  if(pid < 0) {
    perror("fork");
    return 1;
  }
  if(pid == 0) {
    // child: wire pipes to stdin/stdout and exec the simulator in serve mode
    dup2(to_sim[0], STDIN_FILENO);
    dup2(from_sim[1], STDOUT_FILENO);
    close(to_sim[0]);
    close(to_sim[1]);
    close(from_sim[0]);
    close(from_sim[1]);
    execl(sim_path, sim_path, "--serve", (char *)NULL);
    perror("servoterm: exec simulator");
    _exit(127);
  }

  // parent
  close(to_sim[0]);
  close(from_sim[1]);
  int sim_in  = to_sim[1];
  int sim_out = from_sim[0];

  int input_open = 1;
  for(;;) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(sim_out, &rfds);
    if(input_open) {
      FD_SET(in_fd, &rfds);
    }
    int maxfd = (sim_out > in_fd ? sim_out : in_fd) + 1;

    if(select(maxfd, &rfds, NULL, NULL, NULL) < 0) {
      if(errno == EINTR) {
        continue;
      }
      break;
    }

    // Always service the core's output first to avoid pipe back-pressure.
    if(FD_ISSET(sim_out, &rfds)) {
      unsigned char buf[1024];
      ssize_t n = read(sim_out, buf, sizeof(buf));
      if(n <= 0) {
        break;  // core closed -> done
      }
      demux(buf, (int)n);
    }

    if(input_open && FD_ISSET(in_fd, &rfds)) {
      char buf[1024];
      ssize_t n = read(in_fd, buf, sizeof(buf));
      if(n <= 0) {
        // input exhausted: close the core's stdin so it finishes and exits
        close(sim_in);
        input_open = 0;
      } else {
        ssize_t off = 0;
        while(off < n) {
          ssize_t w = write(sim_in, buf + off, n - off);
          if(w <= 0) {
            break;
          }
          off += w;
        }
      }
    }
  }

  if(input_open) {
    close(sim_in);
  }
  // drain anything the core printed before exiting
  for(;;) {
    unsigned char buf[1024];
    ssize_t n = read(sim_out, buf, sizeof(buf));
    if(n <= 0) {
      break;
    }
    demux(buf, (int)n);
  }

  close(sim_out);
  int status = 0;
  waitpid(pid, &status, 0);
  if(script_fp) {
    fclose(script_fp);
  }
  if(scope_fp) {
    fclose(scope_fp);
  }
  return 0;
}
