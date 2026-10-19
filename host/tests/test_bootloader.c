/*
 * The smart-serial bootloader sketch (docs/sserial_bootloader.md):
 * shared/lbp_flash.c driven exactly as LinuxCNC's setsserial flashes a
 * remote, and shared/fwstage.c (the in-band update over the console),
 * both on a model of the STM32F4's flash (bits only go 1 -> 0 without an
 * erase; uneven sectors). Prints TAP.
 */
#include "lbp_flash.h"
#include "fwstage.h"
#include "lbp_dev.h"
#include "crc8.h"
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
// window offset 0 = one 16 kB block below the application (block 0, never
// written); the application (0x08010000) starts at 16 kB: sector 4 (64 kB),
// sectors 5..7 (128 kB each); sectors 8..11 are the staging area, outside
#define KB 1024u
#define WIN (16 * KB + 448 * KB)
static uint8_t mem[WIN];
static int bad_programs;
static const uint32_t sec_start[] = {16 * KB, 80 * KB, 208 * KB, 336 * KB, 464 * KB};

static int erase_sector(void *ctx, int s) {
  (void)ctx;
  memset(mem + sec_start[s], 0xff, sec_start[s + 1] - sec_start[s]);
  return 0;
}
static lbpf_sectors_t sectors = {sec_start, 4, mem, 16 * KB, erase_sector, NULL, 0};
static int win_erase(void *ctx, uint32_t off, uint32_t len) {
  (void)ctx;
  return lbpf_erase_lazy(&sectors, off, len);
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

// ---- the device: shared/lbp_dev.c, fed byte by byte; its flash traffic
// goes to lbp_flash ----
static lbpf_t dev;
static lbpd_t lbp;
static long transactions, wire_bytes, bad_reply_crc;
static uint8_t rx[64];
static int rxn;
static void dev_send(void *ctx, const uint8_t *d, int len) {
  (void)ctx;
  memcpy(rx + rxn, d, len);
  rxn += len;
}
static uint8_t crc8(const uint8_t *p, int len) {
  return (uint8_t)crc8_finalize(crc8_update(crc8_init(), p, len));
}
// one LBP transaction as hostmot2 sends it: request + CRC8 (local writes
// and the RPCs too), the reply checked; returns the reply's data bytes
static int writes_seen, write_ack_bad;
static int device(const uint8_t *req, int len, uint8_t *rep) {
  uint8_t w[32];
  memcpy(w, req, len);
  w[len] = crc8(req, len);
  transactions++;
  rxn = 0;
  for(int i = 0; i <= len; i++) {
    lbpd_byte(&lbp, w[i]);
  }
  wire_bytes += len + 1 + rxn;
  uint8_t c = req[0];
  if((c >> 5 & 1) && ((c >> 6) == 3 || (c >> 6) == 1)) {
    // a local or memory write: SSLBP waits for exactly one byte, the CRC of
    // no data (0x00), and times out (error flag) without it
    writes_seen++;
    if(rxn != 1 || rx[0] != 0) {
      write_ack_bad++;
    }
    return 0;
  }
  if(rxn == 0) {
    return 0;
  }
  if(crc8(rx, rxn - 1) != rx[rxn - 1]) {
    bad_reply_crc++;
  }
  memcpy(rep, rx, rxn - 1);
  return rxn - 1;
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

static int stalled_commits;
// setsserial's sslbp_flash() programming loop, step for step
static int setsserial_flash(const uint8_t *fw, uint32_t size, int max_blocks) {
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
    if(max_blocks-- == 0) {
      return -3;  // the run interrupted (cable, ^C)
    }
    uint32_t erases_before = sectors.sector_erases;
    write_lbp(LBPF_NV_FLASH);
    write_long(LBPF_OFFSET, block_start);
    write_byte(LBPF_COMMIT, LBPF_CMD_ERASE);
    if(read_cookie() != LBPF_COOKIE) {
      return -1;  // "Synch failed during block erase: aborting"
    }
    if(sectors.sector_erases != erases_before) {
      stalled_commits++;  // a real erase: 1-2 s, past hostmot2's 1 s wait
      return -4;          // "Timeout waiting for CMD to clear"
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

// ---- in-band staging: sectors 8..11 (512 kB), the header at the end ----
static uint8_t stage[512 * KB], app[512 * KB];
static int stage_erases, stage_words;
static int st_erase(void *c, uint32_t off, uint32_t len) {
  if(c == stage) {
    stage_erases++;
  }
  memset((uint8_t *)c + off, 0xff, len);
  return 0;
}
static int st_program(void *c, uint32_t off, const uint8_t *d, uint32_t len) {
  uint8_t *m = (uint8_t *)c + off;
  if(c == stage) {
    stage_words++;
    if(len != 4 || (off & 3)) {
      return -1;  // the application programs words only
    }
  }
  for(uint32_t i = 0; i < len; i++) {
    if((m[i] & d[i]) != d[i]) {
      return -1;
    }
    m[i] &= d[i];
  }
  return 0;
}
// the G4's flash: 8-byte aligned units, each programmed once until erased
static uint8_t *g4_done;
static int g4_bad;
static int g4_erase(void *c, uint32_t off, uint32_t len) {
  memset((uint8_t *)c + off, 0xff, len);
  memset(g4_done + off / 8, 0, len / 8);
  return 0;
}
static int g4_program(void *c, uint32_t off, const uint8_t *d, uint32_t len) {
  if(len != 8 || (off & 7) || g4_done[off / 8]) {
    g4_bad++;
    return -1;
  }
  g4_done[off / 8] = 1;
  memcpy((uint8_t *)c + off, d, 8);
  return 0;
}
// the application area as the bootloader writes it: any length
static int st_program_any(void *c, uint32_t off, const uint8_t *d, uint32_t len) {
  memcpy((uint8_t *)c + off, d, len);
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
// the sender (servoterm-cli --update): a line, again while the queue is full;
// the realtime loop programs 1 word per period meanwhile
static int send_line(fwst_t *st, const char *buf, int *full) {
  int r;
  while((r = fwst_data(st, buf)) == -5) {
    (*full)++;
    fwst_poll(st, 1);
  }
  fwst_poll(st, 1);
  return r;
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
  lbp = (lbpd_t){.mem = lbpd_empty_mem, .mem_size = sizeof(lbpd_empty_mem), .name = {0},
                 .unit = 0x12345678, .flash = &dev, .send = dev_send};
  memcpy(lbp.name, LBPD_LOADER_NAME, 4);
  lbpd_init(&lbp);
  // hostmot2 at 115200 (setup mode) keeps a remote only if its name's second
  // byte is 'I'; the HAL name is the name with that byte lower-cased
  char hal[5] = LBPD_LOADER_NAME;
  hal[1] |= 0x20;
  ok(LBPD_LOADER_NAME[1] == 0x49 && !strcmp(hal, "sibl"),
     "loader name: second byte 'I' for hostmot2's setup mode, HAL name sibl", LBPD_LOADER_NAME[1]);
  // hostmot2's enumeration of the remote: name, unit, discovery, tables
  uint8_t x[16];
  char nm[5] = {0};
  for(int i = 0; i < 4; i++) {
    uint8_t q = (uint8_t)(0xd0 + i);
    device(&q, 1, x);
    nm[i] = (char)x[0];
  }
  uint8_t q = 0xbc;
  int un = device(&q, 1, x);
  uint32_t unit;
  memcpy(&unit, x, 4);
  q = 0xbb;
  int dn = device(&q, 1, x);
  ok(!strcmp(nm, LBPD_LOADER_NAME) && un == 4 && unit == 0x12345678 && dn == 6 && x[0] == 1 && x[1] == 0 && x[2] == 8,
     "enumeration: name sIbl, unit number, discovery (1 byte in, none out)", dn);
  uint8_t ptoc[3] = {0x44, 8, 0};  // READ_REM_BYTE at the PTOC: 0 = end of list
  int pn = device(ptoc, 3, x);
  uint8_t pd[2] = {0xbd, 0};       // process data: no output bytes, 1 input byte
  rxn = 0;
  lbpd_byte(&lbp, 0xbd);
  lbpd_byte(&lbp, crc8(pd, 1));
  ok(pn == 1 && x[0] == 0 && rxn == 2 && rx[0] == 0 && rx[1] == crc8(rx, 1) && bad_reply_crc == 0,
     "empty PTOC/GTOC; process data answered with the fault byte", rxn);
  uint8_t badreq[4] = {0x44, 0x05, 0x80, 0};
  badreq[3] = crc8(badreq, 3) ^ 0x55;
  rxn = 0;
  for(int i = 0; i < 4; i++) {
    lbpd_byte(&lbp, badreq[i]);
  }
  ok(rxn == 0 && lbp.crc_errors == 1, "a request with a bad CRC gets no reply", rxn);
  // the bootloader starts: it erases the application area before it
  // answers anything (no erase may stall a commit later)
  lbpf_erase_all(&sectors);
  uint32_t entry_erases = sectors.sector_erases;
  int r = setsserial_flash(fw, size, -1);
  ok(r == 0 && !memcmp(mem + 16 * KB, fw + 16 * KB, app_size) && bad_programs == 0,
     "setsserial's flash sequence: the application written, every byte, never onto unerased flash", r);
  ok(bad_reply_crc == 0 && lbp.crc_errors == 1, "every reply's CRC8 right", bad_reply_crc);
  ok(entry_erases == 4 && sectors.sector_erases == 4 && stalled_commits == 0 && dev.erases == 19 && dev.refused == 0,
     "application area erased at loader entry (4 sectors); 19 erase commits find blank blocks, none stalls", sectors.sector_erases);
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

  // a run interrupted after 7 blocks, then another image in the same session:
  // its block 1 finds stale data, erases everything (that commit stalls:
  // setsserial times out once), and the run after it goes through
  uint8_t *fw3 = malloc(size);
  for(uint32_t i = 0; i < size; i++) {
    fw3[i] = i < 16 * KB ? 0 : (uint8_t)(fw[i] ^ 0x80);
  }
  lbpf_erase_all(&sectors);
  r = setsserial_flash(fw, size, 7);
  stalled_commits = 0;
  int r2 = setsserial_flash(fw3, size, -1);
  int st1 = stalled_commits;
  stalled_commits = 0;
  int r3 = setsserial_flash(fw3, size, -1);
  ok(r == -3 && r2 == -4 && st1 == 1 && r3 == 0 && stalled_commits == 0 &&
         !memcmp(mem + 16 * KB, fw3 + 16 * KB, app_size) && bad_programs == 0,
     "after an interrupted run, the next run erases at block 1 and times out once; the one after goes through", st1);

  // the caveat: an all-zero block is skipped by setsserial and reads erased (0xff)
  uint8_t *fw2 = malloc(size);
  memcpy(fw2, fw, size);
  memset(fw2 + 16 * KB + 32 * KB, 0, 16 * KB);
  lbpf_erase_all(&sectors);
  setsserial_flash(fw2, size, -1);
  ok(mem[16 * KB + 32 * KB] == 0xff, "caveat: setsserial skips all-zero blocks; they read erased (the build must avoid "
                                     "them, or the CRC check fails and the bootloader stays)", mem[48 * KB]);

  // ---- in-band: stage (no erase, queued words), check, mark, apply at boot ----
  uint32_t isize = 100 * KB + 17;
  uint8_t *img = malloc(isize);
  for(uint32_t i = 0; i < isize; i++) {
    img[i] = (uint8_t)(i * 7 + (i >> 8));
  }
  uint32_t crc = fwst_crc32(img, isize, 0);
  fwst_t st = {.stage_size = sizeof(stage), .max_size = 448 * KB, .erase = st_erase, .program = st_program,
               .read = st_read, .ctx = stage};
  memset(stage, 0x77, sizeof(stage));  // whatever was there
  int b0 = fwst_begin(&st, isize, crc);
  memset(app, 0x33, sizeof(app));
  int boot0 = fwst_boot(&st, st_erase, st_program, st_read, app);
  ok(b0 == -2 && boot0 == 0 && stage_erases == 1 && app[0] == 0x33 && fwst_begin(&st, isize, crc) == 0,
     "fwbegin refuses a staging area that is not blank; the bootloader erases it at reset (nothing to apply)", b0);
  stage_erases = 0;
  char buf[160];
  int damaged = 0, repeat = 0, gap = 0, full = 0, resumed = 0;
  for(uint32_t off = 0; off < isize; off += 48) {
    int len = isize - off < 48 ? (int)(isize - off) : 48;
    line(buf, img, off, len);
    if(off == 4800) {  // damaged in transit: refused, sent again
      buf[strlen(buf) - 8] ^= 1;
      damaged = fwst_data(&st, buf);
      line(buf, img, off, len);
    }
    if(off == 6000) {  // the sender restarts: fwbegin again resumes
      resumed = fwst_begin(&st, isize, crc) == 1 && st.got == off;
    }
    send_line(&st, buf, &full);
    if(off == 9600) {
      repeat = fwst_data(&st, buf);  // the same line twice: harmless
    }
  }
  line(buf, img, 0, 48);
  gap = fwst_data(&st, buf);  // out of order now
  int busy = fwst_end(&st), e, polls = 0;
  while((e = fwst_end(&st)) == 1) {
    polls += fwst_poll(&st, 1);
  }
  ok(damaged == -2 && repeat == 0 && gap == -3 && resumed && full > 0 && busy == 1 && e == 0 && st.state == FWST_READY,
     "staging: damaged line refused, repeat harmless, gap refused, resume, queue full -> resend, CRC32 right", e);
  uint32_t lines = st.lines;
  ok(stage_erases == 0 && stage_words == (int)((isize + 3) / 4),
     "the application never erases; it programs one word per realtime period", stage_words);
  fwst_hdr_t h;
  int m = fwst_mark(&st), pend0 = fwst_pending(&st, &h);
  while(st.state == FWST_MARKING) {
    fwst_poll(&st, 1);
  }
  int pend1 = fwst_pending(&st, &h);
  ok(m == 0 && !pend0 && st.state == FWST_MARKED && pend1 && h.size == isize && h.crc == crc,
     "fwapply: the pending mark (header at the end of staging) written by the realtime loop", pend1);
  {
    // F4: the header every F4 bootloader in the field reads (v0.9.18-312,
    // before the G4 layout): magic, size, crc32, state at the last 16 bytes.
    // An application writing another layout is never copied by them.
    uint32_t w[4];
    memcpy(w, stage + sizeof(stage) - 16, sizeof(w));
    ok(w[0] == FWST_MAGIC && w[1] == isize && w[2] == crc && w[3] == 0xffffffffu,
       "F4 pending mark in the layout of the bootloaders in the field (last 16 bytes: magic, size, crc32, state)",
       w[0]);
  }
  // power fails in the middle of the copy: the bootloader copies again
  memset(app, 0x33, 1000);
  int boot1 = fwst_boot(&st, st_erase, st_program, st_read, app);
  int pend2 = fwst_pending(&st, &h);
  int boot2 = fwst_boot(&st, st_erase, st_program, st_read, app);
  int blank = 1;
  for(uint32_t i = 0; i < sizeof(stage); i++) {
    blank &= stage[i] == 0xff;
  }
  ok(boot1 == 1 && !memcmp(app, img, isize) && !pend2 && boot2 == 0 && blank && stage_erases == 1,
     "the bootloader copies, verifies, clears the mark and erases staging for the next update", boot1);
  // a staged image that does not check out is never copied
  fwst_begin(&st, isize, crc);
  for(uint32_t off = 0; off < isize; off += 48) {
    line(buf, img, off, isize - off < 48 ? (int)(isize - off) : 48);
    send_line(&st, buf, &full);
  }
  while(fwst_end(&st) == 1) {
    fwst_poll(&st, 1);
  }
  fwst_mark(&st);
  fwst_poll(&st, 3);
  stage[100] ^= 0x01;  // a bit flips afterwards (st_program would refuse 0->1; the model allows it)
  memset(app, 0x33, sizeof(app));
  int boot3 = fwst_boot(&st, st_erase, st_program, st_read, app);
  ok(boot3 == -1 && app[0] == 0x33 && !fwst_pending(&st, &h), "a staged image that does not check out is never copied; the mark is cleared",
     boot3);
  // a wrong CRC32 for the image
  fwst_begin(&st, isize, crc ^ 1);
  for(uint32_t off = 0; off < isize; off += 48) {
    line(buf, img, off, isize - off < 48 ? (int)(isize - off) : 48);
    send_line(&st, buf, &full);
  }
  while((e = fwst_end(&st)) == 1) {
    fwst_poll(&st, 1);
  }
  ok(e < 0 && st.state == FWST_FAILED && fwst_mark(&st) < 0, "a wrong CRC32 for the image: not ready, no mark", st.state);
  ok(fwst_begin(&st, 449 * KB, crc) == -1, "an image larger than the application area is refused", 0);

  // ---- G4 flash: 64-bit units, each programmable once (ECC) ----
  static uint8_t g4[256 * KB], g4app[256 * KB], g4done[256 * KB / 8];
  memset(g4, 0xff, sizeof(g4));
  memset(g4done, 0, sizeof(g4done));
  g4_bad = 0;
  fwst_t g = {.stage_size = sizeof(g4), .max_size = 224 * KB, .prog_size = 8, .erase = g4_erase, .program = g4_program,
              .read = st_read, .ctx = g4};
  g4_done = g4done;
  int gb = fwst_begin(&g, isize, crc);
  for(uint32_t off = 0; off < isize; off += 48) {
    line(buf, img, off, isize - off < 48 ? (int)(isize - off) : 48);
    send_line(&g, buf, &full);
  }
  while((e = fwst_end(&g)) == 1) {
    fwst_poll(&g, 1);
  }
  fwst_mark(&g);
  while(g.state == FWST_MARKING) {
    fwst_poll(&g, 1);
  }
  int gp = fwst_pending(&g, &h);
  int gboot = fwst_boot(&g, st_erase, st_program_any, st_read, g4app);
  ok(gb == 0 && e == 0 && gp && gboot == 1 && !memcmp(g4app, img, isize) && g4_bad == 0 && !fwst_pending(&g, &h),
     "G4 flash (8-byte units, each once): staged, marked, applied and marked done, no unit programmed twice", g4_bad);
  printf("# in-band: %u bytes in %u lines (~%u console bytes, ~%.0f s at 1 kB/s)\n", isize, lines, lines * 80,
         lines * 80 / 1000.0);
  // SSLBP (Mesa's smart-serial firmware, SSLBP.SUB recvstring) waits for one
  // reply byte to every write; the loader sent none to memory writes, so
  // on the bench each write of setsserial timed out with the error flag
  ok(writes_seen > 100 && write_ack_bad == 0, "every write (local and memory) answered with exactly one 0x00", write_ack_bad);
  {
    uint8_t bad[3] = {0xec, 0x02, 0};  // a local write with a wrong CRC: no reply
    bad[2]         = (uint8_t)(crc8(bad, 2) ^ 0x55);
    rxn            = 0;
    for(int i = 0; i < 3; i++) {
      lbpd_byte(&lbp, bad[i]);
    }
    ok(rxn == 0, "a local write with a bad CRC gets no reply (SSLBP times out cleanly)", rxn);
  }
  printf("1..%d\n# %d/%d passed\n", n, n - fails, n);
  return fails ? 1 : 0;
}
