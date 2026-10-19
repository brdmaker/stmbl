/*
 * printf goes to the console: the smart-serial console's link layer
 * (shared/sscon.c, once the sserial component runs) and USB (to do). As
 * src/syscalls.c on the F4: waits up to 100 ms for room in thread mode,
 * drops what does not fit in an interrupt.
 */
#include <stdint.h>
#include "sscon.h"
#include "stm32g4xx.h"

extern volatile uint64_t systime;

int _write(int fd, const char *ptr, int len) {
  (void)fd;
  if(!sscon0.active) {
    return len;
  }
  int left       = len;
  uint64_t t0    = systime;
  while(left > 0) {
    int n = left > 64 ? 64 : left;
    if(sscon_tx_free(&sscon0) < (uint32_t)n && __get_IPSR() == 0 && systime - t0 < 100) {
      continue;
    }
    uint32_t pm = __get_PRIMASK();
    __disable_irq();
    sscon_write(&sscon0, ptr, n);
    __set_PRIMASK(pm);
    ptr += n;
    left -= n;
    t0 = systime;
  }
  return len;
}
