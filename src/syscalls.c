#include <reent.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "stm32f4xx.h"
#include "usbd_cdc_if.h"
#include "sscon.h"

//int __errno;
size_t __malloc_margin = 256;
char *__brkval;

int _close(int file) {
  return 0;
}

int _fstat(int file, struct stat *st) {
  return 0;
}

int _isatty(int file) {
  return 1;
}

int _lseek(int file, int ptr, int dir) {
  return 0;
}

int _open(const char *name, int flags, int mode) {
  return -1;
}

int _kill_r(struct _reent *r, int pid, int sig){
  return -1;
}

int _getpid_r (struct _reent *r){
  return -1;
}

/* Register name faking - works in collusion with the linker.  */
register char *stack_ptr asm("sp");

void *_sbrk_r(struct _reent *r, ptrdiff_t incr) {
  extern char end;  // provided by the linker script

  if(__brkval == 0)
    __brkval = &end;

  if(__brkval + incr > (char *)__get_MSP() - __malloc_margin) {
    r->_errno = ENOMEM;
    return (void *)-1;
  }

  void *ret = __brkval;
  __brkval += incr;

  return ret;
}

ssize_t _read(int fd, void *ptr, size_t len) {
  (void)fd;
  while(!usb_rx_buf.len)
    ;

  if(len > usb_rx_buf.len)
    len = usb_rx_buf.len;

  char *c = (char *)ptr;
  for(uint16_t i = 0; i < len; i++)
    rb_getc(&usb_rx_buf, c++);

  return len;
}

extern volatile uint64_t systime;

// the console over smart-serial (shared/sscon.h), once LinuxCNC's side has
// spoken: in chunks with interrupts off (realtime code may print too); in
// the main loop, waits up to 100 ms for room (the link moves ~1 kB/s), so a
// long output (showconf) comes through whole but never hangs the drive
static void sscon_out(const char *ptr, int len) {
  if(!sscon0.active) {
    return;
  }
  uint64_t t0 = systime;
  while(len > 0) {
    int n = len > 64 ? 64 : len;
    if(sscon_tx_free(&sscon0) < (uint32_t)n && __get_IPSR() == 0 && systime - t0 < 100) {
      continue;
    }
    uint32_t pm = __get_PRIMASK();
    __disable_irq();
    sscon_write(&sscon0, ptr, n);  // what does not fit is dropped
    __set_PRIMASK(pm);
    ptr += n;
    len -= n;
    t0 = systime;
  }
}

int _write(int fd, const char *ptr, int len) {
  sscon_out(ptr, len);
  if(!USB_CDC_is_connected()) {
    return len;
  }
  char *c = (char *)ptr;
  (void)fd;
  int sent = 0;

  while(len--) {
    // send a queued byte - copy to usb stack buffer
    APP_Rx_Buffer[APP_Rx_ptr_in++] = *c;
    c++;

    // To avoid buffer overflow
    if(APP_Rx_ptr_in >= APP_RX_DATA_SIZE) {
      APP_Rx_ptr_in = 0;
    }
    sent++;
  }
  return sent;
}
