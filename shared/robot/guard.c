/*
 * Collision reaction: stop, back off, estop. See guard.h.
 */
#include "guard.h"
#include "kin.h"
#include "geom.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

void rb_guard_init(rb_guard *g) {
  memset(g, 0, sizeof(*g));
  g->backoff = 0.02;
  g->t_backoff = 0.4;
  g->hold = 0.1;
  g->v_moving = 0.005;
  for(int j = 0; j < RB_MAXJ; j++) {
    g->a_stop[j] = 5.0;
  }
}

static double smooth(double s) {
  return s <= 0.0 ? 0.0 : s >= 1.0 ? 1.0 : s * s * s * (10.0 + s * (-15.0 + 6.0 * s));
}

// solve the 6 x 6 system A x = b in place (partial pivoting); -1 if singular
static int solve6(double A[6][6], double *b, double *x) {
  for(int c = 0; c < 6; c++) {
    int p = c;
    for(int r = c + 1; r < 6; r++) {
      if(fabs(A[r][c]) > fabs(A[p][c])) {
        p = r;
      }
    }
    if(fabs(A[p][c]) < 1e-18) {
      return -1;
    }
    for(int k = 0; k < 6; k++) {
      double t = A[c][k];
      A[c][k] = A[p][k];
      A[p][k] = t;
    }
    double t = b[c];
    b[c] = b[p];
    b[p] = t;
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

void rb_guard_trigger(rb_guard *g, const rb_model *mdl, int reason, const double *q, const double *qd,
                      const double *r) {
  if(g->state != GD_PASS) {
    return;
  }
  int n = mdl->n;
  g->reason = reason;
  g->state = GD_STOP;
  g->t = 0.0;
  memcpy(g->q0, q, sizeof(double) * n);
  memcpy(g->qd0, qd, sizeof(double) * n);
  memcpy(g->q, q, sizeof(double) * n);
  memcpy(g->qd, qd, sizeof(double) * n);
  g->t_stop = 0.0;
  for(int j = 0; j < n; j++) {  // all joints stop together
    double t = fabs(qd[j]) / g->a_stop[j];
    g->t_stop = t > g->t_stop ? t : g->t_stop;
  }
  // backoff direction: back along the path when moving, else along the
  // contact force on the flange
  memset(g->dir, 0, sizeof(g->dir));
  if(reason != GD_COLLISION || g->backoff <= 0.0) {
    return;
  }
  double J[6][RB_MAXJ], v[3] = {0, 0, 0};
  rb_jacobian(mdl, q, J);
  for(int i = 0; i < 3; i++) {
    for(int j = 0; j < n; j++) {
      v[i] += J[i][j] * qd[j];
    }
  }
  double vl = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if(vl > g->v_moving) {
    for(int i = 0; i < 3; i++) {
      g->dir[i] = -v[i] / vl;
    }
  } else if(r) {  // F = (J J^T + l I)^-1 J r
    double A[6][6], b[6], F[6];
    for(int i = 0; i < 6; i++) {
      b[i] = 0.0;
      for(int j = 0; j < n; j++) {
        b[i] += J[i][j] * r[j];
      }
      for(int k = 0; k < 6; k++) {
        double s = 0.0;
        for(int j = 0; j < n; j++) {
          s += J[i][j] * J[k][j];
        }
        A[i][k] = s + (i == k ? 1e-4 : 0.0);
      }
    }
    if(!solve6(A, b, F)) {
      double fl = sqrt(F[0] * F[0] + F[1] * F[1] + F[2] * F[2]);
      if(fl > 1e-6) {
        for(int i = 0; i < 3; i++) {
          g->dir[i] = F[i] / fl;
        }
      }
    }
  }
}

void rb_guard_step(rb_guard *g, const rb_model *mdl, double dt) {
  int n = mdl->n;
  if(g->state == GD_PASS || g->state == GD_DONE) {
    return;
  }
  g->t += dt;
  if(g->state == GD_STOP) {
    double t = g->t < g->t_stop ? g->t : g->t_stop;
    for(int j = 0; j < n; j++) {  // q0 + qd0 t - qd0 t^2 / (2 T)
      double a = g->t_stop > 0.0 ? g->qd0[j] / g->t_stop : 0.0;
      g->q[j] = g->q0[j] + g->qd0[j] * t - 0.5 * a * t * t;
      g->qd[j] = g->qd0[j] - a * t;
    }
    if(g->t >= g->t_stop) {
      for(int j = 0; j < n; j++) {
        g->qd[j] = 0.0;
      }
      g->t = 0.0;
      int back = g->reason == GD_COLLISION && g->backoff > 0.0 &&
                 (g->dir[0] != 0.0 || g->dir[1] != 0.0 || g->dir[2] != 0.0);
      g->state = back ? GD_BACKOFF : GD_HOLD;
      rb_pose(mdl, g->q, g->p0, g->R0);
    }
    return;
  }
  if(g->state == GD_BACKOFF) {
    double s = smooth(g->t / g->t_backoff), p[3], qn[RB_MAXJ];
    for(int i = 0; i < 3; i++) {
      p[i] = g->p0[i] + s * g->backoff * g->dir[i];
    }
    memcpy(qn, g->q, sizeof(double) * n);
    if(rb_ik(mdl, p, g->R0, qn, 1e-7, 1e-7, 50) < 0) {
      g->ik_fail++;
      g->state = GD_HOLD;
      g->t = 0.0;
      for(int j = 0; j < n; j++) {
        g->qd[j] = 0.0;
      }
      return;
    }
    for(int j = 0; j < n; j++) {
      g->qd[j] = (qn[j] - g->q[j]) / dt;
      g->q[j] = qn[j];
    }
    if(g->t >= g->t_backoff) {
      g->state = GD_HOLD;
      g->t = 0.0;
      for(int j = 0; j < n; j++) {
        g->qd[j] = 0.0;
      }
    }
    return;
  }
  if(g->state == GD_HOLD && g->t >= g->hold) {
    g->state = GD_DONE;
    g->estop = 1;
  }
}

void rb_guard_release(rb_guard *g) {
  g->state = GD_PASS;
  g->reason = GD_NONE;
}

void rb_guard_reset(rb_guard *g) {
  g->estop = 0;
  if(g->state == GD_DONE) {
    g->state = GD_PASS;
  }
}

int rb_guard_file(rb_guard *g, rb_model *mdl, gm_world *geo, const char *path, char *err, int errlen) {
  FILE *f = fopen(path, "r");
  if(!f) {
    snprintf(err, errlen, "cannot read %s", path);
    return -1;
  }
  char line[512];
  int ln = 0;
  while(fgets(line, sizeof(line), f)) {
    ln++;
    char *h = strchr(line, '#'), kw[16];
    int off;
    if(h) {
      *h = 0;
    }
    if(sscanf(line, " %15s%n", kw, &off) != 1) {
      continue;
    }
    int r = geo ? gm_line(geo, line) : 0, j;
    double a, b, c, d;
    if(r < 0) {
      snprintf(err, errlen, "%s:%d: bad geometry line", path, ln);
      fclose(f);
      return -1;
    } else if(r > 0) {
      continue;
    }
    if(!strcmp(kw, "dh") && mdl && sscanf(line + off, " %d %lf %lf %lf %lf", &j, &a, &b, &c, &d) == 5 && j >= 1 &&
       j <= RB_MAXJ) {
      rb_link *l = &mdl->link[j - 1];
      l->a = a, l->alpha = b * M_PI / 180.0, l->d = c, l->theta0 = d * M_PI / 180.0;
      mdl->n = j > mdl->n ? j : mdl->n;
    } else if(!strcmp(kw, "backoff")) {
      double v[2] = {20, 0};
      int n = sscanf(line + off, " %lf %lf", v, v + 1);
      g->backoff = n >= 1 ? v[0] / 1000.0 : g->backoff;
      g->t_backoff = n == 2 && v[1] > 0.05 ? v[1] : g->t_backoff;
    } else if(!strcmp(kw, "astop") && sscanf(line + off, " %d %lf", &j, &a) == 2 && j >= 1 && j <= RB_MAXJ && a > 0) {
      g->a_stop[j - 1] = a * M_PI / 180.0;
    }
  }
  fclose(f);
  return 0;
}
