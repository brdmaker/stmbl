# stman: stmbl with an analog output

**stman** is a stmbl without the power stage. In its place is a precise,
floating ±10 V output that drives an existing analog servo or spindle
drive. One stman sits next to each drive, in a small enclosure, and
replaces the long analog cable from the control to the drive:

- **Control side:** smart-serial (RS-485) to the Mesa card. This is the
  module's only connection to the control, and it is isolated.
- **Drive side:** the command (±10 V), the drive's encoder output, its
  24 V enable and status signals, and its analog monitor outputs. Nothing
  else: machine sensors and the drive's service port are out of scope.

The first target is the Gildemeister CTX twin. Its Indramat drives take
±10 V velocity commands and give back encoder signals:
- **feed axes:** DDS 2.1 with the DAE 1.1 analog interface;
- **spindles:** KDA 3.2.

The drive documentation is in
[ctxtwin/electrical/](ctxtwin/electrical/README.md). Any drive with a ±10 V
command input and an encoder output fits the same way.

**Why:**
- **No analog runs through the cabinet.** The ±10 V line is centimetres
  long and stays on the drive's own reference; the long link is digital and
  isolated. This removes the hum seen with a central analog card.
- **The position loop at 10-20 kHz in the module.** stmbl already
  interpolates LinuxCNC's position stream and adds acceleration
  feed-forward (`docs/motion_control.md`), so the command is smooth, not a
  1 kHz staircase.
- **The drive's data in the console:** motor current and speed from its
  monitor outputs, its ready and temperature signals, and faults.
- **The stmbl tooling unchanged:** servoterm, firmware version checks,
  config patches, the setup wizard and the host simulator.

## The board

stman is the stmbl v4.0 LV board, kept as it is where possible:
- **Kept:** STM32F405, its clock, USB and SWD; the supply bucks and LDO;
  FB0 and FB1 exactly as on stmbl (8P8C, same pinout, transceivers,
  sin/cos front end, 5 V / 12 V port supply); the status LEDs.
- **Removed:** the link to the HV board (header P14) and the HV board itself;
  the CMD port's two spare RS-485 channels; the 24 V output transistors, fan
  output, 2x6 expansion header and analog inputs of the IO sheet.
- **Added:** the command DAC, the drive-side I/O, the monitor ADC, the
  watchdog, the ±15 V supply.

The schematic is in [hardware/stman](../hardware/stman/README.md): stmbl
v4.0's sheets converted to KiCad 7 with their drawing kept, edited in place,
plus three new sheets (dac, ain, drvio).

```
                     control side ┊ drive side (board ground = the drive's 0VI)
                                  ┊
 Mesa sserial ── RJ45 ── 2x SN65176B ── ISO7331FC ─┊─ STM32F405 ── SPI ── AD5761R ── filter ── AO1 ── drive E1/E2
 (+5 V from the                   ┊     │                (AD5761R ── filter ── AO2, spindle C axis)
  sserial cable powers            ┊     ├── AD7606 ── AI: monitor outputs (AK1/AK2, N/M), readbacks
  the bus side)                   ┊     ├── FB0 / FB1 (stmbl) ── drive encoder out, HGV, linear scale
                                  ┊     ├── 2x ITS724G ── 8 x 24 V out ── RF, AH, RUN, POS, PAR ...
                                  ┊     ├── 8 x 24 V in ── READY, TVW, INPOS, Bb contact ...
                                  ┊     └── MAX6369 watchdog ── DAC CLEAR, RF / RUN enable gate
                                  ┊   +UI 24 V ── bucks: 5 V, 3.3 V, 12 V, port 5 V ── NMA1215: ±15 V
```

### Ground plan: one isolation barrier

**Supply.** The module runs from the drive's own control supply: **+UI / 0VI** (24 V)
on the DDS (X13/8-9), or the KDA's external 24 V (X2/19, X2/1, with its
internal links X2/2 and X2/18 removed). The board's ground is 0VI.

**The Indramat interface already separates the control side from the
drive's electronics:**
- the 24 V logic (RF, AH, Ired, TVW) runs on +UI/0VI;
- the encoder output stage on X14 is supplied by the control (+5 V ext and
  0 V ext, pins 12/10);
- the command input E1/E2 is differential.

A module on 0VI therefore sits exactly where Indramat expected the control
to be.

**The only connection back to the control** is smart-serial. stmbl's
two RS-485 transceivers for it (SN65176B, U8 receiving on pair 1/2, U9
sending on pair 3/6) stay where they are on the CMD sheet. They are now on the
cable side: they are powered by the cable's +5 V and GND (SS_5V, SS_GND), and
an ISO7331FC (U200, 3 channels, outputs low while unpowered) is their only
path to the MCU. Nothing switches across the barrier, so only a few pF of
isolator capacitance couple the two sides, and no ground loop can form through
the module.

The CMD port's other two pairs (4/5, 7/8; stmbl's U10, U11) are removed.
Mesa's cables carry +5 V and GND on them; solder jumpers JP200/JP201 choose
which pair is which (check against the Mesa card's smart-serial pinout).

**Rules for the build:**
- **Measure before connecting.** With the drive unpowered, measure the
  resistance between 0VI and 0VM, between X14/10 (0 V ext) and 0VM, and from
  each to PE. The manuals name these supplies separately but never say
  "isolated".
- **Never tie 0VM to board ground.** The monitor outputs (AK1/AK2, N, M) are
  referenced to 0VM. The AD7606 measures both the signal and 0VM, each
  through its 1 MΩ inputs, and the firmware subtracts. That keeps the input
  differential and high-impedance.
- **AO−** goes to E2 through 10 Ω, a fitted link that can be removed.
- **Shields:** grounded at the drive end, on the connector housing.
- **USB** is for flashing and setup only. A laptop on USB ties its ground to
  0VI.

### Command outputs: AD5761R

**DAC:** AD5761RBRUZ (16-bit, ±10 V range, internal 2.5 V reference
at 2 ppm/°C, SPI), one per output. AO1 is always fitted. AO2 (second DAC
and filter) is fitted only for the spindle's second input (KDA X4/5-6, C
axis).

**Resolution:**
- 305 µV per step.
- On a DDS at 2000 rpm per 10 V: 0.06 rpm per step, about 0.01 mm/s at the
  slide.
- On the KDA at 6300 rpm per 10 V: 0.19 rpm per step.

**Filter:** a 2nd-order Sallen-Key low-pass after each DAC.
- **Parts:** OPA2197 (36 V precision), 4.7 kΩ / 4.7 kΩ, 22 nF / 10 nF.
- **Response:** about 2.3 kHz corner, Q about 0.74.
- **Purpose:** it smooths the DAC's updates at the loop rate without
  delaying the position loop, which has a bandwidth of a few Hz.

**Output stage:**
- **Series resistor:** 100 Ω.
- **TVS:** SMAJ15CA to ground, for cabinet transients.
- **Load:** the drive's input impedance is about 10-20 kΩ, so the 100 Ω
  causes at most 1 % gain error. The firmware's scale calibration takes it out.

**Supply:**
- **±15 V:** from an NMA1215 DC/DC fed by the board's 12 V. Its output
  common is tied to ground; its isolation is unused, but it gives clean
  ±15 V from one part. LC-filtered.
- **DAC logic:** 3.3 V (DVCC and VLOGIC).

**Fault behaviour:**
- **Power-up:** the AD5761R's output is clamped until its range is
  configured; the firmware then writes the range with the output at 0 V.
- **Watchdog:** MAX6369, kicked by the firmware every loop cycle. If the kicks
  stop, its output drives the DACs' CLEAR inputs low (output to 0 V, no SPI
  needed). Through a 74LVC2G08 it also drops OUT1 and OUT2, which carry RF
  and AH (or RUN): the drive brakes at full torque on its own.
- **Firmware:** on smart-serial loss, the firmware sets 0 V and drops RF
  itself.
- **The hardwired E-stop chain stays.** The drive's Bb contact and the
  supply's Bb1 stay in the cabinet's chain, as today.

### Feedback: FB0 and FB1, as on stmbl

The two 8P8C feedback ports are stmbl's, unchanged:
- **FB0:** quadrature with index, sin/cos (ADC interpolation), resolver,
  and the UART absolute encoders (EnDat, smartabs, Mitsubishi, Sanyo,
  Yaskawa).
- **FB1:** quadrature (and sin/cos).

The firmware components (`enc_fb0`, `res_fb0`, `endat`, ...) and their
configuration work as on any stmbl.

| module | FB0 | FB1 |
|---|---|---|
| servo | drive encoder output (DDS X14) | — |
| servo + linear scale | the scale (TTL, or 1 Vpp via sin/cos) | drive encoder output |
| spindle | HGV 1.1 fine output (C axis) | HGV coarse output or KDA X6 |

**Port supply:** the drive's encoder output (X14/12), the HGV and a scale
take their 5 V from the port, through its 5 V / 12 V jumper (JP2, JP5) and
a polyfuse (F200 FB0, F201 FB1; 0.5 A hold) between the port supply and the
connector's pin 7. The port supply (+5F) is stmbl's 1.5 A buck, switched by
the MCU (EN_5V).

**Adapter cables:** 8P8C to the 15-pin D-sub of the DDS X14 and to the
HGV / KDA X6. The shield lands on the D-sub housing.

### Drive I/O

| | servo (DDS 2.1, DAE 1.1) | spindle (KDA 3.2) |
|---|---|---|
| AO1 ± | X13/1 E1, X13/2 E2 | X4/1 E1, X4/2 E2 |
| AO2 ± | — | X4/5 E3, X4/6 E4 (C axis) |
| OUT1 (watchdog-gated) | RF X13/4 | RF X2/24 |
| OUT2 (watchdog-gated) | AH X13/5 | RUN X2/25 |
| OUT3 | Ired1 X13/6 | POS1 X2/26 |
| OUT4 | Ired2 X13/7 | POS2 X2/27 |
| OUT5 | — | PAR1 X2/30 |
| OUT6 | — | PAR2 X2/31 |
| OUT7 | — | EXT POS X2/37 |
| OUT8 | — | MD-RED X2/28 |
| IN1 | Bb contact X3/6-7 (via +24 V on J8) | READY X2/4 |
| IN2 | TVW X13/3 | TEMP WARN X2/9 |
| IN3 | ASQ X3/10-11 (optional) | INPOS X2/7 |
| IN4 | — | N=CMD X2/5 |
| IN5 | — | N<MIN X2/6 |
| IN6 | — | Bb X11/1-2 (via +24 V on J8) |
| IN7, IN8 | spare | spare |
| AI1 | AK1 X3/1 (speed) | N X2/21 |
| AI2 | AK2 X3/3 (current, 10 V = rated) | M X2/20 |
| AI 0VM | X3/2 | X2/22 |

**Outputs:**
- **Switches:** two ITS724G quad high-side switches (industrial 24 V,
  current limit, thermal shutdown, status per pair to the MCU). They source
  +UI into the drive's inputs.
- **Polarity:** active high, which is what the Indramat inputs need.

**Inputs:**
- **Circuit:** a divider (47 kΩ / 10 kΩ), 1 nF and a BAT54S clamp to
  3.3 V per input. 24 V gives 4.2 V before the clamp, so the input reads a
  clean high. It reads high above about 13 V and low below about 6 V
  (Indramat logic 1 is 12-32 V).
- **Potential-free contacts (Bb, ASQ):** wired between J8's +24 V terminal
  and an input.

**Monitor ADC:** AD7606, 8 channels, simultaneous sampling, 16-bit,
±10 V inputs at 1 MΩ, one 5 V supply, serial interface on SPI1. It is
started each loop cycle (CONVST from the MCU).

| channel | measures |
|---|---|
| V1 | AI1 (AK1 / N) |
| V2 | AI2 (AK2 / M) |
| V3 | AI 0VM: the drive's measurement ground, subtracted from V1 and V2 |
| V4 | AO1 at the connector (readback) |
| V5 | AO2 at the connector (readback) |
| V6 | +15 V / 2 |
| V7 | −15 V / 2 |
| V8 | +24 V / 4 (+UI) |

### Power tree

| rail | from | for |
|---|---|---|
| +24V | P3 (+UI / 0VI), reverse-polarity diode D200 (SS34A), SMAJ24A, 22 µF | everything; ITS724G VBB; the inputs' contact supply |
| +5V | ACT4088 (stmbl U13) | LDO, AD7606 AVCC |
| +3V3 | zldo1117 (stmbl U2) | MCU, DAC logic, AD7606 VDRIVE, ISO7331FC side 1 |
| +12V | ACT4088 (stmbl U27) | NMA1215, FB port option |
| +5F | ACT4088 (stmbl U28), EN_5V | FB0/FB1 port supply |
| +15VA / −15VA | NMA1215 + LC | AD5761R AVDD/AVSS, OPA2197 |
| SS_5V / SS_GND | the smart-serial cable (RJ45 4/5 and 7/8, by JP200/JP201) | U8, U9 and ISO7331FC side 2 only |

### Connectors

| ref | what | pins |
|---|---|---|
| J3 | smart-serial (8P8C, stmbl's CMD) | 1/2 RX pair, 3/6 TX pair, 4/5 and 7/8 the cable's +5 V and GND (JP200/JP201) |
| J1, J2 | FB1, FB0 (8P8C, stmbl) | as stmbl |
| P3 | supply (stmbl's 3.5 mm) | +UI, 0VI |
| J5 | analog out (3.5 mm, 6-pin) | AO1+, AO1−, shield, AO2+, AO2−, shield |
| J6 | analog in (3.5 mm, 4-pin) | AI1, AI2, AI 0VM, shield |
| J7 | 24 V out (3.5 mm, 10-pin) | OUT1-8, 0V, 0V |
| J8 | 24 V in (3.5 mm, 10-pin) | IN1-8, +24V (contact supply), +24V |
| P1 | USB (stmbl) | setup and flashing |

### MCU pins (STM32F405VG, LQFP-100)

These are stmbl's pins, plus the ones freed by the removed CMD channels,
the HV link, the IO sheet and pins stmbl tied to ground.

| function | pin |
|---|---|
| smart-serial RX (USART1_RX) | PA10 (69) |
| smart-serial TX (UART4_TX) | PA0 (23) |
| smart-serial TX enable | PB7 (93) |
| SPI1 SCK / MISO / MOSI | PB3 (89) / PB4 (90) / PB5 (91) |
| DAC1 /SYNC, DAC2 /SYNC | PB6 (92), PB9 (96) |
| DAC /RESET, /ALERT | PD7 (88), PA15 (77) |
| ADC /CS, CONVST, BUSY, RESET | PB2 (37), PB8 (95), PA8 (67), PA9 (68) |
| watchdog WDI, WDO readback | PD6 (87), PE2 (1) |
| OUT1-OUT8 | PE3, PE4, PE5, PE6, PE0, PE1, PA2, PA3 |
| ITS724G status (U50 ST1/2, ST3/4; U51 ST1/2, ST3/4) | PD0, PD1, PC14, PC15 |
| IN1-IN8 | PC4, PC5, PB0, PE15, PC9, PD2, PC0, PC1 |
| LEDs, FB0/FB1, USB, SWD, 24 V / 5 V monitors, EN_5V | as stmbl |

## Firmware

A new stmbl target `stman` reuses:
- the HAL core, `sserial`, the feedback components and the
  motion-control path (command interpolation, feed-forward, position loop);
- the console, firmware versions and config patches.

**New components:**
- **`dac`:** the AD5761R output. Scale in V per unit, offset trim,
  limits. At 0 when disabled, faulted or the link is lost.
- **`ain`:** the AD7606. Converts every loop cycle, subtracts 0VM, and
  gives scaled monitor values (motor current, speed) and the readback
  check.
- **`dio`:** OUT1-8 and IN1-8, with the role names of the I/O table (RF,
  AH, READY, ...), and the ITS724G status.
- **`wdog`:** kicks the MAX6369 while all is well, and stops kicking on a
  fault.

**Loop modes:**
- **mode 0, remote DAC:** LinuxCNC closes the loop and sends a velocity
  command. stman outputs it and returns the encoder position, like a
  central analog card but on the drive.
- **mode 1, position (default):** LinuxCNC sends position, stman
  interpolates and closes the loop at 10-20 kHz (P with velocity
  feed-forward; the drive closes velocity). This is the reason for the
  module.
- **mode 2, dual loop:** position from FB0 (scale), velocity damping from
  FB1 (motor encoder).

**Smart-serial data** (to LinuxCNC):
- **Out:** position and velocity command, enable, outputs.
- **In:** position from FB0 and FB1, motor current (AI2), speed monitor
  (AI1), inputs, fault and status words.

**Faults:** smart-serial loss, drive not ready (IN1/Bb), DAC readback
mismatch, ITS724G overload, ±15 V or 24 V out of range, following error.
Each one sets the DAC to 0, drops OUT1/OUT2 and reports the fault.

## Simulator

`host/` gets an Indramat-like velocity drive model:
- a velocity loop and current limit;
- the encoder emulation with a set line count;
- the drive's input offset and noise;
- the AD5761R's 16-bit steps and the output filter.

The whole module, with its loop at 10-20 kHz against LinuxCNC, can then be
tested before the board exists.

## Open points

- Measure 0VI / 0VM / 0 V ext separation on a real DDS and KDA.
- Check the AD5761R pin numbers against the datasheet (the schematic's
  symbol is drawn from the pin names).
- Check Mesa's smart-serial RJ45 pinout for the supply pairs (JP200/JP201).
- Check the AD7606 serial mode: DOUTA tri-state with /CS high, so it can
  share MISO with the DACs.
- Read the line counts and volts-per-rpm from each drive (keypad on the
  KDA, setup program on the DDS) before the first run.
- Enclosure: DIN-rail case near the drive, with the 8P8C ports and the
  3.5 mm terminals on one face.
