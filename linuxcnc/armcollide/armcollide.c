/*
 * armcollide: collision detection for a serial arm under LinuxCNC (machine
 * protection, not a safety function). A generalised momentum observer
 * compares the joint torques measured through the drives' motor currents
 * with the arm's dynamic model (shared/robot/collide.c); a residual over
 * its joint's threshold sets collision.
 *
 *   loadrt armcollide model=/path/model.arm
 *   addf armcollide.0 servo-thread       (after the drives' read)
 *
 * Model file: see shared/robot/collide.h (DH as genserkins, link inertia,
 * motors and gears, friction, kt, thresholds, observer gain/holdoff).
 *
 * Pins, per joint J (0-based, as LinuxCNC's joints):
 *   armcollide.0.J.pos       in   joint angle [deg], genserkins' DH theta
 *                                 (drive feedback: motor side)
 *   armcollide.0.J.vel       in   joint speed [deg/s] (drive feedback)
 *   armcollide.0.J.current   in   motor current [A] (torque producing, iq)
 *   armcollide.0.J.residual  out  estimated external joint torque [Nm]
 *   armcollide.0.J.threshold out  its threshold now [Nm]
 *   armcollide.0.enable      in   drives enabled and following
 *   armcollide.0.collision   out  latched until enable drops
 *   armcollide.0.joint       out  first joint over its threshold (1-based)
 *   armcollide.0.armed       out  past the hold-off after enable
 */
#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"

#include <math.h>

#include "collide.h"

MODULE_AUTHOR("stmbl");
MODULE_DESCRIPTION("model-based collision detection for a serial arm");
MODULE_LICENSE("GPL");

static char *model = "";
RTAPI_MP_STRING(model, "arm model file (shared/robot/collide.h)");

#define PREFIX "armcollide.0"
#define D2R (M_PI / 180.0)

typedef struct {
  hal_float_t *pos[RB_MAXJ], *vel[RB_MAXJ], *cur[RB_MAXJ], *res[RB_MAXJ], *thr[RB_MAXJ];
  hal_bit_t *enable, *collision, *armed;
  hal_s32_t *joint;
} pins_t;

static int comp_id;
static pins_t *pins;
static rb_collide det;

static void update(void *arg, long period) {
  pins_t *p = arg;
  double q[RB_MAXJ], qd[RB_MAXJ], cur[RB_MAXJ];
  int n = det.mdl.n;
  for(int j = 0; j < n; j++) {
    q[j] = *p->pos[j] * D2R - det.mdl.link[j].theta0;
    qd[j] = *p->vel[j] * D2R;
    cur[j] = *p->cur[j];
  }
  rb_collide_step(&det, *p->enable, q, qd, cur, period * 1e-9);
  for(int j = 0; j < n; j++) {
    *p->res[j] = det.r[j];
    *p->thr[j] = det.thr[j];
  }
  *p->collision = det.hit;
  *p->joint = det.joint;
  *p->armed = det.armed;
}

static int export_pins(void) {
  int r = 0;
  for(int j = 0; j < det.mdl.n && !r; j++) {
    r |= hal_pin_float_newf(HAL_IN, &pins->pos[j], comp_id, PREFIX ".%d.pos", j);
    r |= hal_pin_float_newf(HAL_IN, &pins->vel[j], comp_id, PREFIX ".%d.vel", j);
    r |= hal_pin_float_newf(HAL_IN, &pins->cur[j], comp_id, PREFIX ".%d.current", j);
    r |= hal_pin_float_newf(HAL_OUT, &pins->res[j], comp_id, PREFIX ".%d.residual", j);
    r |= hal_pin_float_newf(HAL_OUT, &pins->thr[j], comp_id, PREFIX ".%d.threshold", j);
  }
  r |= hal_pin_bit_newf(HAL_IN, &pins->enable, comp_id, PREFIX ".enable");
  r |= hal_pin_bit_newf(HAL_OUT, &pins->collision, comp_id, PREFIX ".collision");
  r |= hal_pin_bit_newf(HAL_OUT, &pins->armed, comp_id, PREFIX ".armed");
  r |= hal_pin_s32_newf(HAL_OUT, &pins->joint, comp_id, PREFIX ".joint");
  return r;
}

int rtapi_app_main(void) {
  char err[256];
  comp_id = hal_init("armcollide");
  if(comp_id < 0) {
    return comp_id;
  }
  if(!model || !model[0] || rb_collide_load(&det, model, err, sizeof(err))) {
    rtapi_print_msg(RTAPI_MSG_ERR, "armcollide: %s\n", model && model[0] ? err : "model=<file> is required");
    hal_exit(comp_id);
    return -EINVAL;
  }
  pins = hal_malloc(sizeof(pins_t));
  if(!pins || export_pins()) {
    rtapi_print_msg(RTAPI_MSG_ERR, "armcollide: cannot export pins\n");
    hal_exit(comp_id);
    return -ENOMEM;
  }
  if(hal_export_funct(PREFIX, update, pins, 1, 0, comp_id)) {
    rtapi_print_msg(RTAPI_MSG_ERR, "armcollide: cannot export function\n");
    hal_exit(comp_id);
    return -EINVAL;
  }
  rtapi_print_msg(RTAPI_MSG_INFO, "armcollide: %d joints from %s\n", det.mdl.n, model);
  hal_ready(comp_id);
  return 0;
}

void rtapi_app_exit(void) {
  hal_exit(comp_id);
}
