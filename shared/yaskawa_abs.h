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
 *   bits 0-55   header, status and changing data, partly understood:
 *     byte 0 (as received) bit 0x20: low battery, set below ~2.75 V (the
 *       count is lost below ~2.3 V); follows the voltage, no latch
 *     byte 1 bits 0x1f: the battery status, 0x18 OK, 0x04 after the battery
 *       was lost or failed the encoder's load test (every ~6 min) while
 *       powered: latched until the encoder's power is cycled with a good
 *       battery; bits 0xe0 a counter
 *   A turn count lost with the encoder unpowered is not flagged at all
 *   (brdmot yaskawa.md, battery tests 3 and 4, 2026-11-20).
 */
#include <stdint.h>

#define YAB_BYTES 14

enum { YAB_OK = 0, YAB_NOSYNC = -1, YAB_MANCHESTER = -2, YAB_NOFLAG = -3, YAB_LEN = -4, YAB_CRC = -5 };

/* The request as Manchester chips (one level per 125 ns), for the TX DMA.
 * Returns the number of chips (at most max). */
int yab_request_chips(uint8_t *chips, int max);

/* A request with other data (bench: trying the encoder's other commands):
 * the preamble, the flag, len data bytes (at most 6, MSB first: as on the
 * line, as the reply's bytes are shown), with crc != 0 a CRC-16 as the
 * reply's (yab_crc16) after them, bit-stuffed, the flag, 2 low chips.
 * {0xff, 0xff} without a CRC is the request above. Returns the number of
 * chips, or -1 when they don't fit in max. */
int yab_request_data_chips(const uint8_t *data, int len, int crc, uint8_t *chips, int max);

/* Decode a reply from the receiver's edge times t[0..n-1] (both edges, a
 * free-running timer), ticks_per_bit the timer ticks per bit (84 MHz / 4
 * Mbit/s = 21). Syncs on the first long interval (the capture starts near
 * the end of the preamble), follows the Manchester mid-bit edges, finds
 * the opening flag (either line polarity), removes the stuffed bits up to
 * the closing flag and packs the data MSB first into bytes; no 14 bytes:
 * again from the next long interval, a few times. Returns the number of
 * data bytes, or a YAB_ error (< 0).
 * t needs room for n + 2 values: the two after the last edge are written
 * (sentinels). The times must not wrap within the reply (the comp starts
 * its timer at 0 for each capture). */
int yab_decode(uint16_t *t, int n, int ticks_per_bit, uint8_t *bytes, int maxbytes);

typedef struct {
  uint8_t b[YAB_BYTES];
  uint32_t pos;   // 0 .. 2^20-1
  int16_t turns;
  uint16_t crc, crc_rx;
  uint8_t bat_status;  // byte 1 & 0x1f: YAB_BAT_OK, 0x04 the battery was lost
  uint8_t bat_low;     // byte 0 & 0x20: the battery is low (warning)
} yab_frame_t;
#define YAB_BAT_OK 0x18

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
  int bat_fail;  // the last good frame's battery status is not OK (to fault0.mot_fb_bat)
  int bat_low;   // the last good frame says low battery
} yab_t;
void yab_init(yab_t *d, int max_err);
void yab_poll(yab_t *d, int res, const yab_frame_t *f);
