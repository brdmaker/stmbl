#include "su_conf.h"
#include "su_link.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *drop_messages(const char *in);

char *su_conf_normalize(const char *in) {
  size_t n  = strlen(in);
  char *out = malloc(n + 2);
  size_t o  = 0;
  const char *p = in;
  while(*p) {
    const char *e = strchr(p, '\n');
    size_t len    = e ? (size_t)(e - p) : strlen(p);
    size_t s      = 0;
    while(s < len && (p[s] == ' ' || p[s] == '\t' || p[s] == '\r')) {
      s++;
    }
    size_t t = len;
    while(t > s && (p[t - 1] == ' ' || p[t - 1] == '\t' || p[t - 1] == '\r')) {
      t--;
    }
    for(size_t i = s; i < t; i++) {
      out[o++] = p[i] == '\t' ? ' ' : p[i];
    }
    out[o++] = '\n';
    if(!e) {
      break;
    }
    p = e + 1;
  }
  while(o && out[o - 1] == '\n') {
    o--;
  }
  out[o++] = '\n';
  out[o]   = '\0';
  return out;
}

int su_conf_validate(const char *conf, char *err, size_t errlen) {
  int errors = 0, lineno = 0;
  if(err && errlen) {
    err[0] = '\0';
  }
  for(const char *p = conf; *p;) {
    const char *e = strchr(p, '\n');
    size_t len    = e ? (size_t)(e - p) : strlen(p);
    lineno++;
    int bad = len > SU_CONF_LINE_MAX;
    for(size_t i = 0; i < len && !bad; i++) {
      bad = (unsigned char)p[i] < ' ' || (unsigned char)p[i] > '~';
    }
    if(bad) {
      if(err && !errors) {
        snprintf(err, errlen, "line %d too long (max %d) or not printable: %.*s", lineno, SU_CONF_LINE_MAX,
                 (int)len, p);
      }
      errors++;
    }
    if(!e) {
      break;
    }
    p = e + 1;
  }
  if(strlen(conf) > SU_CONF_SIZE_MAX) {
    if(err && !errors) {
      snprintf(err, errlen, "config is %zu bytes, max %d", strlen(conf), SU_CONF_SIZE_MAX);
    }
    errors++;
  }
  return errors;
}

// does the line set pin? ("pin = ...", "pin=...")
static int sets(const char *line, size_t len, const char *pin) {
  size_t pl = strlen(pin);
  if(len <= pl || strncmp(line, pin, pl) != 0) {
    return 0;
  }
  size_t i = pl;
  while(i < len && line[i] == ' ') {
    i++;
  }
  return i < len && line[i] == '=';
}

static const char *last_line_setting(const char *conf, const char *pin) {
  const char *last = NULL;
  for(const char *p = conf; *p;) {
    const char *e = strchr(p, '\n');
    size_t len    = e ? (size_t)(e - p) : strlen(p);
    if(sets(p, len, pin)) {
      last = p;
    }
    if(!e) {
      break;
    }
    p = e + 1;
  }
  return last;
}

// copy conf, replacing the line at 'at' (or appending when at is NULL) by
// 'line' (NULL: none), and dropping other lines setting pin
static char *rebuild(char *conf, const char *pin, const char *at, const char *line) {
  size_t ll = line ? strlen(line) : 0;
  char *out = malloc(strlen(conf) + ll + 3);
  size_t o  = 0;
  for(const char *p = conf; *p;) {
    const char *e = strchr(p, '\n');
    size_t len    = e ? (size_t)(e - p + 1) : strlen(p);
    if(p == at) {
      if(line) {
        memcpy(out + o, line, ll);
        o += ll;
      }
    } else if(!pin || !sets(p, len - (e ? 1 : 0), pin)) {
      memcpy(out + o, p, len);
      o += len;
    }
    p += len;
  }
  if(!at && line) {
    if(o && out[o - 1] != '\n') {
      out[o++] = '\n';
    }
    memcpy(out + o, line, ll);
    o += ll;
  }
  out[o] = '\0';
  free(conf);
  return out;
}

char *su_conf_set(char *conf, const char *pin, const char *value, const char *comment) {
  char line[256];
  if(comment && *comment) {
    snprintf(line, sizeof(line), "%s = %s # %s\n", pin, value, comment);
    if(strlen(line) - 1 > SU_CONF_LINE_MAX) {  // the comment goes first
      snprintf(line, sizeof(line), "%s = %s\n", pin, value);
    }
  } else {
    snprintf(line, sizeof(line), "%s = %s\n", pin, value);
  }
  return rebuild(conf, pin, last_line_setting(conf, pin), line);
}

char *su_conf_set_num(char *conf, const char *pin, double v, const char *comment) {
  char val[32];
  snprintf(val, sizeof(val), "%.6g", v);
  return su_conf_set(conf, pin, val, comment);
}

char *su_conf_unset(char *conf, const char *pin) {
  return rebuild(conf, pin, NULL, NULL);
}

int su_conf_get(const char *conf, const char *pin, char *out, size_t outlen) {
  const char *l = last_line_setting(conf, pin);
  if(!l) {
    return -1;
  }
  const char *v = strchr(l, '=') + 1;
  while(*v == ' ') {
    v++;
  }
  size_t n = 0;
  while(v[n] && v[n] != '\n' && v[n] != '#') {
    n++;
  }
  while(n && v[n - 1] == ' ') {
    n--;
  }
  snprintf(out, outlen, "%.*s", (int)n, v);
  return 0;
}

int su_conf_has_link(const char *conf, const char *name) {
  size_t nl = strlen(name);
  for(const char *p = conf; *p;) {
    const char *e = strchr(p, '\n');
    size_t len    = e ? (size_t)(e - p) : strlen(p);
    if(len >= 5 + nl && strncmp(p, "link ", 5) == 0 && strncmp(p + 5, name, nl) == 0 &&
       (len == 5 + nl || p[5 + nl] == ' ' || p[5 + nl] == '#')) {
      return 1;
    }
    if(!e) {
      break;
    }
    p = e + 1;
  }
  return 0;
}

char *su_conf_link(char *conf, const char *name) {
  if(su_conf_has_link(conf, name)) {
    return conf;
  }
  // after the last link line: templates first, settings after them
  const char *after = NULL;
  for(const char *p = conf; *p;) {
    const char *e = strchr(p, '\n');
    if(strncmp(p, "link ", 5) == 0) {
      after = e ? e + 1 : p + strlen(p);
    }
    if(!e) {
      break;
    }
    p = e + 1;
  }
  size_t at = after ? (size_t)(after - conf) : 0;
  char line[128];
  snprintf(line, sizeof(line), "link %s\n", name);
  size_t n  = strlen(conf), ll = strlen(line);
  char *out = malloc(n + ll + 1);
  memcpy(out, conf, at);
  memcpy(out + at, line, ll);
  memcpy(out + at + ll, conf + at, n - at + 1);
  free(conf);
  return out;
}

char *su_conf_read(su_link_t *l) {
  const char *t = su_link_query(l, "showconf", 0.5, NULL, 10.0);
  if(!t) {
    return NULL;
  }
  char *text = drop_messages(t);
  char *conf = su_conf_normalize(text);
  free(text);
  return conf;
}

// The drive's own messages (faults, warnings) come whenever they happen, in
// the middle of an answer too: no config line looks like them.
static int is_message(const char *line, size_t len) {
  static const char *const starts[] = {"ERROR:", "INFO:", "WARNING:", "<font"};
  for(size_t i = 0; i < sizeof(starts) / sizeof(starts[0]); i++) {
    size_t n = strlen(starts[i]);
    if(len >= n && strncmp(line, starts[i], n) == 0) {
      return 1;
    }
  }
  return 0;
}

static char *drop_messages(const char *in) {
  char *out = malloc(strlen(in) + 1);
  size_t o  = 0;
  for(const char *p = in; *p;) {
    const char *e = strchr(p, '\n');
    size_t len    = e ? (size_t)(e - p) + 1 : strlen(p);
    if(!is_message(p, len)) {
      memcpy(out + o, p, len);
      o += len;
    }
    p += len;
  }
  out[o] = '\0';
  return out;
}

// a line of the answer starts with "error" (the firmware's own failures,
// "error!", "error: drive enabled"); "Fault 9: HV timeout error" isn't one
static int has_error_line(const char *t) {
  for(const char *p = t; p && *p;) {
    if(strncmp(p, "error", 5) == 0) {
      return 1;
    }
    p = strchr(p, '\n');
    p = p ? p + 1 : NULL;
  }
  return 0;
}

// The drive's RAM config size (`confcrc` ends "size: N words: ..."), -1 no answer.
static long ram_conf_size(su_link_t *l) {
  const char *t = su_link_query(l, "confcrc", 0.1, "crc:", 3.0);
  const char *s = t ? strstr(t, "size: ") : NULL;
  long n;
  return s && sscanf(s + 6, "%ld", &n) == 1 ? n : -1;
}

int su_conf_write(su_link_t *l, const char *text, char *err, size_t errlen) {
  char *conf = su_conf_normalize(text);
  if(su_conf_validate(conf, err, errlen)) {
    free(conf);
    return -1;
  }
  // deleteconf prints nothing, so check the RAM config is empty: a lost
  // deleteconf appended to the old text, and the drive booted the doubled
  // config with two hv comps (SEW bench, 2026-10-29)
  if(!su_link_query(l, "deleteconf", 0.1, NULL, 1.0) || ram_conf_size(l) != 0) {
    snprintf(err, errlen, "deleteconf did not clear the drive's config: flash not written");
    free(conf);
    return -1;
  }
  // a line at a time, each acknowledged ("adding <line>"), so the drive's
  // 64-byte line buffer never overflows. Other output (a warning, a fault)
  // may come first: look for the acknowledgement, not the first line.
  int lineno  = 0;
  long expect = 0;
  for(const char *p = conf; *p;) {
    const char *e = strchr(p, '\n');
    size_t len    = e ? (size_t)(e - p) : strlen(p);
    char cmd[SU_LINE_MAX + 1], want[SU_LINE_MAX + 16];
    snprintf(cmd, sizeof(cmd), "appendconf %.*s", (int)len, p);
    snprintf(want, sizeof(want), "adding %.*s\n", (int)len, p);
    lineno++;
    const char *t = su_link_query(l, cmd, 0.1, want, 2.0);
    if(!t || !strstr(t, want)) {
      snprintf(err, errlen, "no ack for line %d (%s): flash not written", lineno, cmd);
      free(conf);
      return -1;
    }
    expect += (long)len + 1;  // appendconf adds the newline
    if(!e) {
      break;
    }
    p = e + 1;
  }
  long got = ram_conf_size(l);
  if(got != expect) {
    snprintf(err, errlen, "the drive's config is %ld bytes, %ld were sent: flash not written", got, expect);
    free(conf);
    return -1;
  }
  const char *t = su_link_query(l, "flashsaveconf", 0.1, "bytes written", 10.0);
  if(!t || !strstr(t, "bytes written") || has_error_line(t)) {
    snprintf(err, errlen, "flashsaveconf failed: %s", t ? t : "no answer");
    free(conf);
    return -1;
  }
  char *back = su_conf_read(l);
  int ok     = back && strcmp(back, conf) == 0;
  free(back);
  free(conf);
  if(!ok) {
    snprintf(err, errlen, "verify failed: flash differs from what was written");
    return -1;
  }
  return 0;
}
