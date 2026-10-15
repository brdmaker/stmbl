/*
 * Simulated arm: rigid links (shared/robot) driven through elastic geared
 * joints with backlash, by motor rotors whose electrical torque comes from
 * the simulated drives (hv0 with sim_ext = 1).
 *
 * Per joint i, with the transmission q_m = K theta (theta = motor angles,
 * gear ratios and wrist coupling in K):
 *   gear:   e = deadzone(q_m - q, lash),  tau_g = k e + c (qd_m - qd) in contact
 *   link:   M(q) qdd + C qd + g = tau_g - friction + stops + J^T f_ext
 *   rotor:  jm thetadd = te - (K^T tau_g) - bearing friction - brake
 * Friction and brakes are applied so that they stop motion within a step
 * but never reverse it (stable at any step and friction).
 */
#ifndef PLANT_H
#define PLANT_H

#include "robot.h"
#include "geom.h"

typedef struct {
  rb_model arm;                  // links; its armature must be 0 (rotors are here)
  double K[RB_MAXJ][RB_MAXJ];    // joint angle per motor angle
  // per joint (joint side)
  double k[RB_MAXJ];             // gear stiffness [Nm/rad]
  double c[RB_MAXJ];             // gear damping [Nm s/rad]
  double lash[RB_MAXJ];          // total backlash [rad]
  rb_fric fric[RB_MAXJ];         // gearbox friction
  double qmin[RB_MAXJ], qmax[RB_MAXJ];  // hard stops [rad] (equal: none)
  double kstop, cstop;
  // per motor
  double jm[RB_MAXJ];            // rotor inertia [kg m^2]
  double fm[RB_MAXJ];            // rotor bearing friction [Nm]
  double brake[RB_MAXJ];         // brake holding torque [Nm]
  // external force on a link (collision tests)
  rb_force fe;
  int fe_on;
  // geometry: obstacles push back on penetrating bodies (spring k, damper c)
  gm_world geo;
  double contact_f;            // sum of the contact forces, last step [N]
  double contact_max;          // peak since cleared [N]
  double pen_max;              // deepest penetration since cleared [m]
  double prev_d[GM_MAXBODY][GM_MAXOBS];

  // state
  double q[RB_MAXJ], qd[RB_MAXJ];      // links
  double th[RB_MAXJ], w[RB_MAXJ];      // rotors
  double tau_g[RB_MAXJ];               // gear torques, last step
  double tau_f[RB_MAXJ];               // link friction, last step
  int stop_hits[RB_MAXJ];
  int at_stop[RB_MAXJ];
} arm_plant;

// Defaults: no lash, stiff gears, no friction, no stops, K = identity, no
// geometry.
void plant_defaults(arm_plant *p, int n);
// Put the arm at rest at q, rotors where the gears are unloaded (gravity
// preload taken up by the gear springs).
void plant_set_q(arm_plant *p, const double *q);
// Advance by dt with motor torques te[] and brakes released where
// released[i] != 0.
void plant_step(arm_plant *p, const double *te, const int *released, double dt);
double plant_energy(const arm_plant *p);

#endif
