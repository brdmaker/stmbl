# hwtest/fbtest: feedback-port test fixture

An Arduino Uno (ATmega328P, FT232R USB serial) on a feedback port of the
drive under test, to check the port's hardware: input bias resistors,
termination, line receivers, the analog front end, the 5 V supply.

```sh
./fbtest.py                 # Uno on /dev/ttyUSB0, drive on /dev/ttyACM0, fb1
```

The drive must be disabled; no drive pin is changed. About 1 minute;
results in `runs/<date>-<time>/` (`summary.md`, `results.json`,
`fbtest.log`).

| Test | Checks (v4.0 schematic, `hw/kicad/v4.0/fb.sch`) |
|---|---|
| supply | pin 7 as the drive reads it (`io0.fb1`), 4.75–5.5 V |
| idle | each line with the Uno hi-Z and the transceivers disabled (1k pull-downs on DE/RE): pins 1,2,3,6 ~1.05 V (SN65176B receiver ‖ 25k to the 1.83 V op-amp reference), pins 4,5 ~0.2 V (receiver only); the lines of a pair within 30 mV |
| shunt | one line driven high: the drop across the 120 R termination gives the partner's resistance to its bias, ~13k (pins 1,2,3,6) or ~27k (4,5) |
| pairs | each pair driven both ways: the share of Vcc across the termination (120 R against ~25 R per AVR output, 0.71), the receiver (`fb1a/b/z`, pins 1, 3, 5 positive), sin/cos at its clip level (+1.08 / −1.22 V) and the other channel near 0 |

Not covered yet: the sin/cos gain (the Uno's lines are only 0 / 5 V, past
the ±1 V input range; needs a resistor divider on the fixture), the
transceivers as drivers, encoder counting.

Wiring: A0..A5 to RJ45 pins 1..6. Power the Uno **from its USB only**;
leave RJ45 pin 7 (the port's 5 V) off the Uno's 5V pin, with GND on pin 8
(see `DEBUGNOTES.md`, 2026-10-19).

```sh
make -C firmware          # avr-gcc, avr-libc (apt: gcc-avr avr-libc)
make -C firmware flash    # PORT=/dev/ttyUSB0, through optiboot
```

`uno_flash.py` writes the image through the Uno's optiboot loader (avrdude
7.1 lost sync with it on this board) and reads it back.

## Firmware commands

115200 8N1, one line per command and one answer line. See the header of
`firmware/fbtest.c`.

| Command | Answer |
|---|---|
| `i` | `fbtest <ver> pins=6` |
| `o <ddr> <port>` | set A0..A5 (hex bit masks): driven 0/1, or input with pull-up / hi-Z; `ok` |
| `d` | `d <pins hex>` |
| `a [n]` | `a <mV A0> .. <mV A5> vcc=<mV>`: mean of n samples, Vcc from the bandgap (±10 %) |
| `q <us> <steps> <s0> <s1> <s2> <s3>` | quadrature: steps through four port states, `q <index>` |
| `c <bit> <ms>` | edges counted on A<bit>: `c <n>` |

The drive measures the port itself too: `io0.fb1` (pin 7, single-ended),
`adc0.sin1` / `adc0.cos1` (pairs 1-2, 3-6, differential), `io0.fb1a/b/z`
(RS-422 receivers on 1-2, 3-6, 4-5).
