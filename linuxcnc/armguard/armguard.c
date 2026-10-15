/*
 * armguard: collision reaction for a serial arm under LinuxCNC. Between
 * motion and the drives: normally it passes motion's joint commands to the
 * drives and the drives' feedback to motion. On a collision
 * (armcollide.0.collision) it takes the joints over (shared/robot/guard.c):
 * stop, back off 20 mm (back along the path, or along the contact force at
 * rest) to free a tool pressed against the obstacle, hold, then request
 * estop. While it commands the drives it feeds motion its own command as
 * feedback (no following error); the estop then aborts motion, which
 * resyncs to the true feedback when it is enabled again.
 *
 *   loadrt armguard model=/path/model.arm cell=/path/cell.txt
 *   addf armguard.0 servo-thread       (after motion-controller, before the
 *                                       drives' write)
 *
 * model: the dh lines (as armcollide's model file); cell: geometry and the
 * guard's settings (backoff MM T, astop J DEG_S2), see guard.h / geom.h.
 *
 * Pins, per joint J (LinuxCNC joints, genserkins' DH theta, degrees):
 *   armguard.0.J.motion-cmd  in   joint.J.motor-pos-cmd
 *   armguard.0.J.motion-vel  in   joint.J.vel-cmd
 *   armguard.0.J.drive-fb    in   the drive's position feedback
 *   armguard.0.J.residual    in   armcollide.0.J.residual (force direction)
 *   armguard.0.J.cmd         out  -> the drive's position command
 *   armguard.0.J.vel         out  -> the drive's velocity command
 *   armguard.0.J.fb          out  -> joint.J.motor-pos-fb
 *   armguard.0.collision     in   armcollide.0.collision
 *   armguard.0.enable        in   motion.motion-enabled
 *   armguard.0.reset         in   the estop reset (rising edge)
 *   armguard.0.estop         out  estop request, latched until reset
 *   armguard.0.active        out  the guard commands the drives
 *   armguard.0.state         out  0 pass, 1 stop, 2 backoff, 3 hold, 4 done
 *   armguard.0.reason        out  1 collision, 2 clearance (prevention)
 * Collision prevention (cell: prevent MM, the arm's bodies, obstacles):
 * every period, before motion's command is passed on, the guard checks
 * that the arm could still stop (its own stop) without coming within the
 * margin of an obstacle or of itself; if not, and it is approaching, it
 * stops now, then estop (a protective stop, no backoff).
 *   armguard.0.prevent-enable in  prevention on (default on if prevent > 0)
 *   armguard.0.clearance     out  the arm's clearance at motion's command [mm]
 *   armguard.0.clearance-stop out the smallest along the stop path [mm]
 *   armguard.0.tool          in   (s32) the tool on the tool changer now:
 *                                 its bodies (`body ... tool N`) count
 */
#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"

#include <math.h>
#include <string.h>

#include "geom.h"
#include "guard.h"

MODULE_AUTHOR("stmbl");
MODULE_DESCRIPTION("collision reaction (stop, back off, estop) for a serial arm");
MODULE_LICENSE("GPL");

static char *model = "";
RTAPI_MP_STRING(model, "arm model file (dh lines)");
static char *cell = "";
RTAPI_MP_STRING(cell, "cell file: geometry and guard settings");

#define PREFIX "armguard.0"
#define D2R (M_PI / 180.0)

typedef struct {
  hal_float_t *mcmd[RB_MAXJ], *mvel[RB_MAXJ], *dfb[RB_MAXJ], *res[RB_MAXJ];
  hal_float_t *cmd[RB_MAXJ], *vel[RB_MAXJ], *fb[RB_MAXJ];
  hal_bit_t *collision, *enable, *reset, *estop, *active, *prevent_en;
  hal_s32_t *tool;
  hal_s32_t *state, *reason;
  hal_float_t *clear_now, *clear_stop;
  int last_reset;
  double last_mcmd[RB_MAXJ];
} pins_t;

static int comp_id;
static pins_t *pins;
static rb_model mdl;
static gm_world geo;
static rb_guard guard;

static void update(void *arg, long period) {
  pins_t *p = arg;
  double dt = period * 1e-9;
  int n = mdl.n;
  if(*p->reset && !p->last_reset) {
    rb_guard_reset(&guard);
  }
  p->last_reset = *p->reset;
  if(!*p->enable && guard.state != GD_PASS) {
    rb_guard_release(&guard);
  }
  if(*p->enable && *p->collision && guard.state == GD_PASS && !guard.estop) {
    double q[RB_MAXJ], qd[RB_MAXJ], r[RB_MAXJ];
    for(int j = 0; j < n; j++) {  // take over at motion's command and speed
      q[j] = *p->mcmd[j] * D2R - mdl.link[j].theta0;
      qd[j] = *p->mvel[j] * D2R;
      r[j] = *p->res[j];
    }
    rb_guard_trigger(&guard, &mdl, GD_COLLISION, q, qd, r);
  }
  if(*p->enable && *p->prevent_en && guard.margin > 0.0 && guard.state == GD_PASS && !guard.estop) {
    double q[RB_MAXJ], qd[RB_MAXJ];
    for(int j = 0; j < n; j++) {  // the command about to be passed on
      q[j] = *p->mcmd[j] * D2R - mdl.link[j].theta0;
      qd[j] = *p->mvel[j] * D2R;
    }
    geo.tool = *p->tool;
    rb_guard_watch(&guard, &mdl, &geo, q, qd);
    *p->clear_now = guard.clear_now * 1000.0;
    *p->clear_stop = guard.clear_stop * 1000.0;
  }
  int act = guard.state != GD_PASS && guard.state != GD_DONE;
  if(act) {
    rb_guard_step(&guard, &mdl, dt);
  }
  for(int j = 0; j < n; j++) {
    if(act) {
      *p->cmd[j] = (guard.q[j] + mdl.link[j].theta0) / D2R;
      *p->vel[j] = guard.qd[j] / D2R;
      *p->fb[j] = *p->mcmd[j];  // motion sees no following error
    } else if(guard.state == GD_DONE) {  // holding until the estop lands
      *p->fb[j] = *p->mcmd[j];
    } else {
      *p->cmd[j] = *p->mcmd[j];
      *p->vel[j] = *p->mvel[j];
      *p->fb[j] = *p->dfb[j];
    }
    p->last_mcmd[j] = *p->mcmd[j];
  }
  *p->estop = guard.estop;
  *p->active = act;
  *p->state = guard.state;
  if(guard.reason) {
    *p->reason = guard.reason;
  }
  if(!guard.estop && guard.state == GD_PASS) {
    *p->reason = 0;
  }
}

static int export_pins(void) {
  int r = 0;
  for(int j = 0; j < mdl.n && !r; j++) {
    r |= hal_pin_float_newf(HAL_IN, &pins->mcmd[j], comp_id, PREFIX ".%d.motion-cmd", j);
    r |= hal_pin_float_newf(HAL_IN, &pins->mvel[j], comp_id, PREFIX ".%d.motion-vel", j);
    r |= hal_pin_float_newf(HAL_IN, &pins->dfb[j], comp_id, PREFIX ".%d.drive-fb", j);
    r |= hal_pin_float_newf(HAL_IN, &pins->res[j], comp_id, PREFIX ".%d.residual", j);
    r |= hal_pin_float_newf(HAL_OUT, &pins->cmd[j], comp_id, PREFIX ".%d.cmd", j);
    r |= hal_pin_float_newf(HAL_OUT, &pins->vel[j], comp_id, PREFIX ".%d.vel", j);
    r |= hal_pin_float_newf(HAL_OUT, &pins->fb[j], comp_id, PREFIX ".%d.fb", j);
  }
  r |= hal_pin_bit_newf(HAL_IN, &pins->collision, comp_id, PREFIX ".collision");
  r |= hal_pin_bit_newf(HAL_IN, &pins->enable, comp_id, PREFIX ".enable");
  r |= hal_pin_bit_newf(HAL_IN, &pins->reset, comp_id, PREFIX ".reset");
  r |= hal_pin_bit_newf(HAL_OUT, &pins->estop, comp_id, PREFIX ".estop");
  r |= hal_pin_bit_newf(HAL_OUT, &pins->active, comp_id, PREFIX ".active");
  r |= hal_pin_s32_newf(HAL_OUT, &pins->state, comp_id, PREFIX ".state");
  r |= hal_pin_s32_newf(HAL_OUT, &pins->reason, comp_id, PREFIX ".reason");
  r |= hal_pin_bit_newf(HAL_IN, &pins->prevent_en, comp_id, PREFIX ".prevent-enable");
  r |= hal_pin_float_newf(HAL_OUT, &pins->clear_now, comp_id, PREFIX ".clearance");
  r |= hal_pin_float_newf(HAL_OUT, &pins->clear_stop, comp_id, PREFIX ".clearance-stop");
  r |= hal_pin_s32_newf(HAL_IN, &pins->tool, comp_id, PREFIX ".tool");
  if(!r) {
    *pins->prevent_en = guard.margin > 0.0;
  }
  return r;
}

int rtapi_app_main(void) {
  char err[256];
  comp_id = hal_init("armguard");
  if(comp_id < 0) {
    return comp_id;
  }
  memset(&mdl, 0, sizeof(mdl));
  mdl.g[2] = -9.81;
  gm_init(&geo);
  rb_guard_init(&guard);
  if(!model || !model[0] || rb_guard_file(&guard, &mdl, &geo, model, err, sizeof(err)) ||
     (cell && cell[0] && rb_guard_file(&guard, &mdl, &geo, cell, err, sizeof(err))) || mdl.n < 1) {
    rtapi_print_msg(RTAPI_MSG_ERR, "armguard: %s\n", model && model[0] ? err : "model=<file> is required");
    hal_exit(comp_id);
    return -EINVAL;
  }
  pins = hal_malloc(sizeof(pins_t));
  if(!pins || export_pins()) {
    rtapi_print_msg(RTAPI_MSG_ERR, "armguard: cannot export pins\n");
    hal_exit(comp_id);
    return -ENOMEM;
  }
  if(hal_export_funct(PREFIX, update, pins, 1, 0, comp_id)) {
    rtapi_print_msg(RTAPI_MSG_ERR, "armguard: cannot export function\n");
    hal_exit(comp_id);
    return -EINVAL;
  }
  rtapi_print_msg(RTAPI_MSG_INFO, "armguard: %d joints, %d bodies, %d obstacles\n", mdl.n, geo.nb, geo.no);
  hal_ready(comp_id);
  return 0;
}

void rtapi_app_exit(void) {
  hal_exit(comp_id);
}
