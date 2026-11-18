// The F4's end of the HV link: link dropouts the F3 reports, and the
// hv_verify read back. Kept apart from the registers so the host tests it
// (host/tests/test_hv_link.c). From stmbl_freak (d683d54, 1ce8cf4).
#pragma once
#include <stdint.h>

// The F3 counts its rt ticks in link timeout since boot (ls0.timeout, sent
// as f3_state_data_t.link_to). It times out after ~0.33 ms and switches the
// gates off; the F4 only after 3 rt ticks (0.6 ms), so a short dropout
// stopped the bridge without the F4 ever faulting. A rise while enabled
// is a drop: counted, and HV_TIMEOUT_ERROR held until the enable drops,
// unless the F3 reports a fault of its own (that code wins). A fall is an
// F3 reboot: the new baseline. An older F3 sends no link_to: nothing.
typedef struct {
  int have;        // a link_to seen
  uint32_t last;   // link_to at the last state word
  uint32_t drops;  // drops while enabled
  int err;         // a drop while enabled, held until en drops
} hv_link_t;

// on a link_to state word; returns 1 for a new drop
static inline int hv_link_word(hv_link_t *l, uint32_t link_to, int en) {
  int drop = l->have && link_to > l->last && en;
  if(drop) {
    l->drops++;
    l->err = 1;
  }
  l->last = link_to;
  l->have = 1;
  return drop;
}

// every rt tick: the fault to report, from the F3's own code
static inline int hv_link_fault(hv_link_t *l, int f3_fault, int en, int timeout_code) {
  if(!en) {
    l->err = 0;
  }
  if(f3_fault) {
    return f3_fault;  // a real F3 fault code wins
  }
  return l->err ? timeout_code : 0;
}

// hv_verify: the F3 app area read back from its bootloader, compared with
// the image the F4 carries, plus HV_VERIFY_EXTRA words past its end that an
// erased area holds as 0xFFFFFFFF.
#define HV_VERIFY_EXTRA 8
#define HV_VERIFY_LIST 8
typedef struct {
  uint32_t state;  // 0 idle, 1 running, 2 done, 3 no answer (not in its bootloader)
  uint32_t words, total, bad;
  uint32_t addr[HV_VERIFY_LIST], got[HV_VERIFY_LIST], want[HV_VERIFY_LIST];
} hv_verify_t;

static inline void hv_verify_start(hv_verify_t *v, uint32_t image_words) {
  v->state = 1;
  v->words = 0;
  v->bad   = 0;
  v->total = image_words + HV_VERIFY_EXTRA;
}

// word i read back as got; image[] has image_words words. Returns 1 when done.
static inline int hv_verify_word(hv_verify_t *v, uint32_t i, uint32_t addr, uint32_t got, const uint32_t *image,
                                 uint32_t image_words) {
  uint32_t want = i < image_words ? image[i] : 0xffffffffu;
  if(got != want) {
    if(v->bad < HV_VERIFY_LIST) {
      v->addr[v->bad] = addr;
      v->got[v->bad]  = got;
      v->want[v->bad] = want;
    }
    v->bad++;
  }
  v->words = i + 1;
  if(v->words >= v->total) {
    v->state = 2;
    return 1;
  }
  return 0;
}
