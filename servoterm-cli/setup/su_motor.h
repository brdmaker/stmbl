/*
 * Setup wizard: the motor's parameters, the drive limits that follow from
 * them, and plausibility checks.
 *
 * No motor database: the wizard asks the few nameplate values a builder can
 * read (currents, speed, torque if given) and identifies the rest on the
 * drive (idpmsm: r, l, pole pairs, commutation offset, phase order, psi;
 * idm: j, d, f). The nameplate then cross-checks the identification (the
 * torque constant), and the limits are derived from both.
 *
 * Units as in conf0: currents are peak (amplitude) A, nameplates give rms;
 * velocities rad/s (mechanical), torque Nm, psi V*s/rad (Wb, per pole pair).
 */
#pragma once
#include <stddef.h>

// the stmbl v4 HV board (docs "Specs"): UVW 28 A peak, 17 A average; the
// wizard stays below the peak with a margin for the overcurrent filter
#define SU_DRIVE_MAX_AC_CUR 25.0
#define SU_DRIVE_CONT_CUR 17.0
#define SU_DRIVE_MAX_BUS 380.0

typedef struct {
  // nameplate; 0 = unknown
  double rated_cur;     // [A rms]
  double peak_cur;      // [A rms]
  double rated_torque;  // [Nm]
  double max_rpm;       // [rpm]
  // identified; 0 = not yet
  double r, l, psi;
  int pp;
  double j, d, f, o;
} su_motor_t;

typedef struct {
  double max_ac_cur;  // [A peak] conf0.max_ac_cur: the drive's current clamp
  double cur_boost;   // iit0.cur_boost: max_ac_cur / continuous (thermal model)
  double max_force;   // [Nm] conf0.max_force
  double max_vel;     // [rad/s] conf0.max_vel
  double volt_vel;    // [rad/s] no-load speed at the bus voltage (0: psi unknown)
  double test_cur;    // [A] idpmsm0.test_cur: the identification current
} su_limits_t;

// limits for bus voltage bus [V]; uses what is known
void su_motor_limits(const su_motor_t *m, double bus, su_limits_t *out);

// the torque constant [Nm / A rms] from psi and pp (identified), or from the
// nameplate (rated torque / rated current); 0 when unknown
double su_kt_identified(const su_motor_t *m);
double su_kt_nameplate(const su_motor_t *m);

// problems with the identified values: each appends a line to msg; returns
// the number of errors (implausible: don't use), *warnings the number of
// warnings (unusual: check)
int su_motor_check(const su_motor_t *m, double bus, char *msg, size_t len, int *warnings);

// The motor's temperature sensor (the drive's mtemp0, shared/mtemp.h): ids
// in the order of the firmware's conf0.mot_temp_type
#define SU_TEMP_N 6
extern const char *const su_temp_ids[SU_TEMP_N];    // none kty84 pt1000 ptc ntc switch
extern const char *const su_temp_names[SU_TEMP_N];
int su_temp_type(const char *id);  // conf0.mot_temp_type, 0 for none / unknown
// is the sensor's reading with the motor cold plausible? ohm: hv0.mot_temp,
// degc: mtemp0.sensor, err: mtemp0.err (1 open, 2 short), tripped:
// mtemp0.tripped. 0, or 1 with the reason in msg
int su_temp_check(int type, double ohm, double degc, double err, double tripped, char *msg, size_t len);
