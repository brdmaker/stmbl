#include "idx_home_comp.h"
#include "commands.h"
#include "hal.h"
#include "math.h"
#include "defines.h"
#include "angle.h"

HAL_COMP(idx_home);

HAL_PIN(mot_state);  // 0 = disabled, 1 = inc, 2 = start abs, 3 = abs
HAL_PIN(fb);
HAL_PIN(fb_abs);
HAL_PIN(index_en);
HAL_PIN(index_clear);
HAL_PIN(pos_out);
// after index homing: what the command must be shifted by. pos_out
// switches to fb_abs at the index, and LinuxCNC moves its command into
// those coordinates once it sees index_clear; the drive's loop still runs
// in fb's (fb_switch). From the command that drops index_en on, linrev
// takes this off the command (linrev0.cmd_offset).
HAL_PIN(cmd_offset);

struct idx_home_ctx_t {
  int state;
  int lastq;
  int waitabs;
  int armed;  // index_en was set last period: lastq is this request's
  int homed;  // index found and LinuxCNC answered (index_en dropped)
};

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct idx_home_ctx_t *ctx      = (struct idx_home_ctx_t *)ctx_ptr;
  struct idx_home_pin_ctx_t *pins = (struct idx_home_pin_ctx_t *)pin_ptr;

  uint8_t q = 0;

  if(PIN(index_en) > 0) {
    if(PIN(mot_state) == 3) {
      q = quadrant(PIN(fb_abs));
      if(!ctx->armed) {  // a new request: lastq was from the one before, where it stopped past the index
        ctx->lastq = q;
      }
      if(((q == 1 && ctx->lastq == 4) || (q == 4 && ctx->lastq == 1)) || ctx->waitabs == 1) {
        PIN(index_clear) = 1;
        ctx->state       = 1;
        ctx->waitabs     = 0;
      }
      ctx->lastq = q;
    } else {  //index requested, but index not seen
      ctx->waitabs = 1;
    }
    ctx->armed = 1;
  } else {  // no request (or it was withdrawn): nothing pending
    ctx->armed   = 0;
    ctx->waitabs = 0;
  }

  if(!(PIN(index_en) > 0) && ctx->state == 1) {
    PIN(index_clear) = 0;
    ctx->homed       = 1;
  }
  PIN(cmd_offset) = ctx->homed ? minus(PIN(fb_abs), PIN(fb)) : 0.0;

  if(ctx->state == 1) {
    PIN(pos_out) = PIN(fb_abs);
  } else {
    PIN(pos_out) = PIN(fb);
  }
}

hal_comp_t idx_home_comp_struct = {
    .name      = "idx_home",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = 0,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct idx_home_ctx_t),
    .pin_count = sizeof(struct idx_home_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
