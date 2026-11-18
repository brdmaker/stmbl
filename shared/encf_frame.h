// The FANUC alpha (aA64) serial frame of src/comps/encf.c: decoding the
// edge capture times TIM4 records into the frame's bits, and its CRC-5.
// Kept here so the host tests it (host/tests/test_encf_frame.c).
//
// The decoder used to trust the line: a long gap between two edges (a
// dropout, noise, a broken cable) or too many edges wrote past the 10-byte
// frame into whatever followed it in RAM, and its uint8_t bit count could
// wrap (DEBUGNOTES 2026-11-11). Now it never writes past ENCF_BITS.
#pragma once
#include <stdint.h>
#include <string.h>

#define ENCF_BITS 77   // the frame's bits as decoded (bit 0 the first level)
#define ENCF_BYTES 10  // ENCF_BITS rounded up

// cap: count edge times (16-bit timer, either edge), the line starting low
// at cap[0] and idle high after the last edge. bit_ticks: timer ticks a bit.
// Returns the bits the edges covered, at most ENCF_BITS (a reply has > 50).
static inline int encf_decode(const uint16_t *cap, uint32_t count, float bit_ticks, uint8_t *f) {
  memset(f, 0, ENCF_BYTES);
  float per_bit = 1.0f / bit_ticks;
  int sum       = 0;
  for(uint32_t i = 1; i < count && sum < ENCF_BITS; i++) {
    uint16_t diff = cap[i] - cap[i - 1];  // wraps with the timer
    int bits      = (int)((float)diff * per_bit + 0.5f);
    int end       = sum + bits > ENCF_BITS ? ENCF_BITS : sum + bits;
    if(i % 2 == 0) {  // even intervals are high
      for(int j = sum; j < end; j++) {
        f[j / 8] |= 1 << (j % 8);
      }
    }
    sum = end;
  }
  for(int j = sum; j < ENCF_BITS; j++) {  // after the last edge: idle high
    f[j / 8] |= 1 << (j % 8);
  }
  return sum;
}

// ITU CRC-5 over bits 76..1, MSB first (http://freeby.mesanet.com/fabsread.pas)
static inline int encf_crc_ok(const uint8_t *f) {
  uint8_t crc[5] = {0}, o[5] = {0};
  for(int i = 76; i >= 1; i--) {
    uint8_t bit = (f[i / 8] >> (i % 8)) & 1;
    crc[0]      = o[4] ^ bit;
    crc[1]      = o[0];
    crc[2]      = o[1] ^ bit ^ o[4];
    crc[3]      = o[2];
    crc[4]      = o[3] ^ bit ^ o[4];
    memcpy(o, crc, 5);
  }
  return !(crc[0] | crc[1] | crc[2] | crc[3] | crc[4]);
}
