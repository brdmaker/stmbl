/*
 * MCP2515 over SPI, polled: see mcp2515.h.
 */
#include "mcp2515.h"

static int xfer(mcp2515_t *m, mcp_seg_t *s, int n) {
  m->calls++;
  for(int i = 0; i < n; i++) {
    m->bytes += s[i].len;
  }
  return m->xfer(m->ctx, s, n);
}

static int write_reg(mcp2515_t *m, uint8_t addr, uint8_t v) {
  uint8_t tx[3] = {MCP_WRITE, addr, v};
  mcp_seg_t s   = {tx, 0, 3};
  return xfer(m, &s, 1);
}

static int read_reg(mcp2515_t *m, uint8_t addr, uint8_t *v) {
  uint8_t tx[3] = {MCP_READ, addr, 0}, rx[3] = {0};
  mcp_seg_t s   = {tx, rx, 3};
  int r         = xfer(m, &s, 1);
  *v            = rx[2];
  return r;
}

int mcp2515_timing(uint32_t osc_hz, uint32_t bitrate, uint8_t cnf[3]) {
  // bit = nq time quanta of 2 * brp oscillator periods each
  for(uint32_t nq = 16; nq >= 8; nq--) {
    uint32_t div = 2u * bitrate * nq;
    if(osc_hz % div) {
      continue;
    }
    uint32_t brp = osc_hz / div;  // 1..64
    if(brp < 1 || brp > 64) {
      continue;
    }
    uint32_t before = (nq * 3 + 2) / 4;  // sync + prop + ps1, sample at ~75 %
    uint32_t ps2    = nq - before;       // 2..8
    uint32_t rest   = before - 1;        // prop + ps1
    uint32_t prop   = rest / 2, ps1 = rest - prop;  // PS1 >= PRSEG
    if(ps2 < 2 || ps2 > 8 || ps1 < 1 || ps1 > 8 || prop < 1 || prop > 8) {
      continue;
    }
    cnf[0] = (uint8_t)(brp - 1);                                      // CNF1: SJW 1, BRP
    cnf[1] = (uint8_t)(0x80 | ((ps1 - 1) << 3) | (prop - 1));         // CNF2: BTLMODE, PHSEG1, PRSEG
    cnf[2] = (uint8_t)(ps2 - 1);                                      // CNF3: PHSEG2
    return 0;
  }
  return -1;
}

int mcp2515_init(mcp2515_t *m, uint32_t osc_hz, uint32_t bitrate, const uint16_t *ids, int nids) {
  uint8_t cnf[3], v;
  if(mcp2515_timing(osc_hz, bitrate, cnf)) {
    return -1;
  }
  uint8_t rst = MCP_RESET;
  mcp_seg_t s = {&rst, 0, 1};
  if(xfer(m, &s, 1)) {
    return -2;
  }
  // after reset: configuration mode (OPMOD 100)
  if(read_reg(m, MCP_CANSTAT, &v) || (v & 0xE0) != 0x80) {
    return -3;  // no chip, or not an MCP2515
  }
  write_reg(m, MCP_CNF1, cnf[0]);
  write_reg(m, MCP_CNF2, cnf[1]);
  write_reg(m, MCP_CNF3, cnf[2]);
  if(nids > 0) {
    // acceptance filters (configuration mode only): RXB0's two filters take
    // the expected identifiers, so with rollover both buffers are theirs;
    // RXB1's four filters repeat them (it must not take anything else)
    static const uint8_t filt[6] = {0x00, 0x04, 0x08, 0x10, 0x14, 0x18};
    write_reg(m, 0x20, 0xFF);  // RXM0: all 11 bits compared
    write_reg(m, 0x21, 0xE0);
    write_reg(m, 0x24, 0xFF);  // RXM1
    write_reg(m, 0x25, 0xE0);
    for(int f = 0; f < 6; f++) {
      uint16_t id = ids[(f < 2 ? f : f - 2) % nids];
      write_reg(m, filt[f], (uint8_t)(id >> 3));
      write_reg(m, (uint8_t)(filt[f] + 1), (uint8_t)((id & 7) << 5));  // EXIDE 0: standard frames
    }
    write_reg(m, MCP_RXB0CTRL, 0x04);  // RXM 00: the filters; BUKT: roll over into RXB1
    write_reg(m, MCP_RXB1CTRL, 0x00);
  } else {
    write_reg(m, MCP_RXB0CTRL, 0x64);  // RXM 11: any frame; BUKT
    write_reg(m, MCP_RXB1CTRL, 0x60);
  }
  write_reg(m, MCP_TXBCTRL(0), 3);   // TXP: TXB0 first, then TXB1, TXB2
  write_reg(m, MCP_TXBCTRL(1), 2);
  write_reg(m, MCP_TXBCTRL(2), 1);
  write_reg(m, MCP_CANINTE, 0x03);   // RX0IE, RX1IE: the INT pin shows received frames
  write_reg(m, MCP_CANINTF, 0x00);
  uint8_t back[3];
  read_reg(m, MCP_CNF1, &back[0]);
  read_reg(m, MCP_CNF2, &back[1]);
  read_reg(m, MCP_CNF3, &back[2]);
  if(back[0] != cnf[0] || back[1] != cnf[1] || back[2] != cnf[2]) {
    return -4;  // SPI not reliable (clock, wiring)
  }
  write_reg(m, MCP_CANCTRL, 0x00);  // normal mode, no one-shot, CLKOUT off
  if(read_reg(m, MCP_CANSTAT, &v) || (v & 0xE0) != 0x00) {
    return -5;  // did not leave configuration mode
  }
  return 0;
}

static void frame_from(can_frame_t *f, const uint8_t *b) {
  // b: SIDH SIDL EID8 EID0 DLC D0..D7 (standard identifiers only)
  f->id  = (uint16_t)((b[0] << 3) | (b[1] >> 5));
  f->dlc = b[4] & 0x0F;
  if(f->dlc > 8) {
    f->dlc = 8;
  }
  for(int i = 0; i < 8; i++) {
    f->d[i] = b[5 + i];
  }
}

static void frame_to(uint8_t *b, const can_frame_t *f) {
  b[0] = (uint8_t)(f->id >> 3);
  b[1] = (uint8_t)((f->id & 7) << 5);
  b[2] = b[3] = 0;
  b[4]        = f->dlc & 0x0F;
  for(int i = 0; i < 8; i++) {
    b[5 + i] = i < f->dlc ? f->d[i] : 0;
  }
}

int mcp2515_read(mcp2515_t *m, can_frame_t *rx) {
  // 1. which buffers are full / still pending
  uint8_t st_tx[2] = {MCP_READ_STATUS, 0}, st_rx[2] = {0};
  mcp_seg_t s0     = {st_tx, st_rx, 2};
  if(xfer(m, &s0, 1)) {
    return -1;
  }
  m->status = st_rx[1];
  // 2. the full receive buffers, oldest first (RXB0, then the rollover RXB1)
  mcp_seg_t seg[2];
  uint8_t rxb_tx[2][14] = {{0}}, rxb_rx[2][14];
  int nrx = 0;
  for(int b = 0; b < 2; b++) {
    if(m->status & (b ? MCP_ST_RX1IF : MCP_ST_RX0IF)) {
      rxb_tx[nrx][0] = MCP_READ_RXB(b);
      seg[nrx]       = (mcp_seg_t){rxb_tx[nrx], rxb_rx[nrx], 14};
      nrx++;
    }
  }
  if(nrx && xfer(m, seg, nrx)) {
    return -1;
  }
  for(int i = 0; i < nrx; i++) {
    frame_from(&rx[i], rxb_rx[i] + 1);
  }
  m->rx_frames += (uint32_t)nrx;
  return nrx;
}

int mcp2515_write(mcp2515_t *m, const can_frame_t *tx, int ntx) {
  mcp_seg_t seg[3 + 3 + 1];
  uint8_t ab[3][4], ld[3][14];
  int n = 0;
  if(ntx > 3) {
    ntx = 3;
  }
  for(int b = 0; b < 3; b++) {
    if(m->status & MCP_ST_TXREQ(b)) {  // not sent in a whole period (no ACK, bus off, overload): abort
      ab[b][0] = MCP_BITMOD;
      ab[b][1] = MCP_TXBCTRL(b);
      ab[b][2] = 0x08;  // TXREQ
      ab[b][3] = 0x00;
      seg[n++] = (mcp_seg_t){ab[b], 0, 4};
      m->tx_late++;
    }
  }
  uint8_t rts = 0;
  for(int b = 0; b < ntx; b++) {
    ld[b][0] = MCP_LOAD_TXB(b);
    frame_to(ld[b] + 1, &tx[b]);
    seg[n++] = (mcp_seg_t){ld[b], 0, 14};
    rts |= (uint8_t)(1 << b);
  }
  uint8_t rts_cmd = MCP_RTS(rts);
  if(rts) {
    seg[n++] = (mcp_seg_t){&rts_cmd, 0, 1};
  }
  if(n && xfer(m, seg, n)) {
    return -1;
  }
  m->status &= (uint8_t)~(MCP_ST_TXREQ(0) | MCP_ST_TXREQ(1) | MCP_ST_TXREQ(2));
  m->tx_frames += (uint32_t)ntx;
  return 0;
}

int mcp2515_cycle(mcp2515_t *m, can_frame_t *rx, const can_frame_t *tx, int ntx) {
  int nrx = mcp2515_read(m, rx);
  if(nrx < 0 || mcp2515_write(m, tx, ntx) < 0) {
    return -1;
  }
  return nrx;
}

int mcp2515_errors(mcp2515_t *m) {
  uint8_t e, t, r;
  if(read_reg(m, MCP_EFLG, &e) || read_reg(m, MCP_TEC, &t) || read_reg(m, MCP_REC, &r)) {
    return -1;
  }
  m->eflg = e;
  m->tec  = t;
  m->rec  = r;
  if(e & MCP_EFLG_RX0OVR) {
    m->rx_overflow++;
  }
  if(e & MCP_EFLG_RX1OVR) {
    m->rx_overflow++;
  }
  if(e & (MCP_EFLG_RX0OVR | MCP_EFLG_RX1OVR)) {
    uint8_t tx[4] = {MCP_BITMOD, MCP_EFLG, MCP_EFLG_RX0OVR | MCP_EFLG_RX1OVR, 0};
    mcp_seg_t s   = {tx, 0, 4};
    xfer(m, &s, 1);
  }
  return 0;
}
