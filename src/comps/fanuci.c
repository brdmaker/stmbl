/*
 * fanuci: probe for FANUC 1-pair ("αi interface", 2-wire, half-duplex)
 * serial pulsecoders such as the βiA128 (A860-2020-T301), on FB0's Z pair.
 *
 * The protocol is not documented. This comp sends a configurable request
 * on the Z pair, turns the RS-485 transceiver around and records the
 * reply edge by edge, decodes the edges into bits at a configurable bit
 * rate and prints them on the console. Field offsets for position, turns
 * and commutation are pins, so a frame layout found on the bench can be
 * tried without rebuilding. See docs/fanuci.md.
 *
 * Hardware (V4 only): same as yaskawa.c. TX: TIM8 update -> DMA2 stream 1
 * channel 7 writes one GPIO BSRR word per bit to FB0_Z_PIN (PD14), TX
 * enable FB0_Z_TXEN (PD15). RX: FB0_Z_PIN in AF mode as FB0_ENC_TIM (TIM4)
 * CH3, both edges captured -> DMA1 stream 7 channel 2.
 *
 * Run it either from rt (rt_prio, 5 kHz) or from frt (frt_prio, 20 kHz /
 * frt_div). Set only one of the two.
 */
#include "fanuci_comp.h"
#include "commands.h"
#include "hal.h"
#include "math.h"
#include "defines.h"
#include "angle.h"
#include "stm32f4xx_conf.h"
#include "hw/hw.h"
#include "fanuc1p.h"
#include <stdio.h>

HAL_COMP(fanuci);

// request
HAL_PIN(bitrate);    // bit/s, default 2721088 (universal-IRC-1); try 2048000, 1024000
HAL_PIN(req);        // request bit pattern, sent LSB first (default 512 = start 0, 0x00, stop 1)
HAL_PIN(req_bits);   // number of request bits (default 10)
HAL_PIN(lead);       // idle bits sent before the request with the driver on (default 2)
HAL_PIN(tail);       // idle bits after the request before the driver turns off (default 1)
HAL_PIN(idle);       // idle level of the line, logical (default 1)
HAL_PIN(tx_invert);  // invert the transmitted levels (0/1)
HAL_PIN(frt_div);    // when run from frt: poll every frt_div frt cycles (default 2 = 10 kHz)
HAL_PIN(enable);     // 0: don't poll (line stays released)
HAL_PIN(poll_hz);    // >0: poll from TIM7 at exactly this rate (default 8000); 0: from rt / frt
HAL_PIN(first_lead_us); // driver on at idle this long before the first request (default 30)
HAL_PIN(pre_pulses); // before the requests: this many single pulses, one per poll (default 0)
HAL_PIN(pre_us);     // their width in us (default 8, like the 4-wire REQ pulse)
HAL_PIN(pre_idle);   // line level around them (default 0: positive-going pulse)
HAL_PIN(restart);    // rising edge: start again (pre pulses, first lead)

// receive
HAL_PIN(rx_idle);    // receiver level before the reply (default 1, fail-safe high)
HAL_PIN(rx_invert);  // invert the received bits
HAL_PIN(rx_skip);    // drop this many edges at the start of a capture (turnaround glitches)
HAL_PIN(min_edges);  // a capture with fewer edges counts as no reply (default 4)

// fields, in received bits (0 = the first bit after the skipped edges)
HAL_PIN(lsb_first);  // default 1
HAL_PIN(pos_off);
HAL_PIN(pos_len);    // 0: off
HAL_PIN(turns_off);
HAL_PIN(turns_len);
HAL_PIN(com_off);
HAL_PIN(com_len);

// outputs
HAL_PIN(pos);        // rad, 0..2pi, from pos field
HAL_PIN(raw_pos);
HAL_PIN(turns);
HAL_PIN(com_raw);
HAL_PIN(com_pos);    // rad, 0..2pi, from com field
HAL_PIN(edges);      // edges in the last capture
HAL_PIN(nbits);      // decoded bits (up to the last edge)
HAL_PIN(latency_us); // turnaround to the first received edge
HAL_PIN(reply);      // count of captures with >= min_edges
HAL_PIN(noreply);    // count of captures without
HAL_PIN(error);      // 1: last poll got no reply
HAL_PIN(state);      // 0: no reply, 3: reply
HAL_PIN(polls);      // polls done

// console
HAL_PIN(dump);       // rising edge: print one capture
HAL_PIN(autodump);   // >0: print a capture every autodump nrt calls
HAL_PIN(dump_edges); // 1: also print the edge times (timer ticks)

#define F1P_RXN 256
#define F1P_TXN (F1P_MAX_BITS + 8)

static volatile uint16_t rxbuf[F1P_RXN];
static volatile uint32_t txbuf[F1P_TXN];

struct fanuci_ctx_t {
  int txn;           // words in txbuf
  uint32_t cfg[7];   // request config the txbuf was built for
  uint16_t arr;      // TIM8 reload for the bit rate
  int frt_count;
  int started;
  int seq_init;      // 0: run the start sequence (pre pulses, first lead) next
  int seq_left;      // pre pulses still to send
  int first;         // next request is the first one
  float tim7_hz;     // rate TIM7 is set to (0: off)
  // snapshot for the console
  volatile int snap_want;
  volatile int snap_ready;
  uint8_t snap_bits[F1P_MAX_BITS];
  int snap_nbits;
  uint16_t snap_t[F1P_RXN];
  int snap_edges;
  float snap_lat;
  int nrt_count;
};

static struct fanuci_ctx_t *isr_ctx;
static struct fanuci_pin_ctx_t *isr_pins;
static void poll(struct fanuci_ctx_t *ctx, struct fanuci_pin_ctx_t *pins);

#define TIM8_CLK 168000000.0f
#define TIM4_CLK 84000000.0f
#define ZPIN_N FB0_Z_PIN_SOURCE

static inline void zpin_output(void) {
  FB0_Z_PORT->MODER &= ~(3u << (2 * ZPIN_N));
  FB0_Z_PORT->MODER |= (1u << (2 * ZPIN_N));
}

static inline void zpin_af(void) {
  FB0_Z_PORT->MODER &= ~(3u << (2 * ZPIN_N));
  FB0_Z_PORT->MODER |= (2u << (2 * ZPIN_N));
}

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct fanuci_ctx_t *ctx      = (struct fanuci_ctx_t *)ctx_ptr;
  struct fanuci_pin_ctx_t *pins = (struct fanuci_pin_ctx_t *)pin_ptr;
  PIN(bitrate)   = 2721088;
  PIN(req)       = 512;
  PIN(req_bits)  = 10;
  PIN(lead)      = 2;
  PIN(tail)      = 1;
  PIN(idle)      = 1;
  PIN(frt_div)   = 2;
  PIN(enable)    = 1;
  PIN(rx_idle)   = 1;
  PIN(min_edges) = 4;
  PIN(lsb_first) = 1;
  // universal-IRC-1's A64i layout as a first guess (bits after its start bit)
  PIN(pos_off)   = 21;
  PIN(pos_len)   = 16;
  PIN(turns_off) = 40;
  PIN(turns_len) = 16;
  PIN(com_off)   = 59;
  PIN(com_len)   = 8;
  PIN(poll_hz)       = 8000;
  PIN(first_lead_us) = 30;
  PIN(pre_pulses)    = 0;
  PIN(pre_us)        = 8;
  PIN(pre_idle)      = 0;
  ctx->txn       = 0;
  ctx->started   = 0;
  ctx->seq_init  = 0;
  ctx->tim7_hz   = 0;
  isr_ctx        = ctx;
  isr_pins       = pins;
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
  FB0_ENC_TIM->PSC   = 0;
  FB0_ENC_TIM->ARR   = 65535;
  FB0_ENC_TIM->CCMR2 = TIM_CCMR2_CC3S_0;  // CC3 input on TI3, no filter
  FB0_ENC_TIM->CCER  = 0;
  FB0_ENC_TIM->DIER  = TIM_DIER_CC3DE;

  // TX bit clock
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8, ENABLE);
  TIM8->CR1  = 0;
  TIM8->PSC  = 0;
  TIM8->ARR  = 61;
  TIM8->DIER = TIM_DIER_UDE;

  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1 | RCC_AHB1Periph_DMA2, ENABLE);

  // poll timer, off until rt starts it (84 MHz, update interrupt)
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM7, ENABLE);
  TIM7->CR1  = 0;
  TIM7->PSC  = 0;
  TIM7->ARR  = 10499;
  TIM7->DIER = TIM_DIER_UIE;
  TIM7->SR   = 0;
  NVIC_InitTypeDef nv;
  nv.NVIC_IRQChannel                   = TIM7_IRQn;
  nv.NVIC_IRQChannelPreemptionPriority = 1;  // below frt (0), above rt (2)
  nv.NVIC_IRQChannelSubPriority        = 0;
  nv.NVIC_IRQChannelCmd                = ENABLE;
  NVIC_Init(&nv);
}

void TIM7_IRQHandler(void) {
  TIM7->SR = 0;
  if(isr_ctx) {
    poll(isr_ctx, isr_pins);
  }
}

// busy wait on the RX timer (84 MHz, max ~780 us); it is set up again for the capture afterwards
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

static inline void zpin_level(int l) {
  if(l) {
    FB0_Z_PORT->BSRRL = FB0_Z_PIN;
  } else {
    FB0_Z_PORT->BSRRH = FB0_Z_PIN;
  }
}

// (re)build txbuf when the request pins change
static void build_tx(struct fanuci_ctx_t *ctx, struct fanuci_pin_ctx_t *pins) {
  uint32_t cfg[7] = {(uint32_t)PIN(req), (uint32_t)PIN(req_bits), (uint32_t)PIN(lead), (uint32_t)PIN(tail),
                     PIN(idle) > 0.0f, PIN(tx_invert) > 0.0f, (uint32_t)PIN(bitrate)};
  int same = ctx->txn > 0;
  for(int i = 0; i < 7; i++) {
    same = same && cfg[i] == ctx->cfg[i];
  }
  if(same) {
    return;
  }
  uint8_t lv[F1P_TXN];
  int n = f1p_build_levels(lv, F1P_TXN, cfg[0], cfg[1], cfg[2], cfg[3], cfg[4]);
  for(int i = 0; i < n; i++) {
    int l   = lv[i] ^ cfg[5];
    txbuf[i] = l ? FB0_Z_PIN : ((uint32_t)FB0_Z_PIN << 16);  // BSRR: set / reset
  }
  float br = cfg[6] > 1000 ? (float)cfg[6] : 2721088.0f;
  int arr  = (int)(TIM8_CLK / br + 0.5f) - 1;
  ctx->arr = arr < 10 ? 10 : arr;
  ctx->txn = n;
  for(int i = 0; i < 7; i++) {
    ctx->cfg[i] = cfg[i];
  }
}

// the capture of the previous poll
static void collect(struct fanuci_ctx_t *ctx, struct fanuci_pin_ctx_t *pins) {
  int count = F1P_RXN - FB0_TIM_Z_DMA->NDTR;
  DMA_Cmd(FB0_TIM_Z_DMA, DISABLE);
  FB0_ENC_TIM->CR1 &= ~TIM_CR1_CEN;
  FB0_ENC_TIM->CCER = 0;
  if(!ctx->started) {
    return;
  }
  int skip = (int)PIN(rx_skip);
  if(skip < 0) {
    skip = 0;
  }
  if(skip > count) {
    skip = count;
  }
  const uint16_t *t = (const uint16_t *)&rxbuf[skip];
  int n             = count - skip;
  PIN(edges)        = n;

  if(n < (int)PIN(min_edges)) {
    PIN(noreply)++;
    PIN(error) = 1;
    PIN(state) = 0;
    PIN(nbits) = 0;
    return;
  }
  float br  = ctx->cfg[6] > 1000 ? (float)ctx->cfg[6] : 2721088.0f;
  float tpb = TIM4_CLK / br;
  uint8_t bits[F1P_MAX_BITS];
  // bits up to the last edge, plus one bit time
  int span = (int)((float)(uint16_t)(t[n - 1] - t[0]) / tpb + 1.5f);
  if(span > F1P_MAX_BITS) {
    span = F1P_MAX_BITS;
  }
  int nb = f1p_decode(t, n, tpb, PIN(rx_idle) > 0.0f, bits, span);
  if(PIN(rx_invert) > 0.0f) {
    for(int i = 0; i < nb; i++) {
      bits[i] ^= 1;
    }
  }
  PIN(nbits)      = nb;
  PIN(latency_us) = (float)t[0] / (TIM4_CLK / 1e6f);
  PIN(reply)++;
  PIN(error) = 0;
  PIN(state) = 3;

  int lsb = PIN(lsb_first) > 0.0f;
  if(PIN(pos_len) > 0.0f) {
    int len      = (int)PIN(pos_len);
    uint32_t v   = f1p_field(bits, nb, (int)PIN(pos_off), len, lsb);
    PIN(raw_pos) = v;
    PIN(pos)     = (float)v / (float)(1ull << len) * 2.0f * M_PI;
  }
  if(PIN(turns_len) > 0.0f) {
    PIN(turns) = f1p_field(bits, nb, (int)PIN(turns_off), (int)PIN(turns_len), lsb);
  }
  if(PIN(com_len) > 0.0f) {
    int len      = (int)PIN(com_len);
    uint32_t v   = f1p_field(bits, nb, (int)PIN(com_off), len, lsb);
    PIN(com_raw) = v;
    PIN(com_pos) = (float)v / (float)(1ull << len) * 2.0f * M_PI;
  }

  if(ctx->snap_want && !ctx->snap_ready) {
    for(int i = 0; i < nb; i++) {
      ctx->snap_bits[i] = bits[i];
    }
    ctx->snap_nbits = nb;
    for(int i = 0; i < n; i++) {
      ctx->snap_t[i] = t[i];
    }
    ctx->snap_edges = n;
    ctx->snap_lat   = PIN(latency_us);
    ctx->snap_want  = 0;
    ctx->snap_ready = 1;
  }
}

static void listen(struct fanuci_ctx_t *ctx);

static void transmit_and_listen(struct fanuci_ctx_t *ctx, uint32_t extra_lead_ticks) {
  // line at idle, driver on
  int idle = ctx->cfg[4] ^ ctx->cfg[5];
  zpin_level(idle);
  zpin_output();
  FB0_Z_TXEN_PORT->BSRRL = FB0_Z_TXEN_PIN;
  if(extra_lead_ticks) {
    wait_ticks(extra_lead_ticks);  // the long first lead, interrupts still on
  }
  __disable_irq();  // keep the request and the turnaround in one piece

  // TX DMA: one BSRR word per TIM8 update
  DMA_Cmd(DMA2_Stream1, DISABLE);
  while(DMA2_Stream1->CR & DMA_SxCR_EN) {
  }
  DMA_ClearFlag(DMA2_Stream1, DMA_FLAG_TCIF1 | DMA_FLAG_HTIF1 | DMA_FLAG_TEIF1 | DMA_FLAG_DMEIF1 | DMA_FLAG_FEIF1);
  DMA2_Stream1->CR   = DMA_Channel_7 | DMA_DIR_MemoryToPeripheral | DMA_MemoryInc_Enable |
                     DMA_PeripheralDataSize_Word | DMA_MemoryDataSize_Word | DMA_Priority_VeryHigh;
  DMA2_Stream1->PAR  = (uint32_t)&FB0_Z_PORT->BSRRL;
  DMA2_Stream1->M0AR = (uint32_t)txbuf;
  DMA2_Stream1->NDTR = ctx->txn;
  DMA2_Stream1->FCR  = 0;

  TIM8->CR1 = 0;
  TIM8->ARR = ctx->arr;
  TIM8->CNT = 0;
  TIM8->SR  = 0;
  DMA_Cmd(DMA2_Stream1, ENABLE);
  TIM8->CR1 = TIM_CR1_CEN;

  // wait for the last word (the tail idle bits keep the last data bit whole)
  for(int i = 0; i < 20000 && !(DMA2->LISR & DMA_FLAG_TCIF1); i++) {
  }
  // let the last written bit run its full time
  TIM8->SR = 0;
  for(int i = 0; i < 2000 && !(TIM8->SR & TIM_SR_UIF); i++) {
  }
  TIM8->CR1 = 0;

  listen(ctx);
  __enable_irq();
}

// one old-style pulse (4-wire REQ): driver on at pre_idle, !pre_idle for pre_us, back, then listen
static void pre_pulse(struct fanuci_ctx_t *ctx, struct fanuci_pin_ctx_t *pins) {
  int idle    = (PIN(pre_idle) > 0.0f) ^ (PIN(tx_invert) > 0.0f);
  float us    = PIN(pre_us) > 0.1f ? PIN(pre_us) : 8.0f;
  zpin_level(idle);
  zpin_output();
  FB0_Z_TXEN_PORT->BSRRL = FB0_Z_TXEN_PIN;
  wait_ticks(84 * 2);  // 2 us settle
  __disable_irq();
  zpin_level(!idle);
  wait_ticks((uint32_t)(us * 84.0f));
  zpin_level(idle);
  wait_ticks(84);
  listen(ctx);
  __enable_irq();
}

// turnaround: driver off, pin back to the capture input, start the capture
static void listen(struct fanuci_ctx_t *ctx) {
  FB0_Z_TXEN_PORT->BSRRH = FB0_Z_TXEN_PIN;
  zpin_af();

  // RX DMA: timer value at each edge
  DMA_Cmd(FB0_TIM_Z_DMA, DISABLE);
  while(FB0_TIM_Z_DMA->CR & DMA_SxCR_EN) {
  }
  DMA_ClearFlag(FB0_TIM_Z_DMA, FB0_TIM_Z_DMA_TCIF | DMA_FLAG_HTIF7 | DMA_FLAG_TEIF7 | DMA_FLAG_DMEIF7 | DMA_FLAG_FEIF7);
  FB0_TIM_Z_DMA->CR   = FB0_TIM_Z_DMA_CHAN | DMA_DIR_PeripheralToMemory | DMA_MemoryInc_Enable |
                      DMA_PeripheralDataSize_HalfWord | DMA_MemoryDataSize_HalfWord | DMA_Priority_VeryHigh;
  FB0_TIM_Z_DMA->PAR  = (uint32_t)&FB0_ENC_TIM->CCR3;
  FB0_TIM_Z_DMA->M0AR = (uint32_t)rxbuf;
  FB0_TIM_Z_DMA->NDTR = F1P_RXN;
  FB0_TIM_Z_DMA->FCR  = 0;
  DMA_Cmd(FB0_TIM_Z_DMA, ENABLE);

  FB0_ENC_TIM->CNT  = 0;
  FB0_ENC_TIM->CCMR2 = TIM_CCMR2_CC3S_0;
  FB0_ENC_TIM->CCER = TIM_CCER_CC3E | TIM_CCER_CC3P | TIM_CCER_CC3NP;  // both edges
  FB0_ENC_TIM->DIER = TIM_DIER_CC3DE;
  FB0_ENC_TIM->CR1  = TIM_CR1_CEN;
  ctx->started      = 1;
}

static void poll(struct fanuci_ctx_t *ctx, struct fanuci_pin_ctx_t *pins) {
  collect(ctx, pins);
  if(PIN(enable) <= 0.0f) {
    FB0_Z_TXEN_PORT->BSRRH = FB0_Z_TXEN_PIN;
    zpin_af();
    ctx->started  = 0;
    ctx->seq_init = 0;
    return;
  }
  PIN(polls)++;
  if(!ctx->seq_init) {
    ctx->seq_left = (int)PIN(pre_pulses);
    ctx->first    = 1;
    ctx->seq_init = 1;
  }
  if(ctx->seq_left > 0) {
    ctx->seq_left--;
    pre_pulse(ctx, pins);
    return;
  }
  build_tx(ctx, pins);
  uint32_t lead = 0;
  if(ctx->first && PIN(first_lead_us) > 0.0f) {
    lead = (uint32_t)(PIN(first_lead_us) * 84.0f);
  }
  ctx->first = 0;
  transmit_and_listen(ctx, lead);
}

static void tim7_set(struct fanuci_ctx_t *ctx, float hz) {
  if(hz == ctx->tim7_hz) {
    return;
  }
  TIM7->CR1 = 0;
  if(hz > 0.0f) {
    if(hz < 1300.0f) {
      hz = 1300.0f;
    }
    if(hz > 20000.0f) {
      hz = 20000.0f;
    }
    TIM7->ARR = (uint32_t)(84e6f / hz + 0.5f) - 1;
    TIM7->CNT = 0;
    TIM7->SR  = 0;
    TIM7->CR1 = TIM_CR1_CEN;
  }
  ctx->tim7_hz = hz > 0.0f ? hz : 0.0f;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct fanuci_ctx_t *ctx      = (struct fanuci_ctx_t *)ctx_ptr;
  struct fanuci_pin_ctx_t *pins = (struct fanuci_pin_ctx_t *)pin_ptr;
  isr_ctx                       = ctx;
  isr_pins                      = pins;
  if(RISING_EDGE(PIN(restart))) {
    ctx->seq_init = 0;
  }
  if(PIN(poll_hz) > 0.0f) {
    tim7_set(ctx, PIN(poll_hz));  // TIM7 polls
  } else {
    tim7_set(ctx, 0.0f);
    poll(ctx, pins);
  }
}

static void frt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct fanuci_ctx_t *ctx      = (struct fanuci_ctx_t *)ctx_ptr;
  struct fanuci_pin_ctx_t *pins = (struct fanuci_pin_ctx_t *)pin_ptr;
  if(PIN(poll_hz) > 0.0f) {
    return;  // TIM7 polls
  }
  int div = (int)PIN(frt_div);
  if(div < 1) {
    div = 1;
  }
  if(++ctx->frt_count >= div) {
    ctx->frt_count = 0;
    poll(ctx, pins);
  }
}

static void rt_stop(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct fanuci_ctx_t *ctx = (struct fanuci_ctx_t *)ctx_ptr;
  TIM7->CR1                = 0;
  ctx->tim7_hz             = 0;
  ctx->seq_init            = 0;
  FB0_Z_TXEN_PORT->BSRRH   = FB0_Z_TXEN_PIN;
}

static void nrt_func(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct fanuci_ctx_t *ctx      = (struct fanuci_ctx_t *)ctx_ptr;
  struct fanuci_pin_ctx_t *pins = (struct fanuci_pin_ctx_t *)pin_ptr;
  int every                      = (int)PIN(autodump);
  if(RISING_EDGE(PIN(dump)) || (every > 0 && ++ctx->nrt_count >= every)) {
    ctx->nrt_count = 0;
    ctx->snap_want = 1;
  }
  if(ctx->snap_ready) {
    char s[F1P_MAX_BITS + F1P_MAX_BITS / 8 + 2];
    f1p_format(s, sizeof(s), ctx->snap_bits, ctx->snap_nbits, 8);
    printf("f1p e=%d lat=%dns n=%d %s\n", ctx->snap_edges, (int)(ctx->snap_lat * 1000.0f), ctx->snap_nbits, s);
    if(PIN(dump_edges) > 0.0f) {
      printf("f1p t=");
      for(int i = 0; i < ctx->snap_edges; i++) {
        printf("%d%s", ctx->snap_t[i], i + 1 < ctx->snap_edges ? "," : "\n");
      }
    }
    ctx->snap_ready = 0;
  }
}

hal_comp_t fanuci_comp_struct = {
    .name      = "fanuci",
    .nrt       = nrt_func,
    .rt        = rt_func,
    .frt       = frt_func,
    .nrt_init  = nrt_init,
    .hw_init   = hw_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = rt_stop,
    .frt_stop  = rt_stop,
    .ctx_size  = sizeof(struct fanuci_ctx_t),
    .pin_count = sizeof(struct fanuci_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
