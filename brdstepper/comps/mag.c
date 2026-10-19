#include "mag_comp.h"
#include "hal.h"
#include "defines.h"
#include "angle.h"
#include "brd.h"
#include <math.h>

// mag: the AS5047P magnetic encoder (14 bit, absolute) on SPI1: the motor feedback,
// pins as the stock encoder components so fb_switch takes it as an
// absolute encoder (state 3).
//
// Frames: a read command 0x4000 | address, bit 15 even parity. The answer
// comes in the next frame: bit 15 parity, bit 14 error flag, bits 13..0
// data. Each rt period sends the angle command (ANGLECOM, 0x3FFF), then a
// NOP to clock out the fresh angle: ~6 us of SPI.

HAL_COMP(mag);

HAL_PIN(pos);      // shaft angle [rad], -pi..pi
HAL_PIN(abs_pos);
HAL_PIN(state);    // 3: absolute, valid; 0: no valid reading
HAL_PIN(error);    // 1 while readings fail
HAL_PIN(errors);   // count of bad frames
HAL_PIN(res);      // counts per turn (16384)
HAL_PIN(raw);

#define CMD_NOP 0xC000u      // read 0x0000, parity set
#define CMD_ANGLECOM 0xFFFFu  // read 0x3FFF (dynamic angle error compensated), parity set

struct mag_ctx_t {
  uint32_t bad_run;  // bad frames in a row
};

static int parity_ok(uint16_t f) {
  return (__builtin_popcount(f) & 1) == 0;  // even over all 16 bits
}

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct mag_pin_ctx_t *pins = (struct mag_pin_ctx_t *)pin_ptr;
  (void)ctx_ptr;
  PIN(res) = 16384;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct mag_ctx_t *ctx      = (struct mag_ctx_t *)ctx_ptr;
  struct mag_pin_ctx_t *pins = (struct mag_pin_ctx_t *)pin_ptr;
  (void)period;
  hw_as5047_read(CMD_ANGLECOM);
  uint16_t f = hw_as5047_read(CMD_NOP);
  if(!parity_ok(f) || (f & 0x4000)) {
    PIN(errors) += 1;
    PIN(error) = 1;
    if(++ctx->bad_run > 10) {  // 2 ms without a good angle: the feedback is gone
      PIN(state) = 0;
    }
    return;
  }
  ctx->bad_run = 0;
  uint16_t a   = f & 0x3fff;
  PIN(raw)     = a;
  PIN(pos)     = mod(a * (2.0 * M_PI / 16384.0));
  PIN(abs_pos) = PIN(pos);
  PIN(state)   = 3;
  PIN(error)   = 0;
}

hal_comp_t mag_comp_struct = {
    .name      = "mag",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct mag_ctx_t),
    .pin_count = sizeof(struct mag_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
