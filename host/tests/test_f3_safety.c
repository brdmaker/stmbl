/*
 * shared/f3_safety.h: the F3's bounded ADC wait and its watchdog feed
 * (DEBUGNOTES 2026-11-11, A4). Prints TAP.
 */
#include "f3_safety.h"
#include <stdio.h>

static int n, fails;
static void ok(int c, const char *name) {
  n++;
  fails += !c;
  printf("%s %d - f3_safety: %s\n", c ? "ok" : "not ok", n, name);
}

int main(void) {
  volatile uint32_t isr1 = 0, isr2 = 0;
  ok(f3_wait_flags(&isr1, 2, &isr2, 1 << 17, F3_ADC_WAIT_POLLS) == 0, "DMA stalled: gives up, doesn't hang");
  isr1 = 2;
  ok(f3_wait_flags(&isr1, 2, &isr2, 1 << 17, F3_ADC_WAIT_POLLS) == 0, "one of the two done: still waits for both");
  isr2 = 1 << 17;
  ok(f3_wait_flags(&isr1, 2, &isr2, 1 << 17, F3_ADC_WAIT_POLLS) == 1, "both done");
  ok(F3_ADC_WAIT_POLLS * 2 / 72.0 * 4 < 66.7, "the wait stays under one rt period (~4 cycles a read)");

  wdog_feed_t f = {0};
  uint32_t rt = 0;
  int fed = 0;
  for(uint32_t ms = 1; ms <= 100; ms++) {
    rt += 15;
    fed += f3_wdog_may_feed(&f, rt, 0, 1, ms);
  }
  ok(fed == 100, "rt running: fed every ms");
  fed = 0;
  for(uint32_t ms = 101; ms <= 200; ms++) {
    fed += f3_wdog_may_feed(&f, rt, 0, 1, ms);
  }
  ok(fed == 0, "rt hung (no ticks): not fed, the watchdog resets");
  fed = 0;
  for(uint32_t ms = 201; ms <= 300; ms++) {
    fed += f3_wdog_may_feed(&f, rt, 1, 1, ms);
  }
  ok(fed == 100, "rt stopped by the console: fed, the console stays");
  fed = 0;
  for(uint32_t ms = 301; ms <= 400; ms++) {
    fed += f3_wdog_may_feed(&f, rt, 1, 0, ms);
  }
  ok(fed == 0, "rt stopped by an error (overrun, ADC timeout): not fed, reset");
  fed = 0;
  for(int i = 0; i < 100; i++) {
    fed += f3_wdog_may_feed(&f, rt += 15, 0, 1, 400);
  }
  ok(fed == 1, "main loop hung (time stands still): fed once at most, then the watchdog resets");
  ok(F3_WDOG_RLR * 0.1 >= 10 && F3_WDOG_RLR <= 4095, "timeout 10..409 ms, in RLR's range");
  printf("1..%d\n", n);
  return fails != 0;
}
