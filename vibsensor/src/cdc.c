/*
 * USB CDC: not yet. The board has USB (PA11 / PA12) for development; until
 * then the console is on the measurement bus only (vnode, src/syscalls.c).
 */
#include "usbd_cdc_if.h"

int cdc_tx(void *data, uint32_t len) {
  (void)data;
  (void)len;
  return 0;
}
int cdc_getline(char *ptr, int len) {
  (void)ptr;
  (void)len;
  return 0;
}
int cdc_is_connected(void) {
  return 0;
}
