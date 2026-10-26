/*
 * Simulated pneumatic tool changer on the flange and the tools in their
 * docks, for armcore.c. The tools, their TCPs, masses and docks are the
 * tool table (shared/robot/tool.h lines in the arm file, usually an
 * included tools file).
 *
 * The lock: one valve drives a piston over `stroke` seconds (0 unlocked,
 * 1 locked); sensors "unlocked" (x < 0.05) and "locked" (x > 0.95). Going
 * locked past half stroke it captures a docked tool whose dock pose the
 * flange is within 2 mm / 2 deg of: the tool is then on the flange
 * (present, its ID on the coded pins, its mass the plant's tool payload).
 * Going unlocked past half stroke it lets the tool go: into its dock if
 * the flange is at the dock pose, else the tool falls (dropped, gone).
 * Locked with nothing captured, the piston simply ends locked.
 *
 * Lines:
 *   changer STROKE [ID]   a changer, lock stroke time [s]; ID: the tool on
 *                         the flange at start (locked), else none (unlocked)
 *   tool / tcp / toolmass / dock   the tool table (tool.h)
 */
#ifndef CHANGERSIM_H
#define CHANGERSIM_H

#include "tool.h"

enum { CS_DOCKED, CS_MOUNTED, CS_DROPPED };

typedef struct {
  int on;
  double stroke;
  double x;              // piston 0 (unlocked) .. 1 (locked)
  int valve;
  rb_tools tt;
  int state[RB_MAXTOOL];
  int mounted;           // the tool on the flange (0 none)
  int slip;              // fault injection: the tool slips off (present lost)
} cs_changer;

void cs_init(cs_changer *c);
int cs_line(cs_changer *c, const char *line);  // 1 used, 0 not ours, -1 bad
// advance with the flange pose; returns +1 a tool was captured, -1 one
// was let go (docked or dropped), 0 none
int cs_step(cs_changer *c, const double R6[3][3], const double p6[3], double dt);
int cs_locked(const cs_changer *c);
int cs_unlocked(const cs_changer *c);
int cs_present(const cs_changer *c);
int cs_id(const cs_changer *c);

#endif
