/*
 * The simulated portal machine with its drives, see portalcore.h.
 */
#include "portalcore.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

pt_drive pt_drv[3];
pt_bus pt_B;
pp_plant pt_P;
double pt_x0 = 0.0, pt_y0 = 0.0;
long pt_ticks = 0;

static char libpath[1024];
static char dir[1024] = ".";
#define NEED_OFF 5  // set the offsets at this exchange: the first reports no position
static int need_off = NEED_OFF;

static void rel_path(char *out, size_t n, const char *p) {
  if(p[0] == '/') {
    snprintf(out, n, "%s", p);
  } else {
    snprintf(out, n, "%s/%s", dir, p);
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

static int drive_open(pt_drive *d, const char *conf, const char *simfile) {
  d->h = dlmopen(LM_ID_NEWLM, libpath, RTLD_NOW | RTLD_LOCAL);
  if(!d->h) {
    fprintf(stderr, "portalcore: dlmopen %s: %s\n", libpath, dlerror());
    return -1;
  }
  d->boot = dlsym(d->h, "stmbl_sim_boot");
  d->tick = dlsym(d->h, "stmbl_sim_tick");
  d->update = dlsym(d->h, "stmbl_sim_update");
  d->pin = dlsym(d->h, "stmbl_sim_pin");
  d->cmd = dlsym(d->h, "stmbl_sim_cmd");
  if(!d->boot || !d->tick || !d->update || !d->pin || !d->cmd) {
    fprintf(stderr, "portalcore: %s lacks the stmbl_sim_* API\n", libpath);
    return -1;
  }
  char *c = read_file(conf);
  if(!c) {
    fprintf(stderr, "portalcore: cannot read %s\n", conf);
    return -1;
  }
  strip_comments(c);
  char extra[4096] = "hv0.sim_ext = 1\n";
  if(simfile) {
    char *s = read_file(simfile);
    if(!s) {
      fprintf(stderr, "portalcore: cannot read %s\n", simfile);
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
  fflush(stdout);  // the boot echoes every config line: keep it off stdout
  int saved = dup(1), null = open(getenv("PT_BOOTLOG") ? getenv("PT_BOOTLOG") : "/dev/null", O_WRONLY | O_CREAT | O_APPEND, 0644);
  dup2(null, 1);
  int rc = d->boot(text, extra);
  d->cmd("debug_level 0");
  dup2(saved, 1);
  close(saved);
  close(null);
  free(text);
  if(rc) {
    fprintf(stderr, "portalcore: drive %s did not boot\n", conf);
    return -1;
  }
  d->sim_pos = d->pin("hv0.sim_pos");
  d->sim_vel = d->pin("hv0.sim_vel");
  d->te_int = d->pin("hv0.sim_te_int");
  d->udc = d->pin("hv0.sim_udc");
  d->p_in = d->pin("pe0.p_in");
  if(!d->sim_pos || !d->sim_vel || !d->te_int || !d->udc || !d->p_in) {
    fprintf(stderr, "portalcore: %s: hv0 sim pins missing\n", conf);
    return -1;
  }
  d->on = 1;
  return 0;
}

void ptcore_home(double X, double Y) {
  memset(pt_P.q, 0, sizeof(pt_P.q));
  memset(pt_P.qd, 0, sizeof(pt_P.qd));
  pt_P.q[PP_X] = (X - pt_x0) / 1000.0;
  pt_P.q[PP_Y] = (Y - pt_y0) / 1000.0;
  need_off = NEED_OFF;
}

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

// the gantry link (portal line glink DELAY [EVERY]): Y1's and Y2's gantry0
// components exchange position error and velocity, each arriving DELAY rt
// cycles later, sampled every EVERY rt cycles (a drive-to-drive CAN link:
// 2 1 or 2 2 = 2.5 kHz frames; relayed by LinuxCNC over smart-serial at
// 1 kHz: about 10 5)
#define GL_RING 64
static struct {
  int on, delay, every, head;
  float *tx_e[2], *tx_v[2], *rx_e[2], *rx_v[2], *ok[2];
  float ring[GL_RING][2][2];  // [tick][side][err, vel]
} gl;

static int glink_open(int delay, int every) {
  int s[2] = {PP_Y1, PP_Y2};
  for(int i = 0; i < 2; i++) {
    pt_drive *d = &pt_drv[s[i]];
    if(!d->on || !(gl.tx_e[i] = d->pin("gantry0.err_tx")) || !(gl.tx_v[i] = d->pin("gantry0.vel_tx")) ||
       !(gl.rx_e[i] = d->pin("gantry0.err_rx")) || !(gl.rx_v[i] = d->pin("gantry0.vel_rx")) || !(gl.ok[i] = d->pin("gantry0.rx_ok"))) {
      fprintf(stderr, "portalcore: glink: drive %s has no gantry0 (load it in its config)\n", i ? "y2" : "y1");
      return 0;
    }
  }
  gl.on = 1, gl.delay = delay < 1 ? 1 : delay >= GL_RING ? GL_RING - 1 : delay;
  gl.every = every < 1 ? 1 : every;
  memset(gl.ring, 0, sizeof(gl.ring));
  return 1;
}

static void glink_step(void) {
  if(!gl.on) {
    return;
  }
  gl.head = (gl.head + 1) % GL_RING;
  for(int i = 0; i < 2; i++) {
    gl.ring[gl.head][i][0] = *gl.tx_e[i], gl.ring[gl.head][i][1] = *gl.tx_v[i];
  }
  if(pt_ticks % gl.every) {  // between samples: hold
    return;
  }
  const int old = (gl.head - gl.delay + GL_RING) % GL_RING;  // what the other side sent DELAY cycles ago
  for(int i = 0; i < 2; i++) {
    *gl.rx_e[i] = gl.ring[old][1 - i][0], *gl.rx_v[i] = gl.ring[old][1 - i][1];
    *gl.ok[i] = 1.0f;
  }
}

int ptcore_line(char *line) {
  char kw[16];
  double v[8];
  int off = 0;
  char *h = strchr(line, '#');
  if(h) {
    *h = 0;
  }
  if(sscanf(line, " %15s%n", kw, &off) != 1) {
    return 1;
  }
  char *p = line + off;
  pp_plant *P = &pt_P;
  if(!strcmp(kw, "drive") && strstr(p, " none")) {  // drive y2 none: an idler rail (a scale only)
    char name[8];
    if(sscanf(p, " %7s", name) != 1 || strcmp(name, "y2")) {
      return 0;
    }
    pt_drv[PP_Y2].on = 0;  // (a loaded drive stays loaded, unused)
    return 1;
  }
  if(!strcmp(kw, "drive")) {  // drive y1|y2|x CONF SIM PERIOD_MM
    char name[8], conf[256], sim[256], cp[1024], sp[1024];
    if(sscanf(p, " %7s %255s %255s %lf", name, conf, sim, v) != 4) {
      return 0;
    }
    int k = !strcmp(name, "y1") ? PP_Y1 : !strcmp(name, "y2") ? PP_Y2 : !strcmp(name, "x") ? PP_XM : -1;
    if(k < 0) {
      return 0;
    }
    rel_path(cp, sizeof(cp), conf);
    rel_path(sp, sizeof(sp), sim);
    pt_drv[k].period = v[0] / 1000.0;
    return drive_open(&pt_drv[k], cp, sp) == 0;
  }
  int n = nums(p, v, 8);
  if(!strcmp(kw, "frame") && n == 6) {  // M J fx fy fpsi zeta: the feet as modes
    P->mf = v[0], P->jf = v[1];
    double m[3] = {v[0], v[0], v[1]};
    for(int k = 0; k < 3; k++) {
      double w = 2 * M_PI * v[2 + k];
      P->kf[k] = m[k] * w * w;
      P->cf[k] = pp_damp(P->kf[k], m[k], v[5]);
    }
  } else if(!strcmp(kw, "rails") && n == 1) {
    P->L = v[0];
  } else if(!strcmp(kw, "racking") && n == 2) {  // K [Nm/rad] ZETA (with the bridge's J)
    P->kr = v[0], P->cr = pp_damp(v[0], P->jb, v[1]);
  } else if(!strcmp(kw, "bridge") && n == 2) {
    P->mb = v[0], P->jb = v[1];
  } else if(!strcmp(kw, "carriage") && n == 2) {
    P->mc = v[0], P->jc = v[1];
  } else if(!strcmp(kw, "head") && n == 6) {  // m J fx fy_end fy_mid zeta
    P->mh = v[0], P->jh = v[1];
    double wx = 2 * M_PI * v[2], we = 2 * M_PI * v[3], wm = 2 * M_PI * v[4];
    P->kx = v[0] * wx * wx, P->ky_end = v[0] * we * we, P->ky_mid = v[0] * wm * wm;
    P->cx = pp_damp(P->kx, v[0], v[5]), P->cy = pp_damp(P->ky_mid, v[0], v[5]);
  } else if(!strcmp(kw, "guidey") && n == 2) {
    P->fy_c = v[0], P->fy_v = v[1];
  } else if(!strcmp(kw, "guidey2") && n == 2) {  // the Y2 side alone (an idler rail)
    P->fy2_c = v[0], P->fy2_v = v[1];
  } else if(!strcmp(kw, "cogging")) {  // cogging y1|y2|x A[N] PERIOD[mm] PHASE[deg]
    char name[8];
    int skip = 0;
    if(sscanf(p, " %7s%n", name, &skip) != 1 || nums(p + skip, v, 8) != 3) {  // the numbers after the name
      return 0;
    }
    int k = !strcmp(name, "y1") ? PP_Y1 : !strcmp(name, "y2") ? PP_Y2 : !strcmp(name, "x") ? PP_XM : -1;
    if(k < 0 || v[1] <= 0.0) {
      return 0;
    }
    P->cog_a[k] = v[0], P->cog_per[k] = v[1] / 1000.0, P->cog_ph[k] = v[2] * M_PI / 180.0;
  } else if(!strcmp(kw, "guidex") && n == 2) {
    P->fx_c = v[0], P->fx_v = v[1];
  } else if(!strcmp(kw, "stroke") && n == 4) {  // ymin ymax xmin xmax [mm, machine]
    P->ymin = (v[0] - pt_y0) / 1000.0, P->ymax = (v[1] - pt_y0) / 1000.0;
    P->xmin = (v[2] - pt_x0) / 1000.0, P->xmax = (v[3] - pt_x0) / 1000.0;
  } else if(!strcmp(kw, "origin") && n == 2) {  // X Y [mm] of the frame's centre
    pt_x0 = v[0], pt_y0 = v[1];
  } else if(!strcmp(kw, "bus") && (n == 3 || n == 5)) {  // C [uF] mains [V rms] Rsrc [ohm] [chopper V, R ohm]
    pt_B.on = 1, pt_B.C = v[0] * 1e-6, pt_B.vrms = v[1], pt_B.rsrc = v[2], pt_B.freq = 50.0;
    pt_B.v_on = n == 5 ? v[3] : 0.0, pt_B.r_ch = n == 5 ? v[4] : 0.0;
    pt_B.v = pt_B.v_max = pt_B.v_min = v[1] * M_SQRT2;  // precharged
  } else if(!strcmp(kw, "glink") && (n == 1 || n == 2)) {  // DELAY [EVERY] [rt cycles]: the Y drives' gantry link
    return glink_open((int)v[0], n == 2 ? (int)v[1] : 1);
  } else if(!strcmp(kw, "glink") && n == 0 && strstr(p, "off")) {
    gl.on = 0;
    for(int i = 0; i < 2; i++) {
      if(gl.ok[i]) {
        *gl.ok[i] = 0.0f;
      }
    }
  } else if(!strcmp(kw, "home") && n == 2) {
    ptcore_home(v[0], v[1]);
  } else {
    return 0;
  }
  return 1;
}

int ptcore_load(const char *file, const char *lib) {
  snprintf(libpath, sizeof(libpath), "%s", lib);
  snprintf(dir, sizeof(dir), "%s", file);
  char *sl = strrchr(dir, '/');
  if(sl) {
    *sl = 0;
  } else {
    strcpy(dir, ".");
  }
  pp_defaults(&pt_P);
  char *text = read_file(file);
  if(!text) {
    fprintf(stderr, "portalcore: cannot read %s\n", file);
    return -1;
  }
  int rc = 0;
  char *save = NULL;
  for(char *line = strtok_r(text, "\n", &save); line && !rc; line = strtok_r(NULL, "\n", &save)) {
    if(!ptcore_line(line)) {
      fprintf(stderr, "portalcore: %s: bad line: %s\n", file, line);
      rc = -1;
    }
  }
  free(text);
  for(int k = 0; k < 3 && !rc; k++) {
    if(!pt_drv[k].on && k != PP_Y2) {
      fprintf(stderr, "portalcore: %s: drive %s missing\n", file, k == PP_XM ? "x" : k ? "y2" : "y1");
      rc = -1;
    }
    pt_drv[k].pd.scale = pt_drv[k].period * 1000.0;
  }
  return rc;
}

double ptcore_scale(int k) {
  double pos, vel;
  pp_motor(&pt_P, k, &pos, &vel);
  return pos * 1000.0 + (k == PP_XM ? pt_x0 : pt_y0);
}

void ptcore_tool(double out[2]) {
  pp_tool(&pt_P, out, NULL);
  out[0] = out[0] * 1000.0 + pt_x0, out[1] = out[1] * 1000.0 + pt_y0;
}

void ptcore_set_cmd(int k, int en, double pos, double vel) {
  pt_drive *d = &pt_drv[k];
  d->pd.enable = en;
  if(need_off) {  // the offset not known yet: hold where the drive is (as LinuxCNC follows the feedback)
    d->pd.pos_cmd = d->pd.pos_fb, d->pd.vel_cmd = 0.0;
    return;
  }
  d->pd.pos_cmd = pos - d->off;
  d->pd.vel_cmd = vel;
}

double ptcore_fb(int k) {  // an idler's: its scale (a second scale on the passive rail)
  return pt_drv[k].on ? pt_drv[k].pd.pos_fb + pt_drv[k].off : ptcore_scale(k);
}

double ptcore_fb_vel(int k) {
  return pt_drv[k].pd.vel_fb;
}

int ptcore_synced(void) {
  return !need_off;
}

static double wrap(double a) {
  a = fmod(a + M_PI, 2.0 * M_PI);
  return a < 0.0 ? a + M_PI : a - M_PI;
}

// the bus over one rt cycle, in the plant's steps
static void bus_step(double p_drives) {
  pt_bus *b = &pt_B;
  double dt = PT_RT / PT_SUB, vpk = b->vrms * M_SQRT2;
  b->p = p_drives;
  for(int s = 0; s < PT_SUB; s++) {
    double t = (pt_ticks * PT_SUB + s) * dt;
    double vr = vpk * fabs(sin(2.0 * M_PI * b->freq * t));
    double i_rect = vr > b->v ? (vr - b->v) / b->rsrc : 0.0;
    double i_load = p_drives / fmax(b->v, 10.0);
    if(b->r_ch > 0.0) {
      b->ch = b->v > b->v_on ? 1 : b->v < b->v_on - 10.0 ? 0 : b->ch;
    }
    double i_ch = b->ch ? b->v / b->r_ch : 0.0;
    b->e_ch += i_ch * b->v * dt;
    b->e_regen += p_drives < 0.0 ? -p_drives * dt : 0.0;
    b->v += (i_rect - i_load - i_ch) / b->C * dt;
    b->v = fmax(b->v, 0.0);
    b->v_max = fmax(b->v_max, b->v), b->v_min = fmin(b->v_min, b->v);
  }
}

void ptcore_tick(int servo) {
  double F[3] = {0}, p_drives = 0.0;
  for(int k = 0; k < 3; k++) {
    pt_drive *d = &pt_drv[k];
    if(!d->on) {  // an idler rail: no motor
      continue;
    }
    double pos, vel, g = 2.0 * M_PI / d->period;
    pp_motor(&pt_P, k, &pos, &vel);
    *d->sim_pos = (float)wrap(pos * g);
    *d->sim_vel = (float)(vel * g);
    *d->te_int = 0.0f;
    if(pt_B.on) {
      *d->udc = (float)pt_B.v;
    }
    if(servo) {
      d->update(&d->pd, (long)(PT_RT * 1e9 + 0.5));
    } else {
      d->tick();
    }
    F[k] = *d->te_int / PT_RT * g;
    p_drives += *d->p_in;
  }
  glink_step();
  if(pt_B.on) {
    bus_step(p_drives);
  }
  if(servo && need_off && --need_off == 0) {  // absolute scales
    for(int k = 0; k < 3; k++) {
      pt_drv[k].off = pt_drv[k].on ? ptcore_scale(k) - pt_drv[k].pd.pos_fb : 0.0;
    }
  }
  for(int s = 0; s < PT_SUB; s++) {
    pp_step(&pt_P, F, PT_RT / PT_SUB);
  }
  pt_ticks++;
}

void ptcore_servo(long period_ns) {
  static long carry = 0;
  long rt = (long)(PT_RT * 1e9 + 0.5);
  carry += period_ns;
  int first = 1;
  while(carry >= rt) {
    carry -= rt;
    ptcore_tick(first);
    first = 0;
  }
}
