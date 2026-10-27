/*
 * thermo: the TMP117 on I2C2 (vibsensor/src/hw.c), on the mounting face: the machine's
 * temperature for vib0.temp. It converts on its own (continuous, 8
 * averages, 1 s by default); its result register is read once a second in
 * nrt. 7.8125 m degC per count (check: TMP117 datasheet, SNOSD82).
 *   thermo0.temp  degC
 *   thermo0.ok    1 after a good read; 0 when it doesn't answer (temp kept)
 *   thermo0.id    DEVICE_ID register (0x0117 = 279 for the TMP117)
 */
#include "thermo_comp.h"
#include "hal.h"
#include "vs.h"

HAL_COMP(thermo);

HAL_PIN(temp);
HAL_PIN(ok);
HAL_PIN(id);
HAL_PIN(errors);

#define ADDR 0x48
#define REG_TEMP 0x00
#define REG_ID 0x0F

extern volatile uint64_t systime;

struct thermo_ctx_t {
  uint64_t next;
};

static int read16(uint8_t reg, int16_t *v) {
  uint8_t r[2];
  if(hw_i2c(ADDR, &reg, 1, r, 2)) {
    return -1;
  }
  *v = (int16_t)(r[0] << 8 | r[1]);  // big endian
  return 0;
}

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct thermo_pin_ctx_t *pins = (struct thermo_pin_ctx_t *)pin_ptr;
  int16_t id;
  PIN(temp) = 25;
  if(!read16(REG_ID, &id)) {
    PIN(id) = (uint16_t)id & 0x0FFF;
  }
}

static void nrt_func(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct thermo_ctx_t *ctx      = (struct thermo_ctx_t *)ctx_ptr;
  struct thermo_pin_ctx_t *pins = (struct thermo_pin_ctx_t *)pin_ptr;
  if(systime < ctx->next) {
    return;
  }
  ctx->next = systime + 1000;
  int16_t t;
  if(read16(REG_TEMP, &t)) {
    PIN(ok) = 0;
    PIN(errors)++;
    return;
  }
  PIN(temp) = t * 0.0078125f;
  PIN(ok)   = 1;
}

hal_comp_t thermo_comp_struct = {
    .name      = "thermo",
    .nrt       = nrt_func,
    .rt        = 0,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct thermo_ctx_t),
    .pin_count = sizeof(struct thermo_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
