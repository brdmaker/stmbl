#include "su_version.h"

#include <stdio.h>
#include <string.h>

void su_fw_parse(const char *about, su_fw_t *fw) {
  memset(fw, 0, sizeof(*fw));
  fw->api = -1;
  for(const char *p = about; p && *p;) {
    const char *e = strchr(p, '\n');
    char line[160];
    size_t n = e ? (size_t)(e - p) : strlen(p);
    snprintf(line, sizeof(line), "%.*s", (int)(n < sizeof(line) - 1 ? n : sizeof(line) - 1), p);
    p = e ? e + 1 : NULL;

    int api;
    if(sscanf(line, "setup api %d", &api) == 1) {
      fw->api = api;
      continue;
    }
    // "<product> v<major>.<minor>.<patch> <git>": only the first one (F4)
    char prod[32], git[48] = "";
    int a, b, c;
    if(!fw->have_version && sscanf(line, "%31s v%d.%d.%d %47s", prod, &a, &b, &c, git) >= 4) {
      snprintf(fw->product, sizeof(fw->product), "%s", prod);
      snprintf(fw->git, sizeof(fw->git), "%s", git);
      fw->major        = a;
      fw->minor        = b;
      fw->patch        = c;
      fw->have_version = 1;
    }
  }
}

su_fw_check_t su_fw_check(const su_fw_t *fw, int api, const char *git) {
  if(fw->api < 0) {
    return SU_FW_NO_API;
  }
  if(fw->api != api) {
    return SU_FW_API_DIFFERS;
  }
  if(git && *git && fw->have_version && strcmp(fw->git, git) != 0) {
    return SU_FW_GIT_DIFFERS;
  }
  return SU_FW_OK;
}

const char *su_fw_explain(su_fw_check_t r, const su_fw_t *fw, int api, const char *git, char *buf, int len) {
  switch(r) {
    case SU_FW_OK:
      snprintf(buf, len, "Firmware %s v%d.%d.%d %s, setup api %d: matches this wizard.", fw->product, fw->major,
               fw->minor, fw->patch, fw->git, fw->api);
      break;
    case SU_FW_GIT_DIFFERS:
      snprintf(buf, len,
               "Firmware %s (setup api %d) is another build than this wizard (%s). The configuration interface "
               "is the same, so the wizard goes on.",
               fw->git, fw->api, git);
      break;
    case SU_FW_NO_API:
      snprintf(buf, len,
               "The drive's firmware is older than this wizard (\"about\" prints no setup api). Update it first: "
               "from the stmbl source this wizard came with, \"make btburn\" (USB DFU) or \"make flash\" "
               "(ST-Link), after a config backup (servoterm-cli --dump).");
      break;
    case SU_FW_API_DIFFERS:
      snprintf(buf, len,
               "The drive's firmware has setup api %d, this wizard needs %d. Flash the firmware from the same stmbl "
               "source as this wizard (\"make btburn\"), or use the wizard that came with the firmware.",
               fw->api, api);
      break;
  }
  return buf;
}
