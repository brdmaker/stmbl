/*
 * vis3d: the internal state shared by the model loader (model.c) and the
 * renderer (render.c). Not part of the API.
 */
#ifndef VIS3D_INT_H
#define VIS3D_INT_H

#include "vis3d.h"
#include "vmath.h"

#define V3_MAXMAT 32
#define V3_MAXPART 256
#define V3_MAXOBS 32
#define V3_TRAIL 512
#define V3_MAXTOOL 16

typedef struct {
  char name[32];
  float color[3];
  float metal, rough, emissive, alpha;
  int accent;  // takes the accent color (rings, highlights)
} v3_material;

// a run of triangles fixed to one link (0 = base), already placed in it
typedef struct {
  int link, material;
  int first, count;  // vertices in vis3d.vert
  int tool;          // a tool's (its id), drawn on the flange or in its dock; 0 the arm's
  int tool_body;     // the tool's collision body: only drawn if the tool has no parts
} v3_part;

// a tool of the tool changer (shared/robot/tool.h's tool table)
typedef struct {
  int id;
  char name[32];
  float tcp[3];     // its tool point in the flange frame [m]
  int has_tcp;
  int has_dock;
  mat4 dock;        // the flange's pose with the tool in its dock (base frame)
  int has_parts;    // dressed with parts: its collision bodies aren't drawn
} v3_tool;

typedef struct {
  char name[32];
  int first, count;            // its triangles
  int line_first, line_count;  // its edges (boxes)
  float lo[3], hi[3];          // bounds, for the camera
} v3_obstacle;

struct gl_state;

struct vis3d {
  // the chain: Craig DH per joint, in metres and radians
  int n;
  double a[VIS3D_MAXJ], alpha[VIS3D_MAXJ], d[VIS3D_MAXJ], theta0[VIS3D_MAXJ];
  int prismatic[VIS3D_MAXJ];
  char pin[VIS3D_MAXJ][64];
  double lim[VIS3D_MAXJ][2];  // [deg] or [unit]
  double tcp[3];              // tool point in the last link's frame [m]
  double unit;                // file unit in metres (1 or 0.001)
  int q_theta;                // joint values are DH theta (theta0 included), as genserkins' joints
  v3_tool tool[V3_MAXTOOL];
  int ntool;
  int mounted;                // the tool on the flange (id, 0 none)

  double q[VIS3D_MAXJ];         // joints as set [deg / unit]
  mat4 link[VIS3D_MAXJ + 1];    // link frames in the base frame
  float tcp_world[3];

  // geometry: interleaved position (3) and normal (3) per vertex
  float *vert;
  int nvert, capvert;
  float *line;  // line vertices, position only
  int nline, capline;
  v3_part part[V3_MAXPART];
  int npart;
  v3_material mat[V3_MAXMAT];
  int nmat;
  v3_obstacle obs[V3_MAXOBS];
  int nobs;
  float lo[3], hi[3];  // model bounds at zero pose, for the default camera
  int geom_dirty;      // geometry added after gl_init (a cell): upload again

  // overlays
  int guard, nearest;
  float guard_r;
  float tint[VIS3D_MAXJ + 1][4];
  float accent[3];
  int show_obs, show_grid;
  int trail_on, trail_n, trail_head;
  float trail[V3_TRAIL][3];

  // camera
  double yaw, pitch, dist, target[3];
  int cam_set;  // the model or the host set it
  int w, h;

  struct gl_state *gl;
};

void v3_update_frames(vis3d_t *v);
// the frame part pt is drawn in, NULL: not drawn (a tool neither mounted nor docked)
const mat4 *v3_part_frame(const vis3d_t *v, const v3_part *pt);
mat4 v3_view(const vis3d_t *v);
mat4 v3_proj(const vis3d_t *v);
const char *v3_glsl_for(const char *gl_version);

#endif
