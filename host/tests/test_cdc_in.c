/*
 * shared/cdc_in.h: the drive's USB IN buffer (APP_Rx_Buffer), written by the
 * main loop (printf's _write, the scope's cdc_tx) and drained by the USB
 * interrupt as the ST VCP library does (usbd_cdc_core.c, Handle_USBAsynchXfer:
 * from ptr_out up to ptr_in, or to the end, 64 bytes a packet). The writers
 * didn't check the free space: a host reading slower than the drive wrote
 * had unsent bytes overwritten, scope bytes inside text (DEBUGNOTES
 * 2026-10-14). Prints TAP.
 */
#include "cdc_in.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int n, fails;
static void ok(int c, const char *name) {
  n++;
  fails += !c;
  printf("%s %d - cdc_in: %s\n", c ? "ok" : "not ok", n, name);
}

#define SIZE 256
static uint8_t buf[SIZE];
static volatile uint32_t in;
static uint32_t out;  // the library's APP_Rx_ptr_out (it can sit at SIZE until it wraps)

// the USB interrupt: one packet, up to 64 bytes, as the library takes it
static uint8_t got[1 << 20];
static uint32_t ngot;
static void usb_packet(void) {
  if(out == SIZE) {
    out = 0;
  }
  if(out == in) {
    return;
  }
  uint32_t len = in > out ? in - out : SIZE - out;
  if(len > 64) {
    len = 64;
  }
  memcpy(got + ngot, buf + out, len);
  ngot += len;
  out += len;
}

int main(void) {
  srand(7);
  // the expected stream: what the writers say they put in
  static uint8_t want[1 << 20];
  uint32_t nwant = 0, frames_put = 0, frames_dropped = 0, text_dropped = 0;
  uint8_t seq = 128;  // frame bytes 128..199: never text, never 0xFF
  for(int step = 0; step < 20000; step++) {
    if(rand() % 3) {  // a scope frame: 0xFF and 8 bytes, whole or not at all
      uint8_t f[9] = {0xff};
      for(int i = 1; i < 9; i++) {
        f[i] = seq;
      }
      uint32_t w = cdc_in_put(buf, SIZE, &in, out, f, 9, 1);
      if(w == 9) {
        memcpy(want + nwant, f, 9);
        nwant += 9;
        frames_put++;
        seq = seq < 199 ? seq + 1 : 128;
      } else {
        frames_dropped += w == 0;
      }
    } else {  // text: as much as fits
      char t[40];
      int len = snprintf(t, sizeof(t), "line %d\n", step);
      uint32_t w = cdc_in_put(buf, SIZE, &in, out, (const uint8_t *)t, len, 0);
      memcpy(want + nwant, t, w);
      nwant += w;
      text_dropped += len - w;
    }
    if(rand() % 4 == 0) {  // the host reads slower than the drive writes
      usb_packet();
    }
  }
  for(int i = 0; i < 100; i++) {
    usb_packet();
  }
  ok(frames_dropped > 0 && text_dropped > 0, "the host is too slow: some data doesn't fit");
  ok(ngot == nwant, "everything put in came out, nothing more");
  ok(ngot == nwant && memcmp(got, want, nwant) == 0, "byte for byte: no unsent data overwritten");
  int split = 0;
  for(uint32_t i = 0; i < ngot; i++) {
    if(got[i] == 0xff) {
      for(int k = 1; k < 9 && i + k < ngot; k++) {
        split |= got[i + k] != got[i + 1] || got[i + k] == 0xff || got[i + k] == '\n';
      }
    }
  }
  ok(!split, "every scope frame whole (0xFF and 8 bytes of its own)");
  ok(cdc_in_free(0, 0, SIZE) == SIZE - 1, "empty: SIZE - 1 free (in never catches out)");
  ok(cdc_in_free(10, SIZE, SIZE) == SIZE - 1 - 10, "out at SIZE (before the library wraps it) counts as 0");
  printf("1..%d\n", n);
  return fails != 0;
}
