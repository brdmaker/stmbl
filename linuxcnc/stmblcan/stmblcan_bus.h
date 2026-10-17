/*
 * One CAN bus of stmblcan, LinuxCNC's side, without HAL: the servo
 * period's exchange on one MCP2515 (shared/canproto.h frames). The HAL
 * component wraps this; the host tests run it against simulated chips.
 *
 * Per period:
 *   - stmblcan_bus_read, at the start of the servo thread: the feedback
 *     received since the last period, answering the last SYNC
 *   - stmblcan_bus_write, at its end: the commands and a SYNC with the
 *     next counter, commands first (TXB0, TXB1), SYNC last (TXB2). The
 *     drives apply the newest command at SYNC and answer it with their
 *     feedback.
 * A node that has not answered for `timeout` periods is not present.
 */
#pragma once
#include <stdint.h>
#include "canproto.h"
#include "mcp2515.h"
#include "sscon.h"
#include "candesc.h"

#define STMBLCAN_NODES 2  // per bus: the MCP2515's 2 receive buffers
#define STMBLCAN_DEVS 2   // generic (described) devices per bus

typedef struct {
  int node;         // CAN node ID 1..127
  float vel_scale;  // full scale of vel_cmd / vel_fb
  canp_cmd_t cmd;   // to send (the HAL pins)
  canp_fb_t fb;     // received
  float slots[CANP_SLOTS];
  int present;
  uint32_t age;      // periods since the last feedback
  uint32_t fb_frames, missed;
  sscon_t con;       // this drive's console (LinuxCNC's end), if con_internal
  int con_internal;  // 0: cmd.con / fb.con are handled outside (HAL pins, sserialterm)
} stmblcan_node_t;

// a device without our firmware, from its description (candesc.h): its
// frames' fields as values (the HAL pins)
typedef struct {
  candesc_t desc;
  float val[CANDESC_FRAMES][CANDESC_FIELDS];
  uint8_t phase[CANDESC_FRAMES];  // tx frames with every > 1: staggered
  int present;
  uint32_t age, rx_frames, missed;
} stmblcan_dev_t;

typedef struct {
  mcp2515_t mcp;
  stmblcan_node_t node[STMBLCAN_NODES];
  int nnodes;
  stmblcan_dev_t dev[STMBLCAN_DEVS];
  int ndevs;
  uint8_t counter;   // the SYNC counter of the last period
  uint32_t periods, timeout;
  uint32_t foreign;  // frames received that belong to no node
} stmblcan_bus_t;

// add a described device (a file, or what discovery found): a stmbl
// profile becomes a node, anything else a generic device. 0 ok, < 0 full
int stmblcan_bus_add(stmblcan_bus_t *b, const candesc_t *d);
// does a period fit the MCP2515? at most 3 frames out (SYNC included) and
// 2 in, in the worst period; generic tx frames with every > 1 are
// staggered. 0 ok, else -1 with the reason in err
int stmblcan_bus_check(stmblcan_bus_t *b, char *err, int errlen);
// 0 ok, < 0 the chip did not initialise (mcp2515_init)
int stmblcan_bus_init(stmblcan_bus_t *b, uint32_t osc_hz, uint32_t bitrate);
// discovery, before stmblcan_bus_init: ask node for its description. The
// chip is set up for it and left so (stmblcan_bus_init sets it up again).
// wait(ctx, us) lets the bus run (a sleep on the Pi, the simulated bus in
// tests). 0 ok (text in buf), -1 no chip, -2 no answer
int stmblcan_bus_discover(stmblcan_bus_t *b, uint32_t osc_hz, uint32_t bitrate, int node, char *buf, int len,
                          void (*wait)(void *ctx, int us), void *ctx);
// < 0 on an SPI error
int stmblcan_bus_read(stmblcan_bus_t *b);
int stmblcan_bus_write(stmblcan_bus_t *b);
int stmblcan_bus_cycle(stmblcan_bus_t *b);  // both (tests)
