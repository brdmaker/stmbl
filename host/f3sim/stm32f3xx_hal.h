/*
 * A host stand-in for the parts of the STM32F3 HAL and registers that
 * stm32f303/src/comps/io.c uses, so the real io.c runs in a host test
 * (host/tests/test_f3_io.c). Registers are plain structs the test reads and
 * sets; bit values are the real ones (stm32f303xc.h); HAL_GPIO_* record the
 * pin levels per port.
 */
#pragma once
#include <stdint.h>

typedef struct { volatile uint32_t BDTR, CCR1, CCR2, CCR3; } TIM_TypeDef;
typedef struct { volatile uint32_t ISR, IFCR; } DMA_TypeDef;
typedef struct { volatile uint32_t CCR, CNDTR, CPAR, CMAR; } DMA_Channel_TypeDef;
typedef struct { volatile uint32_t CR, CSR; } PWR_TypeDef;
typedef struct { volatile uint32_t CFGR; } ADC_TypeDef;
typedef struct { volatile uint32_t CCR, CDR; } ADC_Common_TypeDef;
typedef struct { volatile uint32_t DHR12R1; } DAC_TypeDef;
typedef struct { volatile uint32_t CSR; } COMP_TypeDef;
typedef struct { volatile uint32_t ODR, IDR, inited; } GPIO_TypeDef;
typedef struct { uint32_t Pin, Mode, Pull, Speed; } GPIO_InitTypeDef;
typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET } GPIO_PinState;

extern TIM_TypeDef f3sim_tim8;
extern DMA_TypeDef f3sim_dma1, f3sim_dma2;
extern DMA_Channel_TypeDef f3sim_dma1_ch1, f3sim_dma2_ch5;
extern PWR_TypeDef f3sim_pwr;
extern ADC_TypeDef f3sim_adc1, f3sim_adc3;
extern ADC_Common_TypeDef f3sim_adc12, f3sim_adc34;
extern DAC_TypeDef f3sim_dac1;
extern COMP_TypeDef f3sim_comp1, f3sim_comp2, f3sim_comp4;
extern GPIO_TypeDef f3sim_gpioa, f3sim_gpiob;

#define TIM8 (&f3sim_tim8)
#define DMA1 (&f3sim_dma1)
#define DMA2 (&f3sim_dma2)
#define DMA1_Channel1 (&f3sim_dma1_ch1)
#define DMA2_Channel5 (&f3sim_dma2_ch5)
#define PWR (&f3sim_pwr)
#define ADC1 (&f3sim_adc1)
#define ADC3 (&f3sim_adc3)
#define ADC12_COMMON (&f3sim_adc12)
#define ADC34_COMMON (&f3sim_adc34)
#define DAC1 (&f3sim_dac1)
#define COMP1 (&f3sim_comp1)
#define COMP2 (&f3sim_comp2)
#define COMP4 (&f3sim_comp4)
#define GPIOA (&f3sim_gpioa)
#define GPIOB (&f3sim_gpiob)

// stm32f303xc.h
#define TIM_BDTR_MOE 0x00008000U
#define DMA_ISR_TCIF1 0x00000002U
#define DMA_ISR_TCIF5 0x00020000U
#define DMA_IFCR_CTCIF1 0x00000002U
#define DMA_IFCR_CTCIF5 0x00020000U
#define DMA_CCR_EN 0x00000001U
#define DMA_CCR_CIRC 0x00000020U
#define DMA_CCR_MINC 0x00000080U
#define DMA_CCR_PSIZE_1 0x00000200U
#define DMA_CCR_MSIZE_1 0x00000800U
#define DMA_CCR_PL_0 0x00001000U
#define PWR_CSR_PVDO 0x00000004U
#define ADC_CFGR_DMAEN 0x00000001U
#define ADC_CFGR_DMACFG 0x00000002U
#define COMP_CSR_COMPxOUT 0x40000000U
#define GPIO_PIN_2 0x0004U
#define GPIO_PIN_7 0x0080U
#define GPIO_PIN_8 0x0100U
#define GPIO_PIN_15 0x8000U
#define GPIO_MODE_INPUT 0x0U
#define GPIO_MODE_OUTPUT_PP 0x1U
#define GPIO_NOPULL 0x0U
#define GPIO_SPEED_FREQ_LOW 0x0U

void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *init);
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState s);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin);
