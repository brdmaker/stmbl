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

// two more, with the motor wired (2026-11-10): a short half-bit after a long
// one arrived late, on the next edge (an interval of 0 ticks). Their bytes:
// the late edge moved back half a bit, CRC good.
static const uint16_t cap_late1[] = {
    8, 29, 50, 61, 71, 92, 102, 113, 123, 134, 144, 155, 165, 176, 186, 197, 218, 239, 260, 281, 302, 313,
    323, 334, 344, 355, 376, 376, 386, 407, 428, 439, 449, 460, 470, 481, 491, 512, 533, 544, 554, 575, 585,
    596, 606, 617, 627, 638, 659, 670, 680, 691, 701, 712, 722, 743, 753, 764, 785, 806, 827, 838, 848, 859,
    869, 890, 911, 932, 953, 974, 984, 995, 1005, 1016, 1037, 1058, 1079, 1090, 1100, 1121, 1131, 1142, 1163,
    1184, 1205, 1216, 1226, 1247, 1257, 1268, 1289, 1310, 1331, 1342, 1352, 1373, 1394, 1415, 1436, 1457,
    1478, 1489, 1499, 1520, 1530, 1541, 1551, 1562, 1583, 1604, 1614, 1625, 1635, 1646, 1667, 1678, 1688,
    1709, 1719, 1730, 1751, 1762, 1772, 1793, 1814, 1835, 1856, 1867, 1877, 1888, 1898, 1909, 1919, 1930,
    1940, 1961, 1982, 1993, 2003, 2014, 2024, 2035, 2045, 2056, 2066, 2077, 2087, 2098, 2108, 2119, 2129,
    2140, 2150, 2161, 2171, 2182, 2192, 2203, 2213, 2224, 2234, 2255, 2276, 2287, 2297, 2318, 2339, 2350,
    2360, 2381, 2391, 2402, 2412, 2423, 2433, 2444, 2465, 2476, 2486, 2507, 2517, 2528, 2538, 2549, 2570,
    2581, 2591, 2612, 2622, 2633, 2643, 2654, 2664, 2675, 2685, 2696, 2706, 2717, 2738};
static const uint16_t cap_late2[] = {
    7, 28, 49, 60, 70, 91, 101, 112, 122, 133, 143, 154, 164, 175, 185, 196, 217, 238, 259, 280, 301, 312,
    322, 333, 343, 364, 364, 375, 385, 406, 427, 438, 448, 459, 469, 480, 490, 511, 532, 543, 553, 574, 584,
    595, 616, 637, 658, 679, 700, 711, 721, 732, 742, 763, 784, 795, 805, 826, 836, 847, 868, 879, 889, 900,
    910, 931, 941, 952, 973, 984, 994, 1015, 1025, 1036, 1057, 1078, 1088, 1099, 1120, 1131, 1141, 1162, 1183,
    1204, 1214, 1225, 1246, 1267, 1277, 1288, 1309, 1330, 1340, 1351, 1361, 1372, 1382, 1393, 1403, 1414,
    1435, 1446, 1456, 1477, 1498, 1509, 1519, 1540, 1550, 1561, 1571, 1582, 1603, 1624, 1634, 1645, 1655,
    1666, 1687, 1698, 1708, 1729, 1739, 1750, 1771, 1782, 1792, 1813, 1834, 1855, 1876, 1887, 1897, 1908,
    1918, 1929, 1939, 1950, 1960, 1981, 2002, 2013, 2023, 2034, 2044, 2055, 2065, 2076, 2086, 2097, 2107,
    2118, 2128, 2139, 2149, 2160, 2170, 2181, 2191, 2202, 2212, 2223, 2233, 2244, 2254, 2265, 2275, 2286,
    2296, 2317, 2327, 2338, 2359, 2380, 2401, 2422, 2443, 2464, 2474, 2485, 2506, 2517, 2527, 2548, 2569,
    2580, 2590, 2601, 2611, 2632, 2642, 2653, 2663, 2674, 2684, 2695, 2705, 2716, 2726, 2737, 2758};
static const char *cap_late_hex[] = {"a084f0d15d34d2a77328200093ce", "a084d44c66cb6fa7732820003564"};


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
        int ne = reply_edges(b, inv, trial % 20, 2.0f, 100u + trial * 97, t, 590);  // ±2 ticks an edge; from 0, as the comp's timer
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
    uint16_t t[sizeof(cap1) / sizeof(cap1[0]) + 2];
    memcpy(t, cap1, sizeof(cap1));
    int nb = yab_decode(t, sizeof(cap1) / sizeof(cap1[0]), 21, out, sizeof(out));
    int r  = nb >= YAB_BYTES ? yab_frame(out, nb, &f) : nb;
    hex2(cap1_hex, b);
    CHECK(r == YAB_OK && memcmp(out, b, YAB_BYTES) == 0, "bench capture: %d bytes, result %d", nb, r);
  }
  // the late edges
  {
    const uint16_t *caps[] = {cap_late1, cap_late2};
    int ns[]               = {sizeof(cap_late1) / sizeof(cap_late1[0]), sizeof(cap_late2) / sizeof(cap_late2[0])};
    for(int k = 0; k < 2; k++) {
      uint16_t t[300];
      memcpy(t, caps[k], sizeof(t[0]) * ns[k]);
      int nb = yab_decode(t, ns[k], 21, out, sizeof(out));
      int r  = nb >= YAB_BYTES ? yab_frame(out, nb, &f) : nb;
      hex2(cap_late_hex[k], b);
      CHECK(r == YAB_OK && memcmp(out, b, YAB_BYTES) == 0, "bench capture with a late edge %d: %d bytes, result %d", k,
            nb, r);
    }
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
    uint16_t t[52];
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
