/*
 * Collision detection for a serial arm (machine protection): a generalised
 * momentum observer (robot.h) compares the joint torques measured through
 * the motor currents with the arm's dynamic model; a residual beyond its
 * joint's threshold is a collision.
 *
 * Inputs per servo period: joint angles and speeds (the drives' feedback,
 * i.e. the motor side, in the model's q) and motor currents. The joint
 * torque from the motors is tau = T i, T = K^-T diag(kt) (gear ratios and
 * wrist coupling as in the arm file, kt = 1.5 * pole pairs * psi), minus
 * the friction model. Threshold of joint j:
 *   thr0 + thr_v |qd| + thr_s exp(-(qd / vs)^2) + thr_rel D + thr_rot A
 * D: peak of the dynamic torque, the larger of |tau - g(q)| and the
 * model's inertial torque |(M(q) qdd)_j| (which includes what the other
 * joints' accelerations push into joint j), A: peak of the rotors'
 * acceleration torque arm_jj |qdd| (qdd from the speed feedback, 5 ms
 * filter), both decaying over the envelope time (default 0.15 s: the
 * gears ring at their resonance after each jerk). The residual lags the
 * torque by
 * ~1 / k: when an acceleration reverses, the torque passes zero while the
 * residual of the model error still shows. And through elastic gears the
 * rotor accelerates ahead of the link at every jerk, which the rigid model
 * reads as an external torque of up to arm_jj |qdd| for ~15 ms.
 * (model errors grow with speed and with the dynamic torque - inertia
 * errors at full acceleration from standstill; near standstill stiction
 * holds an unknown part, up to ~fs, of the load); disarmed for holdoff
 * seconds after enable.
 *
 * Model file: the arm file's format (host/arm/arm_sim.c), lines used:
 *   dh J a alpha d theta0       link J m cx cy cz Ixx Iyy Izz Ixy Ixz Iyz
 *   motor J ratio jm ...        couple J M v        fric J fc fs vs fv
 *   kt J value                  thresh J thr0 thr_v [thr_s vs]
 *                               [Nm, Nm s/rad, Nm, rad/s]
 *   threl J value               thr_rel (relative model error, e.g. 0.2)
 *   thrrot J value              thr_rot (share of the rotor torque, ~1)
 *   envelope T                  decay time of D and A [s]
 *   payload M cx cy cz          payload on the last link [kg, m] (M 0: none);
 *                               an undeclared payload looks like a collision
 *   observer k holdoff eps      (gain [1/s], s, friction smoothing [rad/s])
 * Other lines (drive, gear, limits, home) are ignored.
 */
#ifndef COLLIDE_H
#define COLLIDE_H

#include "robot.h"

typedef struct {
  rb_model mdl;
  rb_fric fric[RB_MAXJ];
  double K[RB_MAXJ][RB_MAXJ];   // joint angle per motor angle
  double jm[RB_MAXJ];           // rotor inertias
  double kt[RB_MAXJ];           // motor torque per amp [Nm/A]
  double T[RB_MAXJ][RB_MAXJ];   // joint torque per motor current
  double thr0[RB_MAXJ], thr_v[RB_MAXJ], thr_s[RB_MAXJ], thr_vs[RB_MAXJ], thr_rel[RB_MAXJ];
  double thr_rot[RB_MAXJ];
  double holdoff;               // [s]
  double envelope;              // decay time of the dynamic threshold terms [s]
  double pay_m, pay_c[3];       // payload now in the model
  rb_observer obs;

  // state / outputs
  double tau[RB_MAXJ];          // measured joint torque (motors)
  double tau_f[RB_MAXJ];        // friction model
  double r[RB_MAXJ];            // residual = external torque estimate [Nm]
  double thr[RB_MAXJ];          // threshold in use
  double dyn[RB_MAXJ];          // D, the decaying peak of |tau - g|
  double rot[RB_MAXJ];          // A, the decaying peak of arm_jj |qdd|
  double qd_prev[RB_MAXJ];      // speed feedback of the last step
  double qdd_f[RB_MAXJ];        // filtered acceleration
  double t_en;                  // time since enable
  int armed;
  int hit;                      // collision latched (until disabled)
  int joint;                    // first joint over its threshold (1-based)
} rb_collide;

// Defaults (k 50/s, holdoff 0.3 s) then the file; 0, or -1 (message in err).
int rb_collide_load(rb_collide *c, const char *path, char *err, int errlen);
// One line of the model file: 1 if used or ignored, 0 if malformed.
int rb_collide_line(rb_collide *c, const char *line);
// Declare the payload on the last link: mass [kg] at cm [m, its frame];
// replaces the previous one (m 0: none). 0, or -1 without a model.
int rb_collide_payload(rb_collide *c, double m, const double cm[3]);
// After loading or changing kt / K / jm.
void rb_collide_setup(rb_collide *c);
// One servo period. q, qd: model angles [rad], speeds [rad/s]; cur:
// motor currents [A]. Returns hit.
int rb_collide_step(rb_collide *c, int enabled, const double *q, const double *qd,
                    const double *cur, double dt);

#endif
