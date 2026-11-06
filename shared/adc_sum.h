#pragma once
// Summing the F4's packed ADC samples (src/comps/adc.c): each 32-bit word of
// the DMA buffer holds two 12-bit samples, sin in the low half, cos in the
// high half (dual ADC mode). Adding the words lane-wise (both halves at
// once, ARM's UADD16 on the Cortex-M4) gives both sums in one instruction a
// word; the halves never carry into each other as long as n * 4095 fits in
// 16 bits: n <= 16. Exact: the same integers as summing each half alone.
//
// adc_lin(): the sum to the input voltage, V_DIFF in adc.c without its two
// divisions: (sum / over / res * ref - in_ref) / in_gain
//          = sum * k - k0,  k = ref / (over * res * in_gain), k0 = in_ref / in_gain
// (float: within a few ulp of the original).
#include <stdint.h>

#define ADC_SUM_MAX_N 16

static inline uint32_t adc_uadd16(uint32_t a, uint32_t b) {
#if defined(__ARM_FEATURE_SIMD32) && __ARM_FEATURE_SIMD32
  uint32_t r;
  __asm__("uadd16 %0, %1, %2" : "=r"(r) : "r"(a), "r"(b));
  return r;
#else
  return ((a + b) & 0x0000ffffu) | (((a >> 16) + (b >> 16)) << 16);
#endif
}

// the sums of n packed words' low and high halves (n <= ADC_SUM_MAX_N)
static inline void adc_sum2(const volatile uint32_t *p, int n, uint32_t *lo, uint32_t *hi) {
  uint32_t acc = 0;
  for(int j = 0; j < n; j++) {
    acc = adc_uadd16(acc, p[j]);
  }
  *lo = acc & 0x0000ffffu;
  *hi = acc >> 16;
}

static inline float adc_lin(uint32_t sum, float k, float k0) {
  return (float)sum * k - k0;
}
