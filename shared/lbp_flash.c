/*
 * Mesa's remote flash protocol over smart-serial, the device's side. See
 * lbp_flash.h.
 */
#include "lbp_flash.h"
#include <string.h>

void lbpf_init(lbpf_t *f) {
  f->nv     = 0;
  f->offset = 0;
  f->ok     = 1;
  f->erases = f->writes = f->refused = 0;
  memset(f->buf, 0xff, sizeof(f->buf));
}

int lbpf_local_write(lbpf_t *f, uint8_t cmd, uint8_t data) {
  if(cmd != LBPF_NV_LOCAL_WRITE) {
    return 0;
  }
  f->nv = data == LBPF_NV_FLASH ? LBPF_NV_FLASH : 0;
  return 1;
}

int lbpf_local_read(lbpf_t *f, uint8_t cmd, uint8_t *out) {
  if(cmd != LBPF_COOKIE_CMD) {
    return 0;
  }
  // the commits run to completion before their LBP transaction ends, so the
  // cookie only has to tell whether it worked
  *out = f->ok ? LBPF_COOKIE : 0;
  return 1;
}

static int commit(lbpf_t *f, uint8_t cmd) {
  uint32_t eb = 1u << f->erase_log2, wb = 1u << f->write_log2;
  uint32_t len = cmd == LBPF_CMD_ERASE ? eb : wb;
  // never block 0 (the bootloader), never past the window, block aligned
  if(f->offset < eb || f->offset + len > f->window_size || f->offset % len) {
    f->refused++;
    return -1;
  }
  if(cmd == LBPF_CMD_ERASE) {
    f->erases++;
    return f->erase(f->ctx, f->offset, eb);
  }
  if(cmd == LBPF_CMD_WRITE) {
    f->writes++;
    return f->program(f->ctx, f->offset, f->buf, wb);
  }
  f->refused++;
  return -1;
}

int lbpf_mem_write(lbpf_t *f, uint16_t addr, const uint8_t *d, int n) {
  if(f->nv != LBPF_NV_FLASH) {
    return 0;
  }
  uint32_t wb = 1u << f->write_log2;
  if(addr == LBPF_OFFSET && n == 4) {
    f->offset = (uint32_t)d[0] | (uint32_t)d[1] << 8 | (uint32_t)d[2] << 16 | (uint32_t)d[3] << 24;
  } else if(addr == LBPF_COMMIT && n == 1) {
    f->ok = commit(f, d[0]) == 0;
  } else if(addr + (uint32_t)n <= wb) {
    memcpy(f->buf + addr, d, n);
  } else {
    f->refused++;
  }
  return 1;
}

int lbpf_mem_read(lbpf_t *f, uint16_t addr, uint8_t *d, int n) {
  if(f->nv != LBPF_NV_FLASH) {
    return 0;
  }
  memset(d, 0, n);
  if(addr == LBPF_ERASE_SIZE && n >= 1) {
    d[0] = f->erase_log2;
  } else if(addr == LBPF_WRITE_SIZE && n >= 1) {
    d[0] = f->write_log2;
  } else if(addr == LBPF_OFFSET && n == 4) {
    memcpy(d, &f->offset, 4);
  }
  return 1;
}

int lbpf_sector_erase(lbpf_sectors_t *s, uint32_t off, uint32_t len) {
  for(int i = 0; i < s->n; i++) {
    uint32_t a = s->sector_start[i], b = s->sector_start[i + 1];
    if(off < b && off + len > a && !(s->erased & (1u << i))) {
      if(s->erase_sector(s->ctx, i)) {
        return -1;
      }
      s->erased |= 1u << i;
    }
  }
  return 0;
}
