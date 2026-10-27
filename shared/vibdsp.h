/*
 * Vibration signal processing for the vibration sensor (docs/vibsensor.md):
 * filters, an FFT, and what is read from a spectrum. Plain C, no state of its
 * own; the vib component (shared/comps/vib.c) holds the state. Used on the
 * sensor's G4 and in the host simulator alike.
 */
#pragma once
#include <stdint.h>

#define VIB_G 9.80665f  // m/s^2 per g

// ---------------------------------------------------------------- biquad
// Direct form II transposed, 2nd-order Butterworth sections (RBJ cookbook).
typedef struct {
  float b0, b1, b2, a1, a2;
  float z1, z2;
} vib_bq_t;

void vib_bq_lp(vib_bq_t *f, float fc, float fs);  // low pass at fc
void vib_bq_hp(vib_bq_t *f, float fc, float fs);  // high pass at fc
void vib_bq_reset(vib_bq_t *f);
static inline float vib_bq(vib_bq_t *f, float x) {
  float y = f->b0 * x + f->z1;
  f->z1   = f->b1 * x - f->a1 * y + f->z2;
  f->z2   = f->b2 * x - f->a2 * y;
  return y;
}

// ---------------------------------------------------------------- moments
// Running sums over a reporting interval: RMS, peak, kurtosis.
typedef struct {
  double s2, s4;
  float peak;
  uint32_t n;
} vib_mom_t;

static inline void vib_mom_add(vib_mom_t *m, float x) {
  float x2 = x * x;
  m->s2 += x2;
  m->s4 += (double)x2 * x2;
  float a = x < 0.0f ? -x : x;
  if(a > m->peak) {
    m->peak = a;
  }
  m->n++;
}
float vib_mom_rms(const vib_mom_t *m);
float vib_mom_kurt(const vib_mom_t *m);  // E[x^4] / E[x^2]^2: sine 1.5, noise 3, impacts more

// ---------------------------------------------------------------- FFT
// In-place complex radix-2 FFT; n a power of two. re/im: n floats each.
void vib_fft(float *re, float *im, int n);
// Hann window in place, and its sum of squares (needed for amplitudes).
float vib_hann(float *x, int n);

// A one-sided power spectrum: p[k] = |X_k|^2, k = 0..n/2. With s2 the
// window's sum of squares, a band's RMS is sqrt(2 sum p / (n s2)): the
// window's leakage stays inside a few bins, so the sum over a band holds a
// tone's whole power whatever its position between bins.
typedef struct {
  const float *p;
  int n;       // the FFT's length; p has n / 2 + 1 bins
  float fs;    // sample rate
  float s2;    // the window's sum of squares
} vib_spec_t;

static inline float vib_bin_hz(const vib_spec_t *s) {
  return s->fs / s->n;
}
float vib_band_rms(const vib_spec_t *s, float f_lo, float f_hi);   // RMS in [f_lo, f_hi]
float vib_tone_rms(const vib_spec_t *s, float f);                  // RMS of a tone at f (+-3 bins)
float vib_bin_rms(const vib_spec_t *s, int k);                     // one bin, as a tone's RMS
// The strongest peak in [f_lo, f_hi], away from the harmonics of f_excl
// (f_excl <= 0: none) by more than excl_bins: its frequency (interpolated)
// and RMS (of the bins around it that are not excluded). Returns 0 if the
// band holds no bin.
int vib_peak(const vib_spec_t *s, float f_lo, float f_hi, float f_excl, int excl_bins, float *f, float *rms);
