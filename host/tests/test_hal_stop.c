/*
 * shared/hal.c: every way the rt or frt stops runs the components' rt_stop
 * and frt_stop, which turn outputs off (the F3's bridge, the F4's brake
 * output io4). hal_run_rt/hal_run_frt stopped on an overrun (RT_TOO_LONG,
 * FRT_TOO_LONG) or an interrupted run (MISC_ERROR) by setting RT_STOP only,
 * so no rt_stop ran: the F3 kept switching its last PWM and the F4 its
 * brake output (DEBUGNOTES 2026-11-11). The real hal.c with a test
 * component. Prints TAP.
 */
#include "../../shared/hal.c"

uint32_t hal_get_systick_value() {
  return 0;
}
uint32_t hal_get_systick_reload() {
  return 1;
}
uint32_t hal_get_systick_freq() {
  return 1;
}
volatile int32_t cmd_active = -1;
uint32_t call_cmd(char *s) {
  (void)s;
  return 0;
}

// a component with an rt and an frt function that can re-enter the hal
static int rt_stops, frt_stops, reenter_rt, reenter_frt;
static void t_rt(float period, void *ctx, hal_pin_inst_t *pins) {
  (void)period, (void)ctx, (void)pins;
  if(reenter_rt) {
    reenter_rt = 0;
    hal_run_rt();  // the next rt tick while this one still runs: an overrun
  }
}
static void t_frt(float period, void *ctx, hal_pin_inst_t *pins) {
  (void)period, (void)ctx, (void)pins;
  if(reenter_frt) {
    reenter_frt = 0;
    hal_run_frt();
  }
}
static void t_rt_stop(void *ctx, hal_pin_inst_t *pins) {
  (void)ctx, (void)pins;
  rt_stops++;
}
static void t_frt_stop(void *ctx, hal_pin_inst_t *pins) {
  (void)ctx, (void)pins;
  frt_stops++;
}

static hal_comp_t tst_comp = {
    .name      = "tst",
    .rt        = t_rt,
    .frt       = t_frt,
    .rt_stop   = t_rt_stop,
    .frt_stop  = t_frt_stop,
    .ctx_size  = 0,
    .pin_count = 2,  // rt_prio, frt_prio
};
const hal_comp_t *comps[] = {&tst_comp};
const pin_t pins[]         = {"rt_prio", "frt_prio"};
const uint32_t comp_count  = 1;
const uint32_t pin_count   = 2;

static int n, fails;
static void ok(int c, const char *name) {
  n++;
  fails += !c;
  printf("%s %d - hal_stop: %s\n", c ? "ok" : "not ok", n, name);
}

static void run(void) {
  hal_init(0.0002, 0.00005);
  hal_set_debug_level(2);
  load_comp(comp_by_name("tst"));
  hal.comp_insts[0].pin_insts[0].value = 1.0;  // rt_prio
  hal.comp_insts[0].pin_insts[1].value = 1.0;  // frt_prio
  hal_start();
  rt_stops = frt_stops = 0;
}

int main(void) {
  run();
  hal_run_rt();
  hal_run_frt();
  ok(hal.rt_state == RT_SLEEP && rt_stops == 0, "running: no stop");

  hal_stop();
  ok(rt_stops == 1 && frt_stops == 1, "console stop: rt_stop and frt_stop");

  run();
  reenter_rt = 1;
  hal_run_rt();
  ok(hal.rt_state == RT_STOP && hal.hal_state == RT_TOO_LONG, "rt overrun stops the rt");
  ok(rt_stops == 1, "rt overrun: rt_stop ran");
  ok(frt_stops == 1, "rt overrun: frt_stop ran");

  run();
  hal_run_rt();
  hal.active_rt_func = 0;  // a run that never finished (a fault in it)
  hal_run_rt();
  ok(hal.rt_state == RT_STOP && hal.hal_state == MISC_ERROR, "interrupted rt run stops the rt");
  ok(rt_stops == 1 && frt_stops == 1, "interrupted rt run: rt_stop and frt_stop ran");

  run();
  reenter_frt = 1;
  hal_run_frt();
  ok(hal.rt_state == RT_STOP && hal.hal_state == FRT_TOO_LONG, "frt overrun stops both");
  ok(rt_stops == 1 && frt_stops == 1, "frt overrun: rt_stop and frt_stop ran");

  run();
  hal_run_frt();
  hal.active_frt_func = 0;
  hal_run_frt();
  ok(hal.rt_state == RT_STOP && hal.hal_state == MISC_ERROR, "interrupted frt run stops both");
  ok(rt_stops == 1 && frt_stops == 1, "interrupted frt run: rt_stop and frt_stop ran");

  run();
  hal_error(0);
  ok(rt_stops == 1 && frt_stops == 1 && hal.hal_state == MISC_ERROR, "fault handler (hal_error): stops ran");

  run();
  hal_stop();
  hal_run_rt();
  ok(rt_stops == 1, "stopped: rt_stop runs once, not every tick");

  printf("1..%d\n", n);
  return fails != 0;
}
