#include "vibaddr.h"
#include <string.h>

static int prefix_match(const uint8_t *uid, const uint8_t *prefix, int n) {
  for(int i = 0; i < n; i++) {
    int b = 7 - i % 8;
    if(((uid[i / 8] >> b) & 1) != ((prefix[i / 8] >> b) & 1)) {
      return 0;
    }
  }
  return 1;
}

static void reply(vibp_frame_t *out, uint8_t code, int len) {
  out->id   = VIBA_RSP_ID;
  out->len  = vibp_fd_len(len);
  out->d[0] = code;
}

static void info(const vibaddr_t *a, vibp_frame_t *out, uint8_t code) {
  memset(out->d, 0, 32);
  reply(out, code, 32);
  memcpy(out->d + 1, a->uid, VIBA_UID);
  out->d[13] = a->node;
  vibp_u16(out->d + 14, a->fw);
  memcpy(out->d + 16, a->name, VIBA_NAME);
}

int vibaddr_rx(vibaddr_t *a, const vibp_frame_t *in, vibp_frame_t *out) {
  if(in->id != VIBA_CMD_ID || in->len < 1) {
    return 0;
  }
  const uint8_t *d = in->d;
  int mine         = in->len >= 1 + VIBA_UID && !memcmp(d + 1, a->uid, VIBA_UID);
  switch(d[0]) {
    case VIBA_SCAN:
      if(in->len < 3 + VIBA_UID || a->muted || (d[1] == 0 && a->node) || d[2] > 8 * VIBA_UID ||
         !prefix_match(a->uid, d + 3, d[2])) {
        return 0;
      }
      memset(out->d, 0, 8);
      reply(out, VIBA_YES, 1);
      return 1;
    case VIBA_QUERY:
      if(!mine) {
        return 0;
      }
      info(a, out, VIBA_INFO);
      return 1;
    case VIBA_SET:
      if(!mine || in->len < 31) {
        return 0;
      }
      if((d[30] & VIBA_SET_NODE) && d[13] <= VIBP_NODE_MAX) {
        a->node = d[13];
      }
      if(d[30] & VIBA_SET_NAME) {
        memcpy(a->name, d + 14, VIBA_NAME);
      }
      info(a, out, VIBA_SET_OK);
      return 1;
    case VIBA_STORE:
      if(!mine) {
        return 0;
      }
      a->store_req = 1;
      memset(out->d, 0, 16);
      reply(out, VIBA_STORED, 14);
      memcpy(out->d + 1, a->uid, VIBA_UID);
      out->d[13] = a->node;
      return 1;
    case VIBA_MUTE:
      if(mine && in->len >= 14) {
        a->muted = d[13] != 0;
      }
      return 0;
    case VIBA_UNMUTE:
      a->muted = 0;
      return 0;
    case VIBA_IDENTIFY:
      a->identify = in->len >= 2 && d[1] != 0;
      return 0;
  }
  return 0;
}

int vibaddr_tap(const vibaddr_t *a, vibp_frame_t *out) {
  if(!a->identify) {
    return 0;
  }
  memset(out->d, 0, 16);
  reply(out, VIBA_TAP, 14);
  memcpy(out->d + 1, a->uid, VIBA_UID);
  out->d[13] = a->node;
  return 1;
}
