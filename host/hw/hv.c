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
 *
 * sim_motor = 1: a two-phase hybrid stepper on two H-bridges (brdstepper)
 * instead of the HV board and a three-phase PMSM. The current loop is then
 * shared/bip_loop.c, the code brdstepper runs, at 20 kHz; the coils (A =
 * alpha, B = beta) and the detent torque (4 per electrical turn) are
 * integrated at 80 kHz; sim_ia / sim_ib are the coil currents.
 *
 * The DC bus (PMSM): sim_supply = 1 (host/sim_board.txt) rectifies the
 * mains (sim_mains V rms, sim_mains_hz) through a single-phase diode bridge
 * and the source resistance sim_mains_r straight into the drive's own bus
 * capacitors (sim_cap); the supply has no capacitors of its own. The bus
 * sags with ripple under load, and the energy a braking motor sends back
 * charges the capacitors: the diode bridge can't return it to the mains.
 * The F3 measures the bus filtered (io.c, ~1.3 ms), sets its voltage limit
 * and duties from that measurement, and switches off with HV_VOLT_ERROR
 * above ABS_MAX_VOLT (410 V) until the F4 disables it. A brake chopper
 * (the supply card's, sim_chop_r > 0) puts sim_chop_r across the bus from
 * sim_chop_on until it is back below sim_chop_off. sim_supply = 0: a stiff
 * sim_udc. Not modelled: the bridge's freewheel diodes charging the
 * bus from back-EMF above it while the bridge is off.
 *
 * Linear motors are simulated as rotary with one revolution = one electrical
 * period (e.g. 60 mm): j = mass * (period / 2pi)^2. sim_min_pos / sim_max_pos
 * (on sim_abs_pos, rad) are end stops: the carriage stops dead there, and
 * sim_stop_hits counts the contacts. Both 0: no stops.
 *
 * sim_type = 1 simulates an induction motor instead (T equivalent circuit,
 * stationary frame, states: stator current is and rotor flux psir):
 *   dpsir/dt = rr / lr * (lm * is - psir) + j * we * psir
 *   dis/dt   = (us - rs * is - lm / lr * dpsir/dt) / (sigma * ls)
 *   torque   = 3/2 * pp * lm / lr * (psir x is)
 * with rs = sim_r, rr = sim_rr, lm = sim_lm, leakage sim_l split evenly
 * between stator and rotor (ls = lr = lm + sim_l / 2), sigma = 1 - lm^2 /
 * (ls * lr), we = pp * sim_vel. sim_psi is not used. sim_id / sim_iq are
 * then the stator current in the rotor flux frame (magnetizing / torque).
 */
#include "hv_comp.h"
#include "hal.h"
#include "defines.h"
#include "angle.h"
#include "bip_loop.h"
#include "vbrake.h"
#include "common.h"
#include "f3hw.h"
#include "mtemp.h"
#include <math.h>
#include "version_number.h"

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
HAL_PIN(brake_on_loss);  // the F3 brakes if the F4 stops (stm32f303/src/comps/ls.c)
HAL_PIN(brake);          // the F4 brakes with a virtual resistor: no voltage-mode clamp on the F3
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
HAL_PIN(sim_brake_delay);    // [s] the brake grips / releases this long after the output
HAL_PIN(sim_brake_on);       // out: the brake grips (mechanically)
HAL_PIN(sim_phase_rev);  // phase order reversed (cancelled by rev = 1)
HAL_PIN(sim_udc);        // sim_supply 0 (and the stepper): the supply voltage [V]
HAL_PIN(sim_supply);     // 0: stiff sim_udc, 1: rectified mains into the drive's capacitors
HAL_PIN(sim_mains);      // mains [V rms] (0: lost)
HAL_PIN(sim_mains_hz);   // [Hz]
HAL_PIN(sim_mains_r);    // mains, rectifier and wiring resistance [ohm]
HAL_PIN(sim_cap);        // the drive's bus capacitance [F]
HAL_PIN(sim_bus_load);   // constant load on the bus [W] (the HV board's own supply)
HAL_PIN(sim_chop_r);     // brake chopper resistor [ohm] (0: none)
HAL_PIN(sim_chop_on);    // [V] it switches on above this ...
HAL_PIN(sim_chop_off);   // [V] ... and off below this
HAL_PIN(sim_chop_e);     // out: the energy it took [J]
HAL_PIN(sim_hv_fault);   // out: the F3's own last fault (over-voltage), kept (the F3 clears it on disable)
HAL_PIN(sim_vbus);       // out: the bus voltage [V] (dc_volt is the F3's filtered measurement)
HAL_PIN(sim_ibus);       // out: the current into the bridge [A], < 0: regeneration
HAL_PIN(sim_irect);      // out: the rectifier current [A]
HAL_PIN(sim_fb_delay);   // extra delay of id/iq/ud/uq_fb [rt cycles], 0..4
HAL_PIN(sim_cur_noise);  // current measurement noise, uniform +- [A]
HAL_PIN(sim_stop_min);   // hard stops on the unwrapped position sim_upos [rad];
HAL_PIN(sim_stop_max);   // both 0: none. Set sim_upos to start against one.
HAL_PIN(sim_motor);      // 0: PMSM on the HV board, 1: two-phase stepper on two H-bridges
HAL_PIN(sim_fault);      // an HV board fault code (common.h fault_t): reported, and the bridge off
HAL_PIN(sim_f4_dead);    // 1: the F4 stops talking (LV side lost); the F3 keeps the last packet
HAL_PIN(sim_f3_brake);   // out: the F3 brakes on its own (brake_on_loss)
HAL_PIN(sim_f3_gave_up); // out: ... and gave up (current not held; kept until the F4 is back)
HAL_PIN(sim_detent);     // stepper detent torque amplitude [Nm]
HAL_PIN(max_duty);       // stepper: usable share of the bridge supply
HAL_PIN(fw_version);     // out: the HV board's firmware version, as src/comps/hv.c (docs/firmware_versions.md)
HAL_PIN(fw_image);       // out: the one this LV firmware carries
HAL_PIN(fw_check);       // 1: they must match (HV_VERSION_ERROR), 0: don't check
HAL_PIN(sim_fw_version); // the simulated HV board's version (default this build's; 0: an HV board from before versioning)
HAL_PIN(sim_sense);      // stepper current sensing: 0 ideal, 1 DRV8876 IPROPI (low side only, see bip_sense)
HAL_PIN(sense_window);   // shortest PWM window that can be sampled, share of the period
HAL_PIN(sense_thr);      // IPROPI noise floor [A]
HAL_PIN(sim_rds);        // bridge resistance in series with each coil [ohm] (DRV8876: 0.35 + 0.35)
HAL_PIN(sim_sense_offset);  // IPROPI offset [A]: + on coil A, - on coil B (DRV8876: up to 7.5 mA)
HAL_PIN(sim_sense_gain);    // IPROPI gain error, share: + on A, - on B (DRV8876 RGT: up to 0.065)
HAL_PIN(sense_cal);         // IPROPI offset calibration at enable [PWM periods] (0: off)
HAL_PIN(sense_off_a);       // the offsets it found [A]
HAL_PIN(sense_off_b);
// the motor's temperature sensor (shared/mtemp.h): mot_temp is its resistance [ohm], as the F3 reports it
HAL_PIN(sim_temp);         // winding temperature [degC]
HAL_PIN(sim_temp_sensor);  // its sensor type (mtemp.h), 0: none fitted (the input reads open, -1)
HAL_PIN(sim_temp_wire);    // 0 ok, 1 open wire, 2 shorted
HAL_PIN(sim_temp_tnat);    // PTC / switch: trip temperature [degC]

HAL_PIN(sim_type);       // 0 = PMSM, 1 = induction motor
HAL_PIN(sim_rr);         // induction: rotor resistance [ohm]
HAL_PIN(sim_lm);         // induction: magnetizing inductance [H]
// sim_ext = 1: the mechanics are simulated outside (host/arm). Before every
// rt cycle the host sets sim_pos and sim_vel; within the cycle the rotor
// advances at sim_vel, and sim_te_int accumulates te * dt for the host to
// read and clear. sim_j/d/f/load/brake are then unused.
HAL_PIN(sim_ext);
HAL_PIN(sim_te_int);

// simulated motor state
HAL_PIN(sim_pos);  // mechanical position [rad], -pi..pi
HAL_PIN(sim_abs_pos);    // mechanical position [rad], not wrapped (double inside)
HAL_PIN(sim_min_pos);    // end stops on sim_abs_pos [rad] (both 0: none)
HAL_PIN(sim_max_pos);
HAL_PIN(sim_stop_hits);  // end stop contacts
HAL_PIN(sim_vel);  // mechanical velocity [rad/s]
HAL_PIN(sim_dist); // mechanical position, not wrapped [rad] (tests: how far a stop went)
HAL_PIN(sim_turn); // a hand on the shaft: turns it by this much [rad], at 1 rev/s, counting down to 0
HAL_PIN(sim_acc);
HAL_PIN(sim_upos);  // unwrapped mechanical position [rad] (sim_pos = mod(sim_upos))
HAL_PIN(sim_torque);  // electrical torque [Nm]
HAL_PIN(sim_id);      // motor frame currents [A]
HAL_PIN(sim_iq);
HAL_PIN(sim_ia);  // stepper coil currents [A]
HAL_PIN(sim_ib);
HAL_PIN(duty_a);  // stepper bridge duties, -1..1
HAL_PIN(duty_b);
HAL_PIN(sim_flux);    // induction: rotor flux magnitude [V*s]
HAL_PIN(sim_slip);    // induction: slip, electrical [rad/s]

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
  double upos;      // unwrapped position (float loses it after many turns)
  float upos_pin;   // what we last wrote to sim_upos (a script may set it)
  bip_state_t bip;  // stepper current loop
  float duty_a, duty_b;  // the duties the next samples are taken with
  float ia_meas, ib_meas;  // last reconstructed coil currents
  bip_cal_t cal;
  int was_en;
  float last_brake[2];  // raw brake-window samples (for the calibration)
  // the F3 on its own (sim_f4_dead)
  float last_pos;
  int last_en, last_flag, braking, brake_done;
  float brake_t;
  vbrake_t vb;
  float vscale;  // curpid's voltage-mode current clamp
  // the DC bus
  int bus_init;
  float vbus;      // [V] on the capacitors
  float udc_meas;  // [V] what the F3 measures (io.c)
  float mains_ph;  // mains phase, 0..1
  float ov_err;    // io.c's err_filter state
  int hv_fault;    // the F3's own fault (over-voltage), until the F4 disables it
  float hbrake_t;  // [s] since the holding brake's output changed
  int chop;        // the chopper is on
  double abs0;      // upos where sim_abs_pos is 0 (the start)
  int abs0_set;
  int at_stop;      // against sim_min_pos / sim_max_pos (sim_stop_hits)
  float isa, isb;  // induction motor: stator current, stationary frame
  float fra, frb;  // rotor flux, stationary frame
};

static float noise(struct hv_ctx_t *ctx, float amp) {  // xorshift32, uniform +- amp
  ctx->rnd = ctx->rnd ? ctx->rnd : 2463534242u;
  ctx->rnd ^= ctx->rnd << 13;
  ctx->rnd ^= ctx->rnd >> 17;
  ctx->rnd ^= ctx->rnd << 5;
  return ((float)(ctx->rnd >> 8) / 8388608.0f - 1.0f) * amp;
}

// what brdstepper measures of a coil current i driven with duty d
// (ref: the commanded coil current, for the sign when a window is too short;
// k: +1 coil A, -1 coil B, the sign of the offset and gain errors)
static float sense(struct hv_ctx_t *ctx, struct hv_pin_ctx_t *pins, float i, float d, float ref, float *prev, float k) {
  float n = PIN(sim_cur_noise);
  if(PIN(sim_sense) <= 0.0) {
    return i + (n > 0.0 ? noise(ctx, n) : 0.0);
  }
  // IPROPI: low side only, drain to source only (shared/bip_loop.h)
  // the mirror: low-side current drain to source, times (1 + gain error), plus offset, never negative
  float dir   = d >= 0.0 ? 1.0 : -1.0;
  float g     = 1.0 + k * PIN(sim_sense_gain), err = k * PIN(sim_sense_offset);
  float drive = MAX(MAX(i * dir, 0.0) * g + err + (n > 0.0 ? noise(ctx, n) : 0.0), 0.0);
  float brake = MAX(fabsf(i) * g + err + (n > 0.0 ? noise(ctx, n) : 0.0), 0.0);
  // the drive's side: the offset its calibration found, taken out
  float off   = k > 0 ? ctx->cal.off_a : ctx->cal.off_b;
  ctx->last_brake[k > 0 ? 0 : 1] = brake;
  drive       = bip_cal_apply(drive, off);
  brake       = bip_cal_apply(brake, off);
  *prev       = bip_sense(drive, brake, d, PIN(sense_window), ref, PIN(sense_thr));
  return *prev;
}

// the rotor: electrical torque te for dt, coulomb friction, brake, load
static void mech_step(struct hv_ctx_t *ctx, struct hv_pin_ctx_t *pins, float te, float mj, float dt) {
  if(PIN(sim_ext) > 0.0) {  // the host's mechanics (host/arm): it sets sim_pos and sim_vel
    PIN(sim_pos)     = mod(PIN(sim_pos) + PIN(sim_vel) * dt);
    PIN(sim_te_int) += te * dt;
    PIN(sim_torque)  = te;
    return;
  }
  float tf     = PIN(sim_f) + (PIN(sim_brake_on) > 0.0 ? PIN(sim_brake) : 0.0);
  float torque = te + PIN(sim_load) - PIN(sim_vel) * PIN(sim_d);
  float v      = PIN(sim_vel);
  if(PIN(sim_upos) != ctx->upos_pin) {  // set from outside
    ctx->upos     = PIN(sim_upos);
    ctx->upos_pin = PIN(sim_upos);
  }
  if(!ctx->abs0_set) {  // sim_abs_pos counts from where the rotor starts
    ctx->abs0     = ctx->upos;
    ctx->abs0_set = 1;
  }
  // coulomb friction: holds the rotor when the other torques can't beat it
  if(fabsf(v) < 1e-4 && fabsf(torque) <= tf) {
    PIN(sim_vel) = 0.0;
    PIN(sim_acc) = 0.0;
  } else {
    float acc = (torque - (fabsf(v) < 1e-4 ? SIGN(torque) : SIGN(v)) * tf) / mj;
    float nv  = v + acc * dt;
    if(v * nv < 0.0) {  // friction stops it, it does not reverse it
      nv = 0.0;
    }
    PIN(sim_acc) = acc;
    PIN(sim_vel) = nv;
    double was   = ctx->upos;
    ctx->upos += (v + nv) * 0.5 * dt;
    // a stiff, dead hard stop: the rotor stops there
    float lo = PIN(sim_stop_min), hi = PIN(sim_stop_max);
    if((lo != 0.0 || hi != 0.0) && (ctx->upos < lo || ctx->upos > hi)) {
      ctx->upos    = ctx->upos < lo ? lo : hi;
      PIN(sim_vel) = 0.0;
      PIN(sim_acc) = 0.0;
    }
    // a linear motor's end stops, on sim_abs_pos (from where it started)
    double alo = PIN(sim_min_pos), ahi = PIN(sim_max_pos);
    if(alo != 0.0 || ahi != 0.0) {
      double a = ctx->upos - ctx->abs0;
      if(a < alo || a > ahi) {
        ctx->upos    = ctx->abs0 + (a < alo ? alo : ahi);
        PIN(sim_vel) = 0.0;
        PIN(sim_acc) = 0.0;
        if(!ctx->at_stop) {
          PIN(sim_stop_hits) += 1.0;
        }
        ctx->at_stop = 1;
      } else {
        ctx->at_stop = 0;
      }
    }
    PIN(sim_dist) += (float)(ctx->upos - was);
    PIN(sim_upos) = ctx->upos_pin = (float)ctx->upos;
  }
  PIN(sim_pos)     = (float)(fmod(fmod(ctx->upos + M_PI, 2.0 * M_PI) + 2.0 * M_PI, 2.0 * M_PI) - M_PI);
  PIN(sim_abs_pos) = (float)(ctx->upos - ctx->abs0);
  PIN(sim_torque)  = te;
}

// sim_motor = 1: brdstepper's loop (shared/bip_loop.c) and a two-phase
// hybrid stepper; returns the rotor-frame feedback as the F3 path does
static void stepper_rt(struct hv_ctx_t *ctx, struct hv_pin_ctx_t *pins, float period, float d_cmd, float q_cmd,
                       float pos, float vel, int en, float *fb) {
  float mr     = MAX(PIN(sim_r) + MAX(PIN(sim_rds), 0.0), 0.001);  // coil + bridge
  float ml     = MAX(PIN(sim_l), 0.00001);
  float mpsi   = PIN(sim_psi);
  float pp     = MAX(PIN(sim_pp), 1.0);
  float mj     = MAX(PIN(sim_j), 1e-8);
  float prev   = PIN(sim_phase_rev) > 0.0 ? -1.0 : 1.0;
  float udc    = MAX(PIN(sim_udc), 0.0);
  float loop_dt = period / F3_PER_RT;
  float mot_dt  = loop_dt / MOT_PER_F3;
  bip_param_t bp = {.r = PIN(r), .l = PIN(l), .psi = PIN(psi), .cur_bw = PIN(cur_bw), .cur_ff = PIN(cur_ff),
                    .cur_ind = PIN(cur_ind), .udc = udc, .max_cur = MAX(PIN(max_cur) * PIN(scale), 0.0),
                    .max_duty = PIN(max_duty), .volt_mode = PIN(cmd_mode) == 0.0};
  bip_out_t o = {0};
  if(en && !ctx->was_en && PIN(sim_sense) > 0.0 && PIN(sense_cal) > 0.0) {
    bip_cal_start(&ctx->cal, (uint32_t)PIN(sense_cal));
  }
  ctx->was_en = en;
  for(int k = 0; k < F3_PER_RT; k++) {
    // what the drive measures and knows: coil currents, its own angle
    float e_k = mod(pos + vel * loop_dt * k);
    float ref_a, ref_b;
    bip_ref(e_k, d_cmd, q_cmd, &ref_a, &ref_b);
    float ia = sense(ctx, pins, PIN(sim_ia), ctx->duty_a, ref_a, &ctx->ia_meas, 1.0);
    float ib = sense(ctx, pins, PIN(sim_ib) * prev, ctx->duty_b, ref_b, &ctx->ib_meas, -1.0);
    int calibrating = bip_cal_sample(&ctx->cal, ctx->last_brake[0], ctx->last_brake[1]);
    // calibrating: both bridges brake (zero volts), the loop held off
    bip_step(&ctx->bip, &bp, mod(pos + vel * loop_dt * k), vel, d_cmd, q_cmd, ia, ib, en && !calibrating, loop_dt,
             &o);
    float ua = o.duty_a * udc, ub = o.duty_b * udc * prev;
    ctx->duty_a = o.duty_a;
    ctx->duty_b = o.duty_b;
    for(int m = 0; m < MOT_PER_F3; m++) {
      float e  = PIN(sim_pos) * pp;
      float se = sinf(e), ce = cosf(e);
      float we = PIN(sim_vel) * pp;
      if(en) {
        PIN(sim_ia) += (ua - mr * PIN(sim_ia) + mpsi * we * se) / ml * mot_dt;
        PIN(sim_ib) += (ub - mr * PIN(sim_ib) - mpsi * we * ce) / ml * mot_dt;
      } else {  // bridges off
        PIN(sim_ia) = 0.0;
        PIN(sim_ib) = 0.0;
      }
      PIN(sim_id) = PIN(sim_ia) * ce + PIN(sim_ib) * se;
      PIN(sim_iq) = -PIN(sim_ia) * se + PIN(sim_ib) * ce;
      float te    = pp * mpsi * PIN(sim_iq) - PIN(sim_detent) * sinf(4.0 * e);
      mech_step(ctx, pins, te, mj, mot_dt);
    }
  }
  PIN(duty_a)      = o.duty_a;
  PIN(duty_b)      = o.duty_b;
  PIN(sense_off_a) = ctx->cal.off_a;
  PIN(sense_off_b) = ctx->cal.off_b;
  PIN(pwm_volt) = udc * (PIN(max_duty) > 0.0 && PIN(max_duty) <= 1.0 ? PIN(max_duty) : 1.0);
  fb[0] = o.id;
  fb[1] = o.iq;
  fb[2] = o.ud;
  fb[3] = o.uq;
}

static void fw_init(struct hv_pin_ctx_t *pins) {
  PIN(fw_image)       = STMBL_FW_VERSION;
  PIN(sim_fw_version) = STMBL_FW_VERSION;
  PIN(fw_check)       = 1.0;
}

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct hv_pin_ctx_t *pins = (struct hv_pin_ctx_t *)pin_ptr;

  fw_init(pins);
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
  PIN(sim_mains)    = 230;
  PIN(sim_mains_hz) = 50;
  PIN(sim_mains_r)  = 1.0;
  PIN(sim_cap)      = 0.00054;
  PIN(sim_bus_load) = 2.0;
  PIN(sim_chop_on)  = 345;
  PIN(sim_chop_off) = 335;
  PIN(max_duty) = 0.95;
  PIN(sense_window) = 0.1;
  PIN(sense_thr)    = 0.01;
  PIN(sim_temp)      = 25;
  PIN(sim_temp_tnat) = 130;
  PIN(mot_temp)      = -1;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct hv_ctx_t *ctx      = (struct hv_ctx_t *)ctx_ptr;
  struct hv_pin_ctx_t *pins = (struct hv_pin_ctx_t *)pin_ptr;

  // the motor's temperature sensor
  int ts = (int)PIN(sim_temp_sensor), tw = (int)PIN(sim_temp_wire);
  PIN(mot_temp) = ts <= 0 || tw == 1 ? -1.0 : tw == 2 ? 0.5 : mtemp_r_of(ts, PIN(sim_temp), 10000.0, 3950.0, PIN(sim_temp_tnat));

  // the holding brake follows its output after sim_brake_delay
  int want = PIN(sim_brake_release) <= 0.0;
  if(want != (PIN(sim_brake_on) > 0.0)) {
    ctx->hbrake_t += period;
    if(ctx->hbrake_t >= PIN(sim_brake_delay)) {
      PIN(sim_brake_on) = want;
      ctx->hbrake_t     = 0.0;
    }
  } else {
    ctx->hbrake_t = 0.0;
  }

  // --- F4 side, as in src/comps/hv.c: what goes into the packet to the F3
  float d_cmd = PIN(d_cmd);
  float q_cmd = PIN(q_cmd);
  float pos   = PIN(pos);
  float vel   = PIN(vel);
  if(PIN(en) <= 0.0) {  // the F3 clears its faults when disabled
    ctx->hv_fault = NO_ERROR;
  }
  int en = PIN(en) > 0.0 && PIN(sim_fault) <= 0.0 && !ctx->hv_fault;  // the F3 switches off on its faults
  if(PIN(rev) > 0.0) {
    q_cmd *= -1.0;
    pos = minus(0, pos);
  }
  if(!en) {
    d_cmd = 0.0;
    q_cmd = 0.0;
  }

  // the F4 stopped (sim_f4_dead): the F3 keeps the last packet's frame; if
  // that packet enabled the drive and asked for it (brake_on_loss), the F3
  // brakes on its own as stm32f303/src/comps/ls.c does, else it switches off
  int f3_brake = 0;
  int volt_cmd = PIN(cmd_mode) == 0.0;
  if(PIN(sim_f4_dead) <= 0.0) {
    ctx->last_pos   = pos;
    ctx->last_en    = en;
    ctx->last_flag  = PIN(brake_on_loss) > 0.0;
    ctx->braking    = 0;
    ctx->brake_done = 0;
    PIN(sim_f3_gave_up) = 0.0;
  } else {
    pos   = ctx->last_pos;
    vel   = 0.0;
    d_cmd = 0.0;
    q_cmd = 0.0;
    if(ctx->last_flag && ctx->last_en && !ctx->brake_done && PIN(sim_fault) <= 0.0) {
      if(!ctx->braking) {
        ctx->braking = 1;
        ctx->brake_t = 0.0;
        vbrake_reset(&ctx->vb);
      }
      f3_brake = 1;
      volt_cmd = 1;
    } else {
      en = 0;
    }
  }
  PIN(sim_f3_brake) = f3_brake;

  float udc = MAX(PIN(sim_udc), 0.0);
  int mains = PIN(sim_supply) > 0.0 && PIN(sim_motor) <= 0.0 && PIN(sim_type) <= 0.5;  // the induction motor: sim_udc
  if(!ctx->bus_init) {  // charged: peak minus two diodes
    ctx->bus_init = 1;
    ctx->vbus     = mains ? MAX(PIN(sim_mains) * M_SQRT2 - 1.8, 0.0) : udc;
    ctx->udc_meas = ctx->vbus;
  }
  if(!mains) {
    ctx->vbus = udc;
  }
  float cap  = MAX(PIN(sim_cap), 1e-6);
  float id = 0.0, iq = 0.0, ud = 0.0, uq = 0.0;
  float max_volt;
  if(PIN(sim_motor) > 0.0) {
    float fb[4];
    stepper_rt(ctx, pins, period, d_cmd, q_cmd, pos, vel, en, fb);
    id = fb[0], iq = fb[1], ud = fb[2], uq = fb[3];
    max_volt = PIN(pwm_volt);
    goto feedback;
  }

  // --- F3 side: curpid with the config the F4 sends, on the bus it measures
  max_volt = ctx->udc_meas / M_SQRT3 * 0.95;
  float r        = MAX(PIN(r), 0.1);
  float l        = MAX(PIN(l), 0.001);
  float psi      = PIN(psi);
  float cur_bw   = PIN(cur_bw);
  float max_cur  = MAX(PIN(max_cur) * PIN(scale), 0.01);

  // cmd_mode 1 (CURRENT_MODE): d/q_cmd are currents, clamped to max_cur;
  // cmd_mode 0 (VOLT_MODE, used by idpmsm): voltages, clamped to max_volt
  int volt_mode = volt_cmd;
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
  int acim   = PIN(sim_type) > 0.5;
  float rr   = MAX(PIN(sim_rr), 0.001);
  float lm   = MAX(PIN(sim_lm), 0.0001);
  float lsr  = lm + ml / 2.0;  // ls = lr
  float sls  = lsr - lm * lm / lsr;  // sigma * ls
  float kr   = lm / lsr;

  float f3_dt  = period / F3_PER_RT;
  float mot_dt = f3_dt / MOT_PER_F3;

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
    if(acim) {
      ia = ctx->isa;
      ib = ctx->isb * prev;
    }
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
    if(f3_brake) {  // ls.c's brake, on the F3's own current measurement
      vbrake_cfg_t c = {.max_cur      = 0.9 * MIN(max_cur, ABS_MAX_CURRENT),
                        .pwm_volt     = max_volt,
                        .dc_volt      = ctx->udc_meas,
                        .high_dc_volt = 0.9 * ABS_MAX_VOLT,
                        .kp           = 0.1,
                        .ki           = 500.0,
                        .stop_cur     = 0.05 * max_cur,
                        .stop_time    = 0.1,
                        .l            = PIN(l),
                        .delay        = 1.5 * f3_dt,
                        .trip         = 1.3,
                        .trip_time    = 0.002};
      vbrake_step(&ctx->vb, &c, id, iq, f3_dt, &d_cmd, &q_cmd);
      float a = sqrtf(d_cmd * d_cmd + q_cmd * q_cmd);
      if(a > max_volt) {
        d_cmd *= max_volt / a;
        q_cmd *= max_volt / a;
      }
    }
    if(volt_mode) {
      // shared/comps/curpid.c in voltage mode: the command scaled down while
      // the current is above max_cur, unless braking (flags.brake, or the
      // F3's own brake)
      if(f3_brake || (PIN(brake) > 0.0 && PIN(sim_f4_dead) <= 0.0)) {
        ctx->vscale = 1.0;
      } else {
        ctx->vscale += (max_cur * max_cur - (id * id + iq * iq)) * 500.0 * f3_dt;
        ctx->vscale = CLAMP(ctx->vscale, 0.0, 1.0);
      }
      ud                = d_cmd * ctx->vscale;
      uq                = q_cmd * ctx->vscale;
      ctx->id_error_sum = 0.0;
      ctx->iq_error_sum = 0.0;
    } else {
      ctx->vscale = 1.0;
    }
    if(!en) {
      ud                = 0.0;
      uq                = 0.0;
      ctx->id_error_sum = 0.0;
      ctx->iq_error_sum = 0.0;
    }
    ctx->ud = ud;
    ctx->uq = uq;

    // inverter output in alpha/beta, as the motor sees it: the duties are
    // set for the measured bus, the bus that is there makes the voltage
    float ks = ctx->vbus / MAX(ctx->udc_meas, 1.0);
    float ua = (ud * ca - uq * sa) * ks;
    float ub = (ud * sa + uq * ca) * prev * ks;
    float uu = sqrtf(ua * ua + ub * ub), umax = ctx->vbus / M_SQRT3;
    if(uu > umax) {
      ua *= umax / uu;
      ub *= umax / uu;
    }

    for(int m = 0; m < MOT_PER_F3 && acim; m++) {
      float we = PIN(sim_vel) * pp;
      if(en) {
        float dfa = rr / lsr * (lm * ctx->isa - ctx->fra) - we * ctx->frb;
        float dfb = rr / lsr * (lm * ctx->isb - ctx->frb) + we * ctx->fra;
        ctx->isa += (ua - mr * ctx->isa - kr * dfa) / sls * mot_dt;
        ctx->isb += (ub - mr * ctx->isb - kr * dfb) / sls * mot_dt;
        ctx->fra += dfa * mot_dt;
        ctx->frb += dfb * mot_dt;
      } else {  // bridge off: no stator current, the rotor flux decays
        ctx->isa = 0.0;
        ctx->isb = 0.0;
        ctx->fra -= rr / lsr * ctx->fra * mot_dt;
        ctx->frb -= rr / lsr * ctx->frb * mot_dt;
      }
      // the rotor as for the PMSM (mech_step: friction, brake, load, stops);
      // the bus is the stiff sim_udc (the mains model is the PMSM's)
      mech_step(ctx, pins, 1.5 * pp * kr * (ctx->fra * ctx->isb - ctx->frb * ctx->isa), mj, mot_dt);
      float flux = sqrtf(ctx->fra * ctx->fra + ctx->frb * ctx->frb);
      PIN(sim_flux) = flux;
      if(flux > 1e-6) {
        float c = ctx->fra / flux, sn = ctx->frb / flux;
        PIN(sim_id) = ctx->isa * c + ctx->isb * sn;
        PIN(sim_iq) = -ctx->isa * sn + ctx->isb * c;
        PIN(sim_slip) = rr / lsr * lm * PIN(sim_iq) / flux;  // slip w_e - w_r
      }
    }

    for(int m = 0; m < MOT_PER_F3 && !acim; m++) {
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

      mech_step(ctx, pins, 1.5 * pp * mpsi * PIN(sim_iq), mj, mot_dt);

      // the bus: the bridge's power (< 0 regenerates), the rectifier
      if(mains) {
        float p    = en ? 1.5 * (mud * PIN(sim_id) + muq * PIN(sim_iq)) : 0.0;
        float vb   = MAX(ctx->vbus, 1.0);
        float vm   = PIN(sim_mains) * M_SQRT2 * fabsf(sinf(2.0 * M_PI * ctx->mains_ph));
        float ir   = MAX(vm - 1.8 - ctx->vbus, 0.0) / MAX(PIN(sim_mains_r), 0.01);
        float ib   = (p + MAX(PIN(sim_bus_load), 0.0)) / vb;
        if(ctx->vbus > PIN(sim_chop_on)) {
          ctx->chop = PIN(sim_chop_r) > 0.0;
        } else if(ctx->vbus < PIN(sim_chop_off)) {
          ctx->chop = 0;
        }
        if(ctx->chop) {
          ib += ctx->vbus / PIN(sim_chop_r);
          PIN(sim_chop_e) += ctx->vbus * ctx->vbus / PIN(sim_chop_r) * mot_dt;
        }
        ctx->vbus  = MAX(ctx->vbus + (ir - ib) * mot_dt / cap, 0.0);
        ctx->mains_ph += PIN(sim_mains_hz) * mot_dt;
        ctx->mains_ph -= floorf(ctx->mains_ph);
        PIN(sim_ibus)  = p / vb;
        PIN(sim_irect) = ir;
      }
    }

    // the F3's measurement (io.c: 0.05 per sample at 15 kHz) and over-voltage cut-off
    ctx->udc_meas += (ctx->vbus - ctx->udc_meas) * MIN(f3_dt / 0.00133, 1.0);
    if(err_filter(&ctx->ov_err, 5.0, 0.001, ctx->udc_meas > ABS_MAX_VOLT) && en) {
      ctx->hv_fault     = HV_VOLT_ERROR;
      PIN(sim_hv_fault) = HV_VOLT_ERROR;
      en                = 0;
    }
  }

  if(f3_brake) {
    ctx->brake_t += period;
    if(ctx->vb.stopped || ctx->vb.gave_up || ctx->brake_t > 2.0) {
      ctx->brake_done = 1;
      PIN(sim_f3_gave_up) = ctx->vb.gave_up;
    }
  }

feedback:
  if(PIN(sim_turn) != 0.0 && PIN(sim_brake_on) > 0.0 && PIN(sim_brake) > 0.0) {
    PIN(sim_turn) = 0.0;  // a closed brake: the hand can't turn it
  }
  if(PIN(sim_turn) != 0.0) {  // the hand wins over the motor
    float step = SIGN(PIN(sim_turn)) * MIN(fabsf(PIN(sim_turn)), 2.0 * M_PI * period);
    // on the unwrapped position: mech_step makes sim_pos from it
    ctx->upos += step;
    PIN(sim_upos) = ctx->upos_pin = (float)ctx->upos;
    PIN(sim_pos)  = (float)(fmod(fmod(ctx->upos + M_PI, 2.0 * M_PI) + 2.0 * M_PI, 2.0 * M_PI) - M_PI);
    PIN(sim_abs_pos) = (float)(ctx->upos - ctx->abs0);
    PIN(sim_dist) += step;
    PIN(sim_turn) -= step;
    PIN(sim_vel)  = 0.0;
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
  PIN(dc_volt)  = PIN(sim_motor) > 0.0 ? udc : ctx->udc_meas;
  PIN(sim_vbus) = PIN(sim_motor) > 0.0 ? udc : ctx->vbus;
  PIN(fault)    = PIN(sim_fault) > 0.0 ? PIN(sim_fault) : ctx->hv_fault;
  PIN(fw_version) = PIN(sim_fw_version);
  if(PIN(fw_check) > 0.0 && PIN(fw_version) != PIN(fw_image) && PIN(fault) == 0.0) {
    PIN(fault) = HV_VERSION_ERROR;  // as src/comps/hv.c: no enable with another HV firmware
  }
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
