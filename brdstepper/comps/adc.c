#include "adc_comp.h"
#include "hal.h"
#include "defines.h"
#include "brd.h"

// brdstepper's slow analog inputs (sampled with the coil currents by
// brdstepper/src/hw.c): the hardware version ID divider. The stock templates
// load "adc"; the F4 board's resolver and sin/cos inputs do not exist here.

HAL_COMP(adc);

HAL_PIN(hwid);     // the ID divider [V]
HAL_PIN(variant);  // 0 smart-serial, 1 CAN, 2 unknown

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct adc_pin_ctx_t *pins = (struct adc_pin_ctx_t *)pin_ptr;
  (void)period;
  (void)ctx_ptr;
  PIN(hwid)    = brd_adc.hwid_v;
  PIN(variant) = hw_variant(brd_adc.hwid_v);
}

hal_comp_t adc_comp_struct = {
    .name      = "adc",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = 0,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = 0,
    .pin_count = sizeof(struct adc_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
