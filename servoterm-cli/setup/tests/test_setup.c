/*
 * Unit tests for the setup wizard's pure modules (no drive): session state,
 * config text, firmware version check, motor limits and checks, feedback
 * resolution from index counts, wiring tables, the config builder.
 * TAP output; make -C servoterm-cli check.
 */
#include "su_conf.h"
#include "su_feedback.h"
#include "su_motor.h"
#include "su_state.h"
#include "su_steps.h"
#include "su_version.h"
#include "su_wiring.h"
#include "su_link.h"

#include <pthread.h>
#include <pty.h>
#include <unistd.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int n, failed;

static void ok(int cond, const char *what) {
  n++;
  printf("%s %d - %s\n", cond ? "ok" : "not ok", n, what);
  failed += !cond;
}

static void test_state(void) {
  su_state_t *s = su_state_new();
  su_state_parse(s, "# comment\n a.b = 1.5 \nc.d=text with = sign\n\nbad line\n");
  ok(su_get_num(s, "a.b", 0) == 1.5, "state: parse a number");
  ok(su_get(s, "c.d") && strcmp(su_get(s, "c.d"), "text with = sign") == 0, "state: value keeps '='");
  su_set_num(s, "a.b", 3);
  ok(strcmp(su_get(s, "a.b"), "3") == 0, "state: integers print without decimals");
  su_set(s, "x.unknown", "unknown");
  ok(su_get_num(s, "x.unknown", -1) == -1, "state: non-numbers give the default");
  su_set(s, "id.r", "1");
  su_set(s, "id.l", "2");
  su_unset_prefix(s, "id.");
  ok(!su_has(s, "id.r") && !su_has(s, "id.l") && su_has(s, "a.b"), "state: unset by prefix");
  char *t       = su_state_to_text(s);
  su_state_t *r = su_state_new();
  su_state_parse(r, t);
  ok(strcmp(su_get(r, "c.d"), "text with = sign") == 0 && su_get_num(r, "a.b", 0) == 3, "state: text round trip");
  free(t);
  su_state_free(r);
  su_state_free(s);
}

static void test_conf(void) {
  char *c = su_conf_normalize("  link pid\r\n\tconf0.r = 1\n\n\n");
  ok(strcmp(c, "link pid\nconf0.r = 1\n") == 0, "conf: normalize");
  c = su_conf_set(c, "conf0.r", "2", "id");
  ok(strcmp(c, "link pid\nconf0.r = 2 # id\n") == 0, "conf: set replaces the line");
  c = su_conf_set(c, "conf0.l", "0.01", NULL);
  c = su_conf_link(c, "pmsm");
  ok(strcmp(c, "link pid\nlink pmsm\nconf0.r = 2 # id\nconf0.l = 0.01\n") == 0,
     "conf: link goes after the links, set appends");
  c = su_conf_link(c, "pmsm");
  ok(su_conf_has_link(c, "pmsm") && strstr(c, "link pmsm\nlink pmsm") == NULL, "conf: link only once");
  ok(!su_conf_has_link(c, "pm"), "conf: has_link matches whole names");
  free(c);
  c = strdup("conf0.r = 1\nconf0.rt = 5\nconf0.r=3\nconf0.l = 2\n");
  char v[32];
  ok(su_conf_get(c, "conf0.r", v, sizeof(v)) == 0 && strcmp(v, "3") == 0, "conf: get the last line, not a prefix");
  c = su_conf_set(c, "conf0.r", "4", NULL);
  ok(strcmp(c, "conf0.rt = 5\nconf0.r = 4\nconf0.l = 2\n") == 0, "conf: set drops earlier duplicates");
  c = su_conf_unset(c, "conf0.rt");
  ok(strcmp(c, "conf0.r = 4\nconf0.l = 2\n") == 0, "conf: unset");
  free(c);
  char err[128];
  ok(su_conf_validate("conf0.r = 1\n", err, sizeof(err)) == 0, "conf: a valid config");
  ok(su_conf_validate("conf0.a_very_long_pin_name_that_goes_on = 1234567890123\n", err, sizeof(err)) == 1,
     "conf: a line too long for appendconf");
  c = su_conf_set(strdup(""), "conf0.mot_fb_offset", "-1.87993", "a comment far too long to fit the line");
  ok(strcmp(c, "conf0.mot_fb_offset = -1.87993\n") == 0, "conf: a comment that doesn't fit is dropped");
  free(c);
}

static void test_version(void) {
  su_fw_t fw;
  su_fw_parse("######## software info ########\nSTMBL v1.2.3 abc1234-dirty\nBranch x\nsetup api 1\n"
              "######## Bootloader info ########\nboot v9.9.9 zzz\n",
              &fw);
  ok(fw.have_version && fw.major == 1 && fw.minor == 2 && fw.patch == 3 && strcmp(fw.git, "abc1234-dirty") == 0,
     "version: the F4's line, not the bootloader's");
  ok(fw.api == 1, "version: setup api");
  ok(su_fw_check(&fw, 1, "abc1234-dirty") == SU_FW_OK, "version: same build");
  ok(su_fw_check(&fw, 1, "def5678") == SU_FW_GIT_DIFFERS, "version: other build, same api: a warning");
  ok(su_fw_check(&fw, 2, "") == SU_FW_API_DIFFERS, "version: other api: refused");
  su_fw_parse("STMBL v1.0.0 old\n", &fw);
  ok(su_fw_check(&fw, 1, "") == SU_FW_NO_API, "version: no api line: firmware too old");
  su_fw_parse("STMBL simulated drive (host/drive.c)\nstmbl_host v0.0.0 sim\nsetup api 1\n", &fw);
  ok(su_fw_check(&fw, 1, "") == SU_FW_OK && strcmp(fw.product, "stmbl_host") == 0, "version: the simulator");
}

static void test_motor(void) {
  su_motor_t m = {0};
  su_limits_t l;
  m.rated_cur = 2.6, m.peak_cur = 7.8;
  su_motor_limits(&m, 325.0, &l);
  ok(fabs(l.max_ac_cur - 7.8 * M_SQRT2) < 1e-9, "motor: max_ac_cur is the peak current's amplitude");
  ok(fabs(l.cur_boost - 3.0) < 1e-9, "motor: thermal boost = peak / rated");
  ok(l.test_cur > 1.8 && l.test_cur < 1.9, "motor: identification at half the rated current");
  ok(l.max_force == 0.0 && l.max_vel == 0.0, "motor: no torque or speed limit before psi is known");
  m.peak_cur = 30.0;
  su_motor_limits(&m, 325.0, &l);
  ok(l.max_ac_cur == SU_DRIVE_MAX_AC_CUR, "motor: the drive's current limit");
  memset(&m, 0, sizeof(m));
  su_motor_limits(&m, 325.0, &l);
  ok(l.max_ac_cur == 2.0 && l.test_cur <= 0.7, "motor: nothing known: careful");

  memset(&m, 0, sizeof(m));
  m.rated_cur = 2.6, m.rated_torque = 1.27, m.max_rpm = 5000;
  m.r = 2.15, m.l = 0.023, m.pp = 5, m.psi = 0.0474, m.j = 4.4e-5, m.f = 0.038, m.d = 1e-4;
  su_motor_limits(&m, 325.0, &l);
  ok(fabs(l.max_force - 1.5 * 5 * 0.0474 * l.max_ac_cur) < 1e-9, "motor: max_force from psi");
  ok(fabs(l.max_vel - 5000 * 2 * M_PI / 60) < 1e-6, "motor: max_vel from the nameplate below the bus limit");
  char msg[1024];
  int warn = -1, err = su_motor_check(&m, 325.0, msg, sizeof(msg), &warn);
  ok(err == 0 && warn == 0, "motor: the burn-in motor checks clean");
  ok(fabs(su_kt_identified(&m) / su_kt_nameplate(&m) - 1.0) < 0.05, "motor: torque constants agree");
  m.pp = 3;  // a wrong encoder resolution shows as a wrong pole count
  err = su_motor_check(&m, 325.0, msg, sizeof(msg), &warn);
  ok(err == 0 && warn == 1 && strstr(msg, "torque constant"), "motor: Kt off by 40 %: a warning");
  m.pp = 0, m.r = 500;
  err = su_motor_check(&m, 325.0, msg, sizeof(msg), &warn);
  ok(err == 2, "motor: no pole pairs, open winding: errors");
  m.pp = 10, m.r = 2.15;  // twice the pole pairs: Kt twice the nameplate's
  err = su_motor_check(&m, 325.0, msg, sizeof(msg), &warn);
  ok(err == 1 && strstr(msg, "error: torque constant"), "motor: Kt off by 2x: an error");
  m.pp = 5, m.max_rpm = 12000;
  err = su_motor_check(&m, 325.0, msg, sizeof(msg), &warn);
  ok(err == 0 && warn == 1 && strstr(msg, "back-EMF"), "motor: top speed above the bus: a warning");
}

static void test_feedback(void) {
  long cnt[8];
  int k = su_fb_parse_index("OK enc_fb0.indexprint = 1\ncnt = 64732\ncnt = 56732\nfoo\ncnt = 48732\n", cnt, 8);
  ok(k == 3 && cnt[2] == 48732, "feedback: index counts from the drive's text");
  long counts;
  int dir;
  ok(su_fb_counts(cnt, k, 65536, &counts, &dir) == 0 && counts == 8000 && dir == -1,
     "feedback: 8000 counts, counting down");
  long up[3] = {65000, 9464, 19464};  // across the wrap
  ok(su_fb_counts(up, 3, 65536, &counts, &dir) == 0 && counts == 10000 && dir == 1,
     "feedback: across the counter's wrap");
  long back[3] = {100, 10100, 100};
  ok(su_fb_counts(back, 3, 65536, &counts, &dir) == -2, "feedback: turned back: rejected");
  ok(su_fb_counts(cnt, 1, 65536, &counts, &dir) == -1, "feedback: one pulse is not enough");
  ok(su_fb_lines(10000) == 2500 && su_fb_lines(10002) == 0, "feedback: lines");
  ok(su_fb_find("enc") && su_fb_find("resolver") && !su_fb_find("nope"), "feedback: types");
  for(int i = 0; i < su_fb_ntypes; i++) {
    char *t = su_wiring_feedback(su_fb_types[i].wiring_col);
    char what[96];
    snprintf(what, sizeof(what), "wiring: a table for %s", su_fb_types[i].id);
    ok(t != NULL, what);
    free(t);
  }
}

static void test_wiring(void) {
  char *t = su_wiring_feedback("Encoder");
  ok(t && strstr(t, "5    Blue Stripe     Z+") && strstr(t, "7    Brown Stripe    VCC"),
     "wiring: encoder pins as in the docs");
  free(t);
  t = su_wiring_command("Smart Serial");
  ok(t && strstr(t, "1    Orange Stripe   RX+") && strstr(t, "6    Green           TX-"), "wiring: smart serial");
  free(t);
  ok(su_wiring_feedback("nope") == NULL, "wiring: unknown column");
  t = su_wiring_power(1, 325);
  ok(t && strstr(t, "brake") && strstr(t, "325 V"), "wiring: bus, brake pointer");
  free(t);
  t = su_wiring_brake(1, 1, 0.5);
  ok(t && strstr(t, "DIO1") && strstr(t, "flyback") && !strstr(t, "relay"), "wiring: a 24 V 0.5 A brake directly");
  free(t);
  t = su_wiring_brake(0, 1, 1.5);
  ok(t && strstr(t, "relay") && strstr(t, "more than the output's 1 A"), "wiring: 1.5 A: through a relay");
  free(t);
  t = su_wiring_brake(0, 0, 0.5);
  ok(t && strstr(t, "isn't 24 V DC"), "wiring: another voltage: through a relay");
  free(t);
}

static void test_temp(void) {
  char m[512];
  ok(su_temp_type("kty84") == 1 && su_temp_type("switch") == 5 && su_temp_type("nope") == 0 && su_temp_type(NULL) == 0,
     "temp: ids are the firmware's type numbers");
  ok(su_temp_check(0, -1, 0, 0, 0, m, sizeof(m)) == 0, "temp: no sensor, nothing to check");
  ok(su_temp_check(1, 581, 20.0, 0, 0, m, sizeof(m)) == 0, "temp: a KTY84 at 20 degC");
  ok(su_temp_check(1, -1, 0, 1, 0, m, sizeof(m)) == 1 && strstr(m, "open"), "temp: open wire");
  ok(su_temp_check(2, 581, 0, 2, 0, m, sizeof(m)) == 1 && strstr(m, "too low for a pt1000"),
     "temp: a KTY read as a PT1000: short / another type");
  ok(su_temp_check(1, 1080, 120.0, 0, 0, m, sizeof(m)) == 1 && strstr(m, "another sensor type"),
     "temp: a PT1000 read as a KTY84: 120 degC cold");
  ok(su_temp_check(3, 150, 0, 0, 0, m, sizeof(m)) == 0, "temp: a cold PTC");
  ok(su_temp_check(5, 1e9, 0, 0, 1, m, sizeof(m)) == 1 && strstr(m, "tripped"), "temp: an open switch cold");
  char *t = su_wiring_temp(1);
  ok(t && strstr(t, "KTY84-130") && strstr(t, "HV side"), "temp: wiring, on the HV side");
  free(t);
  su_state_t *s = su_state_new();
  su_state_parse(s, "motor.temp_sensor = ntc\ntemp.ntc_r25 = 4700\ntemp.ntc_b = 3500\ntemp.warn = 100\ntemp.trip = 120\n");
  char *c = su_build_config(s, 0);
  char v[32];
  ok(su_conf_get(c, "conf0.mot_temp_type", v, sizeof(v)) == 0 && strcmp(v, "4") == 0, "temp: build: the type");
  ok(su_conf_get(c, "conf0.mot_temp_ntc_r25", v, sizeof(v)) == 0 && strcmp(v, "4700") == 0 &&
         su_conf_get(c, "conf0.mot_temp_ntc_b", v, sizeof(v)) == 0,
     "temp: build: the NTC's data");
  ok(su_conf_get(c, "conf0.high_mot_temp", v, sizeof(v)) == 0 && strcmp(v, "100") == 0 &&
         su_conf_get(c, "conf0.max_mot_temp", v, sizeof(v)) == 0 && strcmp(v, "120") == 0,
     "temp: build: warning and trip temperatures");
  char err[128];
  ok(su_conf_validate(c, err, sizeof(err)) == 0, "temp: build: the lines fit the drive");
  free(c);
  su_state_parse(s, "motor.temp_sensor = none\n");
  c = su_build_config(s, 0);
  ok(su_conf_get(c, "conf0.mot_temp_type", v, sizeof(v)) < 0 && su_conf_get(c, "conf0.max_mot_temp", v, sizeof(v)) < 0,
     "temp: build: no sensor, the defaults stay");
  free(c);
  su_state_free(s);
}

static void test_build(void) {
  su_state_t *s = su_state_new();
  su_state_parse(s,
                 "supply.bus = 325\nsupply.chopper = 1\nsupply.chopper_on = 375\nsupply.max_hv_power = 800\n"
                 "motor.rated_cur = 2.6\nmotor.peak_cur = 7.8\nmotor.max_rpm = unknown\nmotor.brake = 1\n"
                 "feedback.type = enc\nfeedback.counts = 10000\nfeedback.rev = 1\n"
                 "link.kind = sserial\nlink.scale = 5\n");
  char *c = su_build_config(s, 0);
  ok(su_conf_has_link(c, "pid") && su_conf_has_link(c, "pmsm") && su_conf_has_link(c, "enc_fb0") &&
         !su_conf_has_link(c, "sserial"),
     "build: the identification config has no command link");
  char v[32];
  ok(su_conf_get(c, "conf0.mot_fb_res", v, sizeof(v)) == 0 && strcmp(v, "10000") == 0, "build: resolution");
  ok(su_conf_get(c, "io0.out0", v, sizeof(v)) == 0 && strcmp(v, "fault0.mot_brake") == 0, "build: holding brake");
  ok(su_conf_get(c, "fault0.brake_time", v, sizeof(v)) == 0 && strcmp(v, "0.15") == 0,
     "build: brake_time without data: 0.15 s");
  ok(su_conf_get(c, "conf0.high_dc_volt", v, sizeof(v)) == 0 && strcmp(v, "380") == 0 &&
         su_conf_get(c, "conf0.max_dc_volt", v, sizeof(v)) == 0 && strcmp(v, "395") == 0,
     "build: bus limits above the chopper");
  ok(su_conf_get(c, "conf0.max_force", v, sizeof(v)) < 0, "build: no torque limit before psi is known");
  ok(su_conf_get(c, "conf0.polecount", v, sizeof(v)) < 0, "build: nothing unidentified is written");
  char err[128];
  ok(su_conf_validate(c, err, sizeof(err)) == 0, "build: every line fits the drive");
  free(c);
  su_state_parse(s, "id.r = 2.15\nid.pp = 5\nid.psi = 0.0474\nlimits.max_vel = 300\n"
                    "brake.output = 2\nbrake.engage_ms = 60\n");
  c = su_build_config(s, 1);
  ok(su_conf_has_link(c, "sserial"), "build: the final config links the command link");
  ok(su_conf_get(c, "io0.out2", v, sizeof(v)) == 0 && su_conf_get(c, "io0.out0", v, sizeof(v)) < 0,
     "build: the brake on the chosen output");
  ok(su_conf_get(c, "fault0.brake_time", v, sizeof(v)) == 0 && strcmp(v, "0.11") == 0,
     "build: brake_time from the engage time: 60 ms * 1.5 + 20 ms");
  su_state_parse(s, "brake.time = 0.2\n");
  free(c);
  c = su_build_config(s, 1);
  ok(su_conf_get(c, "fault0.brake_time", v, sizeof(v)) == 0 && strcmp(v, "0.2") == 0,
     "build: the measured brake_time wins");
  ok(su_conf_get(c, "linrev0.scale", v, sizeof(v)) == 0 && strcmp(v, "5") == 0, "build: units per turn");
  ok(su_conf_get(c, "conf0.max_vel", v, sizeof(v)) == 0 && strcmp(v, "300") == 0, "build: the chosen top speed");
  ok(su_conf_get(c, "conf0.max_force", v, sizeof(v)) == 0, "build: torque limit once psi is known");
  ok(su_conf_validate(c, err, sizeof(err)) == 0, "build: the final config fits the drive");
  free(c);

  // the wizard on the smart-serial console: sserial stays loaded, once
  su_state_parse(s, "console.sserial = 1\n");
  c = su_build_config(s, 0);
  const char *ld = strstr(c, "load sserial\n");
  ok(ld && !strstr(ld + 1, "load sserial") && !su_conf_has_link(c, "sserial"),
     "build: console over smart-serial: sserial loaded once, no command link");
  ok(su_conf_get(c, "sserial0.rt_prio", v, sizeof(v)) == 0 && su_conf_get(c, "fault0.en", v, sizeof(v)) < 0,
     "build: console only: its priorities, the enable not wired");
  ok(ld && strstr(c, "link enc_fb0") < ld && ld < strstr(c, "sserial0.rt_prio"),
     "build: loaded after the templates, before its settings");
  free(c);
  c = su_build_config(s, 1);
  ok(su_conf_has_link(c, "sserial") && !strstr(c, "load sserial"), "build: the final link sserial loads it itself");
  free(c);
  su_state_free(s);
}


// A fake drive on a pty for su_conf_write: answers deleteconf, appendconf,
// confcrc, flashsaveconf and showconf like src/config.c, and mixes the
// drive's own messages into the answers, as a faulted drive does.
static struct {
  int fd;
  char ram[4096], flash[4096];
  int noise;       // a message before every answer
  int drop_delete; // deleteconf lost (as on the SEW bench)
  int stop;
} fd_drive;

static void say(const char *s) {
  if(write(fd_drive.fd, s, strlen(s)) < 0) {
  }
}

static void *fake_drive(void *arg) {
  (void)arg;
  char line[256];
  size_t n = 0;
  while(!fd_drive.stop) {
    char c;
    if(read(fd_drive.fd, &c, 1) != 1) {
      break;
    }
    if(c != '\n') {
      if(n < sizeof(line) - 1) {
        line[n++] = c;
      }
      continue;
    }
    line[n] = 0;
    n       = 0;
    if(!line[0]) {
      continue;
    }
    if(fd_drive.noise) {
      say("ERROR: Fault 9: HV timeout error\nWARNING: over temperature (motor) current clamping active\n");
    }
    char out[512];
    if(!strcmp(line, "deleteconf")) {
      if(!fd_drive.drop_delete) {
        fd_drive.ram[0] = 0;
      }
    } else if(!strncmp(line, "appendconf ", 11)) {
      snprintf(out, sizeof(out), "adding %s\n", line + 11);
      say(out);
      strncat(fd_drive.ram, line + 11, sizeof(fd_drive.ram) - strlen(fd_drive.ram) - 2);
      strcat(fd_drive.ram, "\n");
    } else if(!strcmp(line, "confcrc")) {
      snprintf(out, sizeof(out), "\nsize: %zu words: %zu crc:1234\n", strlen(fd_drive.ram), strlen(fd_drive.ram) / 4);
      say(out);
    } else if(!strcmp(line, "flashsaveconf")) {
      strcpy(fd_drive.flash, fd_drive.ram);
      snprintf(out, sizeof(out), "erasing flash page...\nsaving conf\nOK %zu bytes written\n", strlen(fd_drive.ram) + 1);
      say(out);
    } else if(!strncmp(line, "getf ", 5)) {
      // smart-serial (~1 kB/s): the last answer's tail and the start of this
      // one come first, the rest after more than a read's 50 ms
      snprintf(out, sizeof(out), "%s%s = -", fd_drive.noise ? "0.000000\n" : "", line + 5);
      say(out);
      usleep(120000);
      say("1.5\n");
    } else if(!strcmp(line, "showconf")) {
      if(fd_drive.noise) {
        say("\n");  // the smart-serial console's answer starts so
      }
      say(fd_drive.flash);
      if(fd_drive.noise) {
        say("INFO: Disabled \n");
      }
    }
  }
  return NULL;
}

static void test_conf_write(void) {
  int master, slave;
  char name[64], err[256];
  if(openpty(&master, &slave, name, NULL, NULL) < 0) {
    ok(0, "conf write: openpty");
    return;
  }
  struct termios tio;
  tcgetattr(slave, &tio);
  cfmakeraw(&tio);
  tcsetattr(slave, TCSANOW, &tio);
  fd_drive.fd = master;
  strcpy(fd_drive.flash, "link pid\nold.pin = 1\n");
  strcpy(fd_drive.ram, fd_drive.flash);  // the boot loads flash into RAM
  pthread_t th;
  pthread_create(&th, NULL, fake_drive, NULL);
  su_link_t *l = su_link_open(name, err, sizeof(err));
  ok(l != NULL, "conf write: fake drive opened");
  if(!l) {
    return;
  }
  const char *conf = "link pid\nlink pmsm\nconf0.r = 22.5 # id_pmsm\n";

  fd_drive.noise = 1;
  int r = su_conf_write(l, conf, err, sizeof(err));
  ok(r == 0 && !strcmp(fd_drive.flash, conf), "conf write: fault messages in the answers don't break it");
  if(r) {
    printf("#   %s\n", err);
  }

  double gv = 0.0;
  ok(su_link_get(l, "fb_switch0.pos_fb", &gv) == 0 && gv == -1.5,
     "get: the last answer's tail first (slow console) doesn't hide the value");

  strcpy(fd_drive.ram, fd_drive.flash);
  fd_drive.noise       = 0;
  fd_drive.drop_delete = 1;
  r = su_conf_write(l, "link pid\nconf0.r = 1\n", err, sizeof(err));
  ok(r < 0 && !strcmp(fd_drive.flash, conf), "conf write: a lost deleteconf stops it, the flash isn't doubled");
  ok(strstr(err, "deleteconf") != NULL, "conf write: and says so");
  fd_drive.drop_delete = 0;

  fd_drive.stop = 1;
  su_link_close(l);
  close(slave);
  close(master);
  pthread_join(th, NULL);
}

int main(void) {
  test_state();
  test_conf();
  test_version();
  test_motor();
  test_feedback();
  test_wiring();
  test_temp();
  test_build();
  test_conf_write();
  printf("1..%d\n# %d/%d passed\n", n, n - failed, n);
  return failed != 0;
}
