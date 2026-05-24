/*
 * Host implementation of the firmware CDC (USB serial) interface.
 *
 * On the STM32 these functions move bytes over the USB virtual COM port. On
 * the host the same byte stream is the process's stdout/stdin, so the 'term'
 * component speaks the exact same wire protocol (text responses interleaved
 * with 0xFF scope frames) to whatever is connected to the pipe.
 */
#include "usbd_cdc_if.h"
#include <stdio.h>

static int connected = 0;

void cdc_host_set_connected(int c) {
  connected = c;
}

int cdc_is_connected(void) {
  return connected;
}

// stdout is set unbuffered by the serve loop, so writes (text from printf and
// binary scope frames from here) reach the client in order, immediately.
int cdc_tx(void *data, uint32_t len) {
  return (int)fwrite(data, 1, len, stdout);
}

// Commands are fed to hal_parse directly by the serve loop, so term never
// needs to pull them from here.
int cdc_getline(char *ptr, int len) {
  (void)ptr;
  (void)len;
  return 0;
}
