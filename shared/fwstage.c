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

int fwst_begin(fwst_t *s, uint32_t size, uint32_t crc32) {
  s->lines = s->bad = 0;
  s->got   = 0;
  if(size == 0 || size > s->stage_size || s->erase(s->ctx, 0, s->stage_size)) {
    s->state = FWST_FAILED;
    return -1;
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
  uint8_t d[60];
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
  if(s->program(s->ctx, off, d, (uint32_t)n)) {
    s->state = FWST_FAILED;
    return -4;
  }
  s->got += (uint32_t)n;
  s->lines++;
  return 0;
}

int fwst_end(fwst_t *s) {
  if(s->state != FWST_RECEIVING || s->got != s->size) {
    s->state = FWST_FAILED;
    return -1;
  }
  uint32_t c = fwst_crc32(s->read(s->ctx, 0), s->size, 0);
  s->state   = c == s->crc ? FWST_READY : FWST_FAILED;
  return s->state == FWST_READY ? 0 : -1;
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
