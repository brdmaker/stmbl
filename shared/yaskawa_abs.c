#include "yaskawa_abs.h"

// sync preamble, flag, the data 0xffff with its stuffed zeros, flag (the
// request the old yaskawa comp sends, which this encoder answers)
static const char request[] = "0101010101010101010 01111110 111110 111110 111110 1 01111110";

int yab_request_chips(uint8_t *chips, int max) {
  int n = 0;
  for(int i = 0; request[i] && n + 2 <= max; i++) {
    if(request[i] == '0') {
      chips[n++] = 0;
      chips[n++] = 1;
    } else if(request[i] == '1') {
      chips[n++] = 1;
      chips[n++] = 0;
    }
  }
  for(int i = 0; i < 2 && n < max; i++) {
    chips[n++] = 0;  // the line low at the end, as the old comp
  }
  return n;
}

#define YAB_TRIES 3      // every rt cycle: few
#define YAB_FLAGBITS 48  // the opening flag within this many bits (the preamble is 19)

// One pass over the edges. It runs every rt cycle (~200 edges, 5000/s) on
// the F4, so it is written for registers: the bit timing a constant (the
// comp's 21 ticks: inlined below), sentinel edges instead of bounds checks,
// plain differences (no 16-bit wrap), the bytes through a pointer, a
// sentinel bit in acc for "a byte full". (The first version kept counters
// and limits on the stack: ~50 cycles a bit, 7400 a frame; RT_TOO_LONG with
// id_mot, bench 2026-11-10.)
//
// Manchester: edges at every mid-bit, and at bit boundaries between equal
// bits. From a mid-bit edge, a long interval (a bit) reaches the next
// mid-bit edge; a short one (half a bit) reaches a boundary edge, and a
// second short one the next mid-bit edge. The bit is the level before its
// mid-bit edge (0 -> 01 rises, 1 -> 10 falls); levels alternate edge by
// edge, the first level isn't known: the bits are taken for level 0 before
// the start edge, the opening flag tells the polarity (01111110, or
// 10000001 inverted; the preamble's 0101... holds neither).
// The start: the first long interval at or after edge `from` (both its
// edges are mid-bit). Not a long preamble: the drive starts listening while
// the encoder already sends it (a capture on the bench, 2026-11-10, began
// a bit or two before the flag). *next: where to look again.
// A short half-bit right after a long one can arrive late, on top of the
// next edge (the line not settled: bench, SGMAH, 2026-11-10, ~5 % of frames
// at some positions): a long and then ~0 is two shorts, and in the
// short-short step a long then ~0 is a short.
// The times don't wrap within a reply (the comp starts its timer at 0 for
// each capture; a reply is ~3000 of 65536 ticks). Two sentinel edges after
// the last one, a gap each, end the steps.
// HDLC: the flag within YAB_FLAGBITS bits, then a 0 after five 1s is
// stuffed; six 1s are the closing flag, its leading 0 the last data bit
// taken. *late: the flag was found (a failure from there is the frame's: no
// new start, so rt pays one pass at most over a bad frame).
static inline __attribute__((always_inline)) int decode_from(uint16_t *t, int n, const int tpb, int from,
                                                             uint8_t *bytes, int maxbytes, int *next, int *late) {
  const unsigned lim4 = (unsigned)tpb * 3;  // 4 * the threshold: 3/4 of a bit, between short (1/2) and long (1)
  const unsigned gap  = (unsigned)(tpb + tpb / 2);
  const unsigned tiny = (unsigned)tpb;  // 4 * an interval below it: ~0, a late edge
  if(n < 2) {
    return YAB_NOSYNC;
  }
  t[n]              = (uint16_t)(t[n - 1] + 4 * tpb);  // the sentinels
  t[n + 1]          = (uint16_t)(t[n] + 4 * tpb);
  const uint16_t *p = t + from, *end = t + n - 1;  // end: the last edge
  while(p < end && !(4u * ((unsigned)p[1] - p[0]) > lim4 && (unsigned)p[1] - p[0] <= gap)) {
    p++;
  }
  if(p >= end) {
    return YAB_NOSYNC;
  }
  *next          = (int)(p - t) + 1;
  unsigned level = 0;

// to the next mid-bit edge (p); the reply's end (a gap, the sentinels): goto out
#define YAB_STEP(out)                                                       \
  do {                                                                      \
    unsigned d_ = (unsigned)p[1] - p[0];                                    \
    if(d_ > gap) {                                                          \
      goto out;                                                             \
    }                                                                       \
    if(4 * d_ > lim4) {                                                     \
      if(4 * ((unsigned)p[2] - p[1]) < tiny) {                              \
        p += 2; /* a late edge: two shorts */                               \
      } else {                                                              \
        p += 1;                                                             \
        level ^= 1;                                                         \
      }                                                                     \
    } else {                                                                \
      unsigned d2_ = (unsigned)p[2] - p[1];                                 \
      if(d2_ > gap) {                                                       \
        goto out;                                                           \
      }                                                                     \
      if(4 * d2_ > lim4 && 4 * ((unsigned)p[3] - p[2]) >= tiny) {           \
        return YAB_MANCHESTER; /* short, long (not a late edge): broken */  \
      }                                                                     \
      p += 2;                                                               \
    }                                                                       \
  } while(0)

  // the opening flag
  unsigned sr = 0, inv = 0;
  for(int k = 1;; k++) {
    sr = ((sr << 1) | level) & 0xff;
    if(k >= 8 && (sr == 0x7e || sr == 0x81)) {
      inv = sr == 0x81;
      break;
    }
    if(k >= YAB_FLAGBITS) {
      return YAB_NOFLAG;
    }
    YAB_STEP(noflag);
  }
  *late = 1;
  // the data
  {
    uint8_t *o = bytes, *oend = bytes + maxbytes;
    unsigned ones = 0, acc = 1;  // acc: the bits under a sentinel 1; at bit 8 a byte is full
    while(1) {
      YAB_STEP(noflag);
      unsigned b = level ^ inv;
      if(ones == 5) {
        if(b) {
          return (int)(o - bytes);  // six 1s: the closing flag
        }
        ones = 0;  // a stuffed 0
        continue;
      }
      ones = b ? ones + 1 : 0;
      acc  = (acc << 1) | b;
      if(acc & 0x100) {
        if(o == oend) {
          return YAB_NOFLAG;  // longer than any reply
        }
        *o++ = (uint8_t)acc;
        acc  = 1;
      }
    }
  }
noflag:
  return YAB_NOFLAG;  // no flag, or no closing flag
#undef YAB_STEP
}

static int decode_any(uint16_t *t, int n, int tpb, int from, uint8_t *bytes, int maxbytes, int *next, int *late) {
  return decode_from(t, n, tpb, from, bytes, maxbytes, next, late);
}

int yab_decode(uint16_t *t, int n, int ticks_per_bit, uint8_t *bytes, int maxbytes) {
  int from = 0, r = YAB_NOSYNC;
  for(int tries = 0; tries < YAB_TRIES; tries++) {  // a stray edge first: sync again from the next long interval
    int next = n, late = 0;
    int nb   = ticks_per_bit == 21 ? decode_from(t, n, 21, from, bytes, maxbytes, &next, &late)
                                   : decode_any(t, n, ticks_per_bit, from, bytes, maxbytes, &next, &late);
    if(nb == YAB_NOSYNC) {
      return tries ? r : nb;
    }
    r = nb;
    if(nb >= YAB_BYTES || late) {
      return nb;
    }
    from = next;
  }
  return r;
}

// CRC-16 CCITT (0x1021), a byte at a time: crc = (crc << 8) ^ tab[(crc >> 8) ^ byte]
static const uint16_t crc_tab[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
    0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
    0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
    0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
    0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
    0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
    0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
    0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
    0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
    0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
    0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
    0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
    0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
    0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
    0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
    0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
    0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
    0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
    0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
    0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
    0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0};

uint16_t yab_crc16(const uint8_t *data, int n) {
  uint16_t c = 0xffff;
  for(int i = 0; i < n; i++) {
    c = (uint16_t)((c << 8) ^ crc_tab[((c >> 8) ^ data[i]) & 0xff]);
  }
  return c ^ 0xffff;
}

// bit-reversed: on the F4 one instruction
static inline uint32_t rev32(uint32_t x) {
#ifdef __arm__
  __asm__("rbit %0, %1" : "=r"(x) : "r"(x));
  return x;
#else
  x = ((x >> 1) & 0x55555555u) | ((x & 0x55555555u) << 1);
  x = ((x >> 2) & 0x33333333u) | ((x & 0x33333333u) << 2);
  x = ((x >> 4) & 0x0f0f0f0fu) | ((x & 0x0f0f0f0fu) << 4);
  x = ((x >> 8) & 0x00ff00ffu) | ((x & 0x00ff00ffu) << 8);
  return (x >> 16) | (x << 16);
#endif
}

int yab_frame(const uint8_t *bytes, int nbytes, yab_frame_t *f) {
  // the data ends with the closing flag's leading 0: one bit more than 14 bytes
  if(nbytes < YAB_BYTES) {
    return YAB_LEN;
  }
  for(int i = 0; i < YAB_BYTES; i++) {
    f->b[i] = bytes[i];
  }
  // received bits 56-87 (bytes 7-10, MSB first) reversed: bit 56 + j at j
  uint32_t r1 = rev32((uint32_t)bytes[7] << 24 | (uint32_t)bytes[8] << 16 | (uint32_t)bytes[9] << 8 | bytes[10]);
  uint32_t r2 = rev32((uint32_t)bytes[11] << 24);  // bits 88-95
  f->pos      = r1 & 0xfffff;                                           // bits 56-75
  f->turns    = (int16_t)(uint16_t)((r1 >> 20) | ((r2 & 0xf) << 12));  // bits 76-91
  f->crc      = yab_crc16(bytes, 12);
  f->crc_rx   = (uint16_t)((bytes[12] << 8) | bytes[13]);
  return f->crc == f->crc_rx ? YAB_OK : YAB_CRC;
}

void yab_init(yab_t *d, int max_err) {
  uint8_t *p = (uint8_t *)d;
  for(unsigned i = 0; i < sizeof(*d); i++) {
    p[i] = 0;
  }
  d->max_err = max_err < 1 ? 1 : max_err;
  d->error   = 1;
}

void yab_poll(yab_t *d, int res, const yab_frame_t *f) {
  if(res == YAB_OK) {
    d->f       = *f;
    d->have    = 1;
    d->ok++;
    d->bad_run = 0;
    d->state   = 3;
    d->error   = 0;
    return;
  }
  if(res == YAB_NOREPLY) {
    d->noreply++;
  } else if(res == YAB_CRC) {
    d->crc_er++;
  } else {
    d->frame_er++;
  }
  d->bad_run++;
  if(d->bad_run >= (uint32_t)d->max_err || !d->have) {
    d->state = 0;
    d->error = 1;
  }
}
