/*
 * armkins: kinematics of a serial arm for LinuxCNC (uspace), genserkins'
 * convention (Craig DH, joints = DH theta [deg], A B C with
 * R = Rz(C) Ry(B) Rx(A)), plus the tool on a tool changer: the Cartesian
 * position is the tool's TCP (shared/robot/tool.h). Switchable
 * kinematics: the switchkins type is the tool ID (0: the flange), from the
 * tool table, so a tool change switches the TCP.
 *
 *   loadrt armkins model=/path/model.arm tools=/path/tools.txt
 *   (as [KINS]KINEMATICS; loaded before motmod)
 *   net ... motion.analog-out-03 => motion.switchkins-type
 *   G-code: M68 E3 Q<tool> then M66 E0 L0 (the interpreter resyncs)
 *
 * model: its dh lines (armcollide's model file); tools: the tool table.
 * Pins: armkins.tool (s32 out) the TCP's tool now; armkins.ik-fails (s32
 * out) inverse kinematics that did not converge.
 */
#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"
#include "kinematics.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "kin.h"
#include "tool.h"

MODULE_AUTHOR("stmbl");
MODULE_DESCRIPTION("serial arm kinematics with the tool changer's TCP");
MODULE_LICENSE("GPL");

static char *model = "";
RTAPI_MP_STRING(model, "file with the arm's dh lines");
static char *tools = "";
RTAPI_MP_STRING(tools, "tool table (shared/robot/tool.h)");

#define D2R (M_PI / 180.0)

static int comp_id;
static rb_model mdl;
static rb_tools tt;
static int cur;
static struct {
  hal_s32_t *tool, *ik_fails;
} *pins;

int kinematicsForward(const double *joint, EmcPose *world, const KINEMATICS_FORWARD_FLAGS *fflags,
                      KINEMATICS_INVERSE_FLAGS *iflags) {
  double q[RB_MAXJ], p[3], R[3][3], rpy[3];
  (void)fflags, (void)iflags;
  for(int j = 0; j < mdl.n; j++) {
    q[j] = joint[j] * D2R - mdl.link[j].theta0;
  }
  rb_tcp_pose(&mdl, rb_tool_get(&tt, cur), q, p, R);
  rb_rpy_from_R(R, rpy);
  world->tran.x = p[0] * 1000.0;
  world->tran.y = p[1] * 1000.0;
  world->tran.z = p[2] * 1000.0;
  world->a = rpy[0] / D2R;
  world->b = rpy[1] / D2R;
  world->c = rpy[2] / D2R;
  world->u = world->v = world->w = 0.0;
  return 0;
}

int kinematicsInverse(const EmcPose *world, double *joint, const KINEMATICS_INVERSE_FLAGS *iflags,
                      KINEMATICS_FORWARD_FLAGS *fflags) {
  double q[RB_MAXJ], p[3], R[3][3], rpy[3];
  (void)iflags, (void)fflags;
  for(int j = 0; j < mdl.n; j++) {  // from the current joints: the nearest solution
    q[j] = joint[j] * D2R - mdl.link[j].theta0;
  }
  p[0] = world->tran.x / 1000.0;
  p[1] = world->tran.y / 1000.0;
  p[2] = world->tran.z / 1000.0;
  rpy[0] = world->a * D2R;
  rpy[1] = world->b * D2R;
  rpy[2] = world->c * D2R;
  rb_R_from_rpy(rpy, R);
  if(rb_ik_tcp(&mdl, rb_tool_get(&tt, cur), p, R, q, 1e-9, 1e-9, 100) < 0) {
    if(pins) {
      (*pins->ik_fails)++;
    }
    return -1;
  }
  for(int j = 0; j < mdl.n; j++) {
    joint[j] = (q[j] + mdl.link[j].theta0) / D2R;
  }
  return 0;
}

int kinematicsHome(EmcPose *world, double *joint, KINEMATICS_FORWARD_FLAGS *fflags,
                   KINEMATICS_INVERSE_FLAGS *iflags) {
  *fflags = 0;
  *iflags = 0;
  return kinematicsForward(joint, world, fflags, iflags);
}

KINEMATICS_TYPE kinematicsType(void) {
  return KINEMATICS_BOTH;
}

int kinematicsSwitchable(void) {
  return 1;
}

int kinematicsSwitch(int type) {
  if(type < 0 || type >= RB_MAXTOOL || (type && tt.t[type].id != type)) {
    rtapi_print_msg(RTAPI_MSG_ERR, "armkins: no tool %d in the tool table\n", type);
    return -1;
  }
  cur = type;
  if(pins) {
    *pins->tool = cur;
  }
  return 0;
}

EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsInverse);
EXPORT_SYMBOL(kinematicsHome);
EXPORT_SYMBOL(kinematicsSwitchable);
EXPORT_SYMBOL(kinematicsSwitch);

static int read_dh(const char *path) {
  FILE *f = fopen(path, "r");
  char line[512];
  if(!f) {
    return -1;
  }
  memset(&mdl, 0, sizeof(mdl));
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

int rtapi_app_main(void) {
  char err[256] = "";
  comp_id = hal_init("armkins");
  if(comp_id < 0) {
    return comp_id;
  }
  rb_tools_init(&tt);
  if(!model[0] || read_dh(model)) {
    rtapi_print_msg(RTAPI_MSG_ERR, "armkins: model=<file with 6 dh lines> needed (%s)\n", model);
    hal_exit(comp_id);
    return -EINVAL;
  }
  if(tools[0] && rb_tools_file(&tt, tools, err, sizeof(err))) {
    rtapi_print_msg(RTAPI_MSG_ERR, "armkins: %s\n", err);
    hal_exit(comp_id);
    return -EINVAL;
  }
  pins = hal_malloc(sizeof(*pins));
  if(!pins || hal_pin_s32_new("armkins.tool", HAL_OUT, &pins->tool, comp_id) ||
     hal_pin_s32_new("armkins.ik-fails", HAL_OUT, &pins->ik_fails, comp_id)) {
    hal_exit(comp_id);
    return -ENOMEM;
  }
  *pins->tool = 0;
  *pins->ik_fails = 0;
  hal_ready(comp_id);
  return 0;
}

void rtapi_app_exit(void) {
  hal_exit(comp_id);
}
