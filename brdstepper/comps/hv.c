#include "hv_comp.h"
#include "hal.h"
#include "defines.h"
#include "angle.h"
#include "bip_loop.h"
#include "brd.h"
#include <math.h>

// brdstepper's power stage: the two coils' current loop (shared/bip_loop.c)
// on two DRV8876 H-bridges, in place of the F4's link to the HV board. Same
// pin names as src/comps/hv.c, so the pid / stepper templates link to it
// unchanged; the host simulator's stepper mode (host/hw/hv.c, sim_motor =
// 1) runs the same loop and sensing.
//
// frt (20 kHz, after both samples of the PWM period): the coil currents
// from IPROPI (bip_sense), the loop, the duties for the next period.
// rt: supply, temperature, enable / fault, the DRV8876's own current trip.
//
// DRV8876 (SLVSDS7B), strapped PMODE high (PWM mode), IMODE open (fixed
// off-time chopping at the trip, overcurrent latched off; nFAULT then
// means a real fault, not chopping):
// - disabled: both inputs low, coast (outputs Hi-Z), not brake;
// - nSLEEP stays high; the driver is ready tWAKE (1 ms) after it rises. A
//   latched fault (overcurrent) is cleared by nSLEEP low for tSLEEP (1 ms),
//   done when the drive is enabled again after the fault;
// - sense_window: a window must hold the sample tPD (0.65 us, input to
//   output) + tDELAY (1.6 us, current sense) + the ADC's 0.15 us after its
//   start; centred, that is a window of 4.8 us, 9.6 % of 50 us: 0.12, and
//   max_duty 0.88 leaves the brake window as long.

HAL_COMP(hv);

HAL_PIN(d_cmd);
HAL_PIN(q_cmd);
HAL_PIN(pos);  // electrical angle, from the commutation feedback
HAL_PIN(vel);  // electrical speed
HAL_PIN(en);
HAL_PIN(phase_mode);
HAL_PIN(cmd_mode);
HAL_PIN(r);
HAL_PIN(l);
HAL_PIN(psi);
HAL_PIN(cur_bw);
HAL_PIN(cur_ff);
HAL_PIN(cur_ind);
HAL_PIN(max_cur);
HAL_PIN(scale);
HAL_PIN(rev);
HAL_PIN(dc_volt);
HAL_PIN(dc_cur);
HAL_PIN(id_fb);
HAL_PIN(iq_fb);
HAL_PIN(ud_fb);
HAL_PIN(uq_fb);
HAL_PIN(abs_cur);
HAL_PIN(abs_volt);
HAL_PIN(pwm_volt);
HAL_PIN(hv_temp);
HAL_PIN(fault);
// brdstepper
HAL_PIN(max_duty);      // usable share of the supply (both IPROPI windows samplable)
HAL_PIN(sense_window);  // shortest samplable window, share of the period
HAL_PIN(sense_thr);     // IPROPI noise floor [A]
HAL_PIN(sense_cal);     // IPROPI offset calibration at each enable [PWM periods]
HAL_PIN(sense_off_a);   // the offsets found [A]
HAL_PIN(sense_off_b);
HAL_PIN(trip);          // the DRV8876's own current trip, times max_cur
HAL_PIN(ia);            // coil currents [A]
HAL_PIN(ib);
HAL_PIN(duty_a);
HAL_PIN(duty_b);
HAL_PIN(bridge_fault);  // nFAULT
HAL_PIN(en_in);         // the enable input (PA7)

struct hv_ctx_t {
  bip_state_t bip;
  bip_param_t par;   // from the pins, refreshed in rt
  volatile float pos, vel, d_cmd, q_cmd;
  volatile int en, rev;
  float da, db;      // the duties the current samples were taken with
  float ia, ib;      // reconstructed coil currents
  float win, thr;    // IPROPI: shortest samplable window, noise floor
  uint32_t frt_n;    // frt periods since the last rt
  uint32_t ready;    // rt periods since nSLEEP rose (the driver needs 1 ms)
  uint32_t reset;    // rt periods left of an nSLEEP low pulse (fault reset)
  int was_en, faulted;
  int frt_was_en;
  uint32_t cal_n;    // sense_cal, handed to frt
  bip_cal_t cal;
  bip_out_t out;
};

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct hv_pin_ctx_t *pins = (struct hv_pin_ctx_t *)pin_ptr;
  PIN(scale)        = 1.0;
  PIN(max_duty)     = 0.88;
  PIN(sense_window) = 0.12;
  PIN(sense_thr)    = 0.01;
  PIN(sense_cal)    = 64;  // 3.2 ms
  PIN(trip)         = 1.5;
}

static void frt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct hv_ctx_t *ctx = (struct hv_ctx_t *)ctx_ptr;
  (void)pin_ptr;
  // the angle advanced by the speed since the rt loop set it
  float e = mod(ctx->pos + ctx->vel * period * ctx->frt_n);
  ctx->frt_n++;
  // enabled: first the IPROPI offsets, braking with the rotor still
  if(ctx->en && !ctx->frt_was_en && ctx->cal_n > 0) {
    bip_cal_start(&ctx->cal, ctx->cal_n);
  }
  ctx->frt_was_en = ctx->en;
  if(ctx->en && bip_cal_sample(&ctx->cal, brd_adc.ia_brake, brd_adc.ib_brake)) {
    bip_reset(&ctx->bip);
    ctx->da = ctx->db = 0.0f;
    hw_pwm_set(0.0f, 0.0f);  // both inputs high: brake
    return;
  }
  float ra, rb;
  bip_ref(e, ctx->d_cmd, ctx->q_cmd, &ra, &rb);
  float iad = bip_cal_apply(brd_adc.ia_drive, ctx->cal.off_a), iab = bip_cal_apply(brd_adc.ia_brake, ctx->cal.off_a);
  float ibd = bip_cal_apply(brd_adc.ib_drive, ctx->cal.off_b), ibb = bip_cal_apply(brd_adc.ib_brake, ctx->cal.off_b);
  ctx->ia = bip_sense(iad, iab, ctx->da, ctx->win, ra, ctx->thr);
  ctx->ib = bip_sense(ibd, ibb, ctx->db, ctx->win, rb, ctx->thr);
  bip_param_t p = ctx->par;
  p.udc         = brd_adc.vbus;
  bip_step(&ctx->bip, &p, e, ctx->vel, ctx->d_cmd, ctx->q_cmd, ctx->ia, ctx->ib, ctx->en, period, &ctx->out);
  ctx->da = ctx->out.duty_a;
  ctx->db = ctx->out.duty_b;
  if(ctx->en) {
    hw_pwm_set(ctx->da, ctx->db);
  } else {
    hw_pwm_coast();
  }
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct hv_ctx_t *ctx      = (struct hv_ctx_t *)ctx_ptr;
  struct hv_pin_ctx_t *pins = (struct hv_pin_ctx_t *)pin_ptr;
  (void)period;
  int rev       = PIN(rev) > 0.0;
  float max_cur = MAX(PIN(max_cur) * PIN(scale), 0.0);
  int bfault    = hw_bridge_fault();
  int want      = PIN(en) > 0.0;
  // nSLEEP: high, except a 1.2 ms pulse to clear a latched fault when the
  // drive is enabled again after one; then 1.2 ms (tWAKE 1 ms) to be ready
  const uint32_t ms12 = 6;  // rt periods (5 kHz)
  if(bfault && ctx->was_en) {
    ctx->faulted = 1;
  }
  if(want && !ctx->was_en && ctx->faulted) {
    ctx->reset   = ms12;
    ctx->faulted = 0;
  }
  ctx->was_en = want;
  if(ctx->reset > 0) {
    ctx->reset--;
    hw_bridges(0);
    ctx->ready = 0;
  } else {
    hw_bridges(1);
    if(ctx->ready < ms12) {
      ctx->ready++;
    }
  }
  int en = want && !bfault && ctx->ready >= ms12;

  bip_param_t p = {.r = PIN(r), .l = PIN(l), .psi = PIN(psi), .cur_bw = PIN(cur_bw), .cur_ff = PIN(cur_ff),
                   .cur_ind = PIN(cur_ind), .udc = brd_adc.vbus, .max_cur = max_cur, .max_duty = PIN(max_duty),
                   .volt_mode = PIN(cmd_mode) == 0.0};
  // hand over to frt (it runs in a higher priority interrupt)
  __disable_irq();
  ctx->par   = p;
  ctx->pos   = rev ? minus(0, PIN(pos)) : PIN(pos);
  ctx->vel   = rev ? -PIN(vel) : PIN(vel);
  ctx->d_cmd = PIN(d_cmd);
  ctx->q_cmd = rev ? -PIN(q_cmd) : PIN(q_cmd);
  ctx->en    = en;
  ctx->frt_n = 0;
  ctx->win   = CLAMP(PIN(sense_window), 0.0, 0.5);
  ctx->thr   = MAX(PIN(sense_thr), 0.0);
  ctx->cal_n = (uint32_t)CLAMP(PIN(sense_cal), 0, 10000);
  bip_out_t o = ctx->out;
  float ia = ctx->ia, ib = ctx->ib;
  __enable_irq();

  // the DRV8876's own trip: VREF = trip * max_cur * R * AIPROPI (its comparator on IPROPI)
  hw_vref(MAX(PIN(trip), 1.0) * max_cur * BRD_IPROPI_R * BRD_AIPROPI);

  PIN(id_fb)        = o.id;
  PIN(iq_fb)        = rev ? -o.iq : o.iq;
  PIN(ud_fb)        = o.ud;
  PIN(uq_fb)        = rev ? -o.uq : o.uq;
  PIN(abs_cur)      = o.abs_cur;
  PIN(abs_volt)     = o.abs_volt;
  PIN(duty_a)       = o.duty_a;
  PIN(duty_b)       = o.duty_b;
  PIN(ia)           = ia;
  PIN(sense_off_a)  = ctx->cal.off_a;
  PIN(sense_off_b)  = ctx->cal.off_b;
  PIN(ib)           = ib;
  PIN(dc_volt)      = brd_adc.vbus;
  PIN(pwm_volt)     = brd_adc.vbus * CLAMP(PIN(max_duty), 0.0, 1.0);
  PIN(bridge_fault) = bfault;
  PIN(fault)        = bfault;
  PIN(en_in)        = hw_enable_input();
  // NTC: 10k (B 3380) to ground, 10k pull-up to 3.3 V
  float v = CLAMP(brd_adc.ntc_v, 0.01, BRD_VREF - 0.01);
  float rn = 10000.0 * v / (BRD_VREF - v);
  PIN(hv_temp) = 1.0 / (1.0 / 298.15 + logf(rn / 10000.0) / 3380.0) - 273.15;
}

hal_comp_t hv_comp_struct = {
    .name      = "hv",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = frt_func,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct hv_ctx_t),
    .pin_count = sizeof(struct hv_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
