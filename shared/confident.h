#pragma once
// A drive's own name and its motor's type, kept in the flash config as two
// comment lines: "#stmbl_name x-axis", "#stmbl_motor SEW CMP40M/RH1M".
// hal_parse skips them, so they travel with the config (showconf,
// servoterm-cli --dump / --load) and need no storage of their own.
#include <stddef.h>
#include <string.h>

// The flash config's text: erased flash (a new drive's, never written: all
// 0xFF, no terminating 0) is an empty config. Printed or copied as it was,
// it went on past the config's sector, and its 0xFF bytes read as scope
// frame markers in servoterm-cli (the bench, 2026-10-29).
static inline const char *conf_text(const char *flash) {
  return (!flash || (unsigned char)flash[0] == 0xFF) ? "" : flash;
}

#define CONF_NAME_KEY "#stmbl_name"
#define CONF_MOTOR_KEY "#stmbl_motor"
// "appendconf " + key + ' ' + value must fit the console's 63-char line
#define CONF_IDENT_LEN 32

// the line "KEY value" at p (a line's start), or 0
static inline const char *conf_ident_line(const char *p, const char *key) {
  size_t k = strlen(key);
  return !strncmp(p, key, k) && (p[k] == ' ' || p[k] == '\n' || p[k] == '\r' || p[k] == 0) ? p + k : 0;
}

// KEY's value from config text into out ("" if none), blanks trimmed
static inline void conf_ident_get(const char *text, const char *key, char *out, size_t n) {
  out[0] = 0;
  for(const char *p = text; p && *p; p = strchr(p, '\n'), p = p ? p + 1 : 0) {
    const char *v = conf_ident_line(p, key);
    if(!v) {
      continue;
    }
    while(*v == ' ') {
      v++;
    }
    size_t len = strcspn(v, "\r\n");
    while(len && v[len - 1] == ' ') {
      len--;
    }
    len = len < n - 1 ? len : n - 1;
    memcpy(out, v, len);
    out[len] = 0;
    return;
  }
}

// Replace KEY's lines in text (size bytes) by "KEY value" at the top, or
// remove them for an empty value. -1 if it doesn't fit (text unchanged).
static inline int conf_ident_set(char *text, size_t size, const char *key, const char *value) {
  size_t len = strlen(text), add = *value ? strlen(key) + 1 + strlen(value) + 1 : 0;
  size_t drop = 0;
  for(const char *p = text; *p;) {
    const char *e = strchr(p, '\n');
    size_t l      = e ? (size_t)(e - p) + 1 : strlen(p);
    drop += conf_ident_line(p, key) ? l : 0;
    p += l;
  }
  if(len - drop + add + 1 > size) {
    return -1;
  }
  char *w = text;  // drop KEY's lines in place
  for(char *p = text; *p;) {
    char *e  = strchr(p, '\n');
    size_t l = e ? (size_t)(e - p) + 1 : strlen(p);
    if(!conf_ident_line(p, key)) {
      memmove(w, p, l);
      w += l;
    }
    p += l;
  }
  *w = 0;
  if(add) {
    memmove(text + add, text, strlen(text) + 1);
    memcpy(text, key, strlen(key));
    text[strlen(key)] = ' ';
    memcpy(text + strlen(key) + 1, value, add - strlen(key) - 2);
    text[add - 1] = '\n';
  }
  return 0;
}
