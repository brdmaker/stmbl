#include "iit_comp.h"
#include "commands.h"
#include "hal.h"
#include "math.h"
#include "defines.h"

HAL_COMP(iit);

// conf
HAL_PIN(amb_temp);
HAL_PIN(high_temp);
HAL_PIN(max_temp);
HAL_PIN(max_cur);
HAL_PIN(cur_boost);
HAL_PIN(max_time);

// out
HAL_PIN(temp);
// in
HAL_PIN(cur);

struct iit_ctx_t {
  // double: at a motor-length time constant (max_time minutes) a float e is
  // so large that one period's increment rounds away, and temp stalled short
  // of its final value (DEBUGNOTES 2026-11-11, tests/iit_long.test). A few
  // software double operations a period on the M4.
  double e;
};

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct iit_ctx_t *ctx      = (struct iit_ctx_t *)ctx_ptr;
  struct iit_pin_ctx_t *pins = (struct iit_pin_ctx_t *)pin_ptr;
  PIN(amb_temp) = 30.0;
  PIN(high_temp) = 80.0;
  PIN(max_temp) = 100.0;
  PIN(max_time) = 10.0;
  PIN(cur_boost) = 3.0;
  ctx->e = 0.0;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct iit_ctx_t *ctx      = (struct iit_ctx_t *)ctx_ptr;
  struct iit_pin_ctx_t *pins = (struct iit_pin_ctx_t *)pin_ptr;

  float cur_n = PIN(max_cur) / MAX(PIN(cur_boost), 1.0);
  float max_e = PIN(max_cur) * PIN(max_cur) * MAX(PIN(max_time), 0.1);
  float rise  = PIN(high_temp) - PIN(amb_temp);

  // max_cur 0 (not configured yet) or high_temp <= amb_temp: no model. temp
  // was 0/0 = NaN, which every later limit compared false against, for good
  // (the NaN stayed in e). Keep it a number (tests/iit_guard.test).
  if(!(max_e > 0.0) || !(rise > 0.0)) {
    ctx->e    = 0.0;
    PIN(temp) = PIN(amb_temp);
    return;
  }

  float temp = (float)ctx->e / max_e * (PIN(max_temp) - PIN(amb_temp)) + PIN(amb_temp);

  float pin  = PIN(cur) * PIN(cur);
  float pout = (temp - PIN(amb_temp)) * cur_n * cur_n / rise;

  ctx->e += (double)((pin - pout) * period);

  PIN(temp) = temp;
}

hal_comp_t iit_comp_struct = {
  .name      = "iit",
  .nrt       = 0,
  .rt        = rt_func,
  .frt       = 0,
  .nrt_init  = nrt_init,
  .rt_start  = 0,
  .frt_start = 0,
  .rt_stop   = 0,
  .frt_stop  = 0,
  .ctx_size  = sizeof(struct iit_ctx_t),
  .pin_count = sizeof(struct iit_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
