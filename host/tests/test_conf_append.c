/*
 * shared/conf_append.h: appendconf never writes past config[] and leaves
 * the config whole when a line doesn't fit. Prints TAP.
 */
#include "conf_append.h"
#include <stdio.h>
#include <string.h>

static int n, fails;

static void ok(int c, const char *name) {
  n++;
  fails += !c;
  printf("%s %d - conf_append: %s\n", c ? "ok" : "not ok", n, name);
}

#define SZ 32
static struct {
  char config[SZ];
  char canary[16];
} m;

static void reset(void) {
  memset(m.config, 0, SZ);
  memset(m.canary, 0x5a, sizeof(m.canary));
}

static int canary_ok(void) {
  for(size_t i = 0; i < sizeof(m.canary); i++)
    if((unsigned char)m.canary[i] != 0x5a)
      return 0;
  return 1;
}

int main(void) {
  reset();
  ok(conf_append(m.config, SZ, "a=1") == 0, "a short line fits");
  ok(!strcmp(m.config, "a=1\n"), "line plus newline appended");
  ok(conf_append(m.config, SZ, "bb=22") == 0 && !strcmp(m.config, "a=1\nbb=22\n"), "appends after the last line");

  // fill to exactly size - 1 characters: the last byte that fits
  reset();
  memset(m.config, 'x', SZ - 3);
  ok(conf_append(m.config, SZ, "y") == 0 && strlen(m.config) == SZ - 1, "a line that just fits");
  ok(canary_ok(), "the exact fit stays inside config[]");

  // a line too long for the space left
  reset();
  memset(m.config, 'x', SZ - 5);
  char before[SZ];
  memcpy(before, m.config, SZ);
  ok(conf_append(m.config, SZ, "toolongline=123") != 0, "a line that doesn't fit is refused");
  ok(canary_ok(), "nothing written past config[] (overflowed before 2026-11-10)");
  ok(!memcmp(before, m.config, SZ), "the config is left unchanged, not cut mid-line");

  // a full config (strlen == size - 1)
  reset();
  memset(m.config, 'x', SZ - 1);
  ok(conf_append(m.config, SZ, "a") != 0 && canary_ok(), "a full config refuses even one byte");
  ok(m.config[SZ - 1] == 0, "and stays terminated");

  // the line fits but its '\n' doesn't
  reset();
  memset(m.config, 'x', SZ - 3);
  ok(conf_append(m.config, SZ, "ab") != 0 && canary_ok() && strlen(m.config) == SZ - 3, "no room for the newline: refused");

  printf("1..%d\n", n);
  return fails != 0;
}
