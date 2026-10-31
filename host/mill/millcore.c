/*
 * The simulated mill and its drives, see millcore.h.
 */
#include "millcore.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define RT MC_RT
#define SUB 10


mc_drive mc_drv[MP_NAX];
mc_trt_t mc_trt;
mp_plant mc_P;
double mc_servo_t = 0.001;
long mc_ticks = 0;
static char libpath[1024], dir[1024] = ".";
#define NEED_OFF 5
static int need_off = NEED_OFF;
static const char *axname = "xyzac";
#define NAX (mc_P.nax)
// LinuxCNC's units per plant unit: mm per m, degrees per rad
static double unit(int k) {
  return k >= MA ? 180.0 / M_PI : 1000.0;
}

// --- the drives --------------------------------------------------------------

static void rel_path(char *out, size_t n, const char *p) {
  snprintf(out, n, p[0] == '/' ? "%s%s" : "%s/%s", p[0] == '/' ? "" : dir, p);
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

static int drive_open(mc_drive *d, const char *conf, const char *simfile) {
  d->h = dlmopen(LM_ID_NEWLM, libpath, RTLD_NOW | RTLD_LOCAL);
  if(!d->h) {
    fprintf(stderr, "mill_sim: dlmopen %s: %s\n", libpath, dlerror());
    return -1;
  }
  d->boot = dlsym(d->h, "stmbl_sim_boot");
  d->tick = dlsym(d->h, "stmbl_sim_tick");
  d->update = dlsym(d->h, "stmbl_sim_update");
  d->pin = dlsym(d->h, "stmbl_sim_pin");
  d->cmd = dlsym(d->h, "stmbl_sim_cmd");
  if(!d->boot || !d->tick || !d->update || !d->pin || !d->cmd) {
    fprintf(stderr, "mill_sim: %s lacks the stmbl_sim_* API\n", libpath);
    return -1;
  }
  char *c = NULL, *s = simfile ? read_file(simfile) : NULL;  // conf: a.conf,b.conf,... joined
  size_t cl = 0;
  char list[2048];
  snprintf(list, sizeof(list), "%s", conf);
  for(char *save = NULL, *f = strtok_r(list, ",", &save); f; f = strtok_r(NULL, ",", &save)) {
    char path[1024];
    rel_path(path, sizeof(path), f);
    char *part = read_file(path);
    if(!part) {
      fprintf(stderr, "mill_sim: cannot read %s\n", path);
      free(c);
      return -1;
    }
    c = realloc(c, cl + strlen(part) + 2);
    strcpy(c + cl, part);
    cl += strlen(part);
    c[cl++] = '\n', c[cl] = 0;
    free(part);
  }
  if(!c || (simfile && !s)) {
    fprintf(stderr, "mill_sim: cannot read %s\n", !c ? conf : simfile);
    return -1;
  }
  strip_comments(c);
  char extra[4096] = "hv0.sim_ext = 1\n";
  if(s) {
    strip_comments(s);
    strncat(extra, s, sizeof(extra) - strlen(extra) - 1);
    free(s);
  }
  char *text = malloc(strlen(c) + 32);
  sprintf(text, "debug_level 0\n%s", c);
  free(c);
  fflush(stdout);  // the boot echoes every config line: keep it off stdout
  int saved = dup(1), null = open(getenv("MILL_BOOTLOG") ? getenv("MILL_BOOTLOG") : "/dev/null", O_WRONLY | O_CREAT | O_APPEND, 0644);
  dup2(null, 1);
  int rc = d->boot(text, extra);
  d->cmd("debug_level 0");
  dup2(saved, 1);
  close(saved);
  close(null);
  free(text);
  if(rc) {
    fprintf(stderr, "mill_sim: drive %s did not boot\n", conf);
    return -1;
  }
  d->sim_pos = d->pin("hv0.sim_pos");
  d->sim_vel = d->pin("hv0.sim_vel");
  d->te_int = d->pin("hv0.sim_te_int");
  if(!d->sim_pos || !d->sim_vel || !d->te_int) {
    fprintf(stderr, "mill_sim: %s: hv0 sim pins missing\n", conf);
    return -1;
  }
  d->brake = d->pin("fault0.mot_brake");  // > 0: released (io0.out0 on the drive)
  d->on = 1;
  return 0;
}

static double wrap(double a) {
  a = fmod(a + M_PI, 2.0 * M_PI);
  return a < 0.0 ? a + M_PI : a - M_PI;
}

static double gauss(void) {  // Box-Muller, fixed seed: runs repeat
  static unsigned long long x = 88172645463325252ULL;
  double u[2];
  for(int i = 0; i < 2; i++) {
    x ^= x << 13, x ^= x >> 7, x ^= x << 17;
    u[i] = ((x >> 11) + 0.5) / 9007199254740992.0;
  }
  return sqrt(-2.0 * log(u[0])) * cos(2.0 * M_PI * u[1]);
}

// what LinuxCNC's scale (or the motor's feedback) reads: mm
double mccore_scale(int k) {
  return mc_P.s[k] * unit(k);
}

static void tick(int servo) {
  double T[MP_NAX] = {0};
  for(int k = 0; k < NAX; k++) {
    mc_drive *d = &mc_drv[k];
    double g = 2.0 * M_PI / mc_P.p[k];
    *d->sim_pos = (float)wrap(mc_P.phi[k] + (d->rnoise > 0 ? d->rnoise * gauss() : 0.0));
    *d->sim_vel = (float)mc_P.w[k];
    *d->te_int = 0.0f;
    if(d->scale_pos) {
      // as the motor's: hv0 moves the rotor over the rt cycle before enc_fb0
      // reads it, so the drive sees the end of the cycle; the scale too
      double s = mc_P.s[k] + mc_P.v[k] * RT + (d->scale_noise > 0 ? d->scale_noise * gauss() : 0.0);
      s = floor(s / d->scale_res + 0.5) * d->scale_res;  // the count; the drive's res quantizes it the same
      *d->scale_pos = (float)wrap(s * g);
      *d->scale_abs = (float)(s * g);
    }
    if(servo) {
      d->update(&d->pd, (long)(RT * 1e9 + 0.5));
    } else {
      d->tick();
    }
    T[k] = *d->te_int / RT;
    mc_P.brake[k] = d->has_brake && !(d->brake && *d->brake > 0.0f);
  }
  if(servo && need_off && --need_off == 0) {  // absolute positions: LinuxCNC's offset
    for(int k = 0; k < NAX; k++) {
      mc_drv[k].off = mccore_scale(k) - mc_drv[k].pd.pos_fb;
    }
  }
  for(int s = 0; s < SUB; s++) {
    mp_step(&mc_P, T, RT / SUB);
  }
  mc_ticks++;
}

// One servo period: LinuxCNC sends the packet at t_send; it reaches the
// drive `latency` later and is taken at the next rt cycle (on the real
// drive the frt loop reads the UART, rt uses it). Then the rt cycles up to
// the next send. 1 kHz: always 5 cycles after; 2 kHz: alternately 2 and 3.
static double t_send = 0.0, latency = 50e-6;
void mccore_servo(double period) {
  static double last = 0.0;
  if(period != last) {  // the drives' command interpolation (vel_int mode 1) needs the packet rate
    last = period;
    for(int k = 0; k < NAX; k++) {
      float *f = mc_drv[k].on ? mc_drv[k].pin("vel_int0.cmd_freq") : NULL;
      if(f) {
        *f = (float)(1.0 / period);
      }
    }
  }
  double t_arr = t_send + latency;
  int pending  = 1;
  t_send += period;
  while(mc_ticks * RT < t_send - 1e-9) {
    int now = pending && mc_ticks * RT >= t_arr - 1e-9;
    pending = pending && !now;
    tick(now);
  }
}

void mccore_set_cmd(int k, int en, double pos, double vel) {
  mc_drive *d = &mc_drv[k];
  d->pd.enable = en;
  if(need_off) {
    d->pd.pos_cmd = d->pd.pos_fb, d->pd.vel_cmd = 0.0;
    return;
  }
  d->pd.pos_cmd = pos - d->off;
  d->pd.vel_cmd = vel;
}

double mccore_fb_vel(int k) {
  return mc_drv[k].pd.vel_fb;
}

int mccore_synced(void) {
  return !need_off;
}

void mccore_angles(double t, double out[2]) {
  double dt = mc_ticks * RT - t;
  for(int i = 0; i < 2; i++) {
    out[i] = NAX > MA + i ? (mc_P.s[MA + i] - mc_P.v[MA + i] * dt) * 180.0 / M_PI : 0.0;
  }
}

void mccore_trt_fwd(const double *j, double out[3]) {
  const mc_trt_t *k = &mc_trt;
  double a = j[3] * M_PI / 180.0, c = j[4] * M_PI / 180.0, dy = k->dy, dz = k->dz + k->tool;
  double X = j[0] - k->xr, Y = j[1] - dy - k->yr, Z = j[2] - dz - k->zr;
  out[0] = cos(c) * X + sin(c) * cos(a) * Y + sin(c) * sin(a) * Z + sin(c) * dy + k->xr;
  out[1] = -sin(c) * X + cos(c) * cos(a) * Y + cos(c) * sin(a) * Z + cos(c) * dy + k->yr;
  out[2] = -sin(a) * Y + cos(a) * Z + dz + k->zr;
}

void mccore_trt_inv(const double *w, double a_deg, double c_deg, double *j) {
  const mc_trt_t *k = &mc_trt;
  double a = a_deg * M_PI / 180.0, c = c_deg * M_PI / 180.0, dy = k->dy, dz = k->dz + k->tool;
  double X = w[0] - k->xr, Y = w[1] - k->yr, Z = w[2] - k->zr;
  j[0] = cos(c) * X - sin(c) * Y + k->xr;
  j[1] = sin(c) * cos(a) * X + cos(c) * cos(a) * Y - sin(a) * Z - cos(a) * dy + sin(a) * dz + dy + k->yr;
  j[2] = sin(c) * sin(a) * X + cos(c) * sin(a) * Y + cos(a) * Z - sin(a) * dy - cos(a) * dz + dz + k->zr;
  j[3] = a_deg, j[4] = c_deg;
}

void mccore_work(double t, double out[3]) {
  double j[5], ang[2];
  mccore_tool(t, j);
  mccore_angles(t, ang);
  j[3] = ang[0], j[4] = ang[1];
  mccore_trt_fwd(j, out);
}

void mccore_tool(double t, double out[3]) {
  mp_tool(&mc_P, out);
  double dt = mc_ticks * RT - t, v[3] = {mc_P.v[MX] + mc_P.vhx, mc_P.v[MY] + mc_P.vyb + mc_P.vhy, mc_P.v[MZ]};
  for(int k = 0; k < 3; k++) {
    out[k] = (out[k] - v[k] * dt) * 1000.0;
  }
}

double mccore_fb(int k) {
  return mc_drv[k].pd.pos_fb + mc_drv[k].off;
}

void mccore_home(const double *h) {
  for(int k = 0; k < NAX; k++) {
    mc_P.s[k] = h[k] / unit(k), mc_P.v[k] = 0;
    mc_P.phi[k] = mc_P.s[k] * 2.0 * M_PI / (mc_P.p[k] * (1.0 + mc_P.lead[k])), mc_P.w[k] = 0;
  }
  // Z hangs on its screw: stretch it by its weight, so it starts at rest
  mc_P.phi[MZ] += (mc_P.mz + mc_P.mh) * mc_P.g / mp_kscrew(&mc_P, MZ) * 2.0 * M_PI / mc_P.p[MZ];
  if(NAX > MA && mc_P.kt[MA] > 0) {  // A hangs on its gear with its unbalance
    mc_P.phi[MA] -= mc_P.ub * mc_P.g * sin(mc_P.s[MA] + mc_P.ub0) / mc_P.kt[MA] * 2.0 * M_PI / mc_P.p[MA];
  }
  mc_P.xf = mc_P.vxf = mc_P.yf = mc_P.vyf = mc_P.yb = mc_P.vyb = mc_P.hx = mc_P.vhx = mc_P.hy = mc_P.vhy = 0;
  need_off = NEED_OFF;
}

// --- the machine file ---------------------------------------------------------

static int nums(const char *s, double *v, int max) {
  int n = 0;
  char *end;
  for(; n < max; s = end) {
    double x = strtod(s, &end);
    if(end == s) {
      break;
    }
    v[n++] = x;
  }
  return n;
}

int mccore_axis(const char *s) {
  return !strcmp(s, "x") ? MX : !strcmp(s, "y") ? MY : !strcmp(s, "z") ? MZ : !strcmp(s, "a") ? MA : !strcmp(s, "c") ? MC : -1;
}

static double hz2k(double m, double f) {
  double w = 2.0 * M_PI * f;
  return m * w * w;
}

static double home_at[MP_NAX];
static int loading = 0;

int mccore_line(char *line) {
  char kw[16], ax[8];
  double v[8];
  int off = 0, off2 = 0;
  char *h = strchr(line, '#');
  if(h) {
    *h = 0;
  }
  if(sscanf(line, " %15s%n", kw, &off) != 1) {
    return 1;
  }
  char *p = line + off;
  int k = -1;
  if(sscanf(p, " %7s%n", ax, &off2) == 1) {
    k = mccore_axis(ax);
  }
  char *q = p + off2;  // after an axis name
  if(!strcmp(kw, "include")) {  // include FILE: machine lines from another file
    char fn[256], path[1024];
    if(sscanf(p, " %255s", fn) != 1) {
      return 0;
    }
    rel_path(path, sizeof(path), fn);
    char *text = read_file(path);
    if(!text) {
      fprintf(stderr, "mill_sim: cannot read %s\n", path);
      return 0;
    }
    int ok = 1;
    char *save = NULL;
    for(char *l = strtok_r(text, "\n", &save); l && ok; l = strtok_r(NULL, "\n", &save)) {
      if(!(ok = mccore_line(l))) {
        fprintf(stderr, "mill_sim: %s: bad line: %s\n", path, l);
      }
    }
    free(text);
    return ok;
  }
  if(!strcmp(kw, "drive")) {
    char conf[256], sim[256], cp[1024], sp[1024];
    if(k < 0 || sscanf(q, " %255s %255s %lf", conf, sim, v) != 3) {
      return 0;
    }
    snprintf(cp, sizeof(cp), "%s", conf);  // relative names: drive_open
    rel_path(sp, sizeof(sp), sim);
    mc_P.p[k] = v[0] / unit(k);  // a rotary axis: rad a motor turn
    if(k >= MA) {  // no end stops unless a stroke line sets them
      mc_P.smin[k] = -1e6, mc_P.smax[k] = 1e6;
    }
    mc_drv[k].pd.scale = v[0];
    return drive_open(&mc_drv[k], cp, sp) == 0;
  }
  if(!strcmp(kw, "rotary") && k >= MA && nums(q, v, 8) == 4) {
    mc_P.jm[k] = v[0], mc_P.kt[k] = v[1], mc_P.jl[k] = v[2], mc_P.zeta[k] = v[3];
  } else if(!strcmp(kw, "unbalance") && nums(p, v, 8) == 2) {
    mc_P.ub = v[0], mc_P.ub0 = v[1] * M_PI / 180.0;
  } else if(!strcmp(kw, "trt") && nums(p, v, 8) >= 5) {
    mc_trt.xr = v[0], mc_trt.yr = v[1], mc_trt.zr = v[2], mc_trt.dy = v[3], mc_trt.dz = v[4];
    mc_trt.tool = nums(p, v, 8) >= 6 ? v[5] : 0.0;
  } else if(!strcmp(kw, "screw") && k >= 0 && k < MA && nums(q, v, 8) == 6) {
    mc_P.jm[k] = v[0], mc_P.ea[k] = v[1], mc_P.l0[k] = v[2], mc_P.ldir[k] = v[3], mc_P.ka[k] = v[4], mc_P.zeta[k] = v[5];
  } else if(!strcmp(kw, "fric") && k >= 0 && nums(q, v, 8) == 4) {
    mc_P.tc[k] = v[0], mc_P.tv[k] = v[1], mc_P.fc[k] = v[2], mc_P.fv[k] = v[3];
  } else if(!strcmp(kw, "stroke") && k >= 0 && nums(q, v, 8) == 2) {
    mc_P.smin[k] = v[0] / unit(k), mc_P.smax[k] = v[1] / unit(k);
  } else if(!strcmp(kw, "scale") && k >= 0 && k < MA && nums(q, v, 8) >= 1) {
    mc_drv[k].scale_res = v[0] * 1e-6;
    mc_drv[k].scale_noise = nums(q, v, 8) >= 2 ? v[1] * 1e-6 : 0.0;
    if(!mc_drv[k].on || !(mc_drv[k].scale_pos = mc_drv[k].pin("enc_fb1.sim_pos")) || !(mc_drv[k].scale_abs = mc_drv[k].pin("enc_fb1.sim_abs_pos"))) {
      fprintf(stderr, "mill_sim: scale %s: the drive has no enc_fb1 (load it in its config)\n", ax);
      return 0;
    }
  } else if(!strcmp(kw, "lead") && k >= 0 && nums(q, v, 8) == 1) {  // um per m: lead error / thermal growth
    mc_P.lead[k] = v[0] * 1e-6;
  } else if(!strcmp(kw, "brake") && k >= 0) {  // a holding brake on the motor (released by fault0.mot_brake)
    mc_drv[k].has_brake = 1;
  } else if(!strcmp(kw, "rnoise") && k >= 0 && nums(q, v, 8) == 1) {
    float *res = mc_drv[k].on ? mc_drv[k].pin("enc_fb0.res") : NULL;
    mc_drv[k].rnoise = res ? v[0] * 2.0 * M_PI / *res : 0.0;
  } else if(!strcmp(kw, "frame") && nums(p, v, 8) == 4) {
    mc_P.mf = v[0];
    double Mx = v[0] + mc_P.mt + mc_P.mb;  // in x the table and bridge go with it (set table/bridge first)
    mc_P.kfx = hz2k(Mx, v[1]), mc_P.cfx = mp_damp(mc_P.kfx, Mx, v[3]);
    mc_P.kfy = hz2k(v[0], v[2]), mc_P.cfy = mp_damp(mc_P.kfy, v[0], v[3]);
  } else if(!strcmp(kw, "table") && nums(p, v, 8) == 1) {
    mc_P.mt = v[0];
  } else if(!strcmp(kw, "carriage") && nums(p, v, 8) == 1) {
    mc_P.mc = v[0];
  } else if(!strcmp(kw, "zslide") && nums(p, v, 8) == 1) {
    mc_P.mz = v[0];
  } else if(!strcmp(kw, "bridge") && nums(p, v, 8) == 3) {
    mc_P.mb = v[0];
    double M = v[0] + mc_P.mc + mc_P.mz + mc_P.mh;  // set carriage, zslide, head first
    mc_P.kb = hz2k(M, v[1]), mc_P.cb = mp_damp(mc_P.kb, M, v[2]);
  } else if(!strcmp(kw, "head") && nums(p, v, 8) == 6) {
    mc_P.mh = v[0], mc_P.zh = v[5];
    for(int i = 0; i < 2; i++) {
      mc_P.khx[i] = hz2k(v[0], v[1 + i]), mc_P.khy[i] = hz2k(v[0], v[3 + i]);
    }
  } else if(!strcmp(kw, "latency") && nums(p, v, 8) == 1 && v[0] >= 0) {  // [us] send to the drive having it
    latency = v[0] * 1e-6;
  } else if(!strcmp(kw, "servo") && nums(p, v, 8) == 1 && v[0] > 0) {
    mc_servo_t = v[0] / 1000.0;
  } else if(!strcmp(kw, "home") && (nums(p, v, 8) == 3 || nums(p, v, 8) == 5)) {  // while loading: once the pitches are known
    memset(home_at, 0, sizeof(home_at));
    memcpy(home_at, v, nums(p, v, 8) * sizeof(double));
    if(!loading) {
      mccore_home(home_at);
    }
  } else if(!strcmp(kw, "cut") && nums(p, v, 8) == 3) {
    mc_P.cut[0] = v[0], mc_P.cut[1] = v[1], mc_P.cut[2] = v[2];
  } else {
    return 0;
  }
  return 1;
}

int mccore_load(const char *file, const char *lib) {
  snprintf(libpath, sizeof(libpath), "%s", lib);
  snprintf(dir, sizeof(dir), "%s", file);
  char *sl = strrchr(dir, '/');
  if(sl) {
    *sl = 0;
  } else {
    strcpy(dir, ".");
  }
  mp_defaults(&mc_P);
  char *text = read_file(file);
  if(!text) {
    fprintf(stderr, "mill_sim: cannot read %s\n", file);
    return -1;
  }
  int rc = 0;
  char *save = NULL;
  loading = 1;
  for(char *line = strtok_r(text, "\n", &save); line && !rc; line = strtok_r(NULL, "\n", &save)) {
    if(!mccore_line(line)) {
      fprintf(stderr, "mill_sim: %s: bad line: %s\n", file, line);
      rc = -1;
    }
  }
  loading = 0;
  free(text);
  if(!rc && mc_drv[MA].on != mc_drv[MC].on) {
    fprintf(stderr, "mill_sim: %s: a five-axis machine has drives a and c\n", file);
    rc = -1;
  }
  mc_P.nax = mc_drv[MA].on && mc_drv[MC].on ? 5 : 3;
  for(int k = MA; k < mc_P.nax && !rc; k++) {
    if(mc_P.kt[k] <= 0 || mc_P.jl[k] <= 0) {
      fprintf(stderr, "mill_sim: %s: rotary %c: its stiffness and inertia (a rotary line)\n", file, axname[k]);
      rc = -1;
    }
  }
  mccore_home(home_at);
  for(int k = 0; k < 3 && !rc; k++) {
    if(!mc_drv[k].on) {
      fprintf(stderr, "mill_sim: %s: drive %c missing\n", file, axname[k]);
      rc = -1;
    }
  }
  return rc;
}

