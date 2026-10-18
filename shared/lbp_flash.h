/*
 * Mesa's remote flash protocol over smart-serial (LinuxCNC's
 * `setsserial flash`), as a device implements it: the piece a bootloader
 * adds to its LBP (smart-serial byte protocol) parser. Sketch, see
 * docs/sserial_bootloader.md.
 *
 * On the wire (hostmot2's sserial.h, setsserial.c):
 *   local write 0xEC, data 0x02     non-volatile access: flash (0: off)
 *   in flash mode, memory accesses (LBP read/write with a 2 byte address)
 *   go to a flash window instead of the process data tables:
 *     0x8000 (4 bytes)   offset in the window of the next erase / write
 *     0x8004 (1, read)   log2 of the erase block size
 *     0x8005 (1, read)   log2 of the write block size
 *     0x8007 (1, write)  commit: 0xFE erase the block at offset, 0xFD write
 *                        the buffer to offset
 *     0 .. write size-1  the write buffer (setsserial: 8 bytes at a time)
 *   local read 0xDF (cookie) after each commit: 0x5A once it is done; any
 *   other value and setsserial aborts.
 * setsserial never touches block 0 (it is the device's bootloader) and
 * skips blocks of all zero bytes; it insists on 115200 baud.
 *
 * The window: offset 0 is one erase block below the application, so block
 * 1 is the application's start. The platform gives erase / program of the
 * window (the uniform blocks presented need not be the flash's sectors:
 * lbpf_erase_lazy for uneven ones).
 */
#pragma once
#include <stdint.h>

#define LBPF_NV_LOCAL_WRITE 0xEC
#define LBPF_NV_FLASH 0x02
#define LBPF_COOKIE_CMD 0xDF
#define LBPF_COOKIE 0x5A
#define LBPF_OFFSET 0x8000
#define LBPF_ERASE_SIZE 0x8004
#define LBPF_WRITE_SIZE 0x8005
#define LBPF_COMMIT 0x8007
#define LBPF_CMD_ERASE 0xFE
#define LBPF_CMD_WRITE 0xFD
#define LBPF_MAX_WRITE 256

typedef struct {
  // configuration
  uint8_t erase_log2, write_log2;  // presented block sizes
  uint32_t window_size;            // bytes, block 0 included
  // erase [off, off + len) / program len bytes at off (window offsets);
  // 0 ok, < 0 failed
  int (*erase)(void *ctx, uint32_t off, uint32_t len);
  int (*program)(void *ctx, uint32_t off, const uint8_t *d, uint32_t len);
  void *ctx;
  // state
  uint8_t nv;      // 0, or LBPF_NV_FLASH
  uint32_t offset;
  uint8_t buf[LBPF_MAX_WRITE];
  uint8_t ok;      // the last commit succeeded (the cookie tells)
  uint32_t erases, writes, refused;
} lbpf_t;

void lbpf_init(lbpf_t *f);
// a local write (cmd, data); 1 if it was ours
int lbpf_local_write(lbpf_t *f, uint8_t cmd, uint8_t data);
// a local read: 1 if ours, *out the reply
int lbpf_local_read(lbpf_t *f, uint8_t cmd, uint8_t *out);
// memory write / read at a 2-byte address while in flash mode: 1 if ours
// (in flash mode every access is), 0 = the normal tables
int lbpf_mem_write(lbpf_t *f, uint16_t addr, const uint8_t *d, int n);
int lbpf_mem_read(lbpf_t *f, uint16_t addr, uint8_t *d, int n);

// Uneven physical sectors behind uniform blocks, and the timing problem:
// on a single-bank part (STM32F4, G4) an erase stalls the whole chip, and
// an F4 128 kB sector takes 1-2 s, longer than hostmot2 waits for the
// cookie after a commit (setsserial: 1 s). So the bootloader erases the
// application area when it starts (lbpf_erase_all, before it answers
// anything), and an erase commit (lbpf_erase_lazy) finds the block blank
// and returns at once. Only stale data costs an erase: at block 1 (a new
// setsserial run after a failed one) every sector, otherwise the block's
// sector. setsserial writes blocks in order, so stale data in a sector
// lies in front of what this run wrote there; that commit may time out,
// and setsserial run again then finds everything blank.
typedef struct {
  const uint32_t *sector_start;  // window offsets, ascending, n + 1 entries (end)
  int n;
  const uint8_t *map;            // the window's bytes, readable
  uint32_t first;                // window offset of block 1
  int (*erase_sector)(void *ctx, int sector);
  void *ctx;
  uint32_t sector_erases;
} lbpf_sectors_t;
int lbpf_erase_all(lbpf_sectors_t *s);
int lbpf_erase_lazy(lbpf_sectors_t *s, uint32_t off, uint32_t len);
