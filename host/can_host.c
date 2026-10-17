/*
 * CAN in the simulator (docs/can_support.md): the simulated drive's CAN
 * controller (can_hw.h, for shared/comps/can.c) as a node on a simulated
 * bus, and LinuxCNC's end on the same bus as it runs on the Pi: the real
 * linuxcnc/stmblcan bus code and MCP2515 driver, against a simulated
 * MCP2515 (mcp2515_sim.c). host/main.c's lcnccan commands drive it.
 */
#include "can_host.h"
#include "can_hw.h"
#include "mcp2515_sim.h"
#include <stdio.h>
#include <string.h>

static sim_bus_t bus;
static sim_chip_t chip;
static stmblcan_bus_t pi;  // LinuxCNC's end
static int ready;

// ---- the drive's controller: a node on the bus, with filters ----
#define DRV_Q 32
static sim_node_t drv_node;
static can_frame_t drv_rx[DRV_Q];
static uint32_t drv_rx_head, drv_rx_tail, drv_rx_lost;
static uint16_t drv_ids[8];
static int drv_nids;

static void drv_on_frame(sim_node_t *n, const can_frame_t *f) {
  (void)n;
  int ok = drv_nids == 0;
  for(int i = 0; i < drv_nids; i++) {
    ok |= f->id == drv_ids[i];
  }
  if(!ok) {
    return;
  }
  if(drv_rx_head - drv_rx_tail >= DRV_Q) {
    drv_rx_lost++;
    return;
  }
  drv_rx[drv_rx_head++ % DRV_Q] = *f;
}

void can_hw_filter(const uint16_t *ids, int n) {
  drv_nids = n > 8 ? 8 : n;
  memcpy(drv_ids, ids, sizeof(uint16_t) * (size_t)drv_nids);
}

int can_hw_recv(can_frame_t *f) {
  if(drv_rx_tail == drv_rx_head) {
    return 0;
  }
  *f = drv_rx[drv_rx_tail++ % DRV_Q];
  return 1;
}

int can_hw_send(const can_frame_t *f) {
  if(drv_node.ntx >= 16) {
    return -1;
  }
  sim_node_send(&drv_node, f);
  return 0;
}

// ---- LinuxCNC's end ----
int can_host_init(int node, float vel_scale) {
  memset(&bus, 0, sizeof(bus));
  memset(&pi, 0, sizeof(pi));
  sim_chip_init(&chip, &bus);
  drv_node.rx  = drv_on_frame;
  drv_node.ntx = 0;
  bus.node[bus.nnode++] = &drv_node;
  pi.mcp.xfer = sim_xfer;
  pi.mcp.ctx  = &chip;
  pi.nnodes   = 1;
  pi.node[0]  = (stmblcan_node_t){.node = node, .vel_scale = vel_scale};
  ready       = stmblcan_bus_init(&pi, 16000000, 1000000) == 0;
  return ready ? 0 : -1;
}

// discovery as at stmblcan's load; wait() lets the simulation run (the
// drive answers in its rt loop)
int can_host_discover(int node, char *buf, int len, void (*wait)(void *ctx, int us)) {
  memset(&bus, 0, sizeof(bus));
  memset(&pi, 0, sizeof(pi));
  sim_chip_init(&chip, &bus);
  drv_node.rx  = drv_on_frame;
  drv_node.ntx = 0;
  bus.node[bus.nnode++] = &drv_node;
  pi.mcp.xfer = sim_xfer;
  pi.mcp.ctx  = &chip;
  ready       = 1;  // the bus runs during discovery
  int r       = stmblcan_bus_discover(&pi, 16000000, 1000000, node, buf, len, wait, 0);
  ready       = 0;
  return r;
}

stmblcan_node_t *can_host_node(void) {
  return ready ? &pi.node[0] : 0;
}

void can_host_read(void) {
  if(ready) {
    stmblcan_bus_read(&pi);
  }
}

void can_host_write(void) {
  if(ready) {
    stmblcan_bus_write(&pi);
  }
}

void can_host_run(double us) {
  if(ready) {
    sim_bus_run(&bus, us);
  }
}

void can_host_drive_attached(int on) {
  bus.nnode = on ? 1 : 0;
}

void can_host_stat(void) {
  stmblcan_node_t *n = &pi.node[0];
  printf("can: present %d fb %u missed %u tx_late %u rx_overflow %u foreign %u bus %.0f us frames %u unacked %u "
         "drive_lost %u\n",
         n->present, n->fb_frames, n->missed, pi.mcp.tx_late, pi.mcp.rx_overflow, pi.foreign, bus.busy_us, bus.frames,
         bus.unacked, drv_rx_lost);
}
