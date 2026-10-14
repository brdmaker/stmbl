#include "rlpsij_comp.h"
#include "commands.h"
#include "hal.h"
#include "math.h"
#include "defines.h"
#include "angle.h"

HAL_COMP(rlpsij);

HAL_PIN(r);
HAL_PIN(l);
HAL_PIN(psi);
HAL_PIN(j);
HAL_PIN(load);
HAL_PIN(friction);
HAL_PIN(damping);
HAL_PIN(polecount);
HAL_PIN(fb_offset);

HAL_PIN(cmd_mode);
HAL_PIN(q_cmd);
HAL_PIN(d_cmd);
HAL_PIN(iq_fb);
HAL_PIN(id_fb);
HAL_PIN(uq_fb);
HAL_PIN(ud_fb);

HAL_PIN(abs_pos_fb);
HAL_PIN(com_pos);
HAL_PIN(en_out);

HAL_PIN(en);
HAL_PIN(test_cur);
HAL_PIN(test_vel);
HAL_PIN(rl_ki);
HAL_PIN(vel_ki);
HAL_PIN(vel_kp);
HAL_PIN(rl_time);
HAL_PIN(vel_time);
HAL_PIN(max_vel);  // state 5 reverses torque at +-max_vel, aborts at 2x [rad/s]
HAL_PIN(lpf);

HAL_PIN(timer);
HAL_PIN(multi_pos);
HAL_PIN(state);
HAL_PIN(vel);
HAL_PIN(acc);
HAL_PIN(last_pos);
HAL_PIN(error_sum);
HAL_PIN(seg_vel);  // state 5: velocity at the last torque reversal
HAL_PIN(seg_t);    // state 5: net torque integral since then
HAL_PIN(seg_start);  // state 5: timer at the last reversal
HAL_PIN(j_n);      // state 5: segments averaged into j
HAL_PINA(i, 4);


static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct rlpsij_pin_ctx_t *pins = (struct rlpsij_pin_ctx_t *)pin_ptr;
  PIN(test_cur) = 1.0;
  PIN(test_vel) = 2.0 * M_PI * 1.0;
  PIN(rl_ki) = 10.0;
  PIN(vel_ki) = 0.01;
  PIN(vel_kp) = 0.1;
  PIN(rl_time) = 1.0;
  PIN(vel_time) = 4.0;
  PIN(max_vel) = 10.0 * PIN(test_vel);
  PIN(lpf) = 0.9955;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  // struct rlpsij_ctx_t * ctx = (struct rlpsij_ctx_t *)ctx_ptr;
  struct rlpsij_pin_ctx_t *pins = (struct rlpsij_pin_ctx_t *)pin_ptr;

  float vel = minus(PIN(abs_pos_fb), PIN(last_pos)) / period;
  float acc = (vel - PIN(vel)) / period;
  PIN(vel) = PIN(vel) * 0.99 + vel * (1.0 - 0.99);
  PIN(acc) = PIN(acc) * 0.99 + acc * (1.0 - 0.99);
  PIN(last_pos) = PIN(abs_pos_fb);

  PIN(timer) += period;

  float vel_cmd = 0.0;
  float error = 0.0;

  // Runaway guard for the spinning states (wrong commutation, reversed
  // feedback, bad psi). Uses the raw velocity: the filtered one lags ~20 ms,
  // which on a light rotor is hundreds of rad/s.
  if((PIN(state) == 4.0 || PIN(state) == 5.0) && ABS(vel) > 2.0 * PIN(max_vel)){
    PIN(q_cmd) = 0.0;
    PIN(d_cmd) = 0.0;
    PIN(en_out) = 0.0;
    PIN(state) = 7.0;
  }

  switch((int)PIN(state)){
    case 0: // disabled
      PIN(timer) = 0.0;
      PIN(en_out) = 0.0;
      
      if(PIN(en) > 0.0){
        PIN(state) = 1;
      }
    break;

    case 1: // r, fb_offset
      PIN(en_out) = 1.0;
      PIN(com_pos) = 0.0;
      PIN(cmd_mode) = 0.0; // volt mode

      PIN(d_cmd) += PIN(rl_ki) * period * (PIN(test_cur) - PIN(id_fb));

      if(PIN(id_fb) > PIN(test_cur) * 0.5){
        PIN(r) = PIN(r) * PIN(lpf) + PIN(ud_fb) / PIN(id_fb) * (1.0 - PIN(lpf));
      }

      if(PIN(timer) > PIN(rl_time)){
        PIN(fb_offset) = PIN(abs_pos_fb);
        PIN(timer) = 0.0;
        PIN(state) = 2.0;
      }
    break;

    case 2: // l
      PIN(timer) = 0.0;
      PIN(state) = 3.0;
      PIN(polecount) = 0.0;
      PIN(d_cmd) = 0.0;
    break;

    case 3: // polecount
      PIN(cmd_mode) = 1.0;
      PIN(d_cmd) = PIN(test_cur);

      PIN(com_pos) += 2.0 * M_PI * period;

      if(PIN(com_pos) >= M_PI){
        PIN(polecount) += 1.0;
        PIN(com_pos) = -M_PI;
      }
      if(PIN(timer) > 1.0 && ABS(minus(PIN(abs_pos_fb), PIN(fb_offset))) < 2.0 * M_PI * 0.01){
        PIN(d_cmd) = 0.0;
        PIN(com_pos) = 0.0;
        PIN(error_sum) = 0.0;
        PIN(timer) = 0.0;
        PIN(state) = 4.0;
      }
    break;

    case 4: // psi, friction, damping, load
      vel_cmd = PIN(test_vel) / 10.0;
      int phase = 0;
      if(PIN(timer) > PIN(vel_time) / 4.0){
        phase = 1;
        vel_cmd = PIN(test_vel);
      }
      if(PIN(timer) > PIN(vel_time) / 4.0 * 2.0)
      {
        phase = 2;
        vel_cmd = -PIN(test_vel) / 10.0;
      }
      if(PIN(timer) > PIN(vel_time) / 4.0 * 3.0)
      {
        phase = 3;
        vel_cmd = -PIN(test_vel);
      }

      PIN(com_pos) = mod(minus(PIN(abs_pos_fb), PIN(fb_offset)) * PIN(polecount));

      error = vel_cmd - PIN(vel);
      PIN(error_sum) += PIN(vel_ki) * period * (vel_cmd - PIN(vel));
      PIN(error_sum) = LIMIT(PIN(error_sum), PIN(test_cur));
      PIN(q_cmd) = PIN(vel_kp) * error + PIN(error_sum);
      PIN(q_cmd) = LIMIT(PIN(q_cmd), PIN(test_cur));

      if(ABS(vel_cmd - PIN(vel)) < PIN(test_vel) * 0.05){
        float bemf = PIN(uq_fb) - PIN(iq_fb) * PIN(r);
        PIN(psi) = PIN(psi) * PIN(lpf) + bemf / PIN(vel) / PIN(polecount) * (1.0 - PIN(lpf));

        PINA(i, phase) = PINA(i, phase) * PIN(lpf) + PIN(iq_fb) * (1.0 - PIN(lpf));        
      }

      if(PIN(timer) > PIN(vel_time)){
        PIN(load) = (PINA(i, 0) + PINA(i, 1) + PINA(i, 2) + PINA(i, 3)) / 4.0;
        PIN(damping) = (PINA(i, 1) - PINA(i, 0) - PINA(i, 3) + PINA(i, 2)) / 2.0 / (PIN(test_vel) * 0.9);
        PIN(friction) = (PINA(i, 1) - PIN(damping) * PIN(test_vel) - PINA(i, 3) - PIN(damping) * PIN(test_vel)) / 2.0;

        PIN(load) *= 3.0 / 2.0 * PIN(polecount) * PIN(psi);
        PIN(damping) *= 3.0 / 2.0 * PIN(polecount) * PIN(psi);
        PIN(load) *= 3.0 / 2.0 * PIN(polecount) * PIN(psi);

        PIN(q_cmd) = 0.0;
        PIN(j) = 0.0;
        PIN(j_n) = 0.0;
        PIN(timer) = 0.0;
        PIN(state) = 5.0;
      }
    break;

    case 5: { // j: torque steps, reversed at +-max_vel
      PIN(com_pos) = mod(minus(PIN(abs_pos_fb), PIN(fb_offset)) * PIN(polecount));

      if(PIN(timer) > PIN(vel_time)){ // brake to standstill
        PIN(q_cmd) = -SIGN(vel) * PIN(test_cur);
        if(ABS(vel) < PIN(test_vel) * 0.1 || PIN(timer) > PIN(vel_time) * 1.5){
          PIN(q_cmd) = 0.0;
          PIN(d_cmd) = 0.0;
          PIN(state) = 6.0;
        }
        break;
      }

      if(PIN(q_cmd) == 0.0){
        PIN(q_cmd) = PIN(test_cur);
        PIN(seg_vel) = vel;
        PIN(seg_t) = 0.0;
        PIN(seg_start) = PIN(timer);
        break;
      }

      float t = 3.0 / 2.0 * PIN(polecount) * PIN(psi) * PIN(iq_fb);
      t -= PIN(load);
      t -= PIN(friction) * SIGN(vel);
      t -= PIN(damping) * vel;
      PIN(seg_t) += t * period;

      // j = momentum change / velocity change over each segment, so no
      // acceleration estimate is needed. A heavy rotor that never reaches
      // max_vel reverses after vel_time / 2.
      if((PIN(q_cmd) > 0.0 && vel >= PIN(max_vel)) || (PIN(q_cmd) < 0.0 && vel <= -PIN(max_vel)) ||
         PIN(timer) - PIN(seg_start) > PIN(vel_time) / 2.0){
        float dv = vel - PIN(seg_vel);
        if(ABS(dv) > PIN(test_vel)){
          PIN(j) = (PIN(j) * PIN(j_n) + PIN(seg_t) / dv) / (PIN(j_n) + 1.0);
          PIN(j_n) += 1.0;
        }
        PIN(q_cmd) = -PIN(q_cmd);
        PIN(seg_vel) = vel;
        PIN(seg_t) = 0.0;
        PIN(seg_start) = PIN(timer);
      }
    } break;

    case 6: // done
      PIN(com_pos) = mod(minus(PIN(abs_pos_fb), PIN(fb_offset)) * PIN(polecount));
      if(PIN(en) <= 0.0){
        PIN(state) = 0.0;
      }
    break;

    case 7: // aborted: overspeed, output off, results invalid
      PIN(en_out) = 0.0;
      if(PIN(en) <= 0.0){
        PIN(state) = 0.0;
      }
    break;
  }
}

hal_comp_t rlpsij_comp_struct = {
    .name      = "rlpsij",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = 0,
    .pin_count = sizeof(struct rlpsij_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
