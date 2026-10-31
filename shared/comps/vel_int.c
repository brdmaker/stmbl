#include "vel_int_comp.h"
#include "commands.h"
#include "hal.h"
#include "math.h"
#include "defines.h"
#include "angle.h"

HAL_COMP(vel_int);

HAL_PIN(pos_in);
HAL_PIN(pos_out);

HAL_PIN(vel_in);
HAL_PIN(vel_out);

// mode 0: extrapolate the last position with vel_in (LinuxCNC's vel-cmd: the
//         average over the period before, half a period late); acc_out is
//         acc_in (vel0's estimate, when the config enables it)
// mode 1: LinuxCNC's planner moves with constant acceleration between its
//         updates: from the last three positions the velocity and the
//         acceleration at the newest one are exact (v = d1/T + a T/2,
//         a = (d1 - d0)/T^2, T = 1/cmd_freq); extrapolate quadratically,
//         acc_out to the acceleration feedforward. vel_in only says stop.
HAL_PIN(mode);
HAL_PIN(acc_in);
HAL_PIN(acc_out);

HAL_PIN(cmd_freq);
HAL_PIN(real_cmd_freq);

HAL_PIN(wd);
HAL_PIN(error);

struct vel_int_ctx_t {
  float pos;
  float counter;
  float cmd_freq;
  float last_p;  // last command, to detect a new one (per instance)
  float last_v;
  // mode 1
  float p1, p2;  // the two packets before
  int n;         // packets seen (up to 2)
  float v, a;    // velocity and acceleration at the newest packet
  float t;       // time since it
};

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct vel_int_ctx_t *ctx      = (struct vel_int_ctx_t *)ctx_ptr;
  struct vel_int_pin_ctx_t *pins = (struct vel_int_pin_ctx_t *)pin_ptr;

  ctx->pos     = 0.0;
  ctx->counter = 0.0;
  PIN(pos_in)  = 0.0;
  PIN(pos_out) = 0.0;

  PIN(vel_in)  = 0.0;
  PIN(vel_out) = 0.0;

  PIN(wd)    = 0.002;
  PIN(error) = 0.0;

  PIN(cmd_freq) = 1000.0;
  ctx->cmd_freq = PIN(cmd_freq);
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct vel_int_ctx_t *ctx      = (struct vel_int_ctx_t *)ctx_ptr;
  struct vel_int_pin_ctx_t *pins = (struct vel_int_pin_ctx_t *)pin_ptr;

  float p     = PIN(pos_in);
  float v     = PIN(vel_in);
  int new_cmd = p != ctx->last_p || v != ctx->last_v;  // before the watchdog zeroes v

  if(PIN(mode) > 0.0) {
    if(new_cmd) {
      float T  = 1.0 / MAX(PIN(cmd_freq), 1.0);
      float d1 = ctx->n >= 1 ? minus(p, ctx->p1) : 0.0;
      float d0 = ctx->n >= 2 ? minus(ctx->p1, ctx->p2) : 0.0;
      int stop = v == 0.0 || d1 == 0.0;
      if(stop || ctx->n < 2) {  // stopped (or starting): hold
        ctx->v = d1 / T, ctx->a = 0.0;
        if(v == 0.0) {
          ctx->v = 0.0;
        }
      } else {
        ctx->a = (d1 - d0) / (T * T);
        ctx->v = d1 / T + ctx->a * T * 0.5;
      }
      // after a stop the history is "standing at p": standing still the
      // packets repeat and aren't new, so p2 would stay whatever came before
      // the stop (a jump while disabled, say), and the next move's first
      // acceleration would be (d1 - that) / T^2
      ctx->p2 = stop ? p : ctx->p1;
      ctx->p1 = p;
      ctx->n  = stop ? 2 : ctx->n < 2 ? ctx->n + 1 : 2;
      ctx->t  = 0.0;
      ctx->counter = 0.0;
      ctx->last_p = p, ctx->last_v = v;
      ctx->cmd_freq += 1.0;
    } else {
      ctx->t += period;
      ctx->counter += period;
    }
    float vv = ctx->v + ctx->a * ctx->t, aa = ctx->a;
    if(ctx->counter > PIN(wd)) {  // no more packets: stand where it got to
      PIN(error) = vv != 0.0 || aa != 0.0;  // ... an error if it was moving
      vv = aa = 0.0, ctx->t -= period;
    } else {
      PIN(error) = 0.0;
    }
    ctx->cmd_freq -= ctx->cmd_freq * period;
    PIN(real_cmd_freq) = ctx->cmd_freq * 0.001 + PIN(real_cmd_freq) * 0.999;
    PIN(pos_out) = mod(ctx->p1 + ctx->v * ctx->t + 0.5f * ctx->a * ctx->t * ctx->t);
    PIN(vel_out) = vv;
    PIN(acc_out) = aa;
    return;
  }
  PIN(acc_out) = PIN(acc_in);

  // a new command is no timeout: after a standstill (the same packet
  // repeated, so no new command for a while) the first moving one used to
  // be zeroed and flagged for a cycle
  if(!new_cmd && ctx->counter > PIN(wd) && v != 0.0) {
    v          = 0;
    PIN(error) = 1.0;
  } else {
    PIN(error) = 0.0;
    ctx->counter += period;
  }

  // A new command resets the extrapolation to pos_in. Checking vel_in too
  // matters when a move stops: the last packet repeats the position with
  // vel = 0, and checking pos_in alone would keep the overshoot
  // extrapolated from the packet before.
  if(new_cmd) {
    ctx->last_p  = p;
    ctx->last_v  = PIN(vel_in);
    ctx->counter = 0.0;
    ctx->pos     = p;
    ctx->cmd_freq += 1.0;
  } else {
    ctx->pos += v * period;
  }

  ctx->cmd_freq -= ctx->cmd_freq * period;

  PIN(real_cmd_freq) = ctx->cmd_freq * 0.001 + PIN(real_cmd_freq) * 0.999;

  ctx->pos = mod(ctx->pos);

  PIN(pos_out) = ctx->pos;
  PIN(vel_out) = v;
}

hal_comp_t vel_int_comp_struct = {
    .name      = "vel_int",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct vel_int_ctx_t),
    .pin_count = sizeof(struct vel_int_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
