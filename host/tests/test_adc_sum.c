// shared/adc_sum.h (src/comps/adc.c's fast path) and shared/sserial_sync.h
// (the clock lock with reply_mode 1): unit tests.
//   adc_sum2: the same integers as summing each half alone, n 1..16, random
//             12-bit samples and the worst case (all 4095)
//   adc_lin:  within 2e-6 V of V_DIFF for every sum, both boards' op-amp values
//   sserial_sync_scale: the original rule for reply_mode 0; the "packet
//             already answered" case for reply_mode 1
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "adc_sum.h"
#include "sserial_sync.h"

static int fails = 0, checks = 0;
#define CHECK(c, ...)                \
  do {                               \
    checks++;                        \
    if(!(c)) {                       \
      fails++;                       \
      printf("FAIL: " __VA_ARGS__);  \
      printf("\n");                  \
    }                                \
  } while(0)

// inc/hw/hw.h's op-amp front ends (V3, V4)
typedef struct {
  float op_ref, r_in, r_fb, r_low, r_high;
} amp_t;
static const amp_t AMPS[] = {{5.0f, 1000.0f, 3900.0f, 180.0f, 470.0f}, {1.83f, 10000.0f, 15000.0f, 470.0f, 22.0f}};
#define ADC_REF 3.3f
#define ADC_RES 4096.0f

static float v_diff(uint32_t sum, int over, const amp_t *a) {  // adc.c's V_DIFF, as compiled (single-precision constants)
  float in_ref  = a->op_ref * a->r_low / (a->r_high + a->r_low);
  float in_gain = a->r_fb / a->r_in * a->r_low / (a->r_high + a->r_low);
  return (((float)sum) / (float)over / ADC_RES * ADC_REF - in_ref) / in_gain;
}

int main(void) {
  srand(1234);
  // adc_sum2
  for(int n = 1; n <= ADC_SUM_MAX_N; n++) {
    for(int rep = 0; rep < 2000; rep++) {
      uint32_t buf[ADC_SUM_MAX_N];
      uint32_t lo0 = 0, hi0 = 0;
      for(int j = 0; j < n; j++) {
        uint32_t l = rep == 0 ? 4095 : (uint32_t)(rand() & 0x0fff), h = rep == 0 ? 4095 : (uint32_t)(rand() & 0x0fff);
        buf[j] = l | (h << 16);
        lo0 += l, hi0 += h;
      }
      uint32_t lo, hi;
      adc_sum2(buf, n, &lo, &hi);
      CHECK(lo == lo0 && hi == hi0, "adc_sum2 n %d rep %d: %u %u, expected %u %u", n, rep, (unsigned)lo, (unsigned)hi, (unsigned)lo0, (unsigned)hi0);
    }
  }
  // adc_lin against V_DIFF
  for(unsigned a = 0; a < sizeof(AMPS) / sizeof(AMPS[0]); a++) {
    const amp_t *A = &AMPS[a];
    float in_ref = A->op_ref * A->r_low / (A->r_high + A->r_low);
    float in_gain = A->r_fb / A->r_in * A->r_low / (A->r_high + A->r_low);
    for(int over = 1; over <= 10; over++) {
      float k = ADC_REF / ((float)over * ADC_RES * in_gain), k0 = in_ref / in_gain;
      double worst = 0;
      for(uint32_t sum = 0; sum <= (uint32_t)over * 4095; sum++) {
        double d = fabs((double)adc_lin(sum, k, k0) - (double)v_diff(sum, over, A));
        worst = d > worst ? d : worst;
      }
      CHECK(worst < 2e-6, "adc_lin amp %u over %d: %g V off V_DIFF", a, over, worst);
    }
  }
  // sserial_sync_scale (goal 5)
  for(int irq = 0; irq <= 1; irq++) {
    CHECK(sserial_sync_scale(7, 5, irq, 0) == 0.9f, "late in the packet: 0.9");
    CHECK(sserial_sync_scale(3, 5, irq, 0) == 1.1f, "early in the packet: 1.1");
    CHECK(sserial_sync_scale(5, 5, irq, 1) == 1.0f, "on goal: 1.0");
    CHECK(sserial_sync_scale(0, 5, irq, 0) == 1.0f, "no packet near: 1.0");
  }
  CHECK(sserial_sync_scale(0, 5, 0, 1) == 1.0f, "mode 0 ignores the idle time");
  CHECK(sserial_sync_scale(0, 5, 1, 1) == 0.9f, "mode 1: answered just before the check: late, 0.9");
  printf("test_adc_sum: %d/%d passed\n", checks - fails, checks);
  return fails ? 1 : 0;
}
