/*
 * brdstepper flash: dual bank (DBANK = 1, the G473's factory default:
 * check the option bytes at first flash), 2 kB pages, programmed in 64-bit
 * double words, each once until erased (ECC).
 *
 *   bank 1  0x08000000  bootloader 16 kB, application 240 kB
 *   bank 2  0x08040000  update staging 240 kB, config 16 kB (0x0807C000)
 *
 * The code runs from bank 1, so erasing and programming bank 2 does not
 * stall it (read while write): config saves and update staging go on
 * while the drive runs.
 */
#include "brd.h"
#include "flash.h"

#define KEY1 0x45670123u
#define KEY2 0xCDEF89ABu
#define SR_ERRORS                                                                                                   \
  (FLASH_SR_PROGERR | FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_SIZERR | FLASH_SR_PGSERR | FLASH_SR_MISERR | \
   FLASH_SR_FASTERR | FLASH_SR_OPERR)

static void unlock(void) {
  if(FLASH->CR & FLASH_CR_LOCK) {
    FLASH->KEYR = KEY1;
    FLASH->KEYR = KEY2;
  }
}

static int wait(void) {
  while(FLASH->SR & FLASH_SR_BSY) {
  }
  int err   = (FLASH->SR & SR_ERRORS) != 0;
  FLASH->SR = SR_ERRORS | FLASH_SR_EOP;
  return err ? -1 : 0;
}

int flash_erase_page(uint32_t addr) {
  if(addr < FLASH_BANK2 || addr >= FLASH_END) {
    return -1;  // bank 2 only: never the running code
  }
  uint32_t page = (addr - FLASH_BANK2) / FLASH_PAGE;
  unlock();
  wait();
  FLASH->CR = FLASH_CR_PER | FLASH_CR_BKER | (page << FLASH_CR_PNB_Pos);
  FLASH->CR |= FLASH_CR_STRT;
  int r     = wait();
  FLASH->CR = FLASH_CR_LOCK;
  return r;
}

int flash_program(uint32_t addr, const void *data, uint32_t len) {
  if((addr & 7) || (len & 7) || addr < FLASH_BANK2 || addr + len > FLASH_END) {
    return -1;
  }
  const uint8_t *d = data;
  int r            = 0;
  unlock();
  wait();
  FLASH->CR = FLASH_CR_PG;
  for(uint32_t i = 0; i < len && !r; i += 8) {
    uint32_t lo, hi;
    __builtin_memcpy(&lo, d + i, 4);
    __builtin_memcpy(&hi, d + i + 4, 4);
    *(volatile uint32_t *)(addr + i)     = lo;  // first word, then the second: one double word
    *(volatile uint32_t *)(addr + i + 4) = hi;
    r                                    = wait();
    if(!r && (*(volatile uint32_t *)(addr + i) != lo || *(volatile uint32_t *)(addr + i + 4) != hi)) {
      r = -1;
    }
  }
  FLASH->CR = FLASH_CR_LOCK;
  return r;
}
