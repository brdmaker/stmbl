/*
 * yaskawa_abs: feedback driver for Yaskawa Sigma-II serial absolute encoders
 * (the 16-bit absolute of an SGMAH-xxA1..., 20 bits per turn and a signed
 * 16-bit turn count in its reply; found on the bench 2026-11-10, brdmot
 * yaskawa.md). The decoding and the state are shared/yaskawa_abs.c, tested
 * on recorded frames by host/tests/test_yaskawa_abs.c. The old `yaskawa`
 * comp stays for the encoders it was written for.
 *
 * Every rt cycle: the capture of the last reply is stopped, the next
 * request goes out on FB0's Z pair (TIM8 update -> DMA2 stream 1 ch 7
 * writes GPIO BSRR words, a chip of 125 ns, ~14 us; TX enable PD15), and
 * rt decodes the last reply meanwhile (Manchester at 4 Mbit/s from the
 * edge times, HDLC flag and bit stuffing, CRC-16). When the request is out
 * the DMA's interrupt (priority 1, above rt) turns the line around and
 * TIM4 CH3 captures the reply's edges (DMA1 stream 7 ch 2) into the other
 * of two buffers. Bench (2026-11-10, pid and pmsm loaded): 47 us of rt.
 * Load no other FB0 encoder comp with it.
 */
#include "yaskawa_abs_comp.h"
#include "commands.h"
#include "hal.h"
#include "math.h"
#include "defines.h"
#include "angle.h"
#include "stm32f4xx_conf.h"
#include "hw/hw.h"
#include "yaskawa_abs.h"
#include <stdio.h>

HAL_COMP(yaskawa_abs);

HAL_PIN(enable);     // 0: stop polling (default 1)
HAL_PIN(max_err);    // bad polls in a row before error (default 4)

HAL_PIN(pos);        // rad, -pi..pi, position in the turn
HAL_PIN(abs_pos);    // the same
HAL_PIN(mpos);       // rad, turns * 2pi + position
HAL_PIN(turns);      // signed 16 bits
HAL_PIN(raw_pos);    // 0 .. 2^20-1
HAL_PIN(state);      // 0 none / error, 3 good
HAL_PIN(error);

HAL_PIN(ok);
HAL_PIN(crc_er);
HAL_PIN(frame_er);   // no sync, not Manchester, no flag, too short
HAL_PIN(noreply);
HAL_PIN(edges);      // edges in the last capture
HAL_PIN(res);        // the last poll: 0 ok, 1 no reply, < 0 a YAB_ error

HAL_PIN(us_col);     // time: all of the rt function
HAL_PIN(us_dec);     // time: of that yab_decode
HAL_PIN(us_tx);      // time: starting the request (the interrupt turns the line around)
HAL_PIN(dump);       // rising edge: print the next capture's edge times, result and bytes

#define YAB_RXN 600
#define YAB_TXN 128
#define ZPIN_N FB0_Z_PIN_SOURCE

// two capture buffers: rt decodes one while the other takes the next reply
static volatile uint16_t rxbuf[2][YAB_RXN + 2];  // + the decoder's two sentinels
static volatile uint32_t txbuf[YAB_TXN];

struct yaskawa_abs_ctx_t {
  yab_t d;
  int txn;
  volatile int sel;      // the buffer the next capture goes to
  volatile int started;  // the turnaround interrupt started a capture
  volatile int dump_want, dump_ready;
  int dres, dn;
  uint16_t dedges[YAB_RXN];
  uint8_t dbytes[YAB_BYTES];
};

static struct yaskawa_abs_ctx_t *g_ctx;

static inline void zpin_output(void) {
  FB0_Z_PORT->MODER &= ~(3u << (2 * ZPIN_N));
  FB0_Z_PORT->MODER |= (1u << (2 * ZPIN_N));
}

static inline void zpin_af(void) {
  FB0_Z_PORT->MODER &= ~(3u << (2 * ZPIN_N));
  FB0_Z_PORT->MODER |= (2u << (2 * ZPIN_N));
}

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct yaskawa_abs_ctx_t *ctx      = (struct yaskawa_abs_ctx_t *)ctx_ptr;
  struct yaskawa_abs_pin_ctx_t *pins = (struct yaskawa_abs_pin_ctx_t *)pin_ptr;
  PIN(enable)  = 1;
  PIN(max_err) = 4;
  PIN(error)   = 1;
  yab_init(&ctx->d, 4);
  ctx->started = 0;
  ctx->sel     = 0;
}

static void hw_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct yaskawa_abs_ctx_t *ctx = (struct yaskawa_abs_ctx_t *)ctx_ptr;
  GPIO_InitTypeDef GPIO_InitStruct;

  GPIO_InitStruct.GPIO_Pin   = FB0_Z_TXEN_PIN;  // TX enable, off
  GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_OUT;
  GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStruct.GPIO_PuPd  = GPIO_PuPd_NOPULL;
  GPIO_Init(FB0_Z_TXEN_PORT, &GPIO_InitStruct);
  GPIO_ResetBits(FB0_Z_TXEN_PORT, FB0_Z_TXEN_PIN);

  GPIO_InitStruct.GPIO_Pin  = FB0_Z_PIN;  // AF (TIM4 CH3 capture), output while sending
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
  GPIO_Init(FB0_Z_PORT, &GPIO_InitStruct);
  GPIO_PinAFConfig(FB0_Z_PORT, FB0_Z_PIN_SOURCE, FB0_ENC_TIM_AF);

  RCC_APB1PeriphClockCmd(FB0_ENC_TIM_RCC, ENABLE);  // RX: 84 MHz, both edges
  FB0_ENC_TIM->CR1   = 0;
  FB0_ENC_TIM->SMCR  = 0;
  FB0_ENC_TIM->PSC   = 0;
  FB0_ENC_TIM->ARR   = 65535;
  FB0_ENC_TIM->CCMR2 = TIM_CCMR2_CC3S_0;
  FB0_ENC_TIM->CCER  = 0;
  FB0_ENC_TIM->DIER  = TIM_DIER_CC3DE;

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8, ENABLE);  // TX: a chip every 168 / 21 MHz = 125 ns
  TIM8->CR1  = 0;
  TIM8->PSC  = 0;
  TIM8->ARR  = 20;
  TIM8->DIER = TIM_DIER_UDE;

  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1 | RCC_AHB1Periph_DMA2, ENABLE);

  uint8_t chips[YAB_TXN];
  int n = yab_request_chips(chips, YAB_TXN);
  for(int i = 0; i < n; i++) {
    txbuf[i] = chips[i] ? FB0_Z_PIN : ((uint32_t)FB0_Z_PIN << 16);  // BSRR: set / reset
  }
  ctx->txn = n;

  g_ctx = ctx;
  NVIC_InitTypeDef nv;  // the turnaround, when the request is out
  nv.NVIC_IRQChannel                   = DMA2_Stream1_IRQn;
  nv.NVIC_IRQChannelPreemptionPriority = 1;  // below frt (0), above rt (2)
  nv.NVIC_IRQChannelSubPriority        = 0;
  nv.NVIC_IRQChannelCmd                = ENABLE;
  NVIC_Init(&nv);
}

// the request out by DMA (~14 us); rt decodes meanwhile
static void transmit(struct yaskawa_abs_ctx_t *ctx) {
  FB0_Z_PORT->BSRRH = FB0_Z_PIN;  // low, as the request starts (chip 0)
  zpin_output();
  FB0_Z_TXEN_PORT->BSRRL = FB0_Z_TXEN_PIN;

  DMA2_Stream1->CR &= ~DMA_SxCR_EN;
  while(DMA2_Stream1->CR & DMA_SxCR_EN) {
  }
  DMA_ClearFlag(DMA2_Stream1, DMA_FLAG_TCIF1 | DMA_FLAG_HTIF1 | DMA_FLAG_TEIF1 | DMA_FLAG_DMEIF1 | DMA_FLAG_FEIF1);
  DMA2_Stream1->CR   = DMA_Channel_7 | DMA_DIR_MemoryToPeripheral | DMA_MemoryInc_Enable |
                     DMA_PeripheralDataSize_Word | DMA_MemoryDataSize_Word | DMA_Priority_VeryHigh | DMA_SxCR_TCIE;
  DMA2_Stream1->PAR  = (uint32_t)&FB0_Z_PORT->BSRRL;
  DMA2_Stream1->M0AR = (uint32_t)txbuf;
  DMA2_Stream1->NDTR = ctx->txn;
  DMA2_Stream1->FCR  = 0;
  TIM8->CR1 = 0;
  TIM8->CNT = 0;
  TIM8->SR  = 0;
  DMA2_Stream1->CR |= DMA_SxCR_EN;
  TIM8->CR1 = TIM_CR1_CEN;
}

// the request is out: the line turned around, the capture started
void DMA2_Stream1_IRQHandler(void) {
  DMA2->LIFCR = DMA_LIFCR_CTCIF1 | DMA_LIFCR_CHTIF1 | DMA_LIFCR_CTEIF1 | DMA_LIFCR_CDMEIF1 | DMA_LIFCR_CFEIF1;
  DMA2_Stream1->CR &= ~DMA_SxCR_EN;
  TIM8->SR = 0;  // the last chip its full time
  for(int i = 0; i < 100 && !(TIM8->SR & TIM_SR_UIF); i++) {
  }
  TIM8->CR1 = 0;

  FB0_Z_TXEN_PORT->BSRRH = FB0_Z_TXEN_PIN;
  zpin_af();

  struct yaskawa_abs_ctx_t *ctx = g_ctx;
  DMA1_Stream7->CR &= ~DMA_SxCR_EN;
  while(DMA1_Stream7->CR & DMA_SxCR_EN) {
  }
  DMA1->HIFCR        = DMA_HIFCR_CTCIF7 | DMA_HIFCR_CHTIF7 | DMA_HIFCR_CTEIF7 | DMA_HIFCR_CDMEIF7 | DMA_HIFCR_CFEIF7;
  DMA1_Stream7->CR   = DMA_Channel_2 | DMA_DIR_PeripheralToMemory | DMA_MemoryInc_Enable |
                     DMA_PeripheralDataSize_HalfWord | DMA_MemoryDataSize_HalfWord | DMA_Priority_VeryHigh;
  DMA1_Stream7->PAR  = (uint32_t)&FB0_ENC_TIM->CCR3;
  DMA1_Stream7->M0AR = (uint32_t)rxbuf[ctx->sel];
  DMA1_Stream7->NDTR = YAB_RXN;
  DMA1_Stream7->FCR  = 0;
  DMA1_Stream7->CR |= DMA_SxCR_EN;

  FB0_ENC_TIM->CNT   = 0;
  FB0_ENC_TIM->CCMR2 = TIM_CCMR2_CC3S_0;
  FB0_ENC_TIM->CCER  = TIM_CCER_CC3E | TIM_CCER_CC3P | TIM_CCER_CC3NP;
  FB0_ENC_TIM->DIER  = TIM_DIER_CC3DE;
  FB0_ENC_TIM->CR1   = TIM_CR1_CEN;
  ctx->started       = 1;
}

// SysTick counts down at HCLK
static inline float us_since(uint32_t t0) {
  uint32_t t1 = SysTick->VAL;
  uint32_t d  = t0 >= t1 ? t0 - t1 : t0 + SysTick->LOAD + 1 - t1;
  return (float)d * 1e6f / (float)hal_get_systick_freq();
}

// the reply in buffer k, n edges
static void collect(struct yaskawa_abs_ctx_t *ctx, struct yaskawa_abs_pin_ctx_t *pins, uint16_t *buf, int n) {
  PIN(edges) = n;
  int res    = YAB_NOREPLY;
  yab_frame_t f;
  uint8_t bytes[24] = {0};
  if(n >= 16) {
    uint32_t t0 = SysTick->VAL;
    int nb      = yab_decode(buf, n, 21, bytes, sizeof(bytes));
    PIN(us_dec) = us_since(t0);
    res    = nb >= YAB_BYTES ? yab_frame(bytes, nb, &f) : (nb < 0 ? nb : YAB_LEN);
  }
  ctx->d.max_err = PIN(max_err) < 1.0f ? 1 : (int)PIN(max_err);
  yab_poll(&ctx->d, res, &f);
  yab_t *d = &ctx->d;
  if(d->have) {
    float p      = (float)d->f.pos * (2.0f * (float)M_PI / 1048576.0f);  // 0 .. 2pi
    PIN(pos)     = p > (float)M_PI ? p - 2.0f * (float)M_PI : p;
    PIN(abs_pos) = PIN(pos);
    PIN(turns)   = d->f.turns;
    PIN(mpos)    = (float)d->f.turns * 2.0f * (float)M_PI + p;
    PIN(raw_pos) = d->f.pos;
  }
  PIN(state)    = d->state;
  PIN(error)    = d->error;
  PIN(ok)       = d->ok;
  PIN(crc_er)   = d->crc_er;
  PIN(frame_er) = d->frame_er;
  PIN(noreply)  = d->noreply;
  PIN(res) = res;
  if(ctx->dump_want && !ctx->dump_ready) {
    for(int i = 0; i < YAB_BYTES; i++) {
      ctx->dbytes[i] = bytes[i];
    }
    for(int i = 0; i < n; i++) {
      ctx->dedges[i] = buf[i];
    }
    ctx->dn   = n;
    ctx->dres = res;
    ctx->dump_want  = 0;
    ctx->dump_ready = 1;
  }
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct yaskawa_abs_ctx_t *ctx      = (struct yaskawa_abs_ctx_t *)ctx_ptr;
  struct yaskawa_abs_pin_ctx_t *pins = (struct yaskawa_abs_pin_ctx_t *)pin_ptr;
  uint32_t t0 = SysTick->VAL;
  // the last capture stopped (the reply long over: ~35 us of the 200)
  int had = ctx->started;
  int n   = had ? YAB_RXN - (int)DMA1_Stream7->NDTR : 0;
  int k   = ctx->sel;
  DMA1_Stream7->CR &= ~DMA_SxCR_EN;
  FB0_ENC_TIM->CR1 &= ~TIM_CR1_CEN;
  FB0_ENC_TIM->CCER = 0;
  ctx->started      = 0;
  // the next request out now, into the other buffer; decoding meanwhile
  if(PIN(enable) > 0.0f) {
    ctx->sel = k ^ 1;
    transmit(ctx);
  } else {
    FB0_Z_TXEN_PORT->BSRRH = FB0_Z_TXEN_PIN;
    zpin_af();
  }
  PIN(us_tx) = us_since(t0);
  if(had) {
    collect(ctx, pins, (uint16_t *)rxbuf[k], n);
  }
  PIN(us_col) = us_since(t0);
}

static void rt_stop(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  DMA2_Stream1->CR &= ~(DMA_SxCR_EN | DMA_SxCR_TCIE);
  TIM8->CR1 = 0;
  FB0_Z_TXEN_PORT->BSRRH = FB0_Z_TXEN_PIN;
  zpin_af();
}

static void nrt_func(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct yaskawa_abs_ctx_t *ctx      = (struct yaskawa_abs_ctx_t *)ctx_ptr;
  struct yaskawa_abs_pin_ctx_t *pins = (struct yaskawa_abs_pin_ctx_t *)pin_ptr;
  if(RISING_EDGE(PIN(dump))) {
    ctx->dump_want = 1;
  }
  if(ctx->dump_ready) {
    printf("yab res=%d n=%d edges", ctx->dres, ctx->dn);
    for(int i = 0; i < ctx->dn; i++) {
      printf(" %u", ctx->dedges[i]);
    }
    printf("\nyab bytes");
    for(int i = 0; i < YAB_BYTES; i++) {
      printf(" %02x", ctx->dbytes[i]);
    }
    printf("\n");
    ctx->dump_ready = 0;
  }
}

hal_comp_t yaskawa_abs_comp_struct = {
    .name      = "yaskawa_abs",
    .nrt       = nrt_func,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .hw_init   = hw_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = rt_stop,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct yaskawa_abs_ctx_t),
    .pin_count = sizeof(struct yaskawa_abs_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
