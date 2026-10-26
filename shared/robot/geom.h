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
} gm_world;

// a body that is there now (not another tool's)
#define GM_ACTIVE(w, i) ((w)->body[i].tool == 0 || (w)->body[i].tool == (w)->tool)

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
