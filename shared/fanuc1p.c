#include "fanuc1p.h"

int f1p_build_levels(uint8_t *lv, int max, uint32_t req, int req_bits, int lead, int tail, int idle) {
  int n = 0;
  idle  = idle ? 1 : 0;
  if(req_bits > 32) {
    req_bits = 32;
  }
  for(int i = 0; i < lead && n < max; i++) {
    lv[n++] = idle;
  }
  for(int i = 0; i < req_bits && n < max; i++) {
    lv[n++] = (req >> i) & 1;
  }
  for(int i = 0; i < tail && n < max; i++) {
    lv[n++] = idle;
  }
  return n;
}

int f1p_decode(const uint16_t *t, int n, float ticks_per_bit, int idle, uint8_t *bits, int maxbits) {
  int nb    = 0;
  int level = idle ? 0 : 1;  // level after the first edge
  if(n <= 0 || ticks_per_bit <= 0.0f) {
    return 0;
  }
  for(int i = 1; i < n && nb < maxbits; i++) {
    uint16_t dt = (uint16_t)(t[i] - t[i - 1]);
    int k       = (int)((float)dt / ticks_per_bit + 0.5f);
    if(k < 1) {
      k = 1;
    }
    for(int j = 0; j < k && nb < maxbits; j++) {
      bits[nb++] = level;
    }
    level = !level;
  }
  // after the last edge the line holds its level
  while(nb < maxbits) {
    bits[nb++] = level;
  }
  return nb;
}

uint32_t f1p_field(const uint8_t *bits, int nbits, int off, int len, int lsb_first) {
  uint32_t v = 0;
  if(len > 32) {
    len = 32;
  }
  for(int i = 0; i < len; i++) {
    int k    = off + i;
    uint32_t b = (k >= 0 && k < nbits) ? (bits[k] & 1) : 0;
    if(lsb_first) {
      v |= b << i;
    } else {
      v = (v << 1) | b;
    }
  }
  return v;
}

int f1p_format(char *out, int max, const uint8_t *bits, int nbits, int group) {
  int n = 0;
  for(int i = 0; i < nbits && n < max - 2; i++) {
    if(group > 0 && i > 0 && (i % group) == 0) {
      out[n++] = ' ';
    }
    out[n++] = bits[i] ? '1' : '0';
  }
  if(max > 0) {
    out[n < max ? n : max - 1] = 0;
  }
  return n;
}
