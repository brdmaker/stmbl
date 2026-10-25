#include "vib_comp.h"
#include "commands.h"
#include "hal.h"
#include "defines.h"
#include "vibbuf.h"
#include "vibdsp.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The vibration sensor's processing (docs/vibsensor.md): the accelerometer's
// samples (vibbuf, 26.7 kHz x y z) become, per reporting interval and axis,
//   rms_*    acceleration RMS (g), after the hp filter (gravity, drift gone)
//   peak_*   largest |acceleration| (g); crest_* = peak / rms
//   kurt_*   kurtosis: 1.5 a sine, 3 noise, more: impacts (bearings, chipping)
//   vel_*    velocity RMS (mm/s) in vel_hp..vel_lp: ISO 10816 / 20816's
//            measure with 10..1000 Hz
// and per FFT block (fft_n samples of fft_axis, Hann window):
//   fft_f, fft_a   the strongest peak: Hz, g RMS
//   ord1..3        spindle orders 1x 2x 3x rpm (g RMS): unbalance,
//                  misalignment, looseness
//   tpf            tooth passing (teeth x rpm)
//   chat_f, chat_a the strongest peak in chat_lo..chat_hi that is no
//                  harmonic of the spindle (+-4 bins): chatter
//   env, env_f     bearing band (env_lo..env_hi) RMS, and the strongest
//                  repetition in its envelope (Hz): a bearing defect's
//                  BPFO / BPFI / BSF
// Alarms:
//   crash          latched when |a| (x y z, filtered) passes shock_lim (g);
//                  the rt path, so within a sample period. shock: the
//                  largest |a| since; reset = 1 clears both
//   level          0 ok, 1 vel_warn, 2 vel_alarm (largest vel_*)
//   temp_level     0 ok, 1 temp_warn, 2 temp_alarm
// Every parameter is a pin: set it from the config, the console, or over CAN.
// The FFTs run in nrt; rt only filters and sums. One instance: one sensor.

HAL_COMP(vib);

HAL_PIN(en);
HAL_PIN(fs);        // samples per second (from the sensor)
HAL_PIN(hp);        // Hz, acceleration high pass
HAL_PIN(interval);  // s, reporting interval of the rms / peak / kurt / vel values
HAL_PIN(vel_hp);
HAL_PIN(vel_lp);
HAL_PIN(env_lo);
HAL_PIN(env_hi);
HAL_PIN(env_lp);    // Hz, envelope smoothing
HAL_PIN(fft_n);     // 256..4096, a power of two
HAL_PIN(fft_axis);  // 0 x, 1 y, 2 z
HAL_PIN(rpm);       // the spindle's speed (from the machine)
HAL_PIN(teeth);
HAL_PIN(chat_lo);
HAL_PIN(chat_hi);
HAL_PIN(shock_lim);
HAL_PIN(reset);
HAL_PIN(vel_warn);
HAL_PIN(vel_alarm);
HAL_PIN(temp);
HAL_PIN(temp_warn);
HAL_PIN(temp_alarm);

HAL_PIN(rms_x);
HAL_PIN(rms_y);
HAL_PIN(rms_z);
HAL_PIN(peak_x);
HAL_PIN(peak_y);
HAL_PIN(peak_z);
HAL_PIN(crest_x);
HAL_PIN(crest_y);
HAL_PIN(crest_z);
HAL_PIN(kurt_x);
HAL_PIN(kurt_y);
HAL_PIN(kurt_z);
HAL_PIN(vel_x);
HAL_PIN(vel_y);
HAL_PIN(vel_z);
HAL_PIN(env);
HAL_PIN(env_f);
HAL_PIN(fft_f);
HAL_PIN(fft_a);
HAL_PIN(ord1);
HAL_PIN(ord2);
HAL_PIN(ord3);
HAL_PIN(tpf);
HAL_PIN(chat_f);
HAL_PIN(chat_a);
HAL_PIN(crash);
HAL_PIN(shock);
HAL_PIN(level);
HAL_PIN(temp_level);
HAL_PIN(reports);  // intervals reported
HAL_PIN(ffts);     // FFT blocks done
HAL_PIN(lost);     // FFT blocks dropped (nrt too slow), plus samples the buffer dropped

#define FFT_MAX 4096
#define ENV_DEC 8      // the envelope is decimated by 8 (3.3 kHz)
#define ENV_N 1024     // its FFT: 0.3 s, 3.3 Hz bins

vibbuf_t vibbuf;

// Big buffers are static: one instance.
static float blk[2][FFT_MAX];      // samples of fft_axis, double buffered (rt fills one)
static float eblk[2][ENV_N];       // the envelope, decimated
static float re[FFT_MAX], im[FFT_MAX];
static float spec[FFT_MAX / 2 + 1];  // the last spectrum's power, for the console and CAN
static float espec[ENV_N / 2 + 1];
static vib_spec_t last_spec;

struct vib_ctx_t {
  vib_bq_t hp[3], vhp[3], vlp[3], ehp, elp, esm;
  float vint[3], leak;
  vib_mom_t acc[3], vel[3], envm;
  uint32_t n_int;      // samples in this interval
  int fill, fill_n, blk_w;
  volatile int ready;  // -1 none, else the buffer nrt is to do
  int efill, edec, eblk_w;
  volatile int eready;
  float set[8];        // the parameters the filters were made for
  int n;               // FFT length in use
};

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct vib_ctx_t *ctx      = (struct vib_ctx_t *)ctx_ptr;
  struct vib_pin_ctx_t *pins = (struct vib_pin_ctx_t *)pin_ptr;
  PIN(en)         = 1;
  PIN(hp)         = 2;
  PIN(interval)   = 0.1;
  PIN(vel_hp)     = 10;
  PIN(vel_lp)     = 1000;
  PIN(env_lo)     = 1000;
  PIN(env_hi)     = 6000;
  PIN(env_lp)     = 500;
  PIN(fft_n)      = 2048;
  PIN(fft_axis)   = 0;
  PIN(teeth)      = 2;
  PIN(chat_lo)    = 200;
  PIN(chat_hi)    = 5000;
  PIN(shock_lim)    = 8;
  PIN(vel_warn)   = 4.5;   // ISO 10816-3, group 2 rigid: B/C boundary 2.8, C/D 4.5;
  PIN(vel_alarm)  = 11.2;  // machine tools vary: set them from the machine's own baseline
  PIN(temp_warn)  = 60;
  PIN(temp_alarm) = 80;
  memset(ctx, 0, sizeof(*ctx));
  ctx->ready = ctx->eready = -1;
  if(vibbuf.fs <= 0.0f) {
    vibbuf.fs        = 26667.0f;
    vibbuf.g_per_lsb = 0.000488f;
  }
}

static int pow2_n(float v) {
  int n = 256;
  while(n < FFT_MAX && n * 1.5f <= v) {
    n <<= 1;
  }
  return n;
}

// (re)make the filters when a parameter they use changed
static void setup(struct vib_ctx_t *ctx, struct vib_pin_ctx_t *pins, float fs) {
  float s[8] = {fs, PIN(hp), PIN(vel_hp), PIN(vel_lp), PIN(env_lo), PIN(env_hi), PIN(env_lp), PIN(fft_n) + 10 * PIN(fft_axis)};
  if(!memcmp(s, ctx->set, sizeof(s))) {
    return;
  }
  memcpy(ctx->set, s, sizeof(s));
  for(int i = 0; i < 3; i++) {
    vib_bq_hp(&ctx->hp[i], PIN(hp), fs);
    vib_bq_hp(&ctx->vhp[i], PIN(vel_hp), fs);
    vib_bq_lp(&ctx->vlp[i], PIN(vel_lp), fs);
  }
  vib_bq_hp(&ctx->ehp, PIN(env_lo), fs);
  vib_bq_lp(&ctx->elp, PIN(env_hi), fs);
  vib_bq_lp(&ctx->esm, PIN(env_lp), fs);
  ctx->leak  = expf(-2.0f * (float)M_PI * 1.0f / fs);  // the integrator forgets below ~1 Hz
  ctx->n     = pow2_n(PIN(fft_n));
  PIN(fft_n) = ctx->n;
  ctx->set[7] = PIN(fft_n) + 10 * PIN(fft_axis);
  ctx->fill = ctx->efill = ctx->edec = 0;  // a block from before is of other samples
}

static void report(struct vib_ctx_t *ctx, struct vib_pin_ctx_t *pins) {
  float *rms[3]   = {&PIN(rms_x), &PIN(rms_y), &PIN(rms_z)};
  float *peak[3]  = {&PIN(peak_x), &PIN(peak_y), &PIN(peak_z)};
  float *crest[3] = {&PIN(crest_x), &PIN(crest_y), &PIN(crest_z)};
  float *kurt[3]  = {&PIN(kurt_x), &PIN(kurt_y), &PIN(kurt_z)};
  float *vel[3]   = {&PIN(vel_x), &PIN(vel_y), &PIN(vel_z)};
  float vmax      = 0.0f;
  for(int i = 0; i < 3; i++) {
    *rms[i]   = vib_mom_rms(&ctx->acc[i]);
    *peak[i]  = ctx->acc[i].peak;
    *crest[i] = *rms[i] > 1e-6f ? *peak[i] / *rms[i] : 0.0f;
    *kurt[i]  = vib_mom_kurt(&ctx->acc[i]);
    *vel[i]   = vib_mom_rms(&ctx->vel[i]);
    vmax      = MAX(vmax, *vel[i]);
    memset(&ctx->acc[i], 0, sizeof(vib_mom_t));
    memset(&ctx->vel[i], 0, sizeof(vib_mom_t));
  }
  PIN(env) = vib_mom_rms(&ctx->envm);
  memset(&ctx->envm, 0, sizeof(vib_mom_t));
  PIN(level) = vmax >= PIN(vel_alarm) ? 2 : vmax >= PIN(vel_warn) ? 1 : 0;
  PIN(reports)++;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct vib_ctx_t *ctx      = (struct vib_ctx_t *)ctx_ptr;
  struct vib_pin_ctx_t *pins = (struct vib_pin_ctx_t *)pin_ptr;
  float fs                   = vibbuf.fs;
  PIN(fs)                    = fs;
  setup(ctx, pins, fs);
  if(PIN(reset) > 0.0f) {
    PIN(crash)       = 0;
    PIN(shock)  = 0;
    PIN(reset) = 0;
  }
  PIN(temp_level) = PIN(temp) >= PIN(temp_alarm) ? 2 : PIN(temp) >= PIN(temp_warn) ? 1 : 0;
  if(PIN(en) <= 0.0f) {
    vibbuf.rd = vibbuf.wr;
    return;
  }
  float g     = vibbuf.g_per_lsb;
  float dt    = 1.0f / fs;
  int axis    = CLAMP((int)PIN(fft_axis), 0, 2);
  float crash = PIN(shock_lim);
  uint32_t per_int = (uint32_t)MAX(1.0f, PIN(interval) * fs);
  while(vibbuf_count(&vibbuf)) {
    vib_xyz_t r = vibbuf_get(&vibbuf);
    float raw[3] = {r.x * g, r.y * g, r.z * g};
    float a[3];
    float m2 = 0.0f;
    for(int i = 0; i < 3; i++) {
      a[i] = vib_bq(&ctx->hp[i], raw[i]);
      vib_mom_add(&ctx->acc[i], a[i]);
      ctx->vint[i] = ctx->vint[i] * ctx->leak + a[i] * VIB_G * 1000.0f * dt;  // mm/s
      float v      = vib_bq(&ctx->vlp[i], vib_bq(&ctx->vhp[i], ctx->vint[i]));
      vib_mom_add(&ctx->vel[i], v);
      m2 += a[i] * a[i];
    }
    if(crash > 0.0f && m2 >= crash * crash) {
      PIN(crash)      = 1;
      PIN(shock) = MAX(PIN(shock), sqrtf(m2));
    }
    // the bearing band and its envelope
    float e = vib_bq(&ctx->elp, vib_bq(&ctx->ehp, a[axis]));
    vib_mom_add(&ctx->envm, e);
    float env = vib_bq(&ctx->esm, fabsf(e));
    if(++ctx->edec >= ENV_DEC) {
      ctx->edec = 0;
      eblk[ctx->eblk_w][ctx->efill++] = env;
      if(ctx->efill >= ENV_N) {
        ctx->efill = 0;
        if(ctx->eready < 0) {
          ctx->eready = ctx->eblk_w;
          ctx->eblk_w ^= 1;
        }
      }
    }
    // the FFT block
    blk[ctx->blk_w][ctx->fill++] = a[axis];
    if(ctx->fill >= ctx->n) {
      ctx->fill = 0;
      if(ctx->ready < 0) {
        ctx->fill_n = ctx->n;
        ctx->ready  = ctx->blk_w;
        ctx->blk_w ^= 1;
      } else {
        PIN(lost)++;  // nrt still has the other one: this block is written over
      }
    }
    if(++ctx->n_int >= per_int) {
      ctx->n_int = 0;
      report(ctx, pins);
    }
  }
  if(vibbuf.dropped) {
    PIN(lost) += vibbuf.dropped;
    vibbuf.dropped = 0;
  }
}

static void spectrum(const float *x, int n, float fs, float *p, vib_spec_t *s) {
  float mean = 0.0f;
  for(int i = 0; i < n; i++) {
    mean += x[i];
  }
  mean /= n;
  for(int i = 0; i < n; i++) {
    re[i] = x[i] - mean;
    im[i] = 0.0f;
  }
  s->s2 = vib_hann(re, n);
  vib_fft(re, im, n);
  for(int k = 0; k <= n / 2; k++) {
    p[k] = re[k] * re[k] + im[k] * im[k];
  }
  s->p  = p;
  s->n  = n;
  s->fs = fs;
}

static void nrt_func(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct vib_ctx_t *ctx      = (struct vib_ctx_t *)ctx_ptr;
  struct vib_pin_ctx_t *pins = (struct vib_pin_ctx_t *)pin_ptr;
  float fs                   = vibbuf.fs;
  if(ctx->ready >= 0) {
    spectrum(blk[ctx->ready], ctx->fill_n, fs, spec, &last_spec);
    ctx->ready = -1;
    float f, a;
    vib_peak(&last_spec, 2.0f, fs / 2, 0.0f, 0, &f, &a);
    PIN(fft_f) = f;
    PIN(fft_a) = a;
    float f1   = PIN(rpm) / 60.0f;
    if(f1 > 0.0f) {
      PIN(ord1) = vib_tone_rms(&last_spec, f1);
      PIN(ord2) = vib_tone_rms(&last_spec, 2 * f1);
      PIN(ord3) = vib_tone_rms(&last_spec, 3 * f1);
      PIN(tpf)  = PIN(teeth) > 0 ? vib_tone_rms(&last_spec, PIN(teeth) * f1) : 0.0f;
    } else {
      PIN(ord1) = PIN(ord2) = PIN(ord3) = PIN(tpf) = 0;
    }
    vib_peak(&last_spec, PIN(chat_lo), PIN(chat_hi), f1, 4, &f, &a);  // +-4 bins: a Hann tone's skirt
    PIN(chat_f) = f;
    PIN(chat_a) = a;
    PIN(ffts)++;
  }
  if(ctx->eready >= 0) {
    vib_spec_t es;
    spectrum(eblk[ctx->eready], ENV_N, fs / ENV_DEC, espec, &es);
    ctx->eready = -1;
    float f, a;
    vib_peak(&es, 2.0f, es.fs / 2, 0.0f, 0, &f, &a);
    PIN(env_f) = f;
  }
}

// The last spectrum, for the console (vibspec) and the CAN spectrum frames.
const vib_spec_t *vib_last_spectrum(void) {
  return last_spec.p ? &last_spec : 0;
}

// vibspec [N]: the N strongest peaks of the last spectrum (Hz, g RMS)
void vibspec(char *ptr) {
  int n = 5;
  sscanf(ptr, " %i", &n);
  const vib_spec_t *s = vib_last_spectrum();
  if(!s) {
    printf("vibspec: no spectrum yet\n");
    return;
  }
  printf("vibspec: n %i, %.2f Hz per bin\n", s->n, (double)vib_bin_hz(s));
  // local maxima, largest first
  static uint8_t used[FFT_MAX / 2 + 1];
  memset(used, 0, sizeof(used));
  for(int i = 0; i < n; i++) {
    int best = -1;
    for(int k = 2; k < s->n / 2; k++) {
      if(!used[k] && s->p[k] >= s->p[k - 1] && s->p[k] >= s->p[k + 1] && (best < 0 || s->p[k] > s->p[best])) {
        best = k;
      }
    }
    if(best < 0) {
      break;
    }
    float f, a;
    vib_peak(s, (best - 0.4f) * vib_bin_hz(s), (best + 0.4f) * vib_bin_hz(s), 0.0f, 0, &f, &a);
    printf("  %8.1f Hz  %.4f g\n", (double)f, (double)a);
    for(int k = MAX(0, best - 4); k <= MIN(s->n / 2, best + 4); k++) {
      used[k] = 1;
    }
  }
}
COMMAND("vibspec", vibspec, "vibspec [N]: the N strongest peaks of the vibration spectrum");

hal_comp_t vib_comp_struct = {
    .name      = "vib",
    .nrt       = nrt_func,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct vib_ctx_t),
    .pin_count = sizeof(struct vib_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
