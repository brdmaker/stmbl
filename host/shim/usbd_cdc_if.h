/*
 * Host shim for inc/usbd_cdc_if.h. Shadows the firmware header (host/shim is
 * first on the include path) so 'term' builds without the USB stack. The
 * implementation is host/cdc_host.c.
 */
#pragma once
#include <stdint.h>

int cdc_tx(void *data, uint32_t len);
int cdc_getline(char *ptr, int len);
int cdc_is_connected(void);

enum { CDC_NONE, CDC_STDIO, CDC_FIFO };
void cdc_host_set_mode(int mode);
void cdc_host_set_connected(int connected);

// CDC_FIFO: the other end, for the thread that serves the pty
uint32_t cdc_host_read_tx(void *data, uint32_t len);
uint32_t cdc_host_write_rx(const void *data, uint32_t len);
