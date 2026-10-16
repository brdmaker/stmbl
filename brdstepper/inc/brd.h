/*
 * brdstepper board: pins and the hardware layer (brdstepper/src/hw.c).
 * Pin plan and parts: docs/brdstepper_hw.md. MCU STM32G473CEU6 (UFQFPN-48),
 * 170 MHz from an 8 MHz crystal.
 *
 * Not yet run on a board: the drivers follow RM0440 and ST's pin data
 * (alternate functions from STM32_open_pin_data); details marked "check"
 * need the reference manual at the bench.
 */
#pragma once
#include <stdint.h>
#include "stm32g4xx.h"

#define BRD_SYSCLK 170000000u
#define BRD_PWM_HZ 20000u                            // PWM and the frt loop
#define BRD_PWM_ARR (BRD_SYSCLK / (2u * BRD_PWM_HZ))  // centre aligned: 4250
#define BRD_FRT_PER_RT 4                             // rt at 5 kHz

// pins (docs/brdstepper_hw.md)
//   PA8, PA9   TIM1 CH1, CH2 (AF6)  DRV8876 #1 IN1, IN2 (coil A)
//   PA10       TIM1 CH3 (AF6)       DRV8876 #2 IN1 (coil B)
//   PC6        TIM8 CH1 (AF4)       DRV8876 #2 IN2 (coil B)
//   PA0        ADC1_IN1             IPROPI A
//   PA1        ADC2_IN2             IPROPI B
//   PA2        ADC1_IN3             motor supply (100k / 10k)
//   PA3        ADC1_IN4             NTC (10k, 10k pull-up)
//   PB0        ADC1_IN15            spare analog input
//   PB1        ADC1_IN12            hardware version ID divider
//   PA4        DAC1_OUT1            DRV8876 VREF (both)
//   PA5        nSLEEP (both)        PA6 nFAULT (both)   PA7 enable input
//   PB3..PB5   SPI1 (AF5)           AS5047P, PA15 its CS
//   PB6, PB7   USART1 (AF7)         smart-serial variant
//   PB8, PB9   FDCAN1 (AF9)         CAN variant (BOOT0 off in the option bytes)
//   PB12 LED   PB13 CAN standby     PA11, PA12 USB
#define ADC_CH_IA 1     // ADC1
#define ADC_CH_IB 2     // ADC2
#define ADC_CH_VBUS 3   // ADC1
#define ADC_CH_NTC 4    // ADC1
#define ADC_CH_AIN 15   // ADC1
#define ADC_CH_HWID 12  // ADC1

// analog scaling
#define BRD_VREF 3.3f
#define BRD_IPROPI_R 4700.0f       // IPROPI resistor [ohm] (parts list: 4.7k ~ 1.5 A)
#define BRD_AIPROPI 0.000450f      // DRV8876 current mirror gain [A/A]: check the datasheet
#define BRD_VBUS_DIV (110.0f / 10.0f)

typedef enum { BRD_SSERIAL = 0, BRD_CAN = 1, BRD_UNKNOWN = 2 } brd_variant_t;

typedef struct {
  // per PWM period: both windows of each coil (shared/bip_loop.h, bip_sense)
  float ia_drive, ia_brake, ib_drive, ib_brake;  // [A], >= 0
  float vbus;                                    // [V]
  float ntc_v, ain_v, hwid_v;                    // [V]
} brd_adc_t;

void hw_clock_init(void);
void hw_init(void);                 // GPIO, PWM, ADC, DAC, SPI; bridges asleep
void hw_pwm_start(void);            // counters running, ADC triggered
void hw_pwm_set(float da, float db);  // duties -1..1 (sign: direction); applied at the next period
void hw_bridges(int on);            // nSLEEP
int hw_bridge_fault(void);          // nFAULT low
int hw_enable_input(void);          // PA7
void hw_vref(float v);              // DRV8876 VREF (current trip) [V]
void hw_led(int on);
uint16_t hw_as5047_read(uint16_t cmd);  // one 16-bit SPI frame, returns the previous frame's answer
brd_variant_t hw_variant(float hwid_v);
extern volatile brd_adc_t brd_adc;   // the latest samples (written in the frt interrupt)
extern volatile uint32_t brd_adc_count;
