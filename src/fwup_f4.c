/*
 * The fwup component's platform on the F4 (see shared/fwup.h): staging in
 * flash sectors 8..11, the application in 4..7 (the bootloader's layout).
 *
 * One flash bank: a word program stalls every instruction fetch, the
 * interrupts' too, for ~16 us (100 us max). fwup programs only while the
 * drive is disabled, and hal.flash_grace lets the rt / frt interrupts run
 * late for the next two rt periods instead of stopping the HAL.
 */
#include "fwup.h"
#include "hal.h"
#include "stm32f4xx_conf.h"

#define STAGE_START 0x08080000

const uint32_t fwup_stage_size = 0x80000;
const uint32_t fwup_max_size   = 0x70000;

int fwup_stage_program(uint32_t off, const uint8_t *d, uint32_t len) {
  if(len != 4 || (off & 3) || off + len > fwup_stage_size) {
    return -1;
  }
  uint32_t w = d[0] | d[1] << 8 | d[2] << 16 | (uint32_t)d[3] << 24;
  hal.flash_grace = 2;
  FLASH_Unlock();
  FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR |
                  FLASH_FLAG_PGSERR);
  int r = FLASH_ProgramWord(STAGE_START + off, w) != FLASH_COMPLETE;
  FLASH_Lock();
  return r || *(volatile uint32_t *)(STAGE_START + off) != w ? -1 : 0;
}

const uint8_t *fwup_stage_read(uint32_t off) {
  return (const uint8_t *)(STAGE_START + off);
}

void fwup_reset(uint32_t trigger) {
  hal_stop();
  *((volatile uint32_t *)0x2001C000) = trigger;  // read by the bootloader
  NVIC_SystemReset();
}
