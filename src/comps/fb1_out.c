// fb1_out: drive the fb1 RS-485 transceivers, for testing the port
// (hwtest/fbtest). Each pair (a: pins 1-2, b: 3-6, z: 5-4) has a
// SN65176B with DE and /RE tied to one enable pin (1k pull-down, receive
// by default) and D and R tied to one MCU pin. With en_x > 0 the
// transceiver transmits the level of x (or the quadrature pattern below);
// with en_x <= 0 it receives again and io0.fb1x reads the line.
//
// div > 0: a and b step through quadrature, one step every div rt periods,
// direction by the sign of div; z is high in step 0. div = 0: a, b and z
// are the static levels of the pins.
#include "fb1_out_comp.h"
#include "commands.h"
#include "hal.h"
#include "math.h"
#include "defines.h"
#include "angle.h"
#include "stm32f4xx_conf.h"
#include "hw/hw.h"

HAL_COMP(fb1_out);

HAL_PIN(en_a);
HAL_PIN(en_b);
HAL_PIN(en_z);
HAL_PIN(a);
HAL_PIN(b);
HAL_PIN(z);
HAL_PIN(div);
HAL_PIN(step);  // quadrature step count

struct fb1_out_ctx_t {
  uint32_t en;  // bit 0..2: a, b, z transmitting
  uint32_t ticks;
  int32_t step;
};

static GPIO_TypeDef *const d_port[3]  = {FB1_A_PORT, FB1_B_PORT, FB1_Z_PORT};
static const uint16_t d_pin[3]        = {FB1_A_PIN, FB1_B_PIN, FB1_Z_PIN};
static GPIO_TypeDef *const en_port[3] = {FB1_A_TXEN_PORT, FB1_B_TXEN_PORT, FB1_Z_TXEN_PORT};
static const uint16_t en_pin[3]       = {FB1_A_TXEN_PIN, FB1_B_TXEN_PIN, FB1_Z_TXEN_PIN};

static void mode(GPIO_TypeDef *port, uint16_t pin, uint32_t m) {  // 0 in, 1 out
  uint32_t n    = __builtin_ctz(pin) * 2;
  port->MODER = (port->MODER & ~(3u << n)) | (m << n);
}

static void set(int i, int on, int level) {
  if(level > 0) {
    d_port[i]->BSRRL = d_pin[i];
  } else {
    d_port[i]->BSRRH = d_pin[i];
  }
  if(on) {
    // receiver off first (its R output drives the same net), then the MCU pin
    en_port[i]->BSRRL = en_pin[i];
    mode(d_port[i], d_pin[i], 1);
  } else {
    mode(d_port[i], d_pin[i], 0);
    en_port[i]->BSRRH = en_pin[i];
  }
}

static void hw_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct fb1_out_ctx_t *ctx = (struct fb1_out_ctx_t *)ctx_ptr;
  GPIO_InitTypeDef GPIO_InitStructure;
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
  for(int i = 0; i < 3; i++) {
    en_port[i]->BSRRH            = en_pin[i];
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Pin  = en_pin[i];
    GPIO_Init(en_port[i], &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_Pin  = d_pin[i];
    GPIO_Init(d_port[i], &GPIO_InitStructure);
  }
  ctx->en    = 0;
  ctx->ticks = 0;
  ctx->step  = 0;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct fb1_out_ctx_t *ctx      = (struct fb1_out_ctx_t *)ctx_ptr;
  struct fb1_out_pin_ctx_t *pins = (struct fb1_out_pin_ctx_t *)pin_ptr;

  int level[3];
  int div = (int)PIN(div);
  if(div != 0) {
    if(++ctx->ticks >= (uint32_t)ABS(div)) {
      ctx->ticks = 0;
      ctx->step += div > 0 ? 1 : -1;
    }
    int q    = ctx->step & 3;  // 0: a0 b0, 1: a1 b0, 2: a1 b1, 3: a0 b1
    level[0] = q == 1 || q == 2;
    level[1] = q >= 2;
    level[2] = q == 0;
  } else {
    level[0] = PIN(a) > 0;
    level[1] = PIN(b) > 0;
    level[2] = PIN(z) > 0;
  }
  PIN(step) = ctx->step;

  float en[3] = {PIN(en_a), PIN(en_b), PIN(en_z)};
  for(int i = 0; i < 3; i++) {
    int on = en[i] > 0;
    if(on) {
      if(!(ctx->en & (1u << i))) {
        set(i, 1, level[i]);
      } else if(level[i]) {
        d_port[i]->BSRRL = d_pin[i];
      } else {
        d_port[i]->BSRRH = d_pin[i];
      }
      ctx->en |= 1u << i;
    } else if(ctx->en & (1u << i)) {
      set(i, 0, 0);
      ctx->en &= ~(1u << i);
    }
  }
}

static void rt_stop(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct fb1_out_ctx_t *ctx = (struct fb1_out_ctx_t *)ctx_ptr;
  for(int i = 0; i < 3; i++) {
    set(i, 0, 0);
  }
  ctx->en = 0;
}

const hal_comp_t fb1_out_comp_struct = {
    .name      = "fb1_out",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = 0,
    .hw_init   = hw_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = rt_stop,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct fb1_out_ctx_t),
    .pin_count = sizeof(struct fb1_out_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
