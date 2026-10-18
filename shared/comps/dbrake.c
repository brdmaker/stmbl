#include "dbrake_comp.h"
#include "commands.h"
#include "hal.h"
#include "math.h"
#include "defines.h"
#include "angle.h"

// Dynamic brake: the motor's own back-EMF stops it, with no position
// feedback (docs/emergency_stop.md). fault0.dbrake asks for it (a feedback
// fault, a following error, ...). Between the motor's current command
// (pmsm_ttc0, fb_switch0) and hv0: in voltage mode, a virtual resistor
// u = -rv * i, which needs no rotor angle (the same rotation turns u and
// i), in a frame frozen where braking began. rv = 0 is a short circuit:
// the energy goes into the windings. rv rises when the current would
// exceed max_cur; then part of the energy goes into the DC bus, so rv
// falls again above high_dc_volt. At speed a short is limited by the
// motor's inductance and brakes weakly; some rv brakes harder (the burn-in
// motor from 188 rad/s: 5.0 mm shorted, 2.5-3 mm with the current held at
// 2 A), at the cost of that energy in the bus. stopped: the current stayed
// below stop_cur for stop_time.
//
// The gains are relative to rn = pwm_volt / max_cur, the resistance that
// would drive max_cur at full voltage: rv = rn * (kp * e + ki * int(e)),
// e = (i - max_cur) / max_cur.
//
//   dbrake0.d_in / q_in / mode_in / pos_in / vel_in   the normal command
//   dbrake0.d_out / q_out / mode_out / pos_out / vel_out   to hv0

HAL_COMP(dbrake);

HAL_PIN(brake);

HAL_PIN(d_in);
HAL_PIN(q_in);
HAL_PIN(mode_in);
HAL_PIN(pos_in);
HAL_PIN(vel_in);

HAL_PIN(d_out);
HAL_PIN(q_out);
HAL_PIN(mode_out);
HAL_PIN(pos_out);
HAL_PIN(vel_out);

HAL_PIN(id_fb);
HAL_PIN(iq_fb);
HAL_PIN(dc_volt);
HAL_PIN(high_dc_volt);
HAL_PIN(pwm_volt);  // [V] the largest voltage the HV board applies

HAL_PIN(max_cur);    // [A]
HAL_PIN(rv_max);     // [ohm], 0: 2 * rn
HAL_PIN(ki);         // [1/s], relative
HAL_PIN(kp);         // relative
HAL_PIN(stop_cur);   // [A], 0: 5 % of max_cur
HAL_PIN(stop_time);  // [s]

HAL_PIN(rv);   // [ohm]
HAL_PIN(cur);  // [A]
HAL_PIN(stopped);
HAL_PIN(active);

struct dbrake_ctx_t {
  int active;
  float pos;  // the frame, frozen
  float rvi;  // integral part of rv
  float low;  // [s] below stop_cur
};

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct dbrake_pin_ctx_t *pins = (struct dbrake_pin_ctx_t *)pin_ptr;
  PIN(rv_max)                   = 0.0;
  PIN(ki)                       = 500.0;
  PIN(kp)                       = 0.1;
  PIN(stop_time)                = 0.1;
  PIN(mode_in)                  = 1.0;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct dbrake_ctx_t *ctx      = (struct dbrake_ctx_t *)ctx_ptr;
  struct dbrake_pin_ctx_t *pins = (struct dbrake_pin_ctx_t *)pin_ptr;

  float id = PIN(id_fb);
  float iq = PIN(iq_fb);
  float i  = sqrtf(id * id + iq * iq);
  PIN(cur) = i;

  if(PIN(brake) <= 0.0) {
    ctx->active   = 0;
    PIN(d_out)    = PIN(d_in);
    PIN(q_out)    = PIN(q_in);
    PIN(mode_out) = PIN(mode_in);
    PIN(pos_out)  = PIN(pos_in);
    PIN(vel_out)  = PIN(vel_in);
    PIN(rv)       = 0.0;
    PIN(stopped)  = 0.0;
    PIN(active)   = 0.0;
    return;
  }

  if(!ctx->active) {
    ctx->active = 1;
    ctx->pos    = PIN(pos_in);
    ctx->rvi    = 0.0;
    ctx->low    = 0.0;
  }
  float max_cur = MAX(PIN(max_cur), 0.01);
  float rn      = MAX(PIN(pwm_volt), 1.0) / max_cur;
  float rv_max  = PIN(rv_max) > 0.0 ? PIN(rv_max) : 2.0 * rn;
  float e       = (i - max_cur) / max_cur;
  if(PIN(high_dc_volt) > 0.0 && PIN(dc_volt) > PIN(high_dc_volt)) {
    e = -1.0;  // the bus is full: back towards the short circuit
  }
  ctx->rvi = CLAMP(ctx->rvi + rn * PIN(ki) * e * period, 0.0, rv_max);
  float rv = CLAMP(ctx->rvi + rn * PIN(kp) * MAX(e, 0.0), 0.0, rv_max);

  PIN(d_out)    = -rv * id;
  PIN(q_out)    = -rv * iq;
  PIN(mode_out) = 0.0;  // VOLT_MODE
  PIN(pos_out)  = ctx->pos;
  PIN(vel_out)  = 0.0;  // the F3 must not turn the frame between packets
  PIN(rv)       = rv;
  PIN(active)   = 1.0;

  float stop_cur = PIN(stop_cur) > 0.0 ? PIN(stop_cur) : 0.05 * max_cur;
  ctx->low       = i < stop_cur ? ctx->low + period : 0.0;
  PIN(stopped)   = ctx->low >= PIN(stop_time);
}

hal_comp_t dbrake_comp_struct = {
    .name      = "dbrake",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct dbrake_ctx_t),
    .pin_count = sizeof(struct dbrake_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
