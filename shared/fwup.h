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
extern const uint32_t fwup_prog_size;   // the flash's programming unit: 4 (F4) or 8 (G4)
// 1: writing flash stalls the CPU (one bank): only while the drive is
// disabled; 0: the staging area is in the other bank (G4 dual bank)
extern const int fwup_single_bank;
// one unit (fwup_prog_size bytes, aligned) into the staging area; 0 ok
int fwup_stage_program(uint32_t off, const uint8_t *d, uint32_t len);
const uint8_t *fwup_stage_read(uint32_t off);
void fwup_reset(uint32_t trigger);
