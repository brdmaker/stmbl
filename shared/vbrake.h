/*
 * Dynamic braking with a virtual resistor (docs/emergency_stop.md): the
 * voltages u = -rv * i in voltage mode, in a frame that doesn't turn. A
 * resistor in the dq frame is one in any frame, so no rotor angle is
 * needed. rv = 0 is a short circuit (the energy goes into the windings);
 * rv rises only to hold the current at its target (the power stage's and
 * the motor's limit), and falls back towards the short when the bus is
 * high. So energy goes into the DC bus (1.5 * rv * i^2, reported as power)
 * only when the short alone would draw too much current: the current
 * comes first, overcurrent destroys the power stage at once, while the bus
 * has high_dc_volt here and the F3's ABS_MAX_VOLT cut-off. (Capping that
 * power as well can't work: below the current target rv is 0 anyway.)
 *
 * It starts at rv = rn (pwm_volt / max_cur, so about max_cur at full
 * back-EMF) and relaxes towards the short while the current is below its
 * target: starting as a short, the current overshot up to twice the
 * short-circuit current before rv could rise.
 *
 * Stability: u follows i a delay later. When the winding's time constant
 * L / R is not much longer than that delay (low-inductance motors), an rv
 * much above the winding's own impedance turns into delayed feedback with
 * a gain above 1: the voltage flips sign every sample. So rv stays below
 * l / delay. If the current then can't be held (above trip * max_cur for
 * trip_time), the brake gives up (gave_up): the caller switches the
 * outputs off; coasting is safe for the power stage.
 *
 * Used by the F4's dbrake component (shared/comps/dbrake.c), by the F3 when
 * the F4 stops talking (stm32f303/src/comps/ls.c), and by the simulated F3
 * (host/hw/hv.c). Header only, plain C.
 */
#pragma once
#include <math.h>

typedef struct {
  float max_cur;       // [A] the current target: below the HV board's own clamp
  float pwm_volt;      // [V] the largest voltage the HV board applies
  float dc_volt;       // [V] the DC bus now
  float high_dc_volt;  // [V] above this rv backs off (0: off)
  float kp, ki;        // relative to rn = pwm_volt / max_cur: 0.1, 500/s
  float l;             // [H] the motor's phase inductance (0: unknown, no limit)
  float delay;         // [s] from the current measured to the voltage applied
  float trip;          // give up above trip * max_cur ... (0: never)
  float trip_time;     // [s] ... for this long
  float stop_cur;      // [A] below this ...
  float stop_time;     // [s] ... this long: stopped
} vbrake_cfg_t;

typedef struct {
  float rvi;   // integral part of rv
  float rv;    // [ohm] last rv
  float low;   // [s] below stop_cur
  float power; // [W] into the bus
  float over;  // [s] above trip
  int stopped;
  int gave_up;
  int started;
} vbrake_t;

static inline void vbrake_reset(vbrake_t *b) {
  b->rvi = b->rv = b->low = b->power = b->over = 0.0f;
  b->stopped = 0;
  b->gave_up = 0;
  b->started = 0;
}

static inline void vbrake_step(vbrake_t *b, const vbrake_cfg_t *c, float id, float iq, float period, float *ud,
                               float *uq) {
  float i2      = id * id + iq * iq;
  float i       = sqrtf(i2);
  float max_cur = c->max_cur > 0.01f ? c->max_cur : 0.01f;
  float rn      = (c->pwm_volt > 1.0f ? c->pwm_volt : 1.0f) / max_cur;
  float rv_max  = 2.0f * rn;
  if(c->l > 0.0f && c->delay > 0.0f && c->l / c->delay < rv_max) {  // stability
    rv_max = c->l / c->delay;
  }
  if(!b->started) {  // start resistive
    b->started = 1;
    b->rvi     = rn < rv_max ? rn : rv_max;
  }
  float e       = (i - max_cur) / max_cur;
  if(c->high_dc_volt > 0.0f && c->dc_volt > c->high_dc_volt) {
    e = -1.0f;  // the bus is full: back towards the short circuit
  }
  b->rvi += rn * c->ki * e * period;
  b->rvi = b->rvi < 0.0f ? 0.0f : b->rvi > rv_max ? rv_max : b->rvi;
  float rv = b->rvi + rn * c->kp * (e > 0.0f ? e : 0.0f);
  rv       = rv < 0.0f ? 0.0f : rv > rv_max ? rv_max : rv;
  *ud      = -rv * id;
  *uq      = -rv * iq;
  b->rv    = rv;
  b->power = 1.5f * rv * i2;
  float sc = c->stop_cur > 0.0f ? c->stop_cur : 0.05f * max_cur;
  b->low   = i < sc ? b->low + period : 0.0f;
  b->stopped = b->low >= c->stop_time;
  b->over    = (c->trip > 0.0f && i > c->trip * max_cur) ? b->over + period : 0.0f;
  if(c->trip_time > 0.0f && b->over >= c->trip_time) {
    b->gave_up = 1;
  }
}
