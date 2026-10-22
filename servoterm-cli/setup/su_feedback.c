#include "su_feedback.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const su_fb_type_t su_fb_types[] = {
    {"enc", "Incremental encoder (A/B with index Z)", "enc_fb0", NULL, "enc_fb0.pos", SU_FB_RES_MEASURED,
     "Encoder", 1},
    {"sincos", "Sin/cos encoder, 1 Vpp", "enc_fb0", "fb_switch0.mot_pos = enc_fb0.ipos", "enc_fb0.ipos",
     SU_FB_RES_MEASURED, "1Vpp", 0},
    {"resolver", "Resolver", "res_fb0", NULL, "res0.pos", SU_FB_RES_POLES, "Resolver", 0},
    {"smartabs", "Tamagawa smartabs (serial absolute)", "smartabs_fb0", NULL, "smartabs0.pos", SU_FB_RES_FIXED,
     "RS485", 0},
    {"mitsubishi", "Mitsubishi serial absolute encoder", "encm_fb0", NULL, "encm0.pos", SU_FB_RES_FIXED,
     "Mitsubishi", 0},
    {"sanyo", "Sanyo Denki serial absolute encoder", "encs_fb0", NULL, "encs0.pos", SU_FB_RES_FIXED,
     "Sanyo-Denki", 0},
};
const int su_fb_ntypes = sizeof(su_fb_types) / sizeof(su_fb_types[0]);

const su_fb_type_t *su_fb_find(const char *id) {
  for(int i = 0; id && i < su_fb_ntypes; i++) {
    if(strcmp(su_fb_types[i].id, id) == 0) {
      return &su_fb_types[i];
    }
  }
  return NULL;
}

int su_fb_parse_index(const char *text, long *cnt, int n) {
  int k = 0;
  for(const char *p = text; p && *p && k < n;) {
    long v;
    if(sscanf(p, "cnt = %ld", &v) == 1) {
      cnt[k++] = v;
    }
    p = strchr(p, '\n');
    p = p ? p + 1 : NULL;
  }
  return k;
}

int su_fb_counts(const long *cnt, int n, long wrap, long *counts, int *dir) {
  if(n < 2) {
    return -1;
  }
  long first = 0;
  for(int i = 1; i < n; i++) {
    long d = ((cnt[i] - cnt[i - 1]) % wrap + wrap) % wrap;
    if(d > wrap / 2) {
      d -= wrap;
    }
    if(d == 0) {
      return -2;  // the same count twice: res already matches, or back and forth
    }
    if(i == 1) {
      first = d;
    } else if(d != first) {
      return -2;
    }
  }
  *counts = labs(first);
  *dir    = first > 0 ? 1 : -1;
  return 0;
}

long su_fb_lines(long counts) {
  return counts > 0 && counts % 4 == 0 ? counts / 4 : 0;
}
