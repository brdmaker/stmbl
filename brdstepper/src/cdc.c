/*
 * USB CDC for term (shared/comps/term.c): not yet. The board has USB
 * (PA11 / PA12) and the console will run there too; until then term reads
 * the smart-serial console only.
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
