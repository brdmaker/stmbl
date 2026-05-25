/* test_pins.c — unit tests for pin connection topology.
 *
 * Covers: self-link on load, single-hop read, multi-hop chain collapse via
 * relink, re-connection overwrite, zero initial value, and pointer-range
 * check across multiple loaded components.
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

static void test_fresh_pin_self_linked(void) {
  setup();
  hal_parse("load sim");
  hal_pin_inst_t *amp = pin_inst_by_name("sim", 0, "amp");
  ok(amp != NULL,        "fresh: sim0.amp pin found");
  ok(amp->source == amp, "fresh: newly loaded pin is self-linked");
}

static void test_single_hop(void) {
  setup();
  hal_parse("load sim\nload sim\nsim1.amp = sim0.amp");
  hal_pin_inst_t *src = pin_inst_by_name("sim", 0, "amp");
  hal_pin_inst_t *snk = pin_inst_by_name("sim", 1, "amp");
  ok(snk->source == src,               "single_hop: sink->source is source pin");
  src->value = 3.14f;
  ok_float(snk->source->value, 3.14f, 1e-5f, "single_hop: reads source->value");
}

static void test_chain_relink(void) {
  /* Three-hop logical chain: sim2.amp <- sim1.amp <- sim0.amp.
   * After one relink pass each pin's source pointer is advanced one step,
   * so sim2.amp->source collapses to sim0.amp directly. */
  setup();
  hal_parse("load sim\nload sim\nload sim");
  hal_parse("sim1.amp = sim0.amp");
  hal_parse("sim2.amp = sim1.amp");
  hal_parse("relink");

  hal_pin_inst_t *p0 = pin_inst_by_name("sim", 0, "amp");
  hal_pin_inst_t *p1 = pin_inst_by_name("sim", 1, "amp");
  hal_pin_inst_t *p2 = pin_inst_by_name("sim", 2, "amp");

  ok(p1->source == p0, "chain_relink: sim1->source is sim0 after relink");
  ok(p2->source == p0, "chain_relink: sim2->source collapsed to sim0 after relink");

  p0->value = 9.9f;
  ok_float(p2->source->value, 9.9f, 1e-5f,
           "chain_relink: sim2 reads sim0 value after collapse");
}

static void test_reconnect(void) {
  /* Connect A <- B, then reconnect A <- C: A must follow C not B. */
  setup();
  hal_parse("load sim\nload sim\nload sim");
  hal_parse("sim0.amp = sim1.amp");
  hal_parse("sim0.amp = sim2.amp");

  hal_pin_inst_t *p0 = pin_inst_by_name("sim", 0, "amp");
  hal_pin_inst_t *p2 = pin_inst_by_name("sim", 2, "amp");
  ok(p0->source == p2, "reconnect: after overwrite, sink follows new source");
}

static void test_rt_prio_default_zero(void) {
  /* rt_prio is not set by nrt_init; must start at 0 so the comp is excluded
   * from the RT schedule until explicitly enabled. */
  setup();
  hal_parse("load sim");
  hal_pin_inst_t *rt = pin_inst_by_name("sim", 0, "rt_prio");
  ok(rt != NULL,                    "rt_prio_zero: pin found");
  ok_float(rt->value, 0.0f, 1e-7f, "rt_prio_zero: default value is 0.0");
}

static void test_pin_pool_bounds(void) {
  /* After loading several comps, every pin_insts pointer must lie within
   * the static hal.pin_insts[] array. */
  setup();
  hal_comp_t *sim = comp_by_name("sim");
  for(int i = 0; i < 5; i++) load_comp(sim);

  int all_in_range = 1;
  for(uint32_t i = 0; i < hal.comp_inst_count; i++) {
    hal_pin_inst_t *p = hal.comp_insts[i].pin_insts;
    if(p < hal.pin_insts || p >= hal.pin_insts + HAL_MAX_PINS) {
      all_in_range = 0;
    }
  }
  ok(all_in_range, "pool_bounds: all pin_insts pointers within pin_insts[]");
}

static void test_source_chain_in_pool(void) {
  /* After connecting pins, all source pointers in the chain must remain
   * within the static pin_insts[] pool. */
  setup();
  hal_parse("load sim\nload sim\nload sim");
  hal_parse("sim1.amp = sim0.amp");
  hal_parse("sim2.amp = sim1.amp");

  int all_ok = 1;
  for(uint32_t i = 0; i < hal.comp_inst_count; i++) {
    for(uint32_t j = 0; j < hal.comp_insts[i].comp->pin_count; j++) {
      hal_pin_inst_t *s = hal.comp_insts[i].pin_insts[j].source;
      if(s < hal.pin_insts || s >= hal.pin_insts + HAL_MAX_PINS) {
        all_ok = 0;
      }
    }
  }
  ok(all_ok, "source_chain: all source pointers within pin_insts[]");
}

int main(void) {
  test_fresh_pin_self_linked();
  test_single_hop();
  test_chain_relink();
  test_reconnect();
  test_rt_prio_default_zero();
  test_pin_pool_bounds();
  test_source_chain_in_pool();
  return done_testing();
}
