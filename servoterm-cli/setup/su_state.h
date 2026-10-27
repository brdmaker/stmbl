/*
 * Setup wizard: the session state.
 *
 * Every answer and every measured value is a key = value pair ("supply.mains
 * = 230", "id.r = 2.15"). The steps read and write them here, and after each
 * step the wizard saves the whole state to the session file, so an
 * interrupted setup resumes where it stopped, and a GUI can show or edit any
 * of it. Keys are lower case, dotted: <step>.<name>; "done.<step> = 1" marks
 * a finished step.
 */
#pragma once
#include <stddef.h>

typedef struct su_state su_state_t;

su_state_t *su_state_new(void);
void su_state_free(su_state_t *s);

// value of key, or NULL
const char *su_get(const su_state_t *s, const char *key);
// number, or def when the key is missing or not a number
double su_get_num(const su_state_t *s, const char *key, double def);
int su_has(const su_state_t *s, const char *key);

void su_set(su_state_t *s, const char *key, const char *value);
void su_set_num(su_state_t *s, const char *key, double value);
void su_unset(su_state_t *s, const char *key);
// drop every key starting with prefix ("id." forgets the identification)
void su_unset_prefix(su_state_t *s, const char *prefix);

// iterate: i from 0 while it returns 1
int su_entry(const su_state_t *s, size_t i, const char **key, const char **value);

// "key = value" lines, '#' comments; returns 0, or -1 when the file can't
// be read / written
int su_state_load(su_state_t *s, const char *path);
int su_state_save(const su_state_t *s, const char *path);
// the same from / to text (parse appends; to_text is malloc'd)
void su_state_parse(su_state_t *s, const char *text);
char *su_state_to_text(const su_state_t *s);
