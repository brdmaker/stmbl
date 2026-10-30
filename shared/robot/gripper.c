/*
 * Electric finger gripper and payload measurement, see gripper.h.
 */
#include "gripper.h"
#include "kin.h"
#include <math.h>
#include <string.h>

void rb_gripper_init(rb_gripper *g) {
  memset(g, 0, sizeof(*g));
  g->gap_open = 80.0;
  g->gap_min = 0.0;
  g->v = 40.0;
  g->e_contact = 1.0;
  g->squeeze = 1.2;
  g->w_tol = 1.0;
  g->lost_tol = 0.8;
  g->state = GR_OPEN;
  g->cmd = g->gap_open;
}

static void set_state(rb_gripper *g, int s) {
  if(g->state != s) {
    g->state = s;
    g->events++;
  }
}

void rb_gripper_step(rb_gripper *g, int close, double gap, double dt) {
  if(!close) {
    if(g->state != GR_OPEN && g->state != GR_OPENING) {
      set_state(g, GR_OPENING);
      g->cmd = gap > g->cmd ? gap : g->cmd;  // from where the fingers are
    }
    if(g->state == GR_OPENING) {
      g->cmd += g->v * dt;
      if(g->cmd >= g->gap_open) {
        g->cmd = g->gap_open;
        if(gap > g->gap_open - g->e_contact) {
          set_state(g, GR_OPEN);
        }
      }
    }
    return;
  }
  switch(g->state) {
    case GR_OPEN:
    case GR_OPENING:
      set_state(g, GR_CLOSING);
      g->cmd = gap < g->cmd ? gap : g->cmd;
      // fall through
    case GR_CLOSING:
      if(gap - g->cmd > g->e_contact && gap > g->gap_min + g->w_tol) {  // blocked: contact
        g->width = gap;
        g->cmd = gap - g->squeeze;
        int fits = g->w_expect > 0.0 ? fabs(gap - g->w_expect) <= g->w_tol : 1;
        set_state(g, fits ? GR_GRIPPED : GR_WRONG);
        break;
      }
      g->cmd -= g->v * dt;
      if(g->cmd <= g->gap_min) {
        g->cmd = g->gap_min;
        if(gap <= g->gap_min + g->w_tol) {
          g->width = gap;
          set_state(g, GR_EMPTY);
        }
      }
      break;
    case GR_GRIPPED:
      if(gap < g->width - g->lost_tol) {  // the fingers closed past the part
        set_state(g, GR_LOST);
      }
      break;
    default:  // WRONG, EMPTY, LOST: hold until opened
      break;
  }
}

int rb_flange_wrench(const rb_model *mdl, const double *q, const double *r, double F[6]) {
  // J^T F = r, least squares: F = (J J^T + l I)^-1 J r
  int n = mdl->n;
  double J[6][RB_MAXJ], A[6][7];
  rb_jacobian(mdl, q, J);
  for(int i = 0; i < 6; i++) {
    double b = 0.0;
    for(int j = 0; j < n; j++) {
      b += J[i][j] * r[j];
    }
    for(int k = 0; k < 6; k++) {
      double s = 0.0;
      for(int j = 0; j < n; j++) {
        s += J[i][j] * J[k][j];
      }
      A[i][k] = s + (i == k ? 1e-10 : 0.0);
    }
    A[i][6] = b;
  }
  for(int c = 0; c < 6; c++) {  // Gauss, partial pivoting
    int p = c;
    for(int k = c + 1; k < 6; k++) {
      if(fabs(A[k][c]) > fabs(A[p][c])) {
        p = k;
      }
    }
    if(fabs(A[p][c]) < 1e-15) {
      return -1;
    }
    for(int k = 0; k < 7; k++) {
      double t = A[c][k];
      A[c][k] = A[p][k];
      A[p][k] = t;
    }
    for(int k = c + 1; k < 6; k++) {
      double f = A[k][c] / A[c][c];
      for(int m = c; m < 7; m++) {
        A[k][m] -= f * A[c][m];
      }
    }
  }
  for(int i = 5; i >= 0; i--) {
    double s = A[i][6];
    for(int k = i + 1; k < 6; k++) {
      s -= A[i][k] * F[k];
    }
    F[i] = s / A[i][i];
  }
  return 0;
}

void rb_grip_payload_init(rb_grip_payload *p) {
  memset(p, 0, sizeof(*p));
  p->settle = 0.3;
  p->measure = 0.3;
  p->v_min = 0.15;
  p->share = 0.3;
  p->tol_abs = 1.0;
  p->tol_rel = 0.15;
}

void rb_grip_payload_step(rb_grip_payload *p, const rb_model *mdl, int event, const double *q, const double *qd,
                          const double *r, double dt) {
  p->update = 0;
  if(event) {
    p->state = GP_SETTLE;
    p->t = p->t_meas = 0.0;
    p->sr_sum = p->ss_sum = 0.0;
    p->mute = 1;
    p->m_target = event > 0 ? p->m_expect : 0.0;
    p->learn = event > 0 && p->m_expect <= 0.0;
    p->m_model = p->m_target;  // the model takes the part (or loses it) now
    p->update = 1;
    if(event < 0) {  // released: nothing to weigh (the model's floor, up to
      p->state = GP_IDLE;  // ~0.8 kg, is too close to the tolerance to
      p->mute = 0;         // tell a part stuck to a finger)
      return;
    }
  }
  if(p->state != GP_SETTLE && p->state != GP_MEASURE) {
    return;
  }
  p->t += dt;
  if(p->state == GP_SETTLE && p->t >= p->settle) {
    p->state = GP_MEASURE;
    p->mute = p->learn;
  }
  if(p->state != GP_MEASURE) {
    return;
  }
  // the gravity torque per kg of a mass at the expected centre of mass:
  // s_j = g . (Jv_j + Jw_j x d), d from the flange origin
  int n = mdl->n;
  double J[6][RB_MAXJ], R[RB_MAXJ][3][3], o[RB_MAXJ][3], d[3];
  rb_jacobian(mdl, q, J);
  rb_fk(mdl, q, R, o);
  for(int i = 0; i < 3; i++) {
    d[i] = R[n - 1][i][0] * p->c_expect[0] + R[n - 1][i][1] * p->c_expect[1] + R[n - 1][i][2] * p->c_expect[2];
  }
  double ss_all = 0.0, ss = 0.0, sr = 0.0;
  for(int j = 0; j < n; j++) {
    double v[3] = {J[0][j] + J[4][j] * d[2] - J[5][j] * d[1], J[1][j] + J[5][j] * d[0] - J[3][j] * d[2],
                   J[2][j] + J[3][j] * d[1] - J[4][j] * d[0]};
    double sj = mdl->g[0] * v[0] + mdl->g[1] * v[1] + mdl->g[2] * v[2];
    ss_all += sj * sj;
    if(fabs(qd[j]) >= p->v_min) {  // a standing joint's friction hides the weight
      ss += sj * sj;
      sr += sj * r[j];
    }
  }
  // the missing mass: r = dm s, least squares over the moving joints and the
  // samples (the joints carrying most of the weight count most); a sample
  // when the moving joints carry a good part of it
  if(ss_all <= 0.0 || ss < p->share * ss_all) {
    return;
  }
  p->sr_sum += sr;
  p->ss_sum += ss;
  p->t_meas += dt;
  if(p->t_meas < p->measure || p->ss_sum <= 0.0) {
    return;
  }
  p->m_meas = p->m_model + p->sr_sum / p->ss_sum;
  p->mute = 0;
  if(p->learn) {
    p->m_model = p->m_meas > 0.0 ? p->m_meas : 0.0;
    p->update = 1;
    p->state = GP_OK;
    return;
  }
  double tol = p->tol_rel * p->m_target;
  tol = tol > p->tol_abs ? tol : p->tol_abs;
  p->state = fabs(p->m_meas - p->m_target) <= tol ? GP_OK : GP_MISMATCH;
}
