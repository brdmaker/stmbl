/*
 * Rigid-body dynamics of a serial arm, see robot.h.
 *
 * rb_rnea is Craig's recursive Newton-Euler, written with the first moment
 * h = m c and the inertia about the link origin so that the torques are
 * linear in the parameters (the regressor reuses it). Base acceleration is
 * -g, which adds gravity to every link.
 */
#include "robot.h"
#include <math.h>
#include <string.h>

static void cross(const double a[3], const double b[3], double o[3]) {
  double t0 = a[1] * b[2] - a[2] * b[1];
  double t1 = a[2] * b[0] - a[0] * b[2];
  double t2 = a[0] * b[1] - a[1] * b[0];
  o[0] = t0, o[1] = t1, o[2] = t2;
}

static void mv(const double R[3][3], const double v[3], double o[3]) {  // R v
  double t[3];
  for(int i = 0; i < 3; i++) {
    t[i] = R[i][0] * v[0] + R[i][1] * v[1] + R[i][2] * v[2];
  }
  memcpy(o, t, sizeof(t));
}

static void mtv(const double R[3][3], const double v[3], double o[3]) {  // R^T v
  double t[3];
  for(int i = 0; i < 3; i++) {
    t[i] = R[0][i] * v[0] + R[1][i] * v[1] + R[2][i] * v[2];
  }
  memcpy(o, t, sizeof(t));
}

static void iv(const double I[6], const double v[3], double o[3]) {  // I v
  double t[3];
  t[0] = I[0] * v[0] + I[3] * v[1] + I[4] * v[2];
  t[1] = I[3] * v[0] + I[1] * v[1] + I[5] * v[2];
  t[2] = I[4] * v[0] + I[5] * v[1] + I[2] * v[2];
  memcpy(o, t, sizeof(t));
}

// Rotation (link i in link i-1) and origin of link i in link i-1.
static void dh(const rb_link *l, double q, double R[3][3], double p[3]) {
  double th = q + l->theta0;
  double ct = cos(th), st = sin(th), ca = cos(l->alpha), sa = sin(l->alpha);
  R[0][0] = ct, R[0][1] = -st, R[0][2] = 0.0;
  R[1][0] = st * ca, R[1][1] = ct * ca, R[1][2] = -sa;
  R[2][0] = st * sa, R[2][1] = ct * sa, R[2][2] = ca;
  p[0] = l->a, p[1] = -sa * l->d, p[2] = ca * l->d;
}

void rb_link_from_com(rb_link *l, double m, const double c[3], const double Ic[6]) {
  double cc = c[0] * c[0] + c[1] * c[1] + c[2] * c[2];
  l->m = m;
  for(int i = 0; i < 3; i++) {
    l->h[i] = m * c[i];
  }
  // parallel axis: I_o = I_c + m (|c|^2 E - c c^T)
  l->I[0] = Ic[0] + m * (cc - c[0] * c[0]);
  l->I[1] = Ic[1] + m * (cc - c[1] * c[1]);
  l->I[2] = Ic[2] + m * (cc - c[2] * c[2]);
  l->I[3] = Ic[3] - m * c[0] * c[1];
  l->I[4] = Ic[4] - m * c[0] * c[2];
  l->I[5] = Ic[5] - m * c[1] * c[2];
}

void rb_fk(const rb_model *mdl, const double *q, double R[][3][3], double p[][3]) {
  double Rp[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}, pp[3] = {0, 0, 0};
  for(int i = 0; i < mdl->n; i++) {
    double Ri[3][3], pi[3];
    dh(&mdl->link[i], q[i], Ri, pi);
    for(int r = 0; r < 3; r++) {
      for(int c = 0; c < 3; c++) {
        R[i][r][c] = Rp[r][0] * Ri[0][c] + Rp[r][1] * Ri[1][c] + Rp[r][2] * Ri[2][c];
      }
    }
    mv(Rp, pi, p[i]);
    for(int k = 0; k < 3; k++) {
      p[i][k] += pp[k];
    }
    memcpy(Rp, R[i], sizeof(Rp));
    memcpy(pp, p[i], sizeof(pp));
  }
}

void rb_point(const rb_model *mdl, const double *q, int k, const double r[3], double out[3]) {
  double R[RB_MAXJ][3][3], p[RB_MAXJ][3];
  rb_fk(mdl, q, R, p);
  mv(R[k], r, out);
  for(int i = 0; i < 3; i++) {
    out[i] += p[k][i];
  }
}

void rb_rnea(const rb_model *mdl, const double *q, const double *qd, const double *qdd,
             int gravity, const rb_force *fe, int nf, double *tau) {
  int n = mdl->n;
  double R[RB_MAXJ][3][3], P[RB_MAXJ][3];  // link i in link i-1
  double R0[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
  double R0i[RB_MAXJ][3][3];               // link i in the base (for fe)
  double F[RB_MAXJ][3], N[RB_MAXJ][3];
  double w[3] = {0, 0, 0}, wd[3] = {0, 0, 0}, vd[3] = {0, 0, 0};
  if(gravity) {
    for(int k = 0; k < 3; k++) {
      vd[k] = -mdl->g[k];
    }
  }

  for(int i = 0; i < n; i++) {
    const rb_link *l = &mdl->link[i];
    dh(l, q[i], R[i], P[i]);
    double t[3], t2[3], wn[3], wdn[3], vdn[3];
    // vd_i = R^T (wd x p + w x (w x p) + vd)
    cross(wd, P[i], t);
    cross(w, P[i], t2);
    cross(w, t2, t2);
    for(int k = 0; k < 3; k++) {
      t[k] += t2[k] + vd[k];
    }
    mtv(R[i], t, vdn);
    // w_i = R^T w + qd z;  wd_i = R^T wd + (R^T w) x qd z + qdd z
    mtv(R[i], w, wn);
    mtv(R[i], wd, wdn);
    double qz[3] = {0, 0, qd[i]};
    cross(wn, qz, t);
    for(int k = 0; k < 3; k++) {
      wdn[k] += t[k];
    }
    wdn[2] += qdd[i];
    wn[2] += qd[i];
    memcpy(w, wn, sizeof(w));
    memcpy(wd, wdn, sizeof(wd));
    memcpy(vd, vdn, sizeof(vd));

    // F = m vd + wd x h + w x (w x h);  N = I wd + w x I w + h x vd
    cross(wd, l->h, t);
    cross(w, l->h, t2);
    cross(w, t2, t2);
    for(int k = 0; k < 3; k++) {
      F[i][k] = l->m * vd[k] + t[k] + t2[k];
    }
    iv(l->I, wd, N[i]);
    iv(l->I, w, t);
    cross(w, t, t);
    cross(l->h, vd, t2);
    for(int k = 0; k < 3; k++) {
      N[i][k] += t[k] + t2[k];
    }

    for(int r = 0; r < 3; r++) {
      for(int c = 0; c < 3; c++) {
        R0i[i][r][c] = R0[r][0] * R[i][0][c] + R0[r][1] * R[i][1][c] + R0[r][2] * R[i][2][c];
      }
    }
    memcpy(R0, R0i[i], sizeof(R0));
  }

  for(int j = 0; j < nf; j++) {  // external forces: the joints need less
    int k = fe[j].link;
    if(k < 0 || k >= n) {
      continue;
    }
    double f[3], m[3];
    mtv(R0i[k], fe[j].f, f);
    cross(fe[j].r, f, m);
    for(int c = 0; c < 3; c++) {
      F[k][c] -= f[c];
      N[k][c] -= m[c];
    }
  }

  double f[3] = {0, 0, 0}, nn[3] = {0, 0, 0};
  for(int i = n - 1; i >= 0; i--) {
    double fc[3], nc[3], t[3];
    if(i < n - 1) {  // child forces into this frame
      mv(R[i + 1], f, fc);
      mv(R[i + 1], nn, nc);
      cross(P[i + 1], fc, t);
    } else {
      fc[0] = fc[1] = fc[2] = 0.0;
      nc[0] = nc[1] = nc[2] = 0.0;
      t[0] = t[1] = t[2] = 0.0;
    }
    for(int k = 0; k < 3; k++) {
      f[k] = F[i][k] + fc[k];
      nn[k] = N[i][k] + nc[k] + t[k];
    }
    tau[i] = nn[2];
  }
  for(int i = 0; i < n; i++) {
    for(int j = 0; j < n; j++) {
      tau[i] += mdl->arm[i][j] * qdd[j];
    }
  }
}

void rb_mass(const rb_model *mdl, const double *q, double M[RB_MAXJ][RB_MAXJ]) {
  double z[RB_MAXJ] = {0}, e[RB_MAXJ], t[RB_MAXJ];
  for(int j = 0; j < mdl->n; j++) {
    memset(e, 0, sizeof(e));
    e[j] = 1.0;
    rb_rnea(mdl, q, z, e, 0, 0, 0, t);
    for(int i = 0; i < mdl->n; i++) {
      M[i][j] = t[i];
    }
  }
  for(int i = 0; i < mdl->n; i++) {  // exact symmetry
    for(int j = i + 1; j < mdl->n; j++) {
      M[i][j] = M[j][i] = 0.5 * (M[i][j] + M[j][i]);
    }
  }
}

void rb_gravity(const rb_model *mdl, const double *q, double *tau) {
  double z[RB_MAXJ] = {0};
  rb_rnea(mdl, q, z, z, 1, 0, 0, tau);
}

void rb_coriolis(const rb_model *mdl, const double *q, const double *qd, double *tau) {
  double z[RB_MAXJ] = {0};
  rb_rnea(mdl, q, qd, z, 0, 0, 0, tau);
}

int rb_chol(int n, double A[RB_MAXJ][RB_MAXJ]) {
  for(int j = 0; j < n; j++) {
    double s = A[j][j];
    for(int k = 0; k < j; k++) {
      s -= A[j][k] * A[j][k];
    }
    if(s <= 0.0) {
      return -1;
    }
    A[j][j] = sqrt(s);
    for(int i = j + 1; i < n; i++) {
      double t = A[i][j];
      for(int k = 0; k < j; k++) {
        t -= A[i][k] * A[j][k];
      }
      A[i][j] = t / A[j][j];
    }
  }
  return 0;
}

void rb_chol_solve(int n, const double L[RB_MAXJ][RB_MAXJ], const double *b, double *x) {
  double y[RB_MAXJ];
  for(int i = 0; i < n; i++) {
    double t = b[i];
    for(int k = 0; k < i; k++) {
      t -= L[i][k] * y[k];
    }
    y[i] = t / L[i][i];
  }
  for(int i = n - 1; i >= 0; i--) {
    double t = y[i];
    for(int k = i + 1; k < n; k++) {
      t -= L[k][i] * x[k];
    }
    x[i] = t / L[i][i];
  }
}

int rb_fd(const rb_model *mdl, const double *q, const double *qd, const double *tau,
          const rb_force *fe, int nf, double *qdd) {
  double M[RB_MAXJ][RB_MAXJ], h[RB_MAXJ], b[RB_MAXJ], z[RB_MAXJ] = {0};
  rb_mass(mdl, q, M);
  rb_rnea(mdl, q, qd, z, 1, fe, nf, h);  // bias, external forces included
  for(int i = 0; i < mdl->n; i++) {
    b[i] = tau[i] - h[i];
  }
  if(rb_chol(mdl->n, M)) {
    return -1;
  }
  rb_chol_solve(mdl->n, M, b, qdd);
  return 0;
}

void rb_energy(const rb_model *mdl, const double *q, const double *qd, double *kin, double *pot) {
  double M[RB_MAXJ][RB_MAXJ], R[RB_MAXJ][3][3], p[RB_MAXJ][3];
  int n = mdl->n;
  rb_mass(mdl, q, M);
  double t = 0.0;
  for(int i = 0; i < n; i++) {
    for(int j = 0; j < n; j++) {
      t += 0.5 * qd[i] * M[i][j] * qd[j];
    }
  }
  rb_fk(mdl, q, R, p);
  double v = 0.0;
  for(int i = 0; i < n; i++) {  // -g . (m p + R h)
    double rh[3];
    mv(R[i], mdl->link[i].h, rh);
    for(int k = 0; k < 3; k++) {
      v -= mdl->g[k] * (mdl->link[i].m * p[i][k] + rh[k]);
    }
  }
  *kin = t;
  *pot = v;
}

void rb_params_get(const rb_model *mdl, double *pi) {
  int n = mdl->n;
  for(int i = 0; i < n; i++) {
    const rb_link *l = &mdl->link[i];
    double *o = pi + i * RB_NPAR;
    o[0] = l->m;
    for(int k = 0; k < 3; k++) {
      o[1 + k] = l->h[k];
    }
    for(int k = 0; k < 6; k++) {
      o[4 + k] = l->I[k];
    }
  }
  for(int i = 0; i < n; i++) {
    pi[n * RB_NPAR + i] = mdl->arm[i][i];
  }
}

void rb_params_set(rb_model *mdl, const double *pi) {
  int n = mdl->n;
  for(int i = 0; i < n; i++) {
    rb_link *l = &mdl->link[i];
    const double *o = pi + i * RB_NPAR;
    l->m = o[0];
    for(int k = 0; k < 3; k++) {
      l->h[k] = o[1 + k];
    }
    for(int k = 0; k < 6; k++) {
      l->I[k] = o[4 + k];
    }
  }
  for(int i = 0; i < n; i++) {
    mdl->arm[i][i] = pi[n * RB_NPAR + i];
  }
}

void rb_regressor(const rb_model *mdl, const double *q, const double *qd, const double *qdd,
                  double Y[RB_MAXJ][RB_NCOL]) {
  int n = mdl->n;
  rb_model u = *mdl;  // same geometry, one unit parameter at a time
  double pi[RB_NCOL], t[RB_MAXJ];
  memset(u.arm, 0, sizeof(u.arm));
  memset(Y, 0, sizeof(double) * RB_MAXJ * RB_NCOL);
  for(int c = 0; c < n * RB_NPAR; c++) {
    memset(pi, 0, sizeof(pi));
    pi[c] = 1.0;
    rb_params_set(&u, pi);
    rb_rnea(&u, q, qd, qdd, 1, 0, 0, t);
    for(int i = 0; i < n; i++) {
      Y[i][c] = t[i];
    }
  }
  for(int i = 0; i < n; i++) {
    Y[i][n * RB_NPAR + i] = qdd[i];
  }
}

int rb_armature(rb_model *mdl, const double K[RB_MAXJ][RB_MAXJ], const double *jm) {
  int n = mdl->n;
  double A[RB_MAXJ][2 * RB_MAXJ], Ki[RB_MAXJ][RB_MAXJ];
  for(int i = 0; i < n; i++) {  // Gauss-Jordan inverse of K
    for(int j = 0; j < n; j++) {
      A[i][j] = K[i][j];
      A[i][n + j] = i == j;
    }
  }
  for(int c = 0; c < n; c++) {
    int p = c;
    for(int r = c + 1; r < n; r++) {
      if(fabs(A[r][c]) > fabs(A[p][c])) {
        p = r;
      }
    }
    if(fabs(A[p][c]) < 1e-12) {
      return -1;
    }
    for(int j = 0; j < 2 * n; j++) {
      double t = A[c][j];
      A[c][j] = A[p][j];
      A[p][j] = t;
    }
    double d = A[c][c];
    for(int j = 0; j < 2 * n; j++) {
      A[c][j] /= d;
    }
    for(int r = 0; r < n; r++) {
      if(r != c && A[r][c] != 0.0) {
        double f = A[r][c];
        for(int j = 0; j < 2 * n; j++) {
          A[r][j] -= f * A[c][j];
        }
      }
    }
  }
  for(int i = 0; i < n; i++) {
    for(int j = 0; j < n; j++) {
      Ki[i][j] = A[i][n + j];
    }
  }
  for(int i = 0; i < n; i++) {  // Ki^T diag(jm) Ki
    for(int j = 0; j < n; j++) {
      double s = 0.0;
      for(int k = 0; k < n; k++) {
        s += Ki[k][i] * jm[k] * Ki[k][j];
      }
      mdl->arm[i][j] = s;
    }
  }
  return 0;
}

double rb_friction(const rb_fric *f, double qd) {
  double eps = f->eps > 0.0 ? f->eps : 1e-3;
  double s = tanh(qd / eps);
  double st = f->vs > 0.0 ? (f->fs - f->fc) * exp(-(qd / f->vs) * (qd / f->vs)) : 0.0;
  return (f->fc + st) * s + f->fv * qd;
}

void rb_observer_reset(rb_observer *o) {
  memset(o->r, 0, sizeof(o->r));
  memset(o->integ, 0, sizeof(o->integ));
  o->init = 0;
}

// r = k (p - p0 - integral(tau + C^T qd - g + r)),  p = M qd.
// C^T qd = Mdot qd - C qd (Mdot - 2C is skew), and over one step
// integral(Mdot qd) = (M_k - M_k-1) (qd_k + qd_k-1) / 2; the rest by the
// trapezoid rule. Second order in dt, no acceleration needed.
void rb_observer_step(rb_observer *o, const rb_model *mdl, const double *q, const double *qd,
                      const double *tau, double dt) {
  int n = mdl->n;
  double M[RB_MAXJ][RB_MAXJ], g[RB_MAXJ], cq[RB_MAXJ], p[RB_MAXJ], b[RB_MAXJ];
  rb_mass(mdl, q, M);
  rb_gravity(mdl, q, g);
  rb_coriolis(mdl, q, qd, cq);
  for(int i = 0; i < n; i++) {
    p[i] = 0.0;
    for(int j = 0; j < n; j++) {
      p[i] += M[i][j] * qd[j];
    }
    b[i] = tau[i] - cq[i] - g[i];
  }
  if(!o->init) {
    memcpy(o->p0, p, sizeof(o->p0));
    memset(o->integ, 0, sizeof(o->integ));
    memset(o->r, 0, sizeof(o->r));
    o->init = 1;
  } else {
    for(int i = 0; i < n; i++) {
      double mq = 0.0;
      for(int j = 0; j < n; j++) {
        mq += (M[i][j] - o->Mprev[i][j]) * 0.5 * (qd[j] + o->qdprev[j]);
      }
      o->integ[i] += mq + 0.5 * (b[i] + o->bprev[i]) * dt + o->r[i] * dt;
      o->r[i] = o->k * (p[i] - o->p0[i] - o->integ[i]);
    }
  }
  memcpy(o->Mprev, M, sizeof(M));
  memcpy(o->qdprev, qd, sizeof(o->qdprev));
  memcpy(o->bprev, b, sizeof(o->bprev));
}
