/*
 * Setup wizard: the link to the drive's console (USB CDC serial, the
 * smart-serial console's pty, or the simulator's pty).
 *
 * The drive takes command lines of at most 63 characters and answers in
 * text, interleaved with scope frames (0xFF + 8 channel bytes, 0xFE resets);
 * the frames are dropped here. Received text accumulates in a buffer that
 * the caller reads and clears: commands answer asynchronously, and some
 * output (the encoder's "cnt = N" at the index) comes whenever it happens.
 *
 * Everything lives in the su_link_t, no globals, so a GUI can hold several.
 */
#pragma once
#include <stddef.h>

#define SU_LINE_MAX 63

typedef struct su_link su_link_t;

su_link_t *su_link_open(const char *dev, char *err, size_t errlen);
void su_link_close(su_link_t *l);

// every piece of text from the drive, as it comes (logging, a GUI's console)
void su_link_on_text(su_link_t *l, void (*fn)(void *user, const char *text, size_t n), void *user);

// send one command line (no newline); -1 on error or a line too long
int su_link_send(su_link_t *l, const char *line);
// read for up to sec; returns text bytes received, -1 when the drive is gone
// (a drive that goes away and comes back within 15 s is reopened: sends and
// reads go on; su_link_reconnects counts it)
int su_link_pump(su_link_t *l, double sec);
int su_link_reconnects(const su_link_t *l);
// the text received since the last clear
const char *su_link_text(const su_link_t *l);
void su_link_clear(su_link_t *l);

// send cmd and collect the answer: until done appears (if given), else
// until the drive has been quiet for quiet s after answering; at most
// timeout s. Returns the text (valid until the next call) or NULL on error.
const char *su_link_query(su_link_t *l, const char *cmd, double quiet, const char *done, double timeout);

// a pin's value in full precision ("getf pin" prints "pin = 4.38e-05");
// 0 ok, -1 no answer
int su_link_get(su_link_t *l, const char *pin, double *v);
// "pin = value" (a number or another pin: a link); 0 when the drive said OK
int su_link_set(su_link_t *l, const char *pin, const char *value);
int su_link_set_num(su_link_t *l, const char *pin, double v);

// "reset" and reconnect: a real drive re-enumerates on USB, so the port is
// closed and reopened until the drive answers again (up to timeout s)
int su_link_reset(su_link_t *l, double timeout);

// the last occurrence of "<pin> = <number>" in text (also "<pin> <= src =
// <number>"), 0 found
int su_parse_pin(const char *text, const char *pin, double *v);
