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

// sync 1: place each command at its LinuxCNC time instead of its arrival.
// LinuxCNC's servo thread computes its positions on an exact 1/cmd_freq grid
// but they arrive with its (and the link's) timing jitter: taken when it
// arrives, an update early or late by d moves the command by vel x d, and a
// lost one is extrapolated past the watchdog or (mode 1) taken as one period
// when it was two (DEBUGNOTES 2026-11-11, tests/lcnc_jitter.test). The drive's
// clock is locked to LinuxCNC's (sserial clock_scale), so the grid is tracked
// by dead reckoning, its phase pulled toward the arrivals by sync_k each
// update; a gap counts as the whole periods it spans. After more than
// sync_resync periods without an update (standing still, the packets don't
// change) the next arrival starts the grid again, as with sync 0.
// Needs cmd_freq at LinuxCNC's servo rate, as mode 1 does (set lower,
// sync_resyncs counts up and the following error is worse than sync 0). A
// lost update leaves a gap of two periods plus the jitter: wd 0.003 rides
// through one (wd 0.002 stops the extrapolation and flags error).
HAL_PIN(sync);
HAL_PIN(sync_k);       // the phase loop's gain, 0.02 (a time constant of 50 updates)
HAL_PIN(sync_resync);  // [updates] 8
HAL_PIN(sync_dev);     // out: the last update's arrival minus its place on the grid [s]
HAL_PIN(sync_lost);    // out: updates the grid says were missed, counted
HAL_PIN(sync_resyncs); // out: updates over half a period early (cmd_freq too low?), counted

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
  // sync
  float tn;      // time since the newest command's place on LinuxCNC's grid
  int locked;    // tn means something
  int k1;        // periods the newest step spans (mode 1)
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

  PIN(sync)        = 0.0;
  PIN(sync_k)      = 0.02;
  PIN(sync_resync) = 8.0;
  ctx->locked      = 0;
  ctx->k1          = 1;
}

// sync: a new command arrived tn after the last one's place on the grid.
// Returns the periods it spans (1 unless updates were lost) and sets tn to the
// time since its own place (negative when it came early).
static int sync_arrive(struct vel_int_ctx_t *ctx, struct vel_int_pin_ctx_t *pins, float T) {
  int k = (int)(ctx->tn / T + 0.5);
  // k 0: more than half a period early on the grid. Not jitter (that is far
  // less); cmd_freq lower than LinuxCNC's rate. Start the grid again each
  // time, which takes the updates as they arrive (sync 0).
  if(!ctx->locked || ctx->tn > PIN(sync_resync) * T || k < 1) {
    if(ctx->locked && k < 1) {
      PIN(sync_resyncs)++;
    }
    ctx->locked   = 1;
    ctx->tn       = 0.0;
    PIN(sync_dev) = 0.0;
    return 1;
  }
  float dev = ctx->tn - k * T;
  PIN(sync_dev) = dev;
  PIN(sync_lost) += k - 1;
  ctx->tn = dev * (1.0 - CLAMP(PIN(sync_k), 0.0, 1.0));
  return k;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct vel_int_ctx_t *ctx      = (struct vel_int_ctx_t *)ctx_ptr;
  struct vel_int_pin_ctx_t *pins = (struct vel_int_pin_ctx_t *)pin_ptr;

  float p     = PIN(pos_in);
  float v     = PIN(vel_in);
  int new_cmd = p != ctx->last_p || v != ctx->last_v;  // before the watchdog zeroes v
  int sync    = PIN(sync) > 0.0;
  float T     = 1.0 / MAX(PIN(cmd_freq), 1.0);
  int k       = 1;  // periods since the last command (sync)
  if(sync) {
    // also mode 1 after a lost update: without sync it takes the two
    // periods' step as one, twice the speed and a spike of acceleration
    // (0.48 rad of following error at 100 rad/s, tests/lcnc_jitter_sync.test).
    // The arrival gap alone can't tell: with +-0.4 ms of jitter a late then
    // an early update are 1.8 ms apart, and rounding that made it worse.
    ctx->tn += period;
    if(new_cmd) {
      k = sync_arrive(ctx, pins, T);
    }
  } else {
    ctx->locked = 0;
  }

  if(PIN(mode) > 0.0) {
    if(new_cmd) {
      // the two steps span t1 and t0 (one period each, unless sync saw
      // updates lost): average speeds s1, s0 at their middles
      float t1 = k * T, t0 = ctx->k1 * T;
      float d1 = ctx->n >= 1 ? minus(p, ctx->p1) : 0.0;
      float d0 = ctx->n >= 2 ? minus(ctx->p1, ctx->p2) : 0.0;
      int stop = v == 0.0 || d1 == 0.0;
      if(stop || ctx->n < 2) {  // stopped (or starting): hold
        ctx->v = d1 / t1, ctx->a = 0.0;
        if(v == 0.0) {
          ctx->v = 0.0;
        }
      } else {
        ctx->a = (d1 / t1 - d0 / t0) / (0.5 * (t1 + t0));
        ctx->v = d1 / t1 + ctx->a * t1 * 0.5;
      }
      ctx->k1 = stop ? 1 : k;
      // after a stop the history is "standing at p": standing still the
      // packets repeat and aren't new, so p2 would stay whatever came before
      // the stop (a jump while disabled, say), and the next move's first
      // acceleration would be (d1 - that) / T^2
      ctx->p2 = stop ? p : ctx->p1;
      ctx->p1 = p;
      ctx->n  = stop ? 2 : ctx->n < 2 ? ctx->n + 1 : 2;
      ctx->t  = sync ? ctx->tn : 0.0;  // sync: at its place on the grid
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
    ctx->pos     = sync ? p + v * ctx->tn : p;  // sync: at its place on the grid
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
