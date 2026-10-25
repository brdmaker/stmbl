/*
 * The accelerometer's samples, from the sensor driver (the IIS3DWB's FIFO
 * on the sensor, vibsim in the host simulator) to the vib component: one
 * writer, one reader, no locks. Samples are the sensor's raw int16 x, y, z;
 * the writer also says the sample rate and the g per count.
 */
#pragma once
#include <stdint.h>

#define VIBBUF_SIZE 4096  // samples (xyz), a power of two: 150 ms at 26.7 kHz

typedef struct {
  int16_t x, y, z;
} vib_xyz_t;

typedef struct {
  vib_xyz_t s[VIBBUF_SIZE];
  volatile uint32_t wr, rd;  // free-running counts
  volatile uint32_t dropped; // samples lost to a full buffer
  float fs;                  // samples per second
  float g_per_lsb;           // IIS3DWB: 0.061 mg at +-2 g ... 0.488 mg at +-16 g
} vibbuf_t;

extern vibbuf_t vibbuf;

static inline void vibbuf_put(vibbuf_t *b, vib_xyz_t v) {
  if(b->wr - b->rd >= VIBBUF_SIZE) {
    b->dropped++;
    return;
  }
  b->s[b->wr & (VIBBUF_SIZE - 1)] = v;
  b->wr++;
}

static inline uint32_t vibbuf_count(const vibbuf_t *b) {
  return b->wr - b->rd;
}

static inline vib_xyz_t vibbuf_get(vibbuf_t *b) {
  vib_xyz_t v = b->s[b->rd & (VIBBUF_SIZE - 1)];
  b->rd++;
  return v;
}
