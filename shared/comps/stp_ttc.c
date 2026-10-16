#include "stp_ttc_comp.h"
#include "hal.h"
#include "defines.h"
#include <math.h>

// Torque to current for a two-phase hybrid stepper (brdstepper), in place
// of pmsm_ttc: torque = polecount * psi * iq (two phases: no 3/2), and the
// detent torque (4 cycles per electrical turn) cancelled from the
// electrical angle.
//
//   stp_ttc0.torque = pid0.torque_cmd    the torque wanted [Nm]
//   stp_ttc0.pos_in = vel2.pos_out       the electrical angle [rad]
//   stp_ttc0.detent = 0.004              detent amplitude [Nm] (0: off),
//   stp_ttc0.detent_phase = 0            its phase [rad]: the motor makes
//                                        -detent sin(4 pos + phase), this
//                                        adds the opposite
//   stp_ttc0.cur                         -> hv0.q_cmd [A]
//   stp_ttc0.torque_fb                   from cur_fb (hv0.iq_fb), less the detent

HAL_COMP(stp_ttc);

HAL_PIN(psi);
HAL_PIN(polecount);
HAL_PIN(torque);
HAL_PIN(pos_in);
HAL_PIN(pos_out);
HAL_PIN(detent);
HAL_PIN(detent_phase);
HAL_PIN(cur);
HAL_PIN(cur_fb);
HAL_PIN(torque_fb);

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct stp_ttc_pin_ctx_t *pins = (struct stp_ttc_pin_ctx_t *)pin_ptr;
  float kt  = MAX(PIN(polecount), 1.0) * MAX(PIN(psi), 1e-5);  // [Nm/A]
  float pos = PIN(pos_in);
  float det = PIN(detent) * sinf(4.0 * pos + PIN(detent_phase));
  PIN(pos_out)   = pos;
  PIN(cur)       = (PIN(torque) + det) / kt;
  PIN(torque_fb) = PIN(cur_fb) * kt - det;
}

hal_comp_t stp_ttc_comp_struct = {
    .name      = "stp_ttc",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = 0,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = 0,
    .pin_count = sizeof(struct stp_ttc_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
