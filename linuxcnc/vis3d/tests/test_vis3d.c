/*
 * vis3d without GL: the model loader, the kinematics (against shared/robot's
 * rb_fk, the arm_sim / armkins model), STL loading, units, errors, the
 * camera's projection. TAP output.
 */
#include "robot.h"
#include "vis3d.h"
#include "vis3d_int.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int n, failed;

static void ok(int cond, const char *what) {
  n++;
  printf("%s %d - %s\n", cond ? "ok" : "not ok", n, what);
  failed += !cond;
}

static const char *TMP = "tests/tmp";

static void write(const char *name, const char *text) {
  char p[256];
  snprintf(p, sizeof(p), "%s/%s", TMP, name);
  FILE *f = fopen(p, "w");
  fputs(text, f);
  fclose(f);
}

static vis3d_t *load(const char *name, char *err) {
  char p[256];
  snprintf(p, sizeof(p), "%s/%s", TMP, name);
  return vis3d_load(p, err, 300);
}

static void test_arm6(void) {
  char err[300];
  vis3d_t *v = vis3d_load("models/arm6.vis", err, sizeof(err));
  ok(v != NULL, "arm6: loads (with arm_sim's model included)");
  if(!v) {
    printf("# %s\n", err);
    return;
  }
  ok(vis3d_joints(v) == 6, "arm6: 6 joints from the dh lines");
  ok(!strcmp(vis3d_joint_pin(v, 0), "joint.0.pos-fb") && !strcmp(vis3d_joint_pin(v, 5), "joint.5.pos-fb"),
     "arm6: HAL pins");
  double mn, mx;
  vis3d_joint_limits(v, 1, &mn, &mx);
  ok(mn == -60 && mx == 90, "arm6: J2 limits from arcmate_base.arm");
  ok(vis3d_obstacles(v) == 3 && !strcmp(vis3d_obstacle_name(v, 0), "conveyor"), "arm6: 3 obstacles");
  int tris, draws;
  vis3d_stats(v, &tris, &draws);
  printf("# arm6: %d triangles, %d draws\n", tris, draws);
  ok(tris > 5000 && tris < 60000 && draws > 20, "arm6: a few thousand triangles, a draw per part");

  // the kinematics: the same frames as shared/robot (arm_sim, armkins)
  rb_model m;
  memset(&m, 0, sizeof(m));
  m.n                 = 6;
  double dh[6][4]     = {{0, 0, 0.45, 0}, {0.15, -90, 0, -90}, {0.60, 0, 0, 0}, {0.12, -90, 0.65, 0}, {0, 90, 0, 0}, {0, -90, 0.10, 180}};
  for(int j = 0; j < 6; j++) {
    m.link[j].a = dh[j][0], m.link[j].alpha = dh[j][1] * M_PI / 180, m.link[j].d = dh[j][2];
    m.link[j].theta0 = dh[j][3] * M_PI / 180;
  }
  srand(7);
  double worst_p = 0, worst_r = 0;
  for(int trial = 0; trial < 200; trial++) {
    double q[6], qr[6], R[6][3][3], p[6][3];
    for(int j = 0; j < 6; j++) {
      q[j] = (rand() / (double)RAND_MAX - 0.5) * 300, qr[j] = q[j] * M_PI / 180;
    }
    vis3d_set_joints(v, q, 6);
    rb_fk(&m, qr, R, p);
    for(int k = 1; k <= 6; k++) {
      double vp[3], vR[9];
      vis3d_frame(v, k, vp, vR);
      for(int i = 0; i < 3; i++) {
        worst_p = fmax(worst_p, fabs(vp[i] - p[k - 1][i]));
        for(int c = 0; c < 3; c++) {
          worst_r = fmax(worst_r, fabs(vR[i * 3 + c] - R[k - 1][i][c]));
        }
      }
    }
  }
  printf("# FK vs rb_fk over 200 poses: position %.2g m, rotation %.2g\n", worst_p, worst_r);
  ok(worst_p < 1e-5 && worst_r < 1e-5, "arm6: link frames match shared/robot's rb_fk (200 random poses)");

  double q[6] = {10, 20, 30, 40, 50, 60}, p6[3], R6[9], pt[3], Rt[9];
  vis3d_set_joints(v, q, 6);
  vis3d_frame(v, 6, p6, R6);
  vis3d_frame(v, -1, pt, Rt);
  double want[3] = {p6[0] + R6[2] * 0.13, p6[1] + R6[5] * 0.13, p6[2] + R6[8] * 0.13};
  ok(fabs(pt[0] - want[0]) < 1e-5 && fabs(pt[1] - want[1]) < 1e-5 && fabs(pt[2] - want[2]) < 1e-5,
     "arm6: the tool point is tcp in the flange frame");

  vis3d_resize(v, 1280, 720);
  double x, y;
  ok(vis3d_project(v, pt, &x, &y) && x > 0 && x < 1280 && y > 0 && y < 720, "camera: the tool point is in view");
  double eyeback[3];  // twice as far as the camera, on its side: behind it
  {
    double yaw = v->yaw * M_PI / 180, pit = v->pitch * M_PI / 180;
    for(int i = 0; i < 3; i++) {
      eyeback[i] = v->target[i];
    }
    eyeback[0] += 2 * v->dist * cos(pit) * cos(yaw), eyeback[1] += 2 * v->dist * cos(pit) * sin(yaw);
    eyeback[2] += 2 * v->dist * sin(pit);
  }
  ok(!vis3d_project(v, eyeback, &x, &y), "camera: a point behind the camera doesn't project");
  double yaw0 = v->yaw;
  vis3d_orbit(v, 100, 0);
  vis3d_zoom(v, 1000);
  ok(v->yaw != yaw0 && v->dist < 100, "camera: orbit turns, zoom is bounded");

  vis3d_trail(v, 1, 1);
  for(int i = 0; i < 50; i++) {
    double qq[6] = {i * 1.0, 0, 0, 0, -90, 0};
    vis3d_set_joints(v, qq, 6);
  }
  ok(v->trail_n == 50, "trail: a point per move of more than 2 mm");
  vis3d_set_joints(v, (double[]){49.0001, 0, 0, 0, -90, 0}, 6);
  ok(v->trail_n == 50, "trail: no point for a move below 2 mm");
  vis3d_free(v);
}

static void test_joints_theta(void) {
  char err[300];
  write("theta.vis", "include ../../models/arm6.vis\njoints theta\n");
  vis3d_t *a = load("theta.vis", err), *b = vis3d_load("models/arm6.vis", err, 300);
  ok(a && b, "joints theta: loads");
  if(!a || !b) {
    return;
  }
  // genserkins' joint = the model's offset + theta0 (J2 -90, J6 180)
  double q[6] = {10, 20, 30, 40, 50, 60}, th[6] = {10, 20 - 90, 30, 40, 50, 60 + 180};
  vis3d_set_joints(b, q, 6);
  vis3d_set_joints(a, th, 6);
  double pa[3], pb[3], Ra[9], Rb[9], worst = 0;
  vis3d_frame(a, -1, pa, Ra), vis3d_frame(b, -1, pb, Rb);
  for(int i = 0; i < 3; i++) {
    worst = fmax(worst, fabs(pa[i] - pb[i]));
  }
  ok(worst < 1e-6, "joints theta: genserkins' joint values give the same pose");
  vis3d_free(a);
  vis3d_free(b);
}

static void test_units_and_shapes(void) {
  char err[300];
  write("mm.vis",
        "units mm\n"
        "dh 1 0 0 450 0\n"
        "dh 2 150 -90 0 -90\n"
        "tcp 0 0 100\n"
        "part 1 box 100 100 100 material dark\n"
        "part 2 capsule 0 0 0 600 0 0 90\n"
        "part 2 ring 100 10 30 axis y at 0 0 0\n"
        "obstacle wall box 1000 0 500 50 800 500\n"
        "obstacle floor plane 0 0 0 0 0 1\n");
  vis3d_t *v = load("mm.vis", err);
  ok(v != NULL, "units mm: loads");
  if(!v) {
    printf("# %s\n", err);
    return;
  }
  double p[3], R[9];
  vis3d_frame(v, 1, p, R);
  ok(fabs(p[2] - 450) < 1e-3, "units mm: frames in the model's unit (450 mm)");
  ok(fabs(v->d[0] - 0.45) < 1e-9, "units mm: metres inside");
  ok(vis3d_obstacles(v) == 1, "obstacles: a plane is the floor, not drawn");
  int tris, draws;
  vis3d_stats(v, &tris, &draws);
  ok(draws == 4, "parts: box, capsule, ring, the wall");
  vis3d_free(v);
}

static void test_stl(void) {
  char err[300];
  // binary: a tetrahedron, normals zero (the loader computes them)
  float P[4][3] = {{0, 0, 0}, {10, 0, 0}, {0, 10, 0}, {0, 0, 10}};
  int F[4][3]   = {{0, 2, 1}, {0, 1, 3}, {0, 3, 2}, {1, 2, 3}};
  char path[256];
  snprintf(path, sizeof(path), "%s/tet.stl", TMP);
  FILE *f = fopen(path, "wb");
  char hdr[80] = "solid this header says solid but the file is binary";
  fwrite(hdr, 1, 80, f);
  unsigned int nt = 4;
  fwrite(&nt, 4, 1, f);
  for(int t = 0; t < 4; t++) {
    float rec[12] = {0};
    for(int k = 0; k < 3; k++) {
      memcpy(rec + 3 + 3 * k, P[F[t][k]], 12);
    }
    fwrite(rec, 4, 12, f);
    unsigned short attr = 0;
    fwrite(&attr, 2, 1, f);
  }
  fclose(f);
  write("tet_ascii.stl", "solid t\n facet normal 0 0 0\n  outer loop\n   vertex 0 0 0\n   vertex 10 0 0\n   vertex 0 10 0\n"
                         "  endloop\n endfacet\n facet normal 0 0 0\n  outer loop\n   vertex 0 0 0\n   vertex 0 0 10\n"
                         "   vertex 10 0 0\n  endloop\n endfacet\nendsolid t\n");
  write("stl.vis", "units mm\ndh 1 0 0 0 0\npart 1 mesh tet.stl scale 2 at 0 0 100\npart 1 mesh tet_ascii.stl\n");
  vis3d_t *v = load("stl.vis", err);
  ok(v != NULL, "stl: binary (with a lying 'solid' header) and ASCII load");
  if(!v) {
    printf("# %s\n", err);
    return;
  }
  ok(v->npart == 2 && v->part[0].count == 12 && v->part[1].count == 6, "stl: 4 + 2 triangles");
  float zmax = 0;
  for(int i = v->part[0].first; i < v->part[0].first + v->part[0].count; i++) {
    zmax = fmaxf(zmax, v->vert[i * 6 + 2]);
  }
  ok(fabsf(zmax - 0.12f) < 1e-5f, "stl: scale and placement (10 mm x 2 + 100 mm, in metres)");
  float *nrm = v->vert + v->part[0].first * 6 + 3;
  ok(fabsf(sqrtf(nrm[0] * nrm[0] + nrm[1] * nrm[1] + nrm[2] * nrm[2]) - 1) < 1e-5f, "stl: normals computed");
  vis3d_free(v);
}

static void test_dialects(void) {
  // GL_VERSION strings as Mesa reports them, the Pi 4's (V3D) included
  struct {
    const char *ver, *want;
  } c[] = {{"OpenGL ES 3.1 Mesa 25.0.7", "300 es"}, {"OpenGL ES 3.2 Mesa 25.2.8", "300 es"},
           {"OpenGL ES 2.0 Mesa 22.3.6", "100"},     {"3.1 Mesa 25.0.7", "120"},
           {"2.1 Mesa 24.2.8", "120"},               {"4.5 (Core Profile) Mesa 25.2.8", "330"},
           {"3.3 (Compatibility Profile) Mesa", "330"}};
  int good = 1;
  for(size_t i = 0; i < sizeof(c) / sizeof(c[0]); i++) {
    const char *d = v3_glsl_for(c[i].ver);
    if(!d || strcmp(d, c[i].want)) {
      printf("# %s: %s, want %s\n", c[i].ver, d ? d : "NULL", c[i].want);
      good = 0;
    }
  }
  ok(good, "GLSL dialect from GL_VERSION (the Pi 4's ES 3.1 and desktop 2.1 / 3.1 included)");
}

static void test_errors(void) {
  char err[300];
  write("bad1.vis", "dh 1 0 0 0 0\npart 1 box 1 1 1 material nope\n");
  vis3d_t *v = load("bad1.vis", err);
  ok(!v && strstr(err, "bad1.vis:2") && strstr(err, "unknown material nope"), "errors: unknown material, with file:line");
  write("bad2.vis", "dh 1 0 0 0 0\npart 1 cone 1 1\n");
  v = load("bad2.vis", err);
  ok(!v && strstr(err, "unknown shape cone"), "errors: unknown shape");
  write("bad3.vis", "part 0 box 1 1 1\n");
  v = load("bad3.vis", err);
  ok(!v && strstr(err, "no joints"), "errors: no dh lines");
  write("bad4.vis", "dh 1 0 0 0 0\npart 1 mesh missing.stl\n");
  v = load("bad4.vis", err);
  ok(!v && strstr(err, "cannot open"), "errors: a missing mesh");
  write("bad5.vis", "include nothere.vis\n");
  v = load("bad5.vis", err);
  ok(!v && strstr(err, "nothere.vis"), "errors: a missing include");
  write("skip.vis", "drive 1 j1.conf sim.txt\nmotor 1 120 0.00035 0.02 8\ndh 1 0 0 0.4 0\nselfignore 0 2\n");
  v = load("skip.vis", err);
  ok(v != NULL, "arm_sim lines the view doesn't need are skipped");
  vis3d_free(v);
}

int main(void) {
  char cmd[64];
  snprintf(cmd, sizeof(cmd), "mkdir -p %s", TMP);
  if(system(cmd) != 0) {
    return 1;
  }
  test_arm6();
  test_joints_theta();
  test_units_and_shapes();
  test_stl();
  test_errors();
  test_dialects();
  printf("1..%d\n# %d/%d passed\n", n, n - failed, n);
  return failed != 0;
}
