// Bootloader and hv_update decisions, kept apart from the registers so the
// host tests them (host/tests/test_boot_logic.c). From comparing with the
// stmbl_freak fork (DEBUGNOTES 2026-11-11), for two open entries:
// "After make btburn the drive came back in DFU at every start" and
// "hv_update on a v207 HV board: an erase race?".
#pragma once
#include <stdint.h>

// F4 bootloader: PA13 (SWDIO, pulled up) shorted to GND at power-up asks
// for the ROM's DFU. It was read once, right after its pull-up was turned
// on: a pin still charging (or SWD traffic) read low, and the drive went to
// DFU. Now low on every one of n samples (~10 ms apart in all), and never
// while a debugger is attached (its resets drive SWDIO).
static inline int boot_pin_wants_dfu(int (*pin_low)(void *), void *ctx, void (*pause)(void), int debugger,
                                     int samples) {
  if(debugger) {
    return 0;
  }
  for(int i = 0; i < samples; i++) {
    if(!pin_low(ctx)) {
      return 0;
    }
    pause();
  }
  return samples > 0;
}

// F3 bootloader: erase [start, end) a page at a time, skipping pages that
// are already blank. hv0 repeats the erase command until it sees the
// answer; every one the bootloader took in erased all 56 pages again
// (1-2 s), and the writes that followed timed out. A repeat now answers in
// a few ms. Returns the pages erased, or -1 when an erase failed.
static inline int boot_erase_pages(uint32_t start, uint32_t end, uint32_t page, const uint32_t *(*at)(uint32_t),
                                   int (*erase)(uint32_t)) {
  int n = 0;
  for(uint32_t a = start; a < end; a += page) {
    const uint32_t *p = at(a);
    uint32_t blank    = 0xffffffffu;
    for(uint32_t i = 0; i < page / 4; i++) {
      blank &= p[i];
    }
    if(blank == 0xffffffffu) {
      continue;
    }
    if(erase(a)) {
      return -1;
    }
    n++;
  }
  return n;
}

// hv_update's CRC_CHECK: the F3 bootloader computes the app's CRC in its
// timer interrupt (2-3 ms) and answers only then; a check sent every rt
// tick re-arms the F4's receive each time and can lose that answer every
// time. Send it, then listen BOOT_CRC_LISTEN ticks. Returns 1: send now.
#define BOOT_CRC_LISTEN 100
static inline int boot_crc_send(uint32_t *wait) {
  if(*wait) {
    (*wait)--;
    return 0;
  }
  *wait = BOOT_CRC_LISTEN;
  return 1;
}

// hv_update's ERASE_FLASH: a full erase takes 1-2 s and a lost answer costs
// a round; 12 s at the rt rate (5 kHz) before FLASH_FAILED (was 20000, 4 s)
#define BOOT_ERASE_TICKS 60000
