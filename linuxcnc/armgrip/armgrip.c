/*
 * armgrip: an electric finger gripper (its own stmbl drive) and the payload
 * it picks up, under LinuxCNC (shared/robot/gripper.h). Machine
 * protection, not a safety function.
 *
 *   loadrt armgrip model=/path/model.arm      (armcollide's model file)
 *   addf armgrip.0 servo-thread               (after armcollide.0)
 *
 * The fingers: closing is a position move at `speed` with the drive's
 * current limit as the grip force; more than 1 mm of lag is contact, the
 * gap then the part's width: GRIPPED if it is width-expect (+-1 mm), else
 * WRONG; closed to 0: EMPTY; a gripped part slipping out: LOST.
 * The payload: on GRIPPED armcollide's model takes mass-expect at
 * (cx, cy, cz) at once (muted while the fingers squeeze); the lift that
 * follows checks the mass from armcollide's residuals (joints moving faster
 * than 0.15 rad/s; at standstill friction hides it). Off by more than 1 kg
 * (or 15 %): mismatch. On release the model drops it. mass-expect 0: the
 * mass is learnt on the lift (detection muted until then).
 *
 * Pins:
 *   armgrip.0.enable        in   machine on (motion.motion-enabled)
 *   armgrip.0.close         in   close the fingers (M64 / M65 P0)
 *   armgrip.0.width-expect  in   the part's width [mm] (0: any)
 *   armgrip.0.mass-expect   in   its mass [kg] (0: unknown, learn it)
 *   armgrip.0.cx/cy/cz      in   its centre of mass [m, flange frame]
 *                                (all four taken while the fingers are open)
 *   armgrip.0.gap-open      in   open position [mm] (default 75)
 *   armgrip.0.speed         in   finger speed [mm/s] (default 40)
 *   armgrip.0.gap-fb        in   the gripper drive's position [mm of gap]
 *   armgrip.0.gap-cmd, .gap-vel, .drive-enable  out  to the gripper drive
 *                                (while disabled the command follows the
 *                                feedback, as LinuxCNC's joints do)
 *   armgrip.0.J.pos, .J.vel in   joint angle [deg, genserkins DH theta]
 *                                and speed [deg/s] (drive feedback)
 *   armgrip.0.J.residual    in   armcollide.0.J.residual
 *   armgrip.0.state         out  0 open, 1 closing, 2 gripped, 3 wrong,
 *                                4 empty, 5 opening, 6 lost
 *   armgrip.0.width         out  measured width [mm]
 *   armgrip.0.gripped       out  state == gripped (M66 wait)
 *   armgrip.0.pay-state     out  0 idle, 1 settle, 2 measure, 3 ok, 4 mismatch
 *   armgrip.0.pay-ok        out  pay-state == ok (M66 wait)
 *   armgrip.0.pay-meas      out  the measured mass [kg]
 *   armgrip.0.payload-mass, .payload-cx/-cy/-cz  out  to armcollide
 *   armgrip.0.mute          out  to armcollide.0.mute
 *   armgrip.0.fault         out  latched until reset: 6 payload mismatch,
 *   armgrip.0.reason        out  7 part lost, 8 grip failed (wrong width,
 *                                empty, with a width expected)
 *   armgrip.0.reset         in   clears fault (rising edge)
 *   armgrip.0.chain-in      in   the estop chain ...
 *   armgrip.0.chain-out     out  ... and !fault (into emc-enable-in)
 */
#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"

#include <math.h>
#include <string.h>

#include "collide.h"
#include "gripper.h"

MODULE_AUTHOR("stmbl");
MODULE_DESCRIPTION("electric finger gripper and payload check for a serial arm");
MODULE_LICENSE("GPL");

static char *model = "";
RTAPI_MP_STRING(model, "arm model file (shared/robot/collide.h)");

#define PREFIX "armgrip.0"
#define D2R (M_PI / 180.0)

typedef struct {
  hal_float_t *pos[RB_MAXJ], *vel[RB_MAXJ], *res[RB_MAXJ];
  hal_bit_t *enable, *close, *drv_en, *gripped, *pay_ok, *mute, *fault, *reset, *chain_in, *chain_out;
  hal_float_t *w_expect, *m_expect, *c[3], *gap_open, *speed, *gap_fb, *gap_cmd, *gap_vel;
  hal_float_t *width, *pay_meas, *pay_m, *pay_c[3];
  hal_s32_t *state, *pay_state, *reason;
  int prev_state, last_reset, latched, why;
  double cmd_prev;
} pins_t;

static int comp_id;
static pins_t *pins;
static rb_collide det;  // the model (DH, inertia, gravity)
static rb_gripper grip;
static rb_grip_payload gpay;

static void set_fault(pins_t *p, int why) {
  if(!p->latched) {
    p->latched = 1;
    p->why = why;
  }
}

static void update(void *arg, long period) {
  pins_t *p = arg;
  double dt = period * 1e-9, gap = *p->gap_fb;
  int en = *p->enable;
  if(grip.state == GR_OPEN || grip.state == GR_OPENING) {  // the next part's data
    grip.w_expect = *p->w_expect;
    gpay.m_expect = *p->m_expect;
    for(int k = 0; k < 3; k++) {
      gpay.c_expect[k] = *p->c[k];
    }
  }
  grip.gap_open = *p->gap_open > 0.0 ? *p->gap_open : 75.0;
  grip.v = *p->speed > 0.0 ? *p->speed : 40.0;
  if(en) {
    rb_gripper_step(&grip, *p->close, gap, dt);
  } else {
    grip.cmd = gap;
  }
  *p->gap_cmd = grip.cmd;
  *p->gap_vel = (grip.cmd - p->cmd_prev) / dt;
  *p->drv_en = en;
  p->cmd_prev = grip.cmd;

  int ev = 0, s = grip.state;
  if(s == GR_GRIPPED && p->prev_state != GR_GRIPPED) {
    ev = +1;
  } else if(p->prev_state == GR_GRIPPED && (s == GR_OPENING || s == GR_OPEN)) {
    ev = -1;
  } else if(s == GR_LOST && p->prev_state != GR_LOST) {
    set_fault(p, 7);
  } else if(grip.w_expect > 0.0 && s != p->prev_state && (s == GR_WRONG || s == GR_EMPTY)) {
    set_fault(p, 8);
  }
  p->prev_state = s;

  int n = det.mdl.n, was = gpay.state;
  double q[RB_MAXJ], qd[RB_MAXJ], r[RB_MAXJ];
  for(int j = 0; j < n; j++) {
    q[j] = *p->pos[j] * D2R - det.mdl.link[j].theta0;
    qd[j] = *p->vel[j] * D2R;
    r[j] = *p->res[j];
  }
  rb_grip_payload_step(&gpay, &det.mdl, ev, q, qd, r, dt);
  if(gpay.update) {
    *p->pay_m = gpay.m_model;
    for(int k = 0; k < 3; k++) {
      *p->pay_c[k] = gpay.c_expect[k];
    }
  }
  if(gpay.state == GP_MISMATCH && was != GP_MISMATCH && en) {
    set_fault(p, 6);
  }
  if(*p->reset && !p->last_reset) {
    p->latched = 0;
    p->why = 0;
    if(gpay.state == GP_MISMATCH) {
      gpay.state = GP_IDLE;
    }
  }
  p->last_reset = *p->reset;

  *p->mute = gpay.mute;
  *p->state = grip.state;
  *p->width = grip.width;
  *p->gripped = grip.state == GR_GRIPPED;
  *p->pay_state = gpay.state;
  *p->pay_ok = gpay.state == GP_OK;
  *p->pay_meas = gpay.m_meas;
  *p->fault = p->latched;
  *p->reason = p->why;
  *p->chain_out = *p->chain_in && !p->latched;
}

static int export_pins(void) {
  int r = 0;
  for(int j = 0; j < det.mdl.n && !r; j++) {
    r |= hal_pin_float_newf(HAL_IN, &pins->pos[j], comp_id, PREFIX ".%d.pos", j);
    r |= hal_pin_float_newf(HAL_IN, &pins->vel[j], comp_id, PREFIX ".%d.vel", j);
    r |= hal_pin_float_newf(HAL_IN, &pins->res[j], comp_id, PREFIX ".%d.residual", j);
  }
  r |= hal_pin_bit_newf(HAL_IN, &pins->enable, comp_id, PREFIX ".enable");
  r |= hal_pin_bit_newf(HAL_IN, &pins->close, comp_id, PREFIX ".close");
  r |= hal_pin_float_newf(HAL_IN, &pins->w_expect, comp_id, PREFIX ".width-expect");
  r |= hal_pin_float_newf(HAL_IN, &pins->m_expect, comp_id, PREFIX ".mass-expect");
  static const char *cn[3] = {"cx", "cy", "cz"};
  for(int k = 0; k < 3 && !r; k++) {
    r |= hal_pin_float_newf(HAL_IN, &pins->c[k], comp_id, PREFIX ".%s", cn[k]);
    r |= hal_pin_float_newf(HAL_OUT, &pins->pay_c[k], comp_id, PREFIX ".payload-%s", cn[k]);
  }
  r |= hal_pin_float_newf(HAL_IN, &pins->gap_open, comp_id, PREFIX ".gap-open");
  r |= hal_pin_float_newf(HAL_IN, &pins->speed, comp_id, PREFIX ".speed");
  r |= hal_pin_float_newf(HAL_IN, &pins->gap_fb, comp_id, PREFIX ".gap-fb");
  r |= hal_pin_float_newf(HAL_OUT, &pins->gap_cmd, comp_id, PREFIX ".gap-cmd");
  r |= hal_pin_float_newf(HAL_OUT, &pins->gap_vel, comp_id, PREFIX ".gap-vel");
  r |= hal_pin_bit_newf(HAL_OUT, &pins->drv_en, comp_id, PREFIX ".drive-enable");
  r |= hal_pin_s32_newf(HAL_OUT, &pins->state, comp_id, PREFIX ".state");
  r |= hal_pin_float_newf(HAL_OUT, &pins->width, comp_id, PREFIX ".width");
  r |= hal_pin_bit_newf(HAL_OUT, &pins->gripped, comp_id, PREFIX ".gripped");
  r |= hal_pin_s32_newf(HAL_OUT, &pins->pay_state, comp_id, PREFIX ".pay-state");
  r |= hal_pin_bit_newf(HAL_OUT, &pins->pay_ok, comp_id, PREFIX ".pay-ok");
  r |= hal_pin_float_newf(HAL_OUT, &pins->pay_meas, comp_id, PREFIX ".pay-meas");
  r |= hal_pin_float_newf(HAL_OUT, &pins->pay_m, comp_id, PREFIX ".payload-mass");
  r |= hal_pin_bit_newf(HAL_OUT, &pins->mute, comp_id, PREFIX ".mute");
  r |= hal_pin_bit_newf(HAL_OUT, &pins->fault, comp_id, PREFIX ".fault");
  r |= hal_pin_s32_newf(HAL_OUT, &pins->reason, comp_id, PREFIX ".reason");
  r |= hal_pin_bit_newf(HAL_IN, &pins->reset, comp_id, PREFIX ".reset");
  r |= hal_pin_bit_newf(HAL_IN, &pins->chain_in, comp_id, PREFIX ".chain-in");
  r |= hal_pin_bit_newf(HAL_OUT, &pins->chain_out, comp_id, PREFIX ".chain-out");
  if(!r) {
    *pins->gap_open = 75.0;
    *pins->speed = 40.0;
  }
  return r;
}

int rtapi_app_main(void) {
  char err[256];
  comp_id = hal_init("armgrip");
  if(comp_id < 0) {
    return comp_id;
  }
  if(!model || !model[0] || rb_collide_load(&det, model, err, sizeof(err))) {
    rtapi_print_msg(RTAPI_MSG_ERR, "armgrip: model=<model file> needed (%s)\n", model && model[0] ? err : "");
    hal_exit(comp_id);
    return -EINVAL;
  }
  rb_gripper_init(&grip);
  rb_grip_payload_init(&gpay);
  pins = hal_malloc(sizeof(pins_t));
  if(!pins) {
    hal_exit(comp_id);
    return -ENOMEM;
  }
  memset(pins, 0, sizeof(*pins));
  if(export_pins() || hal_export_funct(PREFIX, update, pins, 1, 0, comp_id)) {
    rtapi_print_msg(RTAPI_MSG_ERR, "armgrip: cannot export pins / function\n");
    hal_exit(comp_id);
    return -EINVAL;
  }
  pins->prev_state = GR_OPEN;
  hal_ready(comp_id);
  return 0;
}

void rtapi_app_exit(void) {
  hal_exit(comp_id);
}
