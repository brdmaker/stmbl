# fanuci: probing FANUC 1-pair pulsecoders (βiA128 …) with stmbl

`fanuci` is an **experimental** stmbl component for working out the serial
protocol of FANUC's i-series pulsecoders (βiA128, αiA1000 …) in their
**1-pair** ("αi interface", "2-wire", "high speed") mode. FANUC wires these
encoders with one twisted pair, RD/*RD, used in both directions. stmbl's
`encf` speaks the older 2-pair α protocol (request pulse on one pair, data
on the other) and can't reach them.

The protocol is undocumented, so `fanuci` does not decode a known frame.
It sends a request whose bit rate, bit pattern, polarity and timing are
pins, turns the RS-485 transceiver around, records the reply edge by edge,
decodes the edges into bits and prints them on the console. Field offsets
for position, turns and commutation are pins too, so a layout found on the
bench can be tried without rebuilding.

Status: builds for stmbl v4 and its decoding is tested on the host
(`host/tests/test_fanuc1p.c`). It has **not run on hardware yet**.

Background, sources and bench results for the owner's motors are in the
private notes (`brdmaker/brdmot`, `encoders.md`).

## What is known about the protocol

| | value | source |
|---|---|---|
| line | half-duplex RS-485 on RD/*RD | FANUC amplifier manuals: "αi interface (1-pair communication)" |
| bit rate | ≈ 2.72 Mbit/s (`2721.088` kHz); some say 2.048 | universal-IRC-1 (a working FPGA implementation); forum posts |
| request | a serial character, not a pulse; "can be all zeros"; that code uses the pattern `0b0001110000` | universal-IRC-1 README and `fanucabs.vhd` |
| polling | "must be polled at 8 kHz" | universal-IRC-1 README |
| reply | ~73 bits; 16 bit position at bit 21, 16 bit turns at 40, 8 bit commutation at 59 (LSB first, counted after its start bit) | universal-IRC-1 `A64i` format |
| commutation | 256 steps per electrical turn | the same, and FANUC's "AMR" value |
| status | battery, turn counter, phase, LED, checksum, overheat … (names only) | FANUC alarm lists |

None of this is confirmed for the βiA128. That's what the probe is for.

## Hardware

stmbl v4 (4.3 on the bench), **FB0** port, Z pair:

| encoder | signal | FB0 (8P8C) |
|---|---|---|
| motor pin 6 | RD | 5 (Z+) |
| motor pin 5 | *RD | 4 (Z−) |
| 8, 9 | +5 V | 7 |
| 7, 10 | 0 V | 8 |
| 4 | +6 V battery | not connected (or the battery) |
| 3 | frame | connector shell |

- FB0 supply jumper on **5 V**. 12 V destroys the encoder.
- The wrong polarity on 5/4 doesn't harm anything; `tx_invert` and
  `rx_invert` flip it in software.
- Encoder only: no high voltage, motor phases disconnected.

`fanuci` uses the same peripherals as `yaskawa`:
- TX: TIM8 update → DMA2 stream 1 ch. 7, one GPIO BSRR word per bit on PD14;
  TX enable PD15.
- RX: PD14 as TIM4 CH3, both edges → DMA1 stream 7 ch. 2, timer at 84 MHz
  (≈ 31 ticks per bit at 2.72 Mbit/s).

**Load no other FB0 encoder comp at the same time** (`encf`, `yaskawa`,
`encs`, `encm`, `smartabs`, `dmm`, `usart`): they share these timers and
DMA streams.

## Build and flash

On the bench Pi (binutils 2.40; see `CLAUDE.md` and `DEBUGNOTES.md`):

1. Back up the drive's config: `servoterm-cli --dump > backup.txt`.
2. `make update` (USB or the smart-serial console), alternating with
   `make ssburn`, as `CLAUDE.md` says; `make btburn` (DFU) only as the
   backup. Use no other flashing method.
3. Check `about` and that the config is unchanged.

The component is in the normal firmware image. It does nothing until it
is loaded.

## Run

The template `conf/template/fanuci_fb0.txt`:

```
load fanuci
fanuci0.rt_prio = 1
io0.fb0y = fanuci0.error
io0.fb0g = 1
```

That polls from the rt thread at 5 kHz. For the faster rates, use the frt
thread instead (20 kHz / `frt_div`: 2 → 10 kHz, 3 → 6.7 kHz):

```
fanuci0.rt_prio = 0
fanuci0.frt_prio = 1
fanuci0.frt_div = 2
```

Set only one of `rt_prio` / `frt_prio`. Don't link `fanuci0` to
`fb_switch0` or the motor yet: the position isn't known to be right.

## Pins

**Request**

| pin | default | meaning |
|---|---|---|
| `bitrate` | 2721088 | bit/s (TX: TIM8 at 168 MHz, rounded; RX decoding) |
| `req` | 512 | request bits, sent LSB first. 512 = start 0, data 0x00, stop 1 |
| `req_bits` | 10 | number of request bits |
| `lead` | 2 | idle bits with the driver on before the request |
| `tail` | 1 | idle bits after it, before the driver turns off |
| `idle` | 1 | logical idle level |
| `tx_invert` | 0 | invert what is sent |
| `enable` | 1 | 0: stop polling, line released |
| `frt_div` | 2 | frt only: poll every n frt cycles |

**Receive**

| pin | default | meaning |
|---|---|---|
| `rx_idle` | 1 | receiver level before the reply (fail-safe high) |
| `rx_invert` | 0 | invert the received bits |
| `rx_skip` | 0 | drop this many edges at the start of a capture |
| `min_edges` | 4 | fewer edges count as no reply |

**Fields** (bit offsets in the decoded bits; bit 0 = the first bit after
the first edge, normally the start bit)

| pin | default | |
|---|---|---|
| `lsb_first` | 1 | first bit on the wire = bit 0 of the field |
| `pos_off`, `pos_len` | 21, 16 | → `raw_pos`, `pos` (rad, 0…2π) |
| `turns_off`, `turns_len` | 40, 16 | → `turns` |
| `com_off`, `com_len` | 59, 8 | → `com_raw`, `com_pos` (rad, 0…2π) |

Set a `_len` to 0 to switch that field off.

**Outputs**

| pin | meaning |
|---|---|
| `edges` | edges in the last capture |
| `nbits` | bits decoded (up to the last edge) |
| `latency_us` | from the end of the request to the first received edge |
| `reply` / `noreply` | counters |
| `error`, `state` | 1 / 0 without a reply, 0 / 3 with one |

**Console**

| pin | meaning |
|---|---|
| `dump` | 0 → 1: print one capture |
| `autodump` | > 0: print a capture every n nrt calls (about n ms) |
| `dump_edges` | 1: also print the edge timestamps (TIM4 ticks) |

A dump line:

```
f1p e=38 lat=1450ns n=74 01101100 10010011 ...
f1p t=121,152,214,...        (with dump_edges = 1)
```

`e` is the edge count, `lat` the turnaround latency, `n` the bit count,
then the bits in groups of 8, first received bit first.

## Finding the protocol on the bench

Keep a log of every setting tried and what came back (in brdmot).

1. **Is anything sent?** With a scope (or the logic analyzer through a
   receiver) on FB0 pins 5/4 you should see a ~4 µs burst every 200 µs (5
   kHz). If not, check the comp is loaded (`fanuci0.reply`/`noreply`
   count up) and that no other FB0 comp is loaded.
2. **First tries**, watching `fanuci0.reply` and `fanuci0.edges`:
   - defaults (2.72 Mbit/s, `req` 512);
   - `tx_invert 1`;
   - `req 112` (universal-IRC-1's pattern), with `tx_invert` 0 and 1;
   - `req 0`, `req_bits 10`, and the same with `idle 0`;
   - each of the above at `bitrate 2048000`;
   - from frt at 10 kHz (`frt_div 2`) and 6.7 kHz (`frt_div 3`), in case
     the encoder only answers when polled fast.
3. **A real reply** looks like this:
   - `edges` steady (tens of edges) on every poll;
   - `latency_us` a few µs at most;
   - dumps identical while the shaft stands still;
   - with `rx_invert` wrong, the bits are inverted but just as steady.
4. **Pin down the framing.** With `dump_edges 1`, look at the shortest edge
   distance: it should be about 1 bit (≈ 31 ticks at 2.72 Mbit/s). If it's
   not, the bit rate is off: `bitrate` = 84e6 / shortest distance. Check
   whether the first bits are a fixed preamble, and whether the reply is
   one long frame or UART-style characters (start bit, 8 bits, stop bit).
5. **Find the fields.**
   - Dump with the shaft still, turn it a few degrees, dump again. The
     bits that changed are the position. The low bits change most, so in
     an LSB-first field they change at the start of the field.
   - Set `pos_off`/`pos_len` and check that `pos` follows the shaft
     smoothly over a full turn (128K counts for a βiA128 = 17 bits).
   - The commutation field repeats 4 times per turn on these 4-pole-pair
     motors.
   - The turn count steps once per revolution.
   - Status bits change when you pull the battery, or on power-up.
6. **Find the check.** Save 50–100 dumps with the shaft in different
   positions. The last 5–8 bits are probably a CRC: test candidates
   (CRC-5 ITU as in `encf`, CRC-8 variants) offline against the saved
   frames.
7. **Watch it live** with the scope component (`docs/capture_scope.md`):
   `fanuci0.pos`, `fanuci0.com_pos`, `fanuci0.edges`.

## Turning the probe into a feedback driver

Once the frame is known:

1. Put the frame layout and the check into `shared/fanuc1p.c` with a host
   test built from real recorded frames (`host/tests/test_fanuc1p.c`:
   paste dumps as test vectors).
2. Give `fanuci` the usual feedback outputs (`pos` −π…π, `abs_pos`,
   `com_pos`, `state` 3 only on a good check, `error`), and stop printing.
3. Add a template like `conf/template/fanuc_fb0.txt` that links them to
   `fb_switch0`, with `conf0.mot_fb_res` = 131072 for the βiA128.
4. Add a `DEBUGNOTES.md` entry and update this file.

## Code

| file | what |
|---|---|
| `src/comps/fanuci.c` | the comp: STM32 timers, DMA, turnaround, pins, console |
| `shared/fanuc1p.c`, `.h` | pure C: request levels, edge → bit decoding, fields, dump text |
| `host/tests/test_fanuc1p.c` | unit test (in `make -C host check`): request patterns, round trips of random 73-bit frames through jittered edge timestamps with timer wrap-around, 3 % bit-rate error, fields, formatting |
| `conf/template/fanuci_fb0.txt` | template |

One poll from `poll()`:
1. collect the previous capture (edges → bits → fields, snapshot for the
   console);
2. rebuild the TX words if a request pin changed;
3. drive the idle level, enable the driver, clock the words out
   (TIM8 + DMA), wait for the end (≈ 4 µs busy wait at 2.72 Mbit/s with
   the defaults);
4. turn around: driver off, PD14 back to the capture input;
5. start the capture DMA. The reply is collected at the next poll.

Load: ≈ 4 µs busy wait plus decoding (≲ 10 µs) per poll. From frt at
10 kHz that is a few percent of the CPU; watch for `FRT_TOO_LONG` if
other frt comps are loaded.
