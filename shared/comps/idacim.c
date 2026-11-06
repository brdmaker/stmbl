#include "idacim_comp.h"
#include "commands.h"
#include "hal.h"
#include "math.h"
#include "defines.h"
#include "angle.h"

// Induction motor identification, all at standstill (the rotor does not
// turn: dc on one axis makes no torque), so a conveyor can stay coupled.
// T equivalent circuit, leakage split evenly (ls = lr = lm + ll / 2).
//   1  rs: voltage mode, d axis current regulated to test_cur for rl_time;
//      rs = ud / id over the last third (dc steady state: no rotor current).
//   2  sigma * ls: voltage square wave around rs * test_cur, half period
//      L_HALF << rotor time constant, so the rotor flux cannot follow;
//      least squares of ud - rs * id = l * did/dt.
//   3  rest: zero voltage for wait_time, the rotor flux decays.
//   4  rotor: current step to test_cur (current mode) for step_time, from
//      zero rotor flux. With the measured current (it takes ~20 ms to
//      settle, so it is not assumed constant), the rotor part of the voltage
//        e = ud - rs * id - sigma * ls * did/dt = lm / lr * dpsir/dt
//      integrates to phi = lm / lr * psir, which ends at a * i with
//      a = lm^2 / lr; and dpsir/dt = (lm * id - psir) / tau_r gives
//        e * tau_r = a * id - phi    (least squares over all samples).
//      rs here is u(end) / i(end) of the step itself, so an rs error does not
//      integrate up; the sums are kept split as e = p - rs * q so rs can be
//      applied at the end. Then lr = sigma * ls + a, lm = sqrt(a * lr),
//      rr = lr / tau_r.
//   5  done (outputs valid), 6 aborted (current not reached).

HAL_COMP(idacim);

HAL_PIN(en);
HAL_PIN(test_cur);   // test current [A] (d axis amplitude)
HAL_PIN(rl_time);    // [s]
HAL_PIN(wait_time);  // [s]
HAL_PIN(step_time);  // [s], several tau_r
HAL_PIN(rl_ki);      // state 1 base integral gain [V/(A*s)]
HAL_PIN(l_half);     // state 2 half period [s]

HAL_PIN(ud);         // hv0.ud_fb, hv0.id_fb
HAL_PIN(id);

HAL_PIN(cmd_mode);   // to hv0
HAL_PIN(d_cmd);
HAL_PIN(q_cmd);
HAL_PIN(com_pos);
HAL_PIN(en_out);

HAL_PIN(rs);         // results
HAL_PIN(rr);
HAL_PIN(lm);
HAL_PIN(ll);         // total leakage
HAL_PIN(sls);        // sigma * ls
HAL_PIN(tau_r);
HAL_PIN(state);

HAL_PIN(timer);
HAL_PIN(su);         // state 1: sums over the steady part
HAL_PIN(si);
HAL_PIN(s11);        // state 2: least squares
HAL_PIN(s1y);
HAL_PIN(last_id);
// state 4: p = ud - sigma * ls * did/dt, q = id (interval average),
// fp, fq their integrals; sums of products for the least squares.
HAL_PIN(fp);
HAL_PIN(fq);
HAL_PIN(spp);
HAL_PIN(spq);
HAL_PIN(sqq);
HAL_PIN(spi);
HAL_PIN(sqi);
HAL_PIN(spfp);
HAL_PIN(spfq);
HAL_PIN(sqfp);
HAL_PIN(sqfq);
HAL_PIN(tail_u);     // sums of u and i over the last fifth, and the count
HAL_PIN(tail_i);
HAL_PIN(tail_n);

#define RL_BW 20.0
#define L_TIME 0.2

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct idacim_pin_ctx_t *pins = (struct idacim_pin_ctx_t *)pin_ptr;
  PIN(test_cur)  = 0.5;
  PIN(rl_time)   = 1.0;
  PIN(wait_time) = 1.0;
  PIN(step_time) = 1.0;
  PIN(rl_ki)     = 10.0;
  PIN(l_half)    = 0.002;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct idacim_pin_ctx_t *pins = (struct idacim_pin_ctx_t *)pin_ptr;

  PIN(timer) += period;
  PIN(com_pos) = 0.0;
  PIN(q_cmd)   = 0.0;

  if(PIN(en) <= 0.0) {
    PIN(state)  = 0.0;
    PIN(en_out) = 0.0;
    PIN(d_cmd)  = 0.0;
    PIN(timer)  = 0.0;
    return;
  }

  switch((int)PIN(state)) {
    case 0:
      PIN(en_out)   = 1.0;
      PIN(cmd_mode) = 0.0;
      PIN(d_cmd)    = 0.0;
      PIN(su)       = 0.0;
      PIN(si)       = 0.0;
      PIN(timer)    = 0.0;
      PIN(state)    = 1.0;
      break;

    case 1: {  // rs
      float ki = MAX(PIN(rl_ki), RL_BW * ABS(PIN(d_cmd)) / PIN(test_cur));
      PIN(d_cmd) += ki * period * (PIN(test_cur) - PIN(id));
      if(PIN(timer) > PIN(rl_time) * 2.0 / 3.0) {
        PIN(su) += PIN(ud);
        PIN(si) += PIN(id);
      }
      if(PIN(timer) > PIN(rl_time)) {
        if(PIN(si) <= 0.0 || ABS(PIN(id) - PIN(test_cur)) > 0.1 * PIN(test_cur)) {
          PIN(state) = 6.0;
          break;
        }
        PIN(rs)      = PIN(su) / PIN(si);
        PIN(s11)     = 0.0;
        PIN(s1y)     = 0.0;
        PIN(last_id) = PIN(id);
        PIN(timer)   = 0.0;
        PIN(state)   = 2.0;
      }
    } break;

    case 2: {  // sigma * ls
      float x = (PIN(id) - PIN(last_id)) / period;
      float y = PIN(ud) - PIN(rs) * (PIN(id) + PIN(last_id)) / 2.0;
      PIN(last_id) = PIN(id);
      PIN(s11) += x * x;
      PIN(s1y) += x * y;
      float u0   = PIN(rs) * PIN(test_cur);
      PIN(d_cmd) = u0 * ((((int)(PIN(timer) / MAX(PIN(l_half), period))) & 1) ? 0.5 : 1.5);
      if(PIN(timer) > L_TIME) {
        PIN(sls)   = PIN(s11) > 0.0 ? PIN(s1y) / PIN(s11) : 0.0;
        PIN(d_cmd) = 0.0;
        PIN(timer) = 0.0;
        PIN(state) = 3.0;
      }
    } break;

    case 3:  // rest
      PIN(cmd_mode) = 0.0;
      PIN(d_cmd)    = 0.0;
      if(PIN(timer) > PIN(wait_time)) {
        PIN(fp)     = 0.0;
        PIN(fq)     = 0.0;
        PIN(spp)    = 0.0;
        PIN(spq)    = 0.0;
        PIN(sqq)    = 0.0;
        PIN(spi)    = 0.0;
        PIN(sqi)    = 0.0;
        PIN(spfp)   = 0.0;
        PIN(spfq)   = 0.0;
        PIN(sqfp)   = 0.0;
        PIN(sqfq)   = 0.0;
        PIN(tail_u) = 0.0;
        PIN(tail_i) = 0.0;
        PIN(tail_n) = 0.0;
        PIN(last_id) = PIN(id);
        PIN(timer)  = 0.0;
        PIN(state)  = 4.0;
      }
      break;

    case 4: {  // rotor: current step
      PIN(cmd_mode) = 1.0;
      PIN(d_cmd)    = PIN(test_cur);
      // the voltage was applied between the last and this current sample
      float p = PIN(ud) - PIN(sls) * (PIN(id) - PIN(last_id)) / period;
      float q = (PIN(id) + PIN(last_id)) / 2.0;
      float i = PIN(id);
      PIN(last_id) = PIN(id);
      PIN(spp) += p * p;
      PIN(spq) += p * q;
      PIN(sqq) += q * q;
      PIN(spi) += p * i;
      PIN(sqi) += q * i;
      PIN(fp) += p * period;
      PIN(fq) += q * period;
      PIN(spfp) += p * PIN(fp);
      PIN(spfq) += p * PIN(fq);
      PIN(sqfp) += q * PIN(fp);
      PIN(sqfq) += q * PIN(fq);
      if(PIN(timer) > PIN(step_time) * 0.8) {
        PIN(tail_u) += PIN(ud);
        PIN(tail_i) += PIN(id);
        PIN(tail_n) += 1.0;
      }
      if(PIN(timer) > PIN(step_time)) {
        float i_end = PIN(tail_i) / MAX(PIN(tail_n), 1.0);
        float rs    = PIN(tail_u) / MAX(PIN(tail_i), 1e-6);
        // with e = p - rs * q and phi = fp - rs * fq
        float phi   = PIN(fp) - rs * PIN(fq);
        float a     = phi / MAX(i_end, 1e-6);  // lm^2 / lr
        float see   = PIN(spp) - 2.0 * rs * PIN(spq) + rs * rs * PIN(sqq);
        float sei   = PIN(spi) - rs * PIN(sqi);
        float sephi = PIN(spfp) - rs * (PIN(spfq) + PIN(sqfp)) + rs * rs * PIN(sqfq);
        // per-sample products: the integrals above carry one period each
        float tau   = (a * sei - sephi) / MAX(see, 1e-12);
        if(a <= 0.0 || tau <= 0.0 || PIN(sls) <= 0.0) {
          PIN(state) = 6.0;
          break;
        }
        PIN(tau_r)    = tau;
        float lr      = PIN(sls) + a;
        PIN(lm)       = sqrtf(a * lr);
        PIN(ll)       = 2.0 * (lr - PIN(lm));
        PIN(rr)       = lr / tau;
        PIN(d_cmd)    = 0.0;
        PIN(cmd_mode) = 0.0;
        PIN(state)    = 5.0;
      }
    } break;

    case 5:  // done
    case 6:  // aborted
      PIN(en_out) = 0.0;
      PIN(d_cmd)  = 0.0;
      break;
  }
}

hal_comp_t idacim_comp_struct = {
    .name      = "idacim",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = 0,
    .pin_count = sizeof(struct idacim_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
