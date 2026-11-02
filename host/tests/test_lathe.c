/* linuxcnc/lathe's turret and spindle logic (lathe_logic.c): the turret against a simulated one (a
 * closed-loop stepper following its command, an index at a random angle), the spindle's turns from
 * wrapped feedback and its index */
#include "lathe_logic.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static double idx0 = 0.371, width = 0.01, slip_add = 0;
static int broken = 0;
static int index_at(double fb) { double f = fb + slip_add - idx0; f -= floor(f); return !broken && f < width; }

/* a change to tool; returns the time it took (or -1: a fault), the least command seen going backwards */
static double change(turret_t *t, int tool, double *back) {
  double dt = 0.001, time = 0, last = t->cmd, backs = 0;
  for (int i = 0; i < 40000; i++) {
    int done = turret_step(t, dt, 1, tool, index_at(t->cmd), t->cmd);
    if (t->cmd < last - 1e-12) backs += last - t->cmd;
    last = t->cmd; time += dt;
    if (t->state == TUR_FAULT) return -1;
    if (done) break;
  }
  for (int i = 0; i < 3; i++) turret_step(t, dt, 0, tool, index_at(t->cmd), t->cmd);  /* the request drops */
  if (back) *back = backs;
  return time;
}
static double station_angle(const turret_t *t, int n) { double a = idx0 + t->offset + (n - 1.0) / t->stations; return a - floor(a); }
static double frac(double v) { return v - floor(v); }

static void test_turret(void) {
  turret_t t = {.stations = 6, .offset = 0.05, .overtravel = 0.03, .lock = 0.01, .speed = 1.0, .lock_speed = 0.2, .acc = 20, .dwell = 0.2, .timeout = 8};
  turret_init(&t);
  double back;
  double tm = change(&t, 3, &back);
  CHECK(tm > 0 && t.homed && t.station == 3, "T3 first: homes on the index, at station 3 (t %.2f homed %d station %d err %s)", tm, t.homed, t.station, turret_error(t.err));
  /* the index is sampled each period: its angle within a period's turn (1 ms at 1 turn/s) */
  CHECK(fabs(frac(t.cmd + t.lock) - station_angle(&t, 3)) < 0.0015, "locked %.4f short of station 3's angle: %.5f vs %.5f", t.lock, frac(t.cmd + t.lock), station_angle(&t, 3));
  CHECK(fabs(back - (t.overtravel + t.lock)) < 1e-6, "backwards only for the lock: %.5f", back);
  CHECK(fabs(t.index_pos - idx0) < 0.002, "the index found at %.4f (it is at %.4f)", t.index_pos, idx0);
  double c0 = t.cmd;
  tm = change(&t, 3, NULL);
  CHECK(tm >= 0 && tm < 0.01 && t.cmd == c0, "T3 again: done at once, no move (%.3f s)", tm);
  tm = change(&t, 2, &back);
  CHECK(tm > 0 && t.station == 2 && t.cmd > c0 + 0.6, "T2 (behind it): forward most of a turn (%.3f turns)", t.cmd - c0);
  CHECK(fabs(frac(t.cmd + t.lock) - station_angle(&t, 2)) < 0.0015 && fabs(back - 0.04) < 1e-6, "station 2, locked; back only the lock (%.5f vs %.5f, back %.4f)", frac(t.cmd + t.lock), station_angle(&t, 2), back);
  tm = change(&t, 7, NULL);
  CHECK(tm > 0 && t.station == 1, "T7 on 6 stations: station 1 (%d)", t.station);
  tm = change(&t, 0, NULL);
  CHECK(tm >= 0 && t.station == 1, "T0: nothing to do");
  /* the turret slipped 0.05 turn: the next index it passes, where it shouldn't be (to station 1 it does) */
  slip_add = 0.05;
  tm = change(&t, 6, NULL);
  CHECK(tm > 0, "to station 6: no index on the way, no fault yet");
  tm = change(&t, 1, NULL);
  CHECK(tm < 0 && t.err == TUR_E_SLIP && !t.homed, "a slip: a fault (%s), unhomed", turret_error(t.err));
  slip_add = 0;
  /* no index: broken sensor */
  turret_init(&t); broken = 1;
  tm = change(&t, 2, NULL);
  CHECK(tm < 0 && t.err == TUR_E_NOINDEX && t.cmd >= 1.49, "no index: a fault after 1.5 turns (%s, %.2f)", turret_error(t.err), t.cmd);
  broken = 0;
  /* too slow: the timeout */
  turret_init(&t); t.speed = 0.05; t.timeout = 2;
  tm = change(&t, 5, NULL);
  CHECK(tm < 0 && t.err == TUR_E_TIMEOUT, "a timeout (%s)", turret_error(t.err));
}

static void test_spindle(void) {
  lspindle_t s; lspindle_init(&s, 1.0);
  double dt = 0.001, motor = 0.3, pc = 0;
  int ie = 0;
  double w = lspindle_cmd(&s, dt, 10.0, &pc);
  CHECK(fabs(w - 10 * 6.283185307179586) < 1e-9, "10 turns/s: %.3f rad/s", w);
  for (int i = 0; i < 5000; i++) { lspindle_cmd(&s, dt, 10.0, &pc); CHECK(pc >= -M_PI - 1e-9 && pc <= M_PI + 1e-9, "pos_cmd wrapped"); if (pc < -M_PI - 1e-9 || pc > M_PI + 1e-9) break; }
  /* 50 turns/s for 2 s, the feedback wrapped to ±pi */
  double revs = 0;
  for (int i = 0; i <= 2000; i++) { double fb = remainder(motor + 50 * 6.283185307179586 * i * dt, 6.283185307179586); revs = lspindle_fb(&s, fb, 0, &ie); }
  CHECK(fabs(revs - 100) < 1e-6, "turns from wrapped feedback: %.6f (100)", revs);
  /* index on the motor's zero: cleared there, the turns from 0 */
  ie = 1; double r0 = revs, at = -1;
  for (int i = 2001; i <= 2100 && at < 0; i++) { double fb = remainder(motor + 50 * 6.283185307179586 * i * dt, 6.283185307179586); revs = lspindle_fb(&s, fb, 0, &ie); if (!ie) at = revs; }
  CHECK(at >= 0 && at < 0.06 && r0 > 99, "index on the motor's zero: cleared, %.4f turns after it", at);
  /* a sensor at 1.5 motor turns a spindle turn */
  lspindle_t q; lspindle_init(&q, 1.5); q.use_sensor = 1;
  double sens0 = 0.2;  /* the sensor's spindle angle */
  ie = 0; double rv = 0;
  for (int i = 0; i <= 3000; i++) {
    double sp = 8.0 * i * dt, fb = remainder(sp * 1.5 * 6.283185307179586, 6.283185307179586);
    int sensor = frac(sp - sens0) < 0.01;
    if (i == 1000) ie = 1;
    rv = lspindle_fb(&q, fb, sensor, &ie);
  }
  double expect = 8.0 * 3.0 - (sens0 + ceil(8.0 * 1.0 - sens0));  /* turns since the first sensor edge after t 1 s */
  CHECK(!ie && fabs(rv - expect) < 0.01, "a sensor, ratio 1.5: %.4f turns since its edge (%.4f)", rv, expect);
}

int main(void) {
  test_turret();
  test_spindle();
  printf("test_lathe: %s\n", fails ? "FAILED" : "ok");
  return fails ? 1 : 0;
}
