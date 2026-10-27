/*
 * stmbl_arm: the simulated arm with one simulated STMBL drive per joint
 * (host/arm/armcore.c) as a LinuxCNC realtime component (uspace).
 *
 *   loadrt stmbl_arm arm=/path/arcmate.arm lib=/path/libstmbl_drive.so [cell=/path/cell.txt]
 *   addf stmbl-arm.0 servo-thread
 *
 * Per joint J (0-based, as LinuxCNC's joints), the pins of a drive's
 * smart-serial link, in joint degrees in genserkins' convention (the DH
 * theta, i.e. the model angle + theta0):
 *   stmbl-arm.0.J.pos-cmd, .vel-cmd, .enable  (in)
 *   stmbl-arm.0.J.pos-fb, .vel-fb, .current, .power, .fault  (out)
 * and the truth of the simulation, for tests:
 *   stmbl-arm.0.J.q          link angle [deg, DH theta]
 *   stmbl-arm.0.J.tau-ext    joint torque of the external force (J^T f) [Nm]
 *   stmbl-arm.0.J.tau-gear   gear torque [Nm]
 *   stmbl-arm.0.J.stop-hits  hard stop hits
 *   stmbl-arm.0.contact-force  sum of the contact forces with the cell's
 *                            obstacles [N]; .clearance: the arm's smallest
 *                            distance to them [mm] (cell=, else 0)
 * An external force on a link (collision tests):
 *   stmbl-arm.0.force-link (s32, 1..6, 0 = none), .force-x/-y/-z [N, base
 *   frame], .force-rx/-ry/-rz [m, link frame]
 * Fault injection and payload:
 *   stmbl-arm.0.J.link-lost  in: the drive gets no more packets; after 3
 *                            servo periods without a reply .fault rises, as
 *                            hm2's smart-serial error wired into amp-fault
 *   stmbl-arm.0.payload-mass, .payload-cx/-cy/-cz  [kg, m in the last link's
 *                            frame], applied when they change
 * With a gripper in the arm file (gripper drive ..., host/arm/gripsim.h),
 * its drive's smart-serial pins, in mm of finger gap:
 *   stmbl-arm.0.grip.pos-cmd, .vel-cmd, .enable  (in)
 *   stmbl-arm.0.grip.pos-fb, .current, .power, .fault  (out)
 * and the truth: .grip.force (contact force per finger [N]), .grip.held
 * (the part held, -1 none); .grip.drop (in, rising edge): the held part
 * slips out. Parts: `part` lines in the cell file.
 * With a tool changer (changer ..., host/arm/changersim.h):
 *   stmbl-arm.0.changer.valve      in   the lock valve (locked when set)
 *   stmbl-arm.0.changer.locked, .unlocked, .present  out  its sensors
 *   stmbl-arm.0.changer.id         out  (s32) the coded tool ID (0 none)
 *   stmbl-arm.0.changer.slip       in   rising edge: the tool comes off
 *   stmbl-arm.0.changer.plant-tool out  (s32) the truth: the tool on it
 * The cell file's obstacle, contact and part lines are the plant's;
 * `include FILE` reads another (relative to the including one).
 * Each drive's console is not exposed; its config is the ARMFILE's.
 */
#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "armcore.h"

MODULE_AUTHOR("stmbl");
MODULE_DESCRIPTION("simulated arm on simulated STMBL drives");
MODULE_LICENSE("GPL");

static char *arm = "";
RTAPI_MP_STRING(arm, "arm description file (host/arm/arcmate/arcmate.arm)");
static char *lib = "";
RTAPI_MP_STRING(lib, "libstmbl_drive.so");
static char *cell = "";
RTAPI_MP_STRING(cell, "cell file: its obstacle and contact lines become the plant's");

#define PREFIX "stmbl-arm.0"
#define D2R (M_PI / 180.0)

typedef struct {
  hal_float_t *pos_cmd, *vel_cmd, *pos_fb, *vel_fb, *current, *power, *q, *tau_ext, *tau_gear;
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
  hal_float_t *contact_force, *clearance;
  // the gripper
  hal_float_t *g_cmd, *g_vel, *g_fb, *g_cur, *g_pow, *g_force;
  hal_bit_t *g_en, *g_fault, *g_drop;
  hal_s32_t *g_held;
  int g_drop_last;
  // the tool changer
  hal_bit_t *c_valve, *c_locked, *c_unlocked, *c_present, *c_slip;
  hal_s32_t *c_id, *c_tool;
  int c_slip_last;
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

  if(arm_has_grip) {
    armcore_grip_cmd(*p->g_en, *p->g_cmd, *p->g_vel);
    if(*p->g_drop && !p->g_drop_last) {
      armcore_grip_drop();
    }
    p->g_drop_last = *p->g_drop;
  }

  if(arm_cs.on) {
    armcore_changer_valve(*p->c_valve);
    if(*p->c_slip && !p->c_slip_last) {
      arm_cs.slip = 1;
    }
    p->c_slip_last = *p->c_slip;
  }

  armcore_servo(period);

  if(arm_cs.on) {
    int l, u, pr, id;
    armcore_changer_sensors(&l, &u, &pr, &id);
    *p->c_locked = l, *p->c_unlocked = u, *p->c_present = pr, *p->c_id = id;
    *p->c_tool = arm_cs.mounted;
  }

  if(arm_has_grip) {
    *p->g_fb = armcore_grip_fb();
    *p->g_cur = arm_gdrv.pd.current;
    *p->g_pow = arm_gdrv.pd.power;
    *p->g_fault = arm_gdrv.pd.fault;
    *p->g_force = arm_gs.force;
    *p->g_held = gs_held(&arm_gs);
  }

  double jtf[RB_MAXJ] = {0};
  if(arm_P.fe_on) {
    double z[RB_MAXJ] = {0}, a[RB_MAXJ], b[RB_MAXJ];
    rb_rnea(&arm_P.arm, arm_P.q, z, z, 0, 0, 0, a);
    rb_rnea(&arm_P.arm, arm_P.q, z, z, 0, &arm_P.fe, 1, b);
    for(int j = 0; j < arm_nj; j++) {
      jtf[j] = a[j] - b[j];
    }
  }
  *p->contact_force = arm_P.contact_f;
  if(arm_P.geo.no > 0) {
    *p->clearance = gm_clearance(&arm_P.geo, NULL) * 1000.0;  // bodies placed by the plant
  }
  for(int j = 0; j < arm_nj; j++) {
    jpins_t *jp = &p->j[j];
    *jp->pos_fb = armcore_fb(j) + theta0(j);
    *jp->vel_fb = armcore_fb_vel(j);
    *jp->current = arm_drv[j].pd.current;
    *jp->power = arm_drv[j].pd.power;
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
    r |= hal_pin_float_newf(HAL_OUT, &jp->power, comp_id, PREFIX ".%d.power", j);
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
  r |= hal_pin_float_newf(HAL_OUT, &pins->contact_force, comp_id, PREFIX ".contact-force");
  r |= hal_pin_float_newf(HAL_OUT, &pins->clearance, comp_id, PREFIX ".clearance");
  static const char *pn[4] = {"mass", "cx", "cy", "cz"};
  for(int k = 0; k < 4 && !r; k++) {
    r |= hal_pin_float_newf(HAL_IN, &pins->pay[k], comp_id, PREFIX ".payload-%s", pn[k]);
  }
  if(arm_cs.on && !r) {
    r |= hal_pin_bit_newf(HAL_IN, &pins->c_valve, comp_id, PREFIX ".changer.valve");
    r |= hal_pin_bit_newf(HAL_OUT, &pins->c_locked, comp_id, PREFIX ".changer.locked");
    r |= hal_pin_bit_newf(HAL_OUT, &pins->c_unlocked, comp_id, PREFIX ".changer.unlocked");
    r |= hal_pin_bit_newf(HAL_OUT, &pins->c_present, comp_id, PREFIX ".changer.present");
    r |= hal_pin_s32_newf(HAL_OUT, &pins->c_id, comp_id, PREFIX ".changer.id");
    r |= hal_pin_bit_newf(HAL_IN, &pins->c_slip, comp_id, PREFIX ".changer.slip");
    r |= hal_pin_s32_newf(HAL_OUT, &pins->c_tool, comp_id, PREFIX ".changer.plant-tool");
    if(!r) {
      *pins->c_valve = arm_cs.valve;  // as at start (a tool on: locked)
    }
  }
  if(arm_has_grip && !r) {
    r |= hal_pin_float_newf(HAL_IN, &pins->g_cmd, comp_id, PREFIX ".grip.pos-cmd");
    r |= hal_pin_float_newf(HAL_IN, &pins->g_vel, comp_id, PREFIX ".grip.vel-cmd");
    r |= hal_pin_bit_newf(HAL_IN, &pins->g_en, comp_id, PREFIX ".grip.enable");
    r |= hal_pin_float_newf(HAL_OUT, &pins->g_fb, comp_id, PREFIX ".grip.pos-fb");
    r |= hal_pin_float_newf(HAL_OUT, &pins->g_cur, comp_id, PREFIX ".grip.current");
    r |= hal_pin_float_newf(HAL_OUT, &pins->g_pow, comp_id, PREFIX ".grip.power");
    r |= hal_pin_bit_newf(HAL_OUT, &pins->g_fault, comp_id, PREFIX ".grip.fault");
    r |= hal_pin_float_newf(HAL_OUT, &pins->g_force, comp_id, PREFIX ".grip.force");
    r |= hal_pin_s32_newf(HAL_OUT, &pins->g_held, comp_id, PREFIX ".grip.held");
    r |= hal_pin_bit_newf(HAL_IN, &pins->g_drop, comp_id, PREFIX ".grip.drop");
  }
  return r;
}

// the cell's obstacle, contact and part lines (and its includes)
static int read_cell(const char *path, int depth) {
  FILE *f = fopen(path, "r");
  char line[512];
  if(!f || depth > 4) {
    if(f) {
      fclose(f);
    }
    return -1;
  }
  while(fgets(line, sizeof(line), f)) {
    char kw[16], name[256], inc[768];
    if(sscanf(line, " %15s", kw) != 1) {
      continue;
    }
    if(!strcmp(kw, "include") && sscanf(line, " include %255s", name) == 1) {
      const char *sl = strrchr(path, '/');
      if(name[0] == '/' || !sl) {
        snprintf(inc, sizeof(inc), "%s", name);
      } else {
        snprintf(inc, sizeof(inc), "%.*s/%s", (int)(sl - path), path, name);
      }
      if(read_cell(inc, depth + 1)) {
        fclose(f);
        return -1;
      }
    } else if(!strcmp(kw, "obstacle") || !strcmp(kw, "contact") || (!strcmp(kw, "part") && arm_has_grip)) {
      armcore_line(line);
    }
  }
  fclose(f);
  return 0;
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
  if(cell && cell[0] && read_cell(cell, 0)) {  // the cell's obstacles push back on the arm
    rtapi_print_msg(RTAPI_MSG_ERR, "stmbl_arm: cannot read %s\n", cell);
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
