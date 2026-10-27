/*
 * vibsensor board: pins and the hardware layer (vibsensor/src/hw.c,
 * src/fdcan.c). docs/vibsensor.md. MCU STM32G473CEU6 (UFQFPN-48), 170 MHz
 * from an 8 MHz crystal, as brdstepper.
 *
 * Not yet run on a board: the drivers follow RM0440, ST's pin data and the
 * IIS3DWB / TMP117 datasheets as remembered; lines marked "check" need the
 * documents at the bench.
 */
#pragma once
#include <stdint.h>
#include "stm32g4xx.h"

#define VS_SYSCLK 170000000u
#define VS_RT_HZ 5000u  // rt (TIM6): the IIS3DWB's FIFO into vibbuf, vib's filters

// pins (check against ST's pin data with the board)
//   PA4        IIS3DWB CS            PA5, PA6, PA7  SPI1 SCK, MISO, MOSI (AF5)
//   PB0        IIS3DWB INT1 (FIFO watermark; polled for now)
//   PA8, PA9   I2C2 SDA, SCL (AF4)   TMP117 at 0x48 (ADD0 to GND)
//   PB8, PB9   FDCAN1 RX, TX (AF9)   (BOOT0 off in the option bytes, as brdstepper)
//   PB13       CAN transceiver STB (low: normal)
//   PB12       LED                   PA11, PA12 USB    PA13, PA14 SWD

void hw_clock_init(void);
void hw_init(void);        // GPIO, SPI1, I2C2, TIM6 (rt), LED
void hw_led(int on);
// SPI1 to the IIS3DWB: one transaction, CS low; tx may be 0 (send 0s), rx may be 0
void hw_spi(const uint8_t *tx, uint8_t *rx, int n);
// I2C2: write w bytes then read r bytes from addr; 0 ok, < 0 no answer / timeout
int hw_i2c(uint8_t addr, const uint8_t *w, int nw, uint8_t *r, int nr);
void fdcan_init(void);     // FDCAN1: 1 Mbit/s arbitration, 5 Mbit/s data
void fdcan_poll(void);     // bus-off recovery, from the main loop
