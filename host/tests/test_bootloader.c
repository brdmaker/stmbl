/*
 * The smart-serial bootloader sketch (docs/sserial_bootloader.md):
 * shared/lbp_flash.c driven exactly as LinuxCNC's setsserial flashes a
 * remote, and shared/fwstage.c (the in-band update over the console),
 * both on a model of the STM32F4's flash (bits only go 1 -> 0 without an
 * erase; uneven sectors). Prints TAP.
 */
#include "lbp_flash.h"
#include "fwstage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int n, fails;
static void ok(int c, const char *name, double got) {
  n++;
  fails += !c;
  printf("%s %d - bootloader: %s\n", c ? "ok" : "not ok", n, name);
  if(!c) {
    printf("#   got %g\n", got);
  }
}

// ---- flash model: the update window over stmbl's F4 application area ----
// window offset 0 = one 16 kB block below the application (block 0, the
// bootloader's: never written); the application (0x08010000) starts at
// 16 kB: sector 4 (64 kB), sectors 5..11 (128 kB each)
#define KB 1024u
#define WIN (16 * KB + 960 * KB)
static uint8_t mem[WIN];
static int bad_programs, sector_erases;
static const uint32_t sec_start[] = {16 * KB, 80 * KB, 208 * KB, 336 * KB, 464 * KB, 592 * KB, 720 * KB, 848 * KB,
                                     976 * KB};

static int erase_sector(void *ctx, int s) {
  (void)ctx;
  memset(mem + sec_start[s], 0xff, sec_start[s + 1] - sec_start[s]);
  sector_erases++;
  return 0;
}
static lbpf_sectors_t sectors = {sec_start, 8, 0, erase_sector, NULL};
static int win_erase(void *ctx, uint32_t off, uint32_t len) {
  (void)ctx;
  return lbpf_sector_erase(&sectors, off, len);
}
static int win_program(void *ctx, uint32_t off, const uint8_t *d, uint32_t len) {
  (void)ctx;
  for(uint32_t i = 0; i < len; i++) {
    if((mem[off + i] & d[i]) != d[i]) {  // a 0 -> 1: not erased
      bad_programs++;
      return -1;
    }
    mem[off + i] &= d[i];
  }
  return 0;
}

// ---- the device: its LBP parser hands flash traffic to lbp_flash ----
static lbpf_t dev;
static long transactions, wire_bytes;
// one LBP transaction: request bytes in, reply bytes out (their count)
static int device(const uint8_t *req, int len, uint8_t *rep) {
  transactions++;
  wire_bytes += len + 1;  // + CRC
  uint8_t cmd = req[0], ct = cmd >> 6, wr = (cmd >> 5) & 1, as = (cmd >> 2) & 1, ds = cmd & 3;
  if(ct == 3) {  // local
    if(wr) {
      lbpf_local_write(&dev, cmd, req[1]);
      return 0;
    }
    lbpf_local_read(&dev, cmd, rep);
    wire_bytes += 2;
    return 1;
  }
  if(ct == 1 && as) {  // memory, 2 byte address
    uint16_t addr = req[1] | req[2] << 8;
    int nb = 1 << ds;
    if(wr) {
      lbpf_mem_write(&dev, addr, req + 3, nb);
      return 0;
    }
    lbpf_mem_read(&dev, addr, rep, nb);
    wire_bytes += nb + 1;
    return nb;
  }
  return 0;
}

// ---- the host: setsserial's helpers as LBP bytes (hostmot2 sserial.h) ----
static void write_lbp(uint8_t data) {  // LBPNONVOL_flag + LBPWRITE: local write 0xEC
  uint8_t r[2] = {0xEC, data}, x[8];
  device(r, 2, x);
}
static int read_cookie(void) {
  uint8_t r[1] = {0xDF}, x[8];
  device(r, 1, x);
  return x[0];
}
static uint8_t read_byte(uint16_t a) {  // READ_REM_BYTE_CMD 0x44
  uint8_t r[3] = {0x44, (uint8_t)a, (uint8_t)(a >> 8)}, x[8];
  device(r, 3, x);
  return x[0];
}
static void write_byte(uint16_t a, uint8_t v) {  // 0x64
  uint8_t r[4] = {0x64, (uint8_t)a, (uint8_t)(a >> 8), v}, x[8];
  device(r, 4, x);
}
static void write_long(uint16_t a, uint32_t v) {  // 0x66
  uint8_t r[7] = {0x66, (uint8_t)a, (uint8_t)(a >> 8), (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16),
                  (uint8_t)(v >> 24)},
          x[8];
  device(r, 7, x);
}
static void write_double(uint16_t a, const uint8_t *d8) {  // 0x67
  uint8_t r[11] = {0x67, (uint8_t)a, (uint8_t)(a >> 8)}, x[8];
  memcpy(r + 3, d8, 8);
  device(r, 11, x);
}

// setsserial's sslbp_flash() programming loop, step for step
static int setsserial_flash(const uint8_t *fw, uint32_t size) {
  write_lbp(LBPF_NV_FLASH);
  int write_sz = 1 << read_byte(LBPF_WRITE_SIZE);
  int erase_sz = 1 << read_byte(LBPF_ERASE_SIZE);
  write_lbp(0);
  for(uint32_t blocknum = 1; blocknum * erase_sz < size; blocknum++) {
    uint32_t block_start = blocknum * erase_sz, t;
    for(t = 0; t < (uint32_t)erase_sz && fw[block_start + t] == 0; t++) {
    }
    if(t == (uint32_t)erase_sz) {  // all zeros: skipped
      continue;
    }
    write_lbp(LBPF_NV_FLASH);
    write_long(LBPF_OFFSET, block_start);
    write_byte(LBPF_COMMIT, LBPF_CMD_ERASE);
    if(read_cookie() != LBPF_COOKIE) {
      return -1;  // "Synch failed during block erase: aborting"
    }
    write_lbp(0);
    write_lbp(LBPF_NV_FLASH);
    for(int i = 0; i < erase_sz; i += write_sz) {
      write_long(LBPF_OFFSET, block_start + i);
      for(int j = 0; j < write_sz; j += 8) {
        write_double((uint16_t)j, fw + block_start + i + j);
      }
      write_byte(LBPF_COMMIT, LBPF_CMD_WRITE);
      if(read_cookie() != LBPF_COOKIE) {
        return -2;
      }
    }
    write_lbp(0);
  }
  return 0;
}

// ---- in-band staging: 512 kB at its own offsets ----
static uint8_t stage[512 * KB], app[512 * KB];
static int st_erase(void *c, uint32_t off, uint32_t len) {
  memset((uint8_t *)c + off, 0xff, len);
  return 0;
}
static int st_program(void *c, uint32_t off, const uint8_t *d, uint32_t len) {
  uint8_t *m = (uint8_t *)c + off;
  for(uint32_t i = 0; i < len; i++) {
    if((m[i] & d[i]) != d[i]) {
      return -1;
    }
    m[i] &= d[i];
  }
  return 0;
}
static const uint8_t *st_read(void *c, uint32_t off) {
  return (const uint8_t *)c + off;
}
static void b64(const uint8_t *in, int len, char *out) {
  static const char t[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  int o = 0;
  for(int i = 0; i < len; i += 3) {
    uint32_t v = (uint32_t)in[i] << 16 | (i + 1 < len ? (uint32_t)in[i + 1] << 8 : 0) | (i + 2 < len ? in[i + 2] : 0);
    out[o++]   = t[(v >> 18) & 63];
    out[o++]   = t[(v >> 12) & 63];
    out[o++]   = i + 1 < len ? t[(v >> 6) & 63] : '=';
    out[o++]   = i + 2 < len ? t[v & 63] : '=';
  }
  out[o] = 0;
}
static void line(char *buf, const uint8_t *img, uint32_t off, int len) {
  char e[80];
  b64(img + off, len, e);
  sprintf(buf, "%u %s %04x", off, e, fwst_crc16(img + off, len));
}

int main(void) {
  // an image: block 0 placeholder (zeros) + a 300 kB application (no
  // all-zero 16 kB block, as the build has to ensure)
  uint32_t app_size = 300 * KB, size = 16 * KB + app_size;
  uint8_t *fw = calloc(1, size);
  srand(1203);
  for(uint32_t i = 16 * KB; i < size; i++) {
    fw[i] = (uint8_t)(rand() & 0xff) | 1;
  }
  memset(mem, 0x5a, sizeof(mem));  // an old application there
  lbpf_init(&dev);
  dev.erase_log2 = 14, dev.write_log2 = 8, dev.window_size = WIN;
  dev.erase = win_erase, dev.program = win_program;
  int r = setsserial_flash(fw, size);
  ok(r == 0 && !memcmp(mem + 16 * KB, fw + 16 * KB, app_size) && bad_programs == 0,
     "setsserial's flash sequence: the application written, every byte, never onto unerased flash", r);
  ok(sector_erases == 3 && dev.erases == 19 && dev.refused == 0,
     "uniform 16 kB blocks over the F4's 64 / 128 kB sectors: each touched sector erased once", sector_erases);
  int untouched = 1;
  for(uint32_t i = 0; i < 16 * KB; i++) {
    untouched &= mem[i] == 0x5a;
  }
  ok(untouched, "block 0 (the bootloader's) never written", 0);
  printf("# %ld LBP transactions, %ld bytes on the wire: %.0f s at 115200 baud (+ hostmot2's per-command overhead)\n",
         transactions, wire_bytes, wire_bytes * 10.0 / 115200);
  // block 0 refused, and the cookie says so
  write_lbp(LBPF_NV_FLASH);
  write_long(LBPF_OFFSET, 0);
  write_byte(LBPF_COMMIT, LBPF_CMD_ERASE);
  int ck = read_cookie();
  write_lbp(0);
  ok(ck != LBPF_COOKIE && dev.refused == 1, "an erase of block 0 is refused: no cookie, setsserial aborts", ck);
  ok(read_byte(LBPF_ERASE_SIZE) != 14, "out of flash mode, memory accesses are the normal tables again", 0);

  // the caveat: an all-zero block is skipped by setsserial and reads erased (0xff)
  uint8_t *fw2 = malloc(size);
  memcpy(fw2, fw, size);
  memset(fw2 + 16 * KB + 32 * KB, 0, 16 * KB);
  sectors.erased = 0;
  setsserial_flash(fw2, size);
  ok(mem[16 * KB + 32 * KB] == 0xff, "caveat: setsserial skips all-zero blocks; they read erased (the build must avoid "
                                     "them, or the CRC check fails and the bootloader stays)", mem[48 * KB]);

  // ---- in-band: stage, check, apply ----
  uint32_t isize = 100 * KB + 17;
  uint8_t *img = malloc(isize);
  for(uint32_t i = 0; i < isize; i++) {
    img[i] = (uint8_t)(i * 7 + (i >> 8));
  }
  uint32_t crc = fwst_crc32(img, isize, 0);
  fwst_t st = {.stage_size = sizeof(stage), .erase = st_erase, .program = st_program, .read = st_read, .ctx = stage};
  memset(stage, 0, sizeof(stage));
  fwst_begin(&st, isize, crc);
  char buf[160];
  int damaged = 0, repeat = 0, gap = 0;
  for(uint32_t off = 0; off < isize; off += 48) {
    int len = isize - off < 48 ? (int)(isize - off) : 48;
    line(buf, img, off, len);
    if(off == 4800) {  // damaged in transit: refused, sent again
      buf[strlen(buf) - 8] ^= 1;
      damaged = fwst_data(&st, buf);
      line(buf, img, off, len);
    }
    fwst_data(&st, buf);
    if(off == 9600) {
      repeat = fwst_data(&st, buf);  // the same line twice: harmless
    }
  }
  line(buf, img, 0, 48);
  gap = fwst_data(&st, buf);  // out of order now
  int e = fwst_end(&st);
  ok(damaged == -2 && repeat == 0 && gap == -3 && e == 0 && st.state == FWST_READY,
     "staging: a damaged line refused, a repeat harmless, a gap refused, CRC32 of the whole image right", e);
  memset(app, 0, sizeof(app));
  int a = fwst_apply(&st, isize, crc, st_erase, st_program, st_read, app);
  ok(a == 0 && !memcmp(app, img, isize), "the bootloader's copy into the application, verified", a);
  stage[100] ^= 0xff;
  memset(app, 0x33, sizeof(app));
  a = fwst_apply(&st, isize, crc, st_erase, st_program, st_read, app);
  ok(a == -1 && app[0] == 0x33, "a staged image that does not check out is never copied", a);
  fwst_begin(&st, isize, crc ^ 1);
  for(uint32_t off = 0; off < isize; off += 48) {
    line(buf, img, off, isize - off < 48 ? (int)(isize - off) : 48);
    fwst_data(&st, buf);
  }
  ok(fwst_end(&st) < 0 && st.state == FWST_FAILED, "a wrong CRC32 for the image: not ready", st.state);
  printf("# in-band: %u bytes in %u lines (~%u console bytes, ~%.0f s at 1 kB/s)\n", isize, st.lines,
         st.lines * 80, st.lines * 80 / 1000.0);
  printf("1..%d\n# %d/%d passed\n", n, n - fails, n);
  return fails ? 1 : 0;
}
