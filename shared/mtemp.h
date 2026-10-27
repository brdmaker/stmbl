#pragma once
// Motor temperature sensors on the HV board's motor temperature input
// (hv0.mot_temp, ohms: stm32f303/src/comps/io.c): resistance <-> temperature
// for the drive (shared/comps/mtemp.c) and the simulator (host/hw/hv.c).
#include <math.h>

enum {
  MTEMP_NONE   = 0,
  MTEMP_KTY84  = 1,  // KTY84-130 silicon sensor (SEW CMP "KTY", Siemens 1FK...)
  MTEMP_PT1000 = 2,
  MTEMP_PTC    = 3,  // PTC thermistor (DIN 44081/44082): trips, no reading
  MTEMP_NTC    = 4,  // NTC thermistor: r25, beta
  MTEMP_SWITCH = 5,  // normally closed thermal switch (TH, Klixon): trips
  MTEMP_TYPES
};
enum { MTEMP_OK = 0, MTEMP_OPEN = 1, MTEMP_SHORT = 2 };

// KTY84-130, datasheet typical values: R [ohm] at -40 .. 250 degC in 10 degC steps
static const float mtemp_kty84[30] = {359, 391, 424, 460, 498, 538, 581, 626, 672, 722,
                                      773, 826, 882, 940, 1000, 1062, 1127, 1194, 1262, 1334,
                                      1407, 1482, 1560, 1640, 1722, 1807, 1893, 1982, 2073, 2166};
#define MTEMP_KTY_T0 -40.0f
#define MTEMP_KTY_DT 10.0f

// PT1000, IEC 60751 above 0 degC (also used below: < 0.1 degC off at -40)
#define MTEMP_PT_A 3.9083e-3f
#define MTEMP_PT_B -5.775e-7f

// The resistance a sensor shows at temperature t [degC]. NTC: r25, beta;
// PTC and switch: tnat, their trip temperature.
static inline float mtemp_r_of(int type, float t, float r25, float beta, float tnat) {
  switch(type) {
    case MTEMP_KTY84: {
      float x = (t - MTEMP_KTY_T0) / MTEMP_KTY_DT;
      int i   = (int)floorf(x);
      i       = i < 0 ? 0 : i > 28 ? 28 : i;
      return mtemp_kty84[i] + (x - i) * (mtemp_kty84[i + 1] - mtemp_kty84[i]);
    }
    case MTEMP_PT1000:
      return 1000.0f * (1.0f + MTEMP_PT_A * t + MTEMP_PT_B * t * t);
    case MTEMP_NTC:
      return r25 * expf(beta * (1.0f / (t + 273.15f) - 1.0f / 298.15f));
    case MTEMP_PTC: {  // 100 ohm cold, 1330 at tnat, 4000 at tnat + 5 (DIN 44081)
      if(t < tnat - 20.0f) {
        return 100.0f;
      }
      if(t < tnat) {
        return 100.0f * powf(13.3f, (t - tnat + 20.0f) / 20.0f);
      }
      return 1330.0f * powf(4000.0f / 1330.0f, (t - tnat) / 5.0f);
    }
    case MTEMP_SWITCH:
      return t < tnat ? 0.1f : 1e9f;
    default:
      return -1.0f;
  }
}

// The temperature [degC] a KTY84 / PT1000 / NTC reading means (else 0)
static inline float mtemp_t_of(int type, float r, float r25, float beta) {
  switch(type) {
    case MTEMP_KTY84: {
      int i = 0;
      while(i < 28 && r > mtemp_kty84[i + 1]) {
        i++;
      }
      return MTEMP_KTY_T0 + MTEMP_KTY_DT * (i + (r - mtemp_kty84[i]) / (mtemp_kty84[i + 1] - mtemp_kty84[i]));
    }
    case MTEMP_PT1000: {  // r = 1000 (1 + A t + B t^2)
      float c = 1.0f - r / 1000.0f;
      return (-MTEMP_PT_A + sqrtf(MTEMP_PT_A * MTEMP_PT_A - 4.0f * MTEMP_PT_B * c)) / (2.0f * MTEMP_PT_B);
    }
    case MTEMP_NTC:
      return 1.0f / (1.0f / 298.15f + logf(r / r25) / beta) - 273.15f;
    default:
      return 0.0f;
  }
}

// A broken wire: open or short for this type (a tripped PTC or switch reads
// high, so for them only a short or far beyond any PTC counts)
static inline int mtemp_wire(int type, float r, float r25) {
  if(!(r == r) || r < 0.0f || r > 1e7f) {  // nan, negative: the input saturated
    return type == MTEMP_SWITCH ? MTEMP_OK : MTEMP_OPEN;
  }
  switch(type) {
    case MTEMP_KTY84:
      return r < 250.0f ? MTEMP_SHORT : r > 3000.0f ? MTEMP_OPEN : MTEMP_OK;
    case MTEMP_PT1000:
      return r < 600.0f ? MTEMP_SHORT : r > 3000.0f ? MTEMP_OPEN : MTEMP_OK;
    case MTEMP_NTC:
      return r < 0.002f * r25 ? MTEMP_SHORT : r > 50.0f * r25 ? MTEMP_OPEN : MTEMP_OK;
    case MTEMP_PTC:
      return r < 10.0f ? MTEMP_SHORT : r > 100000.0f ? MTEMP_OPEN : MTEMP_OK;
    default:
      return MTEMP_OK;
  }
}
