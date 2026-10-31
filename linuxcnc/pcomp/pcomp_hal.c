/*
 * pcomp: physics-based command conditioning for a portal (gantry) machine
 * as a LinuxCNC realtime component (shared/pcomp/pcomp.h). Between motion
 * and the drives: the planner's X, Y in, conditioned commands out, and the
 * drives' feedback back to motion relative to the conditioned command, so
 * motion's following error check sees the drives' own error, not the
 * conditioning's delay and leads.
 *
 *   loadrt pcomp
 *   addf pcomp.0 servo-thread        (after motion-controller, before the drives)
 *
 * Pins (pcomp.0.*):
 *   x-cmd, y-cmd          in   motion's X, Y (joint.0 / joint.1.motor-pos-cmd)
 *   x-vel-cmd, y-vel-cmd  in   their velocity (joint.N.vel-cmd), passed on
 *                              while the conditioning is off
 *   x-out, y-out          out  to the drives (y-out to both Y drives) [mm]
 *   x-vel, y-vel          out  their velocity [mm/s]
 *   x-fb, y1-fb, y2-fb    in   the drives' position feedback
 *   x-fb-out, y1-fb-out, y2-fb-out  out  to motion (joint.N.motor-pos-fb):
 *                         the feedback + (command in now - command out the
 *                         period before): the drives' feedback read here is
 *                         where they got to with pcomp's previous output, and
 *                         motion holds it against this period's command
 *   enable                in   0: everything passes through. Wire it to the
 *                              amplifiers' enable: while they are off, motion
 *                              makes its command follow the feedback, and
 *                              through the conditioning's delay that loop
 *                              would wind up; on enable it starts again at
 *                              standstill (default 1)
 *   shaper, defl, shrink, servo   in  (bit) the parts of the conditioning
 *   jerk-ms               in   the moving average after the shaper [ms]
 *   fx, fy-end, fy-mid    in   the model: the head's modes [Hz] (x; y at the
 *                              rails, at mid-span)
 *   zeta                  in   their damping ratio
 *   rails                 in   rail spacing [m]; x-center: machine X of
 *                              the bridge's centre [mm]
 *   servo-tau2            in   the drives' lag per acceleration [s^2]
 *   delay                 out  the conditioning's mean delay [ms]
 *   dio-in-0..3 / dio-out-0..3      bits delayed by `delay` (laser, spindle:
 *   s32-in-0 / s32-out-0            the tool acts where the motion is)
 *
 * Change the model and the switches at standstill.
 */
#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"

#include <math.h>
#include <string.h>

#include "pcomp.h"

MODULE_AUTHOR("stmbl");
MODULE_DESCRIPTION("physics-based command conditioning for a portal machine");
MODULE_LICENSE("GPL");

#define PREFIX "pcomp.0"
#define NDIO 4

typedef struct {
  hal_float_t *cmd[2], *vcmd[2], *out[2], *vel[2], *fb[3], *fb_out[3];
  hal_bit_t *enable, *shaper, *defl, *shrink, *servo;
  hal_float_t *jerk_ms, *fx, *fy_end, *fy_mid, *zeta, *rails, *x_center, *tau2, *delay;
  hal_bit_t *dio_in[NDIO], *dio_out[NDIO];
  hal_s32_t *s32_in, *s32_out;
  pc_t pc;
  double dt;
  int was_on;
  double out_prev[2];  // the commands out, last period
  int prev_on;         // ... and they were conditioned
  // the delay line for the I/O
  unsigned char dio_hist[PC_HIST];
  int s32_hist[PC_HIST];
  int io_head;
} state_t;

static int comp_id;
static state_t *st;

static void update(void *arg, long period) {
  state_t *s = arg;
  double dt = period * 1e-9;
  pc_t *c = &s->pc;
  if(dt != s->dt) {  // the servo period: (re)start
    pc_model m = {0};
    pc_init(c, &m, dt);
    s->dt = dt;
  }
  pc_model *m = &c->m;  // the model from the pins, every period
  m->fx = *s->fx, m->fy_end = *s->fy_end, m->fy_mid = *s->fy_mid, m->zeta = *s->zeta;
  m->L = *s->rails, m->x0 = *s->x_center, m->servo = *s->tau2;
  int on = *s->enable && m->fx > 0.0 && m->fy_end > 0.0 && m->fy_mid > 0.0 && m->L > 0.0 && m->zeta >= 0.0 && m->zeta < 1.0;
  c->shaper = on && *s->shaper;
  c->defl = on && *s->defl;
  c->shrink = on && *s->shrink;
  c->servo = on && *s->servo;
  c->jerk = on ? (int)lround(*s->jerk_ms * 1e-3 / dt) : 0;
  if(!on || !s->was_on) {
    c->primed = 0;  // start again where the command stands
  }
  s->was_on = on;
  double xy[2] = {*s->cmd[0], *s->cmd[1]};
  c->vin[0] = *s->vcmd[0], c->vin[1] = *s->vcmd[1];
  pc_step(c, xy);
  for(int k = 0; k < 2; k++) {
    *s->out[k] = c->out[k];
    *s->vel[k] = c->vout[k];
  }
  // feedback relative to the conditioned command: motion sees the drives'
  // own error
  double off[3] = {xy[0] - s->out_prev[0], xy[1] - s->out_prev[1], xy[1] - s->out_prev[1]};
  for(int j = 0; j < 3; j++) {
    *s->fb_out[j] = *s->fb[j] + (on && s->prev_on ? off[j] : 0.0);
  }
  s->out_prev[0] = c->out[0], s->out_prev[1] = c->out[1];
  s->prev_on = on;
  *s->delay = c->delay * 1000.0;
  // the I/O, delayed as much as the motion
  s->io_head = (s->io_head + 1) % PC_HIST;
  unsigned char bits = 0;
  for(int i = 0; i < NDIO; i++) {
    bits |= (*s->dio_in[i] ? 1 : 0) << i;
  }
  s->dio_hist[s->io_head] = bits;
  s->s32_hist[s->io_head] = *s->s32_in;
  int back = (int)lround(c->delay / dt);
  back = back < 0 ? 0 : back > PC_HIST - 1 ? PC_HIST - 1 : back;
  int i = (s->io_head - back + PC_HIST) % PC_HIST;
  for(int k = 0; k < NDIO; k++) {
    *s->dio_out[k] = (s->dio_hist[i] >> k) & 1;
  }
  *s->s32_out = s->s32_hist[i];
}

static int export_pins(void) {
  int r = 0;
  static const char *ax[2] = {"x", "y"}, *fb[3] = {"x", "y1", "y2"};
  for(int k = 0; k < 2; k++) {
    r |= hal_pin_float_newf(HAL_IN, &st->cmd[k], comp_id, PREFIX ".%s-cmd", ax[k]);
    r |= hal_pin_float_newf(HAL_IN, &st->vcmd[k], comp_id, PREFIX ".%s-vel-cmd", ax[k]);
    r |= hal_pin_float_newf(HAL_OUT, &st->out[k], comp_id, PREFIX ".%s-out", ax[k]);
    r |= hal_pin_float_newf(HAL_OUT, &st->vel[k], comp_id, PREFIX ".%s-vel", ax[k]);
  }
  for(int j = 0; j < 3; j++) {
    r |= hal_pin_float_newf(HAL_IN, &st->fb[j], comp_id, PREFIX ".%s-fb", fb[j]);
    r |= hal_pin_float_newf(HAL_OUT, &st->fb_out[j], comp_id, PREFIX ".%s-fb-out", fb[j]);
  }
  r |= hal_pin_bit_newf(HAL_IN, &st->enable, comp_id, PREFIX ".enable");
  r |= hal_pin_bit_newf(HAL_IN, &st->shaper, comp_id, PREFIX ".shaper");
  r |= hal_pin_bit_newf(HAL_IN, &st->defl, comp_id, PREFIX ".defl");
  r |= hal_pin_bit_newf(HAL_IN, &st->shrink, comp_id, PREFIX ".shrink");
  r |= hal_pin_bit_newf(HAL_IN, &st->servo, comp_id, PREFIX ".servo");
  r |= hal_pin_float_newf(HAL_IN, &st->jerk_ms, comp_id, PREFIX ".jerk-ms");
  r |= hal_pin_float_newf(HAL_IN, &st->fx, comp_id, PREFIX ".fx");
  r |= hal_pin_float_newf(HAL_IN, &st->fy_end, comp_id, PREFIX ".fy-end");
  r |= hal_pin_float_newf(HAL_IN, &st->fy_mid, comp_id, PREFIX ".fy-mid");
  r |= hal_pin_float_newf(HAL_IN, &st->zeta, comp_id, PREFIX ".zeta");
  r |= hal_pin_float_newf(HAL_IN, &st->rails, comp_id, PREFIX ".rails");
  r |= hal_pin_float_newf(HAL_IN, &st->x_center, comp_id, PREFIX ".x-center");
  r |= hal_pin_float_newf(HAL_IN, &st->tau2, comp_id, PREFIX ".servo-tau2");
  r |= hal_pin_float_newf(HAL_OUT, &st->delay, comp_id, PREFIX ".delay");
  for(int i = 0; i < NDIO; i++) {
    r |= hal_pin_bit_newf(HAL_IN, &st->dio_in[i], comp_id, PREFIX ".dio-in-%d", i);
    r |= hal_pin_bit_newf(HAL_OUT, &st->dio_out[i], comp_id, PREFIX ".dio-out-%d", i);
  }
  r |= hal_pin_s32_newf(HAL_IN, &st->s32_in, comp_id, PREFIX ".s32-in-0");
  r |= hal_pin_s32_newf(HAL_OUT, &st->s32_out, comp_id, PREFIX ".s32-out-0");
  if(!r) {  // on, all parts off until set; a 20 ms average when the shaper is on
    *st->enable = 1;
    *st->jerk_ms = 20.0;
    *st->zeta = 0.03;
  }
  return r;
}

int rtapi_app_main(void) {
  comp_id = hal_init("pcomp");
  if(comp_id < 0) {
    return comp_id;
  }
  st = hal_malloc(sizeof(state_t));
  if(!st) {
    hal_exit(comp_id);
    return -ENOMEM;
  }
  memset(st, 0, sizeof(*st));
  if(export_pins()) {
    rtapi_print_msg(RTAPI_MSG_ERR, "pcomp: cannot export pins\n");
    hal_exit(comp_id);
    return -EINVAL;
  }
  if(hal_export_funct(PREFIX, update, st, 1, 0, comp_id)) {
    rtapi_print_msg(RTAPI_MSG_ERR, "pcomp: cannot export function\n");
    hal_exit(comp_id);
    return -EINVAL;
  }
  hal_ready(comp_id);
  return 0;
}

void rtapi_app_exit(void) {
  hal_exit(comp_id);
}
