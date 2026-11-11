/*
 * shared/hv_link.h: link dropouts the F3 reports (hv0.link_to/link_drops)
 * and the hv_verify comparison (DEBUGNOTES 2026-11-11). Prints TAP.
 */
#include "hv_link.h"
#include <stdio.h>

static int n, fails;
static void ok(int c, const char *name) {
  n++;
  fails += !c;
  printf("%s %d - hv_link: %s\n", c ? "ok" : "not ok", n, name);
}

#define TO 9  // HV_TIMEOUT_ERROR
int main(void) {
  hv_link_t l = {0};
  hv_link_word(&l, 7, 1);  // the first word: the baseline (dropouts before this F4 knew)
  ok(l.drops == 0 && hv_link_fault(&l, 0, 1, TO) == 0, "first link_to: a baseline, no drop");
  hv_link_word(&l, 7, 1);
  ok(hv_link_fault(&l, 0, 1, TO) == 0, "unchanged: no fault");
  ok(hv_link_word(&l, 12, 1) == 1 && l.drops == 1, "a rise while enabled: a drop");
  ok(hv_link_fault(&l, 0, 1, TO) == TO, "and HV_TIMEOUT_ERROR, though the link is back");
  hv_link_word(&l, 12, 1);
  ok(hv_link_fault(&l, 0, 1, TO) == TO, "held while enabled");
  ok(hv_link_fault(&l, 16, 1, TO) == 16, "a real F3 fault code wins over it");
  ok(hv_link_fault(&l, 0, 0, TO) == 0, "cleared when the enable drops");
  ok(hv_link_word(&l, 20, 0) == 0 && l.drops == 1, "a rise while disabled: no drop");
  ok(hv_link_word(&l, 3, 1) == 0 && hv_link_fault(&l, 0, 1, TO) == 0, "a fall (F3 reboot): the new baseline");
  hv_link_t old = {0};
  ok(hv_link_fault(&old, 0, 1, TO) == 0 && old.drops == 0, "an older F3 (no link_to words): nothing");

  uint32_t img[4] = {1, 2, 3, 4};
  hv_verify_t v   = {0};
  hv_verify_start(&v, 4);
  int done = 0;
  for(uint32_t i = 0; i < v.total && !done; i++) {
    done = hv_verify_word(&v, i, 0x08004000 + 4 * i, i < 4 ? img[i] : 0xffffffffu, img, 4);
  }
  ok(done && v.state == 2 && v.bad == 0 && v.words == 12, "the image and 8 erased words after it: no difference");
  hv_verify_start(&v, 4);
  uint32_t back[12] = {1, 9, 3, 4, 0xffffffffu, 0, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu};
  for(uint32_t i = 0; i < 12; i++) {
    hv_verify_word(&v, i, 0x08004000 + 4 * i, back[i], img, 4);
  }
  ok(v.bad == 2 && v.addr[0] == 0x08004004 && v.got[0] == 9 && v.want[0] == 2, "a wrong word: listed with address, value, image");
  ok(v.addr[1] == 0x08004014 && v.want[1] == 0xffffffffu, "a written word past the image: listed (not erased)");
  hv_verify_start(&v, 100);
  for(uint32_t i = 0; i < 108; i++) {
    hv_verify_word(&v, i, 0x08004000 + 4 * i, 0, img, 0);
  }
  ok(v.bad == 108 && v.addr[HV_VERIFY_LIST - 1] == 0x08004000 + 4 * (HV_VERIFY_LIST - 1), "many differences: counted, the first 8 listed");
  printf("1..%d\n", n);
  return fails != 0;
}
