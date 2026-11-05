#pragma once
/*
 * FANUC 1-pair ("αi interface", "2-wire", "high speed") pulsecoder probe:
 * the hardware-independent part. See docs/fanuci.md.
 *
 * The protocol is not documented. This module only does what a probe
 * needs and the host tests check:
 *  - turn a request bit pattern into one level per bit time for the
 *    transmitter (lead idle, pattern LSB first, tail idle);
 *  - turn the receiver's edge timestamps back into bits;
 *  - pull integer fields out of the bits;
 *  - print the bits for the console dump.
 */
#include <stdint.h>

#define F1P_MAX_BITS 192

/* One level (0/1) per bit time for the request: lead idle bits, then
 * req_bits of req LSB first, then tail idle bits. Levels are logical: the
 * caller maps them to the pin (and applies tx inversion). Returns the
 * number of levels written (at most max). */
int f1p_build_levels(uint8_t *lv, int max, uint32_t req, int req_bits, int lead, int tail, int idle);

/* Decode a capture: t[0..n-1] are timer values at successive edges of the
 * receiver output (both edges), ticks_per_bit is the bit time in timer
 * ticks, idle is the line level before the first edge (the first edge goes
 * to !idle). The level between edge i and i+1 lasts round(dt/ticks) bits
 * (at least 1). After the last edge the line stays at its level until
 * maxbits. Timer values may wrap at 16 bit. Returns the number of bits. */
int f1p_decode(const uint16_t *t, int n, float ticks_per_bit, int idle, uint8_t *bits, int maxbits);

/* An unsigned field of len (1..32) bits starting at bit off; lsb_first: the
 * first bit on the wire is bit 0 of the field. Bits past nbits read as 0. */
uint32_t f1p_field(const uint8_t *bits, int nbits, int off, int len, int lsb_first);

/* The bits as '0'/'1', a space every group bits (0: none); returns length. */
int f1p_format(char *out, int max, const uint8_t *bits, int nbits, int group);

/* The βiA128 1-pair frame (bench 2026-11-04, brdmot probe-log.md): 100 bits
 * at 2.72 Mbit/s, bit 0 the start bit (0), then five 16-bit words LSB first
 * at bits 1+19k .. 16+19k, each followed by the sync 1 1 0 (the last one by
 * 1 1 only), then a CRC-5 (x^5+x^3+x+1, init 0, over bits 0-94, MSB first)
 * at 95-99.
 *   w0: bits 0-7 status (10100000 seen), bits 8-15 position bits 0-7
 *   w1: position bits 8-23 (24 bit per turn)
 *   w2: turns, 16 bit two's complement
 *   w3: bits 0-7 commutation (256 per electrical turn), 8-15 status (bit 10,
 *       frame bit 68, set after the encoder lost its backup: not referenced)
 *   w4: unknown, 0x804B seen */
#define F1P_FRAME_BITS 100
#define F1P_CRC_POLY 0x0B

typedef struct {
  uint16_t w[5];
  uint32_t pos;   // 0 .. 2^24-1
  int16_t turns;
  uint8_t com;    // 0 .. 255 per electrical turn
  uint8_t crc, crc_rx;
} f1p_frame_t;

enum { F1P_OK = 0, F1P_SHORT = -1, F1P_SYNC = -2, F1P_CRC = -3 };

/* CRC-5 of n bits, MSB first, poly F1P_CRC_POLY, init 0 */
uint8_t f1p_crc5(const uint8_t *bits, int n);
/* Decode a frame from received bits (bit 0 = start bit). The words and the
 * CRCs are filled in whatever the result; F1P_OK only if the start bit, every
 * sync and the CRC are right. */
int f1p_frame(const uint8_t *bits, int nbits, f1p_frame_t *f);
/* frame bit (0..99) as stored in the words: -1 if it isn't a word bit */
int f1p_frame_bit(const f1p_frame_t *f, int bit);

/* The same on packed bits, for the driver's rt (the byte arrays above cost
 * ~65 cycles a bit on the F4): bit i is (w[i >> 5] >> (i & 31)) & 1. w holds
 * F1P_PACKED_WORDS words (one spare, read past the end by f1p_frame_packed).
 * Results equal f1p_decode / f1p_frame (host/tests/test_fanuc_tw.c). */
#define F1P_PACKED_WORDS ((F1P_MAX_BITS + 31) / 32 + 1)
int f1p_decode_packed(const uint16_t *t, int n, float ticks_per_bit, int idle, uint32_t *w, int maxbits);
int f1p_frame_packed(const uint32_t *w, int nbits, f1p_frame_t *f);
static inline int f1p_bit_packed(const uint32_t *w, int i) {
  return (w[i >> 5] >> (i & 31)) & 1;
}
