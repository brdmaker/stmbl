#include "io_comp.h"
#include "hal.h"
#include "defines.h"
#include "brd.h"
#include "common.h"

// brdstepper's I/O: the status LED and the pins the stock templates link
// (the F4 board's fan, outputs and indicator LEDs, which brdstepper has not:
// accepted and ignored). LED: off disabled, on enabled, blinking on a fault.

HAL_COMP(io);

HAL_PIN(state);  // fault0.state
HAL_PIN(fault);
HAL_PIN(fan);
HAL_PIN(out0);
HAL_PIN(out1);
HAL_PIN(out2);
HAL_PIN(ind0);
HAL_PIN(ind1);
HAL_PIN(cmdg);
HAL_PIN(cmdy);
HAL_PIN(fb0g);
HAL_PIN(fb0y);
HAL_PIN(clock_scale);

struct io_ctx_t {
  uint32_t n;
};

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct io_pin_ctx_t *pins = (struct io_pin_ctx_t *)pin_ptr;
  (void)ctx_ptr;
  PIN(clock_scale) = 1.0;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct io_ctx_t *ctx      = (struct io_ctx_t *)ctx_ptr;
  struct io_pin_ctx_t *pins = (struct io_pin_ctx_t *)pin_ptr;
  (void)period;
  ctx->n++;
  int st = (int)PIN(state);
  if(st == SOFT_FAULT || st == HARD_FAULT || PIN(fault) > 0.0) {
    hw_led((ctx->n / 1000) & 1);  // 2.5 Hz at 5 kHz
  } else {
    hw_led(st == ENABLED);
  }
}

hal_comp_t io_comp_struct = {
    .name      = "io",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct io_ctx_t),
    .pin_count = sizeof(struct io_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
