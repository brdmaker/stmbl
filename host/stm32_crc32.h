#pragma once
#include <stdint.h>

/*
 * CRC-32/MPEG-2: matches the STM32 hardware CRC unit used on both the F4
 * (CRC_CalcBlockCRC) and F3 (HAL_CRC_Calculate).
 * Polynomial 0x04C11DB7, init 0xFFFFFFFF, no input/output reflection.
 * Processes 32-bit words, same as the STM32 peripheral.
 */
static inline uint32_t stm32_crc32_word(uint32_t crc, uint32_t data) {
  for(int i = 0; i < 32; i++) {
    if((crc ^ data) & 0x80000000U) {
      crc = (crc << 1) ^ 0x04C11DB7U;
    } else {
      crc <<= 1;
    }
    data <<= 1;
  }
  return crc;
}

static inline uint32_t stm32_crc32(const uint32_t *data, int nwords) {
  uint32_t crc = 0xFFFFFFFFU;
  for(int i = 0; i < nwords; i++) {
    crc = stm32_crc32_word(crc, data[i]);
  }
  return crc;
}
