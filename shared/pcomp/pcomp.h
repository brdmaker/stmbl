/*
 * pcomp: conditioning of a portal's commands from a model of its
 * physics, between the trajectory planner (LinuxCNC's X, Y every servo
 * period) and the drives. Plain C, no HAL: host/portal/portal_sim uses it,
 * and linuxcnc/pcomp (the HAL component).
 *
 *   shaper   a ZV input shaper against the head's modes: the command plus
 *            a copy half a damped period later (amplitudes 1/(1+K), K/(1+K),
 *            K = exp(-zeta pi / sqrt(1 - zeta^2))). Two in series, for the
 *            x and the y mode, and the same filter on both axes, so the
 *            path keeps its shape (only delayed and slightly rounded). The
 *            y mode's frequency is the model's at the carriage's position.
 *   defl     feedforward of the head's deflection: the head lags its
 *            carriage by a / w^2 under acceleration a, so the scales are
 *            commanded a / w^2 ahead of where the tool should be.
 *   shrink   the shaper's own error: averaging the path over its impulses
 *            (delay variance s2) gives s = x(t - mean) + s2/2 x'' : it is
 *            pulled towards the inside of curves by s2/2 a (a circle shrinks
 *            by s2/2 v^2/R); the command gets - s2/2 a.
 *   servo    the drives' own steady lag under acceleration: like a
 *            smoothing filter, the scales end up tau2 a towards the
 *            acceleration (on a circle tau2 v^2/R inside it; measured that
 *            way, pc_model.servo); the command gets - tau2 a.
 *   jerk     a moving average of N periods after the shaper: LinuxCNC's
 *            acceleration steps become ramps (the ZV alone leaves smaller
 *            steps, and a lead of a K would jump the command by K da). Its
 *            variance (N^2 - 1)/12 dt^2 counts in s2.
 * All the leads are acceleration times a constant, the acceleration taken
 * from the shaped and averaged (smooth) command: out = s + a (defl/w^2 -
 * servo - s2/2). The head's lag is inertial (it bends against the
 * acceleration, +); the filters and the servo average (towards it, -).
 *
 * The model is an estimate (pc_model): the machine's own values, or
 * identified ones, or deliberately wrong ones to test robustness.
 */
#ifndef PCOMP_H
#define PCOMP_H

#define PC_HIST 256  // servo periods of command history (> both shaper delays)

typedef struct {
  double fx;              // head mode in x [Hz]
  double fy_end, fy_mid;  // in y at the rails / at mid-span [Hz]
  double zeta;            // damping ratio
  double L;               // rail spacing [m]
  double x0;              // machine X of the bridge's centre [mm]
  double servo;           // the drives' lag per acceleration, tau2 [s^2]
} pc_model;

typedef struct {
  pc_model m;
  int shaper, defl, shrink, servo;
  int jerk;                // moving average length [periods] (0, 1: none)
  double dt;               // servo period [s]
  double in[PC_HIST][2];   // planner commands X, Y [mm], newest at head
  double zv[PC_HIST][2];   // after the ZV shapers
  double sh[PC_HIST][2];   // shaped commands (after the moving average)
  int head, n;
  double vin[2];           // the planner's velocity [mm/s] (set before pc_step;
                           // passed through when nothing is on)
  double out[2], vout[2];  // to the drives: position [mm], velocity [mm/s]
  double delay;            // the conditioning's mean delay now [s]: delay the
                           // machine's I/O (laser, spindle) as much
  double a_prev[2];        // the shaped command's acceleration, last period
  int primed;
} pc_t;

void pc_init(pc_t *c, const pc_model *m, double dt);
// head mode frequency in y at machine X [Hz]
double pc_fy(const pc_model *m, double X);
// one servo period: the planner's X, Y [mm] in; c->out, c->vout to the drives
void pc_step(pc_t *c, const double xy[2]);

#endif
