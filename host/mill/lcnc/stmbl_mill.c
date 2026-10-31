/*
 * stmbl_mill: the simulated fixed-bridge mill (the Scripta CN 720) with its
 * three simulated STMBL drives (host/mill/millcore.c) as a LinuxCNC
 * realtime component (uspace).
 *
 *   loadrt stmbl_mill machine=/path/cn720.mill lib=/path/libstmbl_drive.so
 *   addf stmbl-mill.0 servo-thread
 *
 * The servo thread's period is the drives' packet rate (1 ms, 0.5 ms ...;
 * their vel_int0.cmd_freq follows it). Per joint J (0 = X, 1 = Y, 2 = Z:
 * trivkins XYZ; Y is the table, machine Y the tool relative to it), the
 * pins of a drive's smart-serial link, in mm:
 *   stmbl-mill.0.J.pos-cmd, .vel-cmd, .enable  (in)
 *   stmbl-mill.0.J.pos-fb, .vel-fb, .current, .power, .fault  (out)
 * and the truth of the simulation, for tests:
 *   stmbl-mill.0.tool-x, .tool-y, .tool-z  the tool relative to the work at
 *                                    the servo thread's time [mm, machine]
 *   stmbl-mill.0.head-x, .head-y     the spindle tip's deflection [um]
 *   stmbl-mill.0.bridge-y            the bridge's sway on its columns [um]
 *   stmbl-mill.0.frame-x, .frame-y   the base on its feet [um]
 *   stmbl-mill.0.stop-hits           (s32) end stop hits
 */
#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"

#include <math.h>

#include "millcore.h"

MODULE_AUTHOR("stmbl");
MODULE_DESCRIPTION("simulated fixed-bridge mill on simulated STMBL drives");
MODULE_LICENSE("GPL");

static char *machine = "";
RTAPI_MP_STRING(machine, "machine file (host/mill/machine/cn720.mill)");
static char *lib = "";
RTAPI_MP_STRING(lib, "libstmbl_drive.so");

#define PREFIX "stmbl-mill.0"

typedef struct {
  hal_float_t *pos_cmd, *vel_cmd, *pos_fb, *vel_fb, *current, *power;
  hal_bit_t *enable, *fault;
} jpins_t;

typedef struct {
  jpins_t j[3];
  hal_float_t *tool[3], *head[2], *bridge_y, *frame[2];
  hal_s32_t *stop_hits;
} pins_t;

static int comp_id;
static pins_t *pins;
static double tnow = 0.0;  // the servo thread's time, as the core counts it

static void update(void *arg, long period) {
  pins_t *p = arg;
  for(int j = 0; j < 3; j++) {
    mccore_set_cmd(j, *p->j[j].enable, *p->j[j].pos_cmd, *p->j[j].vel_cmd);
  }
  mccore_servo(period * 1e-9);
  tnow += period * 1e-9;
  for(int j = 0; j < 3; j++) {
    jpins_t *jp = &p->j[j];
    mc_drive *d = &mc_drv[j];
    *jp->pos_fb = mccore_fb(j);
    *jp->vel_fb = mccore_fb_vel(j);
    *jp->current = d->pd.current;
    *jp->power = d->pd.power;
    *jp->fault = d->pd.fault;
  }
  double t[3];
  mccore_tool(tnow, t);
  for(int k = 0; k < 3; k++) {
    *p->tool[k] = t[k];
  }
  *p->head[0] = mc_P.hx * 1e6, *p->head[1] = mc_P.hy * 1e6;
  *p->bridge_y = mc_P.yb * 1e6;
  *p->frame[0] = mc_P.xf * 1e6, *p->frame[1] = mc_P.yf * 1e6;
  *p->stop_hits = mc_P.hits[0] + mc_P.hits[1] + mc_P.hits[2];
}

static int export_pins(void) {
  int r = 0;
  for(int j = 0; j < 3 && !r; j++) {
    jpins_t *jp = &pins->j[j];
    r |= hal_pin_float_newf(HAL_IN, &jp->pos_cmd, comp_id, PREFIX ".%d.pos-cmd", j);
    r |= hal_pin_float_newf(HAL_IN, &jp->vel_cmd, comp_id, PREFIX ".%d.vel-cmd", j);
    r |= hal_pin_bit_newf(HAL_IN, &jp->enable, comp_id, PREFIX ".%d.enable", j);
    r |= hal_pin_float_newf(HAL_OUT, &jp->pos_fb, comp_id, PREFIX ".%d.pos-fb", j);
    r |= hal_pin_float_newf(HAL_OUT, &jp->vel_fb, comp_id, PREFIX ".%d.vel-fb", j);
    r |= hal_pin_float_newf(HAL_OUT, &jp->current, comp_id, PREFIX ".%d.current", j);
    r |= hal_pin_float_newf(HAL_OUT, &jp->power, comp_id, PREFIX ".%d.power", j);
    r |= hal_pin_bit_newf(HAL_OUT, &jp->fault, comp_id, PREFIX ".%d.fault", j);
  }
  r |= hal_pin_float_newf(HAL_OUT, &pins->tool[0], comp_id, PREFIX ".tool-x");
  r |= hal_pin_float_newf(HAL_OUT, &pins->tool[1], comp_id, PREFIX ".tool-y");
  r |= hal_pin_float_newf(HAL_OUT, &pins->tool[2], comp_id, PREFIX ".tool-z");
  r |= hal_pin_float_newf(HAL_OUT, &pins->head[0], comp_id, PREFIX ".head-x");
  r |= hal_pin_float_newf(HAL_OUT, &pins->head[1], comp_id, PREFIX ".head-y");
  r |= hal_pin_float_newf(HAL_OUT, &pins->bridge_y, comp_id, PREFIX ".bridge-y");
  r |= hal_pin_float_newf(HAL_OUT, &pins->frame[0], comp_id, PREFIX ".frame-x");
  r |= hal_pin_float_newf(HAL_OUT, &pins->frame[1], comp_id, PREFIX ".frame-y");
  r |= hal_pin_s32_newf(HAL_OUT, &pins->stop_hits, comp_id, PREFIX ".stop-hits");
  return r;
}

int rtapi_app_main(void) {
  comp_id = hal_init("stmbl_mill");
  if(comp_id < 0) {
    return comp_id;
  }
  if(!machine || !machine[0] || !lib || !lib[0]) {
    rtapi_print_msg(RTAPI_MSG_ERR, "stmbl_mill: machine=<machine file> and lib=<libstmbl_drive.so> are required\n");
    hal_exit(comp_id);
    return -EINVAL;
  }
  if(mccore_load(machine, lib)) {
    rtapi_print_msg(RTAPI_MSG_ERR, "stmbl_mill: cannot load %s\n", machine);
    hal_exit(comp_id);
    return -EINVAL;
  }
  pins = hal_malloc(sizeof(pins_t));
  if(!pins || export_pins()) {
    rtapi_print_msg(RTAPI_MSG_ERR, "stmbl_mill: cannot export pins\n");
    hal_exit(comp_id);
    return -ENOMEM;
  }
  if(hal_export_funct(PREFIX, update, pins, 1, 0, comp_id)) {
    rtapi_print_msg(RTAPI_MSG_ERR, "stmbl_mill: cannot export function\n");
    hal_exit(comp_id);
    return -EINVAL;
  }
  rtapi_print_msg(RTAPI_MSG_INFO, "stmbl_mill: %s\n", machine);
  hal_ready(comp_id);
  return 0;
}

void rtapi_app_exit(void) {
  hal_exit(comp_id);
}
