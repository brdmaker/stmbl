/*
 * Tool table, TCP kinematics and the tool changer's monitor, see tool.h.
 */
#include "tool.h"
#include "kin.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define D2R (M_PI / 180.0)

void rb_tools_init(rb_tools *tt) {
  memset(tt, 0, sizeof(*tt));
  for(int i = 0; i < RB_MAXTOOL; i++) {
    for(int k = 0; k < 3; k++) {
      tt->t[i].tcp_R[k][k] = 1.0;
      tt->t[i].dock_R[k][k] = 1.0;
    }
  }
  snprintf(tt->t[0].name, sizeof(tt->t[0].name), "flange");
}

static int nums(const char *p, double *v, int max) {
  int n = 0, off;
  while(n < max && sscanf(p, " %lf%n", &v[n], &off) == 1) {
    p += off;
    n++;
  }
  return n;
}

int rb_tools_line(rb_tools *tt, const char *line) {
  char kw[16], name[16];
  int off, id;
  double v[12];
  if(sscanf(line, " %15s%n", kw, &off) != 1) {
    return 0;
  }
  const char *p = line + off;
  rb_tool *t = &tt->t[tt->cur];
  if(!strcmp(kw, "tool")) {
    if(sscanf(p, " %d %15s", &id, name) != 2 || id < 1 || id >= RB_MAXTOOL) {
      return -1;
    }
    tt->cur = id;
    t = &tt->t[id];
    t->id = id;
    snprintf(t->name, sizeof(t->name), "%s", name);
    return 1;
  }
  if(!strcmp(kw, "tcp")) {
    if(tt->cur < 1 || nums(p, v, 6) != 6) {
      return -1;
    }
    double rpy[3] = {v[3] * D2R, v[4] * D2R, v[5] * D2R};
    for(int k = 0; k < 3; k++) {
      t->tcp_p[k] = v[k] / 1000.0;
    }
    rb_R_from_rpy(rpy, t->tcp_R);
    return 1;
  }
  if(!strcmp(kw, "toolmass")) {
    if(tt->cur < 1 || nums(p, v, 4) != 4) {
      return -1;
    }
    t->m = v[0];
    memcpy(t->c, v + 1, sizeof(t->c));
    return 1;
  }
  if(!strcmp(kw, "dock")) {
    int n = tt->cur < 1 ? 0 : nums(p, v, 12);
    if(n != 9 && n != 12) {
      return -1;
    }
    double rpy[3] = {v[3] * D2R, v[4] * D2R, v[5] * D2R};
    for(int k = 0; k < 3; k++) {
      t->dock_p[k] = v[k] / 1000.0;
      t->approach[k] = v[6 + k] / 1000.0;
      t->slide[k] = v[n == 12 ? 9 + k : 6 + k] / 1000.0;
    }
    rb_R_from_rpy(rpy, t->dock_R);
    t->has_dock = 1;
    return 1;
  }
  return 0;
}

static int tools_file(rb_tools *tt, const char *path, char *err, int errlen, int depth) {
  FILE *f = fopen(path, "r");
  if(!f || depth > 4) {
    snprintf(err, errlen, "cannot read %s", path);
    if(f) {
      fclose(f);
    }
    return -1;
  }
  char line[512];
  int ln = 0;
  while(fgets(line, sizeof(line), f)) {
    ln++;
    line[strcspn(line, "#")] = 0;
    char name[256], inc[768];
    if(sscanf(line, " include %255s", name) == 1) {
      const char *sl = strrchr(path, '/');
      if(name[0] == '/' || !sl) {
        snprintf(inc, sizeof(inc), "%s", name);
      } else {
        snprintf(inc, sizeof(inc), "%.*s/%s", (int)(sl - path), path, name);
      }
      if(tools_file(tt, inc, err, errlen, depth + 1)) {
        fclose(f);
        return -1;
      }
    } else if(rb_tools_line(tt, line) < 0) {
      snprintf(err, errlen, "%s:%d: bad tool line", path, ln);
      fclose(f);
      return -1;
    }
  }
  fclose(f);
  return 0;
}

int rb_tools_file(rb_tools *tt, const char *path, char *err, int errlen) {
  return tools_file(tt, path, err, errlen, 0);
}

const rb_tool *rb_tool_get(const rb_tools *tt, int id) {
  return id >= 1 && id < RB_MAXTOOL && tt->t[id].id == id ? &tt->t[id] : &tt->t[0];
}

void rb_tcp_pose(const rb_model *mdl, const rb_tool *t, const double *q, double p[3], double R[3][3]) {
  double pf[3], Rf[3][3];
  rb_pose(mdl, q, pf, Rf);
  for(int i = 0; i < 3; i++) {
    p[i] = pf[i];
    for(int k = 0; k < 3; k++) {
      p[i] += Rf[i][k] * t->tcp_p[k];
      R[i][k] = Rf[i][0] * t->tcp_R[0][k] + Rf[i][1] * t->tcp_R[1][k] + Rf[i][2] * t->tcp_R[2][k];
    }
  }
}

void rb_flange_from_tcp(const rb_tool *t, const double p[3], const double R[3][3], double pf[3],
                        double Rf[3][3]) {
  // Rf = R Rt^T, pf = p - Rf pt
  for(int i = 0; i < 3; i++) {
    for(int k = 0; k < 3; k++) {
      Rf[i][k] = R[i][0] * t->tcp_R[k][0] + R[i][1] * t->tcp_R[k][1] + R[i][2] * t->tcp_R[k][2];
    }
  }
  for(int i = 0; i < 3; i++) {
    pf[i] = p[i] - (Rf[i][0] * t->tcp_p[0] + Rf[i][1] * t->tcp_p[1] + Rf[i][2] * t->tcp_p[2]);
  }
}

int rb_ik_tcp(const rb_model *mdl, const rb_tool *t, const double p[3], const double R[3][3], double *q,
              double tol_p, double tol_r, int maxit) {
  double pf[3], Rf[3][3];
  rb_flange_from_tcp(t, p, R, pf, Rf);
  return rb_ik(mdl, pf, Rf, q, tol_p, tol_r, maxit);
}

void rb_changer_init(rb_changer *c) {
  memset(c, 0, sizeof(*c));
  c->t_switch = 0.5;
  c->t_debounce = 0.01;
  c->dock_tol_p = 0.002;
  c->dock_tol_r = 0.035;
}

void rb_changer_reset(rb_changer *c) {
  c->fault = CH_OK;
  c->t_lost = 0.0;
  c->check = 0;
  c->mounted = c->s_present ? c->s_id : 0;  // after a fault: what is there now
}

int rb_in_dock(const rb_changer *c, const rb_tools *tt, int id, const double pf[3], const double Rf[3][3]) {
  const rb_tool *t = rb_tool_get(tt, id);
  if(!t->id || !t->has_dock) {
    return 0;
  }
  double dp = 0.0, Rr[3][3], w[3];
  for(int i = 0; i < 3; i++) {
    dp += (pf[i] - t->dock_p[i]) * (pf[i] - t->dock_p[i]);
    for(int k = 0; k < 3; k++) {  // Rd^T Rf
      Rr[i][k] = t->dock_R[0][i] * Rf[0][k] + t->dock_R[1][i] * Rf[1][k] + t->dock_R[2][i] * Rf[2][k];
    }
  }
  rb_rotvec_from_R(Rr, w);
  return sqrt(dp) <= c->dock_tol_p && sqrt(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]) <= c->dock_tol_r;
}

static void fault(rb_changer *c, int f) {
  if(!c->fault) {
    c->fault = f;
  }
}

void rb_changer_step(rb_changer *c, const rb_tools *tt, int en, const double pf[3], const double Rf[3][3],
                     double dt) {
  // the valve follows the request, but never lets a tool go outside its dock
  int want = c->lock_req;
  if(!want && c->valve && c->mounted && !rb_in_dock(c, tt, c->mounted, pf, Rf)) {
    if(en) {
      fault(c, CH_UNDOCKED);
    }
    want = 1;
  }
  if(want != c->valve) {
    c->valve = want;
    c->t = 0.0;
    c->check = want && !c->mounted;  // locking: see what it took
  }
  c->t += dt;
  int settled = c->valve ? (c->s_locked && !c->s_unlocked) : (c->s_unlocked && !c->s_locked);
  if(!settled && c->t > c->t_switch && en) {
    fault(c, CH_TIMEOUT);
  }
  if(c->valve && settled && c->check) {
    c->mounted = c->s_present ? c->s_id : 0;
    c->check = 0;
    if(c->mounted != c->expect && en) {
      fault(c, CH_WRONG);  // locked onto another tool (or none)
    }
  } else if(!c->valve && settled) {
    c->mounted = 0;  // released (in its dock)
  }
  // a mounted tool must stay locked and present
  if(c->mounted && c->valve && c->t > c->t_switch) {
    int ok = c->s_locked && c->s_present && c->s_id == c->mounted;
    c->t_lost = ok ? 0.0 : c->t_lost + dt;
    if(c->t_lost > c->t_debounce && en) {
      fault(c, CH_LOST);
    }
  } else {
    c->t_lost = 0.0;
  }
}
