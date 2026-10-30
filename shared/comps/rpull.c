// A resistive sensor on a feedback port's pin 7: its resistance from the
// pin's voltage (shared/rpull.h, docs/motor_temp_lv.md), for mtemp0.r
// (templates mtemp_fb0, mtemp_fb1).
#include "rpull_comp.h"
#include "commands.h"
#include "hal.h"
#include "math.h"
#include "defines.h"
#include "rpull.h"

HAL_COMP(rpull);

HAL_PIN(v);      // in: the pin's voltage [V] (io0.fb0 / io0.fb1)
HAL_PIN(vsup);   // the pull-up's supply [V] when vsup_meas isn't plausible (+5 V)
// The supply as the same ADC sees it through the same divider: the other
// port's pin 7, still jumpered to +5 V (io0.fb1 for FB0). Then the ADC's
// reference and the supply's tolerance cancel (the bench: 5.12 V, not 5.00,
// 667 vs 649 ohm). Used while it reads 4.5..5.5 V.
HAL_PIN(vsup_meas);
HAL_PIN(rpull);  // the pull-up [ohm] (R137 on FB0, R140 on FB1: 10 k)
HAL_PIN(rload);  // the ADC divider to GND [ohm] (10 k + 1 k)
HAL_PIN(r);      // out: the sensor [ohm], -1 open (mtemp's convention)

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct rpull_pin_ctx_t *pins = (struct rpull_pin_ctx_t *)pin_ptr;
  PIN(vsup)  = 5.0;
  PIN(rpull) = 10000.0;
  PIN(rload) = 11000.0;
  PIN(r)     = -1.0;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct rpull_pin_ctx_t *pins = (struct rpull_pin_ctx_t *)pin_ptr;
  (void)period;
  (void)ctx_ptr;
  float m  = PIN(vsup_meas);
  float vs = m >= 4.5 && m <= 5.5 ? m : PIN(vsup);
  PIN(r)   = rpull_r(PIN(v), vs, PIN(rpull), PIN(rload));
}

hal_comp_t rpull_comp_struct = {
    .name      = "rpull",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = 0,
    .pin_count = sizeof(struct rpull_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
