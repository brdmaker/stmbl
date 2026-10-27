/*
 * vibsensor firmware: stmbl's HAL with vib and vnode on an STM32G473
 * (docs/vibsensor.md).
 *
 * Timing:
 *   rt   5 kHz  TIM6: the IIS3DWB's FIFO into vibbuf (iis), vib's filters
 *   nrt         the main loop: vib's FFTs, vnode (the bus), thermo, the
 *               console, config
 * SysTick at 1 kHz counts systime (ms); its counter times the HAL.
 * The LED: blinks fast without a node number, slowly with one, and is on
 * while vib0.crash is latched.
 */
#include "vs.h"
#include "hal.h"
#include "commands.h"
#include "defines.h"
#include "version_number.h"
#include <stdio.h>

volatile uint64_t systime = 0;

uint32_t hal_get_systick_value(void) {
  return SysTick->VAL;
}
uint32_t hal_get_systick_reload(void) {
  return SysTick->LOAD;
}
uint32_t hal_get_systick_freq(void) {
  return VS_SYSCLK;
}
void hal_init_watchdog(float time) {  // IWDG: to do
  (void)time;
}
void hal_reset_watchdog(void) {
}

void SysTick_Handler(void) {
  systime++;
}

void TIM6_DAC_IRQHandler(void) {
  TIM6->SR = 0;
  hal_run_rt();
  if(TIM6->SR & TIM_SR_UIF) {  // the next rt period came already
    hal_stop();
    hal.hal_state = RT_TOO_LONG;
  }
}

void Wait(uint32_t ms) {
  uint64_t t = systime + ms;
  while(t >= systime) {
  }
}

void nv_reset(char *ptr) {
  (void)ptr;
  NVIC_SystemReset();
}
COMMAND("reset", nv_reset, "reset the sensor");

// ST's ROM bootloader (USB DFU): system memory at 0x1FFF0000
void bootloader(char *ptr) {
  (void)ptr;
  hal_stop();
  __disable_irq();
  SysTick->CTRL = 0;
  for(int i = 0; i < 8; i++) {
    NVIC->ICER[i] = 0xffffffff;
    NVIC->ICPR[i] = 0xffffffff;
  }
  __enable_irq();
  SYSCFG->MEMRMP = 1;  // system memory at 0
  uint32_t sp = *(volatile uint32_t *)0x1FFF0000, pc = *(volatile uint32_t *)0x1FFF0004;
  __set_MSP(sp);
  ((void (*)(void))pc)();
}
COMMAND("bootloader", bootloader, "enter the ROM bootloader (USB DFU)");

void about(char *ptr) {
  (void)ptr;
  extern volatile uint32_t fdcan_rx_lost, fdcan_busoff;
  printf("vibsensor (STM32G473), firmware v%d\n", STMBL_FW_VERSION);
  printf("GCC %s\n", __VERSION__);
  printf("can: rx lost %lu, bus off %lu\n", (unsigned long)fdcan_rx_lost, (unsigned long)fdcan_busoff);
  hal_parse("vnid");
}
COMMAND("about", about, "show system infos");

static float value(hal_pin_inst_t *p) {
  return p ? p->source->value : 0.0f;
}

int main(void) {
  extern uint32_t g_pfnVectors;
  SCB->VTOR = (uint32_t)&g_pfnVectors;
  hw_clock_init();
  SysTick_Config(VS_SYSCLK / 1000);
  NVIC_SetPriority(SysTick_IRQn, 1);
  RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
  hw_init();
  fdcan_init();
  NVIC_SetPriority(TIM6_DAC_IRQn, 2);

  hal_init(0.0002, 0.00005);
  hal_parse("flashloadconf");
  hal_parse("loadconf");  // an empty (erased) config: src/config.c links the default
  hal_parse("start");
  NVIC_EnableIRQ(TIM6_DAC_IRQn);
  NAME vn = "vnode", vb = "vib", pn = "node", pc = "crash";  // the lookup takes whole NAMEs
  hal_pin_inst_t *node  = pin_inst_by_name(vn, 0, pn);
  hal_pin_inst_t *crash = pin_inst_by_name(vb, 0, pc);

  while(1) {
    hal_run_nrt();
    fdcan_poll();
    hw_led(value(crash) > 0.0f || (systime / (value(node) > 0.0f ? 1000 : 125)) % 2);
  }
}
