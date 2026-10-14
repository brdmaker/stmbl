#include "idpmsm_comp.h"
#include "hal.h"
#include "defines.h"
#include "angle.h"

HAL_COMP(idpmsm);

HAL_PIN(d_cmd);
HAL_PIN(q_cmd);
HAL_PIN(com_pos);
HAL_PIN(cmd_mode);
HAL_PIN(en);
HAL_PIN(en_out);

HAL_PIN(id_fb);
HAL_PIN(iq_fb);
HAL_PIN(ud_fb);
HAL_PIN(uq_fb);
HAL_PIN(pos_fb);
HAL_PIN(vel_fb);

HAL_PIN(state);
HAL_PIN(timer);

HAL_PIN(r);
HAL_PIN(l);

HAL_PIN(pp);
HAL_PIN(com_offset);
HAL_PIN(out_rev);
// conf0.mot_fb_rev. pos_fb comes reversed by fb_switch, and fb_switch also
// reverses mot_fb_offset, so the printed offset must be reversed too.
HAL_PIN(fb_rev);

HAL_PIN(test_cur);
HAL_PIN(test_vel);
HAL_PIN(ki);
HAL_PIN(vel_bw);

HAL_PIN(pi);

HAL_PIN(pwm_volt);

HAL_PIN(psi);

HAL_PIN(cur_bw);
HAL_PIN(cur_sum);
HAL_PIN(auto_step);

HAL_PIN(tmp0);
HAL_PIN(tmp1);
HAL_PIN(tmp2);
HAL_PIN(tmp3);
HAL_PIN(avg_test_volt);
HAL_PIN(l_half);  // L step: rt cycles per half of the square wave

// L step: current slope and voltage, summed per half (high / low voltage)
struct idpmsm_ctx_t {
  float prev_u;   // previous sample
  float prev_i;
  int run;        // samples in a row at the same voltage
  int n[2];       // [0] low, [1] high
  float di[2];    // sum of the current steps between samples
  float u[2];     // sum of the applied voltage
  float i[2];     // sum of the mean current between two samples
  unsigned cycle;
};

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  //struct idpmsm_ctx_t * ctx = (struct idpmsm_ctx_t *)ctx_ptr;
  struct idpmsm_pin_ctx_t *pins = (struct idpmsm_pin_ctx_t *)pin_ptr;
  PIN(l_half) = 8.0;
  PIN(test_cur) = 3.0;
  PIN(test_vel) = 50.0;
  PIN(ki) = 1.0;
  PIN(pi) = 1.0;
  PIN(vel_bw) = 20.0;
  PIN(cur_bw) = 1.0;
  PIN(auto_step) = 4.2;
}

static void nrt(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  //struct idpmsm_ctx_t * ctx = (struct idpmsm_ctx_t *)ctx_ptr;
  struct idpmsm_pin_ctx_t *pins = (struct idpmsm_pin_ctx_t *)pin_ptr;

  switch((int)(PIN(state) * 10.0 + 0.5)){
    case 0:
      PIN(r) = 0.1;
      PIN(l) = 0.001;
      PIN(psi) = 0.055;
      PIN(pp) = 3.0;
      PIN(com_offset) = 0.0;
      PIN(out_rev) = 0.0;
      PIN(cur_bw) = 1.0;
    break;

    case 10: // r
      PIN(state) = 1.1;
      PIN(timer) = 0.0;
      PIN(d_cmd) = 0.0;
      PIN(q_cmd) = 0.0;
      PIN(com_pos) = 0.0;
      PIN(cmd_mode) = 0.0;

      if(PIN(auto_step) >= 1){
        PIN(state) = 1.2;
      }
      else{
        printf("Measure r, l\n");
        printf("the motor can move a bit\n");
        printf("idpmsm0.state = 1.2 <font color='green'>to start</font>\n");
      }
    break;

    case 14:
      printf("conf0.r = %f <font color='green'># append to config</font>\n", PIN(r));
      printf("conf0.l = %f <font color='green'># append to config</font>\n", PIN(l));
      PIN(state) = 2.0;
    break;

    case 20: // pp, out_rev, com_offset
      PIN(state) = 2.1;
      PIN(timer) = 0.0;
      //PIN(d_cmd) = 0.0;
      PIN(q_cmd) = 0.0;
      PIN(com_pos) = 0.0;
      PIN(cmd_mode) = 0.0;

      if(PIN(auto_step) >= 2){
        PIN(state) = 2.2;
      }
      else{
        printf("Measure com_offset, polepairs, out_rev\n");
        printf("the motor will move\n");
        printf("idpmsm0.state = 2.2 <font color='green'>to start</font>\n");
      }
    break;

    case 25: // pp, out_rev, com_offset
      printf("conf0.polecount = %f <font color='green'># append to config</font>\n", PIN(pp));
      printf("conf0.mot_fb_offset = %f <font color='green'># append to config</font>\n", PIN(fb_rev) > 0.0 ? -PIN(com_offset) : PIN(com_offset));
      if(PIN(out_rev) > 0.0){
        printf("conf0.out_rev = 1 <font color='green'># append to config</font>\n");
      }
      PIN(state) = 3.0;
    break;

    case 30:
      PIN(state) = 3.1;
      PIN(timer) = 0.0;
      PIN(d_cmd) = 0.0;
      PIN(q_cmd) = 0.0;
      PIN(cur_sum) = 0.0;
      PIN(cmd_mode) = 0.0;
      PIN(cur_bw) = 250.0;

      if(PIN(auto_step) >= 3){
        PIN(state) = 3.2;
      }
      else{
        printf("Measure torque constant\n");
        printf("the motor will move\n");
        printf("id0.state = 3.2 to start\n");
      }
    break;

    case 33:
      printf("conf0.psi = %f <font color='green'># append to config</font>\n", PIN(psi));
      printf("done\n");
      printf("continue with id_mot\n");

      PIN(state) = 3.4;
    break;
  }
 
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct idpmsm_ctx_t * ctx = (struct idpmsm_ctx_t *)ctx_ptr;
  struct idpmsm_pin_ctx_t *pins = (struct idpmsm_pin_ctx_t *)pin_ptr;

  if(PIN(en) <= 0.0){
    PIN(state) = 0.0;
  }

  switch((int)(PIN(state) * 10.0 + 0.5)){
    case 0:
      PIN(en_out) = 0.0;
      PIN(cmd_mode) = 0.0;
      PIN(timer) = 0.0;
      PIN(d_cmd) = 0.0;
      PIN(q_cmd) = 0.0;
      PIN(cur_bw) = 1.0;

      if(PIN(en) > 0.0){
        PIN(state) = 1.0;
      }
    break;

    case 12: // r
      PIN(en_out) = 1.0;
      PIN(cmd_mode) = 1.0;
      PIN(cur_bw) = 1.0;
      PIN(q_cmd) = 0.0;
      PIN(com_pos) = 0.0;

      PIN(d_cmd) = PIN(test_cur);

      PIN(r) = PIN(r) * 0.99 + PIN(ud_fb) / MAX(PIN(id_fb), 0.01) * 0.01;

      PIN(timer) += period;
      if(PIN(timer) >= 2.0){
        PIN(timer) = 0.0;
        PIN(state) = 1.3;
        PIN(d_cmd) = 0.0;
        PIN(en_out) = 0.0;
        ctx->run   = 0;  // the L step's sums
        ctx->cycle = 0;
        for(int k = 0; k < 2; k++) {
          ctx->n[k]  = 0;
          ctx->di[k] = 0.0;
          ctx->u[k]  = 0.0;
          ctx->i[k]  = 0.0;
        }

        PIN(avg_test_volt) = PIN(test_cur) * MAX(PIN(r), 0.01);
        PIN(avg_test_volt) = LIMIT(PIN(avg_test_volt), PIN(pwm_volt) / 2.0);
      }
    break;

    case 13: // l
      PIN(en_out) = 1.0;
      PIN(cmd_mode) = 0.0;
      PIN(q_cmd) = 0.0;
      PIN(cur_bw) = 1.0;

      // Square wave in ud, l_half rt cycles at 1.5x then 0.5x the voltage that
      // drives test_cur. Between two samples at the same voltage,
      // l * di = period * (u - r * i), with i the mean current in between, so
      //   l = (u_high - u_low - r * (i_high - i_low)) / (di/dt_high - di/dt_low).
      // The r term is small when the halves are short against l / r, and a
      // delay of the current sample doesn't change a slope. Each sample's voltage is ud_fb, which comes with id_fb from the
      // F3; a pair of samples counts once the voltage has been the same for
      // three samples, so pairs that straddle a switch are left out.
      // (Before, ud toggled every cycle and l came from the two halves' mean
      // currents, which assumes the current is sampled exactly at the ripple
      // peaks: it read 2x to 4x high, depending on the sample timing.)
      {
        int half = MAX((int)PIN(l_half), 2);
        PIN(d_cmd) = PIN(avg_test_volt) * (((ctx->cycle++ / half) & 1) ? 0.5 : 1.5);

        float u = PIN(ud_fb);
        float i = PIN(id_fb);
        if(ABS(u - ctx->prev_u) < 0.05 * PIN(avg_test_volt)) {
          ctx->run++;
        } else {
          ctx->run = 0;
        }
        if(ctx->run >= 2) {
          int k = u > PIN(avg_test_volt);
          ctx->n[k]++;
          ctx->di[k] += i - ctx->prev_i;
          ctx->u[k] += u;
          ctx->i[k] += (i + ctx->prev_i) * 0.5;
        }
        ctx->prev_u = u;
        ctx->prev_i = i;
      }

      PIN(timer) += period;
      if(PIN(timer) >= 1.0){
        if(ctx->n[0] > 0 && ctx->n[1] > 0) {
          float ds = (ctx->di[1] / ctx->n[1] - ctx->di[0] / ctx->n[0]) / period;
          float du = ctx->u[1] / ctx->n[1] - ctx->u[0] / ctx->n[0];
          du -= PIN(r) * (ctx->i[1] / ctx->n[1] - ctx->i[0] / ctx->n[0]);
          if(ds > 0.0) {
            PIN(l) = du / ds;
          }
        }
        PIN(timer) = 0.0;
        PIN(state) = 1.4;
        PIN(d_cmd) = PIN(avg_test_volt);
        PIN(en_out) = 0.0;
      }
    break;

    case 22: // pp, out_rev
      PIN(en_out) = 1.0;
      PIN(cmd_mode) = 1.0;
      PIN(cur_bw) = 100.0;
      PIN(q_cmd) = 0.0;

      PIN(d_cmd) = PIN(test_cur);

      PIN(com_pos) += PIN(test_vel) * period;
      PIN(com_pos) = mod(PIN(com_pos));

      if(ABS(PIN(vel_fb)) > 0.1){
        PIN(pp) = PIN(pp) * 0.995 + PIN(test_vel) / PIN(vel_fb) * 0.005;
      }

      PIN(timer) += period;
      if(PIN(timer) >= 3.0){
        PIN(timer) = 0.0;
        
        if(PIN(pp) < 0.0){
          PIN(out_rev) = 1.0;
          PIN(pp) *= -1.0;
        }
        PIN(pp) = (int)(PIN(pp) + 0.5);

        PIN(tmp0) = 0.0;  // the offset step's average, as sin / cos
        PIN(tmp1) = 0.0;
        PIN(state) = 2.3;
      }
    break;

    case 23: // mot_fb_offset
      PIN(en_out) = 1.0;
      PIN(cmd_mode) = 1.0;
      PIN(cur_bw) = 100.0;
      PIN(q_cmd) = 0.0;

      PIN(d_cmd) = PIN(test_cur);

      PIN(com_pos) = 0.0;

      // average -pos_fb as a unit vector: a plain average of the angle mixes
      // +pi and -pi when the rotor locks near the wrap
      PIN(tmp0) = PIN(tmp0) * 0.99 - sinf(PIN(pos_fb)) * 0.01;
      PIN(tmp1) = PIN(tmp1) * 0.99 + cosf(PIN(pos_fb)) * 0.01;
      PIN(com_offset) = atan2f(PIN(tmp0), PIN(tmp1));

      PIN(timer) += period;
      if(PIN(timer) >= 2.0){
        PIN(en_out) = 0.0;  // was en, an input linked to fault0.en_out
        PIN(d_cmd) = 0.0;
        PIN(state) = 2.5;
        PIN(timer) = 0.0;
      }
    break;

    case 32: // psi
      PIN(en_out) = 1.0;
      PIN(cmd_mode) = 1.0;
      PIN(cur_bw) = 250.0;
      PIN(d_cmd) = 0.0;
      PIN(com_pos) = mod((PIN(pos_fb) + PIN(com_offset)) * PIN(pp));

      float vel_error = PIN(test_vel) - PIN(vel_fb);
      vel_error = LIMIT(vel_error, PIN(test_vel) / 100.0);
      PIN(cur_sum) += PIN(ki) * vel_error * period;

      PIN(q_cmd) = PIN(vel_bw) * period * vel_error + PIN(cur_sum);
      
      if(ABS(PIN(vel_fb)) > 0.1){
        float psi  = (PIN(uq_fb) - PIN(iq_fb) * PIN(r)) / (PIN(vel_fb) * PIN(pp));
        PIN(psi) = PIN(psi) * (1.0 - period / PIN(pi)) + psi * period / PIN(pi);
      }

      PIN(psi) = CLAMP(PIN(psi), 0.001, 1.0);

      PIN(timer) += period;
      if(PIN(timer) >= 5.0){
        PIN(timer) = 0.0;
        PIN(en_out) = 0.0;
        PIN(d_cmd) = 0.0;
        PIN(q_cmd) = 0.0;
        PIN(cur_sum) = 0.0;
        PIN(cmd_mode) = 0.0;

        PIN(state) = 3.3;
      }
    break;

    case 100:
      PIN(com_pos) = mod((PIN(pos_fb) + PIN(com_offset)) * PIN(pp));
  }
}


hal_comp_t idpmsm_comp_struct = {
    .name      = "idpmsm",
    .nrt       = nrt,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct idpmsm_ctx_t),
    .pin_count = sizeof(struct idpmsm_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};