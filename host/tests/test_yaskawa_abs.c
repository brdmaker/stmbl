// shared/yaskawa_abs.c: the Yaskawa Sigma-II absolute encoder's reply, from
// edge timestamps as the drive captures them, on frames recorded on the
// bench (brdmot captures/sgmah-turn3-20261110.txt; position and turns
// worked out offline). Exits 1 on failure.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "yaskawa_abs.h"

static int fails, tests;
#define CHECK(c, ...)                \
  do {                               \
    tests++;                         \
    if(!(c)) {                       \
      fails++;                       \
      printf("not ok %d - ", tests); \
      printf(__VA_ARGS__);           \
      printf("\n");                  \
    }                                \
  } while(0)

static const struct {
  const char *hex;
  uint32_t pos;
  int turns;
} rec[] = {
    {"a0f8ad7c7480a3a496f00000fe1e", 1009957, 0},
    {"a0f8a1af671c80f99f4000007eb1", 194975, 0},
    {"a0f80d4811a67baf603ffff05ae4", 788213, -1},
    {"a0f83b5494b408338b6ffff0626f", 446924, -1},
    {"a0f87fc1e468d63dc98ffff04db3", 103356, -1},
};

static void hex2(const char *h, uint8_t *b) {
  for(int i = 0; i < YAB_BYTES; i++) {
    unsigned v;
    sscanf(h + 2 * i, "%2x", &v);
    b[i] = (uint8_t)v;
  }
}

// a capture on the bench (drive 207D35A05631, 2026-11-10, yaskawa_abs0.dump):
// the edge times as TIM4 took them; the preamble mostly before the capture
// started, which the first decoder (8 long intervals for sync) failed on
static const uint16_t cap1[] = {
    0, 21, 42, 53, 63, 84, 94, 105, 115, 126, 136, 147, 157, 168, 178, 189, 210, 231, 252, 273, 294, 305, 315,
    326, 336, 347, 357, 368, 378, 389, 399, 410, 420, 431, 441, 452, 462, 473, 483, 504, 525, 536, 546, 567,
    577, 588, 609, 620, 630, 641, 651, 672, 693, 714, 735, 756, 777, 798, 819, 840, 850, 861, 871, 882, 892,
    903, 913, 924, 945, 966, 987, 998, 1008, 1029, 1039, 1050, 1071, 1092, 1102, 1113, 1134, 1155, 1165, 1176,
    1186, 1197, 1207, 1218, 1239, 1250, 1260, 1281, 1302, 1323, 1344, 1355, 1365, 1376, 1386, 1407, 1428,
    1449, 1470, 1491, 1501, 1512, 1522, 1533, 1554, 1575, 1585, 1596, 1617, 1628, 1638, 1659, 1680, 1701,
    1722, 1743, 1764, 1785, 1795, 1806, 1816, 1827, 1837, 1848, 1869, 1890, 1900, 1911, 1921, 1932, 1942,
    1953, 1963, 1974, 1995, 2016, 2026, 2037, 2047, 2058, 2068, 2079, 2089, 2100, 2121, 2142, 2152, 2163,
    2173, 2184, 2194, 2205, 2226, 2237, 2247, 2258, 2268, 2279, 2289, 2300, 2310, 2321, 2331, 2352, 2362,
    2373, 2383, 2394, 2415, 2436, 2446, 2457, 2478, 2489, 2499, 2520, 2530, 2541, 2551, 2562, 2583, 2594,
    2604, 2625, 2646, 2667, 2677, 2688, 2698, 2709, 2719, 2730, 2740, 2751, 2761, 2772, 2793};
static const char cap1_hex[] = "a004c557e6de515d957bfff03b39";

// the encoder's reply as line levels: preamble (its last npre bits), flag, the bits stuffed,
// flag; Manchester chips; then the edge times (84 MHz timer, a chip 10.5
// ticks) with jitter, the line idle (1) before. inv: the other polarity.
static int reply_edges(const uint8_t *b, int inv, int npre, float jitter, unsigned t0, uint16_t *t, int maxe) {
  char bits[400];
  int nb = 0, ones = 0;
  const char *pre = "0101010101010101010" "01111110";
  for(int i = 19 - npre; pre[i]; i++) {
    bits[nb++] = pre[i] - '0';
  }
  for(int k = 0; k < YAB_BYTES * 8; k++) {
    int x = (b[k >> 3] >> (7 - (k & 7))) & 1;
    bits[nb++] = x;
    ones = x ? ones + 1 : 0;
    if(ones == 5) {
      bits[nb++] = 0;
      ones = 0;
    }
  }
  const char *fl = "01111110";
  for(int i = 0; fl[i]; i++) {
    bits[nb++] = fl[i] - '0';
  }
  int level = 1, ne = 0;
  for(int k = 0; k < 2 * nb && ne < maxe; k++) {
    int bit  = bits[k / 2];
    int chip = (k & 1) ? !bit : bit;  // 0 -> 01, 1 -> 10
    chip ^= inv;
    if(chip != level) {
      float j = jitter * ((float)rand() / RAND_MAX * 2.0f - 1.0f);
      t[ne++] = (uint16_t)(t0 + (unsigned)lroundf(k * 10.5f + j));
      level   = chip;
    }
  }
  if(level != 1 && ne < maxe) {
    t[ne++] = (uint16_t)(t0 + (unsigned)lroundf(2 * nb * 10.5f));
  }
  return ne;
}

int main(void) {
  srand(7);
  uint8_t b[YAB_BYTES], out[32];
  yab_frame_t f;
  for(unsigned i = 0; i < sizeof(rec) / sizeof(rec[0]); i++) {
    hex2(rec[i].hex, b);
    CHECK(yab_frame(b, YAB_BYTES, &f) == YAB_OK, "frame %u: CRC", i);
    CHECK(f.pos == rec[i].pos && f.turns == rec[i].turns, "frame %u: pos %u turns %d, want %u %d", i,
          (unsigned)f.pos, f.turns, (unsigned)rec[i].pos, rec[i].turns);
    for(int inv = 0; inv < 2; inv++) {
      for(int trial = 0; trial < 40; trial++) {
        uint16_t t[601];
        int ne = reply_edges(b, inv, trial % 20, 2.0f, 65000u + trial * 97, t, 600);  // ±2 ticks an edge
        int nb = yab_decode(t, ne, 21, out, sizeof(out));
        yab_frame_t g;
        int r = nb >= YAB_BYTES ? yab_frame(out, nb, &g) : nb;
        CHECK(r == YAB_OK && memcmp(out, b, YAB_BYTES) == 0 && g.pos == rec[i].pos,
              "frame %u inv %d preamble %d: decoded %d bytes, result %d", i, inv, trial % 20, nb, r);
        // a stray edge before the reply (the line turning around)
        memmove(t + 1, t, sizeof(t[0]) * ne);
        t[0] = (uint16_t)(t[1] - 4);
        nb   = yab_decode(t, ne + 1, 21, out, sizeof(out));
        r    = nb >= YAB_BYTES ? yab_frame(out, nb, &g) : nb;
        CHECK(r == YAB_OK && memcmp(out, b, YAB_BYTES) == 0, "frame %u inv %d preamble %d, a stray edge first: %d bytes, result %d",
              i, inv, trial % 20, nb, r);
      }
    }
  }
  // the bench capture
  {
    int nb = yab_decode(cap1, sizeof(cap1) / sizeof(cap1[0]), 21, out, sizeof(out));
    int r  = nb >= YAB_BYTES ? yab_frame(out, nb, &f) : nb;
    hex2(cap1_hex, b);
    CHECK(r == YAB_OK && memcmp(out, b, YAB_BYTES) == 0, "bench capture: %d bytes, result %d", nb, r);
  }
  // the table CRC against a bit-wise one, on random data
  for(int k = 0; k < 200; k++) {
    uint8_t d[12];
    uint16_t c = 0xffff;
    for(int i = 0; i < 12; i++) {
      d[i] = (uint8_t)rand();
      c ^= (uint16_t)(d[i] << 8);
      for(int j = 0; j < 8; j++) {
        c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
      }
    }
    uint16_t want = (uint16_t)(c ^ 0xffffu);
    CHECK(yab_crc16(d, 12) == want, "table CRC %d", k);
  }
  // a flipped bit: the CRC catches it
  hex2(rec[0].hex, b);
  b[8] ^= 0x10;
  CHECK(yab_frame(b, YAB_BYTES, &f) == YAB_CRC, "a flipped bit fails the CRC");
  // nothing but noise: no sync
  {
    uint16_t t[50];
    for(int i = 0; i < 50; i++) {
      t[i] = (uint16_t)(i * 7 + (i % 3));
    }
    CHECK(yab_decode(t, 50, 21, out, sizeof(out)) < 0, "noise: no frame");
  }
  // the request: Manchester chips of its 54 bits and 2 more
  {
    uint8_t c[200];
    int n = yab_request_chips(c, 200);
    CHECK(n == 2 * 54 + 2, "request chips %d", n);
    CHECK(c[0] == 0 && c[1] == 1 && c[2] == 1 && c[3] == 0, "request starts 0 1 (chips 01 10)");
  }
  // driver state: error until a good frame, tolerates max_err - 1 bad polls
  {
    yab_t d;
    yab_init(&d, 3);
    CHECK(d.state == 0 && d.error == 1, "no frame yet: error");
    hex2(rec[0].hex, b);
    yab_frame(b, YAB_BYTES, &f);
    yab_poll(&d, YAB_OK, &f);
    CHECK(d.state == 3 && d.error == 0, "good frame: state 3");
    yab_poll(&d, YAB_CRC, &f);
    yab_poll(&d, YAB_NOREPLY, &f);
    CHECK(d.state == 3 && d.f.pos == rec[0].pos, "two bad polls: last frame kept");
    yab_poll(&d, YAB_NOSYNC, &f);
    CHECK(d.state == 0 && d.error == 1 && d.crc_er == 1 && d.noreply == 1 && d.frame_er == 1, "three: error");
  }
  printf("%s test_yaskawa_abs: %d/%d passed\n", fails ? "FAIL" : "ok", tests - fails, tests);
  return fails ? 1 : 0;
}
