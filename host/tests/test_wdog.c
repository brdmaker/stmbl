/*
 * shared/wdog.h: reset cause, watchdog feeding and the breathing LED.
 * Prints TAP.
 */
#include "wdog.h"
#include <stdio.h>
#include <string.h>

static int n, fails;

static void ok(int c, const char *name) {
  n++;
  fails += !c;
  printf("%s %d - wdog: %s\n", c ? "ok" : "not ok", n, name);
}

static void reset_cause(void) {
  // what the F4 shows after each kind of reset (PINRSTF comes with all)
  ok(wdog_reset_cause(WDOG_CSR_POR | WDOG_CSR_BOR | WDOG_CSR_PIN) == RST_POWER_ON, "power-on, not brown-out or pin");
  ok(wdog_reset_cause(WDOG_CSR_IWDG | WDOG_CSR_PIN) == RST_WATCHDOG, "watchdog");
  ok(wdog_reset_cause(WDOG_CSR_SFT | WDOG_CSR_PIN) == RST_SOFTWARE, "software reset (reset, crash)");
  ok(wdog_reset_cause(WDOG_CSR_PIN) == RST_PIN, "reset pin alone");
  ok(wdog_reset_cause(WDOG_CSR_BOR | WDOG_CSR_PIN) == RST_BROWNOUT, "brown-out");
  ok(wdog_reset_cause(0) == RST_POWER_ON, "no flag: the bootloader cleared them at power-on");
  ok(wdog_reset_cause(0x3) == RST_POWER_ON, "LSION, LSIRDY aren't reset flags");
  ok(!strcmp(wdog_reset_names[RST_WATCHDOG], "watchdog (hang)"), "names line up with the enum");
  ok(sizeof(wdog_reset_names) / sizeof(wdog_reset_names[0]) == RST_LOW_POWER + 1, "a name for every cause");
}

static void feeding(void) {
  wdog_feed_t f = {0};
  uint32_t rt = 0, fed = 0;
  // running: rt 5 ticks a ms, the main loop polls every ms
  for(uint32_t ms = 1; ms <= 100; ms++) {
    rt += 5;
    fed += wdog_may_feed(&f, rt, 0, ms);
  }
  ok(fed == 100, "rt and main loop running: fed every ms");

  // polled twice within a ms: once
  ok(!wdog_may_feed(&f, rt + 5, 0, 100), "not twice in one ms");

  // rt stuck (its counter stands), time goes on: no feed
  fed = 0;
  for(uint32_t ms = 101; ms <= 200; ms++) {
    fed += wdog_may_feed(&f, rt, 0, ms);
  }
  ok(fed == 0, "rt stuck: never fed");

  // rt stopped on purpose (flashsaveconf, safe mode): fed
  fed = 0;
  for(uint32_t ms = 201; ms <= 300; ms++) {
    fed += wdog_may_feed(&f, rt, 1, ms);
  }
  ok(fed == 100, "rt stopped on purpose: fed");

  // systime stuck (interrupts off): no feed, even with rt stopped
  ok(!wdog_may_feed(&f, rt, 1, 300) && !wdog_may_feed(&f, rt + 5, 0, 300), "systime stuck: never fed");
}

static int duty(uint32_t alive_ms) {
  int on = 0;
  for(uint32_t t = 0; t < WDOG_PWM_STEPS; t++) {
    on += wdog_breathe(alive_ms, t);
  }
  return on;
}

static void breathing(void) {
  ok(duty(0) == WDOG_PWM_MIN, "dim at the start of a breath");
  ok(duty(WDOG_BREATHE_MS / 2) == WDOG_PWM_STEPS, "full at the middle");
  ok(duty(WDOG_BREATHE_MS) == WDOG_PWM_MIN, "dim again after a breath");
  int up = 1;
  for(uint32_t ms = 0; ms < WDOG_BREATHE_MS / 2; ms += 10) {
    up &= duty(ms + 10) >= duty(ms);
  }
  ok(up, "brightens steadily over the first half");
  // main loop hung: alive_ms stands, so the brightness stands
  int same = 1;
  for(int k = 0; k < 10; k++) {
    same &= duty(700) == duty(700);
  }
  ok(same && duty(700) > WDOG_PWM_MIN && duty(700) < WDOG_PWM_STEPS, "frozen clock: one steady brightness");
  // never dark while running: a lit LED still says "powered"
  int lit = 1;
  for(uint32_t ms = 0; ms < 3 * WDOG_BREATHE_MS; ms++) {
    lit &= duty(ms) >= WDOG_PWM_MIN;
  }
  ok(lit, "never fully dark");
}

int main(void) {
  reset_cause();
  feeding();
  breathing();
  printf("1..%d\n", n);
  if(fails) {
    printf("# %d of %d wdog tests failed\n", fails, n);
  }
  return fails != 0;
}
