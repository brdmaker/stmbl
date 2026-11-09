#pragma once
/*
 * fanuc_tw: FANUC 2-wire (1-pair, "αi interface") serial pulsecoders such as
 * the βiA128, the hardware-free part of src/comps/fanuc_tw.c: what one poll's
 * result does to the driver's state. The frame itself is decoded by
 * shared/fanuc1p.c (f1p_frame). See docs/fanuc_tw.md.
 */
#include <stdint.h>
#include "fanuc1p.h"

#define F2W_NOREPLY 1
#define F2W_BAT_BIT 6  // battery missing / low: set while it is, clears when it is back (bench 2026-11-06)  // a poll result besides F1P_OK / F1P_SHORT / F1P_SYNC / F1P_CRC

typedef struct {
  // config
  int max_err;   // this many bad polls in a row: error (FANUC ignores isolated CRC errors)
  int nref_bit;  // frame bit that says "not referenced" (68), -1: none
  // counters
  uint32_t ok, crc_er, sync_er, noreply, bad_run;
  // the last good frame
  f1p_frame_t f;
  int have;      // a good frame since start
  int nref;      // the not-referenced bit of the last good frame
  int bat_fail;  // the battery alarm (frame bit 6) of the last good frame
  int ref_move;  // in: allow running while not referenced, to move through the reference
  int nref_fault;  // not referenced and no ref_move: keep the drive from enabling (fault 20)
  int state;     // 0 none / error, 1 good but not referenced (incremental), 3 good and referenced (absolute)
  int error;
  // debug: bits of each word that changed since the last f2w_chg_reset
  uint16_t chg[5];
} f2w_t;

void f2w_init(f2w_t *d, int max_err, int nref_bit);
// one poll: res F1P_OK (f is the new frame), F1P_SHORT, F1P_SYNC, F1P_CRC or F2W_NOREPLY
void f2w_poll(f2w_t *d, int res, const f1p_frame_t *f);
void f2w_chg_reset(f2w_t *d);
// the status bits under the masks (w0, w3, w4) differ between a and b
int f2w_status_differs(const f1p_frame_t *a, const f1p_frame_t *b, uint16_t m0, uint16_t m3, uint16_t m4);
