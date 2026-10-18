#include "dbrake_comp.h"
#include "commands.h"
#include "hal.h"
#include "math.h"
#include "defines.h"
#include "angle.h"
#include "vbrake.h"

// Dynamic brake: the motor's own back-EMF stops it, with no position
// feedback (docs/emergency_stop.md). fault0.dbrake asks for it (a feedback
// fault, a following error, ...). Between the motor's current command
// (pmsm_ttc0, fb_switch0) and hv0: voltage mode with a virtual resistor
// (shared/vbrake.h, the same law the F3 runs on its own when the F4 stops
// talking), in a frame frozen where braking began.
//
// rv = 0 is a short circuit: the energy goes into the windings. At speed a
// short is held back by the motor's inductance and brakes weakly. rv rises
// only to hold the current at cur_share (0.9) of max_cur: the power stage
// is protected by the current; the energy that then goes into the DC bus
// is reported (power), and rv backs off above high_dc_volt. 0.9: the F3's
// own voltage-mode clamp at max_cur would otherwise fight it.
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

HAL_PIN(max_cur);    // [A] the motor's (conf0.max_ac_cur)
HAL_PIN(cur_share);  // the brake current target, share of max_cur
HAL_PIN(ki);         // [1/s], relative to rn = pwm_volt / max_cur
HAL_PIN(kp);         // relative
HAL_PIN(stop_cur);   // [A], 0: 5 % of max_cur
HAL_PIN(stop_time);  // [s]

HAL_PIN(rv);     // [ohm]
HAL_PIN(cur);    // [A]
HAL_PIN(power);  // [W] into the DC bus
HAL_PIN(stopped);
HAL_PIN(active);

struct dbrake_ctx_t {
  int active;
  float pos;  // the frame, frozen
  vbrake_t b;
};

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct dbrake_pin_ctx_t *pins = (struct dbrake_pin_ctx_t *)pin_ptr;
  PIN(cur_share)                = 0.9;
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
    PIN(power)    = 0.0;
    PIN(stopped)  = 0.0;
    PIN(active)   = 0.0;
    return;
  }

  if(!ctx->active) {
    ctx->active = 1;
    ctx->pos    = PIN(pos_in);
    vbrake_reset(&ctx->b);
  }
  vbrake_cfg_t c = {.max_cur      = PIN(max_cur) * PIN(cur_share),
                    .pwm_volt     = PIN(pwm_volt),
                    .dc_volt      = PIN(dc_volt),
                    .high_dc_volt = PIN(high_dc_volt),
                    .kp           = PIN(kp),
                    .ki           = PIN(ki),
                    .stop_cur     = PIN(stop_cur) > 0.0 ? PIN(stop_cur) : 0.05 * PIN(max_cur),
                    .stop_time    = PIN(stop_time)};
  float ud, uq;
  vbrake_step(&ctx->b, &c, id, iq, period, &ud, &uq);

  PIN(d_out)    = ud;
  PIN(q_out)    = uq;
  PIN(mode_out) = 0.0;  // VOLT_MODE
  PIN(pos_out)  = ctx->pos;
  PIN(vel_out)  = 0.0;  // the F3 must not turn the frame between packets
  PIN(rv)       = ctx->b.rv;
  PIN(power)    = ctx->b.power;
  PIN(active)   = 1.0;
  PIN(stopped)  = ctx->b.stopped;
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
