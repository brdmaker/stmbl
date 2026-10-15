/*
 * Simulated electric finger gripper, see gripsim.h.
 */
#include "gripsim.h"
#include "kin.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define D2R (M_PI / 180.0)

void gs_init(gs_gripper *g) {
  memset(g, 0, sizeof(*g));
  g->lead = 4.0;
  g->gap_max = 80.0;
  g->mf = 0.3;
  g->jm = 1e-5;
  g->fc = 0.01;
  g->jaw_z = 0.15;
  g->jaw_len = 0.06;
  g->jaw_w = 0.03;
  g->k = 2e6;
  g->c = 200.0;
  g->hold = 20.0;
  g->part = -1;
  g->active = 1;
}

int gs_line(gs_gripper *g, const char *line) {
  char kw[16], sub[16], name[16];
  double v[10];
  int off;
  if(sscanf(line, " %15s%n", kw, &off) != 1) {
    return 0;
  }
  const char *p = line + off;
  if(!strcmp(kw, "gripper")) {
    if(sscanf(p, " %15s%n", sub, &off) != 1) {
      return -1;
    }
    p += off;
    if(!strcmp(sub, "mech")) {
      if(sscanf(p, " %lf %lf %lf %lf %lf", v, v + 1, v + 2, v + 3, v + 4) != 5) {
        return -1;
      }
      g->lead = v[0], g->gap_max = v[1], g->mf = v[2], g->jm = v[3], g->fc = v[4];
      return 1;
    }
    if(!strcmp(sub, "tool")) {
      return sscanf(p, " %d", &g->tool) == 1 ? 1 : -1;
    }
    if(!strcmp(sub, "jaw")) {
      if(sscanf(p, " %lf %lf %lf", v, v + 1, v + 2) != 3) {
        return -1;
      }
      g->jaw_z = v[0], g->jaw_len = v[1], g->jaw_w = v[2];
      return 1;
    }
    return 0;  // "gripper drive" is armcore's
  }
  if(!strcmp(kw, "partmass")) {  // partmass NAME M: another part of the same shape
    for(int i = 0; i < g->np; i++) {
      if(sscanf(p, " %15s %lf", name, v) == 2 && !strcmp(name, g->parts[i].name)) {
        g->parts[i].mass = v[0];
        return 1;
      }
    }
    return -1;
  }
  if(!strcmp(kw, "part")) {
    int n = 0, o2;
    if(g->np >= GS_MAXPART || sscanf(p, " %15s%n", name, &o2) != 1) {
      return -1;
    }
    p += o2;
    while(n < 10 && sscanf(p, " %lf%n", &v[n], &o2) == 1) {
      p += o2;
      n++;
    }
    if(n != 7 && n != 10) {
      return -1;
    }
    gs_part *pt = &g->parts[g->np++];
    memset(pt, 0, sizeof(*pt));
    snprintf(pt->name, sizeof(pt->name), "%s", name);
    memcpy(pt->c, v, 3 * sizeof(double));
    memcpy(pt->h, v + 3, 3 * sizeof(double));
    pt->mass = v[6];
    double rpy[3] = {0, 0, 0};
    if(n == 10) {
      rpy[0] = v[7] * D2R, rpy[1] = v[8] * D2R, rpy[2] = v[9] * D2R;
    }
    rb_R_from_rpy(rpy, pt->R);
    return 1;
  }
  return 0;
}

double gs_gap(const gs_gripper *g) {
  return 2.0 * g->lead * g->th / (2.0 * M_PI);
}

void gs_set_gap(gs_gripper *g, double gap_mm) {
  g->th = gap_mm / (2.0 * g->lead) * 2.0 * M_PI;
  g->w = 0.0;
}

int gs_held(const gs_gripper *g) {
  for(int i = 0; i < g->np; i++) {
    if(g->parts[i].held) {
      return i;
    }
  }
  return -1;
}

void gs_drop(gs_gripper *g) {
  int i = gs_held(g);
  if(i >= 0) {
    g->parts[i].held = 0;
    g->parts[i].dropped = 1;
  }
}

// the part between the jaws and its width along the flange x [mm]; -1 none
static int in_jaws(const gs_gripper *g, const double R6[3][3], const double p6[3], double *width) {
  for(int i = 0; i < g->np; i++) {
    const gs_part *pt = &g->parts[i];
    if(pt->dropped) {
      continue;
    }
    double c6[3], ext[3];
    for(int k = 0; k < 3; k++) {  // centre in the flange frame
      c6[k] = R6[0][k] * (pt->c[0] - p6[0]) + R6[1][k] * (pt->c[1] - p6[1]) + R6[2][k] * (pt->c[2] - p6[2]);
    }
    for(int k = 0; k < 3; k++) {  // its half extent along the flange axes
      ext[k] = 0.0;
      for(int a = 0; a < 3; a++) {
        double ax = R6[0][k] * pt->R[0][a] + R6[1][k] * pt->R[1][a] + R6[2][k] * pt->R[2][a];
        ext[k] += fabs(ax) * pt->h[a];
      }
    }
    if(pt->held || (fabs(c6[0]) < 0.005 && fabs(c6[1]) < ext[1] + g->jaw_w / 2 &&
                    fabs(c6[2] - g->jaw_z) < ext[2] + g->jaw_len / 2 && 2.0 * ext[0] * 1000.0 < g->gap_max)) {
      *width = 2.0 * ext[0] * 1000.0;
      return i;
    }
  }
  return -1;
}

int gs_step(gs_gripper *g, double te, const double R6[3][3], const double p6[3], double dt) {
  int ev = 0;
  double r = g->lead / 1000.0 / (2.0 * M_PI);  // finger travel per rad [m]
  double J = g->jm + 2.0 * g->mf * r * r;
  double gap = gs_gap(g), w;
  // a held part follows the flange
  for(int i = 0; i < g->np; i++) {
    gs_part *pt = &g->parts[i];
    if(pt->held) {
      for(int a = 0; a < 3; a++) {
        pt->c[a] = p6[a] + R6[a][0] * pt->c6[0] + R6[a][1] * pt->c6[1] + R6[a][2] * pt->c6[2];
        for(int b = 0; b < 3; b++) {
          pt->R[a][b] = R6[a][0] * pt->R6[0][b] + R6[a][1] * pt->R6[1][b] + R6[a][2] * pt->R6[2][b];
        }
      }
    }
  }
  g->part = g->active ? in_jaws(g, R6, p6, &w) : -1;
  g->force = 0.0;
  double t = te;
  if(g->part >= 0 && gap < w) {  // each finger squeezes the part
    double f = g->k * (w - gap) / 2000.0 - g->c * g->w * r;
    g->force = f > 0.0 ? f : 0.0;
    t += 2.0 * g->force * r;
  }
  if(gap < 0.0) {  // end stops
    t += 1e3 * (0.0 - gap) * r;
  } else if(gap > g->gap_max) {
    t -= 1e3 * (gap - g->gap_max) * r;
  }
  double wp = g->w + dt * t / J, dw = dt * g->fc / J;
  g->w = fabs(wp) <= dw ? 0.0 : wp - (wp > 0 ? dw : -dw);
  g->th += dt * g->w;
  // hold / release
  if(g->part >= 0) {
    gs_part *pt = &g->parts[g->part];
    if(!pt->held && g->force > g->hold) {
      pt->held = 1;
      for(int k = 0; k < 3; k++) {
        pt->c6[k] = R6[0][k] * (pt->c[0] - p6[0]) + R6[1][k] * (pt->c[1] - p6[1]) + R6[2][k] * (pt->c[2] - p6[2]);
        for(int b = 0; b < 3; b++) {
          pt->R6[k][b] = R6[0][k] * pt->R[0][b] + R6[1][k] * pt->R[1][b] + R6[2][k] * pt->R[2][b];
        }
      }
      ev = +1;
    } else if(pt->held && gs_gap(g) > w + 0.5) {
      pt->held = 0;
      ev = -1;
    }
  }
  return ev;
}
