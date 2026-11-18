// The F3's overcurrent trip current: the largest phase current magnitude.
//
// stm32f303/src/comps/io.c took MAX3(ABS(iu), iv, iw): only U's magnitude,
// so a negative V or W current never tripped HV_OVERCURRENT_PEAK/RMS
// (DEBUGNOTES 2026-11-10).
#pragma once
#include "defines.h"

static inline float phase_iabs(float iu, float iv, float iw) {
  return MAX3(ABS(iu), ABS(iv), ABS(iw));
}
