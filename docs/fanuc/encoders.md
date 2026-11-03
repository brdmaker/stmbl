# FANUC serial encoders

| encoder | type | on | counts/turn | stmbl |
|---|---|---|---|---|
| αA64 | A860-0360-T001 | α2/3000 | 65,536 (16 bit) | `encf`, template `conf/template/fanuc_fb0.txt` |
| βiA128 | A860-2020-T301 | βiS 12/2000, βiS 4/4000 | 131,072 (2¹⁷) | not implemented; to be tested with `encf` |

Both are absolute while their battery (+6 V) is kept up ✱. Without the
battery they count incrementally until they cross the index once.

## What `encf` does (src/comps/encf.c)

- **Link:** RS-485, 1.024 Mbit/s. Every real-time period the drive sends a
  request on the TX pair (SPI3 shifting out `encf0.req_len`, TX enable on
  PD15). The encoder answers on the RX pair (FB0 A), captured edge by edge
  with a timer and DMA.
- **Reply:** 76 bits, checked with a 5-bit CRC (ITU, MSB first). As the
  comment in `encf.c` describes it for the αA64:

| bits | field |
|---|---|
| 0–4 | constant 0b00101 |
| 5 | battery fail |
| 8 | un-indexed (1 until the index has been crossed) |
| 18–33 | position, 16 bits per turn (the αA1000 adds lower bits in 10–15) |
| 36–51 | turns, 16 bits |
| 54–63 | commutation track, 10 bits (absolute, always valid) |
| 71–75 | CRC-5 |

- **Pins to watch:**
  - `encf0.crc_ok`, `encf0.crc_er` count good and bad frames;
  - `encf0.dma` is the number of edges captured in the last frame;
  - `encf0.pos`, `encf0.abs_pos`, `encf0.turns`, `encf0.com_pos` are the
    decoded values;
  - `encf0.index` is the un-indexed bit.
- **Raw dump:** `encf0.send_step = 100` prints the received frame bits to
  the console, one line per print.

Whether the βiA128 answers the same request with the same frame is not
known. FANUC's i-series encoders may use a different request or a longer
frame ✱.

## Wiring to stmbl FB0

As `encf` uses the port: RX on the A pair, request on the Z pair. The
FANUC pin numbers are for the amplifier's JF1 and are from memory ✱;
check them by continuity, or in the motor manuals (B-65302, B-65142E). The
motor's own encoder connector has different numbers.

| signal | FANUC JF1 (amplifier) ✱ | stmbl FB0 (8P8C) |
|---|---|---|
| RD / *RD (data from the encoder) | 1 / 2 | 1 / 2 (A+ / A−) |
| REQ / *REQ (request to the encoder) | 5 / 6 | 5 / 4 (Z+ / Z−) |
| +5 V | 9, 18, 20 | 7 (VCC, 5 V jumper setting) |
| 0 V | 12, 14, 16 | 8 |
| +6 V battery | 7 | not connected for the first tests |
| shield | shell | connector shell |

## The cable on hand

A third-party green encoder cable, cut at one end (colours are the cable
maker's, not FANUC's). Known so far:

| wires | kind | role |
|---|---|---|
| violet / black | twisted pair | RD or REQ ✱ |
| white / brown | twisted pair | REQ or RD ✱ |
| yellow, yellow, green, green, grey, pink, blue | single, thicker | supply: +5 V and 0 V (doubled), battery +6 V ✱ |
| foil shield | | to the connector shell |

Still to do: ring each wire to its pin in the motor-end connector (encoder
unplugged, ohmmeter only), then fill in the colour → pin → FB0 table.
Don't find the supply by powering up: a wrong 5 V can kill the encoder.

## Bench test

Encoder only: no high voltage, motor phases disconnected.

1. Back up the drive's config: `servoterm-cli --dump`.
2. Load the `fanuc_fb0` template (`encf` on FB0).
3. **α2/3000 first.** Expect `crc_ok` counting up and `pos` following the
   shaft. If not, the cable or the supply is wrong; fix that before trying
   the βiA128.
4. **Then a βiA128 on the same cable.**
   - `crc_ok` counting: it speaks the αA64 frame. Check the resolution
     (`pos` over one turn) and the commutation track.
   - `crc_er` counting or `dma` near zero: set `encf0.send_step = 100`,
     turn the shaft slowly in one direction, and save about 20 lines. Do
     the same with the αA64 for comparison.

## If the βiA128 differs

1. **Record the FANUC system's own exchange** (request and reply) with a
   logic analyzer, as in [fanuc-system.md](fanuc-system.md). Don't use
   stmbl as the listener: `encf` drives the request line itself.
2. **Decode:** the bits that change with the angle locate the position,
   the fixed ones the framing; check the CRC on the captures.
3. **Implement** a βiA128 mode in `encf` (or a new comp) with a host test
   that decodes the recorded frames (`host/tests/*.test`), before it runs
   on a motor.
