/*
 * shared/iabs.h: the F3's overcurrent trip (io.c, HV_OVERCURRENT_PEAK/RMS)
 * sees every phase's current, of either sign. Prints TAP.
 */
#include "iabs.h"
#include <stdio.h>

static int n, fails;

static void ok(int c, const char *name) {
  n++;
  fails += !c;
  printf("%s %d - iabs: %s\n", c ? "ok" : "not ok", n, name);
}

int main(void) {
  ok(phase_iabs(31.0f, -15.0f, -16.0f) == 31.0f, "+U peak");
  ok(phase_iabs(-31.0f, 15.0f, 16.0f) == 31.0f, "-U peak");
  ok(phase_iabs(-15.0f, 31.0f, -16.0f) == 31.0f, "+V peak");
  ok(phase_iabs(15.0f, -31.0f, 16.0f) == 31.0f, "-V peak (missed before 2026-11-10)");
  ok(phase_iabs(-15.0f, -16.0f, 31.0f) == 31.0f, "+W peak");
  ok(phase_iabs(15.0f, 16.0f, -31.0f) == 31.0f, "-W peak (missed before 2026-11-10)");
  ok(phase_iabs(0.0f, -40.0f, 0.0f) > 30.0f, "a lone -V fault current (V shorted to ground) trips");
  ok(phase_iabs(0.0f, 0.0f, 0.0f) == 0.0f, "no current");
  printf("1..%d\n", n);
  return fails != 0;
}
