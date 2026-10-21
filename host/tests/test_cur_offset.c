// shared/cur_offset.h: the F3's current offsets follow drift while the
// bridge is off (DEBUGNOTES "Align phasing: the rotor drifts"). TAP.
#include <math.h>
#include <stdio.h>
#include "cur_offset.h"

static int n, fails;
static void ok(int c, const char *name, double v) {
  n++;
  if(!c) {
    fails++;
  }
  printf("%sok %d - cur_offset: %s%s", c ? "" : "not ", n, name, c ? "\n" : "");
  if(!c) {
    printf(" (got %g)\n", v);
  }
}

#define F 15000.0f  // the F3's loop
#define DT (1.0f / F)

// raw phase readings (what the ADC gives as "current"), sensor offsets
// drifted by drift[]; real current cur[] flows only with the bridge on
static void run(cur_offset_t *s, float *off, const float *drift, const float *cur, int bridge_on, float t) {
  for(int k = 0; k < (int)(t * F); k++) {
    float i[3];
    for(int p = 0; p < 3; p++) {
      float raw = drift[p] - (bridge_on ? cur[p] : 0.0f);  // i = off - raw
      i[p]      = off[p] - raw;
    }
    cur_offset_track(s, bridge_on, i, off, 3, DT, 0.1f, 1.0f, 0.5f);
  }
}

int main(void) {
  cur_offset_t s = {0};
  float off[3]   = {0, 0, 0};             // calibrated at power-up
  float drift[3] = {0.09f, -0.05f, -0.04f};  // the sensors drifted since
  float none[3]  = {0, 0, 0};

  // bridge off for 6 s: the offsets follow (1 s time constant)
  run(&s, off, drift, none, 0, 6.0f);
  float e = fabsf(off[0] - drift[0]) + fabsf(off[1] - drift[1]) + fabsf(off[2] - drift[2]);
  ok(e < 0.002f, "bridge off: offsets converge on the drift (reading back to ~0)", e);

  // bridge on: a real current must not be taken for offset
  float cur[3] = {1.0f, -0.5f, -0.5f};
  float o0     = off[0];
  run(&s, off, drift, cur, 1, 2.0f);
  ok(off[0] == o0, "bridge on: offsets frozen", off[0] - o0);

  // just switched off: the current decays first (settle 0.1 s), no update
  drift[0] = 0.2f;
  o0       = off[0];
  run(&s, off, drift, none, 0, 0.05f);
  ok(off[0] == o0, "within the settle time after the bridge goes off: no update", off[0] - o0);

  // a reading above 0.5 A with the bridge off is no offset: left alone
  cur_offset_t s2 = {0};
  float off2[3]   = {0, 0, 0};
  float big[3]    = {2.0f, 0, 0};
  run(&s2, off2, big, none, 0, 3.0f);
  ok(off2[0] == 0.0f, "a reading above max_err is not taken as offset", off2[0]);

  printf("1..%d\n# %d/%d passed\n", n, n - fails, n);
  return fails != 0;
}
