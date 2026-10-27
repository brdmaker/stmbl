// shared/vibproto.h: the measurement bus's frames, and its load
// (docs/vibsensor.md). Prints the load table the document quotes.
#include "vibproto.h"
#include <math.h>
#include <stdio.h>

static int fails, checks;
#define CHECK(c)                                               \
  do {                                                         \
    checks++;                                                  \
    if(!(c)) {                                                 \
      fails++;                                                 \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);      \
    }                                                          \
  } while(0)

static int fd_len_ok(int n) {
  return n <= 8 || n == 12 || n == 16 || n == 20 || n == 24 || n == 32 || n == 48 || n == 64;
}

int main(void) {
  vibp_frame_t f;

  vibp_event_t e = {VIBP_EV_CRASH, 1, 513, 21.5f}, e2;
  vibp_put_event(&f, 3, &e);
  vibp_get_event(&f, &e2);
  CHECK(f.id == 0x013 && f.len == 8);
  CHECK(e2.type == 1 && e2.value == 1 && e2.seq == 513 && e2.x == 21.5f);

  vibp_state_t s = {123456789, 12000.0f, 2500.0f, 4, VIBP_ST_SPINDLE | VIBP_ST_RUNNING, 7}, s2;
  vibp_put_state(&f, &s);
  vibp_get_state(&f, &s2);
  CHECK(f.id == VIBP_STATE_ID && fd_len_ok(f.len));
  CHECK(s2.time_ms == s.time_ms && s2.rpm == s.rpm && s2.feed == s.feed && s2.teeth == 4 && s2.flags == 3 && s2.tool == 7);

  vibp_feat_t v = {1000, 7, 2, 1, {0.1f, 0.2f, 0.3f}, {1, 2, 3}, {4.5f, 5.5f, 6.5f}, {1.5f, 3.0f, 9.0f}, 0.25f, 41.5f}, v2;
  vibp_put_feat(&f, 1, &v);
  vibp_get_feat(&f, &v2);
  CHECK(f.id == 0x101 && f.len == 64);
  CHECK(!memcmp(&v, &v2, sizeof(v)));

  vibp_sum_t u = {2000, 9, 2048, 99.8f, 0.707f, {0.7f, 0.01f, 0.002f}, 0.35f, 1730.0f, 0.28f, 87.0f, 6000.0f}, u2;
  vibp_put_sum(&f, 2, &u);
  vibp_get_sum(&f, &u2);
  CHECK(f.id == 0x142 && fd_len_ok(f.len));
  CHECK(!memcmp(&u, &u2, sizeof(u)));

  // a 2048-point spectrum: 1025 bins in 40 frames, back within 16 bits of its largest
  static float rms[1025], back[1025];
  for(int k = 0; k < 1025; k++) {
    rms[k] = 0.001f + 0.5f * expf(-0.5f * (k - 300) * (k - 300) / 4.0f);
  }
  static vibp_frame_t sf[64];
  int nf = vibp_spec_frames(sf, 4, 77, 0, rms, 1025);
  CHECK(nf == 40);
  for(int i = 0; i < nf; i++) {
    vibp_spec_t sp;
    CHECK(sf[i].id == 0x304 && sf[i].len == 64);
    vibp_get_spec(&sf[i], &sp);
    CHECK(sp.seq == 77 && sp.total == 1025);
    for(int j = 0; j < VIBP_SPEC_BINS && sp.first + j < sp.total; j++) {
      back[sp.first + j] = sp.bin[j] / 65535.0f * sp.scale;
    }
  }
  float err = 0.0f;
  for(int k = 0; k < 1025; k++) {
    err = fmaxf(err, fabsf(back[k] - rms[k]));
  }
  CHECK(err <= 0.5f / 65535.0f * 1.01f);

  vibp_raw_t r = {4000000000u, 1, {{0}}}, r2;
  for(int i = 0; i < VIBP_RAW_SAMPLES; i++) {
    for(int a = 0; a < 3; a++) {
      r.s[i][a] = (int16_t)(i * 1000 - a * 12345 - 32768 * (i == 8));
    }
  }
  vibp_put_raw(&f, 1, &r);
  vibp_get_raw(&f, &r2);
  CHECK(f.id == 0x401 && f.len == 64);
  CHECK(!memcmp(&r, &r2, sizeof(r)));

  CHECK(vibp_put_con(&f, VIBP_CON_TO_ID(2), 5, "vib0.fft_n = 4096", 17) == 17);
  CHECK(f.id == 0x602 && f.len == 20 && f.d[0] == 5 && f.d[1] == 17 && !memcmp(f.d + 2, "vib0.fft_n", 10));
  char longtext[100];
  memset(longtext, 'a', sizeof(longtext));
  CHECK(vibp_put_con(&f, VIBP_CON_FROM_ID(2), 6, longtext, 100) == VIBP_CON_TEXT && f.len == 64);
  for(int n = 0; n <= 64; n++) {
    CHECK(fd_len_ok(vibp_fd_len(n)) && vibp_fd_len(n) >= n);
  }

  // the bus: 1 Mbit/s arbitration, 5 Mbit/s data
  float t64 = vibp_frame_us(64, 1000, 5000), t8 = vibp_frame_us(8, 1000, 5000);
  CHECK(t64 > 130 && t64 < 160);
  CHECK(vibp_frame_us(8, 1000, 0) > 110 && vibp_frame_us(8, 1000, 0) < 130);
  // four sensors: features at 10 Hz, summaries at 13 Hz (2048 points at
  // 26.7 kHz), the PC's state at 50 Hz, a spectrum (40 frames) at 2 Hz each
  float base = 4 * (10 * t64 + 13 * vibp_frame_us(48, 1000, 5000)) + 50 * vibp_frame_us(16, 1000, 5000);
  float spec = 4 * 2 * 40 * t64;
  // raw streaming: 26667 / 9 frames a second per sensor
  float raw = 26667.0f / VIBP_RAW_SAMPLES * t64;
  printf("vibproto: 64 B frame %.0f us, 8 B %.0f us at 1/5 Mbit/s\n", t64, t8);
  printf("vibproto: load: 4 sensors' features + state %.1f %%, + spectra at 2 Hz %.1f %%, raw stream of one sensor %.1f %%\n",
         base / 1e4, (base + spec) / 1e4, raw / 1e4);
  CHECK(base / 1e4 < 5.0f);
  CHECK(raw / 1e4 < 50.0f);

  printf("test_vibproto: %d checks, %d failed\n", checks, fails);
  return fails != 0;
}
