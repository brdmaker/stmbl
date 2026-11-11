/*
 * shared/yaskawa_frame.h: the Yaskawa Sigma serial encoder's reply decoder
 * (src/comps/yaskawa.c) and what the component does with a bad reply.
 * Replies are built here as the encoder sends them: preamble, HDLC flag,
 * bit-stuffed data with its CRC-16, flag, Manchester-coded at 4 Mbit/s
 * (half a bit 10.5 ticks of the 84 MHz capture timer), and decoded from
 * the edge times. Noise (a glitch, a reply that never ends) used to write
 * past the reply buffers, and a bad reply's position was used anyway
 * (DEBUGNOTES 2026-11-11). Built with AddressSanitizer in make check.
 * Prints TAP.
 */
#include "yaskawa_frame.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int n, fails;
static void ok(int c, const char *name) {
  n++;
  fails += !c;
  printf("%s %d - yaskawa_frame: %s\n", c ? "ok" : "not ok", n, name);
}

#define DBITS 112  // the reply's 14 bytes
#define HALF 10.5f
#define BIT_TIME 15

// data bits (MSB first per byte, as the decoder fills reply[]): the
// position at bit 64 LSB first (yaskawa0.off/len 64/15), the CRC in 96..111
static void make_data(char *d, uint32_t pos15, uint32_t extra) {
  uint8_t b[14] = {0};
  b[1]          = 0x5a;  // something in the header bytes
  b[3]          = extra & 0xff;
  for(int i = 0; i < 15; i++) {
    int k = 64 + i;
    if((pos15 >> i) & 1) {
      b[k / 8] |= 1 << (7 - k % 8);
    }
  }
  yaskawa_crc16_t c = yaskawa_crc16_finalize(yaskawa_crc16_update(yaskawa_crc16_init(), b, 12));
  b[12]             = c >> 8;
  b[13]             = c & 0xff;
  for(int k = 0; k < DBITS; k++) {
    d[k] = (b[k / 8] >> (7 - k % 8)) & 1 ? '1' : '0';
  }
  d[DBITS] = 0;
}

// wire bits: preamble, flag, stuffed data, flag
static int make_wire(const char *d, char *w, int end_flag) {
  int k = 0;
  for(const char *p = "0101010101"; *p; p++) {
    w[k++] = *p;
  }
  for(const char *p = "01111110"; *p; p++) {
    w[k++] = *p;
  }
  int ones = 0;
  for(int i = 0; d[i]; i++) {
    w[k++] = d[i];
    ones   = d[i] == '1' ? ones + 1 : 0;
    if(ones == 5) {
      w[k++] = '0';
      ones   = 0;
    }
  }
  if(end_flag) {
    for(const char *p = "01111110"; *p; p++) {
      w[k++] = *p;
    }
  }
  w[k] = 0;
  return k;
}

// Manchester (1 -> high low, 0 -> low high) edge times; idle high
static int edges(const char *w, uint16_t *cap, int max, float t0, float jitter, unsigned seed) {
  srand(seed);
  int level = 1, k = 0;
  float t   = t0;
  for(int i = 0; w[i]; i++) {
    int h[2] = {w[i] == '1', w[i] != '1'};
    for(int j = 0; j < 2; j++) {
      if(h[j] != level && k < max) {
        cap[k++] = (uint16_t)(t + jitter * ((rand() / (float)RAND_MAX) * 2 - 1) + 0.5f);
        level    = h[j];
      }
      t += HALF;
    }
  }
  if(level != 1 && k < max) {
    cap[k++] = (uint16_t)(t + 0.5f);
  }
  return k;
}

// the captures in a heap block of exactly count entries: ASan sees a read past
static int decode(const uint16_t *cap, int count, yask_frame_t *fr) {
  uint16_t *c = malloc(count * sizeof(*c));
  memcpy(c, cap, count * sizeof(*c));
  int r = yask_decode(c, count, BIT_TIME, fr);
  free(c);
  return r;
}

int main(void) {
  setvbuf(stdout, 0, _IONBF, 0);
  char d[DBITS + 1], w[400];
  uint16_t cap[600];
  yask_frame_t *fr = malloc(sizeof(*fr));  // exactly the frame: ASan sees a write past

  make_data(d, 0x1234, 7);
  int nw = make_wire(d, w, 1);
  int k  = edges(w, cap, 300, 100, 0, 1);
  ok(k > 80, "a built reply has more than 80 edges");
  ok(decode(cap, k, fr) == YASK_OK, "a built reply decodes with a good CRC");
  ok(yask_bits(fr, 64, 15) == 0x1234, "the position at bit 64, 15 bits");
  ok(strncmp(fr->m, d, DBITS) == 0, "every data bit back");

  make_data(d, 0x7fff, 0xff);  // many ones: stuffing
  make_wire(d, w, 1);
  k = edges(w, cap, 300, 65400, 1.5f, 2);  // timer wrap, +-1.5 ticks jitter
  ok(decode(cap, k, fr) == YASK_OK && yask_bits(fr, 64, 15) == 0x7fff, "stuffed ones, jitter, the timer wrapping");

  // a glitch: two edges 1 tick apart in the middle of the data
  make_data(d, 0x0abc, 3);
  make_wire(d, w, 1);
  k = edges(w, cap, 300, 100, 0, 3);
  memmove(cap + 62, cap + 60, (k - 60) * sizeof(cap[0]));
  cap[60] = cap[59] + 3;
  cap[61] = cap[60] + 1;
  for(int i = 62; i < k + 2; i++) {
    if(cap[i] < cap[61]) {
      cap[i] = cap[61] + 1;
    }
  }
  int r = decode(cap, k + 2, fr);
  ok(r != YASK_OK, "a glitch in the data: the reply is rejected");

  // a reply that never ends: alternating bits for 280 bits, no flag
  memset(d, 0, sizeof(d));
  for(int i = 0; i < 280 && i < (int)sizeof(w) - 30; i++) {
    w[i] = 0;
  }
  {
    char wl[400];
    int j = 0;
    for(const char *p = "010101010101111110"; *p; p++) {
      wl[j++] = *p;
    }
    for(int i = 0; i < 280; i++) {
      wl[j++] = i % 2 ? '1' : '0';
    }
    wl[j] = 0;
    k     = edges(wl, cap, 300, 100, 0, 4);
    r     = decode(cap, k, fr);
    ok(r != YASK_OK, "a reply with no end flag (300 edges): rejected");
    ok(fr->nbits <= YASK_BITS_MAX && fr->m[fr->nbits] == 0, "and nothing written past the bits (ASan checks the writes)");
  }

  ok(decode(cap, 50, fr) == YASK_SHORT, "50 edges: no reply");

  // yask_track: a bad reply holds the position on its course
  yask_track_t t = {0};
  int err;
  uint32_t p = yask_track(&t, 0, 0, 15, 2, &err);
  ok(err == 1, "no good reply yet: error");
  yask_track(&t, 1, 100, 15, 2, &err);
  p = yask_track(&t, 1, 110, 15, 2, &err);
  ok(p == 110 && err == 0, "good replies: their position, no error");
  p = yask_track(&t, 0, 31000, 15, 2, &err);  // a bad CRC with a wild position
  ok(p == 120 && err == 0, "a bad reply: its position ignored, the last moved on by the last step");
  p = yask_track(&t, 0, 5, 15, 2, &err);
  ok(p == 130 && err == 0, "two bad in a row: still on course, no error (max_bad 2)");
  p = yask_track(&t, 0, 5, 15, 2, &err);
  ok(err == 1, "three bad in a row: error (fault0.mot_fb_error)");
  p = yask_track(&t, 1, 145, 15, 2, &err);
  ok(p == 145 && err == 0, "a good reply again: its position, the error clears");
  yask_track_t u = {0};
  yask_track(&u, 1, 32760, 15, 2, &err);
  yask_track(&u, 1, 2, 15, 2, &err);  // across the wrap: +10
  p = yask_track(&u, 0, 0, 15, 2, &err);
  ok(p == 12, "the step across the 15-bit wrap");
  yask_track_t v = {0};
  yask_track(&v, 1, 50, 15, 0, &err);
  yask_track(&v, 0, 0, 15, 0, &err);
  ok(err == 1, "max_bad 0: every bad reply is an error (the strict setting)");
  (void)nw;
  free(fr);
  printf("1..%d\n", n);
  return fails != 0;
}
