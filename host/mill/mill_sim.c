/*
 * mill_sim: three simulated stmbl drives (X, Y, Z ball-screw axes) on a
 * simulated fixed-bridge mill (mplant.c), and a script front end that plays
 * LinuxCNC: every servo period (1 ms, or `servo 0.5` for 2 kHz) each drive
 * gets its command over the simulated smart-serial link.
 *
 * Each drive is a copy of libstmbl_drive.so in its own dlmopen namespace
 * (as host/portal). Every 200 us rt cycle the motors' angles go to the
 * drives, one drive tick, their torques back, then the plant in 10 steps.
 * A packet reaches the drive `latency` after LinuxCNC sends it and is
 * taken at the next rt cycle. A servo period that isn't a whole number of
 * rt cycles (0.5 ms = 2.5) gives the drives their packets 2 and 3 rt cycles
 * apart, as on the real link, whose drive clock locks to the packet stream.
 *
 * usage: mill_sim MACHINEFILE [SCRIPT]   (script from stdin without SCRIPT)
 *
 * MACHINEFILE (mm for machine positions, else SI):
 *   drive x|y|z CONF SIM PITCH       drive config (a,b,...: files joined), sim-only
 *                                    lines, screw pitch [mm]
 *   screw x|y|z JM EA L0 LDIR KA ZETA   motor side inertia (rotor, coupling,
 *                                    screw) [kg m^2], the screw's E A [N], nut
 *                                    distance from the fixed bearing at 0 [m]
 *                                    and +1/-1 as the axis grows, nut + bearing
 *                                    + coupling stiffness [N/m], damping ratio
 *   fric x|y|z TC TV FC FV           motor side Coulomb [Nm] / viscous [Nm s/rad],
 *                                    guide Coulomb [N] / viscous [N s/m]
 *   stroke x|y|z MIN MAX             end stops [mm]
 *   frame M FX FY ZETA               the base [kg] on its feet: modes [Hz]
 *   table M / carriage M / zslide M  [kg]
 *   bridge M FY ZETA                 the bridge beam [kg]; the columns' y mode
 *                                    [Hz] with everything it carries
 *   head M FX_TOP FX_BOT FY_TOP FY_BOT ZETA   the spindle tip's modal mass and
 *                                    its modes [Hz] with Z at the top / bottom
 *   scale x|y|z RES [NOISE]          a linear scale on the axis [um per count]
 *                                    (and its noise, um rms): the drive's
 *                                    enc_fb1 reads it (the drive config links
 *                                    it as the joint feedback)
 *   lead x|y|z UM_PER_M              the nut travels this much more per metre than
 *                                    the motor's turns say: the screw's lead error,
 *                                    or its growth when warm (11.5 um/m per K)
 *   brake x|y|z                      a holding brake on the motor: the rotor
 *                                    stands unless fault0.mot_brake releases it
 *   rnoise x|y|z LSB                 motor feedback noise, rms in counts of
 *                                    enc_fb0.res (a resolver's jitter)
 *   servo MS                         LinuxCNC's servo period [ms] (1, 0.5, ...)
 *   latency US                       the packet's way from LinuxCNC into the drive
 *                                    (50 us); it is taken at the next rt cycle
 *   include FILE                     machine lines from FILE (relative to this one)
 *   home X Y Z                       start position [mm]
 *
 * Script commands (plus the machine lines above):
 *   run T                            simulate T seconds
 *   lcnc EN                          enable the drives where the machine is
 *   move T X Y Z / tmove T X Y Z     smooth (quintic) / trapezoid move in T s
 *   trapfrac F                       trapezoid's acceleration fraction (0.25)
 *   circle T CX CY R TURNS           XY circle(s) at constant speed from the
 *                                    current angle (enter it first)
 *   square T CX CY A [S]             an XY square of side A, each side a
 *                                    trapezoid move of T s, exact stop (S 1:
 *                                    jerk-limited, quintic sides)
 *   cut FX FY FZ                     a steady force on the tool [N]
 *   drv x|y|z|all LINE               a console line to a drive
 *   mark                             clear the maxima and the rms sums
 *   print NAME...                    mill.NAME = value. NAME: x y z (LinuxCNC's
 *                                    feedback [mm]), cx cy cz (command), tx ty tz
 *                                    (tool relative to the work), etool (|tool -
 *                                    command| xy [um]), econt (xy contour error:
 *                                    the tool's distance from the commanded
 *                                    path, LAG ms ago [um]), ez (z error [um]),
 *                                    ferr_x ferr_y ferr_z (command - feedback
 *                                    [um]), hx hy (head [um]), yb (bridge [um]),
 *                                    frame_x frame_y [um], cur_x cur_y cur_z [A],
 *                                    fault, fault_code, t; max_NAME (largest
 *                                    |value| since mark: econt etool ez ferr_*
 *                                    hx hy yb frame_* cur_*), rms_NAME (econt
 *                                    etool ez ferr_*)
 *   log FILE | log off               every servo period: t cx cy cz tx ty tz
 *                                    x y z cur_x cur_y cur_z hx hy yb, then watches
 *   watch x|y|z PIN                  also log a drive pin
 */
#include "mplant.h"
#include "stmbl_sim_api.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define RT 0.0002
#define SUB 10

typedef struct {
  void *h;
  int (*boot)(const char *, const char *);
  void (*tick)(void);
  void (*update)(stmbl_sim_pd_t *, long);
  float *(*pin)(const char *);
  void (*cmd)(const char *);
  float *sim_pos, *sim_vel, *te_int, *scale_pos, *scale_abs, *brake;
  stmbl_sim_pd_t pd;
  double off;            // LinuxCNC position [mm] = pos_fb + off
  double scale_res;      // [m per count] (0: no scale)
  double scale_noise;    // [m rms]
  double rnoise;         // motor feedback noise [rad rms]
  int has_brake;         // a holding brake (machine line brake)
  int on;
} drv_t;

static drv_t D[3];
static mp_plant P;
static double servo_t = 0.001;
static long ticks = 0;
static char libpath[1024], dir[1024] = ".";
#define NEED_OFF 5
static int need_off = NEED_OFF;
static const char *axname = "xyz";

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

static int drive_open(drv_t *d, const char *conf, const char *simfile) {
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
static double scale_mm(int k) {
  return P.s[k] * 1000.0;
}

static void tick(int servo) {
  double T[3] = {0};
  for(int k = 0; k < 3; k++) {
    drv_t *d = &D[k];
    double g = 2.0 * M_PI / P.p[k];
    *d->sim_pos = (float)wrap(P.phi[k] + (d->rnoise > 0 ? d->rnoise * gauss() : 0.0));
    *d->sim_vel = (float)P.w[k];
    *d->te_int = 0.0f;
    if(d->scale_pos) {
      // as the motor's: hv0 moves the rotor over the rt cycle before enc_fb0
      // reads it, so the drive sees the end of the cycle; the scale too
      double s = P.s[k] + P.v[k] * RT + (d->scale_noise > 0 ? d->scale_noise * gauss() : 0.0);
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
    P.brake[k] = d->has_brake && !(d->brake && *d->brake > 0.0f);
  }
  if(servo && need_off && --need_off == 0) {  // absolute positions: LinuxCNC's offset
    for(int k = 0; k < 3; k++) {
      D[k].off = scale_mm(k) - D[k].pd.pos_fb;
    }
  }
  for(int s = 0; s < SUB; s++) {
    mp_step(&P, T, RT / SUB);
  }
  ticks++;
}

// One servo period: LinuxCNC sends the packet at t_send; it reaches the
// drive `latency` later and is taken at the next rt cycle (on the real
// drive the frt loop reads the UART, rt uses it). Then the rt cycles up to
// the next send. 1 kHz: always 5 cycles after; 2 kHz: alternately 2 and 3.
static double t_send = 0.0, latency = 50e-6;
static void servo_period(void) {
  double t_arr = t_send + latency;
  int pending  = 1;
  t_send += servo_t;
  while(ticks * RT < t_send - 1e-9) {
    int now = pending && ticks * RT >= t_arr - 1e-9;
    pending = pending && !now;
    tick(now);
  }
}

static void set_cmd(int k, int en, double pos, double vel) {
  drv_t *d = &D[k];
  d->pd.enable = en;
  if(need_off) {
    d->pd.pos_cmd = d->pd.pos_fb, d->pd.vel_cmd = 0.0;
    return;
  }
  d->pd.pos_cmd = pos - d->off;
  d->pd.vel_cmd = vel;
}

static double fb(int k) {
  return D[k].pd.pos_fb + D[k].off;
}

static void home(double X, double Y, double Z) {
  double h[3] = {X, Y, Z};
  for(int k = 0; k < 3; k++) {
    P.s[k] = h[k] / 1000.0, P.v[k] = 0;
    P.phi[k] = P.s[k] * 2.0 * M_PI / (P.p[k] * (1.0 + P.lead[k])), P.w[k] = 0;
  }
  // Z hangs on its screw: stretch it by its weight, so it starts at rest
  P.phi[MZ] += (P.mz + P.mh) * P.g / mp_kscrew(&P, MZ) * 2.0 * M_PI / P.p[MZ];
  P.xf = P.vxf = P.yf = P.vyf = P.yb = P.vyb = P.hx = P.vhx = P.hy = P.vhy = 0;
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

static int axis_of(const char *s) {
  return !strcmp(s, "x") ? MX : !strcmp(s, "y") ? MY : !strcmp(s, "z") ? MZ : -1;
}

static double hz2k(double m, double f) {
  double w = 2.0 * M_PI * f;
  return m * w * w;
}

static double home_at[3];
static int loading = 0;

static int machine_line(char *line) {
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
    k = axis_of(ax);
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
      if(!(ok = machine_line(l))) {
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
    P.p[k] = v[0] / 1000.0;
    D[k].pd.scale = v[0];
    return drive_open(&D[k], cp, sp) == 0;
  }
  if(!strcmp(kw, "screw") && k >= 0 && nums(q, v, 8) == 6) {
    P.jm[k] = v[0], P.ea[k] = v[1], P.l0[k] = v[2], P.ldir[k] = v[3], P.ka[k] = v[4], P.zeta[k] = v[5];
  } else if(!strcmp(kw, "fric") && k >= 0 && nums(q, v, 8) == 4) {
    P.tc[k] = v[0], P.tv[k] = v[1], P.fc[k] = v[2], P.fv[k] = v[3];
  } else if(!strcmp(kw, "stroke") && k >= 0 && nums(q, v, 8) == 2) {
    P.smin[k] = v[0] / 1000.0, P.smax[k] = v[1] / 1000.0;
  } else if(!strcmp(kw, "scale") && k >= 0 && nums(q, v, 8) >= 1) {
    D[k].scale_res = v[0] * 1e-6;
    D[k].scale_noise = nums(q, v, 8) >= 2 ? v[1] * 1e-6 : 0.0;
    if(!D[k].on || !(D[k].scale_pos = D[k].pin("enc_fb1.sim_pos")) || !(D[k].scale_abs = D[k].pin("enc_fb1.sim_abs_pos"))) {
      fprintf(stderr, "mill_sim: scale %s: the drive has no enc_fb1 (load it in its config)\n", ax);
      return 0;
    }
  } else if(!strcmp(kw, "lead") && k >= 0 && nums(q, v, 8) == 1) {  // um per m: lead error / thermal growth
    P.lead[k] = v[0] * 1e-6;
  } else if(!strcmp(kw, "brake") && k >= 0) {  // a holding brake on the motor (released by fault0.mot_brake)
    D[k].has_brake = 1;
  } else if(!strcmp(kw, "rnoise") && k >= 0 && nums(q, v, 8) == 1) {
    float *res = D[k].on ? D[k].pin("enc_fb0.res") : NULL;
    D[k].rnoise = res ? v[0] * 2.0 * M_PI / *res : 0.0;
  } else if(!strcmp(kw, "frame") && nums(p, v, 8) == 4) {
    P.mf = v[0];
    double Mx = v[0] + P.mt + P.mb;  // in x the table and bridge go with it (set table/bridge first)
    P.kfx = hz2k(Mx, v[1]), P.cfx = mp_damp(P.kfx, Mx, v[3]);
    P.kfy = hz2k(v[0], v[2]), P.cfy = mp_damp(P.kfy, v[0], v[3]);
  } else if(!strcmp(kw, "table") && nums(p, v, 8) == 1) {
    P.mt = v[0];
  } else if(!strcmp(kw, "carriage") && nums(p, v, 8) == 1) {
    P.mc = v[0];
  } else if(!strcmp(kw, "zslide") && nums(p, v, 8) == 1) {
    P.mz = v[0];
  } else if(!strcmp(kw, "bridge") && nums(p, v, 8) == 3) {
    P.mb = v[0];
    double M = v[0] + P.mc + P.mz + P.mh;  // set carriage, zslide, head first
    P.kb = hz2k(M, v[1]), P.cb = mp_damp(P.kb, M, v[2]);
  } else if(!strcmp(kw, "head") && nums(p, v, 8) == 6) {
    P.mh = v[0], P.zh = v[5];
    for(int i = 0; i < 2; i++) {
      P.khx[i] = hz2k(v[0], v[1 + i]), P.khy[i] = hz2k(v[0], v[3 + i]);
    }
  } else if(!strcmp(kw, "latency") && nums(p, v, 8) == 1 && v[0] >= 0) {  // [us] send to the drive having it
    latency = v[0] * 1e-6;
  } else if(!strcmp(kw, "servo") && nums(p, v, 8) == 1 && v[0] > 0) {
    servo_t = v[0] / 1000.0;
  } else if(!strcmp(kw, "home") && nums(p, v, 8) == 3) {  // while loading: once the pitches are known
    memcpy(home_at, v, sizeof(home_at));
    if(!loading) {
      home(v[0], v[1], v[2]);
    }
  } else if(!strcmp(kw, "cut") && nums(p, v, 8) == 3) {
    P.cut[0] = v[0], P.cut[1] = v[1], P.cut[2] = v[2];
  } else {
    return 0;
  }
  return 1;
}

static int load_machine(const char *file) {
  snprintf(dir, sizeof(dir), "%s", file);
  char *sl = strrchr(dir, '/');
  if(sl) {
    *sl = 0;
  } else {
    strcpy(dir, ".");
  }
  mp_defaults(&P);
  char *text = read_file(file);
  if(!text) {
    fprintf(stderr, "mill_sim: cannot read %s\n", file);
    return -1;
  }
  int rc = 0;
  char *save = NULL;
  loading = 1;
  for(char *line = strtok_r(text, "\n", &save); line && !rc; line = strtok_r(NULL, "\n", &save)) {
    if(!machine_line(line)) {
      fprintf(stderr, "mill_sim: %s: bad line: %s\n", file, line);
      rc = -1;
    }
  }
  loading = 0;
  free(text);
  home(home_at[0], home_at[1], home_at[2]);
  for(int k = 0; k < 3 && !rc; k++) {
    if(!D[k].on) {
      fprintf(stderr, "mill_sim: %s: drive %c missing\n", file, axname[k]);
      rc = -1;
    }
  }
  return rc;
}

// --- LinuxCNC's side ----------------------------------------------------------

static double cmd[3], cmdv[3];
static int en = 0;
static double trapfrac = 0.25, tnow = 0.0;
static FILE *logfile = NULL;
#define NWATCH 16
static float *watch[NWATCH];
static int nwatch = 0;

// the commands and tool points of the last HIST periods: the contour error
#define HIST 2000
#define LAG_S 0.025  // the tool point judged is this old: the path around it, both sides
static double hist[HIST][2], thist[HIST][2];
static int hist_n = 0, hist_head = 0;

static double contour_error(void) {
  int lag = (int)lround(LAG_S / servo_t);
  if(hist_n <= lag) {
    return 0.0;
  }
  const double *t = thist[(hist_head - 1 - lag + HIST) % HIST];
  double best = 1e30;
  for(int i = 0; i + 1 < hist_n; i++) {
    const double *a = hist[(hist_head - hist_n + i + HIST) % HIST], *b = hist[(hist_head - hist_n + i + 1 + HIST) % HIST];
    double d[2] = {b[0] - a[0], b[1] - a[1]}, l2 = d[0] * d[0] + d[1] * d[1];
    double u = l2 > 0 ? ((t[0] - a[0]) * d[0] + (t[1] - a[1]) * d[1]) / l2 : 0.0;
    u = u < 0 ? 0 : u > 1 ? 1 : u;
    double e = hypot(t[0] - a[0] - u * d[0], t[1] - a[1] - u * d[1]);
    best = e < best ? e : best;
  }
  return best * 1000.0;
}

// The tool relative to the work at LinuxCNC's time tnow [mm]. A servo period
// that isn't whole rt cycles leaves the plant up to an rt cycle off the
// servo grid (2 kHz: 2 or 3 cycles per period); moved back along its
// velocity, so the errors compare tool and command at the same moment.
static void tool_mm(double t[3]) {
  mp_tool(&P, t);
  double dt = ticks * RT - tnow, v[3] = {P.v[MX] + P.vhx, P.v[MY] + P.vyb + P.vhy, P.v[MZ]};
  for(int k = 0; k < 3; k++) {
    t[k] = (t[k] - v[k] * dt) * 1000.0;
  }
}

static float *pinv(int k, const char *name) {
  return D[k].pin(name);
}

enum { M_ECONT, M_ETOOL, M_EZ, M_FX, M_FY, M_FZ, M_HX, M_HY, M_YB, M_FRX, M_FRY, M_CX, M_CY, M_CZ, NMAX };
static const char *maxname[NMAX] = {"econt", "etool", "ez", "ferr_x", "ferr_y", "ferr_z", "hx", "hy", "yb", "frame_x", "frame_y", "cur_x", "cur_y", "cur_z"};
static double mx[NMAX], sq[NMAX];
static long nsq = 0;

static double value(const char *n) {
  double t[3];
  tool_mm(t);
  for(int k = 0; k < 3; k++) {
    char b[16];
    if(!strcmp(n, (snprintf(b, sizeof(b), "%c", axname[k]), b))) return fb(k);
    if(!strcmp(n, (snprintf(b, sizeof(b), "c%c", axname[k]), b))) return cmd[k];
    if(!strcmp(n, (snprintf(b, sizeof(b), "t%c", axname[k]), b))) return t[k];
    if(!strcmp(n, (snprintf(b, sizeof(b), "ferr_%c", axname[k]), b))) return (cmd[k] - fb(k)) * 1000.0;
    if(!strcmp(n, (snprintf(b, sizeof(b), "cur_%c", axname[k]), b))) return D[k].pd.current;
  }
  if(!strcmp(n, "etool")) return hypot(t[0] - cmd[0], t[1] - cmd[1]) * 1000.0;
  if(!strcmp(n, "econt")) return contour_error();
  if(!strcmp(n, "ez")) return (t[2] - cmd[2]) * 1000.0;
  if(!strcmp(n, "hx")) return P.hx * 1e6;
  if(!strcmp(n, "hy")) return P.hy * 1e6;
  if(!strcmp(n, "yb")) return P.yb * 1e6;
  if(!strcmp(n, "frame_x")) return P.xf * 1e6;
  if(!strcmp(n, "frame_y")) return P.yf * 1e6;
  if(!strcmp(n, "fault")) return D[0].pd.fault || D[1].pd.fault || D[2].pd.fault;
  if(!strcmp(n, "fault_code")) {
    for(int k = 0; k < 3; k++) {
      float *f = pinv(k, "fault0.fault");
      if(f && *f != 0.0f) return *f;
    }
    return 0;
  }
  if(!strcmp(n, "hits")) return P.hits[0] + P.hits[1] + P.hits[2];
  if(!strcmp(n, "t")) return tnow;
  return NAN;
}

static void servo(void) {
  for(int k = 0; k < 3; k++) {
    set_cmd(k, en, cmd[k], cmdv[k]);
  }
  servo_period();
  tnow += servo_t;
  hist[hist_head][0] = cmd[0], hist[hist_head][1] = cmd[1];
  double t[3];
  tool_mm(t);
  thist[hist_head][0] = t[0], thist[hist_head][1] = t[1];
  hist_head = (hist_head + 1) % HIST;
  hist_n = hist_n < HIST ? hist_n + 1 : HIST;
  if(need_off) {  // hold where the machine is until the positions are read
    for(int k = 0; k < 3; k++) {
      cmd[k] = scale_mm(k);
    }
    return;
  }
  for(int i = 0; i < NMAX; i++) {
    double v = value(maxname[i]);
    mx[i] = fabs(v) > mx[i] ? fabs(v) : mx[i];
    sq[i] += v * v;
  }
  nsq++;
  if(logfile) {
    fprintf(logfile, "%.5f %.5f %.5f %.5f %.5f %.5f %.5f %.5f %.5f %.5f %.3f %.3f %.3f %.3f %.3f %.3f", tnow, cmd[0], cmd[1], cmd[2], t[0], t[1], t[2],
            fb(0), fb(1), fb(2), D[0].pd.current, D[1].pd.current, D[2].pd.current, P.hx * 1e6, P.hy * 1e6, P.yb * 1e6);
    for(int i = 0; i < nwatch; i++) {
      fprintf(logfile, " %.6g", *watch[i]);
    }
    fprintf(logfile, "\n");
  }
}

static void run(double T) {
  for(long i = lround(T / servo_t); i > 0; i--) {
    servo();
  }
}

static void profile(int trap, double u, double *s, double *ds) {
  if(!trap) {
    *s = u * u * u * (10 - 15 * u + 6 * u * u);
    *ds = 30 * u * u * (1 - 2 * u + u * u);
    return;
  }
  double f = trapfrac, vmax = 1.0 / (1.0 - f);
  if(u < f) {
    *s = 0.5 * vmax / f * u * u, *ds = vmax / f * u;
  } else if(u > 1 - f) {
    double r = 1 - u;
    *s = 1 - 0.5 * vmax / f * r * r, *ds = vmax / f * r;
  } else {
    *s = 0.5 * vmax * f + vmax * (u - f), *ds = vmax;
  }
}

static void move(int trap, double T, const double *b) {
  double a[3] = {cmd[0], cmd[1], cmd[2]};
  long n = lround(T / servo_t);
  for(long i = 1; i <= n; i++) {
    double s, ds;
    profile(trap, (double)i / n, &s, &ds);
    for(int k = 0; k < 3; k++) {
      cmd[k] = a[k] + (b[k] - a[k]) * s;
      cmdv[k] = (b[k] - a[k]) * ds / T;
    }
    servo();
  }
  cmdv[0] = cmdv[1] = cmdv[2] = 0.0;
}

static void circle(double T, double cx, double cy, double r, double turns) {
  double a0 = atan2(cmd[1] - cy, cmd[0] - cx), w = 2 * M_PI * turns / T;
  long n = lround(T / servo_t);
  for(long i = 1; i <= n; i++) {
    double a = a0 + w * i * servo_t;
    cmd[0] = cx + r * cos(a), cmd[1] = cy + r * sin(a);
    cmdv[0] = -r * w * sin(a), cmdv[1] = r * w * cos(a);
    servo();
  }
  cmdv[0] = cmdv[1] = 0.0;
}

static void print(char *names) {
  for(char *n = strtok(names, " \t\n"); n; n = strtok(NULL, " \t\n")) {
    double v = NAN;
    if(!strncmp(n, "max_", 4) || !strncmp(n, "rms_", 4)) {
      for(int i = 0; i < NMAX; i++) {
        if(!strcmp(n + 4, maxname[i])) {
          v = n[0] == 'm' ? mx[i] : nsq ? sqrt(sq[i] / nsq) : 0.0;
        }
      }
    } else {
      v = value(n);
    }
    if(isnan(v)) {
      printf("ERROR: unknown value %s\n", n);
    } else {
      printf("mill.%s = %.6g\n", n, v);
    }
  }
}

static int script_line(char *line) {
  char kw[16];
  int off = 0;
  char *h = strchr(line, '#');
  if(h) {
    *h = 0;
  }
  if(sscanf(line, " %15s%n", kw, &off) != 1) {
    return 0;
  }
  char *p = line + off;
  double v[6];
  if(!strcmp(kw, "run") && sscanf(p, "%lf", v) == 1) {
    run(v[0]);
  } else if(!strcmp(kw, "lcnc") && sscanf(p, "%lf", v) == 1) {
    if(v[0] != 0.0) {  // LinuxCNC enables once it has read the positions
      while(need_off) {
        servo();
      }
    }
    en = v[0] != 0.0;
    if(en) {
      for(int k = 0; k < 3; k++) {
        cmd[k] = fb(k);
      }
    }
  } else if((!strcmp(kw, "move") || !strcmp(kw, "tmove")) && sscanf(p, "%lf %lf %lf %lf", v, v + 1, v + 2, v + 3) == 4) {
    move(kw[0] == 't', v[0], v + 1);
  } else if(!strcmp(kw, "trapfrac") && sscanf(p, "%lf", v) == 1) {
    trapfrac = v[0];
  } else if(!strcmp(kw, "circle") && sscanf(p, "%lf %lf %lf %lf %lf", v, v + 1, v + 2, v + 3, v + 4) == 5) {
    circle(v[0], v[1], v[2], v[3], v[4]);
  } else if(!strcmp(kw, "square") && sscanf(p, "%lf %lf %lf %lf", v, v + 1, v + 2, v + 3) == 4) {
    int smooth = sscanf(p, "%*f %*f %*f %*f %lf", v + 4) == 1 && v[4] != 0.0;  // 1: S-curve (quintic) sides
    double a = v[3] / 2, c[4][2] = {{v[1] - a, v[2] - a}, {v[1] + a, v[2] - a}, {v[1] + a, v[2] + a}, {v[1] - a, v[2] + a}};
    for(int i = 0; i <= 4; i++) {
      double b[3] = {c[i % 4][0], c[i % 4][1], cmd[2]};
      move(!smooth, v[0], b);
    }
  } else if(!strcmp(kw, "drv")) {
    char who[8];
    int n = 0;
    if(sscanf(p, " %7s %n", who, &n) != 1) {
      return -1;
    }
    p[strcspn(p, "\r\n")] = 0;
    for(int k = 0; k < 3; k++) {
      char nm[2] = {axname[k], 0};
      if(!strcmp(who, "all") || !strcmp(who, nm)) {
        D[k].cmd(p + n);
        fflush(stdout);
      }
    }
  } else if(!strcmp(kw, "watch")) {
    char who[8], pin[64];
    int k;
    if(sscanf(p, " %7s %63s", who, pin) != 2 || nwatch == NWATCH || (k = axis_of(who)) < 0) {
      return -1;
    }
    float *w = D[k].pin(pin);
    if(!w) {
      return -1;
    }
    watch[nwatch++] = w;
  } else if(!strcmp(kw, "mark")) {
    memset(mx, 0, sizeof(mx));
    memset(sq, 0, sizeof(sq));
    nsq = 0;
  } else if(!strcmp(kw, "print")) {
    print(p);
  } else if(!strcmp(kw, "log")) {
    char fn[512];
    if(logfile) {
      fclose(logfile);
      logfile = NULL;
    }
    if(sscanf(p, " %511s", fn) == 1 && strcmp(fn, "off")) {
      if(!(logfile = fopen(fn, "w"))) {
        return -1;
      }
    }
  } else {
    return machine_line(line) ? 0 : -1;
  }
  return 0;
}

int main(int argc, char **argv) {
  if(argc < 2) {
    fprintf(stderr, "usage: mill_sim MACHINEFILE [SCRIPT]\n");
    return 2;
  }
  const char *sl = strrchr(argv[0], '/');
  snprintf(libpath, sizeof(libpath), "%.*s../arm/libstmbl_drive.so", sl ? (int)(sl - argv[0] + 1) : 0, argv[0]);
  if(load_machine(argv[1])) {
    return 1;
  }
  FILE *in = argc > 2 ? fopen(argv[2], "r") : stdin;
  if(!in) {
    fprintf(stderr, "mill_sim: cannot read %s\n", argv[2]);
    return 1;
  }
  char line[1024];
  int rc = 0;
  while(fgets(line, sizeof(line), in)) {
    char copy[1024];
    snprintf(copy, sizeof(copy), "%s", line);
    if(script_line(line) < 0) {
      printf("ERROR: bad line: %s", copy);
      rc = 1;
    }
    fflush(stdout);
  }
  if(logfile) {
    fclose(logfile);
  }
  return rc;
}
