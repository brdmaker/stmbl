/* lathe_logic: the turret's sequence and the spindle's turns (see lathe_logic.h) */
#include "lathe_logic.h"

#include <math.h>

#define TWO_PI 6.283185307179586

void turret_init(turret_t *t) {
  turret_t c = *t;  /* keep the set-up */
  t->state = TUR_IDLE; t->err = TUR_OK; t->homed = 0; t->station = 0; t->target = 0; t->last_index = 0;
  t->cmd = 0; t->vel = 0; t->goal = 0; t->t = 0; t->t_all = 0; t->index_pos = 0;
  t->stations = c.stations > 0 ? c.stations : 6;
  if (t->speed <= 0) t->speed = 1.0;
  if (t->lock_speed <= 0) t->lock_speed = 0.2;
  if (t->acc <= 0) t->acc = 10.0;
  if (t->timeout <= 0) t->timeout = 10.0;
  if (t->slip <= 0) t->slip = 0.02;
}

int turret_station(const turret_t *t, int tool) {
  if (tool <= 0) return 0;
  return (tool - 1) % t->stations + 1;
}

const char *turret_error(int err) {
  switch (err) {
    case TUR_E_NOINDEX: return "no index within 1.5 turns";
    case TUR_E_SLIP: return "the index where it shouldn't be: slipped or lost steps";
    case TUR_E_TIMEOUT: return "the change took too long";
    case TUR_E_STATION: return "no such station";
    default: return "";
  }
}

/* toward t->goal with the speed limit v, the acceleration limit, stopping on it; 1 when there */
static int move(turret_t *t, double dt, double v) {
  double rem = t->goal - t->cmd;
  if (fabs(rem) < 1e-9 && fabs(t->vel) < 1e-6) { t->cmd = t->goal; t->vel = 0; return 1; }
  double vmax = sqrt(2.0 * t->acc * fabs(rem));      /* to stop on the goal */
  double want = (rem > 0 ? 1 : -1) * fmin(v, vmax);
  double dv = t->acc * dt;
  t->vel += fmax(-dv, fmin(dv, want - t->vel));
  double step = t->vel * dt;
  if ((rem > 0 && step >= rem) || (rem < 0 && step <= rem)) { t->cmd = t->goal; t->vel = 0; return 1; }
  t->cmd += step;
  return 0;
}
/* the station's angle at or after p (the lock: never back to it) */
static double station_at(const turret_t *t, int n, double p) {
  double a = t->index_pos + t->offset + (double)(n - 1) / t->stations;
  return a + ceil(p - a + 1e-9);
}

int turret_step(turret_t *t, double dt, int request, int tool, int index, double fb) {
  int edge = index && !t->last_index;
  t->last_index = index;
  if (t->state != TUR_IDLE && t->state != TUR_DONE && t->state != TUR_FAULT) {
    t->t_all += dt;
    if (t->t_all > t->timeout) { t->state = TUR_FAULT; t->err = TUR_E_TIMEOUT; t->vel = 0; }
  }
  if (edge && t->homed && t->state != TUR_HOME && t->vel > 0) {  /* every index after homing, forward: where it should be? */
    double d = fb - t->index_pos, e = d - floor(d + 0.5);
    if (fabs(e) > t->slip) { t->state = TUR_FAULT; t->err = TUR_E_SLIP; t->homed = 0; t->vel = 0; }
  }
  switch (t->state) {
    case TUR_IDLE:
      if (!request) break;
      t->target = turret_station(t, tool);
      t->t_all = 0;
      if (t->target == 0 || (t->homed && t->target == t->station)) { t->state = TUR_DONE; break; }
      if (!t->homed) { t->state = TUR_HOME; t->goal = t->cmd + 1.5; break; }
      t->goal = station_at(t, t->target, t->cmd) + t->overtravel; t->state = TUR_FWD;
      break;
    case TUR_HOME:
      if (edge) {
        t->index_pos = fb; t->homed = 1;
        t->goal = station_at(t, t->target, t->cmd) + t->overtravel; t->state = TUR_FWD;
        break;
      }
      if (move(t, dt, t->speed)) { t->state = TUR_FAULT; t->err = TUR_E_NOINDEX; }
      break;
    case TUR_FWD:
      if (move(t, dt, t->speed)) { t->goal = t->cmd - t->overtravel - t->lock; t->state = TUR_LOCK; }
      break;
    case TUR_LOCK:
      if (move(t, dt, t->lock_speed)) { t->state = TUR_DWELL; t->t = 0; }
      break;
    case TUR_DWELL:
      t->t += dt;
      if (t->t >= t->dwell) { t->station = t->target; t->state = TUR_DONE; }
      break;
    case TUR_DONE:
      if (!request) t->state = TUR_IDLE;
      break;
    case TUR_FAULT:  /* until set up again (unhome: the HAL glue's reset) */
      t->vel = 0;
      break;
  }
  return t->state == TUR_DONE;
}

void lspindle_init(lspindle_t *s, double ratio) {
  s->ratio = ratio > 0 ? ratio : 1.0; s->use_sensor = 0; s->init = 0; s->last_sensor = 0; s->last_fb = 0; s->acc = 0; s->origin = 0; s->rev0 = 0; s->cmd_angle = 0;
}

double lspindle_cmd(lspindle_t *s, double dt, double speed, double *pos_cmd) {
  double w = speed * s->ratio * TWO_PI;  /* motor rad/s */
  s->cmd_angle = remainder(s->cmd_angle + w * dt, TWO_PI);
  if (pos_cmd) *pos_cmd = s->cmd_angle;
  return w;
}

double lspindle_fb(lspindle_t *s, double fb, int sensor, int *index_enable) {
  if (!s->init) { s->init = 1; s->last_fb = fb; s->last_sensor = sensor; s->acc = 0; s->origin = remainder(fb, TWO_PI); }
  double d = remainder(fb - s->last_fb, TWO_PI);  /* a period's turn is under half a turn: wrapped or not */
  double before = s->acc;
  s->acc += d;
  s->last_fb = fb;
  int edge = sensor && !s->last_sensor;
  s->last_sensor = sensor;
  if (index_enable && *index_enable) {
    int at = 0;
    if (s->use_sensor) at = edge;
    else at = floor((s->origin + before) / TWO_PI) != floor((s->origin + s->acc) / TWO_PI);  /* the motor's zero angle passed */
    if (at) {
      double a = s->origin + s->acc;  /* the motor's angle, multi-turn */
      double z = s->use_sensor ? s->acc : TWO_PI * (d >= 0 ? floor(a / TWO_PI) : ceil(a / TWO_PI)) - s->origin;
      s->rev0 = z / (TWO_PI * s->ratio);
      *index_enable = 0;
    }
  }
  return s->acc / (TWO_PI * s->ratio) - s->rev0;
}
