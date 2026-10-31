/*
 * The simulated fixed-bridge mill (mplant.c) with its three simulated stmbl
 * drives, shared by mill_sim (scripts) and the LinuxCNC component
 * (lcnc/stmbl_mill.c). Built like host/portal/portalcore.c: each drive is a
 * copy of libstmbl_drive.so in its own dlmopen namespace. Every 200 us rt
 * cycle the motors' angles (and a scale's position) go to the drives, one
 * drive tick, their torques back, then the plant in 10 steps.
 *
 * A packet reaches the drive `latency` after LinuxCNC sends it and is
 * taken at the next rt cycle. A servo period that isn't a whole number of
 * rt cycles (0.5 ms = 2.5) gives the drives their packets 2 and 3 rt cycles
 * apart, as on the real link, whose drive clock locks to the packet stream.
 * The drives' vel_int0.cmd_freq follows the servo period.
 *
 * LinuxCNC positions are mm along each axis, with an offset set at the
 * first exchanges (absolute positions): machine X = mccore_fb(MX) ...
 *
 * The machine file (mm for machine positions, else SI):
 *   drive x|y|z CONF SIM PITCH       drive config (a,b,...: files joined), sim-only
 *                                    lines, screw pitch [mm]
 *   screw x|y|z JM EA L0 LDIR KA ZETA   motor side inertia (rotor, coupling,
 *                                    screw) [kg m^2], the screw's E A [N], nut
 *                                    distance from the fixed bearing at 0 [m]
 *                                    and +1/-1 as the axis grows, nut + bearing
 *                                    + coupling stiffness [N/m], damping ratio
 *   fric x|y|z TC TV FC FV           motor side Coulomb [Nm] / viscous [Nm s/rad],
 *                                    guide Coulomb [N] / viscous [N s/m]
 *   stroke x|y|z MIN MAX             end stops [mm]
 *   frame M FX FY ZETA               the base [kg] on its feet: modes [Hz]
 *   table M / carriage M / zslide M  [kg]
 *   bridge M FY ZETA                 the bridge beam [kg]; the columns' y mode
 *                                    [Hz] with everything it carries
 *   head M FX_TOP FX_BOT FY_TOP FY_BOT ZETA   the spindle tip's modal mass and
 *                                    its modes [Hz] with Z at the top / bottom
 *   scale x|y|z RES [NOISE]          a linear scale on the axis [um per count]
 *                                    (and its noise, um rms): the drive's
 *                                    enc_fb1 reads it (the drive config links
 *                                    it as the joint feedback)
 *   lead x|y|z UM_PER_M              the nut travels this much more per metre than
 *                                    the motor's turns say: the screw's lead error,
 *                                    or its growth when warm (11.5 um/m per K)
 *   brake x|y|z                      a holding brake on the motor: the rotor
 *                                    stands unless fault0.mot_brake releases it
 *   rnoise x|y|z LSB                 motor feedback noise, rms in counts of
 *                                    enc_fb0.res (a resolver's jitter)
 * A five-axis machine (drives a and c too: the trunnion's tilt about X and
 * the rotary table on it; LinuxCNC's xyzac-trt-kins). Angles in degrees:
 *   drive a|c CONF SIM DEG           DEG: the axis' degrees a motor turn (360 / the gear ratio)
 *   rotary a|c JM KT JL ZETA         motor side inertia [kg m^2], the drive train's torsional
 *                                    stiffness at the axis [Nm/rad], what turns [kg m^2], damping
 *   fric a|c TC TV FC FV             as above; FC [Nm], FV [Nm s/rad] at the axis
 *   stroke a|c MIN MAX               [deg]
 *   unbalance ME OFF                 A's unbalance m e [kg m], its angle from straight down [deg]
 *   trt XR YR ZR DY DZ [TOOL]        xyzac-trt-kins' x/y/z-rot-point, y/z-offset, tool-offset [mm]:
 *                                    the work frame the tool is judged in (mccore_work)
 *   home X Y Z [A C]
 *   servo MS                         LinuxCNC's servo period [ms] (1, 0.5, ...)
 *   latency US                       the packet's way from LinuxCNC into the drive
 *                                    (50 us); it is taken at the next rt cycle
 *   include FILE                     machine lines from FILE (relative to this one)
 *   home X Y Z                       start position [mm]
 *
 */
#ifndef MILLCORE_H
#define MILLCORE_H

#include "mplant.h"
#include "stmbl_sim_api.h"

#define MC_RT 0.0002

typedef struct {
  void *h;
  int (*boot)(const char *, const char *);
  void (*tick)(void);
  void (*update)(stmbl_sim_pd_t *, long);
  float *(*pin)(const char *);
  void (*cmd)(const char *);
  float *sim_pos, *sim_vel, *te_int, *scale_pos, *scale_abs, *brake;
  stmbl_sim_pd_t pd;
  double off;            // LinuxCNC position [mm] = pos_fb + off
  double scale_res;      // [m per count] (0: no scale)
  double scale_noise;    // [m rms]
  double rnoise;         // motor feedback noise [rad rms]
  int has_brake;         // a holding brake (machine line brake)
  int on;
} mc_drive;

extern mc_drive mc_drv[MP_NAX];  // MX, MY, MZ (, MA, MC)
extern mp_plant mc_P;
extern double mc_servo_t;   // the machine file's servo period [s] (mill_sim's; LinuxCNC has its own)
extern long mc_ticks;       // rt cycles run

// Load a machine file (drive configs relative to it); lib: libstmbl_drive.so.
// 0, or -1 with a message on stderr.
int mccore_load(const char *file, const char *lib);
int mccore_line(char *line);  // one machine line: 1 if understood
int mccore_axis(const char *s);  // "x" "y" "z" "a" "c" -> MX MY MZ MA MC, else -1
void mccore_home(const double *h);  // at rest there [mm, deg] (nax values)

// LinuxCNC's side, axis k: enable, position [mm], velocity [mm/s]
void mccore_set_cmd(int k, int en, double pos, double vel);
double mccore_fb(int k);      // position feedback [mm]
double mccore_fb_vel(int k);  // [mm/s]
int mccore_synced(void);      // the offsets are set
double mccore_scale(int k);   // where the load really is [mm]
// one servo period [s]: the packet at its start, the rt cycles up to the next
void mccore_servo(double period);
// the tool relative to the work at LinuxCNC's time t [s] (the plant moved
// back along its velocity from the last rt cycle) [mm]
void mccore_tool(double t, double out[3]);
// the rotary axes' angles at time t [deg] (A, C; 0 on a three-axis machine)
void mccore_angles(double t, double out[2]);

// xyzac-trt-kins (LinuxCNC 2.9 trtfuncs.c), its parameters from the machine's trt line [mm]
typedef struct {
  double xr, yr, zr, dy, dz, tool;
} mc_trt_t;
extern mc_trt_t mc_trt;
// joints X Y Z [mm] A C [deg] -> the tool tip in the work's frame [mm], and back
void mccore_trt_fwd(const double *j, double out[3]);
void mccore_trt_inv(const double *w, double a, double c, double *j);
// the tool tip at time t in the work's frame (the simulated tool and table angles) [mm]
void mccore_work(double t, double out[3]);

#endif
