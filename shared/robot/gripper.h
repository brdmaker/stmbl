/*
 * Electric finger gripper (a stmbl drive moving two fingers) and the
 * payload it picks up.
 *
 * rb_gripper: the fingers. Closing is a position move at speed v with the
 * drive's current limit as the grip force: when the fingers lag their
 * command by more than e_contact they have met something; the measured gap
 * is then the part's width, and the command holds `squeeze` inside it: the
 * position loop presses at the current limit (the grip force; any squeeze
 * past a fraction of a mm gives the same force) and a part slipping out
 * lets the fingers close on by more than lost_tol. Lag and squeeze stay
 * under the drive's max_pos_error (stock 1.5 rad: 1.9 mm of gap with a
 * 4 mm/turn screw per finger). States:
 *   GR_OPEN, GR_CLOSING, GR_OPENING  moving / open
 *   GR_GRIPPED   stopped on something w_expect +- w_tol wide (any width
 *                over gap_min + w_tol if w_expect is 0)
 *   GR_WRONG     stopped on something of another width
 *   GR_EMPTY     closed to gap_min with nothing between the fingers
 *   GR_LOST      was gripping, the fingers closed further: the part slipped
 * Gaps in mm (the gripper drive's position, sserial scale = mm per turn).
 *
 * rb_grip_payload: what the arm carries. A payload the dynamic model does
 * not know shows in the collision residuals r: a mass error dm at the part's
 * centre of mass c adds r = dm s, s_j = g . dp_c/dq_j its gravity torque per
 * kg (least squares over the joints and samples; the external wrench of
 * rb_flange_wrench has 6 unknowns and would turn every joint's model error
 * into mass - the wrist joints' 1 Nm alone are 1 kg). The centre of mass is
 * the part's data (c_expect), not measured.
 *
 * Not at standstill: there static friction holds an unknown part of any new
 * load (on the simulated ARC Mate J2 takes 2 of the 12 Nm a 2 kg part adds),
 * so the residuals only see the weight while the joints that carry it move
 * (kinetic friction, which the model has). The procedure after a grip:
 *   1. the model takes the expected part (m_expect, c_expect) at once, so
 *      collision detection keeps working; muted for `settle` s while the
 *      fingers squeeze;
 *   2. GP_MEASURE: fits dm over `measure` s of samples, each over the joints
 *      moving faster than v_min (a standing joint's friction hides the
 *      weight), taken when those carry `share` of the weight's sum s_j^2 -
 *      the lift-off move after the grip (a vertical lift may be mostly J3,
 *      J2 creeping at 1 deg/s in its stiction band);
 *   3. GP_OK when the measured mass m_meas = m_model + dm is m_expect within
 *      the tolerance, else GP_MISMATCH (a wrong or a lost part).
 * A release empties the model at once, unchecked: the residual floor (up to
 * ~0.8 kg on a lift) is too close to the tolerance to tell a part stuck to
 * a finger from none. With m_expect = 0 on a grip (part
 * unknown) the model learns the measured mass instead: detection stays
 * muted until then, so lift such a part slowly and clear of everything.
 */
#ifndef GRIPPER_H
#define GRIPPER_H

#include "robot.h"

enum { GR_OPEN, GR_CLOSING, GR_GRIPPED, GR_WRONG, GR_EMPTY, GR_OPENING, GR_LOST };

typedef struct {
  // configuration [mm, mm/s]
  double gap_open;     // open position
  double gap_min;      // fully closed
  double v;            // finger speed (gap rate)
  double e_contact;    // lag that means contact
  double squeeze;      // command inside the contact gap while gripping (1.2)
  double w_expect;     // expected part width (0: any)
  double w_tol;        // its tolerance
  double lost_tol;     // closing further than this while gripping: lost (0.8)
  // state
  int state;
  double cmd;          // gap command
  double width;        // measured width (the gap at contact)
  int events;          // count of state changes (for edge detection)
} rb_gripper;

void rb_gripper_init(rb_gripper *g);
// close != 0: close (or keep gripping), else open. gap: measured gap.
void rb_gripper_step(rb_gripper *g, int close, double gap, double dt);

// External wrench at the flange (force F[0..2] [N], moment F[3..5] [Nm]
// about the flange origin, base frame) explaining joint torques r.
int rb_flange_wrench(const rb_model *mdl, const double *q, const double *r, double F[6]);

enum { GP_IDLE, GP_SETTLE, GP_MEASURE, GP_OK, GP_MISMATCH };

typedef struct {
  // configuration
  double settle;       // mute after the event [s] (0.3)
  double measure;      // averaging time while moving [s] (0.3)
  double v_min;        // a joint counts above this speed [rad/s] (0.15: 3x the
                       // gears' Stribeck speed, friction kinetic)
  double share;        // the moving joints' share of sum s_j^2 for a sample (0.3)
  double tol_abs;      // allowed |measured - expected| [kg] (0.5) ...
  double tol_rel;      // ... or relative (0.15), whichever is larger
  // request from outside
  double m_expect;     // the part's mass [kg] (0: unknown, learn it)
  double c_expect[3];  // its centre of mass in the last link's frame [m]
  // state / results
  int state;
  double t, t_meas;
  double m_target;     // what this event should leave: m_expect or 0
  double m_model;      // payload mass the model has now
  double sr_sum, ss_sum;
  double m_meas;       // measured payload [kg]
  int learn;           // learning an unknown mass
  int mute;            // collision detection to be muted
  int update;          // set: the model's payload should become (m_model, c_expect)
} rb_grip_payload;

void rb_grip_payload_init(rb_grip_payload *p);
// event: +1 a part was gripped (expect m_expect), -1 it was released, 0
// none. q, qd, r: joints, speeds and collision residuals (the model having
// the payload m_model). Sets update once per model change.
void rb_grip_payload_step(rb_grip_payload *p, const rb_model *mdl, int event, const double *q, const double *qd,
                          const double *r, double dt);

#endif
