/*
 * hv_lv_host.c — host-only HAL component: F4 (LV) side of the inter-CPU link.
 *
 * Mirrors src/comps/hv.c but replaces the STM32 DMA/UART hardware with
 * non-blocking read()/write() on a POSIX fd (socketpair or PTY).
 *
 * The fd is passed via the environment variable HV_LV_HOST_FD (decimal int).
 * If absent the component silently does nothing.
 *
 * HAL pins are identical to hv.c so existing HAL scripts wire up correctly.
 * The bootloader flash state machine is omitted — simulation stays in
 * SLAVE_IN_APP mode permanently.
 */
#include "hv_lv_host_comp.h"
#include "hal.h"
#include "common.h"
#include "stm32_crc32.h"

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

HAL_COMP(hv_lv_host);

/* Commands sent to F3 */
HAL_PIN(d_cmd);
HAL_PIN(q_cmd);
HAL_PIN(pos);
HAL_PIN(vel);
HAL_PIN(en);
HAL_PIN(phase_mode);
HAL_PIN(cmd_mode);

/* Motor config written to F3 via rotating WRITE_CONF */
HAL_PIN(r);
HAL_PIN(l);
HAL_PIN(psi);
HAL_PIN(cur_bw);
HAL_PIN(cur_ff);
HAL_PIN(cur_ind);
HAL_PIN(max_y);
HAL_PIN(max_cur);
HAL_PIN(dac);

/* Feedback received from F3 */
HAL_PIN(id_fb);
HAL_PIN(iq_fb);
HAL_PIN(ud_fb);
HAL_PIN(uq_fb);
HAL_PIN(abs_cur);
HAL_PIN(abs_volt);
HAL_PIN(duty);

/* Rotating state data received from F3 */
HAL_PIN(dc_volt);
HAL_PIN(pwm_volt);
HAL_PIN(u_fb);
HAL_PIN(v_fb);
HAL_PIN(w_fb);
HAL_PIN(hv_temp);
HAL_PIN(mot_temp);
HAL_PIN(core_temp);
HAL_PIN(y);

/* Diagnostics */
HAL_PIN(fault);
HAL_PIN(ignore_fault_pin);
HAL_PIN(rev);
HAL_PIN(crc_error);
HAL_PIN(scale);

/* Simulation only: how many RT cycles without a reply before timeout fault */
#define HV_HOST_TIMEOUT_CYCLES 200

struct hv_lv_host_ctx_t {
  int      fd;
  uint8_t  rxbuf[sizeof(packet_from_hv_t)];
  int      rxlen;
  uint8_t  conf_addr;
  uint32_t timeout;
  f3_config_data_t config;
  f3_state_data_t  state;
};

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct hv_lv_host_ctx_t *ctx = (struct hv_lv_host_ctx_t *)ctx_ptr;
  (void)pin_ptr;

  ctx->fd        = -1;
  ctx->rxlen     = 0;
  ctx->conf_addr = 0;
  ctx->timeout   = 0;
  memset(&ctx->config, 0, sizeof(ctx->config));
  memset(&ctx->state,  0, sizeof(ctx->state));

  const char *fd_str = getenv("HV_LV_HOST_FD");
  if(!fd_str) return;

  ctx->fd = atoi(fd_str);
  if(ctx->fd < 0) return;

  int flags = fcntl(ctx->fd, F_GETFL, 0);
  if(flags >= 0) fcntl(ctx->fd, F_SETFL, flags | O_NONBLOCK);

  fprintf(stderr, "hv_lv_host: using fd %d\n", ctx->fd);
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct hv_lv_host_ctx_t *ctx      = (struct hv_lv_host_ctx_t *)ctx_ptr;
  struct hv_lv_host_pin_ctx_t *pins = (struct hv_lv_host_pin_ctx_t *)pin_ptr;
  (void)period;

  if(ctx->fd < 0) return;

  /* Accumulate incoming bytes from F3 */
  int space = (int)sizeof(packet_from_hv_t) - ctx->rxlen;
  if(space > 0) {
    ssize_t n = read(ctx->fd, ctx->rxbuf + ctx->rxlen, (size_t)space);
    if(n > 0) ctx->rxlen += (int)n;
  }

  /* Process a complete packet_from_hv_t */
  if(ctx->rxlen >= (int)sizeof(packet_from_hv_t)) {
    packet_from_hv_t *pkt = (packet_from_hv_t *)ctx->rxbuf;

    uint32_t crc = stm32_crc32(
        (uint32_t *)&pkt->header.slave_addr,
        sizeof(packet_from_hv_t) / 4 - 1);

    if(pkt->header.crc == crc &&
       pkt->header.slave_addr == 0 &&
       pkt->header.len == (sizeof(packet_from_hv_t) - sizeof(stmbl_talk_header_t)) / 4) {

      PIN(id_fb) = pkt->id_fb;
      PIN(iq_fb) = pkt->iq_fb;
      if(PIN(rev) > 0.0f) {
        PIN(iq_fb) *= -1.0f;
      }
      PIN(ud_fb)   = pkt->ud_fb;
      PIN(uq_fb)   = pkt->uq_fb;
      if(PIN(rev) > 0.0f) {
        PIN(uq_fb) *= -1.0f;
      }
      PIN(fault)   = pkt->fault;
      PIN(abs_cur) = sqrtf(PIN(id_fb) * PIN(id_fb) + PIN(iq_fb) * PIN(iq_fb));
      PIN(abs_volt) = sqrtf(PIN(ud_fb) * PIN(ud_fb) + PIN(uq_fb) * PIN(uq_fb));
      if(PIN(pwm_volt) > 0.0f) {
        PIN(duty) = PIN(abs_volt) / PIN(pwm_volt);
      }

      /* Rotating state slot from F3 */
      uint8_t slot = pkt->header.conf_addr;
      if(slot < sizeof(f3_state_data_t) / 4) {
        ctx->state.data[slot] = pkt->header.config.f32;
      }
      PIN(dc_volt)   = ctx->state.pins.dc_volt;
      PIN(pwm_volt)  = ctx->state.pins.pwm_volt;
      PIN(u_fb)      = ctx->state.pins.u_fb;
      PIN(v_fb)      = ctx->state.pins.v_fb;
      PIN(w_fb)      = ctx->state.pins.w_fb;
      PIN(hv_temp)   = ctx->state.pins.hv_temp;
      PIN(mot_temp)  = ctx->state.pins.mot_temp;
      PIN(core_temp) = ctx->state.pins.core_temp;
      PIN(y)         = ctx->state.pins.y;

      ctx->timeout = 0;
    } else {
      PIN(crc_error) += 1.0f;
    }

    /* Consume the packet */
    ctx->rxlen -= (int)sizeof(packet_from_hv_t);
    if(ctx->rxlen > 0)
      memmove(ctx->rxbuf, ctx->rxbuf + sizeof(packet_from_hv_t), (size_t)ctx->rxlen);
  }

  /* Timeout */
  ctx->timeout++;
  if(ctx->timeout > HV_HOST_TIMEOUT_CYCLES) {
    PIN(fault) = HV_TIMEOUT_ERROR;
  }

  /* Build and send packet_to_hv_t */
  ctx->config.pins.r       = PIN(r);
  ctx->config.pins.l       = PIN(l);
  ctx->config.pins.psi     = PIN(psi);
  ctx->config.pins.cur_bw  = PIN(cur_bw);
  ctx->config.pins.cur_ff  = PIN(cur_ff);
  ctx->config.pins.cur_ind = PIN(cur_ind);
  ctx->config.pins.max_y   = PIN(max_y);
  ctx->config.pins.max_cur = PIN(max_cur) * (PIN(scale) > 0.0f ? PIN(scale) : 1.0f);
  ctx->config.pins.dac     = PIN(dac);

  packet_to_hv_t pkt;
  memset(&pkt, 0, sizeof(pkt));

  float d_cmd = PIN(d_cmd);
  float q_cmd = PIN(q_cmd);
  float pos   = PIN(pos);
  if(PIN(rev) > 0.0f) {
    q_cmd *= -1.0f;
    /* pos negation matches minus(0, pos) from the firmware */
    pos = -pos;
  }

  if(PIN(en) > 0.0f) {
    pkt.d_cmd        = d_cmd;
    pkt.q_cmd        = q_cmd;
    pkt.flags.enable = 1;
  } else {
    pkt.d_cmd        = 0.0f;
    pkt.q_cmd        = 0.0f;
    pkt.flags.enable = 0;
  }
  pkt.pos                    = pos;
  pkt.vel                    = PIN(vel);
  pkt.flags.cmd_type         = (uint32_t)PIN(cmd_mode);
  pkt.flags.phase_type       = (uint32_t)PIN(phase_mode);
  pkt.flags.ignore_fault_pin = (PIN(ignore_fault_pin) > 0.0f) ? 1 : 0;
  pkt.flags.buf              = 0;

  pkt.header.slave_addr       = 0;
  pkt.header.len              = (sizeof(packet_to_hv_t) - sizeof(stmbl_talk_header_t)) / 4;
  pkt.header.flags.cmd        = WRITE_CONF;
  pkt.header.flags.counter++;
  pkt.header.conf_addr        = ctx->conf_addr;
  pkt.header.config.f32       = ctx->config.data[ctx->conf_addr];
  ctx->conf_addr = (ctx->conf_addr + 1) % (sizeof(f3_config_data_t) / 4);

  pkt.header.crc = stm32_crc32(
      (uint32_t *)&pkt.header.slave_addr,
      sizeof(packet_to_hv_t) / 4 - 1);

  write(ctx->fd, &pkt, sizeof(pkt));
}

const hal_comp_t hv_lv_host_comp_struct = {
  .name      = "hv_lv_host",
  .nrt       = 0,
  .rt        = rt_func,
  .frt       = 0,
  .nrt_init  = nrt_init,
  .hw_init   = 0,
  .rt_start  = 0,
  .frt_start = 0,
  .rt_stop   = 0,
  .frt_stop  = 0,
  .ctx_size  = sizeof(struct hv_lv_host_ctx_t),
  .pin_count = sizeof(struct hv_lv_host_pin_ctx_t) / sizeof(hal_pin_inst_t),
};
