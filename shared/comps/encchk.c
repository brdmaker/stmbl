#include "encchk_comp.h"
#include "hal.h"
#include "defines.h"
#include "angle.h"
#include <math.h>

// encchk: checks the motor feedback open loop, before it closes any loop
// (conf/template/enc_check.txt, the setup wizard's encoder step).
//
// A d current at an angle this comp turns (through dbrake0, so the fault
// comp still switches it off) drags the rotor round like a stepper, slowly,
// `turns` mechanical turns one way, a stop, then back. The feedback only
// watches. Every cycle the feedback, followed through the turns, is compared
// with the field: dev = (pp * the feedback's travel) - (the field's travel),
// electrical, both from where they stood once the rotor had settled. The
// rotor lags the field by its load angle (a few degrees against friction),
// so dev stays small while the feedback is sound. It grows with
//   - lost or extra counts (a bad line, a slipping coupling),
//   - a wrong resolution or pole pair count (linearly with the travel),
//   - the wrong direction (twice the travel),
//   - a glitch frame (a spike for one cycle).
// On the SGMAH's encoder this was done by hand first (brdmot
// tools/bench/openloop_run.py, 2026-11-16: 6 turns each way, no jump in
// 33 000 frames).
//
// error: 1 |dev| over max_dev (the run stops at once).
// Not for a hanging load: the rotor is held only by the field.

HAL_COMP(encchk);

HAL_PIN(en);      // fault0.en_out
HAL_PIN(pos);     // the motor feedback, rad, -pi..pi (fb_switch0.mot_pos)
HAL_PIN(rev);     // > 0: the feedback counts the other way (conf0.mot_fb_rev)
HAL_PIN(fb_pp);   // the feedback's turns per mechanical turn (fb_switch0.mot_polecount)
HAL_PIN(pp);      // the motor's pole pairs (conf0.polecount)

HAL_PIN(angle);   // out: the field's electrical angle (dbrake0.pos_in)
HAL_PIN(cur);     // out: the d current (dbrake0.d_in)

HAL_PIN(i);        // A: the d current
HAL_PIN(freq);     // mechanical turns per second
// mechanical turns per second^2: the field speeds up and slows down (a heavy
// rotor could not jump to the field's speed: a simulated spindle, j 0.02,
// fell 0.5 rad behind at once)
HAL_PIN(acc);
HAL_PIN(turns);    // mechanical turns each way
HAL_PIN(ramp_t);   // s: the current ramps up / down
HAL_PIN(settle);   // s: standing, before the turns and between them
HAL_PIN(max_dev);  // rad electrical: error 1 above

HAL_PIN(state);  // 0 idle, 1 ramp up, 2 settle, 3 forward, 4 stop, 5 back, 6 ramp down, 10 done
HAL_PIN(error);
HAL_PIN(dev);        // rad electrical, now
HAL_PIN(dev_max);    // |dev| at most, over the turns
HAL_PIN(turns_fwd);  // the feedback's mechanical turns, forward (should be +turns)
HAL_PIN(turns_back); // and back (should be -turns)

struct encchk_ctx_t {
  float timer;
  float last_pos;
  float mech;    // the feedback's mechanical travel, followed through the turns
  float field;   // the field's electrical travel
  float mech0;   // both where the rotor had settled
  float field0;
  float mech_s;  // where the current direction started
  int started;
  float v;       // the field's speed, electrical rad/s
};

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct encchk_pin_ctx_t *pins = (struct encchk_pin_ctx_t *)pin_ptr;
  PIN(i)       = 1.0;
  PIN(freq)    = 0.5;
  PIN(acc)     = 1.0;
  PIN(turns)   = 1.0;
  PIN(ramp_t)  = 0.5;
  PIN(settle)  = 1.0;
  PIN(max_dev) = 0.5;
  PIN(fb_pp)   = 1.0;
  PIN(pp)      = 1.0;
}

static void to_state(struct encchk_ctx_t *ctx, struct encchk_pin_ctx_t *pins, float s) {
  PIN(state) = s;
  ctx->timer = 0.0;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct encchk_ctx_t *ctx      = (struct encchk_ctx_t *)ctx_ptr;
  struct encchk_pin_ctx_t *pins = (struct encchk_pin_ctx_t *)pin_ptr;

  float fb_pp = MAX(PIN(fb_pp), 1.0);
  float pp    = MAX(PIN(pp), 1.0);

  if(PIN(en) <= 0.0) {
    PIN(cur)     = 0.0;
    ctx->started = 0;
    if(PIN(state) != 10.0) {
      PIN(state) = 0.0;
    }
    return;
  }

  // the feedback through the turns, mechanical
  if(!ctx->started) {
    ctx->started  = 1;
    ctx->last_pos = PIN(pos);
    ctx->mech     = 0.0;
    ctx->field    = 0.0;
    ctx->v        = 0.0;
    PIN(angle)    = 0.0;
    PIN(error)    = 0.0;
    PIN(dev)      = 0.0;
    PIN(dev_max)  = 0.0;
    PIN(turns_fwd) = PIN(turns_back) = 0.0;
    to_state(ctx, pins, 1.0);
  }
  float d       = minus(PIN(pos), ctx->last_pos);
  ctx->last_pos = PIN(pos);
  ctx->mech += (PIN(rev) > 0.0 ? -d : d) / fb_pp;

  ctx->timer += period;
  float vmax = 2.0 * M_PI * PIN(freq) * pp;            // electrical rad/s
  float a    = 2.0 * M_PI * MAX(PIN(acc), 0.01) * pp;  // electrical rad/s^2
  float span = 2.0 * M_PI * PIN(turns) * pp;           // electrical, each way
  float left = 0.0;  // of this direction's travel

  switch((int)(PIN(state) + 0.5)) {
    case 1:  // the current up, the field still: the rotor aligns
      PIN(cur) = PIN(i) * MIN(ctx->timer / MAX(PIN(ramp_t), period), 1.0);
      if(ctx->timer >= PIN(ramp_t)) {
        to_state(ctx, pins, 2.0);
      }
      break;
    case 2:  // settle, then from here
      PIN(cur) = PIN(i);
      if(ctx->timer >= PIN(settle)) {
        ctx->mech0  = ctx->mech;
        ctx->field0 = ctx->field;
        ctx->mech_s = ctx->mech;
        to_state(ctx, pins, 3.0);
      }
      break;
    case 3:  // forward
      left   = span - (ctx->field - ctx->field0);
      ctx->v = MIN(MIN(ctx->v + a * period, vmax), sqrtf(2.0 * a * MAX(left, 0.0)) + a * period);
      ctx->field += ctx->v * period;
      if(ctx->field - ctx->field0 >= span) {
        ctx->field = ctx->field0 + span;
        to_state(ctx, pins, 4.0);
      }
      break;
    case 4:  // stop, then back
      ctx->v = 0.0;
      if(ctx->timer >= PIN(settle)) {
        PIN(turns_fwd) = (ctx->mech - ctx->mech_s) / (2.0 * M_PI);
        ctx->mech_s    = ctx->mech;
        to_state(ctx, pins, 5.0);
      }
      break;
    case 5:  // back to the start
      left   = ctx->field - ctx->field0;
      ctx->v = MIN(MIN(ctx->v + a * period, vmax), sqrtf(2.0 * a * MAX(left, 0.0)) + a * period);
      ctx->field -= ctx->v * period;
      if(ctx->field <= ctx->field0) {
        ctx->field = ctx->field0;
        to_state(ctx, pins, 6.0);
      }
      break;
    case 6:  // the current down
      if(ctx->timer >= PIN(settle) && PIN(turns_back) == 0.0) {
        PIN(turns_back) = (ctx->mech - ctx->mech_s) / (2.0 * M_PI);
      }
      if(ctx->timer >= PIN(settle)) {
        PIN(cur) = PIN(i) * MAX(1.0 - (ctx->timer - PIN(settle)) / MAX(PIN(ramp_t), period), 0.0);
        if(PIN(cur) <= 0.0) {
          to_state(ctx, pins, 10.0);
        }
      }
      break;
    default:  // 10 done (or an error)
      PIN(cur) = 0.0;
      break;
  }

  PIN(angle) = mod(ctx->field);
  if(PIN(state) >= 3.0 && PIN(state) <= 6.0) {
    PIN(dev)     = (ctx->mech - ctx->mech0) * pp - (ctx->field - ctx->field0);
    PIN(dev_max) = MAX(PIN(dev_max), ABS(PIN(dev)));
    if(ABS(PIN(dev)) > PIN(max_dev)) {
      PIN(error) = 1.0;
      PIN(cur)   = 0.0;
      to_state(ctx, pins, 10.0);
    }
  }
}

hal_comp_t encchk_comp_struct = {
    .name      = "encchk",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct encchk_ctx_t),
    .pin_count = sizeof(struct encchk_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
