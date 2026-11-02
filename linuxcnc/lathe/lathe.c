/*
 * lathe: a lathe's turret and spindle for LinuxCNC (the logic: lathe_logic.c; README.md for the wiring).
 *
 *   loadrt lathe turret=1 spindle=1
 *   addf lathe.turret servo-thread
 *   addf lathe.spindle servo-thread
 *
 * The turret (an index-homed, reverse-locked turret on a stepper: a stepgen in position mode, turns):
 *   lathe.turret.change      in   iocontrol.0.tool-change
 *   lathe.turret.tool        in   iocontrol.0.tool-prep-number (s32): station (tool - 1) % stations + 1
 *   lathe.turret.changed     out  -> iocontrol.0.tool-changed
 *   lathe.turret.index       in   the sensor: one pulse a turret turn
 *   lathe.turret.pos-cmd     out  -> the stepgen's position-cmd [turret turns]
 *   lathe.turret.pos-fb      in   <- its position-fb
 *   lathe.turret.enable      in / lathe.turret.enable-out out: the stepgen's enable (off while faulted)
 *   lathe.turret.chain-in    in   the E-STOP chain ... lathe.turret.chain-out out ... and no turret fault
 *   lathe.turret.reset       in   rising edge: clears a fault (and the homing: the next change homes)
 *   lathe.turret.homed, busy, fault out;  station, state, error out (s32)
 *   set up (pins: setp): stations (s32, 6), offset, overtravel, lock [turns], speed, lock-speed [turns/s],
 *   acc [turns/s^2], dwell, timeout [s], slip [turns]
 *   lathe.turret.sim-index   out  a simulated index from pos-fb at sim-index-at [turns] (a simulation)
 *
 * The spindle (an stmbl drive in velocity mode, smart-serial or CAN):
 *   lathe.spindle.speed-rps  in   spindle.0.speed-out-rps;   lathe.spindle.on in  spindle.0.on
 *   lathe.spindle.vel-cmd    out  -> the drive's vel_cmd [motor rad/s];  pos-cmd out: its angle, wrapped
 *   lathe.spindle.pos-fb     in   <- the drive's pos_fb [motor rad];  vel-fb in <- its vel_fb [rad/s]
 *   lathe.spindle.revs       out  -> spindle.0.revs;  speed-fb out -> spindle.0.speed-in [turns/s]
 *   lathe.spindle.index-enable io <=> spindle.0.index-enable;  index-sensor in: a 1/turn sensor
 *   lathe.spindle.at-speed   out  -> spindle.0.at-speed (within at-speed-tol of the command)
 *   set up: ratio (motor turns a spindle turn, 1), use-sensor (bit), at-speed-tol (0.05)
 */
#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"
#include "rtapi_math.h"

#include <string.h>

#include "lathe_logic.h"

MODULE_AUTHOR("stmbl");
MODULE_DESCRIPTION("a lathe's index-homed, reverse-locked turret and its stmbl spindle");
MODULE_LICENSE("GPL");

static int turret = 1, spindle = 1;
RTAPI_MP_INT(turret, "1: the turret's function");
RTAPI_MP_INT(spindle, "1: the spindle's function");

typedef struct {
  hal_bit_t *change, *changed, *index, *enable, *enable_out, *chain_in, *chain_out, *reset, *homed, *busy, *fault, *sim_index;
  hal_s32_t *tool, *station, *state, *error, *stations;
  hal_float_t *pos_cmd, *pos_fb, *offset, *overtravel, *lock, *speed, *lock_speed, *acc, *dwell, *timeout, *slip, *sim_at;
  int last_reset;
  turret_t t;
} tpins_t;

typedef struct {
  hal_float_t *speed, *vel_cmd, *pos_cmd, *pos_fb, *vel_fb, *revs, *speed_fb, *ratio, *tol;
  hal_bit_t *on, *index_enable, *sensor, *use_sensor, *at_speed;
  lspindle_t s;
} spins_t;

static int comp_id;
static tpins_t *tp;
static spins_t *sp;

static void turret_fn(void *arg, long period) {
  tpins_t *p = arg;
  turret_t *t = &p->t;
  t->stations = *p->stations > 0 ? *p->stations : 1;
  t->offset = *p->offset; t->overtravel = *p->overtravel; t->lock = *p->lock;
  t->speed = *p->speed > 0 ? *p->speed : 0.5; t->lock_speed = *p->lock_speed > 0 ? *p->lock_speed : 0.1;
  t->acc = *p->acc > 0 ? *p->acc : 5; t->dwell = *p->dwell; t->timeout = *p->timeout > 0 ? *p->timeout : 10;
  t->slip = *p->slip > 0 ? *p->slip : 0.02;
  if (*p->reset && !p->last_reset && t->state == TUR_FAULT) {  /* a reset: from where it stands, unhomed */
    double c = t->cmd;
    turret_init(t);
    t->cmd = c;
  }
  p->last_reset = *p->reset;
  if (!*p->enable && t->state != TUR_FAULT) t->vel = 0;  /* the machine off: it stands */
  else turret_step(t, period * 1e-9, *p->change, *p->tool, *p->index, *p->pos_fb);
  *p->pos_cmd = t->cmd;
  *p->changed = t->state == TUR_DONE && *p->change;
  *p->homed = t->homed;
  *p->busy = t->state != TUR_IDLE && t->state != TUR_DONE && t->state != TUR_FAULT;
  *p->fault = t->state == TUR_FAULT;
  *p->enable_out = *p->enable && t->state != TUR_FAULT;
  *p->chain_out = *p->chain_in && t->state != TUR_FAULT;
  *p->station = t->station; *p->state = t->state; *p->error = t->err;
  double f = *p->pos_fb - *p->sim_at;
  f -= floor(f);
  *p->sim_index = f < 0.01;
}

static void spindle_fn(void *arg, long period) {
  spins_t *p = arg;
  lspindle_t *s = &p->s;
  s->ratio = *p->ratio > 0 ? *p->ratio : 1.0;
  s->use_sensor = *p->use_sensor;
  double cmd = *p->on ? *p->speed : 0.0, pc = 0;
  *p->vel_cmd = lspindle_cmd(s, period * 1e-9, cmd, &pc);
  *p->pos_cmd = pc;
  int ie = *p->index_enable;
  *p->revs = lspindle_fb(s, *p->pos_fb, *p->sensor, &ie);
  *p->index_enable = ie;
  double fb = *p->vel_fb / (2.0 * M_PI * s->ratio);
  *p->speed_fb = fb;
  double tol = *p->tol > 0 ? *p->tol : 0.05;
  *p->at_speed = fabs(fb - cmd) <= tol * fabs(cmd) + 0.2;  /* stopped: within 0.2 turns/s */
}

#define TP "lathe.turret"
#define SP "lathe.spindle"

static int export_turret(void) {
  int r = 0;
  tpins_t *p = tp;
  r |= hal_pin_bit_newf(HAL_IN, &p->change, comp_id, TP ".change");
  r |= hal_pin_s32_newf(HAL_IN, &p->tool, comp_id, TP ".tool");
  r |= hal_pin_bit_newf(HAL_OUT, &p->changed, comp_id, TP ".changed");
  r |= hal_pin_bit_newf(HAL_IN, &p->index, comp_id, TP ".index");
  r |= hal_pin_float_newf(HAL_OUT, &p->pos_cmd, comp_id, TP ".pos-cmd");
  r |= hal_pin_float_newf(HAL_IN, &p->pos_fb, comp_id, TP ".pos-fb");
  r |= hal_pin_bit_newf(HAL_IN, &p->enable, comp_id, TP ".enable");
  r |= hal_pin_bit_newf(HAL_OUT, &p->enable_out, comp_id, TP ".enable-out");
  r |= hal_pin_bit_newf(HAL_IN, &p->chain_in, comp_id, TP ".chain-in");
  r |= hal_pin_bit_newf(HAL_OUT, &p->chain_out, comp_id, TP ".chain-out");
  r |= hal_pin_bit_newf(HAL_IN, &p->reset, comp_id, TP ".reset");
  r |= hal_pin_bit_newf(HAL_OUT, &p->homed, comp_id, TP ".homed");
  r |= hal_pin_bit_newf(HAL_OUT, &p->busy, comp_id, TP ".busy");
  r |= hal_pin_bit_newf(HAL_OUT, &p->fault, comp_id, TP ".fault");
  r |= hal_pin_s32_newf(HAL_OUT, &p->station, comp_id, TP ".station");
  r |= hal_pin_s32_newf(HAL_OUT, &p->state, comp_id, TP ".state");
  r |= hal_pin_s32_newf(HAL_OUT, &p->error, comp_id, TP ".error");
  r |= hal_pin_s32_newf(HAL_IN, &p->stations, comp_id, TP ".stations");
  r |= hal_pin_float_newf(HAL_IN, &p->offset, comp_id, TP ".offset");
  r |= hal_pin_float_newf(HAL_IN, &p->overtravel, comp_id, TP ".overtravel");
  r |= hal_pin_float_newf(HAL_IN, &p->lock, comp_id, TP ".lock");
  r |= hal_pin_float_newf(HAL_IN, &p->speed, comp_id, TP ".speed");
  r |= hal_pin_float_newf(HAL_IN, &p->lock_speed, comp_id, TP ".lock-speed");
  r |= hal_pin_float_newf(HAL_IN, &p->acc, comp_id, TP ".acc");
  r |= hal_pin_float_newf(HAL_IN, &p->dwell, comp_id, TP ".dwell");
  r |= hal_pin_float_newf(HAL_IN, &p->timeout, comp_id, TP ".timeout");
  r |= hal_pin_float_newf(HAL_IN, &p->slip, comp_id, TP ".slip");
  r |= hal_pin_bit_newf(HAL_OUT, &p->sim_index, comp_id, TP ".sim-index");
  r |= hal_pin_float_newf(HAL_IN, &p->sim_at, comp_id, TP ".sim-index-at");
  if (!r) {
    *p->stations = 6; *p->overtravel = 0.03; *p->lock = 0.01; *p->speed = 1.0; *p->lock_speed = 0.2; *p->acc = 10;
    *p->dwell = 0.3; *p->timeout = 10; *p->slip = 0.02; *p->enable = 1; *p->chain_in = 1;
    p->t.stations = 6;
    turret_init(&p->t);
  }
  return r;
}

static int export_spindle(void) {
  int r = 0;
  spins_t *p = sp;
  r |= hal_pin_float_newf(HAL_IN, &p->speed, comp_id, SP ".speed-rps");
  r |= hal_pin_bit_newf(HAL_IN, &p->on, comp_id, SP ".on");
  r |= hal_pin_float_newf(HAL_OUT, &p->vel_cmd, comp_id, SP ".vel-cmd");
  r |= hal_pin_float_newf(HAL_OUT, &p->pos_cmd, comp_id, SP ".pos-cmd");
  r |= hal_pin_float_newf(HAL_IN, &p->pos_fb, comp_id, SP ".pos-fb");
  r |= hal_pin_float_newf(HAL_IN, &p->vel_fb, comp_id, SP ".vel-fb");
  r |= hal_pin_float_newf(HAL_OUT, &p->revs, comp_id, SP ".revs");
  r |= hal_pin_float_newf(HAL_OUT, &p->speed_fb, comp_id, SP ".speed-fb");
  r |= hal_pin_bit_newf(HAL_IO, &p->index_enable, comp_id, SP ".index-enable");
  r |= hal_pin_bit_newf(HAL_IN, &p->sensor, comp_id, SP ".index-sensor");
  r |= hal_pin_bit_newf(HAL_IN, &p->use_sensor, comp_id, SP ".use-sensor");
  r |= hal_pin_bit_newf(HAL_OUT, &p->at_speed, comp_id, SP ".at-speed");
  r |= hal_pin_float_newf(HAL_IN, &p->ratio, comp_id, SP ".ratio");
  r |= hal_pin_float_newf(HAL_IN, &p->tol, comp_id, SP ".at-speed-tol");
  if (!r) {
    *p->ratio = 1.0; *p->tol = 0.05;
    lspindle_init(&p->s, 1.0);
  }
  return r;
}

int rtapi_app_main(void) {
  comp_id = hal_init("lathe");
  if (comp_id < 0) return comp_id;
  int r = 0;
  if (turret) {
    tp = hal_malloc(sizeof(tpins_t));
    if (!tp) r = -ENOMEM;
    else { memset(tp, 0, sizeof(*tp)); r = export_turret() || hal_export_funct(TP, turret_fn, tp, 1, 0, comp_id); }
  }
  if (!r && spindle) {
    sp = hal_malloc(sizeof(spins_t));
    if (!sp) r = -ENOMEM;
    else { memset(sp, 0, sizeof(*sp)); r = export_spindle() || hal_export_funct(SP, spindle_fn, sp, 1, 0, comp_id); }
  }
  if (r) {
    rtapi_print_msg(RTAPI_MSG_ERR, "lathe: cannot export pins / functions\n");
    hal_exit(comp_id);
    return -EINVAL;
  }
  hal_ready(comp_id);
  return 0;
}

void rtapi_app_exit(void) {
  hal_exit(comp_id);
}
