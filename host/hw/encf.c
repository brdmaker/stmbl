/*
 * Simulation stand-in for src/comps/encf.c: a FANUC alpha-type serial
 * absolute encoder (2^22 counts per turn, a 16-bit turn counter, a battery)
 * on the simulated motor shaft. Same pins as the real component, plus sim_*.
 *
 * The encoder is on the shaft: its angle is the shaft's unwrapped position
 * (sim_abs_pos, linked to hv0.sim_upos) plus a whole number of turns kept
 * in static memory (its battery-backed turn counter), so the single-turn
 * angle always matches the rotor, as commutation needs. Over a drive
 * `reset` the shaft keeps its place with hv0.sim_keep_pos = 1 (and can move
 * while off: hv0.sim_off_move); the encoder follows it with its battery.
 *   sim_batt      0: no battery while the drive is off (acts at the next
 *                 reset; kept in static memory, since a reset sets every
 *                 pin back): the turn count starts from 0 and the encoder
 *                 is un-indexed (mt_ok 0) until its angle passes 0 (the
 *                 index), then indexed again with that new, wrong count, as
 *                 a real one would be
 *   sim_turns     a whole number of turns added to the encoder's count
 *                 (another machine position for the same shaft angle; acts
 *                 at once, like a motor fitted at another turn)
 *   sim_batt_low  1: the encoder reports a battery failure (batt)
 *   sim_turn_err  added to the turn count it sends (a fault)
 *   sim_fail      1: no frames (error)
 *   sim_rev       1: the encoder counts the other way (conf0.mot_fb_rev)
 *   sim_pp        pole pairs, for com_pos (electrical angle)
 *   sim_enc       out: the encoder's position [turns]
 */
#include "encf_comp.h"
#include "hal.h"
#include "defines.h"
#include "angle.h"
#include <math.h>

HAL_COMP(encf);

// same pins as src/comps/encf.c
HAL_PIN(error);
HAL_PIN(dma);
HAL_PIN(pos);
HAL_PIN(abs_pos);
HAL_PIN(state);
HAL_PIN(turns);
HAL_PIN(mt_ok);
HAL_PIN(batt);
HAL_PIN(com_pos);
HAL_PIN(index);
HAL_PIN(req_len);
HAL_PIN(send_step);
HAL_PIN(crc_ok);
HAL_PIN(crc_er);
HAL_PIN(freq);
HAL_PIN(bit_ticks);

// simulation
HAL_PIN(sim_pos);      // shaft angle [rad] (hv0.sim_pos), for com_pos
HAL_PIN(sim_abs_pos);  // unwrapped shaft position [rad] (hv0.sim_upos)
HAL_PIN(sim_rev);
HAL_PIN(sim_pp);
HAL_PIN(sim_batt);
HAL_PIN(sim_turns);
HAL_PIN(sim_batt_low);
HAL_PIN(sim_turn_err);
HAL_PIN(sim_fail);
HAL_PIN(sim_enc);

// what the encoder keeps over a drive reset (its battery)
static struct {
  int init;
  int64_t shift;    // its turn count minus the shaft's whole turns
  int unindexed;    // the count was lost and the index not passed since
  int no_batt;      // at the next boot
  double last;      // its position at the last cycle [turns]
} mem;

#define RES (1 << 22)

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct encf_pin_ctx_t *pins = (struct encf_pin_ctx_t *)pin_ptr;
  PIN(sim_batt) = 1.0;
  PIN(sim_pp)   = 4.0;
}

struct encf_ctx_t {
  int booted;
};

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct encf_ctx_t *ctx      = (struct encf_ctx_t *)ctx_ptr;
  struct encf_pin_ctx_t *pins = (struct encf_pin_ctx_t *)pin_ptr;

  double shaft = PIN(sim_abs_pos) / (2.0 * M_PI);  // [turns]
  shaft        = PIN(sim_rev) > 0.0 ? -shaft : shaft;
  if(!ctx->booted) {  // a boot
    ctx->booted = 1;
    if(mem.init && mem.no_batt) {  // the turns are gone, the angle (optical) is not
      mem.shift     = -(int64_t)floor(shaft);
      mem.unindexed = 1;
    }
    mem.no_batt = 0;
    mem.init    = 1;
    mem.last    = shaft + mem.shift;
  }
  if(PIN(sim_batt) <= 0.0) {
    mem.no_batt = 1;
  }
  if(PIN(sim_turns) != 0.0) {
    mem.shift += (int64_t)lround(PIN(sim_turns));
    PIN(sim_turns) = 0.0;
  }
  double enc = shaft + mem.shift;
  if(mem.unindexed && floor(enc) != floor(mem.last)) {
    mem.unindexed = 0;  // passed the index: indexed again (with the count it has)
  }
  mem.last     = enc;
  PIN(sim_enc) = enc;

  if(PIN(sim_fail) > 0.0) {
    PIN(error) = 1.0;
    PIN(state) = 1.0;
    PIN(crc_er) += 1.0;
    return;
  }
  double t    = floor(enc);
  int32_t cnt = CLAMP((int32_t)floor((enc - t) * RES), 0, RES - 1);
  int64_t tc  = (int64_t)t + lround(PIN(sim_turn_err));
  float p     = mod((float)cnt * 2.0f * M_PI / RES);

  PIN(abs_pos) = p;
  PIN(pos)     = p;
  PIN(turns)   = (float)(tc & 0xffff);  // the frame's 16-bit field
  PIN(index)   = mem.unindexed;
  PIN(mt_ok)   = mem.unindexed ? 0.0 : 1.0;
  PIN(batt)    = PIN(sim_batt_low) > 0.0;
  PIN(com_pos) = mod(PIN(sim_pos) * PIN(sim_pp));
  PIN(state)   = 3.0;
  PIN(error)   = 0.0;
  PIN(crc_ok) += 1.0;
}

hal_comp_t encf_comp_struct = {
    .name      = "encf",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct encf_ctx_t),
    .pin_count = sizeof(struct encf_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
