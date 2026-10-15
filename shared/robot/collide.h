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
 *   thr0 + thr_v |qd| + thr_s exp(-(qd / vs)^2)
 * (model errors grow with speed; near standstill stiction holds an unknown
 * part, up to ~fs, of the load); disarmed for holdoff seconds after
 * enable.
 *
 * Model file: the arm file's format (host/arm/arm_sim.c), lines used:
 *   dh J a alpha d theta0       link J m cx cy cz Ixx Iyy Izz Ixy Ixz Iyz
 *   motor J ratio jm ...        couple J M v        fric J fc fs vs fv
 *   kt J value                  thresh J thr0 thr_v [thr_s vs]
 *                               [Nm, Nm s/rad, Nm, rad/s]
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
  double thr0[RB_MAXJ], thr_v[RB_MAXJ], thr_s[RB_MAXJ], thr_vs[RB_MAXJ];
  double holdoff;               // [s]
  rb_observer obs;

  // state / outputs
  double tau[RB_MAXJ];          // measured joint torque (motors)
  double tau_f[RB_MAXJ];        // friction model
  double r[RB_MAXJ];            // residual = external torque estimate [Nm]
  double thr[RB_MAXJ];          // threshold in use
  double t_en;                  // time since enable
  int armed;
  int hit;                      // collision latched (until disabled)
  int joint;                    // first joint over its threshold (1-based)
} rb_collide;

// Defaults (k 50/s, holdoff 0.3 s) then the file; 0, or -1 (message in err).
int rb_collide_load(rb_collide *c, const char *path, char *err, int errlen);
// One line of the model file: 1 if used or ignored, 0 if malformed.
int rb_collide_line(rb_collide *c, const char *line);
// After loading or changing kt / K / jm.
void rb_collide_setup(rb_collide *c);
// One servo period. q, qd: model angles [rad], speeds [rad/s]; cur:
// motor currents [A]. Returns hit.
int rb_collide_step(rb_collide *c, int enabled, const double *q, const double *qd,
                    const double *cur, double dt);

#endif
