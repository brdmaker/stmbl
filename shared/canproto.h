/*
 * stmbl over CAN, classic frames (docs/can_support.md): what LinuxCNC's
 * stmblcan component and a drive's CAN component exchange. Header only,
 * plain C, used on both sides and in the host tests.
 *
 * Per bus and servo period:
 *   0x080          SYNC from the host, 1 byte: a counter (wraps at 256)
 *   0x200 + node   command, 8 bytes
 *   0x180 + node   feedback, 8 bytes (sent by the drive after SYNC)
 *
 * command:  0..3 pos_cmd (float, little endian)  4..5 vel_cmd (int16,
 *           full scale = vel_scale)  6 flags  7 console data
 * feedback: 0..3 pos_fb  4..5 vel_fb (int16) or, when the SYNC counter
 *           % 8 == 7, a slot value  6 flags  7 console data
 * flags:    command:  0 enable, 1 index_enable, 2..5 out 0..3
 *           feedback: 0 fault,  1 index,        2..5 in 0..3
 *           both:     6 console toggle, 7 console ack
 * The console is shared/sscon.h's 10-bit field: data byte 7, toggle and
 * ack in flags 6 and 7.
 *
 * Slots: the feedback's bytes 4..5 in every 8th period carry slot
 * (counter / 8) % CANP_SLOTS instead of vel_fb, as an int16 with the
 * slot's own full scale: current, power, temperatures at 1 kHz / 32 each.
 */
#pragma once
#include <stdint.h>
#include <string.h>

#define CANP_SYNC_ID 0x080
#define CANP_CMD_ID(node) (0x200 + (node))
#define CANP_FB_ID(node) (0x180 + (node))
#define CANP_NODE_MAX 127

#define CANP_SLOTS 4
enum { CANP_SLOT_CURRENT, CANP_SLOT_POWER, CANP_SLOT_TEMP, CANP_SLOT_VBUS };
static const float canp_slot_scale[CANP_SLOTS] = {50.0f, 5000.0f, 200.0f, 1000.0f};  // A, W, degC, V

typedef struct {
  uint16_t id;
  uint8_t dlc;
  uint8_t d[8];
} can_frame_t;

typedef struct {
  float pos, vel;
  uint8_t enable, index_enable, out;  // out: 4 bits
  uint16_t con;                       // the 10-bit console field (shared/sscon.h)
} canp_cmd_t;

typedef struct {
  float pos, vel;
  uint8_t fault, index, in;  // in: 4 bits
  uint16_t con;
} canp_fb_t;

static inline int16_t canp_q16(float v, float scale) {
  float x = scale > 0.0f ? v / scale * 32767.0f : 0.0f;
  x       = x > 32767.0f ? 32767.0f : x < -32767.0f ? -32767.0f : x;
  return (int16_t)(x >= 0.0f ? x + 0.5f : x - 0.5f);
}
static inline float canp_f16(int16_t q, float scale) {
  return q * scale / 32767.0f;
}
static inline void canp_put16(uint8_t *p, int16_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)((uint16_t)v >> 8);
}
static inline int16_t canp_get16(const uint8_t *p) {
  return (int16_t)(p[0] | p[1] << 8);
}

static inline int canp_slot_period(uint8_t counter) {
  return (counter & 7) == 7;
}
static inline int canp_slot_of(uint8_t counter) {
  return (counter >> 3) % CANP_SLOTS;
}

static inline void canp_sync(can_frame_t *f, uint8_t counter) {
  f->id   = CANP_SYNC_ID;
  f->dlc  = 1;
  f->d[0] = counter;
}

static inline void canp_cmd_encode(can_frame_t *f, int node, const canp_cmd_t *c, float vel_scale) {
  f->id  = CANP_CMD_ID(node);
  f->dlc = 8;
  memcpy(f->d, &c->pos, 4);  // little endian on ARM, x86 and the Pi
  canp_put16(f->d + 4, canp_q16(c->vel, vel_scale));
  f->d[6] = (c->enable ? 1 : 0) | (c->index_enable ? 2 : 0) | (uint8_t)((c->out & 15) << 2) |
            (uint8_t)(((c->con >> 8) & 3) << 6);
  f->d[7] = (uint8_t)c->con;
}

static inline int canp_cmd_decode(const can_frame_t *f, canp_cmd_t *c, float vel_scale) {
  if(f->dlc != 8) {
    return -1;
  }
  memcpy(&c->pos, f->d, 4);
  c->vel          = canp_f16(canp_get16(f->d + 4), vel_scale);
  c->enable       = f->d[6] & 1;
  c->index_enable = (f->d[6] >> 1) & 1;
  c->out          = (f->d[6] >> 2) & 15;
  c->con          = (uint16_t)(f->d[7] | ((f->d[6] >> 6) & 3) << 8);
  return 0;
}

// slot_value is used in slot periods (the SYNC counter that this answers)
static inline void canp_fb_encode(can_frame_t *f, int node, const canp_fb_t *b, float vel_scale, uint8_t counter,
                                  const float *slots) {
  f->id  = CANP_FB_ID(node);
  f->dlc = 8;
  memcpy(f->d, &b->pos, 4);
  if(canp_slot_period(counter)) {
    int s = canp_slot_of(counter);
    canp_put16(f->d + 4, canp_q16(slots ? slots[s] : 0.0f, canp_slot_scale[s]));
  } else {
    canp_put16(f->d + 4, canp_q16(b->vel, vel_scale));
  }
  f->d[6] = (b->fault ? 1 : 0) | (b->index ? 2 : 0) | (uint8_t)((b->in & 15) << 2) |
            (uint8_t)(((b->con >> 8) & 3) << 6);
  f->d[7] = (uint8_t)b->con;
}

// vel is left as it was in slot periods; the slot value goes to slots[]
static inline int canp_fb_decode(const can_frame_t *f, canp_fb_t *b, float vel_scale, uint8_t counter, float *slots) {
  if(f->dlc != 8) {
    return -1;
  }
  memcpy(&b->pos, f->d, 4);
  if(canp_slot_period(counter)) {
    int s = canp_slot_of(counter);
    if(slots) {
      slots[s] = canp_f16(canp_get16(f->d + 4), canp_slot_scale[s]);
    }
  } else {
    b->vel = canp_f16(canp_get16(f->d + 4), vel_scale);
  }
  b->fault = f->d[6] & 1;
  b->index = (f->d[6] >> 1) & 1;
  b->in    = (f->d[6] >> 2) & 15;
  b->con   = (uint16_t)(f->d[7] | ((f->d[6] >> 6) & 3) << 8);
  return 0;
}
