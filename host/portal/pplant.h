/*
 * Simulated portal (gantry) machine: a planar multibody model.
 *
 *   frame    the machine's base, standing on leveling feet: springs and
 *            dampers to the floor in x, y and yaw (Xf, Yf, psi)
 *   bridge   moves along the frame's Y rails (y), driven by two linear
 *            motors Y1 / Y2 at the rails (x = -L/2, +L/2); it can rack
 *            (yaw theta) against the guide blocks' moment stiffness when
 *            they don't move together
 *   carriage moves along the bridge (x), driven by the X motor
 *   head     the tool head (Z column, spindle) on the carriage, on springs
 *            (hx, hy): its Y stiffness is lowest at mid-span, where the
 *            bridge bends most
 *
 * Generalized coordinates q = Xf Yf psi y theta x hx hy; the motor and
 * guide forces act between the bodies, on the relative coordinates, so the
 * reactions on the frame follow from the equations of motion. The mass
 * matrix M(q) and the velocity terms are built from the bodies' positions
 * (numerical Jacobians), solved every step: M qdd = Q - sum m J^T Jd qd.
 *
 * Machine coordinates (what the scales and LinuxCNC see): relative to the
 * frame, x from the bridge's centre, y along the rails from the frame's
 * centre. The tool point relative to the frame (the work on the table) is
 * what the machine cuts. Units: m, kg, s, rad, N.
 */
#ifndef PPLANT_H
#define PPLANT_H

#define PP_N 8
enum { PP_XF, PP_YF, PP_PSI, PP_Y, PP_TH, PP_X, PP_HX, PP_HY };
enum { PP_Y1, PP_Y2, PP_XM };  // the motors

typedef struct {
  // frame on its feet
  double mf, jf;            // mass [kg], yaw inertia [kg m^2]
  double kf[3], cf[3];      // feet stiffness x, y [N/m], yaw [Nm/rad]; damping
  // bridge, carriage, head
  double L;                 // rail spacing: Y1 at x = -L/2, Y2 at +L/2
  double kr, cr;            // racking: the guide blocks' yaw stiffness [Nm/rad], damping
  double mb, jb, mc, jc, mh, jh;
  double kx, cx;            // head on the carriage, x
  double ky_end, ky_mid, cy;  // head y: stiffness at the rails / mid-span
  // guides: Coulomb [N] (regularized below vs [m/s]) and viscous [N s/m]
  double fy_c, fy_v;        // per Y side
  double fy2_c, fy2_v;      // the Y2 side alone, when it differs (an idler rail; < 0: as Y1)
  double fx_c, fx_v;
  double vs;
  // cogging (iron-core motors): a force -A sin(2 pi s/period + phase) on
  // motor k at its scale position s (A 0: none)
  double cog_a[3], cog_per[3], cog_ph[3];
  // strokes (end stops, spring kstop / damper cstop)
  double ymin, ymax, xmin, xmax, kstop, cstop;

  double q[PP_N], qd[PP_N], qdd[PP_N];
  double F[3];              // motor forces, last step [N]
  int stop_hits[3];
  int at_stop[3];
} pp_plant;

void pp_defaults(pp_plant *p);
// damping from a ratio of critical for the stiffness on a mass
double pp_damp(double k, double m, double zeta);
// head y stiffness at carriage position x
double pp_ky(const pp_plant *p, double x);
// advance by dt with motor forces F[PP_Y1..PP_XM]
void pp_step(pp_plant *p, const double *F, double dt);

// the motors' positions and speeds along their scales [m, m/s]
void pp_motor(const pp_plant *p, int k, double *pos, double *vel);
// tool point relative to the frame [m], and its velocity
void pp_tool(const pp_plant *p, double out[2], double vel[2]);
// total momentum in the floor's frame (x, y) [kg m/s]: constant without feet
void pp_momentum(const pp_plant *p, double out[2]);
// kinetic + spring energy [J]
double pp_energy(const pp_plant *p);
// the force the feet put on the floor (x, y) [N]
void pp_feet(const pp_plant *p, double out[2]);

#endif
