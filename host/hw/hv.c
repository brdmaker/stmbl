/*
 * Simulation stand-in for src/comps/hv.c: the HV board (F3 current loop,
 * inverter) plus the motor it drives.
 *
 * The pin names match the real hv component, so the drive's config templates
 * link to it unchanged. The F3 side runs the same current controller as
 * shared/comps/curpid.c at 20 kHz (current or voltage mode), and a PMSM model (as in
 * shared/comps/motsim.c) is integrated at 80 kHz. The sim_* pins describe the
 * simulated motor and supply; host/sim_board.txt links them to conf0 so the
 * simulated motor matches what the config says about it.
 */
#include "hv_comp.h"
#include "hal.h"
#include "defines.h"
#include "angle.h"
#include <math.h>

HAL_COMP(hv);

// same pins as src/comps/hv.c
HAL_PIN(d_cmd);
HAL_PIN(q_cmd);
HAL_PIN(pos);
HAL_PIN(vel);
HAL_PIN(en);
HAL_PIN(phase_mode);
HAL_PIN(cmd_mode);
HAL_PIN(r);
HAL_PIN(l);
HAL_PIN(psi);
HAL_PIN(cur_bw);
HAL_PIN(cur_ff);
HAL_PIN(cur_ind);
HAL_PIN(max_y);
HAL_PIN(max_cur);
HAL_PIN(dac);
HAL_PIN(dc_volt);
HAL_PIN(id_fb);
HAL_PIN(iq_fb);
HAL_PIN(ud_fb);
HAL_PIN(uq_fb);
HAL_PIN(abs_cur);
HAL_PIN(abs_volt);
HAL_PIN(duty);
HAL_PIN(hv_temp);
HAL_PIN(mot_temp);
HAL_PIN(core_temp);
HAL_PIN(fault);
HAL_PIN(ignore_fault_pin);
HAL_PIN(y);
HAL_PIN(u_fb);
HAL_PIN(v_fb);
HAL_PIN(w_fb);
HAL_PIN(rev);
HAL_PIN(pwm_volt);
HAL_PIN(uart_sr);
HAL_PIN(uart_dr);
HAL_PIN(crc_error);
HAL_PIN(scale);
HAL_PIN(state);
HAL_PIN(value);

// simulated motor
HAL_PIN(sim_r);      // phase resistance [ohm]
HAL_PIN(sim_l);      // phase inductance [H]
HAL_PIN(sim_psi);    // flux linkage [V*s/rad]
HAL_PIN(sim_pp);     // pole pairs
HAL_PIN(sim_j);      // inertia [kg*m^2]
HAL_PIN(sim_d);      // damping [Nm/(rad/s)]
HAL_PIN(sim_f);      // coulomb friction [Nm]
HAL_PIN(sim_load);   // external load torque [Nm]
HAL_PIN(sim_brake);  // holding brake torque [Nm], applied while not released
HAL_PIN(sim_brake_release);  // brake output (io0.out0), 1 = released
HAL_PIN(sim_phase_rev);  // phase order reversed (cancelled by rev = 1)
HAL_PIN(sim_udc);        // supply voltage [V]
HAL_PIN(sim_fb_delay);   // extra delay of id/iq/ud/uq_fb [rt cycles], 0..4
HAL_PIN(sim_cur_noise);  // current measurement noise, uniform +- [A]
// sim_ext = 1: the mechanics are simulated outside (host/arm). Before every
// rt cycle the host sets sim_pos and sim_vel; within the cycle the rotor
// advances at sim_vel, and sim_te_int accumulates te * dt for the host to
// read and clear. sim_j/d/f/load/brake are then unused.
HAL_PIN(sim_ext);
HAL_PIN(sim_te_int);

// simulated motor state
HAL_PIN(sim_pos);  // mechanical position [rad], -pi..pi
HAL_PIN(sim_vel);  // mechanical velocity [rad/s]
HAL_PIN(sim_acc);
HAL_PIN(sim_torque);  // electrical torque [Nm]
HAL_PIN(sim_id);      // motor frame currents [A]
HAL_PIN(sim_iq);

#define F3_PER_RT 4    // F3 current loop at 20 kHz
#define MOT_PER_F3 4   // motor model at 80 kHz

#define FB_DELAY_MAX 4

struct hv_ctx_t {
  float fb[FB_DELAY_MAX + 1][4];  // id, iq, ud, uq of the last cycles
  unsigned fb_pos;
  uint32_t rnd;  // noise generator state
  float id_error_sum;
  float iq_error_sum;
  float ud;  // F3 output voltage, rotor frame of the F3 angle
  float uq;
};

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct hv_pin_ctx_t *pins = (struct hv_pin_ctx_t *)pin_ptr;

  PIN(ignore_fault_pin) = 1;
  PIN(scale)            = 1;
  PIN(hv_temp)          = 30;
  PIN(core_temp)        = 30;

  PIN(sim_r)   = 1;
  PIN(sim_l)   = 0.01;
  PIN(sim_psi) = 0.05;
  PIN(sim_pp)  = 4;
  PIN(sim_j)   = 0.00003;
  PIN(sim_d)   = 0.0001;
  PIN(sim_f)   = 0.01;
  PIN(sim_udc) = 300;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct hv_ctx_t *ctx      = (struct hv_ctx_t *)ctx_ptr;
  struct hv_pin_ctx_t *pins = (struct hv_pin_ctx_t *)pin_ptr;

  // --- F4 side, as in src/comps/hv.c: what goes into the packet to the F3
  float d_cmd = PIN(d_cmd);
  float q_cmd = PIN(q_cmd);
  float pos   = PIN(pos);
  float vel   = PIN(vel);
  int en      = PIN(en) > 0.0;
  if(PIN(rev) > 0.0) {
    q_cmd *= -1.0;
    pos = minus(0, pos);
  }
  if(!en) {
    d_cmd = 0.0;
    q_cmd = 0.0;
  }

  // --- F3 side: curpid with the config the F4 sends
  float udc      = MAX(PIN(sim_udc), 0.0);
  float max_volt = udc / M_SQRT3 * 0.95;
  float r        = MAX(PIN(r), 0.1);
  float l        = MAX(PIN(l), 0.001);
  float psi      = PIN(psi);
  float cur_bw   = PIN(cur_bw);
  float max_cur  = MAX(PIN(max_cur) * PIN(scale), 0.01);

  // cmd_mode 1 (CURRENT_MODE): d/q_cmd are currents, clamped to max_cur;
  // cmd_mode 0 (VOLT_MODE, used by idpmsm): voltages, clamped to max_volt
  int volt_mode = PIN(cmd_mode) == 0.0;
  float abscmd  = d_cmd * d_cmd + q_cmd * q_cmd;
  float lim     = volt_mode ? max_volt : max_cur;
  float s       = CLAMP(sqrtf(lim * lim / MAX(abscmd, lim * 0.1)), 0.0, 1.0);
  d_cmd *= s;
  q_cmd *= s;

  // --- motor
  float mr   = MAX(PIN(sim_r), 0.001);
  float ml   = MAX(PIN(sim_l), 0.00001);
  float mpsi = PIN(sim_psi);
  float pp   = MAX(PIN(sim_pp), 1.0);
  float mj   = MAX(PIN(sim_j), 1e-7);
  float prev = PIN(sim_phase_rev) > 0.0 ? -1.0 : 1.0;

  float f3_dt  = period / F3_PER_RT;
  float mot_dt = f3_dt / MOT_PER_F3;
  float id = 0.0, iq = 0.0, ud = 0.0, uq = 0.0;

  for(int k = 0; k < F3_PER_RT; k++) {
    // the F3 advances the angle by the commanded velocity between packets
    float a = mod(pos + vel * f3_dt * k);
    float sa, ca;
    sincos_fast(a, &sa, &ca);

    // measured phase current in the F3's alpha/beta frame
    float sm, cm;
    sincos_fast(mod(PIN(sim_pos) * pp), &sm, &cm);
    float ia = PIN(sim_id) * cm - PIN(sim_iq) * sm;
    float ib = (PIN(sim_id) * sm + PIN(sim_iq) * cm) * prev;
    id       = ia * ca + ib * sa;
    iq       = -ia * sa + ib * ca;

    // predictor, as curpid with ksp = 1
    float indd = vel * l * iq;
    float indq = vel * (l * id + psi);
    float idp  = id + (ctx->ud - r * id + indd) / l * f3_dt;
    float iqp  = iq + (ctx->uq - r * iq - indq) / l * f3_dt;

    float id_error = d_cmd - idp;
    float iq_error = q_cmd - iqp;
    ud = LIMIT(PIN(cur_ff) * r * d_cmd - PIN(cur_ind) * indd + cur_bw * l * id_error, max_volt);
    uq = LIMIT(PIN(cur_ff) * r * q_cmd + PIN(cur_ind) * indq + cur_bw * l * iq_error, max_volt);
    if(cur_bw * r > 0.0) {
      ctx->id_error_sum = LIMIT(ctx->id_error_sum + cur_bw * r * id_error * f3_dt, max_volt - ud);
      ctx->iq_error_sum = LIMIT(ctx->iq_error_sum + cur_bw * r * iq_error * f3_dt, max_volt - uq);
    } else {
      ctx->id_error_sum = 0.0;
      ctx->iq_error_sum = 0.0;
    }
    ud += ctx->id_error_sum;
    uq += ctx->iq_error_sum;
    if(volt_mode) {
      ud                = d_cmd;
      uq                = q_cmd;
      ctx->id_error_sum = 0.0;
      ctx->iq_error_sum = 0.0;
    }
    if(!en) {
      ud                = 0.0;
      uq                = 0.0;
      ctx->id_error_sum = 0.0;
      ctx->iq_error_sum = 0.0;
    }
    ctx->ud = ud;
    ctx->uq = uq;

    // inverter output in alpha/beta, as the motor sees it
    float ua = ud * ca - uq * sa;
    float ub = (ud * sa + uq * ca) * prev;

    for(int m = 0; m < MOT_PER_F3; m++) {
      sincos_fast(mod(PIN(sim_pos) * pp), &sm, &cm);
      float mud = ua * cm + ub * sm;
      float muq = -ua * sm + ub * cm;
      float mid = PIN(sim_id);
      float miq = PIN(sim_iq);
      float we  = PIN(sim_vel) * pp;

      if(en) {
        PIN(sim_id) += (mud - mr * mid + we * ml * miq) / ml * mot_dt;
        PIN(sim_iq) += (muq - mr * miq - we * (ml * mid + mpsi)) / ml * mot_dt;
      } else {  // bridge off: no current path
        PIN(sim_id) = 0.0;
        PIN(sim_iq) = 0.0;
      }

      float te     = 1.5 * pp * mpsi * PIN(sim_iq);
      if(PIN(sim_ext) > 0.0) {
        PIN(sim_pos) = mod(PIN(sim_pos) + PIN(sim_vel) * mot_dt);
        PIN(sim_te_int) += te * mot_dt;
        PIN(sim_torque) = te;
        continue;
      }
      float tf     = PIN(sim_f) + (PIN(sim_brake_release) > 0.0 ? 0.0 : PIN(sim_brake));
      float torque = te + PIN(sim_load) - PIN(sim_vel) * PIN(sim_d);
      float v      = PIN(sim_vel);
      // coulomb friction: holds the rotor when the other torques can't beat it
      if(fabsf(v) < 1e-4 && fabsf(torque) <= tf) {
        PIN(sim_vel) = 0.0;
        PIN(sim_acc) = 0.0;
      } else {
        float acc = (torque - (fabsf(v) < 1e-4 ? SIGN(torque) : SIGN(v)) * tf) / mj;
        float nv  = v + acc * mot_dt;
        if(v * nv < 0.0) {  // friction stops it, it does not reverse it
          nv = 0.0;
        }
        PIN(sim_acc) = acc;
        PIN(sim_vel) = nv;
        PIN(sim_pos) = mod(PIN(sim_pos) + (v + nv) * 0.5 * mot_dt);
      }
      PIN(sim_torque) = te;
    }
  }

  // --- back to the F4, as src/comps/hv.c reports it, optionally delayed
  ctx->fb_pos = (ctx->fb_pos + 1) % (FB_DELAY_MAX + 1);
  if(PIN(sim_cur_noise) > 0.0) {  // xorshift32: same noise on every run
    for(int n = 0; n < 2; n++) {
      ctx->rnd = ctx->rnd ? ctx->rnd : 2463534242u;
      ctx->rnd ^= ctx->rnd << 13;
      ctx->rnd ^= ctx->rnd >> 17;
      ctx->rnd ^= ctx->rnd << 5;
      float e = ((float)(ctx->rnd >> 8) / 8388608.0f - 1.0f) * PIN(sim_cur_noise);
      if(n == 0) {
        id += e;
      } else {
        iq += e;
      }
    }
  }
  ctx->fb[ctx->fb_pos][0] = id;
  ctx->fb[ctx->fb_pos][1] = iq;
  ctx->fb[ctx->fb_pos][2] = ud;
  ctx->fb[ctx->fb_pos][3] = uq;
  unsigned dly = (unsigned)CLAMP(PIN(sim_fb_delay), 0, FB_DELAY_MAX);
  float *fb    = ctx->fb[(ctx->fb_pos + FB_DELAY_MAX + 1 - dly) % (FB_DELAY_MAX + 1)];
  id = fb[0];
  iq = fb[1];
  ud = fb[2];
  uq = fb[3];
  PIN(id_fb) = id;
  PIN(iq_fb) = iq;
  PIN(ud_fb) = ud;
  PIN(uq_fb) = uq;
  if(PIN(rev) > 0.0) {
    PIN(uq_fb) *= -1.0;
    PIN(iq_fb) *= -1.0;
  }
  PIN(abs_cur)  = sqrtf(id * id + iq * iq);
  PIN(abs_volt) = sqrtf(ud * ud + uq * uq);
  PIN(pwm_volt) = max_volt;
  PIN(duty)     = max_volt > 0.0 ? PIN(abs_volt) / max_volt : 0.0;
  PIN(dc_volt)  = udc;
  PIN(fault)    = 0.0;
  PIN(value)    = 1.0;
}

hal_comp_t hv_comp_struct = {
    .name      = "hv",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct hv_ctx_t),
    .pin_count = sizeof(struct hv_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
