/*
 * In-band firmware update over the console (the smart-serial console while
 * LinuxCNC runs, or USB): the application receives the new image into a
 * staging area of its own flash; the bootloader copies it into place at the
 * next reset, only if it checks out. See docs/sserial_bootloader.md.
 *
 * Console lines (servoterm-cli --update sends them):
 *   fwbegin SIZE CRC32           start (or resume: the same SIZE and CRC32)
 *   fwdata OFFSET BASE64 CRC16   up to 60 bytes at OFFSET (fwup: 30; in order; a repeat of
 *                                the last line is fine, a gap is not)
 *   fwend                        CRC32 over the staged image: ok or not
 *   fwapply                      mark the update pending, then reset
 *
 * The flash: STM32F4 and G4 have one bank, and erasing or programming it
 * stalls every instruction fetch from flash, interrupts included. An F4
 * 128 kB sector erase takes 1-2 s: the control loop and the smart-serial
 * link would stop. So the application never erases: the bootloader leaves
 * the staging area blank (fwst_boot), fwbegin only checks that it is, and
 * the received bytes go through a small queue that the realtime loop
 * programs a word at a time (fwst_poll), where the stall (~16 us typical,
 * 100 us max per word) fits into the period's slack.
 *
 * The pending mark is a header at the end of the staging area (24 bytes):
 * magic, size, CRC32 at 0..11, then a state unit at 16 that stays all ones
 * while the update is pending. The bootloader programs it to 0 when done:
 * bits only go 1 -> 0, no erase. The state has a programming unit of its
 * own because some flash programs each unit once only (the G4's 64-bit
 * double words, with ECC).
 *
 * prog_size is the flash's programming unit: 4 (F4: words) or 8 (G4:
 * double words). Everything programmed is whole, aligned units; a tail is
 * padded with 0xff.
 */
#pragma once
#include <stdint.h>

enum { FWST_IDLE, FWST_RECEIVING, FWST_READY, FWST_MARKING, FWST_MARKED, FWST_FAILED };

#define FWST_MAGIC 0x53535550u  // "PUSS": sserial update, pending
#define FWST_HDR_SIZE 24u  // magic, size, crc32, (ones), state unit at 16
#define FWST_HDR_STATE 16u
#define FWST_QSIZE 512u

typedef struct {
  uint32_t magic, size, crc, pad;
  uint32_t state, pad2;  // state: 0xffffffff pending, 0 done
} fwst_hdr_t;

typedef struct {
  // configuration
  uint32_t stage_size;  // bytes, the header's 16 at the end included
  uint32_t max_size;    // largest image (the application area)
  int (*erase)(void *ctx, uint32_t off, uint32_t len);  // staging offsets (bootloader only)
  uint32_t prog_size;   // programming unit: 4 or 8 (0: 4)
  int (*program)(void *ctx, uint32_t off, const uint8_t *d, uint32_t len);  // one unit, aligned
  const uint8_t *(*read)(void *ctx, uint32_t off);      // staging bytes, mapped
  void *ctx;
  // state
  int state;
  uint32_t size, crc, got;
  uint32_t lines, bad;
  uint8_t q[FWST_QSIZE];  // received, not yet programmed: staging [done, got)
  uint32_t done;
  uint32_t hdr_units;      // header units programmed (FWST_MARKING)
} fwst_t;

uint32_t fwst_crc32(const uint8_t *d, uint32_t n, uint32_t crc);  // IEEE, crc = 0 to start
uint16_t fwst_crc16(const uint8_t *d, int n);                     // CCITT-FALSE (as scopedump)

// application side (no erase; fwst_poll from the realtime loop)
// 0 started, 1 resumed (got bytes already staged), < 0 refused:
// -1 size, -2 staging not blank (reset the drive: the bootloader erases it)
int fwst_begin(fwst_t *s, uint32_t size, uint32_t crc32);
// "OFFSET BASE64 CRC16" (the text after "fwdata "): 0 taken, < 0 refused:
// -1 syntax / state, -2 bad CRC16, -3 gap, -4 flash failed, -5 queue full
// (send it again)
int fwst_data(fwst_t *s, const char *line);
// 0: staged image complete and its CRC32 right; 1: still programming (ask
// again); < 0 failed
int fwst_end(fwst_t *s);
// queue the pending mark (needs FWST_READY); fwst_poll writes it, then the
// state is FWST_MARKED and the application resets
int fwst_mark(fwst_t *s);
// program up to max_units units of queued data or header: the realtime
// loop's share; returns units programmed
int fwst_poll(fwst_t *s, int max_units);

// bootloader side
int fwst_pending(const fwst_t *s, fwst_hdr_t *h);  // 1: a pending update
// at reset: apply a pending update (copy, verify, clear the mark; a bad
// staged image is cleared, not copied; a failed copy stays pending and is
// tried again), then erase the staging area if it is not blank.
// 0 nothing to do, 1 applied, -1 staged image bad, -2 copy failed
int fwst_boot(fwst_t *s, int (*app_erase)(void *, uint32_t, uint32_t),
              int (*app_program)(void *, uint32_t, const uint8_t *, uint32_t),
              const uint8_t *(*app_read)(void *, uint32_t), void *app_ctx);
// copy size bytes from the staging area to the application, then verify
int fwst_apply(const fwst_t *stage, uint32_t size, uint32_t crc32,
               int (*app_erase)(void *, uint32_t, uint32_t),
               int (*app_program)(void *, uint32_t, const uint8_t *, uint32_t),
               const uint8_t *(*app_read)(void *, uint32_t), void *app_ctx);
