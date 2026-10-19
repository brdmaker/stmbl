#include "qstop_comp.h"
#include "commands.h"
#include "hal.h"
#include "math.h"
#include "defines.h"
#include "angle.h"

// Quick stop: a controlled stop in the command path, before pid
// (docs/emergency_stop.md). fault0.qstop asks for it (link lost, enable
// dropped while moving): a trajectory from where the motor is (pos_fb,
// vel_fb) to standstill at dec, with its position, velocity and
// acceleration, so pid's position loop keeps it on the planned path (with
// the velocity loop alone the stop rang: the velocity estimate lags at
// these decelerations). The position error is limited to max_err: a motor
// that can't follow is not pulled back further than that. stopped tells
// fault0 when the motor stands still; fault0 then switches off.
//
// While hold or hold2 is set (the link's error, vel_int's watchdog), the
// last good command is extrapolated at its velocity: what the link
// components output then (a frozen or zeroed position) is not a command.
//
//   qstop0.pos_in / vel_in / acc_in   the command (reslimit0, vel0)
//   qstop0.pos_fb / vel_fb            feedback (fb_switch0, vel1)
//   qstop0.dec                        deceleration [rad/s^2]; 0: dec_share
//                                     (0.3) of what the motor can do,
//                                     max_torque / (j + j_sys)
//   qstop0.tj                         the deceleration builds up and eases
//                                     off over tj (3 ms): an S-curve
//   qstop0.stop_vel                   standstill [rad/s]
//   qstop0.max_power                  [W] the braking power that goes back
//                                     into the DC bus, j * a * v, is held
//                                     to the HV board's rating
//                                     (conf0.max_hv_power; 0: no limit)
//
// The drive's velocity observer (vel1) lags at these decelerations (its
// torque feedforward is a 50 Hz high-pass), so pid over-brakes and the stop
// rings for a while (DEBUGNOTES). On the burn-in motor from 188 rad/s:
// 30 % stops in 3.1 mm with 0.01 rad of reverse travel, 50 % in 2.7 mm
// with 0.11 rad; coasting takes 26 mm.

HAL_COMP(qstop);

HAL_PIN(stop);
HAL_PIN(hold);
HAL_PIN(hold2);

HAL_PIN(pos_in);
HAL_PIN(vel_in);
HAL_PIN(acc_in);
HAL_PIN(pos_fb);
HAL_PIN(vel_fb);

HAL_PIN(pos_out);
HAL_PIN(vel_out);
HAL_PIN(acc_out);

HAL_PIN(dec);
HAL_PIN(max_torque);
HAL_PIN(j);
HAL_PIN(j_sys);
HAL_PIN(dec_share);
HAL_PIN(tj);  // [s]
HAL_PIN(max_power);  // [W]
HAL_PIN(power);      // [W] out: j * |a| * |v| of the plan
HAL_PIN(stop_vel);
HAL_PIN(max_err);  // [rad]
HAL_PIN(stopped);
HAL_PIN(active);

struct qstop_ctx_t {
  int active;
  float pos, vel;  // the last good command, extrapolated while held
  float v;         // the ramp
  float a;         // its acceleration
  float p;         // its position
  float still;     // [s] at standstill
};

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct qstop_pin_ctx_t *pins = (struct qstop_pin_ctx_t *)pin_ptr;
  PIN(dec)                     = 0.0;
  PIN(dec_share)                = 0.3;
  PIN(tj)                       = 0.003;
  PIN(stop_vel)                = 1.0;
  PIN(max_err)                  = 0.5;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct qstop_ctx_t *ctx      = (struct qstop_ctx_t *)ctx_ptr;
  struct qstop_pin_ctx_t *pins = (struct qstop_pin_ctx_t *)pin_ptr;

  if(PIN(stop) > 0.0) {
    if(!ctx->active) {
      ctx->active = 1;
      ctx->v      = PIN(vel_fb);
      ctx->a      = 0.0;
      ctx->p      = PIN(pos_fb);
      ctx->still  = 0.0;
    }
    float dec = PIN(dec);
    if(dec <= 0.0) {  // a share of what the motor can do
      dec = PIN(dec_share) * PIN(max_torque) / MAX(PIN(j) + PIN(j_sys), 1e-9);
    }
    // an S-curve: |a| rises to dec at jerk dec / tj, and eases off so v
    // and a reach 0 together (from |a|, easing off takes a^2 / 2 jerk of v)
    float jt = MAX(PIN(j) + PIN(j_sys), 1e-9);
    if(PIN(max_power) > 0.0 && ABS(ctx->v) > 1e-3) {  // regen at most max_power
      dec = MIN(dec, PIN(max_power) / (jt * ABS(ctx->v)));
    }
    float jerk = dec / MAX(PIN(tj), period);
    float dir  = ctx->v > 0.0 ? -1.0 : 1.0;  // the acceleration that brakes
    float aa   = ABS(ctx->a);
    if(ABS(ctx->v) <= aa * aa / (2.0 * jerk) + aa * period) {
      aa = MAX(aa - jerk * period, 0.0);
    } else {
      aa = MIN(aa + jerk * period, dec);
    }
    float v = ctx->v + dir * aa * period;
    if(v * ctx->v <= 0.0) {  // through 0: stopped
      v  = 0.0;
      aa = 0.0;
    }
    ctx->v     = v;
    ctx->a     = dir * aa;
    float acc  = ctx->a;
    PIN(power) = jt * aa * ABS(v);
    ctx->p  = mod(ctx->p + ctx->v * period);
    float e = minus(ctx->p, PIN(pos_fb));
    float m = MAX(PIN(max_err), 0.0);
    if(ABS(e) > m) {  // can't follow: don't drag it back further
      ctx->p = mod(PIN(pos_fb) + (e > 0.0 ? m : -m));
    }
    PIN(pos_out) = ctx->p;
    PIN(vel_out) = ctx->v;
    PIN(acc_out) = acc;
    if(ctx->v == 0.0 && ABS(PIN(vel_fb)) < PIN(stop_vel)) {
      ctx->still += period;
    } else {
      ctx->still = 0.0;
    }
    PIN(stopped) = ctx->still >= 0.02;
    PIN(active)  = 1.0;
    ctx->pos     = ctx->p;
    ctx->vel     = 0.0;
    return;
  }

  ctx->active  = 0;
  PIN(power)   = 0.0;
  PIN(stopped) = 0.0;
  PIN(active)  = 0.0;
  if(PIN(hold) > 0.0 || PIN(hold2) > 0.0) {
    ctx->pos     = mod(ctx->pos + ctx->vel * period);
    PIN(pos_out) = ctx->pos;
    PIN(vel_out) = ctx->vel;
    PIN(acc_out) = 0.0;
  } else {
    ctx->pos     = PIN(pos_in);
    ctx->vel     = PIN(vel_in);
    PIN(pos_out) = PIN(pos_in);
    PIN(vel_out) = PIN(vel_in);
    PIN(acc_out) = PIN(acc_in);
  }
}

hal_comp_t qstop_comp_struct = {
    .name      = "qstop",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct qstop_ctx_t),
    .pin_count = sizeof(struct qstop_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
