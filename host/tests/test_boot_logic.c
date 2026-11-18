/*
 * shared/boot_logic.h: the F4 bootloader's PA13 DFU check, the F3
 * bootloader's erase, hv_update's CRC check pacing (DEBUGNOTES 2026-11-11).
 * Prints TAP.
 */
#include "boot_logic.h"
#include <stdio.h>
#include <string.h>

static int n, fails;
static void ok(int c, const char *name) {
  n++;
  fails += !c;
  printf("%s %d - boot_logic: %s\n", c ? "ok" : "not ok", n, name);
}

// a pin: low for the first `low_for` reads, then as `then_low`
typedef struct {
  int reads, low_for, then_low;
} pin_t;
static int pin_low(void *c) {
  pin_t *p = c;
  return p->reads++ < p->low_for ? 1 : p->then_low;
}
static int pauses;
static void pause_(void) {
  pauses++;
}

// fake F3 flash: 8 pages of 2 KB
#define PAGE 2048
#define PAGES 8
static uint32_t flash[PAGES][PAGE / 4];
static int erases;
static const uint32_t *at(uint32_t a) {
  return flash[(a - 0x08004000) / PAGE];
}
static int erase(uint32_t a) {
  erases++;
  memset(flash[(a - 0x08004000) / PAGE], 0xff, PAGE);
  return 0;
}
static int erase_fail(uint32_t a) {
  (void)a;
  return 1;
}

int main(void) {
  pin_t p = {0, 1, 0};  // the pin still charging: low on the first read only
  ok(!boot_pin_wants_dfu(pin_low, &p, pause_, 0, 100), "pin low only at the first read (pull-up charging): no DFU");
  p = (pin_t){0, 1000, 1};
  pauses = 0;
  ok(boot_pin_wants_dfu(pin_low, &p, pause_, 0, 100) && p.reads == 100 && pauses == 100,
     "PA13 shorted to GND: low on all 100 samples, DFU");
  p = (pin_t){0, 99, 0};
  ok(!boot_pin_wants_dfu(pin_low, &p, pause_, 0, 100), "high on the last sample: no DFU");
  p = (pin_t){0, 1000, 1};
  ok(!boot_pin_wants_dfu(pin_low, &p, pause_, 1, 100) && p.reads == 0, "a debugger attached: the pin isn't read");

  memset(flash, 0xff, sizeof(flash));
  flash[1][7] = 0x12345678;  // an app in pages 1, 2, 5
  flash[2][0] = 0;
  flash[5][511] = 1;
  erases = 0;
  int e  = boot_erase_pages(0x08004000, 0x08004000 + PAGES * PAGE, PAGE, at, erase);
  ok(e == 3 && erases == 3, "first erase: the three written pages");
  int blank = 1;
  for(int i = 0; i < PAGES; i++) {
    for(int j = 0; j < PAGE / 4; j++) {
      blank &= flash[i][j] == 0xffffffffu;
    }
  }
  ok(blank, "the whole app area blank after it");
  erases = 0;
  e      = boot_erase_pages(0x08004000, 0x08004000 + PAGES * PAGE, PAGE, at, erase);
  ok(e == 0 && erases == 0, "a repeated erase command: no page erased (answers at once)");
  flash[3][0] = 0;
  ok(boot_erase_pages(0x08004000, 0x08004000 + PAGES * PAGE, PAGE, at, erase_fail) == -1, "a failed erase: -1 (NAK)");

  uint32_t w = 0;
  int sent   = 0;
  for(int t = 0; t < 1000; t++) {
    sent += boot_crc_send(&w);
  }
  ok(sent == 10, "CRC_CHECK: sent once per 100 ticks of listening (10 in 1000), not every tick");
  ok(BOOT_ERASE_TICKS / 5000 >= 10, "the erase gets 10 s or more at 5 kHz (a full erase 1-2 s)");
  printf("1..%d\n", n);
  return fails != 0;
}
