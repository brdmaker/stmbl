/*
 * In-band firmware update over the console (the smart-serial console while
 * LinuxCNC runs, or USB): the application receives the new image into a
 * staging area of its own flash; the bootloader copies it into place at the
 * next reset, only if it checks out. Sketch, see
 * docs/sserial_bootloader.md.
 *
 * Console lines (servoterm-cli would send them):
 *   fwbegin SIZE CRC32           erase the staging area for SIZE bytes
 *   fwdata OFFSET BASE64 CRC16   48 bytes at OFFSET (in order; a repeat of
 *                                the last line is fine, a gap is not)
 *   fwend                        CRC32 over the staged image: ok or not
 * Then the application marks the update pending and resets; the bootloader
 * copies (fwst_apply), verifies and clears the mark, resuming the copy if
 * power failed during it (the staged image is still there and good).
 */
#pragma once
#include <stdint.h>

enum { FWST_IDLE, FWST_RECEIVING, FWST_READY, FWST_FAILED };

typedef struct {
  uint32_t stage_size;  // bytes available
  int (*erase)(void *ctx, uint32_t off, uint32_t len);  // staging offsets
  int (*program)(void *ctx, uint32_t off, const uint8_t *d, uint32_t len);
  const uint8_t *(*read)(void *ctx, uint32_t off);      // staging bytes, mapped
  void *ctx;
  // state
  int state;
  uint32_t size, crc, got;
  uint32_t lines, bad;
} fwst_t;

uint32_t fwst_crc32(const uint8_t *d, uint32_t n, uint32_t crc);  // IEEE, crc = 0 to start
uint16_t fwst_crc16(const uint8_t *d, int n);                     // CCITT-FALSE (as scopedump)
int fwst_begin(fwst_t *s, uint32_t size, uint32_t crc32);
// "OFFSET BASE64 CRC16" (the text after "fwdata "): 0 taken, < 0 refused
int fwst_data(fwst_t *s, const char *line);
int fwst_end(fwst_t *s);  // 0: staged image complete and its CRC32 right
// bootloader: copy size bytes from the staging read() to the application
// (its erase / program at application offsets), then verify
int fwst_apply(const fwst_t *stage, uint32_t size, uint32_t crc32,
               int (*app_erase)(void *, uint32_t, uint32_t),
               int (*app_program)(void *, uint32_t, const uint8_t *, uint32_t),
               const uint8_t *(*app_read)(void *, uint32_t), void *app_ctx);
