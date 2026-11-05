#include "fanuc_tw.h"

void f2w_init(f2w_t *d, int max_err, int nref_bit) {
  uint8_t *p = (uint8_t *)d;
  for(unsigned i = 0; i < sizeof(*d); i++) {
    p[i] = 0;
  }
  d->max_err  = max_err < 1 ? 1 : max_err;
  d->nref_bit = nref_bit;
  d->error    = 1;
}

void f2w_poll(f2w_t *d, int res, const f1p_frame_t *f) {
  if(res == F1P_OK) {
    if(d->have) {
      for(int k = 0; k < 5; k++) {
        d->chg[k] |= d->f.w[k] ^ f->w[k];
      }
    }
    d->f       = *f;
    d->have    = 1;
    d->ok++;
    d->bad_run = 0;
    d->nref    = d->nref_bit >= 0 && f1p_frame_bit(f, d->nref_bit) == 1;
    d->state   = d->nref ? 1 : 3;
    d->error   = 0;
    return;
  }
  if(res == F2W_NOREPLY) {
    d->noreply++;
  } else if(res == F1P_CRC) {
    d->crc_er++;
  } else {
    d->sync_er++;  // F1P_SYNC or F1P_SHORT: a broken frame
  }
  d->bad_run++;
  if(d->bad_run >= (uint32_t)d->max_err || !d->have) {
    d->state = 0;
    d->error = 1;
  }
  // else: keep the last good frame and state for this poll
}

void f2w_chg_reset(f2w_t *d) {
  for(int k = 0; k < 5; k++) {
    d->chg[k] = 0;
  }
}

int f2w_status_differs(const f1p_frame_t *a, const f1p_frame_t *b, uint16_t m0, uint16_t m3, uint16_t m4) {
  return ((a->w[0] ^ b->w[0]) & m0) || ((a->w[3] ^ b->w[3]) & m3) || ((a->w[4] ^ b->w[4]) & m4);
}
