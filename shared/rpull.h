#pragma once
/*
 * A resistive sensor on a feedback port's pin 7 (a KTY, PT1000, NTC, PTC or
 * thermal switch to pin 8, GND): its resistance from the pin's voltage.
 * With the port's supply jumper open (JP2 on FB0, JP5 on FB1, see
 * docs/motor_temp_lv.md) pin 7 has a pull-up (rpull, 10 k to +5 V) and the
 * ADC's divider to GND (rload, 10 k + 1 k); io0.fb0 / io0.fb1 is the pin's
 * voltage. The sensor gets what the pull-up gives minus what the divider
 * takes. Host-tested (host/tests/mot_temp_fb.test).
 */

// ohms; -1 when no current flows into the sensor (open wire, nothing
// fitted, or the jumper still closed: pin 7 at the supply)
static inline float rpull_r(float v, float vsup, float rpull, float rload) {
  if(!(v == v) || rpull <= 0.0f || rload <= 0.0f) {
    return -1.0f;
  }
  if(v <= 0.0f) {
    return 0.0f;  // shorted
  }
  float i = (vsup - v) / rpull - v / rload;  // into the sensor [A]
  if(i <= 1e-7f) {
    return -1.0f;
  }
  float r = v / i;
  return r > 1e6f ? -1.0f : r;
}

// the pin's voltage for a sensor of r ohms (the simulator; r < 0: open)
static inline float rpull_v(float r, float vsup, float rpull, float rload) {
  float load = r < 0.0f ? rload : r * rload / (r + rload);
  return vsup * load / (rpull + load);
}
