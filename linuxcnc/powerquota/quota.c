/*
 * Power quotas: fuse models and the feed scale, see quota.h.
 */
#include "quota.h"
#include <math.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

static void add_profile(pq_system *s, const char *name, int n, const double *mt, double mag, double tau) {
  pq_profile *p = &s->prof[s->nprof++];
  memset(p, 0, sizeof(*p));
  snprintf(p->name, sizeof(p->name), "%s", name);
  p->n = n;
  for(int k = 0; k < n; k++) {
    p->m[k] = mt[2 * k];
    p->t[k] = mt[2 * k + 1];
  }
  p->mag = mag;
  p->tau = tau;
}

void pq_init(pq_system *s) {
  memset(s, 0, sizeof(*s));
  s->margin = 0.5;
  s->start = 0.2;
  s->trip = 0.9;
  s->f_min = 0.05;
  s->regen = 0.0;
  s->mag_margin = 0.7;
  s->t_down = 0.02;
  s->t_up = 1.0;
  s->scale = 1.0;
  s->alarm_quota = -1;
  // IEC 60898-1 breakers: no trip at 1.13 In (1 h), trip at 1.45 In within
  // 1 h, 2.55 In within 1..60 s (B, C); the fast edge
  static const double mcb[] = {1.13, 3600, 1.45, 120, 2.0, 15, 2.55, 4, 3.0, 2.5, 4.0, 1.0, 5.0, 0.5, 10.0, 0.1};
  add_profile(s, "B", 8, mcb, 3.0, 300.0);
  add_profile(s, "C", 8, mcb, 5.0, 300.0);
  add_profile(s, "D", 8, mcb, 10.0, 300.0);
  // IEC 60269 gG fuse (pre-arcing, fast edge of a 16 A gG): no trip at
  // 1.25 In, 1.6 In within 1 h
  static const double gg[] = {1.25, 3600, 1.6, 600, 2.0, 20, 3.0, 2.0, 4.0, 0.4, 5.0, 0.15, 10.0, 0.008};
  add_profile(s, "gG", 7, gg, 0.0, 600.0);
}

static int find_profile(const pq_system *s, const char *name) {
  for(int k = 0; k < s->nprof; k++) {
    if(!strcmp(s->prof[k].name, name)) {
      return k;
    }
  }
  return -1;
}

static int find_quota(const pq_system *s, const char *name) {
  for(int k = 0; k < s->nq; k++) {
    if(!strcmp(s->q[k].name, name)) {
      return k;
    }
  }
  return -1;
}

static int nums(const char *p, double *v, int max) {
  int n = 0, off;
  while(n < max && sscanf(p, " %lf%n", &v[n], &off) == 1) {
    p += off;
    n++;
  }
  return n;
}

int pq_line(pq_system *s, const char *line, char *err, int errlen) {
  char buf[512], kw[16], a[16], b[16];
  int off;
  double v[2 * PQ_MAXPT + 1];
  snprintf(buf, sizeof(buf), "%s", line);
  buf[strcspn(buf, "#\r\n")] = 0;
  if(sscanf(buf, " %15s%n", kw, &off) != 1) {
    return 0;
  }
  const char *p = buf + off;
  if(!strcmp(kw, "profile")) {
    int o2, n;
    if(sscanf(p, " %15s%n", a, &o2) != 1 || (n = nums(p + o2, v, 2 * PQ_MAXPT + 1)) < 2 || n % 2 ||
       n > 2 * PQ_MAXPT) {
      snprintf(err, errlen, "profile NAME M1 T1 M2 T2 ... (up to %d points)", PQ_MAXPT);
      return -1;
    }
    for(int k = 0; k < n / 2; k++) {  // rising m, falling t
      if(v[2 * k] <= 1.0 || v[2 * k + 1] <= 0.0 || (k && (v[2 * k] <= v[2 * k - 2] || v[2 * k + 1] >= v[2 * k - 1]))) {
        snprintf(err, errlen, "profile %s: points need m > 1 rising, t > 0 falling", a);
        return -1;
      }
    }
    int k = find_profile(s, a);
    if(k < 0) {
      if(s->nprof >= PQ_MAXP) {
        snprintf(err, errlen, "too many profiles");
        return -1;
      }
      add_profile(s, a, n / 2, v, 0.0, 300.0);
    } else {  // redefine the curve, keep magnetic / cooling
      double mag = s->prof[k].mag, tau = s->prof[k].tau;
      pq_profile *pp = &s->prof[k];
      pp->n = n / 2;
      for(int i = 0; i < n / 2; i++) {
        pp->m[i] = v[2 * i], pp->t[i] = v[2 * i + 1];
      }
      pp->mag = mag, pp->tau = tau;
    }
    return 0;
  }
  if(!strcmp(kw, "magnetic") || !strcmp(kw, "cool")) {
    int k;
    if(sscanf(p, " %15s %lf", a, v) != 2 || (k = find_profile(s, a)) < 0 || v[0] < 0.0 ||
       (kw[0] == 'c' && v[0] <= 0.0)) {
      snprintf(err, errlen, "%s PROFILE VALUE (a known profile)", kw);
      return -1;
    }
    if(kw[0] == 'm') {
      s->prof[k].mag = v[0];
    } else {
      s->prof[k].tau = v[0];
    }
    return 0;
  }
  if(!strcmp(kw, "quota")) {
    int o2, n, k;
    if(sscanf(p, " %15s %15s%n", a, b, &o2) != 2 || (n = nums(p + o2, v, 3)) < 2 || (k = find_profile(s, b)) < 0 ||
       v[0] <= 0.0 || v[1] <= 0.0 || (n == 3 && (v[2] <= 0.0 || v[2] > 1.0)) || s->nq >= PQ_MAXQ ||
       find_quota(s, a) >= 0) {
      snprintf(err, errlen, "quota NAME PROFILE IN VOLT [PF] (a known profile, a new name, at most %d)", PQ_MAXQ);
      return -1;
    }
    pq_quota *q = &s->q[s->nq++];
    memset(q, 0, sizeof(*q));
    snprintf(q->name, sizeof(q->name), "%s", a);
    q->prof = k;
    q->in = v[0];
    q->volt = v[1];
    q->pf = n == 3 ? v[2] : 0.9;
    q->want = 1.0;
    return 0;
  }
  if(!strcmp(kw, "load")) {
    int d, k;
    if(sscanf(p, " %15s %d %lf", a, &d, v) != 3 || (k = find_quota(s, a)) < 0 || d < 0 || d >= PQ_MAXD ||
       v[0] < 0.0) {
      snprintf(err, errlen, "load QUOTA DRIVE SHARE (a known quota, drive 0..%d)", PQ_MAXD - 1);
      return -1;
    }
    s->q[k].share[d] = v[0];
    s->nd = d + 1 > s->nd ? d + 1 : s->nd;
    return 0;
  }
  if(!strcmp(kw, "drives")) {
    int d;
    if(sscanf(p, " %d", &d) != 1 || d < 1 || d > PQ_MAXD) {
      snprintf(err, errlen, "drives N (1..%d)", PQ_MAXD);
      return -1;
    }
    s->nd = d > s->nd ? d : s->nd;
    return 0;
  }
  if(!strcmp(kw, "rates")) {
    if(nums(p, v, 2) != 2 || v[0] <= 0.0 || v[1] <= 0.0) {
      snprintf(err, errlen, "rates TDOWN TUP [s]");
      return -1;
    }
    s->t_down = v[0], s->t_up = v[1];
    return 0;
  }
  static const struct {
    const char *kw;
    size_t off;
    double lo, hi;
  } scalars[] = {
      {"margin", offsetof(pq_system, margin), 0.01, 1.0}, {"start", offsetof(pq_system, start), 0.0, 1.0},
      {"trip", offsetof(pq_system, trip), 0.01, 1.0},     {"fmin", offsetof(pq_system, f_min), 0.0, 1.0},
      {"regen", offsetof(pq_system, regen), 0.0, 1.0},    {"magmargin", offsetof(pq_system, mag_margin), 0.1, 1.0},
  };
  for(size_t k = 0; k < sizeof(scalars) / sizeof(scalars[0]); k++) {
    if(!strcmp(kw, scalars[k].kw)) {
      if(nums(p, v, 1) != 1 || v[0] < scalars[k].lo || v[0] > scalars[k].hi) {
        snprintf(err, errlen, "%s %g..%g", kw, scalars[k].lo, scalars[k].hi);
        return -1;
      }
      *(double *)((char *)s + scalars[k].off) = v[0];
      return 0;
    }
  }
  snprintf(err, errlen, "unknown line: %s", kw);
  return -1;
}

int pq_file(pq_system *s, const char *path, char *err, int errlen) {
  FILE *f = fopen(path, "r");
  char line[512], e[200];
  int ln = 0;
  if(!f) {
    snprintf(err, errlen, "cannot read %s", path);
    return -1;
  }
  while(fgets(line, sizeof(line), f)) {
    ln++;
    if(pq_line(s, line, e, sizeof(e))) {
      snprintf(err, errlen, "%s:%d: %s", path, ln, e);
      fclose(f);
      return -1;
    }
  }
  fclose(f);
  if(s->nq < 1 || s->nd < 1) {
    snprintf(err, errlen, "%s: no quota / no load lines", path);
    return -1;
  }
  if(s->start >= s->margin || s->margin >= s->trip) {
    snprintf(err, errlen, "%s: need start < margin < trip", path);
    return -1;
  }
  return 0;
}

double pq_trip_time(const pq_profile *p, double m) {
  if(p->n < 1 || m < p->m[0]) {
    return 1e30;
  }
  if(p->mag > 0.0 && m >= p->mag) {
    return 0.0;
  }
  int k = 0;
  while(k + 1 < p->n && m > p->m[k + 1]) {
    k++;
  }
  if(k + 1 >= p->n) {  // beyond the last point: on along the last segment's slope
    if(p->n < 2) {
      return p->t[0];
    }
    k = p->n - 2;
  }
  double x = (log(m) - log(p->m[k])) / (log(p->m[k + 1]) - log(p->m[k]));
  return exp(log(p->t[k]) + x * (log(p->t[k + 1]) - log(p->t[k])));
}

void pq_reset(pq_system *s) {
  s->alarm = 0;
  s->alarm_quota = -1;
}

void pq_step(pq_system *s, const double *p_drive, double dt) {
  double want = 1.0;
  s->total = 0.0;
  for(int d = 0; d < s->nd; d++) {
    s->total += p_drive[d];
  }
  for(int k = 0; k < s->nq; k++) {
    pq_quota *q = &s->q[k];
    const pq_profile *pr = &s->prof[q->prof];
    double pos = 0.0, neg = 0.0;
    for(int d = 0; d < s->nd; d++) {
      double x = q->share[d] * p_drive[d];
      if(x >= 0.0) {
        pos += x;
      } else {
        neg -= x;
      }
    }
    q->p = pos - s->regen * neg;
    q->p = q->p > 0.0 ? q->p : 0.0;
    q->i = q->p / (q->volt * q->pf);
    q->m = q->i / q->in;
    double t = pq_trip_time(pr, q->m);
    if(t < 1e29) {  // heating
      q->damage += t > 0.0 ? dt / t : 1.0;
    } else {  // cooling
      q->damage -= q->damage * dt / pr->tau;
    }
    q->damage = q->damage < 0.0 ? 0.0 : q->damage;
    // what it asks for: by its damage ...
    double w = 1.0;
    if(q->damage > s->start) {
      w = 1.0 - (1.0 - s->f_min) * (q->damage - s->start) / (s->margin - s->start);
    }
    // ... and away from the magnetic trip
    if(pr->mag > 0.0 && q->m > s->mag_margin * pr->mag) {
      double wm = s->scale * s->mag_margin * pr->mag / q->m;
      w = wm < w ? wm : w;
    }
    w = w < s->f_min ? s->f_min : (w > 1.0 ? 1.0 : w);
    q->want = w;
    want = w < want ? w : want;
    if(q->damage >= s->trip && !s->alarm) {
      s->alarm = 1;
      s->alarm_quota = k;
    }
  }
  double tc = want < s->scale ? s->t_down : s->t_up;
  s->scale += (want - s->scale) * (dt / (tc + dt));
  s->limiting = s->scale < 0.999;
}
