#include "yaskawa_comp.h"
#include "commands.h"
#include "hal.h"
#include "math.h"
#include "defines.h"
#include "angle.h"
#include "stm32f4xx_conf.h"
#include "hw/hw.h"
#include "yaskawa_crc16.h"
#include "yaskawa_frame.h"

HAL_COMP(yaskawa);

HAL_PIN(pos);
HAL_PIN(error);
HAL_PIN(error_sum);
HAL_PIN(dump);
HAL_PIN(len);
HAL_PIN(off);

HAL_PIN(len2);
HAL_PIN(off2);
HAL_PIN(probe2);

HAL_PIN(len3);
HAL_PIN(len4);
HAL_PIN(len5);

HAL_PIN(probe3);
HAL_PIN(probe4);
HAL_PIN(probe5);

HAL_PIN(send);
HAL_PIN(crc_ok);
HAL_PIN(crc_error);

// noise diagnosis and handling (shared/yaskawa_frame.h, 2026-11-11)
HAL_PIN(max_bad);    // bad replies in a row before error (default 2; 0: every one)
HAL_PIN(bad);        // bad replies in a row now
HAL_PIN(err_short);  // counts: no reply (80 edges or fewer)
HAL_PIN(err_sync);   // no start flag
HAL_PIN(err_code);   // not Manchester: a glitch on the line
HAL_PIN(err_frame);  // no end flag / too few bits
HAL_PIN(ic_filter);  // capture input filter, TIMx_CCMR2 IC3F 0..15 (default 2: 4 samples at 84 MHz, ~48 ns)

//TODO: use context
static volatile uint32_t txbuf[128];
static int pos;
static int dfdf;
static volatile char m_data2[YASK_BITS_MAX + 1];
static yask_frame_t frame;
static yask_track_t track;
static volatile uint16_t tim_data[300];
static DMA_InitTypeDef DMA_InitStructuretx;
static DMA_InitTypeDef DMA_InitStructurerx;
//uint8_t yaskara_reply_len;

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  // struct yaskawa_ctx_t *ctx = (struct yaskawa_ctx_t *)ctx_ptr;
  struct yaskawa_pin_ctx_t *pins = (struct yaskawa_pin_ctx_t *)pin_ptr;

  PIN(len)  = 15;
  PIN(off)  = 64;
  PIN(len3) = 57;
  PIN(len4) = 58;
  PIN(len5) = 59;
  PIN(max_bad)   = 2;
  PIN(ic_filter) = 2;
}

static void hw_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  // struct yaskawa_ctx_t *ctx = (struct yaskawa_ctx_t *)ctx_ptr;
  // struct yaskawa_pin_ctx_t *pins = (struct yaskawa_pin_ctx_t *)pin_ptr;

  GPIO_InitTypeDef GPIO_InitStruct;

  //TX enable
  GPIO_InitStruct.GPIO_Pin   = FB0_Z_TXEN_PIN;
  GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_OUT;
  GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_InitStruct.GPIO_PuPd  = GPIO_PuPd_NOPULL;
  GPIO_Init(FB0_Z_TXEN_PORT, &GPIO_InitStruct);
  GPIO_ResetBits(FB0_Z_TXEN_PORT, FB0_Z_TXEN_PIN);

  //TX
  GPIO_InitStruct.GPIO_Pin   = FB0_Z_PIN;
  GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_OUT;//TODO: default to AF?
  GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_InitStruct.GPIO_PuPd  = GPIO_PuPd_NOPULL;
  GPIO_Init(FB0_Z_PORT, &GPIO_InitStruct);
  GPIO_PinAFConfig(FB0_Z_PORT, FB0_Z_PIN_SOURCE, FB0_ENC_TIM_AF);

  RCC_APB1PeriphClockCmd(FB0_ENC_TIM_RCC, ENABLE);


  //manchaster
  //0 -> 01
  //1 -> 10
  //SYNC                framing                         framing
  //0101010101010101010 01111110 111110 111110 111110 1 01111110;
  //010101... are to sync receiver
  //01111110 is hdlc start and end flag for framing
  //0 is appended after 5 ones => transmitted data is 0xffff
  //reply has the same format
  const char request[] = "0101010101010101010 01111110 111110 111110 111110 1 01111110";
  char request_m[sizeof(request) * 2];
  uint32_t tim_a = FB0_Z_PIN << 16;
  uint32_t tim_b = FB0_Z_PIN;

  //encode in manchaster
  int j = 0;
  for(int i = 0; request[i]; i++) {
    if(request[i] == '0') {
      request_m[j++] = '0';
      request_m[j++] = '1';
    } else if(request[i] == '1') {
      request_m[j++] = '1';
      request_m[j++] = '0';
    }
  }
  request_m[j] = '\0';

  //build txbuf for dma
  for(int i = 0; request_m[i]; i++) {
    if(request_m[i] == '0') {
      txbuf[pos++] = tim_a;
    } else if(request_m[i] == '1') {
      txbuf[pos++] = tim_b;
    }
  }
  txbuf[pos++] = tim_a;
  txbuf[pos++] = tim_a;

  DMA_InitStructuretx.DMA_Channel            = DMA_Channel_7;
  DMA_InitStructuretx.DMA_PeripheralBaseAddr = (uint32_t)&FB0_Z_PORT->BSRRL;  //TODO: change
  DMA_InitStructuretx.DMA_Memory0BaseAddr    = (uint32_t)&txbuf;
  DMA_InitStructuretx.DMA_DIR                = DMA_DIR_MemoryToPeripheral;
  DMA_InitStructuretx.DMA_BufferSize         = pos;
  DMA_InitStructuretx.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
  DMA_InitStructuretx.DMA_MemoryInc          = DMA_MemoryInc_Enable;
  DMA_InitStructuretx.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
  DMA_InitStructuretx.DMA_MemoryDataSize     = DMA_PeripheralDataSize_Word;
  DMA_InitStructuretx.DMA_Mode               = DMA_Mode_Normal;
  DMA_InitStructuretx.DMA_Priority           = DMA_Priority_VeryHigh;
  DMA_InitStructuretx.DMA_FIFOMode           = DMA_FIFOMode_Disable;
  DMA_InitStructuretx.DMA_FIFOThreshold      = DMA_FIFOThreshold_HalfFull;
  DMA_InitStructuretx.DMA_MemoryBurst        = DMA_MemoryBurst_Single;
  DMA_InitStructuretx.DMA_PeripheralBurst    = DMA_PeripheralBurst_Single;

  DMA_InitStructurerx.DMA_Channel            = DMA_Channel_2;
  DMA_InitStructurerx.DMA_PeripheralBaseAddr = (uint32_t)&FB0_ENC_TIM->CCR3;  //TODO: change
  DMA_InitStructurerx.DMA_Memory0BaseAddr    = (uint32_t)&tim_data;
  DMA_InitStructurerx.DMA_DIR                = DMA_DIR_PeripheralToMemory;
  DMA_InitStructurerx.DMA_BufferSize         = ARRAY_SIZE(tim_data);
  DMA_InitStructurerx.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
  DMA_InitStructurerx.DMA_MemoryInc          = DMA_MemoryInc_Enable;
  DMA_InitStructurerx.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
  DMA_InitStructurerx.DMA_MemoryDataSize     = DMA_PeripheralDataSize_HalfWord;
  DMA_InitStructurerx.DMA_Mode               = DMA_Mode_Normal;
  DMA_InitStructurerx.DMA_Priority           = DMA_Priority_VeryHigh;
  DMA_InitStructurerx.DMA_FIFOMode           = DMA_FIFOMode_Disable;
  DMA_InitStructurerx.DMA_FIFOThreshold      = DMA_FIFOThreshold_HalfFull;
  DMA_InitStructurerx.DMA_MemoryBurst        = DMA_MemoryBurst_Single;
  DMA_InitStructurerx.DMA_PeripheralBurst    = DMA_PeripheralBurst_Single;

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8, ENABLE);
  TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
  TIM_TimeBaseStructure.TIM_ClockDivision     = TIM_CKD_DIV1;
  TIM_TimeBaseStructure.TIM_CounterMode       = TIM_CounterMode_Up;
  TIM_TimeBaseStructure.TIM_Period            = 20;  // 168 / (20 + 1) = 8MHz
  TIM_TimeBaseStructure.TIM_Prescaler         = 0;
  TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
  TIM_TimeBaseInit(TIM8, &TIM_TimeBaseStructure);
  TIM_ARRPreloadConfig(TIM8, ENABLE);
  TIM_DMACmd(TIM8, TIM_DMA_Update, ENABLE);
  TIM_Cmd(TIM8, ENABLE);

  FB0_ENC_TIM->CR1 &= ~TIM_CR1_CEN;
  FB0_ENC_TIM->ARR = 65535;
  FB0_ENC_TIM->CNT = 3300;
  FB0_ENC_TIM->CR1 |= TIM_CR1_CEN;  // enable tim

  DMA_Cmd(DMA1_Stream7, DISABLE);
  DMA_DeInit(DMA1_Stream7);
  DMA_Init(DMA1_Stream7, &DMA_InitStructurerx);

  DMA_Cmd(DMA2_Stream1, DISABLE);
  DMA_DeInit(DMA2_Stream1);
  DMA_Init(DMA2_Stream1, &DMA_InitStructuretx);
  dfdf = 0;
}

// RM0090 10.3.17: a stream is stopped once EN reads back 0 (bounded)
static void dma_stop(DMA_Stream_TypeDef *s) {
  s->CR &= ~DMA_SxCR_EN;
  for(int i = 0; i < 100 && (s->CR & DMA_SxCR_EN); i++) {
  }
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  // struct yaskawa_ctx_t *ctx      = (struct yaskawa_ctx_t *)ctx_ptr;
  struct yaskawa_pin_ctx_t *pins = (struct yaskawa_pin_ctx_t *)pin_ptr;

  while(FB0_ENC_TIM->CNT < 3300) {
  }

  int count = ARRAY_SIZE(tim_data) - DMA1_Stream7->NDTR;
  DMA_Cmd(DMA1_Stream7, DISABLE);

  // decode bounded, and use a reply only with its CRC (shared/yaskawa_frame.h)
  int st = yask_decode((const uint16_t *)tim_data, count, 15, &frame);
  switch(st) {
    case YASK_OK:
      PIN(crc_ok)++;
      break;
    case YASK_CRC:
      PIN(crc_error)++;
      break;
    case YASK_SHORT:
      PIN(err_short)++;
      break;
    case YASK_NO_SYNC:
      PIN(err_sync)++;
      break;
    case YASK_CODE:
      PIN(err_code)++;
      break;
    default:
      PIN(err_frame)++;
  }
  int len    = CLAMP((int)PIN(len), 1, 31);
  int error  = 0;
  uint32_t p = yask_track(&track, st == YASK_OK, yask_bits(&frame, (int)PIN(off), len), len, MAX(PIN(max_bad), 0), &error);
  PIN(pos)   = (float)p / (float)(1u << len) * M_PI * 2.0 - M_PI;
  PIN(error) = error;
  PIN(bad)   = track.bad;
  if(st == YASK_OK) {
    PIN(probe2) = yask_bits(&frame, (int)PIN(off2), CLAMP((int)PIN(len2), 0, 31));
    PIN(probe3) = yask_bits(&frame, (int)PIN(len3), 1);
    PIN(probe4) = yask_bits(&frame, (int)PIN(len4), 1);
    PIN(probe5) = yask_bits(&frame, (int)PIN(len5), 1);
  }

  if(dfdf < 1 && frame.nbits > 0) {
    for(int i = 0; i <= frame.nbits && i < ARRAY_SIZE(m_data2); i++) {
      m_data2[i] = frame.m[i];
    }
    dfdf = 1;
  }

  // PIN(send) = send;

  FB0_Z_TXEN_PORT->BSRRL = FB0_Z_TXEN_PIN;  //TX enable
  FB0_Z_PORT->MODER &= ~GPIO_MODER_MODER14_1;
  FB0_Z_PORT->MODER |= GPIO_MODER_MODER14_0;  //set tx pin to output

  dma_stop(DMA2_Stream1);
  DMA_DeInit(DMA2_Stream1);
  DMA_Init(DMA2_Stream1, &DMA_InitStructuretx);
  DMA_ClearFlag(DMA2_Stream1, DMA_FLAG_FEIF1 | DMA_FLAG_DMEIF1 | DMA_FLAG_TEIF1 | DMA_FLAG_HTIF1 | DMA_FLAG_TCIF1);
  DMA_Cmd(DMA2_Stream1, ENABLE);  //transmit request

  TIM8->CR1 &= ~TIM_CR1_CEN;  // disable tim
  TIM8->ARR  = 20;            // 168 / 2 / (9 + 1) = 8.4MHz
  TIM8->DIER = TIM_DIER_UDE;  // cc3 dma
  // TIM8->CCMR2 = 0; // cc3 output
  // TIM8->CCR3 = 1;
  TIM8->CNT = 0;
  TIM8->CR1 |= TIM_CR1_CEN;

  dma_stop(DMA1_Stream7);
  DMA_ClearFlag(DMA1_Stream7, DMA_FLAG_FEIF7 | DMA_FLAG_DMEIF7 | DMA_FLAG_TEIF7 | DMA_FLAG_HTIF7 | DMA_FLAG_TCIF7);
  DMA_Cmd(DMA1_Stream7, ENABLE);

  FB0_ENC_TIM->CR1 &= ~TIM_CR1_CEN;
  // cc3 input ti3, digital filter IC3F: a spike shorter than the filter is
  // no edge (a half bit is ~10.5 counts, 125 ns)
  FB0_ENC_TIM->CCMR2 = TIM_CCMR2_CC3S_0 | ((CLAMP((uint32_t)PIN(ic_filter), 0, 15)) << 4);
  FB0_ENC_TIM->CCER  = TIM_CCER_CC3E | TIM_CCER_CC3P | TIM_CCER_CC3NP;  // cc3 en, rising edge, falling edge
  FB0_ENC_TIM->ARR   = 65535;
  FB0_ENC_TIM->DIER  = TIM_DIER_CC3DE;  // cc3 dma
  FB0_ENC_TIM->CNT   = 0;
  FB0_ENC_TIM->CCR3  = 0;

  for(int i = 0; i < 20000 && !(DMA2->LISR & DMA_FLAG_TCIF1); i++) {
  }  //wait for request (~16 us), bounded

  FB0_Z_TXEN_PORT->BSRRH = FB0_Z_TXEN_PIN;     //TX disable
  FB0_Z_PORT->MODER &= ~GPIO_MODER_MODER14_0;  //set tx pin to af
  FB0_Z_PORT->MODER |= GPIO_MODER_MODER14_1;

  FB0_ENC_TIM->CR1 |= TIM_CR1_CEN;  // enable tim

  if(PIN(error) > 0.0) {
    PIN(error_sum)
    ++;
  }
}

static void nrt_func(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  // struct yaskawa_ctx_t *ctx      = (struct yaskawa_ctx_t *)ctx_ptr;
  struct yaskawa_pin_ctx_t *pins = (struct yaskawa_pin_ctx_t *)pin_ptr;
  if(RISING_EDGE(PIN(dump))) {
    for(int i = 0; i < 14; i++) {
      for(int j = 0; j < 8; j++) {
        printf("%c", m_data2[i * 8 + j]);
      }
      printf("|");
    }
    printf("\n");
    dfdf = 0;
  }
}

hal_comp_t yaskawa_comp_struct = {
    .name      = "yaskawa",
    .nrt       = nrt_func,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .hw_init   = hw_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = 0,
    .pin_count = sizeof(struct yaskawa_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
