/*
 * Simulation stand-in for src/comps/hw/io4.c: same pins, no hardware behind them.
 * Inputs read low; outputs (LEDs, fan) go nowhere. out0_state is what the
 * brake output (DIO0) drives, as the F4 does: out0's source now, after a
 * console override too (hv0.sim_brake_release follows it).
 */
#include "io_comp.h"
#include "hal.h"

HAL_COMP(io);

// same pins as src/comps/hw/io4.c
HAL_PIN(fan);
HAL_PIN(state);
HAL_PIN(fault);
HAL_PIN(out0);
HAL_PIN(out1);
HAL_PIN(out2);
HAL_PIN(out_freq);
HAL_PIN(out_arr);
HAL_PIN(out_cnt);
HAL_PIN(out_ccr1);
HAL_PIN(in0);
HAL_PIN(in1);
HAL_PIN(ind0);
HAL_PIN(ind1);
HAL_PIN(ind0n);
HAL_PIN(ind1n);
HAL_PIN(th0);
HAL_PIN(th1);
HAL_PIN(CTX);
HAL_PIN(CRX);
HAL_PIN(C12);
HAL_PIN(C36);
HAL_PIN(C54);
HAL_PIN(C78);
HAL_PIN(cmd_remap);
HAL_PIN(swd_remap);
HAL_PIN(DIO);
HAL_PIN(CK);
HAL_PIN(fb0g);
HAL_PIN(fb0y);
HAL_PIN(fb1g);
HAL_PIN(fb1y);
HAL_PIN(cmdg);
HAL_PIN(cmdy);
HAL_PIN(fb0);
HAL_PIN(fb1);
HAL_PIN(fbd0);
HAL_PIN(fbd1);
HAL_PIN(fbd0n);
HAL_PIN(fbd1n);
HAL_PIN(fbth0);
HAL_PIN(fbth1);
HAL_PIN(fb0a);
HAL_PIN(fb0b);
HAL_PIN(fb0z);
HAL_PIN(fb1a);
HAL_PIN(fb1b);
HAL_PIN(fb1z);
HAL_PIN(fbsd);
HAL_PIN(master_arr);
HAL_PIN(clock_scale);
HAL_PIN(out0_state);  // sim only: the level DIO0 drives

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct io_pin_ctx_t *pins = (struct io_pin_ctx_t *)pin_ptr;
  (void)period;
  (void)ctx_ptr;
  PIN(out0_state) = PIN(out0) > 0.0 ? 1.0 : 0.0;
}

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct io_pin_ctx_t *pins = (struct io_pin_ctx_t *)pin_ptr;
  (void)pins;
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
    .ctx_size  = 0,
    .pin_count = sizeof(struct io_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
