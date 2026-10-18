#include "fwup_comp.h"
#include "commands.h"
#include "hal.h"
#include "defines.h"
#include "fwstage.h"
#include "fwup.h"
#include <stdio.h>
#include <string.h>

// In-band firmware update over the console (the smart-serial console while
// LinuxCNC runs, or USB): servoterm-cli --update sends the image as lines,
// this component stages it in the drive's own flash and fwapply hands it to
// the bootloader, which copies it into place at the reset. See
// docs/sserial_bootloader.md and shared/fwstage.h.
//
//   fwup0.en = fault0.en_out     the drive's enable: while it is on, the
//                                commands are refused and nothing is written
//   fwup0.words = 1              flash words programmed per rt period: each
//                                stalls the chip (one flash bank) ~16 us
//   fwup0.state                  0 idle, 1 receiving, 2 ready, 3 marking,
//                                4 marked, 5 failed
//   fwup0.got                    bytes received
// Commands (each answers one line, "fw ok ...", "fw busy" or "fw err ..."):
//   fwbegin SIZE CRC32           start, or resume the same image
//   fwdata OFFSET BASE64 CRC16   FWUP_LINE bytes at OFFSET (a console line
//                                holds 63 characters)
//   fwend                        all programmed and the CRC32 right?
//   fwapply                      mark the update pending, reset: the
//                                bootloader copies it
//   fwstat                       state, bytes, bad lines
//   sserialboot                  reset into the smart-serial loader (Mesa's
//                                setsserial flash), for recovery

HAL_COMP(fwup);

HAL_PIN(en);
HAL_PIN(words);
HAL_PIN(state);
HAL_PIN(got);

struct fwup_ctx_t {
  uint32_t reset_trigger;  // != 0: reset when the reply had time to go out
  uint32_t reset_wait;     // rt periods
};

static fwst_t fwst;
static struct fwup_ctx_t *g_ctx;
static struct fwup_pin_ctx_t *g_pins;

static int st_program(void *ctx, uint32_t off, const uint8_t *d, uint32_t len) {
  (void)ctx;
  return fwup_stage_program(off, d, len);
}
static const uint8_t *st_read(void *ctx, uint32_t off) {
  (void)ctx;
  return fwup_stage_read(off);
}

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct fwup_ctx_t *ctx      = (struct fwup_ctx_t *)ctx_ptr;
  struct fwup_pin_ctx_t *pins = (struct fwup_pin_ctx_t *)pin_ptr;
  PIN(words)                  = 1;
  memset(&fwst, 0, sizeof(fwst));
  fwst.stage_size = fwup_stage_size;
  fwst.max_size   = fwup_max_size;
  fwst.program    = st_program;
  fwst.read       = st_read;
  g_ctx           = ctx;
  g_pins          = pins;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct fwup_ctx_t *ctx      = (struct fwup_ctx_t *)ctx_ptr;
  struct fwup_pin_ctx_t *pins = (struct fwup_pin_ctx_t *)pin_ptr;
  if(PIN(en) <= 0.0) {  // the drive is off: a late period does no harm
    fwst_poll(&fwst, (int)CLAMP(PIN(words), 0, 16));
  }
  PIN(state) = fwst.state;
  PIN(got)   = fwst.got;
  if(ctx->reset_trigger && ctx->reset_wait > 0) {
    ctx->reset_wait--;
  }
}

static void nrt_func(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct fwup_ctx_t *ctx = (struct fwup_ctx_t *)ctx_ptr;
  (void)pin_ptr;
  if(ctx->reset_trigger && ctx->reset_wait == 0) {
    if(ctx->reset_trigger != FWUP_TRIGGER_NONE || fwst.state == FWST_MARKED) {
      uint32_t t         = ctx->reset_trigger;
      ctx->reset_trigger = 0;
      fwup_reset(t);
    }
  }
}

static int refused(void) {
  if(!g_pins) {
    printf("fw err not loaded (load fwup)\n");
    return 1;
  }
  if(g_pins->en.source->value > 0.0) {
    printf("fw err drive enabled\n");
    return 1;
  }
  return 0;
}

void fw_begin(char *ptr) {
  unsigned size, crc;
  if(refused()) {
    return;
  }
  if(sscanf(ptr, "%u %x", &size, &crc) != 2) {
    printf("fw err usage: fwbegin SIZE CRC32\n");
    return;
  }
  int r = fwst_begin(&fwst, size, crc);
  if(r == 1) {
    printf("fw ok resume %u\n", (unsigned)fwst.got);
  } else if(r == 0) {
    printf("fw ok 0\n");
  } else if(r == -2) {
    printf("fw err staging not blank: reset the drive (the bootloader erases it)\n");
  } else {
    printf("fw err size (max %u)\n", (unsigned)fwst.max_size);
  }
}
COMMAND("fwbegin", fw_begin, "firmware update: begin SIZE CRC32");

void fw_data(char *ptr) {
  if(refused()) {
    return;
  }
  int r = fwst_data(&fwst, ptr);
  if(r == 0) {
    printf("fw ok %u\n", (unsigned)fwst.got);
  } else if(r == -5) {
    printf("fw busy\n");
  } else {
    printf("fw err %d at %u\n", r, (unsigned)fwst.got);
  }
}
COMMAND("fwdata", fw_data, "firmware update: OFFSET BASE64 CRC16");

void fw_end(char *ptr) {
  if(refused()) {
    return;
  }
  int r = fwst_end(&fwst);
  if(r == 1) {
    printf("fw busy\n");
  } else if(r == 0) {
    printf("fw ok staged %u %08x\n", (unsigned)fwst.size, (unsigned)fwst.crc);
  } else {
    printf("fw err crc\n");
  }
}
COMMAND("fwend", fw_end, "firmware update: check the staged image");

void fw_apply(char *ptr) {
  if(refused()) {
    return;
  }
  if(fwst_mark(&fwst)) {
    printf("fw err not ready (fwend first)\n");
    return;
  }
  printf("fw ok reset: the bootloader copies the update\n");
  g_ctx->reset_wait    = 1000;  // 0.2 s at 5 kHz: the reply goes out first
  g_ctx->reset_trigger = FWUP_TRIGGER_NONE;
}
COMMAND("fwapply", fw_apply, "firmware update: apply the staged image (reset)");

void fw_stat(char *ptr) {
  static const char *names[] = {"idle", "receiving", "ready", "marking", "marked", "failed"};
  printf("fw state %s got %u size %u lines %u bad %u\n", names[fwst.state], (unsigned)fwst.got,
         (unsigned)fwst.size, (unsigned)fwst.lines, (unsigned)fwst.bad);
}
COMMAND("fwstat", fw_stat, "firmware update: state");

void sserial_boot(char *ptr) {
  if(refused()) {
    return;
  }
  printf("fw ok reset into the smart-serial loader (115200 baud, setsserial flash)\n");
  g_ctx->reset_wait    = 1000;
  g_ctx->reset_trigger = FWUP_TRIGGER_SSERIAL;
}
COMMAND("sserialboot", sserial_boot, "reset into the smart-serial bootloader");

hal_comp_t fwup_comp_struct = {
    .name      = "fwup",
    .nrt       = nrt_func,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct fwup_ctx_t),
    .pin_count = sizeof(struct fwup_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
