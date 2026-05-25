/* Minimal TAP test runner.  Include once per test binary.
 *
 * ok(cond, desc)               — emit "ok N - desc" or "not ok N - desc"
 * ok_float(a, b, eps, desc)    — ok() with floating-point tolerance
 * done_testing()               — print "1..N" plan, return exit code
 */
#pragma once
#include <stdio.h>
#include <math.h>

static int _t_count = 0;
static int _t_fail  = 0;

#define ok(cond, desc) do { \
  _t_count++; \
  if(cond) { \
    printf("ok %d - %s\n", _t_count, (desc)); \
  } else { \
    printf("not ok %d - %s\n", _t_count, (desc)); \
    _t_fail++; \
  } \
} while(0)

#define ok_float(a, b, eps, desc) \
  ok(fabsf((float)(a) - (float)(b)) < (float)(eps), (desc))

static inline int done_testing(void) {
  printf("1..%d\n", _t_count);
  if(_t_fail) {
    printf("# FAIL: %d/%d tests failed\n", _t_fail, _t_count);
  }
  return _t_fail ? 1 : 0;
}
