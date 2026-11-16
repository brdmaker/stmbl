#include "idjf_comp.h"
#include "hal.h"
#include "defines.h"
#include "angle.h"
#include <math.h>

// idjf: friction, offset and inertia in torque mode, before any closed-loop
// identification (conf/template/id_jf.txt, the setup wizard).
//
// Every closed-loop step starts blind otherwise: pid's stiffness scales with
// its inertia, and an unknown motor starts with a deliberately small one and
// no friction feedforward. Against friction the loop is then soft: in the
// simulator a motor with much friction lost the track completely in idm's
// gentle run (DEBUGNOTES 2026-11-16). Here the torque is commanded directly
// (pmsm_ttc0.torque), the encoder only watches:
//
// 1. Breakaway: the torque ramps from 0 at `ramp` Nm/s until the shaft
//    turns faster than vel_th, one way (brk_pos) and the other (brk_neg).
//    Only a scale for the pulses: it depends on the ramp rate (detection
//    lag, creep), so it isn't the friction.
// 2. Pulses: per direction two torques, b0 + 1.5 b and b0 + 3 b (b the
//    breakaway's size, at least min_dt; b0 the breakaway's centre), each for
//    `tp` from standstill. With the measured torque T(t) (torque_fb) and
//    the distance turned x: j x = X - c tp^2 / 2, X the torque's double
//    integral over the pulse, c the friction and offset that way. Two
//    torques give j = dX / dx and c. friction = (c+ - c-) / 2, offset =
//    (c+ + c-) / 2, j the mean. Measured, not commanded torque: the current
//    falls as the shaft speeds up (0.08 -> 0.053 Nm in 14 ms, simulator).
//    The position, not the speed: the speed estimate's lag (~half a 20 ms
//    pulse) read j 2x high. tp doubles (up to tp_max) until the smaller
//    pulse turns dx_min.
//
// Between the steps the shaft is braked to a stop (a torque against its
// speed, the size of the breakaway), not left to coast: a light rotor with
// little friction coasted past max_travel. A pulse that turns too little at
// tp_max is repeated with twice the torque (the breakaway can read low).
//
// Guards (the torque goes to 0):
// the torque never above max_torque; travel from the start over max_travel;
// a step taking longer than its timeout. error: 1 the torque reached
// max_torque without motion (blocked, a brake, or too weak), 2 travel,
// 3 timeout, 4 the pulses turn too little, 5 an implausible fit.
//
// Not for a hanging load: with the torque off between the steps it falls.

HAL_COMP(idjf);

HAL_PIN(en);
HAL_PIN(vel);  // rad/s, measured (vel1.vel, model-free)
HAL_PIN(pos);    // rad, -pi..pi (fb_switch0.pos_fb)
HAL_PIN(ready);  // > 0: commutation known (fb_switch0.state; incremental encoders phase first)

HAL_PIN(torque);     // out: the torque command (pmsm_ttc0.torque)
HAL_PIN(torque_fb);  // measured torque (pmsm_ttc0.torque_fb)

HAL_PIN(max_torque);  // Nm
HAL_PIN(ramp);        // Nm/s
HAL_PIN(vel_th);      // rad/s: moving
HAL_PIN(max_travel);  // rad from the start
HAL_PIN(min_dt);      // Nm: the pulses' scale at least this
HAL_PIN(tp);          // s: the first pulse length
HAL_PIN(tp_max);      // s
HAL_PIN(dx_min);      // rad: the smaller pulse must turn this

HAL_PIN(state);  // 0 idle, 1 settle, 2 ramp +, 3 stop, 4 ramp -, 5 stop, 6 pulses, 10 done
HAL_PIN(error);
HAL_PIN(brk_pos);
HAL_PIN(brk_neg);
HAL_PIN(friction);
HAL_PIN(offset);
HAL_PIN(j_pos);
HAL_PIN(j_neg);
HAL_PIN(j);
HAL_PIN(travel);  // out: the largest distance from the start
HAL_PIN(t_pulse);  // out: the pulse length used

#define NP 4  // pulses: +small, +large, -small, -large
static const float pulse_sign[NP] = {1.0, 1.0, -1.0, -1.0};
static const float pulse_k[NP]    = {1.5, 3.0, 1.5, 3.0};

struct idjf_ctx_t {
  float timer;     // in this state
  float still;     // time standing
  float travel;    // signed, unwrapped
  float last_pos;
  int pulse;       // 0..NP-1
  int pulsing;     // 1 torque on, 0 coasting to a stop
  float x0;        // travel at the pulse's start
  float s1, s2;    // the measured torque's integral and double integral
  float bscale;    // the pulses' torque scale, doubled when they turn too little
  float dx[NP];    // the distances turned
  float tx[NP];    // the torques' double integrals
  float h[NP];     // tp^2 / 2
};

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct idjf_pin_ctx_t *pins = (struct idjf_pin_ctx_t *)pin_ptr;
  PIN(max_torque) = 1.0;
  PIN(ramp)       = 0.2;
  PIN(vel_th)     = 1.0;
  PIN(max_travel) = 3.0;
  PIN(min_dt)     = 0.01;
  PIN(tp)         = 0.02;
  PIN(tp_max)     = 0.32;
  PIN(dx_min)     = 0.05;
}

#define STOP_TIME 0.1  // s standing before the next step
#define STOP_TIMEOUT 3.0
#define RAMP_TIMEOUT 30.0

static void to_state(struct idjf_ctx_t *ctx, struct idjf_pin_ctx_t *pins, float s) {
  PIN(state)  = s;
  ctx->timer  = 0.0;
  ctx->still  = 0.0;
  PIN(torque) = 0.0;
}

static void fail(struct idjf_ctx_t *ctx, struct idjf_pin_ctx_t *pins, float code) {
  PIN(error) = code;
  to_state(ctx, pins, 10.0);
}

// brake to a stop: a torque against the speed (b), off once nearly still
static float brake(struct idjf_pin_ctx_t *pins, float v, float b) {
  return ABS(v) > 0.5 * PIN(vel_th) ? -SIGN(v) * b : 0.0;
}

// j and c (friction + offset that way) from a small and a large pulse of
// the same length: j x = X - c h
static int fit(struct idjf_ctx_t *ctx, int i, float *j, float *c) {
  float dx = ctx->dx[i + 1] - ctx->dx[i];
  float dX = ctx->tx[i + 1] - ctx->tx[i];
  if(!(dx * dX > 0.0) || !(ctx->h[i] > 0.0)) {
    return -1;
  }
  *j = dX / dx;
  *c = (ctx->tx[i] - *j * ctx->dx[i]) / ctx->h[i];
  return 0;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct idjf_ctx_t *ctx      = (struct idjf_ctx_t *)ctx_ptr;
  struct idjf_pin_ctx_t *pins = (struct idjf_pin_ctx_t *)pin_ptr;

  if(PIN(en) <= 0.0) {
    PIN(state)  = 0.0;
    PIN(torque) = 0.0;
    return;
  }
  float v = PIN(vel);
  ctx->timer += period;
  ctx->still = ABS(v) < 0.25 * PIN(vel_th) ? ctx->still + period : 0.0;
  if(PIN(state) >= 1.0) {
    ctx->travel += minus(PIN(pos), ctx->last_pos);
    PIN(travel) = MAX(PIN(travel), ABS(ctx->travel));
  }
  ctx->last_pos = PIN(pos);
  int s = (int)(PIN(state) + 0.5);

  if(s >= 1 && s < 10 && ABS(ctx->travel) > PIN(max_travel)) {
    fail(ctx, pins, 2.0);
    return;
  }

  switch(s) {
    case 0:  // enabled: start
      PIN(error)  = 0.0;
      PIN(travel) = 0.0;
      PIN(brk_pos) = PIN(brk_neg) = PIN(friction) = PIN(offset) = 0.0;
      PIN(j_pos) = PIN(j_neg) = PIN(j) = 0.0;
      PIN(t_pulse) = PIN(tp);
      ctx->travel  = 0.0;
      ctx->bscale  = 1.0;
      to_state(ctx, pins, 1.0);
      break;

    case 1:  // standing still at no torque, commutation known
      if(PIN(ready) <= 0.0) {
        ctx->timer = 0.0;
        ctx->still = 0.0;
      } else if(ctx->still > STOP_TIME) {
        to_state(ctx, pins, 2.0);
      } else if(ctx->timer > STOP_TIMEOUT) {
        fail(ctx, pins, 3.0);
      }
      break;

    case 2:  // ramp up until it turns
    case 4: {
      float sign  = s == 2 ? 1.0 : -1.0;
      PIN(torque) = sign * PIN(ramp) * ctx->timer;
      if(sign * v > PIN(vel_th)) {
        if(s == 2) {
          PIN(brk_pos) = PIN(torque);
        } else {
          PIN(brk_neg) = PIN(torque);
        }
        to_state(ctx, pins, s + 1.0);
      } else if(ABS(PIN(torque)) > PIN(max_torque)) {
        fail(ctx, pins, 1.0);
      } else if(ctx->timer > RAMP_TIMEOUT) {
        fail(ctx, pins, 3.0);
      }
    } break;

    case 3:  // brake to a stop
    case 5:
      PIN(torque) = brake(pins, v, MAX(ABS(s == 3 ? PIN(brk_pos) : PIN(brk_neg)), PIN(min_dt)));
      if(ctx->still > STOP_TIME) {
        to_state(ctx, pins, s == 3 ? 4.0 : 6.0);
        ctx->pulse   = 0;
        ctx->pulsing = 0;
      } else if(ctx->timer > STOP_TIMEOUT) {
        fail(ctx, pins, 3.0);
      }
      break;

    case 6: {  // the pulses, each from standstill
      float b0 = (PIN(brk_pos) + PIN(brk_neg)) / 2.0;
      float b  = MAX((PIN(brk_pos) - PIN(brk_neg)) / 2.0, PIN(min_dt));
      int i    = ctx->pulse;
      b *= ctx->bscale;
      if(!ctx->pulsing) {  // braked to a stop
        PIN(torque) = brake(pins, v, b);
        if(ctx->still > STOP_TIME) {
          if(i >= NP) {
            float jp, cp, jn, cn;
            if(fit(ctx, 0, &jp, &cp) || fit(ctx, 2, &jn, &cn) || !(jp > 0.0) || !(jn > 0.0)) {
              fail(ctx, pins, 5.0);
              break;
            }
            PIN(j_pos)    = jp;
            PIN(j_neg)    = jn;
            PIN(j)        = (jp + jn) / 2.0;
            PIN(friction) = (cp - cn) / 2.0;
            PIN(offset)   = (cp + cn) / 2.0;
            to_state(ctx, pins, 10.0);
            break;
          }
          ctx->pulsing = 1;
          ctx->timer   = 0.0;
          ctx->x0      = ctx->travel;
          ctx->s1      = 0.0;
          ctx->s2      = 0.0;
        } else if(ctx->timer > STOP_TIMEOUT) {
          fail(ctx, pins, 3.0);
        }
        break;
      }
      float t = b0 + pulse_sign[i] * pulse_k[i] * b;
      if(ABS(t) > PIN(max_torque)) {
        fail(ctx, pins, 1.0);
        break;
      }
      PIN(torque) = t;
      ctx->s1 += PIN(torque_fb) * period;
      ctx->s2 += ctx->s1 * period;
      float tp    = PIN(t_pulse);
      if(ctx->timer >= tp) {
        float dx     = ctx->travel - ctx->x0;
        ctx->pulsing = 0;
        ctx->timer   = 0.0;
        PIN(torque)  = 0.0;
        if((i == 0 || i == 2) && pulse_sign[i] * dx < PIN(dx_min)) {
          if(tp * 2.0 <= PIN(tp_max)) {
            PIN(t_pulse) = tp * 2.0;  // longer, from this direction's first pulse on
          } else if(ABS(b0) + 6.0 * b <= PIN(max_torque)) {
            ctx->bscale *= 2.0;  // stronger (both torques of the pair, and the other way's)
          } else {
            fail(ctx, pins, 4.0);
          }
          break;
        }
        ctx->dx[i] = dx;
        ctx->tx[i] = ctx->s2;
        ctx->h[i]  = tp * tp / 2.0;
        ctx->pulse  = i + 1;
      }
    } break;

    default:  // 10: done
      PIN(torque) = 0.0;
      break;
  }
}

hal_comp_t idjf_comp_struct = {
    .name      = "idjf",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct idjf_ctx_t),
    .pin_count = sizeof(struct idjf_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
