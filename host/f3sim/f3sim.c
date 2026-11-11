// host/f3sim/stm32f3xx_hal.h's registers and GPIO calls
#include "stm32f3xx_hal.h"

TIM_TypeDef f3sim_tim8;
DMA_TypeDef f3sim_dma1, f3sim_dma2;
DMA_Channel_TypeDef f3sim_dma1_ch1, f3sim_dma2_ch5;
PWR_TypeDef f3sim_pwr;
ADC_TypeDef f3sim_adc1, f3sim_adc3;
ADC_Common_TypeDef f3sim_adc12, f3sim_adc34;
DAC_TypeDef f3sim_dac1;
COMP_TypeDef f3sim_comp1, f3sim_comp2, f3sim_comp4;
GPIO_TypeDef f3sim_gpioa, f3sim_gpiob;

void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *init) {
  port->inited |= init->Pin;
}
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState s) {
  if(s == GPIO_PIN_SET) {
    port->ODR |= pin;
  } else {
    port->ODR &= ~(uint32_t)pin;
  }
}
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin) {
  return (port->IDR & pin) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}
