#include "ringbuf.h"

// the other side's index: acquire, so its data is visible before it
#define LOAD(x) __atomic_load_n(&(x), __ATOMIC_ACQUIRE)
// our own index: release, so our data is stored before it
#define STORE(x, v) __atomic_store_n(&(x), (v), __ATOMIC_RELEASE)

// a + d, mod 2 * bufsize (a < 2 * bufsize, d <= bufsize)
static unsigned advance(const struct ringbuf *rb, unsigned a, unsigned d) {
  a += d;
  return a >= 2 * rb->bufsize ? a - 2 * rb->bufsize : a;
}

// index into buf
static unsigned at(const struct ringbuf *rb, unsigned a) {
  return a >= rb->bufsize ? a - rb->bufsize : a;
}

static unsigned used(const struct ringbuf *rb, unsigned rd, unsigned wr) {
  return wr >= rd ? wr - rd : wr + 2 * rb->bufsize - rd;
}

unsigned rb_len(const struct ringbuf *rb) {
  return used(rb, LOAD(rb->rd), LOAD(rb->wr));
}

unsigned rb_free(const struct ringbuf *rb) {
  return rb->bufsize - rb_len(rb);
}

int rb_getc(struct ringbuf *rb, char *data) {
  return rb_read(rb, data, 1);
}

int rb_putc(struct ringbuf *rb, const char data) {
  return rb_write(rb, &data, 1);
}

int rb_read(struct ringbuf *rb, void *data, int len) {
  unsigned rd = rb->rd;  // ours
  unsigned n  = used(rb, rd, LOAD(rb->wr));
  if(len < 0) {
    len = 0;
  }
  if((unsigned)len < n) {
    n = len;
  }
  unsigned p  = at(rb, rd);
  unsigned n1 = rb->bufsize - p < n ? rb->bufsize - p : n;
  memcpy(data, rb->buf + p, n1);
  memcpy((char *)data + n1, rb->buf, n - n1);
  STORE(rb->rd, advance(rb, rd, n));
  return n;
}

int rb_write(struct ringbuf *rb, const void *data, int len) {
  unsigned wr = rb->wr;  // ours
  unsigned n  = rb->bufsize - used(rb, LOAD(rb->rd), wr);
  if(len < 0) {
    len = 0;
  }
  if((unsigned)len < n) {
    n = len;
  }
  unsigned p  = at(rb, wr);
  unsigned n1 = rb->bufsize - p < n ? rb->bufsize - p : n;
  memcpy(rb->buf + p, data, n1);
  memcpy(rb->buf, (const char *)data + n1, n - n1);
  STORE(rb->wr, advance(rb, wr, n));
  return n;
}

int rb_getline(struct ringbuf *rb, char *ptr, int len) {
  char c;

  // still dropping the rest of a line that was too long
  while(rb->skip) {
    if(!rb_getc(rb, &c)) {
      return 0;
    }
    if(c == '\n') {
      rb->skip = 0;
    }
  }

  // Look for the end of the line without consuming anything, so an
  // incomplete line stays in place while the writer adds to it.
  unsigned avail = rb_len(rb);
  unsigned n     = avail < (unsigned)len ? avail : (unsigned)len;
  unsigned p     = at(rb, rb->rd);
  for(unsigned i = 0; i < n; i++) {
    if(rb->buf[p] == '\n') {
      rb_read(rb, ptr, i + 1);
      ptr[i] = '\0';
      return i + 1;
    }
    if(++p >= rb->bufsize) {
      p = 0;
    }
  }

  // len bytes and no '\n': the line can never fit. Drop it, up to and
  // including its '\n', instead of keeping it at the head of the buffer
  // (which blocked every later line).
  if(avail >= (unsigned)len) {
    rb->skip = 1;
    return -1;
  }
  return 0;
}

int rb_undo(struct ringbuf *rb, int len) {
  if(len <= 0) {
    return 0;
  }
  // the bytes before rd are still there unless the writer reused them
  if(rb_len(rb) + (unsigned)len > rb->bufsize) {
    return 0;
  }
  unsigned rd = rb->rd;
  STORE(rb->rd, rd >= (unsigned)len ? rd - len : rd + 2 * rb->bufsize - len);
  return len;
}
