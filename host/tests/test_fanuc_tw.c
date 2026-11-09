// shared/fanuc1p.c (f1p_frame) and shared/fanuc_tw.c: the βiA128 2-wire frame
// and the fanuc_tw driver's state, on frames recorded on the bench
// (brdmot captures/, 2026-11-04/05). Exits 1 on failure.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fanuc1p.h"
#include "fanuc_tw.h"

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

// recorded frames, first bit received first; values decoded offline
// (brdmot tools/f1p_capture.py)
static const struct {
  const char *name, *bits;
  uint32_t pos;
  int turns, com, res, nref;
} rec[] = {
    {"turn 0", "0101000000100101111010110001111110101100111111111111111110111111100000001111011010010000000011100111", 6262226, -2, 127, F1P_OK, 0},
    {"turn 300", "0101000000100111011011101110100110111101101111111111111110011001100000000011011010010000000011110011", 14251890, -5, 102, F1P_OK, 0},
    {"turn 599", "0101000000111100011001011001100010101101110111111111111110111000100000001111011010010000000011101000", 5347870, -9, 71, F1P_OK, 0},
    // after a 5 V cycle: commutation 22 off until the reference is passed
    {"cold 0", "0101000000110111011000011110101111101100110111111111111110000001110000011111011010010000000011111011", 8222838, -10, 224, F1P_OK, 0},
    // one flipped bit (frame bit 7): the CRC catches it
    {"cold 76", "0101000100011010011010110111011111101101010111111111111110010111110000001011011010010000000011101000", 8318252, -11, 250, F1P_CRC, 0},
    // after a night without power and battery: turns 0, bit 68 set
    {"desk 0", "0101000001100011011000110101000001101100000000000000000110000001110010000111011010010000000011110100", 6335587, 0, 224, F1P_OK, 1},
};

static int bits_of(const char *s, uint8_t *b) {
  int n = 0;
  for(; s[n]; n++) {
    b[n] = s[n] == '1';
  }
  return n;
}

// the receiver's edges for a frame: idle 1, the bits, back to idle
static int edges_of(const uint8_t *bits, int n, float tpb, float jitter, unsigned t0, uint16_t *t) {
  int ne = 0, level = 1;
  for(int i = 0; i < n; i++) {
    if(bits[i] != level) {
      float j = jitter * ((float)rand() / RAND_MAX * 2.0f - 1.0f);
      t[ne++] = (uint16_t)(t0 + (unsigned)lroundf(i * tpb + j));
      level   = bits[i];
    }
  }
  if(level != 1) {
    t[ne++] = (uint16_t)(t0 + (unsigned)lroundf(n * tpb));
  }
  return ne;
}

int main(void) {
  srand(5);
  uint8_t b[F1P_MAX_BITS];
  f1p_frame_t f;

  for(unsigned i = 0; i < sizeof(rec) / sizeof(rec[0]); i++) {
    int n   = bits_of(rec[i].bits, b);
    int res = f1p_frame(b, n, &f);
    CHECK(res == rec[i].res, "%s: result %d, want %d", rec[i].name, res, rec[i].res);
    CHECK(f.pos == rec[i].pos, "%s: pos %u, want %u", rec[i].name, (unsigned)f.pos, (unsigned)rec[i].pos);
    CHECK(f.turns == rec[i].turns, "%s: turns %d, want %d", rec[i].name, f.turns, rec[i].turns);
    CHECK(f.com == rec[i].com, "%s: com %d, want %d", rec[i].name, f.com, rec[i].com);
    CHECK(f1p_frame_bit(&f, 68) == rec[i].nref, "%s: bit 68", rec[i].name);
    // the same frame from edge timestamps, as the drive sees it (jitter, wrap)
    uint16_t t[200];
    int ne    = edges_of(b, n, 84e6f / 2721088.0f, 4.0f, 65300u + i * 37, t);
    uint8_t d[F1P_MAX_BITS];
    int nb    = f1p_decode(t, ne, 84e6f / 2721088.0f, 1, d, F1P_FRAME_BITS);
    f1p_frame_t g;
    int res2 = f1p_frame(d, nb, &g);
    CHECK(res2 == rec[i].res && g.pos == f.pos && g.turns == f.turns && g.com == f.com,
          "%s: from edges: result %d pos %u", rec[i].name, res2, (unsigned)g.pos);
    // the packed decoder (the driver's) gives the same, bit for bit
    uint32_t pw[F1P_PACKED_WORDS];
    int pn = f1p_decode_packed(t, ne, 84e6f / 2721088.0f, 1, pw, F1P_FRAME_BITS);
    int same = pn == nb;
    for(int k = 0; same && k < nb; k++) {
      same = f1p_bit_packed(pw, k) == d[k];
    }
    CHECK(same, "%s: packed bits equal the byte decoder's (%d / %d bits)", rec[i].name, pn, nb);
    f1p_frame_t h;
    int res3 = f1p_frame_packed(pw, pn, &h);
    CHECK(res3 == res2 && memcmp(h.w, g.w, sizeof(h.w)) == 0 && h.crc == g.crc && h.crc_rx == g.crc_rx,
          "%s: packed frame result %d, want %d", rec[i].name, res3, res2);
  }

  // what was seen on the bench, checked over the recorded frames
  {
    int n = bits_of(rec[0].bits, b);
    f1p_frame(b, n, &f);
    CHECK(f.w[4] == 0x804B, "w4 0x%04x", f.w[4]);
    CHECK((f.w[0] & 0xff) == 0x05, "w0 status 0x%02x (bits 1-8: 10100000)", f.w[0] & 0xff);
    // commutation = 1024 * pos / 2^24 within a count once referenced
    int e = (int)f.com - (int)((f.pos * 1024ull >> 24) & 255);
    CHECK(abs(e) <= 1, "commutation from the position: off by %d", e);
  }

  // packed decoder on random frames, timer wrap and jitter: equal to the byte decoder
  for(int trial = 0; trial < 500; trial++) {
    uint8_t fr[F1P_FRAME_BITS];
    for(int k = 0; k < F1P_FRAME_BITS; k++) {
      fr[k] = rand() & 1;
    }
    fr[0] = 0;
    uint16_t t[200];
    int ne = edges_of(fr, F1P_FRAME_BITS, 84e6f / 2721088.0f, 5.0f, 65000u + trial * 13, t);
    uint8_t d[F1P_MAX_BITS];
    uint32_t pw[F1P_PACKED_WORDS];
    int nb   = f1p_decode(t, ne, 84e6f / 2721088.0f, 1, d, F1P_FRAME_BITS + trial % 40);
    int pn   = f1p_decode_packed(t, ne, 84e6f / 2721088.0f, 1, pw, F1P_FRAME_BITS + trial % 40);
    int same = pn == nb;
    for(int k = 0; same && k < nb; k++) {
      same = f1p_bit_packed(pw, k) == d[k];
    }
    f1p_frame_t a, c;
    same = same && f1p_frame(d, nb, &a) == f1p_frame_packed(pw, pn, &c) && memcmp(a.w, c.w, sizeof(a.w)) == 0 &&
           a.crc == c.crc && a.crc_rx == c.crc_rx;
    CHECK(same, "random frame %d: packed differs", trial);
  }

  // a broken sync is caught, as is a short frame
  {
    int n = bits_of(rec[0].bits, b);
    b[36] ^= 1;  // the sync after w1
    CHECK(f1p_frame(b, n, &f) == F1P_SYNC, "sync error");
    bits_of(rec[0].bits, b);
    CHECK(f1p_frame(b, 90, &f) == F1P_SHORT, "short frame");
    b[0] = 1;
    CHECK(f1p_frame(b, n, &f) == F1P_SYNC, "start bit 1");
  }

  // driver state
  {
    f2w_t d;
    f2w_init(&d, 3, 68);
    CHECK(d.state == 0 && d.error == 1, "no frame yet: state 0, error");
    f2w_poll(&d, F2W_NOREPLY, 0);
    CHECK(d.state == 0 && d.error == 1 && d.noreply == 1, "no reply before the first frame");
    int n = bits_of(rec[5].bits, b);
    f1p_frame(b, n, &f);
    f2w_poll(&d, F1P_OK, &f);
    CHECK(d.state == 1 && d.error == 0 && d.nref == 1, "bit 68 set: state 1 (incremental), no error");
    n = bits_of(rec[0].bits, b);
    f1p_frame(b, n, &f);
    f2w_poll(&d, F1P_OK, &f);
    CHECK(d.state == 3 && d.nref == 0 && d.f.pos == rec[0].pos, "referenced: state 3");
    CHECK(d.chg[3] & (1 << 10), "the change of bit 68 is recorded (w3 bit 10)");
    f2w_poll(&d, F1P_CRC, &f);
    f2w_poll(&d, F1P_SYNC, &f);
    CHECK(d.state == 3 && d.error == 0 && d.f.pos == rec[0].pos, "2 bad polls (max 3): last frame kept");
    f2w_poll(&d, F2W_NOREPLY, 0);
    CHECK(d.state == 0 && d.error == 1, "3 bad polls in a row: error");
    CHECK(d.crc_er == 1 && d.sync_er == 1 && d.noreply == 2, "counters");
    n = bits_of(rec[1].bits, b);
    f1p_frame(b, n, &f);
    f2w_poll(&d, F1P_OK, &f);
    CHECK(d.state == 3 && d.error == 0 && d.bad_run == 0, "recovers on the next good frame");
    f2w_chg_reset(&d);
    CHECK(!d.chg[0] && !d.chg[3], "chg reset");
    f1p_frame_t a, c;
    n = bits_of(rec[0].bits, b);
    f1p_frame(b, n, &a);
    n = bits_of(rec[5].bits, b);
    f1p_frame(b, n, &c);
    CHECK(f2w_status_differs(&a, &c, 0x00ff, 0x1f00, 0xffff), "status differs (bit 68)");
    CHECK(!f2w_status_differs(&a, &c, 0x00ff, 0x0000, 0xffff), "masked out");
  }

  // the battery alarm (frame bit 6, w0 bit 5): words as traced on the bench
  // with the battery pulled (w0 f225) and back (f205), 2026-11-06
  {
    uint16_t ws[2] = {0xf225, 0xf205};
    for(int k = 0; k < 2; k++) {
      uint16_t w[5] = {ws[k], 0xc0ee, 0x0028, 0x0020, 0x804b};
      uint8_t fb[F1P_FRAME_BITS] = {0};
      for(int j = 0; j < 5; j++) {
        for(int i = 0; i < 16; i++) {
          fb[1 + 19 * j + i] = (w[j] >> i) & 1;
        }
        fb[17 + 19 * j] = 1;
        fb[18 + 19 * j] = 1;  // the third sync bit 0 (and none after w4)
      }
      uint8_t c = f1p_crc5(fb, 95);
      for(int i = 0; i < 5; i++) {
        fb[95 + i] = (c >> (4 - i)) & 1;
      }
      f1p_frame_t f;
      CHECK(f1p_frame(fb, F1P_FRAME_BITS, &f) == F1P_OK, "battery frame %d decodes", k);
      f2w_t d;
      f2w_init(&d, 3, 68);
      f2w_poll(&d, F1P_OK, &f);
      CHECK(d.bat_fail == (k == 0) && d.state == 3, "battery alarm %d: bat_fail %d, still state 3", k, d.bat_fail);
    }
    int n = bits_of(rec[5].bits, b);
    f1p_frame(b, n, &f);
    f2w_t d;
    f2w_init(&d, 3, 68);
    f2w_poll(&d, F1P_OK, &f);
    CHECK(d.bat_fail == 0, "after a night without battery the alarm is off again (bit 68 says it)");
  }

  // ref_move: not referenced blocks the drive (nref_fault, fault 20) unless
  // ref_move lets it run to move through the reference; referenced, nothing blocks
  {
    f2w_t d;
    f2w_init(&d, 3, 68);
    int n = bits_of(rec[5].bits, b);  // not referenced
    f1p_frame(b, n, &f);
    f2w_poll(&d, F1P_OK, &f);
    CHECK(d.nref == 1 && d.nref_fault == 1 && d.state == 1, "not referenced: nref_fault, state 1");
    d.ref_move = 1;
    f2w_poll(&d, F1P_OK, &f);
    CHECK(d.nref == 1 && d.nref_fault == 0 && d.state == 1, "ref_move: no nref_fault, still incremental (state 1)");
    n = bits_of(rec[0].bits, b);  // referenced
    f1p_frame(b, n, &f);
    f2w_poll(&d, F1P_OK, &f);
    CHECK(d.nref == 0 && d.nref_fault == 0 && d.state == 3, "through the reference: state 3");
    d.ref_move = 0;
    f2w_poll(&d, F1P_OK, &f);
    CHECK(d.nref_fault == 0 && d.state == 3, "ref_move off again: still fine");
  }

  printf("%s test_fanuc_tw: %d/%d passed\n", fails ? "FAIL" : "ok", tests - fails, tests);
  return fails ? 1 : 0;
}
