/*
 * Current loop for a two-phase (bipolar) motor: a hybrid stepper driven by
 * two H-bridges, one per coil (brdstepper). The stepper is a two-phase PMSM
 * with many pole pairs (50 for 1.8 degree steps): the loop runs in the rotor
 * frame (d/q) from the encoder's electrical angle, like stmbl's three-phase
 * current loop (the F3's curpid), and drives each coil with its own bridge.
 *
 * Coil frame: coil A is alpha, coil B is beta.
 *   id =  ia cos(e) + ib sin(e)      ua = ud cos(e) - uq sin(e)
 *   iq = -ia sin(e) + ib cos(e)      ub = ud sin(e) + uq cos(e)
 *   torque = polepairs * psi * iq    (two phases: no 3/2 factor)
 *
 * Each bridge gives -udc..+udc to its coil, so the voltage vector's limit is
 * the circle inside that square, |u| <= udc (times max_duty).
 *
 * Plain C, no HAL: used by brdstepper's power stage component on the drive
 * and by the host simulator's stepper mode (host/hw/hv.c), so the simulator
 * tests the code that runs on the drive.
 */
#pragma once
#include <stdint.h>

typedef struct {
  float r, l, psi;    // coil resistance [ohm], inductance [H], flux linkage [V s / rad]
  float cur_bw;       // current loop bandwidth [rad/s]
  float cur_ff;       // resistive feedforward gain (0..1)
  float cur_ind;      // back-EMF / cross-coupling feedforward gain (0..1)
  float udc;          // bridge supply [V]
  float max_cur;      // current command limit [A]
  float max_duty;     // usable share of udc (< 1: room for current sampling)
  int volt_mode;      // 1: d/q commands are voltages (identification)
} bip_param_t;

typedef struct {
  float id_sum, iq_sum;  // integrators [V]
  float ud, uq;          // last output, for the predictor
} bip_state_t;

typedef struct {
  float id, iq;          // measured, rotor frame [A]
  float ud, uq;          // commanded, rotor frame [V]
  float ua, ub;          // coil voltages [V]
  float duty_a, duty_b;  // bridge duties, -1..1 (sign: current direction)
  float abs_cur, abs_volt;
} bip_out_t;

void bip_reset(bip_state_t *s);
// one loop period dt: electrical angle e [rad] and electrical speed w
// [rad/s] at the sampling instant, current commands (rotor frame), coil
// currents measured; en = 0: bridges off, integrators cleared
void bip_step(bip_state_t *s, const bip_param_t *p, float e, float w, float d_cmd, float q_cmd, float ia, float ib,
              int en, float dt, bip_out_t *o);

// Signed coil current from the DRV8876's current mirror (IPROPI), which
// only sees the LOW-side MOSFETs, and only current flowing drain to
// source: driving, it reads the current if it flows the way the bridge
// drives (else 0); braking (both low sides on, slow decay), it reads
// the magnitude. brdstepper samples both windows of each PWM period (the
// drive window centred at the counter's 0, the brake window at its top):
//   drive, brake    the two samples [A, >= 0]
//   duty            the duty those samples were taken with (its sign is the
//                   drive direction)
//   min_window      shortest window that can be sampled, as a share of the
//                   period (IPROPI settling + ADC sampling)
//   prev            a prediction of the current (the commanded coil current,
//                   bip_ref): its sign is used when the drive window is too
//                   short to tell (small duty, near the current's zero)
//   thr             noise floor [A]
float bip_sense(float drive, float brake, float duty, float min_window, float prev, float thr);
// the commanded coil currents: the rotor-frame command turned by angle e
void bip_ref(float e, float d_cmd, float q_cmd, float *ia, float *ib);
