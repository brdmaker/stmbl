/*
 * sserialterm: the console of stmbl drives over smart-serial, as a
 * pseudo-terminal per drive (LinuxCNC uspace realtime module).
 *
 * The drive's console runs in its process data (shared/sscon.h): a 10-bit
 * field each way, con_cmd (to the drive) and con_fb (from it), raw u32 pins
 * in hostmot2. sserialterm is LinuxCNC's end of that link; a thread moves
 * the bytes to and from a pty, so servoterm-cli (or any terminal) talks to
 * the drive as over USB: no extra cable.
 *
 *   loadrt sserialterm tty=/tmp/drive-x,/tmp/drive-y
 *   addf sserialterm.0 servo-thread            (after hostmot2's read)
 *   net x-con-fb  hm2_7i96s.0.stbl.0.0.con_fb  => sserialterm.0.con-fb
 *   net x-con-cmd sserialterm.0.con-cmd        => hm2_7i96s.0.stbl.0.0.con_cmd
 *   servoterm-cli -d /tmp/drive-x
 *
 * Pins, per drive N (in the order of tty=):
 *   sserialterm.N.con-fb     u32 in    the drive's con_fb
 *   sserialterm.N.con-cmd    u32 out   to the drive's con_cmd
 *   sserialterm.N.bytes-in   u32 out   bytes from the drive
 *   sserialterm.N.bytes-out  u32 out   bytes to the drive
 *   sserialterm.N.active     bit out   the drive has answered
 *   sserialterm.N.client     bit out   a program has the pty open
 * One function, sserialterm.0, serves all of them.
 */
#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "sscon.h"

MODULE_AUTHOR("stmbl");
MODULE_DESCRIPTION("stmbl drive consoles over smart-serial, as ptys");
MODULE_LICENSE("GPL");

static char *tty = "";
RTAPI_MP_STRING(tty, "pty link paths, one per drive, comma separated");

#define MAXCON 16

typedef struct {
  hal_u32_t *fb, *cmd, *n_in, *n_out;
  hal_bit_t *active, *client;
} pins_t;

typedef struct {
  sscon_t con;
  char path[256];
  int master;
  volatile int client;
  uint8_t out[1024];  // drive -> pty, not yet written
  uint32_t out_len, out_pos;
} term_t;

static int comp_id, nterm;
static pins_t *pins;
static term_t *terms;
static pthread_t thread;
static volatile int running;

static void update(void *arg, long period) {
  (void)arg, (void)period;
  for(int i = 0; i < nterm; i++) {
    term_t *t = &terms[i];
    pins_t *p = &pins[i];
    *p->cmd = sscon_frame(&t->con, (uint16_t)(*p->fb & 0x3ff));
    *p->n_in = t->con.n_rx;
    *p->n_out = t->con.n_tx;
    *p->active = t->con.active;
    *p->client = t->client;
  }
}

static int open_pty(term_t *t) {
  int m = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
  if(m < 0 || grantpt(m) || unlockpt(m)) {
    return -1;
  }
  const char *slave = ptsname(m);
  // raw, no echo: a serial port
  int s = open(slave, O_RDWR | O_NOCTTY);
  if(s >= 0) {
    struct termios tio;
    if(tcgetattr(s, &tio) == 0) {
      cfmakeraw(&tio);
      tcsetattr(s, TCSANOW, &tio);
    }
    close(s);
  }
  unlink(t->path);
  if(symlink(slave, t->path)) {
    rtapi_print_msg(RTAPI_MSG_ERR, "sserialterm: cannot link %s -> %s: %s\n", t->path, slave, strerror(errno));
    close(m);
    return -1;
  }
  t->master = m;
  return 0;
}

static void *io_thread(void *arg) {
  (void)arg;
  while(running) {
    struct pollfd pf[MAXCON];
    for(int i = 0; i < nterm; i++) {
      term_t *t = &terms[i];
      pf[i].fd = t->master;
      pf[i].events = POLLIN | (t->out_len > t->out_pos ? POLLOUT : 0);
      pf[i].revents = 0;
    }
    poll(pf, nterm, 5);
    for(int i = 0; i < nterm; i++) {
      term_t *t = &terms[i];
      if(pf[i].revents & POLLHUP) {  // nobody has the pty open: what the drive says is dropped
        t->client = 0;
        uint8_t junk[256];
        while(sscon_read(&t->con, junk, sizeof(junk)) > 0) {
        }
        t->out_len = t->out_pos = 0;
        continue;
      }
      t->client = 1;
      if(pf[i].revents & POLLIN) {  // to the drive: only what its ring takes
        uint8_t in[256];
        uint32_t room = sscon_tx_free(&t->con);
        ssize_t n = room ? read(t->master, in, room < sizeof(in) ? room : sizeof(in)) : 0;
        if(n > 0) {
          sscon_write(&t->con, in, (uint32_t)n);
        }
      }
      if(t->out_pos == t->out_len) {
        t->out_len = sscon_read(&t->con, t->out, sizeof(t->out));
        t->out_pos = 0;
      }
      if(t->out_len > t->out_pos) {
        ssize_t n = write(t->master, t->out + t->out_pos, t->out_len - t->out_pos);
        if(n > 0) {
          t->out_pos += (uint32_t)n;
        }
      }
    }
  }
  return NULL;
}

int rtapi_app_main(void) {
  comp_id = hal_init("sserialterm");
  if(comp_id < 0) {
    return comp_id;
  }
  char list[1024];
  snprintf(list, sizeof(list), "%s", tty ? tty : "");
  terms = calloc(MAXCON, sizeof(term_t));
  pins = hal_malloc(MAXCON * sizeof(pins_t));
  if(!terms || !pins) {
    hal_exit(comp_id);
    return -ENOMEM;
  }
  for(char *s = strtok(list, ","); s && nterm < MAXCON; s = strtok(NULL, ",")) {
    term_t *t = &terms[nterm];
    snprintf(t->path, sizeof(t->path), "%s", s);
    if(open_pty(t)) {
      hal_exit(comp_id);
      return -EINVAL;
    }
    pins_t *p = &pins[nterm];
    int r = hal_pin_u32_newf(HAL_IN, &p->fb, comp_id, "sserialterm.%d.con-fb", nterm);
    r |= hal_pin_u32_newf(HAL_OUT, &p->cmd, comp_id, "sserialterm.%d.con-cmd", nterm);
    r |= hal_pin_u32_newf(HAL_OUT, &p->n_in, comp_id, "sserialterm.%d.bytes-in", nterm);
    r |= hal_pin_u32_newf(HAL_OUT, &p->n_out, comp_id, "sserialterm.%d.bytes-out", nterm);
    r |= hal_pin_bit_newf(HAL_OUT, &p->active, comp_id, "sserialterm.%d.active", nterm);
    r |= hal_pin_bit_newf(HAL_OUT, &p->client, comp_id, "sserialterm.%d.client", nterm);
    if(r) {
      hal_exit(comp_id);
      return -EINVAL;
    }
    nterm++;
  }
  if(nterm < 1) {
    rtapi_print_msg(RTAPI_MSG_ERR, "sserialterm: tty=<pty path>[,...] needed\n");
    hal_exit(comp_id);
    return -EINVAL;
  }
  if(hal_export_funct("sserialterm.0", update, NULL, 0, 0, comp_id)) {
    hal_exit(comp_id);
    return -EINVAL;
  }
  running = 1;
  if(pthread_create(&thread, NULL, io_thread, NULL)) {
    hal_exit(comp_id);
    return -EINVAL;
  }
  rtapi_print_msg(RTAPI_MSG_INFO, "sserialterm: %d consoles\n", nterm);
  hal_ready(comp_id);
  return 0;
}

void rtapi_app_exit(void) {
  running = 0;
  pthread_join(thread, NULL);
  for(int i = 0; i < nterm; i++) {
    close(terms[i].master);
    unlink(terms[i].path);
  }
  hal_exit(comp_id);
}
