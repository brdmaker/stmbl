#include "rlpsij_comp.h"
#include "commands.h"
#include "hal.h"
#include "math.h"
#include "defines.h"
#include "angle.h"

HAL_COMP(rlpsij);

// Motor identification. State sequence:
//   1  r: voltage mode, d axis locked, current regulated to test_cur;
//      r from a least-squares fit of ud = r * id + l * did/dt over all
//      samples, so a slow l/r (current still rising) does not bias it.
//   2  l: rotor still held, a voltage square wave around r * test_cur on the
//      d axis (voltage mode: applied unchanged over each rt period, no
//      back-EMF at standstill); l from a least-squares fit of
//      ud - r * id = l * did/dt. Unobservable when l / r is below ~1 rt period.
//   3  polecount: d-axis field rotated one electrical turn per second.
//   4  psi, j: drive segments up to +-max_vel (or for vel_time / 2), each
//      followed by a zero-current coast down to half speed (or for
//      vel_time / 8). The drive current starts at test_cur / 8 and adapts so
//      a segment lasts >= SEG_MIN (a light rotor would otherwise overshoot
//      max_vel within the current loop's own rise time).
//      psi from a least-squares fit of uq - r * iq = l' * diq/dt + psi * w_el
//      over all samples: the current never settles on a light rotor, and
//      without back-EMF feedforward the coast current is not zero either.
//      l' absorbs the 20 kHz loop's in-period voltage changes; not reported.
//      j, friction, load, damping from a least-squares fit of the momentum
//      equation of every drive segment and coast (measured current, no
//      acceleration estimate):
//      kt * int(iq) = j * dv + friction * int(sign v) + load * t + damping * int(v)
//   5  friction, damping, load again from the steady currents at speed steps
//      +-test_vel/10, +-test_vel, with a speed loop tuned from j / kt and the
//      state 4 fit as feedforward.
//   6  done, 7 aborted (overspeed, or nothing measured; output off).

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
HAL_PIN(com_vel);  // electrical velocity of com_pos, for hv0.vel (curpid predictor)
HAL_PIN(en_out);

HAL_PIN(en);
HAL_PIN(test_cur);
HAL_PIN(test_vel);
HAL_PIN(rl_ki);    // state 1 base integral gain [V/(A*s)]
HAL_PIN(rl_time);
HAL_PIN(vel_time);
HAL_PIN(vel_bw);   // state 5 speed loop bandwidth [rad/s]
HAL_PIN(max_vel);  // states 4/5 reverse torque at +-max_vel, abort at 2x [rad/s]
HAL_PIN(lpf);

HAL_PIN(timer);
HAL_PIN(multi_pos);
HAL_PIN(state);
HAL_PIN(vel);
HAL_PIN(acc);
HAL_PIN(last_pos);
HAL_PIN(error_sum);
// state 4 least squares, x1 = diq/dt, x2 = w_el, y = uq - r * iq
// (state 1: x1 = id, x2 = did/dt, y = ud; state 2: s11, s1y with
// x1 = did/dt, y = ud - r * id)
HAL_PIN(s11);
HAL_PIN(s12);
HAL_PIN(s22);
HAL_PIN(s1y);
HAL_PIN(s2y);
HAL_PIN(last_iq);
HAL_PIN(drive_cur);  // state 4: current magnitude of the drive segments
HAL_PIN(coast);      // state 4: 1 while coasting
HAL_PIN(seg_vel);    // state 4: velocity at the start of the segment/coast
HAL_PIN(seg_start);  // state 4: timer at the start of the segment/coast
HAL_PIN(seg_q);      // state 4: integral of iq since then
HAL_PIN(seg_s);      // state 4: integral of sign(vel)
HAL_PIN(seg_p);      // state 4: integral of vel
// State 4 momentum fit: normal equations of int(iq) = a . x with
// a = (dv, int(sign v), t, int(v)), x = (j, friction, load, damping) / kt;
// ls[0..9] = upper triangle of sum(a a'), ls[10..13] = sum(a * int(iq)).
HAL_PINA(ls, 14);
HAL_PIN(j_n);   // segments in the fit
HAL_PIN(jq);    // j / kt [A*s^2/rad]
HAL_PIN(ff_f);  // friction / kt [A], state 5 feedforward
HAL_PIN(ff_l);  // load / kt [A]
HAL_PIN(ff_d);  // damping / kt [A*s/rad]
HAL_PIN(vel_c); // state 5 speed loop feedback, lightly filtered
HAL_PINA(i, 4);

// State 1: the integral gain grows with the voltage reached (~r * test_cur),
// so the current settles in ~1/RL_BW s whatever the resistance.
#define RL_BW 20.0
// State 4: shortest drive segment, and how long a stalled rotor waits
// before the drive current is raised [s].
#define SEG_MIN 0.01
#define SEG_STALL 0.05
// State 2: half period and length of the voltage square wave [s].
#define L_HALF 0.005
#define L_TIME 0.2

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct rlpsij_pin_ctx_t *pins = (struct rlpsij_pin_ctx_t *)pin_ptr;
  PIN(test_cur) = 1.0;
  PIN(test_vel) = 2.0 * M_PI * 1.0;
  PIN(rl_ki) = 10.0;
  PIN(rl_time) = 1.0;
  PIN(vel_time) = 4.0;
  PIN(vel_bw) = 50.0;
  PIN(max_vel) = 10.0 * PIN(test_vel);
  PIN(lpf) = 0.9955;
}

// Add one momentum equation (see ls above).
static void seg_eq(struct rlpsij_pin_ctx_t *pins, float q, float dv, float s, float t, float p) {
  float a[4] = {dv, s, t, p};
  int k      = 0;
  for(int r = 0; r < 4; r++){
    for(int c = r; c < 4; c++){
      PINA(ls, k++) += a[r] * a[c];
    }
    PINA(ls, 10 + r) += a[r] * q;
  }
  PIN(j_n) += 1.0;
}

// Solve the normal equations; a parameter the data do not determine (pivot
// tiny against its own diagonal) is dropped and set to 0.
static void seg_solve(struct rlpsij_pin_ctx_t *pins, float x[4]) {
  float m[4][4], v[4], d0[4];
  int k = 0;
  for(int r = 0; r < 4; r++){
    for(int c = r; c < 4; c++){
      m[r][c] = m[c][r] = PINA(ls, k++);
    }
    v[r]  = PINA(ls, 10 + r);
    d0[r] = m[r][r];
  }
  int used[4] = {1, 1, 1, 1};
  for(int p = 0; p < 4; p++){
    if(m[p][p] <= 1e-6 * d0[p] || d0[p] <= 0.0){
      used[p] = 0;
      continue;
    }
    for(int r = p + 1; r < 4; r++){
      float f = m[r][p] / m[p][p];
      for(int c = p; c < 4; c++){
        m[r][c] -= f * m[p][c];
      }
      v[r] -= f * v[p];
    }
  }
  for(int p = 3; p >= 0; p--){
    x[p] = 0.0;
    if(!used[p]){
      continue;
    }
    float sum = v[p];
    for(int c = p + 1; c < 4; c++){
      sum -= m[p][c] * x[c];
    }
    x[p] = sum / m[p][p];
  }
}

static void abort_id(struct rlpsij_pin_ctx_t *pins) {
  PIN(q_cmd) = 0.0;
  PIN(d_cmd) = 0.0;
  PIN(en_out) = 0.0;
  PIN(state) = 7.0;
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
  PIN(com_vel) = 0.0;
  if(PIN(state) == 3.0){
    PIN(com_vel) = 2.0 * M_PI;
  } else if(PIN(state) >= 4.0 && PIN(state) <= 6.0){
    PIN(com_vel) = vel * PIN(polecount);
  }

  // Runaway guard for the spinning states (wrong commutation, reversed
  // feedback, bad psi). Uses the raw velocity: the filtered one lags ~20 ms,
  // which on a light rotor is hundreds of rad/s.
  if((PIN(state) == 4.0 || PIN(state) == 5.0) && ABS(vel) > 2.0 * PIN(max_vel)){
    abort_id(pins);
  }

  switch((int)PIN(state)){
    case 0: // disabled
      PIN(timer) = 0.0;
      PIN(en_out) = 0.0;

      if(PIN(en) > 0.0){
        PIN(r) = 0.0;
        PIN(s11) = 0.0;
        PIN(s12) = 0.0;
        PIN(s22) = 0.0;
        PIN(s1y) = 0.0;
        PIN(s2y) = 0.0;
        PIN(last_iq) = PIN(id_fb);
        PIN(d_cmd) = 0.0;
        PIN(q_cmd) = 0.0;
        PIN(state) = 1;
      }
    break;

    case 1: { // r, fb_offset
      PIN(en_out) = 1.0;
      PIN(com_pos) = 0.0;
      PIN(cmd_mode) = 0.0; // volt mode

      float ki = MAX(PIN(rl_ki), RL_BW * ABS(PIN(d_cmd)) / PIN(test_cur));
      PIN(d_cmd) += ki * period * (PIN(test_cur) - PIN(id_fb));

      // ud is applied unchanged over the period (voltage mode)
      float x1 = (PIN(id_fb) + PIN(last_iq)) / 2.0;
      float x2 = (PIN(id_fb) - PIN(last_iq)) / period;
      PIN(last_iq) = PIN(id_fb);
      PIN(s11) += x1 * x1;
      PIN(s12) += x1 * x2;
      PIN(s22) += x2 * x2;
      PIN(s1y) += x1 * PIN(ud_fb);
      PIN(s2y) += x2 * PIN(ud_fb);

      if(PIN(timer) > PIN(rl_time)){
        float det = PIN(s11) * PIN(s22) - PIN(s12) * PIN(s12);
        if(det > 1e-6 * PIN(s11) * PIN(s22)){
          PIN(r) = (PIN(s1y) * PIN(s22) - PIN(s2y) * PIN(s12)) / det;
        } else {  // current settles within a period: no di/dt to fit
          PIN(r) = PIN(s11) > 0.0 ? PIN(s1y) / PIN(s11) : 0.0;
        }
        PIN(fb_offset) = PIN(abs_pos_fb);
        PIN(s11) = 0.0;
        PIN(s1y) = 0.0;
        PIN(last_iq) = PIN(id_fb);
        PIN(timer) = 0.0;
        PIN(state) = 2.0;
      }
    } break;

    case 2: { // l
      PIN(com_pos) = 0.0;
      PIN(cmd_mode) = 0.0;
      float x = (PIN(id_fb) - PIN(last_iq)) / period;
      float y = PIN(ud_fb) - PIN(r) * (PIN(id_fb) + PIN(last_iq)) / 2.0;
      PIN(last_iq) = PIN(id_fb);
      PIN(s11) += x * x;
      PIN(s1y) += x * y;
      PIN(d_cmd) = PIN(r) * PIN(test_cur) * ((((int)(PIN(timer) / L_HALF)) & 1) ? 0.5 : 1.5);

      if(PIN(timer) > L_TIME){
        PIN(l) = PIN(s11) > 0.0 ? PIN(s1y) / PIN(s11) : 0.0;
        PIN(timer) = 0.0;
        PIN(state) = 3.0;
        PIN(polecount) = 0.0;
        PIN(d_cmd) = 0.0;
      }
    } break;

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
        PIN(q_cmd) = 0.0;
        PIN(com_pos) = 0.0;
        PIN(s11) = 0.0;
        PIN(s12) = 0.0;
        PIN(s22) = 0.0;
        PIN(s1y) = 0.0;
        PIN(s2y) = 0.0;
        PIN(last_iq) = PIN(iq_fb);
        PIN(drive_cur) = PIN(test_cur) / 8.0;
        for(int k = 0; k < 14; k++){
          PINA(ls, k) = 0.0;
        }
        PIN(j_n) = 0.0;
        PIN(coast) = 0.0;
        PIN(timer) = 0.0;
        PIN(state) = 4.0;
      }
    break;

    case 4: { // psi, l, j / kt: drive segments and coasts
      PIN(com_pos) = mod(minus(PIN(abs_pos_fb), PIN(fb_offset)) * PIN(polecount));

      float x1 = (PIN(iq_fb) - PIN(last_iq)) / period;
      float x2 = vel * PIN(polecount);
      float y  = PIN(uq_fb) - PIN(r) * PIN(iq_fb);
      PIN(last_iq) = PIN(iq_fb);
      PIN(s11) += x1 * x1;
      PIN(s12) += x1 * x2;
      PIN(s22) += x2 * x2;
      PIN(s1y) += x1 * y;
      PIN(s2y) += x2 * y;

      if(PIN(timer) > PIN(vel_time)){ // brake to standstill
        PIN(q_cmd) = -SIGN(vel) * PIN(drive_cur);
        if(ABS(vel) < PIN(test_vel) * 0.1 || PIN(timer) > PIN(vel_time) * 1.5){
          PIN(q_cmd) = 0.0;
          float det = PIN(s11) * PIN(s22) - PIN(s12) * PIN(s12);
          float x[4];
          seg_solve(pins, x);
          if(PIN(j_n) < 1.0 || x[0] <= 0.0 || det <= 0.0){
            abort_id(pins);
            break;
          }
          PIN(psi) = (PIN(s2y) * PIN(s11) - PIN(s1y) * PIN(s12)) / det;
          if(PIN(psi) <= 0.0){
            abort_id(pins);
            break;
          }
          float kt = 3.0 / 2.0 * PIN(polecount) * PIN(psi);
          PIN(jq) = x[0];
          PIN(ff_f) = x[1];
          PIN(ff_l) = x[2];
          PIN(ff_d) = x[3];
          PIN(j) = kt * x[0];
          PIN(friction) = kt * x[1];
          PIN(load) = kt * x[2];
          PIN(damping) = kt * x[3];
          PIN(vel_c) = vel;
          PIN(error_sum) = 0.0;
          for(int k = 0; k < 4; k++){
            PINA(i, k) = 0.0;
          }
          PIN(timer) = 0.0;
          PIN(state) = 5.0;
        }
        break;
      }

      if(PIN(coast) > 0.0){
        PIN(q_cmd) = 0.0;
        PIN(seg_q) += PIN(iq_fb) * period;
        PIN(seg_s) += SIGN(vel) * period;
        PIN(seg_p) += vel * period;
        if(ABS(vel) < ABS(PIN(seg_vel)) * 0.5 || PIN(timer) - PIN(seg_start) > PIN(vel_time) / 8.0){
          seg_eq(pins, PIN(seg_q), vel - PIN(seg_vel), PIN(seg_s), PIN(timer) - PIN(seg_start), PIN(seg_p));
          PIN(coast) = 0.0;
          PIN(q_cmd) = vel > 0.0 ? -PIN(drive_cur) : PIN(drive_cur);
          PIN(seg_vel) = vel;
          PIN(seg_start) = PIN(timer);
          PIN(seg_q) = 0.0;
          PIN(seg_s) = 0.0;
          PIN(seg_p) = 0.0;
        }
        break;
      }

      float t_seg = PIN(timer) - PIN(seg_start);
      if(PIN(q_cmd) == 0.0 ||
         (t_seg > SEG_STALL && ABS(vel - PIN(seg_vel)) < PIN(test_vel) * 0.5 && PIN(drive_cur) < PIN(test_cur))){
        // first segment, or stalled on friction: (re)start with more current
        if(PIN(q_cmd) != 0.0){
          PIN(drive_cur) = MIN(PIN(drive_cur) * 2.0, PIN(test_cur));
        }
        PIN(q_cmd) = PIN(q_cmd) < 0.0 ? -PIN(drive_cur) : PIN(drive_cur);
        PIN(seg_vel) = vel;
        PIN(seg_start) = PIN(timer);
        PIN(seg_q) = 0.0;
        PIN(seg_s) = 0.0;
        PIN(seg_p) = 0.0;
        break;
      }

      PIN(seg_q) += PIN(iq_fb) * period;
      PIN(seg_s) += SIGN(vel) * period;
      PIN(seg_p) += vel * period;

      // A heavy rotor that never reaches max_vel stops driving after vel_time / 2.
      if((PIN(q_cmd) > 0.0 && vel >= PIN(max_vel)) || (PIN(q_cmd) < 0.0 && vel <= -PIN(max_vel)) ||
         t_seg > PIN(vel_time) / 2.0){
        seg_eq(pins, PIN(seg_q), vel - PIN(seg_vel), PIN(seg_s), t_seg, PIN(seg_p));
        if(t_seg < SEG_MIN){
          PIN(drive_cur) *= 0.5;
        } else if(t_seg > SEG_MIN * 4.0){
          PIN(drive_cur) = MIN(PIN(drive_cur) * 2.0, PIN(test_cur));
        }
        PIN(coast) = 1.0;
        PIN(q_cmd) = 0.0;
        PIN(seg_vel) = vel;
        PIN(seg_start) = PIN(timer);
        PIN(seg_q) = 0.0;
        PIN(seg_s) = 0.0;
        PIN(seg_p) = 0.0;
      }
    } break;

    case 5: { // friction, damping, load
      float quarter = PIN(vel_time) / 4.0;
      int phase = CLAMP((int)(PIN(timer) / quarter), 0, 3);
      if(phase != CLAMP((int)((PIN(timer) - period) / quarter), 0, 3)){
        PIN(error_sum) = 0.0;  // the feedforward carries the new direction
      }
      float vel_cmd = PIN(test_vel) * ((phase & 1) ? 1.0 : 0.1) * ((phase & 2) ? -1.0 : 1.0);

      PIN(com_pos) = mod(minus(PIN(abs_pos_fb), PIN(fb_offset)) * PIN(polecount));

      // jq = j / kt, so this puts the speed loop crossover at vel_bw; the
      // state 4 fit as feedforward lets a friction-bound rotor break away.
      PIN(vel_c) = PIN(vel_c) * 0.9 + vel * 0.1;
      float kp = PIN(jq) * PIN(vel_bw);
      float error = vel_cmd - PIN(vel_c);
      float ff = PIN(ff_f) * SIGN(vel_cmd) + PIN(ff_l) + PIN(ff_d) * vel_cmd;
      PIN(error_sum) += kp * PIN(vel_bw) / 4.0 * period * error;
      PIN(error_sum) = LIMIT(PIN(error_sum), PIN(test_cur));
      PIN(q_cmd) = LIMIT(ff + kp * error + PIN(error_sum), PIN(test_cur));

      // second half of each phase: speed and current steady
      if(PIN(timer) - phase * quarter > quarter / 2.0){
        PINA(i, phase) = PINA(i, phase) * PIN(lpf) + PIN(iq_fb) * (1.0 - PIN(lpf));
      }

      if(PIN(timer) > PIN(vel_time)){
        float kt = 3.0 / 2.0 * PIN(polecount) * PIN(psi);
        float damping = (PINA(i, 1) - PINA(i, 0) - PINA(i, 3) + PINA(i, 2)) / 2.0 / (PIN(test_vel) * 0.9);
        PIN(friction) = ((PINA(i, 1) - PINA(i, 3)) / 2.0 - damping * PIN(test_vel)) * kt;
        PIN(load) = (PINA(i, 0) + PINA(i, 1) + PINA(i, 2) + PINA(i, 3)) / 4.0 * kt;
        PIN(damping) = damping * kt;

        PIN(q_cmd) = 0.0;
        PIN(d_cmd) = 0.0;
        PIN(state) = 6.0;
      }
    } break;

    case 6: // done
      PIN(com_pos) = mod(minus(PIN(abs_pos_fb), PIN(fb_offset)) * PIN(polecount));
      if(PIN(en) <= 0.0){
        PIN(state) = 0.0;
      }
    break;

    case 7: // aborted, output off, results invalid
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
