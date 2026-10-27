/*
 * Simulated pneumatic tool changer, see changersim.h.
 */
#include "changersim.h"
#include "kin.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

void cs_init(cs_changer *c) {
  memset(c, 0, sizeof(*c));
  c->stroke = 0.1;
  rb_tools_init(&c->tt);
}

int cs_line(cs_changer *c, const char *line) {
  double s;
  int id = 0;
  char kw[16];
  if(sscanf(line, " %15s", kw) != 1) {
    return 0;
  }
  if(!strcmp(kw, "changer")) {
    int n = sscanf(line, " changer %lf %d", &s, &id);
    if(n < 1 || s <= 0.0) {
      return -1;
    }
    c->on = 1;
    c->stroke = s;
    c->mounted = n == 2 && id > 0 && id < RB_MAXTOOL ? id : 0;
    c->state[c->mounted] = c->mounted ? CS_MOUNTED : CS_DOCKED;
    c->valve = c->mounted != 0;
    c->x = c->valve ? 1.0 : 0.0;
    return 1;
  }
  return rb_tools_line(&c->tt, line);
}

// within 2 mm / 2 deg of tool id's dock
static int at_dock(const cs_changer *c, int id, const double R6[3][3], const double p6[3]) {
  const rb_tool *t = &c->tt.t[id];
  if(t->id != id || !t->has_dock) {
    return 0;
  }
  double d = 0.0, Rr[3][3], w[3];
  for(int i = 0; i < 3; i++) {
    d += (p6[i] - t->dock_p[i]) * (p6[i] - t->dock_p[i]);
    for(int k = 0; k < 3; k++) {
      Rr[i][k] = t->dock_R[0][i] * R6[0][k] + t->dock_R[1][i] * R6[1][k] + t->dock_R[2][i] * R6[2][k];
    }
  }
  rb_rotvec_from_R(Rr, w);
  return sqrt(d) < 0.002 && sqrt(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]) < 0.035;
}

int cs_step(cs_changer *c, const double R6[3][3], const double p6[3], double dt) {
  if(!c->on) {
    return 0;
  }
  double x0 = c->x;
  c->x += (c->valve ? 1.0 : -1.0) * dt / c->stroke;
  c->x = c->x < 0.0 ? 0.0 : (c->x > 1.0 ? 1.0 : c->x);
  int ev = 0;
  if(x0 < 0.5 && c->x >= 0.5 && !c->mounted) {  // locking: capture a docked tool
    for(int i = 1; i < RB_MAXTOOL; i++) {
      if(c->tt.t[i].id == i && c->state[i] == CS_DOCKED && at_dock(c, i, R6, p6)) {
        c->mounted = i;
        c->state[i] = CS_MOUNTED;
        ev = +1;
        break;
      }
    }
  } else if(x0 >= 0.5 && c->x < 0.5 && c->mounted) {  // unlocking: let it go
    int i = c->mounted;
    c->state[i] = at_dock(c, i, R6, p6) ? CS_DOCKED : CS_DROPPED;
    c->mounted = 0;
    ev = -1;
  }
  if(c->slip && c->mounted) {  // injected: the tool comes off the plate
    c->state[c->mounted] = CS_DROPPED;
    c->mounted = 0;
    ev = -1;
  }
  c->slip = 0;
  return ev;
}

int cs_locked(const cs_changer *c) {
  return c->x > 0.95;
}

int cs_unlocked(const cs_changer *c) {
  return c->x < 0.05;
}

int cs_present(const cs_changer *c) {
  return c->mounted != 0;
}

int cs_id(const cs_changer *c) {
  return c->mounted;
}
