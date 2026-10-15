/*
 * stmbl_arm: the simulated arm with one simulated STMBL drive per joint
 * (host/arm/armcore.c) as a LinuxCNC realtime component (uspace).
 *
 *   loadrt stmbl_arm arm=/path/arcmate.arm lib=/path/libstmbl_drive.so
 *   addf stmbl-arm.0 servo-thread
 *
 * Per joint J (0-based, as LinuxCNC's joints), the pins of a drive's
 * smart-serial link, in joint degrees in genserkins' convention (the DH
 * theta, i.e. the model angle + theta0):
 *   stmbl-arm.0.J.pos-cmd, .vel-cmd, .enable  (in)
 *   stmbl-arm.0.J.pos-fb, .vel-fb, .current, .fault  (out)
 * and the truth of the simulation, for tests:
 *   stmbl-arm.0.J.q          link angle [deg, DH theta]
 *   stmbl-arm.0.J.tau-ext    joint torque of the external force (J^T f) [Nm]
 *   stmbl-arm.0.J.tau-gear   gear torque [Nm]
 *   stmbl-arm.0.J.stop-hits  hard stop hits
 * An external force on a link (collision tests):
 *   stmbl-arm.0.force-link (s32, 1..6, 0 = none), .force-x/-y/-z [N, base
 *   frame], .force-rx/-ry/-rz [m, link frame]
 * Fault injection and payload:
 *   stmbl-arm.0.J.link-lost  in: the drive gets no more packets; after 3
 *                            servo periods without a reply .fault rises, as
 *                            hm2's smart-serial error wired into amp-fault
 *   stmbl-arm.0.payload-mass, .payload-cx/-cy/-cz  [kg, m in the last link's
 *                            frame], applied when they change
 * Each drive's console is not exposed; its config is the ARMFILE's.
 */
#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"

#include <math.h>
#include <string.h>

#include "armcore.h"

MODULE_AUTHOR("stmbl");
MODULE_DESCRIPTION("simulated arm on simulated STMBL drives");
MODULE_LICENSE("GPL");

static char *arm = "";
RTAPI_MP_STRING(arm, "arm description file (host/arm/arcmate/arcmate.arm)");
static char *lib = "";
RTAPI_MP_STRING(lib, "libstmbl_drive.so");

#define PREFIX "stmbl-arm.0"
#define D2R (M_PI / 180.0)

typedef struct {
  hal_float_t *pos_cmd, *vel_cmd, *pos_fb, *vel_fb, *current, *q, *tau_ext, *tau_gear;
  hal_bit_t *enable, *fault, *link_lost;
  hal_s32_t *stop_hits;
  int lost;  // servo periods without a reply
} jpins_t;

typedef struct {
  jpins_t j[RB_MAXJ];
  hal_s32_t *force_link;
  hal_float_t *force[6];
  hal_float_t *pay[4];
  double pay_now[4];
} pins_t;

static int comp_id;
static pins_t *pins;

static double theta0(int j) {
  return arm_P.arm.link[j].theta0 / D2R;
}

static void update(void *arg, long period) {
  pins_t *p = arg;
  for(int j = 0; j < arm_nj; j++) {
    jpins_t *jp = &p->j[j];
    armcore_set_cmd(j, *jp->enable, *jp->pos_cmd - theta0(j), *jp->vel_cmd);
    arm_drv[j].link_lost = *jp->link_lost;
    jp->lost = *jp->link_lost ? jp->lost + 1 : 0;
  }
  if(memcmp(p->pay_now, (double[4]){*p->pay[0], *p->pay[1], *p->pay[2], *p->pay[3]}, sizeof(p->pay_now))) {
    for(int k = 0; k < 4; k++) {
      p->pay_now[k] = *p->pay[k];
    }
    armcore_payload(p->pay_now[0], p->pay_now + 1);
  }
  int fl = *p->force_link;
  arm_P.fe_on = fl >= 1 && fl <= arm_nj;
  if(arm_P.fe_on) {
    arm_P.fe.link = fl - 1;
    for(int k = 0; k < 3; k++) {
      arm_P.fe.f[k] = *p->force[k];
      arm_P.fe.r[k] = *p->force[3 + k];
    }
  }

  armcore_servo(period);

  double jtf[RB_MAXJ] = {0};
  if(arm_P.fe_on) {
    double z[RB_MAXJ] = {0}, a[RB_MAXJ], b[RB_MAXJ];
    rb_rnea(&arm_P.arm, arm_P.q, z, z, 0, 0, 0, a);
    rb_rnea(&arm_P.arm, arm_P.q, z, z, 0, &arm_P.fe, 1, b);
    for(int j = 0; j < arm_nj; j++) {
      jtf[j] = a[j] - b[j];
    }
  }
  for(int j = 0; j < arm_nj; j++) {
    jpins_t *jp = &p->j[j];
    *jp->pos_fb = armcore_fb(j) + theta0(j);
    *jp->vel_fb = armcore_fb_vel(j);
    *jp->current = arm_drv[j].pd.current;
    *jp->fault = arm_drv[j].pd.fault || jp->lost >= 3;
    *jp->q = arm_P.q[j] / D2R + theta0(j);
    *jp->tau_ext = jtf[j];
    *jp->tau_gear = arm_P.tau_g[j];
    *jp->stop_hits = arm_P.stop_hits[j];
  }
}

static int export_pins(void) {
  int r = 0;
  for(int j = 0; j < arm_nj && !r; j++) {
    jpins_t *jp = &pins->j[j];
    r |= hal_pin_float_newf(HAL_IN, &jp->pos_cmd, comp_id, PREFIX ".%d.pos-cmd", j);
    r |= hal_pin_float_newf(HAL_IN, &jp->vel_cmd, comp_id, PREFIX ".%d.vel-cmd", j);
    r |= hal_pin_bit_newf(HAL_IN, &jp->enable, comp_id, PREFIX ".%d.enable", j);
    r |= hal_pin_float_newf(HAL_OUT, &jp->pos_fb, comp_id, PREFIX ".%d.pos-fb", j);
    r |= hal_pin_float_newf(HAL_OUT, &jp->vel_fb, comp_id, PREFIX ".%d.vel-fb", j);
    r |= hal_pin_float_newf(HAL_OUT, &jp->current, comp_id, PREFIX ".%d.current", j);
    r |= hal_pin_bit_newf(HAL_OUT, &jp->fault, comp_id, PREFIX ".%d.fault", j);
    r |= hal_pin_float_newf(HAL_OUT, &jp->q, comp_id, PREFIX ".%d.q", j);
    r |= hal_pin_float_newf(HAL_OUT, &jp->tau_ext, comp_id, PREFIX ".%d.tau-ext", j);
    r |= hal_pin_float_newf(HAL_OUT, &jp->tau_gear, comp_id, PREFIX ".%d.tau-gear", j);
    r |= hal_pin_s32_newf(HAL_OUT, &jp->stop_hits, comp_id, PREFIX ".%d.stop-hits", j);
    r |= hal_pin_bit_newf(HAL_IN, &jp->link_lost, comp_id, PREFIX ".%d.link-lost", j);
  }
  static const char *fn[6] = {"x", "y", "z", "rx", "ry", "rz"};
  r |= hal_pin_s32_newf(HAL_IN, &pins->force_link, comp_id, PREFIX ".force-link");
  for(int k = 0; k < 6 && !r; k++) {
    r |= hal_pin_float_newf(HAL_IN, &pins->force[k], comp_id, PREFIX ".force-%s", fn[k]);
  }
  static const char *pn[4] = {"mass", "cx", "cy", "cz"};
  for(int k = 0; k < 4 && !r; k++) {
    r |= hal_pin_float_newf(HAL_IN, &pins->pay[k], comp_id, PREFIX ".payload-%s", pn[k]);
  }
  return r;
}

int rtapi_app_main(void) {
  comp_id = hal_init("stmbl_arm");
  if(comp_id < 0) {
    return comp_id;
  }
  if(!arm || !arm[0] || !lib || !lib[0]) {
    rtapi_print_msg(RTAPI_MSG_ERR, "stmbl_arm: arm=<arm file> and lib=<libstmbl_drive.so> are required\n");
    hal_exit(comp_id);
    return -EINVAL;
  }
  if(armcore_load(arm, lib)) {
    rtapi_print_msg(RTAPI_MSG_ERR, "stmbl_arm: cannot load %s\n", arm);
    hal_exit(comp_id);
    return -EINVAL;
  }
  pins = hal_malloc(sizeof(pins_t));
  if(!pins || export_pins()) {
    rtapi_print_msg(RTAPI_MSG_ERR, "stmbl_arm: cannot export pins\n");
    hal_exit(comp_id);
    return -ENOMEM;
  }
  if(hal_export_funct(PREFIX, update, pins, 1, 0, comp_id)) {
    rtapi_print_msg(RTAPI_MSG_ERR, "stmbl_arm: cannot export function\n");
    hal_exit(comp_id);
    return -EINVAL;
  }
  rtapi_print_msg(RTAPI_MSG_INFO, "stmbl_arm: %d joints from %s\n", arm_nj, arm);
  hal_ready(comp_id);
  return 0;
}

void rtapi_app_exit(void) {
  hal_exit(comp_id);
}
