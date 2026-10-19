# hwtest/fbtest: feedback-port test fixture

A board-level test of a drive's feedback port without a motor or encoder:
an Arduino Uno on the port's RJ45 drives known levels onto the lines and
measures each line on its own, while the drive reports what it sees (its
receivers, its sin/cos ADC, its pin 7 sense) and, through the `fbtx` test
comp, transmits for the Uno to measure. Together they check the input
resistors, the termination, the receivers, the transmitters and the analog
front end against the schematic.

Status (2026-10-19):

| | |
|---|---|
| Fixture rev 1 (A0..A5 on pins 1..6) | built, on fb1 of the bench drive |
| `fbtest.py`: supply, idle, shunt, pairs, transmit | 57/57 on the bench drive (firmware v0.9.18-318) |
| Fixture rev 2 (PWM sources for sin/cos) | designed below, not built yet |
| sin/cos gain, receiver threshold, encoder counting | planned, needs rev 2 |
| fb0, cmd port | not started |

## Running

```sh
cd hwtest/fbtest
./fbtest.py                 # Uno on /dev/ttyUSB0, drive on /dev/ttyACM0, fb1
./fbtest.py --no-transmit   # no fbtx, no drive reset
```

- The drive must be disabled; no motor is needed.
- The transmit test changes the running HAL only: `stop`, the lines of
  `conf/template/fbtest_fb1.txt` (`load fbtx`, ...), `start`, and a `reset`
  at the end (`--no-reset` keeps fbtx loaded). The flash config is never
  written.
- Needs firmware v0.9.18-318 or later (the `fbtx` comp).
- About 2 minutes. Results in `runs/<date>-<time>/`: `summary.md`,
  `results.json`, `fbtest.log` (every Uno and drive command with its
  answer). Exit status 0 on PASS.

## The fixture (rev 1)

| Uno | RJ45 | Signal (encoder / sin-cos) |
|---|---|---|
| A0 | 1 | A+ / sin+ |
| A1 | 2 | A− / sin− |
| A2 | 3 | B+ / cos+ |
| A3 | 4 | Z− |
| A4 | 5 | Z+ |
| A5 | 6 | B− / cos− |
| — | 7 | 5 V of the port: **not connected to the Uno** |
| GND | 8 | GND |

**Power the Uno from its USB only.** With pin 7 also on the Uno's 5V pin
the Uno reset in the middle of serial transfers, and its bootloader could
not be used (`DEBUGNOTES.md`, 2026-10-19). Not understood further: two
supplies on one 5 V net, or a loop through the drive's ground.

Firmware:

```sh
make -C firmware          # avr-gcc, avr-libc (apt: gcc-avr avr-libc)
make -C firmware flash    # PORT=/dev/ttyUSB0, through the Uno's optiboot
```

`uno_flash.py` does the flashing: avrdude 7.1 synced before optiboot had
started after the reset, optiboot reset itself on the partial command, and
with an empty flash never answered again. The uploader waits 0.25 s after
the reset, writes only the pages with data and reads them back.

## fb1 on the v4.0 board

From `hw/kicad/v4.0/fb.sch` / `stmbl_4.0.net` (sheet `/ctrl/FB1/`):

- Each pair (1-2, 3-6, 4-5) has a 120 Ω termination and an SN65176B
  transceiver. Its DE and /RE are tied to one F4 pin with a 1k pull-down,
  so it receives unless the firmware raises the pin: PE10 (A), PE12 (B),
  PE14 (Z). D and R are tied to one F4 net: PE9 (A), PE11 (B), PE13 (Z),
  which is also wired to PB10..PB15 (unused by the firmware).
- Pins 1-2 and 3-6 also feed an LM358 difference amplifier (10k inputs,
  15k to the 1.83 V reference and as feedback): `adc0.sin1` / `adc0.cos1`.
  Its range at the connector is about +1.08 / −1.22 V
  (`OP_*` in `inc/hw/hw.h`).
- Pin 7 is +5F or +12V by jumper JP5, sensed through 10k/1k as `io0.fb1`.

So with the transceivers receiving, each line sits at a bias:

| Pins | Network | Expected | Bench drive |
|---|---|---|---|
| 1, 2, 3, 6 | receiver input ‖ 25k to 1.83 V | ~1.05 V, ~13 kΩ | 1.085–1.092 V, 12.7–13.4 kΩ |
| 4, 5 | receiver input only | ~0.2 V, ~27 kΩ | 0.193–0.194 V, 26–27 kΩ |

(The receiver's own input, ~27 kΩ to ~0.19 V, is taken from pins 4-5;
the SN65176B datasheet gives ≥ 12 kΩ.)

## Tests

Limits are in the top of `fbtest.py`. Bench values: drive 3, fb1,
2026-10-19.

| Test | How | Pass | Bench |
|---|---|---|---|
| supply | `io0.fb1` (pin 7) | 4.75–5.5 V | 5.21 V |
| idle | Uno hi-Z, each line measured | pins 1,2,3,6: 0.9–1.25 V; 4,5: 0.05–0.4 V; a pair within 30 mV | see the table above; pairs within 2 mV |
| shunt | one line driven high; the drop across the 120 Ω gives the current into the partner, so the partner's resistance to its bias: R = 120 Ω × (V<sub>partner</sub> − V<sub>idle</sub>) / (V<sub>driven</sub> − V<sub>partner</sub>) | 9–17 kΩ (1,2,3,6), 18–40 kΩ (4,5) | 12.7–13.4 kΩ, 26–27 kΩ |
| pairs | each pair driven 5 V / 0 V both ways (~31 mA) | share of Vcc across the pair 0.62–0.80 (120 Ω against ~25 Ω per AVR output: 0.71; no termination ≈ 1, two fitted ≈ 0.55); `fb1a/b/z` follow pins 1, 3, 5; sin/cos at the clip level, the other channel within 50 mV | 0.709–0.715; receivers right; +1.06 / −1.16 V |
| transmit | `fbtx` transmits each pair both ways, Uno hi-Z | differential 2.0–4.5 V (RS-422: ≥ 2 V), offset 1–3 V, the other pairs within 30 mV of idle | 2.91–2.95 V, 2.68 V, ≤ 9 mV |
| transmit, quadrature | `fbtx0.div = 5` (a step a ms), Uno counts edges on pins 1 and 3 for 1 s | 490–510 | 501, 501 |

Limits of the method:

- The shunt values rest on the 120 Ω termination being right; the pairs
  test checks that independently (the share of Vcc), so a wrong
  termination fails there. The Uno resolves ~5 mV on a 23–38 mV drop:
  ±15 %, enough for a missing or wrong-value part, not for tolerance.
- The Uno's internal pull-ups (20–50 kΩ) are no good as a known load; the
  first experiments with them gave inconsistent drops.
- With 0 / 5 V lines the sin/cos channels only show their clip levels,
  not their gain.
- The receivers have no fail-safe bias: with the lines idle `fb1a/b/z`
  read either level (seen both).
- The Uno's `vcc=` is from its 1.1 V bandgap, ±10 % until calibrated.

## fbtx

`src/comps/fbtx.c`, for port tests only:

| Pin | |
|---|---|
| `en_a`, `en_b`, `en_z` | > 0: that transceiver transmits (DE high, then the F4 pin to output); ≤ 0: back to receiving (pin to input first) |
| `a`, `b`, `z` | the levels transmitted (pins 1, 3, 5 positive) |
| `div` | ≠ 0: a and b step in quadrature every `div` rt periods (rt is 5 kHz), backwards for div < 0; z high in step 0 |
| `step` | quadrature step count |

By hand: `stop`, `load fbtx`, `fbtx0.rt_prio = 15`, `start`, then e.g.
`fbtx0.a = 1`, `fbtx0.en_a = 1`. `reset` removes it. Component names must
be letters and `_` only (the console takes digits as the instance number;
`tools/create_hal_tbl.py` refuses others).

## Uno firmware commands

115200 8N1, one line per command, one answer line (`err ...` on a bad
command). Details in the header of `firmware/fbtest.c`.

| Command | Answer |
|---|---|
| `i` | `fbtest <ver> pins=6` |
| `o <ddr> <port>` | A0..A5 (hex bit masks): ddr 1 driven at port's level; ddr 0 with port 1 pull-up, 0 hi-Z; `ok` |
| `d` | `d <pins hex>` |
| `a [n]` | `a <mV A0> .. <mV A5> vcc=<mV>`, mean of n (16) samples against AVcc |
| `q <us> <steps> <s0> <s1> <s2> <s3>` | steps through four port states (hex), `q <index>` |
| `c <bit> <ms>` | edges on A<bit> for ms: `c <n>` |

## Rev 2: sin/cos sources (planned)

To test the analog front end inside its ±1 V range, each of the four analog
lines gets a filtered PWM source, both ends of each pair driven:

```
Uno PWM ──47Ω──┬──150Ω── RJ45 line (the Uno's A pin stays on it as the meter)
              10µF
               │
              GND
```

| RJ45 pin | PWM (62.5 kHz, 8-bit fast PWM) |
|---|---|
| 1 (sin+) | D9 (OC1A) |
| 2 (sin−) | D10 (OC1B) |
| 3 (cos+) | D11 (OC2A) |
| 6 (cos−) | D3 (OC2B) |

Parts: 4 × 47 Ω, 4 × 150 Ω, 4 × 10 µF (≥ 6.3 V). Pins 4-5 have no analog
circuit and stay as in rev 1. Timer0 stays free.

Why this way:

- **Both ends of each pair driven.** With a source on one line only, the
  120 Ω termination ties the other line to it while only ~13 kΩ holds it
  at its bias: the differential would be ~120 / 13k of the swing (±45 mV).
  With two sources the difference of the duties sets the differential and
  their mean the common mode, separately.
- **Range.** About 5 V × 120 / (120 + 2 × ~220 Ω) ≈ ±0.9 V differential:
  covers 1 Vpp encoders (±0.5 V) and most of the front end's range.
- **No precise source needed.** The Uno measures both lines directly, so
  PWM and resistor tolerances drop out; the accuracy is the Uno's ADC.
- **No analog switch.** The network stays connected; the firmware keeps it
  from fighting the A pins instead:

  | Test | PWM pins |
  |---|---|
  | idle | hi-Z; wait ~1 s (the 10 µF charges through ~13 kΩ) or preset the duty to the bias first |
  | shunt, pairs | set to the A pin's level and settled (~10 ms) before the A pin drives: no current in the network, the rev 1 limits hold |
  | transmit | hi-Z (no DC load once charged) |
  | sin/cos | A pins hi-Z (meters), duties set the lines |
  | quadrature into the drive | from the PWM pins at 0 / 100 %, up to ~200 steps/s (the RC) |

  The cost: no fast quadrature from the A pins (against the network at
  mid level they would carry ~17 mA on top of the termination's 31 mA,
  past the AVR's 40 mA), and ~40 mV ripple on the filter node (a few mV
  differential at the line; both sides average). Fast counting can use
  `fbtx`'s own quadrature.
- **Calibration.** One multimeter reading of the Uno's 5V pin calibrates
  the bandgap (stored in EEPROM) to ~0.5 %. No external AREF: the lines
  go to 5 V, and selecting the AVcc reference with an external reference
  on AREF shorts it.

Tests it enables:

- sin/cos gain and offset per channel: a line fit over ~±0.8 V
  (expected gain 1 within the resistor tolerances, `adc0` offsets ~0);
- common-mode rejection: both lines of a pair moved together;
- receiver threshold and hysteresis: a slow differential ramp through 0
  (SN65176B: within ±200 mV);
- encoder counting: quadrature into `fb1a/b` with a counter comp loaded in
  RAM (`enc_cmd` with `remap = 2` uses fb1's TIM1), steps compared;
- a slow sin/cos rotation through the drive's sin/cos position chain.

Firmware work for rev 2: a `p <pin> <duty>` / `p off` command, the
mirrored drive sequences above, the bandgap calibration in EEPROM.

## Other ports

fb0 and the command port use the same parts with other F4 pins (`hw.h`:
`FB0_*`, `CMD_*`); a fixture there needs a port option in `fbtest.py` and
an `fbtx`-like comp for their transceivers (fb0 already has `o_fb` for
A/B).
