#include "su_state.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct su_state {
  char **key;
  char **val;
  size_t n, cap;
};

su_state_t *su_state_new(void) {
  return calloc(1, sizeof(su_state_t));
}

void su_state_free(su_state_t *s) {
  if(!s) {
    return;
  }
  for(size_t i = 0; i < s->n; i++) {
    free(s->key[i]);
    free(s->val[i]);
  }
  free(s->key);
  free(s->val);
  free(s);
}

static long find(const su_state_t *s, const char *key) {
  for(size_t i = 0; i < s->n; i++) {
    if(strcmp(s->key[i], key) == 0) {
      return (long)i;
    }
  }
  return -1;
}

const char *su_get(const su_state_t *s, const char *key) {
  long i = find(s, key);
  return i < 0 ? NULL : s->val[i];
}

double su_get_num(const su_state_t *s, const char *key, double def) {
  const char *v = su_get(s, key);
  if(!v || !*v) {
    return def;
  }
  char *end;
  double d = strtod(v, &end);
  while(*end == ' ') {
    end++;
  }
  return (end == v || *end) ? def : d;
}

int su_has(const su_state_t *s, const char *key) {
  return find(s, key) >= 0;
}

void su_set(su_state_t *s, const char *key, const char *value) {
  long i = find(s, key);
  if(i >= 0) {
    free(s->val[i]);
    s->val[i] = strdup(value);
    return;
  }
  if(s->n == s->cap) {
    s->cap = s->cap ? 2 * s->cap : 32;
    s->key = realloc(s->key, s->cap * sizeof(char *));
    s->val = realloc(s->val, s->cap * sizeof(char *));
  }
  s->key[s->n] = strdup(key);
  s->val[s->n] = strdup(value);
  s->n++;
}

void su_set_num(su_state_t *s, const char *key, double value) {
  char buf[64];
  if(value == floor(value) && fabs(value) < 1e9) {
    snprintf(buf, sizeof(buf), "%.0f", value);
  } else {
    snprintf(buf, sizeof(buf), "%.6g", value);
  }
  su_set(s, key, buf);
}

static void drop(su_state_t *s, size_t i) {
  free(s->key[i]);
  free(s->val[i]);
  memmove(s->key + i, s->key + i + 1, (s->n - i - 1) * sizeof(char *));
  memmove(s->val + i, s->val + i + 1, (s->n - i - 1) * sizeof(char *));
  s->n--;
}

void su_unset(su_state_t *s, const char *key) {
  long i = find(s, key);
  if(i >= 0) {
    drop(s, (size_t)i);
  }
}

void su_unset_prefix(su_state_t *s, const char *prefix) {
  size_t n = strlen(prefix);
  for(size_t i = s->n; i-- > 0;) {
    if(strncmp(s->key[i], prefix, n) == 0) {
      drop(s, i);
    }
  }
}

int su_entry(const su_state_t *s, size_t i, const char **key, const char **value) {
  if(i >= s->n) {
    return 0;
  }
  *key   = s->key[i];
  *value = s->val[i];
  return 1;
}

static char *trim(char *p) {
  while(*p == ' ' || *p == '\t') {
    p++;
  }
  char *e = p + strlen(p);
  while(e > p && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) {
    *--e = '\0';
  }
  return p;
}

void su_state_parse(su_state_t *s, const char *text) {
  char *copy = strdup(text);
  char *save = NULL;
  for(char *line = strtok_r(copy, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
    char *l = trim(line);
    if(!*l || *l == '#') {
      continue;
    }
    char *eq = strchr(l, '=');
    if(!eq) {
      continue;
    }
    *eq = '\0';
    su_set(s, trim(l), trim(eq + 1));
  }
  free(copy);
}

char *su_state_to_text(const su_state_t *s) {
  size_t len = 1;
  for(size_t i = 0; i < s->n; i++) {
    len += strlen(s->key[i]) + strlen(s->val[i]) + 4;
  }
  char *out = malloc(len);
  size_t o  = 0;
  for(size_t i = 0; i < s->n; i++) {
    o += sprintf(out + o, "%s = %s\n", s->key[i], s->val[i]);
  }
  out[o] = '\0';
  return out;
}

int su_state_load(su_state_t *s, const char *path) {
  FILE *f = fopen(path, "r");
  if(!f) {
    return -1;
  }
  char line[1024];
  while(fgets(line, sizeof(line), f)) {
    su_state_parse(s, line);
  }
  fclose(f);
  return 0;
}

int su_state_save(const su_state_t *s, const char *path) {
  char tmp[1024];
  snprintf(tmp, sizeof(tmp), "%s.tmp", path);
  FILE *f = fopen(tmp, "w");
  if(!f) {
    return -1;
  }
  fprintf(f, "# stmbl setup wizard session (servoterm-cli --wizard); resume with --session\n");
  char *text = su_state_to_text(s);
  fputs(text, f);
  free(text);
  if(fclose(f) != 0) {
    return -1;
  }
  return rename(tmp, path);  // a crash never leaves half a session
}
