/*
 * MCP2515 CAN controller, driven directly over SPI from LinuxCNC's
 * realtime thread (docs/can_support.md, "Raspberry Pi with MCP2515s").
 * Polled once per servo period, no interrupts. The SPI goes through a
 * callback: spidev on the Pi, a simulated chip in the host tests.
 *
 * Per period, at the start of the servo thread (mcp2515_read):
 *   1. READ STATUS: which receive buffers are full, which transmit
 *      buffers are still pending
 *   2. READ RX BUFFER for the full ones (clears their flags)
 * and at its end, once the commands are computed (mcp2515_write):
 *   3. one batch: abort what is still pending from the last period
 *      (counted: late), LOAD TX BUFFER for up to 3 frames, one RTS
 * Transmit order is the buffers' priority: frames[0] first (TXB0 has the
 * highest TXP). Receive: the expected identifiers only (acceptance
 * filters), RXB0 rolling over into RXB1.
 *
 * Instructions and registers: MCP2515 data sheet DS20001801K.
 */
#pragma once
#include <stdint.h>
#include "canproto.h"

// one chip-select assertion: len bytes out, len bytes in (rx may be NULL)
typedef struct {
  const uint8_t *tx;
  uint8_t *rx;
  uint16_t len;
} mcp_seg_t;
// n segments in one call (spidev: one SPI_IOC_MESSAGE, cs_change between); 0 ok
typedef int (*mcp_xfer_fn)(void *ctx, mcp_seg_t *segs, int n);

typedef struct {
  mcp_xfer_fn xfer;
  void *ctx;
  // counters
  uint32_t calls, bytes;    // SPI calls and bytes, all time
  uint32_t rx_frames, tx_frames;
  uint32_t tx_late;         // frames still pending a period later: aborted
  uint32_t rx_overflow;     // frames lost: both receive buffers were full
  uint8_t eflg, tec, rec;   // the last error state read
  uint8_t status;           // READ STATUS of the last mcp2515_read
} mcp2515_t;

// instructions
#define MCP_RESET 0xC0
#define MCP_READ 0x03
#define MCP_WRITE 0x02
#define MCP_BITMOD 0x05
#define MCP_READ_STATUS 0xA0
#define MCP_READ_RXB(n) (0x90 | ((n) << 2))  // from RXBnSIDH
#define MCP_LOAD_TXB(n) (0x40 | ((n) << 1))  // from TXBnSIDH
#define MCP_RTS(mask) (0x80 | (mask))
// registers
#define MCP_CANSTAT 0x0E
#define MCP_CANCTRL 0x0F
#define MCP_TEC 0x1C
#define MCP_REC 0x1D
#define MCP_CNF3 0x28
#define MCP_CNF2 0x29
#define MCP_CNF1 0x2A
#define MCP_CANINTE 0x2B
#define MCP_CANINTF 0x2C
#define MCP_EFLG 0x2D
#define MCP_TXBCTRL(n) (0x30 + 0x10 * (n))
#define MCP_RXB0CTRL 0x60
#define MCP_RXB1CTRL 0x70
// READ STATUS bits
#define MCP_ST_RX0IF 0x01
#define MCP_ST_RX1IF 0x02
#define MCP_ST_TXREQ(n) (0x04 << (2 * (n)))
// EFLG
#define MCP_EFLG_RX0OVR 0x40
#define MCP_EFLG_RX1OVR 0x80
#define MCP_EFLG_TXBO 0x20

// CNF1..3 for osc_hz and bitrate: 8..16 time quanta, sample point ~75 %;
// 0 ok, -1 not possible (e.g. 1 Mbit/s from 8 MHz)
int mcp2515_timing(uint32_t osc_hz, uint32_t bitrate, uint8_t cnf[3]);
// reset, timing, receive with rollover, transmit priorities, normal mode.
// ids: accept only these identifiers (up to 2 on RXB0's filters: a foreign
// device then cannot take the receive buffers; 3..6: the rest on RXB1's
// filters, without rollover); nids 0: any frame.
// 0 ok, < 0: the chip does not answer as an MCP2515 should
int mcp2515_init(mcp2515_t *m, uint32_t osc_hz, uint32_t bitrate, const uint16_t *ids, int nids);
// the frames received since the last read (up to 2, into rx): the number,
// < 0 on an SPI error
int mcp2515_read(mcp2515_t *m, can_frame_t *rx);
// up to 3 frames to send, in this order; 0 ok
int mcp2515_write(mcp2515_t *m, const can_frame_t *tx, int ntx);
// both at once (tests)
int mcp2515_cycle(mcp2515_t *m, can_frame_t *rx, const can_frame_t *tx, int ntx);
// read and clear the error flags (every few hundred periods): EFLG, TEC,
// REC into m; counts receive overflows
int mcp2515_errors(mcp2515_t *m);
