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
