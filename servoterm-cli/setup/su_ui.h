/*
 * Setup wizard: the user interface, as callbacks.
 *
 * The wizard's steps (su_steps.c) never print or read the terminal
 * themselves; they ask through this table. servoterm-cli fills it with a
 * terminal front-end or an answers file (su_cli_ui.c); a GUI fills it with
 * dialogs. Every question carries a key: the same key names the answer in
 * the session state (su_state.h) and in an answers file, so a GUI can use it
 * as a form field and a script can answer by name.
 *
 * Return values: 0 answered, SU_ABORT the user cancelled the wizard (the
 * session is kept, the wizard can resume), SU_UNKNOWN (number only) the user
 * doesn't know the value: the step measures it or uses a safe default.
 */
#pragma once

#define SU_ABORT (-1)
#define SU_UNKNOWN 1

typedef enum {
  SU_MSG_HEADING,  // a step's title
  SU_MSG_INFO,     // explanation, results
  SU_MSG_WARN,     // something to check, the wizard goes on
  SU_MSG_ERROR,    // the step failed
  SU_MSG_WIRING,   // a wiring table (fixed-width text)
  SU_MSG_DRIVE,    // raw text from the drive (verbose)
} su_msg_t;

struct su_link;

typedef struct su_ui {
  void *user;

  void (*say)(void *user, su_msg_t kind, const char *text);

  // one of n options (ids: stable names for scripts and forms, names: for
  // people); def is preselected; returns its index in *out
  int (*choose)(void *user, const char *key, const char *question, const char *const *ids,
                const char *const *names, int n, int def, int *out);

  // a number in [min, max]; def is the suggestion (nan: none). unknown_ok:
  // the user may answer "unknown" (SU_UNKNOWN)
  int (*number)(void *user, const char *key, const char *question, const char *unit, double def,
                double min, double max, int unknown_ok, double *out);

  // yes / no; *out 1 = yes
  int (*confirm)(void *user, const char *key, const char *question, int *out);

  // the user does something by hand (wire, turn the shaft, ...) and then
  // continues. drive is the open link: an answers file can play the hand
  // in the simulator through it.
  int (*action)(void *user, const char *key, const char *instruction, struct su_link *drive);

  // a long operation: fraction 0..1, or < 0 when unknown
  void (*progress)(void *user, const char *what, double fraction);
} su_ui_t;
