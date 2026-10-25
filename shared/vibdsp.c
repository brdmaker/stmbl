#include "vibdsp.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------------------------------------------------------------- biquad
static void bq_set(vib_bq_t *f, float b0, float b1, float b2, float a0, float a1, float a2) {
  f->b0 = b0 / a0;
  f->b1 = b1 / a0;
  f->b2 = b2 / a0;
  f->a1 = a1 / a0;
  f->a2 = a2 / a0;
}

static float bq_w(float fc, float fs, float *cw) {
  float lim = 0.45f * fs;
  fc        = fc < 0.01f ? 0.01f : fc > lim ? lim : fc;
  float w   = 2.0f * (float)M_PI * fc / fs;
  *cw       = cosf(w);
  return sinf(w) / (2.0f * 0.70710678f);  // alpha, Q = 1/sqrt(2)
}

void vib_bq_lp(vib_bq_t *f, float fc, float fs) {
  float cw, al = bq_w(fc, fs, &cw);
  bq_set(f, (1 - cw) / 2, 1 - cw, (1 - cw) / 2, 1 + al, -2 * cw, 1 - al);
}

void vib_bq_hp(vib_bq_t *f, float fc, float fs) {
  float cw, al = bq_w(fc, fs, &cw);
  bq_set(f, (1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + al, -2 * cw, 1 - al);
}

void vib_bq_reset(vib_bq_t *f) {
  f->z1 = f->z2 = 0.0f;
}

// ---------------------------------------------------------------- moments
float vib_mom_rms(const vib_mom_t *m) {
  return m->n ? sqrtf((float)(m->s2 / m->n)) : 0.0f;
}

float vib_mom_kurt(const vib_mom_t *m) {
  if(!m->n || m->s2 <= 0.0) {
    return 0.0f;
  }
  double v = m->s2 / m->n;
  return (float)(m->s4 / m->n / (v * v));
}

// ---------------------------------------------------------------- FFT
void vib_fft(float *re, float *im, int n) {
  for(int i = 1, j = 0; i < n; i++) {  // bit reversal
    int bit = n >> 1;
    for(; j & bit; bit >>= 1) {
      j ^= bit;
    }
    j ^= bit;
    if(i < j) {
      float t = re[i];
      re[i]   = re[j];
      re[j]   = t;
      t       = im[i];
      im[i]   = im[j];
      im[j]   = t;
    }
  }
  for(int len = 2; len <= n; len <<= 1) {
    float ang = -2.0f * (float)M_PI / len;
    float wr = cosf(ang), wi = sinf(ang);
    for(int i = 0; i < n; i += len) {
      float cr = 1.0f, ci = 0.0f;
      for(int k = 0; k < len / 2; k++) {
        int a = i + k, b = a + len / 2;
        float tr = re[b] * cr - im[b] * ci;
        float ti = re[b] * ci + im[b] * cr;
        re[b]    = re[a] - tr;
        im[b]    = im[a] - ti;
        re[a] += tr;
        im[a] += ti;
        float t = cr * wr - ci * wi;
        ci      = cr * wi + ci * wr;
        cr      = t;
      }
    }
  }
}

float vib_hann(float *x, int n) {
  float s2 = 0.0f;
  for(int i = 0; i < n; i++) {
    float w = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * i / n);
    x[i] *= w;
    s2 += w * w;
  }
  return s2;
}

// ---------------------------------------------------------------- spectrum
static float to_rms(const vib_spec_t *s, double p) {
  return sqrtf((float)(2.0 * p / ((double)s->n * s->s2)));
}

static int bin_of(const vib_spec_t *s, float f) {
  int k = (int)lroundf(f / vib_bin_hz(s));
  return k < 0 ? 0 : k > s->n / 2 ? s->n / 2 : k;
}

float vib_bin_rms(const vib_spec_t *s, int k) {
  return to_rms(s, s->p[k]);
}

float vib_band_rms(const vib_spec_t *s, float f_lo, float f_hi) {
  int a = bin_of(s, f_lo), b = bin_of(s, f_hi);
  if(a < 1) {
    a = 1;  // not DC
  }
  double p = 0.0;
  for(int k = a; k <= b; k++) {
    p += s->p[k];
  }
  return to_rms(s, p);
}

float vib_tone_rms(const vib_spec_t *s, float f) {
  float df = 3.0f * vib_bin_hz(s);
  return f > df ? vib_band_rms(s, f - df, f + df) : 0.0f;
}

static int near_harmonic(const vib_spec_t *s, int k, float f_excl, int excl_bins) {
  if(f_excl <= 0.0f) {
    return 0;
  }
  float f = k * vib_bin_hz(s);
  int h   = (int)lroundf(f / f_excl);
  if(h < 1) {
    return fabsf(f - f_excl) / vib_bin_hz(s) <= excl_bins;
  }
  return fabsf(f - h * f_excl) / vib_bin_hz(s) <= excl_bins;
}

int vib_peak(const vib_spec_t *s, float f_lo, float f_hi, float f_excl, int excl_bins, float *f, float *rms) {
  int a = bin_of(s, f_lo), b = bin_of(s, f_hi), best = -1;
  if(a < 1) {
    a = 1;
  }
  for(int k = a; k <= b; k++) {
    if(near_harmonic(s, k, f_excl, excl_bins)) {
      continue;
    }
    if(best < 0 || s->p[k] > s->p[best]) {
      best = k;
    }
  }
  if(best < 0) {
    *f = *rms = 0.0f;
    return 0;
  }
  // Gaussian interpolation on the log magnitudes: for a Hann window within a
  // few hundredths of a bin
  float d = 0.0f;
  if(best > 0 && best < s->n / 2) {
    float l = logf(s->p[best - 1] + 1e-30f), c = logf(s->p[best] + 1e-30f), r = logf(s->p[best + 1] + 1e-30f);
    float q = l - 2.0f * c + r;
    if(q < 0.0f) {
      d = 0.5f * (l - r) / q;
      d = d > 0.5f ? 0.5f : d < -0.5f ? -0.5f : d;
    }
  }
  *f = (best + d) * vib_bin_hz(s);
  double p = 0.0;
  for(int k = best - 3; k <= best + 3; k++) {  // not the excluded tones' skirts
    if(k >= 1 && k <= s->n / 2 && !near_harmonic(s, k, f_excl, excl_bins)) {
      p += s->p[k];
    }
  }
  *rms = to_rms(s, p);
  return 1;
}
