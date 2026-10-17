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

#if __GNUC__ < 5
#error gcc to old (< 5.0)
#endif

#define APP_START 0x08010000
#define APP_END 0x08100000
#define APP_RANGE_VALID(a, s) (!(((a) | (s)) & 3) && (a) >= APP_START && ((a) + (s)) <= APP_END)
#define VERSION_INFO_OFFSET 0x188
const version_info_t *app_info = (void *)(APP_START + VERSION_INFO_OFFSET);

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

  void (*SysMemBootJump)(void);
  if(requested || (*((unsigned long *)0x2001C000) == 0xDEADBEEF) || pin || !app_ok()) {  //Memory map, datasheet
    *((unsigned long *)0x2001C000) = 0xCAFEFEED;                                         //Reset bootloader trigger
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
  } else {
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
}
