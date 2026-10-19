/*
 * The fwup component's platform on brdstepper (shared/fwup.h): staging in
 * flash bank 2. The code runs from bank 1, so programming does not stall
 * it: updates are staged while the drive runs (fwup_single_bank = 0).
 * The staging pages are erased by the bootloader (fwst_boot), as on the F4.
 */
#include "fwup.h"
#include "flash.h"
#include "brd.h"

const uint32_t fwup_stage_size = FLASH_STAGE_SIZE;
const uint32_t fwup_max_size   = FLASH_STAGE_SIZE - 24;
const uint32_t fwup_prog_size  = 8;
const int fwup_single_bank     = 0;

int fwup_stage_program(uint32_t off, const uint8_t *d, uint32_t len) {
  if(len != 8 || off + len > FLASH_STAGE_SIZE) {
    return -1;
  }
  return flash_program(FLASH_STAGE + off, d, 8);
}

const uint8_t *fwup_stage_read(uint32_t off) {
  return (const uint8_t *)(FLASH_STAGE + off);
}

void fwup_reset(uint32_t trigger) {
  hw_bridges(0);
  *(volatile uint32_t *)0x2001FFF0 = trigger;  // read by the bootloader (the top of RAM, below the stack: check the boot ld)
  NVIC_SystemReset();
}
