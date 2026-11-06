#include "su_motor.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static double clamp(double x, double lo, double hi) {
  return x < lo ? lo : x > hi ? hi : x;
}

void su_motor_limits(const su_motor_t *m, double bus, su_limits_t *o) {
  memset(o, 0, sizeof(*o));
  double rated = m->rated_cur > 0.0 ? m->rated_cur * M_SQRT2 : 0.0;  // peak
  double peak  = m->peak_cur > 0.0 ? m->peak_cur * M_SQRT2 : 3.0 * rated;  // servo motors: ~3x
  if(peak <= 0.0) {
    peak = 2.0;  // nothing known: a current no motor this drive takes minds
  }
  o->max_ac_cur = clamp(peak, 0.1, SU_DRIVE_MAX_AC_CUR);
  double cont   = rated > 0.0 ? fmin(rated, SU_DRIVE_CONT_CUR) : o->max_ac_cur / 3.0;
  o->cur_boost  = clamp(o->max_ac_cur / cont, 1.0, 5.0);
  // identification: enough current to measure and to turn a heavy rotor
  // (the pole-pair step's field has to pull it along), well below what the
  // motor takes
  o->test_cur = clamp(0.5 * (rated > 0.0 ? rated : o->max_ac_cur / 3.0), 0.2, 8.0);

  if(m->psi > 0.0 && m->pp > 0) {
    o->max_force = 1.5 * m->pp * m->psi * o->max_ac_cur;
    o->volt_vel  = bus / sqrt(3.0) * 0.95 / (m->pp * m->psi);
    o->max_vel   = 0.9 * o->volt_vel;  // headroom for the current loop
  }
  if(m->max_rpm > 0.0) {
    double v   = m->max_rpm * 2.0 * M_PI / 60.0;
    o->max_vel = o->max_vel > 0.0 ? fmin(o->max_vel, v) : v;
  }
}

double su_kt_identified(const su_motor_t *m) {
  // torque = 1.5 pp psi i_peak = 1.5 pp psi sqrt(2) i_rms
  return m->psi > 0.0 && m->pp > 0 ? 1.5 * m->pp * m->psi * M_SQRT2 : 0.0;
}

double su_kt_nameplate(const su_motor_t *m) {
  return m->rated_torque > 0.0 && m->rated_cur > 0.0 ? m->rated_torque / m->rated_cur : 0.0;
}

static void add(char *msg, size_t len, const char *fmt, ...) {
  size_t n = strlen(msg);
  if(n + 1 >= len) {
    return;
  }
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg + n, len - n, fmt, ap);
  va_end(ap);
  n = strlen(msg);
  if(n + 1 < len) {
    msg[n]     = '\n';
    msg[n + 1] = '\0';
  }
}

int su_motor_check(const su_motor_t *m, double bus, char *msg, size_t len, int *warnings) {
  int err = 0, warn = 0;
  if(len) {
    msg[0] = '\0';
  }
  if(!(m->r > 0.01 && m->r < 100.0)) {
    add(msg, len, "error: winding resistance %.4g ohm is outside 0.01..100: check the motor wiring", m->r);
    err++;
  }
  if(!(m->l > 1e-5 && m->l < 0.5)) {
    add(msg, len, "error: inductance %.4g H is outside 10 uH..0.5 H", m->l);
    err++;
  } else if(m->r > 0.0) {
    double tau = m->l / m->r;
    if(tau < 5e-5) {
      add(msg, len, "warning: L/R = %.3g ms is very short: the current loop runs close to its limit", tau * 1e3);
      warn++;
    }
  }
  if(m->pp < 1 || m->pp > 30) {
    add(msg, len, "error: %d pole pairs: check the encoder resolution and that the motor turned freely", m->pp);
    err++;
  }
  if(!(m->psi > 1e-4 && m->psi < 2.0)) {
    add(msg, len, "error: flux linkage %.4g Wb is implausible", m->psi);
    err++;
  }
  double kn = su_kt_nameplate(m), ki = su_kt_identified(m);
  if(kn > 0.0 && ki > 0.0) {
    double ratio = ki / kn;
    if(ratio < 0.5 || ratio > 2.0) {
      add(msg, len,
          "error: torque constant %.3g Nm/A (identified) vs %.3g Nm/A (nameplate): off by %.0f %%; the pole pairs "
          "or the flux are wrong (check the encoder resolution, that the motor turned freely, and the nameplate)",
          ki, kn, (ratio - 1.0) * 100.0);
      err++;
    } else if(ratio < 0.7 || ratio > 1.4) {
      add(msg, len,
          "warning: torque constant %.3g Nm/A (identified) vs %.3g Nm/A (nameplate): off by %.0f %%; check the "
          "pole pairs and the encoder resolution",
          ki, kn, (ratio - 1.0) * 100.0);
      warn++;
    }
  }
  if(m->psi > 0.0 && m->pp > 0 && m->max_rpm > 0.0 && bus > 0.0) {
    double need = m->psi * m->pp * m->max_rpm * 2.0 * M_PI / 60.0 * sqrt(3.0);  // line-line peak
    if(need > 0.95 * bus) {
      add(msg, len,
          "warning: at %.0f rpm the motor's back-EMF is %.0f V, about the bus (%.0f V): the top speed is limited "
          "to %.0f rpm",
          m->max_rpm, need, bus, m->max_rpm * 0.9 * bus / need);
      warn++;
    }
  }
  if(m->j < 0.0 || (m->j > 0.0 && (m->j < 1e-7 || m->j > 1.0))) {
    add(msg, len, "error: inertia %.3g kg m^2 is implausible", m->j);
    err++;
  }
  if(m->f < 0.0 || m->d < 0.0) {
    add(msg, len, "error: negative friction or damping: the motion identification was disturbed");
    err++;
  }
  if(warnings) {
    *warnings = warn;
  }
  return err;
}

const char *const su_temp_ids[SU_TEMP_N]   = {"none", "kty84", "pt1000", "ptc", "ntc", "switch"};
const char *const su_temp_names[SU_TEMP_N] = {
    "none (the drive's thermal model alone)", "KTY84-130 (SEW CMP \"KTY\", Siemens, ...)", "PT1000",
    "PTC thermistor (trips, DIN 44081)",       "NTC thermistor",                          "thermal switch (normally closed)"};

int su_temp_type(const char *id) {
  for(int i = 0; id && i < SU_TEMP_N; i++) {
    if(strcmp(id, su_temp_ids[i]) == 0) {
      return i;
    }
  }
  return 0;
}

int su_temp_check(int type, double ohm, double degc, double err, double tripped, char *msg, size_t len) {
  msg[0] = 0;
  if(type <= 0) {
    return 0;
  }
  if(err == 1.0) {
    snprintf(msg, len,
             "The temperature sensor reads open (%.0f ohm): a broken or unplugged wire, or nothing on the HV\n"
             "board's motor temperature connector.",
             ohm);
    return 1;
  }
  if(err == 2.0) {
    snprintf(msg, len,
             "The temperature sensor reads %.0f ohm, too low for a %s: shorted wires, or another\n"
             "sensor type (a KTY84 reads ~580 ohm cold, a PT1000 ~1100, a PTC 50..250 per sensor).",
             ohm, su_temp_ids[type]);
    return 1;
  }
  if(type == 3 || type == 5) {
    if(tripped > 0.0) {
      snprintf(msg, len,
               "The %s reads tripped (%.0f ohm) with the motor cold: an open wire, or not a %s.",
               type == 3 ? "PTC" : "thermal switch", ohm, type == 3 ? "PTC" : "normally closed switch");
      return 1;
    }
    return 0;
  }
  if(degc < 5.0 || degc > 45.0) {
    snprintf(msg, len,
             "The temperature sensor reads %.0f degC (%.0f ohm) with the motor cold: another sensor type\n"
             "than %s (a KTY84 reads ~580 ohm at 20 degC, a PT1000 ~1080, an NTC 10k ~12k),\n"
             "or the motor isn't at room temperature.",
             degc, ohm, su_temp_ids[type]);
    return 1;
  }
  return 0;
}
