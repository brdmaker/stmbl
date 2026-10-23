#include "commands.h"
#include "commandslist.h"
#include "hal.h"
#include "fwver.h"
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

// At boot, after the config: a config for another firmware says so.
void conf_version_check(void) {
  int conf = conf_version_of(conf_flash_text());
  if(conf > STMBL_FW_VERSION) {
    printf("WARNING: the config is for firmware v%d, this is v%d: it may use pins this one lacks\n", conf, STMBL_FW_VERSION);
  } else if(conf && conf < STMBL_FW_VERSION) {
    printf("WARNING: the config is for firmware v%d, this is v%d: patch it (conf/patches, the console's Drives tab)\n", conf, STMBL_FW_VERSION);
  }
}

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
