/*
 * shared/sscon.c: the console link over smart-serial, two ends through a
 * channel that loses and repeats frames, as hostmot2 can. Prints TAP.
 */
#include "sscon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int n, fails;

static void ok(int c, const char *name, double got) {
  n++;
  fails += !c;
  printf("%s %d - sscon: %s\n", c ? "ok" : "not ok", n, name);
  if(!c) {
    printf("#   got %g\n", got);
  }
}

static sscon_t host, drive;

// one servo period: host request -> drive -> reply; loss: a frame dropped
// (the receiver sees the previous one again)
static uint16_t req, rep;
static void period(double loss) {
  uint16_t r = sscon_frame(&host, rep);  // hostmot2 read the last reply
  if((double)rand() / RAND_MAX >= loss) {
    req = r;
  }
  uint16_t a = sscon_frame(&drive, req);
  if((double)rand() / RAND_MAX >= loss) {
    rep = a;
  }
}

static int xfer(const char *msg, int len, double loss, int max_periods, sscon_t *from, sscon_t *to, char *got) {
  sscon_write(from, msg, len);
  int k = 0, g = 0;
  while(g < len && k < max_periods) {
    period(loss);
    g += sscon_read(to, got + g, len - g);
    k++;
  }
  got[g] = 0;
  return k;
}

// sscon_out, the drive's _write: a clock, a link that takes a byte every
// ms (or none: dead), the watchdog feed it must call while it waits
static uint32_t clk, last_feed, max_gap, link_dead;
static uint32_t now_ms(void) {
  return clk;
}
static void idle(void) {  // the main loop waits a ms: the link moves a byte, the watchdog is fed
  clk++;
  if(!link_dead && drive.tx_tail != drive.tx_head) {
    drive.tx_tail++;
  }
  if(clk - last_feed > max_gap) {
    max_gap = clk - last_feed;
  }
  last_feed = clk;
}
static const sscon_out_t out = {now_ms, idle, sscon_write};

int main(void) {
  srand(1203);
  char msg[1200], got[1300];
  for(int i = 0; i < 1000; i++) {
    msg[i] = (char)(' ' + (i * 7) % 90);
  }
  msg[1000] = 0;
  int k = xfer(msg, 1000, 0.0, 5000, &host, &drive, got);
  ok(!strcmp(got, msg) && k <= 1003, "host -> drive, 1000 bytes, clean link: a byte a period", k);
  k = xfer(msg, 1000, 0.0, 5000, &drive, &host, got);
  ok(!strcmp(got, msg) && k <= 1003, "drive -> host, 1000 bytes, clean link: a byte a period", k);
  k = xfer(msg, 1000, 0.1, 20000, &drive, &host, got);
  ok(!strcmp(got, msg), "drive -> host with 10 % of frames lost each way: every byte, in order, once", k);
  k = xfer(msg, 1000, 0.3, 50000, &host, &drive, got);
  ok(!strcmp(got, msg), "host -> drive with 30 % lost: every byte, in order, once", k);

  // both ways at once
  sscon_write(&host, "show\n", 5);
  sscon_write(&drive, "hello\n", 6);
  for(int i = 0; i < 20; i++) {
    period(0.0);
  }
  char line[64];
  int a = sscon_getline(&drive, line, sizeof(line));
  int b = sscon_getline(&drive, line, sizeof(line));
  char hl[64];
  int c = sscon_getline(&host, hl, sizeof(hl));
  ok(a == 4 && b == -1 && c == 5 && !strcmp(hl, "hello"), "both ways at once; getline: a line, then none yet", a);

  // flow control: the drive's ring full, the host's bytes wait
  char big[600];
  memset(big, 'x', sizeof(big));
  big[599] = '\n';
  sscon_write(&host, big, sizeof(big));
  for(int i = 0; i < 1000; i++) {
    period(0.0);
  }
  uint32_t held = (uint32_t)(drive.rx_head - drive.rx_tail);
  ok(held == SSCON_RXSIZE && host.tx_head - host.tx_tail > 0, "receiver full: the sender waits (no ack)", held);
  int t = sscon_getline(&drive, line, sizeof(line));
  for(int i = 0; i < 2000; i++) {
    period(0.0);
    char tmp[64];
    sscon_read(&drive, tmp, sizeof(tmp));
  }
  ok(t == -2 && host.tx_head == host.tx_tail, "a line longer than the ring is dropped; the rest flows again", t);

  // the host restarts (LinuxCNC restart): its link state zero, the drive's not
  sscon_t fresh;
  memset(&fresh, 0, sizeof(fresh));
  host = fresh;
  for(int i = 0; i < 5; i++) {
    period(0.0);
  }
  sscon_write(&host, "about\n", 6);
  for(int i = 0; i < 20; i++) {
    period(0.0);
  }
  char dl[64];
  int d;
  while((d = sscon_getline(&drive, dl, sizeof(dl))) == 0) {
  }
  ok(d == 5 && !strcmp(dl, "about"), "after the host restarts: nothing spurious, the next line arrives whole", d);

  // the link times out with output queued (LinuxCNC stopped mid-reply):
  // the drive stops queueing, so its prints don't wait for a ring that
  // nobody drains, and starts again when the host sends
  sscon_write(&drive, "a reply nobody reads\n", 21);
  sscon_reset(&drive);
  ok(!drive.active && drive.tx_head == drive.tx_tail, "a link timeout: inactive, pending output dropped", drive.active);
  host = fresh;
  sscon_write(&host, "about\n", 6);
  for(int i = 0; i < 20; i++) {
    period(0.0);
  }
  while((d = sscon_getline(&drive, dl, sizeof(dl))) == 0) {
  }
  ok(drive.active && d == 5, "  ...and active again once the host sends", d);
  {  // a scopedump through the console: ~10 kB at a byte a ms, > the watchdog's 2 s
    static char dump[10000];
    memset(dump, 'x', sizeof(dump));
    sscon_reset(&drive);
    clk = last_feed = max_gap = link_dead = 0;
    uint32_t q = sscon_out(&drive, &out, dump, sizeof(dump), 100);
    ok(q == sizeof(dump), "sscon_out: a 10 kB dump over a 1 kB/s link comes through whole", q);
    ok(clk > 2000 && max_gap <= 2, "sscon_out: feeds the watchdog while it waits (a scopedump took > 2 s)", max_gap);
    sscon_reset(&drive);
    clk = last_feed = max_gap = 0;
    link_dead = 1;
    q = sscon_out(&drive, &out, dump, sizeof(dump), 100);
    ok(clk <= 101 && q == SSCON_TXSIZE, "sscon_out: a dead link waits one timeout, then drops the rest", clk);
    clk = 0;
    q = sscon_out(&drive, &out, dump, 100, 0);
    ok(clk == 0 && q == 0, "sscon_out: in an interrupt (wait 0) never waits", clk);
  }

  printf("1..%d\n# %d/%d passed\n", n, n - fails, n);
  return fails ? 1 : 0;
}
