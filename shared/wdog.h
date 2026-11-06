#pragma once
/*
 * The F4's watchdog, reset cause and alive LED: the logic, kept here so the
 * host tests it (host/tests/test_wdog.c). The F4 side is src/wdog.c.
 *
 * A hang used to leave the drive on USB with the console dead until a power
 * cycle, and the power cycle wiped every trace of it (SEW bench,
 * 2026-10-29). Now the independent watchdog resets a hung main loop or a
 * stuck interrupt, the crash record says it was the watchdog and what ran
 * (rt/frt/nrt function, console command), and the state LED breathes only
 * while both rt and the main loop run.
 */
#include <stdint.h>

// RCC_CSR reset flags (STM32F4, RM0090 7.3.21)
#define WDOG_CSR_BOR (1u << 25)
#define WDOG_CSR_PIN (1u << 26)
#define WDOG_CSR_POR (1u << 27)
#define WDOG_CSR_SFT (1u << 28)
#define WDOG_CSR_IWDG (1u << 29)
#define WDOG_CSR_WWDG (1u << 30)
#define WDOG_CSR_LPWR (1u << 31)

typedef enum {
  RST_UNKNOWN,
  RST_POWER_ON,
  RST_BROWNOUT,
  RST_PIN,
  RST_SOFTWARE,
  RST_WATCHDOG,
  RST_WWDG,
  RST_LOW_POWER,
} wdog_reset_t;

static const char *const wdog_reset_names[] = {"unknown", "power-on", "brown-out", "reset pin", "software",
                                               "watchdog (hang)", "window watchdog", "low-power"};

// Every reset also sets PINRSTF (the core drives NRST) and a power-on also
// sets BORRSTF, so the most specific flag wins. No flag at all: the stmbl
// bootloader clears them at a power-on or brown-out (bootloader/src/main.c,
// its DFU request in CCM), so that's a power-on.
static inline wdog_reset_t wdog_reset_cause(uint32_t csr) {
  if(!(csr & (WDOG_CSR_BOR | WDOG_CSR_PIN | WDOG_CSR_POR | WDOG_CSR_SFT | WDOG_CSR_IWDG | WDOG_CSR_WWDG | WDOG_CSR_LPWR))) {
    return RST_POWER_ON;
  }
  if(csr & WDOG_CSR_IWDG) {
    return RST_WATCHDOG;
  }
  if(csr & WDOG_CSR_WWDG) {
    return RST_WWDG;
  }
  if(csr & WDOG_CSR_LPWR) {
    return RST_LOW_POWER;
  }
  if(csr & WDOG_CSR_SFT) {
    return RST_SOFTWARE;
  }
  if(csr & WDOG_CSR_POR) {
    return RST_POWER_ON;
  }
  if(csr & WDOG_CSR_BOR) {
    return RST_BROWNOUT;
  }
  if(csr & WDOG_CSR_PIN) {
    return RST_PIN;
  }
  return RST_UNKNOWN;
}

// What ran last, written by the rt and frt interrupts into CCM (kept over
// a reset): after a watchdog reset no handler runs, so this is all there is.
typedef struct {
  uint32_t magic;
  int32_t rt_func, frt_func, nrt_func;  // hal.active_*_func, -1 idle
  int32_t cmd;                          // console command running, -1 none
  uint32_t uptime_ms;
} wdog_crumb_t;

#define WDOG_CRUMB_MAGIC 0x57444f47  // "WDOG"

// Feed only when time moves on and rt has run since the last feed, or rt is
// stopped on purpose (flashsaveconf, crash safe mode, no config). A hung
// main loop doesn't feed; a stuck interrupt stops systime or rt.
typedef struct {
  uint32_t rt_count;
  uint32_t ms;
} wdog_feed_t;

static inline int wdog_may_feed(wdog_feed_t *f, uint32_t rt_count, int rt_stopped, uint32_t ms) {
  if(ms == f->ms || (rt_count == f->rt_count && !rt_stopped)) {
    return 0;
  }
  f->ms       = ms;
  f->rt_count = rt_count;
  return 1;
}

// The state LED breathes: rt does the PWM (WDOG_PWM_STEPS ticks, 156 Hz at
// 5 kHz) and the brightness follows the main loop's clock (alive_ms). Both
// running: it breathes. Main loop hung: it stays at one brightness. rt
// hung: it freezes on or off.
#define WDOG_BREATHE_MS 2000
#define WDOG_PWM_STEPS 32
#define WDOG_PWM_MIN 2  // never quite dark: a lit LED still says "powered"

static inline int wdog_breathe(uint32_t alive_ms, uint32_t rt_tick) {
  uint32_t half  = WDOG_BREATHE_MS / 2;
  uint32_t t     = alive_ms % WDOG_BREATHE_MS;
  uint32_t tri   = t < half ? t : WDOG_BREATHE_MS - t;  // 0..half
  uint32_t level = WDOG_PWM_MIN + (WDOG_PWM_STEPS - WDOG_PWM_MIN) * tri * tri / (half * half);
  return rt_tick % WDOG_PWM_STEPS < level;
}
