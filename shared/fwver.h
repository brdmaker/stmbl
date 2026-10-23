#pragma once
// Firmware versions (docs/firmware_versions.md): the HV board's running one,
// found in its state rotation; the flash config's, from its marker line.
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "version_number.h"

// The F3 sends its state a word a packet, in a rotation (conf_addr 0, 1, ...).
// An F3 from before versioning has no fw_version word: its rotation starts
// again before reaching it. running: -1 not known yet, 0 an F3 from before,
// else its STMBL_FW_VERSION.
typedef struct {
  int32_t running;
  uint16_t last;
  uint8_t seen;
} hv_fwver_t;

static inline void hv_fwver_init(hv_fwver_t *v) {
  v->running = -1;
  v->last    = 0;
  v->seen    = 0;
}

static inline void hv_fwver_word(hv_fwver_t *v, uint16_t addr, uint32_t value, uint16_t version_word) {
  if(addr == version_word) {
    v->running = (int32_t)value;
    v->seen    = 1;
  } else if(addr < v->last && !v->seen) {
    v->running = 0;  // a whole rotation without it
  }
  v->last = addr;
}

// A config's version: its "#stmbl_conf_version N" line, 0 if it has none (written before versioning).
#define STMBL_CONF_MARKER "#stmbl_conf_version"
static inline int conf_version_of(const char *text) {
  if(!text) {
    return 0;
  }
  const char *p = strstr(text, STMBL_CONF_MARKER);
  while(p && p != text && p[-1] != '\n') {  // only at a line's start
    p = strstr(p + 1, STMBL_CONF_MARKER);
  }
  return p ? atoi(p + sizeof(STMBL_CONF_MARKER) - 1) : 0;
}
