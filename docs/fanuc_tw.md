# fanuc_tw: FANUC 2-wire serial pulsecoders (βiA128)

`fanuc_tw` is the feedback driver for FANUC's i-series pulsecoders in their
2-wire (1-pair, "αi interface") mode: the βiA128 (A860-2020-T301) of the βiS
motors. FANUC wires these with a single twisted pair, RD/*RD, used in both
directions. The older 4-wire α pulsecoders (αA64) are `encf`'s.

The protocol is not documented by FANUC. It was worked out on the bench with
the `fanuci` probe (docs/fanuci.md; the bench log, captures and FANUC notes
are in the private repository `brdmaker/brdmot`, `probe-log.md`).

## Hardware

stmbl v4, **FB0**, Z pair. The encoder's 5 V from FB0 pin 7: supply jumper
JP2 on **5 V** (12 V destroys the encoder).

| encoder (motor connector) | signal | FB0 (8P8C) |
|---|---|---|
| 6 | RD | 5 (Z+) |
| 5 | *RD | 4 (Z−) |
| 8, 9 | +5 V | 7 |
| 7, 10 | 0 V | 8 |
| 4 | +6 V battery | battery box only, never to stmbl |
| 3 | frame | connector shell |

Wiring with the original FANUC cable and a junction box: brdmot `harness/`.

Without the battery the encoder keeps its position for about 10 minutes on an
internal capacitor. After that it starts with turns 0, not referenced (see
`nref`), until the shaft has passed its reference once.

Load no other FB0 encoder comp at the same time (`encf`, `fanuci`, `yaskawa`,
`encs`, `encm`, `smartabs`, `dmm`, `usart`): they share TIM4, TIM8 and the
DMA streams. `fanuc_tw` polls from TIM6 (`fanuci` uses TIM7).

## The link and the frame

Every poll: the driver on, the request `start 0, 0x00, stop 1` at 2.72
Mbit/s with interrupts off (30 µs of idle before the very first one), the
driver off, the reply captured edge by edge; rt decodes it. A cold encoder
only starts answering when polled at ~8 kHz, so the driver polls from TIM6
at 8 kHz until 2000 good frames in a row, then once per rt cycle (5 kHz)
from rt (`rt_poll` 1); after 40 bad polls it goes back to 8 kHz. `poll_hz`
> 0 keeps TIM6 at that rate. The decoder costs ~20 µs of rt. The
encoder answers 8.5 µs after the request with 100 bits at the same rate:

| frame bits | word | field |
|---|---|---|
| 0 | | start bit, 0 |
| 1–16 | w0 | bits 0–7 status (`10100000` while running; bit 5 = frame bit 6: **battery alarm**, set while the battery is missing or low, clears when it is back), bits 8–15 position bits 0–7 |
| 17–19 | | sync `110` |
| 20–35 | w1 | position bits 8–23: **24 bit per turn** |
| 36–38 | | sync |
| 39–54 | w2 | **turns**, 16 bit two's complement |
| 55–57 | | sync |
| 58–73 | w3 | bits 0–7 commutation (256 per electrical turn, = 1024·pos/2²⁴ once referenced); bits 8–15 status: bit 10 (frame bit 68) = **not referenced**; bits 13–15 (frame bits 71–73) change all the time (a counter?) |
| 74–76 | | sync |
| 77–92 | w4 | unknown, `0x804B` on the encoder on hand |
| 93–94 | | `11` |
| 95–99 | | **CRC-5**, x⁵+x³+x+1, init 0, over bits 0–94, MSB first |

All of it LSB first. `shared/fanuc1p.c` (`f1p_frame`) decodes it;
`host/tests/test_fanuc_tw.c` checks it on recorded frames.

## Config

```
link fanuc_tw_fb0
```

The template links `pos`/`abs_pos`/`state` to `fb_switch0`'s motor
feedback (`mot_polecount` 1, `conf0.mot_fb_res` 2²⁴) and `error` to
`fault0.mot_fb_error`. Commutation comes from the absolute position and
`conf0.mot_fb_offset`, as with a resolver; the encoder's own commutation
field is only an output (`com_pos`): after a power-up it is ~30° el. off
until the reference is passed.

`state`:

| state | when | fb_switch |
|---|---|---|
| 3 | good frames, referenced (bit 68 clear) | absolute: commutation from `pos` + `mot_fb_offset` |
| 1 | good frames, not referenced (bit 68 set) | incremental: the drive phases the motor first |
| 0 | `max_err` bad polls in a row (no reply, CRC or sync error), or none yet | `error` 1 → fault |

**Position lost or battery alarm: no enable.** The template links `nref_fault`
(`nref` unless `ref_move`) to `fault0.mot_fb_nref` and `bat_fail` to
`fault0.mot_fb_bat`. While either is
on the drive is in fault and won't enable: fault 20 *mot FB not
referenced* (the encoder lost its position: turn the motor through its
reference, about one turn, then enable) and 21 *mot FB battery alarm*
(battery missing or low). The battery alarm while enabled stops the drive
(the normal stop, `fault0.err_stop`: the feedback still counts). LinuxCNC
sees the fault bit in the process data; the code and its text come over
the smart-serial console (`fault0.fault`, the drive's `ERROR: Fault n`
line; webui/mill's Drives tab names them). Unlink them in a config to run
without the check.

**Moving through the reference (`ref_move`).** In a machine the shaft may not
turn by hand. `fanuc_tw0.ref_move = 1` holds fault 20 off while the encoder
isn't referenced: the drive enables incrementally (state 1: `fb_switch`
phases the motor first) and can be moved slowly up to a turn. Once the
encoder passes its reference, the state goes to 3 (absolute) and `ref_move`
has nothing left to do; set it back to 0. (Not yet tried on the bench: it
needs an encoder that lost its position.)

**encmon** (the encoder monitor, linked by the motor configs and the setup
wizard): `fanuc_tw`'s position is the reply to the previous cycle's
request, about 0.2 ms older than a resolver's, so `encmon0.delay` is
0.00032 (the template's 0.00012 + 0.2 ms). Bench, fast moves: worst
residual 0.22 of the 0.30 trip with 0.12 ms, 0.17 with 0.32 ms. And `encmon0.min_bemf`
is 10 V: near the 5 V default (~15 rad/s here) a fixed couple of volts of
voltage error made small moves reach 0.37; at 10 V 0.22, and a 30° el.
offset error still trips. The setup wizard sets both.

Isolated bad frames keep the last good one (FANUC alarms only on errors that
persist); on the bench 1 frame in ~900 had a flipped bit, caught by the CRC.

## Pins

**Link:** `enable` (1), `poll_hz` (0: wake at 8 kHz, then rt; `rt_poll` shows which), `bitrate` (2721088), `req` (512),
`max_err` (4), `nref_bit` (68; −1: ignore), `ref_move` (0).

**Feedback:** `pos`, `abs_pos` (rad, −π…π), `mpos` (turns·2π + position),
`turns`, `com_pos` (the encoder's commutation field, electrical rad),
`state`, `error`, `nref`, `nref_fault`, `bat_fail` (the battery alarm: a warning, the drive runs on; the position is lost at the next power-off).

**Counters:** `ok`, `crc_er`, `sync_er`, `noreply`, `latency_us`, `edges`.

## Debugging the unknown bits

| pin | |
|---|---|
| `w0` … `w4` | the five words of the last good frame |
| `chg0` … `chg4` | bits of each word that changed since `chg_reset` (rising edge). With the shaft still: `chg0` 0x3F00 (position noise), `chg3` 0xE000 (bits 71–73) |
| `trace` | 1: print `f2w trace <ms> w0= w3= w4= pos= turns=` whenever the status bits under `trace_m0` (0x00ff), `trace_m3` (0x1f00), `trace_m4` (0xffff) change: battery out, power-up, the reference, overheating |
| `fld_bit`, `fld_len`, `fld` | a field by frame bit, LSB first, sync bits skipped: for the motor temperature once it is found |
| `dump` | rising edge: print the last frame split into start / words / sync, and its fields |
| `alt_req`, `alt_every` | send the request `alt_req` (10 bits, e.g. `512 + 2·code` for the byte `code`) every `alt_every` polls instead of the normal one; the position holds over that poll. The reply: `alt_res` (0 a valid frame, −1/−2/−3 short/sync/CRC, 1 none), `alt_edges`, `alt_w0` … `alt_w4`, `alt_dump` prints it |

The motor temperature (FANUC reads the motor's thermistor through the
pulsecoder: OHAL2, the CNC's diagnosis) is not found yet. It isn't in the
normal frame as an obvious value: heat the motor and watch `chg*` / `trace`,
or look for it in the replies to other request codes (`alt_req`). Careful
with unknown request codes: some may be commands (clear an alarm, reset the
turns, write the encoder's EEPROM).
