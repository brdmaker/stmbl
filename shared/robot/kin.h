/*
 * Kinematics of a serial arm (robot.h's model): flange pose, Jacobian,
 * inverse kinematics, and orientation in LinuxCNC genserkins' convention.
 *
 * The pose is that of the last link's frame (genserkins' tool point with
 * no tool offset): position p [m] and rotation R (columns = frame axes) in
 * the base frame. A, B, C as genserkins: roll a, pitch b, yaw c with
 * R = Rz(c) Ry(b) Rx(a), radians here.
 */
#ifndef KIN_H
#define KIN_H

#include "robot.h"

void rb_pose(const rb_model *mdl, const double *q, double p[3], double R[3][3]);
void rb_R_from_rpy(const double rpy[3], double R[3][3]);
void rb_rpy_from_R(const double R[3][3], double rpy[3]);
// Rotation vector (axis * angle) of R, and R from one.
void rb_rotvec_from_R(const double R[3][3], double w[3]);
void rb_R_from_rotvec(const double w[3], double R[3][3]);
// Geometric Jacobian of the flange: rows 0-2 linear, 3-5 angular velocity
// (base frame), per joint speed.
void rb_jacobian(const rb_model *mdl, const double *q, double J[6][RB_MAXJ]);
// Inverse kinematics by damped least squares from the start q (in/out, the
// nearest solution: give the current joints). Converged when the position
// error < tol_p [m] and the rotation error < tol_r [rad]; returns the
// iterations used, or -1 (q is then the last iterate).
int rb_ik(const rb_model *mdl, const double p[3], const double R[3][3], double *q, double tol_p,
          double tol_r, int maxit);

#endif
