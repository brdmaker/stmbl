/*
 * IIS3DWB accelerometer on SPI1 (vibsensor/src/hw.c): 26.667 kHz x y z
 * through its FIFO into vibbuf for vib (shared/comps/vib.c).
 *
 * Set up at load: soft reset, block data update and address increment,
 * the accelerometer on at full scale range_g, the FIFO continuous with
 * every sample. In rt (5 kHz, ~5.3 samples a period) the FIFO's fill level
 * is read and that many words (tag + x y z, 7 bytes each) are read out,
 * one transaction a word. At 5.3 MHz SPI that is ~60 us of the 200 us
 * period: SPI with DMA, started by INT1 at a watermark, would free it.
 *
 * Registers from the IIS3DWB datasheet as remembered (the LSM6 family's
 * map): check each against DS12569 at the bench.
 *   iis0.who      WHO_AM_I: 0x7B (123) when the chip answers
 *   iis0.range_g  2, 4, 8, 16 (g full scale); a change is applied at once
 *   iis0.fifo     words found in the FIFO at the last read
 *   iis0.ovr      FIFO overruns seen (samples lost: rt too late)
 *   iis0.samples  samples put into vibbuf
 *   iis0.temp     the chip's own temperature (degC), not the machine's
 */
#include "iis_comp.h"
#include "hal.h"
#include "defines.h"
#include "vibbuf.h"
#include "vs.h"
#include "main.h"

HAL_COMP(iis);

HAL_PIN(who);
HAL_PIN(range_g);
HAL_PIN(fifo);
HAL_PIN(ovr);
HAL_PIN(samples);
HAL_PIN(temp);

#define WHO_AM_I 0x0F
#define FIFO_CTRL3 0x09  // BDR_XL[3:0]: 0xA = 26667 Hz
#define FIFO_CTRL4 0x0A  // FIFO_MODE[2:0]: 6 = continuous
#define CTRL1_XL 0x10    // XL_EN[7:5] = 101 on; FS_XL[3:2]: 00 2 g, 01 16 g, 10 4 g, 11 8 g
#define CTRL3_C 0x12     // BOOT 7, BDU 6, IF_INC 2, SW_RESET 0
#define OUT_TEMP_L 0x20  // 256 LSB / degC, 0 = 25 degC
#define FIFO_STATUS1 0x3A
#define FIFO_STATUS2 0x3B  // DIFF_FIFO[9:8] in 1:0, FIFO_OVR_IA 6
#define FIFO_DATA_OUT_TAG 0x78
#define TAG_XL 0x02  // the tag's sensor field (bits 7:3) of an accelerometer word

struct iis_ctx_t {
  float range;  // applied
  uint32_t tick;
};

static void wr(uint8_t reg, uint8_t v) {
  uint8_t b[2] = {reg, v};
  hw_spi(b, 0, 2);
}

static void rd(uint8_t reg, uint8_t *d, int n) {
  uint8_t tx[16] = {(uint8_t)(reg | 0x80)}, rx[16];
  n = n > 15 ? 15 : n;
  hw_spi(tx, rx, n + 1);
  for(int i = 0; i < n; i++) {
    d[i] = rx[i + 1];
  }
}

static uint8_t fs_bits(float g) {
  return g <= 2 ? 0 : g <= 4 ? 2 : g <= 8 ? 3 : 1;
}

static float lsb(float g) {  // g per count: 0.061 mg at 2 g, doubling with the range
  return g <= 2 ? 0.000061f : g <= 4 ? 0.000122f : g <= 8 ? 0.000244f : 0.000488f;
}

static void apply_range(struct iis_ctx_t *ctx, struct iis_pin_ctx_t *pins) {
  float g          = PIN(range_g) <= 2 ? 2 : PIN(range_g) <= 4 ? 4 : PIN(range_g) <= 8 ? 8 : 16;
  PIN(range_g)     = g;
  ctx->range       = g;
  vibbuf.g_per_lsb = lsb(g);
  wr(CTRL1_XL, 0xA0 | (uint8_t)(fs_bits(g) << 2));
}

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct iis_ctx_t *ctx      = (struct iis_ctx_t *)ctx_ptr;
  struct iis_pin_ctx_t *pins = (struct iis_pin_ctx_t *)pin_ptr;
  uint8_t who;
  wr(CTRL3_C, 0x01);  // soft reset
  Wait(2);
  wr(CTRL3_C, 0x44);  // BDU, IF_INC
  rd(WHO_AM_I, &who, 1);
  PIN(who)     = who;
  PIN(range_g) = 16;
  vibbuf.fs    = 26667.0f;
  apply_range(ctx, pins);
  wr(FIFO_CTRL3, 0x0A);
  wr(FIFO_CTRL4, 0x06);
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct iis_ctx_t *ctx      = (struct iis_ctx_t *)ctx_ptr;
  struct iis_pin_ctx_t *pins = (struct iis_pin_ctx_t *)pin_ptr;
  if(PIN(range_g) != ctx->range) {
    apply_range(ctx, pins);
  }
  uint8_t st[2];
  rd(FIFO_STATUS1, st, 2);
  int n = st[0] | (st[1] & 3) << 8;
  if(st[1] & 0x40) {
    PIN(ovr)++;
  }
  PIN(fifo) = n;
  n         = MIN(n, 16);  // the rest next period: bounded time here
  for(int i = 0; i < n; i++) {
    uint8_t w[7];
    rd(FIFO_DATA_OUT_TAG, w, 7);
    if((w[0] >> 3) != TAG_XL) {
      continue;
    }
    vib_xyz_t v = {(int16_t)(w[1] | w[2] << 8), (int16_t)(w[3] | w[4] << 8), (int16_t)(w[5] | w[6] << 8)};
    vibbuf_put(&vibbuf, v);
    PIN(samples)++;
  }
  if(++ctx->tick >= 5000) {  // the die temperature once a second
    ctx->tick = 0;
    uint8_t t[2];
    rd(OUT_TEMP_L, t, 2);
    PIN(temp) = 25.0f + (int16_t)(t[0] | t[1] << 8) / 256.0f;
  }
}

hal_comp_t iis_comp_struct = {
    .name      = "iis",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct iis_ctx_t),
    .pin_count = sizeof(struct iis_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
