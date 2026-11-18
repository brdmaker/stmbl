/*
 * 3D geometry of a serial arm and its surroundings, for collision
 * prevention (clearance) and for the simulated plant (contact forces).
 * Units m and rad; angles in the description file are degrees.
 *
 * Shapes: sphere (centre a, radius r), capsule (segment a-b, radius r),
 * box (centre a, half sizes b, orientation R from A B C as genserkins:
 * Rz(c) Ry(b) Rx(a)), plane (point a, outward normal b: the obstacle is the
 * half-space behind it, e.g. the floor). Arm bodies are spheres or capsules
 * fixed to a link frame (0 = base, 1..n = links, as robot.h's frames);
 * obstacles are fixed in the world (base frame) or, for parts on the arm's
 * own base, on link 0.
 *
 * Description lines (also accepted, and ignored, by the arm file parsers):
 *   body L capsule x1 y1 z1 x2 y2 z2 r [tool T]   arm body on link L (its
 *   body L sphere x y z r [tool T]                frame); with `tool T`: a
 *                                         tool's body, there only while tool
 *                                         T is mounted (gm_world.tool)
 *   obstacle NAME sphere x y z r          world obstacles (base frame)
 *   obstacle NAME capsule x1 y1 z1 x2 y2 z2 r
 *   obstacle NAME box cx cy cz hx hy hz [a b c]
 *   obstacle NAME plane px py pz nx ny nz
 *   obstacle ... dock T                   (any of these) part of tool T's
 *                                         dock: see below
 *   tool T ...  /  dock x y z a b c ...   tool table lines (tool.h): a dock
 *                                         [mm, deg, the flange pose in it]
 *                                         adds tool T's stand (a fork plate,
 *                                         a post to the floor, as the
 *                                         console draws it), and the tool's
 *                                         bodies that follow (`body L ...
 *                                         tool T`) are also obstacles at the
 *                                         dock: the tool standing in it.
 *                                         Dock T's obstacles count unless
 *                                         tool T is mounted (gm_world.tool:
 *                                         the dock is empty) or the arm works
 *                                         at that dock (gm_world.dock: a tool
 *                                         change); gm_world.dock -1: none
 *                                         count (the simulated plant)
 *   selfignore I J                        body pair (0-based, in order of
 *                                         the body lines) not checked
 *   selfskip N                            bodies on links N apart or less
 *                                         are not checked against each other
 *                                         (default 1: neighbours)
 *   contact K C                           plant: contact stiffness [N/m],
 *                                         damping [N s/m]
 */
#ifndef GEOM_H
#define GEOM_H

#include "robot.h"

#define GM_MAXOBS 32
#define GM_MAXBODY 16
#define GM_MAXTOOL 16

enum { GM_SPHERE, GM_CAPSULE, GM_BOX, GM_PLANE };

typedef struct {
  int type;
  int link;            // bodies: link frame (0 base); obstacles: -1 world
  double a[3], b[3];   // as above, in the link frame / world
  double R[3][3];      // box orientation
  double r;
  double wa[3], wb[3];  // bodies: a, b in the world after gm_update
  char name[16];
  int tool;            // bodies: of tool T only (0: always there)
  int dock;            // obstacles: of tool T's dock (0: always there)
} gm_shape;

typedef struct gm_world_s {
  gm_shape body[GM_MAXBODY];
  int nb;
  gm_shape obs[GM_MAXOBS];
  int no;
  unsigned char ignore[GM_MAXBODY][GM_MAXBODY];
  int selfskip;
  double k, c;         // plant contact
  int tool;            // the tool mounted now (0 none): its bodies count
  int dock;            // the dock being worked at (0 none, -1: no dock obstacles at all)
  // parsing the tool table: the tool being read, the docks [m, base frame]
  int cur_tool;
  int has_dock[GM_MAXTOOL];
  double dock_p[GM_MAXTOOL][3], dock_R[GM_MAXTOOL][3][3];
} gm_world;

// a body that is there now (not another tool's)
#define GM_ACTIVE(w, i) ((w)->body[i].tool == 0 || (w)->body[i].tool == (w)->tool)
// an obstacle that is there now (not an empty dock's, not the dock being worked at)
#define GM_OBS_ACTIVE(w, j) \
  ((w)->obs[j].dock == 0 || ((w)->dock >= 0 && (w)->obs[j].dock != (w)->tool && (w)->obs[j].dock != (w)->dock))

// A pair's closest points: d = signed distance (< 0: penetration) [m],
// pa on the body's core (segment / centre), pb on the other shape's
// surface, n = unit normal pushing the body away from the other.
typedef struct {
  double d;
  int body, other, self;  // self: other is a body, else an obstacle
  double pa[3], pb[3], n[3];
} gm_pair;

void gm_init(gm_world *w);
// One description line: 1 used, 0 not geometry (or malformed: -1).
int gm_line(gm_world *w, const char *line);
// Place the bodies for joints q (rad, robot.h's q).
void gm_update(gm_world *w, const rb_model *mdl, const double *q);
// Smallest signed distance over body-obstacle pairs and the checked
// body-body pairs; *best gets the pair (if not NULL). No shapes: 1e9.
double gm_clearance(const gm_world *w, gm_pair *best);
// Body-obstacle pairs with d < 0 (penetrating), up to max. Count.
int gm_contacts(const gm_world *w, gm_pair *out, int max);
// Signed distance between a body (sphere/capsule, placed) and a shape.
double gm_dist(const gm_shape *body, const gm_shape *other, int other_is_body, gm_pair *p);

#endif
