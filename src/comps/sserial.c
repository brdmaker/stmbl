#include "sserial_comp.h"
/*
* This file is part of the stmbl project.
*
* Copyright (C) 2013 Rene Hopf <renehopf@mac.com>
* Copyright (C) 2015 Ian McMahon <facetious.ian@gmail.com>
* Copyright (C) 2013 Nico Stute <crinq@crinq.de>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "commands.h"
#include "hal.h"
#include "math.h"
#include "defines.h"
#include "stm32f4xx_conf.h"
#include "hw/hw.h"
#include "sserial.h"
#include "crc8.h"
#include "sserial_q.h"
#include "sscon.h"
#include "setup.h"
#include "sserial_sync.h"
#include "sserial_port.h"
#include <string.h>

HAL_COMP(sserial);

// pins
HAL_PIN(error);
HAL_PIN(crc_error);  //counts crc errors, is never reset
HAL_PIN(connected);  //connection status TODO: not stable during startup, needs link to pd
HAL_PIN(timeout);    // 20khz / 1khz * 2 reads = 40
HAL_PIN(port);       // 0 CMD, 1 enc0, 2 enc1, 3 enc0 on USART6 (shared/sserial_port.h); default: the instance number

HAL_PIN(pos_cmd);
HAL_PIN(pos_cmd_d);
HAL_PIN(pos_fb);
HAL_PIN(vel_fb);
HAL_PIN(current);
HAL_PIN(power);  // the drive's input power [W] (pe0.p_in): 8 bit, 20 W a step, noise-shaped
HAL_PIN(scale);

HAL_PIN(clock_scale);
HAL_PIN(available);
HAL_PIN(phase);

HAL_PIN(in0);
HAL_PIN(in1);
HAL_PIN(in2);
HAL_PIN(in3);
HAL_PIN(fault);

HAL_PIN(out0);
HAL_PIN(out1);
HAL_PIN(out2);
HAL_PIN(out3);
HAL_PIN(enable);
HAL_PIN(index_clear);
HAL_PIN(index_out);
HAL_PIN(pos_advance);

// 0: the frt (20 kHz) answers packets, busy-waiting up to ~22 us for a
// process-data packet's last bytes. 1: the USART's idle-line interrupt
// answers as soon as a packet has ended (~4 us after its last byte), no
// waiting; the frt only keeps the clock lock and the link timeout.
HAL_PIN(reply_mode);
HAL_PIN(irq_count);  // packets answered from the interrupt (reply_mode 1)

#pragma pack(push, 1)
//*****************************************************************************
uint8_t sserial_slave[] = {
0x0D,0x0A,0xAB,0x01,0xC7,0x01,0x00,0x00,// 0..7
0x00,0x00,0x00,0x00,0xA0,0x20,0x10,0x80,// 8..15
0x00,0x00,0x80,0xFF,0x00,0x00,0x80,0x7F,// 16..23
0x08,0x00,0x72,0x61,0x64,0x00,0x70,0x6F,// 24..31
0x73,0x5F,0x63,0x6D,0x64,0x00,0x00,0x00,// 32..39
0x00,0x00,0x00,0x00,0xA0,0x20,0x10,0x80,// 40..47
0x00,0x00,0x80,0xFF,0x00,0x00,0x80,0x7F,// 48..55
0x26,0x00,0x72,0x61,0x64,0x00,0x76,0x65,// 56..63
0x6C,0x5F,0x63,0x6D,0x64,0x00,0x00,0x00,// 64..71
0xA0,0x0A,0x06,0x80,0x00,0x00,0x00,0x00,// 72..79
0x00,0x00,0x00,0x00,0x46,0x00,0x6E,0x6F,// 80..87
0x6E,0x65,0x00,0x63,0x6F,0x6E,0x5F,0x63,// 88..95
0x6D,0x64,0x00,0x00,0xA0,0x04,0x01,0x80,// 96..103
0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x3F,// 104..111
0x63,0x00,0x6E,0x6F,0x6E,0x65,0x00,0x6F,// 112..119
0x75,0x74,0x00,0x00,0xA0,0x01,0x07,0x80,// 120..127
0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x3F,// 128..135
0x7B,0x00,0x6E,0x6F,0x6E,0x65,0x00,0x65,// 136..143
0x6E,0x61,0x62,0x6C,0x65,0x00,0x00,0x00,// 144..151
0x00,0x00,0x00,0x00,0xA0,0x20,0x10,0x00,// 152..159
0x00,0x00,0x80,0xFF,0x00,0x00,0x80,0x7F,// 160..167
0x96,0x00,0x72,0x61,0x64,0x00,0x70,0x6F,// 168..175
0x73,0x5F,0x66,0x62,0x00,0x00,0x00,0x00,// 176..183
0x00,0x00,0x00,0x00,0xA0,0x20,0x10,0x00,// 184..191
0x00,0x00,0x80,0xFF,0x00,0x00,0x80,0x7F,// 192..199
0xB5,0x00,0x72,0x61,0x64,0x00,0x76,0x65,// 200..207
0x6C,0x5F,0x66,0x62,0x00,0x00,0x00,0x00,// 208..215
0xA0,0x08,0x03,0x00,0x00,0x00,0xF0,0xC1,// 216..223
0x00,0x00,0xF0,0x41,0xD5,0x00,0x41,0x00,// 224..231
0x63,0x75,0x72,0x72,0x65,0x6E,0x74,0x00,// 232..239
0x00,0x00,0x00,0x00,0xA0,0x08,0x03,0x00,// 240..247
0x00,0x00,0x20,0xC5,0x00,0x00,0x20,0x45,// 248..255
0xF0,0x00,0x57,0x00,0x70,0x6F,0x77,0x65,// 256..263
0x72,0x00,0x00,0x00,0xA0,0x0A,0x06,0x00,// 264..271
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,// 272..279
0x0A,0x01,0x6E,0x6F,0x6E,0x65,0x00,0x63,// 280..287
0x6F,0x6E,0x5F,0x66,0x62,0x00,0x00,0x00,// 288..295
0xA0,0x04,0x01,0x00,0x00,0x00,0xC8,0xC2,// 296..303
0x00,0x00,0xC8,0x42,0x26,0x01,0x6E,0x6F,// 304..311
0x6E,0x65,0x00,0x69,0x6E,0x00,0x00,0x00,// 312..319
0xA0,0x01,0x07,0x00,0x00,0x00,0x00,0x00,// 320..327
0x00,0x00,0x80,0x3F,0x3E,0x01,0x6E,0x6F,// 328..335
0x6E,0x65,0x00,0x66,0x61,0x75,0x6C,0x74,// 336..343
0x00,0x00,0x00,0x00,0xA0,0x01,0x07,0x40,// 344..351
0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x3F,// 352..359
0x59,0x01,0x6E,0x6F,0x6E,0x65,0x00,0x69,// 360..367
0x6E,0x64,0x65,0x78,0x5F,0x65,0x6E,0x61,// 368..375
0x62,0x6C,0x65,0x00,0x00,0x00,0x00,0x00,// 376..383
0xA0,0x20,0x10,0x80,0x00,0x00,0x80,0xFF,// 384..391
0x00,0x00,0x80,0x7F,0x7C,0x01,0x6E,0x6F,// 392..399
0x6E,0x65,0x00,0x73,0x63,0x61,0x6C,0x65,// 400..407
0x00,0xB0,0x00,0x01,0x00,0x50,0x6F,0x73,// 408..415
0x69,0x74,0x69,0x6F,0x6E,0x20,0x6D,0x6F,// 416..423
0x64,0x65,0x00,0x0C,0x00,0x2C,0x00,0x48,// 424..431
0x00,0x64,0x00,0x7C,0x00,0x9C,0x00,0xBC,// 432..439
0x00,0xD8,0x00,0xF4,0x00,0x0C,0x01,0x28,// 440..447
0x01,0x40,0x01,0x5C,0x01,0x00,0x00,0x80,// 448..455
0x01,0x99,0x01,0x00,0x00,
};

const discovery_rpc_t discovery = {
  .ptocp  = 0x01AB,
  .gtocp  = 0x01C7,
  .input  = 13,
  .output = 10,
};

typedef struct {
  float pos_cmd;
  float vel_cmd;
  uint32_t con_cmd : 10;
  uint32_t out_0 : 1;
  uint32_t out_1 : 1;
  uint32_t out_2 : 1;
  uint32_t out_3 : 1;
  uint32_t enable : 1;
  uint32_t index_enable : 1;
} sserial_out_process_data_t; //size:10 bytes
_Static_assert(sizeof(sserial_out_process_data_t) == 10, "sserial_out_process_data_t size error!");

typedef struct {
  float pos_fb;
  float vel_fb;
  int8_t current;
  int8_t power;
  uint32_t con_fb : 10;
  uint32_t in_0 : 1;
  uint32_t in_1 : 1;
  uint32_t in_2 : 1;
  uint32_t in_3 : 1;
  uint32_t fault : 1;
  uint32_t index_enable : 1;
} sserial_in_process_data_t; //size:12 bytes
_Static_assert(sizeof(sserial_in_process_data_t) == 12, "sserial_in_process_data_t size error!");
//global name:scale addr:0x17c size:32 dir:0x80
#define scale_address 380
//******************************************************************************
#pragma pack(pop)

/* The physical port of an instance (sserialN.port, shared/sserial_port.h). sserial0 is the CMD
   port as always; more instances (load sserial again) run on the encoder ports, e.g. for a card
   between Mesa cards (conf/template/sserial_router.txt, docs/sserial_router.md). All ports carry the
   same PTOC; each instance has its own buffers, parser, process data and writable table copy.

   v4.1, from the v4.0 netlist (hw/kicad/v4.0, U26 = the F405, LQFP100), RJ45 pairs:
     CMD   1/2 = PA10 (USART1 RX), PB4, PA15; driver CMD_12E PB6 (left off)
           3/6 = PA0 (UART4 TX), PB3, PA8;   driver CMD_36E PB7
     enc0  1/2 = PD12, PC7 (USART6 RX), PC11 (USART3 RX); driver PD11
           3/6 = PD13, PC8, PC10 (USART3 TX);             driver PD10
           4/5 = PD14, PC6 (USART6 TX), PC12;             driver PD15
     enc1  1/2 = PE9, PB11 (USART3 RX), PB14; driver PE10
           3/6 = PE11, PB12, PB13 (no UART TX); driver PE12
           4/5 = PE13, PB10 (USART3 TX), PB15; driver PE14
   Port 1 (enc0, USART3) uses the CMD port's pairs: the same cable to a Mesa port. Ports 2 (enc1)
   and 3 (enc0 on USART6) transmit on 4/5: a cable with the Mesa's receive pair on 4/5. Ports 1
   and 2 share USART3; for all three links take 0, 2 and 3. The other MCU pins on a used pair stay
   inputs: no other component on that encoder port (endat and encf use PC10/PC12 as SPI3, encm
   USART6). */
typedef struct {
  USART_TypeDef *rx_usart, *tx_usart;
  IRQn_Type rx_irq;  // the receiver's idle-line interrupt (reply_mode 1)
  DMA_Stream_TypeDef *rx_dma, *tx_dma;
  uint32_t rx_chan, tx_chan, tx_tcif;
  GPIO_TypeDef *rx_port, *tx_port;
  uint16_t rx_pin, tx_pin;
  uint8_t rx_src, tx_src, rx_af, tx_af;
  GPIO_TypeDef *drv_on_port;  // the transmit pair's driver: on
  uint16_t drv_on_pin;
  GPIO_TypeDef *drv_off_port[2];  // the other pairs' drivers: off (receive)
  uint16_t drv_off_pin[2];
} sserial_port_t;

static const sserial_port_t sserial_ports[] = {
    {
        // 0: CMD
        .rx_usart    = USART1,
        .tx_usart    = UART4,
        .rx_irq      = USART1_IRQn,
        .rx_dma      = DMA2_Stream5,
        .tx_dma      = DMA1_Stream4,
        .rx_chan     = DMA_Channel_4,
        .tx_chan     = DMA_Channel_4,
        .tx_tcif     = DMA_FLAG_TCIF4,
        .rx_port     = GPIOA,
        .tx_port     = GPIOA,
        .rx_pin      = GPIO_Pin_10,
        .tx_pin      = GPIO_Pin_0,
        .rx_src      = GPIO_PinSource10,
        .tx_src      = GPIO_PinSource0,
        .rx_af       = GPIO_AF_USART1,
        .tx_af       = GPIO_AF_UART4,
        .drv_on_port = CMD_B_EN_PORT,  // PB7, CMD_36E
        .drv_on_pin  = CMD_B_EN_PIN,
        // CMD_12E (PB6) as before: not driven, its pull-down keeps the driver off
    },
    {
        // 1: enc0
        .rx_usart        = USART3,
        .tx_usart        = USART3,
        .rx_irq          = USART3_IRQn,
        .rx_dma          = DMA1_Stream1,
        .tx_dma          = DMA1_Stream3,
        .rx_chan         = DMA_Channel_4,
        .tx_chan         = DMA_Channel_4,
        .tx_tcif         = DMA_FLAG_TCIF3,
        .rx_port         = GPIOC,
        .tx_port         = GPIOC,
        .rx_pin          = GPIO_Pin_11,
        .tx_pin          = GPIO_Pin_10,
        .rx_src          = GPIO_PinSource11,
        .tx_src          = GPIO_PinSource10,
        .rx_af           = GPIO_AF_USART3,
        .tx_af           = GPIO_AF_USART3,
        .drv_on_port     = FB0_B_EN_PORT,  // PD10, E36
        .drv_on_pin      = FB0_B_EN_PIN,
        .drv_off_port[0] = FB0_A_EN_PORT,  // PD11, E12: the receive pair
        .drv_off_pin[0]  = FB0_A_EN_PIN,
        .drv_off_port[1] = FB0_Z_TXEN_PORT,  // PD15, E45
        .drv_off_pin[1]  = FB0_Z_TXEN_PIN,
    },
    {
        // 2: enc1. Its 3/6 pair (PE11, PB12, PB13) has no UART TX: transmit on 4/5
        .rx_usart        = USART3,
        .tx_usart        = USART3,
        .rx_irq          = USART3_IRQn,
        .rx_dma          = DMA1_Stream1,
        .tx_dma          = DMA1_Stream3,
        .rx_chan         = DMA_Channel_4,
        .tx_chan         = DMA_Channel_4,
        .tx_tcif         = DMA_FLAG_TCIF3,
        .rx_port         = GPIOB,
        .tx_port         = GPIOB,
        .rx_pin          = GPIO_Pin_11,
        .tx_pin          = GPIO_Pin_10,
        .rx_src          = GPIO_PinSource11,
        .tx_src          = GPIO_PinSource10,
        .rx_af           = GPIO_AF_USART3,
        .tx_af           = GPIO_AF_USART3,
        .drv_on_port     = FB1_Z_TXEN_PORT,  // PE14, E45
        .drv_on_pin      = FB1_Z_TXEN_PIN,
        .drv_off_port[0] = FB1_A_TXEN_PORT,  // PE10, E12: the receive pair
        .drv_off_pin[0]  = FB1_A_TXEN_PIN,
        .drv_off_port[1] = FB1_B_TXEN_PORT,  // PE12, E36
        .drv_off_pin[1]  = FB1_B_TXEN_PIN,
    },
    {
        // 3: enc0 on USART6 (as encm uses it), TX on 4/5 as enc1: enc0 beside enc1
        .rx_usart        = USART6,
        .tx_usart        = USART6,
        .rx_irq          = USART6_IRQn,
        .rx_dma          = DMA2_Stream2,
        .tx_dma          = DMA2_Stream6,
        .rx_chan         = DMA_Channel_5,
        .tx_chan         = DMA_Channel_5,
        .tx_tcif         = DMA_FLAG_TCIF6,
        .rx_port         = GPIOC,
        .tx_port         = GPIOC,
        .rx_pin          = GPIO_Pin_7,
        .tx_pin          = GPIO_Pin_6,
        .rx_src          = GPIO_PinSource7,
        .tx_src          = GPIO_PinSource6,
        .rx_af           = GPIO_AF_USART6,
        .tx_af           = GPIO_AF_USART6,
        .drv_on_port     = FB0_Z_TXEN_PORT,  // PD15, E45
        .drv_on_pin      = FB0_Z_TXEN_PIN,
        .drv_off_port[0] = FB0_A_EN_PORT,  // PD11, E12: the receive pair
        .drv_off_pin[0]  = FB0_A_EN_PIN,
        .drv_off_port[1] = FB0_B_EN_PORT,  // PD10, E36
        .drv_off_pin[1]  = FB0_B_EN_PIN,
    },
};
_Static_assert(sizeof(sserial_ports) / sizeof(sserial_ports[0]) == SSERIAL_PORTS, "sserial_port.h and the port table differ");

struct sserial_ctx_t {
  const sserial_port_t *hw;
  uint32_t port;
  volatile uint8_t rxbuf[128];  //rx dma buffer
  volatile uint8_t txbuf[128];  //tx dma buffer
  uint16_t address;             //current address pointer
  int rxpos;                    //read pointer for rx ringbuffer
  uint32_t timeout;
  lbp_t lbp;
  unit_no_t unit;
  uint32_t max_waste_ticks;
  uint32_t block_bytes;
  sserial_out_process_data_t data_out;
  sserial_in_process_data_t data_in;
  ssq_t power_q;  // power's rounding error, carried frame to frame
  // reply_mode 1: the idle interrupt parses; it runs between the frt (priority 0) and rt (2),
  // so only one of them parses
  struct sserial_pin_ctx_t *irq_pins;  // the pins, for the interrupt
  volatile uint32_t reply_irq;         // reply_mode > 0, as the frt last saw it
  volatile uint64_t frt_ms;            // the frt's last run: the interrupt stops when hal does
  volatile uint64_t idle_ms;           // the last packet's end (systime, SysTick->VAL)
  volatile uint32_t idle_val;
  uint32_t last_bufferpos;
  uint8_t slave[sizeof(sserial_slave)];  // this port's copy: the host writes scale into it
};

extern volatile uint64_t systime;                         // ms (main.c)
static struct sserial_ctx_t *port_ctx[SSERIAL_PORTS];  // for the interrupts, set by hw_init
static const char name[] = LBPCardName;

static uint8_t crc_reuest(struct sserial_ctx_t *ctx, uint8_t len) {
  uint8_t crc = crc8_init();
  for(int i = ctx->rxpos; i < ctx->rxpos + len; i++) {
    crc = crc8_update(crc, (void *)&(ctx->rxbuf[i % sizeof(ctx->rxbuf)]), 1);
  }
  crc = crc8_finalize(crc);
  return crc == ctx->rxbuf[(ctx->rxpos + len) % sizeof(ctx->rxbuf)];
}

static uint8_t crc8(uint8_t *addr, uint8_t len) {
  uint8_t crc = crc8_init();
  crc         = crc8_update(crc, addr, len);
  return crc8_finalize(crc);
}

static void tx_start(struct sserial_ctx_t *ctx, uint32_t len) {
  DMA_Stream_TypeDef *dma = ctx->hw->tx_dma;
  DMA_SetCurrDataCounter(dma, len);
  DMA_Cmd(dma, DISABLE);
  DMA_ClearFlag(dma, ctx->hw->tx_tcif);
  DMA_Cmd(dma, ENABLE);
}

static void send(struct sserial_ctx_t *ctx, uint8_t len, uint8_t docrc) {
  ctx->timeout = 0;
  if(docrc) {
    ctx->txbuf[len] = crc8((uint8_t *)ctx->txbuf, len);
    tx_start(ctx, len + 1);
  } else {
    tx_start(ctx, len);
  }
}

//next received packet will be written to bufferpos
static uint32_t rx_bufferpos(struct sserial_ctx_t *ctx) {
  return sizeof(ctx->rxbuf) - DMA_GetCurrDataCounter(ctx->hw->rx_dma);
}

static void usart_clock(USART_TypeDef *u) {
  if(u == USART1) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
  } else if(u == USART3) {
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
  } else if(u == UART4) {
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART4, ENABLE);
  } else if(u == USART6) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART6, ENABLE);
  }
}

static void gpio_out(GPIO_TypeDef *port, uint16_t pin, int on) {
  if(!port) {
    return;
  }
  GPIO_InitTypeDef GPIO_InitStruct;
  GPIO_InitStruct.GPIO_Pin   = pin;
  GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_OUT;
  GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_InitStruct.GPIO_PuPd  = GPIO_PuPd_NOPULL;
  GPIO_Init(port, &GPIO_InitStruct);
  if(on) {
    GPIO_SetBits(port, pin);
  } else {
    GPIO_ResetBits(port, pin);
  }
}

//TODO: lbp command 0xe6 to set mode

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct sserial_ctx_t *ctx      = (struct sserial_ctx_t *)ctx_ptr;
  struct sserial_pin_ctx_t *pins = (struct sserial_pin_ctx_t *)pin_ptr;
  // sserialN on port N unless the config says otherwise (sserialN.port).
  // nrt_init runs while the instance is loaded, before comp_inst_count counts it
  PIN(port) = 0;
  for(int i = 0; i <= hal.comp_inst_count && i < HAL_MAX_COMPS; i++) {
    if(hal.comp_insts[i].pin_insts == pin_ptr) {  //not ctx: a comp without ctx shares its address
      PIN(port) = hal.comp_insts[i].instance;
    }
  }
  memcpy(ctx->slave, sserial_slave, sizeof(ctx->slave));
}

static void hw_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct sserial_ctx_t *ctx      = (struct sserial_ctx_t *)ctx_ptr;
  struct sserial_pin_ctx_t *pins = (struct sserial_pin_ctx_t *)pin_ptr;

  ctx->port = (uint32_t)CLAMP(PIN(port), 0, SSERIAL_PORTS - 1);
  PIN(port) = ctx->port;
  for(int i = 0; i < SSERIAL_PORTS; i++) {  //one instance a USART (sserial_port.h)
    if(port_ctx[i] && port_ctx[i] != ctx && sserial_port_conflict(i, ctx->port)) {
      PIN(error) = 1;
      return;
    }
  }
  ctx->hw                  = &sserial_ports[ctx->port];
  const sserial_port_t *hw = ctx->hw;

  PIN(timeout) = 100.0;  // 20khz / 1khz * 2 reads = 40

  GPIO_InitTypeDef GPIO_InitStruct;
  USART_InitTypeDef USART_InitStruct;
  DMA_InitTypeDef DMA_InitStructure;
  usart_clock(hw->rx_usart);
  usart_clock(hw->tx_usart);
  //USART TX
  GPIO_PinAFConfig(hw->tx_port, hw->tx_src, hw->tx_af);
  GPIO_InitStruct.GPIO_Pin   = hw->tx_pin;
  GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_AF;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStruct.GPIO_PuPd  = GPIO_PuPd_UP;
  GPIO_Init(hw->tx_port, &GPIO_InitStruct);

  //USART RX
  GPIO_PinAFConfig(hw->rx_port, hw->rx_src, hw->rx_af);
  GPIO_InitStruct.GPIO_Pin = hw->rx_pin;
  GPIO_Init(hw->rx_port, &GPIO_InitStruct);

  USART_InitStruct.USART_BaudRate            = 2500000;
  USART_InitStruct.USART_WordLength          = USART_WordLength_8b;
  USART_InitStruct.USART_StopBits            = USART_StopBits_1;
  USART_InitStruct.USART_Parity              = USART_Parity_No;
  USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  if(hw->rx_usart == hw->tx_usart) {
    USART_InitStruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(hw->rx_usart, &USART_InitStruct);
  } else {
    USART_InitStruct.USART_Mode = USART_Mode_Rx;
    USART_Init(hw->rx_usart, &USART_InitStruct);
    USART_InitStruct.USART_Mode = USART_Mode_Tx;
    USART_Init(hw->tx_usart, &USART_InitStruct);
  }

  USART_Cmd(hw->rx_usart, ENABLE);
  USART_Cmd(hw->tx_usart, ENABLE);

  //idle line: a packet has ended (reply_mode 1). Above rt (2), below the frt (0).
  ctx->irq_pins      = pins;
  port_ctx[ctx->port] = ctx;
  USART_ITConfig(hw->rx_usart, USART_IT_IDLE, ENABLE);
  NVIC_InitTypeDef NVIC_InitStructure;
  NVIC_InitStructure.NVIC_IRQChannel                   = hw->rx_irq;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
  NVIC_Init(&NVIC_InitStructure);

  //RX DMA
  DMA_Cmd(hw->rx_dma, DISABLE);
  DMA_DeInit(hw->rx_dma);

  DMA_InitStructure.DMA_Channel            = hw->rx_chan;
  DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t) & (hw->rx_usart->DR);
  DMA_InitStructure.DMA_Memory0BaseAddr    = (uint32_t)ctx->rxbuf;
  DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralToMemory;
  DMA_InitStructure.DMA_BufferSize         = sizeof(ctx->rxbuf);
  DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
  DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
  DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
  DMA_InitStructure.DMA_MemoryDataSize     = DMA_PeripheralDataSize_Byte;
  DMA_InitStructure.DMA_Mode               = DMA_Mode_Circular;
  DMA_InitStructure.DMA_Priority           = DMA_Priority_High;
  DMA_InitStructure.DMA_FIFOMode           = DMA_FIFOMode_Disable;
  DMA_InitStructure.DMA_FIFOThreshold      = DMA_FIFOThreshold_HalfFull;
  DMA_InitStructure.DMA_MemoryBurst        = DMA_MemoryBurst_Single;
  DMA_InitStructure.DMA_PeripheralBurst    = DMA_PeripheralBurst_Single;
  DMA_Init(hw->rx_dma, &DMA_InitStructure);

  DMA_Cmd(hw->rx_dma, ENABLE);

  USART_DMACmd(hw->rx_usart, USART_DMAReq_Rx, ENABLE);

  //TX DMA
  DMA_Cmd(hw->tx_dma, DISABLE);
  DMA_DeInit(hw->tx_dma);

  DMA_InitStructure.DMA_Channel            = hw->tx_chan;
  DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t) & (hw->tx_usart->DR);
  DMA_InitStructure.DMA_Memory0BaseAddr    = (uint32_t)ctx->txbuf;
  DMA_InitStructure.DMA_DIR                = DMA_DIR_MemoryToPeripheral;
  DMA_InitStructure.DMA_BufferSize         = sizeof(ctx->txbuf);
  DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
  DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
  DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
  DMA_InitStructure.DMA_MemoryDataSize     = DMA_PeripheralDataSize_Byte;
  DMA_InitStructure.DMA_Mode               = DMA_Mode_Normal;  //was DMA_Priority_Low: the same 0
  DMA_InitStructure.DMA_Priority           = DMA_Priority_High;
  DMA_InitStructure.DMA_FIFOMode           = DMA_FIFOMode_Disable;
  DMA_InitStructure.DMA_FIFOThreshold      = DMA_FIFOThreshold_HalfFull;
  DMA_InitStructure.DMA_MemoryBurst        = DMA_MemoryBurst_Single;
  DMA_InitStructure.DMA_PeripheralBurst    = DMA_PeripheralBurst_Single;
  DMA_Init(hw->tx_dma, &DMA_InitStructure);

  USART_DMACmd(hw->tx_usart, USART_DMAReq_Tx, ENABLE);

  //the transceivers: the transmit pair drives, the others listen
  for(int i = 0; i < 2; i++) {
    gpio_out(hw->drv_off_port[i], hw->drv_off_pin[i], 0);
  }
  gpio_out(hw->drv_on_port, hw->drv_on_pin, 1);

  //generate unit number from 96bit unique chip ID; the port in the top bits, so the two
  //links of one card differ (LinuxCNC shows it: which side is which)
  ctx->unit.unit = (U_ID[0] ^ U_ID[1] ^ U_ID[2]) ^ (ctx->port << 28);

  ctx->rxpos   = 0;
  ctx->timeout = 1000;  //make sure we start in timeout

  //bytes to wait before expected end of transmission to prevent timeouts
  ctx->block_bytes = 5;
  //calculate timeout in systicks for block_bytes
  ctx->max_waste_ticks = (1.0 / 2500000.0) * 11.0 * (float)ctx->block_bytes / (1.0f / (float)hal_get_systick_freq());

  PIN(clock_scale) = 1.0;
  PIN(phase)       = 0;
}

// one packet (or nothing, if the first is incomplete). spin: wait for a
// process-data packet's last bytes (reply_mode 0), else leave it for later.
static void parse(struct sserial_ctx_t *ctx, struct sserial_pin_ctx_t *pins, int spin) {
  //next received packet will be written to bufferpos
  uint32_t bufferpos = rx_bufferpos(ctx);
  //how many packets we have the the rx buffer for processing
  uint32_t available = (bufferpos - ctx->rxpos + sizeof(ctx->rxbuf)) % sizeof(ctx->rxbuf);

  if(available >= 1) {
    ctx->lbp.byte = ctx->rxbuf[ctx->rxpos];

    if(ctx->lbp.ct == CT_LOCAL && ctx->lbp.wr == 0) {  //local read, cmd+crc = 2b
      ctx->timeout = 0;
      if(available >= 2) {
        switch(ctx->lbp.byte) {
          case LBPCookieCMD:
            ctx->txbuf[0] = LBPCookie;
            break;
          case LBPStatusCMD:  //TODO: return status
            ctx->txbuf[0] = 0x00;
            break;
          case LBPCardName0Cmd ... LBPCardName3Cmd:
            ctx->txbuf[0] = name[ctx->lbp.byte - LBPCardName0Cmd];
            break;
          default:  //TODO: handle unknown command condition
            ctx->txbuf[0] = 0x00;
        }
        send(ctx, 1, 1);
        ctx->rxpos += 2;
      }
    } else if(ctx->lbp.ct == CT_LOCAL && ctx->lbp.wr == 1) {  //local write, cmd+data+crc = 3b
      ctx->timeout = 0;
      //0xFF and 0xFC are not followed by crc
      if(ctx->rxbuf[ctx->rxpos] == 0xFF) {
        // reset parser
        ctx->rxpos += 1;
      } else if(ctx->rxbuf[ctx->rxpos] == 0xFC) {
        // todo
        ctx->rxpos += 1;
      } else if(available >= 3) {  //writes do not expect crc in reply
        ctx->txbuf[0] = 0x00;
        send(ctx, 1, 0);
        ctx->rxpos += 3;
      }
    } else if(ctx->lbp.ct == CT_RPC) {  //RPC TODO: check for ct should not required for rpc
      ctx->timeout = 0;
      if(ctx->lbp.byte == UnitNumberRPC && available >= 2) {  //ctx->unit number, cmd+crc = 2b
        ctx->txbuf[0] = ctx->unit.byte[0];
        ctx->txbuf[1] = ctx->unit.byte[1];
        ctx->txbuf[2] = ctx->unit.byte[2];
        ctx->txbuf[3] = ctx->unit.byte[3];
        send(ctx, 4, 1);
        ctx->rxpos += 2;
      } else if(ctx->lbp.byte == DiscoveryRPC && available >= 2) {  //discovery, cmd+crc = 2b
        memcpy((void *)ctx->txbuf, ((uint8_t *)&discovery), sizeof(discovery));
        send(ctx, sizeof(discovery), 1);
        ctx->rxpos += 2;
      } else if(ctx->lbp.byte == ProcessDataRPC && available >= discovery.output + 2 - (spin ? ctx->block_bytes : 0)) {  //process data, requires cmd+output bytes+crc
        uint32_t t1         = hal_get_systick_value();
        uint32_t wait_ticks = 0;
        //wait with ctx->timeout until rest of process data is received
        while(spin && available < discovery.output + 2 && wait_ticks <= ctx->max_waste_ticks) {
          uint32_t t2 = hal_get_systick_value();
          if(t1 < t2) {
            t1 += hal_get_systick_reload();
          }
          wait_ticks = t1 - t2;
          //next received packet will be written to bufferpos
          bufferpos = rx_bufferpos(ctx);
          //how many packets we have the the rx buffer for processing
          available = (bufferpos - ctx->rxpos + sizeof(ctx->rxbuf)) % sizeof(ctx->rxbuf);
        }
        //TODO: fault handling on ctx->timeout...
        //set input pins
        ctx->data_in.pos_fb  = PIN(pos_fb) + PIN(vel_fb) * PIN(pos_advance);
        ctx->data_in.vel_fb  = PIN(vel_fb);
        ctx->data_in.current = CLAMP(PIN(current) / (30.0f / 128.0f), -127, 127);
        ctx->data_in.power   = ssq_encode(&ctx->power_q, PIN(power), 20.0f, 127);
        ctx->data_in.in_0    = (PIN(in0) > 0) ? 1 : 0;
        ctx->data_in.in_1    = (PIN(in1) > 0) ? 1 : 0;
        ctx->data_in.in_2    = (PIN(in2) > 0) ? 1 : 0;
        ctx->data_in.in_3    = (PIN(in3) > 0) ? 1 : 0;
        ctx->data_in.fault   = (PIN(fault) > 0) ? 1 : 0;

        //copy output pins from rx buffer
        for(int i = 0; i < discovery.output; i++) {
          ((uint8_t *)(&ctx->data_out))[i] = ctx->rxbuf[(ctx->rxpos + i + 1) % sizeof(ctx->rxbuf)];
        }
        int crc_ok = crc_reuest(ctx, discovery.output + 1);

        //the console (shared/sscon.h): only a valid request moves it
        if(ctx->port == 0 && crc_ok) {  //the console is on the CMD port only
          ctx->data_in.con_fb = sscon_frame(&sscon0, ctx->data_out.con_cmd);
        }

        //set bidirectional pins
        PIN(index_out)       = ctx->data_out.index_enable;
        ctx->data_in.index_enable = (PIN(index_clear) > 0) ? 0 : ctx->data_out.index_enable;

        //copy input pins to tx buffer
        ctx->txbuf[0] = 0x00;  //fault byte
        for(int i = 0; i < (discovery.input - 1); i++) {
          ctx->txbuf[i + 1] = ((uint8_t *)(&ctx->data_in))[i];
        }
        if(crc_ok) {
          //send buffer
          tx_start(ctx, discovery.input + 1);
          ctx->txbuf[discovery.input] = crc8((uint8_t *)ctx->txbuf, discovery.input);
          //send(ctx, discovery.input, 1);
          ctx->timeout = 0;
          //set output pins

          PIN(pos_cmd)   = ctx->data_out.pos_cmd;
          PIN(pos_cmd_d) = ctx->data_out.vel_cmd;
          PIN(out0)      = ctx->data_out.out_0;
          PIN(out1)      = ctx->data_out.out_1;
          PIN(out2)      = ctx->data_out.out_2;
          PIN(out3)      = ctx->data_out.out_3;
          PIN(enable)    = ctx->data_out.enable;
        } else {
          PIN(crc_error)
          ++;
          PIN(connected) = 0;
          PIN(error)     = 1;
          PIN(pos_cmd)   = 0;
          PIN(pos_cmd_d) = 0;
          PIN(out0)      = 0;
          PIN(out1)      = 0;
          PIN(out2)      = 0;
          PIN(out3)      = 0;
          PIN(enable)    = 0;
        }
        ctx->rxpos += discovery.output + 2;
      }
    } else if(ctx->lbp.ct == CT_RW && ctx->lbp.wr == 0) {  //read
      //size = 1 + 2*ctx->lbp.as  + 1
      int size = 2 * ctx->lbp.as + 2;
      ctx->timeout  = 0;
      if(available >= size) {
        if(ctx->lbp.as) {  //ctx->address included in command = cmd+addr+addr+crc
          ctx->address = ctx->rxbuf[(ctx->rxpos + 1) % sizeof(ctx->rxbuf)] + (ctx->rxbuf[(ctx->rxpos + 2) % sizeof(ctx->rxbuf)] << 8);
          ctx->rxpos += 4;
        } else {  //ctx->address not included in command = cmd+crc
          ctx->rxpos += 2;
        }
        //TODO: causes timeouts...
        //if((ctx->address + (1 << ctx->lbp.ds)) < ARRAY_SIZE(ctx->slave)) {  //check if ctx->address is valid
        memcpy((void *)ctx->txbuf, &ctx->slave[ctx->address], (1 << ctx->lbp.ds));
        send(ctx, (1 << ctx->lbp.ds), 1);
        //}
        if(ctx->lbp.ai) {  //auto increment ctx->address by datasize
          ctx->address += (1 << ctx->lbp.ds);
        }
      }
    } else if(ctx->lbp.ct == CT_RW && ctx->lbp.wr == 1) {  // ctx->lbp (addr1 addr2) data0, data1,...
      //size = 1 + 2*ai +ds +crc
      int size = 2 * ctx->lbp.as + (1 << ctx->lbp.ds) + 2;
      ctx->timeout  = 0;
      if(available >= size) {
        if(ctx->lbp.as) {  //ctx->address included in command = cmd+addr+addr+crc
          ctx->address = ctx->rxbuf[(ctx->rxpos + 1) % sizeof(ctx->rxbuf)] + (ctx->rxbuf[(ctx->rxpos + 2) % sizeof(ctx->rxbuf)] << 8);
          ctx->rxpos += 3;
        } else {  //ctx->address not included in command = cmd+crc
          ctx->rxpos += 1;
        }
        //TODO: check size
        if((ctx->address + (1 << ctx->lbp.ds)) < ARRAY_SIZE(ctx->slave)) {  //check if ctx->address is valid
          for(int i = 0; i < (1 << ctx->lbp.ds); i++) {
            ctx->slave[ctx->address + i] = ctx->rxbuf[(ctx->rxpos + i) % sizeof(ctx->rxbuf)];
          }
        }
        ctx->rxpos += (1 << ctx->lbp.ds) + 1;
        //update globals
        float tmp;
        memcpy(&tmp, &ctx->slave[scale_address], 4);
        PIN(scale) = tmp;
        if(ctx->lbp.ai) {  //auto increment ctx->address by datasize
          ctx->address += (1 << ctx->lbp.ds);
        }
      }
    } else {
      //TODO: handle unkown packet
    }
  }
  ctx->rxpos = ctx->rxpos % sizeof(ctx->rxbuf);
}

// SysTick ticks since the last packet ended, if within the last ms or so (else ~0u)
static uint32_t since_idle(struct sserial_ctx_t *ctx) {
  uint64_t ms  = systime;
  uint32_t now = SysTick->VAL, load = SysTick->LOAD + 1;
  if(ms == ctx->idle_ms) {
    return ctx->idle_val >= now ? ctx->idle_val - now : 0;
  }
  if(ms == ctx->idle_ms + 1) {
    return ctx->idle_val + load - now;
  }
  return ~0u;
}

static void link_lost(struct sserial_pin_ctx_t *pins) {
  PIN(connected) = 0;
  PIN(error)     = 1;
  PIN(pos_cmd)   = 0;
  PIN(pos_cmd_d) = 0;
  PIN(out0)      = 0;
  PIN(out1)      = 0;
  PIN(out2)      = 0;
  PIN(out3)      = 0;
  PIN(enable)    = 0;
}

static void frt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct sserial_ctx_t *ctx      = (struct sserial_ctx_t *)ctx_ptr;
  struct sserial_pin_ctx_t *pins = (struct sserial_pin_ctx_t *)pin_ptr;
  if(!ctx->hw) {  //no port (a second instance on a taken port)
    return;
  }
  ctx->irq_pins  = pins;
  ctx->reply_irq = PIN(reply_mode) > 0.0;
  ctx->frt_ms    = systime;
  uint32_t bufferpos = rx_bufferpos(ctx);
  //how many packets we have the the rx buffer for processing
  uint32_t available = (bufferpos - ctx->rxpos + sizeof(ctx->rxbuf)) % sizeof(ctx->rxbuf);

  PIN(phase) += 1.0;

  uint32_t goal    = 5;
  PIN(clock_scale) = 1.0;
  if(PIN(phase) > 3) {
    PIN(phase) = 0;
    // reply_mode 1: a packet answered within the last frt period counts as late
    int recent       = since_idle(ctx) < hal_get_systick_freq() / FRT_FREQ;
    PIN(clock_scale) = sserial_sync_scale(available, goal, ctx->reply_irq, recent);
  }

  PIN(available) = available;

  if(sserial_link_alive(ctx->reply_irq, bufferpos, ctx->last_bufferpos)) {
    ctx->timeout = 0;  // mode 1: bytes coming in; the idle interrupt parses them
  }
  if(!ctx->reply_irq) {
    parse(ctx, pins, 1);
  } else if(available > 0 && bufferpos == ctx->last_bufferpos) {
    // bytes waiting and nothing new for a whole frt period: a missed idle
    // interrupt; let it parse (it, not the frt, owns the parser in mode 1)
    NVIC_SetPendingIRQ(ctx->hw->rx_irq);
  }
  ctx->last_bufferpos = bufferpos;
  bufferpos           = rx_bufferpos(ctx);

  if(ctx->timeout > PIN(timeout)) {  //TODO: clamping
    if(ctx->port == 0 && PIN(connected) > 0) {  //the link went away: the console's other side restarts
      sscon_reset(&sscon0);
    }
    link_lost(pins);
    ctx->rxpos = bufferpos;
  } else {
    PIN(connected) = 1;
    PIN(error)     = 0;
  }
  ctx->rxpos = ctx->rxpos % sizeof(ctx->rxbuf);
  ctx->timeout++;
}

// the idle line after a packet (reply_mode 1): answer it now. u: the interrupt's USART; the
// port on it is the one started (ports sharing a USART can't both run)
static void idle_irq(USART_TypeDef *u) {
  struct sserial_ctx_t *ctx = 0;
  for(int i = 0; i < SSERIAL_PORTS; i++) {
    if(port_ctx[i] && sserial_ports[i].rx_usart == u) {
      ctx = port_ctx[i];
    }
  }
  if(u->SR & USART_SR_IDLE) {
    (void)u->DR;  // SR then DR clears IDLE; the DMA has taken the data
    if(ctx) {
      ctx->idle_ms  = systime;
      ctx->idle_val = SysTick->VAL;
    }
  }
  if(!ctx) {
    return;
  }
  struct sserial_pin_ctx_t *pins = ctx->irq_pins;
  if(!ctx->reply_irq || !pins || systime > ctx->frt_ms + 2) {  // mode 0, or hal stopped
    return;
  }
  for(int i = 0; i < 4; i++) {  // what has arrived, a packet at a time
    int before = ctx->rxpos;
    parse(ctx, pins, 0);
    if(ctx->rxpos == before) {
      break;
    }
    PIN(irq_count) += 1.0;
  }
}

void USART1_IRQHandler(void) {
  idle_irq(USART1);
}

void USART3_IRQHandler(void) {
  idle_irq(USART3);
}

void USART6_IRQHandler(void) {
  idle_irq(USART6);
}

const hal_comp_t sserial_comp_struct = {
    .name      = "sserial",
    .nrt       = 0,
    .rt        = 0,
    .frt       = frt_func,
    .nrt_init  = nrt_init,
    .hw_init   = hw_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct sserial_ctx_t),
    .pin_count = sizeof(struct sserial_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
