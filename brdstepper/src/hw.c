/*
 * brdstepper hardware layer: clocks, GPIO, the two H-bridges' PWM, the ADC,
 * VREF, the AS5047P. Register level (CMSIS), RM0440. See brd.h.
 *
 * PWM and sampling: TIM1 and TIM8 count centre aligned at 20 kHz, TIM8
 * started by TIM1 so both coils switch together. Every channel is in PWM
 * mode 2 (low while CNT < CCR), so a channel's low phase is centred at the
 * counter's 0. A coil driven forward has IN1 high and IN2 low for |d| of
 * the period around CNT = 0 (drive), both high the rest (brake, low-side
 * slow decay); backward the other way round (DRV8876 PWM mode, PMODE high:
 * 10 forward, 01 reverse, 11 brake, 00 coast). TIM1's update at both ends of
 * the count (RCR = 0) triggers ADC1 + ADC2 injected simultaneously: at
 * CNT = 0 the drive window, at the top the brake window, the two samples
 * shared/bip_loop.c's bip_sense needs (the DRV8876 senses the low side
 * only).
 */
#include "brd.h"

volatile brd_adc_t brd_adc;
volatile uint32_t brd_adc_count;

// ---- clocks: 8 MHz HSE -> PLL 170 MHz (range 1 boost) ----
void hw_clock_init(void) {
  RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
  (void)RCC->APB1ENR1;
  PWR->CR5 &= ~PWR_CR5_R1MODE;  // range 1 boost: up to 170 MHz
  FLASH->ACR = FLASH_ACR_LATENCY_4WS | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;
  RCC->CR |= RCC_CR_HSEON;
  while(!(RCC->CR & RCC_CR_HSERDY)) {
  }
  // 8 MHz / 2 = 4 MHz, * 85 = 340 MHz VCO, / 2 = 170 MHz (R), / 4 = 85 MHz (Q: FDCAN)
  RCC->PLLCFGR = RCC_PLLCFGR_PLLSRC_HSE | (1u << RCC_PLLCFGR_PLLM_Pos) | (85u << RCC_PLLCFGR_PLLN_Pos) |
                 (0u << RCC_PLLCFGR_PLLR_Pos) | RCC_PLLCFGR_PLLREN | (1u << RCC_PLLCFGR_PLLQ_Pos) | RCC_PLLCFGR_PLLQEN;
  RCC->CR |= RCC_CR_PLLON;
  while(!(RCC->CR & RCC_CR_PLLRDY)) {
  }
  // above 80 MHz: switch with AHB / 2, wait 1 us, then / 1 (RM0440, "clock
  // source switching" in the RCC chapter: check)
  RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE | RCC_CFGR_SW)) | RCC_CFGR_HPRE_DIV2 | RCC_CFGR_SW_PLL;
  while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {
  }
  for(volatile int i = 0; i < 200; i++) {
  }
  RCC->CFGR &= ~RCC_CFGR_HPRE;
  SystemCoreClock = BRD_SYSCLK;
}

// ---- GPIO ----
enum { IN = 0, OUT = 1, AF = 2, AN = 3 };
static void pin(GPIO_TypeDef *g, int n, int mode, int af, int pull) {
  g->MODER   = (g->MODER & ~(3u << (2 * n))) | ((uint32_t)mode << (2 * n));
  g->PUPDR   = (g->PUPDR & ~(3u << (2 * n))) | ((uint32_t)pull << (2 * n));  // 1 up, 2 down
  g->OSPEEDR |= 3u << (2 * n);
  if(n < 8) {
    g->AFR[0] = (g->AFR[0] & ~(15u << (4 * n))) | ((uint32_t)af << (4 * n));
  } else {
    g->AFR[1] = (g->AFR[1] & ~(15u << (4 * (n - 8)))) | ((uint32_t)af << (4 * (n - 8)));
  }
}
static void set(GPIO_TypeDef *g, int n, int v) {
  g->BSRR = v ? (1u << n) : (1u << (n + 16));
}

// ---- PWM ----
static void pwm_timer(TIM_TypeDef *t) {
  t->CR1  = TIM_CR1_CMS_0 | TIM_CR1_ARPE;  // centre aligned 1, ARR preload
  t->PSC  = 0;
  t->ARR  = BRD_PWM_ARR;
  t->RCR  = 0;  // update at both ends of the count
  t->BDTR = TIM_BDTR_MOE;
}

void hw_pwm_set(float da, float db) {
  // forward: IN1 high (CCR 0), IN2 low for |d| around CNT = 0; backward: swapped
  uint32_t a = (uint32_t)((da < 0 ? -da : da) * BRD_PWM_ARR);
  uint32_t b = (uint32_t)((db < 0 ? -db : db) * BRD_PWM_ARR);
  a          = a > BRD_PWM_ARR ? BRD_PWM_ARR : a;
  b          = b > BRD_PWM_ARR ? BRD_PWM_ARR : b;
  TIM1->CCR1 = da < 0 ? a : 0;  // A IN1
  TIM1->CCR2 = da < 0 ? 0 : a;  // A IN2
  TIM1->CCR3 = db < 0 ? b : 0;  // B IN1
  TIM8->CCR1 = db < 0 ? 0 : b;  // B IN2
}

void hw_pwm_coast(void) {
  // PWM mode 2 with CCR above ARR: the output never goes active (high)
  TIM1->CCR1 = BRD_PWM_ARR + 1;
  TIM1->CCR2 = BRD_PWM_ARR + 1;
  TIM1->CCR3 = BRD_PWM_ARR + 1;
  TIM8->CCR1 = BRD_PWM_ARR + 1;
}

// ---- ADC: ADC1 + ADC2, injected simultaneous, triggered by TIM1 TRGO2 ----
static void adc_on(ADC_TypeDef *a) {
  a->CR &= ~ADC_CR_DEEPPWD;
  a->CR |= ADC_CR_ADVREGEN;
  for(volatile int i = 0; i < 4000; i++) {  // > 20 us
  }
  a->CR |= ADC_CR_ADCAL;  // single-ended calibration
  while(a->CR & ADC_CR_ADCAL) {
  }
  for(volatile int i = 0; i < 100; i++) {
  }
  a->ISR = ADC_ISR_ADRDY;
  a->CR |= ADC_CR_ADEN;
  while(!(a->ISR & ADC_ISR_ADRDY)) {
  }
}

// JSQR: 4 conversions (JL = 3), external trigger TIM1_TRGO2 on its rising edge.
// JEXTSEL 8 = TIM1_TRGO2 for ADC1/2: check RM0440's injected trigger table.
#define JEXT_TIM1_TRGO2 8u
static uint32_t jsqr(int c1, int c2, int c3, int c4) {
  return (3u << ADC_JSQR_JL_Pos) | (JEXT_TIM1_TRGO2 << ADC_JSQR_JEXTSEL_Pos) | (1u << ADC_JSQR_JEXTEN_Pos) |
         ((uint32_t)c1 << ADC_JSQR_JSQ1_Pos) | ((uint32_t)c2 << ADC_JSQR_JSQ2_Pos) | ((uint32_t)c3 << ADC_JSQR_JSQ3_Pos) |
         ((uint32_t)c4 << ADC_JSQR_JSQ4_Pos);
}

static void adc_init(void) {
  RCC->AHB2ENR |= RCC_AHB2ENR_ADC12EN;
  (void)RCC->AHB2ENR;
  ADC12_COMMON->CCR = (3u << ADC_CCR_CKMODE_Pos) |  // HCLK / 4 = 42.5 MHz, synchronous to the timer
                      (5u << ADC_CCR_DUAL_Pos);      // injected simultaneous only
  adc_on(ADC1);
  adc_on(ADC2);
  // 6.5 cycles everywhere (paired conversions must take equally long): 0.45 us each
  ADC1->SMPR1 = 0x09249249u;  // 001 for channels 0..9
  ADC1->SMPR2 = 0x01249249u;  // 001 for channels 10..18
  ADC2->SMPR1 = 0x09249249u;
  ADC2->SMPR2 = 0x01249249u;
  ADC1->JSQR  = jsqr(ADC_CH_IA, ADC_CH_VBUS, ADC_CH_NTC, ADC_CH_HWID);
  ADC2->JSQR  = jsqr(ADC_CH_IB, ADC_CH_IB, ADC_CH_IB, ADC_CH_IB);
  ADC1->IER   = ADC_IER_JEOSIE;
  NVIC_SetPriority(ADC1_2_IRQn, 0);  // the frt loop
  NVIC_EnableIRQ(ADC1_2_IRQn);
}

static float amps(uint32_t raw) {
  return raw * (BRD_VREF / 4095.0f) / BRD_IPROPI_R / BRD_AIPROPI;
}

// one injected sequence done: which window it sampled, from the counter's direction
void brd_adc_isr(void (*period_done)(void)) {
  ADC1->ISR = ADC_ISR_JEOS;
  int brake = (TIM1->CR1 & TIM_CR1_DIR) != 0;  // counting down: the trigger was the top (brake window)
  float ia = amps(ADC1->JDR1), ib = amps(ADC2->JDR1);
  if(brake) {
    brd_adc.ia_brake = ia;
    brd_adc.ib_brake = ib;
    brd_adc.vbus     = ADC1->JDR2 * (BRD_VREF / 4095.0f) * BRD_VBUS_DIV;
    brd_adc.ntc_v    = ADC1->JDR3 * (BRD_VREF / 4095.0f);
    brd_adc.hwid_v   = ADC1->JDR4 * (BRD_VREF / 4095.0f);
    brd_adc_count++;
    period_done();  // both windows of this period are in: run the loop
  } else {
    brd_adc.ia_drive = ia;
    brd_adc.ib_drive = ib;
  }
}

// ---- the rest ----
void hw_init(void) {
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN | RCC_AHB2ENR_GPIOCEN | RCC_AHB2ENR_DAC1EN;
  RCC->APB2ENR |= RCC_APB2ENR_TIM1EN | RCC_APB2ENR_TIM8EN | RCC_APB2ENR_SPI1EN;
  (void)RCC->APB2ENR;

  // bridges asleep first
  set(GPIOA, 5, 0);
  pin(GPIOA, 5, OUT, 0, 2);  // nSLEEP, pulled down on the board too
  pin(GPIOA, 6, IN, 0, 1);   // nFAULT
  pin(GPIOA, 7, IN, 0, 2);   // enable input
  set(GPIOB, 12, 0);
  pin(GPIOB, 12, OUT, 0, 0);  // LED
  set(GPIOB, 13, 1);
  pin(GPIOB, 13, OUT, 0, 0);  // CAN standby (until the CAN variant runs)

  // analog inputs
  pin(GPIOA, 0, AN, 0, 0);
  pin(GPIOA, 1, AN, 0, 0);
  pin(GPIOA, 2, AN, 0, 0);
  pin(GPIOA, 3, AN, 0, 0);
  pin(GPIOB, 0, AN, 0, 0);
  pin(GPIOB, 1, AN, 0, 0);
  pin(GPIOA, 4, AN, 0, 0);  // DAC1_OUT1

  // PWM: TIM1 CH1..3 (PA8..PA10, AF6), TIM8 CH1 (PC6, AF4), all PWM mode 2 with preload
  pwm_timer(TIM1);
  pwm_timer(TIM8);
  uint32_t pwm2_12 = (7u << TIM_CCMR1_OC1M_Pos) | TIM_CCMR1_OC1PE | (7u << TIM_CCMR1_OC2M_Pos) | TIM_CCMR1_OC2PE;
  TIM1->CCMR1      = pwm2_12;
  TIM1->CCMR2      = (7u << TIM_CCMR2_OC3M_Pos) | TIM_CCMR2_OC3PE;
  TIM1->CCER       = TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E;
  TIM1->CR2        = (1u << TIM_CR2_MMS_Pos) |    // TRGO = enable: starts TIM8
              (2u << TIM_CR2_MMS2_Pos);    // TRGO2 = update: triggers the ADC at both ends
  TIM8->CCMR1      = (7u << TIM_CCMR1_OC1M_Pos) | TIM_CCMR1_OC1PE;
  TIM8->CCER       = TIM_CCER_CC1E;
  TIM8->SMCR       = (6u << TIM_SMCR_SMS_Pos) | (0u << TIM_SMCR_TS_Pos);  // trigger mode, ITR0 = TIM1: check
  hw_pwm_coast();
  pin(GPIOA, 8, AF, 6, 0);
  pin(GPIOA, 9, AF, 6, 0);
  pin(GPIOA, 10, AF, 6, 0);
  pin(GPIOC, 6, AF, 4, 0);

  adc_init();

  // DAC1 OUT1: VREF, the DRV8876's own current trip (a backstop)
  DAC1->MCR = DAC_MCR_HFSEL_1;  // AHB above 160 MHz
  DAC1->CR  = DAC_CR_EN1;
  hw_vref(BRD_VREF);

  // SPI1 to the AS5047P: mode 1, 16 bit, 170 / 32 = 5.3 MHz, CS by hand (PA15)
  set(GPIOA, 15, 1);
  pin(GPIOA, 15, OUT, 0, 0);
  pin(GPIOB, 3, AF, 5, 0);
  pin(GPIOB, 4, AF, 5, 0);
  pin(GPIOB, 5, AF, 5, 0);
  SPI1->CR2 = (15u << SPI_CR2_DS_Pos);
  SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | (4u << SPI_CR1_BR_Pos) | SPI_CR1_CPHA | SPI_CR1_SPE;
}

void hw_pwm_start(void) {
  ADC1->CR |= ADC_CR_JADSTART;  // the master: both ADCs wait for the trigger
  TIM1->EGR = TIM_EGR_UG;
  TIM8->EGR = TIM_EGR_UG;
  TIM1->CR1 |= TIM_CR1_CEN;  // TIM8 follows (trigger mode)
}

void hw_bridges(int on) {
  set(GPIOA, 5, on);
}

int hw_bridge_fault(void) {
  return !(GPIOA->IDR & (1u << 6));
}

int hw_enable_input(void) {
  return (GPIOA->IDR & (1u << 7)) != 0;
}

void hw_vref(float v) {
  float x     = v / BRD_VREF;
  x           = x < 0 ? 0 : x > 1 ? 1 : x;
  DAC1->DHR12R1 = (uint32_t)(x * 4095.0f);
}

void hw_led(int on) {
  set(GPIOB, 12, on);
}

uint16_t hw_as5047_read(uint16_t cmd) {
  set(GPIOA, 15, 0);
  while(!(SPI1->SR & SPI_SR_TXE)) {
  }
  *(volatile uint16_t *)&SPI1->DR = cmd;
  while(!(SPI1->SR & SPI_SR_RXNE)) {
  }
  uint16_t r = *(volatile uint16_t *)&SPI1->DR;
  while(SPI1->SR & SPI_SR_BSY) {
  }
  set(GPIOA, 15, 1);  // CS high >= 350 ns before the next frame (the caller's pace)
  return r;
}

brd_variant_t hw_variant(float v) {
  if(v < 0.4f) {
    return BRD_SSERIAL;
  }
  if(v > 1.3f && v < 2.0f) {
    return BRD_CAN;
  }
  return BRD_UNKNOWN;
}
