/*
 * shared/bip_loop.c, brdstepper's two-coil current loop, against an exact
 * model of two coils (R, L, back-EMF of a turning rotor): step response,
 * tracking while turning, the voltage limit, current limit, voltage mode,
 * disable. Prints TAP.
 */
#include "bip_loop.h"
#include <math.h>
#include <stdio.h>

static int n, fails;
static void ok(int c, const char *name, double got) {
  n++;
  fails += !c;
  printf("%s %d - bip: %s\n", c ? "ok" : "not ok", n, name);
  if(!c) {
    printf("#   got %g\n", got);
  }
}

// NEMA11 28x45: 6.8 ohm, 4.9 mH, psi 2.8 mV s/rad, 50 pole pairs, 24 V
static const float R = 6.8f, L = 0.0049f, PSI = 0.0028f, UDC = 24.0f, DT = 0.00005f;
static bip_param_t P = {.r = 6.8f, .l = 0.0049f, .psi = 0.0028f, .cur_bw = 3000.0f, .cur_ff = 1.0f, .cur_ind = 1.0f,
                        .udc = 24.0f, .max_cur = 1.0f, .max_duty = 0.95f, .volt_mode = 0};

typedef struct {
  float ia, ib, e, w;  // coil currents, electrical angle and speed
} coils_t;

// one loop period: the bridges' average voltage on the coils, 20 sub-steps
static void plant(coils_t *c, const bip_out_t *o) {
  float h = DT / 20;
  for(int k = 0; k < 20; k++) {
    float ua = o->duty_a * UDC, ub = o->duty_b * UDC;
    c->ia += (ua - R * c->ia + PSI * c->w * sinf(c->e)) / L * h;
    c->ib += (ub - R * c->ib - PSI * c->w * cosf(c->e)) / L * h;
    c->e += c->w * h;
  }
}

static float iq_of(const coils_t *c) {
  return -c->ia * sinf(c->e) + c->ib * cosf(c->e);
}
static float id_of(const coils_t *c) {
  return c->ia * cosf(c->e) + c->ib * sinf(c->e);
}

int main(void) {
  bip_state_t s;
  bip_out_t o;
  coils_t c = {0};

  // 1. a current step at standstill (rotor at 30 degrees electrical)
  bip_reset(&s);
  c.e = 0.5236f;
  float t_rise = -1, over = 0;
  for(int i = 0; i < 200; i++) {  // 10 ms
    bip_step(&s, &P, c.e, 0, 0, 0.5f, c.ia, c.ib, 1, DT, &o);
    plant(&c, &o);
    float iq = iq_of(&c);
    if(t_rise < 0 && iq >= 0.45f) {
      t_rise = (i + 1) * DT;
    }
    over = fmaxf(over, iq - 0.5f);
  }
  ok(t_rise > 0 && t_rise < 0.0012f, "0.5 A step: 90 % within 1.2 ms (bandwidth 3000 rad/s)", t_rise);
  // the F3 curpid design: the integrator charges during the rise while the
  // resistive feedforward already gives the steady voltage (here ~11 %)
  ok(over < 0.075f, "  ...overshoot under 15 %", over);
  ok(fabsf(iq_of(&c) - 0.5f) < 0.005f && fabsf(id_of(&c)) < 0.005f, "  ...settles: iq 0.5 A, id 0", iq_of(&c));
  ok(fabsf(c.ia - (-0.5f * sinf(c.e))) < 0.005f && fabsf(c.ib - 0.5f * cosf(c.e)) < 0.005f,
     "  ...the coil currents are the rotor-frame current turned by the angle", c.ia);

  // 2. turning at 5 rev/s mechanical (250 rev/s electrical): back-EMF 4.4 V
  bip_reset(&s);
  c = (coils_t){.w = 2.0f * (float)M_PI * 250.0f};
  float err = 0;
  for(int i = 0; i < 2000; i++) {  // 100 ms
    float e_now = c.e;
    bip_step(&s, &P, e_now, c.w, 0, 0.4f, c.ia, c.ib, 1, DT, &o);
    plant(&c, &o);
    if(i > 400) {
      err = fmaxf(err, fabsf(iq_of(&c) - 0.4f));
    }
  }
  ok(err < 0.03f, "turning, 4.4 V back-EMF: iq stays within 30 mA of 0.4 A (back-EMF feedforward)", err);
  P.cur_ind = 0;
  bip_reset(&s);
  c   = (coils_t){.w = 2.0f * (float)M_PI * 250.0f};
  float err0 = 0;
  for(int i = 0; i < 2000; i++) {
    bip_step(&s, &P, c.e, c.w, 0, 0.4f, c.ia, c.ib, 1, DT, &o);
    plant(&c, &o);
    if(i > 400) {
      err0 = fmaxf(err0, fabsf(iq_of(&c) - 0.4f));
    }
  }
  P.cur_ind = 1;
  ok(err0 > err, "  ...without the feedforward, larger error (the integrator alone)", err0);

  // 3. the voltage limit: a command the supply cannot drive
  bip_reset(&s);
  c          = (coils_t){.w = 2.0f * (float)M_PI * 1000.0f};  // 20 rev/s: back-EMF 17.6 V
  float umax = 0, dmax = 0;
  for(int i = 0; i < 400; i++) {
    bip_step(&s, &P, c.e, c.w, 0, 1.0f, c.ia, c.ib, 1, DT, &o);
    plant(&c, &o);
    umax = fmaxf(umax, o.abs_volt);
    dmax = fmaxf(dmax, fmaxf(fabsf(o.duty_a), fabsf(o.duty_b)));
  }
  ok(umax <= 24.0f * 0.95f + 1e-3f && dmax <= 0.95f + 1e-4f,
     "voltage limit: |u| <= 0.95 udc, each duty within +-0.95 (the circle inside the bridges' square)", umax);
  ok(fabsf(s.iq_sum) <= 24.0f * 0.95f + 1e-3f, "  ...the integrator does not wind up past the limit", s.iq_sum);

  // 4. current limit
  bip_reset(&s);
  c = (coils_t){0};
  for(int i = 0; i < 400; i++) {
    bip_step(&s, &P, c.e, 0, 2.0f, 2.0f, c.ia, c.ib, 1, DT, &o);
    plant(&c, &o);
  }
  float amp = sqrtf(c.ia * c.ia + c.ib * c.ib);
  ok(fabsf(amp - 1.0f) < 0.01f, "current command limited to max_cur (1 A) as a vector", amp);

  // 5. voltage mode: d/q are voltages (identification), limited to the circle
  bip_reset(&s);
  P.volt_mode = 1;
  bip_step(&s, &P, 0.3f, 0, 3.0f, 4.0f, 0, 0, 1, DT, &o);
  int vm1 = fabsf(o.ud - 3.0f) < 1e-4f && fabsf(o.uq - 4.0f) < 1e-4f;
  bip_step(&s, &P, 0.3f, 0, 30.0f, 40.0f, 0, 0, 1, DT, &o);
  P.volt_mode = 0;
  ok(vm1 && fabsf(o.abs_volt - 22.8f) < 0.01f, "voltage mode: ud, uq passed through, limited to 0.95 udc", o.abs_volt);

  // 6. disabled: no output, integrators cleared
  bip_reset(&s);
  for(int i = 0; i < 100; i++) {
    bip_step(&s, &P, 0, 0, 0, 0.5f, 0.1f, 0.1f, 1, DT, &o);
  }
  float sum_before = s.iq_sum;
  bip_step(&s, &P, 0, 0, 0, 0.5f, 0.1f, 0.1f, 0, DT, &o);
  ok(sum_before != 0 && o.duty_a == 0 && o.duty_b == 0 && s.iq_sum == 0 && s.uq == 0,
     "disabled: zero duty, integrators and predictor cleared", s.iq_sum);

  // 7. no supply: no division by zero, zero duty
  bip_param_t p0 = P;
  p0.udc         = 0;
  bip_step(&s, &p0, 0, 0, 0, 0.5f, 0, 0, 1, DT, &o);
  ok(o.duty_a == 0 && o.duty_b == 0 && !isnan(o.ud), "no supply voltage: zero duty, no NaN", o.duty_a);

  // 8. bip_sense: the DRV8876's low-side-only IPROPI, two samples a period
  const float W = 0.1f, T = 0.01f;
  ok(fabsf(bip_sense(0.3f, 0.3f, 0.5f, W, 0, T) - 0.3f) < 1e-6f, "IPROPI: driving forward, current forward: +0.3 A", 0);
  ok(fabsf(bip_sense(0.0f, 0.3f, 0.5f, W, 0, T) + 0.3f) < 1e-6f,
     "IPROPI: driving forward, current backward (drive window 0, brake 0.3): -0.3 A", 0);
  ok(fabsf(bip_sense(0.3f, 0.3f, -0.5f, W, 0, T) + 0.3f) < 1e-6f, "IPROPI: driving backward, current backward: -0.3 A", 0);
  ok(fabsf(bip_sense(0.0f, 0.3f, -0.5f, W, 0, T) - 0.3f) < 1e-6f, "IPROPI: driving backward, current forward: +0.3 A", 0);
  ok(fabsf(bip_sense(0.0f, 0.2f, 0.02f, W, -0.1f, T) + 0.2f) < 1e-6f &&
         fabsf(bip_sense(0.0f, 0.2f, 0.02f, W, 0.1f, T) - 0.2f) < 1e-6f,
     "IPROPI: drive window too short (2 %): the magnitude from the brake window, the sign predicted", 0);
  ok(fabsf(bip_sense(0.25f, 0.0f, 0.97f, W, 0, T) - 0.25f) < 1e-6f,
     "IPROPI: brake window too short (97 %): the drive window alone", 0);
  ok(bip_sense(0.004f, 0.006f, 0.5f, W, 0.2f, T) == 0.0f && bip_sense(0.0f, 0.008f, 0.02f, W, 0.2f, T) == 0.0f,
     "IPROPI: below the noise floor: 0 (a rectified |noise| would bias the loop)", 0);

  // 9. offset calibration: a positive offset measured, a negative one reads 0
  bip_cal_t cal;
  bip_cal_start(&cal, 4);
  int busy = 0;
  for(int i = 0; i < 4; i++) {
    busy += bip_cal_sample(&cal, 0.0075f + (i & 1 ? 0.001f : -0.001f), 0.0f);
  }
  ok(busy == 3 && fabsf(cal.off_a - 0.0075f) < 1e-6f && cal.off_b == 0.0f && bip_cal_sample(&cal, 1, 1) == 0,
     "IPROPI offset calibration: the mean of the brake-window samples at zero current", cal.off_a);
  ok(fabsf(bip_cal_apply(0.1075f, 0.0075f) - 0.1f) < 1e-6f && bip_cal_apply(0.005f, 0.0075f) == 0.0f,
     "  ...applied: subtracted, never below 0", 0);

  printf("1..%d\n# %d/%d passed\n", n, n - fails, n);
  return fails ? 1 : 0;
}
