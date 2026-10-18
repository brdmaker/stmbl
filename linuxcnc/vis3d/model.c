/*
 * vis3d: the model file, its geometry, the arm's frames and the camera.
 *
 * Model file (text, '#' comments, angles in degrees, lengths in the file's
 * unit, `units m` (default) or `units mm`):
 *
 *   dh J a alpha d theta0             Craig DH as genserkins / arm_sim (J from 1)
 *   prismatic J                       joint J moves d instead of theta
 *   joints offset|theta               joint values are offsets from theta0 (default, as
 *                                     arm_sim's q), or the DH theta itself (theta0
 *                                     included), as genserkins' joint positions
 *   pin J NAME                        the HAL pin with joint J's position
 *   limits J MIN MAX                  joint limits (deg / unit)
 *   tcp X Y Z                         tool point in the last link's frame
 *   material NAME [color R G B] [metal M] [rough R] [emissive E] [alpha A] [accent]
 *   part L box SX SY SZ [at X Y Z] [rpy A B C] [material M]
 *   part L cylinder R H [axis x|y|z] [at ..] [rpy ..] [material M]
 *   part L ring R T H [axis x|y|z] [at ..] [rpy ..] [material M]   a tube: radius, wall, height
 *   part L sphere R [at ..] [material M]
 *   part L capsule X1 Y1 Z1 X2 Y2 Z2 R [material M]
 *   part L mesh FILE.stl [scale S] [at ..] [rpy ..] [material M]
 *   body L capsule|sphere ... [tool T] arm_sim's collision bodies (shared/robot/geom.h):
 *                                     drawn with material "body"; tool bodies skipped
 *   obstacle NAME box CX CY CZ HX HY HZ [A B C] | capsule .. | sphere X Y Z R
 *                                     world obstacles, as geom.h (planes are skipped)
 *   camera YAW PITCH DIST [target X Y Z]
 *   include FILE                      relative to this file
 *
 * L is the link a part is fixed to: 0 the base, J the link after joint J.
 * Lines this loader doesn't know are skipped, so arm_sim's model files
 * (drive, link, motor, gear, ...) can be included for their dh, limits and
 * body lines.
 */
#include "vis3d_int.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEG (M_PI / 180.0)

typedef struct {
  vis3d_t *v;
  char *err;
  int errlen;
  int depth;
} parse_t;

static int fail(parse_t *p, const char *file, int line, const char *fmt, ...) {
  char msg[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);
  if(p->err && !p->err[0]) {
    snprintf(p->err, p->errlen, "%s:%d: %s", file, line, msg);
  }
  return -1;
}

// ---- geometry buffers --------------------------------------------------------

static void push_vert(vis3d_t *v, const float *p, const float *n) {
  if(v->nvert == v->capvert) {
    v->capvert = v->capvert ? 2 * v->capvert : 4096;
    v->vert    = realloc(v->vert, v->capvert * 6 * sizeof(float));
  }
  float *o = v->vert + v->nvert * 6;
  o[0] = p[0], o[1] = p[1], o[2] = p[2], o[3] = n[0], o[4] = n[1], o[5] = n[2];
  v->nvert++;
}

static void push_line(vis3d_t *v, const float *a, const float *b) {
  if(v->nline + 2 > v->capline) {
    v->capline = v->capline ? 2 * v->capline : 1024;
    v->line    = realloc(v->line, v->capline * 3 * sizeof(float));
  }
  memcpy(v->line + v->nline * 3, a, 3 * sizeof(float));
  memcpy(v->line + v->nline * 3 + 3, b, 3 * sizeof(float));
  v->nline += 2;
}

// a triangle placed by M (normals rotated by M)
static void tri(vis3d_t *v, mat4 M, const float *a, const float *b, const float *c, const float *na,
                const float *nb, const float *nc) {
  float p[3], n[3];
  m4_point(M, a, p), m4_dir(M, na, n), v3_norm(n), push_vert(v, p, n);
  m4_point(M, b, p), m4_dir(M, nb, n), v3_norm(n), push_vert(v, p, n);
  m4_point(M, c, p), m4_dir(M, nc, n), v3_norm(n), push_vert(v, p, n);
}

static void flat_tri(vis3d_t *v, mat4 M, const float *a, const float *b, const float *c) {
  float e1[3], e2[3], n[3];
  v3_sub(b, a, e1), v3_sub(c, a, e2), v3_cross(e1, e2, n), v3_norm(n);
  tri(v, M, a, b, c, n, n, n);
}

static void quad(vis3d_t *v, mat4 M, const float *a, const float *b, const float *c, const float *d) {
  flat_tri(v, M, a, b, c), flat_tri(v, M, a, c, d);
}

static void gen_box(vis3d_t *v, mat4 M, float sx, float sy, float sz) {
  float x = sx / 2, y = sy / 2, z = sz / 2;
  float p[8][3] = {{-x, -y, -z}, {x, -y, -z}, {x, y, -z}, {-x, y, -z}, {-x, -y, z}, {x, -y, z}, {x, y, z}, {-x, y, z}};
  quad(v, M, p[0], p[3], p[2], p[1]);  // -z
  quad(v, M, p[4], p[5], p[6], p[7]);  // +z
  quad(v, M, p[0], p[1], p[5], p[4]);  // -y
  quad(v, M, p[2], p[3], p[7], p[6]);  // +y
  quad(v, M, p[1], p[2], p[6], p[5]);  // +x
  quad(v, M, p[3], p[0], p[4], p[7]);  // -x
}

#define SEG 40

// a tube along z, centred: outer radius r, inner ri (0: a solid cylinder), height h
static void gen_tube(vis3d_t *v, mat4 M, float r, float ri, float h) {
  float z0 = -h / 2, z1 = h / 2;
  for(int i = 0; i < SEG; i++) {
    float a0 = 2 * M_PI * i / SEG, a1 = 2 * M_PI * (i + 1) / SEG;
    float c0 = cosf(a0), s0 = sinf(a0), c1 = cosf(a1), s1 = sinf(a1);
    float n0[3] = {c0, s0, 0}, n1[3] = {c1, s1, 0}, up[3] = {0, 0, 1}, dn[3] = {0, 0, -1};
    float A[3] = {r * c0, r * s0, z0}, B[3] = {r * c1, r * s1, z0}, C[3] = {r * c1, r * s1, z1}, D[3] = {r * c0, r * s0, z1};
    tri(v, M, A, B, C, n0, n1, n1), tri(v, M, A, C, D, n0, n1, n0);
    float ia[3] = {ri * c0, ri * s0, z1}, ib[3] = {ri * c1, ri * s1, z1};
    float ja[3] = {ri * c0, ri * s0, z0}, jb[3] = {ri * c1, ri * s1, z0};
    tri(v, M, D, C, ib, up, up, up), tri(v, M, D, ib, ia, up, up, up);  // top
    tri(v, M, A, ja, jb, dn, dn, dn), tri(v, M, A, jb, B, dn, dn, dn);  // bottom
    if(ri > 0) {
      float m0[3] = {-c0, -s0, 0}, m1[3] = {-c1, -s1, 0};
      tri(v, M, ja, ia, ib, m0, m0, m1), tri(v, M, ja, ib, jb, m0, m1, m1);
    }
  }
}

// a sphere of radius r; with h > 0 a capsule: two hemispheres h apart along z
static void gen_capsule(vis3d_t *v, mat4 M, float r, float h) {
  const int RINGS = 16;
  for(int k = 0; k < RINGS; k++) {
    for(int i = 0; i < SEG; i++) {
      float t0 = M_PI * k / RINGS, t1 = M_PI * (k + 1) / RINGS;
      float a0 = 2 * M_PI * i / SEG, a1 = 2 * M_PI * (i + 1) / SEG;
      float n[4][3] = {{sinf(t0) * cosf(a0), sinf(t0) * sinf(a0), cosf(t0)},
                       {sinf(t0) * cosf(a1), sinf(t0) * sinf(a1), cosf(t0)},
                       {sinf(t1) * cosf(a1), sinf(t1) * sinf(a1), cosf(t1)},
                       {sinf(t1) * cosf(a0), sinf(t1) * sinf(a0), cosf(t1)}};
      float p[4][3];
      for(int j = 0; j < 4; j++) {
        float off = n[j][2] >= 0 ? h / 2 : -h / 2;
        if(k == RINGS / 2 - 1 && (j == 2 || j == 3)) {
          off = h / 2;  // the equator ring belongs to the top half
        }
        p[j][0] = r * n[j][0], p[j][1] = r * n[j][1], p[j][2] = r * n[j][2] + off;
      }
      tri(v, M, p[0], p[2], p[1], n[0], n[2], n[1]);
      tri(v, M, p[0], p[3], p[2], n[0], n[3], n[2]);
    }
  }
  if(h > 0) {  // the straight part between the hemispheres
    for(int i = 0; i < SEG; i++) {
      float a0 = 2 * M_PI * i / SEG, a1 = 2 * M_PI * (i + 1) / SEG;
      float n0[3] = {cosf(a0), sinf(a0), 0}, n1[3] = {cosf(a1), sinf(a1), 0};
      float A[3] = {r * n0[0], r * n0[1], -h / 2}, B[3] = {r * n1[0], r * n1[1], -h / 2};
      float C[3] = {r * n1[0], r * n1[1], h / 2}, D[3] = {r * n0[0], r * n0[1], h / 2};
      tri(v, M, A, B, C, n0, n1, n1), tri(v, M, A, C, D, n0, n1, n0);
    }
  }
}

// the frame that turns +z into the segment a -> b, centred between them
static mat4 seg_frame(const float *a, const float *b, float *len) {
  float d[3];
  v3_sub(b, a, d);
  *len = sqrtf(v3_dot(d, d));
  mat4 M = m4_translate((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (a[2] + b[2]) / 2);
  if(*len < 1e-9f) {
    return M;
  }
  float z[3] = {d[0] / *len, d[1] / *len, d[2] / *len}, x[3], y[3];
  float ref[3] = {fabsf(z[0]) < 0.9f ? 1.0f : 0.0f, fabsf(z[0]) < 0.9f ? 0.0f : 1.0f, 0};
  v3_cross(ref, z, x), v3_norm(x), v3_cross(z, x, y);
  mat4 R = m4_identity();
  R.m[0] = x[0], R.m[1] = x[1], R.m[2] = x[2];
  R.m[4] = y[0], R.m[5] = y[1], R.m[6] = y[2];
  R.m[8] = z[0], R.m[9] = z[1], R.m[10] = z[2];
  return m4_mul(M, R);
}

// ---- STL ---------------------------------------------------------------------

static int load_stl(vis3d_t *v, mat4 M, const char *path, char *why, int whylen) {  // why: >= 600
  FILE *f = fopen(path, "rb");
  if(!f) {
    snprintf(why, whylen, "cannot open %s", path);
    return -1;
  }
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);
  unsigned char *b = malloc(size + 1);
  if(fread(b, 1, size, f) != (size_t)size) {
    fclose(f);
    free(b);
    snprintf(why, whylen, "cannot read %s", path);
    return -1;
  }
  fclose(f);
  b[size] = 0;
  uint32_t ntri = size >= 84 ? (uint32_t)b[80] | (uint32_t)b[81] << 8 | (uint32_t)b[82] << 16 | (uint32_t)b[83] << 24 : 0;
  int ok = 0;
  if(size >= 84 && 84 + 50 * (long)ntri == size) {  // binary: the size says so ("solid" headers lie)
    for(uint32_t t = 0; t < ntri; t++) {
      float q[12];
      memcpy(q, b + 84 + 50 * t, sizeof(q));
      flat_tri(v, M, q + 3, q + 6, q + 9);  // the file's normal is often zero: recompute
    }
    ok = 1;
  } else if(!strncmp((char *)b, "solid", 5)) {  // ASCII
    float p[3][3];
    int k = 0;
    for(char *s = strstr((char *)b, "vertex"); s; s = strstr(s + 6, "vertex")) {
      if(sscanf(s + 6, "%f %f %f", &p[k][0], &p[k][1], &p[k][2]) != 3) {
        break;
      }
      if(++k == 3) {
        flat_tri(v, M, p[0], p[1], p[2]);
        k = 0;
        ok = 1;
      }
    }
  }
  free(b);
  if(!ok) {
    snprintf(why, whylen, "%s is not an STL file", path);
    return -1;
  }
  return 0;
}

// ---- the parser --------------------------------------------------------------

static int find_mat(vis3d_t *v, const char *name) {
  for(int i = 0; i < v->nmat; i++) {
    if(!strcmp(v->mat[i].name, name)) {
      return i;
    }
  }
  return -1;
}

static v3_material *new_mat(vis3d_t *v, const char *name) {
  int i = find_mat(v, name);
  if(i < 0 && v->nmat < V3_MAXMAT) {
    i = v->nmat++;
    memset(&v->mat[i], 0, sizeof(v->mat[i]));
    snprintf(v->mat[i].name, sizeof(v->mat[i].name), "%s", name);
    v->mat[i].color[0] = v->mat[i].color[1] = v->mat[i].color[2] = 0.6f;
    v->mat[i].rough = 0.5f, v->mat[i].alpha = 1.0f;
  }
  return i < 0 ? NULL : &v->mat[i];
}

static void default_materials(vis3d_t *v) {
  struct {
    const char *n;
    float r, g, b, metal, rough, em, alpha;
    int accent;
  } d[] = {
      {"body", 0.82f, 0.85f, 0.88f, 0.25f, 0.42f, 0, 1, 0},  // painted arm
      {"dark", 0.10f, 0.12f, 0.15f, 0.6f, 0.35f, 0, 1, 0},   // motors, covers
      {"metal", 0.55f, 0.58f, 0.62f, 0.9f, 0.25f, 0, 1, 0},  // flange, tool
      {"accent", 0, 0, 0, 0.2f, 0.3f, 0.9f, 1, 1},           // rings, glowing lines
      {"warn", 0.94f, 0.68f, 0.25f, 0.1f, 0.5f, 0.15f, 1, 0},
      {"obstacle", 0.75f, 0.55f, 0.22f, 0.0f, 0.8f, 0, 0.35f, 0},
  };
  for(size_t i = 0; i < sizeof(d) / sizeof(d[0]); i++) {
    v3_material *m = new_mat(v, d[i].n);
    m->color[0] = d[i].r, m->color[1] = d[i].g, m->color[2] = d[i].b;
    m->metal = d[i].metal, m->rough = d[i].rough, m->emissive = d[i].em, m->alpha = d[i].alpha;
    m->accent = d[i].accent;
  }
}

// the options after a part's shape: at, rpy, axis, material, scale, tool
typedef struct {
  float at[3], rpy[3], scale;
  char axis;
  int mat, tool;
} opts_t;

static int parse_opts(parse_t *p, char **tok, int ntok, opts_t *o, const char *file, int line) {
  for(int i = 0; i < ntok; i++) {
    if(!strcmp(tok[i], "at") && i + 3 < ntok) {
      for(int k = 0; k < 3; k++) {
        o->at[k] = atof(tok[++i]) * p->v->unit;
      }
    } else if(!strcmp(tok[i], "rpy") && i + 3 < ntok) {
      for(int k = 0; k < 3; k++) {
        o->rpy[k] = atof(tok[++i]) * DEG;
      }
    } else if(!strcmp(tok[i], "axis") && i + 1 < ntok) {
      o->axis = tok[++i][0];
    } else if(!strcmp(tok[i], "material") && i + 1 < ntok) {
      o->mat = find_mat(p->v, tok[++i]);
      if(o->mat < 0) {
        return fail(p, file, line, "unknown material %s", tok[i]);
      }
    } else if(!strcmp(tok[i], "scale") && i + 1 < ntok) {
      o->scale = atof(tok[++i]);
    } else if(!strcmp(tok[i], "tool") && i + 1 < ntok) {
      o->tool = 1, i++;
    } else {
      return fail(p, file, line, "unexpected \"%s\"", tok[i]);
    }
  }
  return 0;
}

static mat4 place(const opts_t *o) {
  mat4 M = m4_mul(m4_translate(o->at[0], o->at[1], o->at[2]), m4_rpy(o->rpy[0], o->rpy[1], o->rpy[2]));
  if(o->axis == 'x') {
    M = m4_mul(M, m4_rot_y(M_PI / 2));
  } else if(o->axis == 'y') {
    M = m4_mul(M, m4_rot_x(-M_PI / 2));
  }
  return M;
}

static int begin_part(vis3d_t *v, int link, int mat) {
  if(v->npart == V3_MAXPART) {
    return -1;
  }
  v3_part *pt = &v->part[v->npart];
  pt->link = link, pt->material = mat, pt->first = v->nvert, pt->count = 0;
  return 0;
}

static void end_part(vis3d_t *v) {
  v3_part *pt = &v->part[v->npart];
  pt->count   = v->nvert - pt->first;
  if(pt->count) {
    v->npart++;
  }
}

static double num(char **tok, int i, double unit) {
  return atof(tok[i]) * unit;
}

static int parse_file(parse_t *p, const char *path);

static int parse_line(parse_t *p, const char *file, int line, char **tok, int ntok, const char *dir) {
  vis3d_t *v = p->v;
  const char *k = tok[0];
  double u = v->unit;
#define NEED(n)                                                       \
  do {                                                                \
    if(ntok < (n)) {                                                  \
      return fail(p, file, line, "%s needs %d values", k, (n) - 1); \
    }                                                                 \
  } while(0)
#define JOINT(i)                                                                \
  int j = atoi(tok[i]) - 1;                                                     \
  if(j < 0 || j >= VIS3D_MAXJ) {                                                \
    return fail(p, file, line, "joint %s out of 1..%d", tok[i], VIS3D_MAXJ); \
  }

  if(!strcmp(k, "units")) {
    NEED(2);
    if(!strcmp(tok[1], "mm")) {
      v->unit = 0.001;
    } else if(!strcmp(tok[1], "m")) {
      v->unit = 1.0;
    } else {
      return fail(p, file, line, "units m or mm");
    }
  } else if(!strcmp(k, "dh")) {
    NEED(6);
    JOINT(1);
    v->a[j] = num(tok, 2, u), v->alpha[j] = atof(tok[3]) * DEG, v->d[j] = num(tok, 4, u);
    v->theta0[j] = atof(tok[5]) * DEG;
    if(j + 1 > v->n) {
      v->n = j + 1;
    }
  } else if(!strcmp(k, "joints")) {
    NEED(2);
    if(strcmp(tok[1], "theta") && strcmp(tok[1], "offset")) {
      return fail(p, file, line, "joints offset or theta");
    }
    v->q_theta = !strcmp(tok[1], "theta");
  } else if(!strcmp(k, "prismatic")) {
    NEED(2);
    JOINT(1);
    v->prismatic[j] = 1;
  } else if(!strcmp(k, "pin")) {
    NEED(3);
    JOINT(1);
    snprintf(v->pin[j], sizeof(v->pin[j]), "%s", tok[2]);
  } else if(!strcmp(k, "limits")) {
    NEED(4);
    JOINT(1);
    v->lim[j][0] = atof(tok[2]), v->lim[j][1] = atof(tok[3]);
  } else if(!strcmp(k, "tcp")) {
    NEED(4);
    for(int i = 0; i < 3; i++) {
      v->tcp[i] = num(tok, 1 + i, u);
    }
  } else if(!strcmp(k, "material")) {
    NEED(2);
    v3_material *m = new_mat(v, tok[1]);
    if(!m) {
      return fail(p, file, line, "too many materials (%d)", V3_MAXMAT);
    }
    for(int i = 2; i < ntok; i++) {
      if(!strcmp(tok[i], "color") && i + 3 < ntok) {
        for(int c = 0; c < 3; c++) {
          m->color[c] = atof(tok[++i]);
        }
      } else if(!strcmp(tok[i], "metal") && i + 1 < ntok) {
        m->metal = atof(tok[++i]);
      } else if(!strcmp(tok[i], "rough") && i + 1 < ntok) {
        m->rough = atof(tok[++i]);
      } else if(!strcmp(tok[i], "emissive") && i + 1 < ntok) {
        m->emissive = atof(tok[++i]);
      } else if(!strcmp(tok[i], "alpha") && i + 1 < ntok) {
        m->alpha = atof(tok[++i]);
      } else if(!strcmp(tok[i], "accent")) {
        m->accent = 1;
      } else {
        return fail(p, file, line, "material: unexpected \"%s\"", tok[i]);
      }
    }
  } else if(!strcmp(k, "part") || !strcmp(k, "body")) {
    NEED(3);
    int link = atoi(tok[1]);
    if(link < 0 || link > VIS3D_MAXJ) {
      return fail(p, file, line, "link %d out of 0..%d", link, VIS3D_MAXJ);
    }
    const char *shape = tok[2];
    int nargs = !strcmp(shape, "box") ? 3 : !strcmp(shape, "cylinder") ? 2 : !strcmp(shape, "ring") ? 3
              : !strcmp(shape, "sphere") ? (!strcmp(k, "body") ? 4 : 1) : !strcmp(shape, "capsule") ? 7
              : !strcmp(shape, "mesh") ? 1 : -1;
    if(nargs < 0) {
      return fail(p, file, line, "unknown shape %s", shape);
    }
    NEED(3 + nargs);
    opts_t o = {{0, 0, 0}, {0, 0, 0}, 1.0f, 'z', find_mat(v, k[0] == 'b' ? "body" : "body"), 0};
    if(parse_opts(p, tok + 3 + nargs, ntok - 3 - nargs, &o, file, line) < 0) {
      return -1;
    }
    if(o.tool) {
      return 0;  // a tool's collision body: the tool's own parts draw it
    }
    if(begin_part(v, link, o.mat) < 0) {
      return fail(p, file, line, "too many parts (%d)", V3_MAXPART);
    }
    char **a = tok + 3;
    if(!strcmp(shape, "box")) {
      gen_box(v, place(&o), num(a, 0, u), num(a, 1, u), num(a, 2, u));
    } else if(!strcmp(shape, "cylinder")) {
      gen_tube(v, place(&o), num(a, 0, u), 0, num(a, 1, u));
    } else if(!strcmp(shape, "ring")) {
      gen_tube(v, place(&o), num(a, 0, u), num(a, 0, u) - num(a, 1, u), num(a, 2, u));
    } else if(!strcmp(shape, "sphere")) {
      if(!strcmp(k, "body")) {  // body L sphere x y z r
        o.at[0] = num(a, 0, u), o.at[1] = num(a, 1, u), o.at[2] = num(a, 2, u);
        gen_capsule(v, place(&o), num(a, 3, u), 0);
      } else {
        gen_capsule(v, place(&o), num(a, 0, u), 0);
      }
    } else if(!strcmp(shape, "capsule")) {
      float pa[3] = {num(a, 0, u), num(a, 1, u), num(a, 2, u)}, pb[3] = {num(a, 3, u), num(a, 4, u), num(a, 5, u)}, len;
      mat4 M = seg_frame(pa, pb, &len);
      gen_capsule(v, M, num(a, 6, u), len);
    } else {  // mesh
      char path[512], why[600];
      snprintf(path, sizeof(path), "%s%s", a[0][0] == '/' ? "" : dir, a[0]);
      mat4 M = m4_mul(place(&o), m4_scale(o.scale * u, o.scale * u, o.scale * u));
      if(load_stl(v, M, path, why, sizeof(why)) < 0) {
        return fail(p, file, line, "%s", why);
      }
    }
    end_part(v);
  } else if(!strcmp(k, "obstacle")) {
    NEED(3);
    if(v->nobs == V3_MAXOBS) {
      return fail(p, file, line, "too many obstacles (%d)", V3_MAXOBS);
    }
    if(!strcmp(tok[2], "plane")) {
      return 0;  // the floor: the grid draws it
    }
    v3_obstacle *ob = &v->obs[v->nobs];
    snprintf(ob->name, sizeof(ob->name), "%s", tok[1]);
    char **a = tok + 3;
    int om   = find_mat(v, "obstacle");
    begin_part(v, -1 - v->nobs, om);  // negative link: obstacle i (world)
    ob->first = v->nvert, ob->line_first = v->nline;
    if(!strcmp(tok[2], "box")) {
      NEED(9);
      float c[3] = {num(a, 0, u), num(a, 1, u), num(a, 2, u)}, h[3] = {num(a, 3, u), num(a, 4, u), num(a, 5, u)};
      float r[3] = {0, 0, 0};
      if(ntok >= 12) {
        for(int i = 0; i < 3; i++) {
          r[i] = atof(a[6 + i]) * DEG;
        }
      }
      mat4 M = m4_mul(m4_translate(c[0], c[1], c[2]), m4_rpy(r[0], r[1], r[2]));
      gen_box(v, M, 2 * h[0], 2 * h[1], 2 * h[2]);
      float q[8][3];
      for(int i = 0; i < 8; i++) {
        float l[3] = {(i & 1) ? h[0] : -h[0], (i & 2) ? h[1] : -h[1], (i & 4) ? h[2] : -h[2]};
        m4_point(M, l, q[i]);
      }
      int e[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
      for(int i = 0; i < 12; i++) {
        push_line(v, q[e[i][0]], q[e[i][1]]);
      }
    } else if(!strcmp(tok[2], "capsule")) {
      NEED(10);
      float pa[3] = {num(a, 0, u), num(a, 1, u), num(a, 2, u)}, pb[3] = {num(a, 3, u), num(a, 4, u), num(a, 5, u)}, len;
      mat4 M = seg_frame(pa, pb, &len);  // first: len is set here
      gen_capsule(v, M, num(a, 6, u), len);
    } else if(!strcmp(tok[2], "sphere")) {
      NEED(7);
      gen_capsule(v, m4_translate(num(a, 0, u), num(a, 1, u), num(a, 2, u)), num(a, 3, u), 0);
    } else {
      return fail(p, file, line, "obstacle shape box, capsule, sphere or plane");
    }
    ob->count = v->nvert - ob->first, ob->line_count = v->nline - ob->line_first;
    for(int i = 0; i < 3; i++) {
      ob->lo[i] = 1e30f, ob->hi[i] = -1e30f;
    }
    for(int i = ob->first; i < ob->first + ob->count; i++) {
      for(int c = 0; c < 3; c++) {
        ob->lo[c] = fminf(ob->lo[c], v->vert[i * 6 + c]), ob->hi[c] = fmaxf(ob->hi[c], v->vert[i * 6 + c]);
      }
    }
    end_part(v);
    v->nobs++;
  } else if(!strcmp(k, "camera")) {
    NEED(4);
    v->yaw = atof(tok[1]), v->pitch = atof(tok[2]), v->dist = num(tok, 3, u);
    if(ntok >= 8 && !strcmp(tok[4], "target")) {
      for(int i = 0; i < 3; i++) {
        v->target[i] = num(tok, 5 + i, u);
      }
    }
    v->cam_set = 1;
  } else if(!strcmp(k, "include")) {
    NEED(2);
    char path[512];
    snprintf(path, sizeof(path), "%s%s", tok[1][0] == '/' ? "" : dir, tok[1]);
    double keep = v->unit;  // an included file starts in metres, as arm_sim's
    v->unit     = 1.0;
    int r       = parse_file(p, path);
    v->unit     = keep;
    if(r < 0) {
      return fail(p, file, line, "in %s", tok[1]);
    }
  }
  // anything else: an arm_sim line this view doesn't need
  return 0;
#undef NEED
#undef JOINT
}

static int parse_file(parse_t *p, const char *path) {
  if(++p->depth > 8) {
    return fail(p, path, 0, "includes nested too deep");
  }
  FILE *f = fopen(path, "r");
  if(!f) {
    p->depth--;
    return fail(p, path, 0, "cannot open");
  }
  char dir[512];
  snprintf(dir, sizeof(dir), "%s", path);
  char *slash = strrchr(dir, '/');
  if(slash) {
    slash[1] = 0;
  } else {
    dir[0] = 0;
  }
  char buf[1024];
  int line = 0, rc = 0;
  while(rc == 0 && fgets(buf, sizeof(buf), f)) {
    line++;
    char *hash = strchr(buf, '#');
    if(hash) {
      *hash = 0;
    }
    char *tok[48];
    int ntok = 0;
    for(char *s = strtok(buf, " \t\r\n"); s && ntok < 48; s = strtok(NULL, " \t\r\n")) {
      tok[ntok++] = s;
    }
    if(ntok) {
      rc = parse_line(p, path, line, tok, ntok, dir);
    }
  }
  fclose(f);
  p->depth--;
  return rc;
}

// ---- API: the model ----------------------------------------------------------

vis3d_t *vis3d_load(const char *path, char *err, int errlen) {
  vis3d_t *v = calloc(1, sizeof(vis3d_t));
  v->unit = 1.0;
  for(int j = 0; j < VIS3D_MAXJ; j++) {
    v->lim[j][0] = -180, v->lim[j][1] = 180;
  }
  v->accent[0] = 0.15f, v->accent[1] = 0.81f, v->accent[2] = 0.76f;
  v->show_obs = v->show_grid = 1;
  v->nearest  = -1;
  v->trail_on = 1;
  default_materials(v);
  if(err && errlen) {
    err[0] = 0;
  }
  parse_t p = {v, err, errlen, 0};
  if(parse_file(&p, path) < 0) {
    vis3d_free(v);
    return NULL;
  }
  if(v->n == 0) {
    if(err) {
      snprintf(err, errlen, "%s: no dh lines: no joints", path);
    }
    vis3d_free(v);
    return NULL;
  }
  v3_update_frames(v);
  // bounds at the zero pose, for the default camera
  for(int c = 0; c < 3; c++) {
    v->lo[c] = 1e30f, v->hi[c] = -1e30f;
  }
  for(int i = 0; i < v->npart; i++) {
    const v3_part *pt = &v->part[i];
    if(pt->link < 0) {
      continue;
    }
    for(int k = pt->first; k < pt->first + pt->count; k++) {
      float w[3];
      m4_point(v->link[pt->link], v->vert + k * 6, w);
      for(int c = 0; c < 3; c++) {
        v->lo[c] = fminf(v->lo[c], w[c]), v->hi[c] = fmaxf(v->hi[c], w[c]);
      }
    }
  }
  if(v->lo[0] > v->hi[0]) {  // no parts: around the frames
    for(int c = 0; c < 3; c++) {
      v->lo[c] = -0.5f, v->hi[c] = 0.5f;
    }
  }
  if(!v->cam_set) {
    float sz = fmaxf(fmaxf(v->hi[0] - v->lo[0], v->hi[1] - v->lo[1]), v->hi[2] - v->lo[2]);
    v->yaw = 35, v->pitch = 22, v->dist = 2.4 * sz;
    v->target[0] = 0, v->target[1] = 0, v->target[2] = 0.45 * v->hi[2];
  }
  v->trail_n = 0;
  return v;
}

int vis3d_joints(const vis3d_t *v) {
  return v->n;
}
const char *vis3d_joint_pin(const vis3d_t *v, int j) {
  return j >= 0 && j < v->n ? v->pin[j] : "";
}
void vis3d_joint_limits(const vis3d_t *v, int j, double *mn, double *mx) {
  if(j >= 0 && j < v->n) {
    *mn = v->lim[j][0], *mx = v->lim[j][1];
  }
}
int vis3d_obstacles(const vis3d_t *v) {
  return v->nobs;
}
const char *vis3d_obstacle_name(const vis3d_t *v, int i) {
  return i >= 0 && i < v->nobs ? v->obs[i].name : "";
}

void v3_update_frames(vis3d_t *v) {
  mat4 T     = m4_identity();
  v->link[0] = T;
  for(int j = 0; j < v->n; j++) {
    double th = v->q_theta ? 0.0 : v->theta0[j], d = v->d[j];
    if(v->prismatic[j]) {
      d += v->q[j] * v->unit;
    } else {
      th += v->q[j] * DEG;
    }
    T              = m4_mul(T, m4_dh(v->a[j], v->alpha[j], d, th));
    v->link[j + 1] = T;
  }
  float t[3] = {v->tcp[0], v->tcp[1], v->tcp[2]};
  m4_point(T, t, v->tcp_world);
}

void vis3d_set_joints(vis3d_t *v, const double *q, int n) {
  for(int j = 0; j < n && j < v->n; j++) {
    v->q[j] = q[j];
  }
  v3_update_frames(v);
  if(v->trail_on) {  // a new point once the tool point moved 2 mm
    float *last = v->trail[(v->trail_head + V3_TRAIL - 1) % V3_TRAIL], d[3];
    v3_sub(v->tcp_world, last, d);
    if(v->trail_n == 0 || v3_dot(d, d) > 4e-6f) {
      memcpy(v->trail[v->trail_head], v->tcp_world, sizeof(v->tcp_world));
      v->trail_head = (v->trail_head + 1) % V3_TRAIL;
      if(v->trail_n < V3_TRAIL) {
        v->trail_n++;
      }
    }
  }
}

void vis3d_frame(const vis3d_t *v, int k, double p[3], double R[9]) {
  mat4 T = k < 0 || k > v->n ? v->link[v->n] : v->link[k];
  for(int r = 0; r < 3; r++) {
    for(int c = 0; c < 3; c++) {
      R[r * 3 + c] = T.m[c * 4 + r];
    }
  }
  // in the model's unit, as everything else in the API
  if(k < 0) {
    for(int i = 0; i < 3; i++) {
      p[i] = v->tcp_world[i] / v->unit;
    }
  } else {
    p[0] = T.m[12] / v->unit, p[1] = T.m[13] / v->unit, p[2] = T.m[14] / v->unit;
  }
}

void vis3d_set_guard(vis3d_t *v, int state, double radius, int nearest) {
  v->guard = state, v->guard_r = radius * v->unit, v->nearest = nearest;
}
void vis3d_set_tint(vis3d_t *v, int link, float r, float g, float b, float a) {
  if(link >= 0 && link <= VIS3D_MAXJ) {
    v->tint[link][0] = r, v->tint[link][1] = g, v->tint[link][2] = b, v->tint[link][3] = a;
  }
}
void vis3d_trail(vis3d_t *v, int on, int clear) {
  v->trail_on = on;
  if(clear) {
    v->trail_n = 0, v->trail_head = 0;
  }
}
void vis3d_set_accent(vis3d_t *v, float r, float g, float b) {
  v->accent[0] = r, v->accent[1] = g, v->accent[2] = b;
}
void vis3d_show(vis3d_t *v, int obstacles, int grid) {
  v->show_obs = obstacles, v->show_grid = grid;
}

// ---- camera ------------------------------------------------------------------

void vis3d_camera(vis3d_t *v, double yaw, double pitch, double dist, const double target[3]) {
  v->yaw = yaw, v->pitch = pitch, v->dist = dist * v->unit;
  if(target) {
    for(int i = 0; i < 3; i++) {
      v->target[i] = target[i] * v->unit;
    }
  }
  v->cam_set = 1;
}

void vis3d_orbit(vis3d_t *v, double dx, double dy) {
  v->yaw -= dx * 0.3;
  v->pitch += dy * 0.3;
  v->pitch = v->pitch < -5 ? -5 : v->pitch > 85 ? 85 : v->pitch;
}

void vis3d_zoom(vis3d_t *v, double f) {
  v->dist *= f;
  double s = fmax(fmax(v->hi[0] - v->lo[0], v->hi[1] - v->lo[1]), v->hi[2] - v->lo[2]);
  v->dist  = v->dist < 0.3 * s ? 0.3 * s : v->dist > 20 * s ? 20 * s : v->dist;
}

static void eye_of(const vis3d_t *v, float *eye) {
  double y = v->yaw * DEG, p = v->pitch * DEG;
  eye[0] = v->target[0] + v->dist * cos(p) * cos(y);
  eye[1] = v->target[1] + v->dist * cos(p) * sin(y);
  eye[2] = v->target[2] + v->dist * sin(p);
}

void vis3d_pan(vis3d_t *v, double dx, double dy) {
  double y = v->yaw * DEG, s = v->dist * 0.0015;
  v->target[0] += s * (dx * sin(y));
  v->target[1] -= s * (dx * cos(y));
  v->target[2] += s * dy;
}

mat4 v3_view(const vis3d_t *v) {
  float eye[3], c[3] = {v->target[0], v->target[1], v->target[2]};
  eye_of(v, eye);
  return m4_look_at(eye, c);
}

mat4 v3_proj(const vis3d_t *v) {
  float aspect = v->h > 0 ? (float)v->w / v->h : 1.0f;
  return m4_perspective(38 * DEG, aspect, v->dist * 0.02f, v->dist * 20.0f);
}

int vis3d_project(const vis3d_t *v, const double p[3], double *x, double *y) {
  mat4 M  = m4_mul(v3_proj(v), v3_view(v));
  float q[3] = {p[0] * v->unit, p[1] * v->unit, p[2] * v->unit};
  float cx = M.m[0] * q[0] + M.m[4] * q[1] + M.m[8] * q[2] + M.m[12];
  float cy = M.m[1] * q[0] + M.m[5] * q[1] + M.m[9] * q[2] + M.m[13];
  float cw = M.m[3] * q[0] + M.m[7] * q[1] + M.m[11] * q[2] + M.m[15];
  if(cw <= 1e-6f) {
    return 0;
  }
  *x = (cx / cw * 0.5 + 0.5) * v->w;
  *y = (0.5 - cy / cw * 0.5) * v->h;
  return 1;
}

void vis3d_stats(const vis3d_t *v, int *tris, int *draws) {
  *tris  = v->nvert / 3;
  *draws = v->npart;
}
