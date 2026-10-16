/*
 * Console over smart-serial, see sscon.h.
 */
#include "sscon.h"
#include <string.h>

sscon_t sscon0;

#define BARRIER() __asm__ volatile("" ::: "memory")

void sscon_reset(sscon_t *c) {
  c->tx_busy = 0;
  c->tx_tog  = 0;
  c->tx_byte = 0;
  c->rx_tog  = 0;
}

static uint32_t rx_count(const sscon_t *c) {
  return c->rx_head - c->rx_tail;
}

static uint32_t tx_count(const sscon_t *c) {
  return c->tx_head - c->tx_tail;
}

uint16_t sscon_frame(sscon_t *c, uint16_t in) {
  uint8_t data = in & 0xff, tog = (in >> 8) & 1, ack = (in >> 9) & 1;
  if(c->tx_busy && ack == c->tx_tog) {  // our byte arrived
    c->tx_busy = 0;
    c->n_tx++;
  }
  if(tog != c->rx_tog) {  // a new byte
    if(data == 0) {       // the other side restarted: nothing to take
      c->rx_tog = tog;
    } else if(rx_count(c) < SSCON_RXSIZE) {
      c->rxb[c->rx_head & (SSCON_RXSIZE - 1)] = data;
      BARRIER();
      c->rx_head++;
      c->rx_tog  = tog;
      c->active  = 1;
      c->n_rx++;
    }  // else: no room, no ack, it comes again
  }
  if(!c->tx_busy && tx_count(c) > 0) {
    c->tx_byte = c->txb[c->tx_tail & (SSCON_TXSIZE - 1)];
    BARRIER();
    c->tx_tail++;
    c->tx_tog ^= 1;
    c->tx_busy = 1;
  }
  return (uint16_t)(c->tx_byte | (c->tx_tog << 8) | (c->rx_tog << 9));
}

uint32_t sscon_tx_free(const sscon_t *c) {
  return SSCON_TXSIZE - tx_count(c);
}

uint32_t sscon_write(sscon_t *c, const void *data, uint32_t len) {
  const uint8_t *d = data;
  uint32_t n = 0;
  for(; n < len && tx_count(c) < SSCON_TXSIZE; n++) {
    if(d[n] == 0) {  // never sent
      continue;
    }
    c->txb[c->tx_head & (SSCON_TXSIZE - 1)] = d[n];
    BARRIER();
    c->tx_head++;
  }
  c->n_lost += len - n;
  return n;
}

uint32_t sscon_read(sscon_t *c, void *data, uint32_t len) {
  uint8_t *d = data;
  uint32_t n = 0;
  for(; n < len && rx_count(c) > 0; n++) {
    d[n] = c->rxb[c->rx_tail & (SSCON_RXSIZE - 1)];
    BARRIER();
    c->rx_tail++;
  }
  return n;
}

int sscon_getline(sscon_t *c, char *line, int len) {
  uint32_t n = rx_count(c), i;
  for(i = 0; i < n; i++) {
    uint8_t b = c->rxb[(c->rx_tail + i) & (SSCON_RXSIZE - 1)];
    if(b == '\n' || b == '\r') {
      break;
    }
  }
  if(i == n) {  // no newline yet; a full ring without one is a line too long
    if(n == SSCON_RXSIZE) {
      c->rx_tail += n;
      return -2;
    }
    return -1;
  }
  int r = (int)i < len ? (int)i : -2;
  for(uint32_t k = 0; k < i && r >= 0; k++) {
    line[k] = (char)c->rxb[(c->rx_tail + k) & (SSCON_RXSIZE - 1)];
  }
  if(r >= 0) {
    line[r] = 0;
  }
  BARRIER();
  c->rx_tail += i + 1;  // the line and its newline
  return r;
}
