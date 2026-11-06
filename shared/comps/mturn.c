#include "mturn_comp.h"
/*
 * mturn: the multi-turn position of an absolute encoder, for linrev.
 *
 * linrev counts motor turns from 0 at every boot, so LinuxCNC only gets a
 * position that is right within one motor turn. An absolute encoder with a
 * battery (or backup capacitor) also reports how many turns it has made.
 * mturn turns that into linrev's turn count, once, on the first good frame
 * after boot (load), and then checks every cycle that linrev and the encoder
 * still agree. The drive's own loop works on the single-turn angle, so the
 * load never moves the motor.
 *
 * Inputs (docs/absolute_encoders.md):
 *   pos         the single-turn angle linrev counts (linrev0.fb_in)
 *   enc_pos     the encoder's own single-turn angle (e.g. encf0.abs_pos)
 *   turns       the encoder's turn count, as it sends it
 *   turns_bits  its counter width: 16 means 0..65535 or -32768..32767,
 *               wrapping; 0 means a signed count that never wraps
 *   dir         > 0: the drive counts the other way (conf0.mot_fb_rev)
 *   mt_ok       1: the encoder says its turn count is valid (battery kept,
 *               referenced); 0: it is not
 *   batt        1: the encoder warns of a low battery (passed to batt_warn)
 *   error       the encoder's frame error (no position this cycle)
 *   lin_rev     linrev0.rev, for the check
 *   clear       rising edge: forget a lost or mismatched turn count (after
 *               the machine has been referenced again); the next good
 *               frame loads again
 *
 * Outputs:
 *   rev, load   linrev0.rev_in / rev_load: load is 1 for the one cycle
 *               linrev must take rev
 *   abs_ok      1: the position over smart-serial is absolute and checked
 *   state       0 waiting for a frame (and for pos to agree with enc_pos,
 *               the same shaft: pos may be a cycle old after boot),
 *               1 absolute, 2 turn count lost
 *               (mt_ok was 0 since boot or the last clear: single-turn
 *               only), 3 mismatch (linrev and the encoder disagreed for
 *               longer than check_time, or pos never agreed with enc_pos
 *               within 50 check_time: wired to another encoder, dir wrong)
 *   batt_warn   batt, held for the GUI
 *   loads       how many times it loaded (1 after boot, +1 after every
 *               encoder error that recovered)
 *   err         the last disagreement [rad of motor angle]
 *
 * Order: after idx_home0 and before linrev0 (rt_prio 2.08 in mturn.txt).
 */
#include "hal.h"
#include "angle.h"
#include "defines.h"
#include <math.h>

HAL_COMP(mturn);

HAL_PIN(pos);
HAL_PIN(enc_pos);
HAL_PIN(turns);
HAL_PIN(turns_bits);
HAL_PIN(dir);
HAL_PIN(mt_ok);
HAL_PIN(batt);
HAL_PIN(error);
HAL_PIN(lin_rev);
HAL_PIN(clear);
HAL_PIN(check_time);  // [s] a disagreement must last this long (default 0.002)

HAL_PIN(rev);
HAL_PIN(load);
HAL_PIN(abs_ok);
HAL_PIN(state);
HAL_PIN(batt_warn);
HAL_PIN(loads);
HAL_PIN(err);

struct mturn_ctx_t {
  int loaded;      // linrev holds the encoder's turns
  int lost;        // the turn count was not valid since boot / the last clear
  int mismatch;    // linrev and the encoder disagreed
  int32_t rev_off; // linrev's turn count minus the encoder's (dir applied), set at load
  float bad_t;     // how long they have disagreed
  float wait_t;    // how long pos and enc_pos have disagreed before the load
  float last_clear;
};

// the encoder's turn count as a signed integer, wrapped to its counter width
static int32_t sturns(float turns, int bits) {
  int32_t t = (int32_t)lroundf(turns);
  if(bits > 0 && bits < 32) {
    int32_t full = (int32_t)1 << bits;
    t &= full - 1;
    if(t >= full / 2) {
      t -= full;
    }
  }
  return t;
}

// a turn difference wrapped to the counter width (0: as is)
static int32_t wrapbits(int32_t d, int bits) {
  if(bits > 0 && bits < 32) {
    int32_t full = (int32_t)1 << bits;
    d &= full - 1;
    if(d >= full / 2) {
      d -= full;
    }
  }
  return d;
}

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct mturn_pin_ctx_t *pins = (struct mturn_pin_ctx_t *)pin_ptr;
  PIN(turns_bits) = 16;
  PIN(mt_ok)      = 1.0;
  PIN(check_time) = 0.002;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct mturn_ctx_t *ctx      = (struct mturn_ctx_t *)ctx_ptr;
  struct mturn_pin_ctx_t *pins = (struct mturn_pin_ctx_t *)pin_ptr;

  int bits     = (int)PIN(turns_bits);
  int32_t sdir = PIN(dir) > 0.0 ? -1 : 1;
  PIN(load)    = 0.0;
  PIN(batt_warn) = PIN(batt) > 0.0;

  int clear_edge  = PIN(clear) > 0.0 && ctx->last_clear <= 0.0;
  ctx->last_clear = PIN(clear);
  if(clear_edge) {
    ctx->lost     = 0;
    ctx->mismatch = 0;
    ctx->loaded   = 0;
    ctx->bad_t    = 0.0;
    ctx->wait_t   = 0.0;
  }

  if(PIN(error) > 0.0) {  // no position this cycle: hold, load again once it is back
    ctx->loaded = 0;
    ctx->bad_t  = 0.0;
  } else {
    if(PIN(mt_ok) <= 0.0) {
      ctx->lost = 1;  // latched: a valid count later is not tied to the machine any more
    }
    int32_t t = sturns(PIN(turns), bits);
    float pos = PIN(pos);
    // the turn counter steps where the encoder's count wraps to 0, so its
    // angle within the turn is 0..2pi (enc_pos may come as -pi..pi)
    float e0  = mod(PIN(enc_pos));
    e0        = e0 < 0.0 ? e0 + 2.0f * M_PI : e0;
    float enc = sdir * e0;
    if(!ctx->loaded && !ctx->lost && !ctx->mismatch && fabsf(minus(pos, enc)) > M_PI * 0.5) {
      // pos and the encoder's angle are the same shaft: until they agree
      // (pos may be a cycle old after boot), a load would be a turn off
      ctx->wait_t += period;
      if(ctx->wait_t >= 50.0 * PIN(check_time)) {
        ctx->mismatch = 1;  // they never agree: pos is not this encoder's
      }
    } else if(!ctx->loaded && !ctx->lost && !ctx->mismatch) {
      // rev * 2pi + pos = sdir * (t * 2pi + e0), to the nearest turn
      ctx->wait_t = 0.0;
      int32_t frac = (int32_t)lroundf((enc - pos) / (2.0f * M_PI));
      int32_t rev  = sdir * t + frac;
      ctx->rev_off = frac;
      ctx->loaded  = 1;
      ctx->bad_t   = 0.0;
      PIN(rev)     = rev;
      PIN(load)    = 1.0;
      PIN(loads) += 1.0;
    } else if(ctx->loaded) {
      // the check: linrev's turns and the encoder's, plus the angle in between
      int32_t n = wrapbits((int32_t)lroundf(PIN(lin_rev)) - sdir * t - ctx->rev_off, bits);
      float e   = n * 2.0f * M_PI + (ctx->rev_off * 2.0f * M_PI + pos - enc);  // rev_off: the turn the load added
      PIN(err)  = e;
      if(fabsf(e) > M_PI * 0.5) {
        ctx->bad_t += period;
        if(ctx->bad_t >= PIN(check_time)) {
          ctx->mismatch = 1;
        }
      } else {
        ctx->bad_t = 0.0;
      }
    }
  }

  if(ctx->mismatch) {
    PIN(state) = 3.0;
  } else if(ctx->lost) {
    PIN(state) = 2.0;
  } else if(ctx->loaded) {
    PIN(state) = 1.0;
  } else {
    PIN(state) = 0.0;
  }
  PIN(abs_ok) = PIN(state) == 1.0;
}

hal_comp_t mturn_comp_struct = {
    .name      = "mturn",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct mturn_ctx_t),
    .pin_count = sizeof(struct mturn_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
