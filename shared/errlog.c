// The flash error log, see errlog.h.
#include "errlog.h"
#include <string.h>

uint32_t errlog_check(const errlog_entry_t *e) {
  const uint32_t *w = (const uint32_t *)e;
  uint32_t c        = 0xe1106ul;
  for(uint32_t i = 0; i < ERRLOG_WORDS - 1; i++) {
    c = (c << 5 | c >> 27) ^ w[i];
  }
  return c == 0xffffffff ? 0 : c;  // never looks erased
}

int errlog_valid(const errlog_entry_t *e) {
  return e->head != 0xffffffff && e->check == errlog_check(e);
}

uint32_t errlog_slots(const errlog_t *l) {
  return l->fl->words / ERRLOG_WORDS;
}

const errlog_entry_t *errlog_at(const errlog_t *l, uint32_t i) {
  return (const errlog_entry_t *)(l->fl->base + i * ERRLOG_WORDS);
}

static int slot_free(const errlog_t *l, uint32_t i) {
  const volatile uint32_t *w = l->fl->base + i * ERRLOG_WORDS;
  for(uint32_t k = 0; k < ERRLOG_WORDS; k++) {
    if(w[k] != 0xffffffff) {
      return 0;
    }
  }
  return 1;
}

static void scan(errlog_t *l) {
  uint32_t n = errlog_slots(l);
  l->next    = 0;
  for(uint32_t i = n; i > 0; i--) {  // after the last slot with anything in it
    if(!slot_free(l, i - 1)) {
      l->next = i;
      break;
    }
  }
}

static int write_entry(errlog_t *l, uint32_t slot, const errlog_entry_t *e) {
  const uint32_t *w = (const uint32_t *)e;
  for(uint32_t k = 0; k < ERRLOG_WORDS; k++) {
    if(l->fl->program(slot * ERRLOG_WORDS + k, w[k])) {
      return -1;
    }
  }
  return 0;
}

static errlog_entry_t keep[ERRLOG_KEEP];

int errlog_open(errlog_t *l, const errlog_flash_t *fl, uint32_t room) {
  memset(l, 0, sizeof(*l));
  l->fl = fl;
  scan(l);
  uint16_t last = 0;
  for(uint32_t i = 0; i < l->next; i++) {
    const errlog_entry_t *e = errlog_at(l, i);
    if(errlog_valid(e) && ERRLOG_TYPE(e) == ERRLOG_BOOT) {
      last = ERRLOG_BOOTNR(e);
    }
  }
  l->boot = last == 0xffff ? 1 : last + 1;  // 0 is never a boot
  if(errlog_slots(l) - l->next >= room) {
    return 0;
  }
  // nearly full: keep the newest valid entries, erase, write them back
  uint32_t k = 0;
  for(uint32_t i = l->next; i > 0 && k < ERRLOG_KEEP; i--) {
    const errlog_entry_t *e = errlog_at(l, i - 1);
    if(errlog_valid(e)) {
      keep[k++] = *e;
    }
  }
  if(fl->erase()) {
    return -1;
  }
  l->next = 0;
  while(k > 0) {
    if(write_entry(l, l->next, &keep[--k])) {
      scan(l);
      return -1;
    }
    l->next++;
  }
  return 0;
}

uint32_t errlog_head(errlog_type_t type, uint32_t code, uint16_t boot) {
  return (uint32_t)type << 24 | (code & 0xff) << 16 | boot;
}

int errlog_append(errlog_t *l, errlog_entry_t *e) {
  if(!l->fl || l->next >= errlog_slots(l)) {
    return -1;
  }
  e->check = errlog_check(e);
  uint32_t slot = l->next++;  // a torn write still uses up its slot
  return write_entry(l, slot, e);
}

int errlog_clear(errlog_t *l) {
  if(l->fl->erase()) {
    return -1;
  }
  l->next = 0;
  return 0;
}
