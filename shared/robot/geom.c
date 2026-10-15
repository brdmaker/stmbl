/*
 * 3D geometry and distances, see geom.h.
 */
#include "geom.h"
#include "kin.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define D2R (M_PI / 180.0)

static double dot(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static double norm(const double a[3]) {
  return sqrt(dot(a, a));
}

void gm_init(gm_world *w) {
  memset(w, 0, sizeof(*w));
  w->selfskip = 1;
  w->k = 1e5;
  w->c = 500.0;
}

static int nums(const char *p, double *v, int max) {
  int n = 0, off;
  while(n < max && sscanf(p, " %lf%n", &v[n], &off) == 1) {
    p += off;
    n++;
  }
  return n;
}

int gm_line(gm_world *w, const char *line) {
  char kw[16], name[16], type[16];
  int off;
  double v[12];
  if(sscanf(line, " %15s%n", kw, &off) != 1) {
    return 0;
  }
  const char *p = line + off;
  if(!strcmp(kw, "body")) {
    int L, o2;
    if(w->nb >= GM_MAXBODY || sscanf(p, " %d %15s%n", &L, type, &o2) != 2) {
      return -1;
    }
    gm_shape *s = &w->body[w->nb];
    memset(s, 0, sizeof(*s));
    s->link = L;
    snprintf(s->name, sizeof(s->name), "body%d", w->nb);
    int n = nums(p + o2, v, 7);
    if(!strcmp(type, "capsule") && n == 7) {
      s->type = GM_CAPSULE;
      memcpy(s->a, v, 3 * sizeof(double));
      memcpy(s->b, v + 3, 3 * sizeof(double));
      s->r = v[6];
    } else if(!strcmp(type, "sphere") && n == 4) {
      s->type = GM_SPHERE;
      memcpy(s->a, v, 3 * sizeof(double));
      memcpy(s->b, v, 3 * sizeof(double));
      s->r = v[3];
    } else {
      return -1;
    }
    w->nb++;
    return 1;
  }
  if(!strcmp(kw, "obstacle")) {
    int o2;
    if(w->no >= GM_MAXOBS || sscanf(p, " %15s %15s%n", name, type, &o2) != 2) {
      return -1;
    }
    gm_shape *s = &w->obs[w->no];
    memset(s, 0, sizeof(*s));
    s->link = -1;
    snprintf(s->name, sizeof(s->name), "%s", name);
    int n = nums(p + o2, v, 9);
    if(!strcmp(type, "sphere") && n == 4) {
      s->type = GM_SPHERE;
      memcpy(s->a, v, 3 * sizeof(double));
      memcpy(s->b, v, 3 * sizeof(double));
      s->r = v[3];
    } else if(!strcmp(type, "capsule") && n == 7) {
      s->type = GM_CAPSULE;
      memcpy(s->a, v, 3 * sizeof(double));
      memcpy(s->b, v + 3, 3 * sizeof(double));
      s->r = v[6];
    } else if(!strcmp(type, "box") && (n == 6 || n == 9)) {
      s->type = GM_BOX;
      memcpy(s->a, v, 3 * sizeof(double));
      memcpy(s->b, v + 3, 3 * sizeof(double));
      double rpy[3] = {0, 0, 0};
      if(n == 9) {
        rpy[0] = v[6] * D2R, rpy[1] = v[7] * D2R, rpy[2] = v[8] * D2R;
      }
      rb_R_from_rpy(rpy, s->R);
    } else if(!strcmp(type, "plane") && n == 6) {
      s->type = GM_PLANE;
      memcpy(s->a, v, 3 * sizeof(double));
      double l = norm(v + 3);
      if(l <= 0.0) {
        return -1;
      }
      for(int i = 0; i < 3; i++) {
        s->b[i] = v[3 + i] / l;
      }
    } else {
      return -1;
    }
    w->no++;
    return 1;
  }
  if(!strcmp(kw, "selfignore")) {
    int i, j;
    if(sscanf(p, " %d %d", &i, &j) != 2 || i < 0 || j < 0 || i >= GM_MAXBODY || j >= GM_MAXBODY) {
      return -1;
    }
    w->ignore[i][j] = w->ignore[j][i] = 1;
    return 1;
  }
  if(!strcmp(kw, "selfskip")) {
    return sscanf(p, " %d", &w->selfskip) == 1 ? 1 : -1;
  }
  if(!strcmp(kw, "contact")) {
    return sscanf(p, " %lf %lf", &w->k, &w->c) == 2 ? 1 : -1;
  }
  return 0;
}

void gm_update(gm_world *w, const rb_model *mdl, const double *q) {
  double R[RB_MAXJ][3][3], p[RB_MAXJ][3];
  rb_fk(mdl, q, R, p);
  for(int i = 0; i < w->nb; i++) {
    gm_shape *s = &w->body[i];
    if(s->link <= 0 || s->link > mdl->n) {
      memcpy(s->wa, s->a, sizeof(s->wa));
      memcpy(s->wb, s->b, sizeof(s->wb));
      continue;
    }
    const double(*Rl)[3] = R[s->link - 1];
    const double *pl = p[s->link - 1];
    for(int k = 0; k < 3; k++) {
      s->wa[k] = pl[k] + Rl[k][0] * s->a[0] + Rl[k][1] * s->a[1] + Rl[k][2] * s->a[2];
      s->wb[k] = pl[k] + Rl[k][0] * s->b[0] + Rl[k][1] * s->b[1] + Rl[k][2] * s->b[2];
    }
  }
}

// --- signed distance of a point to a shape (core: no radius) ---------------

// box: signed distance and the outward gradient
static double sdf_box(const gm_shape *s, const double x[3], double grad[3]) {
  double l[3], qd[3];
  for(int i = 0; i < 3; i++) {  // into the box frame: R^T (x - c)
    l[i] = s->R[0][i] * (x[0] - s->a[0]) + s->R[1][i] * (x[1] - s->a[1]) + s->R[2][i] * (x[2] - s->a[2]);
    qd[i] = fabs(l[i]) - s->b[i];
  }
  double out[3], g[3] = {0, 0, 0}, d;
  double mx = qd[0] > qd[1] ? (qd[0] > qd[2] ? qd[0] : qd[2]) : (qd[1] > qd[2] ? qd[1] : qd[2]);
  if(mx > 0.0) {  // outside: distance to the clamped point
    for(int i = 0; i < 3; i++) {
      out[i] = qd[i] > 0.0 ? qd[i] : 0.0;
    }
    d = norm(out);
    for(int i = 0; i < 3; i++) {
      g[i] = d > 0.0 ? out[i] / d * (l[i] < 0.0 ? -1.0 : 1.0) : 0.0;
    }
  } else {  // inside: the nearest face
    int k = qd[0] > qd[1] ? (qd[0] > qd[2] ? 0 : 2) : (qd[1] > qd[2] ? 1 : 2);
    d = mx;
    g[k] = l[k] < 0.0 ? -1.0 : 1.0;
  }
  for(int i = 0; i < 3; i++) {  // gradient back to the world
    grad[i] = s->R[i][0] * g[0] + s->R[i][1] * g[1] + s->R[i][2] * g[2];
  }
  return d;
}

static void lerp(const double a[3], const double b[3], double t, double o[3]) {
  for(int i = 0; i < 3; i++) {
    o[i] = a[i] + t * (b[i] - a[i]);
  }
}

// closest points of segments p1-q1 and p2-q2 (Ericson, Real-Time Collision Detection 5.1.9)
static void seg_seg(const double p1[3], const double q1[3], const double p2[3], const double q2[3], double c1[3],
                    double c2[3]) {
  double d1[3], d2[3], r[3];
  for(int i = 0; i < 3; i++) {
    d1[i] = q1[i] - p1[i], d2[i] = q2[i] - p2[i], r[i] = p1[i] - p2[i];
  }
  double a = dot(d1, d1), e = dot(d2, d2), f = dot(d2, r), s, t;
  const double eps = 1e-15;
  if(a <= eps && e <= eps) {
    s = t = 0.0;
  } else if(a <= eps) {
    s = 0.0;
    t = f / e;
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
  } else {
    double c = dot(d1, r);
    if(e <= eps) {
      t = 0.0;
      s = -c / a;
      s = s < 0 ? 0 : (s > 1 ? 1 : s);
    } else {
      double b = dot(d1, d2), den = a * e - b * b;
      s = den != 0.0 ? (b * f - c * e) / den : 0.0;
      s = s < 0 ? 0 : (s > 1 ? 1 : s);
      t = (b * s + f) / e;
      if(t < 0.0) {
        t = 0.0;
        s = -c / a;
        s = s < 0 ? 0 : (s > 1 ? 1 : s);
      } else if(t > 1.0) {
        t = 1.0;
        s = (b - c) / a;
        s = s < 0 ? 0 : (s > 1 ? 1 : s);
      }
    }
  }
  lerp(p1, q1, s, c1);
  lerp(p2, q2, t, c2);
}

// closest pair between two cores (segment/point) given radii
static double core_core(const double a1[3], const double b1[3], double r1, const double a2[3], const double b2[3],
                        double r2, gm_pair *p) {
  double c1[3], c2[3], v[3];
  seg_seg(a1, b1, a2, b2, c1, c2);
  for(int i = 0; i < 3; i++) {
    v[i] = c1[i] - c2[i];
  }
  double l = norm(v);
  double n[3] = {0, 0, 1};
  if(l > 1e-12) {
    for(int i = 0; i < 3; i++) {
      n[i] = v[i] / l;
    }
  }
  memcpy(p->pa, c1, sizeof(c1));
  for(int i = 0; i < 3; i++) {
    p->pb[i] = c2[i] + r2 * n[i];
  }
  memcpy(p->n, n, sizeof(n));
  return l - r1 - r2;
}

double gm_dist(const gm_shape *body, const gm_shape *o, int other_is_body, gm_pair *p) {
  const double *a = body->wa, *b = body->wb;
  double r = body->r;
  gm_pair tmp;
  if(!p) {
    p = &tmp;
  }
  if(other_is_body || o->type == GM_SPHERE || o->type == GM_CAPSULE) {
    const double *oa = other_is_body ? o->wa : o->a, *ob = other_is_body ? o->wb : o->b;
    p->d = core_core(a, b, r, oa, ob, o->r, p);
    return p->d;
  }
  if(o->type == GM_PLANE) {
    double sa = dot(o->b, (double[3]){a[0] - o->a[0], a[1] - o->a[1], a[2] - o->a[2]});
    double sb = dot(o->b, (double[3]){b[0] - o->a[0], b[1] - o->a[1], b[2] - o->a[2]});
    const double *c = sa <= sb ? a : b;
    double s = sa <= sb ? sa : sb;
    memcpy(p->pa, c, 3 * sizeof(double));
    for(int i = 0; i < 3; i++) {
      p->pb[i] = c[i] - s * o->b[i];
      p->n[i] = o->b[i];
    }
    p->d = s - r;
    return p->d;
  }
  // box: the signed distance along the segment is convex: golden section
  double lo = 0.0, hi = 1.0, g[3], x[3];
  const double gr = 0.6180339887498949;
  double t1 = hi - gr * (hi - lo), t2 = lo + gr * (hi - lo), f1, f2;
  lerp(a, b, t1, x);
  f1 = sdf_box(o, x, g);
  lerp(a, b, t2, x);
  f2 = sdf_box(o, x, g);
  for(int it = 0; it < 40; it++) {
    if(f1 < f2) {
      hi = t2, t2 = t1, f2 = f1;
      t1 = hi - gr * (hi - lo);
      lerp(a, b, t1, x);
      f1 = sdf_box(o, x, g);
    } else {
      lo = t1, t1 = t2, f1 = f2;
      t2 = lo + gr * (hi - lo);
      lerp(a, b, t2, x);
      f2 = sdf_box(o, x, g);
    }
  }
  double t = 0.5 * (lo + hi), best = 1e9, bt = t;
  double cand[3] = {0.0, t, 1.0};  // the ends too (flat minima)
  for(int k = 0; k < 3; k++) {
    lerp(a, b, cand[k], x);
    double f = sdf_box(o, x, g);
    if(f < best) {
      best = f, bt = cand[k];
    }
  }
  lerp(a, b, bt, x);
  double d = sdf_box(o, x, g);
  memcpy(p->pa, x, sizeof(x));
  for(int i = 0; i < 3; i++) {
    p->n[i] = g[i];
    p->pb[i] = x[i] - d * g[i];
  }
  p->d = d - r;
  return p->d;
}

double gm_clearance(const gm_world *w, gm_pair *best) {
  double dmin = 1e9;
  gm_pair p;
  for(int i = 0; i < w->nb; i++) {
    for(int j = 0; j < w->no; j++) {
      double d = gm_dist(&w->body[i], &w->obs[j], 0, &p);
      if(d < dmin) {
        dmin = d;
        if(best) {
          *best = p;
          best->body = i, best->other = j, best->self = 0;
        }
      }
    }
    for(int j = i + 1; j < w->nb; j++) {
      int li = w->body[i].link, lj = w->body[j].link;
      if(w->ignore[i][j] || abs(li - lj) <= w->selfskip) {
        continue;
      }
      double d = gm_dist(&w->body[i], &w->body[j], 1, &p);
      if(d < dmin) {
        dmin = d;
        if(best) {
          *best = p;
          best->body = i, best->other = j, best->self = 1;
        }
      }
    }
  }
  return dmin;
}

int gm_contacts(const gm_world *w, gm_pair *out, int max) {
  int n = 0;
  gm_pair p;
  for(int i = 0; i < w->nb; i++) {
    for(int j = 0; j < w->no && n < max; j++) {
      if(gm_dist(&w->body[i], &w->obs[j], 0, &p) < 0.0) {
        p.body = i, p.other = j, p.self = 0;
        out[n++] = p;
      }
    }
  }
  return n;
}
