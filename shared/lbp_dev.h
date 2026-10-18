/*
 * A smart-serial (Mesa LBP) device, byte by byte: for a bootloader on a
 * polled UART at 115200 baud. Answers what hostmot2 needs to find it (name,
 * unit number, discovery, its tables: here empty, no process data) and
 * hands the flash traffic of `setsserial flash` to lbp_flash. Request and
 * reply formats as the application's parser (src/comps/sserial.c); every
 * request with a CRC8 is checked (a bad one gets no reply), every reply
 * that carries one gets it.
 */
#pragma once
#include <stdint.h>
#include "lbp_flash.h"

typedef struct {
  // configuration
  const uint8_t *mem;  // the tables: discovery at 0 (input, output, ptocp, gtocp)
  uint32_t mem_size;
  char name[4];        // card name ("stbl")
  uint32_t unit;       // unit (serial) number
  lbpf_t *flash;       // NULL: no flash mode
  void (*send)(void *ctx, const uint8_t *d, int n);
  void *ctx;
  // state
  uint8_t buf[48];
  int len;
  uint16_t addr;
  uint32_t cmds, crc_errors, good;  // good: requests with a right CRC
} lbpd_t;

// the bootloader's tables: no process data (1 input byte: the fault byte)
extern const uint8_t lbpd_empty_mem[10];

void lbpd_init(lbpd_t *d);
void lbpd_byte(lbpd_t *d, uint8_t b);  // a received byte
void lbpd_idle(lbpd_t *d);             // the line went quiet: drop a partial request
