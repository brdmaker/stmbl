# Baldor linear motors: reference for stmbl

What we need from Baldor's linear-motor documentation to run these motors
on stmbl. It is extracted from two Baldor documents, which are not in the
repository:

- **MN1800**, *Linear Motors Installation & Operating Manual* (1/01): wiring,
  connectors, air gaps, encoders.
- **BR1202**, the linear motors and stages catalog: motor data and
  dimensions.

The shop's motor (2026-10): a Baldor nameplate ("SPEC # …NB0389A00", serial
"…0801040014"; the left edge is hidden), one 3-pin power connector, and a
6-pin Hall connector that matches LD9073A00. That points to an **LMCF**
(cog-free) coil, possibly as part of a stage. Still to measure: the
phase-to-phase resistance (to pick the model), the encoder and the pitch.
See [Bring-up checks](#bring-up-checks).

## Contents

- [Families and how to tell them apart](#families-and-how-to-tell-them-apart)
- [Electrical period and stmbl settings](#electrical-period-and-stmbl-settings)
- [Wiring](#wiring)
- [Encoders used on Baldor stages](#encoders-used-on-baldor-stages)
- [Mechanical installation notes](#mechanical-installation-notes)
- [LMCF data (cog-free)](#lmcf-data-cog-free)
- [LMBL data (iron core)](#lmbl-data-iron-core)
- [Catalog numbers](#catalog-numbers)
- [Bring-up checks](#bring-up-checks)

## Families and how to tell them apart

| Family | What it is | How to recognise it | stmbl |
|---|---|---|---|
| **LMCF** | brushless, ironless coil moving inside a U-channel magnet track | magnets on both inner walls of a U channel; no pull between coil and track | PMSM (`link pmsm`) |
| **LMBL** | brushless, iron-core coil over a flat magnet plate | flat magnet track; the coil is pulled down hard (0.9–32 kN) | PMSM |
| LMBR | brushed DC, moving magnets and brushes over a commutator primary | commutator bar along the primary | DC |
| LMNC / LMNM | non-commutated voice coil (moving coil / moving magnet), short stroke | cylinder, 2 wires (white +, black −) | DC |
| LMSS / LMDS | linear stepper (single / dual axis), forcer on a toothed platen | etched teeth in a steel platen, DE-9 or DB-25 connector | 2- or 4-phase stepper |
| LMAC / LMPY | linear induction / polynoid | 3-phase AC on a plate / rod | — |
| LSE / LSC / LSS / LSX | complete stages: LMCF or LMBL motor, bearings, encoder, limits, bellows | aluminium stage with cable chain and bellows | PMSM |

The LMCF and LMBL come with Hall sensors as standard (the `H` in `-HC0`),
for sine-commutation alignment at power-up, trapezoidal drive and crash
recovery. Custom versions without Halls exist.

## Electrical period and stmbl settings

The drive has no linear mode. A linear motor runs as a rotary one in which
one revolution is one electrical period (north to north): see
`host/README.md`, "Linear motors".

| Family | Magnet pitch (catalog) | Electrical period p | Multiple-coil spacing (MN1800) |
|---|---|---|---|
| LMCF | 1.200 in (30.48 mm) | **60.96 mm** | multiple of 2.4 in |
| LMBL size A, B | 0.9 in (22.86 mm) | **45.72 mm** | multiple of 1.8 in |
| LMBL size E | not printed (the track repeats every 3.6 in) | probably 91.44 mm: measure it | — |
| LMSS / LMDS | tooth pitch 0.040 in (1.016 mm) | full step 0.010 in (2-phase), 0.005 in (4-phase) | — |

For an LMCF:
- `conf0.polecount = 1`;
- `conf0.mot_fb_res` = encoder counts per 60.96 mm (12192 for a 5 µm
  quadrature scale; 60960 at 1 µm);
- `linrev0.scale = 60.96`, so LinuxCNC works in mm.

The constants convert with p/2π (`host/README.md`):
- `psi = Kf · p / (3π)`: the column in the tables below;
- `j = m · (p/2π)²` for a moving mass m;
- `max_force` = peak force · p/2π.

The catalog's R and L are **phase to phase**, the data are at a 75 °C rise
(LMCF) or 100 °C (LMBL), and the catalog doesn't say whether Kf is peak or
RMS. So the psi column is a starting value: let `id_pmsm` / `rlpsij` measure
r, l and psi on the motor.

## Wiring

### Power, Halls, limits: AY1763A00 (flying leads; LMBL, LMCF and the stages)

| Motor cable | LinDrive / MintDrive | "Trap" drives |
|---|---|---|
| Phase A (U) | red | black |
| Phase B (V) | white | red |
| Phase C (W) | black | white |
| Motor ground | green | |
| Thermal switch + | blue | |
| Thermal switch − | orange | |

The manual says the phasing is for LinDrive/MintDrive, and other controls
may need a different order (most trap drives: U black, V red, W white).
Phases are 120° apart. **On stmbl, don't trust the colours:** match the
phase order to the Halls (see [Bring-up checks](#bring-up-checks)).

| Hall cable | colour | | Limit cable | colour |
|---|---|---|---|---|
| Hall 1 | white | | Limit + output | white |
| Hall 2 | red | | Limit − output | red |
| Hall 3 | black | | Home output | black |
| Hall ground | green | | Limit & home power (+4…+24 V) | brown |
| Hall +5 V | brown | | Limit & home ground | green |

In the AY1779A00 / AY1801A00 / AY1802A00 stage cables, the thermal-switch
colours are swapped (orange +, blue −) and the Hall ground may be blue, green
or yellow; the +5 V may be brown or violet.

### LD9073A00 Hall connector (6 pin to flying leads; LMCF)

| Pin | Colour | Signal |
|---|---|---|
| 1 | black | Hall C (3) |
| 2 | red | Hall B (2) |
| 3 | — | not connected |
| 4 | green | ground |
| 5 | white | Hall A (1) |
| 6 | brown | +5 V |

### Stage encoder + Hall D-sub (15 pin, LinDrive)

AY1775A00 (LSE, RGH24) and AY1800A00 (RGH22) use the same pinout:

| Pin | Signal | | Pin | Signal |
|---|---|---|---|---|
| 1 | A+ | | 8 | C− (reference mark) |
| 2 | B+ | | 9 | Hall 3 |
| 3 | C+ (reference mark) | | 10 | Hall 2 |
| 4 | Hall 1 | | 11 | +5 V (Halls brown) |
| 5 | n.c. | | 12 | n.c. |
| 6 | A− | | 13 | encoder GND (Halls green) |
| 7 | B− | | 14, 15 | n.c. (LSX: Hall shield, encoder shield) |

The outer shield goes to the shell, the inner shield to pin 13.

The wire colours differ by encoder:

| Cable | A+ | B+ | C+ | A− | B− | C− | +5 V | GND |
|---|---|---|---|---|---|---|---|---|
| AY1775A00 (RGH24) | green | blue | pink | yellow | red | gray | brown | white |
| AY1800A00 (RGH22) | green | blue | violet | yellow | red | gray | brown | white |
| AY1759A00 (RSF) | green | orange | blue | yellow | red | violet | black | brown |

AY1780A00 (MintDrive, 9 pin):

| Pin | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |
|---|---|---|---|---|---|---|---|---|---|
| Signal | +5 V | C+ | B− | inner shield | A+ | C− | 0 V | B+ | A− |

### Linear stepper connectors

AY0165A00 (LMSS roller bearing, DE-9 male):

| Pin | 1 | 3 | 5 | 7 | 9 | others |
|---|---|---|---|---|---|---|
| Colour | white | green | black | red | orange | — |
| Signal | A1+ | B1+ | ground | A1− | B1− | n.c. |

LD9145A00 (air bearing, 9 pin):

| Pin | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |
|---|---|---|---|---|---|---|---|---|---|
| Signal | A+ | A− | B+ | B− | ground | C+ | C− | D+ | D− |

## Encoders used on Baldor stages

All of them are 5 V, differential (RS-422) A/B plus a reference mark C/Z,
which suits stmbl's FB0 encoder input.

| Encoder | Resolution [µm] | Max speed | Reference mark | Supply |
|---|---|---|---|---|
| Renishaw RGH22 / RGH24 D, X, Z, Y | 5, 1, 0.5, 0.1 | 5, 3, 1.5, 0.3 m/s (RGH22) | ± one count, bidirectional | 5 V 120–150 mA |
| RSF MSA6x (enclosed) / MS6x (open glass) | 0.1, 1, 5, 10 | e.g. 10 m/s at 10 µm | 35 mm from each end (MSA6x); every n × 50 mm (MS6x) | 5 V 150–200 mA |
| ELGO Mix 4 / Mix 5 (magnetic) | 5, 10, 50 | Mix 4: 2, 4, 20 m/s; Mix 5: 1.2, 2.5, 12.5 m/s | Mix 4: one shot after power-up; Mix 5: every pass (≤ 0.1 m/s) | 5 V 200 mA |
| Danaher LR0xxRD | 5, 10, 25 | 20 m/s | yes | 5 V 100 mA |

Periodic reference marks (MS6x every n × 50 mm, Mix 5) re-latch `enc_fb`'s
absolute position at each mark. If the mark spacing is not a multiple of the
electrical period (60.96 mm), commutation goes wrong after the next mark
(`host/tests/linear_periodic_marks.test`). In that case, use only one mark,
or commutate from the Halls.

## Mechanical installation notes

- **LMCF:** centre the coil in the U channel, with equal vertical and
  horizontal air gaps over the whole travel. The coil must never touch the
  magnets. The modular tracks butt end to end, and their hole pattern repeats
  across the joints.
- **LMBL:** air gap 0.030 ± 0.005 in, set with a non-magnetic shim that is
  removed before running. The bearings carry the magnetic pull (up to
  32 kN). The coil snaps onto the track, so fingers stay clear.
- Magnet tracks: unpack one section at a time, keep them apart from each
  other and from steel, and never flip them inside the box.
- Stages: mount on a surface flat to 0.0005 in/ft (0.010 in/ft at most).
  Lubricate the recirculating rails once a year (lithium No. 2 grease); the
  cross-roller bearings normally need nothing.
- Before running: set the drive's continuous and peak current to the
  motor's, start with low gains, and strain-relieve the moving cables.

## LMCF data (cog-free)

The catalog's figures (BR1202, as printed), with psi computed for
p = 60.96 mm. The coil length without Halls is about 14 mm shorter. Size
letters (magnet track width): A 40.7, B 53.6, C 57.2, D 86.4, E 114.3,
F 152.4 mm. Track sections are 121.9, 182.9, 243.8, 304.8 or 609.6 mm long.

| model | poles | coil w/ Halls [mm] | coil [kg] | cont. [N] / [A] | peak [N] / [A] | Kf [N/A] | R ph-ph [Ω] | L ph-ph [mH] | psi [Vs] |
|---|---|---|---|---|---|---|---|---|---|
| LMCF02A-HC0 | 2 | 87.6 | 0.08 | 5.3 / 1.7 | 16 / 5.1 | 3.1 | 2.7 | 3.2 | 0.020 |
| LMCF02B-HC0 | 2 | 87.6 | 0.11 | 13.8 / 2.1 | 41.8 / 6.3 | 6.7 | 3.2 | 15.9 | 0.043 |
| LMCF04B-HC0 | 4 | 148.6 | 0.22 | 27.8 / 2.1 | 83.3 / 6.3 | 13.2 | 6.4 | 4.0 | 0.085 |
| LMCF02C-HC0 | 2 | 87.6 | 0.18 | 29 / 1.9 | 86.8 / 5.7 | 15.2 | 5.3 | 12.1 | 0.098 |
| LMCF04C-HC0 | 4 | 148.6 | 0.32 | 58 / 1.9 | 173 / 5.7 | 30.4 | 10.6 | 10.6 | 0.197 |
| LMCF06C-HC0 | 6 | 209.6 | 0.57 | 87 / 1.9 | 260 / 5.7 | 45.6 | 15.9 | 15.9 | 0.295 |
| LMCF08C-HC0 | 8 | 270.5 | 0.75 | 115.6 / 1.9 | 347 / 5.7 | 60.9 | 21.1 | 21.2 | 0.394 |
| LMCF02D-HC0 | 2 | 87.6 | 0.35 | 36.8 / 1.5 | 110 / 4.4 | 24.8 | 9.1 | 12.1 | 0.160 |
| LMCF04D-HC0 | 4 | 148.6 | 0.6 | 73.6 / 1.5 | 220 / 4.4 | 49.6 | 18.2 | 24.2 | 0.321 |
| LMCF06D-HC0 | 6 | 209.6 | 0.9 | 110 / 1.5 | 331 / 4.4 | 74.4 | 27.3 | 36.2 | 0.481 |
| LMCF08D-HC0 | 8 | 270.5 | 1.2 | 147 / 1.5 | 441 / 4.4 | 99.3 | 36.4 | 48.3 | 0.642 |
| LMCF10D-HC0 | 10 | 331.5 | 1.5 | 183.6 / 3.0 | 550 / 8.9 | 61.8 | 11.3 | 15.0 | 0.400 |
| LMCF12D-HC0 | 12 | 392.4 | 1.8 | 220.3 / 3.0 | 660 / 8.9 | 74.2 | 13.6 | 18.0 | 0.480 |
| LMCF04E-HC0 | 4 | 148.6 | 0.77 | 124 / 1.6 | 372 / 4.7 | 79.9 | 23 | 34.9 | 0.517 |
| LMCF06E-HC0 | 6 | 209.6 | 1.12 | 185 / 3.1 | 556 / 9.3 | 59.7 | 8.6 | 13.0 | 0.386 |
| LMCF08E-HC0 | 8 | 270.5 | 1.47 | 251 / 3.1 | 753 / 9.2 | 82 | 11.7 | 18.4 | 0.530 |
| LMCF10E-HC0 | 10 | 331.5 | 1.82 | 314 / 3.1 | 942 / 9.2 | 102.5 | 14.7 | 23.0 | 0.663 |
| LMCF12E-HC0 | 12 | 392.4 | 2.17 | 377 / 3.1 | 1132 / 9.2 | 123 | 17.6 | 27.6 | 0.796 |
| LMCF14E-HC0 | 14 | 453 | 2.52 | 440 / 3.1 | 1321 / 9.2 | 143.5 | 20.5 | 32.2 | 0.928 |
| LMCF04F-HC0 | 4 | 156.2 | 1.65 | 191 / 2.6 | 578 / 7.8 | 74.4 | 10.5 | 13.6 | 0.481 |
| LMCF08F-HC0 | 8 | 278.1 | 3.07 | 387 / 2.6 | 1152 / 7.8 | 148.8 | 21.0 | 27.2 | 0.962 |
| LMCF12F-HC0 | 12 | 400 | 4.49 | 578 / 3.9 | 1726 / 11.6 | 148.4 | 14.0 | 18.0 | 0.960 |
| LMCF16F-HC0 | 16 | 522 | 5.91 | 771 / 5.2 | 2300 / 15.6 | 148.0 | 10.4 | 13.4 | 0.957 |

The LMCF08F/12F/16F hot resistance is 27.1 / 18.0 / 13.5 Ω.
02B (15.9 mH) and 04B (4.0 mH) are as printed.

## LMBL data (iron core)

The catalog's figures, with psi computed for p = 45.72 mm (sizes A and B).
Resistance is at 25 °C. The coil length is the same with or without Halls.
Width code: A 76.2, B 101.6, E 209.6 mm.

| model | poles | coil w/ Halls [mm] | coil [kg] | cont. [N] / [A] | peak [N] / [A] | Kf [N/A] | R ph-ph [Ω] | L ph-ph [mH] | psi [Vs] |
|---|---|---|---|---|---|---|---|---|---|
| LMBL02A-HC0 | 2 | 116 | 0.91 | 44.5 / 1.0 | 133.5 / 3 | 44.5 | 19 | 53 | 0.216 |
| LMBL04A-HC0 | 4 | 161 | 1.14 | 66.8 / 1.6 | 200.3 / 4.8 | 41.8 | 15 | 62 | 0.203 |
| LMBL06A-HC0 | 6 | 207 | 1.7 | 89 / 1.7 | 267 / 5.1 | 52 | 14 | 59 | 0.252 |
| LMBL06B-HC0 | 6 | 207 | 2.02 | 133.4 / 3.6 | 400 / 10.8 | 36.9 | 4.7 | 23.2 | 0.179 |
| LMBL08A-HC0 | 8 | 253 | 2.2 | 133.4 / 2.6 | 400 / 7.8 | 51.2 | 11.6 | 46.8 | 0.248 |
| LMBL08B-HC0 | 8 | 253 | 2.7 | 200 / 5.5 | 600 / 16.5 | 36.5 | 3.9 | 18.7 | 0.177 |
| LMBL10A-HC0 | 10 | 299 | 2.5 | 177.9 / 3.3 | 533 / 9.9 | 53.8 | 10.2 | 20.0 | 0.261 |
| LMBL10B-HC0 | 10 | 299 | 3.37 | 270 / 6.9 | 800 / 20.8 | 38.3 | 3.1 | 15.8 | 0.186 |
| LMBL12A-HC0 | 12 | 345 | 2.9 | 222 / 4.5 | 667 / 13.5 | 49.4 | 4.3 | 17.9 | 0.240 |
| LMBL12B-HC0 | 12 | 345 | 3.98 | 330 / 7.6 | 1000 / 22.7 | 44.0 | 2.3 | 11.6 | 0.213 |
| LMBL14A-HC0 | 14 | 390 | 3.5 | 267 / 4.2 | 800 / 12.5 | 64.1 | 5.8 | 20.8 | 0.311 |
| LMBL14B-HC0 | 14 | 390 | 4.6 | 400 / 6.0 | 1200 / 18.0 | 66.7 | 4.2 | 20.2 | 0.324 |
| LMBL16A-HC0 | 16 | 436 | 3.86 | 311.5 / 6.1 | 934 / 18.3 | 51.2 | 3.6 | 14.7 | 0.248 |
| LMBL16B-HC0 | 16 | 436 | 5.21 | 470 / 6.5 | 1400 / 19.4 | 72.5 | 4.8 | 23.1 | 0.352 |
| LMBL18A-HC0 | 18 | 482 | 4.2 | 356 / 4.8 | 1070 / 14.5 | 73.8 | 6.5 | 26.8 | 0.358 |
| LMBL18B-HC0 | 18 | 482 | 5.7 | 530 / 5.1 | 1602 / 15.4 | 104.1 | 8.6 | 42.1 | 0.505 |

Size E: LMBL08E-HC0 has 8 poles, 1110 N / 6.6 A continuous, 3520 N / 19.8 A
peak, 169 N/A, 23 Ω (75 °C) and 94 mH. LMBL17E-HC0 has 17 poles, 2224 N /
14.7 A, 7030 N / 44.1 A, 151 N/A, 10 Ω and 42 mH. Their period is to be
measured.

## Catalog numbers

- **Motor:** `LMCF` / `LMBL` + number of poles (02…18) + size letter, then
  `-` + `H` (Halls) or `N` (none) + cooling (`C` convection, `A` air, `W`
  water) + termination (`0` = 10 ft flying leads) + an optional `P`
  (parallel winding, LMCF). Example: `LMCF04C-HC0`.
- **Magnet tracks:** `LTCF-<size><length code>` (04, 07, 09, 12 or 24 =
  4.8, 7.2, 9.6, 12 or 24 in) and `LTBL-<size><length code>` (A/B: 05, 07,
  18; E: 07, 10, 25).
- **Spec numbers** like the shop motor's "NB0389A00" are Baldor build
  numbers, not catalog numbers. Neither document decodes them: ABB/Baldor
  support can look them up with the serial number.

## Bring-up checks

Do these by hand, with the drive off, before stmbl moves the motor:

1. **Halls:** 5 V on brown, ground on green. Slide the coil slowly: each of
   white, red and black should be high for half of every period, and the
   three should be offset by a third of a period (LMCF: 60.96 / 20.32 mm).
   This confirms the pitch, and whether the outputs need pull-ups.
2. **Phase-to-phase resistance**, all three pairs equal: it picks the model
   from the tables above.
3. **Phase order against the Halls:** push the coil by hand and scope the
   back-EMF U−V, V−W and W−U against Hall 1/2/3. Each Hall edge should sit
   at the same point of one back-EMF wave. This sets the order of U/V/W on
   the drive (the colours depend on the drive Baldor wired them for).
4. **Encoder:** resolution and reference marks (single, or periodic every
   50 mm?), so `mot_fb_res` and homing can be set.
5. **Thermal switch** (blue/orange) if the cable has one: wire it to a
   drive or LinuxCNC fault input.
