#include "fb_switch_comp.h"
#include "commands.h"
#include "hal.h"
#include "math.h"
#include "defines.h"
#include "angle.h"

HAL_COMP(fb_switch);

HAL_PIN(polecount);
HAL_PIN(track_fb);
HAL_PIN(offset_first_enable);

HAL_PIN(pos_fb);
HAL_PIN(vel_fb);
HAL_PIN(com_fb);
HAL_PIN(joint_fb);
HAL_PIN(state);  // 0 = disabled, 1 = enabled

HAL_PIN(cmd_pos);

HAL_PIN(mot_pos);
HAL_PIN(mot_abs_pos);
HAL_PIN(mot_polecount);
HAL_PIN(mot_offset);
HAL_PIN(mot_state);  // 0 = disabled, 1 = inc, 2 = start abs, 3 = abs
HAL_PIN(mot_rev);
HAL_PIN(mot_fb_no_offset);
HAL_PIN(mot_abs_fb_no_offset);

HAL_PIN(plot_fb_pos);

HAL_PIN(com_pos);
HAL_PIN(com_abs_pos);
HAL_PIN(com_polecount);
HAL_PIN(com_offset);
HAL_PIN(com_state);
HAL_PIN(com_rev);
HAL_PIN(com_fb_no_offset);

HAL_PIN(joint_pos);
HAL_PIN(joint_abs_pos);
HAL_PIN(joint_offset);
HAL_PIN(joint_state);
HAL_PIN(joint_rev);
HAL_PIN(joint_fb_no_offset);

HAL_PIN(mot_joint_ratio);

HAL_PIN(phase_time);
HAL_PIN(phase_cur);
HAL_PIN(phase_gain);
HAL_PIN(phase_mode);      // 0: align (rotor follows a d current), 1: probe (default),
                          // 2: none (something else commutates: id_pmsm, rlpsij)
HAL_PIN(phase_pulse);     // probe: current pulse length [s]
HAL_PIN(phase_rest);      // probe: pause after each pulse [s]
HAL_PIN(phase_min_move);  // probe / check: least movement that counts [rad]
HAL_PIN(phase_error);     // 1: phasing found no angle or its check failed (fault0)
HAL_PIN(phase_check);     // result of the last check: + / - pulse movement [rad]
HAL_PIN(phase_fail);      // why phase_error: 1 nothing moved while probing, 2 the check
                          // moved it the wrong way (wrong angle), 3 the check could not move it
HAL_PIN(phase_amp);       // probe: current and pulse length of the last sweep
HAL_PIN(phase_len);
HAL_PIN(offset);
HAL_PIN(id);

HAL_PIN(current_com_pos);

HAL_PIN(en);

struct fb_switch_ctx_t {
  int32_t current_com_pos;
  float phase_start_pos;
  float cmd_com_offset;
  float cmd_mot_offset;
  float cmd_joint_offset;
  float com_offset;
  float phase_timer;
  int32_t phase_state;
  // probe phasing (current_com_pos 6) and the check (7)
  int32_t probe_k;
  float probe_t, probe_amp, probe_len, probe_pos0, probe_angle, probe_last, probe_still;
  float probe_target, probe_settle;  // the angle to turn to before the next pulse
  int32_t probe_on;                  // 1: in the pulse or its pause
  float probe_c, probe_s, probe_max, check_plus, check_minus;
};

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct fb_switch_ctx_t *ctx      = (struct fb_switch_ctx_t *)ctx_ptr;
  struct fb_switch_pin_ctx_t *pins = (struct fb_switch_pin_ctx_t *)pin_ptr;

  ctx->current_com_pos = 10;
  ctx->cmd_mot_offset      = 0.0;
  ctx->com_offset      = 0.0;
  PIN(phase_cur)       = 1.0;
  PIN(phase_time)      = 1.0;
  PIN(phase_gain)      = 100.0;
  PIN(phase_mode)      = 1.0;
  PIN(phase_pulse)     = 0.004;
  PIN(phase_rest)      = 0.016;
  PIN(phase_min_move)  = 0.002;
  PIN(offset_first_enable) = 1.0;
}

/*
 * Autophasing without commutation feedback, until the encoder index is
 * seen (or for ever without one). The rotor's electrical angle is unknown;
 * com_offset is the correction to the angle assumed from the motor
 * encoder. Two methods:
 *
 * align (phase_mode 0, the original): ramp a d current at the assumed
 * angle and turn the angle towards wherever the rotor goes. The rotor
 * moves up to half an electrical period, and if it cannot move (against a
 * mechanical stop, a brake, a stiff axis) the angle stays at its first
 * guess, up to 180 deg off.
 *
 * probe (phase_mode 1): short current pulses at PROBES angles around the
 * assumed one, each followed by a pause; the movement each causes is
 * correlated with the pulse angle. The fundamental's phase is the angle of
 * the most positive torque, the q axis, even when a stop blocks one
 * direction (that only halves it) or friction eats the small ones. The
 * pulses start at phase_cur / 8 for phase_pulse and grow while the rotor
 * moves less than 4 * phase_min_move: the current doubles up to
 * phase_cur, then the length quadruples up to phase_time / 4 (heavy
 * rotors, friction). Each pause lasts at least phase_rest and ends once
 * the rotor has stopped (at most 4 pulse lengths). A few micrometres of
 * travel in one direction are enough.
 *
 * The field never jumps: between pulses, with no current, it turns to the
 * next angle at PHASE_TURN rad/s and waits 4 ms (vel2 makes hv0.pos and
 * hv0.vel from com_fb; a jump was a speed spike, and with cur_ind the
 * decoupling then drove a q current the wrong way).
 *
 * Then, for both, the check: a pulse on +q and one on -q must not move the
 * rotor the wrong way, and one of them must move it the right way; else
 * phase_error (fault0: PHASING_ERROR) instead of running with a wrong angle.
 */
#define PROBES 16
#define PHASE_TURN 100.0  // rad/s, electrical

static void probe_start(struct fb_switch_ctx_t *ctx, struct fb_switch_pin_ctx_t *pins, float mot_pos, float amp, float len) {
  PIN(phase_amp) = amp;
  PIN(phase_len) = len;
  ctx->probe_k     = 0;
  ctx->probe_t     = 0.0;
  ctx->probe_amp   = amp;
  ctx->probe_len   = len;
  ctx->probe_pos0  = mot_pos;
  ctx->probe_last  = mot_pos;
  ctx->probe_still = 0.0;
  ctx->probe_target = 0.0;
  ctx->probe_settle = 0.0;
  ctx->probe_on     = 0;
  ctx->probe_c     = 0.0;
  ctx->probe_s     = 0.0;
  ctx->probe_max   = 0.0;
}

static void check_start(struct fb_switch_ctx_t *ctx, float mot_pos, float amp, float len) {
  ctx->current_com_pos = 7;
  ctx->probe_k         = 0;
  ctx->probe_t         = 0.0;
  ctx->probe_amp       = amp;
  ctx->probe_len       = len;
  ctx->probe_pos0      = mot_pos;
  ctx->probe_last      = mot_pos;
  ctx->probe_still     = 0.0;
  ctx->probe_target    = M_PI / 2.0;  // +q
  ctx->probe_settle    = 0.0;
  ctx->probe_on        = 0;
  ctx->check_plus      = 0.0;
  ctx->check_minus     = 0.0;
}

// one current pulse and its pause; returns 1 (and the movement) when done
static int pulse(struct fb_switch_ctx_t *ctx, struct fb_switch_pin_ctx_t *pins, float period, float mot_pos, float *moved) {
  if(!ctx->probe_on) {  // turn the field to the probe angle, no current
    float d    = minus(ctx->probe_target, ctx->probe_angle);
    float step = PHASE_TURN * period;
    if(ABS(d) > step) {
      ctx->probe_angle  = mod(ctx->probe_angle + SIGN(d) * step);
      ctx->probe_settle = 0.0;
      return 0;
    }
    ctx->probe_angle = ctx->probe_target;
    ctx->probe_settle += period;
    if(ctx->probe_settle < 0.004) {
      return 0;
    }
    ctx->probe_on    = 1;
    ctx->probe_t     = 0.0;
    ctx->probe_pos0  = mot_pos;
    ctx->probe_last  = mot_pos;
    ctx->probe_still = 0.0;
  }
  ctx->probe_t += period;
  if(ctx->probe_t < ctx->probe_len) {
    PIN(id) = ctx->probe_amp;
    return 0;
  }
  // the pause: at least phase_rest, then until the rotor has stopped (no
  // count for 10 ms, or a quarter pulse: a heavy rotor coasts slowly), at
  // most 4 pulse lengths
  if(ABS(minus(mot_pos, ctx->probe_last)) > 1e-6) {
    ctx->probe_last  = mot_pos;
    ctx->probe_still = 0.0;
  } else {
    ctx->probe_still += period;
  }
  float rest = ctx->probe_t - ctx->probe_len;
  if(rest < PIN(phase_rest) || (ctx->probe_still < MAX(0.01, ctx->probe_len / 4.0) && rest < 4.0 * MAX(ctx->probe_len, PIN(phase_rest)))) {
    return 0;
  }
  *moved          = minus(mot_pos, ctx->probe_pos0);
  ctx->probe_on   = 0;
  ctx->probe_settle = 0.0;
  return 1;
}

static void phasing_done(struct fb_switch_ctx_t *ctx, struct fb_switch_pin_ctx_t *pins, float mot_pos) {
  ctx->cmd_mot_offset  = minus(PIN(cmd_pos), mot_pos);
  ctx->current_com_pos = 4.0;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct fb_switch_ctx_t *ctx      = (struct fb_switch_ctx_t *)ctx_ptr;
  struct fb_switch_pin_ctx_t *pins = (struct fb_switch_pin_ctx_t *)pin_ptr;

  float mot_pos   = PIN(mot_pos);
  float com_pos   = PIN(com_pos);
  float joint_pos = PIN(joint_pos);

  float mot_abs_pos   = PIN(mot_abs_pos);
  float com_abs_pos   = PIN(com_abs_pos);
  float joint_abs_pos = PIN(joint_abs_pos);

  float mot_offset   = PIN(mot_offset);
  float com_offset   = PIN(com_offset);
  float joint_offset = PIN(joint_offset);


  if(PIN(mot_rev) > 0.0) {
    mot_pos *= -1.0;
    mot_abs_pos *= -1.0;
    mot_offset *= -1.0;
  }

  if(PIN(com_rev) > 0.0) {
    com_pos *= -1.0;
    com_abs_pos *= -1.0;
    com_offset *= -1.0;
  }

  if(PIN(joint_rev) > 0.0) {
    joint_pos *= -1.0;
    joint_abs_pos *= -1.0;
    joint_offset *= -1.0;
  }

  PIN(com_fb_no_offset)     = com_pos;
  PIN(mot_fb_no_offset)     = mot_pos;
  PIN(mot_abs_fb_no_offset) = mot_abs_pos;
  PIN(joint_fb_no_offset)   = joint_pos;

  PIN(id) = 0.0;

  if(PIN(joint_state) == 1.0 || PIN(joint_state) == 3.0){
    PIN(pos_fb) = mod(joint_pos + ctx->cmd_joint_offset);
    PIN(plot_fb_pos) = PIN(joint_fb);
  }
  else if(PIN(mot_state) == 1.0 || PIN(mot_state) == 3.0){
    PIN(pos_fb) = mod(mot_pos + ctx->cmd_mot_offset);
    PIN(plot_fb_pos) = PIN(pos_fb);
  }
  else if(PIN(com_state) == 1.0 || PIN(com_state) == 3.0){
    PIN(pos_fb) = mod(com_pos + ctx->cmd_com_offset);
    PIN(plot_fb_pos) = PIN(pos_fb);
  }

  if(PIN(mot_state) == 1.0 || PIN(mot_state) == 3.0){
    PIN(vel_fb) = mot_pos;
  }
  else if(PIN(com_state) == 1.0 || PIN(com_state) == 3.0){
    PIN(vel_fb) = com_pos;
  }

  if(PIN(en) <= 0.0) {
    PIN(state)           = 0.0;
    PIN(phase_error)     = 0.0;  // fault0 must not see the last one at the next enable
    ctx->current_com_pos = 10;
    ctx->com_offset      = 0.0;
    if((PIN(track_fb) <= 0.0 || PIN(offset_first_enable) > 0) && PIN(mot_state) > 0.0){
      PIN(offset_first_enable) = 0;
      ctx->cmd_com_offset      = minus(PIN(cmd_pos), com_pos);
      ctx->cmd_mot_offset      = minus(PIN(cmd_pos), mot_pos);
      ctx->cmd_joint_offset    = minus(PIN(cmd_pos), joint_pos);
    }
    PIN(plot_fb_pos)          = mod(mot_pos);
  } else {
    PIN(state) = 1.0;
    if(PIN(joint_state) >= 2.0 && ctx->current_com_pos > 3.0) {
      ctx->current_com_pos = 3; // joint fb absolute
      ctx->com_offset      = minus(mod((joint_abs_pos + joint_offset) * PIN(polecount) / PIN(mot_joint_ratio)), mod(mot_pos * PIN(polecount) / PIN(mot_polecount)));
    }
    if(PIN(com_state) >= 2.0 && ctx->current_com_pos > 2.0) {
      ctx->current_com_pos = 2; // com fb absolute
      ctx->com_offset      = minus(mod((com_abs_pos + com_offset) * PIN(polecount) / PIN(com_polecount)), mod(mot_pos * PIN(polecount) / PIN(mot_polecount)));
    }
    if(PIN(mot_state) >= 2.0 && ctx->current_com_pos > 1.0) {
      ctx->current_com_pos = 1; // mot fb absolute
      ctx->com_offset      = 0.0;
    }
    if(ctx->current_com_pos > 4.0) { // autophasing
      switch(ctx->current_com_pos) {
        case 5: // phasing
          ctx->phase_timer += period;
          PIN(id) = CLAMP(ctx->phase_timer / (MAX(PIN(phase_time), 0.1) / 4.0) * PIN(phase_cur), 0.0, PIN(phase_cur)); // ramp up id
          ctx->com_offset += minus(ctx->phase_start_pos, mot_pos) * PIN(phase_gain) * period;

          if(ctx->phase_timer >= MAX(PIN(phase_time), 0.1)) { // end
            ctx->phase_timer     = 0.0;
            // the field was at phase_start_pos; from here it follows mot_pos
            ctx->com_offset += mod((ctx->phase_start_pos - mot_pos) * PIN(polecount) / PIN(mot_polecount));
            ctx->probe_angle = 0.0;
            check_start(ctx, mot_pos, PIN(phase_cur) / 2.0, PIN(phase_pulse));
          }
        break;

        case 6: { // probe phasing
          float moved;
          if(pulse(ctx, pins, period, mot_pos, &moved)) {
            ctx->probe_c += moved * cosf(ctx->probe_angle);
            ctx->probe_s += moved * sinf(ctx->probe_angle);
            ctx->probe_max = MAX(ctx->probe_max, ABS(moved));
            ctx->probe_k++;
            ctx->probe_target = 2.0 * M_PI * ctx->probe_k / PROBES;
            if(ctx->probe_k >= PROBES) {
              float amp = ctx->probe_amp, len = ctx->probe_len;
              float max_len = MAX(PIN(phase_time), 0.1) / 4.0;
              if(ctx->probe_max < 4.0 * PIN(phase_min_move) && amp < PIN(phase_cur)) {
                probe_start(ctx, pins, mot_pos, MIN(amp * 2.0, PIN(phase_cur)), len);  // too little: stronger
              } else if(ctx->probe_max < 4.0 * PIN(phase_min_move) && len < max_len) {
                probe_start(ctx, pins, mot_pos, amp, MIN(len * 4.0, max_len));  // then longer
              } else if(ctx->probe_max < PIN(phase_min_move)) {
                PIN(phase_error) = 1.0;  // nothing moved at phase_cur and phase_time: blocked
                PIN(phase_fail)  = 1.0;
                ctx->current_com_pos = 8;
              } else {
                float est = mod(atan2f(ctx->probe_s, ctx->probe_c) - M_PI / 2.0);  // q at the peak
                ctx->probe_angle = mod(ctx->probe_angle + ctx->com_offset - est);  // field stays put
                ctx->com_offset  = est;
                check_start(ctx, mot_pos, amp, len);
              }
            }
          }
        } break;

        case 7: { // check: +q must not move it back, -q not forward, one of them must move it
          float moved;
          if(pulse(ctx, pins, period, mot_pos, &moved)) {
            if(ctx->probe_k++ == 0) {
              ctx->check_plus   = moved;
              ctx->probe_target = -M_PI / 2.0;
            } else {
              ctx->check_minus = moved;
              float m          = PIN(phase_min_move);
              float max_len    = MAX(PIN(phase_time), 0.1) / 4.0;
              PIN(phase_check) = ctx->check_plus - ctx->check_minus;
              if(ctx->check_plus <= -m || ctx->check_minus >= m) {  // the wrong way: wrong angle
                PIN(phase_error) = 1.0;
                PIN(phase_fail)  = 2.0;
                ctx->current_com_pos = 8;
              } else if(ctx->check_plus > m || ctx->check_minus < -m) {
                phasing_done(ctx, pins, mot_pos);
              } else if(ctx->probe_len < max_len) {  // nothing moved: longer pulses
                check_start(ctx, mot_pos, ctx->probe_amp, MIN(ctx->probe_len * 4.0, max_len));
              } else {
                PIN(phase_error) = 1.0;  // blocked: cannot tell
                PIN(phase_fail)  = 3.0;
                ctx->current_com_pos = 8;
              }
            }
          }
        } break;

        case 8: // failed: no current until the next enable
          break;

        default: // init
          ctx->phase_start_pos = mot_pos; // safe start pos
          PIN(com_fb) = 0.0;
          ctx->com_offset = 0.0;
          PIN(phase_error) = 0.0;
          PIN(phase_fail)  = 0.0;
          if(PIN(phase_mode) >= 2.0) {
            phasing_done(ctx, pins, mot_pos);
          } else if(PIN(phase_mode) > 0.0) {
            ctx->probe_angle = 0.0;
            probe_start(ctx, pins, mot_pos, PIN(phase_cur) / 8.0, PIN(phase_pulse));
            ctx->current_com_pos = 6.0;
          } else {
            ctx->current_com_pos = 5.0;
          }
      }
    }

    switch(ctx->current_com_pos) {
      case 4: // autophasing + mot fb -> com fb
        PIN(com_fb) = mod(mot_pos * PIN(polecount) / PIN(mot_polecount) + ctx->com_offset);
        break;

      case 6: // probe phasing, check: pulses at probe_angle from the current estimate
      case 7:
        PIN(state)  = 0.0;
        PIN(com_fb) = mod(mot_pos * PIN(polecount) / PIN(mot_polecount) + ctx->com_offset + ctx->probe_angle);
        break;

      case 3: // joint fb -> com fb
        if(PIN(joint_state) != 3.0) {
          PIN(com_fb) = mod(mot_pos * PIN(polecount) / PIN(mot_polecount) + ctx->com_offset); // tracking
        } else {
          PIN(com_fb) = mod((joint_abs_pos + joint_offset) * PIN(polecount));
        }
        break;

      case 2: // com fb -> com fb
        if(PIN(com_state) != 3.0) {
          PIN(com_fb) = mod(mot_pos * PIN(polecount) / PIN(mot_polecount) + ctx->com_offset); // tracking
        } else {
          PIN(com_fb) = mod((com_abs_pos + com_offset) * PIN(polecount) / PIN(com_polecount));
        }
        break;

      case 1: // mot fb -> com fb
        if(PIN(mot_state) != 3.0) {
          PIN(state) = 0.0;
        } else {
          PIN(com_fb) = mod((mot_abs_pos + mot_offset) * PIN(polecount) / PIN(mot_polecount));
        }
        break;

      default:
        PIN(state) = 0.0;
        PIN(com_fb) = mod(ctx->phase_start_pos * PIN(polecount) / PIN(mot_polecount) + ctx->com_offset);
    }
  }
  PIN(current_com_pos) = ctx->current_com_pos;
  PIN(offset) = ctx->com_offset;
}

hal_comp_t fb_switch_comp_struct = {
    .name      = "fb_switch",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct fb_switch_ctx_t),
    .pin_count = sizeof(struct fb_switch_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
