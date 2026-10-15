/*
 * Simulated electric finger gripper on the flange (link 6) and the parts
 * it can pick up, for armcore.c.
 *
 * Mechanics: the gripper drive's rotor (sim_ext, as the arm's) turns a
 * screw moving both fingers: gap = 2 * lead * turns, with the fingers'
 * mass reflected to the rotor, Coulomb friction and end stops at 0 and
 * gap_max. A part between the jaws (its centre in the jaw region, in the
 * flange frame) of width w (its extent along the closing axis, flange x)
 * pushes back when gap < w: a stiff spring per finger. Above `hold` N per
 * finger it is held: it follows the flange and its mass is the plant's
 * payload (armcore_payload); opened past w it is released where it is.
 *
 * Description lines:
 *   gripper drive CONF [SIM]              the gripper's stmbl drive
 *   gripper mech LEAD GAPMAX MF JM FC     lead [mm/turn per finger], max gap
 *                                         [mm], finger mass [kg], rotor
 *                                         inertia [kg m^2], friction [Nm]
 *   gripper jaw Z LEN WIDTH               jaw centre on the flange z [m], finger
 *                                         length (along z) and width (y) [m]
 *   part NAME cx cy cz hx hy hz MASS [a b c]   a box part [m, kg, deg]
 *   partmass NAME MASS                    change a part's mass (not while held)
 */
#ifndef GRIPSIM_H
#define GRIPSIM_H

#include "robot.h"

#define GS_MAXPART 8

typedef struct {
  char name[16];
  double c[3], h[3], R[3][3];  // world pose (when not held)
  double mass;
  int held, dropped;
  double c6[3], R6[3][3];      // pose in the flange frame while held
} gs_part;

typedef struct {
  int on;
  double lead, gap_max, mf, jm, fc;
  double jaw_z, jaw_len, jaw_w;
  double k, c, hold;           // contact spring [N/m], damper, hold force [N]
  double th, w;                // rotor angle [rad] (0: closed), speed
  double force;                // contact force per finger [N]
  int part;                    // part between the jaws, -1 none
  gs_part parts[GS_MAXPART];
  int np;
} gs_gripper;

void gs_init(gs_gripper *g);
// a description line: 1 used, 0 not ours, -1 malformed
int gs_line(gs_gripper *g, const char *line);
double gs_gap(const gs_gripper *g);  // [mm]
void gs_set_gap(gs_gripper *g, double gap_mm);
// advance: te the drive's torque, R6/p6 the flange pose; returns the payload
// change: +1 picked a part up, -1 released (dropped) one, 0 none
int gs_step(gs_gripper *g, double te, const double R6[3][3], const double p6[3], double dt);
// the part held now (-1 none)
int gs_held(const gs_gripper *g);
// let the held part slip out
void gs_drop(gs_gripper *g);

#endif
