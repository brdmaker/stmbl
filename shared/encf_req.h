// encf's request word, sent by the frt timer interrupt instead of the rt
// code (encf0.req_sync = 1, stmbl_freak's req_sync).
//
// The FANUC encoder latches its position on the request. The rt code runs
// after the ADC's DMA, a few microseconds of jitter after the timer edge;
// the frt timer (TIM_SLAVE) counts TIM_MASTER, which also triggers that ADC,
// so the next frt update after the rt arms the word is always the same one,
// a fixed time after the edge. The frt interrupt (priority 0) sends the word
// first thing (src/main.c). On stmbl_freak's alpha 3 axis this lowered the
// velocity noise above 200 Hz from 0.14 to 0.105 rad/s.
//
// A one-word mailbox: 0 = nothing to send. The rt (lower priority) arms it,
// the frt takes it; a 16-bit store and load are atomic on the M4.
// host/tests/test_encf_req.c.
#pragma once
#include <stdint.h>

extern volatile uint16_t encf_req_word;  // src/main.c (host: the test)

// rt: send now (sync 0, returns the word to write) or arm it for the next frt
// update (returns 0, nothing to write now).
static inline uint16_t encf_req_arm(volatile uint16_t *slot, uint16_t word, int sync) {
  if(sync) {
    *slot = word;
    return 0;
  }
  *slot = 0;  // a word still armed from sync 1 must not go out as well
  return word;
}

// frt: the word to send now, or 0; sends each armed word once.
static inline uint16_t encf_req_take(volatile uint16_t *slot) {
  uint16_t w = *slot;
  if(w) {
    *slot = 0;
  }
  return w;
}
