/*
 * ls_host.c — host-only HAL component: F3 (HV) side of the inter-CPU link.
 *
 * Mirrors stm32f303/src/comps/ls.c but replaces the STM32 DMA/UART hardware
 * with non-blocking read()/write() on a POSIX fd.
 *
 * The fd is passed via the environment variable HV_F3_HOST_FD (decimal int).
 * If absent the component silently does nothing.
 *
 * HAL pins are identical to ls.c so F3-side control components (curpid, svm,
 * dq/idq) wire to this component exactly as they do on real hardware.
 *
 * Simplified vs real ls.c:
 *   - No DMA phase-lock PLL (dma_pos_cmd, inc, window pins omitted)
 *   - No USART RTOF idle-line reset
 *   - Response sent immediately after each valid received packet
 */
#include "ls_host_comp.h"
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

HAL_COMP(ls_host);

/* Commands received from F4 */
HAL_PIN(d_cmd);
HAL_PIN(q_cmd);
HAL_PIN(pos);
HAL_PIN(vel);
HAL_PIN(en);
HAL_PIN(cmd_mode);
HAL_PIN(phase_mode);
HAL_PIN(ignore_fault_pin);

/* Motor config received from F4 via WRITE_CONF rotation */
HAL_PIN(r);
HAL_PIN(l);
HAL_PIN(psi);
HAL_PIN(cur_bw);
HAL_PIN(cur_ff);
HAL_PIN(cur_ind);
HAL_PIN(max_y);
HAL_PIN(max_cur);
HAL_PIN(dac);

/* Current/voltage feedback sent back to F4 (driven by curpid/svm or set directly) */
HAL_PIN(id_fb);
HAL_PIN(iq_fb);
HAL_PIN(ud_fb);
HAL_PIN(uq_fb);

/* State data sent back to F4 in rotating slots */
HAL_PIN(dc_volt);
HAL_PIN(pwm_volt);
HAL_PIN(u_fb);
HAL_PIN(v_fb);
HAL_PIN(w_fb);
HAL_PIN(hv_temp);
HAL_PIN(mot_temp);
HAL_PIN(core_temp);
HAL_PIN(y);

/* Fault sent to F4 */
HAL_PIN(fault_in);

/* Diagnostics */
HAL_PIN(crc_error);
HAL_PIN(crc_ok);
HAL_PIN(timeout);
HAL_PIN(fault);   /* communication fault */

/* Simulation only: cycles without a packet before timeout */
#define LS_HOST_TIMEOUT_CYCLES 200

struct ls_host_ctx_t {
  int      fd;
  uint8_t  rxbuf[sizeof(packet_to_hv_t)];
  int      rxlen;
  uint32_t timeout;
  uint32_t tx_addr;
  f3_config_data_t config;
  f3_state_data_t  state;
};

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct ls_host_ctx_t *ctx = (struct ls_host_ctx_t *)ctx_ptr;
  (void)pin_ptr;

  ctx->fd      = -1;
  ctx->rxlen   = 0;
  ctx->timeout = 0;
  ctx->tx_addr = 0;
  memset(&ctx->config, 0, sizeof(ctx->config));
  memset(&ctx->state,  0, sizeof(ctx->state));

  const char *fd_str = getenv("HV_F3_HOST_FD");
  if(!fd_str) return;

  ctx->fd = atoi(fd_str);
  if(ctx->fd < 0) return;

  int flags = fcntl(ctx->fd, F_GETFL, 0);
  if(flags >= 0) fcntl(ctx->fd, F_SETFL, flags | O_NONBLOCK);

  fprintf(stderr, "ls_host: using fd %d\n", ctx->fd);
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct ls_host_ctx_t *ctx      = (struct ls_host_ctx_t *)ctx_ptr;
  struct ls_host_pin_ctx_t *pins = (struct ls_host_pin_ctx_t *)pin_ptr;
  (void)period;

  if(ctx->fd < 0) return;

  /* Accumulate incoming bytes from F4 */
  int space = (int)sizeof(packet_to_hv_t) - ctx->rxlen;
  if(space > 0) {
    ssize_t n = read(ctx->fd, ctx->rxbuf + ctx->rxlen, (size_t)space);
    if(n > 0) ctx->rxlen += (int)n;
  }

  int got_packet = 0;

  /* Process a complete packet_to_hv_t */
  if(ctx->rxlen >= (int)sizeof(packet_to_hv_t)) {
    packet_to_hv_t *pkt = (packet_to_hv_t *)ctx->rxbuf;

    uint32_t crc = stm32_crc32(
        (uint32_t *)&pkt->header.slave_addr,
        sizeof(packet_to_hv_t) / 4 - 1);

    if(pkt->header.crc == crc &&
       pkt->header.slave_addr == 0 &&
       pkt->header.len == (sizeof(packet_to_hv_t) - sizeof(stmbl_talk_header_t)) / 4) {

      /* Apply config slot if WRITE_CONF */
      if(pkt->header.flags.cmd == WRITE_CONF) {
        uint8_t a = pkt->header.conf_addr;
        if(a < sizeof(f3_config_data_t) / 4) {
          ctx->config.data[a] = pkt->header.config.f32;
        }
      }

      /* Update process-data output pins */
      PIN(en)         = pkt->flags.enable;
      PIN(phase_mode) = pkt->flags.phase_type;
      PIN(cmd_mode)   = pkt->flags.cmd_type;
      PIN(ignore_fault_pin) = pkt->flags.ignore_fault_pin;
      PIN(d_cmd)      = pkt->d_cmd;
      PIN(q_cmd)      = pkt->q_cmd;
      PIN(pos)        = pkt->pos;
      PIN(vel)        = pkt->vel;

      /* Propagate received config to HAL pins */
      PIN(r)       = ctx->config.pins.r;
      PIN(l)       = ctx->config.pins.l;
      PIN(psi)     = ctx->config.pins.psi;
      PIN(cur_bw)  = ctx->config.pins.cur_bw;
      PIN(cur_ff)  = ctx->config.pins.cur_ff;
      PIN(cur_ind) = ctx->config.pins.cur_ind;
      PIN(max_y)   = ctx->config.pins.max_y;
      PIN(max_cur) = ctx->config.pins.max_cur;
      PIN(dac)     = ctx->config.pins.dac;

      PIN(crc_ok) += 1.0f;
      ctx->timeout = 0;
      got_packet   = 1;
    } else {
      PIN(crc_error) += 1.0f;
    }

    ctx->rxlen -= (int)sizeof(packet_to_hv_t);
    if(ctx->rxlen > 0)
      memmove(ctx->rxbuf, ctx->rxbuf + sizeof(packet_to_hv_t), (size_t)ctx->rxlen);
  }

  /* Timeout handling */
  ctx->timeout++;
  if(ctx->timeout > LS_HOST_TIMEOUT_CYCLES) {
    PIN(en)  = 0.0f;
    PIN(vel) = 0.0f;
    PIN(timeout) += 1.0f;
    PIN(fault)    = 1.0f;
  }

  /* pwm_volt derived from dc_volt and phase_mode, same as real ls.c */
  switch((uint16_t)PIN(phase_mode)) {
    case PHASE_90_3PH:
      PIN(pwm_volt) = PIN(dc_volt) / (float)M_SQRT2 * 0.95f;
      break;
    case PHASE_90_4PH:
      PIN(pwm_volt) = PIN(dc_volt) * 0.95f;
      break;
    case PHASE_120_3PH:
      PIN(pwm_volt) = PIN(dc_volt) / (float)M_SQRT3 * 0.95f;
      break;
    case PHASE_180_2PH:
    case PHASE_180_3PH:
      PIN(pwm_volt) = PIN(dc_volt) * 0.95f;
      break;
    default:
      PIN(pwm_volt) = 0.0f;
      break;
  }

  /* Send reply immediately if we received a valid packet */
  if(got_packet) {
    /* Collect state data */
    ctx->state.pins.u_fb      = PIN(u_fb);
    ctx->state.pins.v_fb      = PIN(v_fb);
    ctx->state.pins.w_fb      = PIN(w_fb);
    ctx->state.pins.hv_temp   = PIN(hv_temp);
    ctx->state.pins.mot_temp  = PIN(mot_temp);
    ctx->state.pins.core_temp = PIN(core_temp);
    ctx->state.pins.y         = PIN(y);
    ctx->state.pins.dc_volt   = PIN(dc_volt);
    ctx->state.pins.pwm_volt  = PIN(pwm_volt);

    packet_from_hv_t reply;
    memset(&reply, 0, sizeof(reply));
    reply.id_fb  = PIN(id_fb);
    reply.iq_fb  = PIN(iq_fb);
    reply.ud_fb  = PIN(ud_fb);
    reply.uq_fb  = PIN(uq_fb);
    reply.fault  = (uint8_t)PIN(fault_in);
    reply.buf    = 0;

    /* Rotating state slot */
    reply.header.conf_addr  = (uint8_t)(ctx->tx_addr);
    reply.header.config.f32 = ctx->state.data[ctx->tx_addr];
    ctx->tx_addr = (ctx->tx_addr + 1) % (sizeof(f3_state_data_t) / 4);

    reply.header.slave_addr      = 0;
    reply.header.len             = (sizeof(packet_from_hv_t) - sizeof(stmbl_talk_header_t)) / 4;
    reply.header.flags.cmd       = WRITE_CONF;
    reply.header.flags.counter++;

    reply.header.crc = stm32_crc32(
        (uint32_t *)&reply.header.slave_addr,
        sizeof(packet_from_hv_t) / 4 - 1);

    write(ctx->fd, &reply, sizeof(reply));
  }
}

const hal_comp_t ls_host_comp_struct = {
  .name      = "ls_host",
  .nrt       = 0,
  .rt        = rt_func,
  .frt       = 0,
  .nrt_init  = nrt_init,
  .hw_init   = 0,
  .rt_start  = 0,
  .frt_start = 0,
  .rt_stop   = 0,
  .frt_stop  = 0,
  .ctx_size  = sizeof(struct ls_host_ctx_t),
  .pin_count = sizeof(struct ls_host_pin_ctx_t) / sizeof(hal_pin_inst_t),
};
