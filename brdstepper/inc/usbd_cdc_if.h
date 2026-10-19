#pragma once
#include <stdint.h>
// brdstepper/src/cdc.c (USB CDC: to do)
int cdc_tx(void *data, uint32_t len);
int cdc_getline(char *ptr, int len);
int cdc_is_connected(void);
