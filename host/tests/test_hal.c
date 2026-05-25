/* test_hal.c — unit tests for the HAL engine.
 *
 * Covers: load_comp() lifecycle and bounds, sort_rt() priority ordering,
 * hal_start() state transitions, hal_run_rt() with RT_STOP, and
 * hal_parse() pin set / pin connect / invalid-connect error handling.
 */
#include "runner.h"
#include "hal.h"
#include <string.h>
#include <math.h>

#define RT_PERIOD  0.0002f
#define FRT_PERIOD 0.00005f

static void setup(void) {
  hal_init(RT_PERIOD, FRT_PERIOD);
  hal_set_debug_level(2);  /* suppress all HAL print output */
}

/* ------------------------------------------------------------------ load_comp */

static void test_load_single(void) {
  setup();
  hal_comp_t *sim = comp_by_name("sim");
  ok(sim != NULL, "load/single: comp_by_name(sim) not NULL");

  int ret = load_comp(sim);
  ok(ret == 1,                       "load/single: load_comp returns 1");
  ok(hal.comp_inst_count == 1,       "load/single: comp_inst_count == 1");

  hal_comp_inst_t *inst = &hal.comp_insts[0];
  ok(inst->comp == sim,              "load/single: inst->comp matches");
  ok(inst->instance == 0,            "load/single: first instance number is 0");
  ok(inst->state == PRE_HW_INIT,     "load/single: state is PRE_HW_INIT");

  /* every pin must start self-linked */
  int self_linked = 1;
  for(uint32_t i = 0; i < sim->pin_count; i++) {
    if(inst->pin_insts[i].source != &inst->pin_insts[i]) {
      self_linked = 0;
      break;
    }
  }
  ok(self_linked, "load/single: all pins self-linked after load");

  /* ctx and pin_insts pointers must lie within their static pool bounds */
  ok((uint8_t *)inst->ctx >= hal.ctxs &&
     (uint8_t *)inst->ctx + inst->ctx_size <= hal.ctxs + HAL_MAX_CTX,
     "load/single: ctx within ctxs pool");
  ok(inst->pin_insts >= hal.pin_insts &&
     inst->pin_insts <  hal.pin_insts + HAL_MAX_PINS,
     "load/single: pin_insts within pin_insts array");

#ifdef STMBL_HOST
  ok(inst->magic == HAL_COMP_INST_MAGIC, "load/single: magic cookie correct");
#endif
}

static void test_load_two_instances(void) {
  setup();
  hal_comp_t *sim = comp_by_name("sim");
  load_comp(sim);
  load_comp(sim);

  ok(hal.comp_inst_count == 2,   "load/two: comp_inst_count == 2");
  ok(hal.comp_insts[0].instance == 0, "load/two: first instance == 0");
  ok(hal.comp_insts[1].instance == 1, "load/two: second instance == 1");

  /* pin regions must be contiguous and non-overlapping */
  ok(hal.comp_insts[1].pin_insts ==
     hal.comp_insts[0].pin_insts + sim->pin_count,
     "load/two: pin regions contiguous");
}

static void test_load_null(void) {
  setup();
  int ret = load_comp(NULL);
  ok(ret == 0,                   "load/null: load_comp(NULL) returns 0");
  ok(hal.comp_inst_count == 0,   "load/null: comp_inst_count unchanged");
}

static void test_load_exhaustion(void) {
  setup();
  hal_comp_t *sim = comp_by_name("sim");
  int loaded = 0;
  while(load_comp(sim)) loaded++;
  ok(loaded > 0,                            "load/limit: loaded at least one sim");
  ok(hal.comp_inst_count == (uint32_t)loaded, "load/limit: comp_inst_count matches");
  ok(load_comp(sim) == 0,                   "load/limit: extra load fails at limit");
}

/* ------------------------------------------------------- hal_start / sort_rt */

static void test_sort_rt_order(void) {
  setup();
  hal_comp_t *sim = comp_by_name("sim");
  load_comp(sim);  /* sim0 */
  load_comp(sim);  /* sim1 */
  /* give sim0 higher priority number (runs later), sim1 lower (runs first) */
  hal_parse("sim0.rt_prio = 2.0\nsim1.rt_prio = 1.0");
  hal_start();

  ok(hal.rt_comp_count == 2,               "sort_rt: two RT comps scheduled");
  ok(hal.rt_comps[0] == &hal.comp_insts[1], "sort_rt: sim1 (prio 1) runs first");
  ok(hal.rt_comps[1] == &hal.comp_insts[0], "sort_rt: sim0 (prio 2) runs second");
}

static void test_start_state_transitions(void) {
  setup();
  hal_comp_t *sim = comp_by_name("sim");
  load_comp(sim);
  hal_parse("sim0.rt_prio = 1.0");
  hal_start();

  ok(hal.rt_state == RT_SLEEP,          "start: rt_state is RT_SLEEP");
  ok(hal.comp_insts[0].state == STARTED, "start: comp state is STARTED");
  ok(hal.hal_state == HAL_OK2,          "start: hal_state is HAL_OK2");
}

/* --------------------------------------------------------------- hal_run_rt */

static void test_run_rt_stopped(void) {
  setup();
  hal_comp_t *sim = comp_by_name("sim");
  load_comp(sim);
  /* do not call hal_start — rt_state stays RT_STOP */
  hal_run_rt();
  ok(hal.rt_state == RT_STOP, "run_rt/stopped: state stays RT_STOP");
}

static void test_run_rt_ticks(void) {
  setup();
  hal_comp_t *sim = comp_by_name("sim");
  load_comp(sim);
  hal_parse("sim0.rt_prio = 1.0");
  hal_start();

  hal_run_rt();
  hal_run_rt();
  ok(hal.rt_state == RT_SLEEP, "run_rt/ticks: rt_state RT_SLEEP after two ticks");
  ok(hal.hal_state == HAL_OK2, "run_rt/ticks: hal_state HAL_OK2 after clean ticks");
}

/* --------------------------------------------------------- hal_parse pin ops */

static void test_pin_set(void) {
  setup();
  hal_parse("load sim\nsim0.freq = 42.0");
  hal_pin_inst_t *p = pin_inst_by_name("sim", 0, "freq");
  ok(p != NULL,                          "pin_set: pin_inst_by_name finds sim0.freq");
  ok_float(p->value, 42.0f, 1e-5f,       "pin_set: value set to 42.0");
  ok(p->source == p,                     "pin_set: pin self-linked after set");
}

static void test_pin_connect(void) {
  setup();
  hal_parse("load sim\nload sim\nsim1.amp = sim0.amp");
  hal_pin_inst_t *src = pin_inst_by_name("sim", 0, "amp");
  hal_pin_inst_t *snk = pin_inst_by_name("sim", 1, "amp");
  ok(src != NULL && snk != NULL,         "pin_connect: both pins found");
  ok(snk->source == src,                 "pin_connect: sink->source points to src");
  src->value = 7.5f;
  ok_float(snk->source->value, 7.5f, 1e-5f, "pin_connect: sink reads src value");
}

static void test_pin_connect_invalid(void) {
  setup();
  hal_parse("load sim");
  /* connect to a nonexistent component — must not crash */
  hal_parse("sim0.amp <= nocomp0.amp");
  hal_pin_inst_t *p = pin_inst_by_name("sim", 0, "amp");
  ok(p != NULL,        "pin_connect_invalid: sim0.amp still exists");
  ok(p->source == p,   "pin_connect_invalid: pin stays self-linked on failed connect");
}

/* ----------------------------------------------------------------------- main */

int main(void) {
  test_load_single();
  test_load_two_instances();
  test_load_null();
  test_load_exhaustion();
  test_sort_rt_order();
  test_start_state_transitions();
  test_run_rt_stopped();
  test_run_rt_ticks();
  test_pin_set();
  test_pin_connect();
  test_pin_connect_invalid();
  return done_testing();
}
