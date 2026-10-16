/*
 * Unit tests of linuxcnc/powerquota/quota.c: the fuse models and the feed
 * scale, against a synthetic machine. Prints TAP.
 */
#include "quota.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ntest, nfail;

static void ok(int cond, const char *name, double got, double lim) {
  ntest++;
  if(!cond) {
    nfail++;
  }
  printf("%s %d - quota: %s\n", cond ? "ok" : "not ok", ntest, name);
  if(!cond) {
    printf("#   got %g, limit %g\n", got, lim);
  }
}

static void cfg(pq_system *s, const char *text) {
  char buf[2048], err[200];
  snprintf(buf, sizeof(buf), "%s", text);
  for(char *l = strtok(buf, "\n"); l; l = strtok(NULL, "\n")) {
    if(pq_line(s, l, err, sizeof(err))) {
      printf("# config: %s: %s\n", l, err);
      exit(1);
    }
  }
}

// a machine: drive d draws base + peak * pulse(t) at feed scale 1; the
// power of motion goes with the feed as scale^1.5 (speed lower, the same
// accelerations for shorter), the base (holding, idle) does not
static double pulse(double t, double period, double duty, double phase) {
  double x = fmod(t / period + phase, 1.0);
  return x < duty ? 1.0 : 0.0;
}

int main(void) {
  pq_system s;
  pq_init(&s);
  int c = 1;  // C
  ok(pq_trip_time(&s.prof[c], 1.0) > 1e29 && fabs(pq_trip_time(&s.prof[c], 2.0) - 15.0) < 1e-9 &&
         fabs(pq_trip_time(&s.prof[c], 2.55) - 4.0) < 1e-9 && pq_trip_time(&s.prof[c], 5.0) == 0.0 &&
         pq_trip_time(&s.prof[0], 3.0) == 0.0 && fabs(pq_trip_time(&s.prof[3], 1.6) - 600) < 1e-9,
     "profiles: C never at In, 15 s at 2 In, 4 s at 2.55 In, instant at 5 In; B instant at 3; gG 600 s at 1.6",
     pq_trip_time(&s.prof[c], 2.0), 15);
  double t25 = pq_trip_time(&s.prof[c], 2.25);
  ok(t25 < 15.0 && t25 > 4.0, "  ...log-log between the points (2.25 In)", t25, 0);

  // a constant overload, no feed reaction (fmin 1): the damage reaches 1
  // at the curve's trip time
  pq_init(&s);
  cfg(&s, "quota L1 C 16 230 1\nload L1 0 1\nfmin 1\n");
  double p2 = 2.0 * 16 * 230, t = 0.0, t_alarm = -1, t_blow = -1;
  while(t < 20.0) {
    pq_step(&s, &p2, 0.001);
    t += 0.001;
    if(s.alarm && t_alarm < 0) {
      t_alarm = t;
    }
    if(s.q[0].damage >= 1.0 && t_blow < 0) {
      t_blow = t;
    }
  }
  ok(fabs(t_blow - 15.0) < 0.01 && fabs(t_alarm - 13.5) < 0.01,
     "constant 2 In on C: modelled blow at 15 s, alarm (0.9) at 13.5 s", t_blow, 15);
  double d0 = s.q[0].damage, zero = 0.0;
  for(int k = 0; k < 300000; k++) {
    pq_step(&s, &zero, 0.001);
  }
  ok(fabs(s.q[0].damage - d0 * exp(-1.0)) < 0.01 * d0, "  ...cooling with tau (300 s: 1/e)", s.q[0].damage,
     d0 * exp(-1.0));

  // six 2 kW drives on one 3 kW supply (C 16 A, 230 V, pf 0.9: 3.3 kW):
  // bursts of 2 kW per drive, staggered, 40 % duty, + 50 W each holding
  pq_init(&s);
  cfg(&s, "quota L1 C 16 230 0.9\ndrives 6\nload L1 0 1\nload L1 1 1\nload L1 2 1\nload L1 3 1\nload L1 4 1\n"
          "load L1 5 1\n");
  double p[6], dmax = 0, smin = 1, work = 0, peak = 0;
  t = 0.0;
  for(int k = 0; k < 600000; k++) {  // 10 min
    for(int d = 0; d < 6; d++) {
      p[d] = 50.0 + 2000.0 * pulse(t, 1.2, 0.4, d / 6.0) * pow(s.scale, 1.5);
    }
    pq_step(&s, p, 0.001);
    t += 0.001;
    dmax = s.q[0].damage > dmax ? s.q[0].damage : dmax;
    smin = s.scale < smin ? s.scale : smin;
    peak = s.q[0].m > peak ? s.q[0].m : peak;
    work += s.scale * 0.001;
  }
  printf("# 6 x 2 kW on 3 kW: damage max %.3f, feed min %.3f, mean %.3f, load peak %.2f In\n", dmax, smin, work / t,
         peak);
  ok(dmax < s.margin + 0.05 && !s.alarm, "6 x 2 kW bursts on a 3 kW C 16 A supply: the fuse stays under the margin",
     dmax, s.margin);
  ok(smin < 0.9 && work / t > 0.4, "  ...by slowing (feed min < 0.9), still working (mean feed > 0.4)", smin, 0.9);
  ok(peak < 0.7 * 5.0, "  ...current peaks kept off the magnetic trip (< 0.7 x 5 In)", peak, 3.5);

  // the same drives, two per phase on three 16 A phases: one phase busy
  pq_init(&s);
  cfg(&s, "quota L1 C 16 230 0.9\nquota L2 C 16 230 0.9\nquota L3 C 16 230 0.9\n"
          "load L1 0 1\nload L1 1 1\nload L2 2 1\nload L2 3 1\nload L3 4 1\nload L3 5 1\n");
  dmax = 0, smin = 1;
  double d2 = 0, d3 = 0;
  t = 0.0;
  for(int k = 0; k < 300000; k++) {
    for(int d = 0; d < 6; d++) {
      double peakw = d < 2 ? 4500.0 : 1000.0;  // J1, J2 on L1: hard work
      p[d] = 50.0 + peakw * pulse(t, 1.0, 0.6, d / 6.0) * pow(s.scale, 1.5);
    }
    pq_step(&s, p, 0.001);
    t += 0.001;
    dmax = s.q[0].damage > dmax ? s.q[0].damage : dmax;
    d2 = s.q[1].damage > d2 ? s.q[1].damage : d2;
    d3 = s.q[2].damage > d3 ? s.q[2].damage : d3;
    smin = s.scale < smin ? s.scale : smin;
  }
  ok(dmax < s.margin + 0.05 && dmax > s.start && d2 < 1e-9 && d3 < 1e-9 && smin < 0.9 && !s.alarm,
     "two drives per phase: the busy phase L1 limits, L2 / L3 untouched", dmax, s.margin);

  // a three-phase drive: a third of its power on each phase
  pq_init(&s);
  cfg(&s, "quota L1 C 10 230 1\nquota L2 C 10 230 1\nquota L3 C 10 230 1\n"
          "load L1 0 0.3333333\nload L2 0 0.3333333\nload L3 0 0.3333333\n");
  double p3 = 3 * 230 * 10 * 1.5;
  pq_step(&s, &p3, 0.001);
  ok(fabs(s.q[0].m - 1.5) < 1e-4 && fabs(s.q[1].m - 1.5) < 1e-4 && fabs(s.q[2].m - 1.5) < 1e-4,
     "a three-phase drive: 1/3 on each phase (1.5 In each)", s.q[0].m, 1.5);

  // a load that does not fall with the feed (holding, stalled): alarm
  // before the fuse blows
  pq_init(&s);
  cfg(&s, "quota L1 B 10 230 1\nload L1 0 1\n");
  double hold = 2.5 * 10 * 230;
  t = 0.0, t_alarm = -1;
  double dmx = 0;
  while(t < 30.0) {
    pq_step(&s, &hold, 0.001);
    t += 0.001;
    dmx = s.q[0].damage > dmx ? s.q[0].damage : dmx;
    if(s.alarm && t_alarm < 0) {
      t_alarm = t;
      break;
    }
  }
  ok(t_alarm > 0 && dmx < 0.91 && fabs(s.scale - s.f_min) < 0.01 && s.alarm_quota == 0,
     "static 2.5 In (feed cannot help): feed at the minimum, alarm before the fuse (damage 0.9)", t_alarm, 0);
  pq_reset(&s);
  ok(!s.alarm, "  ...reset clears the alarm", s.alarm, 0);

  // regeneration: with a common DC bus it offsets the others
  pq_init(&s);
  cfg(&s, "quota L1 C 16 230 1\nload L1 0 1\nload L1 1 1\nregen 1\n");
  double pr[2] = {2000, -800};
  pq_step(&s, pr, 0.001);
  double p_bus = s.q[0].p;
  pq_init(&s);
  cfg(&s, "quota L1 C 16 230 1\nload L1 0 1\nload L1 1 1\n");
  pq_step(&s, pr, 0.001);
  ok(fabs(p_bus - 1200) < 1e-9 && fabs(s.q[0].p - 2000) < 1e-9,
     "regeneration: offsets with regen 1 (1200 W), not with separate rectifiers (2000 W)", p_bus, 1200);

  // a step towards the magnetic trip: the feed falls within ~0.1 s
  pq_init(&s);
  cfg(&s, "quota L1 C 16 230 1\nload L1 0 1\n");
  t = 0.0;
  double t_cut = -1;
  for(int k = 0; k < 500; k++) {
    double pm = 4.5 * 16 * 230 * s.scale;  // the power falls with the feed
    pq_step(&s, &pm, 0.001);
    t += 0.001;
    if(s.q[0].m < 0.7 * 5 + 0.05 && t_cut < 0) {
      t_cut = t;
    }
  }
  ok(t_cut > 0 && t_cut < 0.1, "a jump to 4.5 In on C (magnetic 5): under 0.7 x 5 In within 0.1 s", t_cut, 0.1);

  // config: a profile of one's own, errors
  pq_init(&s);
  char err[200];
  int bad = 0;
  bad += pq_line(&s, "profile slow 1.3 7200 2 120 6 5", err, sizeof(err)) != 0;
  bad += pq_line(&s, "magnetic slow 8", err, sizeof(err)) != 0;
  bad += pq_line(&s, "cool slow 900", err, sizeof(err)) != 0;
  bad += pq_line(&s, "quota main slow 25 400 0.85", err, sizeof(err)) != 0;
  bad += pq_line(&s, "load main 3 0.333", err, sizeof(err)) != 0;
  bad += pq_line(&s, "quota x nosuch 10 230", err, sizeof(err)) != -1;
  bad += pq_line(&s, "profile y 2 10 1.5 100", err, sizeof(err)) != -1;  // m falling
  bad += pq_line(&s, "load nosuch 0 1", err, sizeof(err)) != -1;
  bad += pq_line(&s, "frobnicate 1", err, sizeof(err)) != -1;
  ok(!bad && s.nq == 1 && s.nd == 4 && s.prof[s.q[0].prof].mag == 8 && fabs(pq_trip_time(&s.prof[4], 2) - 120) < 1e-9,
     "config: own profile (curve, magnetic, cooling), quota, load; bad lines refused", bad, 0);

  printf("1..%d\n# %d/%d passed\n", ntest, ntest - nfail, ntest);
  return nfail ? 1 : 0;
}
