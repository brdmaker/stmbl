// The drive's USB IN buffer (the ST VCP library's APP_Rx_Buffer): the main
// loop writes (printf's _write, the scope's cdc_tx), the USB interrupt sends
// from APP_Rx_ptr_out to APP_Rx_ptr_in. The writers didn't check the free
// space: a host reading slower than the drive wrote had unsent bytes
// overwritten, scope bytes inside text and garbled reads (DEBUGNOTES
// 2026-10-14, host/tests/test_cdc_in.c). Now nothing is written over unsent
// data: a scope frame goes whole or not at all, text as much as fits.
#pragma once
#include <stdint.h>

// what the writer may put without catching up with out (one byte stays free:
// in == out means empty to the library). out can sit at size until the
// library wraps it.
static inline uint32_t cdc_in_free(uint32_t in, uint32_t out, uint32_t size) {
  out %= size;
  uint32_t used = (in + size - out) % size;
  return size - 1 - used;
}

// returns the bytes put: len, or fewer (text, whole 0), or 0 (whole 1, no room)
static inline uint32_t cdc_in_put(uint8_t *buf, uint32_t size, volatile uint32_t *in, uint32_t out, const uint8_t *d,
                                  uint32_t len, int whole) {
  uint32_t i    = *in;
  uint32_t room = cdc_in_free(i, out, size);
  if(len > room) {
    if(whole) {
      return 0;
    }
    len = room;
  }
  for(uint32_t k = 0; k < len; k++) {
    buf[i++] = d[k];
    if(i >= size) {
      i = 0;
    }
  }
  __atomic_store_n(in, i, __ATOMIC_RELEASE);  // the data first, then the pointer the interrupt reads
  return len;
}
