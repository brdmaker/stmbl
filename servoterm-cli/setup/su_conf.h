/*
 * Setup wizard: the drive's config text.
 *
 * The drive keeps its config as text in flash (showconf prints it; deleteconf,
 * appendconf per line and flashsaveconf write it; it is parsed at boot). A
 * config is "link <template>" lines and "pin = value" lines; for a pin set
 * twice the last line wins. Lines are at most 52 characters ("appendconf "
 * plus the line must fit the drive's 63), the whole text 15 KiB.
 *
 * The text functions return malloc'd strings and take ownership of the text
 * they edit (like realloc), so they chain: c = su_conf_set(c, ...).
 */
#pragma once
#include <stddef.h>

struct su_link;

#define SU_CONF_LINE_MAX (63 - 11)  // 63 - strlen("appendconf ")
#define SU_CONF_SIZE_MAX (15 * 1024 - 1)

// as the drive stores it: no CR, tabs as spaces, lines trimmed, no
// trailing empty lines, one final newline
char *su_conf_normalize(const char *in);

// lines too long or not printable, size: number of problems; each is
// described in err (if given)
int su_conf_validate(const char *conf, char *err, size_t errlen);

// "pin = value # comment": replaces the last line setting pin (dropping
// earlier ones), or appends. comment may be NULL.
char *su_conf_set(char *conf, const char *pin, const char *value, const char *comment);
char *su_conf_set_num(char *conf, const char *pin, double v, const char *comment);
// the value the drive would use for pin (the last line), copied into out
int su_conf_get(const char *conf, const char *pin, char *out, size_t outlen);
// "link name" once, after the other link lines
char *su_conf_link(char *conf, const char *name);
int su_conf_has_link(const char *conf, const char *name);
// drop every line for pin
char *su_conf_unset(char *conf, const char *pin);

// the drive's flash config (normalized), NULL on error
char *su_conf_read(struct su_link *l);
// write conf to flash and read it back; 0 ok. The drive runs it after a
// reset. err gets the reason on failure.
int su_conf_write(struct su_link *l, const char *conf, char *err, size_t errlen);
