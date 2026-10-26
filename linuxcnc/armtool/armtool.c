/*
 * armtool: the pneumatic tool changer's monitor under LinuxCNC
 * (shared/robot/tool.h, rb_changer). Machine protection, not a safety
 * function.
 *
 *   loadrt armtool model=/path/model.arm tools=/path/tools.txt
 *   addf armtool.0 servo-thread
 *
 * It drives the lock valve from the program's request, watches the lock
 * sensors, the tool's presence and coded ID, knows the mounted tool and
 * hands its mass to armcollide and its bodies to armguard; the M6 routine
 * (hwtest/arcmate-sim/toolchange.ngc) reads a tool's dock from it.
 *
 * Pins:
 *   armtool.0.enable         in   machine on (motion.motion-enabled)
 *   armtool.0.lock           in   lock request (M64 / M65 P1)
 *   armtool.0.expect         in   the tool the next lock should take (M68 E4)
 *   armtool.0.locked, .unlocked, .present  in  the changer's sensors
 *   armtool.0.id             in   (s32) the coded tool ID
 *   armtool.0.J.pos          in   joint angles [deg, DH theta]: the flange
 *                                 pose (unlocking only in the dock)
 *   armtool.0.kins-tool      in   (s32) armkins.tool: the TCP in use
 *   armtool.0.valve          out  the lock valve (never released with a tool
 *                                 on away from its dock)
 *   armtool.0.mounted        out  (s32) the tool on the flange (0 none)
 *   armtool.0.tool-mass, .tool-cx/-cy/-cz  out  its mass -> armcollide
 *   armtool.0.dock-select    in   a tool ID (M68 E5): its dock below
 *   armtool.0.dock-0 .. -11  out  dock pose x y z a b c [mm, deg], the bare
 *                                 flange's approach point x y z, the
 *                                 slide-out point x y z [mm] (-> motion's
 *                                 analog inputs, read with M66 E<n> L0)
 *   armtool.0.fault          out  latched until reset; .reason: 1 lock
 *   armtool.0.reason         out  sensors timeout, 2 wrong tool, 3 tool
 *                                 lost, 4 unlock away from the dock, 5
 *                                 moving with the kinematics on another
 *                                 tool's TCP
 *   armtool.0.reset          in   rising edge: clears the fault (the
 *                                 mounted tool is then what the sensors show)
 *   armtool.0.chain-in       in   the estop chain ...
 *   armtool.0.chain-out      out  ... and no fault
 */
#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "kin.h"
#include "tool.h"

MODULE_AUTHOR("stmbl");
MODULE_DESCRIPTION("pneumatic tool changer monitor for a serial arm");
MODULE_LICENSE("GPL");

static char *model = "";
RTAPI_MP_STRING(model, "file with the arm's dh lines");
static char *tools = "";
RTAPI_MP_STRING(tools, "tool table (shared/robot/tool.h)");

#define PREFIX "armtool.0"
#define D2R (M_PI / 180.0)
#define CH_KINS 5

typedef struct {
  hal_float_t *pos[RB_MAXJ];
  hal_bit_t *enable, *lock, *s_locked, *s_unlocked, *s_present, *valve, *fault, *reset, *chain_in, *chain_out;
  hal_float_t *expect, *dock_sel, *dock[12], *tmass, *tc[3];
  hal_s32_t *id, *kins, *mounted, *reason;
  int last_reset, kfault;
  double qprev[RB_MAXJ], t_mov;
  int moving;
} pins_t;

static int comp_id;
static pins_t *pins;
static rb_model mdl;
static rb_tools tt;
static rb_changer chg;

static void update(void *arg, long period) {
  pins_t *p = arg;
  double q[RB_MAXJ], pf[3], Rf[3][3], dt = period * 1e-9;
  int en = *p->enable;
  for(int j = 0; j < mdl.n; j++) {
    q[j] = *p->pos[j] * D2R - mdl.link[j].theta0;
  }
  p->t_mov += dt;
  if(p->t_mov >= 0.05) {  // moving: a joint over 0.1 deg in 50 ms (not the settling at enable)
    p->moving = 0;
    for(int j = 0; j < mdl.n; j++) {
      p->moving |= fabs(*p->pos[j] - p->qprev[j]) > 0.1;
      p->qprev[j] = *p->pos[j];
    }
    p->t_mov = 0.0;
  }
  int moving = p->moving;
  rb_pose(&mdl, q, pf, Rf);
  chg.lock_req = *p->lock;
  chg.expect = (int)lrint(*p->expect);
  chg.s_locked = *p->s_locked;
  chg.s_unlocked = *p->s_unlocked;
  chg.s_present = *p->s_present;
  chg.s_id = *p->id;
  if(*p->reset && !p->last_reset) {
    rb_changer_reset(&chg);
    p->kfault = 0;
  }
  p->last_reset = *p->reset;
  rb_changer_step(&chg, &tt, en, pf, Rf, dt);
  if(en && moving && *p->kins && *p->kins != chg.mounted && !p->kfault && !chg.fault) {
    p->kfault = 1;  // moving on the TCP of a tool that is not on (standing
  }                 // still it may be put right: after a lost tool, M68 E3 Q0)
  const rb_tool *t = rb_tool_get(&tt, chg.mounted);
  *p->valve = chg.valve;
  *p->mounted = chg.mounted;
  *p->tmass = chg.mounted ? t->m : 0.0;
  for(int k = 0; k < 3; k++) {
    *p->tc[k] = t->c[k];
  }
  const rb_tool *d = rb_tool_get(&tt, (int)lrint(*p->dock_sel));
  double rpy[3];
  rb_rpy_from_R(d->dock_R, rpy);
  for(int k = 0; k < 3; k++) {
    *p->dock[k] = d->dock_p[k] * 1000.0;
    *p->dock[3 + k] = rpy[k] / D2R;
    *p->dock[6 + k] = (d->dock_p[k] + d->approach[k]) * 1000.0;
    *p->dock[9 + k] = (d->dock_p[k] + d->slide[k]) * 1000.0;
  }
  int f = chg.fault ? chg.fault : (p->kfault ? CH_KINS : 0);
  *p->fault = f != 0;
  *p->reason = f;
  *p->chain_out = *p->chain_in && !f;
}

static int read_dh(const char *path) {
  FILE *f = fopen(path, "r");
  char line[512];
  if(!f) {
    return -1;
  }
  while(fgets(line, sizeof(line), f)) {
    int j;
    double a, al, d, t0;
    if(sscanf(line, " dh %d %lf %lf %lf %lf", &j, &a, &al, &d, &t0) == 5 && j >= 1 && j <= RB_MAXJ) {
      mdl.link[j - 1].a = a, mdl.link[j - 1].alpha = al * D2R, mdl.link[j - 1].d = d;
      mdl.link[j - 1].theta0 = t0 * D2R;
      mdl.n = j > mdl.n ? j : mdl.n;
    }
  }
  fclose(f);
  return mdl.n == 6 ? 0 : -1;
}

static int export_pins(void) {
  int r = 0;
  for(int j = 0; j < mdl.n && !r; j++) {
    r |= hal_pin_float_newf(HAL_IN, &pins->pos[j], comp_id, PREFIX ".%d.pos", j);
  }
  r |= hal_pin_bit_newf(HAL_IN, &pins->enable, comp_id, PREFIX ".enable");
  r |= hal_pin_bit_newf(HAL_IN, &pins->lock, comp_id, PREFIX ".lock");
  r |= hal_pin_float_newf(HAL_IN, &pins->expect, comp_id, PREFIX ".expect");
  r |= hal_pin_bit_newf(HAL_IN, &pins->s_locked, comp_id, PREFIX ".locked");
  r |= hal_pin_bit_newf(HAL_IN, &pins->s_unlocked, comp_id, PREFIX ".unlocked");
  r |= hal_pin_bit_newf(HAL_IN, &pins->s_present, comp_id, PREFIX ".present");
  r |= hal_pin_s32_newf(HAL_IN, &pins->id, comp_id, PREFIX ".id");
  r |= hal_pin_s32_newf(HAL_IN, &pins->kins, comp_id, PREFIX ".kins-tool");
  r |= hal_pin_bit_newf(HAL_OUT, &pins->valve, comp_id, PREFIX ".valve");
  r |= hal_pin_s32_newf(HAL_OUT, &pins->mounted, comp_id, PREFIX ".mounted");
  r |= hal_pin_float_newf(HAL_OUT, &pins->tmass, comp_id, PREFIX ".tool-mass");
  static const char *cn[3] = {"cx", "cy", "cz"};
  for(int k = 0; k < 3 && !r; k++) {
    r |= hal_pin_float_newf(HAL_OUT, &pins->tc[k], comp_id, PREFIX ".tool-%s", cn[k]);
  }
  r |= hal_pin_float_newf(HAL_IN, &pins->dock_sel, comp_id, PREFIX ".dock-select");
  for(int k = 0; k < 12 && !r; k++) {
    r |= hal_pin_float_newf(HAL_OUT, &pins->dock[k], comp_id, PREFIX ".dock-%d", k);
  }
  r |= hal_pin_bit_newf(HAL_OUT, &pins->fault, comp_id, PREFIX ".fault");
  r |= hal_pin_s32_newf(HAL_OUT, &pins->reason, comp_id, PREFIX ".reason");
  r |= hal_pin_bit_newf(HAL_IN, &pins->reset, comp_id, PREFIX ".reset");
  r |= hal_pin_bit_newf(HAL_IN, &pins->chain_in, comp_id, PREFIX ".chain-in");
  r |= hal_pin_bit_newf(HAL_OUT, &pins->chain_out, comp_id, PREFIX ".chain-out");
  return r;
}

int rtapi_app_main(void) {
  char err[256] = "";
  comp_id = hal_init("armtool");
  if(comp_id < 0) {
    return comp_id;
  }
  rb_tools_init(&tt);
  rb_changer_init(&chg);
  if(!model[0] || read_dh(model) || !tools[0] || rb_tools_file(&tt, tools, err, sizeof(err))) {
    rtapi_print_msg(RTAPI_MSG_ERR, "armtool: model=<dh file> tools=<tool table> needed (%s)\n", err);
    hal_exit(comp_id);
    return -EINVAL;
  }
  pins = hal_malloc(sizeof(pins_t));
  if(!pins) {
    hal_exit(comp_id);
    return -ENOMEM;
  }
  memset(pins, 0, sizeof(*pins));
  if(export_pins() || hal_export_funct(PREFIX, update, pins, 1, 0, comp_id)) {
    rtapi_print_msg(RTAPI_MSG_ERR, "armtool: cannot export pins / function\n");
    hal_exit(comp_id);
    return -EINVAL;
  }
  hal_ready(comp_id);
  return 0;
}

void rtapi_app_exit(void) {
  hal_exit(comp_id);
}
