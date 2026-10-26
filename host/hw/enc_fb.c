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
 * Reference marks (sim_ref_mode) on the unwrapped position sim_abs_pos:
 * 0 = once per revolution at sim_index (above); 1 = a single mark at
 * sim_ref_pos (a linear scale with one reference actuator); 2 = marks every
 * sim_ref_period from sim_ref_pos (a scale with periodic marks). On a mark
 * the counter is latched, as on the drive.
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
HAL_PIN(sim_abs_pos);     // unwrapped shaft position [rad] (hv0.sim_abs_pos)
HAL_PIN(sim_ref_mode);    // 0 per revolution, 1 single, 2 periodic
HAL_PIN(sim_ref_pos);     // mark position on sim_abs_pos [rad]
HAL_PIN(sim_ref_period);  // mark spacing, mode 2 [rad]
HAL_PIN(sim_absolute);  // 1: an absolute encoder, state 3 from the start

struct enc_fb_ctx_t {
  int init;
  int32_t count;  // raw count, 0..res-1
  float boot;     // raw angle at boot (sim_boot_zero)
  float absoffset;
  float last_raw;  // shaft angle at the last call
  float off;       // what the injected faults added to the shaft angle
  double last_abs;
  int abs_init;    // last_abs valid
};

static int32_t count_of(float angle, int32_t res) {
  int32_t c = (int32_t)floorf(mod(angle) / (2.0f * M_PI) * res) % res;
  return c < 0 ? c + res : c;
}

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct enc_fb_pin_ctx_t *pins = (struct enc_fb_pin_ctx_t *)pin_ptr;
  PIN(res)  = 16384;
  PIN(ires) = 1024;
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
  int32_t c   = count_of(raw + ctx->off + PIN(sim_offset) - ctx->boot, res);
  int32_t ic  = count_of(PIN(sim_index) - ctx->boot, res);

  if(!ctx->init) {
    ctx->init  = 1;
    ctx->count = c;
  }
  if(PIN(sim_absolute) > 0.0 && PIN(state) < 3.0) {
    PIN(state)     = 3.0;
    ctx->absoffset = mod(ic * 2.0f * M_PI / res);
  }

  // index pulse: the count stepped onto, or across, the index count
  int32_t step = c - ctx->count;
  if(step > res / 2) {
    step -= res;
  } else if(step < -res / 2) {
    step += res;
  }
  int crossed = 0;
  for(int32_t i = 1; i <= ABS(step) && !crossed; i++) {
    int32_t k = ctx->count + (step > 0 ? i : -i);
    k %= res;
    if(k < 0) {
      k += res;
    }
    crossed = k == ic;
  }
  ctx->count = c;

  // reference marks on the unwrapped position
  int mode = (int)PIN(sim_ref_mode);
  if(mode == 1 || mode == 2) {
    double a = PIN(sim_abs_pos), b = ctx->last_abs;
    double lo = a < b ? a : b, hi = a < b ? b : a;
    crossed  = 0;
    double m = PIN(sim_ref_pos);
    if(mode == 2 && PIN(sim_ref_period) > 0.0) {  // nearest mark at or above lo
      double per = PIN(sim_ref_period);
      m += ceil((lo - m) / per) * per;
    }
    if(ctx->abs_init && m >= lo && m <= hi && hi > lo) {
      crossed = 1;
      float raw_m = PIN(sim_rev) > 0.0 ? -(float)m : (float)m;
      ic          = count_of(raw_m - ctx->boot, res);
    }
  }
  ctx->last_abs = PIN(sim_abs_pos);
  ctx->abs_init = 1;

  PIN(index) = crossed;
  if(crossed && PIN(en_index) > 0.0) {
    PIN(state)     = 3.0;
    PIN(ccr3)      = ic;
    ctx->absoffset = mod(ic * 2.0f * M_PI / res);
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

hal_comp_t enc_fb_comp_struct = {
    .name      = "enc_fb",
    .nrt       = 0,
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
