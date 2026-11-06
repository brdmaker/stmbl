#pragma once
/*
 * The error log: boots, crashes, watchdog resets and drive faults, kept in
 * flash so they outlast a power cycle (the crash record in CCM doesn't; a
 * hang on the SEW bench, 2026-10-29, was gone after the 24 V cycle that
 * recovered it). The F4 keeps it in sector 3 (src/errlog_f4.c, `errlog`);
 * the host tests it on a simulated flash (host/tests/test_errlog.c).
 *
 * Append-only 32-byte entries in erased flash. A slot is free when all its
 * words are erased, so an entry torn by a power cut is skipped (its check
 * fails) and never written over. When the log is nearly full, the boot
 * erases it and writes back the newest ERRLOG_KEEP entries.
 */
#include <stdint.h>

typedef enum {
  ERRLOG_BOOT = 1,  // code: wdog_reset_t; a: RCC_CSR
  ERRLOG_CRASH,     // code: the fault handler; a, b, c: pc, lr, cfsr
  ERRLOG_WATCHDOG,  // a hang
  ERRLOG_FAULT,     // code: fault_t; a, b, c: dc_volt, hv_temp, mot_temp (float bits)
  ERRLOG_NOTE,      // code, a, b, c: what the writer says (usbdiag: the USB state)
} errlog_type_t;

typedef struct {
  uint32_t head;       // type << 24 | code << 16 | boot (16 bits); never all ones
  uint32_t uptime_ms;  // since this boot
  uint32_t a, b, c;
  int16_t rt_func, frt_func;  // hal.active_*_func, -1 none
  int16_t nrt_func, cmd;      // cmd: the console command running, -1 none
  uint32_t check;
} errlog_entry_t;

#define ERRLOG_WORDS (sizeof(errlog_entry_t) / 4)
#define ERRLOG_KEEP 64  // entries kept when the boot compacts a full log

#define ERRLOG_TYPE(e) ((e)->head >> 24)
#define ERRLOG_CODE(e) (((e)->head >> 16) & 0xff)
#define ERRLOG_BOOTNR(e) ((e)->head & 0xffff)

// the flash: words read in place; program one word (0: ok), erase it all
typedef struct {
  const volatile uint32_t *base;
  uint32_t words;
  int (*program)(uint32_t word, uint32_t value);
  int (*erase)(void);
} errlog_flash_t;

typedef struct {
  const errlog_flash_t *fl;
  uint32_t next;   // first free slot (entries)
  uint16_t boot;   // this boot's number
  uint32_t faults; // fault entries this boot (capped)
} errlog_t;

#define ERRLOG_FAULTS_PER_BOOT 32  // a fault loop wears no flash

uint32_t errlog_check(const errlog_entry_t *e);
int errlog_valid(const errlog_entry_t *e);
uint32_t errlog_slots(const errlog_t *l);
const errlog_entry_t *errlog_at(const errlog_t *l, uint32_t i);  // i < l->next

// At boot, before rt: scan, compact when fewer than `room` slots are free,
// number this boot. 0, or -1 when the flash failed.
int errlog_open(errlog_t *l, const errlog_flash_t *fl, uint32_t room);
// 0 written, -1 full or the flash failed
int errlog_append(errlog_t *l, errlog_entry_t *e);
uint32_t errlog_head(errlog_type_t type, uint32_t code, uint16_t boot);
int errlog_clear(errlog_t *l);
