/*
 * The simulated arm with one simulated stmbl drive per joint, shared by
 * arm_sim (scripts) and the LinuxCNC component (lcnc/stmbl_arm.c).
 *
 * The drives are copies of libstmbl_drive.so, each in its own dlmopen
 * namespace. Every 200 us rt cycle (armcore_tick): rotor angle and speed
 * into each drive, one drive tick, its mean electrical torque back, then
 * the plant (plant.c) in four 50 us steps. The LinuxCNC side writes the
 * process data with armcore_set_cmd before every 1 ms (5 ticks) servo
 * period; armcore_servo exchanges it on the first tick, as hm2 does.
 *
 * Joint angles at this interface are the model's q (degrees). Each drive's
 * position feedback starts wherever its encoder says; armcore keeps an
 * offset per joint so that feedback = q of the motor (absolute encoders
 * with a calibrated zero), set at the first exchange and by armcore_home.
 */
#ifndef ARMCORE_H
#define ARMCORE_H

#include "plant.h"
#include "gripsim.h"
#include "changersim.h"
#include "stmbl_sim_api.h"

#define ARM_RT 0.0002
#define ARM_SUB 4  // plant steps per rt cycle

typedef struct {
  void *h;
  int (*boot)(const char *, const char *);
  void (*tick)(void);
  void (*update)(stmbl_sim_pd_t *, long);
  float *(*pin)(const char *);
  void (*cmd)(const char *);
  float *sim_pos, *sim_vel, *te_int, *brake_rel;
  stmbl_sim_pd_t pd;
  double ratio;
  double off;  // joint q [deg] = pos_fb + off
  int link_lost;  // fault injection: the drive gets no more packets
} arm_drive;

extern arm_drive arm_drv[RB_MAXJ];
extern int arm_nj;
extern arm_plant arm_P;
extern long arm_ticks;
extern gs_gripper arm_gs;      // the simulated gripper (gripsim.h)
extern arm_drive arm_gdrv;     // its drive
extern int arm_has_grip;
extern cs_changer arm_cs;      // the simulated tool changer (changersim.h)

// Load ARMFILE (format: arm_sim.c); drive configs relative to its
// directory, the drive library from libpath. 0, or -1 with a message on
// stderr.
int armcore_load(const char *armfile, const char *libpath);
// Read a description file (its "include FILE" lines too, relative to the
// arm file's directory): 0, or -1 with a message.
int armcore_include(const char *file);
// One arm description line (dh, link, motor, couple, gear, fric, limits,
// home, drive): 1 if understood.
int armcore_line(char *line);
// Rest the arm at q [deg] (and have LinuxCNC's offsets set again).
void armcore_home(const double *q_deg);

// LinuxCNC side, joint j: enable, position [deg, model q], velocity [deg/s].
void armcore_set_cmd(int j, int en, double pos, double vel);
double armcore_fb(int j);       // position feedback [deg, model q]
double armcore_fb_vel(int j);   // [deg/s]
double armcore_qm(int j);       // joint angle the motor encoder sees [deg]
int armcore_synced(void);       // the feedback offsets are set
// The gripper's drive (LinuxCNC side): enable, gap [mm], gap rate [mm/s];
// its gap feedback [mm]; let the held part slip out.
void armcore_grip_cmd(int en, double gap, double vel);
double armcore_grip_fb(void);
void armcore_grip_drop(void);
// Payload on the last link: mass [kg] at c [m, its frame]; replaces the
// previous one (m 0: none). The tool changer's tool is a second one
// (armcore_tool_payload, set by the changer simulation itself).
int armcore_payload(double m, const double c[3]);
int armcore_tool_payload(double m, const double c[3]);
// The tool changer (LinuxCNC side): the lock valve; the sensors.
void armcore_changer_valve(int lock);
void armcore_changer_sensors(int *locked, int *unlocked, int *present, int *id);

// One rt cycle; servo != 0 exchanges the process data first.
void armcore_tick(int servo);
// One servo period: period_ns of rt cycles, exchange on the first.
void armcore_servo(long period_ns);

#endif
