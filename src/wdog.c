/*
 * The F4's independent watchdog, the reset cause and the crumb of what ran
 * last; the logic is in shared/wdog.h (host/tests/test_wdog.c).
 *
 * IWDG on the LSI (32 kHz nominal, 17..47 kHz): /64, reload 1000 = 2 s
 * (1.4..3.8 s). That outlasts the config sector's erase (16 KB, 0.5 s max),
 * which blocks the main loop (flashsaveconf). Any reset stops the IWDG, so
 * the stmbl bootloader and ST's DFU run without it.
 *
 * The main loop feeds it (wdog_poll), and only while rt runs: a hung main
 * loop or a stuck interrupt resets the drive within 2 s, the F3 has already
 * stopped the motor (it disables after a few lost packets). At the next
 * boot the crash record says "Watchdog" with the crumb: the rt, frt and nrt
 * functions and the console command running when it was last written.
 */
#include <stdio.h>
#include <string.h>
#include "stm32f4xx.h"
#include "hal.h"
#include "commands.h"
#include "wdog.h"
#include "main.h"

#define crumb ((wdog_crumb_t *)0x1000FD80)  // CCM, kept over a reset (see stm32f4xx_it.c)

extern volatile uint64_t systime;
void crash_watchdog(const wdog_crumb_t *c);  // stm32f4xx_it.c

static wdog_reset_t cause = RST_UNKNOWN;
static uint32_t csr;
static wdog_feed_t feed;
static int running;
volatile uint32_t wdog_rt_count;  // rt interrupts, for the feed check and the LED's PWM
volatile uint32_t wdog_alive_ms;  // the main loop's clock, for the LED

// First thing in main(): read and clear the reset flags; a watchdog reset
// goes into the crash record (and counts toward its safe mode).
void wdog_boot(void) {
  csr   = RCC->CSR;
  cause = wdog_reset_cause(csr);
  RCC->CSR |= RCC_CSR_RMVF;
  if(cause == RST_WATCHDOG) {
    wdog_crumb_t none = {WDOG_CRUMB_MAGIC, -1, -1, -1, -1, 0};
    crash_watchdog(crumb->magic == WDOG_CRUMB_MAGIC ? crumb : &none);
  }
  crumb->rt_func = crumb->frt_func = crumb->nrt_func = crumb->cmd = -1;
  crumb->uptime_ms = 0;
  crumb->magic     = WDOG_CRUMB_MAGIC;
}

const char *wdog_reset_name(void) {
  return wdog_reset_names[cause];
}

uint32_t wdog_reset(uint32_t *flags) {  // the error log's boot entry
  *flags = csr;
  return cause;
}

void wdog_start(void) {
  DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_IWDG_STOP;  // halted by a debugger: stopped too
  IWDG->KR  = 0x5555;                               // PR, RLR writable
  IWDG->PR  = 4;                                    // LSI / 64
  IWDG->RLR = 1000 - 1;
  IWDG->KR  = 0xAAAA;
  IWDG->KR  = 0xCCCC;  // start; only a reset stops it
  running   = 1;
}

// The main loop (and Wait) call this: keeps the LED's clock and feeds.
void wdog_poll(void) {
  uint32_t ms   = (uint32_t)systime;
  wdog_alive_ms = ms;
  if(running && wdog_may_feed(&feed, wdog_rt_count, hal.rt_state == RT_STOP, ms)) {
    IWDG->KR = 0xAAAA;
  }
}

// rt and frt interrupts: a few stores into CCM
void wdog_crumb(void) {
  crumb->rt_func   = (int32_t)hal.active_rt_func;
  crumb->frt_func  = (int32_t)hal.active_frt_func;
  crumb->nrt_func  = (int32_t)hal.active_nrt_func;
  crumb->cmd       = cmd_active;
  crumb->uptime_ms = (uint32_t)systime;
}

void wdog_rt(void) {
  wdog_rt_count++;
  wdog_crumb();
}

static int drive_enabled(void) {
  NAME comp = "fault", pin = "en_out";
  hal_pin_inst_t *en = pin_inst_by_name(comp, 0, pin);
  return en && en->source->value > 0.0;
}

// Hang on purpose, to check the watchdog and its record (hwtest).
void wdtest(char *ptr) {
  if(drive_enabled()) {
    printf("error: drive enabled, disable it first\n");
    return;
  }
  if(!running) {
    printf("error: the watchdog is not running\n");
    return;
  }
  if(ptr && !strncmp(ptr, "irq", 3)) {
    printf("interrupts off and hanging: watchdog reset in about 2 s\n");
    Wait(10);
    __disable_irq();
    while(1) {
    }
  }
  if(ptr && !strncmp(ptr, "main", 4)) {
    printf("main loop hanging (rt runs on): watchdog reset in about 2 s\n");
    while(1) {
    }
  }
  printf("wdtest main | irq: hang the main loop, or everything, until the watchdog resets\n");
}
COMMAND("wdtest", wdtest, "hang on purpose to test the watchdog: wdtest main | irq");
