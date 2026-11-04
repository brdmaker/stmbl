// shared/fanuc1p.c: the fanuci probe's request builder, edge decoder and
// field extraction (docs/fanuci.md). Prints TAP-like lines, exits 1 on failure.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fanuc1p.h"

static int fails, tests;
#define CHECK(c, ...)                   \
  do {                                  \
    tests++;                            \
    if(!(c)) {                          \
      fails++;                          \
      printf("not ok %d - ", tests);    \
      printf(__VA_ARGS__);              \
      printf("\n");                     \
    }                                   \
  } while(0)

// a reply as the receiver sees it: idle, then the bits, then idle again;
// edges at bit boundaries where the level changes, timestamps in timer ticks
// with jitter, starting at t0 (wraps at 16 bit like the hardware timer)
static int edges_of(const uint8_t *bits, int n, int idle, float tpb, float jitter, unsigned t0, uint16_t *t) {
  int ne = 0, level = idle;
  for(int i = 0; i < n; i++) {
    if(bits[i] != level) {
      float j  = jitter * ((float)rand() / RAND_MAX * 2.0f - 1.0f);
      t[ne++]  = (uint16_t)(t0 + (unsigned)lroundf(i * tpb + j));
      level    = bits[i];
    }
  }
  if(level != idle) {  // back to idle after the frame
    t[ne++] = (uint16_t)(t0 + (unsigned)lroundf(n * tpb));
  }
  return ne;
}

int main(void) {
  srand(1);

  // request levels: lead idle, LSB first, tail idle
  uint8_t lv[64];
  int n = f1p_build_levels(lv, 64, 512, 10, 2, 1, 1);
  CHECK(n == 13, "build_levels length %d", n);
  const uint8_t want[13] = {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1};
  CHECK(memcmp(lv, want, 13) == 0, "build_levels 512/10: start 0, 0x00, stop 1");
  n = f1p_build_levels(lv, 64, 0x70, 10, 0, 0, 0);  // universal-IRC-1's 0b0001110000
  CHECK(n == 10 && lv[4] && lv[5] && lv[6] && !lv[3] && !lv[7], "build_levels 0x70");
  n = f1p_build_levels(lv, 5, 0xffff, 16, 2, 2, 0);
  CHECK(n == 5, "build_levels respects max");

  // fields
  uint8_t b[64] = {0};
  b[3] = 1;
  b[5] = 1;  // bits 3..5 = 1,0,1
  CHECK(f1p_field(b, 64, 3, 3, 1) == 5, "field lsb_first");
  CHECK(f1p_field(b, 64, 3, 3, 0) == 5, "field msb_first symmetric");
  b[4] = 1;
  b[5] = 0;  // 1,1,0
  CHECK(f1p_field(b, 64, 3, 3, 1) == 3 && f1p_field(b, 64, 3, 3, 0) == 6, "field bit order");
  CHECK(f1p_field(b, 4, 3, 3, 1) == 1, "field past nbits reads 0");

  // round trip: a frame in the universal-IRC-1 A64i layout, decoded from edges
  const float tpb = 84e6f / 2721088.0f;  // TIM4 ticks per bit (~30.9)
  for(int trial = 0; trial < 200; trial++) {
    uint8_t frame[80];
    int nb = 73;
    // start bit 0 after idle 1, then random bits, with fields placed
    for(int i = 0; i < nb; i++) {
      frame[i] = rand() & 1;
    }
    frame[0]       = 0;
    uint32_t pos   = rand() & 0xffff, turns = rand() & 0xffff, com = rand() & 0xff;
    for(int i = 0; i < 16; i++) {
      frame[21 + i] = (pos >> i) & 1;
      frame[40 + i] = (turns >> i) & 1;
    }
    for(int i = 0; i < 8; i++) {
      frame[59 + i] = (com >> i) & 1;
    }
    frame[nb - 1] = 1;  // ends at idle level
    uint16_t t[200];
    unsigned t0 = 65400u + trial;  // forces wrap-around inside the frame
    int ne      = edges_of(frame, nb, 1, tpb, 4.0f, t0, t);
    uint8_t out[F1P_MAX_BITS];
    int span = (int)((float)(uint16_t)(t[ne - 1] - t[0]) / tpb + 1.5f);
    int got  = f1p_decode(t, ne, tpb, 1, out, span);
    int ok   = got >= 67;
    for(int i = 0; ok && i < 67; i++) {
      ok = out[i] == frame[i];
    }
    CHECK(ok, "round trip trial %d: bits match (got %d bits)", trial, got);
    CHECK(f1p_field(out, got, 21, 16, 1) == pos, "trial %d pos", trial);
    CHECK(f1p_field(out, got, 40, 16, 1) == turns, "trial %d turns", trial);
    CHECK(f1p_field(out, got, 59, 8, 1) == com, "trial %d com", trial);
  }

  // decode: bit rate off by 3 % still decodes a short frame
  {
    uint8_t frame[24] = {0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1};
    uint16_t t[40];
    int ne = edges_of(frame, 24, 1, tpb * 1.03f, 0.0f, 100, t);
    uint8_t out[40];
    int got = f1p_decode(t, ne, tpb, 1, out, 24);
    CHECK(got == 24 && memcmp(out, frame, 23) == 0, "3%% bit-rate error tolerated");
  }

  // format
  char s[64];
  uint8_t f[10] = {1, 0, 1, 1, 0, 0, 0, 0, 1, 1};
  f1p_format(s, sizeof(s), f, 10, 8);
  CHECK(strcmp(s, "10110000 11") == 0, "format groups: '%s'", s);
  f1p_format(s, 6, f, 10, 0);
  CHECK(strlen(s) <= 5, "format respects max");

  // degenerate input
  {
    uint8_t out[4];
    uint16_t one = 5;
    CHECK(f1p_decode(&one, 0, tpb, 1, out, 4) == 0, "no edges: no bits");
    CHECK(f1p_decode(&one, 1, 0.0f, 1, out, 4) == 0, "zero bit time: no bits");
    CHECK(f1p_decode(&one, 1, tpb, 1, out, 4) == 4 && out[0] == 0, "one edge: line held at !idle");
  }

  printf("%s test_fanuc1p: %d/%d passed\n", fails ? "FAIL" : "ok", tests - fails, tests);
  return fails ? 1 : 0;
}
