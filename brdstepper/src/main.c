/*
 * brdstepper firmware: stmbl's HAL and shared components on an STM32G473.
 *
 * Timing:
 *   frt  20 kHz  ADC1_2 interrupt, after the brake-window sample of each
 *                PWM period (brdstepper/src/hw.c): the current loop
 *   rt    5 kHz  every 4th frt, in a lower priority interrupt (TIM7's
 *                vector, pended by software): position / velocity
 *   nrt          the main loop: console, commands, config
 * SysTick at 1 kHz counts systime (ms); its counter times the HAL.
 */
#include "brd.h"
#include "hal.h"
#include "commands.h"
#include "defines.h"
#include <stdio.h>

volatile uint64_t systime = 0;

uint32_t hal_get_systick_value(void) {
  return SysTick->VAL;
}
uint32_t hal_get_systick_reload(void) {
  return SysTick->LOAD;
}
uint32_t hal_get_systick_freq(void) {
  return BRD_SYSCLK;
}
void hal_init_watchdog(float time) {  // IWDG: to do
  (void)time;
}
void hal_reset_watchdog(void) {
}

void SysTick_Handler(void) {
  systime++;
}

static uint32_t frt_n;
static void period_done(void) {
  hal_run_frt();
  if(++frt_n >= BRD_FRT_PER_RT) {
    frt_n = 0;
    NVIC_SetPendingIRQ(TIM7_DAC_IRQn);
  }
}

void ADC1_2_IRQHandler(void) {
  extern void brd_adc_isr(void (*period_done)(void));
  brd_adc_isr(period_done);
  // a sequence waiting is normally the next drive-window sample (25 us after
  // the brake one, the counter counting up); one waiting while it counts
  // down again is the next brake-window sample: the loop took a whole period
  if((ADC1->ISR & ADC_ISR_JEOS) && (TIM1->CR1 & TIM_CR1_DIR)) {
    hal_stop();
    hal.hal_state = FRT_TOO_LONG;
  }
}

void TIM7_DAC_IRQHandler(void) {
  hal_run_rt();
  if(NVIC_GetPendingIRQ(TIM7_DAC_IRQn)) {  // the next rt period came already
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
COMMAND("reset", nv_reset, "reset brdstepper");

// ST's ROM bootloader (USB DFU): system memory at 0x1FFF0000
void bootloader(char *ptr) {
  (void)ptr;
  hal_stop();
  __disable_irq();
  hw_bridges(0);
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
  extern brd_variant_t brd_variant;
  printf("brdstepper (STM32G473, %s variant)\n",
         brd_variant == BRD_SSERIAL ? "smart-serial" : brd_variant == BRD_CAN ? "CAN" : "unknown");
  printf("GCC %s, CMSIS %u.%u\n", __VERSION__, (unsigned)(__CM4_CMSIS_VERSION >> 16), (unsigned)(__CM4_CMSIS_VERSION & 0xffff));
  printf("vbus %.1f V, %lu PWM periods\n", (double)brd_adc.vbus, (unsigned long)brd_adc_count);
}
COMMAND("about", about, "show system infos");

brd_variant_t brd_variant = BRD_UNKNOWN;

int main(void) {
  extern uint32_t g_pfnVectors;
  SCB->VTOR = (uint32_t)&g_pfnVectors;
  hw_clock_init();
  SysTick_Config(BRD_SYSCLK / 1000);
  NVIC_SetPriority(SysTick_IRQn, 1);
  RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
  hw_init();
  NVIC_SetPriority(TIM7_DAC_IRQn, 2);  // rt below frt
  NVIC_EnableIRQ(TIM7_DAC_IRQn);

  hal_init(0.0002, 0.00005);
  load_comp(comp_by_name("term"));
  hal_parse("flashloadconf");
  hal_parse("loadconf");  // an empty (erased) config: brdstepper/src/config.c links the default
  hal_parse("start");
  hw_pwm_start();
  Wait(2);
  brd_variant = hw_variant(brd_adc.hwid_v);

  while(1) {
    hal_run_nrt();
    Wait(1);
  }
}
