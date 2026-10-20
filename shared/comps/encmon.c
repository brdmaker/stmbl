#include "encmon_comp.h"
#include "commands.h"
#include "hal.h"
#include "math.h"
#include "defines.h"
#include "angle.h"

// Encoder monitor: checks the motor feedback against the motor's own
// voltage equation. The current loop works in the frame of com (hv0.pos, the
// vel2 observer's angle); the feedback says the rotor is at pos
// (fb_switch0.com_fb), so the back-EMF psi * w lies at delta = pos - com in
// that frame. With w differentiated from pos (not vel2.vel: the observer
// lags by tens of rad/s in a fast ramp), the dq voltages must satisfy
//   uq = r * iq + l * diq/dt + w * l * id + psi * w * cos(delta)
//   ud = r * id + l * did/dt - w * l * iq - psi * w * sin(delta)
// A wrong speed (lost or extra counts, a slipping coupling, a stuck
// encoder) leaves a q residual; a wrong angle turns part of the back-EMF
// into a d residual (psi * w * sin(angle error)). The residual is filtered
// and taken relative to the back-EMF psi * w; above thres for trip_time the
// fault output latches (until en goes low).
//
// Only checked when the back-EMF is at least min_bemf (at low speed there
// is nothing to compare) and the voltage is below 90% of max_volt (in
// saturation the applied voltage is not the commanded one).

HAL_COMP(encmon);

HAL_PIN(en);        // check only while the drive is enabled (fault0.en_out)
HAL_PIN(pos);       // electrical rotor angle from the feedback (fb_switch0.com_fb)
HAL_PIN(com);       // angle the current loop uses (hv0.pos)
HAL_PIN(ud);        // applied dq voltages [V] (hv0.ud_fb, hv0.uq_fb)
HAL_PIN(uq);
HAL_PIN(id);        // measured dq currents [A] (hv0.id_fb, hv0.iq_fb)
HAL_PIN(iq);
HAL_PIN(r);         // motor model (conf0.r, conf0.l, conf0.psi)
HAL_PIN(l);
HAL_PIN(psi);
HAL_PIN(max_volt);  // available voltage [V] (hv0.pwm_volt)
HAL_PIN(delay);     // [s] the F3 reports its voltages and currents in a frame
                    // this far ahead of com (it extrapolates the angle between
                    // packets, and its readings arrive a packet late): 0.12 ms
                    // measured on the bench (v4, 5 kHz); the sim's F3 ~75 us
HAL_PIN(com_src);   // fb_switch0.current_com_pos: check only once the angle is
                    // absolute (1..3: index or absolute feedback), not while it
                    // is autophasing's estimate (4..); 0 (unlinked): always

HAL_PIN(min_bemf);   // check only above this back-EMF [V]
HAL_PIN(thres);      // trip above this residual / back-EMF
HAL_PIN(trip_time);  // ... for this long [s]
HAL_PIN(lp);         // residual filter [Hz]

HAL_PIN(err_d);   // filtered residuals [V]
HAL_PIN(err_q);
HAL_PIN(bemf);    // filtered back-EMF psi * w [V]
HAL_PIN(ratio);   // |residual| / |back-EMF|
HAL_PIN(max_ratio); // largest ratio while active, since en
HAL_PIN(active);  // 1 while checking
HAL_PIN(timer);   // time above thres [s]
HAL_PIN(max_timer); // longest time above thres, since en
HAL_PIN(fault);   // latched trip

HAL_PIN(vel);     // electrical velocity, differentiated from pos [rad/s]
HAL_PIN(last_pos);
HAL_PIN(last_id);
HAL_PIN(last_iq);
HAL_PIN(last_src);  // com_src last cycle
HAL_PIN(hold);      // time left to wait after the angle source changed [s]

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct encmon_pin_ctx_t *pins = (struct encmon_pin_ctx_t *)pin_ptr;
  PIN(min_bemf)  = 5.0;
  PIN(thres)     = 0.3;
  PIN(trip_time) = 0.02;
  PIN(lp)        = 50.0;
}

#define HOLD_TIME 0.02  // [s]

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct encmon_pin_ctx_t *pins = (struct encmon_pin_ctx_t *)pin_ptr;

  float w   = minus(PIN(pos), PIN(last_pos)) / period;
  PIN(last_pos) = PIN(pos);
  PIN(vel)  = w;
  float delta = minus(PIN(pos), PIN(com)) + w * PIN(delay);
  // the voltage was applied between the last and this current sample: use
  // the difference for l * di/dt and the average for the other terms
  float did = (PIN(id) - PIN(last_id)) / period;
  float diq = (PIN(iq) - PIN(last_iq)) / period;
  float id  = (PIN(id) + PIN(last_id)) / 2.0;
  float iq  = (PIN(iq) + PIN(last_iq)) / 2.0;
  PIN(last_id) = PIN(id);
  PIN(last_iq) = PIN(iq);

  // a new angle source (autophasing's estimate -> the index or absolute
  // feedback) moves the angle at once: a speed spike here and a current
  // transient; start the filters again once it has settled
  if(PIN(com_src) != PIN(last_src)) {
    PIN(last_src) = PIN(com_src);
    PIN(hold)     = HOLD_TIME;
  }
  if(PIN(hold) > 0.0) {
    PIN(hold) -= period;
    PIN(err_d)  = 0.0;
    PIN(err_q)  = 0.0;
    PIN(bemf)   = 0.0;
    PIN(ratio)  = 0.0;
    PIN(timer)  = 0.0;
    PIN(active) = 0.0;
    return;
  }

  if(PIN(en) <= 0.0) {
    PIN(fault)   = 0.0;
    PIN(timer)   = 0.0;
    PIN(max_ratio) = 0.0;
    PIN(max_timer) = 0.0;
    PIN(err_d)   = 0.0;
    PIN(err_q)   = 0.0;
    PIN(bemf)    = 0.0;
    PIN(ratio)     = 0.0;
    PIN(active)  = 0.0;
    return;
  }

  float eq   = PIN(uq) - PIN(r) * iq - PIN(l) * diq - w * PIN(l) * id - PIN(psi) * w * cosf(delta);
  float ed   = PIN(ud) - PIN(r) * id - PIN(l) * did + w * PIN(l) * iq + PIN(psi) * w * sinf(delta);
  float bemf = PIN(psi) * w;
  float lp   = LP_HZ(PIN(lp));
  PIN(err_d) = PIN(err_d) * (1.0 - lp) + ed * lp;
  PIN(err_q) = PIN(err_q) * (1.0 - lp) + eq * lp;
  PIN(bemf)  = PIN(bemf) * (1.0 - lp) + bemf * lp;

  float u    = sqrtf(PIN(ud) * PIN(ud) + PIN(uq) * PIN(uq));
  int abs_com = PIN(com_src) <= 3.0;  // probe phasing can be ~10 deg off (DEBUGNOTES)
  int active  = abs_com && ABS(PIN(bemf)) > PIN(min_bemf) && u < 0.9 * PIN(max_volt);
  PIN(active) = active;
  if(!active) {
    PIN(ratio)   = 0.0;
    PIN(timer) = 0.0;
    return;
  }

  PIN(ratio) = sqrtf(PIN(err_d) * PIN(err_d) + PIN(err_q) * PIN(err_q)) / ABS(PIN(bemf));
  PIN(max_ratio) = MAX(PIN(max_ratio), PIN(ratio));
  if(PIN(ratio) > PIN(thres)) {
    PIN(timer) += period;
  } else {
    PIN(timer) = 0.0;
  }
  PIN(max_timer) = MAX(PIN(max_timer), PIN(timer));
  if(PIN(timer) > PIN(trip_time)) {
    PIN(fault) = 1.0;
  }
}

hal_comp_t encmon_comp_struct = {
    .name      = "encmon",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = 0,
    .pin_count = sizeof(struct encmon_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
