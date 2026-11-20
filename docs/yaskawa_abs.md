# yaskawa_abs: Yaskawa Σ-II serial absolute encoders

`yaskawa_abs` is the feedback driver for the serial encoders of Yaskawa's
Σ-II motors: the 16-bit absolute (encoder code 1, e.g. SGMAH-08A1A61D).
Its reply carries 20 bits per turn and a signed 16-bit turn count. The
older `yaskawa` comp stays for the encoders it was written for.

The frame was worked out on the bench (2026-11-10, drive 207D35A05631).
The captures and the motor's notes are in the private repository
`brdmaker/brdmot`, `yaskawa.md`.

**Other Σ-II encoders** use the same link and framing with another field
layout. rene-dev/stmbl#58 (2017) decoded the 13-bit incremental encoder
(SGMAH-xxA**A**, encoder code A), counting bits as here, from the first bit
after the flag:
- bit 7: index seen since power-up (0: the compensation is valid);
- bits 58-60: hall sensors U, V, W, repeating 3 times a turn (3 pole pairs);
- bits 67-79: the 13-bit incremental count;
- bits 81-91: 11 bits of compensation (the offset from the index, ×4).

`yaskawa_abs` decodes the absolute layout only. Another layout would be a
second field table on the same decoder (`yab_frame`).

## Hardware

stmbl v4, **FB0**, Z pair (half-duplex RS-485). The encoder's 5 V comes from
FB0 pin 7: supply jumper JP2 on **5 V**.

| signal | Σ-II CN2 pin | FB0 (8P8C) |
|---|---|---|
| PG5V | 1 | 7 |
| PG0V | 2 | 8 |
| BAT+ / BAT− | 3 / 4 | 3.6 V lithium cell only, never to stmbl |
| PS | 5 | 5 (Z+) |
| /PS | 6 | 4 (Z−) |
| FG | shell | shell |

The setup wizard prints this as its "Yaskawa" wiring column.

Load no other FB0 encoder comp at the same time (`yaskawa`, `encf`,
`fanuci`, `fanuc_tw`, `encs`, `encm`, `smartabs`, `dmm`, `usart`). They share
TIM4, TIM8 and the DMA streams. `yaskawa_abs` also takes the DMA2 stream 1
interrupt.

## The link and the frame

Manchester at 4 Mbit/s (0 → chips 01, 1 → chips 10, a chip 125 ns), HDLC
framing: a 0101… preamble, the flag 01111110, the data with a 0 stuffed
after five 1s, the flag again. The request is the old comp's (data 0xffff).

Every rt cycle:
1. The capture of the last reply stops.
2. The next request goes out by DMA (TIM8 update writes GPIO BSRR words,
   ~14 µs).
3. rt decodes the last reply meanwhile.
4. When the request is out, the DMA interrupt (priority 1, above rt) turns
   the line around. TIM4 CH3 then captures the reply's edges into the other
   of two buffers.

So the position is the reply to the previous cycle's request.

The decoder starts at the first long interval. By the time the drive
listens, the preamble is mostly over: a capture began a bit or two before
the flag. The flag tells the polarity.

The reply's data is 14 bytes, in bits as received. Fields are LSB first:

| bits | field |
|---|---|
| 0-15 | header (`a0 38` / `a0 b8` / `a0 f8` / `a0 04`: bits 8-10 differ, status?) |
| 16-55 | not understood: they change with the shaft still |
| **56-75** | **position in the turn, 20 bits** (±1-3 counts of jitter at rest) |
| **76-91** | **turns, signed 16 bits** |
| 92-95 | 0 |
| 96-111 | CRC-16 CCITT (0x1021, init and xorout 0xffff) over bytes 0-11 |

## Config

```
link yaskawa_abs_fb0
```

The template:
- links `pos`, `abs_pos` and `state` to `fb_switch0`'s motor feedback
  (`mot_polecount` 1, `conf0.mot_fb_res` 2²⁰);
- links `error` to `fault0.mot_fb_error`.

Commutation comes from the absolute position and `conf0.mot_fb_offset`, as
with a resolver.

`state`:

| state | when |
|---|---|
| 3 | good frames (absolute: commutation from `pos` + `mot_fb_offset`) |
| 0 | `max_err` bad polls in a row (no reply, frame or CRC error), or none yet; `error` 1 → fault |

Isolated bad frames keep the last good one.

**Not there yet:**
- **Battery alarm:** found but not used yet. On the SGMAH (2026-11-20,
  brdmot `yaskawa.md`, battery test 3) the header's byte 1 (bits 8-15) is
  a 3-bit counter (top) and 5 status bits: `11000` with the battery OK,
  `00100` once the battery was unplugged while powered, and it stays
  `00100` (latched) after the battery is back, until a power cycle with
  the battery on (a latch from a power-up without the battery outlived
  that on 11-19).
  Command 1's second data byte carries the same byte; command 0 reads
  `00 00` throughout. A power-up without the battery restarts the turn
  count at 0. Until the alarm is decoded, `fault0.mot_fb_bat` and
  `mot_fb_nref` stay unlinked and a drive with a flat battery runs with a
  wrong turn count. Other requests: pins `req_n`, `req0`..`req3`,
  `req_crc` (brdmot `yaskawa.md`).
- **mturn:** for the same reason the turn count isn't tied to `mturn`
  (LinuxCNC's absolute position, docs/absolute_encoders.md).

**encmon:** like `fanuc_tw`'s, the position is a cycle older than a
resolver's. The setup wizard sets `encmon0.delay` to 0.00032, as for
`fanuc_tw`. Swept on the SGMAH (2026-11-18, 200 rad/s, 600 rad/s
electrical): the d residual goes linearly with the delay, about 5.7 % of
the back-EMF per 0.1 ms, and crosses 0 at 0.32 ms in one direction and
0.38 ms in the other. At 0.32 ms it is +0.5 % / +3.2 %; at 0 it was
−18 % / +22 %. The q residual is −4 / −5 % (the model's psi or r a few
percent off). The trip threshold is 30 %. Captures: brdmot
`commissioning/sgmah-08a1_2026-11-10/encmon/`.

## Pins

**Link:** `enable` (1), `max_err` (4).

**Request (bench):** `req_n` data bytes (1-4) `req0`..`req3` (as on the line,
MSB first), `req_crc` > 0 adds a CRC-16. Default 2, `ff ff`, no CRC: the
position request. Any other request gets other replies: the position
reads as an error until it is back.

**Feedback:**
- `pos`, `abs_pos` (rad, −π…π);
- `mpos` (turns·2π + position);
- `turns`;
- `raw_pos` (0…2²⁰−1);
- `state`, `error`.

**Counters:** `ok`, `crc_er`, `frame_er` (no sync, not Manchester, no flag,
too short), `noreply`.

**Debugging:**
- `edges`: edges in the last capture.
- `res`: the last poll's result (0 ok, 1 no reply, −1 no sync, −2 not
  Manchester, −3 no flag, −4 too short, −5 CRC).
- `us_col`, `us_dec`, `us_tx`: the rt function's time, of that the decode,
  of that starting the request.
- `dump` (rising edge): prints the next capture's edge times, its result
  and its bytes. The decoder's tests take captures in this form
  (host/tests/test_yaskawa_abs.c).

## Bench (2026-11-10, SGMAH-08A1A61D, drive 207D35A05631)

With pid and pmsm loaded, motor not enabled:
- `yaskawa_abs0` takes 38 µs of rt on average, 39 µs at most (the decode
  ~30 µs, 4400 cycles). rt is 149 µs on average, 151 µs at most, of 200
  (the setup wizard's config).
- Errors: 0 CRC errors, and 8 frame errors in 100 000 polls. The old
  comp counted 1.7 % CRC errors. Single bursts after a reset: up to 2000.
- With the motor wired, a short half-bit after a long one sometimes comes
  ~120 ns late (5 % of frames at some shaft positions). The decoder reads
  it; 1 frame error in 52 000 there.
- A firmware update (`make update`) with it loaded: fine (111 s).

Still to do:
- Run it with `id_mot` (commissioning). With `fanuc_tw`, `id_mot` added
  ~40 µs.
- The status bits, then battery and multi-turn as with `fanuc_tw`.
