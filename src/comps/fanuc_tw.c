/*
 * fanuc_tw: feedback driver for FANUC 2-wire (1-pair, "αi interface") serial
 * pulsecoders, the βiA128 (A860-2020-T301) of the βiS motors, on FB0's Z
 * pair. See docs/fanuc_tw.md. The frame was worked out on the bench with the
 * fanuci probe (brdmot probe-log.md); the decoding and the state logic are
 * shared/fanuc1p.c (f1p_frame) and shared/fanuc_tw.c, tested on recorded
 * frames by host/tests/test_fanuc_tw.c.
 *
 * Every poll (TIM6, 8 kHz): the request (start 0, 0x00, stop 1 at 2.72
 * Mbit/s) on the Z pair, the transceiver turned around, the reply captured
 * edge by edge; the next poll decodes it. Outputs for fb_switch: pos /
 * abs_pos (the 24-bit position in the turn), state 3 once the encoder is
 * referenced (frame bit 68 clear), 1 before (incremental: fb_switch phases),
 * 0 with error after max_err bad polls in a row.
 *
 * Debug for the bits not yet understood: the words as pins (w0..w4), the
 * bits that ever changed (chg0..chg4), a trace of status changes on the
 * console, a field by frame bit offset (fld_*: for the motor temperature
 * once found), and an alternate request code sent every alt_every polls
 * (alt_*), whose reply is kept apart from the position.
 *
 * Hardware (V4 only), as fanuci / yaskawa: TX TIM8 update -> DMA2 stream 1
 * ch 7 writing GPIO BSRR words to PD14, TX enable PD15; RX PD14 as TIM4 CH3,
 * both edges -> DMA1 stream 7 ch 2. Poll timer TIM6 (fanuci uses TIM7).
 * Load no other FB0 encoder comp with it.
 */
#include "fanuc_tw_comp.h"
#include "commands.h"
#include "hal.h"
#include "math.h"
#include "defines.h"
#include "angle.h"
#include "stm32f4xx_conf.h"
#include "hw/hw.h"
#include "fanuc1p.h"
#include "fanuc_tw.h"
#include <stdio.h>

HAL_COMP(fanuc_tw);

// link
HAL_PIN(enable);     // 0: stop polling, line released (default 1)
HAL_PIN(poll_hz);    // poll rate (default 8000)
HAL_PIN(bitrate);    // bit/s (default 2721088)
HAL_PIN(req);        // request, 10 bits LSB first (default 512: start 0, 0x00, stop 1)
HAL_PIN(max_err);    // bad polls in a row before error (default 4)
HAL_PIN(nref_bit);   // frame bit that says "not referenced" (default 68; -1: none)

// feedback
HAL_PIN(pos);        // rad, -pi..pi, position in the turn
HAL_PIN(abs_pos);    // the same (absolute in the turn)
HAL_PIN(mpos);       // rad, turns * 2pi + position
HAL_PIN(turns);      // turn count, signed 16 bit
HAL_PIN(com_pos);    // rad, the encoder's own commutation field (256 per electrical turn); not used by the template
HAL_PIN(state);      // 0 none / error, 1 not referenced (incremental), 3 referenced (absolute)
HAL_PIN(error);
HAL_PIN(nref);       // the not-referenced bit

// counters
HAL_PIN(ok);
HAL_PIN(crc_er);
HAL_PIN(sync_er);    // broken or short frames
HAL_PIN(noreply);
HAL_PIN(latency_us); // end of the request to the first edge of the reply
HAL_PIN(edges);

// debug: the raw words of the last good frame, and the bits that changed
HAL_PIN(w0);         // bits 0-7 status, 8-15 position bits 0-7
HAL_PIN(w1);         // position bits 8-23
HAL_PIN(w2);         // turns
HAL_PIN(w3);         // bits 0-7 commutation, 8-15 status (bit 10 = frame bit 68)
HAL_PIN(w4);         // unknown
HAL_PIN(chg0);
HAL_PIN(chg1);
HAL_PIN(chg2);
HAL_PIN(chg3);
HAL_PIN(chg4);
HAL_PIN(chg_reset);  // rising edge: clear chg0..chg4
HAL_PIN(trace);      // 1: print a line whenever the masked status bits change
HAL_PIN(trace_m0);   // w0 mask (default 0x00ff)
HAL_PIN(trace_m3);   // w3 mask (default 0x1f00; 0xe000, frame bits 71-73, change all the time)
HAL_PIN(trace_m4);   // w4 mask (default 0xffff)
HAL_PIN(fld_bit);    // a field by frame bit (0..99), LSB first ...
HAL_PIN(fld_len);    // ... this long (default 0: off); sync bits are skipped
HAL_PIN(fld);        // ... its value
HAL_PIN(dump);       // rising edge: print the last frame (bits and fields)

// debug: an alternate request code every alt_every polls
HAL_PIN(alt_req);    // request bits (10, LSB first); -1: off (default)
HAL_PIN(alt_every);  // polls between them (default 800 = 10/s at 8 kHz)
HAL_PIN(alt_count);  // replies to it
HAL_PIN(alt_res);    // f1p_frame's result for it: 0 ok, -1 short, -2 sync, -3 crc, 1 no reply
HAL_PIN(alt_edges);
HAL_PIN(alt_w0);
HAL_PIN(alt_w1);
HAL_PIN(alt_w2);
HAL_PIN(alt_w3);
HAL_PIN(alt_w4);
HAL_PIN(alt_dump);   // rising edge: print the last alternate reply

#define F2W_RXN 256
#define F2W_TXN 24
#define F2W_TRACE_N 8
#define TIM8_CLK 168000000.0f
#define TIM4_CLK 84000000.0f
#define ZPIN_N FB0_Z_PIN_SOURCE

extern volatile uint64_t systime;

static volatile uint16_t rxbuf[F2W_RXN];
static volatile uint32_t txbuf[F2W_TXN];
static volatile uint32_t txbuf_alt[F2W_TXN];

typedef struct {
  uint32_t ms;
  f1p_frame_t f;
} f2w_trace_t;

struct fanuc_tw_ctx_t {
  f2w_t d;
  int txn, txn_alt;
  uint32_t req, req_alt, br;  // what the tx buffers were built for
  uint16_t arr;
  float tim6_hz;
  int started, first, cur_alt;
  uint32_t polls;
  // last frame bits for dump
  uint8_t bits[F1P_MAX_BITS];
  int nbits;
  volatile int dump_want, dump_ready;
  uint8_t dbits[F1P_MAX_BITS];
  int dnbits;
  // alternate reply
  uint8_t abits[F1P_MAX_BITS];
  int anbits;
  volatile int adump_ready;
  // trace ring
  f1p_frame_t traced;
  int have_traced;
  f2w_trace_t tr[F2W_TRACE_N];
  volatile uint32_t tr_head, tr_tail, tr_lost;
};

static struct fanuc_tw_ctx_t *isr_ctx;
static struct fanuc_tw_pin_ctx_t *isr_pins;
static void poll(struct fanuc_tw_ctx_t *ctx, struct fanuc_tw_pin_ctx_t *pins);

static inline void zpin_output(void) {
  FB0_Z_PORT->MODER &= ~(3u << (2 * ZPIN_N));
  FB0_Z_PORT->MODER |= (1u << (2 * ZPIN_N));
}

static inline void zpin_af(void) {
  FB0_Z_PORT->MODER &= ~(3u << (2 * ZPIN_N));
  FB0_Z_PORT->MODER |= (2u << (2 * ZPIN_N));
}

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct fanuc_tw_ctx_t *ctx      = (struct fanuc_tw_ctx_t *)ctx_ptr;
  struct fanuc_tw_pin_ctx_t *pins = (struct fanuc_tw_pin_ctx_t *)pin_ptr;
  PIN(enable)    = 1;
  PIN(poll_hz)   = 8000;
  PIN(bitrate)   = 2721088;
  PIN(req)       = 512;
  PIN(max_err)   = 4;
  PIN(nref_bit)  = 68;
  PIN(trace_m0)  = 0x00ff;
  PIN(trace_m3)  = 0x1f00;
  PIN(trace_m4)  = 0xffff;
  PIN(alt_req)   = -1;
  PIN(alt_every) = 800;
  PIN(state)     = 0;
  PIN(error)     = 1;
  f2w_init(&ctx->d, 4, 68);
  ctx->txn     = 0;
  ctx->txn_alt = 0;
  ctx->tim6_hz = 0;
  ctx->started = 0;
  ctx->first   = 1;
  isr_ctx      = ctx;
  isr_pins     = pins;
}

static void hw_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  GPIO_InitTypeDef GPIO_InitStruct;

  // TX enable, off: the line is released
  GPIO_InitStruct.GPIO_Pin   = FB0_Z_TXEN_PIN;
  GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_OUT;
  GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStruct.GPIO_PuPd  = GPIO_PuPd_NOPULL;
  GPIO_Init(FB0_Z_TXEN_PORT, &GPIO_InitStruct);
  GPIO_ResetBits(FB0_Z_TXEN_PORT, FB0_Z_TXEN_PIN);

  // Z data pin: AF = TIM4 CH3 input, switched to output while sending
  GPIO_InitStruct.GPIO_Pin   = FB0_Z_PIN;
  GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_AF;
  GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStruct.GPIO_PuPd  = GPIO_PuPd_UP;
  GPIO_Init(FB0_Z_PORT, &GPIO_InitStruct);
  GPIO_PinAFConfig(FB0_Z_PORT, FB0_Z_PIN_SOURCE, FB0_ENC_TIM_AF);

  // RX timer: free running at 84 MHz, CH3 captures both edges
  RCC_APB1PeriphClockCmd(FB0_ENC_TIM_RCC, ENABLE);
  FB0_ENC_TIM->CR1   = 0;
  FB0_ENC_TIM->SMCR  = 0;
  FB0_ENC_TIM->PSC   = 0;
  FB0_ENC_TIM->ARR   = 65535;
  FB0_ENC_TIM->CCMR2 = TIM_CCMR2_CC3S_0;
  FB0_ENC_TIM->CCER  = 0;
  FB0_ENC_TIM->DIER  = TIM_DIER_CC3DE;

  // TX bit clock
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8, ENABLE);
  TIM8->CR1  = 0;
  TIM8->PSC  = 0;
  TIM8->ARR  = 61;
  TIM8->DIER = TIM_DIER_UDE;

  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1 | RCC_AHB1Periph_DMA2, ENABLE);

  // poll timer, started by rt (84 MHz, update interrupt)
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, ENABLE);
  TIM6->CR1  = 0;
  TIM6->PSC  = 0;
  TIM6->ARR  = 10499;
  TIM6->DIER = TIM_DIER_UIE;
  TIM6->SR   = 0;
  NVIC_InitTypeDef nv;
  nv.NVIC_IRQChannel                   = TIM6_DAC_IRQn;
  nv.NVIC_IRQChannelPreemptionPriority = 1;  // below frt (0), above rt (2)
  nv.NVIC_IRQChannelSubPriority        = 0;
  nv.NVIC_IRQChannelCmd                = ENABLE;
  NVIC_Init(&nv);
}

void TIM6_DAC_IRQHandler(void) {
  TIM6->SR = 0;
  if(isr_ctx) {
    poll(isr_ctx, isr_pins);
  }
}

// busy wait on the RX timer (84 MHz); it is set up again for the capture afterwards
static void wait_ticks(uint32_t ticks) {
  if(ticks > 65000) {
    ticks = 65000;
  }
  FB0_ENC_TIM->CR1  = 0;
  FB0_ENC_TIM->CCER = 0;
  FB0_ENC_TIM->DIER = 0;
  FB0_ENC_TIM->CNT  = 0;
  FB0_ENC_TIM->CR1  = TIM_CR1_CEN;
  while(FB0_ENC_TIM->CNT < ticks) {
  }
  FB0_ENC_TIM->CR1 = 0;
}

// request words: 2 idle bits, 10 request bits LSB first, 1 idle bit; idle 1
static int build(volatile uint32_t *buf, uint32_t req) {
  uint8_t lv[F2W_TXN];
  int n = f1p_build_levels(lv, F2W_TXN, req, 10, 2, 1, 1);
  for(int i = 0; i < n; i++) {
    buf[i] = lv[i] ? FB0_Z_PIN : ((uint32_t)FB0_Z_PIN << 16);  // BSRR: set / reset
  }
  return n;
}

static void build_tx(struct fanuc_tw_ctx_t *ctx, struct fanuc_tw_pin_ctx_t *pins) {
  uint32_t req = (uint32_t)PIN(req);
  uint32_t alt = PIN(alt_req) >= 0.0f ? (uint32_t)PIN(alt_req) : 0;
  uint32_t br  = PIN(bitrate) > 100000.0f ? (uint32_t)PIN(bitrate) : 2721088;
  if(!ctx->txn || req != ctx->req) {
    ctx->txn = build(txbuf, req);
    ctx->req = req;
  }
  if(!ctx->txn_alt || alt != ctx->req_alt) {
    ctx->txn_alt = build(txbuf_alt, alt);
    ctx->req_alt = alt;
  }
  if(br != ctx->br) {
    int arr  = (int)(TIM8_CLK / (float)br + 0.5f) - 1;
    ctx->arr = arr < 10 ? 10 : arr;
    ctx->br  = br;
  }
}

// turnaround: driver off, pin back to the capture input, start the capture
static void listen(struct fanuc_tw_ctx_t *ctx) {
  FB0_Z_TXEN_PORT->BSRRH = FB0_Z_TXEN_PIN;
  zpin_af();

  DMA_Cmd(FB0_TIM_Z_DMA, DISABLE);
  while(FB0_TIM_Z_DMA->CR & DMA_SxCR_EN) {
  }
  DMA_ClearFlag(FB0_TIM_Z_DMA, FB0_TIM_Z_DMA_TCIF | DMA_FLAG_HTIF7 | DMA_FLAG_TEIF7 | DMA_FLAG_DMEIF7 | DMA_FLAG_FEIF7);
  FB0_TIM_Z_DMA->CR   = FB0_TIM_Z_DMA_CHAN | DMA_DIR_PeripheralToMemory | DMA_MemoryInc_Enable |
                      DMA_PeripheralDataSize_HalfWord | DMA_MemoryDataSize_HalfWord | DMA_Priority_VeryHigh;
  FB0_TIM_Z_DMA->PAR  = (uint32_t)&FB0_ENC_TIM->CCR3;
  FB0_TIM_Z_DMA->M0AR = (uint32_t)rxbuf;
  FB0_TIM_Z_DMA->NDTR = F2W_RXN;
  FB0_TIM_Z_DMA->FCR  = 0;
  DMA_Cmd(FB0_TIM_Z_DMA, ENABLE);

  FB0_ENC_TIM->CNT   = 0;
  FB0_ENC_TIM->CCMR2 = TIM_CCMR2_CC3S_0;
  FB0_ENC_TIM->CCER  = TIM_CCER_CC3E | TIM_CCER_CC3P | TIM_CCER_CC3NP;  // both edges
  FB0_ENC_TIM->DIER  = TIM_DIER_CC3DE;
  FB0_ENC_TIM->CR1   = TIM_CR1_CEN;
  ctx->started       = 1;
}

static void transmit_and_listen(struct fanuc_tw_ctx_t *ctx, int alt) {
  FB0_Z_PORT->BSRRL = FB0_Z_PIN;  // idle 1
  zpin_output();
  FB0_Z_TXEN_PORT->BSRRL = FB0_Z_TXEN_PIN;
  if(ctx->first) {
    wait_ticks(84 * 30);  // driver on 30 us before the first request (it woke the encoder on the bench)
    ctx->first = 0;
  }
  __disable_irq();  // the request and the turnaround in one piece

  DMA_Cmd(DMA2_Stream1, DISABLE);
  while(DMA2_Stream1->CR & DMA_SxCR_EN) {
  }
  DMA_ClearFlag(DMA2_Stream1, DMA_FLAG_TCIF1 | DMA_FLAG_HTIF1 | DMA_FLAG_TEIF1 | DMA_FLAG_DMEIF1 | DMA_FLAG_FEIF1);
  DMA2_Stream1->CR   = DMA_Channel_7 | DMA_DIR_MemoryToPeripheral | DMA_MemoryInc_Enable |
                     DMA_PeripheralDataSize_Word | DMA_MemoryDataSize_Word | DMA_Priority_VeryHigh;
  DMA2_Stream1->PAR  = (uint32_t)&FB0_Z_PORT->BSRRL;
  DMA2_Stream1->M0AR = (uint32_t)(alt ? txbuf_alt : txbuf);
  DMA2_Stream1->NDTR = alt ? ctx->txn_alt : ctx->txn;
  DMA2_Stream1->FCR  = 0;

  TIM8->CR1 = 0;
  TIM8->ARR = ctx->arr;
  TIM8->CNT = 0;
  TIM8->SR  = 0;
  DMA_Cmd(DMA2_Stream1, ENABLE);
  TIM8->CR1 = TIM_CR1_CEN;

  for(int i = 0; i < 20000 && !(DMA2->LISR & DMA_FLAG_TCIF1); i++) {
  }
  TIM8->SR = 0;  // let the last bit run its full time
  for(int i = 0; i < 2000 && !(TIM8->SR & TIM_SR_UIF); i++) {
  }
  TIM8->CR1 = 0;

  listen(ctx);
  __enable_irq();
}

static void set_outputs(struct fanuc_tw_ctx_t *ctx, struct fanuc_tw_pin_ctx_t *pins) {
  f2w_t *d = &ctx->d;
  if(d->have) {
    float p      = (float)d->f.pos * (2.0f * M_PI / 16777216.0f);
    PIN(pos)     = mod(p);
    PIN(abs_pos) = PIN(pos);
    PIN(turns)   = d->f.turns;
    PIN(mpos)    = (float)d->f.turns * 2.0f * M_PI + p;
    PIN(com_pos) = mod((float)d->f.com * (2.0f * M_PI / 256.0f));
    PIN(w0)      = d->f.w[0];
    PIN(w1)      = d->f.w[1];
    PIN(w2)      = d->f.w[2];
    PIN(w3)      = d->f.w[3];
    PIN(w4)      = d->f.w[4];
  }
  PIN(state)   = d->state;
  PIN(error)   = d->error;
  PIN(nref)    = d->nref;
  PIN(ok)      = d->ok;
  PIN(crc_er)  = d->crc_er;
  PIN(sync_er) = d->sync_er;
  PIN(noreply) = d->noreply;
  PIN(chg0)    = d->chg[0];
  PIN(chg1)    = d->chg[1];
  PIN(chg2)    = d->chg[2];
  PIN(chg3)    = d->chg[3];
  PIN(chg4)    = d->chg[4];
}

// the capture of the previous poll
static void collect(struct fanuc_tw_ctx_t *ctx, struct fanuc_tw_pin_ctx_t *pins) {
  int n = F2W_RXN - FB0_TIM_Z_DMA->NDTR;
  DMA_Cmd(FB0_TIM_Z_DMA, DISABLE);
  FB0_ENC_TIM->CR1 &= ~TIM_CR1_CEN;
  FB0_ENC_TIM->CCER = 0;
  if(!ctx->started) {
    return;
  }
  const uint16_t *t = (const uint16_t *)rxbuf;
  int alt           = ctx->cur_alt;
  int res           = F2W_NOREPLY;
  f1p_frame_t f;
  uint8_t *bits = alt ? ctx->abits : ctx->bits;
  int nb        = 0;
  if(n >= 4) {
    float tpb = TIM4_CLK / (float)ctx->br;
    int span  = (int)((float)(uint16_t)(t[n - 1] - t[0]) / tpb + 1.5f);
    if(span < F1P_FRAME_BITS) {
      span = F1P_FRAME_BITS;  // the line holds its level after the last edge
    }
    if(span > F1P_MAX_BITS) {
      span = F1P_MAX_BITS;
    }
    nb  = f1p_decode(t, n, tpb, 1, bits, span);
    res = f1p_frame(bits, nb, &f);
  }
  if(alt) {
    ctx->anbits    = nb;
    PIN(alt_res)   = res;
    PIN(alt_edges) = n;
    if(res != F2W_NOREPLY) {
      PIN(alt_count)++;
      PIN(alt_w0) = f.w[0];
      PIN(alt_w1) = f.w[1];
      PIN(alt_w2) = f.w[2];
      PIN(alt_w3) = f.w[3];
      PIN(alt_w4) = f.w[4];
    }
    return;  // the position stays from the last normal poll
  }
  ctx->nbits = nb;
  PIN(edges) = n;
  if(n >= 1) {
    PIN(latency_us) = (float)t[0] / (TIM4_CLK / 1e6f);
  }
  f2w_poll(&ctx->d, res, &f);
  if(res == F1P_OK && PIN(trace) > 0.0f) {
    if(ctx->have_traced &&
       f2w_status_differs(&ctx->traced, &f, (uint16_t)PIN(trace_m0), (uint16_t)PIN(trace_m3), (uint16_t)PIN(trace_m4))) {
      if(ctx->tr_head - ctx->tr_tail < F2W_TRACE_N) {
        f2w_trace_t *e = &ctx->tr[ctx->tr_head % F2W_TRACE_N];
        e->ms          = (uint32_t)systime;
        e->f           = f;
        ctx->tr_head++;
      } else {
        ctx->tr_lost++;
      }
    }
    ctx->traced      = f;
    ctx->have_traced = 1;
  }
  if(res == F1P_OK && PIN(fld_len) > 0.0f) {
    // fld_len bits from frame bit fld_bit on, LSB first, skipping the sync bits
    uint32_t v = 0;
    int k      = 0;
    for(int b = (int)PIN(fld_bit); b < F1P_FRAME_BITS && k < (int)PIN(fld_len) && k < 32; b++) {
      int x = f1p_frame_bit(&f, b);
      if(x < 0) {
        continue;
      }
      v |= (uint32_t)x << k++;
    }
    PIN(fld) = v;
  }
  if(ctx->dump_want && !ctx->dump_ready) {
    for(int i = 0; i < nb; i++) {
      ctx->dbits[i] = bits[i];
    }
    ctx->dnbits     = nb;
    ctx->dump_want  = 0;
    ctx->dump_ready = 1;
  }
  set_outputs(ctx, pins);
}

static void poll(struct fanuc_tw_ctx_t *ctx, struct fanuc_tw_pin_ctx_t *pins) {
  collect(ctx, pins);
  if(PIN(enable) <= 0.0f) {
    FB0_Z_TXEN_PORT->BSRRH = FB0_Z_TXEN_PIN;
    zpin_af();
    ctx->started = 0;
    ctx->first   = 1;
    return;
  }
  build_tx(ctx, pins);
  ctx->polls++;
  int every    = (int)PIN(alt_every);
  ctx->cur_alt = PIN(alt_req) >= 0.0f && every > 0 && ctx->polls % every == 0;
  transmit_and_listen(ctx, ctx->cur_alt);
}

static void tim6_set(struct fanuc_tw_ctx_t *ctx, float hz) {
  if(hz == ctx->tim6_hz) {
    return;
  }
  TIM6->CR1 = 0;
  if(hz > 0.0f) {
    if(hz < 1300.0f) {
      hz = 1300.0f;
    }
    if(hz > 12000.0f) {
      hz = 12000.0f;
    }
    TIM6->ARR = (uint32_t)(84e6f / hz + 0.5f) - 1;
    TIM6->CNT = 0;
    TIM6->SR  = 0;
    TIM6->CR1 = TIM_CR1_CEN;
  }
  ctx->tim6_hz = hz > 0.0f ? hz : 0.0f;
}

static void rt_start(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct fanuc_tw_ctx_t *ctx      = (struct fanuc_tw_ctx_t *)ctx_ptr;
  struct fanuc_tw_pin_ctx_t *pins = (struct fanuc_tw_pin_ctx_t *)pin_ptr;
  f2w_init(&ctx->d, (int)PIN(max_err), (int)PIN(nref_bit));
  ctx->first   = 1;
  ctx->started = 0;
  isr_ctx      = ctx;
  isr_pins     = pins;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct fanuc_tw_ctx_t *ctx      = (struct fanuc_tw_ctx_t *)ctx_ptr;
  struct fanuc_tw_pin_ctx_t *pins = (struct fanuc_tw_pin_ctx_t *)pin_ptr;
  ctx->d.max_err  = PIN(max_err) < 1.0f ? 1 : (int)PIN(max_err);
  ctx->d.nref_bit = (int)PIN(nref_bit);
  if(RISING_EDGE(PIN(chg_reset))) {
    f2w_chg_reset(&ctx->d);
  }
  tim6_set(ctx, PIN(poll_hz) > 0.0f ? PIN(poll_hz) : 8000.0f);
}

static void rt_stop(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct fanuc_tw_ctx_t *ctx = (struct fanuc_tw_ctx_t *)ctx_ptr;
  tim6_set(ctx, 0.0f);
  FB0_Z_TXEN_PORT->BSRRH = FB0_Z_TXEN_PIN;
}

static void print_bits(const char *tag, const uint8_t *b, int n) {
  printf("%s n=%d ", tag, n);
  for(int i = 0; i < n; i++) {
    putchar(b[i] ? '1' : '0');
    if(i % 19 == 0 || i % 19 == 16) {  // the start bit | 16 data bits | 3 sync bits
      putchar(' ');
    }
  }
  putchar('\n');
}

static void nrt_func(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct fanuc_tw_ctx_t *ctx      = (struct fanuc_tw_ctx_t *)ctx_ptr;
  struct fanuc_tw_pin_ctx_t *pins = (struct fanuc_tw_pin_ctx_t *)pin_ptr;
  if(RISING_EDGE(PIN(dump))) {
    ctx->dump_want = 1;
  }
  if(ctx->dump_ready) {
    f1p_frame_t f;
    int r = f1p_frame(ctx->dbits, ctx->dnbits, &f);
    print_bits("f2w", ctx->dbits, ctx->dnbits);
    printf("f2w res=%d pos=%lu turns=%d com=%u w0=%04x w1=%04x w2=%04x w3=%04x w4=%04x crc=%u/%u\n", r,
           (unsigned long)f.pos, f.turns, f.com, f.w[0], f.w[1], f.w[2], f.w[3], f.w[4], f.crc, f.crc_rx);
    ctx->dump_ready = 0;
  }
  if(RISING_EDGE(PIN(alt_dump))) {
    print_bits("f2w alt", ctx->abits, ctx->anbits);
  }
  while(ctx->tr_tail != ctx->tr_head) {
    f2w_trace_t *e = &ctx->tr[ctx->tr_tail % F2W_TRACE_N];
    printf("f2w trace %lu ms w0=%04x w3=%04x w4=%04x pos=%lu turns=%d\n", (unsigned long)e->ms, e->f.w[0], e->f.w[3],
           e->f.w[4], (unsigned long)e->f.pos, e->f.turns);
    ctx->tr_tail++;
  }
  if(ctx->tr_lost) {
    printf("f2w trace: %lu changes not printed\n", (unsigned long)ctx->tr_lost);
    ctx->tr_lost = 0;
  }
}

hal_comp_t fanuc_tw_comp_struct = {
    .name      = "fanuc_tw",
    .nrt       = nrt_func,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .hw_init   = hw_init,
    .rt_start  = rt_start,
    .frt_start = 0,
    .rt_stop   = rt_stop,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct fanuc_tw_ctx_t),
    .pin_count = sizeof(struct fanuc_tw_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
