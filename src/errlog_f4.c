/*
 * The flash error log on the F4 (shared/errlog.h): sector 3, 16 KB, 512
 * entries; unused before (stm32_flash.ld: PARAM1).
 *
 * At boot, before rt starts: compact if nearly full (a sector erase,
 * 0.5 s max), then a boot entry with the reset cause, then the crash or hang
 * from the CCM crash record if it isn't logged yet. While running: a drive
 * fault when fault0.fault turns non-zero or changes, written once the
 * outputs are off (fault0.en_out 0), at most ERRLOG_FAULTS_PER_BOOT a boot.
 * A word program stalls the interrupts ~16 us (100 us max); flash_grace
 * lets rt run late, as for fwup.
 */
#include <stdio.h>
#include <string.h>
#include "stm32f4xx_conf.h"
#include "hal.h"
#include "commands.h"
#include "errlog.h"
#include "wdog.h"

#define ERRLOG_BASE 0x0800C000
#define ERRLOG_SIZE 0x4000

extern volatile uint64_t systime;
int crash_unlogged(errlog_entry_t *e, uint16_t boot);  // stm32f4xx_it.c
uint32_t wdog_reset(uint32_t *flags);                  // wdog.c
const char *fault_name(uint32_t f);                    // shared/comps/fault.c

static void flash_clear_flags(void) {
  FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR |
                  FLASH_FLAG_PGSERR);
}

static int prog(uint32_t word, uint32_t value) {
  uint32_t addr   = ERRLOG_BASE + word * 4;
  hal.flash_grace = 2;
  FLASH_Unlock();
  flash_clear_flags();
  int r = FLASH_ProgramWord(addr, value) != FLASH_COMPLETE;
  FLASH_Lock();
  return r || *(volatile uint32_t *)addr != value ? -1 : 0;
}

static int erase(void) {
  FLASH_Unlock();
  flash_clear_flags();
  int r = FLASH_EraseSector(FLASH_Sector_3, VoltageRange_3) != FLASH_COMPLETE;
  FLASH_Lock();
  return r ? -1 : 0;
}

static const errlog_flash_t flash = {(const volatile uint32_t *)ERRLOG_BASE, ERRLOG_SIZE / 4, prog, erase};
static errlog_t elog;
static int open_failed;

static void crumbs(errlog_entry_t *e) {
  e->rt_func  = (int16_t)(int32_t)hal.active_rt_func;
  e->frt_func = (int16_t)(int32_t)hal.active_frt_func;
  e->nrt_func = (int16_t)(int32_t)hal.active_nrt_func;
  e->cmd      = (int16_t)cmd_active;
}

// main(), after setup(), before rt and the watchdog start
void errlog_boot(void) {
  if(errlog_open(&elog, &flash, 16) < 0) {
    open_failed = 1;
  }
  uint32_t csr;
  uint32_t cause    = wdog_reset(&csr);
  errlog_entry_t e  = {errlog_head(ERRLOG_BOOT, cause, elog.boot), 0, csr, 0, 0, -1, -1, -1, -1, 0};
  errlog_entry_t ce = {0};
  if(crash_unlogged(&ce, elog.boot)) {  // the boot before's crash first
    errlog_append(&elog, &ce);
  }
  errlog_append(&elog, &e);
}

static hal_pin_inst_t *pin(const char *comp, const char *name) {
  NAME c, p;
  strncpy(c, comp, sizeof(c) - 1);
  c[sizeof(c) - 1] = 0;
  strncpy(p, name, sizeof(p) - 1);
  p[sizeof(p) - 1] = 0;
  return pin_inst_by_name(c, 0, p);
}

static float val(hal_pin_inst_t *p) {
  return p ? p->source->value : 0.0;
}

static uint32_t fbits(float f) {
  uint32_t u;
  memcpy(&u, &f, 4);
  return u;
}

// The main loop: log drive faults.
void errlog_poll(void) {
  static hal_pin_inst_t *fault, *en_out, *dc, *hvt, *mot;
  static uint32_t last, next_lookup;
  static int pending;
  static errlog_entry_t e;
  if(!elog.fl) {
    return;
  }
  if(!fault || !en_out) {
    if((uint32_t)systime < next_lookup) {
      return;
    }
    next_lookup = (uint32_t)systime + 1000;  // no fault comp (safe mode): look again later
    fault       = pin("fault", "fault");
    en_out      = pin("fault", "en_out");
    dc          = pin("fault", "dc_volt");
    hvt         = pin("fault", "hv_temp");
    mot         = pin("fault", "mot_temp");
    return;
  }
  uint32_t f = (uint32_t)val(fault);
  if(f && f != last && !pending && elog.faults < ERRLOG_FAULTS_PER_BOOT) {
    e.head      = errlog_head(ERRLOG_FAULT, f, elog.boot);
    e.uptime_ms = (uint32_t)systime;
    e.a         = fbits(val(dc));
    e.b         = fbits(val(hvt));
    e.c         = fbits(val(mot));
    crumbs(&e);
    pending = 1;
    elog.faults++;
  }
  last = f;
  if(pending && val(en_out) <= 0.0) {  // outputs off: the flash stall can't hurt
    errlog_append(&elog, &e);
    pending = 0;
  }
}

static void print_func(const char *what, int16_t f) {
  if(f < 0) {
    return;
  }
  if(f < (int32_t)hal.comp_inst_count) {
    printf(" %s %s%lu", what, hal.comp_insts[f].comp->name, hal.comp_insts[f].instance);
  } else {
    printf(" %s #%d", what, f);
  }
}

static float bitsf(uint32_t u) {
  float f;
  memcpy(&f, &u, 4);
  return f;
}

static void print_entry(uint32_t i, const errlog_entry_t *e) {
  if(!errlog_valid(e)) {
    printf("%4lu  damaged (power cut while writing)\n", i);
    return;
  }
  uint32_t code = ERRLOG_CODE(e);
  printf("%4lu  boot %5lu  %9lu ms  ", i, ERRLOG_BOOTNR(e), e->uptime_ms);
  switch(ERRLOG_TYPE(e)) {
    case ERRLOG_BOOT:
      printf("boot: %s (csr 0x%08lx)", code < sizeof(wdog_reset_names) / sizeof(wdog_reset_names[0]) ? wdog_reset_names[code] : "?", e->a);
      break;
    case ERRLOG_CRASH: {
      static const char *names[] = {"HardFault", "NMI", "MemManage", "BusFault", "UsageFault"};
      printf("CRASH %s pc 0x%08lx lr 0x%08lx cfsr 0x%08lx", code < 5 ? names[code] : "?", e->a, e->b, e->c);
      break;
    }
    case ERRLOG_WATCHDOG:
      printf("HANG: watchdog reset");
      break;
    case ERRLOG_FAULT:
      printf("fault %lu %s, dc %.1f V, hv %.1f C, motor %.1f C", code, fault_name(code), bitsf(e->a), bitsf(e->b), bitsf(e->c));
      break;
    default:
      printf("type %lu code %lu", ERRLOG_TYPE(e), code);
  }
  const char *cn = cmd_name(e->cmd);
  if(cn) {
    printf(" cmd %s", cn);
  }
  print_func("rt", e->rt_func);
  print_func("frt", e->frt_func);
  print_func("nrt", e->nrt_func);
  printf("\n");
}

static int drive_enabled(void) {
  hal_pin_inst_t *en = pin("fault", "en_out");
  return en && en->source->value > 0.0;
}

void errlog_cmd(char *ptr) {
  if(!elog.fl) {
    printf("error: no error log\n");
    return;
  }
  if(ptr && !strncmp(ptr, "clear", 5)) {
    if(drive_enabled()) {
      printf("error: drive enabled, disable it before errlog clear\n");
      return;
    }
    int restart = hal.hal_state == HAL_OK2 && hal.rt_state != RT_STOP;
    hal_stop();  // the erase stalls rt (see flashsaveconf)
    int r = errlog_clear(&elog);
    if(restart) {
      hal_start();
    }
    printf(r ? "error: erase failed\n" : "error log cleared\n");
    return;
  }
  uint32_t first = 0;
  if(!(ptr && !strncmp(ptr, "all", 3)) && elog.next > 40) {
    first = elog.next - 40;
  }
  printf("error log: %lu of %lu entries used, this is boot %u%s\n", elog.next, errlog_slots(&elog), elog.boot,
         open_failed ? " (compacting failed)" : "");
  if(first) {
    printf("(the last 40; `errlog all` for all)\n");
  }
  for(uint32_t i = first; i < elog.next; i++) {
    print_entry(i, errlog_at(&elog, i));
  }
}
COMMAND("errlog", errlog_cmd, "the error log in flash (boots, crashes, hangs, faults): errlog [all | clear]");
