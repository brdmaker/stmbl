/*
 * printf goes to the console on the measurement bus: a ring that vnode
 * empties into frames (0x580 + node, src/fdcan.c), four a pass of the main
 * loop. What doesn't fit is dropped: vnode runs in that same loop, so
 * waiting for it would never end.
 */
#include <stdint.h>
#include "stm32g4xx.h"

#define CON_OUT 2048
static char ring[CON_OUT];
static volatile uint32_t w, r;

int _write(int fd, const char *ptr, int len) {
  (void)fd;
  for(int i = 0; i < len; i++) {
    uint32_t pm = __get_PRIMASK();
    __disable_irq();
    if(w - r < CON_OUT) {
      ring[w % CON_OUT] = ptr[i];
      w++;
    }
    __set_PRIMASK(pm);
  }
  return len;
}

int con_out_read(char *buf, int max) {
  int n = 0;
  while(n < max && r != w) {
    buf[n++] = ring[r % CON_OUT];
    r++;
  }
  return n;
}
