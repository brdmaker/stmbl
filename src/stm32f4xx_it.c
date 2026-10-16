/**
  ******************************************************************************
  * @file    IO_Toggle/stm32f4xx_it.c
  * @author  MCD Application Team
  * @version V1.0.0
  * @date    19-September-2011
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and
  *          peripherals interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2011 STMicroelectronics</center></h2>
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_it.h"
#include "hal.h"

/** @addtogroup STM32F4_Discovery_Peripheral_Examples
  * @{
  */

/** @addtogroup IO_Toggle
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M4 Processor Exceptions Handlers                         */
/******************************************************************************/

/*
 * Faults: record and reset.
 *
 * The handlers used to call hal_error() and return into the faulting
 * instruction, which faulted again forever. A fault in the main loop left
 * USB enumerating but the console dead (the bench drive's "rebooted, then
 * hung", 2026-10-14). Now each handler saves a record in CCM RAM, which no
 * code uses and startup does not clear, so it survives the reset; the
 * `crash` command prints it. After CRASH_MAX_RESETS crashes without
 * CRASH_OK_MS of running in between, the drive boots in safe mode: no
 * config, HAL stopped, console working (crash_safe_mode, main.c). A crash
 * loop at boot cannot hide the record, and `reset` tries a normal boot.
 */
#include <stdio.h>
#include <string.h>
#include "commands.h"

#define CRASH_MAGIC 0x43524153  // "CRAS"
#define CRASH_MAX_RESETS 3
#define CRASH_OK_MS 10000

typedef struct {
  uint32_t magic;
  uint32_t count;        // crashes since power-on (or `crash clear`)
  uint32_t consecutive;  // crashes without CRASH_OK_MS of running in between
  uint32_t handler;      // hal_error_t error_handler numbering
  uint32_t pc, lr, psr;  // stacked by the exception entry
  uint32_t cfsr, hfsr, mmfar, bfar;
  uint32_t rt_func, frt_func, nrt_func;
  uint32_t uptime_ms;
  uint32_t check;
} crash_t;

#define crash ((crash_t *)0x1000FF00)  // top of CCM

extern volatile uint64_t systime;

static uint32_t crash_check(void) {
  uint32_t c        = 0x5a5a5a5a;
  const uint32_t *w = (const uint32_t *)crash;
  for(int i = 0; i < sizeof(crash_t) / 4 - 1; i++) {
    c = (c << 1 | c >> 31) ^ w[i];
  }
  return c;
}

static int crash_valid(void) {
  return crash->magic == CRASH_MAGIC && crash->check == crash_check();
}

void crash_record(uint32_t *frame, uint32_t handler) {
  if(!crash_valid()) {
    memset(crash, 0, sizeof(crash_t));
    crash->magic = CRASH_MAGIC;
  }
  crash->count++;
  crash->consecutive++;
  crash->handler   = handler;
  crash->lr        = frame[5];
  crash->pc        = frame[6];
  crash->psr       = frame[7];
  crash->cfsr      = SCB->CFSR;
  crash->hfsr      = SCB->HFSR;
  crash->mmfar     = SCB->MMFAR;
  crash->bfar      = SCB->BFAR;
  crash->rt_func   = hal.active_rt_func;
  crash->frt_func  = hal.active_frt_func;
  crash->nrt_func  = hal.active_nrt_func;
  crash->uptime_ms = (uint32_t)systime;
  crash->check     = crash_check();

  hal_error(handler);
  NVIC_SystemReset();
}

// At boot: too many crashes in a row, so don't load the config.
int crash_safe_mode(void) {
  return crash_valid() && crash->consecutive >= CRASH_MAX_RESETS;
}

// Called from the main loop: the drive has been up long enough, so the next
// crash may reset again.
void crash_running(void) {
  if(systime >= CRASH_OK_MS && crash_valid() && crash->consecutive) {
    crash->consecutive = 0;
    crash->check       = crash_check();
  }
}

static const char *crash_names[] = {"HardFault", "NMI", "MemManage", "BusFault", "UsageFault"};

void crash_cmd(char *ptr) {
  if(ptr && strncmp(ptr, "clear", 5) == 0) {
    crash->magic = 0;
    printf("crash record cleared\n");
    return;
  }
  if(!crash_valid()) {
    printf("no crash recorded since power-on\n");
    return;
  }
  printf("crashes since power-on: %lu (in a row: %lu)\n", crash->count, crash->consecutive);
  if(crash_safe_mode()) {
    printf("safe mode: config not loaded, HAL stopped; 'reset' boots normally\n");
  }
  printf("last: %s at uptime %lu ms\n", crash->handler < 5 ? crash_names[crash->handler] : "?", crash->uptime_ms);
  printf("pc 0x%08lx lr 0x%08lx psr 0x%08lx\n", crash->pc, crash->lr, crash->psr);
  printf("cfsr 0x%08lx hfsr 0x%08lx mmfar 0x%08lx bfar 0x%08lx\n", crash->cfsr, crash->hfsr, crash->mmfar, crash->bfar);
  printf("active rt func %ld, frt func %ld, nrt func %ld\n", (int32_t)crash->rt_func, (int32_t)crash->frt_func, (int32_t)crash->nrt_func);
  printf("(arm-none-eabi-addr2line -e obj_app/stmbl.elf 0x%08lx)\n", crash->pc);
}
COMMAND("crash", crash_cmd, "show the last fault record, 'crash clear' clears it");

// Each handler passes the stacked frame (MSP or PSP, from EXC_RETURN) and
// its hal_error_t number to crash_record.
#define FAULT_HANDLER(name, num)                \
  __attribute__((naked)) void name(void) {      \
    __asm volatile("tst lr, #4\n"               \
                   "ite eq\n"                   \
                   "mrseq r0, msp\n"            \
                   "mrsne r0, psp\n"            \
                   "movs r1, #" #num "\n"       \
                   "b crash_record\n");         \
  }

FAULT_HANDLER(HardFault_Handler, 0)
FAULT_HANDLER(NMI_Handler, 1)
FAULT_HANDLER(MemManage_Handler, 2)
FAULT_HANDLER(BusFault_Handler, 3)
FAULT_HANDLER(UsageFault_Handler, 4)

/**
  * @brief  This function handles SVCall exception.
  * @param  None
  * @retval None
  */
void SVC_Handler(void) {
}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void) {
}

/**
  * @brief  This function handles PendSVC exception.
  * @param  None
  * @retval None
  */
void PendSV_Handler(void) {
}

/**
  * @brief  This function handles SysTick Handler.
  * @param  None
  * @retval None
  */
//void SysTick_Handler(void)
//{
//}

/******************************************************************************/
/*                 STM32F4xx Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f4xx.s).                                               */
/******************************************************************************/

/**
  * @brief  This function handles PPP interrupt request.
  * @param  None
  * @retval None
  */
/*void PPP_IRQHandler(void)
{
}*/

/**
  * @}
  */

/**
  * @}
  */

/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
