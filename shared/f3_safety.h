// The F3's (HV board) safety logic that the host can test
// (host/tests/test_f3_safety.c); the F3 side is stm32f303/src/main.c and
// stm32f303/src/comps/io.c. From comparing with the stmbl_freak fork
// (DEBUGNOTES 2026-11-11):
//  - A4: io's rt waited for the ADC DMA without a limit; a stalled DMA hung
//    the rt interrupt with the bridge switching its last PWM. And the F3
//    had no watchdog at all.
#pragma once
#include <stdint.h>
#include "wdog.h"

// io's rt: both ADC DMA transfers. The sequence ends ~6 us after the PWM
// tick; F3_ADC_WAIT_POLLS (two register reads each, ~0.12 us at 72 MHz)
// is ~60 us, under the 66.7 us rt period. 1: both done, 0: gave up.
#define F3_ADC_WAIT_POLLS 500
static inline int f3_wait_flags(volatile const uint32_t *a, uint32_t am, volatile const uint32_t *b, uint32_t bm,
                                uint32_t max_polls) {
  for(uint32_t n = 0; n < max_polls; n++) {
    if((*a & am) && (*b & bm)) {
      return 1;
    }
  }
  return 0;
}

// The F3's independent watchdog: LSI 40 kHz / 4, 0.1 ms a count, 20 ms
// (16-27 ms over the LSI's spread). Fed by the main loop each ms while the
// rt runs (shared/wdog.h, wdog_may_feed), or while it is stopped by the
// console (`stop`, hal_state still HAL_OK2). Stopped by an error (overrun,
// ADC timeout, fault): no feed, and the F3 resets and comes back clean.
#define F3_WDOG_RLR 200
static inline int f3_wdog_may_feed(wdog_feed_t *f, uint32_t rt_count, int rt_stopped, int hal_ok, uint32_t ms) {
  return wdog_may_feed(f, rt_count, rt_stopped && hal_ok, ms);
}
