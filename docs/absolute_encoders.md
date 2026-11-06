# Absolute encoders: the position from boot

A multi-turn absolute encoder (FANUC, Yaskawa, Mitsubishi, ... with a
battery or a backup capacitor) knows how many turns it has made, so a
machine with one on every axis needs no homing moves after power-up. stmbl
used only the encoder's angle within one turn: `linrev` counted turns from
0 at every boot, and LinuxCNC got a position right within one motor turn.
`mturn` puts the encoder's turn count into the position LinuxCNC sees.

Status: in the firmware and the host simulator, tested in simulation on the
CN 720's X axis with a simulated FANUC αA64-type encoder
(`host/tests/abs/`). Not yet run on a real drive. The GUI part (machine zero,
the status badges, startup gating in millweb) is not done yet; the plan is
at the end.

## How it works

```
encoder (encf0)                 mturn0                        linrev0
abs_pos, turns, mt_ok, batt --> load: rev = turns + (0|±1) --> rev_in / rev_load
                                check every cycle            --> fb_out = (rev*2pi + pos) * scale/2pi
idx_home0.pos_out (pos) ------>                                 --> sserial0.pos_fb (LinuxCNC)
                                abs_ok --> sserial0.in2, batt_warn --> sserial0.in3
```

- **The load:** on the first good frame after boot, `mturn` sets `linrev`'s
  turn count from the encoder's: `rev = dir·turns + round((dir·e0 − pos)/2π)`,
  where `e0` is the encoder's angle in 0…2π (its turn counter steps where
  its count wraps to 0) and `pos` the angle `linrev` counts. Integer
  arithmetic plus one small rounding, so float precision doesn't matter.
  It waits until `pos` agrees with the encoder (same shaft, within 90°): right
  after boot `pos` can be a cycle old, and a load then is a turn off.
- **The motor doesn't move:** the drive's own position loop works on the
  single-turn angle (`linrev0.cmd_out` is wrapped), so the load only changes
  what LinuxCNC reads.
- **The check:** every cycle, `linrev`'s turn count against the encoder's,
  modulo the encoder's counter width (`turns_bits`, 16 for FANUC): a
  disagreement for longer than `check_time` (2 ms) is a mismatch.
- **Lost count:** `mt_ok` = 0 from the encoder (FANUC: the frame's "un-indexed"
  bit, after the battery was out) latches state 2: single-turn only. The
  encoder may become valid again after passing its index, but its count is
  then not tied to the machine; only `clear` (a rising edge, after the
  machine has been referenced) loads again.
- **Frame errors:** a frame error (cable) drops `abs_ok`; when frames return
  it loads again.

| `mturn0.state` | meaning | `abs_ok` |
|---|---|---|
| 0 | waiting for a good frame | 0 |
| 1 | absolute, checked | 1 |
| 2 | turn count lost: single-turn only until referenced and cleared | 0 |
| 3 | mismatch: `linrev` and the encoder disagreed, or `pos` never agreed with the encoder (wired to another encoder, direction wrong) | 0 |

## Config

After the feedback and the smart-serial templates:

```
link fanuc_fb0
link sserial
link mturn        # mturn0, linrev0.rev_in / rev_load, sserial0.in2 / in3
link fanuc_mt     # the encoder's pins into mturn0 (turns_bits 16)
```

The setup wizard writes these for a FANUC encoder with the smart-serial link
(step `absolute`, [setup_wizard.md](setup_wizard.md)). Multi-turn needs
`linrev`, so it works over smart-serial only (CAN: not yet).

**To LinuxCNC:** the smart-serial input data is already at 96 bits, the limit
of older Mesa smart-serial firmware ([sserial_console.md](sserial_console.md)),
so no new field: `abs_ok` goes on the drive's spare input 2 and the battery
warning on input 3 (the `in` bits of the process data; inputs 0/1 stay the
drive's digital inputs).

**In LinuxCNC:** per joint `HOME_ABSOLUTE_ENCODER = 2` (homed at start
without moving) and `HOME_OFFSET` = where machine zero is in the encoder's
position. millweb's `machine.json` home method `"absolute"` already writes
the first.

**Precision:** the position goes to LinuxCNC as a float. On the CN 720 (10 mm
screw, 700 mm travel = 70 turns) it reads to < 1 µm (`abs_wrap.test`); at
thousands of turns it gets coarse (≈ 0.03 mm at 32768 turns of 10 mm).

## Encoders

| encoder | component | turns | `mt_ok` | battery | state |
|---|---|---|---|---|---|
| FANUC αA64 type (4-wire) | `encf` | 16 bit | frame "un-indexed" bit | frame bit 5 (unverified) | simulated, wizard |
| FANUC βiA128 (1-pair) | `fanuci` | 16 bit (decoded) | unknown (frame bit 68?) | unknown | frame decode in progress |
| Yaskawa Σ-II 16-bit abs | `yaskawa` | not decoded yet | | | `len`/`off` to check |

## Tests (host simulator)

- `host/tests/abs/abs_power_cycle.test`: the CN 720's X axis boots at
  37.25 turns → 372.5 mm; turned by hand, power-cycled (the shaft keeps its
  place: `hv0.sim_keep_pos`), still absolute; enables without moving; jogs.
- `abs_faults.test`: the shaft turned while off; a turn-count fault
  (mismatch); the battery out while off (lost, latched, cleared); a frame
  error and the reload; `abs_ok`/battery on `in2`/`in3`.
- `abs_wrap.test`: the 16-bit counter wrap; 699.9 mm to < 1 µm.
- `tests/run_wizard_sim.py`: a FANUC-encoder motor through the whole wizard.

The simulated encoder (`host/hw/encf.c`) keeps its battery-backed turn count
in static memory over a drive `reset`; its settings for "while off"
(`sim_batt` = 0) and the motor model's `sim_off_move` act at the next reset.

## Next: the GUI (millweb) and LinuxCNC

1. **Machine zero:** per joint "Set machine zero here": the offset from the
   current position into `machine.json`, the INI regenerated, LinuxCNC
   restarted. Stored with the encoder's serial where it reports one, so a
   swapped motor invalidates the zero.
2. **Gate:** a joint is homed only with `abs_ok` (input 2); otherwise a large
   banner: re-reference (fallback homing) or set zero, then `clear`.
   Remember a lost count across restarts (`abs_ok` seen 0 since the last
   zero), since the encoder can look valid again after passing its index.
3. **Status:** per joint absolute OK / battery low / count lost / mismatch;
   turns on the Drives card.
4. **Rotary axes:** the turn count wraps; with a non-integer gear ratio keep
   the table angle modulo 360° in LinuxCNC.
