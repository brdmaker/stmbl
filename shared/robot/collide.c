/*
 * Collision detection, see collide.h.
 */
#include "collide.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define D2R (M_PI / 180.0)

int rb_collide_line(rb_collide *c, const char *line) {
  char kw[16];
  double v[12];
  int n = 0, off = 0;
  if(sscanf(line, " %15s%n", kw, &off) != 1 || kw[0] == '#') {
    return 1;
  }
  const char *p = line + off;
  char *end;
  if(!strcmp(kw, "envelope")) {
    double t;
    if(sscanf(p, " %lf", &t) != 1 || t <= 0.0) {
      return 0;
    }
    c->envelope = t;
    return 1;
  }
  if(!strcmp(kw, "observer")) {
    double k, h, e;
    if(sscanf(p, " %lf %lf %lf", &k, &h, &e) != 3) {
      return 0;
    }
    c->obs.k = k;
    c->holdoff = h;
    for(int j = 0; j < RB_MAXJ; j++) {
      c->fric[j].eps = e;
    }
    return 1;
  }
  if(!strcmp(kw, "payload")) {  // payload M cx cy cz on the last link (M 0: none)
    double v4[4] = {0};
    int k = sscanf(p, " %lf %lf %lf %lf", v4, v4 + 1, v4 + 2, v4 + 3);
    return k >= 1 && rb_collide_payload(c, v4[0], v4 + 1) == 0;
  }
  if(!strcmp(kw, "drive") || !strcmp(kw, "gear") || !strcmp(kw, "limits") || !strcmp(kw, "home")) {
    return 1;  // the plant's, not the model's
  }
  int j = (int)strtol(p, &end, 10) - 1;
  for(p = end; n < 12; p = end) {
    double x = strtod(p, &end);
    if(end == p) {
      break;
    }
    v[n++] = x;
  }
  if(j < 0 || j >= RB_MAXJ) {
    return 0;
  }
  if(j + 1 > c->mdl.n) {
    c->mdl.n = j + 1;
  }
  rb_link *l = &c->mdl.link[j];
  if(!strcmp(kw, "dh") && n == 4) {
    l->a = v[0], l->alpha = v[1] * D2R, l->d = v[2], l->theta0 = v[3] * D2R;
  } else if(!strcmp(kw, "link") && n == 10) {
    rb_link_from_com(l, v[0], v + 1, v + 4);
  } else if(!strcmp(kw, "motor") && n >= 2) {
    c->K[j][j] = 1.0 / v[0];
    c->jm[j] = v[1];
  } else if(!strcmp(kw, "couple") && n == 2 && v[0] >= 1 && v[0] <= RB_MAXJ) {
    c->K[j][(int)v[0] - 1] = v[1];
  } else if(!strcmp(kw, "fric") && n == 4) {
    double eps = c->fric[j].eps;
    c->fric[j] = (rb_fric){.fc = v[0], .fs = v[1], .vs = v[2], .fv = v[3], .eps = eps};
  } else if(!strcmp(kw, "threl") && n == 1) {
    c->thr_rel[j] = v[0];
  } else if(!strcmp(kw, "thrrot") && n == 1) {
    c->thr_rot[j] = v[0];
  } else if(!strcmp(kw, "kt") && n == 1) {
    c->kt[j] = v[0];
  } else if(!strcmp(kw, "thresh") && (n == 2 || n == 4)) {
    c->thr0[j] = v[0], c->thr_v[j] = v[1];
    c->thr_s[j] = n == 4 ? v[2] : 0.0, c->thr_vs[j] = n == 4 ? v[3] : 0.0;
  } else {
    return 0;
  }
  return 1;
}

int rb_collide_payload(rb_collide *c, double m, const double cm[3]) {
  double ic[6] = {0};
  int last = c->mdl.n - 1;
  if(last < 0) {
    return -1;
  }
  if(c->pay_m != 0.0) {
    rb_add_payload(&c->mdl, last, -c->pay_m, c->pay_c, ic);
  }
  c->pay_m = m;
  memcpy(c->pay_c, cm, sizeof(c->pay_c));
  if(c->pay_m != 0.0) {
    rb_add_payload(&c->mdl, last, c->pay_m, c->pay_c, ic);
  }
  return 0;
}

void rb_collide_setup(rb_collide *c) {
  int n = c->mdl.n;
  rb_armature(&c->mdl, c->K, c->jm);
  // T = K^-T diag(kt): from motor torques to joint torques (virtual work)
  double A[RB_MAXJ][RB_MAXJ];
  double M[RB_MAXJ][2 * RB_MAXJ];
  for(int i = 0; i < n; i++) {
    for(int k = 0; k < n; k++) {
      M[i][k] = c->K[i][k];
      M[i][n + k] = i == k;
    }
  }
  for(int col = 0; col < n; col++) {
    int pr = col;
    for(int r = col + 1; r < n; r++) {
      if(fabs(M[r][col]) > fabs(M[pr][col])) {
        pr = r;
      }
    }
    for(int k = 0; k < 2 * n; k++) {
      double t = M[col][k];
      M[col][k] = M[pr][k];
      M[pr][k] = t;
    }
    double d = M[col][col];
    if(fabs(d) < 1e-15) {
      return;
    }
    for(int k = 0; k < 2 * n; k++) {
      M[col][k] /= d;
    }
    for(int r = 0; r < n; r++) {
      if(r != col) {
        double f = M[r][col];
        for(int k = 0; k < 2 * n; k++) {
          M[r][k] -= f * M[col][k];
        }
      }
    }
  }
  for(int i = 0; i < n; i++) {
    for(int k = 0; k < n; k++) {
      A[i][k] = M[i][n + k];  // K^-1
    }
  }
  for(int i = 0; i < n; i++) {
    for(int k = 0; k < n; k++) {
      c->T[i][k] = A[k][i] * c->kt[k];  // (K^-1)^T diag(kt)
    }
  }
}

int rb_collide_load(rb_collide *c, const char *path, char *err, int errlen) {
  memset(c, 0, sizeof(*c));
  c->mdl.g[2] = -9.81;
  c->obs.k = 50.0;
  c->holdoff = 0.3;
  c->envelope = 0.15;
  for(int j = 0; j < RB_MAXJ; j++) {
    c->K[j][j] = 1.0;
    c->fric[j].eps = 0.01;
  }
  FILE *f = fopen(path, "r");
  if(!f) {
    snprintf(err, errlen, "cannot read %s", path);
    return -1;
  }
  char line[512];
  int ln = 0;
  while(fgets(line, sizeof(line), f)) {
    ln++;
    char *h = strchr(line, '#');
    if(h) {
      *h = 0;
    }
    if(!rb_collide_line(c, line)) {
      snprintf(err, errlen, "%s:%d: bad line", path, ln);
      fclose(f);
      return -1;
    }
  }
  fclose(f);
  for(int j = 0; j < c->mdl.n; j++) {
    if(c->kt[j] <= 0.0) {
      snprintf(err, errlen, "%s: no kt for joint %d", path, j + 1);
      return -1;
    }
  }
  rb_collide_setup(c);
  return 0;
}

int rb_collide_step(rb_collide *c, int enabled, const double *q, const double *qd,
                    const double *cur, double dt) {
  int n = c->mdl.n;
  if(!enabled) {
    rb_observer_reset(&c->obs);
    c->t_en = 0.0;
    c->armed = 0;
    c->hit = 0;
    c->joint = 0;
    memset(c->r, 0, sizeof(c->r));
    memset(c->dyn, 0, sizeof(c->dyn));
    memset(c->rot, 0, sizeof(c->rot));
    memset(c->qdd_f, 0, sizeof(c->qdd_f));
    return 0;
  }
  double tm[RB_MAXJ] = {0};
  for(int i = 0; i < n; i++) {
    double t = 0.0;
    for(int k = 0; k < n; k++) {
      t += c->T[i][k] * cur[k];
    }
    c->tau[i] = t;
    c->tau_f[i] = rb_friction(&c->fric[i], qd[i]);
    tm[i] = t - c->tau_f[i];
  }
  rb_observer_step(&c->obs, &c->mdl, q, qd, tm, dt);
  for(int i = 0; i < n; i++) {  // acceleration from the speed feedback, 5 ms filter
    double af = dt / (dt + 0.005);
    if(c->t_en > 0.0) {
      c->qdd_f[i] += af * ((qd[i] - c->qd_prev[i]) / dt - c->qdd_f[i]);
    }
    c->qd_prev[i] = qd[i];
  }
  double g[RB_MAXJ], M[RB_MAXJ][RB_MAXJ], mq[RB_MAXJ];
  rb_gravity(&c->mdl, q, g);
  rb_mass(&c->mdl, q, M);
  for(int i = 0; i < n; i++) {  // inertial torque of the (filtered) accelerations
    mq[i] = 0.0;
    for(int k = 0; k < n; k++) {
      mq[i] += M[i][k] * c->qdd_f[k];
    }
  }
  c->t_en += dt;
  c->armed = c->t_en >= c->holdoff;
  for(int i = 0; i < n; i++) {
    c->r[i] = c->obs.r[i];
    double sv = c->thr_vs[i] > 0.0 ? qd[i] / c->thr_vs[i] : 0.0;
    double dy = fabs(c->tau[i] - g[i]), dr = c->mdl.arm[i][i] * fabs(c->qdd_f[i]);
    dy = fabs(mq[i]) > dy ? fabs(mq[i]) : dy;
    double decay = exp(-dt / c->envelope);
    c->dyn[i] = dy > c->dyn[i] * decay ? dy : c->dyn[i] * decay;
    c->rot[i] = dr > c->rot[i] * decay ? dr : c->rot[i] * decay;
    c->thr[i] = c->thr0[i] + c->thr_v[i] * fabs(qd[i]) + c->thr_s[i] * exp(-sv * sv) +
                c->thr_rel[i] * c->dyn[i] + c->thr_rot[i] * c->rot[i];
    if(c->armed && !c->mute && !c->hit && c->thr0[i] > 0.0 && fabs(c->r[i]) > c->thr[i]) {
      c->hit = 1;
      c->joint = i + 1;
    }
  }
  return c->hit;
}
