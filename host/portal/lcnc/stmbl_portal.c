/*
 * stmbl_portal: the simulated portal machine with its three simulated
 * STMBL drives (host/portal/portalcore.c) as a LinuxCNC realtime component
 * (uspace).
 *
 *   loadrt stmbl_portal portal=/path/portal.portal lib=/path/libstmbl_drive.so
 *   addf stmbl-portal.0 servo-thread
 *
 * Per joint J (0 = X, 1 = Y1, 2 = Y2, a gantry: trivkins coordinates=XYY),
 * the pins of a drive's smart-serial link, in mm:
 *   stmbl-portal.0.J.pos-cmd, .vel-cmd, .enable  (in)
 *   stmbl-portal.0.J.pos-fb, .vel-fb, .current, .power, .fault  (out)
 * and the truth of the simulation, for tests:
 *   stmbl-portal.0.tool-x, .tool-y   the tool point relative to the frame
 *                                    (the work) [mm, machine X Y]
 *   stmbl-portal.0.rack              the bridge's yaw [urad]
 *   stmbl-portal.0.frame-x, .frame-y the frame on its feet [um], .frame-rot
 *                                    [urad]
 *   stmbl-portal.0.feet-x, .feet-y   force on the floor [N]
 *   stmbl-portal.0.head-x, .head-y   the head's deflection on the carriage [um]
 *   stmbl-portal.0.stop-hits         (s32) end stop hits
 *   stmbl-portal.0.bus               the shared DC bus [V] (portal file `bus`)
 */
#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"

#include <math.h>

#include "portalcore.h"

MODULE_AUTHOR("stmbl");
MODULE_DESCRIPTION("simulated portal machine on simulated STMBL drives");
MODULE_LICENSE("GPL");

static char *portal = "";
RTAPI_MP_STRING(portal, "portal description file (host/portal/machine/portal.portal)");
static char *lib = "";
RTAPI_MP_STRING(lib, "libstmbl_drive.so");

#define PREFIX "stmbl-portal.0"

static const int MOTOR[3] = {PP_XM, PP_Y1, PP_Y2};  // joint -> motor

typedef struct {
  hal_float_t *pos_cmd, *vel_cmd, *pos_fb, *vel_fb, *current, *power;
  hal_bit_t *enable, *fault;
} jpins_t;

typedef struct {
  jpins_t j[3];
  hal_float_t *tool[2], *rack, *frame[3], *feet[2], *head[2], *bus;
  hal_s32_t *stop_hits;
} pins_t;

static int comp_id;
static pins_t *pins;

static void update(void *arg, long period) {
  pins_t *p = arg;
  for(int j = 0; j < 3; j++) {
    ptcore_set_cmd(MOTOR[j], *p->j[j].enable, *p->j[j].pos_cmd, *p->j[j].vel_cmd);
  }
  ptcore_servo(period);
  for(int j = 0; j < 3; j++) {
    jpins_t *jp = &p->j[j];
    pt_drive *d = &pt_drv[MOTOR[j]];
    *jp->pos_fb = ptcore_fb(MOTOR[j]);
    *jp->vel_fb = ptcore_fb_vel(MOTOR[j]);
    *jp->current = d->pd.current;
    *jp->power = d->pd.power;
    *jp->fault = d->pd.fault;
  }
  double t[2], f[2];
  ptcore_tool(t);
  pp_feet(&pt_P, f);
  *p->tool[0] = t[0], *p->tool[1] = t[1];
  *p->rack = pt_P.q[PP_TH] * 1e6;
  *p->frame[0] = pt_P.q[PP_XF] * 1e6, *p->frame[1] = pt_P.q[PP_YF] * 1e6, *p->frame[2] = pt_P.q[PP_PSI] * 1e6;
  *p->feet[0] = f[0], *p->feet[1] = f[1];
  *p->head[0] = pt_P.q[PP_HX] * 1e6, *p->head[1] = pt_P.q[PP_HY] * 1e6;
  *p->stop_hits = pt_P.stop_hits[0] + pt_P.stop_hits[2];
  *p->bus = pt_B.on ? pt_B.v : *pt_drv[0].udc;
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
  r |= hal_pin_float_newf(HAL_OUT, &pins->rack, comp_id, PREFIX ".rack");
  r |= hal_pin_float_newf(HAL_OUT, &pins->frame[0], comp_id, PREFIX ".frame-x");
  r |= hal_pin_float_newf(HAL_OUT, &pins->frame[1], comp_id, PREFIX ".frame-y");
  r |= hal_pin_float_newf(HAL_OUT, &pins->frame[2], comp_id, PREFIX ".frame-rot");
  r |= hal_pin_float_newf(HAL_OUT, &pins->feet[0], comp_id, PREFIX ".feet-x");
  r |= hal_pin_float_newf(HAL_OUT, &pins->feet[1], comp_id, PREFIX ".feet-y");
  r |= hal_pin_float_newf(HAL_OUT, &pins->head[0], comp_id, PREFIX ".head-x");
  r |= hal_pin_float_newf(HAL_OUT, &pins->head[1], comp_id, PREFIX ".head-y");
  r |= hal_pin_s32_newf(HAL_OUT, &pins->stop_hits, comp_id, PREFIX ".stop-hits");
  r |= hal_pin_float_newf(HAL_OUT, &pins->bus, comp_id, PREFIX ".bus");
  return r;
}

int rtapi_app_main(void) {
  comp_id = hal_init("stmbl_portal");
  if(comp_id < 0) {
    return comp_id;
  }
  if(!portal || !portal[0] || !lib || !lib[0]) {
    rtapi_print_msg(RTAPI_MSG_ERR, "stmbl_portal: portal=<portal file> and lib=<libstmbl_drive.so> are required\n");
    hal_exit(comp_id);
    return -EINVAL;
  }
  if(ptcore_load(portal, lib)) {
    rtapi_print_msg(RTAPI_MSG_ERR, "stmbl_portal: cannot load %s\n", portal);
    hal_exit(comp_id);
    return -EINVAL;
  }
  pins = hal_malloc(sizeof(pins_t));
  if(!pins || export_pins()) {
    rtapi_print_msg(RTAPI_MSG_ERR, "stmbl_portal: cannot export pins\n");
    hal_exit(comp_id);
    return -ENOMEM;
  }
  if(hal_export_funct(PREFIX, update, pins, 1, 0, comp_id)) {
    rtapi_print_msg(RTAPI_MSG_ERR, "stmbl_portal: cannot export function\n");
    hal_exit(comp_id);
    return -EINVAL;
  }
  rtapi_print_msg(RTAPI_MSG_INFO, "stmbl_portal: %s\n", portal);
  hal_ready(comp_id);
  return 0;
}

void rtapi_app_exit(void) {
  hal_exit(comp_id);
}
