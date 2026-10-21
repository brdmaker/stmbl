// Phase current sensor offsets that follow drift (temperature, time).
//
// The F3 measures its current offsets once, ~200 cycles after the HV side
// powers up. They drift: on the bench the F3 read ~0.09 A with the bridge
// off (2026-10-21), and with zero current commanded its current loop then
// drove a real 0.09 A DC through the motor, a fixed field that rolled a
// bare rotor 15 deg (align phasing's release; DEBUGNOTES "Align phasing").
//
// While the bridge is off no current can flow: after the current has
// decayed (settle), each phase's reading is offset error only, and the
// offset is trimmed towards it with time constant tau. A reading above
// max_err is not offset (a motor driven past the bus voltage feeds the
// diodes) and is left alone.
#pragma once

typedef struct {
  float off_time;  // [s] the bridge has been off for
} cur_offset_t;

// call every cycle; i[] are the phase currents with the present offsets,
// off[] the offsets (i = offset - raw), updated in place
static inline void cur_offset_track(cur_offset_t *s, int bridge_on, const float *i, float *off, int n, float period,
                                    float settle, float tau, float max_err) {
  if(bridge_on) {
    s->off_time = 0.0f;
    return;
  }
  s->off_time += period;
  if(s->off_time < settle) {
    return;
  }
  float k = period / tau;
  for(int p = 0; p < n; p++) {
    if(i[p] < max_err && i[p] > -max_err) {
      off[p] -= k * i[p];
    }
  }
}
