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
  gm_init(&p->geo);
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
  // contact with the obstacles: penalty spring + damper along the normal
  rb_force fe[1 + 8];
  int nf = 0;
  if(p->fe_on) {
    fe[nf++] = p->fe;
  }
  p->contact_f = 0.0;
  if(p->geo.no > 0 && p->geo.nb > 0) {
    gm_pair cp[8];
    double R[RB_MAXJ][3][3], pl[RB_MAXJ][3];
    gm_update(&p->geo, &p->arm, p->q);
    int nc = gm_contacts(&p->geo, cp, 8);
    if(nc) {
      rb_fk(&p->arm, p->q, R, pl);
    }
    static int was[GM_MAXBODY][GM_MAXOBS];
    int now[GM_MAXBODY][GM_MAXOBS] = {{0}};
    for(int k = 0; k < nc; k++) {
      int b = cp[k].body, o = cp[k].other, L = p->geo.body[b].link;
      double dd = was[b][o] ? (cp[k].d - p->prev_d[b][o]) / dt : 0.0;
      double f = -p->geo.k * cp[k].d - p->geo.c * dd;
      p->prev_d[b][o] = cp[k].d;
      now[b][o] = 1;
      if(-cp[k].d > p->pen_max) {
        p->pen_max = -cp[k].d;
      }
      if(f <= 0.0 || L < 1) {
        continue;
      }
      rb_force *F = &fe[nf++];
      F->link = L - 1;
      for(int i = 0; i < 3; i++) {
        F->f[i] = f * cp[k].n[i];
      }
      for(int i = 0; i < 3; i++) {  // the contact point in the link frame
        F->r[i] = R[L - 1][0][i] * (cp[k].pa[0] - pl[L - 1][0]) + R[L - 1][1][i] * (cp[k].pa[1] - pl[L - 1][1]) +
                  R[L - 1][2][i] * (cp[k].pa[2] - pl[L - 1][2]);
      }
      p->contact_f += f;
    }
    memcpy(was, now, sizeof(was));
    if(p->contact_f > p->contact_max) {
      p->contact_max = p->contact_f;
    }
  }
  double M[RB_MAXJ][RB_MAXJ];
  rb_mass(&p->arm, p->q, M);
  rb_rnea(&p->arm, p->q, p->qd, z, 1, fe, nf, h);
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
