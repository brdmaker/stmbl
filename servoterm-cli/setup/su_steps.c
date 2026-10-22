#include "su_steps.h"
#include "su_conf.h"
#include "su_feedback.h"
#include "su_motor.h"
#include "su_version.h"
#include "su_wiring.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

// fault0.state (shared/common.h state_t)
#define ST_DISABLED 0
#define ST_ENABLED 1
#define ST_SOFT_FAULT 3

// ---- helpers ---------------------------------------------------------------

static void say(su_wizard_t *w, su_msg_t kind, const char *fmt, ...) {
  char buf[4096];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  w->ui->say(w->ui->user, kind, buf);
}

static double now(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + tv.tv_usec / 1e6;
}

// wait for drive time t (wall time t / time_scale), reading the drive
static void drive_sleep(su_wizard_t *w, double t) {
  su_link_pump(w->link, t / (w->time_scale > 1.0 ? w->time_scale : 1.0));
}

static const char *out_path(su_wizard_t *w, const char *name, char *buf, size_t len) {
  snprintf(buf, len, "%s/%s", w->out_dir ? w->out_dir : ".", name);
  return buf;
}

static void stamp(char *buf, size_t len, const char *fmt) {
  time_t t = time(NULL);
  strftime(buf, len, fmt, localtime(&t));
}

// a number: the session's value is the suggestion (resume); returns
// 0, SU_UNKNOWN (key unset) or SU_ABORT
static int ask_num(su_wizard_t *w, const char *key, const char *q, const char *unit, double def, double min,
                   double max, int unknown_ok, double *out) {
  double d = su_get_num(w->st, key, def);
  int r    = w->ui->number(w->ui->user, key, q, unit, d, min, max, unknown_ok, out);
  if(r == 0) {
    su_set_num(w->st, key, *out);
  } else if(r == SU_UNKNOWN) {
    su_set(w->st, key, "unknown");
  }
  return r;
}

// one of ids[], shown as names[]; stores the id
static int ask_choice(su_wizard_t *w, const char *key, const char *q, const char *const *ids,
                      const char *const *names, int n, int def, int *out) {
  const char *cur = su_get(w->st, key);
  for(int i = 0; cur && i < n; i++) {
    if(strcmp(ids[i], cur) == 0) {
      def = i;
    }
  }
  int r = w->ui->choose(w->ui->user, key, q, ids, names, n, def, out);
  if(r == 0) {
    su_set(w->st, key, ids[*out]);
  }
  return r;
}

static int ask_yes(su_wizard_t *w, const char *key, const char *q, int *yes) {
  int r = w->ui->confirm(w->ui->user, key, q, yes);
  if(r == 0) {
    su_set_num(w->st, key, *yes);
  }
  return r;
}

static int get(su_wizard_t *w, const char *pin, double *v) {
  if(su_link_get(w->link, pin, v) < 0) {
    say(w, SU_MSG_ERROR, "no answer from the drive for %s", pin);
    return -1;
  }
  return 0;
}

static int set(su_wizard_t *w, const char *pin, const char *value) {
  if(su_link_set(w->link, pin, value) < 0) {
    say(w, SU_MSG_ERROR, "the drive refused \"%s = %s\"", pin, value);
    return -1;
  }
  return 0;
}

static int set_num(su_wizard_t *w, const char *pin, double v) {
  char val[32];
  snprintf(val, sizeof(val), "%.6g", v);
  return set(w, pin, val);
}

// a console command whose answer doesn't matter (link, relink, start)
static int cmd(su_wizard_t *w, const char *line) {
  if(!su_link_query(w->link, line, 0.3, NULL, 5.0)) {
    say(w, SU_MSG_ERROR, "the drive stopped answering (%s)", line);
    return -1;
  }
  return 0;
}

static int disable(su_wizard_t *w) {
  return set(w, "fault0.en", "0");
}

// the drive's fault, if it has one: 0 none
static int drive_fault(su_wizard_t *w, double *code) {
  double s = 0.0;
  *code    = 0.0;
  if(su_link_get(w->link, "fault0.state", &s) < 0) {
    return -1;
  }
  if((int)(s + 0.5) == ST_SOFT_FAULT) {
    su_link_get(w->link, "fault0.fault", code);
    return 1;
  }
  return 0;
}

// poll pin until it reaches done (an identification's last state); the
// motor moves meanwhile. 0 done, -1 timeout / no answer, -2 drive fault
static int wait_state(su_wizard_t *w, const char *pin, double done, double timeout, const char *what) {
  double end = now() + timeout / (w->time_scale > 1.0 ? w->time_scale : 1.0), last = -1.0;
  while(now() < end) {
    double s, f;
    if(su_link_get(w->link, pin, &s) < 0) {
      return -1;
    }
    if(s != last) {
      w->ui->progress(w->ui->user, what, -1.0);
      last = s;
    }
    if(s >= done - 0.05) {
      return 0;
    }
    int fl = drive_fault(w, &f);
    if(fl > 0) {
      say(w, SU_MSG_ERROR, "the drive faulted (fault %.0f) during %s", f, what);
      return -2;
    }
    su_link_pump(w->link, 0.25);
  }
  say(w, SU_MSG_ERROR, "%s did not finish in time", what);
  return -1;
}

// write the config for the current state, reset the drive, quiet its scope
static int apply_config(su_wizard_t *w, int final) {
  char *conf = su_build_config(w->st, final);
  char err[256];
  int r = su_conf_write(w->link, conf, err, sizeof(err));
  free(conf);
  if(r < 0) {
    say(w, SU_MSG_ERROR, "writing the config failed: %s", err);
    return -1;
  }
  if(su_link_reset(w->link, 20.0) < 0) {
    say(w, SU_MSG_ERROR, "the drive did not come back after the reset");
    return -1;
  }
  // the scope stream would fill the buffers while the wizard waits for the user
  set(w, "term0.send_step", "0");
  return 0;
}

// load an identification template on the running drive
static int load_template(su_wizard_t *w, const char *name) {
  char line[64];
  snprintf(line, sizeof(line), "link %s", name);
  if(cmd(w, line) < 0 || cmd(w, "relink") < 0 || cmd(w, "start") < 0) {
    return -1;
  }
  set(w, "term0.send_step", "0");
  return 0;
}

static int enable(su_wizard_t *w) {
  if(set(w, "fault0.en", "0") < 0) {
    return -1;
  }
  drive_sleep(w, 0.2);
  return set(w, "fault0.en", "1");
}

static void motor_from_state(const su_state_t *st, su_motor_t *m) {
  memset(m, 0, sizeof(*m));
  m->rated_cur    = su_get_num(st, "motor.rated_cur", 0.0);
  m->peak_cur     = su_get_num(st, "motor.peak_cur", 0.0);
  m->rated_torque = su_get_num(st, "motor.rated_torque", 0.0);
  m->max_rpm      = su_get_num(st, "motor.max_rpm", 0.0);
  m->r            = su_get_num(st, "id.r", 0.0);
  m->l            = su_get_num(st, "id.l", 0.0);
  m->psi          = su_get_num(st, "id.psi", 0.0);
  m->pp           = (int)su_get_num(st, "id.pp", 0.0);
  m->j            = su_get_num(st, "mech.j", 0.0);
  m->d            = su_get_num(st, "mech.d", 0.0);
  m->f            = su_get_num(st, "mech.f", 0.0);
  m->o            = su_get_num(st, "mech.o", 0.0);
}

static double bus_of(const su_state_t *st) {
  return su_get_num(st, "supply.bus", 325.0);
}

// ---- the config --------------------------------------------------------------

// the outputs hold the axis this long after the brake is told to grip: the
// measured value (brake step), else the datasheet's engage time + 50 %
// + 20 ms, else 0.15 s
static double brake_time_of(const su_state_t *st) {
  if(su_has(st, "brake.time")) {
    return su_get_num(st, "brake.time", 0.15);
  }
  double ms = su_get_num(st, "brake.engage_ms", -1.0);
  return ms > 0.0 ? ms * 1.5e-3 + 0.02 : 0.15;
}

static char *put(char *c, const su_state_t *st, const char *key, const char *pin, const char *tag) {
  const char *v = su_get(st, key);
  if(v && strcmp(v, "unknown") != 0) {
    c = su_conf_set(c, pin, v, tag);
  }
  return c;
}

char *su_build_config(const su_state_t *st, int final) {
  char *c = strdup("");
  c       = su_conf_link(c, "pid");
  c       = su_conf_link(c, "pmsm");
  const su_fb_type_t *fb = su_fb_find(su_get(st, "feedback.type"));
  if(fb) {
    c = su_conf_link(c, fb->link);
  }
  const char *lk = su_get(st, "link.kind");
  if(final && lk && strcmp(lk, "none") != 0) {
    c = su_conf_link(c, lk);
  }
  if(fb && fb->extra) {
    c = su_conf_set(c, "fb_switch0.mot_pos", strchr(fb->extra, '=') + 2, NULL);
  }
  if(su_get_num(st, "motor.brake", 0.0) > 0.0) {
    char out[16];
    snprintf(out, sizeof(out), "io0.out%d", (int)su_get_num(st, "brake.output", 0.0));
    c = su_conf_set(c, out, "fault0.mot_brake", "holding brake");
    c = su_conf_set_num(c, "fault0.brake_time", brake_time_of(st), "brake engage");
  }

  // supply
  if(su_get_num(st, "supply.chopper", 0.0) > 0.0) {
    double on = su_get_num(st, "supply.chopper_on", 375.0);
    c         = su_conf_set_num(c, "conf0.high_dc_volt", on + 5.0, "chopper + 5 V");
    c         = su_conf_set_num(c, "conf0.max_dc_volt", fmin(on + 20.0, 395.0), "chopper + 20 V");
  }
  c = put(c, st, "supply.max_hv_power", "conf0.max_hv_power", "setup");

  // feedback
  c = put(c, st, "feedback.counts", "conf0.mot_fb_res", "setup: measured");
  c = put(c, st, "feedback.rev", "conf0.mot_fb_rev", "setup: hand turn");
  c = put(c, st, "feedback.poles", "conf0.mot_fb_polecount", "setup");

  // identified
  c = put(c, st, "id.r", "conf0.r", "id_pmsm");
  c = put(c, st, "id.l", "conf0.l", "id_pmsm");
  c = put(c, st, "id.pp", "conf0.polecount", "id_pmsm");
  c = put(c, st, "id.offset", "conf0.mot_fb_offset", "id_pmsm");
  c = put(c, st, "id.out_rev", "conf0.out_rev", "id_pmsm");
  c = put(c, st, "id.psi", "conf0.psi", "id_pmsm");
  c = put(c, st, "mech.j", "conf0.j", "id_mot");
  c = put(c, st, "mech.d", "conf0.d", "id_mot");
  c = put(c, st, "mech.f", "conf0.f", "id_mot");
  c = put(c, st, "mech.o", "conf0.o", "id_mot");
  c = put(c, st, "tune.pos_bw", "conf0.pos_bw", "id_pid");
  c = put(c, st, "tune.vel_bw", "conf0.vel_bw", "id_pid");
  c = put(c, st, "tune.vel_d", "conf0.vel_d", "id_pid");

  // limits: from what is known so far (the limits step may lower them)
  su_motor_t m;
  su_limits_t lim;
  motor_from_state(st, &m);
  su_motor_limits(&m, bus_of(st), &lim);
  c = su_conf_set_num(c, "conf0.max_ac_cur", su_get_num(st, "limits.max_ac_cur", lim.max_ac_cur), "setup");
  c = su_conf_set_num(c, "iit0.cur_boost", lim.cur_boost, "setup");
  if(lim.max_force > 0.0) {
    c = su_conf_set_num(c, "conf0.max_force", su_get_num(st, "limits.max_force", lim.max_force), "setup");
  }
  if(lim.max_vel > 0.0) {
    c = su_conf_set_num(c, "conf0.max_vel", su_get_num(st, "limits.max_vel", lim.max_vel), "setup");
  }

  // the command link
  if(final && lk && strcmp(lk, "sserial") == 0) {
    c = put(c, st, "link.scale", "linrev0.scale", "units / rev");
  }
  return c;
}

// ---- steps -------------------------------------------------------------------

static su_result_t step_version(su_wizard_t *w) {
  const char *t = su_link_query(w->link, "about", 0.3, NULL, 3.0);
  if(!t) {
    say(w, SU_MSG_ERROR, "no answer to \"about\": is this an stmbl drive's console?");
    return SU_FAIL;
  }
  su_fw_t fw;
  su_fw_parse(t, &fw);
  su_fw_check_t r = su_fw_check(&fw, w->api, w->git);
  char buf[512];
  su_fw_explain(r, &fw, w->api, w->git, buf, sizeof(buf));
  if(r == SU_FW_NO_API || r == SU_FW_API_DIFFERS) {
    say(w, SU_MSG_ERROR, "%s", buf);
    return SU_FAIL;
  }
  say(w, r == SU_FW_OK ? SU_MSG_INFO : SU_MSG_WARN, "%s", buf);
  su_set(w->st, "drive.firmware", fw.have_version ? fw.git : "unknown");
  return SU_OK;
}

static su_result_t step_backup(su_wizard_t *w) {
  char *conf = su_conf_read(w->link);
  if(!conf) {
    say(w, SU_MSG_ERROR, "could not read the drive's config");
    return SU_FAIL;
  }
  char name[64], path[512];
  stamp(name, sizeof(name), "stmbl-backup-%Y%m%d-%H%M%S.txt");
  FILE *f = fopen(out_path(w, name, path, sizeof(path)), "w");
  if(!f) {
    say(w, SU_MSG_ERROR, "cannot write %s", path);
    free(conf);
    return SU_FAIL;
  }
  fputs(conf, f);
  fclose(f);
  say(w, SU_MSG_INFO,
      "The drive's config (%zu bytes) is saved in %s. To go back to it: servoterm-cli --load %s, then reset.",
      strlen(conf), path, path);
  su_set(w->st, "backup.file", path);
  free(conf);
  return SU_OK;
}

static su_result_t step_safety(su_wizard_t *w) {
  say(w, SU_MSG_INFO,
      "This wizard sets up one stmbl drive with a three-phase servo motor (PMSM / brushless AC):\n"
      "it asks for the nameplate values, helps with the wiring, then measures the motor on the\n"
      "drive and tunes it. During the measurements the motor turns on its own, several turns,\n"
      "with up to a few amps.\n"
      "  - Have an emergency stop that cuts the HV supply within reach.\n"
      "  - The motor must be fastened, nothing loose on the shaft (a key!).\n"
      "  - The wizard writes the drive's config; the old one is backed up.\n"
      "You can stop at any question; run the wizard again with the same session file to go on.");
  int yes = 0;
  if(ask_yes(w, "safety.ok", "Emergency stop within reach and the motor fastened?", &yes) == SU_ABORT) {
    return SU_ABORTED;
  }
  if(!yes) {
    say(w, SU_MSG_WARN, "Then not yet: run the wizard again when it is.");
    return SU_ABORTED;
  }
  return SU_OK;
}

static su_result_t step_supply(su_wizard_t *w) {
  static const char *const ids[]   = {"230", "120", "dc"};
  static const char *const names[] = {"230 V mains, rectified (bus ~325 V)", "120 V mains, rectified (bus ~170 V)",
                                      "a DC supply"};
  int k;
  if(ask_choice(w, "supply.kind", "What feeds the HV side (the motor power)?", ids, names, 3, 0, &k) == SU_ABORT) {
    return SU_ABORTED;
  }
  double bus = k == 0 ? 230.0 * M_SQRT2 : k == 1 ? 120.0 * M_SQRT2 : 0.0;
  if(k == 2) {
    if(ask_num(w, "supply.dc", "DC supply voltage", "V", 300.0, 24.0, SU_DRIVE_MAX_BUS, 0, &bus) == SU_ABORT) {
      return SU_ABORTED;
    }
  }
  su_set_num(w->st, "supply.bus", round(bus));
  int chop = 0;
  if(ask_yes(w, "supply.chopper", "Does the supply have a brake chopper (brake resistor)?", &chop) == SU_ABORT) {
    return SU_ABORTED;
  }
  if(chop) {
    double on, lo = bus * 1.1 + 5.0;  // above the highest mains (+10 %)
    if(ask_num(w, "supply.chopper_on", "The chopper switches on at", "V", fmax(375.0, lo), lo, 375.0, 0, &on) ==
       SU_ABORT) {
      return SU_ABORTED;
    }
  } else {
    say(w, SU_MSG_WARN,
        "Without a brake chopper the energy of every stop goes into the drive's own capacitors\n"
        "(540 uF: ~20 J). Heavy or fast axes will trip over-voltage when they brake hard\n"
        "(docs/emergency_stop.md).");
  }
  double p;
  if(ask_num(w, "supply.max_hv_power", "Braking power the drive may feed back into the bus", "W", 1000.0, 100.0,
             2000.0, 0, &p) == SU_ABORT) {
    return SU_ABORTED;
  }
  return SU_OK;
}

// the holding brake's data (nameplate: "brake 24 V DC 0.5 A" or "... 12 W")
static int ask_brake(su_wizard_t *w) {
  static const char *const vids[]   = {"24", "other"};
  static const char *const vnames[] = {"24 V DC", "another voltage, or AC"};
  static const char *const oids[]   = {"0", "1", "2"};
  static const char *const onames[] = {"DIO0 (usual)", "DIO1", "DIO2"};
  int k, o;
  double cur, ms;
  if(ask_choice(w, "brake.voltage", "The brake's voltage", vids, vnames, 2, 0, &k) == SU_ABORT ||
     ask_num(w, "brake.current", "The brake's current (or its power / 24 V)", "A", NAN, 0.01, 10.0, 1, &cur) ==
         SU_ABORT ||
     ask_choice(w, "brake.output", "Which drive output switches it?", oids, onames, 3, 0, &o) == SU_ABORT ||
     ask_num(w, "brake.engage_ms", "The brake's engage time (datasheet: response / engage time)", "ms", NAN, 1.0,
             1000.0, 1, &ms) == SU_ABORT) {
    return SU_ABORT;
  }
  return 0;
}

static su_result_t step_motor(su_wizard_t *w) {
  say(w, SU_MSG_INFO,
      "From the motor's nameplate (or datasheet). Currents are rms, as nameplates give them.\n"
      "Unknown values are fine: answer \"?\"; the wizard measures what it can and stays careful.");
  double v;
  int r = ask_num(w, "motor.rated_cur", "Rated (continuous) current", "A rms", NAN, 0.1, 40.0, 1, &v);
  if(r == SU_ABORT) {
    return SU_ABORTED;
  }
  if(ask_num(w, "motor.peak_cur", "Peak (maximum) current", "A rms", NAN, 0.1, 60.0, 1, &v) == SU_ABORT ||
     ask_num(w, "motor.rated_torque", "Rated torque", "Nm", NAN, 0.01, 200.0, 1, &v) == SU_ABORT ||
     ask_num(w, "motor.max_rpm", "Maximum speed", "rpm", NAN, 10.0, 20000.0, 1, &v) == SU_ABORT) {
    return SU_ABORTED;
  }
  int brake;
  if(ask_yes(w, "motor.brake", "Does the motor have a holding brake?", &brake) == SU_ABORT) {
    return SU_ABORTED;
  }
  if(brake && ask_brake(w) == SU_ABORT) {
    return SU_ABORTED;
  }
  int vertical;
  if(ask_yes(w, "axis.vertical", "Does gravity pull on the axis (vertical, or a hanging load)?", &vertical) ==
     SU_ABORT) {
    return SU_ABORTED;
  }
  if(vertical && !brake) {
    say(w, SU_MSG_WARN,
        "A vertical axis without a holding brake falls whenever the drive is off (e-stop, power\n"
        "loss, a fault): add a brake or a counterweight.");
  }
  su_motor_t m;
  su_limits_t lim;
  motor_from_state(w->st, &m);
  su_motor_limits(&m, bus_of(w->st), &lim);
  if(m.rated_cur <= 0.0 && m.peak_cur <= 0.0) {
    say(w, SU_MSG_WARN, "No current known: the drive is limited to %.1f A until you set conf0.max_ac_cur.",
        lim.max_ac_cur);
  }
  if(lim.max_ac_cur >= SU_DRIVE_MAX_AC_CUR - 1e-6) {
    say(w, SU_MSG_WARN, "The motor takes more than the drive's %.0f A peak: the drive limits it there.",
        SU_DRIVE_MAX_AC_CUR);
  }
  say(w, SU_MSG_INFO, "Current limit %.2f A peak, identification current %.2f A.", lim.max_ac_cur,
      lim.test_cur);
  return SU_OK;
}

static su_result_t step_feedback(su_wizard_t *w) {
  const char *ids[16], *names[16];
  for(int i = 0; i < su_fb_ntypes; i++) {
    ids[i]   = su_fb_types[i].id;
    names[i] = su_fb_types[i].name;
  }
  int k;
  if(ask_choice(w, "feedback.type", "Which feedback does the motor have?", ids, names, su_fb_ntypes, 0, &k) ==
     SU_ABORT) {
    return SU_ABORTED;
  }
  const su_fb_type_t *fb = &su_fb_types[k];
  if(fb->res == SU_FB_RES_POLES) {
    double poles;
    if(ask_num(w, "feedback.poles_n", "Resolver poles (the nameplate's \"2 pole\", \"6X\" = 6...)", "", 2.0, 2.0,
               32.0, 0, &poles) == SU_ABORT) {
      return SU_ABORTED;
    }
    su_set_num(w->st, "feedback.poles", fmax(1.0, round(poles / 2.0)));
  }
  if(!fb->simulated) {
    say(w, SU_MSG_WARN, "%s: supported by the firmware, but this wizard's steps for it are untested.",
        fb->name);
  }
  return SU_OK;
}

static su_result_t step_wiring(su_wizard_t *w) {
  const su_fb_type_t *fb = su_fb_find(su_get(w->st, "feedback.type"));
  int brake              = su_get_num(w->st, "motor.brake", 0.0) > 0.0;
  char *t                = su_wiring_power(brake, bus_of(w->st));
  say(w, SU_MSG_WIRING, "%s", t);
  free(t);
  if(brake) {
    const char *v = su_get(w->st, "brake.voltage");
    t = su_wiring_brake((int)su_get_num(w->st, "brake.output", 0.0), v && strcmp(v, "24") == 0,
                        su_get_num(w->st, "brake.current", 0.0));
    say(w, SU_MSG_WIRING, "%s", t);
    free(t);
  }
  if(fb && (t = su_wiring_feedback(fb->wiring_col))) {
    say(w, SU_MSG_WIRING, "%s", t);
    free(t);
  }
  say(w, SU_MSG_INFO,
      "Use a shielded cable for the feedback; the shield to the connector shell at the drive.\n"
      "Leave the command connector (LinuxCNC) for later: the wizard doesn't need it.");
  if(w->ui->action(w->ui->user, "wiring.done", "Wire the drive as above, switch on the HV supply, then go on.",
                   w->link) == SU_ABORT) {
    return SU_ABORTED;
  }
  return SU_OK;
}

static su_result_t step_base(su_wizard_t *w) {
  if(apply_config(w, 0) < 0) {
    return SU_FAIL;
  }
  double v, f;
  if(get(w, "hv0.dc_volt", &v) < 0) {
    return SU_FAIL;
  }
  double bus = bus_of(w->st);
  if(v < 0.7 * bus || v > 1.2 * bus) {
    say(w, SU_MSG_ERROR, "The HV bus reads %.0f V, expected about %.0f V: check the HV supply and its wiring.", v,
        bus);
    return SU_FAIL;
  }
  if(drive_fault(w, &f) > 0) {
    say(w, SU_MSG_ERROR, "The drive reports fault %.0f with the first config.", f);
    return SU_FAIL;
  }
  say(w, SU_MSG_INFO, "First config written, the drive restarted; HV bus %.0f V.", v);
  return SU_OK;
}

static su_result_t fbcheck(su_wizard_t *w, const su_fb_type_t *fb);

// the hand turn: index counts (incremental), or the position's direction
static su_result_t step_fbcheck(su_wizard_t *w) {
  const su_fb_type_t *fb = su_fb_find(su_get(w->st, "feedback.type"));
  if(!fb) {
    return SU_FAIL;
  }
  disable(w);
  if(su_get_num(w->st, "motor.brake", 0.0) > 0.0) {  // released for the hand
    if(su_get_num(w->st, "axis.vertical", 0.0) > 0.0) {
      int yes = 0;
      say(w, SU_MSG_WARN, "The wizard releases the holding brake now: a vertical axis would fall.");
      if(ask_yes(w, "fbcheck.secured", "Is the load off the motor, or the axis supported?", &yes) == SU_ABORT ||
         !yes) {
        return SU_ABORTED;
      }
    }
    set(w, "fault0.brake_release", "1");
  }
  su_result_t r = fbcheck(w, fb);
  set(w, "fault0.brake_release", "0");
  return r;
}

static su_result_t fbcheck(su_wizard_t *w, const su_fb_type_t *fb) {
  if(fb->res == SU_FB_RES_MEASURED) {
    set_num(w, "enc_fb0.res", SU_FB_MEASURE_WRAP);
    set(w, "enc_fb0.indexprint", "1");
    su_link_clear(w->link);
    if(w->ui->action(w->ui->user, "fbcheck.turn",
                     "Turn the motor shaft by hand, slowly, at least 3 full turns in the direction\n"
                     "you want to call forward (positive). Then go on.",
                     w->link) == SU_ABORT) {
      return SU_ABORTED;
    }
    su_link_pump(w->link, 0.3);
    long cnt[64], counts = 0;
    int dir = 0;
    int n   = su_fb_parse_index(su_link_text(w->link), cnt, 64);
    int r   = su_fb_counts(cnt, n, SU_FB_MEASURE_WRAP, &counts, &dir);
    set(w, "enc_fb0.indexprint", "0");
    set(w, "enc_fb0.res", "conf0.mot_fb_res");
    if(r == -2) {
      say(w, SU_MSG_ERROR,
          "The index pulses came at different counts: the shaft turned back, or the index (Z) is noisy.\n"
          "Run this step again and turn in one direction only.");
      return SU_FAIL;
    }
    if(r == 0) {
      say(w, SU_MSG_INFO, "Measured %ld counts per revolution (%ld lines), counting %s when turned forward.", counts,
          su_fb_lines(counts), dir > 0 ? "up" : "down");
      if(!su_fb_lines(counts)) {
        say(w, SU_MSG_WARN, "%ld is not a multiple of 4: check the A/B signals.", counts);
      }
      su_set_num(w->st, "feedback.counts", counts);
      su_set_num(w->st, "feedback.rev", dir > 0 ? 0 : 1);
      return SU_OK;
    }
    say(w, SU_MSG_WARN, "No index pulses seen: the encoder has no index (Z), or it isn't wired.");
    double lines;
    if(ask_num(w, "feedback.lines", "Encoder lines per revolution (the label's PPR)", "", 2500, 16, 1048576, 0,
               &lines) == SU_ABORT) {
      return SU_ABORTED;
    }
    su_set_num(w->st, "feedback.counts", 4.0 * lines);
    set_num(w, "conf0.mot_fb_res", 4.0 * lines);
  }

  // direction from the position (no index): a quarter turn forward
  double p0, p1;
  set(w, "conf0.mot_fb_rev", "0");
  if(get(w, fb->pos_pin, &p0) < 0) {
    return SU_FAIL;
  }
  if(w->ui->action(w->ui->user, "fbcheck.quarter",
                   "Turn the shaft by hand about a quarter turn forward (the direction you want to\n"
                   "call positive), then go on.",
                   w->link) == SU_ABORT) {
    return SU_ABORTED;
  }
  if(get(w, fb->pos_pin, &p1) < 0) {
    return SU_FAIL;
  }
  double d = remainder(p1 - p0, 2.0 * M_PI);
  if(fabs(d) < 0.3) {
    say(w, SU_MSG_ERROR, "The feedback moved only %.2f rad: check its wiring and turn further.", d);
    return SU_FAIL;
  }
  su_set_num(w->st, "feedback.rev", d > 0.0 ? 0 : 1);
  say(w, SU_MSG_INFO, "Feedback direction: %s.", d > 0.0 ? "as wired" : "reversed (conf0.mot_fb_rev = 1)");
  return SU_OK;
}

static su_result_t step_gate(su_wizard_t *w) {
  say(w, SU_MSG_INFO,
      "Next the drive measures the motor. It turns on its own:\n"
      "  - electrical (id_pmsm): a few turns at low speed;\n"
      "  - mechanical (id_mot): back and forth, faster;\n"
      "  - tuning (id_pid): short fast moves.\n"
      "Best with the motor uncoupled from the machine. Coupled, the axis must be free to move\n"
      "the travel below, away from its ends, and the identified inertia is the whole axis's.");
  if(su_get_num(w->st, "axis.vertical", 0.0) > 0.0) {
    say(w, SU_MSG_WARN,
        "A vertical axis: measure the motor without its load (uncoupled, or the load taken off or\n"
        "supported). Gravity would read as friction and offset, and the motor turns several turns.\n"
        "The brake step puts the load back on and checks that the brake holds it.");
  }
  int yes = 0;
  if(ask_yes(w, "gate.free", "Is the motor free to turn (uncoupled, or the axis clear)?", &yes) == SU_ABORT) {
    return SU_ABORTED;
  }
  if(!yes) {
    say(w, SU_MSG_WARN, "Then make it free first, and run the wizard again.");
    return SU_ABORTED;
  }
  double turns;
  if(ask_num(w, "gate.turns", "How far may it turn each way from where it stands?", "turns", 2.0, 0.5, 1000.0, 0,
             &turns) == SU_ABORT) {
    return SU_ABORTED;
  }
  return SU_OK;
}

static su_result_t step_electrical(su_wizard_t *w) {
  su_motor_t m;
  su_limits_t lim;
  motor_from_state(w->st, &m);
  su_motor_limits(&m, bus_of(w->st), &lim);
  if(apply_config(w, 0) < 0 || load_template(w, "id_pmsm") < 0) {
    return SU_FAIL;
  }
  set_num(w, "idpmsm0.test_cur", lim.test_cur);
  set(w, "fault0.pos_error", "0");
  say(w, SU_MSG_INFO, "Measuring resistance, inductance, pole pairs, commutation and flux (%.2f A)...",
      lim.test_cur);
  if(enable(w) < 0) {
    return SU_FAIL;
  }
  // the results are read before disabling: idpmsm resets them at state 0
  int r = wait_state(w, "idpmsm0.state", 3.4, 180.0, "id_pmsm");
  double rr, l, pp, off, rev, psi, fbrev = su_get_num(w->st, "feedback.rev", 0.0);
  if(r == 0 && (get(w, "idpmsm0.r", &rr) || get(w, "idpmsm0.l", &l) || get(w, "idpmsm0.pp", &pp) ||
                get(w, "idpmsm0.com_offset", &off) || get(w, "idpmsm0.out_rev", &rev) ||
                get(w, "idpmsm0.psi", &psi))) {
    r = -1;
  }
  disable(w);
  if(r < 0) {
    return SU_FAIL;
  }
  // as idpmsm prints it: fb_switch reverses the offset with the feedback
  off = fbrev > 0.0 ? -off : off;
  su_set_num(w->st, "id.r", rr);
  su_set_num(w->st, "id.l", l);
  su_set_num(w->st, "id.pp", round(pp));
  su_set_num(w->st, "id.offset", off);
  su_set_num(w->st, "id.out_rev", rev > 0.0 ? 1 : 0);
  su_set_num(w->st, "id.psi", psi);
  motor_from_state(w->st, &m);
  char msg[1024];
  int warn = 0, err = su_motor_check(&m, bus_of(w->st), msg, sizeof(msg), &warn);
  say(w, SU_MSG_INFO,
      "r = %.4g ohm, l = %.4g H, %d pole pairs, phase order %s, commutation offset %.4f rad,\n"
      "psi = %.4g Wb: torque constant %.3g Nm/A rms.",
      rr, l, m.pp, rev > 0.0 ? "reversed" : "as wired", off, psi, su_kt_identified(&m));
  if(err || warn) {
    say(w, err ? SU_MSG_ERROR : SU_MSG_WARN, "%s", msg);
  }
  if(err) {
    su_unset_prefix(w->st, "id.");
    return SU_FAIL;
  }
  return SU_OK;
}

// one idm run at max_acc / max_vel; the fit's results. 0 ok
static int run_idm(su_wizard_t *w, double acc, double vel, double *j, double *d, double *f, double *o) {
  set_num(w, "idm0.max_acc", acc);
  set_num(w, "idm0.max_vel", vel);
  if(enable(w) < 0) {
    return -1;
  }
  int r = wait_state(w, "idm0.state", 1.5, 600.0, "id_mot");
  if(r == 0 &&
     (get(w, "idm0.ls_j", j) || get(w, "idm0.ls_d", d) || get(w, "idm0.ls_f", f) || get(w, "idm0.ls_o", o))) {
    r = -1;
  }
  disable(w);
  if(r == 0 && !(*j > 0.0)) {
    say(w, SU_MSG_ERROR, "The inertia fit failed (j = %g): the motion was disturbed.", *j);
    r = -1;
  }
  return r;
}

// Two runs. idm's defaults (250 rad/s^2, 25 rad/s) are safe for any rotor
// but give a high inertia on light motors with much friction: the inertia's
// torque is small against the friction's, and at low speed the friction's
// sign changes often; the fit puts the misfit into j (4x on a 1e-5 kg m^2
// motor). The second run accelerates with 30 % of the peak torque for that
// (high) estimate, so it can't saturate, and runs faster: within ~20 %.
static su_result_t step_mechanical(su_wizard_t *w) {
  if(apply_config(w, 0) < 0 || load_template(w, "id_mot") < 0) {
    return SU_FAIL;
  }
  su_motor_t m;
  su_limits_t lim;
  motor_from_state(w->st, &m);
  su_motor_limits(&m, bus_of(w->st), &lim);
  double travel = su_get_num(w->st, "gate.turns", 2.0) * 2.0 * M_PI;
  travel        = fmin(travel, 10.0);  // idm's default range
  set_num(w, "idm0.min_pos", -travel);
  set_num(w, "idm0.max_pos", travel);
  // idm's adaptive inertia sets the loop gains: start low, it rises (2e-4,
  // its default, made a 4e-7 motor's gains 500x too high: saturation)
  set_num(w, "idm0.inertia", fmin(2e-4, 5e-6 * fmax(lim.max_force, 0.01)));
  say(w, SU_MSG_INFO, "Measuring inertia, friction and damping (moves +-%.1f turns, twice)...", travel / 2.0 / M_PI);
  double j, d, f, o;
  if(run_idm(w, 250.0, 25.0, &j, &d, &f, &o) < 0) {
    return SU_FAIL;
  }
  double acc = fmin(fmax(0.3 * lim.max_force / j, 250.0), 10000.0);
  double vel = fmin(fmax(0.3 * lim.max_vel, 25.0), 200.0);
  vel        = fmin(vel, sqrt(acc * travel));  // reached within the travel
  double j1 = j;
  if(run_idm(w, acc, vel, &j, &d, &f, &o) < 0) {
    return SU_FAIL;
  }
  su_set_num(w->st, "mech.j", j);
  su_set_num(w->st, "mech.d", fmax(d, 0.0));
  su_set_num(w->st, "mech.f", fmax(f, 0.0));
  su_set_num(w->st, "mech.o", o);
  say(w, SU_MSG_INFO,
      "j = %.4g kg m^2 (first run %.4g), friction %.4g Nm, damping %.4g Nm s/rad, offset %.4g Nm\n"
      "(second run at %.0f rad/s^2, %.0f rad/s).",
      j, j1, f, d, o, acc, vel);
  if(j < 1e-5) {
    say(w, SU_MSG_WARN, "The console prints 6 decimals: an inertia of %.2g is known to about %.0f %%.", j,
        0.5e-6 / j * 100.0);
  }
  return SU_OK;
}

static su_result_t step_tuning(su_wizard_t *w) {
  if(apply_config(w, 0) < 0 || load_template(w, "id_pid") < 0) {
    return SU_FAIL;
  }
  say(w, SU_MSG_INFO, "Tuning the position and velocity loops (short fast moves)...");
  if(enable(w) < 0) {
    return SU_FAIL;
  }
  int r = wait_state(w, "ids0.state", 1.4, 600.0, "id_pid");
  double pb, vb, vd;
  if(r == 0 && (get(w, "ids0.pos_bw", &pb) || get(w, "ids0.vel_bw", &vb) || get(w, "ids0.vel_d", &vd))) {
    r = -1;
  }
  disable(w);
  if(r < 0) {
    return SU_FAIL;
  }
  su_set_num(w->st, "tune.pos_bw", pb);
  su_set_num(w->st, "tune.vel_bw", vb);
  su_set_num(w->st, "tune.vel_d", vd);
  say(w, SU_MSG_INFO, "pos_bw = %.4g, vel_bw = %.4g, vel_d = %.4g.", pb, vb, vd);
  return SU_OK;
}

// enabled, holding where the motor stands (no command link yet: the
// command is set by hand); *p0 the position. 0 ok
static int hold_here(su_wizard_t *w, double *p0) {
  double st = 0.0;
  set(w, "fault0.cmd_error", "0");
  if(get(w, "fb_switch0.pos_fb", p0) < 0) {
    return -1;
  }
  set_num(w, "pid0.pos_ext_cmd", *p0);
  set(w, "pid0.vel_ext_cmd", "0");
  set(w, "pid0.acc_ext_cmd", "0");
  if(enable(w) < 0) {
    return -1;
  }
  double end = now() + 5.0;
  do {
    drive_sleep(w, 0.2);
    if(get(w, "fault0.state", &st) < 0) {
      return -1;
    }
  } while((int)(st + 0.5) != ST_ENABLED && (int)(st + 0.5) != ST_SOFT_FAULT && now() < end);
  return (int)(st + 0.5) == ST_ENABLED ? 0 : -1;
}

// the feedback's own position: fb_switch0.pos_fb stands still while the
// drive is disabled (fault0.en_fb), so it can't show a sagging axis
static const char *raw_pos(su_wizard_t *w) {
  const su_fb_type_t *fb = su_fb_find(su_get(w->st, "feedback.type"));
  return fb ? fb->pos_pin : "enc_fb0.pos";
}

// the largest |position - p0| over t (drive time), sampled; p0 from raw_pos
static double drift(su_wizard_t *w, double p0, double t) {
  double m = 0.0, p;
  int n    = (int)(t / 0.05) + 1;
  for(int i = 0; i < n; i++) {
    if(su_link_get(w->link, raw_pos(w), &p) == 0) {
      m = fmax(m, fabs(remainder(p - p0, 2.0 * M_PI)));
    }
    drive_sleep(w, 0.05);
  }
  return m;
}

// The holding brake (fault0.mot_brake on io0.outN, fault0.brake_time):
//  1. released by the drive: the shaft turns by hand;
//  2. (vertical) the load goes back on;
//  3. it holds with the drive off: the user tries to turn the shaft;
//  4. at disable the axis doesn't sag: the outputs hold for brake_time
//     while the brake grips; if it sags, brake_time goes up and again.
static su_result_t step_brake(su_wizard_t *w) {
  if(su_get_num(w->st, "motor.brake", 0.0) <= 0.0) {
    say(w, SU_MSG_INFO, "No holding brake: nothing to do.");
    return SU_OK;
  }
  int vertical = su_get_num(w->st, "axis.vertical", 0.0) > 0.0;
  if(apply_config(w, 0) < 0) {
    return SU_FAIL;
  }
  double p0, p1;
  // 1. released: with the drive off and the brake released by the drive
  // (fault0.brake_release), the shaft turns by hand. (Moving the motor
  // proves nothing: its peak torque is often above the brake's.)
  disable(w);
  set(w, "fault0.brake_release", "1");
  drive_sleep(w, 0.3);
  if(get(w, raw_pos(w), &p0) < 0) {
    return SU_FAIL;
  }
  if(w->ui->action(w->ui->user, "brake.release_turn",
                   "The drive has released the brake (the drive is off): turn the shaft by hand about\n"
                   "a quarter turn; it should turn freely. Then go on.",
                   w->link) == SU_ABORT) {
    set(w, "fault0.brake_release", "0");
    return SU_ABORTED;
  }
  get(w, raw_pos(w), &p1);
  set(w, "fault0.brake_release", "0");
  double moved = fabs(remainder(p1 - p0, 2.0 * M_PI));
  if(moved < 0.2) {
    say(w, SU_MSG_ERROR,
        "With the brake released the shaft moved %.3f rad: the brake doesn't release.\n"
        "Check its wiring (DIO%d, the relay), its voltage, and that the output is the one\n"
        "configured.",
        moved, (int)su_get_num(w->st, "brake.output", 0.0));
    return SU_FAIL;
  }
  say(w, SU_MSG_INFO, "Released by the drive: the shaft turned %.2f rad by hand.", moved);

  // 2. the load back on
  if(vertical && w->ui->action(w->ui->user, "brake.loaded",
                               "Put the axis's load back on (couple the motor, remove the supports),\n"
                               "with the drive off the brake now holds it. Then go on.",
                               w->link) == SU_ABORT) {
    return SU_ABORTED;
  }

  // 3. holds with the drive off
  if(get(w, raw_pos(w), &p0) < 0) {
    return SU_FAIL;
  }
  if(w->ui->action(w->ui->user, "brake.hold_turn",
                   "The drive is off and the brake closed: try to turn the shaft by hand, both ways\n"
                   "(it must not turn). Then go on.",
                   w->link) == SU_ABORT) {
    return SU_ABORTED;
  }
  double d = drift(w, p0, vertical ? 2.0 : 0.2);
  su_set_num(w->st, "brake.hold_drift", d);
  if(d > 0.05) {
    say(w, SU_MSG_ERROR,
        "With the drive off the shaft moved %.3f rad: the brake doesn't hold. Is it wired the\n"
        "right way (released while the output is on)? Is the brake too weak for the load?",
        d);
    return SU_FAIL;
  }
  say(w, SU_MSG_INFO, "Holds with the drive off (moved %.4f rad).", d);

  // 4. no sag at disable
  double bt = su_get_num(w->st, "brake.time", 0.0) > 0.0 ? su_get_num(w->st, "brake.time", 0.15) : 0.0;
  if(bt <= 0.0) {
    double ms = su_get_num(w->st, "brake.engage_ms", -1.0);
    bt        = ms > 0.0 ? ms * 1.5e-3 + 0.02 : 0.15;
  }
  for(int i = 0; i < 6; i++) {
    set_num(w, "fault0.brake_time", bt);
    if(hold_here(w, &p0) < 0) {
      disable(w);
      say(w, SU_MSG_ERROR, "The drive did not enable.");
      return SU_FAIL;
    }
    drive_sleep(w, 0.5);
    get(w, raw_pos(w), &p0);
    disable(w);
    d = drift(w, p0, bt + 0.5);
    say(w, SU_MSG_INFO, "brake_time %.3f s: the axis moved %.4f rad at disable.", bt, d);
    if(d < 0.01) {
      break;
    }
    if(i == 5) {
      say(w, SU_MSG_ERROR, "The axis still moves at disable with brake_time %.2f s: check the brake.", bt);
      return SU_FAIL;
    }
    bt = bt * 1.5 + 0.03;
  }
  su_set_num(w->st, "brake.time", bt);
  su_set_num(w->st, "brake.drop", d);
  if(!vertical) {
    say(w, SU_MSG_INFO, "(No load pulls on the axis, so the disable test can't show the brake's delay:\n"
                        "brake_time stays at %.3f s from the datasheet.)",
        bt);
  }
  return SU_OK;
}

static su_result_t step_limits(su_wizard_t *w) {
  su_motor_t m;
  su_limits_t lim;
  motor_from_state(w->st, &m);
  su_motor_limits(&m, bus_of(w->st), &lim);
  say(w, SU_MSG_INFO,
      "Limits:\n"
      "  current   %.2f A peak (thermal model: continuous %.2f A)\n"
      "  torque    %.3g Nm\n"
      "  speed     %.0f rad/s = %.0f rpm (the bus allows %.0f rpm at no load)",
      lim.max_ac_cur, lim.max_ac_cur / lim.cur_boost, lim.max_force, lim.max_vel, lim.max_vel * 60.0 / 2.0 / M_PI,
      lim.volt_vel * 60.0 / 2.0 / M_PI);
  double rpm;
  if(ask_num(w, "limits.rpm", "Top speed for this axis", "rpm", round(lim.max_vel * 60.0 / 2.0 / M_PI), 1.0,
             lim.max_vel * 60.0 / 2.0 / M_PI + 0.5, 0, &rpm) == SU_ABORT) {
    return SU_ABORTED;
  }
  su_set_num(w->st, "limits.max_vel", rpm * 2.0 * M_PI / 60.0);
  su_set_num(w->st, "limits.max_ac_cur", lim.max_ac_cur);
  su_set_num(w->st, "limits.max_force", lim.max_force);
  return SU_OK;
}

static su_result_t step_link(su_wizard_t *w) {
  static const char *const ids[]   = {"sserial", "can", "none"};
  static const char *const names[] = {"LinuxCNC over Mesa smart-serial", "LinuxCNC over CAN (stmblcan)",
                                      "none for now"};
  int k;
  if(ask_choice(w, "link.kind", "How does the machine controller talk to the drive?", ids, names, 3, 0, &k) ==
     SU_ABORT) {
    return SU_ABORTED;
  }
  if(k == 0) {
    char *t = su_wiring_command("Smart Serial");
    say(w, SU_MSG_WIRING, "%s", t);
    free(t);
    double s;
    if(ask_num(w, "link.scale", "Machine units per motor turn (ballscrew pitch in mm; 360 for degrees)", "", 10.0,
               0.001, 100000.0, 0, &s) == SU_ABORT) {
      return SU_ABORTED;
    }
  } else if(k == 1) {
    say(w, SU_MSG_INFO, "CAN: see docs/can_support.md for the wiring and stmblcan's setup.");
  }
  return SU_OK;
}

// sample pin n times, dt apart (drive time); max |value|
static double max_abs(su_wizard_t *w, const char *pin, int n, double dt) {
  double m = 0.0, v;
  for(int i = 0; i < n; i++) {
    if(su_link_get(w->link, pin, &v) == 0) {
      m = fmax(m, fabs(v));
    }
    drive_sleep(w, dt);
  }
  return m;
}

static su_result_t step_verify(su_wizard_t *w) {
  if(apply_config(w, 1) < 0) {
    return SU_FAIL;
  }
  // no machine controller yet: hold the position where it is, then step
  double p0, st, f;
  set(w, "fault0.cmd_error", "0");
  if(get(w, "fb_switch0.pos_fb", &p0) < 0) {
    return SU_FAIL;
  }
  set_num(w, "pid0.pos_ext_cmd", p0);
  set(w, "pid0.vel_ext_cmd", "0");
  set(w, "pid0.acc_ext_cmd", "0");
  if(enable(w) < 0) {
    return SU_FAIL;
  }
  double end = now() + 5.0;
  do {
    drive_sleep(w, 0.2);
    if(get(w, "fault0.state", &st) < 0) {
      return SU_FAIL;
    }
  } while((int)(st + 0.5) != ST_ENABLED && (int)(st + 0.5) != ST_SOFT_FAULT && now() < end);
  double hold = max_abs(w, "pid0.pos_error", 5, 0.1);
  double err[2], moved[2] = {0.0, 0.0};
  const double step = 0.5;  // rad
  for(int i = 0; i < 2; i++) {
    double a = 0.0, b = 0.0;
    su_link_get(w->link, "fb_switch0.pos_fb", &a);
    set_num(w, "pid0.pos_ext_cmd", remainder(p0 + (i == 0 ? step : 0.0), 2.0 * M_PI));
    drive_sleep(w, 0.5);
    err[i] = max_abs(w, "pid0.pos_error", 5, 0.1);
    su_link_get(w->link, "fb_switch0.pos_fb", &b);
    moved[i] = remainder(b - a, 2.0 * M_PI);
  }
  int fl = drive_fault(w, &f);
  disable(w);
  su_link_reset(w->link, 20.0);  // the links the test overrode
  su_set_num(w->st, "verify.hold", hold);
  su_set_num(w->st, "verify.step", fmax(err[0], err[1]));
  if((int)(st + 0.5) != ST_ENABLED || fl) {
    say(w, SU_MSG_ERROR, "The drive did not enable with the final config (state %.0f, fault %.0f).", st, f);
    return SU_FAIL;
  }
  if(fabs(moved[0] - step) > 0.05 || fabs(moved[1] + step) > 0.05) {
    say(w, SU_MSG_ERROR, "The motor moved %.3f / %.3f rad for steps of +%.1f / -%.1f rad.", moved[0], moved[1], step,
        step);
    return SU_FAIL;
  }
  say(w, SU_MSG_INFO,
      "Holding: position error up to %.4f rad. Steps of +-%.1f rad: moved %.3f / %.3f rad, then\n"
      "position error %.4f / %.4f rad, no fault.",
      hold, step, moved[0], moved[1], err[0], err[1]);
  if(fmax(hold, fmax(err[0], err[1])) > 0.05) {
    say(w, SU_MSG_WARN, "The position error is larger than expected (> 0.05 rad): check the tuning.");
  }
  return SU_OK;
}

static su_result_t step_save(su_wizard_t *w) {
  char *conf = su_conf_read(w->link);
  if(!conf) {
    say(w, SU_MSG_ERROR, "could not read the config back");
    return SU_FAIL;
  }
  char name[64], path[512], rname[64], rpath[512];
  stamp(name, sizeof(name), "stmbl-%Y%m%d-%H%M%S.conf");
  stamp(rname, sizeof(rname), "stmbl-setup-report-%Y%m%d-%H%M%S.txt");
  FILE *f = fopen(out_path(w, name, path, sizeof(path)), "w");
  FILE *r = fopen(out_path(w, rname, rpath, sizeof(rpath)), "w");
  if(!f || !r) {
    say(w, SU_MSG_ERROR, "cannot write %s / %s", path, rpath);
    if(f) {
      fclose(f);
    }
    if(r) {
      fclose(r);
    }
    free(conf);
    return SU_FAIL;
  }
  fputs(conf, f);
  fclose(f);
  char *s = su_state_to_text(w->st);
  fprintf(r, "stmbl setup wizard report\n\n# the drive's config\n%s\n# everything the wizard asked and measured\n%s",
          conf, s);
  fclose(r);
  free(s);
  free(conf);
  su_set(w->st, "final.file", path);
  say(w, SU_MSG_INFO,
      "Done. The drive runs the new config (it is in its flash).\n"
      "  config: %s\n  report: %s\n"
      "Next: connect the machine controller; in LinuxCNC, home and jog slowly first.",
      path, rpath);
  return SU_OK;
}

const su_step_t su_steps[] = {
    {"version", "Firmware version", step_version},
    {"backup", "Back up the drive's config", step_backup},
    {"safety", "Before you start", step_safety},
    {"supply", "Supply", step_supply},
    {"motor", "Motor nameplate", step_motor},
    {"feedback", "Motor feedback", step_feedback},
    {"wiring", "Wiring", step_wiring},
    {"base", "First config", step_base},
    {"fbcheck", "Feedback check (turn by hand)", step_fbcheck},
    {"gate", "Before the motor moves", step_gate},
    {"electrical", "Measure the motor: electrical", step_electrical},
    {"mechanical", "Measure the motor: mechanical", step_mechanical},
    {"tuning", "Tune the control loops", step_tuning},
    {"brake", "Holding brake", step_brake},
    {"limits", "Limits", step_limits},
    {"link", "Machine controller link", step_link},
    {"verify", "Check the final config", step_verify},
    {"save", "Save", step_save},
};
const int su_nsteps = sizeof(su_steps) / sizeof(su_steps[0]);

// ---- engine ------------------------------------------------------------------

static void save_session(su_wizard_t *w) {
  if(w->session && su_state_save(w->st, w->session) < 0) {
    say(w, SU_MSG_WARN, "cannot save the session to %s", w->session);
  }
}

static su_result_t run_one(su_wizard_t *w, int i) {
  char key[64];
  say(w, SU_MSG_HEADING, "%d/%d  %s", i + 1, su_nsteps, su_steps[i].title);
  su_result_t r = su_steps[i].run(w);
  if(r == SU_OK) {
    snprintf(key, sizeof(key), "done.%s", su_steps[i].id);
    su_set(w->st, key, "1");
  }
  save_session(w);
  return r;
}

su_result_t su_wizard_run(su_wizard_t *w) {
  for(int i = 0; i < su_nsteps; i++) {
    char key[64];
    snprintf(key, sizeof(key), "done.%s", su_steps[i].id);
    // the version is checked every time: it may be another drive now
    if(i > 0 && su_get_num(w->st, key, 0.0) > 0.0) {
      continue;
    }
    su_result_t r = run_one(w, i);
    if(r != SU_OK) {
      return r;
    }
  }
  return SU_OK;
}

su_result_t su_wizard_step(su_wizard_t *w, const char *id) {
  for(int i = 0; i < su_nsteps; i++) {
    if(strcmp(su_steps[i].id, id) == 0) {
      return run_one(w, i);
    }
  }
  say(w, SU_MSG_ERROR, "no step \"%s\"", id);
  return SU_FAIL;
}

void su_wizard_redo(su_wizard_t *w, const char *id) {
  int from = su_nsteps;
  for(int i = 0; i < su_nsteps; i++) {
    if(strcmp(su_steps[i].id, id) == 0) {
      from = i;
    }
    if(i >= from) {
      char key[64];
      snprintf(key, sizeof(key), "done.%s", su_steps[i].id);
      su_unset(w->st, key);
    }
  }
}
