/*
 * Lock-free single-producer / single-consumer byte FIFO. Used between the
 * simulated drive (which may run in a realtime thread) and the thread that
 * moves bytes to and from the pty.
 */
#pragma once
#include <stdatomic.h>
#include <stdint.h>

#define FIFO_SIZE 65536  // power of two

typedef struct {
  _Atomic uint32_t head;  // written by the producer
  _Atomic uint32_t tail;  // written by the consumer
  uint8_t buf[FIFO_SIZE];
} fifo_t;

static inline uint32_t fifo_free(fifo_t *f) {
  return FIFO_SIZE - (atomic_load_explicit(&f->head, memory_order_relaxed) -
                      atomic_load_explicit(&f->tail, memory_order_acquire));
}

// All or nothing, so a scope frame is never split by a full FIFO.
static inline int fifo_put(fifo_t *f, const void *data, uint32_t len) {
  if(len > fifo_free(f)) {
    return 0;
  }
  uint32_t h = atomic_load_explicit(&f->head, memory_order_relaxed);
  for(uint32_t i = 0; i < len; i++) {
    f->buf[(h + i) & (FIFO_SIZE - 1)] = ((const uint8_t *)data)[i];
  }
  atomic_store_explicit(&f->head, h + len, memory_order_release);
  return len;
}

static inline uint32_t fifo_get(fifo_t *f, void *data, uint32_t len) {
  uint32_t t = atomic_load_explicit(&f->tail, memory_order_relaxed);
  uint32_t n = atomic_load_explicit(&f->head, memory_order_acquire) - t;
  if(len > n) {
    len = n;
  }
  for(uint32_t i = 0; i < len; i++) {
    ((uint8_t *)data)[i] = f->buf[(t + i) & (FIFO_SIZE - 1)];
  }
  atomic_store_explicit(&f->tail, t + len, memory_order_release);
  return len;
}
