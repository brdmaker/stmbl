/*
 * Simulated MCP2515 and CAN bus: see mcp2515_sim.h.
 */
#include "mcp2515_sim.h"
#include <string.h>

// register map
#define TXB_SIDH(n) (0x31 + 0x10 * (n))
#define RXB_SIDH(n) (0x61 + 0x10 * (n))
#define CANINTF 0x2C
#define EFLG 0x2D
#define TEC 0x1C

double sim_frame_us(int dlc) {
  int stuffed = 34 + 8 * dlc;
  return 47 + 8 * dlc + (stuffed - 1) / 4;  // bits = us at 1 Mbit/s
}

void sim_chip_init(sim_chip_t *c, sim_bus_t *bus) {
  memset(c, 0, sizeof(*c));
  c->bus     = bus;
  c->present = 1;
  bus->chip[bus->nchip++] = c;
}

static void reset(sim_chip_t *c) {
  memset(c->reg, 0, sizeof(c->reg));
  c->reg[0x0F] = 0x87;  // CANCTRL: REQOP configuration
  c->reg[0x0E] = 0x80;  // CANSTAT: configuration mode
}

static int normal(const sim_chip_t *c) {
  return c->present && (c->reg[0x0E] & 0xE0) == 0x00;
}

static void reg_write(sim_chip_t *c, uint8_t a, uint8_t v) {
  a &= 0x7F;
  c->reg[a] = v;
  if(a == 0x0F) {  // CANCTRL: the mode follows the request at once here
    c->reg[0x0E] = (uint8_t)((c->reg[0x0E] & 0x1F) | (v & 0xE0));
  }
}

int sim_xfer(void *ctx, mcp_seg_t *segs, int n) {
  sim_chip_t *c = ctx;
  for(int s = 0; s < n; s++) {
    const uint8_t *tx = segs[s].tx;
    uint8_t *rx       = segs[s].rx;
    int len           = segs[s].len;
    uint8_t out[64];
    memset(out, 0xff, sizeof(out));
    if(c->present && len > 0) {
      uint8_t ins = tx[0];
      if(ins == MCP_RESET) {
        reset(c);
      } else if(ins == MCP_READ && len >= 2) {
        for(int i = 2; i < len; i++) {
          out[i] = c->reg[(tx[1] + i - 2) & 0x7F];
        }
      } else if(ins == MCP_WRITE && len >= 2) {
        for(int i = 2; i < len; i++) {
          reg_write(c, (uint8_t)(tx[1] + i - 2), tx[i]);
        }
      } else if(ins == MCP_BITMOD && len == 4) {
        uint8_t a = tx[1] & 0x7F;
        reg_write(c, a, (uint8_t)((c->reg[a] & ~tx[2]) | (tx[3] & tx[2])));
      } else if(ins == MCP_READ_STATUS) {
        uint8_t st = 0, f = c->reg[CANINTF];
        st |= f & 0x01 ? 0x01 : 0;
        st |= f & 0x02 ? 0x02 : 0;
        for(int b = 0; b < 3; b++) {
          st |= (c->reg[0x30 + 0x10 * b] & 0x08) ? (uint8_t)(0x04 << (2 * b)) : 0;
          st |= (f & (0x04 << b)) ? (uint8_t)(0x08 << (2 * b)) : 0;
        }
        for(int i = 1; i < len; i++) {
          out[i] = st;
        }
      } else if((ins & 0xF9) == 0x90) {  // READ RX BUFFER 0x90 / 0x92 / 0x94 / 0x96
        int b      = (ins >> 2) & 1;
        uint8_t a0 = (uint8_t)(RXB_SIDH(b) + ((ins & 2) ? 5 : 0));
        for(int i = 1; i < len; i++) {
          out[i] = c->reg[(a0 + i - 1) & 0x7F];
        }
        c->reg[CANINTF] &= (uint8_t)~(b ? 0x02 : 0x01);  // cleared at chip-select release
      } else if((ins & 0xF8) == 0x40) {  // LOAD TX BUFFER 0x40..0x45
        int b      = (ins >> 1) & 3;
        uint8_t a0 = (uint8_t)(TXB_SIDH(b) + ((ins & 1) ? 5 : 0));
        if(b < 3) {
          for(int i = 1; i < len; i++) {
            c->reg[(a0 + i - 1) & 0x7F] = tx[i];
          }
        }
      } else if((ins & 0xF8) == 0x80) {  // RTS
        for(int b = 0; b < 3; b++) {
          if(ins & (1 << b)) {
            c->reg[0x30 + 0x10 * b] |= 0x08;
          }
        }
      }
    }
    if(rx) {
      memcpy(rx, out, (size_t)len);
    }
  }
  return 0;
}

void sim_node_send(sim_node_t *n, const can_frame_t *f) {
  if(n->ntx < 16) {
    n->txq[n->ntx++] = *f;
  }
}

// the acceptance filters: does buffer b take identifier id?
static int accepts(const sim_chip_t *c, int b, uint16_t id) {
  uint8_t ctrl = c->reg[b ? 0x70 : 0x60];
  if(((ctrl >> 5) & 3) == 3) {
    return 1;  // RXM 11: filters off
  }
  uint8_t mreg  = b ? 0x24 : 0x20;
  uint16_t mask = (uint16_t)((c->reg[mreg] << 3) | (c->reg[mreg + 1] >> 5));
  static const uint8_t filt[6] = {0x00, 0x04, 0x08, 0x10, 0x14, 0x18};
  for(int f = b ? 2 : 0; f < (b ? 6 : 2); f++) {
    uint16_t fid = (uint16_t)((c->reg[filt[f]] << 3) | (c->reg[filt[f] + 1] >> 5));
    if((id & mask) == (fid & mask)) {
      return 1;
    }
  }
  return 0;
}

static void chip_receive(sim_chip_t *c, const can_frame_t *f) {
  int b;
  int to0 = accepts(c, 0, f->id), to1 = accepts(c, 1, f->id);
  int full0 = c->reg[CANINTF] & 0x01, full1 = c->reg[CANINTF] & 0x02;
  if(!to0 && !to1) {
    return;  // filtered out: never reaches a buffer
  }
  if(to0 && !full0) {
    b = 0;
  } else if(to0 && (c->reg[0x60] & 0x04) && !full1) {  // RXB0 full, BUKT: roll over
    b = 1;
  } else if(!to0 && to1 && !full1) {
    b = 1;
  } else {
    c->reg[EFLG] |= to0 ? 0x40 : 0x80;  // RX0OVR / RX1OVR: lost
    return;
  }
  uint8_t *r = &c->reg[RXB_SIDH(b)];
  r[0]       = (uint8_t)(f->id >> 3);
  r[1]       = (uint8_t)((f->id & 7) << 5);
  r[2] = r[3] = 0;
  r[4]        = f->dlc;
  memcpy(r + 5, f->d, 8);
  c->reg[CANINTF] |= (uint8_t)(b ? 0x02 : 0x01);
}

// the highest priority pending buffer of a chip: TXP, then the buffer number
static int chip_pending(const sim_chip_t *c) {
  int best = -1, bp = -1;
  for(int b = 2; b >= 0; b--) {
    uint8_t ctrl = c->reg[0x30 + 0x10 * b];
    if((ctrl & 0x08) && (int)(ctrl & 3) > bp) {
      best = b;
      bp   = ctrl & 3;
    }
  }
  return best;
}

static void chip_frame(const sim_chip_t *c, int b, can_frame_t *f) {
  const uint8_t *r = &c->reg[TXB_SIDH(b)];
  f->id            = (uint16_t)((r[0] << 3) | (r[1] >> 5));
  f->dlc           = r[4] & 0x0F;
  memcpy(f->d, r + 5, 8);
}

double sim_bus_run(sim_bus_t *bus, double us) {
  double t = 0;
  for(;;) {
    // arbitration: every participant offers its next frame, the lowest ID wins
    int win_chip = -1, win_buf = -1, win_node = -1;
    can_frame_t win, f;
    for(int i = 0; i < bus->nchip; i++) {
      if(!normal(bus->chip[i])) {
        continue;
      }
      int b = chip_pending(bus->chip[i]);
      if(b < 0) {
        continue;
      }
      chip_frame(bus->chip[i], b, &f);
      if(win_chip < 0 && win_node < 0 ? 1 : f.id < win.id) {
        win = f, win_chip = i, win_buf = b, win_node = -1;
      }
    }
    for(int i = 0; i < bus->nnode; i++) {
      if(bus->node[i]->ntx == 0) {
        continue;
      }
      f = bus->node[i]->txq[0];
      if(win_chip < 0 && win_node < 0 ? 1 : f.id < win.id) {
        win = f, win_node = i, win_chip = -1;
      }
    }
    if(win_chip < 0 && win_node < 0) {
      break;
    }
    double ft = sim_frame_us(win.dlc);
    if(t + ft > us) {
      break;
    }
    t += ft;
    bus->busy_us += ft;
    // ACK: any other participant listening
    int others = bus->nnode - (win_node >= 0 ? 1 : 0);
    for(int i = 0; i < bus->nchip; i++) {
      others += (i != win_chip && normal(bus->chip[i]));
    }
    if(!others) {  // nobody acknowledges: error frame, retried (the chip's TEC grows)
      bus->unacked++;
      if(win_chip >= 0 && bus->chip[win_chip]->reg[TEC] < 255) {
        bus->chip[win_chip]->reg[TEC]++;
      }
      continue;
    }
    bus->frames++;
    for(int i = 0; i < bus->nchip; i++) {
      if(i != win_chip && normal(bus->chip[i])) {
        chip_receive(bus->chip[i], &win);
      }
    }
    if(win_chip >= 0) {
      bus->chip[win_chip]->reg[0x30 + 0x10 * win_buf] &= (uint8_t)~0x08;  // sent
      bus->chip[win_chip]->reg[CANINTF] |= (uint8_t)(0x04 << win_buf);
    } else {
      sim_node_t *n = bus->node[win_node];
      memmove(n->txq, n->txq + 1, sizeof(can_frame_t) * (size_t)(--n->ntx));
    }
    for(int i = 0; i < bus->nnode; i++) {
      if(i != win_node && bus->node[i]->rx) {
        bus->node[i]->rx(bus->node[i], &win);
      }
    }
  }
  return t;
}
