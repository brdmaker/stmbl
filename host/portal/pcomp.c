/*
 * pcomp: physics-based command conditioning, see pcomp.h.
 */
#include "pcomp.h"

#include <math.h>
#include <string.h>

double pc_fy(const pc_model *m, double X) {
  // the plant's beam: compliance 1/k quadratic along the bridge, k ~ w^2
  double u = 2.0 * (X - m->x0) / 1000.0 / m->L;
  double ce = 1.0 / (m->fy_end * m->fy_end), cm = 1.0 / (m->fy_mid * m->fy_mid);
  return sqrt(1.0 / (ce + (cm - ce) * (1.0 - u * u)));
}

void pc_init(pc_t *c, const pc_model *m, double dt) {
  memset(c, 0, sizeof(*c));
  c->m = *m;
  c->dt = dt;
}

// ZV shaper for a mode f [Hz]: amplitudes a1 (now), a2 (after delay [s])
static void zv(double f, double zeta, double *a1, double *a2, double *delay) {
  double wd = 2.0 * M_PI * f * sqrt(1.0 - zeta * zeta);
  double K = exp(-zeta * M_PI / sqrt(1.0 - zeta * zeta));
  *a1 = 1.0 / (1.0 + K), *a2 = K / (1.0 + K);
  *delay = M_PI / wd;
}

// ring r at `back` periods ago (fractional: linear between samples)
static double past(const double r[PC_HIST][2], int head, int n, int k, double back) {
  if(back > n - 1) {
    back = n - 1;
  }
  int i = (int)floor(back);
  double f = back - i;
  const double *a = r[(head - i + PC_HIST) % PC_HIST], *b = r[(head - i - 1 + 2 * PC_HIST) % PC_HIST];
  return i + 1 > n - 1 ? a[k] : a[k] * (1.0 - f) + b[k] * f;
}

void pc_step(pc_t *c, const double xy[2]) {
  c->head = (c->head + 1) % PC_HIST;
  memcpy(c->in[c->head], xy, sizeof(c->in[0]));
  c->n = c->n < PC_HIST ? c->n + 1 : PC_HIST;
  if(!c->primed) {  // a history standing still at the first command
    for(int i = 0; i < PC_HIST; i++) {
      memcpy(c->in[i], xy, sizeof(c->in[0]));
      memcpy(c->sh[i], xy, sizeof(c->sh[0]));
      memcpy(c->zv[i], xy, sizeof(c->zv[0]));
    }
    c->n = PC_HIST;
    c->primed = 1;
  }
  double s[2], s2 = 0.0;  // the shaper's delay variance [s^2]
  c->delay = 0.0;
  if(c->shaper) {  // x mode, then the y mode at the carriage's position, both on both axes
    double a1, a2, d1, b1, b2, d2;
    zv(c->m.fx, c->m.zeta, &a1, &a2, &d1);
    zv(pc_fy(&c->m, xy[0]), c->m.zeta, &b1, &b2, &d2);
    s2 = a1 * a2 * d1 * d1 + b1 * b2 * d2 * d2;
    c->delay = a2 * d1 + b2 * d2;
    for(int k = 0; k < 2; k++) {
      // stage 1 at now and at d2 ago, from the input history
      double s1_now = a1 * xy[k] + a2 * past(c->in, c->head, c->n, k, d1 / c->dt);
      double s1_d2 = a1 * past(c->in, c->head, c->n, k, d2 / c->dt) + a2 * past(c->in, c->head, c->n, k, (d1 + d2) / c->dt);
      s[k] = b1 * s1_now + b2 * s1_d2;
    }
  } else {
    s[0] = xy[0], s[1] = xy[1];
  }
  memcpy(c->zv[c->head], s, sizeof(s));
  int nj = c->jerk > 1 ? (c->jerk < PC_HIST ? c->jerk : PC_HIST - 1) : 1;
  if(nj > 1) {  // moving average over the last nj
    for(int k = 0; k < 2; k++) {
      double sum = 0.0;
      for(int i = 0; i < nj; i++) {
        sum += c->zv[(c->head - i + PC_HIST) % PC_HIST][k];
      }
      s[k] = sum / nj;
    }
    s2 += (nj * nj - 1) / 12.0 * c->dt * c->dt;
    c->delay += (nj - 1) / 2.0 * c->dt;
  }
  memcpy(c->sh[c->head], s, sizeof(s));
  double o[2];
  if(c->defl || c->servo || (c->shrink && s2 > 0.0)) {  // leads: acceleration of the shaped command, one period back
    const double *s0 = c->sh[c->head], *s1 = c->sh[(c->head - 1 + PC_HIST) % PC_HIST],
                 *sb = c->sh[(c->head - 2 + PC_HIST) % PC_HIST];
    double w[2] = {2.0 * M_PI * c->m.fx, 2.0 * M_PI * pc_fy(&c->m, s1[0])};
    for(int k = 0; k < 2; k++) {
      double a = (s0[k] - 2.0 * s1[k] + sb[k]) / (c->dt * c->dt);  // mm/s^2
      o[k] = s1[k] + a * ((c->defl ? 1.0 / (w[k] * w[k]) : 0.0) - (c->servo ? c->m.servo : 0.0) - (c->shrink ? 0.5 * s2 : 0.0));
      c->delay += k == 0 ? c->dt : 0.0;  // one period back
    }
  } else {
    o[0] = s[0], o[1] = s[1];
  }
  for(int k = 0; k < 2; k++) {
    c->vout[k] = c->primed > 1 ? (o[k] - c->last[k]) / c->dt : 0.0;
    c->out[k] = c->last[k] = o[k];
  }
  c->primed = 2;
}
