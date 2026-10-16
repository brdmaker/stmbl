/*
 * Two-phase current loop, see bip_loop.h. The controller is the one of the
 * F3's curpid (and host/hw/hv.c's model of it): a one-step predictor, a P
 * term scaled by bandwidth * L, an integrator scaled by bandwidth * R,
 * resistive and back-EMF feedforward, anti-windup against the voltage limit.
 */
#include "bip_loop.h"
#include <math.h>

static float clampf(float x, float lim) {
  return x > lim ? lim : x < -lim ? -lim : x;
}

void bip_reset(bip_state_t *s) {
  s->id_sum = s->iq_sum = 0.0f;
  s->ud = s->uq = 0.0f;
}

void bip_step(bip_state_t *s, const bip_param_t *p, float e, float w, float d_cmd, float q_cmd, float ia, float ib,
              int en, float dt, bip_out_t *o) {
  float se = sinf(e), ce = cosf(e);
  float id = ia * ce + ib * se;
  float iq = -ia * se + ib * ce;
  float udc      = p->udc > 0.0f ? p->udc : 0.0f;
  float max_duty = p->max_duty > 0.0f && p->max_duty <= 1.0f ? p->max_duty : 1.0f;
  float max_volt = udc * max_duty;
  float r        = p->r > 0.01f ? p->r : 0.01f;
  float l        = p->l > 1e-5f ? p->l : 1e-5f;

  // command limit: currents to max_cur, voltages to max_volt
  float lim = p->volt_mode ? max_volt : (p->max_cur > 0.0f ? p->max_cur : 0.0f);
  float a2  = d_cmd * d_cmd + q_cmd * q_cmd;
  if(a2 > lim * lim && a2 > 0.0f) {
    float k = lim / sqrtf(a2);
    d_cmd *= k;
    q_cmd *= k;
  }

  float ud, uq;
  if(!en) {
    ud = uq = 0.0f;
    bip_reset(s);
  } else if(p->volt_mode) {
    ud = d_cmd;
    uq = q_cmd;
    s->id_sum = s->iq_sum = 0.0f;
  } else {
    // predicted currents at the end of the period (the output acts then)
    float indd = w * l * iq;
    float indq = w * (l * id + p->psi);
    float idp  = id + (s->ud - r * id + indd) / l * dt;
    float iqp  = iq + (s->uq - r * iq - indq) / l * dt;
    float ed   = d_cmd - idp;
    float eq   = q_cmd - iqp;
    ud = p->cur_ff * r * d_cmd - p->cur_ind * indd + p->cur_bw * l * ed;
    uq = p->cur_ff * r * q_cmd + p->cur_ind * indq + p->cur_bw * l * eq;
    ud = clampf(ud, max_volt);
    uq = clampf(uq, max_volt);
    if(p->cur_bw * r > 0.0f) {
      s->id_sum = clampf(s->id_sum + p->cur_bw * r * ed * dt, max_volt - fabsf(ud));
      s->iq_sum = clampf(s->iq_sum + p->cur_bw * r * eq * dt, max_volt - fabsf(uq));
    } else {
      s->id_sum = s->iq_sum = 0.0f;
    }
    ud += s->id_sum;
    uq += s->iq_sum;
  }
  // the voltage circle inside the bridges' square; d first (it holds the
  // field / the commutation), q gets what is left
  float u2 = ud * ud + uq * uq;
  if(u2 > max_volt * max_volt) {
    ud        = clampf(ud, max_volt);
    float qm  = sqrtf(max_volt * max_volt - ud * ud);
    uq        = clampf(uq, qm);
  }
  s->ud = ud;
  s->uq = uq;

  o->id       = id;
  o->iq       = iq;
  o->ud       = ud;
  o->uq       = uq;
  o->ua       = ud * ce - uq * se;
  o->ub       = ud * se + uq * ce;
  o->duty_a   = udc > 0.0f ? clampf(o->ua / udc, 1.0f) : 0.0f;
  o->duty_b   = udc > 0.0f ? clampf(o->ub / udc, 1.0f) : 0.0f;
  o->abs_cur  = sqrtf(id * id + iq * iq);
  o->abs_volt = sqrtf(ud * ud + uq * uq);
}

float bip_sense(float drive, float brake, float duty, float min_window, float prev, float thr) {
  float dir      = duty >= 0.0f ? 1.0f : -1.0f;
  int drive_ok   = fabsf(duty) >= min_window;
  int brake_ok   = 1.0f - fabsf(duty) >= min_window;
  float mag      = brake_ok ? brake : drive_ok ? drive : fabsf(prev);
  if(drive_ok) {
    if(drive > thr) {
      return dir * (brake_ok ? 0.5f * (drive + brake) : drive);  // with the drive
    }
    if(mag > thr) {
      return -dir * mag;  // against it: the drive window saw nothing
    }
    return 0.0f;  // below the noise floor either way
  }
  // the drive window was too short to sample: the sign predicted
  if(mag <= thr) {
    return 0.0f;  // only noise: |noise| is never negative, it would bias the loop
  }
  return (prev >= 0.0f ? 1.0f : -1.0f) * mag;
}

void bip_ref(float e, float d_cmd, float q_cmd, float *ia, float *ib) {
  float se = sinf(e), ce = cosf(e);
  *ia = d_cmd * ce - q_cmd * se;
  *ib = d_cmd * se + q_cmd * ce;
}

void bip_cal_start(bip_cal_t *c, uint32_t n) {
  c->n = c->left = n;
  c->sum_a = c->sum_b = 0.0f;
}

int bip_cal_sample(bip_cal_t *c, float brake_a, float brake_b) {
  if(c->left == 0) {
    return 0;
  }
  c->sum_a += brake_a;
  c->sum_b += brake_b;
  if(--c->left == 0) {
    c->off_a = c->sum_a / c->n;
    c->off_b = c->sum_b / c->n;
    return 0;
  }
  return 1;
}
