/*
 * The simulated portal machine with its three simulated stmbl drives (Y1,
 * Y2 and X, linear motors), shared by portal_sim (scripts) and the LinuxCNC
 * component (lcnc/stmbl_portal.c). Built like host/arm/armcore.c: each
 * drive is a copy of libstmbl_drive.so in its own dlmopen namespace; every
 * 200 us rt cycle the motors' positions go to the drives, one drive tick,
 * their forces back, then the plant (pplant.c) in four 50 us steps. The
 * LinuxCNC side sets the process data before each 1 ms servo period.
 *
 * A linear motor runs as a rotary one with one revolution per electrical
 * period p: position [rad] = s 2pi/p, force = torque 2pi/p. LinuxCNC
 * positions are mm along each scale (process data scale = p in mm), with
 * an offset set at the first exchanges (absolute scales): the machine's
 * X = x + x0, Y = y + y0 (portal file `origin`).
 */
#ifndef PORTALCORE_H
#define PORTALCORE_H

#include "pplant.h"
#include "stmbl_sim_api.h"

#define PT_RT 0.0002
#define PT_SUB 4

typedef struct {
  void *h;
  int (*boot)(const char *, const char *);
  void (*tick)(void);
  void (*update)(stmbl_sim_pd_t *, long);
  float *(*pin)(const char *);
  void (*cmd)(const char *);
  float *sim_pos, *sim_vel, *te_int, *udc, *p_in;
  stmbl_sim_pd_t pd;
  double period;  // electrical period [m]
  double off;     // LinuxCNC position [mm] = pos_fb + off
  int on;
} pt_drive;

// The DC bus the three drives share (portal file `bus`): mains through a
// diode bridge charge the capacitance C (the supply's and the drives' own)
// up to the mains peak; the drives draw their input power (pe0.p_in) from
// it and feed back what they brake (negative). Nothing returns energy to
// the mains: braking raises the voltage, until a brake chopper (on above
// v_on, off 10 V lower) burns it in R or the drives trip on overvoltage
// (conf0.max_dc_volt). Without `bus`: a fixed voltage (sim.txt).
typedef struct {
  int on;
  double C, vrms, freq, rsrc;  // [F], mains [V rms], [Hz], source resistance [ohm]
  double v_on, r_ch;           // chopper: switch-on voltage [V], resistor [ohm] (0: none)
  double v, v_max, v_min;      // bus voltage now / max / min since cleared [V]
  double e_ch, e_regen;        // energy burnt in the chopper, fed back by the drives [J]
  double p;                    // the drives' total power, last step [W]
  int ch;                      // chopper conducting
} pt_bus;

extern pt_drive pt_drv[3];  // PP_Y1, PP_Y2, PP_XM
extern pt_bus pt_B;
extern pp_plant pt_P;
extern double pt_x0, pt_y0;  // machine origin [mm]: X = x + x0, Y = y + y0
extern long pt_ticks;

// Load a portal file (format: portal_sim.c); drive configs relative to its
// directory. 0, or -1 with a message on stderr.
int ptcore_load(const char *file, const char *libpath);
int ptcore_line(char *line);  // one portal line: 1 if understood
// put the machine at rest at X, Y [mm] (and set the offsets again)
void ptcore_home(double X, double Y);

// LinuxCNC side, drive k: enable, position [mm], velocity [mm/s]
void ptcore_set_cmd(int k, int en, double pos, double vel);
double ptcore_fb(int k);      // position feedback [mm]
double ptcore_fb_vel(int k);  // [mm/s]
int ptcore_synced(void);
// what a drive's scale reads [mm, LinuxCNC's frame]
double ptcore_scale(int k);
// the tool point relative to the frame in LinuxCNC's X, Y [mm]
void ptcore_tool(double out[2]);

void ptcore_tick(int servo);
void ptcore_servo(long period_ns);

#endif
