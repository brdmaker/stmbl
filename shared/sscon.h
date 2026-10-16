/*
 * Console over smart-serial: a byte stream in a 10-bit process data field
 * each way ("con": bits 0-7 a byte, bit 8 the sender's toggle, bit 9 the
 * ack: the toggle of the last byte it took from the other side).
 *
 * Alternating-bit protocol, one byte in flight each way: a sender offers a
 * byte with a new toggle and repeats it every frame until the other side's
 * ack equals that toggle. A receiver takes a byte when its toggle differs
 * from the last one taken (and it has room; else it does not ack: flow
 * control). Lost or repeated frames (CRC errors, hostmot2 skipping a
 * channel) delay bytes, never lose or duplicate them. One byte per frame
 * each way: ~1 kB/s at a 1 kHz servo thread.
 *
 * Both ends run the same code: the drive (shared/comps via sserial, sscon0)
 * and LinuxCNC (linuxcnc/sserialterm). Byte 0 is never sent: a side that
 * restarted (all zero) offers nothing, and a 0 from the other side's restart
 * is dropped.
 *
 * Rings: single producer, single consumer (the frame side and the console
 * side), lock free.
 */
#pragma once
#include <stdint.h>

// powers of two. The drive's _write waits for room (main loop); the host
// simulator cannot, so it builds with a ring that holds a whole showconf
#ifndef SSCON_TXSIZE
#define SSCON_TXSIZE 2048
#endif
#define SSCON_RXSIZE 256

typedef struct {
  volatile uint32_t tx_head, tx_tail;  // console -> link
  uint8_t txb[SSCON_TXSIZE];
  volatile uint32_t rx_head, rx_tail;  // link -> console
  uint8_t rxb[SSCON_RXSIZE];
  uint8_t tx_byte, tx_tog, tx_busy;  // the byte offered, its toggle, not yet acked
  uint8_t rx_tog;                    // toggle of the last byte taken (our ack)
  volatile uint8_t active;           // the other side has sent us something
  uint32_t n_tx, n_rx, n_lost;       // bytes delivered / taken / dropped (tx ring full)
} sscon_t;

extern sscon_t sscon0;  // the drive's console

void sscon_reset(sscon_t *c);  // link lost or restarted: rings kept, link state cleared
// one frame: the other side's field in, ours out
uint16_t sscon_frame(sscon_t *c, uint16_t in);
// console side: queue output (dropped when full; returns bytes queued)
uint32_t sscon_write(sscon_t *c, const void *data, uint32_t len);
uint32_t sscon_tx_free(const sscon_t *c);
// console side: received bytes
uint32_t sscon_read(sscon_t *c, void *data, uint32_t len);
// a whole line (without the newline, 0 terminated): its length (0: an
// empty line), -1 none yet, -2 longer than len - 1 (dropped)
int sscon_getline(sscon_t *c, char *line, int len);
