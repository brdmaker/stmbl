/*
 * shared/ringbuf.c: one writer (the USB receive interrupt on the F4, the
 * F3's USB) and one reader (the main loop) at the same time. Both used to
 * change rb->len; a lost update dropped or repeated bytes, or stalled the
 * buffer for good (DEBUGNOTES 2026-11-11). Here a writer and a reader
 * thread run on two cores. Prints TAP.
 */
#include "ringbuf.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static int n, fails;

static void ok(int c, const char *name) {
  n++;
  fails += !c;
  printf("%s %d - ringbuf: %s\n", c ? "ok" : "not ok", n, name);
}

static double now(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec * 1e-9;
}

// ---- single thread: the API ----
static void basics(void) {
  struct ringbuf rb = RINGBUF(8);
  char b[16];
  ok(rb_write(&rb, "abcdefghij", 10) == 8, "write stops when full (8 of 10)");
  ok(rb_putc(&rb, 'x') == 0, "putc on a full buffer: 0");
  ok(rb_read(&rb, b, 3) == 3 && !memcmp(b, "abc", 3), "read 3");
  ok(rb_write(&rb, "klm", 3) == 3, "write wraps around");
  ok(rb_read(&rb, b, 16) == 8 && !memcmp(b, "defghklm", 8), "read across the wrap, all 8");
  char c;
  ok(rb_getc(&rb, &c) == 0, "getc on an empty buffer: 0");
  rb_write(&rb, "xyz", 3);
  rb_read(&rb, b, 2);
  ok(rb_undo(&rb, 2) == 2 && rb_read(&rb, b, 3) == 3 && !memcmp(b, "xyz", 3), "undo puts read bytes back");
  ok(rb_undo(&rb, 9) == 0, "undo past the size: refused");
  rb_write(&rb, "ab\ncd", 5);
  ok(rb_getline(&rb, b, 8) == 3 && !strcmp(b, "ab"), "getline");
  ok(rb_getline(&rb, b, 8) == 0, "no complete line yet");
  rb_write(&rb, "\n", 1);
  ok(rb_getline(&rb, b, 8) == 3 && !strcmp(b, "cd"), "the line completes");
  rb_write(&rb, "toolong\nok\n", 11);  // only 8 fit: "toolong\n"
  ok(rb_getline(&rb, b, 4) == -1, "a line longer than len is dropped");
  ok(rb_getline(&rb, b, 4) == 0, "the dropped line is consumed up to its newline");
  rb_write(&rb, "ok\n", 3);
  ok(rb_getline(&rb, b, 4) == 3 && !strcmp(b, "ok"), "the next line after the dropped one");
}

// ---- two threads: one writer, one reader ----
#define TOTAL 20000000u
static struct ringbuf srb = RINGBUF(61);  // odd size: wraps everywhere
static volatile int stop;
static volatile unsigned written;

static void *writer(void *arg) {
  (void)arg;
  unsigned i = 0;
  char chunk[7];
  while(i < TOTAL && !stop) {
    if(i & 1) {  // single bytes and chunks, as the USB interrupt does
      i += rb_putc(&srb, (char)(i * 7));
    } else {
      unsigned k = TOTAL - i < sizeof(chunk) ? TOTAL - i : sizeof(chunk);
      for(unsigned j = 0; j < k; j++) {
        chunk[j] = (char)((i + j) * 7);
      }
      i += rb_write(&srb, chunk, k);
    }
    written = i;
  }
  return 0;
}

static void stress(void) {
  pthread_t t;
  pthread_create(&t, 0, writer, 0);
  unsigned got = 0, bad = 0;
  double last = now();
  char b[13];
  while(got < TOTAL && !bad) {
    int k = (got & 3) ? rb_getc(&srb, b) : rb_read(&srb, b, sizeof(b));
    for(int j = 0; j < k; j++, got++) {
      if(b[j] != (char)(got * 7)) {
        bad = 1;
        printf("# byte %u: got %d, expected %d (written %u)\n", got, b[j], (char)(got * 7), written);
        break;
      }
    }
    if(k) {
      last = now();
    } else if(now() - last > 2.0) {
      printf("# stalled at byte %u of %u, written %u\n", got, TOTAL, written);
      break;
    }
  }
  stop = 1;
  pthread_join(t, 0);
  ok(!bad, "a concurrent writer and reader: every byte in order");
  ok(got == TOTAL, "a concurrent writer and reader: no stall, nothing lost (20 M bytes)");
}

int main(void) {
  basics();
  stress();
  printf("1..%d\n", n);
  return fails != 0;
}
