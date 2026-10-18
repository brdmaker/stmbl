/*
* This file is part of the stmbl project.
*
* Copyright (C) 2013-2015 Rene Hopf <renehopf@mac.com>
* Copyright (C) 2013-2015 Nico Stute <crinq@crinq.de>
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

#include "stm32f4xx_conf.h"
#include "version.h"
#include "lbp_dev.h"
#include "lbp_flash.h"
#include "fwstage.h"

#if __GNUC__ < 5
#error gcc to old (< 5.0)
#endif

#define APP_START 0x08010000
#define APP_END 0x08080000  // sectors 4..7 (448 kB); 8..11 are the staging area
#define APP_RANGE_VALID(a, s) (!(((a) | (s)) & 3) && (a) >= APP_START && ((a) + (s)) <= APP_END)
#define VERSION_INFO_OFFSET 0x188
const version_info_t *app_info = (void *)(APP_START + VERSION_INFO_OFFSET);

// the RAM word the application sets before a reset (see src/main.c)
#define BOOT_TRIGGER (*(volatile uint32_t *)0x2001C000)
#define TRIGGER_DFU 0xDEADBEEF      // ST's ROM DFU loader
#define TRIGGER_SSERIAL 0x5353424C  // "SSBL": the smart-serial loader (sserialboot)
#define TRIGGER_NONE 0xCAFEFEED

#define STAGE_START 0x08080000  // sectors 8..11
#define STAGE_SIZE 0x80000
#define WIN_START (APP_START - 0x4000)  // setsserial's block 0: one 16 kB block below
#define LOADER_DFU_TIMEOUT_S 30         // bad application, no smart-serial traffic: DFU

static int app_ok(void) {
  if(!APP_RANGE_VALID(APP_START, app_info->image_size)) {
    return 0;
  }
  CRC_ResetDR();
  uint32_t crc = CRC_CalcBlockCRC((uint32_t *)APP_START, app_info->image_size / 4);

  if(crc != 0) {
    return 0;
  }

  return 1;
}

// ---- flash: one bank, so erase / program stall this code too (nothing else runs) ----
static int sector_id(int n) {  // FLASH_Sector_n
  return n << 3;
}
static int erase_sectors(int first, int last) {
  FLASH_Unlock();
  FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR |
                  FLASH_FLAG_PGSERR);
  int r = 0;
  for(int i = first; i <= last && !r; i++) {
    r = FLASH_EraseSector(sector_id(i), VoltageRange_3) != FLASH_COMPLETE;
  }
  FLASH_Lock();
  return r ? -1 : 0;
}
// len bytes at addr, word by word; a tail is padded with 0xff (stays erased)
static int program(uint32_t addr, const uint8_t *d, uint32_t len) {
  int r = 0;
  FLASH_Unlock();
  for(uint32_t i = 0; i < len && !r; i += 4) {
    uint32_t w = 0xffffffff;
    for(uint32_t k = 0; k < 4 && i + k < len; k++) {
      ((uint8_t *)&w)[k] = d[i + k];
    }
    r = FLASH_ProgramWord(addr + i, w) != FLASH_COMPLETE;
  }
  FLASH_Lock();
  for(uint32_t i = 0; i < len && !r; i++) {
    r = ((const uint8_t *)addr)[i] != d[i];
  }
  return r ? -1 : 0;
}

// application area, application offsets (fwst_apply)
static int app_erase(void *ctx, uint32_t off, uint32_t len) {
  (void)ctx;
  (void)off;
  // sector 4: 64 kB, 5..7: 128 kB
  return erase_sectors(4, len <= 0x10000 ? 4 : 4 + (int)((len - 0x10000 + 0x1ffff) / 0x20000));
}
static int app_program(void *ctx, uint32_t off, const uint8_t *d, uint32_t len) {
  (void)ctx;
  return program(APP_START + off, d, len);
}
static const uint8_t *app_read(void *ctx, uint32_t off) {
  (void)ctx;
  return (const uint8_t *)(APP_START + off);
}
// staging area (sectors 8..11)
static int stage_erase(void *ctx, uint32_t off, uint32_t len) {
  (void)ctx;
  (void)off;
  (void)len;
  return erase_sectors(8, 11);
}
static int stage_program(void *ctx, uint32_t off, const uint8_t *d, uint32_t len) {
  (void)ctx;
  return program(STAGE_START + off, d, len);
}
static const uint8_t *stage_read(void *ctx, uint32_t off) {
  (void)ctx;
  return (const uint8_t *)(STAGE_START + off);
}
static fwst_t stage = {.stage_size = STAGE_SIZE, .max_size = APP_END - APP_START, .erase = stage_erase,
                       .program = stage_program, .read = stage_read};

// setsserial's window: offset 0 = WIN_START (block 0, refused), block 1 = APP_START
static const uint32_t win_sectors[] = {0x4000, 0x14000, 0x34000, 0x54000, 0x74000};
static int win_erase_sector(void *ctx, int i) {
  (void)ctx;
  return erase_sectors(4 + i, 4 + i);
}
static lbpf_sectors_t sectors = {win_sectors, 4, (const uint8_t *)WIN_START, 0x4000, win_erase_sector, 0, 0};
static int win_erase(void *ctx, uint32_t off, uint32_t len) {
  (void)ctx;
  return lbpf_erase_lazy(&sectors, off, len);
}
static int win_program(void *ctx, uint32_t off, const uint8_t *d, uint32_t len) {
  (void)ctx;
  return program(WIN_START + off, d, len);
}

// ---- the smart-serial loader: 115200 baud, RX by DMA (bytes keep arriving
// while flash stalls the CPU), TX polled ----
static volatile uint8_t rxbuf[256];
static void uart_send(void *ctx, const uint8_t *d, int n) {
  (void)ctx;
  for(int i = 0; i < n; i++) {
    while(!(UART4->SR & USART_SR_TXE)) {
    }
    UART4->DR = d[i];
  }
}

static void uart_init(void) {
  GPIO_InitTypeDef g;
  USART_InitTypeDef u;
  DMA_InitTypeDef dma;
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOB | RCC_AHB1Periph_DMA2, ENABLE);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART4, ENABLE);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource0, GPIO_AF_UART4);  // TX PA0
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);  // RX PA10
  g.GPIO_Pin   = GPIO_Pin_0 | GPIO_Pin_10;
  g.GPIO_Mode  = GPIO_Mode_AF;
  g.GPIO_Speed = GPIO_Speed_50MHz;
  g.GPIO_OType = GPIO_OType_PP;
  g.GPIO_PuPd  = GPIO_PuPd_UP;
  GPIO_Init(GPIOA, &g);
  g.GPIO_Pin   = GPIO_Pin_7;  // TX enable
  g.GPIO_Mode  = GPIO_Mode_OUT;
  g.GPIO_Speed = GPIO_Speed_2MHz;
  g.GPIO_PuPd  = GPIO_PuPd_NOPULL;
  GPIO_Init(GPIOB, &g);
  GPIO_SetBits(GPIOB, GPIO_Pin_7);

  USART_StructInit(&u);
  u.USART_BaudRate = 115200;  // setsserial insists on it
  u.USART_Mode     = USART_Mode_Rx;
  USART_Init(USART1, &u);
  u.USART_Mode = USART_Mode_Tx;
  USART_Init(UART4, &u);

  DMA_DeInit(DMA2_Stream5);
  DMA_StructInit(&dma);
  dma.DMA_Channel            = DMA_Channel_4;
  dma.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DR;
  dma.DMA_Memory0BaseAddr    = (uint32_t)rxbuf;
  dma.DMA_DIR                = DMA_DIR_PeripheralToMemory;
  dma.DMA_BufferSize         = sizeof(rxbuf);
  dma.DMA_MemoryInc          = DMA_MemoryInc_Enable;
  dma.DMA_Mode               = DMA_Mode_Circular;
  DMA_Init(DMA2_Stream5, &dma);
  DMA_Cmd(DMA2_Stream5, ENABLE);
  USART_DMACmd(USART1, USART_DMAReq_Rx, ENABLE);
  USART_Cmd(USART1, ENABLE);
  USART_Cmd(UART4, ENABLE);
}

static void reset_to(uint32_t trigger) {
  BOOT_TRIGGER = trigger;
  NVIC_SystemReset();
}

static void sserial_loader(int dfu_timeout_s) {
  static lbpf_t flash;
  static lbpd_t lbp;
  uint32_t tick_s = SystemCoreClock;  // DWT cycles per second
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  // erase the application area now, before answering anything: an erase
  // during setsserial would stall past hostmot2's 1 s wait (lbp_flash.h)
  lbpf_erase_all(&sectors);

  lbpf_init(&flash);
  flash.erase_log2  = 14;
  flash.write_log2  = 8;
  flash.window_size = APP_END - WIN_START;
  flash.erase       = win_erase;
  flash.program     = win_program;
  lbp.mem           = lbpd_empty_mem;
  lbp.mem_size      = sizeof(lbpd_empty_mem);
  lbp.name[0] = 's', lbp.name[1] = 't', lbp.name[2] = 'b', lbp.name[3] = 'l';
  const uint32_t *uid = (const uint32_t *)0x1FFF7A10;
  lbp.unit  = uid[0] ^ uid[1] ^ uid[2];  // as the application
  lbp.flash = &flash;
  lbp.send  = uart_send;
  lbpd_init(&lbp);
  uart_init();

  uint32_t rxpos = 0, last = DWT->CYCCNT, start = last, writes = 0;
  while(1) {
    uint32_t head = sizeof(rxbuf) - DMA_GetCurrDataCounter(DMA2_Stream5);
    uint32_t now  = DWT->CYCCNT;
    if(rxpos != head) {
      while(rxpos != head) {
        lbpd_byte(&lbp, rxbuf[rxpos]);
        rxpos = (rxpos + 1) % sizeof(rxbuf);
      }
      last = now;
    } else if(now - last > tick_s / 1000) {
      lbpd_idle(&lbp);  // 1 ms quiet: drop a partial request
      if(flash.writes != writes && now - last > tick_s) {
        // setsserial wrote and went quiet for 1 s: a complete image starts
        writes = flash.writes;
        if(app_ok()) {
          reset_to(TRIGGER_NONE);
        }
      }
    }
    if(dfu_timeout_s && lbp.good == 0 && now - start > tick_s) {  // CYCCNT wraps at ~25 s
      start = now;
      if(--dfu_timeout_s == 0) {
        reset_to(TRIGGER_DFU);  // nobody on smart-serial: USB DFU, as before
      }
    }
  }
}

int main(void) {
  GPIO_InitTypeDef GPIO_InitDef;
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_CRC, ENABLE);
  GPIO_InitDef.GPIO_Pin   = GPIO_Pin_13;
  GPIO_InitDef.GPIO_Mode  = GPIO_Mode_IN;
  GPIO_InitDef.GPIO_OType = GPIO_OType_PP;
  GPIO_InitDef.GPIO_PuPd  = GPIO_PuPd_UP;
  GPIO_InitDef.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOA, &GPIO_InitDef);
  uint32_t pin = !GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_13);
  RCC_AHB1PeriphResetCmd(RCC_AHB1Periph_GPIOA, ENABLE);  // reset gpio a
  RCC_AHB1PeriphResetCmd(RCC_AHB1Periph_GPIOA, DISABLE);
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, DISABLE);

  // the app's `bootloader` command (src/main.c): a request word in CCM,
  // then a reset. Taken here, straight from reset; an older bootloader
  // passes it on and the app's SystemInit() takes it.
  volatile uint32_t *dfu_request = (uint32_t *)0x1000FE00;
  if(RCC->CSR & (RCC_CSR_PORRSTF | RCC_CSR_BORRSTF)) {  // power-on: CCM holds junk
    dfu_request[0] = 0;
    dfu_request[1] = 0;
    RCC->CSR |= RCC_CSR_RMVF;
  }
  uint32_t requested = *dfu_request == 0xD0F0B007;
  if(requested) {
    *dfu_request = 0;
    dfu_request[1]++;  // how many it took since power-on (`about` shows it)
  }
  uint32_t trigger = BOOT_TRIGGER;
  BOOT_TRIGGER     = TRIGGER_NONE;  //Reset bootloader trigger

  void (*SysMemBootJump)(void);
  if(requested || trigger == TRIGGER_DFU || pin) {  //Memory map, datasheet
    // AN2606: map the system memory at 0, so the ROM's interrupts (USB) use
    // its own vector table and not ours; without it the ROM's USB never
    // enumerated and the drive needed BOOT0 to recover.
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    SYSCFG->MEMRMP = 0x01;
    __set_MSP(*((uint32_t *)0x1FFF0000));
    //Point the PC to the System Memory reset vector (+4)
    //AN2606
    //Table 64. Bootloader device-dependent parameters
    SysMemBootJump = (void (*)(void))(*((uint32_t *)0x1FFF0004));
    SysMemBootJump();
    while(1)
      ;
  }
  // an update staged by the application (fwapply): copy it into place
  fwst_boot(&stage, app_erase, app_program, app_read, 0);
  if(trigger == TRIGGER_SSERIAL) {
    sserial_loader(0);  // asked for: stay until flashed (or power cycled)
  }
  if(!app_ok()) {
    sserial_loader(LOADER_DFU_TIMEOUT_S);
  }
  uint32_t stack = ((const uint32_t *)APP_START)[0];
  uint32_t entry = ((const uint32_t *)APP_START)[1];
  asm volatile(
      "msr    msp, %0        \n\t"
      "bx     %1             \n\t"
      :
      : "r"(stack), "r"(entry));
  while(1)
    ;
}
