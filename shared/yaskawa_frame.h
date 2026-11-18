// The Yaskawa Sigma serial encoder's reply (src/comps/yaskawa.c): decoding
// the edge times the capture timer records, and what to do with a bad
// reply. Kept here so the host tests it (host/tests/test_yaskawa_frame.c).
//
// The reply: Manchester (1: high low, 0: low high), a preamble, the HDLC
// flag 01111110 (its six ones give the 10 short intervals the sync looks
// for), the data bit-stuffed (a 0 after five ones), the flag again. The
// data's 14 bytes carry the position (yaskawa0.off/len, 64/15) and a CRC-16
// over bytes 0..11 in bytes 12..13.
//
// Before 2026-11-11 (DEBUGNOTES) the decoder wrote its bits without a
// limit: every good reply wrote its end flag one byte past reply[14], and a
// reply with no end flag (noise) wrote up to ~280 bits past both buffers;
// it read one capture past the last. A Manchester code error was flagged
// and then overwritten, and the position of a reply with a bad CRC was
// used anyway: every noisy reply became a position jump.
#pragma once
#include <stdint.h>
#include "yaskawa_crc16.h"

#define YASK_REPLY_BYTES 14
#define YASK_DATA_BITS (YASK_REPLY_BYTES * 8)
#define YASK_BITS_MAX 150  // decoded bits kept (data + end flag), for dump/probes

enum {
  YASK_OK,
  YASK_SHORT,    // 80 edges or fewer: no reply
  YASK_NO_SYNC,  // no start flag
  YASK_CODE,     // not Manchester: a half bit alone (a glitch)
  YASK_FRAME,    // no end flag, or too few data bits
  YASK_CRC,      // the CRC doesn't match
};

typedef struct {
  uint8_t reply[YASK_REPLY_BYTES];
  char m[YASK_BITS_MAX + 1];  // decoded bits '0' / '1', 'H' at the end flag, 0-terminated
  int nbits;                  // bits in m
} yask_frame_t;

static inline int yask_decode(const uint16_t *cap, int count, uint16_t bit_time, yask_frame_t *fr) {
  for(int i = 0; i < YASK_REPLY_BYTES; i++) {
    fr->reply[i] = 0;
  }
  fr->m[0]  = 0;
  fr->nbits = 0;
  if(count <= 80) {
    return YASK_SHORT;
  }
#define YASK_SHORT_AT(i) ((uint16_t)(cap[(i) + 1] - cap[(i)]) < bit_time)
  // the start flag's six ones: 10 short intervals, then a long one
  int counter = 0, start = -1;
  for(int i = 1; i + 1 < count; i++) {
    if(YASK_SHORT_AT(i)) {
      counter++;
    } else if(counter == 10) {
      start = i + 1;
      break;
    } else {
      counter = 0;
    }
  }
  if(start < 0) {
    return YASK_NO_SYNC;
  }
  // from mid bit to mid bit: two short intervals, the same bit again; one
  // long, the other bit. counter goes on from the sync's 10 (as before)
  int pol = 0, w = 0, end = 0;
  for(int i = start; i + 1 < count && w < YASK_BITS_MAX; i++) {
    if(YASK_SHORT_AT(i)) {
      i++;
      if(i + 1 >= count || !YASK_SHORT_AT(i)) {
        fr->m[w]  = 0;
        fr->nbits = w;
        return YASK_CODE;
      }
    } else {
      pol = 1 - pol;
    }
    if(pol == 1) {
      counter++;
      if(w < YASK_DATA_BITS) {
        fr->reply[w / 8] |= 1 << (7 - (w % 8));
      }
      fr->m[w++] = '1';
    } else if(counter == 5) {  // a stuffed 0
      counter = 0;
    } else if(counter == 6) {  // six ones and a 0: the end flag
      fr->m[w++] = 'H';
      end        = 1;
      break;
    } else {
      counter    = 0;
      fr->m[w++] = '0';
    }
  }
#undef YASK_SHORT_AT
  fr->m[w]  = 0;
  fr->nbits = w;
  // the end flag decodes as 0111111H after the data
  if(!end || w - 8 < YASK_DATA_BITS) {
    return YASK_FRAME;
  }
  yaskawa_crc16_t crc  = yaskawa_crc16_finalize(yaskawa_crc16_update(yaskawa_crc16_init(), fr->reply, 12));
  yaskawa_crc16_t data = (fr->reply[13] & 0xff) | (fr->reply[12] << 8);
  return data == crc ? YASK_OK : YASK_CRC;
}

// len bits from bit off, LSB first; bits past the decoded ones read 0
static inline uint32_t yask_bits(const yask_frame_t *fr, int off, int len) {
  uint32_t v = 0;
  for(int i = 0; i < len && i < 32; i++) {
    int k = off + i;
    if(k >= 0 && k < fr->nbits && fr->m[k] == '1') {
      v |= 1u << i;
    }
  }
  return v;
}

// What the component reports, reply by reply. A good reply: its position.
// A bad one (any status but YASK_OK): the last position moved on by the
// last step (constant speed for a reply or two), and error only once more
// than max_bad replies in a row were bad, or before the first good one.
// error goes to fault0.mot_fb_error: one noisy reply no longer stops the
// axis, and no noisy reply moves it.
typedef struct {
  uint32_t last;  // position, counts mod 2^len
  int32_t step;   // last good step, counts
  int valid;      // a good reply seen
  uint32_t bad;   // bad replies in a row
} yask_track_t;

static inline uint32_t yask_track(yask_track_t *t, int good, uint32_t raw, int len, uint32_t max_bad, int *error) {
  uint32_t mask = len >= 32 ? 0xffffffffu : (1u << len) - 1;
  if(good) {
    if(t->valid) {
      int32_t d = (int32_t)((raw - t->last) & mask);
      if(d > (int32_t)(mask >> 1)) {
        d -= (int32_t)mask + 1;  // the shorter way round
      }
      t->step = d;
    }
    t->last  = raw & mask;
    t->valid = 1;
    t->bad   = 0;
    *error   = 0;
    return t->last;
  }
  t->bad++;
  if(t->valid) {
    t->last = (t->last + (uint32_t)t->step) & mask;
  }
  *error = !t->valid || t->bad > max_bad;
  return t->last;
}
