#include "pe_comp.h"
#include "commands.h"
#include "hal.h"
#include "math.h"
#include "defines.h"
#include "angle.h"

// Power estimation. The motor's electrical input from the dq voltages and
// currents the hv reports (amplitude-invariant dq: 3/2 (ud id + uq iq)),
// and from it the drive's input power, the DC side's (p_in): the motor
// power over the inverter's efficiency (times it when the motor
// regenerates), plus the idle draw (logic, fans). p_in, low-passed with
// tau, goes to LinuxCNC over smart-serial (sserial0.power) for the system's
// power quotas (linuxcnc/powerquota).

HAL_COMP(pe);

HAL_PIN(udc);
HAL_PIN(idc);
HAL_PIN(ud);
HAL_PIN(uq);
HAL_PIN(id);
HAL_PIN(iq);
HAL_PIN(torque);
HAL_PIN(vel);
HAL_PIN(r);
HAL_PIN(j);
HAL_PIN(cap);
HAL_PIN(eff);   // inverter efficiency (0.95)
HAL_PIN(idle);  // idle draw [W] (15)
HAL_PIN(tau);   // low-pass of p_in [s] (0.001)
HAL_PIN(phases);  // motor phases (3; 2 for a stepper): dq power = phases / 2 (ud id + uq iq)

HAL_PIN(e_el);
HAL_PIN(e_kin);
HAL_PIN(p_el_dc);  // udc * idc (measured DC current, where there is one)
HAL_PIN(p_el_ac);  // the motor's electrical input [W]
HAL_PIN(p_m);      // mechanical: torque * vel [W]
HAL_PIN(p_t);      // copper losses [W]
HAL_PIN(p_in);     // the drive's input power [W], low-passed

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct pe_pin_ctx_t *pins = (struct pe_pin_ctx_t *)pin_ptr;
  PIN(eff)  = 0.95;
  PIN(idle) = 15.0;
  PIN(tau)  = 0.001;
  PIN(phases) = 3.0;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  // struct pe_ctx_t * ctx = (struct pe_ctx_t *)ctx_ptr;
  struct pe_pin_ctx_t *pins = (struct pe_pin_ctx_t *)pin_ptr;

  float udc = PIN(udc);
  float vel = PIN(vel);
  float id  = PIN(id);
  float iq  = PIN(iq);
  float t   = PIN(torque);

  PIN(e_el)  = PIN(cap) * udc * udc / 2.0;
  PIN(e_kin) = PIN(j) * vel * vel / 2.0;

  PIN(p_el_dc) = udc * PIN(idc);
  float k      = CLAMP(PIN(phases), 2.0, 3.0) / 2.0;
  float p_ac   = k * (PIN(ud) * id + PIN(uq) * iq);
  PIN(p_el_ac) = p_ac;
  PIN(p_m)     = t * vel;
  PIN(p_t)     = k * PIN(r) * (id * id + iq * iq);

  float eff = CLAMP(PIN(eff), 0.1, 1.0);
  float p   = (p_ac > 0.0 ? p_ac / eff : p_ac * eff) + PIN(idle);
  float a   = period / (MAX(PIN(tau), 0.0) + period);
  PIN(p_in) += a * (p - PIN(p_in));
}

hal_comp_t pe_comp_struct = {
    .name      = "pe",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = 0,
    .pin_count = sizeof(struct pe_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
