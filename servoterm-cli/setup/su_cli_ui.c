#include "su_cli_ui.h"
#include "su_link.h"
#include "su_state.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

struct su_cli {
  su_ui_t ui;
  su_state_t *answers;  // NULL: interactive only
  int tty;
  int verbose;
  char last_progress[128];
};

static void trim(char *s) {
  char *p = s;
  while(*p == ' ' || *p == '\t') {
    p++;
  }
  memmove(s, p, strlen(p) + 1);
  size_t n = strlen(s);
  while(n && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ')) {
    s[--n] = '\0';
  }
}

// a line from the terminal; 0 ok, -1 end of input
static int read_line(su_cli_t *c, char *buf, size_t len) {
  (void)c;
  fflush(stderr);
  if(!fgets(buf, (int)len, stdin)) {
    return -1;
  }
  trim(buf);
  return 0;
}

// the answers file's value for key, NULL if none
static const char *scripted(su_cli_t *c, const char *key) {
  return c->answers ? su_get(c->answers, key) : NULL;
}

static int quit(const char *a) {
  return strcasecmp(a, "q") == 0 || strcasecmp(a, "quit") == 0;
}

static void cli_say(void *user, su_msg_t kind, const char *text) {
  su_cli_t *c = user;
  switch(kind) {
    case SU_MSG_HEADING:
      fprintf(stderr, "\n=== %s ===\n", text);
      break;
    case SU_MSG_WARN:
      fprintf(stderr, "WARNING: %s\n", text);
      break;
    case SU_MSG_ERROR:
      fprintf(stderr, "ERROR: %s\n", text);
      break;
    case SU_MSG_DRIVE:
      if(c->verbose) {
        fprintf(stderr, "%s", text);
      }
      break;
    default:
      fprintf(stderr, "%s\n", text);
  }
  c->last_progress[0] = '\0';
}

static int cli_choose(void *user, const char *key, const char *q, const char *const *ids, const char *const *names,
                      int n, int def, int *out) {
  su_cli_t *c   = user;
  const char *a = scripted(c, key);
  fprintf(stderr, "%s\n", q);
  for(int i = 0; i < n; i++) {
    fprintf(stderr, "  %d) %s%s\n", i + 1, names[i], i == def ? "  [default]" : "");
  }
  for(;;) {
    char buf[128];
    if(a) {
      snprintf(buf, sizeof(buf), "%s", a);
      fprintf(stderr, "> %s   (%s)\n", buf, key);
    } else if(c->tty) {
      fprintf(stderr, "> ");
      if(read_line(c, buf, sizeof(buf)) < 0) {
        return SU_ABORT;
      }
    } else if(def >= 0) {
      *out = def;
      fprintf(stderr, "> %s   (default)\n", ids[def]);
      return 0;
    } else {
      fprintf(stderr, "ERROR: no answer for %s\n", key);
      return SU_ABORT;
    }
    if(quit(buf)) {
      return SU_ABORT;
    }
    if(!buf[0] && def >= 0) {
      *out = def;
      return 0;
    }
    int k = atoi(buf);
    if(k >= 1 && k <= n && strspn(buf, "0123456789") == strlen(buf)) {
      *out = k - 1;
      return 0;
    }
    for(int i = 0; i < n; i++) {
      if(strcasecmp(buf, ids[i]) == 0) {
        *out = i;
        return 0;
      }
    }
    fprintf(stderr, "  1..%d, or one of:", n);
    for(int i = 0; i < n; i++) {
      fprintf(stderr, " %s", ids[i]);
    }
    fprintf(stderr, "\n");
    if(a) {
      return SU_ABORT;  // a bad scripted answer doesn't get better by asking again
    }
  }
}

static int cli_number(void *user, const char *key, const char *q, const char *unit, double def, double min,
                      double max, int unknown_ok, double *out) {
  su_cli_t *c   = user;
  const char *a = scripted(c, key);
  for(;;) {
    char buf[128];
    fprintf(stderr, "%s%s%s%s", q, unit && *unit ? " [" : "", unit && *unit ? unit : "", unit && *unit ? "]" : "");
    if(!isnan(def)) {
      fprintf(stderr, " (default %g%s)", def, unknown_ok ? ", ? = unknown" : "");
    } else if(unknown_ok) {
      fprintf(stderr, " (? = unknown)");
    }
    if(a) {
      snprintf(buf, sizeof(buf), "%s", a);
      fprintf(stderr, ": %s   (%s)\n", buf, key);
    } else if(c->tty) {
      fprintf(stderr, ": ");
      if(read_line(c, buf, sizeof(buf)) < 0) {
        return SU_ABORT;
      }
    } else if(!isnan(def)) {
      fprintf(stderr, ": %g   (default)\n", def);
      *out = def;
      return 0;
    } else if(unknown_ok) {
      fprintf(stderr, ": ?   (default)\n");
      return SU_UNKNOWN;
    } else {
      fprintf(stderr, "\nERROR: no answer for %s\n", key);
      return SU_ABORT;
    }
    if(quit(buf)) {
      return SU_ABORT;
    }
    if(unknown_ok && (strcmp(buf, "?") == 0 || strcasecmp(buf, "unknown") == 0)) {
      return SU_UNKNOWN;
    }
    if(!buf[0] && !isnan(def)) {
      *out = def;
      return 0;
    }
    char *end;
    double v = strtod(buf, &end);
    if(end != buf && !*end && v >= min && v <= max) {
      *out = v;
      return 0;
    }
    fprintf(stderr, "  a number from %g to %g%s\n", min, max, unknown_ok ? ", or ?" : "");
    if(a) {
      return SU_ABORT;
    }
  }
}

static int cli_confirm(void *user, const char *key, const char *q, int *out) {
  su_cli_t *c   = user;
  const char *a = scripted(c, key);
  for(;;) {
    char buf[32];
    fprintf(stderr, "%s [y/n]", q);
    if(a) {
      snprintf(buf, sizeof(buf), "%s", a);
      fprintf(stderr, ": %s   (%s)\n", buf, key);
    } else if(c->tty) {
      fprintf(stderr, ": ");
      if(read_line(c, buf, sizeof(buf)) < 0) {
        return SU_ABORT;
      }
    } else {
      fprintf(stderr, "\nERROR: no answer for %s\n", key);
      return SU_ABORT;
    }
    if(quit(buf)) {
      return SU_ABORT;
    }
    if(strchr("yY1", buf[0]) && buf[0]) {
      *out = 1;
      return 0;
    }
    if(strchr("nN0", buf[0]) && buf[0]) {
      *out = 0;
      return 0;
    }
    if(a) {
      return SU_ABORT;
    }
  }
}

// "send <line>; wait <s>; ..." against the drive (the simulator's hand)
static int play(su_cli_t *c, const char *script, su_link_t *drive) {
  char *copy = strdup(script), *save = NULL;
  int rc     = 0;
  for(char *part = strtok_r(copy, ";", &save); part && !rc; part = strtok_r(NULL, ";", &save)) {
    trim(part);
    if(strncmp(part, "send ", 5) == 0) {
      rc = drive && su_link_send(drive, part + 5) == 0 ? 0 : SU_ABORT;
      if(drive) {
        su_link_pump(drive, 0.05);
      }
    } else if(strncmp(part, "wait ", 5) == 0) {
      if(drive) {
        su_link_pump(drive, atof(part + 5));
      } else {
        usleep((useconds_t)(atof(part + 5) * 1e6));
      }
    } else if(strcmp(part, "done") != 0 && part[0]) {
      fprintf(stderr, "ERROR: answers file: unknown action \"%s\"\n", part);
      rc = SU_ABORT;
    }
  }
  free(copy);
  (void)c;
  return rc;
}

static int cli_action(void *user, const char *key, const char *instruction, su_link_t *drive) {
  su_cli_t *c   = user;
  const char *a = scripted(c, key);
  fprintf(stderr, "%s\n", instruction);
  if(a) {
    fprintf(stderr, "> %s   (%s)\n", a, key);
    return play(c, a, drive);
  }
  if(!c->tty) {
    fprintf(stderr, "ERROR: no answer for %s\n", key);
    return SU_ABORT;
  }
  fprintf(stderr, "Press Enter when done (q to stop): ");
  char buf[32];
  if(read_line(c, buf, sizeof(buf)) < 0 || quit(buf)) {
    return SU_ABORT;
  }
  return 0;
}

static void cli_progress(void *user, const char *what, double fraction) {
  su_cli_t *c = user;
  if(strcmp(c->last_progress, what) != 0) {
    fprintf(stderr, "  ... %s", what);
    snprintf(c->last_progress, sizeof(c->last_progress), "%s", what);
  } else {
    fputc('.', stderr);
  }
  if(fraction >= 1.0) {
    fputc('\n', stderr);
  }
  fflush(stderr);
}

su_cli_t *su_cli_new(const char *answers_path, int verbose) {
  su_cli_t *c = calloc(1, sizeof(su_cli_t));
  c->tty      = isatty(STDIN_FILENO);
  c->verbose  = verbose;
  if(answers_path) {
    c->answers = su_state_new();
    if(su_state_load(c->answers, answers_path) < 0) {
      su_cli_free(c);
      return NULL;
    }
  }
  c->ui = (su_ui_t){c, cli_say, cli_choose, cli_number, cli_confirm, cli_action, cli_progress};
  return c;
}

void su_cli_free(su_cli_t *c) {
  if(c) {
    su_state_free(c->answers);
    free(c);
  }
}

su_ui_t *su_cli_ui(su_cli_t *c) {
  return &c->ui;
}
