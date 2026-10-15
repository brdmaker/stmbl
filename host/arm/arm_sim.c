/*
 * arm_sim: six (up to RB_MAXJ) simulated stmbl drives on a simulated arm.
 *
 * The arm and its drives are armcore.c; this is the script front end that
 * plays LinuxCNC (jogs and smooth joint moves, every 1 ms over each drive's
 * simulated smart-serial link, in joint degrees).
 *
 * usage: arm_sim ARMFILE [SCRIPT]   (script from stdin without SCRIPT)
 *
 * ARMFILE (angles in degrees, lengths m, joints 1-based):
 *   drive J CONF [SIMFILE]        drive config of joint J (+ sim-only lines)
 *   dh J a alpha d theta0         Craig DH, as genserkins A-J ALPHA-J D-J
 *   link J m cx cy cz Ixx Iyy Izz Ixy Ixz Iyz   (inertia about the COM)
 *   motor J ratio jm fm brake     motor turns per joint turn, rotor inertia,
 *                                 bearing friction, brake holding torque
 *   couple J M v                  extra joint-J angle per motor-M angle
 *   gear J k c lash_arcmin        stiffness Nm/rad, damping, backlash
 *   fric J fc fs vs fv            gearbox friction (vs, fv in rad/s units)
 *   limits J min max              hard stops
 *   home q1 .. qn                 start pose
 *   payload M cx cy cz            payload on the last link [kg, m from its
 *                                 frame] (M 0: none); detset payload ... for
 *                                 the detector's model
 *
 * Script commands (plus all of ARMFILE's arm lines to change the arm):
 *   run T                         simulate T seconds
 *   lcnc EN [v1 .. vn]            enable, jog velocities [deg/s]
 *   move T q1 .. qn               smooth joint move to q [deg] in T seconds
 *   tmove T q1 .. qn              the same with trapezoidal speed (LinuxCNC's
 *                                 planner: full acceleration from standstill)
 *   lmove T x y z a b c           straight line of the flange to x y z [mm],
 *                                 orientation a b c [deg] (genserkins' A B C:
 *                                 R = Rz(c) Ry(b) Rx(a)), smooth; tlmove:
 *                                 trapezoidal. Inverse kinematics every 1 ms.
 *   arc T cx cy cz nx ny nz ang   arc of the flange about centre c [mm], axis
 *                                 n, by ang [deg]; orientation kept
 *   weave T x y z amp f wx wy wz  line with a weave of amp [mm] at f [Hz]
 *                                 along w (welding), orientation kept
 *   jog J DQ VEL ACC              LinuxCNC's joint jog: J by DQ [deg] at most
 *                                 VEL [deg/s], ACC [deg/s^2] (trapezoid)
 *   trapfrac F                    trapezoids accelerate over F of the time at
 *                                 each end (default 0.25; LinuxCNC jog 30
 *                                 deg/s at 300 deg/s^2 over 30 deg: 0.091)
 *   grip close|open               the gripper (gripper.h): close / open
 *   grip expect W M CX CY CZ      the part to expect: width [mm], mass [kg],
 *                                 centre of mass [m, flange frame]
 *   grip auto 0|1                 measure the payload after a grip / release
 *                                 and update the model (default 1, with a
 *                                 detector): mismatch -> off_reason 6; a part
 *                                 lost from the fingers -> off_reason 7; with
 *                                 W > 0 a part of another width or none
 *                                 (fingers closed) -> off_reason 8
 *   gripdrop                      the held part slips out
 *   guard on|off                  collision reaction (guard.h): stop, back off,
 *                                 then estop, instead of estop at once
 *   backoff MM [T]                its retract (default 20 mm in 0.4 s)
 *   obstacle ... / body ...       geometry (geom.h): the plant's obstacles
 *                                 push back on the bodies
 *   astop J DEG_S2                the guard's stop deceleration of joint J
 *   prevent MM                    collision prevention (guard.h) with margin
 *                                 MM against the geometry defined so far;
 *                                 a protective stop: off_reason 5
 *   react 0|1                     LinuxCNC's reactions (default 1): amp fault
 *                                 -> machine off, collision -> estop, |command
 *                                 - feedback| > ferror -> machine off, no reply
 *                                 from a drive for 3 ms -> machine off; and
 *                                 always: a Cartesian move that would exceed
 *                                 a joint's speed limit is aborted (limitabort)
 *   ferror DEG                    LinuxCNC's FERROR (default 2)
 *   linkloss J 1|0                drive J gets no more smart-serial packets
 *   force J fx fy fz [rx ry rz]   force [N, base frame] at r [m, link J frame]
 *   force off
 *   mark                          remember the gear torques (for gres),
 *                                 clear rmax and the detection time
 *   detector FILE                 run the collision detector (collide.h) on
 *                                 what LinuxCNC sees, every 1 ms
 *   detset LINE                   change one detector model line
 *   rlog FILE                     log t, then qd [rad/s], residual and
 *                                 threshold [Nm]
 *                                 per joint, every 1 ms while armed
 *   print WHAT                    q qd qm fb cmd ferr te tau_g tau_f jtf gres
 *                                 hits fault res thr rmax rmaxm taum speedr
 *                                 coll pose tcp perr ikfail off guard clear
 *                                 contact bodies prevent grip energy
 *                                 time
 *                                 -> "arm.WHAT1 = v" ...
 *   dJ LINE                       console LINE on drive J; a bare pin name
 *                                 prints "dJ_pin = v" (e.g. d2_hv0.iq_fb)
 *   dg LINE                       the same on the gripper drive
 *   all LINE                      LINE on every drive
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "armcore.h"
#include "collide.h"
#include "kin.h"
#include "guard.h"
#include "gripper.h"

#define NJ RB_MAXJ
#define D2R (M_PI / 180.0)
#define P arm_P
#define drv arm_drv
#define nj arm_nj

// LinuxCNC side
static int en = 0;
static int synced = 0;                  // cmd_q taken from the drives
static double jog[NJ];                  // deg/s
static double cmd_q[NJ];                // deg
static double cmd_v[NJ];                // deg/s, last servo period
static double ferror_lim = 2.0;         // LinuxCNC's FERROR [deg]
static double vlim[NJ];                 // joint speed limits [deg/s] (motors)
static double speedr[NJ];               // max |speed| / limit since "mark"
static double perr_max = 0.0;           // max Cartesian path error since "mark" [mm]
static int react = 1;                   // LinuxCNC's reactions (see servo_ms)
static int ik_fails = 0;                // Cartesian moves aborted: no IK solution
// the collision reaction (shared/robot/guard.c): stop, back off, estop
static rb_guard guard;
static int guard_on = 0;
static gm_world guard_geo;              // the guard's view of the cell (prevention)
// the gripper (shared/robot/gripper.c) and the payload it picks up
static rb_gripper grip;
static rb_grip_payload gpay;
static int grip_close = 0, grip_auto = 1, grip_prev_state = GR_OPEN;
static double grip_cmd_prev = 0.0;
static double clear_min = 1e9;          // smallest true clearance since "mark" [m]
static double off_time = -1.0;          // when LinuxCNC turned the machine off
static int off_reason = 0;              // 1 amp fault, 2 collision (estop), 3 ferror,
                                        // 4 smart-serial link (no reply), 5 protective
                                        // stop (collision prevention), 6 payload
                                        // mismatch, 7 part lost from the gripper,
                                        // 8 grip failed (wrong width, empty)
static int lost[NJ];                    // consecutive servo periods without a reply
static int limit_aborts = 0;            // moves aborted: joint speed over its limit
static int off_joint = 0;

// motion segments, as LinuxCNC's planner would send them
enum { SEG_NONE, SEG_JOINT, SEG_LINE, SEG_ARC, SEG_WEAVE };
static struct {
  int type, trap;                     // trap: trapezoidal speed (LinuxCNC-like)
  double ftrap;                       // its acceleration fraction (0: trap_f)
  double T, t;
  double q0[NJ], q1[NJ];              // joint [deg]
  double p0[3], p1[3], R0[3][3], w[3];  // line: positions [m], start rotation,
                                      // rotation vector of R0^T R1
  double c[3], n[3], ang;             // arc: centre, axis, angle [rad]
  double amp, freq, wd[3];            // weave: amplitude [m], [Hz], direction
  int have_cart;
  double pc[3], Rc[3][3];             // commanded pose now
} seg;
static double mark_tau_g[NJ];           // gear torques at "mark"
// collision detector on what LinuxCNC sees (shared/robot/collide.c)
static rb_collide det;
static int det_on = 0;
static double det_rmax[NJ];             // max |residual| since "mark"
static double det_rmaxm[NJ];            // the same while moving (> 0.1 rad/s)
static double det_t_hit = -1.0;         // time of the first detection
static FILE *rlog = NULL;               // residual log (rlog FILE)

static void detector_ms(void) {
  double q[NJ], qd[NJ], cur[NJ];
  int enabled = en;
  for(int j = 0; j < nj; j++) {
    q[j] = armcore_fb(j) * D2R;
    qd[j] = armcore_fb_vel(j) * D2R;
    cur[j] = drv[j].pd.current;
    enabled = enabled && !drv[j].pd.fault;
  }
  int was = det.hit;
  rb_collide_step(&det, enabled, q, qd, cur, 0.001);
  if(det.hit && !was) {
    det_t_hit = arm_ticks * ARM_RT;
  }
  if(rlog && det.armed) {
    fprintf(rlog, "%.4f", arm_ticks * ARM_RT);
    for(int j = 0; j < nj; j++) {
      fprintf(rlog, " %.5f %.4f %.4f", qd[j], det.r[j], det.thr[j]);
    }
    fprintf(rlog, "\n");
  }
  for(int j = 0; j < nj; j++) {
    if(det.armed && fabs(det.r[j]) > det_rmax[j]) {
      det_rmax[j] = fabs(det.r[j]);
    }
    if(det.armed && fabs(qd[j]) > 0.1 && fabs(det.r[j]) > det_rmaxm[j]) {
      det_rmaxm[j] = fabs(det.r[j]);
    }
  }
}

static double smooth(double s) {  // 0..1, zero speed and acceleration at the ends
  return s <= 0.0 ? 0.0 : s >= 1.0 ? 1.0 : s * s * s * (10.0 + s * (-15.0 + 6.0 * s));
}

static double trap_f = 0.25;  // trapezoid: fraction of the time accelerating, each end

static double trapez_f(double s, double f) {  // 0..1, constant acceleration over f at each end
  double a = 1.0 / (f * (1.0 - f));  // reaches 1 at s = 1
  if(s <= 0.0) {
    return 0.0;
  } else if(s >= 1.0) {
    return 1.0;
  } else if(s < f) {
    return 0.5 * a * s * s;
  } else if(s < 1.0 - f) {
    return 0.5 * a * f * f + a * f * (s - f);
  }
  return 1.0 - 0.5 * a * (1.0 - s) * (1.0 - s);
}

static void q_rad(const double *qdeg, double *q) {
  for(int j = 0; j < NJ; j++) {
    q[j] = j < nj ? qdeg[j] * D2R : 0.0;
  }
}

// pose [m, R] of joints qdeg [deg, model q]
static void pose_of(const double *qdeg, double p[3], double R[3][3]) {
  double q[NJ];
  q_rad(qdeg, q);
  rb_pose(&P.arm, q, p, R);
}

static void seg_start(int type, double T) {
  memset(&seg, 0, sizeof(seg));
  seg.type = type;
  seg.T = T > 0.001 ? T : 0.001;
  memcpy(seg.q0, cmd_q, sizeof(seg.q0));
  pose_of(cmd_q, seg.p0, seg.R0);
  for(int j = 0; j < NJ; j++) {
    jog[j] = 0.0;
  }
}

// commanded pose at time t of the segment
static void seg_pose(double t, double p[3], double R[3][3]) {
  double u = t / seg.T, s = seg.trap ? trapez_f(u, seg.ftrap > 0.0 ? seg.ftrap : trap_f) : smooth(u);
  memcpy(R, seg.R0, sizeof(double) * 9);
  if(seg.type == SEG_LINE || seg.type == SEG_WEAVE) {
    double w[3] = {s * seg.w[0], s * seg.w[1], s * seg.w[2]}, Rw[3][3];
    rb_R_from_rotvec(w, Rw);
    for(int i = 0; i < 3; i++) {
      p[i] = seg.p0[i] + s * (seg.p1[i] - seg.p0[i]);
      for(int j = 0; j < 3; j++) {  // R0 exp(s w): w in the tool frame
        R[i][j] = seg.R0[i][0] * Rw[0][j] + seg.R0[i][1] * Rw[1][j] + seg.R0[i][2] * Rw[2][j];
      }
    }
    if(seg.type == SEG_WEAVE) {  // fades in and out over the first/last 10 %
      double win = u < 0.1 ? smooth(u / 0.1) : (u > 0.9 ? smooth((1.0 - u) / 0.1) : 1.0);
      double o = seg.amp * win * sin(2.0 * M_PI * seg.freq * t);
      for(int i = 0; i < 3; i++) {
        p[i] += o * seg.wd[i];
      }
    }
  } else if(seg.type == SEG_ARC) {
    double w[3] = {seg.n[0] * s * seg.ang, seg.n[1] * s * seg.ang, seg.n[2] * s * seg.ang}, Rr[3][3];
    rb_R_from_rotvec(w, Rr);
    for(int i = 0; i < 3; i++) {
      p[i] = seg.c[i];
      for(int j = 0; j < 3; j++) {
        p[i] += Rr[i][j] * (seg.p0[j] - seg.c[j]);
      }
    }
  }
}

static void machine_off(int reason, int joint) {
  if(!en) {
    return;
  }
  en = 0;
  off_time = arm_ticks * ARM_RT;
  off_reason = reason;
  off_joint = joint;
  seg.type = SEG_NONE;
  for(int j = 0; j < NJ; j++) {
    jog[j] = 0.0;
  }
}

static void servo_ms(void) {  // the LinuxCNC servo thread
  if(!synced) {
    for(int j = 0; j < nj; j++) {
      cmd_q[j] = armcore_qm(j);
      float *mv = drv[j].pin("conf0.max_vel");
      vlim[j] = mv ? *mv / drv[j].ratio / D2R : 1e9;
    }
    synced = 1;
  }
  // LinuxCNC's reactions: amp fault -> machine off; collision -> estop;
  // following error over FERROR -> machine off
  for(int j = 0; j < nj; j++) {  // hm2: a drive that stops answering
    lost[j] = drv[j].link_lost ? lost[j] + 1 : 0;
  }
  if(react && en) {
    for(int j = 0; j < nj; j++) {
      if(lost[j] >= 3) {  // the sserial error pins wired into amp-fault
        machine_off(4, j + 1);
      } else if(drv[j].pd.fault) {
        machine_off(1, j + 1);
      } else if(fabs(cmd_q[j] - armcore_fb(j)) > ferror_lim) {
        machine_off(3, j + 1);
      }
    }
    if(det_on && det.hit) {
      if(guard_on) {  // stop and back off first, estop when done
        double q[NJ], qd[NJ], r[NJ];
        for(int j = 0; j < NJ; j++) {
          q[j] = j < nj ? cmd_q[j] * D2R : 0.0;
          qd[j] = j < nj ? cmd_v[j] * D2R : 0.0;
          r[j] = j < nj ? det.r[j] : 0.0;
        }
        rb_guard_trigger(&guard, &P.arm, GD_COLLISION, q, qd, r);
        seg.type = SEG_NONE;
      } else {
        machine_off(2, det.joint);
      }
    }
  }
  if(arm_has_grip) {  // the gripper: fingers, events, payload
    double gap = armcore_grip_fb();
    if(en) {
      rb_gripper_step(&grip, grip_close, gap, 0.001);
    } else {
      grip.cmd = gap;
    }
    armcore_grip_cmd(en, grip.cmd, (grip.cmd - grip_cmd_prev) / 0.001);
    grip_cmd_prev = grip.cmd;
    int ev = 0;
    if(grip.state == GR_GRIPPED && grip_prev_state != GR_GRIPPED) {
      ev = +1;
    } else if(grip_prev_state == GR_GRIPPED && (grip.state == GR_OPENING || grip.state == GR_OPEN)) {
      ev = -1;
    } else if(grip.state == GR_LOST && grip_prev_state != GR_LOST) {
      machine_off(7, 0);
    } else if(grip.w_expect > 0.0 && grip.state != grip_prev_state &&
              (grip.state == GR_WRONG || grip.state == GR_EMPTY)) {
      machine_off(8, 0);
    }
    grip_prev_state = grip.state;
    if(grip_auto && det_on) {
      double q[NJ] = {0}, qd[NJ] = {0}, r[NJ] = {0};
      for(int j = 0; j < nj; j++) {
        q[j] = armcore_fb(j) * D2R;
        qd[j] = cmd_v[j] * D2R;
        r[j] = det.r[j];
      }
      int was = gpay.state;
      rb_grip_payload_step(&gpay, &det.mdl, ev, q, qd, r, 0.001);
      det.mute = gpay.mute;
      if(gpay.update) {  // the model takes the part (or loses it)
        rb_collide_payload(&det, gpay.m_model, gpay.c_expect);
        rb_observer_reset(&det.obs);
      }
      if(gpay.state == GP_MISMATCH && was != GP_MISMATCH && en) {
        machine_off(6, 0);
      }
    }
  }
  if(!en && guard.state != GD_PASS) {
    rb_guard_release(&guard);
  }
  if(!en) {  // LinuxCNC follows the feedback while disabled
    for(int j = 0; j < nj; j++) {
      cmd_q[j] = armcore_synced() ? armcore_fb(j) : armcore_qm(j);
      cmd_v[j] = 0.0;
      armcore_set_cmd(j, 0, cmd_q[j], 0.0);
    }
    return;
  }
  if(guard.state != GD_PASS && guard.state != GD_DONE) {  // the guard commands
    rb_guard_step(&guard, &P.arm, 0.001);
    for(int j = 0; j < nj; j++) {
      cmd_q[j] = guard.q[j] / D2R;
      cmd_v[j] = guard.qd[j] / D2R;
      armcore_set_cmd(j, en, cmd_q[j], cmd_v[j]);
    }
    if(guard.state == GD_DONE) {  // estop (collision) / protective stop (clearance)
      int reason = guard.reason == GD_CLEARANCE ? 5 : 2;
      rb_guard_release(&guard);
      machine_off(reason, reason == 2 ? det.joint : 0);
    }
    return;
  }
  double qn[NJ];
  memcpy(qn, cmd_q, sizeof(qn));
  int cart_seg = seg.type == SEG_LINE || seg.type == SEG_ARC || seg.type == SEG_WEAVE;
  if(seg.type != SEG_NONE) {
    seg.t += 0.001;
    double t = seg.t < seg.T ? seg.t : seg.T;
    if(seg.type == SEG_JOINT) {
      double u = t / seg.T, s = seg.trap ? trapez_f(u, seg.ftrap > 0.0 ? seg.ftrap : trap_f) : smooth(u);
      for(int j = 0; j < nj; j++) {
        qn[j] = seg.q0[j] + s * (seg.q1[j] - seg.q0[j]);
      }
    } else {
      double p[3], R[3][3], q[NJ];
      seg_pose(t, p, R);
      q_rad(cmd_q, q);
      if(rb_ik(&P.arm, p, R, q, 1e-7, 1e-7, 50) < 0) {
        ik_fails++;  // as LinuxCNC: the move is aborted
        seg.type = SEG_NONE;
      } else {
        for(int j = 0; j < nj; j++) {
          qn[j] = q[j] / D2R;
        }
        memcpy(seg.pc, p, sizeof(p));
        memcpy(seg.Rc, R, sizeof(R));
        seg.have_cart = 1;
      }
    }
    if(seg.t >= seg.T) {
      seg.type = SEG_NONE;
    }
  } else {
    for(int j = 0; j < nj; j++) {
      qn[j] += jog[j] * 0.001;
    }
  }
  // LinuxCNC aborts a Cartesian move whose kinematics would exceed a
  // joint's speed limit (near a singularity, or an unreachable target):
  // stop there. Joint moves are planned within the INI's joint limits; an
  // INI that allows more than the motors can do lets them through.
  for(int j = 0; j < nj && cart_seg; j++) {
    if(fabs(qn[j] - cmd_q[j]) / 0.001 > vlim[j]) {
      limit_aborts++;
      seg.type = SEG_NONE;
      memcpy(qn, cmd_q, sizeof(qn));
      break;
    }
  }
  if(guard.margin > 0.0 && guard.state == GD_PASS) {  // prevention: could it still stop?
    double q[NJ] = {0}, qd[NJ] = {0};
    for(int j = 0; j < nj; j++) {
      q[j] = qn[j] * D2R;
      qd[j] = (qn[j] - cmd_q[j]) / 0.001 * D2R;
    }
    if(rb_guard_watch(&guard, &P.arm, &guard_geo, q, qd)) {
      seg.type = SEG_NONE;
    }
  }
  for(int j = 0; j < nj; j++) {
    cmd_v[j] = (qn[j] - cmd_q[j]) / 0.001;
    cmd_q[j] = qn[j];
    double r = fabs(cmd_v[j]) / vlim[j];
    speedr[j] = r > speedr[j] ? r : speedr[j];
    armcore_set_cmd(j, en, cmd_q[j], cmd_v[j]);
  }
  if(guard_geo.nb > 0 && guard.margin > 0.0) {  // the true clearance (links, after the gears)
    double q[NJ] = {0};
    for(int j = 0; j < nj; j++) {
      q[j] = P.q[j];
    }
    gm_world tmp = guard_geo;
    gm_update(&tmp, &P.arm, q);
    double c = gm_clearance(&tmp, NULL);
    clear_min = c < clear_min ? c : clear_min;
  }
  if(seg.have_cart) {  // path error: commanded tool point vs the true one
    double p[3], R[3][3], q[NJ];
    for(int j = 0; j < nj; j++) {
      q[j] = P.q[j];
    }
    rb_pose(&P.arm, q, p, R);
    double e = sqrt((p[0] - seg.pc[0]) * (p[0] - seg.pc[0]) + (p[1] - seg.pc[1]) * (p[1] - seg.pc[1]) +
                    (p[2] - seg.pc[2]) * (p[2] - seg.pc[2])) * 1000.0;
    perr_max = e > perr_max ? e : perr_max;
  }
}

static void run_tick(void) {
  int servo = arm_ticks % 5 == 0;
  if(servo) {
    servo_ms();
  }
  armcore_tick(servo);
  if(servo && det_on) {
    detector_ms();
  }
}

static void print_vals(const char *what) {
  double jtf[NJ] = {0};
  if((!strcmp(what, "jtf") || !strcmp(what, "gres")) && P.fe_on) {  // joint torques of the external force
    double z[NJ] = {0}, a[NJ], b[NJ];
    rb_rnea(&P.arm, P.q, z, z, 0, 0, 0, a);
    rb_rnea(&P.arm, P.q, z, z, 0, &P.fe, 1, b);
    for(int j = 0; j < nj; j++) {
      jtf[j] = a[j] - b[j];
    }
  }
  for(int j = 0; j < nj; j++) {
    double v = NAN;
    if(!strcmp(what, "jtf")) {
      v = jtf[j];
    } else if(!strcmp(what, "gres")) {  // statics: gear torque change = -J^T f
      v = P.tau_g[j] - mark_tau_g[j] + jtf[j];
    } else if(!strcmp(what, "q")) {
      v = P.q[j] / D2R;
    } else if(!strcmp(what, "qd")) {
      v = P.qd[j] / D2R;
    } else if(!strcmp(what, "qm")) {  // motor angles as joint angles
      v = armcore_qm(j);
    } else if(!strcmp(what, "fb")) {  // LinuxCNC's joint feedback
      v = armcore_fb(j);
    } else if(!strcmp(what, "ferr")) {  // LinuxCNC command - link position
      v = cmd_q[j] - P.q[j] / D2R;
    } else if(!strcmp(what, "te")) {
      v = *drv[j].te_int / ARM_RT;
    } else if(!strcmp(what, "tau_g")) {
      v = P.tau_g[j];
    } else if(!strcmp(what, "tau_f")) {
      v = P.tau_f[j];
    } else if(!strcmp(what, "hits")) {
      v = P.stop_hits[j];
    } else if(!strcmp(what, "fault")) {
      v = drv[j].pd.fault;
    } else if(!strcmp(what, "res")) {
      v = det.r[j];
    } else if(!strcmp(what, "thr")) {
      v = det.thr[j];
    } else if(!strcmp(what, "rmax")) {
      v = det_rmax[j];
    } else if(!strcmp(what, "rmaxm")) {
      v = det_rmaxm[j];
    } else if(!strcmp(what, "taum")) {  // joint torque measured via currents
      v = det.tau[j];
    } else if(!strcmp(what, "speedr")) {  // max |speed| / motor limit since mark
      v = speedr[j];
    } else if(!strcmp(what, "cmd")) {
      v = cmd_q[j];
    }
    if(!isnan(v)) {
      printf("arm.%s%d = %.6f\n", what, j + 1, v);
    }
  }
  if(!strcmp(what, "pose") || !strcmp(what, "tcp")) {
    // pose: from LinuxCNC's joint feedback (as genserkins shows it);
    // tcp: the true tool point (links, after the gears)
    double qd[NJ], pp[3], R[3][3], rpy[3];
    for(int j = 0; j < NJ; j++) {
      qd[j] = j < nj ? (what[0] == 'p' ? armcore_fb(j) : P.q[j] / D2R) : 0.0;
    }
    pose_of(qd, pp, R);
    rb_rpy_from_R(R, rpy);
    const char *nm[6] = {"x", "y", "z", "a", "b", "c"};
    for(int i = 0; i < 6; i++) {
      printf("arm.%s_%s = %.6f\n", what, nm[i], i < 3 ? pp[i] * 1000.0 : rpy[i - 3] / D2R);
    }
  }
  if(!strcmp(what, "grip")) {
    int h = gs_held(&arm_gs);
    printf("arm.grip_state = %d\narm.grip_width = %.3f\narm.grip_gap = %.3f\narm.grip_force = %.2f\n"
           "arm.grip_held = %d\narm.pay_state = %d\narm.pay_meas = %.4f\narm.pay_model = %.4f\n",
           grip.state, grip.width, armcore_grip_fb(), arm_gs.force,
           h, gpay.state, gpay.m_meas, gpay.m_model);
    if(h >= 0) {
      printf("arm.part_z = %.4f\n", arm_gs.parts[h].c[2] * 1000.0);
    }
  }
  if(!strcmp(what, "guard")) {
    printf("arm.guard_state = %d\narm.guard_estop = %d\narm.guard_ikfail = %d\n", guard.state, guard.estop,
           guard.ik_fail);
    printf("arm.guard_dx = %.4f\narm.guard_dy = %.4f\narm.guard_dz = %.4f\n", guard.dir[0], guard.dir[1],
           guard.dir[2]);
  }
  if(!strcmp(what, "clear")) {  // the true clearance of the arm to the obstacles [mm]
    double q[NJ];
    for(int j = 0; j < NJ; j++) {
      q[j] = j < nj ? P.q[j] : 0.0;
    }
    gm_update(&P.geo, &P.arm, q);
    gm_pair bp;
    double d = gm_clearance(&P.geo, &bp);
    printf("arm.clear = %.4f\narm.clear_body = %d\narm.clear_other = %d\n", d * 1000.0, bp.body, bp.other);
  }
  if(!strcmp(what, "prevent")) {
    printf("arm.prev_now = %.4f\narm.prev_stop = %.4f\narm.prev_body = %d\narm.prev_other = %d\n"
           "arm.prev_self = %d\narm.clear_min = %.4f\n", guard.clear_now * 1e3, guard.clear_stop * 1e3,
           guard.near_body, guard.near_other, guard.near_self, clear_min * 1e3);
  }
  if(!strcmp(what, "contact")) {  // the plant's contacts with the obstacles
    printf("arm.contact_f = %.4f\narm.contact_max = %.4f\narm.pen_max = %.6f\n", P.contact_f, P.contact_max,
           P.pen_max * 1000.0);
  }
  if(!strcmp(what, "bodies")) {  // where the bodies are [mm]
    double q[NJ];
    for(int j = 0; j < NJ; j++) {
      q[j] = j < nj ? P.q[j] : 0.0;
    }
    gm_update(&P.geo, &P.arm, q);
    for(int i = 0; i < P.geo.nb; i++) {
      gm_shape *b = &P.geo.body[i];
      printf("# body %d link %d: %.0f %.0f %.0f -> %.0f %.0f %.0f r %.0f\n", i, b->link, b->wa[0] * 1e3,
             b->wa[1] * 1e3, b->wa[2] * 1e3, b->wb[0] * 1e3, b->wb[1] * 1e3, b->wb[2] * 1e3, b->r * 1e3);
    }
  }
  if(!strcmp(what, "perr")) {
    printf("arm.perr = %.6f\n", perr_max);
  }
  if(!strcmp(what, "ikfail")) {
    printf("arm.ikfail = %d\narm.limitabort = %d\n", ik_fails, limit_aborts);
  }
  if(!strcmp(what, "off")) {
    printf("arm.en = %d\narm.off_time = %.4f\narm.off_reason = %d\narm.off_joint = %d\n", en,
           off_time, off_reason, off_joint);
  }
  if(!strcmp(what, "coll")) {
    printf("arm.coll = %d\narm.coll_joint = %d\narm.coll_time = %.4f\n", det.hit, det.joint,
           det_t_hit);
  }
  if(!strcmp(what, "energy")) {
    printf("arm.energy = %.6f\n", plant_energy(&P));
  } else if(!strcmp(what, "time")) {
    printf("arm.time = %.6f\n", arm_ticks * ARM_RT);
  }
}

static void drive_line(int j, const char *line) {
  char name[64];
  if(sscanf(line, " %63[A-Za-z0-9_.] %c", name, (char[2]){0}) == 1 && strchr(name, '.')) {
    float *p = drv[j].pin(name);
    if(p) {
      printf("d%d_%s = %.6f\n", j + 1, name, *p);
      return;
    }
  }
  drv[j].cmd(line);
}

static int nums(char *p, double *v, int max) {  // up to max numbers from p
  int n = 0;
  char *end;
  for(; n < max; p = end) {
    double x = strtod(p, &end);
    if(end == p) {
      break;
    }
    v[n++] = x;
  }
  return n;
}

static int joint(const char *s) {
  int j = atoi(s) - 1;
  if(j < 0 || j >= NJ) {
    fprintf(stderr, "arm_sim: bad joint %s\n", s);
    exit(1);
  }
  return j;
}
static void script_line(char *line) {
  char *h = strchr(line, '#');
  if(h) {
    *h = 0;
  }
  line[strcspn(line, "\r\n")] = 0;
  char kw[16];
  int off = 0;
  if(sscanf(line, " %15s%n", kw, &off) != 1) {
    return;
  }
  char *p = line + off;
  if(!strcmp(kw, "run")) {
    double t = atof(p);
    long n = (long)(t / ARM_RT + 0.5);
    for(long i = 0; i < n; i++) {
      run_tick();
    }
  } else if(!strcmp(kw, "lcnc") && (rb_guard_reset(&guard), 1)) {
    char *end;
    en = (int)strtol(p, &end, 10);
    for(int j = 0; j < NJ; j++) {
      p = end;
      double x = strtod(p, &end);
      jog[j] = end == p ? 0.0 : x;
    }
    if(!en) {  // LinuxCNC follows the feedback while disabled
      for(int j = 0; j < nj; j++) {
        cmd_q[j] = armcore_synced() ? armcore_fb(j) : armcore_qm(j);
      }
      seg.type = SEG_NONE;
    }
  } else if(!strcmp(kw, "move") || !strcmp(kw, "tmove")) {
    double v[1 + NJ];
    int n = nums(p, v, 1 + NJ);
    seg_start(SEG_JOINT, v[0]);
    seg.trap = kw[0] == 't';
    for(int j = 0; j < NJ; j++) {
      seg.q1[j] = j + 1 < n ? v[1 + j] : cmd_q[j];
    }
  } else if(!strcmp(kw, "lmove") || !strcmp(kw, "tlmove") || !strcmp(kw, "weave")) {
    // [t]lmove T x y z a b c       (mm, deg genserkins ABC)
    // weave T x y z amp freq wx wy wz   (mm, Hz; orientation kept)
    double v[10];
    int n = nums(p, v, 10);
    int w = kw[0] == 'w';
    if(n < (w ? 9 : 7)) {
      printf("ERROR %s: too few numbers\n", kw);
    } else {
      seg_start(w ? SEG_WEAVE : SEG_LINE, v[0]);
      seg.trap = kw[0] == 't';
      for(int i = 0; i < 3; i++) {
        seg.p1[i] = v[1 + i] / 1000.0;
      }
      double R1[3][3], Rr[3][3];
      if(w) {
        memcpy(R1, seg.R0, sizeof(R1));
        double wn = sqrt(v[6] * v[6] + v[7] * v[7] + v[8] * v[8]);
        seg.amp = v[4] / 1000.0, seg.freq = v[5];
        for(int i = 0; i < 3; i++) {
          seg.wd[i] = wn > 0.0 ? v[6 + i] / wn : 0.0;
        }
      } else {
        double rpy[3] = {v[4] * D2R, v[5] * D2R, v[6] * D2R};
        rb_R_from_rpy(rpy, R1);
      }
      for(int i = 0; i < 3; i++) {  // R0^T R1
        for(int j = 0; j < 3; j++) {
          Rr[i][j] = seg.R0[0][i] * R1[0][j] + seg.R0[1][i] * R1[1][j] + seg.R0[2][i] * R1[2][j];
        }
      }
      rb_rotvec_from_R(Rr, seg.w);
    }
  } else if(!strcmp(kw, "arc")) {  // arc T cx cy cz nx ny nz angle (mm, deg)
    double v[8];
    if(nums(p, v, 8) < 8) {
      printf("ERROR arc: arc T cx cy cz nx ny nz angle\n");
    } else {
      seg_start(SEG_ARC, v[0]);
      double nn = sqrt(v[4] * v[4] + v[5] * v[5] + v[6] * v[6]);
      for(int i = 0; i < 3; i++) {
        seg.c[i] = v[1 + i] / 1000.0;
        seg.n[i] = v[4 + i] / nn;
      }
      seg.ang = v[7] * D2R;
    }
  } else if(!strcmp(kw, "jog")) {  // jog J DQ VEL ACC: LinuxCNC's free planner
    double v[4];
    if(nums(p, v, 4) < 4 || v[0] < 1 || v[0] > nj || v[2] <= 0.0 || v[3] <= 0.0) {
      printf("ERROR jog: jog J DQ VEL ACC\n");
    } else {
      int j = (int)v[0] - 1;
      double d = fabs(v[1]), vel = v[2], acc = v[3], T, f;
      if(d > vel * vel / acc) {  // trapezoid
        T = d / vel + vel / acc;
        f = vel / acc / T;
      } else {  // triangle
        T = 2.0 * sqrt(d / acc);
        f = 0.5;
      }
      seg_start(SEG_JOINT, T);
      seg.trap = 1;
      seg.ftrap = f;
      memcpy(seg.q1, cmd_q, sizeof(seg.q1));
      seg.q1[j] += v[1];
    }
  } else if(!strcmp(kw, "trapfrac")) {
    double f = atof(p);
    trap_f = f > 0.01 && f <= 0.5 ? f : 0.25;
  } else if(!strcmp(kw, "grip")) {  // grip close|open|expect W M CX CY CZ|auto 0/1
    char sub[16];
    double v[5] = {0};
    int o2 = 0;
    if(sscanf(p, " %15s%n", sub, &o2) == 1) {
      if(!strcmp(sub, "close")) {
        grip_close = 1;
      } else if(!strcmp(sub, "open")) {
        grip_close = 0;
      } else if(!strcmp(sub, "expect") && nums(p + o2, v, 5) >= 2) {
        grip.w_expect = v[0];
        gpay.m_expect = v[1];
        gpay.c_expect[0] = v[2], gpay.c_expect[1] = v[3], gpay.c_expect[2] = v[4];
      } else if(!strcmp(sub, "auto")) {
        grip_auto = atoi(p + o2);
      }
    }
  } else if(!strcmp(kw, "gripdrop")) {  // the held part slips out
    armcore_grip_drop();
  } else if(!strcmp(kw, "guard")) {  // guard on|off
    guard_on = strstr(p, "on") != NULL;
  } else if(!strcmp(kw, "backoff")) {  // backoff MM [T]
    double v[2] = {20, 0.4};
    nums(p, v, 2);
    guard.backoff = v[0] / 1000.0;
    guard.t_backoff = v[1] > 0.05 ? v[1] : 0.4;
  } else if(!strcmp(kw, "astop")) {  // astop J DEG_S2: the guard's stop deceleration
    double v[2];
    if(nums(p, v, 2) == 2 && v[0] >= 1 && v[0] <= NJ && v[1] > 0) {
      guard.a_stop[(int)v[0] - 1] = v[1] * D2R;
    }
  } else if(!strcmp(kw, "prevent")) {  // prevent MM: collision prevention margin (0: off)
    guard.margin = atof(p) / 1000.0;
    guard_geo = P.geo;  // the cell as defined so far
  } else if(!strcmp(kw, "react")) {
    react = atoi(p);
  } else if(!strcmp(kw, "ferror")) {
    ferror_lim = atof(p);
  } else if(!strcmp(kw, "linkloss")) {  // linkloss J 1|0
    int j, on;
    if(sscanf(p, " %d %d", &j, &on) == 2 && j >= 1 && j <= nj) {
      drv[j - 1].link_lost = on;
    }
  } else if(!strcmp(kw, "force")) {
    if(strstr(p, "off")) {
      P.fe_on = 0;
    } else {
      char *end;
      P.fe.link = joint(p) - 0;
      strtol(p, &end, 10);
      p = end;
      double v[6] = {0};
      for(int i = 0; i < 6; i++, p = end) {
        v[i] = strtod(p, &end);
      }
      memcpy(P.fe.f, v, sizeof(P.fe.f));
      memcpy(P.fe.r, v + 3, sizeof(P.fe.r));
      P.fe_on = 1;
    }
  } else if(!strcmp(kw, "mark")) {
    memcpy(mark_tau_g, P.tau_g, sizeof(mark_tau_g));
    P.contact_max = 0.0, P.pen_max = 0.0;
    memset(speedr, 0, sizeof(speedr));
    perr_max = 0.0;
    clear_min = 1e9;
    off_time = -1.0, off_reason = 0, off_joint = 0;
    memset(det_rmax, 0, sizeof(det_rmax));
    memset(det_rmaxm, 0, sizeof(det_rmaxm));
    det_t_hit = -1.0;
  } else if(!strcmp(kw, "detector")) {
    char path[512], err[256];
    sscanf(p, " %511s", path);
    char full[1100];
    snprintf(full, sizeof(full), "%s", path);
    if(rb_collide_load(&det, full, err, sizeof(err))) {
      printf("ERROR detector: %s\n", err);
    } else {
      det_on = 1;
    }
  } else if(!strcmp(kw, "rlog")) {
    char path[512];
    if(rlog) {
      fclose(rlog);
    }
    rlog = sscanf(p, " %511s", path) == 1 ? fopen(path, "w") : NULL;
  } else if(!strcmp(kw, "detset")) {  // a model line for the detector
    if(!rb_collide_line(&det, p)) {
      printf("ERROR detset: %s\n", p);
    }
    rb_collide_setup(&det);
  } else if(!strcmp(kw, "print")) {
    char what[32];
    while(sscanf(p, " %31s%n", what, &off) == 1) {
      print_vals(what);
      p += off;
    }
  } else if(kw[0] == 'd' && kw[1] >= '1' && kw[1] <= '9' && kw[2] == 0) {
    drive_line(joint(kw + 1), p);
  } else if(!strcmp(kw, "dg") && arm_has_grip) {  // console line on the gripper drive
    char name[64];
    float *pp = NULL;
    if(sscanf(p, " %63[A-Za-z0-9_.] %c", name, (char[2]){0}) == 1 && strchr(name, '.') && (pp = arm_gdrv.pin(name))) {
      printf("dg_%s = %.6f\n", name, *pp);
    } else {
      arm_gdrv.cmd(p);
    }
  } else if(!strcmp(kw, "all")) {
    for(int j = 0; j < nj; j++) {
      drive_line(j, p);
    }
  } else if(!strcmp(kw, "home")) {
    armcore_line(line);
    synced = 0;
  } else if(!armcore_line(line)) {
    fprintf(stderr, "arm_sim: unknown line: %s\n", line);
    printf("ERROR unknown line: %s\n", line);
  }
  fflush(stdout);
}

int main(int argc, char **argv) {
  if(argc < 2) {
    fprintf(stderr, "usage: arm_sim ARMFILE [SCRIPT]\n");
    return 1;
  }
  char lib[1024] = "./libstmbl_drive.so", exe[1024];
  const char *lp = getenv("STMBL_DRIVE_LIB");
  ssize_t el = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
  if(lp) {
    snprintf(lib, sizeof(lib), "%s", lp);
  } else if(el > 0) {  // next to arm_sim
    exe[el] = 0;
    *strrchr(exe, '/') = 0;
    snprintf(lib, sizeof(lib), "%s/libstmbl_drive.so", exe);
  }
  rb_guard_init(&guard);
  rb_gripper_init(&grip);
  rb_grip_payload_init(&gpay);
  if(armcore_load(argv[1], lib)) {
    return 1;
  }
  if(arm_has_grip) {  // fingers from the simulated gripper's limits (armcore starts them open)
    grip.gap_open = arm_gs.gap_max - 5.0;
    grip.cmd = grip_cmd_prev = grip.gap_open;
  }
  FILE *in = argc > 2 ? fopen(argv[2], "r") : stdin;
  if(!in) {
    fprintf(stderr, "arm_sim: cannot open %s\n", argv[2]);
    return 1;
  }
  char line[512];
  while(fgets(line, sizeof(line), in)) {
    script_line(line);
  }
  return 0;
}
