#include "idm_comp.h"
#include "hal.h"
#include "defines.h"
#include "angle.h"
#include <string.h>
#include <math.h>

HAL_COMP(idm);

HAL_PIN(en);

HAL_PIN(state);
HAL_PIN(sub_state);
HAL_PIN(timer);
HAL_PIN(acc_time);
HAL_PIN(vel_time);
HAL_PIN(time);

HAL_PIN(freq);
HAL_PIN(amp);
HAL_PIN(min_pos);
HAL_PIN(max_pos);
HAL_PIN(max_vel);
HAL_PIN(max_acc);

HAL_PIN(pos);
HAL_PIN(pos_cmd);
HAL_PIN(vel_cmd);
HAL_PIN(acc_cmd);

HAL_PIN(ji);
HAL_PIN(fi);
HAL_PIN(di);
HAL_PIN(li);

HAL_PIN(torque);
HAL_PIN(fb_torque);
// measured torque (pmsm_ttc0.torque_fb). When linked, the least-squares fit
// uses it instead of the torque command: the real torque lags the command
// (current loop), which made the fitted inertia read high.
HAL_PIN(torque_fb);

HAL_PIN(inertia_sum);
HAL_PIN(friction_sum);
HAL_PIN(damping_sum);
HAL_PIN(offset_sum);

HAL_PIN(inertia);
HAL_PIN(damping);
HAL_PIN(friction);
HAL_PIN(offset);

HAL_PIN(pos_bw);
HAL_PIN(vel_bw);
HAL_PIN(vel_d);

HAL_PIN(sys);

HAL_PIN(target);
HAL_PIN(auto_step);

// state 1.3: cycle the stroke speed through 1/n .. n/n of max_vel, so that
// damping (~vel) and friction (~sign(vel)) can be told apart. 0 = constant.
HAL_PIN(vel_steps);

// least-squares fit of torque = j*acc + d*vel + f*sign(vel) + o over states
// 1.2 and 1.3, solved once at the end
HAL_PIN(ls_j);
HAL_PIN(ls_d);
HAL_PIN(ls_f);
HAL_PIN(ls_o);
HAL_PIN(ls_n);
// ls_fb > 0: fit against measured motion (vel_fb, acc_fb, e.g. vel1.vel /
// vel1.acc with vel1.torque = 0) instead of the commanded motion. Measured
// motion stays valid when tracking is poor.
HAL_PIN(ls_fb);
HAL_PIN(vel_fb);
HAL_PIN(acc_fb);
// low-pass (Hz) applied identically to torque, acc, vel and sign(vel)
// before the fit. The model is linear in these signals, so equal filtering
// keeps it exact, while removing the noise of a differentiated acc_fb (noise
// in a regressor biases its coefficient toward 0). 0 = off.
HAL_PIN(ls_lp);
// sys mode: motor inertia (conf0.j); j_sys = fitted inertia - j_mot
HAL_PIN(j_mot);

#define LS_N 4
#define LS_BLOCK 256

struct idm_ctx_t {
  // normal equations, upper triangle of phi*phi' and phi*torque. Samples are
  // summed in float per block of LS_BLOCK and then added to the double
  // totals, which keeps float precision without per-sample double math.
  float blk_a[LS_N][LS_N];
  float blk_b[LS_N];
  int blk_count;
  double a[LS_N][LS_N];
  double b[LS_N];
  int stroke;
  float lpf[LS_N][2];  // two first-order stages per signal: acc, vel, sign, torque
};

static void ls_reset(struct idm_ctx_t *ctx) {
  memset(ctx, 0, sizeof(*ctx));
}

static void ls_flush(struct idm_ctx_t *ctx) {
  for(int i = 0; i < LS_N; i++) {
    for(int k = i; k < LS_N; k++) {
      ctx->a[i][k] += ctx->blk_a[i][k];
      ctx->blk_a[i][k] = 0.0;
    }
    ctx->b[i] += ctx->blk_b[i];
    ctx->blk_b[i] = 0.0;
  }
  ctx->blk_count = 0;
}

static void ls_add(struct idm_ctx_t *ctx, const float phi[LS_N], float y) {
  for(int i = 0; i < LS_N; i++) {
    for(int k = i; k < LS_N; k++) {
      ctx->blk_a[i][k] += phi[i] * phi[k];
    }
    ctx->blk_b[i] += phi[i] * y;
  }
  if(++ctx->blk_count >= LS_BLOCK) {
    ls_flush(ctx);
  }
}

// Solve the normal equations by Gaussian elimination with partial pivoting.
// Returns 0 if the system is singular (motion did not excite all terms).
static int ls_solve(struct idm_ctx_t *ctx, double x[LS_N]) {
  double m[LS_N][LS_N + 1];
  for(int i = 0; i < LS_N; i++) {
    for(int k = 0; k < LS_N; k++) {
      m[i][k] = i <= k ? ctx->a[i][k] : ctx->a[k][i];
    }
    m[i][LS_N] = ctx->b[i];
  }
  for(int c = 0; c < LS_N; c++) {
    int piv = c;
    for(int r = c + 1; r < LS_N; r++) {
      if(fabs(m[r][c]) > fabs(m[piv][c])) {
        piv = r;
      }
    }
    if(fabs(m[piv][c]) < 1e-12 * (fabs(ctx->a[c][c]) + 1e-30)) {
      return 0;
    }
    for(int k = 0; k <= LS_N; k++) {
      double t   = m[c][k];
      m[c][k]   = m[piv][k];
      m[piv][k] = t;
    }
    for(int r = 0; r < LS_N; r++) {
      if(r != c) {
        double f = m[r][c] / m[c][c];
        for(int k = c; k <= LS_N; k++) {
          m[r][k] -= f * m[c][k];
        }
      }
    }
  }
  for(int i = 0; i < LS_N; i++) {
    x[i] = m[i][LS_N] / m[i][i];
  }
  return 1;
}

// two cascaded first-order low-pass stages
static float ls_lp2(float state[2], float in, float k) {
  state[0] += (in - state[0]) * k;
  state[1] += (state[0] - state[1]) * k;
  return state[1];
}

// Add one sample. Samples near standstill are skipped: friction's sign is
// undefined there (pid and the motor both use a dead band).
static void ls_sample(float period, struct idm_ctx_t *ctx, struct idm_pin_ctx_t *pins) {
  float v = PIN(ls_fb) > 0.0 ? PIN(vel_fb) : PIN(vel_cmd);
  float a = PIN(ls_fb) > 0.0 ? PIN(acc_fb) : PIN(acc_cmd);
  float k = LP_HZ(PIN(ls_lp));
  // filter every sample (also skipped ones) so the filters stay continuous
  float fa = ls_lp2(ctx->lpf[0], a, k);
  float fv = ls_lp2(ctx->lpf[1], v, k);
  float fs = ls_lp2(ctx->lpf[2], SIGN(v), k);
  int tfb  = pins->torque_fb.source != &pins->torque_fb;  // linked
  float ft = ls_lp2(ctx->lpf[3], tfb ? PIN(torque_fb) : PIN(torque), k);
  if(ABS(v) < 0.02 * PIN(max_vel)) {
    return;
  }
  float phi[LS_N] = {fa, fv, fs, 1.0};
  ls_add(ctx, phi, ft);
  PIN(ls_n) += 1.0;
}

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct idm_ctx_t *ctx      = (struct idm_ctx_t *)ctx_ptr;
  struct idm_pin_ctx_t *pins = (struct idm_pin_ctx_t *)pin_ptr;
  ls_reset(ctx);
  PIN(fi) = 0.001;
  PIN(li) = 0.01;
  PIN(di) = 1.0;
  PIN(ji) = 10.0;
  PIN(pos_bw) = 5.0;
  PIN(vel_bw) = 40.0;
  PIN(vel_d) = 4.0;
  PIN(max_vel) = 25.0;
  PIN(max_acc) = 250.0;
  PIN(min_pos) = -10.0;
  PIN(max_pos) = 10.0;
  PIN(auto_step) = 1.4;
  PIN(inertia) = 0.0002;
  PIN(ls_lp) = 20.0;
  PIN(vel_steps) = 5.0;
  PIN(ls_fb) = 1.0;
}

static void nrt(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct idm_ctx_t *ctx      = (struct idm_ctx_t *)ctx_ptr;
  struct idm_pin_ctx_t *pins = (struct idm_pin_ctx_t *)pin_ptr;

  switch((int)(PIN(state) * 10.0 + 0.5)){
    case 0:
      //PIN(inertia) = 0.0001;
    break;
    
    case 10:
      PIN(state) = 1.1;
      PIN(timer) = 0.0;
      PIN(target) = PIN(min_pos);

      if(PIN(auto_step) >= 1){
        PIN(state) = 1.2;
      }
      else{
        printf("Measure friction, damping and inertia\n");
        printf("the motor will move\n");
        printf("idm0.state = 1.2 <font color='green'>to start</font>\n");
      }
    break;

    case 14: {
      // rt is finished (state 1.4 is idle there), so the totals are stable.
      // Report the least-squares fit; fall back to the adaptive estimates if
      // the fit is unusable (e.g. vel_fb/acc_fb not linked).
      float j = PIN(inertia);
      float d = PIN(damping);
      float f = PIN(friction);
      float o = PIN(offset);
      ls_flush(ctx);
      double x[LS_N];
      int ok = PIN(ls_n) >= 1000.0 && ls_solve(ctx, x) && x[0] > 0.0;
      if(ok) {
        PIN(ls_j) = x[0];
        PIN(ls_d) = MAX(x[1], 0.0);
        PIN(ls_f) = MAX(x[2], 0.0);
        PIN(ls_o) = x[3];
        j         = PIN(ls_j);
        d         = PIN(ls_d);
        f         = PIN(ls_f);
        o         = PIN(ls_o);
        if(PIN(sys) > 0.0) {
          // the fit sees motor + load inertia
          j = MAX(PIN(ls_j) - PIN(j_mot), 0.0);
        }
      } else {
        printf("<font color='red'>least-squares fit unusable (%.0f samples), "
               "using adaptive estimates</font>\n",
               PIN(ls_n));
        if(PIN(ls_fb) > 0.0) {
          printf("<font color='red'>check idm0.vel_fb / idm0.acc_fb links</font>\n");
        }
      }
      if(PIN(sys) > 0.0){
        printf("conf0.j_sys = %f <font color='green'># append to config</font>\n", j);
      }
      else{
        printf("conf0.j = %f <font color='green'># append to config</font>\n", j);
      }
      printf("conf0.o = %f <font color='green'># append to config</font>\n", o);
      printf("conf0.d = %f <font color='green'># append to config</font>\n", d);
      printf("conf0.f = %f <font color='green'># append to config</font>\n", f);
    }
      printf("done\n");
      if(PIN(sys) > 0.0){
        printf("continue with id_pid\n");
      }
      else{
        printf("continue with id_sys or id_pid\n");
      }
      PIN(state) = 1.5;
    break;
  }
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct idm_ctx_t *ctx      = (struct idm_ctx_t *)ctx_ptr;
  struct idm_pin_ctx_t *pins = (struct idm_pin_ctx_t *)pin_ptr;

  if(PIN(en) <= 0.0){
    PIN(state) = 0.0;
  }

  float to_go;
  float time_to_go;
  float acc;
  float vel;
  float max_acc;
  float max_vel;

  switch((int)(PIN(state) * 10.0 + 0.5)){
    case 0:
      PIN(timer) = 0.0;
      PIN(acc_cmd) = 0.0;
      PIN(vel_cmd) = 0.0;
      PIN(amp) = 0.0;

      if(PIN(en) > 0.0){
        PIN(state) = 1.0;
        PIN(sub_state) = 1.0;
        PIN(timer) = 0.0;

        PIN(inertia_sum) = 0.0;
        PIN(friction_sum) = 0.0;
        PIN(damping_sum) = 0.0;
        PIN(offset_sum) = 0.0;
        PIN(acc_time) = 0.0;
        PIN(vel_time) = 0.0;
        PIN(time) = 0.0;

        ls_reset(ctx);
        PIN(ls_n) = 0.0;
      }
    break;

    case 12: 
      max_acc = PIN(max_acc) / 5.0 * PIN(sub_state);
      max_vel = PIN(max_vel) / 5.0 * PIN(sub_state);

      PIN(pos) += PIN(vel_cmd) * period + PIN(acc_cmd) * period * period / 2.0;
      PIN(pos_cmd) = mod(PIN(pos));
      PIN(vel_cmd) += PIN(acc_cmd) * period;
      to_go = PIN(target) - PIN(pos);
      time_to_go = sqrtf(2.0 * ABS(to_go) / max_acc);
      acc = max_acc * SIGN(to_go);
      vel = acc * time_to_go;
      vel = LIMIT(vel, max_vel);
      acc = (vel - PIN(vel_cmd)) / period;
      PIN(acc_cmd) = LIMIT(acc, max_acc);

      if(time_to_go < period){
        time_to_go = 0.0;
        to_go = 0.0;
        PIN(acc_cmd) = 0.0;
        PIN(pos) = PIN(target);
        PIN(vel_cmd) = 0.0;
      }

      if(ABS(PIN(acc_cmd)) > 0.0){
        PIN(acc_time) += period;
        PIN(inertia_sum) += PIN(acc_cmd) * PIN(torque) * period;
      }
      
      if(ABS(PIN(vel_cmd)) > 0.0){
        PIN(vel_time) += period;
        PIN(damping_sum) += PIN(vel_cmd) * PIN(torque) * period;
        PIN(friction_sum) += SIGN(PIN(vel_cmd)) * PIN(torque) * period;
      }

      PIN(time) += period;
      PIN(offset_sum) += PIN(torque) * period;
      
      PIN(inertia) += period / PIN(ji) * PIN(fb_torque) * PIN(acc_cmd) * period;
      PIN(damping) += period / PIN(di) * PIN(fb_torque) * PIN(vel_cmd) * period;
      PIN(friction) += period / PIN(fi) * PIN(fb_torque) * SIGN(PIN(vel_cmd)) * period;
      PIN(offset) += period / PIN(li) * PIN(fb_torque) * period;

      ls_sample(period, ctx, pins);

      if(PIN(sys) > 0.0){
        PIN(inertia) = CLAMP(PIN(inertia), 0.0, 50.0);
      }
      else{
        PIN(inertia) = CLAMP(PIN(inertia), 0.0000001, 50.0);  // micro motors: 4e-7
      }
      PIN(damping) = CLAMP(PIN(damping), 0.0, 100.0);
      PIN(friction) = CLAMP(PIN(friction), 0.0, 100.0);
      PIN(offset) = CLAMP(PIN(offset), -100.0, 100.0);
    
      PIN(timer) += period;
      if(PIN(timer) < (ABS(PIN(max_pos) - PIN(min_pos)) / max_vel + 2.0 * max_vel / max_acc)){
        PIN(target) = PIN(max_pos);
      }
      else{
        PIN(target) = PIN(min_pos);
      }
      if(PIN(timer) > 2.0 * (ABS(PIN(max_pos) - PIN(min_pos)) / max_vel + 2.0 * max_vel / max_acc)){
        PIN(timer) = 0.0;
        PIN(sub_state)++;
      }

      if(PIN(sub_state) > 5.0){
        PIN(state) = 1.3;

        if(PIN(acc_time) > period){
          PIN(inertia_sum) /= PIN(acc_time);
        }

        if(PIN(vel_time) > period){
          PIN(friction_sum) /= PIN(vel_time);
          PIN(damping_sum) /= PIN(vel_time);
        }

        if(PIN(time) > period){
          PIN(offset_sum) /= PIN(time);
        }
      }
    break;

    case 13: // j, d, f
      max_vel = PIN(max_vel);
      if(PIN(vel_steps) >= 1.0){
        int n = (int)PIN(vel_steps);
        max_vel = PIN(max_vel) * (float)(ctx->stroke % n + 1) / (float)n;
      }

      PIN(pos) += PIN(vel_cmd) * period + PIN(acc_cmd) * period * period / 2.0;
      PIN(pos_cmd) = mod(PIN(pos));
      PIN(vel_cmd) += PIN(acc_cmd) * period;
      to_go = PIN(target) - PIN(pos);
      time_to_go = sqrtf(2.0 * ABS(to_go) / PIN(max_acc));
      acc = PIN(max_acc) * SIGN(to_go);
      vel = acc * time_to_go;
      vel = LIMIT(vel, max_vel);
      acc = (vel - PIN(vel_cmd)) / period;
      PIN(acc_cmd) = LIMIT(acc, PIN(max_acc));

      if(ABS(PIN(max_pos) - PIN(pos)) < 0.1 && PIN(target) != PIN(min_pos)){
        PIN(target) = PIN(min_pos);
        ctx->stroke++;
      }
      else if(ABS(PIN(min_pos) - PIN(pos)) < 0.1 && PIN(target) != PIN(max_pos)){
        PIN(target) = PIN(max_pos);
        ctx->stroke++;
      }

      PIN(inertia) += period / PIN(ji) * PIN(fb_torque) * PIN(acc_cmd) * period;
      PIN(damping) += period / PIN(di) * PIN(fb_torque) * PIN(vel_cmd) * period;
      PIN(friction) += period / PIN(fi) * PIN(fb_torque) * SIGN(PIN(vel_cmd)) * period;
      PIN(offset) += period / PIN(li) * PIN(fb_torque) * period;

      PIN(inertia) = CLAMP(PIN(inertia), 0.0000001, 50.0);
      PIN(damping) = CLAMP(PIN(damping), 0.0, 100.0);
      PIN(friction) = CLAMP(PIN(friction), 0.0, 100.0);
      PIN(offset) = CLAMP(PIN(offset), -100.0, 100.0);

      ls_sample(period, ctx, pins);

      PIN(timer) += period;
      if(PIN(timer) > 90.0){
        PIN(timer) = 0.0;
        PIN(acc_cmd) = 0.0;
        PIN(vel_cmd) = 0.0;
        PIN(amp) = 0.0;

        PIN(state) = 1.4;
      }
    break;
  }
}


hal_comp_t idm_comp_struct = {
    .name      = "idm",
    .nrt       = nrt,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct idm_ctx_t),
    .pin_count = sizeof(struct idm_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
