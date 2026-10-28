#include "commands.h"
#include "commandslist.h"
#include "hal.h"
#include "fwver.h"
#include "confident.h"
#include <string.h>
#include <stdio.h>

// the flash config's text (src/config.c, host/drive.c); none on the F3
__attribute__((weak)) const char *conf_flash_text(void) {
  return 0;
}
static float pin_value(const char *comp, const char *pin, float none) {
  NAME c, p;
  strncpy(c, comp, sizeof(c) - 1);
  c[sizeof(c) - 1] = 0;
  strncpy(p, pin, sizeof(p) - 1);
  p[sizeof(p) - 1] = 0;
  hal_pin_inst_t *i = pin_inst_by_name(c, 0, p);
  return i ? i->source->value : none;
}

// Machine-readable versions (the console, webui/mill, asks for them):
// fw this firmware, hv the HV board's (-1 not known yet, 0 from before
// versioning, -2 no hv0), hv_image the HV firmware this one carries, conf the
// flash config's (0: no marker). Problems follow as words.
void version_cmd(char *ptr) {
  int hv = (int)pin_value("hv", "fw_version", -2), img = (int)pin_value("hv", "fw_image", -2);
  int conf = conf_version_of(conf_flash_text());
  printf("version fw=%d hv=%d hv_image=%d conf=%d%s%s%s\n", STMBL_FW_VERSION, hv, img, conf,
         hv >= 0 && img >= 0 && hv != img ? " hv_mismatch" : "",
         conf && conf < STMBL_FW_VERSION ? " conf_old" : "",
         conf > STMBL_FW_VERSION ? " conf_newer" : "");
}
COMMAND("version", version_cmd, "firmware versions: fw, hv, hv_image, conf (machine-readable)");

// The drive's name and motor type (shared/confident.h), as `ident` prints them
static void ident_get(char *name, char *motor) {
  const char *text = conf_flash_text();
  conf_ident_get(text ? text : "", CONF_NAME_KEY, name, CONF_IDENT_LEN + 1);
  conf_ident_get(text ? text : "", CONF_MOTOR_KEY, motor, CONF_IDENT_LEN + 1);
}

// At boot, after the config: a config for another firmware says so.
void conf_version_check(void) {
  int conf = conf_version_of(conf_flash_text());
  if(conf > STMBL_FW_VERSION) {
    printf("WARNING: the config is for firmware v%d, this is v%d: it may use pins this one lacks\n", conf, STMBL_FW_VERSION);
  } else if(conf && conf < STMBL_FW_VERSION) {
    printf("WARNING: the config is for firmware v%d, this is v%d: patch it (conf/patches, the console's Drives tab)\n", conf, STMBL_FW_VERSION);
  }
  char name[CONF_IDENT_LEN + 1], motor[CONF_IDENT_LEN + 1];
  ident_get(name, motor);
  if(name[0] || motor[0]) {
    printf("drive %s, motor %s\n", name[0] ? name : "(no name)", motor[0] ? motor : "(not set)");
  }
}

// The RAM config that flashsaveconf writes (src/config.c, host/drive.c); none on the F3
__attribute__((weak)) char *conf_ram(size_t *size) {
  *size = 0;
  return 0;
}

// `name` / `motor`: print it; with an argument set it in the flash config
// ("-" clears it). Starts from the flash config, so unsaved appendconf edits
// are dropped; flashsaveconf refuses while the drive is enabled.
static void ident_cmd(const char *what, const char *key, char *ptr) {
  char cur[CONF_IDENT_LEN + 1], val[CONF_IDENT_LEN + 2];
  while(*ptr == ' ') {
    ptr++;
  }
  size_t len = strlen(ptr);
  while(len && ptr[len - 1] == ' ') {
    len--;
  }
  if(len) {
    size_t size;
    char *ram = conf_ram(&size);
    if(!ram) {
      printf("error: no config on this board\n");
      return;
    }
    if(len > CONF_IDENT_LEN || memchr(ptr, '"', len)) {
      printf("error: %s: at most %d characters, no '\"'\n", what, CONF_IDENT_LEN);
      return;
    }
    memcpy(val, ptr, len);
    val[len] = 0;
    if(!strcmp(val, "-")) {
      val[0] = 0;
    }
    char cmd[16] = "flashloadconf";
    call_cmd(cmd);
    if(conf_ident_set(ram, size, key, val) < 0) {
      printf("error: config full\n");
      return;
    }
    strcpy(cmd, "flashsaveconf");
    call_cmd(cmd);
    conf_ident_get(conf_flash_text(), key, cur, sizeof(cur));
    if(strcmp(cur, val)) {
      printf("error: %s not saved\n", what);
      return;
    }
  }
  conf_ident_get(conf_flash_text() ? conf_flash_text() : "", key, cur, sizeof(cur));
  printf("%s: %s\n", what, cur[0] ? cur : "(not set)");
}

void name_cmd(char *ptr) {
  ident_cmd("name", CONF_NAME_KEY, ptr);
}
COMMAND("name", name_cmd, "the drive's name (x-axis): name [TEXT | -], saved in the flash config");

void motor_cmd(char *ptr) {
  ident_cmd("motor", CONF_MOTOR_KEY, ptr);
}
COMMAND("motor", motor_cmd, "the motor's type: motor [TEXT | -], saved in the flash config");

// Machine-readable, for servoterm-cli's prompt and the console
void ident_print(char *ptr) {
  char name[CONF_IDENT_LEN + 1], motor[CONF_IDENT_LEN + 1];
  ident_get(name, motor);
  printf("ident name=\"%s\" motor=\"%s\"\n", name, motor);
}
COMMAND("ident", ident_print, "the drive's name and motor type (machine-readable)");

uint32_t call_cmd(char *s) {
  char c[64];
  char a[64];
  uint32_t matches = sscanf(s, " %[a-zA-Z_0-9] %[ -~]", c, a);
  switch(matches) {
    case 0:
      return (0);
    case 1:
      a[0] = 0;
      /* FALLTHRU */
    case 2:
      for(uint32_t i = 0; i < sizeof(cmd) / sizeof(cmd_t); i++) {
        if(!strcmp(cmd[i].name, c)) {
          cmd[i].ptr(a);
          return (1);
        }
      }
      /* FALLTHRU */
    default:
      return (0);
  }
  return (0);
}

void listcmd(char *ptr) {
  for(uint32_t i = 0; i < sizeof(cmd) / sizeof(cmd_t); i++) {
    printf("%s: %s\n", cmd[i].name, cmd[i].doc);
  }
}

COMMAND("help", listcmd, "print this");

void uptime(char *ptr) {
  extern uint64_t systime;
  uint32_t min = systime/1000/60;
  uint32_t sec = (systime/1000)%60;
  uint32_t hr = min/60;
  min = min%60;
  printf("up %02lu:%02lu:%02lu\n",hr,min,sec);
}

COMMAND("uptime", uptime, "display uptime");
