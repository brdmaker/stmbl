/*
 * lbp_client.c — LBP host-side client (Mesa Smart Serial Local Bus Protocol).
 *
 * Implements the host (master) role: builds command frames, appends CRC-8/MAXIM,
 * writes them to a file descriptor, and reads back slave replies with a
 * configurable per-byte timeout.
 */
#include "lbp_client.h"
#include "crc8.h"

#include <string.h>
#include <unistd.h>
#include <sys/select.h>

/* ---- internal helpers --------------------------------------------------- */

static void advance(lbp_conn_t *c) {
  if(c->advance_fn) c->advance_fn(c->advance_arg);
}

static uint8_t compute_crc(const void *data, int n) {
  crc8_t c = crc8_init();
  c        = crc8_update(c, data, n);
  return (uint8_t)crc8_finalize(c);
}

static int send_all(int fd, const void *buf, int n) {
  const uint8_t *p = (const uint8_t *)buf;
  while(n > 0) {
    ssize_t w = write(fd, p, n);
    if(w <= 0) return 0;
    p += w;
    n -= w;
  }
  return 1;
}

/* Read exactly n bytes, re-calling select() for each chunk to apply the
   per-read timeout independently. */
static int recv_exact(lbp_conn_t *c, void *buf, int n) {
  uint8_t *p   = (uint8_t *)buf;
  int remaining = n;
  while(remaining > 0) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(c->fd, &rfds);
    struct timeval tv = {
      .tv_sec  = c->timeout_ms / 1000,
      .tv_usec = (c->timeout_ms % 1000) * 1000,
    };
    int r = select(c->fd + 1, &rfds, NULL, NULL, &tv);
    if(r <= 0) return 0;
    ssize_t got = read(c->fd, p, remaining);
    if(got <= 0) return 0;
    p += got;
    remaining -= (int)got;
  }
  return 1;
}

/* Send a local-read command (CT_LOCAL, wr=0): [cmd][CRC] → [byte][CRC]. */
static int local_read_cmd(lbp_conn_t *c, uint8_t cmd, uint8_t *result) {
  uint8_t tbuf[2];
  tbuf[0] = cmd;
  tbuf[1] = compute_crc(tbuf, 1);
  if(!send_all(c->fd, tbuf, 2)) return 0;
  advance(c);

  uint8_t rbuf[2];
  if(!recv_exact(c, rbuf, 2)) return 0;
  if(compute_crc(rbuf, 1) != rbuf[1]) return 0;
  *result = rbuf[0];
  return 1;
}

/* Issue an RPC that carries no output data: [rpc][CRC] → [reply_len bytes][CRC]. */
static int simple_rpc(lbp_conn_t *c, uint8_t rpc, void *reply, int reply_len) {
  uint8_t tbuf[2];
  tbuf[0] = rpc;
  tbuf[1] = compute_crc(tbuf, 1);
  if(!send_all(c->fd, tbuf, 2)) return 0;
  advance(c);

  uint8_t rbuf[reply_len + 1];
  if(!recv_exact(c, rbuf, reply_len + 1)) return 0;
  if(compute_crc(rbuf, reply_len) != rbuf[reply_len]) return 0;
  memcpy(reply, rbuf, reply_len);
  return 1;
}

/* ---- public API --------------------------------------------------------- */

void lbp_conn_init(lbp_conn_t *c, int fd, int timeout_ms) {
  c->fd          = fd;
  c->timeout_ms  = timeout_ms;
  c->advance_fn  = NULL;
  c->advance_arg = NULL;
}

int lbp_check_cookie(lbp_conn_t *c) {
  uint8_t resp;
  return local_read_cmd(c, LBPCookieCMD, &resp) && (resp == LBPCookie);
}

int lbp_read_card_name(lbp_conn_t *c, char out[4]) {
  for(int i = 0; i < 4; i++) {
    uint8_t ch;
    if(!local_read_cmd(c, (uint8_t)(LBPCardName0Cmd + i), &ch)) return 0;
    out[i] = (char)ch;
  }
  return 1;
}

int lbp_read_unit_number(lbp_conn_t *c, uint32_t *unit) {
  return simple_rpc(c, UnitNumberRPC, unit, 4);
}

int lbp_discover(lbp_conn_t *c, discovery_rpc_t *disc) {
  return simple_rpc(c, DiscoveryRPC, disc, (int)sizeof(*disc));
}

int lbp_read_mem(lbp_conn_t *c, uint16_t addr, uint8_t ds, void *data) {
  /* cmd: ct=CT_RW(01b), wr=0, as=1 (explicit address), ai=0, ds=ds */
  lbp_t cmd;
  cmd.byte = 0;
  cmd.ct   = CT_RW;
  cmd.wr   = 0;
  cmd.as   = 1;
  cmd.ai   = 0;
  cmd.ds   = ds;

  uint8_t tbuf[4];
  tbuf[0] = cmd.byte;
  tbuf[1] = (uint8_t)(addr & 0xFF);
  tbuf[2] = (uint8_t)(addr >> 8);
  tbuf[3] = compute_crc(tbuf, 3);
  if(!send_all(c->fd, tbuf, 4)) return 0;
  advance(c);

  int data_len = 1 << ds;
  uint8_t rbuf[data_len + 1];
  if(!recv_exact(c, rbuf, data_len + 1)) return 0;
  if(compute_crc(rbuf, data_len) != rbuf[data_len]) return 0;
  memcpy(data, rbuf, data_len);
  return 1;
}

int lbp_process_data(lbp_conn_t *c, const discovery_rpc_t *disc,
                     const void *out_data, uint8_t *fault_out, void *in_data) {
  /* Command: [ProcessDataRPC] [output bytes] [CRC over cmd+output] */
  int cmd_len = 1 + disc->output;
  uint8_t tbuf[cmd_len + 1];
  tbuf[0] = ProcessDataRPC;
  memcpy(tbuf + 1, out_data, disc->output);
  tbuf[cmd_len] = compute_crc(tbuf, cmd_len);
  if(!send_all(c->fd, tbuf, cmd_len + 1)) return 0;
  advance(c);

  /* Reply: [fault byte] [disc->input-1 data bytes] [CRC over disc->input bytes] */
  int reply_total = disc->input + 1;
  uint8_t rbuf[reply_total];
  if(!recv_exact(c, rbuf, reply_total)) return 0;
  if(compute_crc(rbuf, disc->input) != rbuf[disc->input]) return 0;
  if(fault_out) *fault_out = rbuf[0];
  if(in_data) memcpy(in_data, rbuf + 1, disc->input - 1);
  return 1;
}
