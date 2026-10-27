/*
 * Reaction to a collision (and to an imminent one, collision prevention):
 * the guard takes the joint commands over from the motion controller.
 *
 *   1 stop:    decelerate from the current command speed to standstill,
 *              all joints together, within the stop accelerations
 *   2 backoff: (collisions) retract the flange by `backoff` [m] in a
 *              straight line, orientation kept, over t_backoff [s] (inverse
 *              kinematics every step): back along the path if the flange
 *              was moving, else along the contact force estimated from the
 *              collision residuals (flange wrench = (J J^T)^-1 J r); this
 *              frees a tool pressed against the obstacle
 *   3 hold:    hold [s], then request estop (drives off, brakes on)
 *
 * Prevention (rb_guard_watch, every servo period, before the motion is
 * passed on): "if the arm started its stop now, would it keep the margin?"
 * The stop is the guard's own (all joints together, within a_stop), so its
 * path in joint space is known: q + qd t - qd t^2 / (2 T). The clearance of
 * the arm's bodies to the cell's obstacles and to each other (geom.h) is
 * checked at points along it; if it would come within `margin`, and the
 * motion is approaching (the stop path is closer than the arm now: moving
 * away is always allowed), the guard stops now (GD_CLEARANCE: stop, hold,
 * estop; no backoff).
 *
 * Joint angles in robot.h's q [rad], positions in m.
 */
#ifndef GUARD_H
#define GUARD_H

#include "robot.h"

struct gm_world_s;

enum { GD_PASS, GD_STOP, GD_BACKOFF, GD_HOLD, GD_DONE };
enum { GD_NONE, GD_COLLISION, GD_CLEARANCE };

typedef struct {
  // configuration
  double backoff;              // [m], 0: none (default 0.02)
  double t_backoff;            // [s] (default 0.4)
  double a_stop[RB_MAXJ];      // [rad/s^2] (default 5)
  double hold;                 // [s] (default 0.1)
  double v_moving;             // flange speed counted as moving [m/s] (0.005)
  // state
  int state, reason;
  double t;
  double q[RB_MAXJ], qd[RB_MAXJ];   // the command now
  double q0[RB_MAXJ], qd0[RB_MAXJ];
  double t_stop;
  double dir[3];                    // backoff direction (unit, 0: none)
  double p0[3], R0[3][3];           // flange at the start of the backoff
  int estop;                        // estop requested (stays until reset)
  int ik_fail;
  // prevention
  double margin;                    // [m], 0: off
  int samples;                      // points checked along the stop path (4)
  double clear_now, clear_stop;     // clearance now and on the stop path [m]
  int near_body, near_other, near_self;  // the closest pair (stop path)
} rb_guard;

void rb_guard_init(rb_guard *g);
// Take over at the command q, qd. reason GD_COLLISION: r = the collision
// residuals [Nm] (for the force direction; NULL: none); GD_CLEARANCE: stop
// only. Ignored when already active.
void rb_guard_trigger(rb_guard *g, const rb_model *mdl, int reason, const double *q, const double *qd,
                      const double *r);
// One servo period: g->q, g->qd are the commands while state != GD_PASS.
void rb_guard_step(rb_guard *g, const rb_model *mdl, double dt);
// Prevention: q, qd the command about to be passed on; geo the arm's
// bodies and the cell. Triggers a stop (GD_CLEARANCE) when needed; 1 then.
int rb_guard_watch(rb_guard *g, const rb_model *mdl, struct gm_world_s *geo, const double *q, const double *qd);
// Back to pass-through (e.g. the drives were disabled); estop stays.
void rb_guard_release(rb_guard *g);
// Clear the estop request (the operator's reset).
void rb_guard_reset(rb_guard *g);

// Read a description file into the kinematic model (dh lines), the
// geometry (geom.h lines) and the guard settings:
//   backoff MM [T]          retract [mm] over T [s]
//   astop J DEG_S2          stop deceleration of joint J [deg/s^2]
//   prevent MM              prevention margin [mm] (0: off)
//   include FILE            another such file (relative to this one)
// Other lines are ignored. 0, or -1 with a message in err.
int rb_guard_file(rb_guard *g, rb_model *mdl, struct gm_world_s *geo, const char *path, char *err, int errlen);

#endif
