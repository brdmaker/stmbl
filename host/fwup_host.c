/*
 * The fwup component's platform in the simulator (see shared/fwup.h): the
 * staging area and the application area are RAM, and a reset runs what the
 * bootloader would (fwst_boot) and reports it; the simulation goes on.
 */
#include "fwup.h"
#include "fwstage.h"
#include <stdio.h>
#include <string.h>

#define KB 1024u
const uint32_t fwup_stage_size = 512 * KB;
const uint32_t fwup_max_size   = 448 * KB;
static uint8_t stage[512 * KB], app[448 * KB];
static int ready;
static uint32_t words;

static void init(void) {
  if(!ready) {
    memset(stage, 0xff, sizeof(stage));  // the bootloader left it blank
    ready = 1;
  }
}

int fwup_stage_program(uint32_t off, const uint8_t *d, uint32_t len) {
  init();
  if(len != 4 || (off & 3) || off + len > sizeof(stage)) {
    return -1;
  }
  for(uint32_t i = 0; i < len; i++) {
    if((stage[off + i] & d[i]) != d[i]) {
      return -1;  // 0 -> 1 without an erase
    }
    stage[off + i] &= d[i];
  }
  words++;
  return 0;
}

const uint8_t *fwup_stage_read(uint32_t off) {
  init();
  return stage + off;
}

static int b_erase(void *c, uint32_t off, uint32_t len) {
  memset((uint8_t *)c + off, 0xff, len);
  return 0;
}
static int b_program(void *c, uint32_t off, const uint8_t *d, uint32_t len) {
  memcpy((uint8_t *)c + off, d, len);
  return 0;
}
static const uint8_t *b_read(void *c, uint32_t off) {
  return (const uint8_t *)c + off;
}

void fwup_reset(uint32_t trigger) {
  init();
  fwst_t s = {.stage_size = fwup_stage_size, .max_size = fwup_max_size, .erase = b_erase, .program = b_program,
              .read = b_read, .ctx = stage};
  fwst_hdr_t h;
  int pending = fwst_pending(&s, &h);
  int r       = fwst_boot(&s, b_erase, b_program, b_read, app);
  printf("sim: reset (%s), %u staging words programmed\n",
         trigger == FWUP_TRIGGER_SSERIAL ? "smart-serial loader" : "plain", (unsigned)words);
  if(pending) {
    printf("sim: bootloader: %s, %u bytes, crc32 %08x\n",
           r == 1 ? "update applied" : r == -1 ? "staged image bad, not copied" : "copy failed", (unsigned)h.size,
           (unsigned)fwst_crc32(app, h.size, 0));
  } else {
    printf("sim: bootloader: nothing staged\n");
  }
  if(trigger == FWUP_TRIGGER_SSERIAL) {
    printf("sim: bootloader: waits for setsserial at 115200 baud (not simulated)\n");
  }
  words = 0;
}
