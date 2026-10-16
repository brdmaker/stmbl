/*
 * In-band firmware update: staging and applying, see fwstage.h.
 */
#include "fwstage.h"
#include <stdio.h>
#include <string.h>

uint32_t fwst_crc32(const uint8_t *d, uint32_t n, uint32_t crc) {
  crc = ~crc;
  for(uint32_t i = 0; i < n; i++) {
    crc ^= d[i];
    for(int b = 0; b < 8; b++) {
      crc = crc & 1 ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
    }
  }
  return ~crc;
}

uint16_t fwst_crc16(const uint8_t *d, int n) {
  uint16_t c = 0xffff;
  for(int i = 0; i < n; i++) {
    c ^= (uint16_t)d[i] << 8;
    for(int b = 0; b < 8; b++) {
      c = c & 0x8000 ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
    }
  }
  return c;
}

static int b64val(char c) {
  if(c >= 'A' && c <= 'Z') return c - 'A';
  if(c >= 'a' && c <= 'z') return c - 'a' + 26;
  if(c >= '0' && c <= '9') return c - '0' + 52;
  if(c == '+') return 62;
  if(c == '/') return 63;
  return -1;
}

static int blank(const fwst_t *s, uint32_t off, uint32_t len) {
  const uint8_t *p = s->read(s->ctx, off);
  for(uint32_t i = 0; i < len; i++) {
    if(p[i] != 0xff) {
      return 0;
    }
  }
  return 1;
}

static uint32_t unit(const fwst_t *s) {
  return s->prog_size == 8 ? 8u : 4u;
}

int fwst_begin(fwst_t *s, uint32_t size, uint32_t crc32) {
  if(s->state == FWST_RECEIVING && size == s->size && crc32 == s->crc) {
    return 1;  // resume: the sender continues at got
  }
  s->lines = s->bad = 0;
  s->got = s->done = 0;
  if(size == 0 || size > s->max_size || size > s->stage_size - FWST_HDR_SIZE) {
    s->state = FWST_FAILED;
    return -1;
  }
  uint32_t u = unit(s);
  if(!blank(s, 0, (size + u - 1) & ~(u - 1)) || !blank(s, s->stage_size - FWST_HDR_SIZE, FWST_HDR_SIZE)) {
    s->state = FWST_FAILED;  // only the bootloader erases (see fwstage.h)
    return -2;
  }
  s->size  = size;
  s->crc   = crc32;
  s->state = FWST_RECEIVING;
  return 0;
}

int fwst_data(fwst_t *s, const char *line) {
  unsigned off, crc;
  char b64[80];
  if(s->state != FWST_RECEIVING || sscanf(line, "%u %79s %x", &off, b64, &crc) != 3) {
    s->bad++;
    return -1;
  }
  uint8_t d[60] = {0};
  int n = 0, bits = 0;
  uint32_t v = 0;
  for(const char *p = b64; *p; p++) {
    int x = b64val(*p);
    if(x < 0) {
      continue;
    }
    v = (v << 6) | (uint32_t)x;
    bits += 6;
    if(bits >= 8 && n < (int)sizeof(d)) {
      bits -= 8;
      d[n++] = (uint8_t)(v >> bits);
    }
  }
  if(fwst_crc16(d, n) != crc) {  // damaged: the sender repeats it
    s->bad++;
    return -2;
  }
  if(off + (uint32_t)n == s->got && n > 0) {  // the last line again
    return 0;
  }
  if(off != s->got || off + (uint32_t)n > s->size) {  // a gap or beyond the image
    s->bad++;
    return -3;
  }
  if(s->got - s->done + (uint32_t)n > FWST_QSIZE) {
    return -5;  // the realtime loop has not caught up: send it again
  }
  for(int i = 0; i < n; i++) {
    s->q[(off + (uint32_t)i) % FWST_QSIZE] = d[i];
  }
  s->got += (uint32_t)n;
  s->lines++;
  return 0;
}

int fwst_poll(fwst_t *s, int max_units) {
  uint32_t u = unit(s);
  int w      = 0;
  while(w < max_units) {
    uint8_t b[8];
    uint32_t off;
    if(s->state == FWST_RECEIVING && (s->got - s->done >= u || (s->got == s->size && s->done < s->got))) {
      off = s->done;
      for(uint32_t i = 0; i < u; i++) {
        b[i] = off + i < s->got ? s->q[(off + i) % FWST_QSIZE] : 0xff;
      }
      s->done += u;
    } else if(s->state == FWST_MARKING && s->hdr_units < FWST_HDR_STATE / u) {
      // magic, size, crc32 (the state unit at FWST_HDR_STATE stays all ones: pending)
      uint32_t h[4] = {FWST_MAGIC, s->size, s->crc, 0xffffffffu};
      off = s->stage_size - FWST_HDR_SIZE + u * s->hdr_units;
      memcpy(b, (uint8_t *)h + u * s->hdr_units, u);
      if(++s->hdr_units == FWST_HDR_STATE / u) {
        s->state = FWST_MARKED;
      }
    } else {
      break;
    }
    if(s->program(s->ctx, off, b, u)) {
      s->state = FWST_FAILED;
      break;
    }
    w++;
  }
  return w;
}

int fwst_end(fwst_t *s) {
  if(s->state == FWST_READY) {
    return 0;
  }
  if(s->state != FWST_RECEIVING || s->got != s->size) {
    s->state = FWST_FAILED;
    return -1;
  }
  if(s->done < s->got) {
    return 1;  // still in the queue
  }
  uint32_t c = fwst_crc32(s->read(s->ctx, 0), s->size, 0);
  s->state   = c == s->crc ? FWST_READY : FWST_FAILED;
  return s->state == FWST_READY ? 0 : -1;
}

int fwst_mark(fwst_t *s) {
  if(s->state != FWST_READY) {
    return -1;
  }
  s->hdr_units = 0;
  s->state     = FWST_MARKING;
  return 0;
}

int fwst_pending(const fwst_t *s, fwst_hdr_t *h) {
  memcpy(h, s->read(s->ctx, s->stage_size - FWST_HDR_SIZE), sizeof(*h));
  return h->magic == FWST_MAGIC && h->state == 0xffffffffu && h->size > 0 && h->size <= s->max_size &&
         h->size <= s->stage_size - FWST_HDR_SIZE;
}

int fwst_boot(fwst_t *s, int (*app_erase)(void *, uint32_t, uint32_t),
              int (*app_program)(void *, uint32_t, const uint8_t *, uint32_t),
              const uint8_t *(*app_read)(void *, uint32_t), void *app_ctx) {
  fwst_hdr_t h;
  int r = 0;
  if(fwst_pending(s, &h)) {
    int a = fwst_apply(s, h.size, h.crc, app_erase, app_program, app_read, app_ctx);
    if(a < -1) {
      return -2;  // still pending: the next reset copies again
    }
    r = a == 0 ? 1 : -1;
    uint8_t zero[8] = {0};
    s->program(s->ctx, s->stage_size - FWST_HDR_SIZE + FWST_HDR_STATE, zero, unit(s));  // done
  }
  if(!blank(s, 0, s->stage_size)) {
    s->erase(s->ctx, 0, s->stage_size);  // ready for the next fwbegin
  }
  return r;
}

int fwst_apply(const fwst_t *stage, uint32_t size, uint32_t crc32,
               int (*app_erase)(void *, uint32_t, uint32_t),
               int (*app_program)(void *, uint32_t, const uint8_t *, uint32_t),
               const uint8_t *(*app_read)(void *, uint32_t), void *app_ctx) {
  const uint8_t *src = stage->read(stage->ctx, 0);
  if(fwst_crc32(src, size, 0) != crc32) {  // never copy a bad image
    return -1;
  }
  if(app_erase(app_ctx, 0, size)) {
    return -2;
  }
  for(uint32_t off = 0; off < size; off += 256) {
    uint32_t n = size - off < 256 ? size - off : 256;
    if(app_program(app_ctx, off, src + off, n)) {
      return -3;
    }
  }
  return fwst_crc32(app_read(app_ctx, 0), size, 0) == crc32 ? 0 : -4;
}
