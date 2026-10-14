/*
 * Simulation stand-in for src/comps/adc.c: same pins, no hardware behind them.
 * amp0/amp1 read as a healthy encoder signal.
 */
#include "adc_comp.h"
#include "hal.h"

HAL_COMP(adc);

// same pins as src/comps/adc.c
HAL_PIN(sin0);
HAL_PIN(cos0);
HAL_PIN(sin0l);
HAL_PIN(cos0l);
HAL_PIN(quad);
HAL_PIN(amp0);
HAL_PIN(sin1);
HAL_PIN(cos1);
HAL_PIN(sin1l);
HAL_PIN(cos1l);
HAL_PIN(amp1);
HAL_PIN(res_mode);
HAL_PIN(sin_gain);
HAL_PIN(cos_gain);
HAL_PIN(sin_offset);
HAL_PIN(cos_offset);
HAL_PIN(send_step);
HAL_PINA(offset, 8);
HAL_PINA(gain, 8);

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct adc_pin_ctx_t *pins = (struct adc_pin_ctx_t *)pin_ptr;
  PIN(amp0) = 1.0;
  PIN(amp1) = 1.0;
}

hal_comp_t adc_comp_struct = {
    .name      = "adc",
    .nrt       = 0,
    .rt        = 0,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = 0,
    .pin_count = sizeof(struct adc_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
