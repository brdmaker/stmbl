/*
 * portal_sim: three simulated stmbl drives on a simulated portal machine
 * (portalcore.c, pplant.c), a script front end that plays LinuxCNC: every
 * 1 ms it sends each drive its command over the simulated smart-serial
 * link (gantry: Y1 and Y2 get the same Y).
 *
 * usage: portal_sim PORTALFILE [SCRIPT]   (script from stdin without SCRIPT)
 *
 * PORTALFILE (mm for machine positions, else SI):
 *   drive y1|y2|x CONF SIM PERIOD   drive config, sim-only lines, electrical
 *                                   period [mm] (a revolution of the drive)
 *   origin X Y                      machine X, Y [mm] of the frame's centre
 *   stroke YMIN YMAX XMIN XMAX      end stops [mm, machine]
 *   frame M J FX FY FPSI ZETA       the machine's base [kg, kg m^2] on its
 *                                   feet: their modes [Hz] and damping ratio
 *                                   (FX 0: free, no feet)
 *   rails L                         rail spacing [m]
 *   racking K ZETA                  the bridge's yaw on the guide blocks:
 *                                   their moment stiffness [Nm/rad], damping
 *                                   ratio (after bridge)
 *   bridge M J / carriage M J       [kg, kg m^2]
 *   head M J FX FY_END FY_MID ZETA  the tool head on the carriage: modes in
 *                                   x and y (at the rails, at mid-span) [Hz]
 *   guidey FC FV / guidex FC FV     guide friction: Coulomb [N] (per Y side),
 *                                   viscous [N s/m]; guidey2 FC FV: the Y2
 *                                   side alone (an idler rail)
 *   drive y2 none                   one Y motor: Y2 is an idler rail with a
 *                                   passive scale (y2 reads it)
 *   cogging y1|y2|x A PERIOD PHASE  iron-core cogging: -A sin(2pi s/PERIOD +
 *                                   PHASE) [N, mm, deg] at the scale position s
 *   bus C VRMS RSRC [VON RCH]       a shared DC bus instead of the fixed sim.txt
 *                                   voltage: capacitance [uF], mains [V rms]
 *                                   through a diode bridge with source
 *                                   resistance [ohm]; a brake chopper that
 *                                   switches on above VON [V] into RCH [ohm]
 *   glink DELAY [EVERY] | glink off the Y drives' gantry link: their gantry0
 *                                   components exchange position error and
 *                                   velocity, arriving DELAY rt cycles (200 us)
 *                                   later, sampled every EVERY cycles
 *                                   (shared/comps/gantry.c)
 *   home X Y                        start position [mm]
 *
 * Script commands (plus the portal lines above):
 *   run T                           simulate T seconds
 *   lcnc EN                         enable the drives (LinuxCNC's amp enable)
 *   move T X Y                      smooth move (quintic) to X Y [mm] in T s
 *   tmove T X Y                     the same with LinuxCNC's trapezoid (full
 *                                   acceleration over trapfrac of T at each end)
 *   trapfrac F                      default 0.25
 *   circle T CX CY R TURNS          circle(s) from the current position's
 *                                   angle about CX CY, radius R [mm], at
 *                                   constant speed (enter it first)
 *   drv y1|y2|x|all LINE            a console line to a drive (pid0.pos_bw = 60)
 *   mark                            clear the maxima
 *   print NAME...                   portal.NAME = value; NAME: x y1 y2 (scale
 *                                   feedback [mm]), cx cy (command [mm]),
 *                                   tx ty (tool point [mm]), etool (|tool -
 *                                   command| [um]), econt (the tool's distance
 *                                   from the commanded path, 50 ms ago [um]), ferr_x ferr_y1 ferr_y2
 *                                   (command - scale [um]), rack (yaw of the
 *                                   bridge [urad]), frame_x frame_y [um],
 *                                   frame_rot [urad], feet_x feet_y [N],
 *                                   cur_x cur_y1 cur_y2 [A], fault, hits,
 *                                   jammed (racked > 2 mrad, it would bind),
 *                                   mom_x mom_y [kg m/s], energy [J], t [s],
 *                                   bus bus_max bus_min [V] (since mark),
 *                                   comp_delay (the conditioning's mean delay
 *                                   [ms]), e_chopper e_regen [J], fault_code (the
 *                                   first faulted drive's, common.h);
 *                                   with a "max_" prefix: the largest |value|
 *                                   since mark (econt, etool, ferr_*, rack, frame_*,
 *                                   feet_*, cur_*, hx hy [um] head deflection)
 *   log FILE                        log every 1 ms: t cx cy tx ty x y1 y2
 *                                   cur_x cur_y1 cur_y2 frame_x frame_y rack
 *   log off
 *   watch y1|y2|x PIN               also log a drive pin (after the columns)
 *   replay FILE                     play a LinuxCNC recording (hwtest/portal-sim
 *                                   halsampler file) as the planner's commands
 *   comp shaper|defl|shrink 0|1     command conditioning (pcomp.h) between the
 *                                   planner and the drives: input shaper at the
 *                                   head's modes, head deflection feedforward,
 *                                   the shaper's shrink compensated
 *   comp jerk MS                    a moving average of MS ms after the shaper
 *   comp servo TAU2                 the drives' steady lag per acceleration
 *                                   [s^2] fed forward (0: off)
 *   comp model FX FY_END FY_MID ZETA   its model (default: the plant's own)
 */
#include "pcomp.h"
#include "portalcore.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SERVO 0.001

static double cmd[2], cmdv[2];  // X, Y [mm, mm/s]: the path wanted (the planner's)
static pc_t pc;                 // command conditioning (comp), between it and the drives
static int comp_on = 0;
static int en = 0;
static double trapfrac = 0.25;
static FILE *logfile = NULL;
static double tnow = 0.0;
// drive pins appended to the log (watch)
#define NWATCH 16
static float *watch[NWATCH];
static int nwatch = 0;

// the commands and the tool points of the last HIST ms: the path the tool
// should be on, and where it was
#define HIST 400
#define LAG 50  // the tool point judged is LAG ms old: the path around it, both sides
static double hist[HIST][2], thist[HIST][2];
static int hist_n = 0, hist_head = 0;

// contour error: the distance of the tool point of LAG ms ago from the
// commanded path (the polyline of the commands before and after it) [um];
// lag or lead along the path doesn't count (the drives run a command over
// the servo period that follows, so the tool leads the logged command)
static double contour_error(void) {
  double best = 1e30;
  if(hist_n <= LAG) {
    return 0.0;
  }
  const double *t = thist[(hist_head - 1 - LAG + HIST) % HIST];
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

enum { M_ECONT, M_ETOOL, M_FX, M_FY1, M_FY2, M_RACK, M_FRX, M_FRY, M_FRR, M_FTX, M_FTY, M_CX, M_CY1, M_CY2, M_HX, M_HY, NMAX };
static const char *maxname[NMAX] = {"econt", "etool", "ferr_x", "ferr_y1", "ferr_y2", "rack", "frame_x", "frame_y", "frame_rot",
                                    "feet_x", "feet_y", "cur_x", "cur_y1", "cur_y2", "hx", "hy"};
static double mx[NMAX];
static int jammed = 0;  // the bridge racked more than 2 mrad: on a real machine it binds

static double value(const char *name) {
  pp_plant *P = &pt_P;
  double t[2], f[2], m[2];
  ptcore_tool(t);
  pp_feet(P, f);
  if(!strcmp(name, "x")) return ptcore_fb(PP_XM);
  if(!strcmp(name, "y1")) return ptcore_fb(PP_Y1);
  if(!strcmp(name, "y2")) return ptcore_fb(PP_Y2);
  if(!strcmp(name, "cx")) return cmd[0];
  if(!strcmp(name, "cy")) return cmd[1];
  if(!strcmp(name, "tx")) return t[0];
  if(!strcmp(name, "ty")) return t[1];
  if(!strcmp(name, "etool")) return hypot(t[0] - cmd[0], t[1] - cmd[1]) * 1000.0;
  if(!strcmp(name, "econt")) return contour_error();
  if(!strcmp(name, "ferr_x")) return (cmd[0] - ptcore_fb(PP_XM)) * 1000.0;
  if(!strcmp(name, "ferr_y1")) return (cmd[1] - ptcore_fb(PP_Y1)) * 1000.0;
  if(!strcmp(name, "ferr_y2")) return (cmd[1] - ptcore_fb(PP_Y2)) * 1000.0;
  if(!strcmp(name, "rack")) return P->q[PP_TH] * 1e6;
  if(!strcmp(name, "frame_x")) return P->q[PP_XF] * 1e6;
  if(!strcmp(name, "frame_y")) return P->q[PP_YF] * 1e6;
  if(!strcmp(name, "frame_rot")) return P->q[PP_PSI] * 1e6;
  if(!strcmp(name, "feet_x")) return f[0];
  if(!strcmp(name, "feet_y")) return f[1];
  if(!strcmp(name, "cur_x")) return pt_drv[PP_XM].pd.current;
  if(!strcmp(name, "cur_y1")) return pt_drv[PP_Y1].pd.current;
  if(!strcmp(name, "cur_y2")) return pt_drv[PP_Y2].pd.current;
  if(!strcmp(name, "hx")) return P->q[PP_HX] * 1e6;
  if(!strcmp(name, "hy")) return P->q[PP_HY] * 1e6;
  if(!strcmp(name, "fault")) return pt_drv[0].pd.fault || pt_drv[1].pd.fault || pt_drv[2].pd.fault;
  if(!strcmp(name, "hits")) return P->stop_hits[0] + P->stop_hits[2];
  if(!strcmp(name, "jammed")) return jammed;
  pp_momentum(P, m);
  if(!strcmp(name, "mom_x")) return m[0];
  if(!strcmp(name, "mom_y")) return m[1];
  if(!strcmp(name, "energy")) return pp_energy(P);
  if(!strcmp(name, "t")) return tnow;
  if(!strcmp(name, "comp_delay")) return comp_on ? pc.delay * 1000.0 : 0.0;
  if(!strcmp(name, "bus")) return pt_B.v;
  if(!strcmp(name, "bus_max")) return pt_B.v_max;
  if(!strcmp(name, "bus_min")) return pt_B.v_min;
  if(!strcmp(name, "e_chopper")) return pt_B.e_ch;
  if(!strcmp(name, "e_regen")) return pt_B.e_regen;
  if(!strcmp(name, "fault_code")) {  // the first drive in fault: its fault0.fault (common.h)
    for(int k = 0; k < 3; k++) {
      float *f = pt_drv[k].pin ? pt_drv[k].pin("fault0.fault") : NULL;  // no y2 drive: an idler
      if(f && *f != 0.0f) return *f;
    }
    return 0;
  }
  return NAN;
}

static void servo(void) {
  const double *xs = cmd, *vs = cmdv;
  if(comp_on) {
    pc.vin[0] = cmdv[0], pc.vin[1] = cmdv[1];
    pc_step(&pc, cmd);
    xs = pc.out, vs = pc.vout;
  }
  ptcore_set_cmd(PP_XM, en, xs[0], vs[0]);
  ptcore_set_cmd(PP_Y1, en, xs[1], vs[1]);
  ptcore_set_cmd(PP_Y2, en, xs[1], vs[1]);
  ptcore_servo((long)(SERVO * 1e9 + 0.5));
  tnow += SERVO;
  hist[hist_head][0] = cmd[0], hist[hist_head][1] = cmd[1];
  ptcore_tool(thist[hist_head]);
  hist_head = (hist_head + 1) % HIST;
  hist_n = hist_n < HIST ? hist_n + 1 : HIST;
  if(!ptcore_synced()) {  // hold where the machine is until the scales are read
    cmd[0] = ptcore_scale(PP_XM), cmd[1] = 0.5 * (ptcore_scale(PP_Y1) + ptcore_scale(PP_Y2));
    return;
  }
  if(fabs(pt_P.q[PP_TH]) > 2e-3 && !jammed) {
    jammed = 1;
    printf("# t %.3f: the bridge racked %.1f mrad: jammed\n", tnow, pt_P.q[PP_TH] * 1e3);
  }
  for(int i = 0; i < NMAX; i++) {
    double v = fabs(value(maxname[i]));
    mx[i] = v > mx[i] ? v : mx[i];
  }
  if(logfile) {
    double t[2];
    ptcore_tool(t);
    fprintf(logfile, "%.4f %.5f %.5f %.5f %.5f %.5f %.5f %.5f %.3f %.3f %.3f %.3f %.3f %.3f", tnow, cmd[0], cmd[1], t[0], t[1],
            ptcore_fb(PP_XM), ptcore_fb(PP_Y1), ptcore_fb(PP_Y2), value("cur_x"), value("cur_y1"), value("cur_y2"),
            value("frame_x"), value("frame_y"), value("rack"));
    for(int i = 0; i < nwatch; i++) {
      fprintf(logfile, " %.6g", *watch[i]);
    }
    fprintf(logfile, "\n");
  }
}

static void run(double T) {
  for(long i = lround(T / SERVO); i > 0; i--) {
    servo();
  }
}

// s(u), ds/du of a quintic (smooth) or a trapezoid (acceleration over trapfrac)
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

static void move(int trap, double T, double X, double Y) {
  double a[2] = {cmd[0], cmd[1]}, b[2] = {X, Y};
  long n = lround(T / SERVO);
  for(long i = 1; i <= n; i++) {
    double s, ds;
    profile(trap, (double)i / n, &s, &ds);
    for(int k = 0; k < 2; k++) {
      cmd[k] = a[k] + (b[k] - a[k]) * s;
      cmdv[k] = (b[k] - a[k]) * ds / T;
    }
    servo();
  }
  cmdv[0] = cmdv[1] = 0.0;
}

static void circle(double T, double cx, double cy, double r, double turns) {
  double a0 = atan2(cmd[1] - cy, cmd[0] - cx), w = 2 * M_PI * turns / T;
  long n = lround(T / SERVO);
  for(long i = 1; i <= n; i++) {
    double a = a0 + w * i * SERVO;
    cmd[0] = cx + r * cos(a), cmd[1] = cy + r * sin(a);
    cmdv[0] = -r * w * sin(a), cmdv[1] = r * w * cos(a);
    servo();
  }
  cmdv[0] = cmdv[1] = 0.0;
}

// play a LinuxCNC recording (hwtest/portal-sim's halsampler file: the
// sample number, then X and Y commanded, ..., the motion type last) as the
// planner's commands, from 0.2 s before it moves to 0.5 s after
static int replay(const char *fn) {
  FILE *f = fopen(fn, "r");
  if(!f) {
    return -1;
  }
  static double (*xy)[2];
  static int *mt;
  int n = 0, cap = 0;
  char line[1024];
  while(fgets(line, sizeof(line), f)) {
    double v[32];
    int k = 0;
    char *p = line, *end;
    for(; k < 32; p = end) {
      v[k] = strtod(p, &end);
      if(end == p) {
        break;
      }
      k++;
    }
    if(k < 22) {
      continue;  // halsampler stopped mid-line
    }
    if(n == cap) {
      cap = cap ? cap * 2 : 65536;
      xy = realloc(xy, cap * sizeof(*xy));
      mt = realloc(mt, cap * sizeof(*mt));
    }
    xy[n][0] = v[1], xy[n][1] = v[2], mt[n] = (int)v[k - 1];
    n++;
  }
  fclose(f);
  int first = 0, last = n - 1;
  while(first < n && mt[first] == 0) {
    first++;
  }
  while(last > first && mt[last] == 0) {
    last--;
  }
  first = first > 200 ? first - 200 : 0;
  last = last + 500 < n ? last + 500 : n - 1;
  move(0, 0.5, xy[first][0], xy[first][1]);
  run(0.2);
  printf("# replay %s: samples %d..%d\n", fn, first, last);
  for(int i = first; i <= last; i++) {
    for(int k = 0; k < 2; k++) {
      cmdv[k] = (xy[i][k] - cmd[k]) / SERVO;
      cmd[k] = xy[i][k];
    }
    servo();
  }
  cmdv[0] = cmdv[1] = 0.0;
  return 0;
}

static void print(char *names) {
  for(char *n = strtok(names, " \t\n"); n; n = strtok(NULL, " \t\n")) {
    double v = NAN;
    if(!strncmp(n, "max_", 4)) {
      for(int i = 0; i < NMAX; i++) {
        if(!strcmp(n + 4, maxname[i])) {
          v = mx[i];
        }
      }
    } else {
      v = value(n);
    }
    if(isnan(v)) {
      printf("ERROR: unknown value %s\n", n);
    } else {
      printf("portal.%s = %.6g\n", n, v);
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
  double v[5];
  if(!strcmp(kw, "run") && sscanf(p, "%lf", v) == 1) {
    run(v[0]);
  } else if(!strcmp(kw, "lcnc") && sscanf(p, "%lf", v) == 1) {
    en = v[0] != 0.0;
    if(en) {  // LinuxCNC enables where the machine is
      cmd[0] = ptcore_fb(PP_XM), cmd[1] = 0.5 * (ptcore_fb(PP_Y1) + ptcore_fb(PP_Y2));
    }
  } else if((!strcmp(kw, "move") || !strcmp(kw, "tmove")) && sscanf(p, "%lf %lf %lf", v, v + 1, v + 2) == 3) {
    move(kw[0] == 't', v[0], v[1], v[2]);
  } else if(!strcmp(kw, "trapfrac") && sscanf(p, "%lf", v) == 1) {
    trapfrac = v[0];
  } else if(!strcmp(kw, "circle") && sscanf(p, "%lf %lf %lf %lf %lf", v, v + 1, v + 2, v + 3, v + 4) == 5) {
    circle(v[0], v[1], v[2], v[3], v[4]);
  } else if(!strcmp(kw, "drv")) {
    char who[8];
    int n = 0;
    if(sscanf(p, " %7s %n", who, &n) != 1) {
      return -1;
    }
    p[strcspn(p, "\r\n")] = 0;
    for(int k = 0; k < 3; k++) {
      const char *nm = k == PP_XM ? "x" : k == PP_Y1 ? "y1" : "y2";
      if(pt_drv[k].on && (!strcmp(who, "all") || !strcmp(who, nm))) {
        pt_drv[k].cmd(p + n);
        fflush(stdout);
      }
    }
  } else if(!strcmp(kw, "replay")) {
    char fn[512];
    if(sscanf(p, " %511s", fn) != 1 || replay(fn) < 0) {
      return -1;
    }
  } else if(!strcmp(kw, "comp")) {  // comp shaper|defl 0|1, comp model FX FY_END FY_MID ZETA
    char what[16];
    double m[4];
    if(sscanf(p, " servo %lf", m) == 1) {  // the drives' lag tau2 [s^2] (0: off)
      pc.m.servo = m[0], pc.servo = m[0] != 0.0;
    } else if(sscanf(p, " jerk %lf", m) == 1) {  // moving average [ms]
      pc.jerk = (int)lround(m[0] / (SERVO * 1000.0));
    } else if(sscanf(p, " model %lf %lf %lf %lf", m, m + 1, m + 2, m + 3) == 4) {
      pc.m.fx = m[0], pc.m.fy_end = m[1], pc.m.fy_mid = m[2], pc.m.zeta = m[3];
    } else if(sscanf(p, " %15s %lf", what, m) == 2 && (!strcmp(what, "shaper") || !strcmp(what, "defl") || !strcmp(what, "shrink"))) {
      *(!strcmp(what, "shaper") ? &pc.shaper : !strcmp(what, "defl") ? &pc.defl : &pc.shrink) = m[0] != 0.0;
    } else {
      return -1;
    }
    int on = pc.shaper || pc.defl || pc.servo || pc.jerk > 1;
    if(on && !comp_on) {  // start from where the command is
      pc.primed = 0;
    }
    comp_on = on;
  } else if(!strcmp(kw, "watch")) {  // watch y1|y2|x PIN: a drive pin, logged after the columns
    char who[8], pin[64];
    if(sscanf(p, " %7s %63s", who, pin) != 2 || nwatch == NWATCH) {
      return -1;
    }
    int k = !strcmp(who, "x") ? PP_XM : !strcmp(who, "y1") ? PP_Y1 : !strcmp(who, "y2") ? PP_Y2 : -1;
    float *v = k >= 0 && pt_drv[k].on ? pt_drv[k].pin(pin) : NULL;
    if(!v) {
      return -1;
    }
    watch[nwatch++] = v;
  } else if(!strcmp(kw, "mark")) {
    memset(mx, 0, sizeof(mx));
    pt_B.v_max = pt_B.v_min = pt_B.v;
    pt_B.e_ch = pt_B.e_regen = 0.0;
  } else if(!strcmp(kw, "print")) {
    print(p);
  } else if(!strcmp(kw, "log")) {
    char fn[512];
    if(logfile) {
      fclose(logfile);
      logfile = NULL;
    }
    if(sscanf(p, " %511s", fn) == 1 && strcmp(fn, "off")) {
      logfile = fopen(fn, "w");
      if(!logfile) {
        return -1;
      }
    }
  } else {
    return ptcore_line(line) ? 0 : -1;
  }
  return 0;
}

int main(int argc, char **argv) {
  if(argc < 2) {
    fprintf(stderr, "usage: portal_sim PORTALFILE [SCRIPT]\n");
    return 2;
  }
  char lib[1024];
  const char *sl = strrchr(argv[0], '/');
  snprintf(lib, sizeof(lib), "%.*s../arm/libstmbl_drive.so", sl ? (int)(sl - argv[0] + 1) : 0, argv[0]);
  if(ptcore_load(argv[1], lib)) {
    return 1;
  }
  {  // the conditioning's model: the plant's own values to start with
    pp_plant *P = &pt_P;
    pc_model m = {sqrt(P->kx / P->mh) / (2 * M_PI), sqrt(P->ky_end / P->mh) / (2 * M_PI), sqrt(P->ky_mid / P->mh) / (2 * M_PI),
                  P->cy / (2.0 * sqrt(P->ky_mid * P->mh)), P->L, pt_x0, 0.0};
    pc_init(&pc, &m, SERVO);
  }
  FILE *in = argc > 2 ? fopen(argv[2], "r") : stdin;
  if(!in) {
    fprintf(stderr, "portal_sim: cannot read %s\n", argv[2]);
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
