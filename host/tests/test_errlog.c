/*
 * shared/errlog.c on a simulated flash: programming only clears bits, as on
 * the STM32. Prints TAP.
 */
#include "errlog.h"
#include <stdio.h>
#include <string.h>

static int n, fails;

static void ok(int c, const char *name) {
  n++;
  fails += !c;
  printf("%s %d - errlog: %s\n", c ? "ok" : "not ok", n, name);
}

#define WORDS 512  // 64 slots, small to fill quickly
static uint32_t flash[WORDS];
static int erases, cut_after = -1;  // power cut: program no more words after this many

static int prog(uint32_t w, uint32_t v) {
  if(cut_after == 0) {
    return -1;
  }
  if(cut_after > 0) {
    cut_after--;
  }
  if((flash[w] & v) != v) {  // a 0 can't become a 1
    return -1;
  }
  flash[w] &= v;
  return 0;
}
static int erase(void) {
  memset(flash, 0xff, sizeof(flash));
  erases++;
  return 0;
}
static const errlog_flash_t fl = {flash, WORDS, prog, erase};

static int add(errlog_t *l, errlog_type_t t, uint32_t code, uint32_t a) {
  errlog_entry_t e = {errlog_head(t, code, l->boot), 1000, a, 0, 0, -1, -1, 3, 7, 0};
  return errlog_append(l, &e);
}

static int boot(errlog_t *l) {
  int r = errlog_open(l, &fl, 8);
  add(l, ERRLOG_BOOT, 1, 0);
  return r;
}

int main(void) {
  errlog_t l;
  erase();
  erases = 0;

  ok(boot(&l) == 0 && l.boot == 1 && l.next == 1, "first boot on erased flash: boot 1");
  ok(add(&l, ERRLOG_WATCHDOG, 0, 0) == 0 && add(&l, ERRLOG_FAULT, 9, 42) == 0, "entries written");

  // power cycle: everything is still there, the next boot counts on
  ok(boot(&l) == 0 && l.boot == 2 && l.next == 4, "after a power cycle: boot 2, entries kept");
  const errlog_entry_t *e = errlog_at(&l, 2);
  ok(errlog_valid(e) && ERRLOG_TYPE(e) == ERRLOG_FAULT && ERRLOG_CODE(e) == 9 && e->a == 42 && ERRLOG_BOOTNR(e) == 1 &&
         e->nrt_func == 3 && e->cmd == 7,
     "a fault entry reads back");
  ok(ERRLOG_TYPE(errlog_at(&l, 1)) == ERRLOG_WATCHDOG, "the watchdog entry reads back");

  // a power cut in the middle of an entry: skipped, never written over
  cut_after = 3;
  ok(add(&l, ERRLOG_FAULT, 11, 0) == -1, "torn write fails");
  cut_after = -1;
  ok(boot(&l) == 0 && l.boot == 3, "boot after the torn entry");
  ok(!errlog_valid(errlog_at(&l, 4)), "the torn entry is invalid");
  ok(errlog_valid(errlog_at(&l, 5)) && ERRLOG_TYPE(errlog_at(&l, 5)) == ERRLOG_BOOT, "the next entry went after it");

  // fill it: the boot compacts, keeping the newest, in order
  while(add(&l, ERRLOG_FAULT, 2, 1000 + l.next) == 0) {
  }
  ok(l.next == WORDS / ERRLOG_WORDS, "full: append refuses");
  uint32_t newest = errlog_at(&l, l.next - 1)->a;
  ok(boot(&l) == 0 && erases == 1, "a nearly full log is compacted at boot");
  ok(l.boot == 4, "boot numbers go on through a compaction");
  uint32_t kept = l.next - 1;  // minus this boot's entry
  ok(kept == ERRLOG_KEEP || kept == WORDS / ERRLOG_WORDS - 1, "the newest entries kept");
  ok(errlog_at(&l, kept - 1)->a == newest, "the newest is last");
  int ordered = 1;
  uint32_t last = 0;
  for(uint32_t i = 0; i < kept; i++) {  // the fill entries carry 1000 + their slot
    if(ERRLOG_TYPE(errlog_at(&l, i)) == ERRLOG_FAULT && errlog_at(&l, i)->a >= 1000) {
      ordered &= errlog_at(&l, i)->a > last;
      last = errlog_at(&l, i)->a;
    }
  }
  ok(ordered, "in their order");
  int valid = 1;
  for(uint32_t i = 0; i < l.next; i++) {
    valid &= errlog_valid(errlog_at(&l, i));
  }
  ok(valid, "torn entries are dropped by the compaction");

  // the check never looks like erased flash
  errlog_entry_t z;
  memset(&z, 0, sizeof(z));
  ok(errlog_check(&z) != 0xffffffff, "check is never all ones");

  ok(errlog_clear(&l) == 0 && l.next == 0 && boot(&l) == 0 && l.boot == 1, "clear: empty, boot 1 again");

  // boot number 0xffff wraps to 1, never 0
  erase();
  errlog_t w;
  errlog_open(&w, &fl, 8);
  w.boot = 0xffff;
  add(&w, ERRLOG_BOOT, 1, 0);
  ok(errlog_open(&w, &fl, 8) == 0 && w.boot == 1, "boot 0xffff wraps to 1");

  printf("1..%d\n", n);
  if(fails) {
    printf("# %d of %d errlog tests failed\n", fails, n);
  }
  return fails != 0;
}
