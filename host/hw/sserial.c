/*
 * Simulation stand-in for src/comps/sserial.c, the smart-serial link to
 * LinuxCNC. Same pins as the real component; the process data comes from
 * sim_sserial_io instead of the UART. As on the drive, the link drops to
 * error (and all commands to 0) when no update arrived within the timeout.
 */
#include "sserial_comp.h"
#include "hal.h"
#include "defines.h"
#include "sim_io.h"

HAL_COMP(sserial);

// same pins as src/comps/sserial.c
HAL_PIN(error);
HAL_PIN(crc_error);
HAL_PIN(connected);
HAL_PIN(timeout);  // [20 kHz ticks], as on the drive
HAL_PIN(pos_cmd);
HAL_PIN(pos_cmd_d);
HAL_PIN(pos_fb);
HAL_PIN(vel_fb);
HAL_PIN(current);
HAL_PIN(scale);
HAL_PIN(clock_scale);
HAL_PIN(available);
HAL_PIN(phase);
HAL_PIN(in0);
HAL_PIN(in1);
HAL_PIN(in2);
HAL_PIN(in3);
HAL_PIN(fault);
HAL_PIN(out0);
HAL_PIN(out1);
HAL_PIN(out2);
HAL_PIN(out3);
HAL_PIN(enable);
HAL_PIN(index_clear);
HAL_PIN(index_out);
HAL_PIN(pos_advance);

sim_sserial_io_t sim_sserial_io;

struct sserial_ctx_t {
  uint32_t seq;
  float since;  // [s] since the last update
};

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct sserial_ctx_t *ctx      = (struct sserial_ctx_t *)ctx_ptr;
  struct sserial_pin_ctx_t *pins = (struct sserial_pin_ctx_t *)pin_ptr;
  PIN(timeout)     = 100.0;
  PIN(clock_scale) = 1.0;
  ctx->since       = 1.0;  // start in timeout, as the drive does
  ctx->seq         = sim_sserial_io.seq;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct sserial_ctx_t *ctx      = (struct sserial_ctx_t *)ctx_ptr;
  struct sserial_pin_ctx_t *pins = (struct sserial_pin_ctx_t *)pin_ptr;
  sim_sserial_io_t *io           = &sim_sserial_io;

  if(io->seq != ctx->seq) {
    ctx->seq   = io->seq;
    ctx->since = 0.0;

    io->pos_fb  = PIN(pos_fb) + PIN(vel_fb) * PIN(pos_advance);
    io->vel_fb  = PIN(vel_fb);
    io->current = PIN(current);
    io->in[0]   = PIN(in0) > 0;
    io->in[1]   = PIN(in1) > 0;
    io->in[2]   = PIN(in2) > 0;
    io->in[3]   = PIN(in3) > 0;
    io->fault   = PIN(fault) > 0;

    PIN(index_out)      = io->index_enable;
    io->index_enable_fb = PIN(index_clear) > 0 ? 0 : io->index_enable;

    PIN(pos_cmd)   = io->pos_cmd;
    PIN(pos_cmd_d) = io->vel_cmd;
    PIN(scale)     = io->scale;
    PIN(out0)      = io->out[0];
    PIN(out1)      = io->out[1];
    PIN(out2)      = io->out[2];
    PIN(out3)      = io->out[3];
    PIN(enable)    = io->enable;
  } else {
    ctx->since += period;
  }

  if(ctx->since > PIN(timeout) * 0.00005) {
    PIN(connected) = 0;
    PIN(error)     = 1;
    PIN(pos_cmd)   = 0;
    PIN(pos_cmd_d) = 0;
    PIN(out0)      = 0;
    PIN(out1)      = 0;
    PIN(out2)      = 0;
    PIN(out3)      = 0;
    PIN(enable)    = 0;
  } else {
    PIN(connected) = 1;
    PIN(error)     = 0;
  }
}

hal_comp_t sserial_comp_struct = {
    .name      = "sserial",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct sserial_ctx_t),
    .pin_count = sizeof(struct sserial_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
