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
 *
 * Script commands (plus all of ARMFILE's arm lines to change the arm):
 *   run T                         simulate T seconds
 *   lcnc EN [v1 .. vn]            enable, jog velocities [deg/s]
 *   move T q1 .. qn               smooth move to q [deg] in T seconds
 *   force J fx fy fz [rx ry rz]   force [N, base frame] at r [m, link J frame]
 *   force off
 *   mark                          remember the gear torques (for gres),
 *                                 clear rmax and the detection time
 *   detector FILE                 run the collision detector (collide.h) on
 *                                 what LinuxCNC sees, every 1 ms
 *   detset LINE                   change one detector model line
 *   print WHAT                    q qd qm fb ferr te tau_g tau_f jtf gres hits
 *                                 fault res thr rmax rmaxm taum coll energy
 *                                 time
 *                                 -> "arm.WHAT1 = v" ...
 *   dJ LINE                       console LINE on drive J; a bare pin name
 *                                 prints "dJ_pin = v" (e.g. d2_hv0.iq_fb)
 *   all LINE                      LINE on every drive
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "armcore.h"
#include "collide.h"

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
static double mv_t = 0, mv_T = 0, mv_from[NJ], mv_to[NJ];  // smooth move
static double mark_tau_g[NJ];           // gear torques at "mark"
// collision detector on what LinuxCNC sees (shared/robot/collide.c)
static rb_collide det;
static int det_on = 0;
static double det_rmax[NJ];             // max |residual| since "mark"
static double det_rmaxm[NJ];            // the same while moving (> 0.1 rad/s)
static double det_t_hit = -1.0;         // time of the first detection

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

static void servo_ms(void) {  // the LinuxCNC servo thread
  if(!synced) {
    for(int j = 0; j < nj; j++) {
      cmd_q[j] = armcore_qm(j);
    }
    synced = 1;
  }
  for(int j = 0; j < nj; j++) {
    double v = jog[j];
    if(mv_T > 0.0) {
      double s0 = smooth(mv_t / mv_T), s1 = smooth((mv_t + 0.001) / mv_T);
      double q1 = mv_from[j] + (mv_to[j] - mv_from[j]) * s1;
      v = (q1 - (mv_from[j] + (mv_to[j] - mv_from[j]) * s0)) / 0.001;
      cmd_q[j] = q1;
    } else {
      cmd_q[j] += v * 0.001;
    }
    armcore_set_cmd(j, en, cmd_q[j], v);
  }
  if(mv_T > 0.0) {
    mv_t += 0.001;
    if(mv_t >= mv_T) {
      mv_T = 0.0;
    }
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
    }
    if(!isnan(v)) {
      printf("arm.%s%d = %.6f\n", what, j + 1, v);
    }
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
  } else if(!strcmp(kw, "lcnc")) {
    char *end;
    en = (int)strtol(p, &end, 10);
    for(int j = 0; j < NJ; j++) {
      p = end;
      double x = strtod(p, &end);
      jog[j] = end == p ? 0.0 : x;
    }
    if(!en) {  // LinuxCNC follows the feedback while disabled
      for(int j = 0; j < nj; j++) {
        cmd_q[j] = armcore_fb(j);
      }
      mv_T = 0.0;
    }
  } else if(!strcmp(kw, "move")) {
    char *end;
    mv_T = strtod(p, &end);
    p = end;
    for(int j = 0; j < NJ; j++) {
      mv_from[j] = cmd_q[j];
      double x = strtod(p, &end);
      mv_to[j] = end == p ? cmd_q[j] : x;
      p = end;
      jog[j] = 0.0;
    }
    mv_t = 0.0;
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
  if(armcore_load(argv[1], lib)) {
    return 1;
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
