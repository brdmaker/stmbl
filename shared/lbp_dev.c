/*
 * A smart-serial device, byte by byte; see lbp_dev.h.
 */
#include "lbp_dev.h"
#include "crc8.h"
#include <string.h>

#define CT_RW 1
#define CT_RPC 2
#define CT_LOCAL 3
#define RPC_UNIT 0xbc
#define RPC_DISCOVERY 0xbb
#define RPC_PROCESS 0xbd
#define LOCAL_STATUS 0xc1
#define LOCAL_NAME0 0xd0

// input 1 (the fault byte), output 0, ptocp = gtocp = 8: an empty list
const uint8_t lbpd_empty_mem[10] = {1, 0, 8, 0, 8, 0, 0, 0, 0, 0};

void lbpd_init(lbpd_t *d) {
  d->len  = 0;
  d->addr = 0;
  d->cmds = d->crc_errors = d->good = 0;
}

void lbpd_idle(lbpd_t *d) {
  d->len = 0;
}

static uint8_t crc(const uint8_t *p, int n) {
  return (uint8_t)crc8_finalize(crc8_update(crc8_init(), p, n));
}

static void reply(lbpd_t *d, const uint8_t *p, int n, int with_crc) {
  uint8_t out[24];
  memcpy(out, p, n);
  if(with_crc) {
    out[n] = crc(p, n);
  }
  d->send(d->ctx, out, n + (with_crc ? 1 : 0));
}

// bytes a request takes, from its first byte (0: not known yet)
static int need(const lbpd_t *d) {
  uint8_t c = d->buf[0];
  int ct = c >> 6, wr = (c >> 5) & 1, as = (c >> 2) & 1, ds = c & 3;
  if(ct == CT_LOCAL) {
    if(wr) {
      return (c == 0xff || c == 0xfc) ? 1 : 3;
    }
    return 2;
  }
  if(ct == CT_RPC) {
    if(c == RPC_PROCESS) {
      return 1 + (d->mem_size > 1 ? d->mem[1] : 0) + 1;
    }
    return 2;
  }
  if(ct == CT_RW) {
    return 1 + 2 * as + (wr ? (1 << ds) : 0) + 1;
  }
  return 1;  // not a command: dropped
}

static void mem_read(lbpd_t *d, uint16_t a, uint8_t *out, int n) {
  if(d->flash && lbpf_mem_read(d->flash, a, out, n)) {
    return;
  }
  for(int i = 0; i < n; i++) {
    out[i] = (uint32_t)(a + i) < d->mem_size ? d->mem[a + i] : 0;
  }
}

static void handle(lbpd_t *d, int n) {
  uint8_t c = d->buf[0], r[16];
  int ct = c >> 6, wr = (c >> 5) & 1, as = (c >> 2) & 1, ai = (c >> 3) & 1, ds = c & 3;
  d->cmds++;
  if(n > 1 && crc(d->buf, n - 1) != d->buf[n - 1]) {
    d->crc_errors++;  // no reply: hostmot2 retries or reports
    return;
  }
  if(ct != 0 && n > 1) {
    d->good++;
  }
  if(ct == CT_LOCAL) {
    if(wr) {
      if(n == 3) {
        if(d->flash) {
          lbpf_local_write(d->flash, c, d->buf[1]);
        }
        reply(d, r, 0, 1);  // SSLBP waits for the CRC of no data: one 0x00
      }
      return;
    }
    r[0] = 0;
    if(d->flash && lbpf_local_read(d->flash, c, r)) {
    } else if(c >= LOCAL_NAME0 && c < LOCAL_NAME0 + 4) {
      r[0] = (uint8_t)d->name[c - LOCAL_NAME0];
    } else if(c == LBPF_COOKIE_CMD) {
      r[0] = LBPF_COOKIE;
    }
    reply(d, r, 1, 1);
    return;
  }
  if(ct == CT_RPC) {
    if(c == RPC_UNIT) {
      memcpy(r, &d->unit, 4);
      reply(d, r, 4, 1);
    } else if(c == RPC_DISCOVERY) {
      reply(d, d->mem, 6, 1);
    } else if(c == RPC_PROCESS) {
      int in = d->mem[0];
      memset(r, 0, in);  // the fault byte (and no process data)
      reply(d, r, in, 1);
    }
    return;
  }
  if(ct == CT_RW) {
    if(as) {
      d->addr = (uint16_t)(d->buf[1] | d->buf[2] << 8);
    }
    int nb = 1 << ds;
    if(wr) {
      const uint8_t *data = d->buf + 1 + 2 * as;
      if(!(d->flash && lbpf_mem_write(d->flash, d->addr, data, nb))) {
        // the tables are read only here
      }
      // SSLBP waits for a reply to every write: the CRC of no data, one 0x00
      // (SSLBP.MAC sendLBPstring, SSLBP.SUB recvstring); without it each
      // write timed out after 50 ms and set the error flag
      reply(d, r, 0, 1);
    } else {
      mem_read(d, d->addr, r, nb);
      reply(d, r, nb, 1);
    }
    if(ai) {
      d->addr += (uint16_t)nb;
    }
  }
}

void lbpd_byte(lbpd_t *d, uint8_t b) {
  if(d->len >= (int)sizeof(d->buf)) {
    d->len = 0;
  }
  d->buf[d->len++] = b;
  int n = need(d);
  if(d->len >= n) {
    handle(d, n);
    d->len = 0;
  }
}
