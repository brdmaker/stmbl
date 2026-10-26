/*
 * Tools on a tool changer: the tool table (TCP, mass, dock) and the
 * changer's monitor.
 *
 * Tool table lines (a file of their own, included by the arm file, the
 * cell file, and read by LinuxCNC's armkins / armtool):
 *   tool ID NAME                   starts tool ID (1..RB_MAXTOOL-1); the
 *                                  lines below belong to it
 *   tcp x y z a b c                the tool centre point in the flange
 *                                  frame [mm, deg: R = Rz(c) Ry(b) Rx(a)]
 *   toolmass M cx cy cz            mass [kg], centre of mass [m, flange frame]
 *   dock x y z a b c dx dy dz [sx sy sz]   the flange pose when the tool
 *                                  sits locked in its dock [mm, deg, base
 *                                  frame]; the bare flange's way in / out:
 *                                  the approach point dock + d; the way out
 *                                  with the tool on (out of the fork):
 *                                  dock + s (default d) [mm, base frame]
 *   body ... tool ID               its bodies (geom.h)
 * Tool 0 is "no tool": the TCP is the flange.
 *
 * The changer: a master plate on the flange with a pneumatic lock (one
 * valve), sensors "locked" and "unlocked" on the piston, "present" (a tool
 * plate against the master) and the tool's ID (coded pins, valid while
 * present). rb_changer watches them against what was commanded:
 *   CH_TIMEOUT    the lock sensors did not follow the valve within t_switch
 *   CH_WRONG      locked onto another tool than expected (or none)
 *   CH_LOST       the mounted tool's lock or presence went away
 *   CH_UNDOCKED   unlock commanded with a tool on, away from its dock (the
 *                 valve is held locked: the tool would fall)
 */
#ifndef TOOL_H
#define TOOL_H

#include "robot.h"

#define RB_MAXTOOL 16

typedef struct {
  int id;              // 0: unused slot
  char name[16];
  double tcp_p[3], tcp_R[3][3];  // TCP in the flange frame [m]
  double m, c[3];      // mass [kg], centre of mass [m, flange frame]
  int has_dock;
  double dock_p[3], dock_R[3][3];  // flange pose in the dock [m, base frame]
  double approach[3];  // the bare flange's way out [m, base frame]
  double slide[3];     // the way out with the tool on [m, base frame]
} rb_tool;

typedef struct {
  rb_tool t[RB_MAXTOOL];  // indexed by ID; t[0] is "no tool"
  int cur;                // the tool the next lines belong to
} rb_tools;

void rb_tools_init(rb_tools *tt);
// a table line: 1 used, 0 not one, -1 malformed
int rb_tools_line(rb_tools *tt, const char *line);
// read a file (also follows `include`, relative to it); 0 or -1 + err
int rb_tools_file(rb_tools *tt, const char *path, char *err, int errlen);
// the tool (its slot; t[0], the flange, for an unknown ID)
const rb_tool *rb_tool_get(const rb_tools *tt, int id);

// TCP pose for joints q, and the flange pose for a TCP pose
void rb_tcp_pose(const rb_model *mdl, const rb_tool *t, const double *q, double p[3], double R[3][3]);
void rb_flange_from_tcp(const rb_tool *t, const double p[3], const double R[3][3], double pf[3],
                        double Rf[3][3]);
// IK for a TCP pose (rb_ik on the flange pose)
int rb_ik_tcp(const rb_model *mdl, const rb_tool *t, const double p[3], const double R[3][3], double *q,
              double tol_p, double tol_r, int maxit);

enum { CH_OK, CH_TIMEOUT, CH_WRONG, CH_LOST, CH_UNDOCKED };

typedef struct {
  // configuration
  double t_switch;     // lock sensors must follow the valve within [s] (0.5)
  double t_debounce;   // lost lock / presence is a fault after [s] (0.01)
  double dock_tol_p;   // in the dock: flange within [m] (0.002) ...
  double dock_tol_r;   // ... and [rad] (0.035) of the dock pose
  // inputs, set before rb_changer_step
  int lock_req;        // the program's lock command
  int expect;          // the tool that should be on after locking (0 none)
  int s_locked, s_unlocked, s_present, s_id;
  // state / outputs
  int valve;           // the valve: locked when 1
  int mounted;         // the tool on the flange (0 none)
  int fault;           // CH_*, latched until rb_changer_reset
  double t, t_lost;
  int check;           // locking: take the tool when the sensors settle
} rb_changer;

void rb_changer_init(rb_changer *c);
// en: machine on; pf/Rf: the flange pose now (to tell "in the dock")
void rb_changer_step(rb_changer *c, const rb_tools *tt, int en, const double pf[3], const double Rf[3][3],
                     double dt);
// clear the fault; the mounted tool is then what the sensors show (a lost
// tool is gone)
void rb_changer_reset(rb_changer *c);
// is the flange within the dock tolerance of tool id's dock?
int rb_in_dock(const rb_changer *c, const rb_tools *tt, int id, const double pf[3], const double Rf[3][3]);

#endif
