// shared/encf_req.h: encf's request word, from the rt code or the frt timer.
// TAP output. make -C host test_encf_req && host/test_encf_req
#include <stdio.h>
#include "encf_req.h"

volatile uint16_t encf_req_word;

static int n, fails;
static void ok(int c, const char *name) {
  n++;
  fails += !c;
  printf("%s %d - %s\n", c ? "ok" : "not ok", n, name);
}

// one rt then four frt runs (5 kHz rt, 20 kHz frt); returns the frt runs that
// sent, as a bit mask, and *now the word the rt wrote itself
static int period(uint16_t word, int sync, uint16_t *now) {
  *now     = encf_req_arm(&encf_req_word, word, sync);
  int mask = 0;
  for(int i = 0; i < 4; i++) {
    if(encf_req_take(&encf_req_word) == word) {
      mask |= 1 << i;
    }
  }
  return mask;
}

int main(void) {
  uint16_t now;

  int m = period(2046, 1, &now);
  ok(now == 0, "sync: the rt writes nothing itself");
  ok(m == 1, "sync: the first frt update after the rt sends it, once");
  ok(encf_req_word == 0, "sync: nothing left armed");

  m = period(2046, 0, &now);
  ok(now == 2046 && m == 0, "no sync: the rt sends it, the frt doesn't");

  // switching req_sync off while a word is armed (the frt hasn't run yet)
  encf_req_arm(&encf_req_word, 2046, 1);
  now = encf_req_arm(&encf_req_word, 2046, 0);
  ok(now == 2046 && encf_req_take(&encf_req_word) == 0, "sync -> no sync: one request, not two");

  // two rt runs before an frt (the frt held off): still one request
  encf_req_arm(&encf_req_word, 2046, 1);
  encf_req_arm(&encf_req_word, 2046, 1);
  ok(encf_req_take(&encf_req_word) == 2046 && encf_req_take(&encf_req_word) == 0, "re-armed: sent once");

  ok(encf_req_take(&encf_req_word) == 0, "nothing armed: the frt sends nothing");

  printf("1..%d\n", n);
  return fails != 0;
}
