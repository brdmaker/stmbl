/*
 * shared/encf_frame.h: the FANUC alpha (aA64) serial frame decoder of
 * src/comps/encf.c. Frames are built here (fields, CRC-5), turned into the
 * edge capture times TIM4 records, and decoded. A noisy or broken line
 * (a long gap between edges, too many edges) used to write past the 10-byte
 * frame (DEBUGNOTES 2026-11-11). Prints TAP.
 */
#include "encf_frame.h"
#include <stdio.h>

static int n, fails;
static void ok(int c, const char *name) {
  n++;
  fails += !c;
  printf("%s %d - encf_frame: %s\n", c ? "ok" : "not ok", n, name);
}

static void setbits(uint8_t *f, int at, int len, uint32_t v) {
  for(int i = 0; i < len; i++) {
    int j = at + i;
    f[j / 8] = (f[j / 8] & ~(1 << j % 8)) | (((v >> i) & 1) << j % 8);
  }
}
static uint32_t getbits(const uint8_t *f, int at, int len) {
  uint32_t v = 0;
  for(int i = 0; i < len; i++) {
    v |= (uint32_t)((f[(at + i) / 8] >> ((at + i) % 8)) & 1) << i;
  }
  return v;
}

// a frame as the encoder sends it (bit layout of encf.c's fanuc_t), CRC by search
static void make_frame(uint8_t *f, uint32_t pos22, uint32_t turns, uint32_t com) {
  memset(f, 0, ENCF_BYTES);
  setbits(f, 0, 4, 0xA);  // flag0, bit 0 low: the line's first level
  setbits(f, 9, 2, 0);
  setbits(f, 11, 6, pos22 & 0x3f);
  setbits(f, 17, 2, 1);
  setbits(f, 19, 16, pos22 >> 6);
  setbits(f, 35, 2, 1);
  setbits(f, 37, 16, turns);
  setbits(f, 53, 2, 1);
  setbits(f, 55, 10, com);
  setbits(f, 65, 7, 0x60);
  for(uint32_t c = 0; c < 32; c++) {
    setbits(f, 72, 5, c);
    if(encf_crc_ok(f)) {
      return;
    }
  }
}

// the edges TIM4 captures: runs of equal bits, the line starting low
static uint32_t to_captures(const uint8_t *f, uint16_t *cap, float bit_ticks, uint16_t t0) {
  uint32_t k = 0;
  cap[k++]   = t0;
  int level = 0, run = 0;
  float t   = t0;
  for(int j = 0; j < ENCF_BITS; j++) {
    int b = (f[j / 8] >> (j % 8)) & 1;
    if(b != level) {
      t += run * bit_ticks;
      cap[k++] = (uint16_t)(t + 0.5f);
      level    = b;
      run      = 0;
    }
    run++;
  }
  if(level == 0) {  // back to idle high after the frame
    t += run * bit_ticks;
    cap[k++] = (uint16_t)(t + 0.5f);
  }
  return k;
}

static int canary_clean(const uint8_t *c, size_t len) {
  for(size_t i = 0; i < len; i++) {
    if(c[i] != 0x5a) {
      return 0;
    }
  }
  return 1;
}

int main(void) {
  const float bt = 82.0e6f / 1.024e6f;  // ticks a bit, as encf.c
  uint8_t f[ENCF_BYTES], g[ENCF_BYTES];
  uint16_t cap[200];

  make_frame(f, 0x2ABCDE, 0x1234, 0x155);
  ok(encf_crc_ok(f), "a built frame has a good CRC");
  uint32_t k = to_captures(f, cap, bt, 1000);
  int bits   = encf_decode(cap, k, bt, g);
  ok(bits > 50 && encf_crc_ok(g), "decoded: CRC good");
  ok(getbits(g, 11, 6) + (getbits(g, 19, 16) << 6) == 0x2ABCDE, "decoded: 22-bit position");
  ok(getbits(g, 37, 16) == 0x1234 && getbits(g, 55, 10) == 0x155, "decoded: turns and commutation");

  k = to_captures(f, cap, bt * 1.03f, 65000);  // 3 % clock error, timer wrap
  ok(encf_decode(cap, k, bt, g) > 50 && encf_crc_ok(g) && getbits(g, 37, 16) == 0x1234,
     "3 % off the bit rate and across the timer wrap");

  // a broken frame must not write past it: a canary after the frame
  struct {
    uint8_t f[ENCF_BYTES];
    uint8_t canary[32];
  } m;
  memset(m.canary, 0x5a, sizeof(m.canary));
  // a long gap (a dropout of ~490 bits) in a run of ones (even interval)
  // and in a run of zeros (odd)
  for(int parity = 0; parity < 2; parity++) {
    k = to_captures(f, cap, bt, 0);
    for(uint32_t i = 2 + parity; i < k; i++) {
      cap[i] += 40000;
    }
    memset(m.canary, 0x5a, sizeof(m.canary));
    encf_decode(cap, k, bt, m.f);
    ok(canary_clean(m.canary, sizeof(m.canary)),
       parity ? "a long gap in a run of zeros: nothing written past the frame"
              : "a long gap in a run of ones: nothing written past the frame");
  }
  ok(!encf_crc_ok(m.f), "and the frame fails its CRC");

  // a line full of noise: 160 edges (the whole capture buffer) 5 ticks apart
  for(int i = 0; i < 160; i++) {
    cap[i] = i * 5;
  }
  memset(m.canary, 0x5a, sizeof(m.canary));
  encf_decode(cap, 160, bt, m.f);
  ok(canary_clean(m.canary, sizeof(m.canary)), "noise (160 edges): nothing written past the frame");

  // 160 edges a bit apart: 159 bits, more than a frame
  for(int i = 0; i < 160; i++) {
    cap[i] = (uint16_t)(i * bt);
  }
  memset(m.canary, 0x5a, sizeof(m.canary));
  int b2 = encf_decode(cap, 160, bt, m.f);
  ok(canary_clean(m.canary, sizeof(m.canary)) && b2 <= ENCF_BITS, "too many bits: stops at the frame's end");

  k = to_captures(f, cap, bt, 0);
  ok(encf_decode(cap, 1, bt, g) <= 50, "no reply (one edge): too short, rejected");
  printf("1..%d\n", n);
  return fails != 0;
}
