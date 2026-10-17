/*
 * A simulated MCP2515 (SPI instruction level) on a simulated CAN bus, for
 * testing linuxcnc/stmblcan without hardware. Models: the instructions
 * the driver uses (RESET, READ, WRITE, BIT MODIFY, READ STATUS, READ RX
 * BUFFER, LOAD TX BUFFER, RTS), the registers they touch, receive with
 * RXB0 -> RXB1 rollover and overflow flags, transmit priority, ACK (a
 * frame nobody acknowledges stays pending and is retried), arbitration by
 * identifier, and frame times (classic CAN, worst-case stuffing).
 */
#pragma once
#include <stdint.h>
#include "canproto.h"
#include "mcp2515.h"

#define SIM_MAX 8

typedef struct sim_bus sim_bus_t;

typedef struct {
  uint8_t reg[128];
  sim_bus_t *bus;
  int present;  // 0: no chip on this chip select (MISO reads 0xff)
} sim_chip_t;

// a bus participant that is not an MCP2515: a drive, a foreign device
typedef struct sim_node {
  void (*rx)(struct sim_node *n, const can_frame_t *f);  // every frame on the bus
  can_frame_t txq[16];
  int ntx;
  void *ctx;
} sim_node_t;

struct sim_bus {
  sim_chip_t *chip[SIM_MAX];
  int nchip;
  sim_node_t *node[SIM_MAX];
  int nnode;
  double busy_us;  // bus time used, all time
  uint32_t frames, unacked;
};

void sim_chip_init(sim_chip_t *c, sim_bus_t *bus);
int sim_xfer(void *ctx, mcp_seg_t *segs, int n);  // mcp_xfer_fn, ctx = sim_chip_t*
void sim_node_send(sim_node_t *n, const can_frame_t *f);
// run the bus for up to us microseconds; returns the time used
double sim_bus_run(sim_bus_t *b, double us);
double sim_frame_us(int dlc);  // classic CAN, 1 Mbit/s, worst-case stuffing
