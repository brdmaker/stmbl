# brdstepper: closed-loop stepper drive (hardware plan)

A small closed-loop stepper drive on the back of a NEMA11 motor (28 mm
square, M2.5 holes 23 mm apart). It runs stmbl's control software on an
STM32G4, with a magnetic encoder and two H-bridges.

Branch `brdstepper`, built on `sserial_bootloader`, which brings the
console, scope and update code. **Status:** plan; no schematic or firmware
yet.

There are three variants of one layout:
- **smart-serial** to a Mesa card (LinuxCNC);
- **CAN** (CAN-FD controller);
- **USB** on both, for setup, the console and DFU recovery.

## MCU

All three parts below have the same 48-pin package (UFQFPN-48, 7 × 7 mm).
Every pin of the plan below has the same position and function on all of
them; this was checked against ST's pin data (STM32_open_pin_data,
`STM32G431C(6-8-B)Ux`, `STM32G473C(B-C-E)Ux`, `STM32G474C(B-C-E)Ux`).

| part | flash | RAM | flash banks | fit for stmbl's software |
|---|---|---|---|---|
| STM32G431CBU6 | 128 kB | 32 kB | 1 | tight: everything trimmed, no room for staging |
| **STM32G473CEU6** | 512 kB | 128 kB | 2 | comfortable |
| STM32G474CEU6 | 512 kB | 128 kB | 2 | as G473, plus HRTIM (not needed) |

**Recommendation: STM32G473CEU6.** The reasons:
- **RAM:** the stmbl application on the F4 uses ~80 kB of RAM: HAL 26 kB,
  config 15 kB, scope 16 kB. It fits the G473 with buffers trimmed only a
  little; the G431 needs everything cut to a third.
- **Flash:** the F4 application is ~250 kB, of which ~100 kB is the HV
  board's F3 firmware that brdstepper doesn't need. 512 kB holds the
  application, an update staging area and the config, with room left.
- **Dual-bank flash** (DBANK, 2 × 256 kB) is the decisive point. Code
  runs from one bank while the other is erased or programmed: no CPU
  stall. The single-bank problem in `sserial_bootloader.md` goes away. The
  application can erase and write its staging bank while the drive runs,
  with no `flash_grace` and no "drive must be disabled" rule.

The G431CBU6 remains a drop-in fallback if price matters more than
headroom. It is the same board, with a trimmed build.

**Flash layout, G473CE (dual bank):**

| bank | region | size |
|---|---|---|
| 1 | bootloader | 16 kB |
| 1 | application | 224 kB |
| 1 | config | 16 kB |
| 2 | staging (in-band update) | 256 kB |

## Pin plan (UFQFPN-48)

| pin | function | notes |
|---|---|---|
| PA8, PA9 | TIM1 CH1, CH2 → DRV8876 #1 IN1 / IN2 (coil A) | PWM mode, centre-aligned, ≥ 20 kHz |
| PA10 | TIM1 CH3 → DRV8876 #2 IN1 (coil B) | |
| PC6 | TIM8 CH1 → DRV8876 #2 IN2 (coil B) | TIM1's only CH4 pin is USB D−; TIM8 runs synchronised to TIM1 (same period, started by TIM1's trigger) |
| PA0 | ADC1_IN1 ← IPROPI A | ADC1 and ADC2 sample both coils at the same instant, triggered by TIM1 |
| PA1 | ADC2_IN2 ← IPROPI B | |
| PA2 | ADC1_IN3 ← motor supply | 100k / 10k 1 %: 3.0 V at 33 V |
| PA3 | ADC1_IN4 ← NTC | 10k NTC next to the drivers, 10k pull-up |
| PB0 | ADC1_IN15 ← spare analog input | |
| PB1 | ADC1_IN12 ← hardware version ID | a divider, different per variant (below) |
| PA4 | DAC1_OUT1 → both DRV8876 VREF | the chips' own current trip, set by firmware: a backstop |
| PA5 | nSLEEP (both) | 100k pull-down: bridges off until the firmware runs |
| PA6 | nFAULT (both, wired-or) | 10k pull-up, interrupt |
| PA7 | enable / estop input | disables the bridges whatever the link says |
| PB3, PB4, PB5 | SPI1 SCK / MISO / MOSI → AS5047P | |
| PA15 | SPI1 CS → AS5047P | |
| PA11, PA12 | USB D− / D+ | internal D+ pull-up; 48 MHz from HSI48 with CRS |
| PB6, PB7 | USART1 TX / RX → RS-422 | smart-serial variant, 2.5 Mbaud |
| PB8, PB9 | FDCAN1 RX / TX → CAN transceiver | CAN variant; see BOOT0 below |
| PB13 | CAN transceiver standby | CAN variant |
| PB12 | status LED | |
| PF0, PF1 | 8 MHz crystal | for the 2.5 Mbaud link and CAN bit timing |
| PA13, PA14 | SWD | Tag-Connect TC2030 pads |
| PG10 | NRST | 100 nF |
| spare | PB2, PB10, PB11, PB14, PB15, PC4, PC10, PC11, PC13 | step/dir input, second encoder, index, a second UART (USART3 on PB10/11 or PC10/11) |

**BOOT0 / PB8:** PB8 is the only FDCAN1 RX pin left once USB has
PA11/PA12, and it doubles as BOOT0. A CAN transceiver holds RX high while
the bus is idle, which would start ST's ROM bootloader at every power-up.
The flash target therefore sets the option bytes once, on every variant:
`nSWBOOT0 = 0`, `nBOOT0 = 1`, so the pin is ignored. ST's ROM DFU stays
reachable from the firmware: a RAM trigger word and a jump, as stmbl does.

## Variants

There is one PCB. The interface section is chosen at assembly; the
firmware reads the ID divider on PB1 at boot and starts the matching link.

| | smart-serial | CAN |
|---|---|---|
| transceiver | 3.3 V full-duplex RS-422, ≥ 10 Mbps, e.g. SN65HVD30 ✱ | TCAN1044V ✱ (5 V bus side, 3.3 V logic on VIO) |
| connector | JST-GH 6-pin: TX±, RX±, +5 V, GND; adapter cable to the Mesa's RJ45 | 2 × JST-GH 4-pin: CANH, CANL, V+, GND (daisy chain) |
| termination | 120 Ω across RX | 120 Ω behind a solder jumper (bus ends only) |
| logic power | Mesa port 5 V, or the buck | the buck (V+ on the CAN cable) |
| ID (PB1) | 10k / 0 Ω → 0 V | 10k / 10k → 1.65 V |

USB is on both: USB-C on the edge if the housing allows, otherwise a
4-pin JST-SH (D+, D−, 5 V, GND) with a pigtail. USB alone powers the logic,
so setup works without motor power. The same console, `servoterm-cli`,
`--capture` and `--update` run over it, and ST's ROM DFU too.

✱: check the footprint and speed against the board before ordering.

## Parts list

**Core (both variants):**

| ref | qty | part | package | notes |
|---|---|---|---|---|
| U1 | 1 | STM32G473CEU6 (fallback STM32G431CBU6) | UFQFPN-48 | |
| U2, U3 | 2 | DRV8876RGTR | VQFN-16 | one H-bridge per coil, 3.5 A peak, IPROPI current output |
| U4 | 1 | AS5047P-ATSM | TSSOP-14 | 14-bit magnetic encoder, SPI, at 3.3 V |
| U5 | 1 | 3.3 V LDO ≥ 150 mA, e.g. TLV75533PDBVR | SOT-23-5 | from 5 V (Mesa, USB or the buck) |
| U6 | 1 | buck 36 V → 5 V, e.g. TPS560430 ✱ | SOT-23-6 | from the motor supply |
| U7 | 1 | USBLC6-2SC6 | SOT-23-6 | USB ESD |
| D1 | 1 | BAT54C (dual Schottky) | SOT-23 | 5 V OR: USB / Mesa or buck |
| D2 | 1 | TVS, e.g. SMAJ24A ✱ | SMA | for 24 V supplies; DRV8876 abs. max 40 V |
| Y1 | 1 | 8 MHz crystal + 2 × ~10 pF C0G | 2016 | |
| J1 | 1 | USB-C 2.0 receptacle (e.g. GCT USB4105) or JST-SH 4-pin | | + 2 × 5.1k on CC1 / CC2 |
| J2 | 1 | JST-XH 2–3 pin | | motor supply (+ enable) |
| J3 | 1 | JST-PH 4-pin or pads | | motor coils A / B |
| LED1 | 1 | LED 0603 + 1k | | |
| M1 | 1 | diametric magnet 6 × 2.5 mm NdFeB | | rear shaft of a double-shaft motor (or glued) |

**Per DRV8876 (× 2):** 10 µF 50 V X7R 1206 and 100 nF 50 V on VM; 22 nF
50 V CPH–CPL; 100 nF 16 V VCP–VM; an IPROPI resistor (4.7k ≈ 1.5 A full
scale, 3.6k ≈ 2 A; pick for the motor) with 1 nF; PMODE / IMODE straps for
PWM mode (check the datasheet's strap table).

**MCU and measurements:**
- **MCU decoupling:** 100 nF per VDD pin plus 4.7 µF.
- **VDDA / VREF+:** a ferrite bead, 1 µF and 10 nF.
- **Reset and boot:** 100 nF on NRST, 10k pull-down on BOOT0.
- **Driver control:** 100 nF on the DAC output; nSLEEP 100k pull-down,
  nFAULT 10k pull-up.
- **Supply measurement:** 100k / 10k 1 % divider with 10 nF.
- **Temperature:** 10k NTC (B 3380) with a 10k 1 % pull-up.
- **Inputs:** spare analog 1k + 10 nF; enable 10k series + 100k pull-down
  + 10 nF.
- **Hardware version ID:** the divider on PB1 (see Variants).
- **Bulk:** 1–2 × 22 µF 50 V X7R 1210 (or a 47–100 µF polymer) on VM.

**Smart-serial variant:** the RS-422 transceiver with 100 nF, 120 Ω across
RX, JST-GH 6-pin (SM06B-GHS-TB).

**CAN variant:** TCAN1044V with 100 nF on VCC and VIO, 120 Ω with a solder
jumper, 2 × JST-GH 4-pin (SM04B-GHS-TB).

## Software: reuse stmbl, as a new target in this repo

stmbl is already split in two. `shared/` is portable: the HAL, the
components, commands and config, the console link (`sscon`), the
smart-serial LBP parser (`lbp_dev`), update staging (`fwstage`) and the
scope. Each board has its own directory with its startup, linker script
and drivers: `src/` (F4 main board), `stm32f303/` (HV board),
`bootloader/`, `f3_boot/`, and `host/` (the x86 simulator).

brdstepper becomes another such target, not a new project:

```
brdstepper/
  Makefile            make -C brdstepper (or `make brdstepper` at the root)
  g473.ld             flash layout above
  src/main.c          clocks, HAL timing: TIM1 → ADC → rt / frt
  src/hw/             TIM1/TIM8 PWM, ADC, DAC, SPI (AS5047), USART (smart-serial), FDCAN, USB CDC, flash
  comps/              brdstepper's hardware components: the coils' current
                      loop and PWM (as hv), the AS5047 feedback (as enc_fb),
                      the smart-serial field table (as sserial), CAN
  conf/               templates
  boot/               bootloader: the smart-serial loader (shared/lbp_dev.c),
                      CAN and USB, applies staged updates
```

**Reused unchanged** from `shared/`:
- the HAL and the commands;
- the config and templates;
- the control components: `pid`, `vel`, `rev`, `fault`, `pe`, `scope`,
  `fwup`, `term`, `sim`, ...;
- `sscon`, `lbp_dev`, `lbp_flash`, `fwstage`;
- the tools: `servoterm-cli`, `sserialterm`, `setsserial`, `mkssfw`.

**New:**
- **Drivers:** written against CMSIS registers (no vendor HAL, no
  Arduino), as `src/` does.
- **Two-phase current control:** a component for two independent coils
  instead of three-phase PMSM: two PI loops, with the commutation angle
  from the encoder (the stepper is a 50-pole-pair PMSM).
- **The smart-serial field table:** for brdstepper, generated by
  `tools/gentable.c`.
- **The CAN protocol:** to decide; CANopen CiA 402 is the usual choice.

**Testing:** the host simulator (`host/`) runs the same `shared/` code. A
stepper plant model (two coils, the 50 pole pairs, detent torque) lets the
current and position loops be developed and tested on the PC before any
board exists. The hardware drivers are the only part that needs the board.

**The one change to `shared/`:** HAL table sizes and buffer sizes become
per-target defines, as `SSCON_TXSIZE` already is: `HAL_MAX_PINS`,
`HAL_MAX_CTX`, the config buffer, `SCOPE_MEM`. The F4's values stay the
default. This matters mostly for the G431 fallback.

## Next steps

1. Confirm the MCU (G473CEU6) and the variants above.
2. Schematic, then layout, around the 28 mm outline.
3. Meanwhile, in software: the per-target size defines in `shared/`, the
   stepper plant in the simulator, the two-phase current loop component
   and its host tests, then the `brdstepper/` target skeleton (clocks,
   PWM, ADC, SPI, USART) building in CI.
