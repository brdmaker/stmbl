/*
 * Kinematics of a serial arm, see kin.h.
 */
#include "kin.h"
#include <math.h>
#include <string.h>

void rb_pose(const rb_model *mdl, const double *q, double p[3], double R[3][3]) {
  double Rs[RB_MAXJ][3][3], ps[RB_MAXJ][3];
  rb_fk(mdl, q, Rs, ps);
  memcpy(p, ps[mdl->n - 1], sizeof(double) * 3);
  memcpy(R, Rs[mdl->n - 1], sizeof(double) * 9);
}

void rb_R_from_rpy(const double rpy[3], double R[3][3]) {  // Rz(c) Ry(b) Rx(a)
  double ca = cos(rpy[0]), sa = sin(rpy[0]), cb = cos(rpy[1]), sb = sin(rpy[1]);
  double cc = cos(rpy[2]), sc = sin(rpy[2]);
  R[0][0] = cc * cb, R[0][1] = cc * sb * sa - sc * ca, R[0][2] = cc * sb * ca + sc * sa;
  R[1][0] = sc * cb, R[1][1] = sc * sb * sa + cc * ca, R[1][2] = sc * sb * ca - cc * sa;
  R[2][0] = -sb, R[2][1] = cb * sa, R[2][2] = cb * ca;
}

void rb_rpy_from_R(const double R[3][3], double rpy[3]) {
  double cb = sqrt(R[0][0] * R[0][0] + R[1][0] * R[1][0]);
  rpy[1] = atan2(-R[2][0], cb);
  if(cb > 1e-9) {
    rpy[0] = atan2(R[2][1], R[2][2]);
    rpy[2] = atan2(R[1][0], R[0][0]);
  } else {  // pitch +-90 deg: roll and yaw share an axis; put it all in yaw
    rpy[0] = 0.0;
    rpy[2] = atan2(-R[0][1], R[1][1]);
  }
}

void rb_rotvec_from_R(const double R[3][3], double w[3]) {
  double c = 0.5 * (R[0][0] + R[1][1] + R[2][2] - 1.0);
  c = c > 1.0 ? 1.0 : (c < -1.0 ? -1.0 : c);
  double th = acos(c);
  double v[3] = {R[2][1] - R[1][2], R[0][2] - R[2][0], R[1][0] - R[0][1]};
  if(th < 1e-6) {  // small: w ~ vee(R - R^T) / 2
    for(int i = 0; i < 3; i++) {
      w[i] = 0.5 * v[i];
    }
  } else if(M_PI - th < 1e-4) {  // ~180 deg: axis from the diagonal
    int k = R[0][0] > R[1][1] ? (R[0][0] > R[2][2] ? 0 : 2) : (R[1][1] > R[2][2] ? 1 : 2);
    double a[3];
    a[k] = sqrt((R[k][k] + 1.0) * 0.5);
    for(int i = 0; i < 3; i++) {
      if(i != k) {
        a[i] = R[i][k] / (2.0 * a[k]);
      }
    }
    if(a[0] * v[0] + a[1] * v[1] + a[2] * v[2] < 0.0) {
      a[0] = -a[0], a[1] = -a[1], a[2] = -a[2];
    }
    for(int i = 0; i < 3; i++) {
      w[i] = th * a[i];
    }
  } else {
    double f = th / (2.0 * sin(th));
    for(int i = 0; i < 3; i++) {
      w[i] = f * v[i];
    }
  }
}

void rb_R_from_rotvec(const double w[3], double R[3][3]) {
  double th = sqrt(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]);
  double a = th > 1e-12 ? sin(th) / th : 1.0;
  double b = th > 1e-6 ? (1.0 - cos(th)) / (th * th) : 0.5;
  double K[3][3] = {{0, -w[2], w[1]}, {w[2], 0, -w[0]}, {-w[1], w[0], 0}};
  for(int i = 0; i < 3; i++) {
    for(int j = 0; j < 3; j++) {
      double kk = 0.0;
      for(int m = 0; m < 3; m++) {
        kk += K[i][m] * K[m][j];
      }
      R[i][j] = (i == j) + a * K[i][j] + b * kk;
    }
  }
}

void rb_jacobian(const rb_model *mdl, const double *q, double J[6][RB_MAXJ]) {
  double R[RB_MAXJ][3][3], p[RB_MAXJ][3];
  int n = mdl->n;
  rb_fk(mdl, q, R, p);
  const double *pe = p[n - 1];
  for(int i = 0; i < n; i++) {  // revolute about z of frame i (Craig)
    double z[3] = {R[i][0][2], R[i][1][2], R[i][2][2]};
    double r[3] = {pe[0] - p[i][0], pe[1] - p[i][1], pe[2] - p[i][2]};
    J[0][i] = z[1] * r[2] - z[2] * r[1];
    J[1][i] = z[2] * r[0] - z[0] * r[2];
    J[2][i] = z[0] * r[1] - z[1] * r[0];
    J[3][i] = z[0], J[4][i] = z[1], J[5][i] = z[2];
  }
}

// solve the 6 x 6 system A x = b (Gauss, partial pivoting)
static int solve6(double A[6][6], double *b, double *x) {
  for(int c = 0; c < 6; c++) {
    int pr = c;
    for(int r = c + 1; r < 6; r++) {
      if(fabs(A[r][c]) > fabs(A[pr][c])) {
        pr = r;
      }
    }
    if(fabs(A[pr][c]) < 1e-18) {
      return -1;
    }
    for(int k = 0; k < 6; k++) {
      double t = A[c][k];
      A[c][k] = A[pr][k];
      A[pr][k] = t;
    }
    double t = b[c];
    b[c] = b[pr];
    b[pr] = t;
    for(int r = c + 1; r < 6; r++) {
      double f = A[r][c] / A[c][c];
      for(int k = c; k < 6; k++) {
        A[r][k] -= f * A[c][k];
      }
      b[r] -= f * b[c];
    }
  }
  for(int i = 5; i >= 0; i--) {
    double s = b[i];
    for(int k = i + 1; k < 6; k++) {
      s -= A[i][k] * x[k];
    }
    x[i] = s / A[i][i];
  }
  return 0;
}

int rb_ik(const rb_model *mdl, const double pt[3], const double Rt[3][3], double *q, double tol_p,
          double tol_r, int maxit) {
  int n = mdl->n;
  for(int it = 0; it <= maxit; it++) {
    double p[3], R[3][3], e[6], Re[3][3];
    rb_pose(mdl, q, p, R);
    for(int i = 0; i < 3; i++) {
      e[i] = pt[i] - p[i];
      for(int j = 0; j < 3; j++) {  // Rt R^T: the rotation still to go
        Re[i][j] = Rt[i][0] * R[j][0] + Rt[i][1] * R[j][1] + Rt[i][2] * R[j][2];
      }
    }
    rb_rotvec_from_R(Re, e + 3);
    double ep = sqrt(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
    double er = sqrt(e[3] * e[3] + e[4] * e[4] + e[5] * e[5]);
    if(ep < tol_p && er < tol_r) {
      return it;
    }
    if(it == maxit) {
      break;
    }
    // dq = J^T (J J^T + l^2 I)^-1 e; rotation weighted as 1 rad ~ 1 m
    double J[6][RB_MAXJ], A[6][6], y[6];
    rb_jacobian(mdl, q, J);
    double lam2 = 1e-6 + 1e-3 * (ep + er);  // damping grows with the step
    for(int i = 0; i < 6; i++) {
      for(int j = 0; j < 6; j++) {
        double s = 0.0;
        for(int k = 0; k < n; k++) {
          s += J[i][k] * J[j][k];
        }
        A[i][j] = s + (i == j ? lam2 : 0.0);
      }
    }
    if(solve6(A, e, y)) {
      return -1;
    }
    double maxstep = 0.0, dq[RB_MAXJ];
    for(int k = 0; k < n; k++) {
      dq[k] = 0.0;
      for(int i = 0; i < 6; i++) {
        dq[k] += J[i][k] * y[i];
      }
      maxstep = fabs(dq[k]) > maxstep ? fabs(dq[k]) : maxstep;
    }
    double s = maxstep > 0.3 ? 0.3 / maxstep : 1.0;  // no jumps to another branch
    for(int k = 0; k < n; k++) {
      q[k] += s * dq[k];
    }
  }
  return -1;
}
