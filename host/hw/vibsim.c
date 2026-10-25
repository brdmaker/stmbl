/*
 * Simulation stand-in for the vibration sensor's IIS3DWB (docs/vibsensor.md):
 * a machine's vibration, sampled as the sensor does (fs, int16 counts at
 * range_g full scale), written into vibbuf for the vib component. What it is
 * made of, each a pin (amplitudes in g peak; 0: off):
 *   gravity on z (1 g: the filters must take it out), noise (g RMS)
 *   unb        unbalance: 1x rpm, rotating in x / y
 *   mis        misalignment: 2x rpm, x and axial z
 *   tpf_a      tooth passing: teeth x rpm, x / y
 *   chat_f, chat_a   chatter: its own frequency, no harmonic of the spindle
 *   screw_a    a ball screw's turn: feed / pitch (mm/min, mm), z
 *   brg_f, brg_a     a bearing defect: impacts brg_f times a second, each
 *                    ringing the structure's resonance res_f
 *   crash = 1  one impact of crash_g ringing at 700 Hz (the pin goes back to 0)
 *   temp       passed through: the housing's temperature
 */
#include "vibsim_comp.h"
#include "hal.h"
#include "defines.h"
#include "vibbuf.h"
#include <math.h>

HAL_COMP(vibsim);

HAL_PIN(fs);
HAL_PIN(range_g);
HAL_PIN(noise);
HAL_PIN(gravity);
HAL_PIN(rpm);
HAL_PIN(unb);
HAL_PIN(mis);
HAL_PIN(teeth);
HAL_PIN(tpf_a);
HAL_PIN(chat_f);
HAL_PIN(chat_a);
HAL_PIN(feed);
HAL_PIN(pitch);
HAL_PIN(screw_a);
HAL_PIN(brg_f);
HAL_PIN(brg_a);
HAL_PIN(res_f);
HAL_PIN(crash);
HAL_PIN(crash_g);
HAL_PIN(temp);
HAL_PIN(samples);

struct res_t {  // a damped resonance, rung by impulses
  float y1, y2;
};

struct vibsim_ctx_t {
  double frac, ph1, phc, phs, brg_t;
  struct res_t brg, cr;
  uint32_t seed;
  int have_g2;
  float g2;
};

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct vibsim_ctx_t *ctx      = (struct vibsim_ctx_t *)ctx_ptr;
  struct vibsim_pin_ctx_t *pins = (struct vibsim_pin_ctx_t *)pin_ptr;
  PIN(fs)      = 26667;
  PIN(range_g) = 16;
  PIN(gravity) = 1;
  PIN(teeth)   = 2;
  PIN(pitch)   = 5;
  PIN(res_f)   = 3000;
  PIN(crash_g) = 20;
  PIN(temp)    = 25;
  ctx->seed    = 12345;
}

static float uni(struct vibsim_ctx_t *ctx) {
  ctx->seed = ctx->seed * 1664525u + 1013904223u;
  return ((ctx->seed >> 8) + 0.5f) / 16777216.0f;
}

static float gauss(struct vibsim_ctx_t *ctx) {  // Box-Muller, deterministic
  if(ctx->have_g2) {
    ctx->have_g2 = 0;
    return ctx->g2;
  }
  float r = sqrtf(-2.0f * logf(uni(ctx))), t = 2.0f * (float)M_PI * uni(ctx);
  ctx->g2      = r * sinf(t);
  ctx->have_g2 = 1;
  return r * cosf(t);
}

// y[n] = 2 r cos(w) y[n-1] - r^2 y[n-2] + x[n]; an impulse of a sin(w)
// rings with a peak of about a
static float ring(struct res_t *s, float x, float f, float zeta, float fs) {
  float w = 2.0f * (float)M_PI * f / fs;
  float r = expf(-zeta * w);
  float y = 2.0f * r * cosf(w) * s->y1 - r * r * s->y2 + x * sinf(w);
  s->y2   = s->y1;
  s->y1   = y;
  return y;
}

static int16_t count(float g, float lsb) {
  float c = g / lsb;
  c       = c > 32767.0f ? 32767.0f : c < -32768.0f ? -32768.0f : c;
  return (int16_t)lrintf(c);
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct vibsim_ctx_t *ctx      = (struct vibsim_ctx_t *)ctx_ptr;
  struct vibsim_pin_ctx_t *pins = (struct vibsim_pin_ctx_t *)pin_ptr;
  float fs                      = MAX(1000.0f, PIN(fs));
  float lsb                     = MAX(2.0f, PIN(range_g)) / 32768.0f;
  vibbuf.fs                     = fs;
  vibbuf.g_per_lsb              = lsb;
  double dph1                   = 2.0 * M_PI * PIN(rpm) / 60.0 / fs;
  double dphc                   = 2.0 * M_PI * PIN(chat_f) / fs;
  double dphs                   = PIN(pitch) > 0.0f ? 2.0 * M_PI * PIN(feed) / 60.0 / PIN(pitch) / fs : 0.0;
  float kick                    = 0.0f;
  if(PIN(crash) > 0.0f) {
    kick       = PIN(crash_g);
    PIN(crash) = 0;
  }
  ctx->frac += fs * period;
  while(ctx->frac >= 1.0) {
    ctx->frac -= 1.0;
    ctx->ph1 += dph1;
    ctx->phc += dphc;
    ctx->phs += dphs;
    float p1 = (float)ctx->ph1;
    float x = 0.0f, y = 0.0f, z = PIN(gravity);
    x += PIN(unb) * cosf(p1);
    y += PIN(unb) * sinf(p1);
    x += PIN(mis) * cosf(2.0f * p1 + 0.3f);
    z += PIN(mis) * 0.5f * cosf(2.0f * p1);
    x += PIN(tpf_a) * cosf(PIN(teeth) * p1);
    y += PIN(tpf_a) * sinf(PIN(teeth) * p1);
    x += PIN(chat_a) * sinf((float)ctx->phc);
    y += PIN(chat_a) * 0.7f * sinf((float)ctx->phc + 1.0f);
    z += PIN(screw_a) * sinf((float)ctx->phs);
    float imp = 0.0f;
    if(PIN(brg_f) > 0.0f && PIN(brg_a) > 0.0f) {
      ctx->brg_t += 1.0 / fs;
      if(ctx->brg_t >= 1.0 / PIN(brg_f)) {
        ctx->brg_t -= 1.0 / PIN(brg_f);
        imp = PIN(brg_a);
      }
    }
    float b = ring(&ctx->brg, imp, PIN(res_f), 0.03f, fs);
    float c = ring(&ctx->cr, kick, 700.0f, 0.05f, fs);
    kick    = 0.0f;
    x += b + c;
    y += 0.6f * b + 0.8f * c;
    z += 0.3f * b + 0.5f * c;
    if(PIN(noise) > 0.0f) {
      x += PIN(noise) * gauss(ctx);
      y += PIN(noise) * gauss(ctx);
      z += PIN(noise) * gauss(ctx);
    }
    vibbuf_put(&vibbuf, (vib_xyz_t){count(x, lsb), count(y, lsb), count(z, lsb)});
    PIN(samples)++;
  }
  ctx->ph1 = fmod(ctx->ph1, 2.0 * M_PI * 1000.0);  // keep the float phases precise
  ctx->phc = fmod(ctx->phc, 2.0 * M_PI * 1000.0);
  ctx->phs = fmod(ctx->phs, 2.0 * M_PI * 1000.0);
}

hal_comp_t vibsim_comp_struct = {
    .name      = "vibsim",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct vibsim_ctx_t),
    .pin_count = sizeof(struct vibsim_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
