# brdstepper hardware: connections, parts, bring-up

A small closed-loop stepper drive on the back of a NEMA11 motor (28 mm
square, M2.5 holes 23 mm apart). It runs stmbl's control software on an
STM32G473, with an AS5047P magnetic encoder and two DRV8876 H-bridges,
one per coil.

**Status:**
- **Firmware:** builds (`brdstepper/`, see its README); not yet run on a
  board. The bring-up procedure (section 9) uses gdb until there is a
  console.
- **Simulation:** the control code runs in the host simulator with a NEMA11
  model.
- **Hardware:** no schematic yet. This document is the input for it.

**Sources:**
- **MCU:** pin numbers and functions from ST's pin data
  (STM32_open_pin_data: `STM32G473C(B-C-E)Ux` and the G47x GPIO
  alternate functions).
- **DRV8876:** pin numbers, straps and values from TI's datasheet SLVSDS7B
  (Nov 2019).
- **Everything marked ✱:** check against its own datasheet before the
  layout: the AS5047P, the transceivers, the buck, the TVS, the connectors.

**Contents:**
1. [Overview](#1-overview)
2. [MCU and flash](#2-mcu-and-flash)
3. [Power](#3-power)
4. [Connections](#4-connections)
5. [Current sensing](#5-current-sensing)
6. [Variants](#6-variants)
7. [Parts list](#7-parts-list)
8. [Layout notes](#8-layout-notes)
9. [Bring-up](#9-bring-up)
10. [Software](#10-software)

## 1. Overview

```
 motor supply 12-36 V ─┬─ TVS, bulk ──────────────────┬─ DRV8876 #1 ── coil A
 (J2)                  │                              └─ DRV8876 #2 ── coil B
                       └─ buck 5 V ─┐                      ▲ IN1/IN2, IPROPI, nSLEEP, nFAULT, VREF
 USB 5 V (J1) ──────────────────────┼─ OR ─ LDO 3.3 V ── STM32G473CEU6 ── SPI ── AS5047P (shaft magnet)
 Mesa port 5 V (J4) ────────────────┘                      │
                                                           ├─ USART1 ─ RS-422 ── J4 smart-serial (variant A)
                                                           ├─ FDCAN1 ─ CAN ───── J5, J6 CAN (variant B)
                                                           └─ USB FS ─────────── J1
```

There is one PCB with two assembly variants: smart-serial to a Mesa card
(LinuxCNC), or CAN. USB is on both, for setup, the console and DFU
recovery. A resistor divider on PB1 tells the firmware which variant it
runs on.

## 2. MCU and flash

**STM32G473CEU6:** UFQFPN-48, 7 × 7 mm, 512 kB flash (dual bank), 128 kB
RAM, 170 MHz.

The STM32G431CBU6 and STM32G474CEU6 fit the same footprint, and every pin
used below has the same position and function on them. Why the G473:
- **RAM:** stmbl's software needs ~70–80 kB; the G431 has 32 kB.
- **Dual-bank flash:** the firmware writes its config and update staging
  into bank 2 while running from bank 1, with no CPU stall.

**Flash layout** (dual bank, 2 kB pages; `brdstepper/inc/flash.h`):

| bank | address | region | size |
|---|---|---|---|
| 1 | 0x08000000 | bootloader (to do; until then the application starts here) | 16 kB |
| 1 | 0x08004000 | application | 240 kB |
| 2 | 0x08040000 | in-band update staging | 240 kB |
| 2 | 0x0807C000 | config | 16 kB |

**Option bytes:** set once per chip with `make -C brdstepper option_bytes`.
- **nSWBOOT0 = 0, nBOOT0 = 1:** the BOOT0 pin is ignored. PB8 doubles as
  FDCAN1 RX, which a CAN transceiver holds high while the bus is idle; with
  BOOT0 active, every power-up would start ST's ROM bootloader.
- **DBANK = 1:** dual bank. This is the factory default; check it with
  `make option_bytes_show`.

ST's ROM DFU stays reachable from the firmware: the `bootloader` command
jumps there.

## 3. Power

| rail | from | to | notes |
|---|---|---|---|
| VM 12–36 V | J2 (motor supply) | DRV8876 VM ×2, the buck, the VM divider | DRV8876: 4.5–37 V operating, 40 V absolute. TVS at the input, bulk near the drivers. |
| 5V_BUCK | buck (U6) from VM | the 5 V OR | ≥ 300 mA |
| 5V_USB | J1 VBUS | the 5 V OR, via ESD | setup with no motor supply |
| 5V_MESA | J4 pin (smart-serial variant) | the 5 V OR | the Mesa port's 5 V, if the card provides it |
| 5V | dual Schottky OR (D1: BAT54C, and a second for the third source) | LDO, CAN transceiver VCC | ~4.6 V after the diode drop |
| 3V3 | LDO (U5) | MCU VDD / VBAT, AS5047P, transceivers' logic side, pull-ups, NTC | ~60 mA in all |
| VDDA / VREF+ | 3V3 through a ferrite bead | MCU VDDA (21) and VREF+ (20) | 1 µF + 10 nF at the pins; the ADC reference is this rail (`BRD_VREF` 3.3 V) |

**Three 5 V sources:** with all three fitted, use two BAT54Cs, or a BAT54C
plus a single Schottky on 5V_MESA. A variant without the Mesa port needs
only one BAT54C (USB and the buck).

**Load:**
- logic below 100 mA;
- the bridges carry the motor current: 0.67 A per coil for the reference
  motor, 3.5 A peak per DRV8876.

## 4. Connections

### 4.1 MCU (U1, STM32G473CEU6, UFQFPN-48)

| pin | name | net | to |
|---|---|---|---|
| 1 | VBAT | 3V3 | (no battery) |
| 5 | PF0-OSC_IN | OSC_IN | Y1, 8 MHz |
| 6 | PF1-OSC_OUT | OSC_OUT | Y1 |
| 7 | PG10-NRST | NRST | 100 nF to GND, the SWD pads |
| 8 | PA0 (ADC1_IN1) | IPROPI_A | U2 pin 4, RIPROPI A |
| 9 | PA1 (ADC2_IN2) | IPROPI_B | U3 pin 4, RIPROPI B |
| 10 | PA2 (ADC1_IN3) | VM_SENSE | VM divider 100k / 10k, 10 nF |
| 11 | PA3 (ADC1_IN4) | NTC | NTC to GND, 10k to 3V3 |
| 12 | PA4 (DAC1_OUT1) | VREF_DRV | U2 pin 3, U3 pin 3, 100 nF to GND |
| 13 | PA5 | nSLEEP | U2 pin 1, U3 pin 1, 100k to GND |
| 14 | PA6 | nFAULT | U2 pin 2, U3 pin 2, 10k to 3V3 |
| 15 | PA7 | EN_IN | J2 pin 3 (enable / estop) via 10k, 100k to GND, 10 nF |
| 17 | PB0 (ADC1_IN15) | AIN | spare analog input: 1k series, 10 nF |
| 18 | PB1 (ADC1_IN12) | HWID | the variant divider (§6) |
| 20 | VREF+ | VDDA | 1 µF + 10 nF |
| 21 | VDDA | VDDA | ferrite bead from 3V3, 1 µF + 10 nF |
| 23, 35, 48 | VDD | 3V3 | 100 nF each, 4.7 µF once |
| 25 | PB12 | LED | LED1 + 1k to GND |
| 26 | PB13 | CAN_STB | U9 STB (CAN variant) |
| 29 | PC6 (TIM8_CH1, AF4) | B_IN2 | U3 pin 16 |
| 30 | PA8 (TIM1_CH1, AF6) | A_IN1 | U2 pin 15 |
| 31 | PA9 (TIM1_CH2, AF6) | A_IN2 | U2 pin 16 |
| 32 | PA10 (TIM1_CH3, AF6) | B_IN1 | U3 pin 15 |
| 33 | PA11 | USB_DM | U7, J1 D− |
| 34 | PA12 | USB_DP | U7, J1 D+ (the internal pull-up; no resistor) |
| 36 | PA13 | SWDIO | SWD pads |
| 37 | PA14 | SWCLK | SWD pads |
| 38 | PA15 | ENC_CS | U4 CSn |
| 41 | PB3 (SPI1_SCK, AF5) | ENC_CLK | U4 CLK (also SWO: not used) |
| 42 | PB4 (SPI1_MISO, AF5) | ENC_MISO | U4 MISO |
| 43 | PB5 (SPI1_MOSI, AF5) | ENC_MOSI | U4 MOSI |
| 44 | PB6 (USART1_TX, AF7) | SS_TX | U8 D (smart-serial variant) |
| 45 | PB7 (USART1_RX, AF7) | SS_RX | U8 R |
| 46 | PB8-BOOT0 (FDCAN1_RX, AF9) | CAN_RX | U9 RXD (CAN variant); 10k to GND (BOOT0 low until the option bytes are set) |
| 47 | PB9 (FDCAN1_TX, AF9) | CAN_TX | U9 TXD |
| pad | VSS | GND | exposed pad: the only VSS / VSSA connection in this package. Solder it, with vias. |

**Spare I/O:** PB2, PB10, PB11, PB14, PB15, PC4, PC10, PC11, PC13.
Candidates:
- a step/dir input;
- an index or second encoder;
- a second UART (USART3 on PB10 / PB11 or PC10 / PC11).

Bring at least two out to test pads.

### 4.2 H-bridges (U2 coil A, U3 coil B: DRV8876RGTR, VQFN-16)

Both are wired the same, apart from the MCU nets in the last column.

| pin | name | connection | U2 (A) | U3 (B) |
|---|---|---|---|---|
| 1 | nSLEEP | MCU, 100k pull-down (shared) | nSLEEP | nSLEEP |
| 2 | nFAULT | open drain, wired-or, 10k to 3V3 (shared) | nFAULT | nFAULT |
| 3 | VREF | DAC, 100 nF (shared) | VREF_DRV | VREF_DRV |
| 4 | IPROPI | 2.0 kΩ 1 % to GND, ≤ 100 pF, MCU ADC | IPROPI_A | IPROPI_B |
| 5 | IMODE | **open** (Hi-Z): fixed off-time chopping, overcurrent latched off | — | — |
| 6 | OUT1 | motor coil + | A+ | B+ |
| 7 | PGND | GND (power) | | |
| 8 | OUT2 | motor coil − | A− | B− |
| 9 | VM | VM, 100 nF 50 V at the pin, 10 µF 50 V | | |
| 10 | VCP | 100 nF 16 V X7R to VM | | |
| 11 | CPH | 22 nF 50 V X7R to CPL | | |
| 12 | CPL | | | |
| 13 | GND | GND | | |
| 14 | PMODE | **3V3** (logic high: PWM mode, IN1 / IN2) | | |
| 15 | EN/IN1 | MCU timer | A_IN1 (PA8) | B_IN1 (PA10) |
| 16 | PH/IN2 | MCU timer | A_IN2 (PA9) | B_IN2 (PC6) |
| pad | thermal pad | GND, with vias to the ground plane | | |

**Coil direction:** a swapped coil is set right in the config
(`conf0.out_rev`), but A on U2 and B on U3 must not be mixed up with each
other's MCU nets.

### 4.3 Encoder (U4, AS5047P-ATSM, TSSOP-14) ✱

| signal | connection |
|---|---|
| CSn | ENC_CS (PA15) |
| CLK | ENC_CLK (PB3) |
| MISO | ENC_MISO (PB4) |
| MOSI | ENC_MOSI (PB5) |
| VDD, VDD3V3 | together to 3V3: the 3.3 V supply mode. Decoupling as the datasheet's 3.3 V application circuit. ✱ |
| GND, TEST pins | GND, as the datasheet says ✱ |
| A, B, I / U, V, W / PWM | not used (could go to test pads) |

The firmware reads ANGLECOM (0x3FFF) over SPI mode 1, 16 bit, at
5.3 MHz, then a NOP to clock out the fresh angle.

**Mechanics:**
- the IC centred on the motor shaft, within the datasheet's tolerance;
- a diametric magnet, 6 × 2.5 mm NdFeB, on the rear shaft end (a
  double-shaft motor, or glued);
- the air gap per the datasheet (about 0.5–3 mm) ✱.

### 4.4 Smart-serial (variant A: U8 + J4)

A 3.3 V full-duplex RS-422 transceiver (e.g. SN65HVD30 ✱). The driver is
always on.

| transceiver | net | J4 (JST-GH 6) |
|---|---|---|
| D (driver in) | SS_TX (PB6) | |
| Y, Z (driver out) | | 1 TX+, 2 TX− |
| A, B (receiver in) | 120 Ω across, at the transceiver | 3 RX+, 4 RX− |
| R (receiver out) | SS_RX (PB7) | |
| VCC | 3V3, 100 nF | |
| | 5V_MESA (through its Schottky) | 5 +5V |
| GND | GND | 6 GND |

**Cable:** an adapter to the Mesa card's RJ45 smart-serial port. The
drive's TX pair goes to the card's RX pair and vice versa: take the RJ45
pin assignment from the Mesa card's manual ✱. A pin of J4 may carry the
Mesa port's 5 V if the card provides it.

### 4.5 CAN (variant B: U9 + J5, J6)

TCAN1044V ✱ (CAN FD): VCC 5 V for the bus side, VIO 3.3 V for the logic.

| transceiver | net | J5, J6 (JST-GH 4, in parallel: daisy chain) |
|---|---|---|
| TXD | CAN_TX (PB9) | |
| RXD | CAN_RX (PB8) | |
| STB | CAN_STB (PB13); the firmware holds it high (standby) until CAN runs | |
| VCC | 5V, 100 nF | |
| VIO | 3V3, 100 nF | |
| CANH, CANL | 120 Ω across, behind solder jumper JP1 (fit only at the bus ends) | 1 CANH, 2 CANL |
| | VM_IN (power over the cable, optional) | 3 V+ |
| GND | GND | 4 GND |

### 4.6 USB (J1)

| J1 | net | notes |
|---|---|---|
| VBUS | 5V_USB | via U7 (USBLC6-2SC6) |
| D− / D+ | USB_DM (PA11), USB_DP (PA12) | through U7; 90 Ω differential, short |
| CC1, CC2 | 5.1 kΩ each to GND | USB-C sink (the MCU's own CC pins are not used) |
| GND, shield | GND (the shield through 1 MΩ ‖ 4.7 nF, or directly) | |

There is no USB crystal: HSI48 with CRS trims itself to the host's
start-of-frame packets.

If USB-C doesn't fit at the board edge, use a 4-pin JST-SH (VBUS, D−, D+,
GND) with a pigtail. In that case there are no CC resistors.

### 4.7 Other connectors

| ref | type | pins |
|---|---|---|
| J2 | JST-XH 3 ✱ (current rating for the total supply current) | 1 VM_IN, 2 GND, 3 EN_IN (enable / estop input, 3.3–24 V tolerant through 10k + clamp ✱) |
| J3 | JST-PH 4 or solder pads | 1 A+, 2 A−, 3 B+, 4 B− |
| SWD | Tag-Connect TC2030 pads (or 1.27 mm 5-pin) | VCC (3V3), SWDIO, NRST, SWCLK, GND |

**EN_IN:** above 3.6 V the MCU pin needs protection. Use a divider, or a
series resistor with a clamp diode to 3V3, sized for the highest enable
voltage used ✱.

### 4.8 Analog

| net | circuit | firmware |
|---|---|---|
| VM_SENSE | VM ─ 100k ─┬─ 10k ─ GND, 10 nF at the pin | `BRD_VBUS_DIV` 11: 3.0 V at 33 V |
| NTC | 3V3 ─ 10k 1 % ─┬─ NTC 10k B3380 ─ GND, close to the drivers | `hv0.hv_temp` (β formula) |
| HWID | 3V3 ─ R1 ─┬─ R2 ─ GND (§6) | `adc0.hwid`, `adc0.variant` |
| AIN | input ─ 1k ─┬─ 10 nF ─ GND | spare |

## 5. Current sensing

**What the DRV8876 measures** (SLVSDS7B 7.3.3): IPROPI mirrors the
low-side MOSFETs, only current flowing drain to source, AIPROPI =
1000 µA/A.
- **Driving:** it reads the coil current if it flows the way the bridge
  drives, else 0.
- **Braking** (both low sides on, slow decay): it reads the magnitude.
- **Coast:** it reads nothing.

**Datasheet figures:**
- error ±7.5 mA below 0.15 A and ±5 mA to 0.5 A (an offset), ±6.5 % above
  0.5 A (RGT);
- sense delay tDELAY 1.6 µs; input to output tPD 0.65 µs.

**What the firmware does** (`bip_sense` and `bip_cal_*` in
`shared/bip_loop.c`):
- **Two samples:** both coils are sampled twice per PWM period, in the
  drive window (counter at 0) and in the brake window (counter at top).
  ADC1 and ADC2 sample simultaneously.
- **Signed current:** drive ≈ brake means the current flows with the
  drive; drive 0 and brake > 0 means against it.
- **Sign:** when the drive window is too short to sample, the sign comes
  from the current command.
- **Offset calibration:** at each enable, both bridges brake for 3.2 ms
  with the rotor still. The current is zero then, so the reading is the
  offset, and it is subtracted.
- **Windows:** `hv0.sense_window` 0.12 and `hv0.max_duty` 0.88. A window
  must hold tPD + tDELAY + the ADC's 0.15 µs from its start: 4.8 µs
  centred, 9.6 % of 50 µs.

**In the simulator** (NEMA11, 24 V, 0.7 Ω bridge resistance, 5 mA noise):

| sensing | standstill | 10 rev/s | slow-speed ripple (detent comp.) |
|---|---|---|---|
| ideal | 0 | 0.4 mrad | ~210 rad/s² |
| IPROPI, typical (`stepper_ipropi.test`) | 0 | 0.8 mrad | ~750 rad/s² |
| IPROPI, datasheet limits (`stepper_ipropi_worst.test`) | 0 | 0.8 mrad | ~1360 rad/s² |
| the same without the offset calibration | 0 | 1.2 mrad | ~9200 rad/s² |
| no detent compensation | | | ~3000 rad/s² |

**Hardware rules that follow:**
- **RIPROPI:** 2.0 kΩ 1 % (3.3 V = 1.65 A; 1.5 kΩ for 2.2 A).
- **IPROPI cap:** ≤ 100 pF. TI's 10 nF suggestion is for the trip
  comparator; it would make the samples settle far too slowly.
- **IPROPI trace:** short, no PWM nodes nearby.
- **PMODE high, IMODE open:** in the cycle-by-cycle IMODEs nFAULT also goes
  low while chopping, and the firmware treats nFAULT as a fault.
- **VREF:** sets the chip's own current trip, ITRIP = VREF / (RIPROPI ×
  AIPROPI). The firmware sets 1.5 × max_cur: 2.0 V for 0.67 A. VREF also
  clamps the IPROPI voltage.

**Option:** a bidirectional shunt amplifier per coil (INA181-class plus a
shunt between each bridge's PGND and GND ✱, or in series with the coil)
makes all of this unnecessary. It costs 2 amplifiers, 2 shunts and board
space. Worth a footprint if there is room: the ADC inputs would be the
same pins.

## 6. Variants

One PCB; the interface section is chosen at assembly.

| | A: smart-serial | B: CAN |
|---|---|---|
| fitted | U8, J4, the 120 Ω on RX | U9, J5, J6, JP1 |
| not fitted | U9, J5, J6 | U8, J4 |
| logic power | Mesa 5 V, USB or the buck | the buck (V+ on the CAN cable) or USB |
| HWID (PB1): R1 from 3V3, R2 to GND | R1 not fitted, R2 0 Ω → 0 V | R1 10k, R2 10k → 1.65 V |
| `adc0.variant` | 0 | 1 |

## 7. Parts list

Values are per board; ✱ marks parts to check against their datasheet.

**ICs**

| ref | qty | part | package | function |
|---|---|---|---|---|
| U1 | 1 | STM32G473CEU6 | UFQFPN-48 | MCU |
| U2, U3 | 2 | DRV8876RGTR | VQFN-16 (RGT) | H-bridge per coil |
| U4 | 1 | AS5047P-ATSM ✱ | TSSOP-14 | magnetic encoder |
| U5 | 1 | TLV75533PDBVR (or AP2112K-3.3) ✱ | SOT-23-5 | 3.3 V LDO, ≥ 150 mA |
| U6 | 1 | TPS560430 ✱, set to 5 V | SOT-23-6 | buck from VM |
| U7 | 1 | USBLC6-2SC6 | SOT-23-6 | USB ESD |
| U8 | 1 (A) | SN65HVD30 ✱ (3.3 V full-duplex RS-422, ≥ 10 Mbps) | SOIC-8 (or smaller) | smart-serial |
| U9 | 1 (B) | TCAN1044V ✱ | SOIC-8 / VSON-8 | CAN FD |

**Discretes, power**

| ref | qty | value | package | where |
|---|---|---|---|---|
| D1 (D3) | 1 (2) | BAT54C dual Schottky | SOT-23 | 5 V OR (§3) |
| D2 | 1 | TVS SMAJ24A ✱ (for 24 V; lower for less) | SMA | VM input |
| C_bulk | 1–2 | 22 µF 50 V X7R (or 47–100 µF polymer) | 1210 | VM, near the drivers |
| C_VM | 2 + 2 | 10 µF 50 V X7R + 100 nF 50 V | 1206 / 0402 | each DRV8876 VM pin |
| C_VCP | 2 | 100 nF 16 V X7R | 0402 | VCP–VM, each driver |
| C_FLY | 2 | 22 nF 50 V X7R | 0402 | CPH–CPL, each driver |
| buck parts | — | as the TPS560430 datasheet: inductor, input / output caps, feedback for 5 V ✱ | | U6 |
| LDO caps | 2 | 1 µF in, 1 µF out (per the LDO's datasheet) ✱ | 0402 | U5 |

**Discretes, signal**

| ref | qty | value | where |
|---|---|---|---|
| R_IPROPI | 2 | 2.0 kΩ 1 % | IPROPI A, B to GND |
| C_IPROPI | 2 | 100 pF C0G (or not fitted) | IPROPI A, B |
| C_VREF | 1 | 100 nF | VREF_DRV |
| R_SLEEP | 1 | 100 kΩ | nSLEEP to GND |
| R_FAULT | 1 | 10 kΩ | nFAULT to 3V3 |
| R_VM | 2 | 100 kΩ 1 %, 10 kΩ 1 % | VM divider |
| C_VM_SENSE | 1 | 10 nF | VM_SENSE |
| NTC | 1 | 10 kΩ B3380, 0603 | near the drivers |
| R_NTC | 1 | 10 kΩ 1 % | NTC pull-up |
| R_HWID | 1–2 | per variant (§6) | HWID |
| R_EN, R_EN2, C_EN | 3 | 10 kΩ, 100 kΩ, 10 nF (+ clamp ✱) | EN_IN |
| R_AIN, C_AIN | 2 | 1 kΩ, 10 nF | AIN |
| R_BOOT0 | 1 | 10 kΩ | PB8 to GND |
| C_NRST | 1 | 100 nF | NRST |
| C_VDD | 3 + 1 | 100 nF + 4.7 µF | VDD 23, 35, 48 |
| FB1, C_VDDA | 1 + 2 + 2 | ferrite bead 600 Ω @ 100 MHz, 1 µF + 10 nF at VDDA, 1 µF + 10 nF at VREF+ | VDDA / VREF+ |
| Y1, C_Y | 1 + 2 | 8 MHz crystal 2016, ~10 pF C0G (per its CL) ✱ | OSC |
| LED1, R_LED | 1 + 1 | LED 0603, 1 kΩ | PB12 |
| C_ENC | per the AS5047P's 3.3 V circuit ✱ | | U4 |
| R_CC | 2 | 5.1 kΩ | USB-C CC1, CC2 |
| R_TERM | 1 | 120 Ω | RS-422 RX (A) or CAN (B, with JP1) |
| C_XCVR | 1–2 | 100 nF | U8 VCC / U9 VCC and VIO |

**Connectors and mechanics**

| ref | part |
|---|---|
| J1 | USB-C 2.0 receptacle (e.g. GCT USB4105) ✱, or JST-SH 4 |
| J2 | JST-XH 3 ✱ |
| J3 | JST-PH 4, or pads |
| J4 (A) | JST-GH 6, SM06B-GHS-TB |
| J5, J6 (B) | JST-GH 4, SM04B-GHS-TB |
| JP1 (B) | solder jumper |
| SWD | Tag-Connect TC2030 footprint |
| M1 | diametric magnet 6 × 2.5 mm NdFeB |
| board | 28 × 28 mm, M2.5 holes at 23 mm, U4 centred on the shaft |

## 8. Layout notes

- **Power loops:** keep each DRV8876's VM capacitors (100 nF at the pin,
  10 µF next to it) and the PGND return tight. Use thermal pads with vias
  into a ground plane: the drivers dissipate I² × 0.7 Ω per coil, ~0.3 W at
  0.67 A, ~14 °C over ambient (RθJA 45.9 °C/W on TI's board).
- **IPROPI:** route as analog. Short, over solid ground, away from the
  OUTx nodes. The resistor goes at the MCU end with the ADC pin, the
  driver end near its pin.
- **Encoder:** U4 exactly on the shaft axis. No current-carrying copper
  under it: the coil traces stay away, because their field reaches the
  magnet sensor.
- **Ground:** one plane. The MCU's exposed pad is its only ground: vias in
  the pad.
- **USB:** D+/D− as a short 90 Ω pair, with the ESD part at the connector.
- **RS-422 / CAN:** the transceiver close to its connector, the
  termination at the transceiver.
- **Test pads:** 3V3, 5V, VM, IPROPI_A, IPROPI_B, A_IN1, A_IN2, B_IN1,
  B_IN2, nFAULT, ENC_CS, LED, two spare I/O, GND (a few).

## 9. Bring-up

**No console yet:** the firmware has none until USB CDC or smart-serial is
ported (`brdstepper/README.md`, "Not done yet").

**Instead, gdb over SWD** with `brdstepper/tools/brd.gdb`:
```
openocd -f interface/stlink.cfg -f target/stm32g4x.cfg
arm-none-eabi-gdb -x tools/brd.gdb obj/brdstepper.elf      (in brdstepper/)
```

| command | what |
|---|---|
| `brd` | supply, NTC, ID divider, variant, PWM periods counted, HAL state, the four IPROPI samples |
| `pin hv 0 iq_fb` | any HAL pin's value (component, instance, pin) |
| `hal fault0.en 1` | any config line: `hal SINK SOURCE_OR_VALUE` |
| `enc` | one AS5047P angle read |

**The default config** (`brdstepper/conf/brdstepper.txt`):
- the stock pid chain, the stepper template, the AS5047P;
- the NEMA11 parameters (`conf/nema11.txt`);
- `fault0.en = hv0.en_in`: the enable input on J2 pin 3 enables the drive.

With no command source linked, the drive holds its position.

**Ground rules:**
- **Supply:** a current-limited lab supply for VM, 24 V. Limit it to 0.3 A
  for the first power-up, then to what the motor needs.
- **Motor:** free (no load) for the first motion.
- **Flashing:** only with the repo's targets (CLAUDE.md): `make -C
  brdstepper flash`, and `make -C brdstepper option_bytes` once.
- **Halting:** each gdb command halts the core for a moment. The timers
  keep running at the last duty, and the DRV8876's own trip limits the
  current. Prefer reading with the drive disabled.

**Step 1 — bare board, before flashing:**
1. Inspection. Measure for shorts between VM, 5V, 3V3 and GND, unpowered.
2. USB only, no VM: 5V ≈ 4.6 V, 3V3 = 3.3 V, below 30 mA.
3. VM only (24 V, 0.3 A limit): 5V_BUCK 5 V, 3V3 3.3 V. The DRV8876s sleep
   until the firmware raises nSLEEP: a few mA from VM.

**Step 2 — MCU, option bytes, firmware:**
1. `make -C brdstepper option_bytes_show`: SWD connects.
2. `make -C brdstepper option_bytes` (BOOT0 pin ignored, DBANK = 1).
3. `make -C brdstepper flash`.
4. `brd` twice, a second apart:
   - `periods` grows by 20 000 a second: PWM, ADC triggers and the frt loop
     run;
   - `hal_state` is not RT_TOO_LONG / FRT_TOO_LONG.
5. The LED is off (disabled).

**Step 3 — clocks and PWM** (scope, no motor; enable input off):
1. A_IN1, A_IN2, B_IN1, B_IN2 all low: disabled means coast.
2. Enable input on. Before the motor is connected the loop sees no current
   and drives the duty limit, so set a small current limit first:
   `hal conf0.max_ac_cur 0.05`.
3. The PWM is 20.0 kHz: the PLL at 170 MHz, ARR 4250.
4. In each pair one input is high and the other pulses low. The low pulses
   of A and B are centred at the same instant: TIM8 in step with TIM1.
5. Enable input off again.

**Step 4 — analog** (motor unpowered, `brd`):

| field | expected |
|---|---|
| `vbus` | the supply ±2 % |
| `ntc` | ~1.65 V at 25 °C (`pin hv 0 hv_temp` ≈ 25) |
| `hwid`, `variant` | 0 V / 0 (smart-serial), 1.65 V / 1 (CAN) |
| IPROPI, all four | below 0.01 A (disabled: no current) |

**Step 5 — encoder:**
1. `enc` while turning the shaft by hand: the angle follows, one turn
   0 … 16383, and the error flag stays 0.
2. `pin mag 0 state` = 3 and `pin mag 0 errors` stays 0.
3. If it jumps or flags errors, check the magnet's gap and centring.

**Step 6 — bridges and current sensing** (motor connected, VM limit 1 A,
enable input on):
1. **Calibration:** at enable, the firmware brakes for 3.2 ms to measure
   the IPROPI offsets (`pin hv 0 sense_off_a`, `sense_off_b`: below
   0.0075). Then phasing (`fb_switch0`, `conf0.phase_cur`) finds the
   encoder's electrical offset. The rotor moves up to half an electrical
   turn: 3.6° (2 full steps).
2. **It holds:** `pin pid 0 pos_error` is a few counts at most (1 count =
   0.38 mrad).
3. **Probe check, coil A:** load it a little by hand and compare a current
   probe against IPROPI_A on a scope. IPROPI_A is 1 V per 0.5 A (2.0 kΩ ×
   1000 µA/A):
   - in the brake window (around the counter's top) it shows the magnitude;
   - in the drive window it shows the same when the current flows with the
     drive, and 0 against it;
   - it must have settled at the sampling instants (the window centres:
     ≥ 2.4 µs into each window).
4. **Windows too short:** if a reading lags or collapses at high duty or
   low duty, raise `hv0.sense_window` and lower `hv0.max_duty` to match.
5. **nFAULT stays high.** Low means a real fault (overcurrent, UVLO,
   overtemperature), not chopping: IMODE is open.

**Step 7 — motion,** with the sine generator (`sim0`, loaded by the pid
template). Never a step command: a jump of more than `conf0.max_pos_error`
(1.5 rad) faults the drive.
1. `hal sim0.amp 0`, then `hal rev0.in sim0.sin`, then `hal sim0.freq 0.5`.
2. `hal sim0.amp 3` ramps up softly (its filter takes ~0.2 s) to ±3 rad at
   0.5 Hz. Raise `sim0.freq` for speed: 2 Hz × 3 rad ≈ 38 rad/s
   (6 rev/s) peak.
3. Check:
   - `pin pid 0 pos_error`, following as in the simulation
     (`host/tests/stepper*.test`: below 1–2 mrad at 10 rev/s);
   - `pin hv 0 abs_cur` within `conf0.max_ac_cur`;
   - nFAULT high.
4. **Detent compensation:** at a slow sine (0.1 Hz), compare
   `hal stp_ttc0.detent 0` with `hal stp_ttc0.detent 0.004`. Without it the
   motor visibly cogs every full step.
5. **Warm-up:** 10 minutes at the rated current. `pin hv 0 hv_temp` and the
   drivers' case temperature stay well below 100 °C.
6. `hal sim0.amp 0`, then the enable input off.

**Record per board** (as `DEBUGNOTES.md` entries):
- vbus accuracy;
- `sense_off_a`, `sense_off_b`;
- the encoder offset after phasing (`pin conf 0 mot_fb_offset` or
  `fb_switch0`'s offset pins);
- anything that did not go as described here.

## 10. Software

- **Firmware:** `brdstepper/` (README there): stmbl's HAL and shared
  components with the board's drivers.
  - `hv`: the two-coil current loop on the DRV8876s, under stmbl's pin
    names.
  - `mag`: the AS5047P.
  - `io`, `adc`: the LED, the variant ID.
- **Simulation:** `host/` runs the same control code (`shared/bip_loop.c`)
  against a NEMA11 model with the DRV8876's sensing.
  - `make -C host check`: `test_bip`, `stepper.test`,
    `stepper_ipropi.test`, `stepper_ipropi_worst.test`.
- **Not done:**
  - the smart-serial port (USART1 + DMA);
  - USB CDC;
  - CAN (protocol to decide; CANopen CiA 402 is the usual choice);
  - the bootloader;
  - IWDG.
