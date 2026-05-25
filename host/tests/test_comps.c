/* test_comps.c — component math tests.
 *
 * Covers:
 *   sim  — sin output stays within [-amp, +amp] and oscillates over a period
 *   pid  — zero error produces zero torque; 1-rad step matches pid.hal output;
 *           huge error clamps to max_torque
 */
#include "runner.h"
#include "hal.h"
#include <math.h>

#define RT_PERIOD  0.0002f
#define FRT_PERIOD 0.00005f

static void setup(void) {
  hal_init(RT_PERIOD, FRT_PERIOD);
  hal_set_debug_level(2);
}

static void run_rt(int n) {
  for(int i = 0; i < n; i++) hal_run_rt();
}

/* ----------------------------------------------------------------------- sim */

static void test_sim_output_bounded(void) {
  /* sin must stay within [-amp, +amp] at all times including during LP
   * filter warm-up.  Run 3000 ticks (~0.6 s) at 10 Hz; by then freq/amp
   * filters are >99% converged. */
  setup();
  hal_parse("load sim\nsim0.rt_prio = 1.0");
  hal_parse("sim0.amp = 1.0\nsim0.freq = 10.0");
  hal_start();

  hal_pin_inst_t *sin_pin = pin_inst_by_name("sim", 0, "sin");
  hal_pin_inst_t *amp_pin = pin_inst_by_name("sim", 0, "amp");
  ok(sin_pin != NULL && amp_pin != NULL, "sim/bounded: sin and amp pins found");

  int violations = 0;
  for(int i = 0; i < 3000; i++) {
    hal_run_rt();
    float s = sin_pin->source->value;
    float a = amp_pin->source->value + 0.01f;  /* small margin for LP transient */
    if(s > a || s < -a) violations++;
  }
  ok(violations == 0, "sim/bounded: sin stays within [-amp, +amp]");
}

static void test_sim_oscillates(void) {
  /* After LP warm-up the sin must cross both sides of zero — i.e. actually
   * oscillate rather than stick at 0. */
  setup();
  hal_parse("load sim\nsim0.rt_prio = 1.0");
  hal_parse("sim0.amp = 1.0\nsim0.freq = 10.0");
  hal_start();

  run_rt(2000);  /* warm-up: ~0.4 s */

  hal_pin_inst_t *sin_pin = pin_inst_by_name("sim", 0, "sin");
  int saw_pos = 0, saw_neg = 0;
  /* 500 ticks ≈ one full period at 10 Hz */
  for(int i = 0; i < 500; i++) {
    hal_run_rt();
    float v = sin_pin->source->value;
    if(v >  0.1f) saw_pos = 1;
    if(v < -0.1f) saw_neg = 1;
  }
  ok(saw_pos && saw_neg, "sim/oscillates: sin both positive and negative");
}

/* ----------------------------------------------------------------------- pid */

/* Common PID setup matching the pid.hal example (limits, gains, inertia). */
static void setup_pid(void) {
  setup();
  hal_parse(
    "load pid\n"
    "pid0.rt_prio = 1.0\n"
    "pid0.en = 1.0\n"
    "pid0.max_vel = 1000.0\n"
    "pid0.neg_min_vel = 1000.0\n"
    "pid0.max_acc = 1000000.0\n"
    "pid0.max_torque = 1000.0\n"
    "pid0.neg_min_torque = 1000.0\n"
    "pid0.j_mot = 0.001\n"
    "pid0.vel_g = 1.0"
  );
  hal_start();
}

static void test_pid_zero_error(void) {
  /* pos_ext_cmd == pos_fb == 0 → torque must be (near) zero after one tick. */
  setup_pid();
  hal_parse("pid0.pos_ext_cmd = 0.0\npid0.pos_fb = 0.0");
  hal_run_rt();

  hal_pin_inst_t *torque = pin_inst_by_name("pid", 0, "torque_cmd");
  ok(torque != NULL, "pid/zero_error: torque_cmd pin found");
  ok_float(torque->source->value, 0.0f, 1e-3f,
           "pid/zero_error: zero tracking error → zero torque");
}

static void test_pid_unit_step(void) {
  /* Replicate pid.hal: 1 rad step, pos_fb = 0 → after 1 RT tick:
   *   vel_cmd  ≈ pos_bw * 1.0 = 100 rad/s
   *   torque_cmd ≈ 240 Nm  (matches pid.hal reference output)         */
  setup_pid();
  hal_parse("pid0.pos_ext_cmd = 1.0\npid0.pos_fb = 0.0");
  hal_run_rt();

  hal_pin_inst_t *vel_cmd    = pin_inst_by_name("pid", 0, "vel_cmd");
  hal_pin_inst_t *torque_cmd = pin_inst_by_name("pid", 0, "torque_cmd");
  ok(vel_cmd != NULL && torque_cmd != NULL, "pid/unit_step: pins found");
  ok_float(vel_cmd->source->value,    100.0f,  1.0f,
           "pid/unit_step: vel_cmd ≈ pos_bw (100)");
  ok_float(torque_cmd->source->value, 240.0f,  1.0f,
           "pid/unit_step: torque_cmd ≈ 240 (matches pid.hal)");
}

static void test_pid_saturation(void) {
  /* Huge position error must clamp torque_cmd to max_torque, not overflow. */
  setup();
  hal_parse(
    "load pid\n"
    "pid0.rt_prio = 1.0\n"
    "pid0.en = 1.0\n"
    "pid0.max_vel = 1000.0\n"
    "pid0.neg_min_vel = 1000.0\n"
    "pid0.max_acc = 1000000.0\n"
    "pid0.max_torque = 50.0\n"
    "pid0.neg_min_torque = 50.0\n"
    "pid0.j_mot = 0.001\n"
    "pid0.vel_g = 1.0\n"
    "pid0.pos_ext_cmd = 1000000.0\n"
    "pid0.pos_fb = 0.0"
  );
  hal_start();
  hal_run_rt();

  hal_pin_inst_t *torque = pin_inst_by_name("pid", 0, "torque_cmd");
  ok(torque != NULL, "pid/saturation: torque_cmd pin found");
  ok_float(torque->source->value, 50.0f, 0.1f,
           "pid/saturation: huge error clamps to max_torque (50)");
}

int main(void) {
  test_sim_output_bounded();
  test_sim_oscillates();
  test_pid_zero_error();
  test_pid_unit_step();
  test_pid_saturation();
  return done_testing();
}
