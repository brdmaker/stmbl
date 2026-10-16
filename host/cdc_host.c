/*
 * Host implementation of the firmware CDC (USB serial) interface.
 *
 * On the STM32 these functions move bytes over the USB virtual COM port. On
 * the host the same byte stream goes to one of:
 *  - CDC_STDIO: the process's stdout (serve mode, driven by host/servoterm);
 *    commands are fed to hal_parse by the serve loop.
 *  - CDC_FIFO: a pair of FIFOs that another thread connects to a pty (the
 *    simulated drive). Commands come in through the same 512-byte ring
 *    buffer and rb_getline() as on the drive, so its line-length limit
 *    applies too.
 *  - CDC_NONE: not connected (script mode); printf goes to stdout.
 * printf from the firmware code is routed here by host_compat.h.
 */
#include "usbd_cdc_if.h"
#include "ringbuf.h"
#include "fifo.h"
#include "defines.h"
#include "sscon.h"
#include <stdarg.h>
#include <stdio.h>

static int mode = CDC_NONE;
static _Atomic int connected = 0;

static fifo_t to_host;    // drive -> pty
static fifo_t from_host;  // pty -> drive
static struct ringbuf usb_rx_buf = RINGBUF(512);

void cdc_host_set_mode(int m) {
  mode = m;
}

void cdc_host_set_connected(int c) {
  connected = c;
}

int cdc_is_connected(void) {
  return mode != CDC_NONE && connected;
}

int cdc_tx(void *data, uint32_t len) {
  switch(mode) {
    case CDC_STDIO:
      // stdout is unbuffered in serve mode, so text and scope frames stay in order
      return (int)fwrite(data, 1, len, stdout);
    case CDC_FIFO:
      return connected ? fifo_put(&to_host, data, len) : 0;
  }
  return 0;
}

int cdc_getline(char *ptr, int len) {
  if(mode != CDC_FIFO) {
    return 0;
  }
  char buf[64];
  uint32_t n;
  while(usb_rx_buf.len < usb_rx_buf.bufsize &&
        (n = fifo_get(&from_host, buf, MIN(sizeof(buf), (uint32_t)(usb_rx_buf.bufsize - usb_rx_buf.len)))) > 0) {
    rb_write(&usb_rx_buf, buf, n);
  }
  return rb_getline(&usb_rx_buf, ptr, len);
}

uint32_t cdc_host_read_tx(void *data, uint32_t len) {
  return fifo_get(&to_host, data, len);
}

uint32_t cdc_host_write_rx(const void *data, uint32_t len) {
  return fifo_put(&from_host, data, MIN(len, fifo_free(&from_host)));
}

int stmbl_printf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n;
  if(sscon0.active) {  // the console over smart-serial (shared/sscon.h), as _write on the drive
    static char cbuf[16 * 1024];
    va_list aq;
    va_copy(aq, ap);
    int m = vsnprintf(cbuf, sizeof(cbuf), fmt, aq);
    va_end(aq);
    if(m > 0) {
      sscon_write(&sscon0, cbuf, MIN(m, (int)sizeof(cbuf) - 1));
    }
  }
  if(mode == CDC_FIFO) {
    // big enough for showconf, which prints the whole 15 kB config flash;
    // only the drive's own thread calls this
    static char buf[16 * 1024];
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    if(n > 0) {
      cdc_tx(buf, MIN(n, (int)sizeof(buf) - 1));
    }
  } else {
    n = vprintf(fmt, ap);
  }
  va_end(ap);
  return n;
}
