#include "su_link.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define SCOPE_MARKER 0xFF
#define SCOPE_RESET 0xFE
#define SCOPE_CHANNELS 8

struct su_link {
  char *dev;
  int fd;
  int in_frame;  // bytes of a scope frame still to drop
  int reconnects;  // the device came back after it went away
  char *buf;     // text since the last clear
  size_t len, cap;
  void (*on_text)(void *, const char *, size_t);
  void *user;
};

static double now(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + tv.tv_usec / 1e6;
}

static int open_fd(const char *dev) {
  int fd = open(dev, O_RDWR | O_NOCTTY);
  if(fd < 0) {
    return -1;
  }
  struct termios tio;
  if(tcgetattr(fd, &tio) == 0) {  // raw: scope bytes and CR/LF pass untouched
    cfmakeraw(&tio);
    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_cc[VMIN]  = 0;
    tio.c_cc[VTIME] = 0;
    tcsetattr(fd, TCSANOW, &tio);
  }
  tcflush(fd, TCIOFLUSH);
  return fd;
}

static void append(su_link_t *l, const char *p, size_t n) {
  if(!n) {
    return;
  }
  if(l->len + n + 1 > l->cap) {
    while(l->len + n + 1 > l->cap) {
      l->cap = l->cap ? 2 * l->cap : 4096;
    }
    l->buf = realloc(l->buf, l->cap);
  }
  memcpy(l->buf + l->len, p, n);
  l->len += n;
  l->buf[l->len] = '\0';
  if(l->on_text) {
    l->on_text(l->user, p, n);
  }
}

// drop scope frames, keep text (CR dropped)
static int demux(su_link_t *l, const unsigned char *b, int n) {
  char text[1024];
  int t = 0, total = 0;
  for(int i = 0; i < n; i++) {
    if(l->in_frame) {
      l->in_frame--;
      continue;
    }
    if(b[i] == SCOPE_MARKER) {
      l->in_frame = SCOPE_CHANNELS;
      continue;
    }
    if(b[i] == SCOPE_RESET || b[i] == '\r') {
      continue;
    }
    text[t++] = (char)b[i];
    if(t == (int)sizeof(text)) {
      append(l, text, t);
      total += t;
      t = 0;
    }
  }
  append(l, text, t);
  return total + t;
}

su_link_t *su_link_open(const char *dev, char *err, size_t errlen) {
  int fd = open_fd(dev);
  if(fd < 0) {
    snprintf(err, errlen, "cannot open %s: %s", dev, strerror(errno));
    return NULL;
  }
  su_link_t *l = calloc(1, sizeof(su_link_t));
  l->dev       = strdup(dev);
  l->fd        = fd;
  // drop what the drive had queued, end any half line an earlier session left
  su_link_pump(l, 0.3);
  if(write(fd, "\n", 1) != 1) {
    snprintf(err, errlen, "cannot write %s: %s", dev, strerror(errno));
    su_link_close(l);
    return NULL;
  }
  su_link_pump(l, 0.2);
  su_link_clear(l);
  return l;
}

void su_link_close(su_link_t *l) {
  if(!l) {
    return;
  }
  if(l->fd >= 0) {
    close(l->fd);
  }
  free(l->dev);
  free(l->buf);
  free(l);
}

void su_link_on_text(su_link_t *l, void (*fn)(void *, const char *, size_t), void *user) {
  l->on_text = fn;
  l->user    = user;
}

// The drive went away (its USB re-enumerates when its console was stuck,
// usbd_cdc_core.c; a reset): open the same path again, for up to 15 s. A
// stable path (/dev/serial/by-id/...) survives a new ttyACMn. 0: back.
static int reconnect(su_link_t *l) {
  if(l->fd >= 0) {
    close(l->fd);
    l->fd = -1;
  }
  double end = now() + 15.0;
  while(now() < end) {
    struct timespec ts = {0, 300000000};
    nanosleep(&ts, NULL);
    int fd = open_fd(l->dev);
    if(fd >= 0) {
      l->fd       = fd;
      l->in_frame = 0;
      l->reconnects++;
      return 0;
    }
  }
  return -1;
}

int su_link_reconnects(const su_link_t *l) {
  return l->reconnects;
}

int su_link_send(su_link_t *l, const char *line) {
  size_t n = strlen(line);
  char buf[SU_LINE_MAX + 2];
  if(n > SU_LINE_MAX || l->fd < 0) {
    return -1;
  }
  // one write: the drive loses bytes of a line split across USB packets
  memcpy(buf, line, n);
  buf[n] = '\n';
  size_t off = 0;
  int again   = 1;
  while(off < n + 1) {
    ssize_t w = write(l->fd, buf + off, n + 1 - off);
    if(w < 0) {
      if(errno == EINTR || errno == EAGAIN) {
        continue;
      }
      if(again-- > 0 && reconnect(l) == 0) {  // the whole line again
        off = 0;
        continue;
      }
      return -1;
    }
    off += (size_t)w;
  }
  return 0;
}

int su_link_pump(su_link_t *l, double sec) {
  int text   = 0;
  double end = now() + sec;
  if(l->fd < 0) {
    return -1;
  }
  for(;;) {
    double left = end - now();
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(l->fd, &rfds);
    struct timeval tv = {0, 0};
    if(left > 0) {
      tv.tv_sec  = (long)left;
      tv.tv_usec = (long)((left - (long)left) * 1e6);
    }
    int r = select(l->fd + 1, &rfds, NULL, NULL, &tv);
    if(r < 0) {
      if(errno == EINTR) {
        continue;
      }
      return -1;
    }
    if(r == 0) {
      return text;
    }
    unsigned char b[1024];
    ssize_t n = read(l->fd, b, sizeof(b));
    if(n < 0) {
      if(errno == EINTR || errno == EAGAIN) {
        continue;
      }
      if(errno == EIO || errno == ENODEV || errno == ENXIO) {
        return reconnect(l) == 0 ? text : -1;
      }
      return -1;
    }
    if(n == 0) {
      return reconnect(l) == 0 ? text : -1;  // gone (USB re-enumerated, drive reset)
    }
    text += demux(l, b, (int)n);
  }
}

const char *su_link_text(const su_link_t *l) {
  return l->buf ? l->buf : "";
}

void su_link_clear(su_link_t *l) {
  l->len = 0;
  if(l->buf) {
    l->buf[0] = '\0';
  }
}

const char *su_link_query(su_link_t *l, const char *cmd, double quiet, const char *done, double timeout) {
  su_link_clear(l);
  if(su_link_send(l, cmd) < 0) {
    return NULL;
  }
  double end = now() + timeout;
  int got    = 0;
  while(now() < end) {
    int t = su_link_pump(l, done ? 0.05 : quiet);
    if(t < 0) {
      return NULL;
    }
    got += t;
    if(done && strstr(su_link_text(l), done)) {
      break;
    }
    if(!done && got && t == 0) {
      break;
    }
  }
  return su_link_text(l);
}

int su_parse_pin(const char *text, const char *pin, double *v) {
  size_t pl = strlen(pin);
  int found = 0;
  for(const char *p = text; p && *p;) {
    const char *line = p;
    const char *e    = strchr(p, '\n');
    p                = e ? e + 1 : NULL;
    if(strncmp(line, "OK ", 3) == 0) {
      line += 3;
    }
    if(strncmp(line, pin, pl) != 0) {
      continue;
    }
    const char *q = line + pl;
    if(strncmp(q, " <= ", 4) == 0) {  // linked: "pin <= src = v"
      q = strstr(q, " = ");
      if(!q || (e && q > e)) {
        continue;
      }
    } else if(strncmp(q, " = ", 3) != 0) {
      continue;  // another pin with this prefix
    }
    double d;
    if(sscanf(q + 3, "%lf", &d) == 1) {
      *v    = d;
      found = 1;
    }
  }
  return found ? 0 : -1;
}

// the pin's answer line is complete: "pin = v\n" (or "pin <= src = v\n")
static int pin_line_done(const char *text, const char *pin) {
  size_t pl = strlen(pin);
  for(const char *p = text; p && *p;) {
    const char *e = strchr(p, '\n');
    if(!e) {
      return 0;  // the last line, still coming
    }
    if(strncmp(p, pin, pl) == 0 && (strncmp(p + pl, " = ", 3) == 0 || strncmp(p + pl, " <= ", 4) == 0)) {
      return 1;
    }
    p = e + 1;
  }
  return 0;
}

int su_link_get(su_link_t *l, const char *pin, double *v) {
  char cmd[SU_LINE_MAX + 1];
  snprintf(cmd, sizeof(cmd), "getf %s", pin);  // full precision (setup api 1)
  // Read until the pin's own line is complete. The first newline isn't
  // enough: on the smart-serial console (~1 kB/s) the last answer's tail
  // still comes in after the command, and every try failed on it (the SEW
  // bench's brake step, DEBUGNOTES 2026-10-29).
  for(int attempt = 0; attempt < 3; attempt++) {
    su_link_clear(l);
    if(su_link_send(l, cmd) < 0) {
      return -1;
    }
    double end = now() + 2.0;
    while(now() < end) {
      if(su_link_pump(l, 0.05) < 0) {
        return -1;
      }
      if(pin_line_done(su_link_text(l), pin) && su_parse_pin(su_link_text(l), pin, v) == 0) {
        return 0;
      }
    }
  }
  return -1;
}

int su_link_set(su_link_t *l, const char *pin, const char *value) {
  char line[SU_LINE_MAX + 1];
  if(snprintf(line, sizeof(line), "%s = %s", pin, value) > SU_LINE_MAX) {
    return -1;
  }
  char ok[SU_LINE_MAX + 8];
  snprintf(ok, sizeof(ok), "OK %s", pin);
  const char *t = su_link_query(l, line, 0.15, ok, 2.0);
  return t && strstr(t, ok) ? 0 : -1;
}

int su_link_set_num(su_link_t *l, const char *pin, double v) {
  char val[32];
  snprintf(val, sizeof(val), "%.6g", v);
  return su_link_set(l, pin, val);
}

int su_link_reset(su_link_t *l, double timeout) {
  su_link_send(l, "reset");
  su_link_pump(l, 0.3);
  close(l->fd);
  l->fd = -1;
  double end = now() + timeout;
  while(now() < end) {
    struct timespec ts = {0, 300000000};
    nanosleep(&ts, NULL);
    int fd = open_fd(l->dev);
    if(fd < 0) {
      continue;  // not enumerated yet
    }
    l->fd       = fd;
    l->in_frame = 0;
    su_link_pump(l, 0.3);
    if(write(fd, "\n", 1) != 1) {
      close(fd);
      l->fd = -1;
      continue;
    }
    const char *t = su_link_query(l, "about", 0.3, NULL, 2.0);
    if(t && strstr(t, "setup api")) {
      su_link_clear(l);
      return 0;
    }
    close(fd);
    l->fd = -1;
  }
  return -1;
}
