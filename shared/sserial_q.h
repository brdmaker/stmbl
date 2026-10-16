/*
 * Noise-shaped quantisation of a value into a signed n-bit smart-serial
 * process data field (hostmot2 decodes it as raw / 2^(n-1) * full scale).
 *
 * A field narrower than the value's resolution (e.g. power in 8 bits, 20 W
 * a step) is rounded with error feedback: each frame's rounding error goes
 * into the next one. A single frame is off by up to a step, but the average
 * over a few frames is the value's: what anything integrating it (a fuse
 * model, an energy meter, a low-pass) sees. Saturation does not wind up.
 */
#pragma once
#include <stdint.h>

typedef struct {
  float err;  // rounding error carried into the next frame [steps]
} ssq_t;

// v in units, lsb = full scale / 2^(n-1), max = 2^(n-1) - 1
static inline int32_t ssq_encode(ssq_t *q, float v, float lsb, int32_t max) {
  float x   = v / lsb + q->err;
  float r   = x >= 0.0f ? (float)(int32_t)(x + 0.5f) : -(float)(int32_t)(-x + 0.5f);
  int32_t i = (int32_t)r;
  if(i > max) {
    i = max;
  } else if(i < -max) {
    i = -max;
  }
  float e = x - (float)i;
  q->err  = e > 0.5f ? 0.5f : (e < -0.5f ? -0.5f : e);  // no windup when saturated
  return i;
}
