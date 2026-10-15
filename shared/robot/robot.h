/*
 * Rigid-body dynamics of a serial arm with revolute joints (up to RB_MAXJ).
 *
 * Geometry: Craig's (modified) Denavit-Hartenberg parameters, the same as
 * LinuxCNC's genserkins (A-i, ALPHA-i, D-i): the frame of link i is reached
 * from the frame of link i-1 by RotX(alpha) TransX(a) RotZ(theta) TransZ(d),
 * theta = q + theta0. Units SI (m, kg, rad).
 *
 * Inertial parameters per link, in the link's own frame, in the form the
 * dynamics are linear in: mass m, first moment h = m * c (c = centre of
 * mass) and the inertia tensor I about the link frame origin (not about the
 * centre of mass); rb_link_from_com converts. I[6] = Ixx Iyy Izz Ixy Ixz Iyz
 * are tensor elements (Ixy = -integral(x y dm)).
 *
 * The armature matrix (motor rotors reflected through the gears, including
 * wrist coupling, see rb_armature) adds arm * qdd to the joint torques.
 *
 * Plain C99, double precision, no allocation, no globals: usable in the host
 * simulator, a LinuxCNC realtime component and from Python (ctypes).
 */
#ifndef ROBOT_H
#define ROBOT_H

#define RB_MAXJ 6
#define RB_NPAR 10  // inertial parameters per link

typedef struct {
  double a, alpha, d, theta0;  // Craig DH
  double m;                    // mass [kg]
  double h[3];                 // first moment m * c [kg m]
  double I[6];                 // inertia about the frame origin [kg m^2]
} rb_link;

typedef struct {
  int n;                            // joints
  rb_link link[RB_MAXJ];
  double g[3];                      // gravity in the base frame [m/s^2]
  double arm[RB_MAXJ][RB_MAXJ];     // armature (joint space) [kg m^2]
} rb_model;

// A force on one link: f [N] in the base frame, applied at r [m] given in
// that link's frame.
typedef struct {
  int link;
  double r[3];
  double f[3];
} rb_force;

// Joint friction, smooth (usable in an observer): Coulomb fc, breakaway fs
// (Stribeck, decaying with speed scale vs), viscous fv; eps is the speed
// over which the sign function is smoothed [rad/s].
typedef struct {
  double fc, fs, vs, fv, eps;
} rb_fric;

void rb_link_from_com(rb_link *l, double m, const double c[3], const double Ic[6]);

// Frames of the links in the base frame: R[i] (columns = link axes), p[i].
void rb_fk(const rb_model *mdl, const double *q, double R[][3][3], double p[][3]);
// A point r (link frame) of link k in the base frame.
void rb_point(const rb_model *mdl, const double *q, int k, const double r[3], double out[3]);

// Inverse dynamics: joint torques for q, qd, qdd (gravity on if gravity != 0)
// with external forces fe[0..nf-1] acting on the arm.
void rb_rnea(const rb_model *mdl, const double *q, const double *qd, const double *qdd,
             int gravity, const rb_force *fe, int nf, double *tau);
void rb_mass(const rb_model *mdl, const double *q, double M[RB_MAXJ][RB_MAXJ]);
void rb_gravity(const rb_model *mdl, const double *q, double *tau);
// Coriolis/centrifugal torques C(q, qd) qd.
void rb_coriolis(const rb_model *mdl, const double *q, const double *qd, double *tau);
// Forward dynamics; returns 0, or -1 if M is not positive definite.
int rb_fd(const rb_model *mdl, const double *q, const double *qd, const double *tau,
          const rb_force *fe, int nf, double *qdd);
void rb_energy(const rb_model *mdl, const double *q, const double *qd, double *kin, double *pot);

// Regressor: tau = Y * pi, pi = the RB_NPAR parameters of every link
// (m, hx, hy, hz, Ixx, Iyy, Izz, Ixy, Ixz, Iyz) followed by the n diagonal
// armature terms. Columns: n * RB_NPAR + n.
#define RB_NCOL (RB_MAXJ * RB_NPAR + RB_MAXJ)
void rb_regressor(const rb_model *mdl, const double *q, const double *qd, const double *qdd,
                  double Y[RB_MAXJ][RB_NCOL]);
void rb_params_get(const rb_model *mdl, double *pi);
void rb_params_set(rb_model *mdl, const double *pi);

// Armature from motor rotor inertias jm[] (motor side) and the transmission
// q = K theta_m (gear ratios on the diagonal, wrist coupling off it):
// arm = K^-T diag(jm) K^-1. Returns -1 if K is singular.
int rb_armature(rb_model *mdl, const double K[RB_MAXJ][RB_MAXJ], const double *jm);

double rb_friction(const rb_fric *f, double qd);

// Cholesky of the n x n matrix A (in place, lower); solve A x = b.
int rb_chol(int n, double A[RB_MAXJ][RB_MAXJ]);
void rb_chol_solve(int n, const double L[RB_MAXJ][RB_MAXJ], const double *b, double *x);

// Generalised momentum observer (De Luca): r estimates the external joint
// torques, filtered with gain k [1/s] (time constant 1 / k), from the
// measured joint torques tau (motors, minus the friction model if used).
typedef struct {
  double k;
  double r[RB_MAXJ];
  double integ[RB_MAXJ];
  double p0[RB_MAXJ];
  double Mprev[RB_MAXJ][RB_MAXJ];
  double qdprev[RB_MAXJ];
  double bprev[RB_MAXJ];  // tau - C qd - g of the previous step
  int init;
} rb_observer;

void rb_observer_reset(rb_observer *o);
void rb_observer_step(rb_observer *o, const rb_model *mdl, const double *q, const double *qd,
                      const double *tau, double dt);

#endif
