/*
 * Simulation stand-in for src/comps/enc_fb.c: a quadrature encoder with an
 * index pulse on the simulated motor shaft.
 *
 * Same pins as the real component. sim_pos is the shaft angle (linked to
 * hv0.sim_pos by host/sim_board.txt). The count is the shaft angle quantized
 * to res counts per revolution; sim_rev reverses the count direction, and the
 * index sits at sim_index in the encoder's own (raw) angle, which is what
 * conf0.mot_fb_offset measures. As on the drive, state is 1 (incremental)
 * until the first index pulse with en_index set, then 3 (absolute).
 *
 * Fault injection, on the angle the counts come from: sim_freeze holds it
 * (stuck encoder, broken line), sim_scale multiplies its changes (lost or
 * extra counts, slipping coupling; default 1), sim_offset adds to it (a step
 * when changed). With the defaults the counts are the shaft's.
 *
 * sim_res: the encoder's own counts per revolution (0: res). The drive's
 * timer counts them and wraps at res, as TIM_SetAutoreload does on the
 * drive, so a res that doesn't match the encoder shows as an index that
 * moves from turn to turn. indexprint = 1 prints the count at each index
 * pulse ("cnt = N", from nrt), as src/comps/enc_fb.c does.
 */

#include "enc_fb_comp.h"
#include "hal.h"
#include "defines.h"
#include "angle.h"
#include <math.h>

HAL_COMP(enc_fb);

// same pins as src/comps/enc_fb.c
HAL_PIN(res);
HAL_PIN(ires);
HAL_PIN(pos);
HAL_PIN(abs_pos);
HAL_PIN(state);
HAL_PIN(index);
HAL_PIN(a);
HAL_PIN(b);
HAL_PIN(ipos);
HAL_PIN(sin);
HAL_PIN(cos);
HAL_PIN(quad);
HAL_PIN(oquad);
HAL_PIN(oquadoff);
HAL_PIN(qdiff);
HAL_PIN(error);
HAL_PIN(amp);
HAL_PIN(vel);
HAL_PIN(ccr3);
HAL_PIN(en_index);
HAL_PIN(indexprint);

// simulation
HAL_PIN(sim_pos);    // shaft angle [rad]
HAL_PIN(sim_rev);    // count direction reversed
HAL_PIN(sim_index);  // index angle in the encoder's raw angle [rad]
HAL_PIN(sim_boot_zero);  // 1: count from 0 at boot like a real incremental
                         // encoder (raw angle - raw angle at boot); 0: the
                         // raw shaft angle, so commutation starts right
HAL_PIN(sim_fail);   // 1: the encoder is broken (cable): position frozen, error
HAL_PIN(sim_freeze);  // fault: angle held
HAL_PIN(sim_scale);   // fault: gain on angle changes (1 = none)
HAL_PIN(sim_offset);  // fault: added angle [rad]
HAL_PIN(sim_res);    // the encoder's counts per revolution (0: res)

struct enc_fb_ctx_t {
  int init;
  int32_t tcount;  // the encoder's own count, 0..sim_res-1
  int32_t tim;     // the drive's timer, 0..res-1
  int printed, cnt;
  float boot;      // raw angle at boot (sim_boot_zero)
  float absoffset;
  float last_raw;  // shaft angle at the last call
  float off;       // what the injected faults added to the shaft angle
};

static int32_t count_of(float angle, int32_t res) {
  int32_t c = (int32_t)floorf(mod(angle) / (2.0f * M_PI) * res) % res;
  return c < 0 ? c + res : c;
}

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct enc_fb_pin_ctx_t *pins = (struct enc_fb_pin_ctx_t *)pin_ptr;
  PIN(res)       = 16384;
  PIN(ires)      = 1024;
  PIN(sim_scale) = 1.0;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct enc_fb_ctx_t *ctx      = (struct enc_fb_ctx_t *)ctx_ptr;
  struct enc_fb_pin_ctx_t *pins = (struct enc_fb_pin_ctx_t *)pin_ptr;

  if(PIN(sim_fail) > 0.0) {  // nothing counts any more; the drive sees the error
    PIN(error) = 1.0;
    PIN(index) = 0.0;
    return;
  }

  int32_t res = MAX((int32_t)PIN(res), 1);
  int32_t n   = PIN(sim_res) > 0.0 ? (int32_t)PIN(sim_res) : res;  // the encoder's
  float raw   = PIN(sim_rev) > 0.0 ? -PIN(sim_pos) : PIN(sim_pos);
  if(!ctx->init) {
    ctx->boot     = PIN(sim_boot_zero) > 0.0 ? raw : 0.0;
    ctx->last_raw = raw;
    ctx->off      = 0.0;
  }
  // faults as an offset to the shaft angle, so without one the counts are
  // exactly the shaft's (no rounding summed up)
  float d = minus(raw, ctx->last_raw);
  if(PIN(sim_freeze) > 0.0) {
    ctx->off = mod(ctx->off - d);
  } else if(PIN(sim_scale) != 1.0) {
    ctx->off = mod(ctx->off + d * (PIN(sim_scale) - 1.0));
  }
  ctx->last_raw = raw;
  int32_t tc    = count_of(raw + ctx->off + PIN(sim_offset) - ctx->boot, n);
  int32_t ic    = count_of(PIN(sim_index) - ctx->boot, n);

  if(!ctx->init) {
    ctx->init   = 1;
    ctx->tcount = tc;
    ctx->tim    = tc % res;
  }
  ctx->tim %= res;  // res changed: the timer wraps at the new reload

  // the encoder's counts since the last cycle; an index pulse when it
  // stepped onto, or across, the index count
  int32_t step = tc - ctx->tcount;
  if(step > n / 2) {
    step -= n;
  } else if(step < -n / 2) {
    step += n;
  }
  int crossed = 0;
  int32_t at  = 0;  // the timer at the index
  for(int32_t i = 1; i <= ABS(step) && !crossed; i++) {
    int32_t k = (ctx->tcount + (step > 0 ? i : -i)) % n;
    k += k < 0 ? n : 0;
    if(k == ic) {
      crossed = 1;
      at      = ((ctx->tim + (step > 0 ? i : -i)) % res + res) % res;
    }
  }
  ctx->tcount = tc;
  ctx->tim    = ((ctx->tim + step) % res + res) % res;
  int32_t c   = ctx->tim;

  PIN(index) = crossed;
  if(crossed && PIN(en_index) > 0.0) {
    PIN(state)     = 3.0;
    PIN(ccr3)      = at;
    ctx->absoffset = mod(at * 2.0f * M_PI / res);
    if(PIN(indexprint) > 0.0) {
      ctx->cnt     = at;
      ctx->printed = 1;
    }
  }

  float p      = mod(c * 2.0f * M_PI / res);
  PIN(pos)     = p;
  PIN(ipos)    = p;
  PIN(abs_pos) = minus(p, ctx->absoffset);
  PIN(error)   = 0.0;
  PIN(state)   = MAX(PIN(state), 1.0);
  PIN(quad)    = (c & 3) + 1;
  PIN(oquad)   = PIN(quad);
}

static void nrt_func(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct enc_fb_ctx_t *ctx = (struct enc_fb_ctx_t *)ctx_ptr;
  if(ctx->printed) {
    ctx->printed = 0;
    printf("cnt = %i\n", (int)ctx->cnt);
  }
}

hal_comp_t enc_fb_comp_struct = {
    .name      = "enc_fb",
    .nrt       = nrt_func,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct enc_fb_ctx_t),
    .pin_count = sizeof(struct enc_fb_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
