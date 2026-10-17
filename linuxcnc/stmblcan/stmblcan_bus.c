/*
 * One CAN bus of stmblcan: see stmblcan_bus.h.
 */
#include "stmblcan_bus.h"

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
  uint16_t ids[STMBLCAN_NODES];
  for(int i = 0; i < b->nnodes; i++) {
    ids[i] = (uint16_t)CANP_FB_ID(b->node[i].node);
  }
  return mcp2515_init(&b->mcp, osc_hz, bitrate, ids, b->nnodes);
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
    if(!known) {
      b->foreign++;
    }
  }
  for(int i = 0; i < b->nnodes; i++) {
    stmblcan_node_t *n = &b->node[i];
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
  }
  canp_sync(&tx[ntx++], b->counter);
  return mcp2515_write(&b->mcp, tx, ntx);
}

int stmblcan_bus_cycle(stmblcan_bus_t *b) {
  int r = stmblcan_bus_read(b);
  return r < 0 ? r : stmblcan_bus_write(b);
}
