/*
 * Simulated portal machine, see pplant.h.
 */
#include "pplant.h"

#include <math.h>
#include <string.h>

#define NB 4  // bodies: frame, bridge, carriage, head

// positions of the bodies' centres of mass in the floor's frame
static void bodies(const pp_plant *p, const double *q, double pos[NB][2]) {
  (void)p;
  double c = cos(q[PP_TH]), s = sin(q[PP_TH]);
  double r[NB][2] = {
      {0.0, 0.0},                                    // frame
      {0.0, q[PP_Y]},                                // bridge
      {q[PP_X] * c, q[PP_Y] + q[PP_X] * s},          // carriage
      {(q[PP_X] + q[PP_HX]) * c - q[PP_HY] * s,      // head
       q[PP_Y] + (q[PP_X] + q[PP_HX]) * s + q[PP_HY] * c},
  };
  double cp = cos(q[PP_PSI]), sp = sin(q[PP_PSI]);
  for(int i = 0; i < NB; i++) {
    pos[i][0] = q[PP_XF] + cp * r[i][0] - sp * r[i][1];
    pos[i][1] = q[PP_YF] + sp * r[i][0] + cp * r[i][1];
  }
}

static void masses(const pp_plant *p, double m[NB], double j[NB]) {
  m[0] = p->mf, m[1] = p->mb, m[2] = p->mc, m[3] = p->mh;
  j[0] = p->jf, j[1] = p->jb, j[2] = p->jc, j[3] = p->jh;
}

double pp_damp(double k, double m, double zeta) {
  return 2.0 * zeta * sqrt(k * m);
}

void pp_defaults(pp_plant *p) {
  memset(p, 0, sizeof(*p));
  p->mf = 600, p->jf = 200;
  p->L = 1.2;
  p->kr = 2e5, p->cr = pp_damp(p->kr, p->jb, 0.02);
  p->mb = 30, p->jb = 3.6;
  p->mc = 5, p->jc = 0.02;
  p->mh = 7, p->jh = 0.02;
  p->kx = 5.6e5, p->ky_end = 8.4e5, p->ky_mid = 2.2e5;
  p->cx = pp_damp(p->kx, p->mh, 0.03), p->cy = pp_damp(p->ky_mid, p->mh, 0.03);
  p->vs = 1e-3;
  p->fy2_c = p->fy2_v = -1.0;
  p->ymin = -0.5, p->ymax = 0.5, p->xmin = -0.5, p->xmax = 0.5;
  p->kstop = 2e7, p->cstop = 2e4;
}

double pp_ky(const pp_plant *p, double x) {
  // compliance grows towards mid-span like a beam's: 1/k quadratic in x
  double u = 2.0 * x / p->L, ce = 1.0 / p->ky_end, cm = 1.0 / p->ky_mid;
  return 1.0 / (ce + (cm - ce) * (1.0 - u * u));
}

static double fric(double v, double fc, double fv, double vs) {
  return fc * v / (fabs(v) + vs) + fv * v;
}

void pp_motor(const pp_plant *p, int k, double *pos, double *vel) {
  double s = sin(p->q[PP_TH]), c = cos(p->q[PP_TH]);
  if(k == PP_XM) {
    *pos = p->q[PP_X], *vel = p->qd[PP_X];
  } else {
    double a = (k == PP_Y1 ? -0.5 : 0.5) * p->L;
    *pos = p->q[PP_Y] + a * s;
    *vel = p->qd[PP_Y] + a * c * p->qd[PP_TH];
  }
}

// solve A x = b (n x n, A and b destroyed); 0 ok
static int solve(int n, double A[PP_N][PP_N], double *b, double *x) {
  for(int k = 0; k < n; k++) {
    int piv = k;
    for(int i = k + 1; i < n; i++) {
      if(fabs(A[i][k]) > fabs(A[piv][k])) {
        piv = i;
      }
    }
    if(fabs(A[piv][k]) < 1e-15) {
      return -1;
    }
    if(piv != k) {
      for(int j = 0; j < n; j++) {
        double t = A[k][j];
        A[k][j] = A[piv][j], A[piv][j] = t;
      }
      double t = b[k];
      b[k] = b[piv], b[piv] = t;
    }
    for(int i = k + 1; i < n; i++) {
      double f = A[i][k] / A[k][k];
      for(int j = k; j < n; j++) {
        A[i][j] -= f * A[k][j];
      }
      b[i] -= f * b[k];
    }
  }
  for(int i = n - 1; i >= 0; i--) {
    double s = b[i];
    for(int j = i + 1; j < n; j++) {
      s -= A[i][j] * x[j];
    }
    x[i] = s / A[i][i];
  }
  return 0;
}

// angular velocity of body i per qd: the frame turns with psi, the rest
// with psi + theta
static double wrow(int i, int k) {
  return k == PP_PSI || (i > 0 && k == PP_TH);
}

static void accel(pp_plant *p, const double *F) {
  double *q = p->q, *qd = p->qd;
  double m[NB], jr[NB], pos[NB][2], J[NB][2][PP_N], a[NB][2];
  masses(p, m, jr);
  // Jacobians by central differences
  for(int k = 0; k < PP_N; k++) {
    double qp[PP_N], qm[PP_N], pp[NB][2], pm[NB][2], h = 1e-6;
    memcpy(qp, q, sizeof(qp));
    memcpy(qm, q, sizeof(qm));
    qp[k] += h, qm[k] -= h;
    bodies(p, qp, pp);
    bodies(p, qm, pm);
    for(int i = 0; i < NB; i++) {
      J[i][0][k] = (pp[i][0] - pm[i][0]) / (2 * h);
      J[i][1][k] = (pp[i][1] - pm[i][1]) / (2 * h);
    }
  }
  // velocity terms Jd qd: the second derivative along qd
  double vn = 0.0;
  for(int k = 0; k < PP_N; k++) {
    vn += qd[k] * qd[k];
  }
  vn = sqrt(vn);
  memset(a, 0, sizeof(a));
  if(vn > 1e-12) {
    double h = 1e-4 / vn, qp[PP_N], qm[PP_N], pp[NB][2], pm[NB][2];
    for(int k = 0; k < PP_N; k++) {
      qp[k] = q[k] + h * qd[k], qm[k] = q[k] - h * qd[k];
    }
    bodies(p, q, pos);
    bodies(p, qp, pp);
    bodies(p, qm, pm);
    for(int i = 0; i < NB; i++) {
      for(int d = 0; d < 2; d++) {
        a[i][d] = (pp[i][d] - 2 * pos[i][d] + pm[i][d]) / (h * h);
      }
    }
  }
  // generalized forces
  double Q[PP_N] = {0};
  for(int k = 0; k < 3; k++) {  // feet
    Q[k] -= p->kf[k] * q[k] + p->cf[k] * qd[k];
  }
  double c = cos(q[PP_TH]);
  for(int s = 0; s < 2; s++) {  // Y1, Y2 with their guides
    double pos_s, vel_s, arm = (s == 0 ? -0.5 : 0.5) * p->L * c;
    pp_motor(p, s, &pos_s, &vel_s);
    int own = s == 1 && p->fy2_c >= 0.0;  // the Y2 side's own guide (an idler rail)
    double f = F[s] - fric(vel_s, own ? p->fy2_c : p->fy_c, own ? p->fy2_v : p->fy_v, p->vs);
    if(p->cog_a[s] != 0.0) {
      f -= p->cog_a[s] * sin(2.0 * M_PI * pos_s / p->cog_per[s] + p->cog_ph[s]);
    }
    Q[PP_Y] += f, Q[PP_TH] += arm * f;
  }
  Q[PP_TH] -= p->kr * q[PP_TH] + p->cr * qd[PP_TH];
  Q[PP_X] += F[PP_XM] - fric(qd[PP_X], p->fx_c, p->fx_v, p->vs);
  if(p->cog_a[PP_XM] != 0.0) {
    Q[PP_X] -= p->cog_a[PP_XM] * sin(2.0 * M_PI * q[PP_X] / p->cog_per[PP_XM] + p->cog_ph[PP_XM]);
  }
  double ky = pp_ky(p, q[PP_X]), dk = (pp_ky(p, q[PP_X] + 1e-6) - pp_ky(p, q[PP_X] - 1e-6)) / 2e-6;
  Q[PP_HX] -= p->kx * q[PP_HX] + p->cx * qd[PP_HX];
  Q[PP_HY] -= ky * q[PP_HY] + p->cy * qd[PP_HY];
  Q[PP_X] -= 0.5 * dk * q[PP_HY] * q[PP_HY];
  // end stops (the bridge at its centre, the carriage)
  double lim[2][3] = {{p->ymin, p->ymax, PP_Y}, {p->xmin, p->xmax, PP_X}};
  for(int s = 0; s < 2; s++) {
    int k = (int)lim[s][2], idx = s ? 2 : 0;
    double pen = q[k] < lim[s][0] ? q[k] - lim[s][0] : q[k] > lim[s][1] ? q[k] - lim[s][1] : 0.0;
    int at = pen != 0.0;
    if(at) {
      double f = -p->kstop * pen - p->cstop * qd[k];
      Q[k] += pen < 0 ? fmax(f, 0.0) : fmin(f, 0.0);
    }
    if(at && !p->at_stop[idx]) {
      p->stop_hits[idx]++;
    }
    p->at_stop[idx] = at;
  }
  // M qdd = Q - sum m J^T a
  double M[PP_N][PP_N] = {{0}}, b[PP_N];
  for(int k = 0; k < PP_N; k++) {
    b[k] = Q[k];
    for(int i = 0; i < NB; i++) {
      b[k] -= m[i] * (J[i][0][k] * a[i][0] + J[i][1][k] * a[i][1]);
    }
    for(int l = 0; l < PP_N; l++) {
      double s = 0.0;
      for(int i = 0; i < NB; i++) {
        s += m[i] * (J[i][0][k] * J[i][0][l] + J[i][1][k] * J[i][1][l]) + jr[i] * wrow(i, k) * wrow(i, l);
      }
      M[k][l] = s;
    }
  }
  if(solve(PP_N, M, b, p->qdd)) {
    memset(p->qdd, 0, sizeof(p->qdd));
  }
}

void pp_step(pp_plant *p, const double *F, double dt) {
  memcpy(p->F, F, sizeof(p->F));
  accel(p, F);
  for(int k = 0; k < PP_N; k++) {  // semi-implicit Euler
    p->qd[k] += p->qdd[k] * dt;
    p->q[k] += p->qd[k] * dt;
  }
}

void pp_tool(const pp_plant *p, double out[2], double vel[2]) {
  double q[PP_N], pos[NB][2], h = 1e-6;
  memcpy(q, p->q, sizeof(q));
  q[PP_XF] = q[PP_YF] = q[PP_PSI] = 0.0;  // relative to the frame
  bodies(p, q, pos);
  out[0] = pos[3][0], out[1] = pos[3][1];
  if(vel) {
    double qp[PP_N], pp[NB][2];
    for(int k = 0; k < PP_N; k++) {
      qp[k] = q[k] + (k < 3 ? 0.0 : h * p->qd[k]);
    }
    bodies(p, qp, pp);
    vel[0] = (pp[3][0] - pos[3][0]) / h, vel[1] = (pp[3][1] - pos[3][1]) / h;
  }
}

static void velocities(const pp_plant *p, double v[NB][2]) {
  double qp[PP_N], qm[PP_N], pp[NB][2], pm[NB][2], h = 1e-7;
  for(int k = 0; k < PP_N; k++) {
    qp[k] = p->q[k] + h * p->qd[k], qm[k] = p->q[k] - h * p->qd[k];
  }
  bodies(p, qp, pp);
  bodies(p, qm, pm);
  for(int i = 0; i < NB; i++) {
    v[i][0] = (pp[i][0] - pm[i][0]) / (2 * h), v[i][1] = (pp[i][1] - pm[i][1]) / (2 * h);
  }
}

void pp_momentum(const pp_plant *p, double out[2]) {
  double m[NB], jr[NB], v[NB][2];
  masses(p, m, jr);
  velocities(p, v);
  out[0] = out[1] = 0.0;
  for(int i = 0; i < NB; i++) {
    out[0] += m[i] * v[i][0], out[1] += m[i] * v[i][1];
  }
}

double pp_energy(const pp_plant *p) {
  double m[NB], jr[NB], v[NB][2], e = 0.0;
  masses(p, m, jr);
  velocities(p, v);
  for(int i = 0; i < NB; i++) {
    double w = p->qd[PP_PSI] + (i > 0 ? p->qd[PP_TH] : 0.0);
    e += 0.5 * m[i] * (v[i][0] * v[i][0] + v[i][1] * v[i][1]) + 0.5 * jr[i] * w * w;
  }
  for(int k = 0; k < 3; k++) {
    e += 0.5 * p->kf[k] * p->q[k] * p->q[k];
  }
  e += 0.5 * p->kr * p->q[PP_TH] * p->q[PP_TH];
  e += 0.5 * p->kx * p->q[PP_HX] * p->q[PP_HX] + 0.5 * pp_ky(p, p->q[PP_X]) * p->q[PP_HY] * p->q[PP_HY];
  return e;
}

void pp_feet(const pp_plant *p, double out[2]) {
  for(int k = 0; k < 2; k++) {
    out[k] = p->kf[k] * p->q[k] + p->cf[k] * p->qd[k];
  }
}
