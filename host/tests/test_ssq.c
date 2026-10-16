/*
 * shared/sserial_q.h: noise-shaped quantisation of process data (power in
 * 8 bits, 20 W a step). Prints TAP.
 */
#include "sserial_q.h"
#include <math.h>
#include <stdio.h>

static int n, fails;

static void ok(int c, const char *name, double got) {
  n++;
  fails += !c;
  printf("%s %d - ssq: %s\n", c ? "ok" : "not ok", n, name);
  if(!c) {
    printf("#   got %g\n", got);
  }
}

// mean of the decoded values (raw * 20 W) over k frames of value v(t)
static double run(double (*v)(int), int k, double *maxerr) {
  ssq_t q = {0};
  double sum = 0, vsum = 0;
  *maxerr = 0;
  for(int i = 0; i < k; i++) {
    double x = v(i), d = ssq_encode(&q, (float)x, 20.0f, 127) * 20.0;
    sum += d, vsum += x;
    *maxerr = fabs(d - x) > *maxerr ? fabs(d - x) : *maxerr;
  }
  return (sum - vsum) / k;
}

static double idle(int i) {
  (void)i;
  return 15.0;  // a drive at rest: under one step
}
static double ramp(int i) {
  return -300.0 + 1.37 * i;  // regeneration to motoring
}

int main(void) {
  double me;
  double b = run(idle, 1000, &me);
  ok(fabs(b) < 0.05 && me <= 20.0, "15 W (under one 20 W step): mean exact, any frame within a step", b);
  b = run(ramp, 1000, &me);
  ok(fabs(b) < 0.1 && me <= 20.0, "a ramp -300..+1070 W: mean error < 0.1 W, any frame within a step", b);
  double m50 = 0;  // a 50 ms window (a fuse model / meter integrates longer)
  ssq_t q = {0};
  for(int i = 0; i < 50; i++) {
    m50 += ssq_encode(&q, 437.3f, 20.0f, 127) * 20.0;
  }
  ok(fabs(m50 / 50 - 437.3) < 0.5, "437.3 W over 50 frames: within 0.5 W", m50 / 50);
  q.err = 0;
  int sat = ssq_encode(&q, 3000.0f, 20.0f, 127);
  for(int i = 0; i < 100; i++) {
    sat = ssq_encode(&q, 3000.0f, 20.0f, 127);
  }
  int back = ssq_encode(&q, 100.0f, 20.0f, 127);
  ok(sat == 127 && (back == 5 || back == 6), "saturates at 127 (2540 W) and does not wind up: 100 W right after", back);
  q.err = 0;
  ok(ssq_encode(&q, -2600.0f, 20.0f, 127) == -127, "negative saturation at -127", 0);
  printf("1..%d\n# %d/%d passed\n", n, n - fails, n);
  return fails ? 1 : 0;
}
