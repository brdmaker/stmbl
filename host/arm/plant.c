/*
 * Simulated arm, see plant.h.
 */
#include "plant.h"
#include <math.h>
#include <string.h>

void plant_defaults(arm_plant *p, int n) {
  memset(p, 0, sizeof(*p));
  p->arm.n = n;
  p->arm.g[2] = -9.81;
  for(int i = 0; i < n; i++) {
    p->K[i][i] = 1.0;
    p->k[i] = 1e5;
    p->c[i] = 50.0;
    p->jm[i] = 1e-4;
  }
  p->kstop = 1e5;
  p->cstop = 500.0;
}

// Solve A x = b (n x n, partial pivoting); 0 or -1 if singular.
static int solve(int n, const double A[RB_MAXJ][RB_MAXJ], const double *b, double *x) {
  double M[RB_MAXJ][RB_MAXJ + 1];
  for(int i = 0; i < n; i++) {
    memcpy(M[i], A[i], sizeof(double) * n);
    M[i][n] = b[i];
  }
  for(int c = 0; c < n; c++) {
    int pr = c;
    for(int r = c + 1; r < n; r++) {
      if(fabs(M[r][c]) > fabs(M[pr][c])) {
        pr = r;
      }
    }
    if(fabs(M[pr][c]) < 1e-15) {
      return -1;
    }
    for(int j = 0; j <= n; j++) {
      double t = M[c][j];
      M[c][j] = M[pr][j];
      M[pr][j] = t;
    }
    for(int r = c + 1; r < n; r++) {
      double f = M[r][c] / M[c][c];
      for(int j = c; j <= n; j++) {
        M[r][j] -= f * M[c][j];
      }
    }
  }
  for(int i = n - 1; i >= 0; i--) {
    double t = M[i][n];
    for(int j = i + 1; j < n; j++) {
      t -= M[i][j] * x[j];
    }
    x[i] = t / M[i][i];
  }
  return 0;
}

void plant_set_q(arm_plant *p, const double *q) {
  int n = p->arm.n;
  double g[RB_MAXJ], qm[RB_MAXJ];
  rb_gravity(&p->arm, q, g);
  for(int i = 0; i < n; i++) {  // gear deflection that carries gravity
    p->q[i] = q[i];
    p->qd[i] = 0.0;
    double e = p->k[i] > 0.0 ? g[i] / p->k[i] : 0.0;
    qm[i] = q[i] + e + (g[i] >= 0.0 ? 0.5 : -0.5) * p->lash[i];
    p->w[i] = 0.0;
  }
  solve(n, p->K, qm, p->th);
}

static double fric_mag(const rb_fric *f, double v) {  // |friction| at speed |v|
  v = fabs(v);
  double st = f->vs > 0.0 ? (f->fs - f->fc) * exp(-(v / f->vs) * (v / f->vs)) : 0.0;
  return f->fc + st + f->fv * v;
}

void plant_step(arm_plant *p, const double *te, const int *released, double dt) {
  int n = p->arm.n;
  double qm[RB_MAXJ], qmd[RB_MAXJ];

  // gears
  for(int i = 0; i < n; i++) {
    qm[i] = qmd[i] = 0.0;
    for(int j = 0; j < n; j++) {
      qm[i] += p->K[i][j] * p->th[j];
      qmd[i] += p->K[i][j] * p->w[j];
    }
    double d = qm[i] - p->q[i], h = 0.5 * p->lash[i];
    double e = d > h ? d - h : (d < -h ? d + h : 0.0);
    p->tau_g[i] = e != 0.0 ? p->k[i] * e + p->c[i] * (qmd[i] - p->qd[i]) : 0.0;
  }

  // rotors: te - K^T tau_g; bearing friction and brake stop, never reverse
  for(int j = 0; j < n; j++) {
    double t = te[j];
    for(int i = 0; i < n; i++) {
      t -= p->K[i][j] * p->tau_g[i];
    }
    double wp = p->w[j] + dt * t / p->jm[j];
    double fr = p->fm[j] + (released && released[j] ? 0.0 : p->brake[j]);
    double dw = dt * fr / p->jm[j];
    p->w[j] = fabs(wp) <= dw ? 0.0 : wp - (wp > 0.0 ? dw : -dw);
    p->th[j] += dt * p->w[j];
  }

  // links
  double tau[RB_MAXJ], h[RB_MAXJ], b[RB_MAXJ], qdd[RB_MAXJ], z[RB_MAXJ] = {0};
  for(int i = 0; i < n; i++) {
    tau[i] = p->tau_g[i];
    int out = 0;
    if(p->qmax[i] > p->qmin[i]) {
      if(p->q[i] > p->qmax[i]) {
        tau[i] += p->kstop * (p->qmax[i] - p->q[i]) - p->cstop * (p->qd[i] > 0.0 ? p->qd[i] : 0.0);
        out = 1;
      } else if(p->q[i] < p->qmin[i]) {
        tau[i] += p->kstop * (p->qmin[i] - p->q[i]) - p->cstop * (p->qd[i] < 0.0 ? p->qd[i] : 0.0);
        out = 1;
      }
    }
    if(out && !p->at_stop[i]) {
      p->stop_hits[i]++;
    }
    p->at_stop[i] = out;
  }
  double M[RB_MAXJ][RB_MAXJ];
  rb_mass(&p->arm, p->q, M);
  rb_rnea(&p->arm, p->q, p->qd, z, 1, &p->fe, p->fe_on ? 1 : 0, h);
  rb_chol(n, M);
  for(int i = 0; i < n; i++) {
    b[i] = tau[i] - h[i];
  }
  rb_chol_solve(n, M, b, qdd);

  // friction on the predicted speed: sticks when it can stop the joint
  // within the step (diagonal of M^-1), else slides
  double qdp[RB_MAXJ], tf[RB_MAXJ], dq[RB_MAXJ];
  for(int i = 0; i < n; i++) {
    qdp[i] = p->qd[i] + dt * qdd[i];
  }
  for(int i = 0; i < n; i++) {
    double e[RB_MAXJ] = {0}, col[RB_MAXJ];
    e[i] = 1.0;
    rb_chol_solve(n, M, e, col);
    double minv = col[i];
    double fs = fric_mag(&p->fric[i], 0.0);
    if(fabs(qdp[i]) <= dt * minv * fs) {
      tf[i] = qdp[i] / (dt * minv);
    } else {
      tf[i] = (qdp[i] > 0.0 ? 1.0 : -1.0) * fric_mag(&p->fric[i], qdp[i]);
    }
    p->tau_f[i] = tf[i];
  }
  rb_chol_solve(n, M, tf, dq);
  for(int i = 0; i < n; i++) {
    p->qd[i] = qdp[i] - dt * dq[i];
    p->q[i] += dt * p->qd[i];
  }
}

double plant_energy(const arm_plant *p) {
  int n = p->arm.n;
  double kin, pot, e = 0.0;
  rb_energy(&p->arm, p->q, p->qd, &kin, &pot);
  e = kin + pot;
  for(int j = 0; j < n; j++) {
    e += 0.5 * p->jm[j] * p->w[j] * p->w[j];
  }
  for(int i = 0; i < n; i++) {
    double qm = 0.0;
    for(int j = 0; j < n; j++) {
      qm += p->K[i][j] * p->th[j];
    }
    double d = qm - p->q[i], hl = 0.5 * p->lash[i];
    double s = d > hl ? d - hl : (d < -hl ? d + hl : 0.0);
    e += 0.5 * p->k[i] * s * s;
  }
  return e;
}
