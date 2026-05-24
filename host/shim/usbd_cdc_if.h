/*
 * Host shim for inc/usbd_cdc_if.h.
 *
 * Placed first on the include path so the 'term' component compiles on the
 * host without pulling in the STM32 USB device stack. Only the three CDC
 * entry points term.c actually uses are declared here; host/cdc_host.c maps
 * them onto the process's stdin/stdout.
 */
#pragma once
#include <stdint.h>

int cdc_tx(void *data, uint32_t len);
int cdc_getline(char *ptr, int len);
int cdc_is_connected(void);

void cdc_host_set_connected(int connected);
