#pragma once
/*
 * yaskawa_abs: Yaskawa Sigma-II serial absolute encoders (the 16-bit
 * absolute of an SGMAH-xxA1..., found on the bench 2026-11-10, brdmot
 * yaskawa.md), the hardware-free part of src/comps/yaskawa_abs.c.
 *
 * The line: half-duplex RS-485, Manchester at 4 Mbit/s (0 -> chips 01,
 * 1 -> chips 10, a chip 125 ns). Request and reply alike: a 0101...
 * preamble, the HDLC flag 01111110, the data with a 0 stuffed after five
 * 1s, the flag again. The reply's data is 14 bytes; the comp stores the
 * bits as received, MSB first in each byte (so a byte read as a number is
 * bit-reversed); bytes 12-13 are a CRC-16 (CCITT 0x1021, init and xorout
 * 0xffff) over bytes 0-11. Fields are LSB first in received bit order:
 *   bits 56-75  single-turn position, 20 bits (2^20 per turn)
 *   bits 76-91  turns, signed 16 bits
 *   bits 0-55   header, status and changing data not understood yet
 */
#include <stdint.h>

#define YAB_BYTES 14

enum { YAB_OK = 0, YAB_NOSYNC = -1, YAB_MANCHESTER = -2, YAB_NOFLAG = -3, YAB_LEN = -4, YAB_CRC = -5 };

/* The request as Manchester chips (one level per 125 ns), for the TX DMA.
 * Returns the number of chips (at most max). */
int yab_request_chips(uint8_t *chips, int max);

/* Decode a reply from the receiver's edge times t[0..n-1] (both edges, a
 * free-running timer), ticks_per_bit the timer ticks per bit (84 MHz / 4
 * Mbit/s = 21). Syncs on the first long interval (the capture starts near
 * the end of the preamble), follows the Manchester mid-bit edges, finds
 * the opening flag (either line polarity), removes the stuffed bits up to
 * the closing flag and packs the data MSB first into bytes; no 14 bytes:
 * again from the next long interval, a few times. Returns the number of
 * data bytes, or a YAB_ error (< 0). */
int yab_decode(const uint16_t *t, int n, int ticks_per_bit, uint8_t *bytes, int maxbytes);

typedef struct {
  uint8_t b[YAB_BYTES];
  uint32_t pos;   // 0 .. 2^20-1
  int16_t turns;
  uint16_t crc, crc_rx;
} yab_frame_t;

uint16_t yab_crc16(const uint8_t *data, int n);
/* The fields of a decoded reply: YAB_OK, YAB_LEN or YAB_CRC. */
int yab_frame(const uint8_t *bytes, int nbytes, yab_frame_t *f);
/* bit k of a reply in received order (0 = the first data bit) */
static inline int yab_bit(const uint8_t *b, int k) {
  return (b[k >> 3] >> (7 - (k & 7))) & 1;
}

/* The driver's state from one poll's result (as shared/fanuc_tw.h). */
#define YAB_NOREPLY 1
typedef struct {
  int max_err;
  uint32_t ok, crc_er, frame_er, noreply, bad_run;
  yab_frame_t f;
  int have;
  int state;  // 0 none / error, 3 good (absolute)
  int error;
} yab_t;
void yab_init(yab_t *d, int max_err);
void yab_poll(yab_t *d, int res, const yab_frame_t *f);
