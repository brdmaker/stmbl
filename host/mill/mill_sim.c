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
 * MACHINEFILE: see millcore.h.
 *
 * Script commands (plus the machine lines above):
 *   run T                            simulate T seconds
 *   lcnc EN                          enable the drives where the machine is
 *   move T X Y Z / tmove T X Y Z     smooth (quintic) / trapezoid move in T s
 *   trapfrac F                       trapezoid's acceleration fraction (0.25)
 *   move T X Y Z A C                 a five-axis machine: the joints (A C [deg]) too
 *   wmove T X Y Z A C                five-axis, tool centre point: the tool tip moves on a line
 *                                    in the work's frame (quintic) while A and C turn; the joints
 *                                    by xyzac-trt-kins' inverse (the machine's trt line), as
 *                                    LinuxCNC's world mode sends them
 *   rtcp T A C                       the same, the tip staying where it is in the work
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
 *                                    etool ez ferr_*). Five axes: a c (LinuxCNC's
 *                                    feedback [deg]), ca cc, ta tc (the table's
 *                                    angles), ferr_a ferr_c [arcsec], cur_a cur_c,
 *                                    ework (the tool tip against the command in the
 *                                    work's frame [um], lag included), econtw (its
 *                                    distance from the commanded path in the work,
 *                                    LAG ms ago [um]), wx wy wz (the tip in the work
 *                                    [mm])
 *   log FILE | log off               every servo period: t cx cy cz tx ty tz
 *                                    x y z cur_x cur_y cur_z hx hy yb (five axes:
 *                                    then ca cc ta tc ework), then watches
 *   watch x|y|z|a|c PIN              also log a drive pin
 */
#include "millcore.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *axname = "xyzac";
#define NAX (mc_P.nax)

// --- LinuxCNC's side ----------------------------------------------------------

static double cmd[MP_NAX], cmdv[MP_NAX];
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
static double whist[HIST][3], wthist[HIST][3];  // five axes: the commanded and the actual tip in the work [mm]
static int hist_n = 0, hist_head = 0;

static double contour_error(void) {
  int lag = (int)lround(LAG_S / mc_servo_t);
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

// the same in the work's frame, in 3D: the actual tip LAG ago against the commanded path around it
static double contour_error_work(void) {
  int lag = (int)lround(LAG_S / mc_servo_t);
  if(NAX < 5 || hist_n <= lag) {
    return 0.0;
  }
  const double *t = wthist[(hist_head - 1 - lag + HIST) % HIST];
  double best = 1e30;
  for(int i = 0; i + 1 < hist_n; i++) {
    const double *a = whist[(hist_head - hist_n + i + HIST) % HIST], *b = whist[(hist_head - hist_n + i + 1 + HIST) % HIST];
    double d[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, l2 = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
    double u = l2 > 0 ? ((t[0] - a[0]) * d[0] + (t[1] - a[1]) * d[1] + (t[2] - a[2]) * d[2]) / l2 : 0.0;
    u = u < 0 ? 0 : u > 1 ? 1 : u;
    double e = sqrt(pow(t[0] - a[0] - u * d[0], 2) + pow(t[1] - a[1] - u * d[1], 2) + pow(t[2] - a[2] - u * d[2], 2));
    best = e < best ? e : best;
  }
  return best * 1000.0;
}

// The tool relative to the work at LinuxCNC's time tnow [mm] (millcore.h)
static void tool_mm(double t[3]) {
  mccore_tool(tnow, t);
}

static float *pinv(int k, const char *name) {
  return mc_drv[k].pin(name);
}

enum { M_ECONT, M_ETOOL, M_EZ, M_FX, M_FY, M_FZ, M_HX, M_HY, M_YB, M_FRX, M_FRY, M_CX, M_CY, M_CZ, M_FA, M_FC, M_CA, M_CC, M_EW, M_ECW, NMAX };
static const char *maxname[NMAX] = {"econt", "etool", "ez", "ferr_x", "ferr_y", "ferr_z", "hx", "hy", "yb", "frame_x", "frame_y", "cur_x", "cur_y", "cur_z",
                                    "ferr_a", "ferr_c", "cur_a", "cur_c", "ework", "econtw"};
static double mx[NMAX], sq[NMAX];
static long nsq = 0;

// the commanded tip in the work's frame [mm]
static void work_cmd(double w[3]) {
  mccore_trt_fwd(cmd, w);
}

static double value(const char *n) {
  double t[3], ang[2];
  tool_mm(t);
  mccore_angles(tnow, ang);
  for(int k = 0; k < MP_NAX; k++) {
    char b[16];
    int here = k < NAX;  // a three-axis machine's a and c read 0
    if(!strcmp(n, (snprintf(b, sizeof(b), "%c", axname[k]), b))) return here ? mccore_fb(k) : 0.0;
    if(!strcmp(n, (snprintf(b, sizeof(b), "c%c", axname[k]), b))) return here ? cmd[k] : 0.0;
    if(!strcmp(n, (snprintf(b, sizeof(b), "t%c", axname[k]), b))) return k < 3 ? t[k] : ang[k - 3];
    if(!strcmp(n, (snprintf(b, sizeof(b), "ferr_%c", axname[k]), b))) return here ? (cmd[k] - mccore_fb(k)) * (k < 3 ? 1000.0 : 3600.0) : 0.0;
    if(!strcmp(n, (snprintf(b, sizeof(b), "cur_%c", axname[k]), b))) return here ? mc_drv[k].pd.current : 0.0;
  }
  if(NAX >= 5 && (!strcmp(n, "ework") || !strcmp(n, "wx") || !strcmp(n, "wy") || !strcmp(n, "wz"))) {
    double w[3], wc[3];
    mccore_work(tnow, w);
    work_cmd(wc);
    if(n[1] != 'w') return w[n[1] - 'x'];
    return sqrt(pow(w[0] - wc[0], 2) + pow(w[1] - wc[1], 2) + pow(w[2] - wc[2], 2)) * 1000.0;
  }
  if(!strcmp(n, "ework")) return 0.0;
  if(!strcmp(n, "econtw")) return contour_error_work();
  if(!strcmp(n, "etool")) return hypot(t[0] - cmd[0], t[1] - cmd[1]) * 1000.0;
  if(!strcmp(n, "econt")) return contour_error();
  if(!strcmp(n, "ez")) return (t[2] - cmd[2]) * 1000.0;
  if(!strcmp(n, "hx")) return mc_P.hx * 1e6;
  if(!strcmp(n, "hy")) return mc_P.hy * 1e6;
  if(!strcmp(n, "yb")) return mc_P.yb * 1e6;
  if(!strcmp(n, "frame_x")) return mc_P.xf * 1e6;
  if(!strcmp(n, "frame_y")) return mc_P.yf * 1e6;
  if(!strcmp(n, "fault")) {
    for(int k = 0; k < NAX; k++) {
      if(mc_drv[k].pd.fault) return 1;
    }
    return 0;
  }
  if(!strcmp(n, "fault_code")) {
    for(int k = 0; k < NAX; k++) {
      float *f = pinv(k, "fault0.fault");
      if(f && *f != 0.0f) return *f;
    }
    return 0;
  }
  if(!strcmp(n, "hits")) {
    int h = 0;
    for(int k = 0; k < NAX; k++) {
      h += mc_P.hits[k];
    }
    return h;
  }
  if(!strcmp(n, "t")) return tnow;
  return NAN;
}

static void servo(void) {
  for(int k = 0; k < NAX; k++) {
    mccore_set_cmd(k, en, cmd[k], cmdv[k]);
  }
  mccore_servo(mc_servo_t);
  tnow += mc_servo_t;
  hist[hist_head][0] = cmd[0], hist[hist_head][1] = cmd[1];
  double t[3];
  tool_mm(t);
  thist[hist_head][0] = t[0], thist[hist_head][1] = t[1];
  if(NAX >= 5) {
    work_cmd(whist[hist_head]);
    mccore_work(tnow, wthist[hist_head]);
  }
  hist_head = (hist_head + 1) % HIST;
  hist_n = hist_n < HIST ? hist_n + 1 : HIST;
  if(!mccore_synced()) {  // hold where the machine is until the positions are read
    for(int k = 0; k < NAX; k++) {
      cmd[k] = mccore_scale(k);
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
            mccore_fb(0), mccore_fb(1), mccore_fb(2), mc_drv[0].pd.current, mc_drv[1].pd.current, mc_drv[2].pd.current, mc_P.hx * 1e6, mc_P.hy * 1e6, mc_P.yb * 1e6);
    if(NAX >= 5) {
      double ang[2];
      mccore_angles(tnow, ang);
      fprintf(logfile, " %.5f %.5f %.5f %.5f %.3f", cmd[MA], cmd[MC], ang[0], ang[1], value("ework"));
    }
    for(int i = 0; i < nwatch; i++) {
      fprintf(logfile, " %.6g", *watch[i]);
    }
    fprintf(logfile, "\n");
  }
}

static void run(double T) {
  for(long i = lround(T / mc_servo_t); i > 0; i--) {
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
  double a[MP_NAX];
  memcpy(a, cmd, sizeof(a));
  long n = lround(T / mc_servo_t);
  for(long i = 1; i <= n; i++) {
    double s, ds;
    profile(trap, (double)i / n, &s, &ds);
    for(int k = 0; k < NAX; k++) {
      cmd[k] = a[k] + (b[k] - a[k]) * s;
      cmdv[k] = (b[k] - a[k]) * ds / T;
    }
    servo();
  }
  memset(cmdv, 0, sizeof(cmdv));
}

// tool centre point: the tip from where it is to w [mm] in the work's frame, A C to a, c [deg], quintic;
// the joints by the inverse kinematics, their velocities as the difference to the next period's
static void wmove(double T, const double *w, double a, double c) {
  double w0[3], a0 = cmd[MA], c0 = cmd[MC];
  work_cmd(w0);
  long n = lround(T / mc_servo_t);
  for(long i = 1; i <= n; i++) {
    double s, ds, s1, ds1, j[5], j1[5], p[3], p1[3];
    profile(0, (double)i / n, &s, &ds);
    profile(0, (double)(i + 1 > n ? n : i + 1) / n, &s1, &ds1);
    for(int k = 0; k < 3; k++) {
      p[k] = w0[k] + (w[k] - w0[k]) * s, p1[k] = w0[k] + (w[k] - w0[k]) * s1;
    }
    mccore_trt_inv(p, a0 + (a - a0) * s, c0 + (c - c0) * s, j);
    mccore_trt_inv(p1, a0 + (a - a0) * s1, c0 + (c - c0) * s1, j1);
    for(int k = 0; k < NAX; k++) {
      cmdv[k] = (j1[k] - j[k]) / mc_servo_t;
      cmd[k] = j[k];
    }
    servo();
  }
  memset(cmdv, 0, sizeof(cmdv));
}

static void circle(double T, double cx, double cy, double r, double turns) {
  double a0 = atan2(cmd[1] - cy, cmd[0] - cx), w = 2 * M_PI * turns / T;
  long n = lround(T / mc_servo_t);
  for(long i = 1; i <= n; i++) {
    double a = a0 + w * i * mc_servo_t;
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
      while(!mccore_synced()) {
        servo();
      }
    }
    en = v[0] != 0.0;
    if(en) {
      for(int k = 0; k < NAX; k++) {
        cmd[k] = mccore_fb(k);
      }
    }
  } else if((!strcmp(kw, "move") || !strcmp(kw, "tmove")) && sscanf(p, "%lf %lf %lf %lf", v, v + 1, v + 2, v + 3) == 4) {
    double b[MP_NAX] = {v[1], v[2], v[3], cmd[MA], cmd[MC]}, ac[2];
    if(sscanf(p, "%*f %*f %*f %*f %lf %lf", ac, ac + 1) == 2) {
      if(NAX < 5) {
        return -1;
      }
      b[MA] = ac[0], b[MC] = ac[1];
    }
    move(kw[0] == 't', v[0], b);
  } else if(!strcmp(kw, "wmove") && NAX >= 5 && sscanf(p, "%lf %lf %lf %lf %lf %lf", v, v + 1, v + 2, v + 3, v + 4, v + 5) == 6) {
    wmove(v[0], v + 1, v[4], v[5]);
  } else if(!strcmp(kw, "rtcp") && NAX >= 5 && sscanf(p, "%lf %lf %lf", v, v + 1, v + 2) == 3) {
    double w[3];
    work_cmd(w);
    wmove(v[0], w, v[1], v[2]);
  } else if(!strcmp(kw, "trapfrac") && sscanf(p, "%lf", v) == 1) {
    trapfrac = v[0];
  } else if(!strcmp(kw, "circle") && sscanf(p, "%lf %lf %lf %lf %lf", v, v + 1, v + 2, v + 3, v + 4) == 5) {
    circle(v[0], v[1], v[2], v[3], v[4]);
  } else if(!strcmp(kw, "square") && sscanf(p, "%lf %lf %lf %lf", v, v + 1, v + 2, v + 3) == 4) {
    int smooth = sscanf(p, "%*f %*f %*f %*f %lf", v + 4) == 1 && v[4] != 0.0;  // 1: S-curve (quintic) sides
    double a = v[3] / 2, c[4][2] = {{v[1] - a, v[2] - a}, {v[1] + a, v[2] - a}, {v[1] + a, v[2] + a}, {v[1] - a, v[2] + a}};
    for(int i = 0; i <= 4; i++) {
      double b[MP_NAX] = {c[i % 4][0], c[i % 4][1], cmd[2], cmd[MA], cmd[MC]};
      move(!smooth, v[0], b);
    }
  } else if(!strcmp(kw, "drv")) {
    char who[8];
    int n = 0;
    if(sscanf(p, " %7s %n", who, &n) != 1) {
      return -1;
    }
    p[strcspn(p, "\r\n")] = 0;
    for(int k = 0; k < NAX; k++) {
      char nm[2] = {axname[k], 0};
      if(!strcmp(who, "all") || !strcmp(who, nm)) {
        mc_drv[k].cmd(p + n);
        fflush(stdout);
      }
    }
  } else if(!strcmp(kw, "watch")) {
    char who[8], pin[64];
    int k;
    if(sscanf(p, " %7s %63s", who, pin) != 2 || nwatch == NWATCH || (k = mccore_axis(who)) < 0 || k >= NAX) {
      return -1;
    }
    float *w = mc_drv[k].pin(pin);
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
    return mccore_line(line) ? 0 : -1;
  }
  return 0;
}

int main(int argc, char **argv) {
  if(argc < 2) {
    fprintf(stderr, "usage: mill_sim MACHINEFILE [SCRIPT]\n");
    return 2;
  }
  const char *sl = strrchr(argv[0], '/');
  char libpath[1024];
  snprintf(libpath, sizeof(libpath), "%.*s../arm/libstmbl_drive.so", sl ? (int)(sl - argv[0] + 1) : 0, argv[0]);
  if(mccore_load(argv[1], libpath)) {
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
