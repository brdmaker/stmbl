#include "acvf_comp.h"
#include "commands.h"
#include "hal.h"
#include "math.h"
#include "defines.h"
#include "angle.h"

// Sensorless V/f control for induction motors (conveyors, fans, pumps), with
// what an industrial inverter adds to plain V/f:
// - torque from the active power: p = 3/2 * u * id (the voltage is on the d
//   axis), minus the stator copper loss 3/2 * r * |i|^2, over the
//   synchronous speed. Unlike |i|, this does not count the magnetizing
//   current as load.
// - slip compensation: the frequency is raised by the slip that torque
//   causes, so the speed holds under load. With the motor model (rr, lm,
//   ll, e.g. from the acim autotune): slip_e = rr * torque / (3/2 * pp *
//   psir^2), with the rotor flux psir from the stator flux (u - r * i) / we;
//   it stays right when the flux changes (low frequency, field weakening).
//   Without (rr = 0): linear, slip_n at torque_n.
// - flux control: the voltage keeps the stator flux at its rated value
//   u_n / (vel_n * polecount), with the resistive drop r * i added, so the
//   motor keeps its torque at low frequency.
// - current limit: above lim_share * max_cur * scale the frequency is pulled back
//   (motoring) or pushed forward (braking), so a jam slows the belt instead
//   of tripping. The thermal derating (fault0.scale) lowers this limit; it
//   does not scale the voltage, which would collapse the flux and leave a hot
//   motor unable to start again. lim_share (0.9) keeps it below max_cur:
//   the F3 in voltage mode scales its voltage down above max_cur itself
//   (curpid.c), so a limit at max_cur would never see the current above it.
// - blocked: set while the limit has held the speed below half the command
//   for block_time (a blocked belt); for LinuxCNC (sserial in2).
// - active damping: the oscillating part of the torque (filtered at 50 Hz
//   minus filtered at lp) moves the frequency, damp * slip_n * vel_n per
//   torque_n. V/f with ir compensation hunts at light load on the larger
//   motors (370 W: 17 rad/s, 750 W: 79 rad/s peak to peak); damp 0.5..4
//   stops it, damp < 0 makes it worse.
// - speed / position estimate: vel = synchronous speed - slip, pos its
//   integral, for the LinuxCNC feedback (sserial_acvf template).
// Speeds are mechanical [rad/s], voltages and currents dq amplitudes.

HAL_COMP(acvf);

HAL_PIN(en);         // fault0.en_out
HAL_PIN(vel_cmd);    // speed command [rad/s] (ramped)
HAL_PIN(polecount);  // pole pairs
HAL_PIN(u_n);        // rated voltage [V] at vel_n
HAL_PIN(vel_n);      // rated synchronous speed [rad/s] (50 Hz, 4 pole: 157)
HAL_PIN(slip_n);     // rated slip, per unit (1 - rated / synchronous speed)
HAL_PIN(torque_n);   // rated torque [Nm]
HAL_PIN(r);          // stator resistance [ohm] (0: no ir and loss terms)
HAL_PIN(rr);         // rotor resistance [ohm] (0: nameplate slip model)
HAL_PIN(lm);         // magnetizing inductance [H]
HAL_PIN(ll);         // total leakage inductance [H] (split evenly)
HAL_PIN(u_boost);    // minimum voltage while running [V]
HAL_PIN(max_cur);    // current limit [A]
HAL_PIN(lim_ki);     // current limit gain [rad/s / (A*s)]
HAL_PIN(lim_share);  // the limit as a share of max_cur (below the F3's own clamp)
HAL_PIN(scale);      // current limit derating (fault0.scale)
HAL_PIN(block_time); // [s]
HAL_PIN(damp);       // active damping gain (see below)
HAL_PIN(max_volt);   // available voltage [V] (hv0.pwm_volt)
HAL_PIN(id);         // measured current, voltage frame (hv0.id_fb, iq_fb)
HAL_PIN(iq);
HAL_PIN(lp);         // current / torque filter [Hz]; slip compensation is
                     // positive feedback: at 20 Hz it hunts above ~100 rad/s

HAL_PIN(u_cmd);      // voltage command (hv0.d_cmd)
HAL_PIN(com_pos);    // electrical angle of the voltage (hv0.pos)
HAL_PIN(com_vel);    // its speed [rad/s el] (hv0.vel: the F3 interpolates)
HAL_PIN(flux);       // estimated rotor flux [V*s] (model only)
HAL_PIN(vel_sync);   // synchronous speed [rad/s]
HAL_PIN(slip);       // estimated slip [rad/s]
HAL_PIN(vel);        // estimated rotor speed [rad/s]
HAL_PIN(pos);        // estimated rotor position [rad], -pi..pi
HAL_PIN(torque);     // estimated torque [Nm]
HAL_PIN(load);       // torque / torque_n
HAL_PIN(cur);        // filtered |i| [A]
HAL_PIN(lim);        // current limit correction [rad/s]
HAL_PIN(blocked);    // 1: blocked (see above)
HAL_PIN(jtimer);

HAL_PIN(tfast);      // torque filtered at DAMP_HZ
HAL_PIN(tdamp);      // damping term [rad/s]
HAL_PIN(idf);
HAL_PIN(iqf);

#define DAMP_HZ 50.0

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct acvf_pin_ctx_t *pins = (struct acvf_pin_ctx_t *)pin_ptr;
  PIN(polecount) = 2.0;
  PIN(u_n)       = 230.0 * M_SQRT2 / M_SQRT3;
  PIN(vel_n)     = 2.0 * M_PI * 50.0 / 2.0;
  PIN(slip_n)    = 0.05;
  PIN(torque_n)  = 1.0;
  PIN(max_cur)   = 1.0;
  PIN(lim_ki)    = 2000.0;
  PIN(lim_share) = 0.9;
  PIN(scale)     = 1.0;
  PIN(max_volt)  = 1000.0;
  PIN(lp)        = 5.0;
  PIN(block_time) = 3.0;
  PIN(damp)       = 1.0;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct acvf_pin_ctx_t *pins = (struct acvf_pin_ctx_t *)pin_ptr;

  float pp    = MAX(PIN(polecount), 1.0);
  float vel_n = MAX(PIN(vel_n), 1.0);
  float r     = MAX(PIN(r), 0.0);
  float lp    = LP_HZ(PIN(lp));

  PIN(idf) = PIN(idf) * (1.0 - lp) + PIN(id) * lp;
  PIN(iqf) = PIN(iqf) * (1.0 - lp) + PIN(iq) * lp;
  float id  = PIN(idf);
  float iq  = PIN(iqf);
  float cur = sqrtf(id * id + iq * iq);
  PIN(cur)  = cur;

  if(PIN(en) <= 0.0) {
    PIN(u_cmd)    = 0.0;
    PIN(vel_sync) = 0.0;
    PIN(slip)     = 0.0;
    PIN(vel)      = 0.0;
    PIN(torque)   = 0.0;
    PIN(load)     = 0.0;
    PIN(lim)      = 0.0;
    PIN(blocked)    = 0.0;
    PIN(jtimer) = 0.0;
    return;
  }

  // torque from the air-gap power at the last synchronous speed
  float ws     = PIN(vel_sync);
  float p_gap  = 1.5 * (PIN(u_cmd) * PIN(id) - r * (PIN(id) * PIN(id) + PIN(iq) * PIN(iq)));
  float torque = ABS(ws) > 0.05 * vel_n ? p_gap / ws : 0.0;
  PIN(torque)  = PIN(torque) * (1.0 - lp) + torque * lp;
  float lpd    = LP_HZ(DAMP_HZ);
  PIN(tfast)   = PIN(tfast) * (1.0 - lpd) + torque * lpd;
  PIN(load)    = PIN(torque) / MAX(PIN(torque_n), 1e-6);

  float slip;
  if(PIN(rr) > 0.0 && PIN(lm) > 0.0 && ABS(ws) > 0.05 * vel_n) {
    // rotor flux: stator flux (u - r * i) / (j * we) minus the leakage
    // flux, scaled to the rotor (u is on d)
    float we  = ws * pp;
    float lr  = PIN(lm) + PIN(ll) / 2.0;
    float sls = lr - PIN(lm) * PIN(lm) / lr;  // sigma * ls (ls = lr)
    float fsd = -r * iq / we;
    float fsq = -(PIN(u_cmd) - r * id) / we;
    float frd = lr / PIN(lm) * (fsd - sls * id);
    float frq = lr / PIN(lm) * (fsq - sls * iq);
    float fr  = sqrtf(frd * frd + frq * frq);
    PIN(flux) = fr;
    slip      = PIN(rr) * PIN(torque) / (1.5 * pp * MAX(fr * fr, 1e-6)) / pp;
  } else {
    // linear in torque, slip_n at torque_n
    slip = PIN(load) * PIN(slip_n) * vel_n;
  }
  slip       = LIMIT(slip, 2.0 * PIN(slip_n) * vel_n);
  PIN(slip)  = slip;

  // current limit: back off the frequency while above max_cur
  float over = cur - PIN(lim_share) * PIN(max_cur) * CLAMP(PIN(scale), 0.0, 1.0);
  PIN(lim)   = MAX(PIN(lim) + PIN(lim_ki) * over * period, 0.0);
  PIN(lim)   = MIN(PIN(lim), ABS(PIN(vel_cmd)) + ABS(slip));

  float vel_cmd = PIN(vel_cmd);
  ws            = vel_cmd + slip;
  // motoring: lower |ws|; braking (torque against the motion): raise it
  float dir = SIGN(vel_cmd) * (PIN(torque) * vel_cmd >= 0.0 ? -1.0 : 1.0);
  ws += dir * PIN(lim);
  // active damping: the oscillating part of the torque moves the frequency
  PIN(tdamp) = -PIN(damp) * PIN(slip_n) * vel_n * (PIN(tfast) - PIN(torque)) / MAX(PIN(torque_n), 1e-6);
  ws += PIN(tdamp);
  PIN(vel_sync) = ws;

  // voltage: rated stator flux plus the resistive drop
  float we    = ws * pp;
  float psi_n = PIN(u_n) / (vel_n * pp);
  float e     = ABS(we) * psi_n;
  // The r * id term is faded in from e = u_boost to 3 * u_boost: at zero
  // frequency u = r * id would hold any dc current forever (a stalled motor
  // keeps heating, and never recovers once the thermal derating has cut the
  // current limit); there u_boost alone sets the current.
  float k     = CLAMP((e - PIN(u_boost)) / MAX(2.0 * PIN(u_boost), 1e-3), 0.0, 1.0);
  float u     = k * r * id + sqrtf(MAX(e * e - k * k * r * r * iq * iq, 0.0));
  u           = MAX(u, ws != 0.0 ? PIN(u_boost) : 0.0);
  u           = CLAMP(u, 0.0, PIN(max_volt));
  PIN(u_cmd)  = u;

  PIN(com_pos) = mod(PIN(com_pos) + we * period);
  PIN(com_vel) = we;
  PIN(vel)     = ws - slip;

  if(PIN(lim) > 0.5 * ABS(vel_cmd) && ABS(vel_cmd) > 0.05 * vel_n) {
    PIN(jtimer) += period;
  } else {
    PIN(jtimer) = 0.0;
  }
  PIN(blocked) = PIN(jtimer) > PIN(block_time);
  PIN(pos)     = mod(PIN(pos) + PIN(vel) * period);
}

hal_comp_t acvf_comp_struct = {
    .name      = "acvf",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = 0,
    .pin_count = sizeof(struct acvf_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
