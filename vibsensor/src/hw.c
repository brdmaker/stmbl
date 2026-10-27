/*
 * vibsensor hardware layer: clocks, GPIO, SPI1 (IIS3DWB), I2C2 (TMP117),
 * TIM6 (the rt interrupt), the LED. Register level (CMSIS), RM0440. See vs.h.
 */
#include "vs.h"

// ---- clocks: 8 MHz HSE -> PLL 170 MHz (range 1 boost), as brdstepper ----
void hw_clock_init(void) {
  RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
  (void)RCC->APB1ENR1;
  PWR->CR5 &= ~PWR_CR5_R1MODE;  // range 1 boost: up to 170 MHz
  FLASH->ACR = FLASH_ACR_LATENCY_4WS | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;
  RCC->CR |= RCC_CR_HSEON | RCC_CR_HSION;  // HSI16 clocks I2C2
  while(!(RCC->CR & RCC_CR_HSERDY) || !(RCC->CR & RCC_CR_HSIRDY)) {
  }
  // 8 MHz / 2 = 4 MHz, * 85 = 340 MHz VCO, / 2 = 170 MHz (R), / 4 = 85 MHz (Q: FDCAN)
  RCC->PLLCFGR = RCC_PLLCFGR_PLLSRC_HSE | (1u << RCC_PLLCFGR_PLLM_Pos) | (85u << RCC_PLLCFGR_PLLN_Pos) |
                 (0u << RCC_PLLCFGR_PLLR_Pos) | RCC_PLLCFGR_PLLREN | (1u << RCC_PLLCFGR_PLLQ_Pos) | RCC_PLLCFGR_PLLQEN;
  RCC->CR |= RCC_CR_PLLON;
  while(!(RCC->CR & RCC_CR_PLLRDY)) {
  }
  // above 80 MHz: switch with AHB / 2, wait 1 us, then / 1 (check, RM0440)
  RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE | RCC_CFGR_SW)) | RCC_CFGR_HPRE_DIV2 | RCC_CFGR_SW_PLL;
  while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {
  }
  for(volatile int i = 0; i < 200; i++) {
  }
  RCC->CFGR &= ~RCC_CFGR_HPRE;
  SystemCoreClock = VS_SYSCLK;
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

void hw_led(int on) {
  set(GPIOB, 12, on);
}

// ---- SPI1: IIS3DWB, mode 3, 8 bit, 170 MHz / 32 = 5.3 MHz (its limit is 10) ----
static void spi_init(void) {
  RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
  pin(GPIOA, 4, OUT, 0, 0);
  set(GPIOA, 4, 1);
  pin(GPIOA, 5, AF, 5, 0);
  pin(GPIOA, 6, AF, 5, 0);
  pin(GPIOA, 7, AF, 5, 0);
  pin(GPIOB, 0, IN, 0, 0);  // INT1
  SPI1->CR1 = 0;
  SPI1->CR2 = (7u << SPI_CR2_DS_Pos) | SPI_CR2_FRXTH;  // 8 bit, RXNE at a byte
  SPI1->CR1 = SPI_CR1_MSTR | (4u << SPI_CR1_BR_Pos) | SPI_CR1_CPOL | SPI_CR1_CPHA | SPI_CR1_SSM | SPI_CR1_SSI;
  SPI1->CR1 |= SPI_CR1_SPE;
}

void hw_spi(const uint8_t *tx, uint8_t *rx, int n) {
  volatile uint8_t *dr = (volatile uint8_t *)&SPI1->DR;  // 8-bit access: one byte a frame
  set(GPIOA, 4, 0);
  for(int i = 0; i < n; i++) {
    while(!(SPI1->SR & SPI_SR_TXE)) {
    }
    *dr = tx ? tx[i] : 0;
    while(!(SPI1->SR & SPI_SR_RXNE)) {
    }
    uint8_t b = *dr;
    if(rx) {
      rx[i] = b;
    }
  }
  while(SPI1->SR & SPI_SR_BSY) {
  }
  set(GPIOA, 4, 1);
}

// ---- I2C2: TMP117, 100 kHz from HSI16 ----
static void i2c_init(void) {
  RCC->CCIPR = (RCC->CCIPR & ~RCC_CCIPR_I2C2SEL) | RCC_CCIPR_I2C2SEL_1;  // HSI16
  RCC->APB1ENR1 |= RCC_APB1ENR1_I2C2EN;
  pin(GPIOA, 8, AF, 4, 1);
  pin(GPIOA, 9, AF, 4, 1);
  GPIOA->OTYPER |= (1u << 8) | (1u << 9);  // open drain (external pull-ups on the board)
  I2C2->CR1     = 0;
  I2C2->TIMINGR = 0x00303D5Bu;  // 100 kHz at 16 MHz (ST's value for HSI16: check with CubeMX)
  I2C2->CR1     = I2C_CR1_PE;
}

static int i2c_wait(uint32_t flag) {
  for(uint32_t t = 0; t < 200000; t++) {
    uint32_t isr = I2C2->ISR;
    if(isr & I2C_ISR_NACKF) {
      I2C2->ICR = I2C_ICR_NACKCF | I2C_ICR_STOPCF;
      return -1;
    }
    if(isr & flag) {
      return 0;
    }
  }
  return -2;
}

int hw_i2c(uint8_t addr, const uint8_t *w, int nw, uint8_t *r, int nr) {
  if(nw) {
    I2C2->CR2 = ((uint32_t)addr << 1) | ((uint32_t)nw << I2C_CR2_NBYTES_Pos) | (nr ? 0 : I2C_CR2_AUTOEND) | I2C_CR2_START;
    for(int i = 0; i < nw; i++) {
      if(i2c_wait(I2C_ISR_TXIS)) {
        return -1;
      }
      I2C2->TXDR = w[i];
    }
    if(i2c_wait(nr ? I2C_ISR_TC : I2C_ISR_STOPF)) {
      return -1;
    }
  }
  if(nr) {
    I2C2->CR2 = ((uint32_t)addr << 1) | I2C_CR2_RD_WRN | ((uint32_t)nr << I2C_CR2_NBYTES_Pos) | I2C_CR2_AUTOEND |
                I2C_CR2_START;
    for(int i = 0; i < nr; i++) {
      if(i2c_wait(I2C_ISR_RXNE)) {
        return -1;
      }
      r[i] = (uint8_t)I2C2->RXDR;
    }
    if(i2c_wait(I2C_ISR_STOPF)) {
      return -1;
    }
  }
  I2C2->ICR = I2C_ICR_STOPCF;
  return 0;
}

// ---- TIM6: the rt interrupt at 5 kHz ----
static void rt_timer_init(void) {
  RCC->APB1ENR1 |= RCC_APB1ENR1_TIM6EN;
  TIM6->PSC  = 0;
  TIM6->ARR  = VS_SYSCLK / VS_RT_HZ - 1;
  TIM6->DIER = TIM_DIER_UIE;
  TIM6->CR1  = TIM_CR1_CEN;
}

void hw_init(void) {
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN | RCC_AHB2ENR_GPIOCEN;
  (void)RCC->AHB2ENR;
  pin(GPIOB, 12, OUT, 0, 0);
  hw_led(0);
  spi_init();
  i2c_init();
  rt_timer_init();
}
