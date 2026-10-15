/*
 * The simulated arm with its drives, see armcore.h.
 */
#include "armcore.h"
#include "kin.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define D2R (M_PI / 180.0)

arm_drive arm_drv[RB_MAXJ];
int arm_nj = 0;
arm_plant arm_P;
gs_gripper arm_gs;
arm_drive arm_gdrv;
int arm_has_grip = 0;
static double grip_off;  // gripper gap [mm] = pos_fb + grip_off
long arm_ticks = 0;

static char libpath[1024];
static char armdir[1024] = ".";
// set the feedback offsets at the NEED_OFF-th exchange from now: the drive
// reports no position in the first one (0)
#define NEED_OFF 5
static int need_off = NEED_OFF;

static void rel_path(char *out, size_t n, const char *p) {
  if(p[0] == '/') {
    snprintf(out, n, "%s", p);
  } else {
    snprintf(out, n, "%s/%s", armdir, p);
  }
}

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

static void strip_comments(char *s) {  // the drive console has no comments
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
}

static int drive_open_d(arm_drive *d, int j, const char *conf, const char *simfile) {
  d->h = dlmopen(LM_ID_NEWLM, libpath, RTLD_NOW | RTLD_LOCAL);
  if(!d->h) {
    fprintf(stderr, "armcore: dlmopen %s: %s\n", libpath, dlerror());
    return -1;
  }
  d->boot = dlsym(d->h, "stmbl_sim_boot");
  d->tick = dlsym(d->h, "stmbl_sim_tick");
  d->update = dlsym(d->h, "stmbl_sim_update");
  d->pin = dlsym(d->h, "stmbl_sim_pin");
  d->cmd = dlsym(d->h, "stmbl_sim_cmd");
  if(!d->boot || !d->tick || !d->update || !d->pin || !d->cmd) {
    fprintf(stderr, "armcore: %s lacks the stmbl_sim_* API\n", libpath);
    return -1;
  }
  char *c = read_file(conf);
  if(!c) {
    fprintf(stderr, "armcore: cannot read %s\n", conf);
    return -1;
  }
  char extra[4096] = "hv0.sim_ext = 1\n";
  if(simfile) {
    char *s = read_file(simfile);
    if(!s) {
      fprintf(stderr, "armcore: cannot read %s\n", simfile);
      free(c);
      return -1;
    }
    strip_comments(s);
    strncat(extra, s, sizeof(extra) - strlen(extra) - 1);
    free(s);
  }
  char *text = malloc(strlen(c) + 32);
  sprintf(text, "debug_level 0\n%s", c);
  free(c);
  // the boot echoes every config line: keep it out of our stdout
  fflush(stdout);
  int saved = dup(1), null = open("/dev/null", O_WRONLY);
  dup2(null, 1);
  int rc = d->boot(text, extra);
  d->cmd("debug_level 0");  // also flushes this copy's stdout
  dup2(saved, 1);
  close(saved);
  close(null);
  free(text);
  if(rc) {
    fprintf(stderr, "armcore: drive %d did not boot (%s)\n", j + 1, conf);
    return -1;
  }
  d->sim_pos = d->pin("hv0.sim_pos");
  d->sim_vel = d->pin("hv0.sim_vel");
  d->te_int = d->pin("hv0.sim_te_int");
  d->brake_rel = d->pin("hv0.sim_brake_release");
  if(!d->sim_pos || !d->sim_vel || !d->te_int || !d->brake_rel) {
    fprintf(stderr, "armcore: drive %d: hv0 sim pins missing\n", j + 1);
    return -1;
  }
  if(j >= 0 && j + 1 > arm_nj) {
    arm_nj = j + 1;
  }
  return 0;
}

static int drive_open(int j, const char *conf, const char *simfile) {
  return drive_open_d(&arm_drv[j], j, conf, simfile);
}

int armcore_payload(double m, const double c[3]) {
  static double pm = 0.0, pc[3];
  double ic[6] = {0};
  int last = arm_P.arm.n - 1;
  if(last < 0) {
    return -1;
  }
  if(pm != 0.0) {
    rb_add_payload(&arm_P.arm, last, -pm, pc, ic);
  }
  pm = m;
  memcpy(pc, c, sizeof(pc));
  if(pm != 0.0) {
    rb_add_payload(&arm_P.arm, last, pm, pc, ic);
  }
  return 0;
}

double armcore_qm(int j) {
  double v = 0.0;
  for(int k = 0; k < arm_nj; k++) {
    v += arm_P.K[j][k] * arm_P.th[k];
  }
  return v / D2R;
}

void armcore_home(const double *q_deg) {
  double q[RB_MAXJ];
  for(int i = 0; i < RB_MAXJ; i++) {
    q[i] = q_deg[i] * D2R;
  }
  plant_set_q(&arm_P, q);
  need_off = NEED_OFF;
}

int armcore_line(char *line) {
  char kw[16];
  double v[12];
  int n = 0, off = 0;
  char *h = strchr(line, '#');
  if(h) {
    *h = 0;
  }
  if(sscanf(line, " %15s%n", kw, &off) != 1) {
    return 1;
  }
  int g = gm_line(&arm_P.geo, line);  // body / obstacle / selfignore / contact
  if(g) {
    return g > 0;
  }
  g = gs_line(&arm_gs, line);  // gripper mech / jaw, part
  if(g) {
    return g > 0;
  }
  {
    char conf[256], sim[256], cp[1024], sp[1024];
    int k = sscanf(line, " gripper drive %255s %255s", conf, sim);
    if(k >= 1) {
      rel_path(cp, sizeof(cp), conf);
      if(k == 2) {
        rel_path(sp, sizeof(sp), sim);
      }
      if(drive_open_d(&arm_gdrv, -1, cp, k == 2 ? sp : NULL)) {
        return 0;
      }
      arm_has_grip = 1;
      arm_gs.on = 1;
      return 1;
    }
  }
  char *p = line + off, *end;
  if(!strcmp(kw, "drive")) {
    char conf[256], sim[256], cp[1024], sp[1024];
    int j, k = sscanf(p, " %d %255s %255s", &j, conf, sim);
    if(k < 2 || j < 1 || j > RB_MAXJ) {
      return 0;
    }
    rel_path(cp, sizeof(cp), conf);
    if(k == 3) {
      rel_path(sp, sizeof(sp), sim);
    }
    return drive_open(j - 1, cp, k == 3 ? sp : NULL) == 0;
  }
  int j = (int)strtol(p, &end, 10) - 1;
  for(p = end; n < 12; p = end) {
    double x = strtod(p, &end);
    if(end == p) {
      break;
    }
    v[n++] = x;
  }
  if(!strcmp(kw, "payload")) {  // payload M cx cy cz: point mass on the last link
    double v4[4] = {0};
    int k = sscanf(line + off, " %lf %lf %lf %lf", v4, v4 + 1, v4 + 2, v4 + 3);
    return k >= 1 && armcore_payload(v4[0], v4 + 1) == 0;
  }
  if(!strcmp(kw, "home")) {  // all joints: the first number was joint 1's angle
    double q[RB_MAXJ];
    q[0] = j + 1;
    for(int i = 1; i < RB_MAXJ; i++) {
      q[i] = i - 1 < n ? v[i - 1] : 0.0;
    }
    armcore_home(q);
    return 1;
  }
  if(j < 0 || j >= RB_MAXJ) {
    return 0;
  }
  if(j + 1 > arm_P.arm.n) {
    arm_P.arm.n = j + 1;
  }
  rb_link *l = &arm_P.arm.link[j];
  if(!strcmp(kw, "dh") && n == 4) {
    l->a = v[0], l->alpha = v[1] * D2R, l->d = v[2], l->theta0 = v[3] * D2R;
  } else if(!strcmp(kw, "link") && n == 10) {
    rb_link_from_com(l, v[0], v + 1, v + 4);
  } else if(!strcmp(kw, "motor") && n == 4) {
    arm_drv[j].ratio = v[0];
    arm_P.K[j][j] = 1.0 / v[0];
    arm_P.jm[j] = v[1], arm_P.fm[j] = v[2], arm_P.brake[j] = v[3];
  } else if(!strcmp(kw, "couple") && n == 2 && v[0] >= 1 && v[0] <= RB_MAXJ) {
    arm_P.K[j][(int)v[0] - 1] = v[1];
  } else if(!strcmp(kw, "gear") && n == 3) {
    arm_P.k[j] = v[0], arm_P.c[j] = v[1], arm_P.lash[j] = v[2] / 60.0 * D2R;
  } else if(!strcmp(kw, "fric") && n == 4) {
    arm_P.fric[j] = (rb_fric){.fc = v[0], .fs = v[1], .vs = v[2], .fv = v[3]};
  } else if(!strcmp(kw, "limits") && n == 2) {
    arm_P.qmin[j] = v[0] * D2R, arm_P.qmax[j] = v[1] * D2R;
  } else {
    return 0;
  }
  return 1;
}

int armcore_include(const char *file) {  // relative to the arm file's directory
  static int depth = 0;
  char path[1100];
  if(file[0] == '/' || depth == 0) {
    snprintf(path, sizeof(path), "%s", file);
  } else {
    rel_path(path, sizeof(path), file);
  }
  char *text = read_file(path);
  if(!text) {
    fprintf(stderr, "armcore: cannot read %s\n", path);
    return -1;
  }
  if(++depth > 4) {
    fprintf(stderr, "armcore: includes nested too deep (%s)\n", path);
    free(text);
    depth--;
    return -1;
  }
  int rc = 0;
  char *save = NULL;
  for(char *line = strtok_r(text, "\n", &save); line && !rc; line = strtok_r(NULL, "\n", &save)) {
    char inc[512];
    if(sscanf(line, " include %511s", inc) == 1) {
      rc = armcore_include(inc);
    } else if(!armcore_line(line)) {
      fprintf(stderr, "armcore: %s: bad line: %s\n", path, line);
      rc = -1;
    }
  }
  depth--;
  free(text);
  return rc;
}

int armcore_load(const char *armfile, const char *lib) {
  snprintf(libpath, sizeof(libpath), "%s", lib);
  snprintf(armdir, sizeof(armdir), "%s", armfile);
  char *sl = strrchr(armdir, '/');
  if(sl) {
    *sl = 0;
  } else {
    strcpy(armdir, ".");
  }
  plant_defaults(&arm_P, 0);
  gs_init(&arm_gs);
  for(int j = 0; j < RB_MAXJ; j++) {
    arm_drv[j].ratio = 1.0;
  }
  int rc = armcore_include(armfile);
  if(!rc && arm_nj != arm_P.arm.n) {
    fprintf(stderr, "armcore: %d drives for %d joints\n", arm_nj, arm_P.arm.n);
    rc = -1;
  }
  for(int j = 0; j < arm_nj; j++) {
    arm_drv[j].pd.scale = 360.0 / arm_drv[j].ratio;
  }
  if(arm_has_grip) {  // the fingers start open, 5 mm from the end stop
    gs_set_gap(&arm_gs, arm_gs.gap_max - 5.0);
  }
  return rc;
}

void armcore_set_cmd(int j, int en, double pos, double vel) {
  arm_drive *d = &arm_drv[j];
  d->pd.enable = en;
  d->pd.pos_cmd = pos - d->off;
  d->pd.vel_cmd = vel;
  d->pd.scale = 360.0 / d->ratio;
}

void armcore_grip_cmd(int en, double gap, double vel) {
  arm_gdrv.pd.enable = en;
  arm_gdrv.pd.pos_cmd = gap - grip_off;
  arm_gdrv.pd.vel_cmd = vel;
  arm_gdrv.pd.scale = 2.0 * arm_gs.lead;  // gap mm per motor turn
}

double armcore_grip_fb(void) {
  return arm_gdrv.pd.pos_fb + grip_off;
}

void armcore_grip_drop(void) {
  if(gs_held(&arm_gs) >= 0) {
    gs_drop(&arm_gs);
    armcore_payload(0.0, (double[3]){0, 0, 0});
  }
}

int armcore_synced(void) {
  return !need_off;
}

double armcore_fb(int j) {
  return arm_drv[j].pd.pos_fb + arm_drv[j].off;
}

double armcore_fb_vel(int j) {
  return arm_drv[j].pd.vel_fb;
}

static double wrap(double a) {
  a = fmod(a + M_PI, 2.0 * M_PI);
  return a < 0.0 ? a + M_PI : a - M_PI;
}

void armcore_tick(int servo) {
  double te[RB_MAXJ];
  int rel[RB_MAXJ];
  for(int j = 0; j < arm_nj; j++) {
    arm_drive *d = &arm_drv[j];
    *d->sim_pos = (float)wrap(arm_P.th[j]);
    *d->sim_vel = (float)arm_P.w[j];
    *d->te_int = 0.0f;
    if(servo && !d->link_lost) {
      d->update(&d->pd, (long)(ARM_RT * 1e9 + 0.5));
    } else {
      d->tick();
    }
    te[j] = *d->te_int / ARM_RT;
    rel[j] = *d->brake_rel > 0.0f;
  }
  double te_g = 0.0;
  if(arm_has_grip) {  // the gripper drive
    arm_drive *d = &arm_gdrv;
    *d->sim_pos = (float)wrap(arm_gs.th);
    *d->sim_vel = (float)arm_gs.w;
    *d->te_int = 0.0f;
    if(servo && !d->link_lost) {
      d->update(&d->pd, (long)(ARM_RT * 1e9 + 0.5));
    } else {
      d->tick();
    }
    te_g = *d->te_int / ARM_RT;
  }
  if(servo && need_off && --need_off == 0) {  // absolute encoders, calibrated zero
    for(int j = 0; j < arm_nj; j++) {
      arm_drv[j].off = armcore_qm(j) - arm_drv[j].pd.pos_fb;
    }
    grip_off = gs_gap(&arm_gs) - arm_gdrv.pd.pos_fb;
  }
  for(int s = 0; s < ARM_SUB; s++) {
    plant_step(&arm_P, te, rel, ARM_RT / ARM_SUB);
  }
  if(arm_has_grip) {  // the fingers, at the flange's pose
    double R[RB_MAXJ][3][3], p[RB_MAXJ][3];
    rb_fk(&arm_P.arm, arm_P.q, R, p);
    int last = arm_P.arm.n - 1;
    for(int s = 0; s < ARM_SUB; s++) {
      int ev = gs_step(&arm_gs, te_g, R[last], p[last], ARM_RT / ARM_SUB);
      if(ev > 0) {  // a part picked up: its mass hangs on the flange
        gs_part *pt = &arm_gs.parts[gs_held(&arm_gs)];
        armcore_payload(pt->mass, pt->c6);
      } else if(ev < 0) {
        armcore_payload(0.0, (double[3]){0, 0, 0});
      }
    }
  }
  arm_ticks++;
}

void armcore_servo(long period_ns) {
  static long carry = 0;
  long rt = (long)(ARM_RT * 1e9 + 0.5);
  carry += period_ns;
  int first = 1;
  while(carry >= rt) {
    carry -= rt;
    armcore_tick(first);
    first = 0;
  }
}
