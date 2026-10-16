/*
 * The platform under the fwup component (shared/comps/fwup.c): the staging
 * area and the reset. F4: src/fwup_f4.c; host simulator: host/fwup_host.c.
 */
#pragma once
#include <stdint.h>

#define FWUP_TRIGGER_NONE 0xCAFEFEED     // a plain reset (the bootloader applies a pending update)
#define FWUP_TRIGGER_SSERIAL 0x5353424C  // "SSBL": stay in the smart-serial loader
#define FWUP_LINE 30                     // image bytes per fwdata line: 60 console characters

extern const uint32_t fwup_stage_size;  // bytes, the 16 byte header at the end included
extern const uint32_t fwup_max_size;    // the application area
// a word (4 bytes, aligned) into the staging area; 0 ok
int fwup_stage_program(uint32_t off, const uint8_t *d, uint32_t len);
const uint8_t *fwup_stage_read(uint32_t off);
void fwup_reset(uint32_t trigger);
