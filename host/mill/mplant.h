/*
 * Simulated fixed-bridge mill (an engraver / HSC router like the Scripta
 * CN 720): three ball-screw axes on a base that stands on feet.
 *
 *   frame    the base with the table's guides; on its feet (springs and
 *            dampers to the floor in x and y)
 *   table    Y: moves on the base, driven by the Y screw. Machine Y is the
 *            tool relative to the table, so the table moves the other way
 *   bridge   fixed to the base by its columns, which bend in y: the bridge
 *            (with the X carriage, Z slide and head) sways relative to the
 *            base when the table's reaction shakes the base
 *   carriage X: moves along the bridge (rigid in x), carries the Z slide
 *   Z slide  Z: moves vertically on the carriage, gravity pulls it down
 *   head     the spindle's tip as a modal mass on springs to the carriage
 *            (x) and the bridge (y); both soften as Z extends downwards
 *
 * Each axis: the motor's rotor (with the coupling and the screw's own
 * inertia) turns the screw; the nut pushes the load through the axial
 * stiffness of the screw (E A / L, L from the fixed bearing to the nut, so
 * it changes with position) in series with the nut, the bearing and the
 * coupling. The reactions go into the base (X: through the rigid bridge).
 *
 * Positions: s[] are the loads relative to the base along the scales
 * (machine coordinates without compliance): s[MX] carriage, s[MY] machine
 * Y (= minus the table's travel), s[MZ] the Z slide, up positive. The tool
 * relative to the work: x = s[MX] + hx, y = s[MY] + yb + hy, z = s[MZ].
 * Units: m, kg, s, rad, N.
 */
#ifndef MPLANT_H
#define MPLANT_H

enum { MX, MY, MZ };

typedef struct {
  // screw drives, per axis
  double p[3];               // pitch [m] (one motor turn)
  double lead[3];            // the nut's real travel per turn / p - 1: lead error, a warm screw
  double jm[3];              // motor side: rotor, coupling, screw [kg m^2]
  double ea[3];              // the screw's E A [N]
  double l0[3], ldir[3];     // nut distance from the fixed bearing at s = 0 [m], +1/-1 as s grows
  double ka[3];              // nut, bearing, coupling in series [N/m]
  double zeta[3];            // screw mode damping ratio
  double tc[3], tv[3];       // motor side friction: Coulomb [Nm], viscous [Nm s/rad]
  double fc[3], fv[3];       // guide friction on the load: Coulomb [N], viscous [N s/m]
  double smin[3], smax[3];   // end stops [m]
  // bodies
  double mf;                 // base (with what is rigid on it)
  double kfx, cfx, kfy, cfy; // feet
  double mt;                 // table and work (the Y load)
  double mb;                 // bridge beam (sways in y)
  double kb, cb;             // columns in y
  double mc;                 // X carriage (without the Z slide)
  double mz;                 // Z slide and spindle body, without the tip's modal mass
  double mh;                 // the head tip's modal mass
  double khx[2], khy[2];     // its stiffness with Z at the top / at the bottom [N/m]
  double zh;                 // its damping ratio
  double g;
  double vs;                 // Coulomb regularization speed [m/s]

  // state
  double phi[3], w[3];       // motor angle [rad] (unwrapped), speed [rad/s]
  double s[3], v[3];         // loads along the scales [m], [m/s]
  double xf, vxf, yf, vyf;   // base on its feet (absolute)
  double yb, vyb;            // bridge relative to the base
  double hx, vhx, hy, vhy;   // head tip relative to the carriage / bridge
  double cut[3];             // force on the tool from the work [N] (reaction on the table)
  int brake[3];              // the motor's holding brake grips: the rotor stands
  double F[3];               // screw forces, last step [N]
  double acc[3];             // load accelerations along the scales, last step
  int hits[3];
} mp_plant;

void mp_defaults(mp_plant *p);
double mp_damp(double k, double m, double zeta);
// axial stiffness of axis k's drive at its position [N/m]
double mp_kscrew(const mp_plant *p, int k);
// head stiffness at the Z slide's position
void mp_khead(const mp_plant *p, double *kx, double *ky);
// advance by dt with motor torques T[3] [Nm]
void mp_step(mp_plant *p, const double *T, double dt);
// the tool relative to the work, machine axes [m]
void mp_tool(const mp_plant *p, double out[3]);

#endif
