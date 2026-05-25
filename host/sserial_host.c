/*
 * sserial_host.c — host-only HAL component: LBP slave over a file descriptor.
 *
 * Mirrors src/comps/sserial.c but replaces the STM32 UART/DMA hardware with
 * non-blocking read()/write() on a POSIX fd (socketpair, PTY, etc.).
 *
 * The fd is passed via the environment variable SSERIAL_HOST_FD (decimal int).
 * If the variable is absent the component silently does nothing (safe to load
 * in a regular stmbl_host run without a connected test harness).
 *
 * HAL pins match sserial.c so the same HAL scripts and LinuxCNC wiring apply.
 */
#include "sserial_host_comp.h"
#include "hal.h"
#include "sserial.h"
#include "crc8.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

HAL_COMP(sserial_host);

/* HAL pins — identical set to sserial.c */
HAL_PIN(pos_cmd);
HAL_PIN(vel_cmd);
HAL_PIN(pos_fb);
HAL_PIN(vel_fb);
HAL_PIN(current);
HAL_PIN(enable);
HAL_PIN(fault);
HAL_PIN(in0);
HAL_PIN(in1);
HAL_PIN(in2);
HAL_PIN(in3);
HAL_PIN(out0);
HAL_PIN(out1);
HAL_PIN(out2);
HAL_PIN(out3);
HAL_PIN(connected);
HAL_PIN(error);
HAL_PIN(crc_error);
/* Set to 1.0 via sim_cmd before each ProcessData exchange to arm the
   receiver.  frt_func auto-clears it after each exchange so stale frt
   cycles from the previous step cannot consume the next pd command. */
HAL_PIN(pd_arm);

/* Process-data structs (identical to sserial.c, packed) */
#pragma pack(push, 1)
typedef struct {
  float    pos_cmd;
  float    vel_cmd;
  uint32_t out_0        : 1;
  uint32_t out_1        : 1;
  uint32_t out_2        : 1;
  uint32_t out_3        : 1;
  uint32_t enable       : 1;
  uint32_t index_enable : 1;
  uint32_t padding      : 2;
} sserial_out_t;  /* 9 bytes */

typedef struct {
  float    pos_fb;
  float    vel_fb;
  int8_t   current;
  uint32_t in_0         : 1;
  uint32_t in_1         : 1;
  uint32_t in_2         : 1;
  uint32_t in_3         : 1;
  uint32_t fault        : 1;
  uint32_t index_enable : 1;
  uint32_t padding      : 2;
} sserial_in_t;  /* 10 bytes */
#pragma pack(pop)

_Static_assert(sizeof(sserial_out_t) == 9,  "sserial_out_t size");
_Static_assert(sizeof(sserial_in_t)  == 10, "sserial_in_t size");

/* Discovery response served by this component */
static const discovery_rpc_t host_disc = {
  .input  = 11,
  .output = 9,
  .ptocp  = 0x018B,
  .gtocp  = 0x01A5,
};

/* Card name (matches the real firmware) */
static const char card_name[] = LBPCardName;

/* Fixed unit number for the host simulation */
#define HOST_UNIT_NUMBER 0xDEADBEEFu

/*
 * LBP memory map — copied verbatim from src/comps/sserial.c sserial_slave[].
 * Serves CT_RW reads so a host (e.g. mesaflash) can enumerate PDDs via the
 * PTOC/GTOC.  This is the same static descriptor table as the real firmware.
 */
static const uint8_t lbp_mem[] = {
  0x0B,0x09,0x8B,0x01,0xA5,0x01,0x00,0x00,  /* 0..7   */
  0x00,0x00,0x00,0x00,0xA0,0x20,0x10,0x80,  /* 8..15  */
  0x00,0x00,0x80,0xFF,0x00,0x00,0x80,0x7F,  /* 16..23 */
  0x08,0x00,0x72,0x61,0x64,0x00,0x70,0x6F,  /* 24..31 */
  0x73,0x5F,0x63,0x6D,0x64,0x00,0x00,0x00,  /* 32..39 */
  0x00,0x00,0x00,0x00,0xA0,0x20,0x10,0x80,  /* 40..47 */
  0x00,0x00,0x80,0xFF,0x00,0x00,0x80,0x7F,  /* 48..55 */
  0x26,0x00,0x72,0x61,0x64,0x00,0x76,0x65,  /* 56..63 */
  0x6C,0x5F,0x63,0x6D,0x64,0x00,0x00,0x00,  /* 64..71 */
  0xA0,0x04,0x01,0x80,0x00,0x00,0x00,0x00,  /* 72..79 */
  0x00,0x00,0x80,0x3F,0x46,0x00,0x6E,0x6F,  /* 80..87 */
  0x6E,0x65,0x00,0x6F,0x75,0x74,0x00,0x00,  /* 88..95 */
  0xA0,0x01,0x07,0x80,0x00,0x00,0x00,0x00,  /* 96..103 */
  0x00,0x00,0x80,0x3F,0x5F,0x00,0x6E,0x6F,  /* 104..111 */
  0x6E,0x65,0x00,0x65,0x6E,0x61,0x62,0x6C,  /* 112..119 */
  0x65,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 120..127 */
  0xA0,0x20,0x10,0x00,0x00,0x00,0x80,0xFF,  /* 128..135 */
  0x00,0x00,0x80,0x7F,0x7A,0x00,0x72,0x61,  /* 136..143 */
  0x64,0x00,0x70,0x6F,0x73,0x5F,0x66,0x62,  /* 144..151 */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 152..159 */
  0xA0,0x20,0x10,0x00,0x00,0x00,0x80,0xFF,  /* 160..167 */
  0x00,0x00,0x80,0x7F,0x99,0x00,0x72,0x61,  /* 168..175 */
  0x64,0x00,0x76,0x65,0x6C,0x5F,0x66,0x62,  /* 176..183 */
  0x00,0x00,0x00,0x00,0xA0,0x08,0x03,0x00,  /* 184..191 */
  0x00,0x00,0xF0,0xC1,0x00,0x00,0xF0,0x41,  /* 192..199 */
  0xB9,0x00,0x41,0x00,0x63,0x75,0x72,0x72,  /* 200..207 */
  0x65,0x6E,0x74,0x00,0x00,0x00,0x00,0x00,  /* 208..215 */
  0xA0,0x04,0x01,0x00,0x00,0x00,0xC8,0xC2,  /* 216..223 */
  0x00,0x00,0xC8,0x42,0xD4,0x00,0x6E,0x6F,  /* 224..231 */
  0x6E,0x65,0x00,0x69,0x6E,0x00,0x00,0x00,  /* 232..239 */
  0xA0,0x01,0x07,0x00,0x00,0x00,0x00,0x00,  /* 240..247 */
  0x00,0x00,0x80,0x3F,0xEE,0x00,0x6E,0x6F,  /* 248..255 */
  0x6E,0x65,0x00,0x66,0x61,0x75,0x6C,0x74,  /* 256..263 */
  0x00,0x00,0x00,0x00,0xA0,0x01,0x07,0x40,  /* 264..271 */
  0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x3F,  /* 272..279 */
  0x09,0x01,0x6E,0x6F,0x6E,0x65,0x00,0x69,  /* 280..287 */
  0x6E,0x64,0x65,0x78,0x5F,0x65,0x6E,0x61,  /* 288..295 */
  0x62,0x6C,0x65,0x00,0x00,0x00,0x00,0x00,  /* 296..303 */
  0xA0,0x20,0x10,0x80,0x00,0x00,0x80,0xFF,  /* 304..311 */
  0x00,0x00,0x80,0x7F,0x2C,0x01,0x6E,0x6F,  /* 312..319 */
  0x6E,0x65,0x00,0x73,0x63,0x61,0x6C,0x65,  /* 320..327 */
  0x00,0xB0,0x00,0x01,0x00,0x50,0x6F,0x73,  /* 328..335 */
  0x69,0x74,0x69,0x6F,0x6E,0x20,0x6D,0x6F,  /* 336..343 */
  0x64,0x65,0x00,0x00,0xA0,0x02,0x00,0x00,  /* 344..351 */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 352..359 */
  0x5B,0x01,0x00,0x70,0x61,0x64,0x64,0x69,  /* 360..367 */
  0x6E,0x67,0x00,0x00,0xA0,0x02,0x00,0x80,  /* 368..375 */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 376..383 */
  0x73,0x01,0x00,0x70,0x61,0x64,0x64,0x69,  /* 384..391 */
  0x6E,0x67,0x00,0x0C,0x00,0x2C,0x00,0x48,  /* 392..399 */
  0x00,0x60,0x00,0x80,0x00,0xA0,0x00,0xBC,  /* 400..407 */
  0x00,0xD8,0x00,0xF0,0x00,0x0C,0x01,0x5C,  /* 408..415 */
  0x01,0x74,0x01,0x00,0x00,0x30,0x01,0x49,  /* 416..423 */
  0x01,0x00,0x00,                            /* 424..426 */
};

/* Component context — lives in the HAL ctxs pool */
struct sserial_host_ctx_t {
  int     fd;
  uint8_t rxbuf[256];
  int     rxlen;
  uint16_t address;
};

/* ---- helpers ------------------------------------------------------------- */

static uint8_t crc_of(const void *data, int n) {
  crc8_t c = crc8_init();
  c        = crc8_update(c, data, n);
  return (uint8_t)crc8_finalize(c);
}

static void write_bytes(int fd, const void *buf, int n) {
  const uint8_t *p = (const uint8_t *)buf;
  while(n > 0) {
    ssize_t w = write(fd, p, n);
    if(w <= 0) return;
    p += w;
    n -= w;
  }
}

static void send_with_crc(int fd, const void *data, int n) {
  uint8_t crc = crc_of(data, n);
  write_bytes(fd, data, n);
  write_bytes(fd, &crc, 1);
}

/* ---- LBP slave state machine -------------------------------------------- */

/*
 * Process exactly one LBP command from ctx->rxbuf[0..rxlen-1].
 * Returns number of bytes consumed (> 0), or 0 if more data is needed.
 */
static int process_cmd(struct sserial_host_ctx_t *ctx,
                       struct sserial_host_pin_ctx_t *pins) {
  int avail = ctx->rxlen;
  if(avail < 1) return 0;

  lbp_t lbp;
  lbp.byte = ctx->rxbuf[0];

  /* ---- CT_LOCAL read (wr=0, range 0xC0..0xDF) -------------------------- */
  if(lbp.ct == CT_LOCAL && lbp.wr == 0) {
    if(avail < 2) return 0;
    uint8_t resp;
    switch(lbp.byte) {
      case LBPCookieCMD:
        resp = LBPCookie;
        break;
      case LBPStatusCMD:
        resp = 0x00;
        break;
      case LBPCardName0Cmd:
      case LBPCardName1Cmd:
      case LBPCardName2Cmd:
      case LBPCardName3Cmd:
        resp = (uint8_t)card_name[lbp.byte - LBPCardName0Cmd];
        break;
      default:
        resp = 0x00;
        break;
    }
    send_with_crc(ctx->fd, &resp, 1);
    return 2;
  }

  /* ---- CT_LOCAL write — special reset bytes (no CRC follows) ----------- */
  if(lbp.byte == 0xFF) {
    /* Parser reset */
    return 1;
  }
  if(lbp.byte == 0xFC) {
    /* Reserved, no CRC */
    return 1;
  }

  /* ---- CT_LOCAL write (wr=1, range 0xE0..0xFF, excluding special) ------- */
  if(lbp.ct == CT_LOCAL && lbp.wr == 1) {
    if(avail < 3) return 0;
    /* Spec deviation (matches sserial.c): reply 0x00 without CRC */
    uint8_t r = 0x00;
    write_bytes(ctx->fd, &r, 1);
    return 3;
  }

  /* ---- CT_RPC ----------------------------------------------------------- */
  if(lbp.ct == CT_RPC) {
    if(lbp.byte == UnitNumberRPC) {
      if(avail < 2) return 0;
      uint32_t unit = HOST_UNIT_NUMBER;
      send_with_crc(ctx->fd, &unit, 4);
      return 2;
    }

    if(lbp.byte == DiscoveryRPC) {
      if(avail < 2) return 0;
      send_with_crc(ctx->fd, &host_disc, (int)sizeof(host_disc));
      return 2;
    }

    if(lbp.byte == ProcessDataRPC) {
      /* Need: cmd + output bytes + CRC = 1 + host_disc.output + 1 */
      int need = 1 + host_disc.output + 1;
      if(avail < need) return 0;
      /* Only process when the test has armed this exchange (pd_arm=1).
         This prevents stale frt cycles from the previous step consuming
         the command before pin assignments in the pipe are processed. */
      if(PIN(pd_arm) < 0.5f) return 0;
      /* Disarm immediately so residual frt calls cannot consume pd_cmd again. */
      pins->pd_arm.value  = 0.0f;
      pins->pd_arm.source = &pins->pd_arm;

      sserial_out_t out;
      memcpy(&out, ctx->rxbuf + 1, host_disc.output);

      /* Update HAL output pins */
      PIN(pos_cmd) = out.pos_cmd;
      PIN(vel_cmd) = out.vel_cmd;
      PIN(enable)  = (float)out.enable;
      PIN(out0)    = (float)out.out_0;
      PIN(out1)    = (float)out.out_1;
      PIN(out2)    = (float)out.out_2;
      PIN(out3)    = (float)out.out_3;

      /* Build response from HAL input pins */
      sserial_in_t in;
      memset(&in, 0, sizeof(in));
      in.pos_fb  = PIN(pos_fb);
      in.vel_fb  = PIN(vel_fb);
      float curr = PIN(current) / (30.0f / 128.0f);
      if(curr >  127.0f) curr =  127.0f;
      if(curr < -127.0f) curr = -127.0f;
      in.current = (int8_t)curr;
      in.in_0    = (PIN(in0) > 0.0f) ? 1 : 0;
      in.in_1    = (PIN(in1) > 0.0f) ? 1 : 0;
      in.in_2    = (PIN(in2) > 0.0f) ? 1 : 0;
      in.in_3    = (PIN(in3) > 0.0f) ? 1 : 0;
      in.fault   = (PIN(fault) > 0.0f) ? 1 : 0;

      /* Send: [fault=0x00][in_data][CRC over (host_disc.input bytes)] */
      uint8_t txbuf[host_disc.input + 1];
      txbuf[0] = 0x00;  /* fault byte */
      memcpy(txbuf + 1, &in, sizeof(in));
      txbuf[host_disc.input] = crc_of(txbuf, host_disc.input);
      write_bytes(ctx->fd, txbuf, host_disc.input + 1);

      PIN(connected) = 1.0f;
      return need;
    }

    /* Unknown RPC — skip cmd+crc */
    return (avail >= 2) ? 2 : 0;
  }

  /* ---- CT_RW read (wr=0) ----------------------------------------------- */
  if(lbp.ct == CT_RW && lbp.wr == 0) {
    int need = 2 + 2 * lbp.as;  /* 2 or 4 bytes */
    if(avail < need) return 0;
    if(lbp.as) {
      ctx->address = (uint16_t)(ctx->rxbuf[1] | (ctx->rxbuf[2] << 8));
    }
    int data_len = 1 << lbp.ds;
    if(ctx->address + data_len <= (int)sizeof(lbp_mem)) {
      uint8_t tmp[data_len + 1];
      memcpy(tmp, lbp_mem + ctx->address, data_len);
      tmp[data_len] = crc_of(tmp, data_len);
      write_bytes(ctx->fd, tmp, data_len + 1);
    }
    if(lbp.ai) ctx->address += (uint16_t)data_len;
    return need;
  }

  /* ---- CT_RW write (wr=1) —  no response (STMBL deviation) ------------ */
  if(lbp.ct == CT_RW && lbp.wr == 1) {
    int need = 2 + 2 * lbp.as + (1 << lbp.ds);  /* cmd+[addr]+data+crc */
    if(avail < need) return 0;
    if(lbp.as) {
      ctx->address = (uint16_t)(ctx->rxbuf[1] | (ctx->rxbuf[2] << 8));
    }
    if(lbp.ai) ctx->address += (uint16_t)(1 << lbp.ds);
    return need;
  }

  /* Unknown command — skip one byte */
  return 1;
}

/* ---- HAL callbacks ------------------------------------------------------- */

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct sserial_host_ctx_t *ctx = (struct sserial_host_ctx_t *)ctx_ptr;
  (void)pin_ptr;

  ctx->fd      = -1;
  ctx->rxlen   = 0;
  ctx->address = 0;
  memset(ctx->rxbuf, 0, sizeof(ctx->rxbuf));

  const char *fd_str = getenv("SSERIAL_HOST_FD");
  if(!fd_str) return;

  ctx->fd = atoi(fd_str);
  if(ctx->fd < 0) return;

  int flags = fcntl(ctx->fd, F_GETFL, 0);
  if(flags >= 0) fcntl(ctx->fd, F_SETFL, flags | O_NONBLOCK);

  fprintf(stderr, "sserial_host: using fd %d\n", ctx->fd);
}

static void frt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct sserial_host_ctx_t *ctx      = (struct sserial_host_ctx_t *)ctx_ptr;
  struct sserial_host_pin_ctx_t *pins = (struct sserial_host_pin_ctx_t *)pin_ptr;
  (void)period;

  if(ctx->fd < 0) return;

  /* Drain available bytes into rxbuf */
  int space = (int)sizeof(ctx->rxbuf) - ctx->rxlen;
  if(space > 0) {
    ssize_t n = read(ctx->fd, ctx->rxbuf + ctx->rxlen, (size_t)space);
    if(n > 0) ctx->rxlen += (int)n;
  }

  /* Process one command per FRT call */
  int consumed = process_cmd(ctx, pins);
  if(consumed > 0 && consumed <= ctx->rxlen) {
    ctx->rxlen -= consumed;
    memmove(ctx->rxbuf, ctx->rxbuf + consumed, (size_t)ctx->rxlen);
  }
}

const hal_comp_t sserial_host_comp_struct = {
  .name      = "sserial_host",
  .nrt       = 0,
  .rt        = 0,
  .frt       = frt_func,
  .nrt_init  = nrt_init,
  .hw_init   = 0,
  .rt_start  = 0,
  .frt_start = 0,
  .rt_stop   = 0,
  .frt_stop  = 0,
  .ctx_size  = sizeof(struct sserial_host_ctx_t),
  .pin_count = sizeof(struct sserial_host_pin_ctx_t) / sizeof(hal_pin_inst_t),
};
