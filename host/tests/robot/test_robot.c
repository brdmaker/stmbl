/*
 * Tests for shared/robot (TAP output). usage: test_robot rtb_ref.txt
 *
 * - against an independent implementation (Robotics Toolbox for Python,
 *   rtb_ref.txt from gen_robot_ref.py): forward kinematics, inverse
 *   dynamics, mass matrix, gravity of a 6-joint arm with full inertias
 * - internal consistency: M symmetric positive definite, M qdd + C qd + g =
 *   rnea, qd' (Mdot - 2 C) qd = 0, regressor * parameters = rnea
 * - physics: energy conserved in a free swing, energy change = work of the
 *   joint torques, a two-link arm against its hand-derived equations,
 *   external force = J^T f, rotor energy through a coupled transmission
 * - the momentum observer finds an external force and stays at zero
 *   without one
 */
#include "robot.h"
#include "kin.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ntest, nfail;

static void ok(int cond, const char *name, double got, double lim) {
  ntest++;
  if(cond) {
    printf("ok %d - robot: %s\n", ntest, name);
  } else {
    nfail++;
    printf("not ok %d - robot: %s\n#   got %g, limit %g\n", ntest, name, got, lim);
  }
}

static double urand(double a, double b) {
  return a + (b - a) * (rand() / (double)RAND_MAX);
}

static rb_model ref;  // the arm of rtb_ref.txt

static double maxabs(const double *a, const double *b, int n) {
  double m = 0.0;
  for(int i = 0; i < n; i++) {
    double d = fabs(a[i] - b[i]);
    m = d > m ? d : m;
  }
  return m;
}

static void reference_tests(const char *path) {
  FILE *f = fopen(path, "r");
  if(!f) {
    ok(0, "open reference file", 0, 0);
    return;
  }
  char line[8192];
  double e_rne = 0, e_mass = 0, e_grav = 0, e_fk = 0;
  int n_rne = 0, n_mass = 0;
  memset(&ref, 0, sizeof(ref));
  ref.g[2] = -9.81;
  while(fgets(line, sizeof(line), f)) {
    double v[64];
    int k = 0;
    char *s = strchr(line, ' ');
    if(line[0] == '#' || !s) {
      continue;
    }
    char *end;
    for(char *p = s; k < 64; p = end) {
      double x = strtod(p, &end);
      if(end == p) {
        break;
      }
      v[k++] = x;
    }
    if(!strncmp(line, "link ", 5) && k == 14) {
      rb_link *l = &ref.link[ref.n++];
      l->a = v[0], l->alpha = v[1], l->d = v[2], l->theta0 = v[3];
      rb_link_from_com(l, v[4], v + 5, v + 8);
    } else if(!strncmp(line, "rne ", 4) && k == 24) {
      double t[6];
      rb_rnea(&ref, v, v + 6, v + 12, 1, 0, 0, t);
      double e = maxabs(t, v + 18, 6) / (1.0 + maxabs(v + 18, (double[6]){0}, 6));
      e_rne = e > e_rne ? e : e_rne;
      n_rne++;
    } else if(!strncmp(line, "mass ", 5) && k == 42) {
      double M[RB_MAXJ][RB_MAXJ], m[36];
      rb_mass(&ref, v, M);
      for(int i = 0; i < 6; i++) {
        for(int j = 0; j < 6; j++) {
          m[i * 6 + j] = M[i][j];
        }
      }
      double e = maxabs(m, v + 6, 36);
      e_mass = e > e_mass ? e : e_mass;
      n_mass++;
    } else if(!strncmp(line, "grav ", 5) && k == 12) {
      double g[6];
      rb_gravity(&ref, v, g);
      double e = maxabs(g, v + 6, 6);
      e_grav = e > e_grav ? e : e_grav;
    } else if(!strncmp(line, "fk ", 3) && k == 18) {
      double R[RB_MAXJ][3][3], p[RB_MAXJ][3], t[12];
      rb_fk(&ref, v, R, p);
      for(int i = 0; i < 3; i++) {
        for(int j = 0; j < 3; j++) {
          t[i * 3 + j] = R[5][i][j];
        }
        t[9 + i] = p[5][i];
      }
      double e = maxabs(t, v + 6, 12);
      e_fk = e > e_fk ? e : e_fk;
    }
  }
  fclose(f);
  ok(ref.n == 6 && n_rne == 20 && n_mass == 5, "reference file read", ref.n, 6);
  ok(e_fk < 1e-12, "forward kinematics = rtb fkine (6 joints)", e_fk, 1e-12);
  ok(e_rne < 1e-10, "inverse dynamics = rtb rne (20 random states)", e_rne, 1e-10);
  ok(e_mass < 1e-10, "mass matrix = rtb inertia", e_mass, 1e-10);
  ok(e_grav < 1e-10, "gravity torques = rtb gravload", e_grav, 1e-10);
}

static void rand_state(int n, double *q, double *qd, double *qdd) {
  for(int i = 0; i < n; i++) {
    q[i] = urand(-M_PI, M_PI);
    if(qd) {
      qd[i] = urand(-3, 3);
    }
    if(qdd) {
      qdd[i] = urand(-10, 10);
    }
  }
}

static void consistency_tests(void) {
  rb_model m = ref;
  // add an armature with off-diagonal terms (coupled wrist)
  double K[RB_MAXJ][RB_MAXJ] = {{0}}, jm[6] = {2e-4, 2e-4, 1e-4, 3e-5, 3e-5, 3e-5};
  double ratio[6] = {120, 120, 100, 80, 80, 50};
  for(int i = 0; i < 6; i++) {
    K[i][i] = 1.0 / ratio[i];
  }
  K[4][3] = 1.0 / ratio[3];  // J4 motor also turns J5
  K[5][4] = -1.0 / ratio[4];
  rb_armature(&m, K, jm);

  double emin = 1e9, esym = 0, esplit = 0, eskew = 0, ereg = 0;
  for(int t = 0; t < 50; t++) {
    double q[6], qd[6], qdd[6], M[RB_MAXJ][RB_MAXJ], g[6], c[6], tau[6];
    rand_state(6, q, qd, qdd);
    rb_mass(&m, q, M);
    rb_gravity(&m, q, g);
    rb_coriolis(&m, q, qd, c);
    rb_rnea(&m, q, qd, qdd, 1, 0, 0, tau);
    for(int i = 0; i < 6; i++) {
      double s = c[i] + g[i];
      for(int j = 0; j < 6; j++) {
        s += M[i][j] * qdd[j];
        double d = fabs(M[i][j] - M[j][i]);
        esym = d > esym ? d : esym;
      }
      double d = fabs(s - tau[i]) / (1.0 + fabs(tau[i]));
      esplit = d > esplit ? d : esplit;
    }
    double L[RB_MAXJ][RB_MAXJ];
    memcpy(L, M, sizeof(L));
    if(rb_chol(6, L)) {
      emin = -1;
    } else {
      for(int i = 0; i < 6; i++) {
        emin = L[i][i] < emin ? L[i][i] : emin;
      }
    }
    // qd' Mdot qd = 2 qd' C qd, Mdot by central difference along qd
    double h = 1e-6, qp[6], qm[6], Mp[RB_MAXJ][RB_MAXJ], Mm[RB_MAXJ][RB_MAXJ];
    for(int i = 0; i < 6; i++) {
      qp[i] = q[i] + h * qd[i];
      qm[i] = q[i] - h * qd[i];
    }
    rb_mass(&m, qp, Mp);
    rb_mass(&m, qm, Mm);
    double a = 0, b = 0, sc = 0;
    for(int i = 0; i < 6; i++) {
      for(int j = 0; j < 6; j++) {
        a += qd[i] * (Mp[i][j] - Mm[i][j]) / (2 * h) * qd[j];
      }
      b += 2.0 * qd[i] * c[i];
      sc += fabs(qd[i] * c[i]);
    }
    double e = fabs(a - b) / (1.0 + sc);
    eskew = e > eskew ? e : eskew;
    // regressor
    static double Y[RB_MAXJ][RB_NCOL];
    double pi[RB_NCOL];
    rb_model md = m;  // diagonal armature only for the regressor check
    for(int i = 0; i < 6; i++) {
      for(int j = 0; j < 6; j++) {
        md.arm[i][j] = i == j ? m.arm[i][j] : 0.0;
      }
    }
    rb_rnea(&md, q, qd, qdd, 1, 0, 0, tau);
    rb_regressor(&md, q, qd, qdd, Y);
    rb_params_get(&md, pi);
    for(int i = 0; i < 6; i++) {
      double s = 0;
      for(int j = 0; j < 6 * RB_NPAR + 6; j++) {
        s += Y[i][j] * pi[j];
      }
      double d = fabs(s - tau[i]) / (1.0 + fabs(tau[i]));
      ereg = d > ereg ? d : ereg;
    }
  }
  ok(esym == 0.0, "mass matrix symmetric", esym, 0);
  ok(emin > 0.0, "mass matrix positive definite (Cholesky)", emin, 0);
  ok(esplit < 1e-10, "M qdd + C qd + g = rnea", esplit, 1e-10);
  ok(eskew < 1e-5, "qd' (Mdot - 2C) qd = 0", eskew, 1e-5);
  ok(ereg < 1e-10, "regressor * parameters = rnea", ereg, 1e-10);
}

// RK4 of the arm with joint torques tau(t) = ctrl(q, qd, t)
typedef void (*ctrl_fn)(const rb_model *, const double *, const double *, double, double *);

static void rk4(const rb_model *m, double *q, double *qd, double t, double dt, ctrl_fn ctrl,
                const rb_force *fe, int nf) {
  int n = m->n;
  double k1q[6], k1v[6], k2q[6], k2v[6], k3q[6], k3v[6], k4q[6], k4v[6], qt[6], vt[6], tau[6];
  ctrl(m, q, qd, t, tau);
  memcpy(k1q, qd, sizeof(k1q));
  rb_fd(m, q, qd, tau, fe, nf, k1v);
  for(int i = 0; i < n; i++) {
    qt[i] = q[i] + 0.5 * dt * k1q[i], vt[i] = qd[i] + 0.5 * dt * k1v[i];
  }
  ctrl(m, qt, vt, t + 0.5 * dt, tau);
  memcpy(k2q, vt, sizeof(k2q));
  rb_fd(m, qt, vt, tau, fe, nf, k2v);
  for(int i = 0; i < n; i++) {
    qt[i] = q[i] + 0.5 * dt * k2q[i], vt[i] = qd[i] + 0.5 * dt * k2v[i];
  }
  ctrl(m, qt, vt, t + 0.5 * dt, tau);
  memcpy(k3q, vt, sizeof(k3q));
  rb_fd(m, qt, vt, tau, fe, nf, k3v);
  for(int i = 0; i < n; i++) {
    qt[i] = q[i] + dt * k3q[i], vt[i] = qd[i] + dt * k3v[i];
  }
  ctrl(m, qt, vt, t + dt, tau);
  memcpy(k4q, vt, sizeof(k4q));
  rb_fd(m, qt, vt, tau, fe, nf, k4v);
  for(int i = 0; i < n; i++) {
    q[i] += dt / 6.0 * (k1q[i] + 2 * k2q[i] + 2 * k3q[i] + k4q[i]);
    qd[i] += dt / 6.0 * (k1v[i] + 2 * k2v[i] + 2 * k3v[i] + k4v[i]);
  }
}

static void ctrl_zero(const rb_model *m, const double *q, const double *qd, double t, double *tau) {
  (void)q, (void)qd, (void)t;
  memset(tau, 0, sizeof(double) * m->n);
}

static void ctrl_drive(const rb_model *m, const double *q, const double *qd, double t, double *tau) {
  (void)q;
  for(int i = 0; i < m->n; i++) {
    tau[i] = (6 - i) * sin(2.0 * t + i) - 0.5 * qd[i];
  }
}

static void energy_tests(void) {
  double q[6] = {0.1, -0.4, 0.8, 0.3, -1.0, 0.5}, qd[6] = {0.5, -0.3, 0.2, 1.0, -0.8, 2.0};
  double k0, p0, k1, p1;
  rb_energy(&ref, q, qd, &k0, &p0);
  for(int i = 0; i < 20000; i++) {  // 2 s
    rk4(&ref, q, qd, i * 1e-4, 1e-4, ctrl_zero, 0, 0);
  }
  rb_energy(&ref, q, qd, &k1, &p1);
  double e = fabs((k1 + p1) - (k0 + p0)) / (k0 + fabs(p0));
  ok(e < 1e-8, "energy conserved in a 2 s free swing (RK4)", e, 1e-8);
  ok(k1 > 1.0, "  ...and the arm actually moved (kinetic energy J)", k1, 1.0);

  // with torques: dE = integral(qd . tau) dt
  double q2[6] = {0}, qd2[6] = {0}, w = 0.0, dt = 1e-4;
  rb_energy(&ref, q2, qd2, &k0, &p0);
  for(int i = 0; i < 10000; i++) {
    double tau[6], tau1[6], pw0 = 0, pw1 = 0;
    ctrl_drive(&ref, q2, qd2, i * dt, tau);
    for(int j = 0; j < 6; j++) {
      pw0 += qd2[j] * tau[j];
    }
    rk4(&ref, q2, qd2, i * dt, dt, ctrl_drive, 0, 0);
    ctrl_drive(&ref, q2, qd2, (i + 1) * dt, tau1);
    for(int j = 0; j < 6; j++) {
      pw1 += qd2[j] * tau1[j];
    }
    w += 0.5 * (pw0 + pw1) * dt;
  }
  rb_energy(&ref, q2, qd2, &k1, &p1);
  e = fabs((k1 + p1) - (k0 + p0) - w) / (fabs(w) + 1.0);
  ok(e < 1e-6, "energy change = work of the joint torques", e, 1e-6);
}

static void two_link_test(void) {
  // planar arm, joints along z, gravity -y; link frame x points along the link
  double L1 = 0.7, lc1 = 0.3, lc2 = 0.25, m1 = 5, m2 = 3, I1 = 0.2, I2 = 0.1, g = 9.81;
  rb_model m;
  memset(&m, 0, sizeof(m));
  m.n = 2;
  m.g[1] = -g;
  m.link[1].a = L1;
  rb_link_from_com(&m.link[0], m1, (double[3]){lc1, 0, 0}, (double[6]){0.01, 0.01, I1, 0, 0, 0});
  rb_link_from_com(&m.link[1], m2, (double[3]){lc2, 0, 0}, (double[6]){0.01, 0.01, I2, 0, 0, 0});
  double e = 0;
  for(int t = 0; t < 20; t++) {
    double q[2], qd[2], qdd[2], tau[2], ex[2];
    rand_state(2, q, qd, qdd);
    double c2 = cos(q[1]), s2 = sin(q[1]);
    double M11 = I1 + I2 + m1 * lc1 * lc1 + m2 * (L1 * L1 + lc2 * lc2 + 2 * L1 * lc2 * c2);
    double M12 = I2 + m2 * (lc2 * lc2 + L1 * lc2 * c2), M22 = I2 + m2 * lc2 * lc2;
    double hh = m2 * L1 * lc2 * s2;
    double g1 = (m1 * lc1 + m2 * L1) * g * cos(q[0]) + m2 * lc2 * g * cos(q[0] + q[1]);
    double g2 = m2 * lc2 * g * cos(q[0] + q[1]);
    ex[0] = M11 * qdd[0] + M12 * qdd[1] - hh * (2 * qd[0] * qd[1] + qd[1] * qd[1]) + g1;
    ex[1] = M12 * qdd[0] + M22 * qdd[1] + hh * qd[0] * qd[0] + g2;
    rb_rnea(&m, q, qd, qdd, 1, 0, 0, tau);
    double d = maxabs(tau, ex, 2);
    e = d > e ? d : e;
  }
  ok(e < 1e-10, "two-link arm = hand-derived equations", e, 1e-10);
}

static void force_test(void) {
  double e = 0;
  for(int t = 0; t < 10; t++) {
    double q[6], qd[6], qdd[6], t0[6], t1[6];
    rand_state(6, q, qd, qdd);
    rb_force fe = {.link = 4, .r = {0.05, -0.1, 0.2}, .f = {urand(-50, 50), urand(-50, 50), urand(-50, 50)}};
    rb_rnea(&ref, q, qd, qdd, 1, 0, 0, t0);
    rb_rnea(&ref, q, qd, qdd, 1, &fe, 1, t1);
    // J^T f with J of the point by central differences
    double jt[6], h = 1e-6;
    for(int j = 0; j < 6; j++) {
      double qp[6], qm[6], pp[3], pm[3];
      memcpy(qp, q, sizeof(qp));
      memcpy(qm, q, sizeof(qm));
      qp[j] += h, qm[j] -= h;
      rb_point(&ref, qp, fe.link, fe.r, pp);
      rb_point(&ref, qm, fe.link, fe.r, pm);
      jt[j] = 0;
      for(int k = 0; k < 3; k++) {
        jt[j] += (pp[k] - pm[k]) / (2 * h) * fe.f[k];
      }
      double d = fabs((t0[j] - t1[j]) - jt[j]);
      e = d > e ? d : e;
    }
  }
  ok(e < 1e-6, "external force: torque change = J^T f", e, 1e-6);
}

static void armature_test(void) {
  rb_model m;
  memset(&m, 0, sizeof(m));
  m.n = 3;
  double K[RB_MAXJ][RB_MAXJ] = {{1.0 / 100, 0, 0}, {1.0 / 80, 1.0 / 80, 0}, {0, -1.0 / 60, 1.0 / 50}};
  double jm[3] = {3e-4, 1e-4, 5e-5}, wm[3] = {120, -70, 300}, qd[3] = {0}, q[3] = {0}, kin, pot;
  rb_armature(&m, K, jm);
  double em = 0;
  for(int i = 0; i < 3; i++) {
    for(int j = 0; j < 3; j++) {
      qd[i] += K[i][j] * wm[j];
    }
    em += 0.5 * jm[i] * wm[i] * wm[i];
  }
  rb_energy(&m, q, qd, &kin, &pot);
  double e = fabs(kin - em) / em;
  ok(e < 1e-12, "coupled transmission: rotor energy = joint-space armature energy", e, 1e-12);
}

// a servo holding q0 with a small motion on top (as the arm would be)
static const double obs_q0[6] = {0.2, -0.3, 0.5, 0.1, 0.4, -0.2};
static void ctrl_hold(const rb_model *m, const double *q, const double *qd, double t, double *tau) {
  double kp[6] = {20000, 20000, 10000, 2000, 2000, 500};
  rb_gravity(m, q, tau);
  for(int i = 0; i < m->n; i++) {
    double qr = obs_q0[i] + 0.05 * sin(3.0 * t + i), vr = 0.15 * cos(3.0 * t + i);
    tau[i] += kp[i] * (qr - q[i]) + 2.0 * sqrt(kp[i]) * (vr - qd[i]);
  }
}

static rb_force obs_force;
static int obs_force_on;

static void observer_test(void) {
  rb_observer o;
  memset(&o, 0, sizeof(o));
  o.k = 50.0;
  rb_observer_reset(&o);
  double q[6], qd[6] = {0}, dt = 1e-3;
  memcpy(q, obs_q0, sizeof(q));
  double rmax0 = 0, e1 = 0, jtf[6];
  obs_force = (rb_force){.link = 5, .r = {0, 0, 0.1}, .f = {30, -20, 40}};
  for(int i = 0; i < 2000; i++) {
    double t = i * dt, tau[6];
    obs_force_on = t >= 1.0;
    ctrl_hold(&ref, q, qd, t, tau);
    rb_observer_step(&o, &ref, q, qd, tau, dt);
    // from 0.3 s: the servo's start-up jolt (hundreds of Nm within a few
    // ms, faster than the 1 kHz samples resolve) shows as ~0.5 Nm at first
    if(t > 0.3 && t < 1.0) {
      for(int j = 0; j < 6; j++) {
        rmax0 = fabs(o.r[j]) > rmax0 ? fabs(o.r[j]) : rmax0;
      }
    }
    if(t > 1.2) {  // J^T f at this pose: torque the force needs vs none
      double z[6] = {0}, a[6], b[6];
      rb_rnea(&ref, q, z, z, 0, 0, 0, a);
      rb_rnea(&ref, q, z, z, 0, &obs_force, 1, b);
      double sc = 0;
      for(int j = 0; j < 6; j++) {
        jtf[j] = a[j] - b[j];
        sc = fabs(jtf[j]) > sc ? fabs(jtf[j]) : sc;
      }
      double d = maxabs(o.r, jtf, 6) / sc;
      e1 = d > e1 ? d : e1;
    }
    if(getenv("OBS_TRACE") && i % 100 == 0) {
      printf("# t %.1f r %8.3f %8.3f %8.3f %8.3f %8.3f %8.3f qd %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f\n", t,
             o.r[0], o.r[1], o.r[2], o.r[3], o.r[4], o.r[5], qd[0], qd[1], qd[2], qd[3], qd[4], qd[5]);
    }
    // the plant, 0.1 ms steps, torques evaluated every step
    for(int s = 0; s < 10; s++) {
      double qdd[6], tq[6];
      ctrl_hold(&ref, q, qd, t + s * dt / 10, tq);
      rb_fd(&ref, q, qd, tq, &obs_force, obs_force_on, qdd);
      for(int j = 0; j < 6; j++) {  // semi-implicit Euler, 0.1 ms
        qd[j] += qdd[j] * dt / 10;
        q[j] += qd[j] * dt / 10;
      }
    }
  }
  ok(rmax0 < 0.05, "observer: no external force -> residual < 0.05 Nm", rmax0, 0.05);
  ok(e1 < 0.02, "observer: residual = J^T f after 10 time constants (rel.)", e1, 0.02);
}

static double det6(double J[6][RB_MAXJ]) {
  double A[6][6], d = 1.0;
  for(int i = 0; i < 6; i++) {
    for(int j = 0; j < 6; j++) {
      A[i][j] = J[i][j];
    }
  }
  for(int c = 0; c < 6; c++) {
    int p = c;
    for(int r = c + 1; r < 6; r++) {
      if(fabs(A[r][c]) > fabs(A[p][c])) {
        p = r;
      }
    }
    if(p != c) {
      d = -d;
      for(int k = 0; k < 6; k++) {
        double t = A[c][k];
        A[c][k] = A[p][k];
        A[p][k] = t;
      }
    }
    d *= A[c][c];
    if(A[c][c] == 0.0) {
      return 0.0;
    }
    for(int r = c + 1; r < 6; r++) {
      double f = A[r][c] / A[c][c];
      for(int k = c; k < 6; k++) {
        A[r][k] -= f * A[c][k];
      }
    }
  }
  return d;
}

static void kinematics_tests(void) {
  double e_rpy = 0, e_rv = 0, e_jac = 0, e_ik = 0, e_branch = 0;
  int ik_fail = 0, ik_iter = 0;
  for(int t = 0; t < 200; t++) {
    double rpy[3] = {urand(-M_PI, M_PI), urand(-1.4, 1.4), urand(-M_PI, M_PI)}, R[3][3], r2[3];
    rb_R_from_rpy(rpy, R);
    rb_rpy_from_R(R, r2);
    for(int i = 0; i < 3; i++) {
      double d = fabs(remainder(r2[i] - rpy[i], 2 * M_PI));
      e_rpy = d > e_rpy ? d : e_rpy;
    }
    // rotation vectors, also close to 180 deg
    double ax[3] = {urand(-1, 1), urand(-1, 1), urand(-1, 1)};
    double nn = sqrt(ax[0] * ax[0] + ax[1] * ax[1] + ax[2] * ax[2]);
    double th = t % 4 == 0 ? M_PI - 1e-7 * (t % 7) : urand(0, M_PI - 1e-3);
    double w[3] = {ax[0] / nn * th, ax[1] / nn * th, ax[2] / nn * th}, R2[3][3], w2[3], R3[3][3];
    rb_R_from_rotvec(w, R2);
    rb_rotvec_from_R(R2, w2);
    rb_R_from_rotvec(w2, R3);  // compare rotations: w and -w are the same at 180
    for(int i = 0; i < 3; i++) {
      for(int j = 0; j < 3; j++) {
        double d = fabs(R3[i][j] - R2[i][j]);
        e_rv = d > e_rv ? d : e_rv;
      }
    }
  }
  for(int t = 0; t < 50; t++) {
    double q[6], J[6][RB_MAXJ], h = 1e-6, p0[3], R0[3][3];
    rand_state(6, q, 0, 0);
    rb_jacobian(&ref, q, J);
    rb_pose(&ref, q, p0, R0);
    for(int k = 0; k < 6; k++) {
      double qp[6], p1[3], R1[3][3], Rd[3][3], w[3];
      memcpy(qp, q, sizeof(qp));
      qp[k] += h;
      rb_pose(&ref, qp, p1, R1);
      for(int i = 0; i < 3; i++) {
        for(int j = 0; j < 3; j++) {
          Rd[i][j] = R1[i][0] * R0[j][0] + R1[i][1] * R0[j][1] + R1[i][2] * R0[j][2];
        }
      }
      rb_rotvec_from_R(Rd, w);
      for(int i = 0; i < 3; i++) {
        double d1 = fabs((p1[i] - p0[i]) / h - J[i][k]), d2 = fabs(w[i] / h - J[3 + i][k]);
        e_jac = d1 > e_jac ? d1 : e_jac;
        e_jac = d2 > e_jac ? d2 : e_jac;
      }
    }
    // IK from a start up to 0.1 rad off: back to the same pose and, away
    // from singular poses (|det J| > 0.01; near one, another configuration
    // can be as close), the same joint angles
    double pt[3], Rt[3][3], qs[6], p[3], R[3][3];
    int regular = fabs(det6(J)) > 0.01;
    rb_pose(&ref, q, pt, Rt);
    for(int i = 0; i < 6; i++) {
      qs[i] = q[i] + urand(-0.1, 0.1);
    }
    int it = rb_ik(&ref, pt, Rt, qs, 1e-9, 1e-9, 100);
    if(it < 0) {
      ik_fail++;
      continue;
    }
    ik_iter = it > ik_iter ? it : ik_iter;
    rb_pose(&ref, qs, p, R);
    for(int i = 0; i < 3; i++) {
      double d = fabs(p[i] - pt[i]);
      e_ik = d > e_ik ? d : e_ik;
    }
    for(int i = 0; i < 6; i++) {
      double d = regular ? fabs(remainder(qs[i] - q[i], 2 * M_PI)) : 0.0;
      e_branch = d > e_branch ? d : e_branch;
    }
  }
  ok(e_rpy < 1e-12, "rpy (genserkins A B C) -> R -> rpy", e_rpy, 1e-12);
  ok(e_rv < 1e-6, "rotation vector round trip, also near 180 deg", e_rv, 1e-6);
  ok(e_jac < 1e-5, "Jacobian = finite differences of the flange pose", e_jac, 1e-5);
  ok(ik_fail == 0, "IK converges from starts 0.1 rad off (50 random poses)", ik_fail, 0);
  ok(e_ik < 1e-9, "  ...to the pose within 1e-9 m", e_ik, 1e-9);
  ok(e_branch < 1e-6, "  ...to the same joint angles (non-singular poses)", e_branch, 1e-6);
  ok(ik_iter <= 20, "  ...in <= 20 iterations", ik_iter, 20);
}

// LinuxCNC genserkins (genserkins_ref.txt, from hwtest/arcmate-sim) against
// rb_pose / rb_rpy_from_R with the DH lines of the arm file
static void genserkins_tests(const char *refpath, const char *armpath) {
  rb_model m;
  memset(&m, 0, sizeof(m));
  FILE *f = fopen(armpath, "r");
  char line[512];
  while(f && fgets(line, sizeof(line), f)) {
    int j;
    double a, al, d, t0;
    if(sscanf(line, "dh %d %lf %lf %lf %lf", &j, &a, &al, &d, &t0) == 5 && j >= 1 && j <= RB_MAXJ) {
      m.link[j - 1].a = a, m.link[j - 1].alpha = al * M_PI / 180, m.link[j - 1].d = d;
      m.link[j - 1].theta0 = t0 * M_PI / 180;
      m.n = j > m.n ? j : m.n;
    }
  }
  if(f) {
    fclose(f);
  }
  f = fopen(refpath, "r");
  double ep = 0, ea = 0;
  int n = 0;
  while(f && fgets(line, sizeof(line), f)) {
    double v[12];
    if(sscanf(line, "pair %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf", v, v + 1, v + 2, v + 3,
              v + 4, v + 5, v + 6, v + 7, v + 8, v + 9, v + 10, v + 11) != 12) {
      continue;
    }
    double q[RB_MAXJ], p[3], R[3][3], rpy[3];
    for(int j = 0; j < 6; j++) {  // LinuxCNC's joints are the DH theta
      q[j] = v[j] * M_PI / 180 - m.link[j].theta0;
    }
    rb_pose(&m, q, p, R);
    rb_rpy_from_R(R, rpy);
    for(int i = 0; i < 3; i++) {
      double d = fabs(p[i] * 1000 - v[6 + i]);
      ep = d > ep ? d : ep;
      double da = fabs(remainder(rpy[i] * 180 / M_PI - v[9 + i], 360.0));
      ea = da > ea ? da : ea;
    }
    n++;
  }
  if(f) {
    fclose(f);
  }
  ok(m.n == 6 && n >= 8, "genserkins reference read (arm DH, 8 poses)", n, 8);
  ok(ep < 1e-5, "flange position = LinuxCNC genserkins (mm)", ep, 1e-5);
  ok(ea < 1e-5, "A B C = LinuxCNC genserkins (deg)", ea, 1e-5);
}

int main(int argc, char **argv) {
  srand(1203);
  reference_tests(argc > 1 ? argv[1] : "tests/robot/rtb_ref.txt");
  consistency_tests();
  energy_tests();
  two_link_test();
  force_test();
  armature_test();
  observer_test();
  kinematics_tests();
  if(argc > 3) {
    genserkins_tests(argv[2], argv[3]);
  }
  printf("1..%d\n# %d/%d passed\n", ntest, ntest - nfail, ntest);
  return nfail ? 1 : 0;
}
