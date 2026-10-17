/*
 * One CAN bus of stmblcan: see stmblcan_bus.h.
 */
#include "stmblcan_bus.h"
#include <stdio.h>
#include <string.h>

int stmblcan_bus_add(stmblcan_bus_t *b, const candesc_t *d) {
  if(d->stmbl) {
    if(b->nnodes >= STMBLCAN_NODES) {
      return -1;
    }
    b->node[b->nnodes++] = (stmblcan_node_t){.node = d->node, .vel_scale = d->vel_scale};
    return 0;
  }
  if(b->ndevs >= STMBLCAN_DEVS) {
    return -1;
  }
  stmblcan_dev_t *v = &b->dev[b->ndevs++];
  memset(v, 0, sizeof(*v));
  v->desc = *d;
  return 0;
}

static int tx_due(const stmblcan_dev_t *v, int f, uint32_t period) {
  const candesc_frame_t *fr = &v->desc.frame[f];
  return fr->dir == CD_TX && (period + v->phase[f]) % fr->every == 0;
}

int stmblcan_bus_check(stmblcan_bus_t *b, char *err, int errlen) {
  // stagger the generic tx frames that are not sent every period
  int k = 0;
  for(int i = 0; i < b->ndevs; i++) {
    for(int f = 0; f < b->dev[i].desc.nframes; f++) {
      b->dev[i].phase[f] = (uint8_t)(b->dev[i].desc.frame[f].every > 1 ? k++ % b->dev[i].desc.frame[f].every : 0);
    }
  }
  int worst_tx = 0, worst_rx = 0;
  for(uint32_t p = 0; p < 2520; p++) {  // 2520: every period pattern of every 1..10 repeats
    int tx = b->nnodes + (b->nnodes ? 1 : 0), rx = b->nnodes;
    for(int i = 0; i < b->ndevs; i++) {
      for(int f = 0; f < b->dev[i].desc.nframes; f++) {
        const candesc_frame_t *fr = &b->dev[i].desc.frame[f];
        if(fr->dir == CD_TX) {
          tx += tx_due(&b->dev[i], f, p);
        } else {
          rx += (p % fr->every) == 0;  // the device's own rate: assume they meet
        }
      }
    }
    worst_tx = tx > worst_tx ? tx : worst_tx;
    worst_rx = rx > worst_rx ? rx : worst_rx;
  }
  if(worst_tx > 3) {
    snprintf(err, (size_t)errlen, "%d frames to send in one period: the MCP2515 has 3 transmit buffers (SYNC included)",
             worst_tx);
    return -1;
  }
  if(worst_rx > 2) {
    snprintf(err, (size_t)errlen, "%d frames to receive in one period: the MCP2515 has 2 receive buffers", worst_rx);
    return -1;
  }
  return 0;
}

// the identifiers to accept: the stmbl feedback, the generic rx frames
static int rx_ids(const stmblcan_bus_t *b, uint16_t *ids, int max) {
  int n = 0;
  for(int i = 0; i < b->nnodes && n < max; i++) {
    ids[n++] = (uint16_t)CANP_FB_ID(b->node[i].node);
  }
  for(int i = 0; i < b->ndevs; i++) {
    for(int f = 0; f < b->dev[i].desc.nframes && n < max; f++) {
      if(b->dev[i].desc.frame[f].dir == CD_RX) {
        ids[n++] = b->dev[i].desc.frame[f].id;
      }
    }
  }
  return n;
}

int stmblcan_bus_discover(stmblcan_bus_t *b, uint32_t osc_hz, uint32_t bitrate, int node, char *buf, int len,
                          void (*wait)(void *ctx, int us), void *ctx) {
  uint16_t id = (uint16_t)CANP_DESC_RSP_ID(node);
  if(mcp2515_init(&b->mcp, osc_hz, bitrate, &id, 1) < 0) {
    return -1;
  }
  int n = 0;
  for(uint8_t blk = 0; blk < 40; blk++) {
    can_frame_t rq = {.id = (uint16_t)CANP_DESC_REQ_ID(node), .dlc = 2, .d = {CANP_DESC_READ, blk}};
    can_frame_t rx[2];
    int got = 0;
    for(int tries = 0; tries < 3 && !got; tries++) {
      mcp2515_read(&b->mcp, rx);  // drop anything stale
      if(mcp2515_write(&b->mcp, &rq, 1) < 0) {
        return -1;
      }
      for(int t = 0; t < 10 && !got; t++) {  // up to 10 ms for an answer
        wait(ctx, 1000);
        int nr = mcp2515_read(&b->mcp, rx);
        for(int r = 0; r < nr; r++) {
          if(rx[r].id == id && rx[r].dlc == 8 && rx[r].d[0] == blk) {
            for(int i = 0; i < CANP_DESC_CHUNK && n < len - 1; i++) {
              buf[n++] = (char)rx[r].d[1 + i];
            }
            got = 1;
          }
        }
      }
    }
    if(!got) {
      buf[n < len ? n : len - 1] = 0;
      return -2;
    }
    if(memchr(buf + (n > CANP_DESC_CHUNK ? n - CANP_DESC_CHUNK : 0), 0, (size_t)(n > CANP_DESC_CHUNK ? CANP_DESC_CHUNK : n))) {
      return 0;  // the ending 0 came
    }
  }
  buf[len - 1] = 0;
  return 0;
}

int stmblcan_bus_init(stmblcan_bus_t *b, uint32_t osc_hz, uint32_t bitrate) {
  b->counter = 0;
  b->periods = 0;
  if(b->timeout == 0) {
    b->timeout = 10;
  }
  for(int i = 0; i < b->nnodes; i++) {
    stmblcan_node_t *n = &b->node[i];
    n->present         = 0;
    n->age             = b->timeout;
    sscon_reset(&n->con);
  }
  for(int i = 0; i < b->ndevs; i++) {
    b->dev[i].present = 0;
    b->dev[i].age     = b->timeout;
  }
  uint16_t ids[6];
  int nids = rx_ids(b, ids, 6);
  return mcp2515_init(&b->mcp, osc_hz, bitrate, ids, nids);
}

int stmblcan_bus_read(stmblcan_bus_t *b) {
  can_frame_t rx[2];
  int nrx = mcp2515_read(&b->mcp, rx);
  if(nrx < 0) {
    return nrx;
  }
  int got[STMBLCAN_NODES] = {0};
  for(int r = 0; r < nrx; r++) {
    int known = 0;
    for(int i = 0; i < b->nnodes; i++) {
      stmblcan_node_t *n = &b->node[i];
      // the feedback in the chip answers the last SYNC sent (b->counter)
      if(rx[r].id == CANP_FB_ID(n->node) && !canp_fb_decode(&rx[r], &n->fb, n->vel_scale, b->counter, n->slots)) {
        got[i] = known = 1;
        n->fb_frames++;
      }
    }
    for(int i = 0; i < b->ndevs && !known; i++) {
      stmblcan_dev_t *v = &b->dev[i];
      for(int f = 0; f < v->desc.nframes; f++) {
        const candesc_frame_t *fr = &v->desc.frame[f];
        if(fr->dir == CD_RX && rx[r].id == fr->id && rx[r].dlc >= fr->dlc) {
          candesc_decode(fr, &rx[r], v->val[f]);
          v->rx_frames++;
          v->age     = 0;
          v->present = 1;
          known      = 1;
        }
      }
    }
    if(!known) {
      b->foreign++;
    }
  }
  for(int i = 0; i < b->ndevs; i++) {
    stmblcan_dev_t *v = &b->dev[i];
    if(v->age > 0 || v->rx_frames == 0) {  // nothing from it this period
      if(++v->age >= b->timeout + 10) {      // (its frames may come every few periods)
        v->present = 0;
      }
    }
  }
  for(int i = 0; i < b->nnodes; i++) {
    stmblcan_node_t *n = &b->node[i];
    n->index_found = got[i] && n->sent_index && !n->fb.index;
    if(got[i]) {
      n->age     = 0;
      n->present = 1;
    } else {
      if(b->periods > 0 && n->present) {
        n->missed++;
      }
      if(++n->age >= b->timeout) {
        n->present = 0;
      }
    }
  }
  b->periods++;
  if((b->periods & 255) == 0) {
    mcp2515_errors(&b->mcp);
  }
  return 0;
}

int stmblcan_bus_write(stmblcan_bus_t *b) {
  can_frame_t tx[3];
  int ntx = 0;
  b->counter++;
  for(int i = 0; i < b->nnodes && ntx < 2; i++) {
    stmblcan_node_t *n = &b->node[i];
    if(n->con_internal) {
      n->cmd.con = sscon_frame(&n->con, n->fb.con);  // the drive's last field in, ours out
    }
    canp_cmd_encode(&tx[ntx++], n->node, &n->cmd, n->vel_scale);
    n->sent_index = n->cmd.index_enable;
  }
  for(int i = 0; i < b->ndevs; i++) {
    stmblcan_dev_t *v = &b->dev[i];
    for(int f = 0; f < v->desc.nframes && ntx < 3 - (b->nnodes ? 1 : 0); f++) {
      if(tx_due(v, f, b->periods)) {
        candesc_encode(&v->desc.frame[f], v->val[f], &tx[ntx++]);
      }
    }
  }
  if(b->nnodes) {
    canp_sync(&tx[ntx++], b->counter);  // last: the drives apply what came before it
  }
  return mcp2515_write(&b->mcp, tx, ntx);
}

int stmblcan_bus_cycle(stmblcan_bus_t *b) {
  int r = stmblcan_bus_read(b);
  return r < 0 ? r : stmblcan_bus_write(b);
}
