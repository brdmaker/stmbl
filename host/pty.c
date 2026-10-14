/*
 * The simulated drive's USB serial port: a pty whose slave end is symlinked
 * to a fixed path (e.g. /tmp/stmbl-sim), so servoterm-cli or QtServoterm can
 * open it like /dev/ttyACM0. A background thread moves bytes between the
 * pty and the CDC FIFOs in host/cdc_host.c, and writes the simulated flash
 * back to its file after every flashsaveconf. The drive itself never blocks
 * on any of this, so it can run in a realtime thread.
 *
 * A client counts as connected (the drive's DTR check) while it has the
 * slave end open: with no slave open, the master reports POLLHUP.
 */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "usbd_cdc_if.h"
#include "drive.h"
#include "pty.h"

static int master = -1;
static char link_path[256];
static char flash_path[256];
static pthread_t thread;
static volatile int running = 0;

static void save_flash(void) {
  char tmp[300];
  snprintf(tmp, sizeof(tmp), "%s.tmp", flash_path);
  FILE *f = fopen(tmp, "w");
  if(!f) {
    perror(tmp);
    return;
  }
  fputs(drive_flash(), f);
  if(fclose(f) == 0) {
    rename(tmp, flash_path);
  }
}

static void *io_thread(void *arg) {
  (void)arg;
  unsigned flash_version = drive_flash_version();
  uint8_t out[4096];
  uint32_t out_len = 0, out_pos = 0;

  while(running) {
    if(flash_path[0] && drive_flash_version() != flash_version) {
      flash_version = drive_flash_version();
      save_flash();
    }
    if(master < 0) {
      usleep(10000);
      continue;
    }

    struct pollfd p = {.fd = master, .events = POLLIN | (out_len > out_pos ? POLLOUT : 0)};
    poll(&p, 1, 2);

    if(p.revents & POLLHUP) {  // no client has the port open
      cdc_host_set_connected(0);
      while(cdc_host_read_tx(out, sizeof(out)) > 0) {
      }
      out_len = out_pos = 0;
      usleep(20000);
      continue;
    }
    cdc_host_set_connected(1);

    if(p.revents & POLLIN) {
      uint8_t in[512];
      ssize_t n = read(master, in, sizeof(in));
      if(n > 0) {
        // the drive's USB stack has a 512-byte queue; like a client that
        // writes faster than the drive reads, the excess is lost
        cdc_host_write_rx(in, n);
      }
    }

    if(out_pos >= out_len) {
      out_len = cdc_host_read_tx(out, sizeof(out));
      out_pos = 0;
    }
    if(out_len > out_pos) {
      ssize_t n = write(master, out + out_pos, out_len - out_pos);
      if(n > 0) {
        out_pos += n;
      }
    }
  }
  return NULL;
}

int sim_io_start(const char *pty_link, const char *flash_file) {
  cdc_host_set_mode(CDC_FIFO);
  cdc_host_set_connected(0);
  flash_path[0] = 0;
  if(flash_file) {
    snprintf(flash_path, sizeof(flash_path), "%s", flash_file);
  }

  if(pty_link) {
    master = posix_openpt(O_RDWR | O_NOCTTY);
    if(master < 0 || grantpt(master) < 0 || unlockpt(master) < 0) {
      perror("posix_openpt");
      return -1;
    }
    fcntl(master, F_SETFL, O_NONBLOCK);
    const char *slave = ptsname(master);

    // raw, so scope bytes and text pass unchanged, as on the USB port
    int s = open(slave, O_RDWR | O_NOCTTY);
    if(s >= 0) {
      struct termios t;
      tcgetattr(s, &t);
      cfmakeraw(&t);
      tcsetattr(s, TCSANOW, &t);
      close(s);
    }

    snprintf(link_path, sizeof(link_path), "%s", pty_link);
    unlink(link_path);
    if(symlink(slave, link_path) < 0) {
      perror(link_path);
      return -1;
    }
  }

  running = 1;
  if(pthread_create(&thread, NULL, io_thread, NULL) != 0) {
    running = 0;
    return -1;
  }
  return 0;
}

void sim_io_stop(void) {
  if(running) {
    running = 0;
    pthread_join(thread, NULL);
  }
  if(link_path[0]) {
    unlink(link_path);
    link_path[0] = 0;
  }
  if(master >= 0) {
    close(master);
    master = -1;
  }
}

char *sim_read_file(const char *path) {
  FILE *f = fopen(path, "r");
  if(!f) {
    return NULL;
  }
  size_t cap = 4096, len = 0;
  char *buf = malloc(cap);
  size_t n;
  while(buf && (n = fread(buf + len, 1, cap - len - 1, f)) > 0) {
    len += n;
    if(cap - len < 2) {
      cap *= 2;
      buf = realloc(buf, cap);
    }
  }
  fclose(f);
  if(buf) {
    buf[len] = 0;
  }
  return buf;
}
