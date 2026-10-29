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

//#include "stm32f4xx_conf.h"
#include "hal.h"
#include "setup.h"
#include "setup_api.h"
#include "defines.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "main.h"
#include "commands.h"
#include "version_number.h"

extern RCC_ClocksTypeDef RCC_Clocks;

uint32_t hal_get_systick_value() {
  return (SysTick->VAL);
}

uint32_t hal_get_systick_reload() {
  return (SysTick->LOAD);
}

uint32_t hal_get_systick_freq() {
  return (RCC_Clocks.HCLK_Frequency);
}

void wdog_boot(void);   // src/wdog.c
void wdog_start(void);
void wdog_poll(void);
void wdog_crumb(void);
void wdog_rt(void);
const char *wdog_reset_name(void);
void errlog_boot(void);  // src/errlog_f4.c
void errlog_poll(void);

volatile uint64_t systime = 0;

void SysTick_Handler(void) {
  systime++;
}

//20kHz
void TIM_SLAVE_HANDLER(void) {
  TIM_ClearITPendingBit(TIM_SLAVE, TIM_IT_Update);
  hal_run_frt();
  wdog_crumb();
  if(TIM_GetITStatus(TIM_SLAVE, TIM_IT_Update) == SET) {
    if(hal.flash_grace > 0) {  // late after a flash write (fwup)
      hal.flash_late++;
      return;
    }
    hal_stop();
    hal.hal_state = FRT_TOO_LONG;
  }
}

//5 kHz interrupt for hal. at this point all ADCs have been sampled,
//see setup_res() in setup.c if you are interested in the magic behind this.
void DMA2_Stream0_IRQHandler(void) {
  DMA_ClearITPendingBit(DMA2_Stream0, DMA_IT_TCIF0);
  hal_run_rt();
  wdog_rt();
  if(hal.flash_grace > 0) {
    hal.flash_grace--;
    if(DMA_GetITStatus(DMA2_Stream0, DMA_IT_TCIF0) == SET) {
      hal.flash_late++;
      return;
    }
  }
  if(DMA_GetITStatus(DMA2_Stream0, DMA_IT_TCIF0) == SET) {
    hal_stop();
    hal.hal_state = RT_TOO_LONG;
  }
}

// Enter ST's USB DFU (ROM bootloader). The `bootloader` command leaves a
// request in CCM (not cleared at startup, like the crash record) and
// resets; SystemInit() calls dfu_request_check() first thing, so the jump
// into the ROM happens in the reset state, as from the stmbl bootloader.
// Jumping from the running app failed every other time (after dfu-util's
// ":leave", which jumps into flash without a reset, the ROM went straight
// back to flash), and from main() after the clock setup 1 in 10. The stmbl
// bootloader's own DFU path (its magic word, a bad app CRC) did not remap
// the system memory before v0.9.18-286, so the ROM's USB never came up.
#define DFU_REQUEST_ADDR 0x1000FE00
#define DFU_REQUEST 0xD0F0B007

void dfu_request_check(void) {
  if(*((volatile uint32_t *)DFU_REQUEST_ADDR) != DFU_REQUEST) {
    return;
  }
  *((volatile uint32_t *)DFU_REQUEST_ADDR) = 0;
  RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
  SYSCFG->MEMRMP = 0x01;  // system memory at 0: the ROM's own vectors (AN2606)
  void (*rom)(void) = (void (*)(void))(*((uint32_t *)0x1FFF0004));
  __set_MSP(*((uint32_t *)0x1FFF0000));
  rom();
}

void bootloader(char *ptr) {
  hal_stop();
  *((volatile uint32_t *)DFU_REQUEST_ADDR) = DFU_REQUEST;
  __DSB();
  NVIC_SystemReset();
}
COMMAND("bootloader", bootloader, "enter bootloader");

// the CPUs' factory unique IDs (96 bit): this F4 (0x1FFF7A10), and the HV
// board's F3 (0x1FFFF7AC) as it reports it with its state (src/comps/hv.c)
extern uint32_t hv_uid[3];

void uid(char *ptr) {
  const uint32_t *id = (const uint32_t *)0x1FFF7A10;
  printf("lv uid: %08lx%08lx%08lx\n", id[2], id[1], id[0]);
  printf("usb serial: %08lX%04lX\n", id[0] + id[2], id[1] >> 16);
  if(hv_uid[0] | hv_uid[1] | hv_uid[2]) {
    printf("hv uid: %08lx%08lx%08lx\n", hv_uid[2], hv_uid[1], hv_uid[0]);
  } else {
    printf("hv uid: not reported (no HV board, or its firmware is older than v0.9.18-293)\n");
  }
}
COMMAND("uid", uid, "show the unique IDs of the lv (F4) and hv (F3) CPUs");

void nv_reset(char *ptr) {
  NVIC_SystemReset();
}
COMMAND("reset", nv_reset, "reset STMBL");

void about(char *ptr) {
  printf("######## software info ########\n");
  printf(
      "%s v%i.%i.%i %s\n",
      version_info.product_name,
      version_info.major,
      version_info.minor,
      version_info.patch,
      version_info.git_version);
  printf("Branch %s\n", version_info.git_branch);
  printf("setup api %d\n", STMBL_SETUP_API);
  printf("Compiled %s %s ", version_info.build_date, version_info.build_time);
  printf("by %s on %s\n", version_info.build_user, version_info.build_host);
  printf("GCC        %s\n", __VERSION__);
  printf("newlib     %s\n", _NEWLIB_VERSION);
#ifdef __CM4_CMSIS_VERSION
  printf("CMSIS      %i.%i\n", __CM4_CMSIS_VERSION_MAIN, __CM4_CMSIS_VERSION_SUB);
#endif
#ifdef __STM32F4XX_STDPERIPH_VERSION
  printf("StdPeriph  %i.%i.%i\n", __STM32F4XX_STDPERIPH_VERSION_MAIN, __STM32F4XX_STDPERIPH_VERSION_SUB1, __STM32F4XX_STDPERIPH_VERSION_SUB2);
#endif
#ifdef __STM32F3xx_HAL_VERSION
  printf("HAL lib... TODO: print version\n");
#endif
  // printf("CPU ID     %lx %lx %lx\n",U_ID[0], U_ID[1], U_ID[2]);
  printf("size: %lu crc:%lx\n", version_info.image_size, version_info.image_crc);
  printf("firmware v%lu\n", (uint32_t)version_info.fw_version);  // STMBL_FW_VERSION; `version` has all of them
  printf("last reset: %s (`crash` for a crash or hang)\n", wdog_reset_name());
  call_cmd("name");
  call_cmd("motor");
  volatile const version_info_t *bt_version_info = (void *)0x08000188;
  printf("######## Bootloader info ########\n");
  printf("DFU requests taken by the bootloader since power-on: %lu\n", ((volatile uint32_t *)DFU_REQUEST_ADDR)[1]);
  printf(
      "%s v%i.%i.%i %s\n",
      bt_version_info->product_name,
      bt_version_info->major,
      bt_version_info->minor,
      bt_version_info->patch,
      bt_version_info->git_version);
  extern uint8_t _binary_obj_hvf3_hvf3_bin_start;
  extern uint8_t _binary_obj_hvf3_hvf3_bin_size;
  extern uint8_t _binary_obj_hvf3_hvf3_bin_end;
  volatile const version_info_t *hv_version_info = (void *)(&_binary_obj_hvf3_hvf3_bin_start + 0x188);
  printf("######## HV info ########\n");
  printf(
      "%s v%i.%i.%i %s\n",
      hv_version_info->product_name,
      hv_version_info->major,
      hv_version_info->minor,
      hv_version_info->patch,
      hv_version_info->git_version);
  printf("HV firmware carried: v%ld (what the HV board runs: `version`, hv=)\n", hv_version_info->fw_magic == STMBL_FW_MAGIC ? (long)hv_version_info->fw_version : 0L);
  // printf("Branch %s\n",bt_version_info->git_branch);
  // printf("Compiled %s %s ",bt_version_info->build_date, bt_version_info->build_time);
  // printf("by %s on %s\n",bt_version_info->build_user, bt_version_info->build_host);
  //

  printf("hv start:%p ,size:%p ,end%p \n", &_binary_obj_hvf3_hvf3_bin_start, &_binary_obj_hvf3_hvf3_bin_size, &_binary_obj_hvf3_hvf3_bin_end);
}

COMMAND("about", about, "show system infos");


void sleep(char *ptr) {
  float foo = 0;
  sscanf(ptr, " %f", &foo);
  printf("sleeping for %fs\n", foo);
  Wait((uint32_t)(foo * 1000));
  printf("wakeup\n");
}

COMMAND("sleep", sleep, "sleep [s]");

void crash_running(void);
int crash_safe_mode(void);

int main(void) {
  // Relocate interrupt vectors
  extern void *g_pfnVectors;
  SCB->VTOR = (uint32_t)&g_pfnVectors;
  wdog_boot();  // the reset cause, before crash_safe_mode() counts

  setup();
  errlog_boot();  // before rt: compacting erases a sector
  hal_init(0.0002, 0.00005);
  // hal load comps
  load_comp(comp_by_name("term"));
  hal_parse("flashloadconf");
  if(!crash_safe_mode()) {  // crash loop: console only, see stm32f4xx_it.c
    hal_parse("loadconf");
    hal_parse("relink");
    conf_version_check();
  }
  hal_parse("start");

  TIM_Cmd(TIM_MASTER, ENABLE);
  TIM_ITConfig(TIM_SLAVE, TIM_IT_Update, ENABLE);
  wdog_start();

  while(1)  //run non realtime stuff
  {
    hal_run_nrt();
    crash_running();
    wdog_poll();
    errlog_poll();
    //cdc_poll();
    Wait(1);
  }
}

void Wait(uint32_t ms) {
  uint64_t t = systime + ms;
  while(t >= systime) {
    wdog_poll();
  }
}
