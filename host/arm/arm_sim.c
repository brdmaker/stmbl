/*
 * arm_sim: six (up to RB_MAXJ) simulated stmbl drives on a simulated arm.
 *
 * Each drive is a separate copy of libstmbl_drive.so (dlmopen, own
 * namespace, own globals) booted from its own config, with hv0.sim_ext = 1:
 * the electrical side runs in the drive, the mechanics here (plant.c).
 * Every 200 us rt cycle: rotor angle/speed into each drive, one drive tick,
 * its mean electrical torque back, then the plant for 200 us. Every 1 ms
 * the LinuxCNC side sends each drive its joint command over the simulated
 * smart-serial link, in joint degrees (sserial scale = 360 / ratio).
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
 *   mark                          remember the gear torques (for gres)
 *   print WHAT                    q qd qm fb ferr te tau_g tau_f jtf gres hits
 *                                 fault energy time
 *                                 -> "arm.WHAT1 = v" ...
 *   dJ LINE                       console LINE on drive J; a bare pin name
 *                                 prints "dJ_pin = v" (e.g. d2_hv0.iq_fb)
 *   all LINE                      LINE on every drive
 */
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plant.h"
#include "stmbl_sim_api.h"

#define NJ RB_MAXJ
#define D2R (M_PI / 180.0)
#define RT 0.0002
#define SUB 4  // plant steps per rt cycle

typedef struct {
  void *h;
  int (*boot)(const char *, const char *);
  void (*tick)(void);
  void (*update)(stmbl_sim_pd_t *, long);
  float *(*pin)(const char *);
  void (*cmd)(const char *);
  float *sim_pos, *sim_vel, *te_int, *brake_rel, *pos_error;
  stmbl_sim_pd_t pd;
  double ratio;
} drive_t;

static drive_t drv[NJ];
static int nj = 0;
static arm_plant P;
static char libpath[1024] = "./libstmbl_drive.so";  // next to arm_sim
static char armdir[1024] = ".";                         // ARMFILE's directory

static void rel_path(char *out, size_t n, const char *p) {  // relative to ARMFILE
  if(p[0] == '/') {
    snprintf(out, n, "%s", p);
  } else {
    snprintf(out, n, "%s/%s", armdir, p);
  }
}

// LinuxCNC side
static int en = 0;
static double jog[NJ];                  // deg/s
static double cmd_q[NJ];                // deg
// LinuxCNC joint = drive feedback + off (absolute encoders with a
// calibrated zero: set when LinuxCNC first sees the drive)
static double joff[NJ];
static int homed = 0;
static double mv_t = 0, mv_T = 0, mv_from[NJ], mv_to[NJ];  // smooth move
static long ticks = 0;
static double mark_tau_g[NJ];  // gear torques at "mark"

static char *read_file(const char *path) {
  FILE *f = fopen(path, "rb");
  if(!f) {
    return NULL;
  }
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *s = malloc(n + 1);
  if(fread(s, 1, n, f) != (size_t)n) {
    n = 0;
  }
  s[n] = 0;
  fclose(f);
  return s;
}

static char *strip_comments(char *s) {  // the drive console has no comments
  char *o = s;
  for(char *line = s; *line;) {
    char *nl = strchr(line, '\n');
    size_t len = nl ? (size_t)(nl - line) : strlen(line);
    char *h = memchr(line, '#', len);
    size_t keep = h ? (size_t)(h - line) : len;
    memmove(o, line, keep);
    o += keep;
    *o++ = '\n';
    line += len + (nl ? 1 : 0);
  }
  *o = 0;
  return s;
}

static void *sym(void *h, const char *name) {
  void *p = dlsym(h, name);
  if(!p) {
    fprintf(stderr, "arm_sim: %s missing in %s\n", name, libpath);
    exit(1);
  }
  return p;
}

static void drive_open(int j, const char *conf, const char *simfile) {
  drive_t *d = &drv[j];
  d->h = dlmopen(LM_ID_NEWLM, libpath, RTLD_NOW | RTLD_LOCAL);
  if(!d->h) {
    fprintf(stderr, "arm_sim: dlmopen %s: %s\n", libpath, dlerror());
    exit(1);
  }
  d->boot = sym(d->h, "stmbl_sim_boot");
  d->tick = sym(d->h, "stmbl_sim_tick");
  d->update = sym(d->h, "stmbl_sim_update");
  d->pin = sym(d->h, "stmbl_sim_pin");
  d->cmd = sym(d->h, "stmbl_sim_cmd");
  char *c = read_file(conf);
  if(!c) {
    fprintf(stderr, "arm_sim: cannot read %s\n", conf);
    exit(1);
  }
  char extra[4096] = "hv0.sim_ext = 1\n";
  if(simfile) {
    char *s = read_file(simfile);
    if(!s) {
      fprintf(stderr, "arm_sim: cannot read %s\n", simfile);
      exit(1);
    }
    strncat(extra, strip_comments(s), sizeof(extra) - strlen(extra) - 1);
    free(s);
  }
  char *conf_text = malloc(strlen(c) + 32);
  sprintf(conf_text, "debug_level 0\n%s", c);  // no echo of every config line
  free(c);
  c = conf_text;
  // the boot echoes every config line: not into the script's output
  fflush(stdout);
  int saved = dup(1), null = open("/dev/null", O_WRONLY);
  dup2(null, 1);
  int rc = d->boot(c, extra);
  d->cmd = sym(d->h, "stmbl_sim_cmd");
  d->cmd("debug_level 0");  // also flushes this copy's stdout
  dup2(saved, 1);
  close(saved);
  close(null);
  if(rc) {
    fprintf(stderr, "arm_sim: drive %d did not boot\n", j + 1);
    exit(1);
  }
  free(c);
  d->sim_pos = d->pin("hv0.sim_pos");
  d->sim_vel = d->pin("hv0.sim_vel");
  d->te_int = d->pin("hv0.sim_te_int");
  d->brake_rel = d->pin("hv0.sim_brake_release");
  d->pos_error = d->pin("pid0.pos_error");
  if(!d->sim_pos || !d->sim_vel || !d->te_int || !d->brake_rel) {
    fprintf(stderr, "arm_sim: drive %d: hv0 sim pins missing\n", j + 1);
    exit(1);
  }
  if(j + 1 > nj) {
    nj = j + 1;
  }
}

static double wrap(double a) {
  a = fmod(a + M_PI, 2.0 * M_PI);
  return a < 0.0 ? a + M_PI : a - M_PI;
}

static double smooth(double s) {  // 0..1, zero speed and acceleration at the ends
  return s <= 0.0 ? 0.0 : s >= 1.0 ? 1.0 : s * s * s * (10.0 + s * (-15.0 + 6.0 * s));
}

static double qm_deg(int j) {  // the joint angle the motor encoder sees
  double v = 0.0;
  for(int k = 0; k < nj; k++) {
    v += P.K[j][k] * P.th[k];
  }
  return v / D2R;
}

static void servo_ms(void) {  // the LinuxCNC servo thread
  if(!homed) {
    for(int j = 0; j < nj; j++) {
      joff[j] = qm_deg(j) - drv[j].pd.pos_fb;
      cmd_q[j] = qm_deg(j);
    }
    homed = 1;
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
    drv[j].pd.enable = en;
    drv[j].pd.pos_cmd = cmd_q[j] - joff[j];
    drv[j].pd.vel_cmd = v;
    drv[j].pd.scale = 360.0 / drv[j].ratio;
  }
  if(mv_T > 0.0) {
    mv_t += 0.001;
    if(mv_t >= mv_T) {
      mv_T = 0.0;
    }
  }
}

static void run_tick(void) {
  double te[NJ];
  int rel[NJ];
  int servo = ticks % 5 == 0;
  if(servo) {
    servo_ms();
  }
  for(int j = 0; j < nj; j++) {
    drive_t *d = &drv[j];
    *d->sim_pos = (float)wrap(P.th[j]);
    *d->sim_vel = (float)P.w[j];
    *d->te_int = 0.0f;
    if(servo) {
      d->update(&d->pd, (long)(RT * 1e9 + 0.5));
    } else {
      d->tick();
    }
    te[j] = *d->te_int / RT;
    rel[j] = *d->brake_rel > 0.0f;
  }
  for(int s = 0; s < SUB; s++) {
    plant_step(&P, te, rel, RT / SUB);
  }
  ticks++;
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
    }
    if(!strcmp(what, "q")) {
      v = P.q[j] / D2R;
    } else if(!strcmp(what, "qd")) {
      v = P.qd[j] / D2R;
    } else if(!strcmp(what, "qm")) {  // motor angles as joint angles
      v = qm_deg(j);
    } else if(!strcmp(what, "fb")) {  // LinuxCNC's joint feedback
      v = drv[j].pd.pos_fb + joff[j];
    } else if(!strcmp(what, "ferr")) {  // LinuxCNC command - link position
      v = cmd_q[j] - P.q[j] / D2R;
    } else if(!strcmp(what, "te")) {
      v = *drv[j].te_int / RT;
    } else if(!strcmp(what, "tau_g")) {
      v = P.tau_g[j];
    } else if(!strcmp(what, "tau_f")) {
      v = P.tau_f[j];
    } else if(!strcmp(what, "hits")) {
      v = P.stop_hits[j];
    } else if(!strcmp(what, "fault")) {
      v = drv[j].pd.fault;
    }
    if(!isnan(v)) {
      printf("arm.%s%d = %.6f\n", what, j + 1, v);
    }
  }
  if(!strcmp(what, "energy")) {
    printf("arm.energy = %.6f\n", plant_energy(&P));
  } else if(!strcmp(what, "time")) {
    printf("arm.time = %.6f\n", ticks * RT);
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

// arm description lines (ARMFILE, also usable from scripts)
static int arm_line(char *line) {
  char kw[16];
  double v[12];
  int n = 0, off = 0;
  if(sscanf(line, " %15s%n", kw, &off) != 1) {
    return 1;
  }
  char *p = line + off, *end;
  if(!strcmp(kw, "drive")) {
    char conf[256], sim[256];
    int j, k = sscanf(p, " %d %255s %255s", &j, conf, sim);
    if(k < 2) {
      return 0;
    }
    char cp[1024], sp[1024];
    rel_path(cp, sizeof(cp), conf);
    if(k == 3) {
      rel_path(sp, sizeof(sp), sim);
    }
    drive_open(j - 1, cp, k == 3 ? sp : NULL);
    return 1;
  }
  int j = (int)strtol(p, &end, 10) - 1;
  for(p = end; n < 12; p = end) {
    double x = strtod(p, &end);
    if(end == p) {
      break;
    }
    v[n++] = x;
  }
  if(!strcmp(kw, "home")) {  // all joints: the first number was joint 1
    double q[NJ];
    q[0] = j + 1;
    for(int i = 1; i < NJ; i++) {
      q[i] = i - 1 < n ? v[i - 1] : 0.0;
    }
    for(int i = 0; i < NJ; i++) {
      q[i] *= D2R;
      cmd_q[i] = q[i] / D2R;
    }
    plant_set_q(&P, q);
    homed = 0;
    return 1;
  }
  if(j < 0 || j >= NJ) {
    return 0;
  }
  if(j + 1 > P.arm.n) {
    P.arm.n = j + 1;
  }
  rb_link *l = &P.arm.link[j];
  if(!strcmp(kw, "dh") && n == 4) {
    l->a = v[0], l->alpha = v[1] * D2R, l->d = v[2], l->theta0 = v[3] * D2R;
  } else if(!strcmp(kw, "link") && n == 10) {
    rb_link_from_com(l, v[0], v + 1, v + 4);
  } else if(!strcmp(kw, "motor") && n == 4) {
    drv[j].ratio = v[0];
    P.K[j][j] = 1.0 / v[0];
    P.jm[j] = v[1], P.fm[j] = v[2], P.brake[j] = v[3];
  } else if(!strcmp(kw, "couple") && n == 2) {
    P.K[j][(int)v[0] - 1] = v[1];
  } else if(!strcmp(kw, "gear") && n == 3) {
    P.k[j] = v[0], P.c[j] = v[1], P.lash[j] = v[2] / 60.0 * D2R;
  } else if(!strcmp(kw, "fric") && n == 4) {
    P.fric[j] = (rb_fric){.fc = v[0], .fs = v[1], .vs = v[2], .fv = v[3]};
  } else if(!strcmp(kw, "limits") && n == 2) {
    P.qmin[j] = v[0] * D2R, P.qmax[j] = v[1] * D2R;
  } else {
    return 0;
  }
  return 1;
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
    long n = (long)(t / RT + 0.5);
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
        cmd_q[j] = drv[j].pd.pos_fb + joff[j];
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
  } else if(!arm_line(line)) {
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
  const char *lp = getenv("STMBL_DRIVE_LIB");
  char exe[1024];
  ssize_t el = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
  if(lp) {
    snprintf(libpath, sizeof(libpath), "%s", lp);
  } else if(el > 0) {
    exe[el] = 0;
    *strrchr(exe, '/') = 0;
    snprintf(libpath, sizeof(libpath), "%s/libstmbl_drive.so", exe);
  }
  snprintf(armdir, sizeof(armdir), "%s", argv[1]);
  char *sl = strrchr(armdir, '/');
  if(sl) {
    *sl = 0;
  } else {
    strcpy(armdir, ".");
  }
  plant_defaults(&P, 0);
  for(int j = 0; j < NJ; j++) {
    drv[j].ratio = 1.0;
  }
  char *arm = read_file(argv[1]);
  if(!arm) {
    fprintf(stderr, "arm_sim: cannot read %s\n", argv[1]);
    return 1;
  }
  for(char *line = strtok(arm, "\n"); line; line = strtok(NULL, "\n")) {
    char *h = strchr(line, '#');
    if(h) {
      *h = 0;
    }
    if(!arm_line(line)) {
      fprintf(stderr, "arm_sim: %s: bad line: %s\n", argv[1], line);
      return 1;
    }
  }
  free(arm);
  if(nj != P.arm.n) {
    fprintf(stderr, "arm_sim: %d drives for %d joints\n", nj, P.arm.n);
    return 1;
  }
  for(int j = 0; j < nj; j++) {  // LinuxCNC starts where the drives are
    drv[j].pd.scale = 360.0 / drv[j].ratio;
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
