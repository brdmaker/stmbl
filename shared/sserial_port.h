#pragma once
// The physical ports a smart-serial instance (src/comps/sserial.c) can run on, as numbers for
// sserialN.port, and which USART each needs: two instances can't share one. Shared by the
// firmware and the host simulator's model (host/hw/sserial.c), so the rule is tested there.
//
// The wiring (RJ45 pins, MCU pins, DMA, driver enables, cables) is in src/comps/sserial.c above
// the port table. In short, v4.1, pairs by RJ45 pin:
//   0 CMD        RX 1/2 USART1 PA10   TX 3/6 UART4 PA0    the usual port
//   1 enc0       RX 1/2 USART3 PC11   TX 3/6 USART3 PC10  same pairs as CMD: the same cable
//   2 enc1       RX 1/2 USART3 PB11   TX 4/5 USART3 PB10  no UART TX on enc1's 3/6 pair
//   3 enc0 alt   RX 1/2 USART6 PC7    TX 4/5 USART6 PC6   enc0 beside enc1 (USART3 is enc1's)
#include <stdint.h>

#define SSERIAL_PORTS 4

enum { SSERIAL_USART1 = 1, SSERIAL_USART3 = 3, SSERIAL_UART4 = 4, SSERIAL_USART6 = 6 };

static const uint8_t sserial_port_usart[SSERIAL_PORTS][2] = {
    // {rx, tx}
    {SSERIAL_USART1, SSERIAL_UART4},
    {SSERIAL_USART3, SSERIAL_USART3},
    {SSERIAL_USART3, SSERIAL_USART3},
    {SSERIAL_USART6, SSERIAL_USART6},
};

// 1 when ports a and b need the same USART (or are the same port)
static inline int sserial_port_conflict(uint32_t a, uint32_t b) {
  if(a >= SSERIAL_PORTS || b >= SSERIAL_PORTS) {
    return 0;
  }
  for(int i = 0; i < 2; i++) {
    for(int j = 0; j < 2; j++) {
      if(sserial_port_usart[a][i] == sserial_port_usart[b][j]) {
        return 1;
      }
    }
  }
  return 0;
}
