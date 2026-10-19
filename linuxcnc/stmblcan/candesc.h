/*
 * Device descriptions for stmblcan (docs/can_support.md, "Pin inventory"):
 * one text format, whether a device hands it out over CAN (discovery) or
 * it comes from a file (a device without our firmware). Parsed into one
 * table; the generic codec moves values between that table's fields and
 * the frames' bits.
 *
 *   device iomod                      the name (HAL pins: stmblcan.B.iomod.*)
 *   node 32                           node ID (a discovered device knows it)
 *   profile stmbl1                    stmbl's own frames (shared/canproto.h):
 *   vel_scale 100                       no frame lines needed
 *   frame status 0x1A0 8 rx           name, ID ("0x180+node" works too),
 *                                     length, rx: from the device, tx: to it
 *   frame outputs 0x220 4 tx every 2  sent every 2nd period
 *     di    u8   0                    field: name type bit-position [opts]
 *     ain0  u12  8  scale=0.000805    types: bit, u1..u32, s2..s32, f32;
 *     temp  s16  24 scale=0.1 offset=-40   little endian, bit 0 = byte 0's LSB
 *     ready bit  40                   value = raw * scale + offset
 * '#' starts a comment. Lines at most 127 characters.
 */
#pragma once
#include <stdint.h>
#include "canproto.h"

#define CANDESC_FRAMES 4
#define CANDESC_FIELDS 16
#define CANDESC_NAME 24

enum { CD_BIT, CD_UINT, CD_SINT, CD_F32 };
enum { CD_RX, CD_TX };  // seen from LinuxCNC: rx comes from the device

typedef struct {
  char name[CANDESC_NAME];
  uint8_t type, pos, len;  // bit position and length in the frame
  float scale, offset;
} candesc_field_t;

typedef struct {
  char name[CANDESC_NAME];
  uint16_t id;
  uint8_t dlc, dir, every;
  int nfields;
  candesc_field_t field[CANDESC_FIELDS];
} candesc_frame_t;

typedef struct {
  char name[CANDESC_NAME];
  int node;
  int stmbl;        // profile stmbl1: the built-in frames
  float vel_scale;  // (profile stmbl1)
  int nframes;
  candesc_frame_t frame[CANDESC_FRAMES];
} candesc_t;

// parse text (one device); 0 ok, else the line number of the error, with
// the reason in err
int candesc_parse(candesc_t *d, const char *text, char *err, int errlen);
// a text with several devices, each from its "device" line: up to max
// into out, the number into *count. 0 ok, else the line number (of the
// whole text) of the error, with the reason in err
int candesc_parse_all(const char *text, candesc_t *out, int max, int *count, char *err, int errlen);
// a frame's fields from / into values (one float per field, bits as 0 / 1)
void candesc_decode(const candesc_frame_t *f, const can_frame_t *cf, float *values);
void candesc_encode(const candesc_frame_t *f, const float *values, can_frame_t *cf);
// stmbl's own description, as a drive hands it out (shared/comps/can.c)
int candesc_stmbl_text(char *buf, int len, float vel_scale);
