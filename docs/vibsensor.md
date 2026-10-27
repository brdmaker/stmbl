# Vibration and temperature sensor for the machines

A small CAN node with stmbl's firmware framework: an STM32G4, an IIS3DWB
accelerometer and a temperature sensor. It sits on the machine's structure
and reports what the machine does as it moves and cuts. Its uses:

- **Tuning, mainly:** vibration against spindle speed, feed and axis
  motion. It finds:
  - unbalance and resonances on a spindle run-up;
  - the ball screw and the guides on a feed sweep;
  - chatter while cutting, at its own frequency, so a stable speed can be
    chosen.
- **Alarms:** a crash is caught within a sample (37 µs). There are also
  vibration and temperature levels.

**Where the sensors go:** four per machine, each its own node.

| node | where | what it sees |
|---|---|---|
| 1 | base (bed) | the floor and the machine's own rocking, the reference for the others |
| 2 | X axis (saddle or table) | X ball screw and guides, the X servo's ringing |
| 3 | Y axis | the same for Y |
| 4 | spindle housing, at the front bearing | unbalance, bearings, tool passing, chatter, crashes |

On the spindle, x and y are radial and z is axial.

**Status:**
- **Signal processing:** written (`shared/vibdsp.c`, `shared/comps/vib.c`)
  and tested in the host simulator against a simulated sensor on a machine
  (`host/hw/vibsim.c`, `host/tests/vib/`).
- **Bus:** frames (`shared/vibproto.h`), node numbering
  (`shared/vibaddr.c`, `tools/vibmon/vibaddr.py`) and the sensor's side
  (`shared/comps/vnode.c`), all tested in the host.
- **Firmware:** `vibsensor/` (G473: FDCAN, IIS3DWB, TMP117, console on the
  bus) builds; not yet run on a board.
- **PC:** `tools/vibmon/` (numbering, machine state, frames, trends, crash
  latch, watchdog, HAL pins, a JSON socket), tested against simulated
  sensors running the sensor's own code.
- **millweb:** the Measure page (`[MILLWEB] VIBMON`; webui/mill/README.md):
  cards, trends, spectra with the spindle's orders marked, alarms, naming
  the sensors by tapping them, a console; millweb sends vibmon the machine
  state.
- **Board:** a KiCad project, [hardware/vibsensor](../hardware/vibsensor/README.md):
  the schematic, checked against its part list, and a 32 × 18 mm board,
  placed, not routed. The enclosure concept "Shard" (faceted, bolted flat
  by two M4 lugs or onto an M6 stud, two M8 sockets IN / OUT on the tail):
  [hardware/vibsensor/enclosure](../hardware/vibsensor/enclosure/README.md).
- **Not yet:** routing, and a board on the bench. See [What's next](#whats-next).
- **✱ marks values to check against a datasheet.**

## 1. Hardware

| part | choice | why |
|---|---|---|
| MCU | STM32G473CEU6 (as brdstepper) | FDCAN (CAN FD), 170 MHz with FPU, 128 KB RAM: a 4096-point FFT and its buffers take ~70 KB. An STM32G431 (32 KB RAM) would do up to 2048 points. |
| accelerometer | IIS3DWB | 3 axes, 26.667 kHz output rate, flat to ~6.3 kHz ✱, ±2/4/8/16 g, 16 bit, 3 KB FIFO (~19 ms of x y z), SPI up to 10 MHz, −40…105 °C. |
| temperature | TMP117 (I²C, ±0.1 °C) on the mounting face | The IIS3DWB's own temperature is its die's, not the machine's. |
| CAN | TCAN1044V (CAN FD, as brdstepper), no termination on the board | A separate measurement bus (§3). |
| connectors | two M8 A-coded 4-pin sockets wired 1:1, IN male and OUT female: 1 V+ (brown), 2 CAN_H (white), 3 GND (blue), 4 CAN_L (black) ✱ | Small, enough for a 24 V supply and the bus (M8 4-pin: 4 A, 60 V ✱). M8 has no CiA pinout: this one keeps the supply on the opposite pins 1 and 3 and the pair on 2 and 4. There is no shield pin; the shield goes to the metal coupling nut and housing. The sensors daisy-chain on standard M8 4-pin cables (female to male, 4-wire twisted pair, CAN_H/CAN_L one pair): the bus goes through each sensor, so there are no stubs and no T-pieces. The last sensor's OUT takes a terminator plug: an M8 male plug with 120 Ω between pins 2 and 4 (field-wireable and potted, unless a stock one with this pinout is found ✱). |
| power | 24 V from the bus, a buck to 3.3 V, an LDO or filter for the IIS3DWB | — |
| mechanics | the "Shard" housing (hardware/vibsensor/enclosure): milled aluminium base with a solid floor, two M4 lugs (or an M6 stud), the board on two bosses over the floor, potted, a faceted cover with a light pipe; the two M8 sockets on the tail's angled faces | The sensor's path to the machine is short and stiff: board, bosses, potting, solid floor. The housing must resonate well above 6.3 kHz ✱ (to check). A magnet mount is fine for surveys, but caps the usable band at about 2 kHz ✱. |

**On the MCU:**
- **SPI1:** the IIS3DWB with DMA. Its INT1, the FIFO watermark, starts the
  read.
- **I²C:** the TMP117.
- **FDCAN1:** the bus.
- **SWD and USB:** for development.
- **Node number:** set over the bus and kept in flash (§3.1). The sensor
  is potted: there are no jumpers or switches.

## 2. Processing

The sensor's samples (x y z, 26.7 kHz) go through `vibbuf` into the `vib`
component.

**In rt (5 kHz), per sample:**
- the filters;
- the running sums;
- the crash test;
- filling the FFT blocks.

About 300 cycles per sample, ~5 % of the G4 ✱.

**In nrt:** the FFTs (a 4096-point one is ~2.5 ms on the G4 ✱).

```
x y z ──hp (2 Hz)──┬── rms, peak, crest, kurtosis per axis ─────────── every interval
                   ├── ∫ → hp 10 Hz → lp 1 kHz → velocity rms (mm/s) ─ (ISO 10816 / 20816)
                   ├── |a| ≥ shock_lim → crash (latched) ───────────── at once
                   └── fft_axis ─┬── Hann, FFT (fft_n) → peak, orders, tooth passing, chatter
                                 └── bp env_lo..env_hi → rms; |.| → lp → ÷8 → FFT → env_f
```

**Outputs** (HAL pins of `vib0`):

| pin | unit | what | finds |
|---|---|---|---|
| `rms_x/y/z` | g | acceleration RMS | overall level |
| `peak_*`, `crest_*` | g, – | largest \|a\|, peak / RMS | impacts |
| `kurt_*` | – | kurtosis: 1.5 sine, 3 noise, more = spiky | bearing damage, chipping, loose parts |
| `vel_x/y/z` | mm/s | velocity RMS 10…1000 Hz | ISO severity, unbalance, misalignment |
| `fft_f`, `fft_a` | Hz, g | strongest spectral peak | resonances |
| `ord1`, `ord2`, `ord3` | g | 1×, 2×, 3× spindle speed | unbalance, misalignment, looseness |
| `tpf` | g | teeth × rpm | cutting load per tooth, runout |
| `chat_f`, `chat_a` | Hz, g | strongest peak in `chat_lo..chat_hi` that is no spindle harmonic (±4 bins) | chatter |
| `env`, `env_f` | g, Hz | bearing band RMS; the repetition rate in its envelope | bearing defects (BPFO, BPFI, BSF) |
| `crash`, `shock` | –, g | latched over `shock_lim`; the largest \|a\| since | crashes |
| `taps` | count | knocks over `tap_g`, 0.3 s apart | naming a sensor (§3.1) |
| `level` | 0/1/2 | largest `vel_*` against `vel_warn` / `vel_alarm`; judged after the filters' first 0.5 s, falls back only 10 % below a threshold | severity alarm |
| `temp_level` | 0/1/2 | `temp` against `temp_warn` / `temp_alarm`, 2 °C of hysteresis | temperature alarm |
| `reports`, `ffts`, `lost` | count | intervals, FFT blocks, blocks or samples lost | health |

**Parameters** (pins, so set from the config, or the console over the bus):

| pin | default | |
|---|---|---|
| `hp` | 2 Hz | acceleration high pass (gravity, drift) |
| `interval` | 0.1 s | reporting interval of the per-axis values |
| `vel_hp`, `vel_lp` | 10, 1000 Hz | velocity band (ISO: 10…1000 Hz; 2…1000 Hz for slow machines) |
| `fft_n` | 2048 | 256…4096, a power of two (13 Hz or 6.5 Hz bins) |
| `fft_axis` | 0 | the axis the spectrum and the envelope look at |
| `rpm`, `teeth` | from the PC | the machine state frame (§3) |
| `chat_lo`, `chat_hi` | 200, 5000 Hz | where chatter is looked for |
| `env_lo`, `env_hi`, `env_lp` | 1000, 6000, 500 Hz | the bearing band and the envelope's smoothing |
| `shock_lim` | 8 g | crash threshold; `reset` = 1 clears the latch |
| `vel_warn`, `vel_alarm` | 4.5, 11.2 mm/s | from ISO 10816; better: from the machine's own baseline |
| `temp_warn`, `temp_alarm` | 60, 80 °C | — |

**Numbers** (what the simulator tests check):

| case | result |
|---|---|
| 1 g unbalance at 100 Hz | 0.707 g RMS, 11.04 mm/s, crest 1.41, kurtosis 1.50, `ord1` 0.707 g |
| 0.4 g chatter at 1730 Hz | found at 1730 ± 3 Hz with 0.283 g, next to 400 Hz tooth passing and 200 Hz unbalance; without chatter only noise remains |
| bearing defect, 87 impacts/s ringing at 3 kHz | kurtosis from 3 to over 5, `env_f` 87 ± 1.5 Hz |
| 20 g impact | `crash` within the sample; 8 g running does not trip it |

## 3. The measurement bus

**Why a separate bus:** the drives' CAN carries their servo loop every
period, and the measurement traffic is large and bursty. A bus of its own
keeps both simple.

**Bus settings:**
- **Bit rates:** CAN FD, 1 Mbit/s arbitration, 5 Mbit/s data.
- **Topology:** a daisy chain. The PC's adapter and the 24 V feed at one
  end, then base, Y, X and spindle sensors, each cable from one sensor's
  OUT to the next one's IN. Supply and bus pass through every sensor (the
  last one carries the sum of the currents, well under the M8's 4 A).
- **Termination:** 120 Ω at both ends: the PC's adapter at its end, the
  terminator plug in the last sensor's OUT. Without the plug the bus
  may work on short lines, but the 5 Mbit/s data phase rings and frames
  get lost.

### 3.1 Node numbers: found and given over the bus

Every sensor is the same, and potted. It is told apart by its MCU's 96-bit
unique ID (`shared/vibaddr.h`, `shared/vibaddr.c` on the sensor,
`tools/vibmon/vibaddr.py` on the PC).

**Discovery:** the PC searches the IDs bit by bit, like CANopen LSS
Fastscan. It asks "anyone whose ID begins with these n bits?" and every
sensor that matches answers with the same frame, so the answers can overlap
on CAN. 97 questions find one ID; that sensor is muted and the search
repeats. At 3 ms a question, that is 0.3 s per sensor.

**A new sensor:** one without a number (node 0) sends nothing but these
answers, so it never collides with the measurement frames.

**Automatic assignment** (`Addresser.auto`) against the machine's list,
`sensors.json` (uid, node, name, position), run when `vibmon` starts:

| found | done |
|---|---|
| a known sensor on its number | nothing |
| a known sensor on another number, or none (reflashed, reset) | its number and name set back, stored |
| a new sensor | keeps a free number it has, else gets the lowest free one; stored; added to the list without a position |
| a new sensor on a number already taken (from another machine) | moved to a free number |
| a known sensor not found | reported missing. Its number stays reserved, so it gets it back when it returns. |

**Naming the positions:** millweb lists the new sensors. With *Identify*
on, tapping a sensor (a knock over `tap_g`, 1.5 g) makes it answer with its
ID, and the page asks which position it is (base, X, Y, spindle).

**Replacing a sensor:** *Replace* hands a missing sensor's number, name and
position to a new one (`Addresser.replace`).

**Tested:** `tools/vibmon/test_vibaddr.py` runs the PC's code against the
sensors' C code on a simulated bus (in `make -C host check`). It covers:
- four new sensors, two with IDs one bit apart;
- automatic numbers;
- tap-to-name;
- a power cycle;
- a reflashed sensor;
- a missing one and its replacement;
- a stranger on a taken number.

### 3.2 Frames

**Frames** (`shared/vibproto.h`; a lower ID wins arbitration):

| ID | from | length | when | content |
|---|---|---|---|---|
| 0x010 + node | sensor | 8 B | at once | event: crash (shock g), vibration level, temperature level |
| 0x050 | PC | 16 B | every 20 ms | machine state: time, rpm, feed, teeth, flags (spindle, running, tool change), tool |
| 0x100 + node | sensor | 64 B | every interval | features: RMS, peak, velocity, kurtosis per axis, bearing band, temperature, level |
| 0x140 + node | sensor | 48 B | every FFT block | summary: peak, orders, tooth passing, chatter, envelope, rpm used |
| 0x300 + node | sensor | 64 B | on request or streamed | spectrum: 26 bins a frame, 16 bit scaled to the largest (2048 points: 40 frames) |
| 0x400 + node | sensor | 64 B | streamed | raw: 9 x y z samples a frame, numbered |
| 0x600 + node | PC | ≤64 B | — | console to the sensor (stmbl's commands: parameters, `showconf`, `flashsaveconf`) |
| 0x580 + node | sensor | ≤64 B | — | console from the sensor |
| 0x7E5 / 0x7E4 | PC / sensor | ≤32 B | setup | node numbers (§3.1) |

**Load** (from `test_vibproto`): a 64-byte frame takes 150 µs, an 8-byte
one 50 µs.

| traffic | load |
|---|---|
| 4 sensors' features and summaries, plus the machine state | 1.6 % |
| … plus a spectrum from each at 2 Hz | 6.4 % |
| raw samples streamed from one sensor (26.7 kHz × 3 axes) | 44.5 % |

So all four report all the time, and one at a time streams raw for a
detailed look. Classic CAN could carry the features, but not the raw
stream.

**The PC's side:** `tools/vibmon/` (README there).
- **Hardware:** SocketCAN with an MCP2518FD on the Raspberry Pi, or a USB
  CAN FD adapter (gs_usb / candleLight, PEAK) ✱.
- **Daemon:** a userspace daemon (`vibmon`). It:
  - sends the machine state from LinuxCNC's HAL (spindle speed, feed,
    tool, teeth from the tool table);
  - decodes the sensors' frames;
  - keeps trends;
  - feeds millweb;
  - raises HAL pins for the alarms.
- **The stop is in software** (decided): there is no wired alarm line. A
  sensor's crash event (ID 0x010 + node, the bus's highest priority)
  reaches `vibmon`, which sets a HAL pin; the HAL config links it to a
  feed hold or to the E-stop chain (`iocontrol.0.emc-enable-in` through an
  `and2`). The way there, sensor → bus → SocketCAN → userspace → HAL, is a
  few ms ✱ (to be measured): enough to stop after the impact and before a
  second one, not to prevent the first.
- **Watchdog:** `vibmon` also watches each sensor's features frame (every
  0.1 s). A sensor gone silent for 0.5 s raises its own pin, so a lost
  sensor is seen, and can be made to hold the machine like a crash.

## 4. Using it for tuning

| test | how | look at |
|---|---|---|
| spindle run-up | the spindle from 0 to max, slowly; `ord1` against rpm | unbalance (`ord1` ∝ rpm²), resonances (a peak in `ord1` at some rpm), bearings (`env`, `kurt`) |
| feed sweep, X and Y | moves at rising feeds; velocity and spectrum of the axis sensor | the ball screw at feed / pitch, guide noise, a servo ringing (a peak that doesn't move with feed) |
| servo tuning | step moves, sensors on the axis and the base | settling and ringing after a stop, with the drives' gains |
| cutting | a test cut; `chat_f`, `chat_a` against rpm | chatter. Move the rpm so a multiple of the tooth passing frequency lands near `chat_f` (stability lobes) |
| baseline | each machine once, healthy | set `vel_warn` / `vel_alarm` from it; trends against it |

## 5. Simulator

`host/hw/vibsim.c` stands in for the IIS3DWB on a machine. It generates
x y z samples at 26.667 kHz in int16 counts at `range_g` full scale. The
ingredients are pins:
- gravity on z;
- noise;
- unbalance (1× rpm, rotating);
- misalignment (2×);
- tooth passing (teeth × rpm);
- chatter (its own frequency);
- a ball screw (feed / pitch);
- a bearing defect (impacts at `brg_f` ringing `res_f`);
- a crash (one impact of `crash_g`);
- temperature.

```
cd host && make && ./stmbl_host
load vibsim
load vib
vibsim0.rt_prio = 1
vib0.rt_prio = 2
vib0.rpm = vibsim0.rpm
autonrt 1        # nrt (the FFTs) about once per ms, as on the sensor
start
vibsim0.rpm = 6000
vibsim0.unb = 1
run 1
vib0.vel_x
vibspec 5        # the 5 strongest peaks of the last spectrum
```

**Tests:** `host/tests/vib/*.test` (unbalance, chatter, bearing, crash,
parameters) and `test_vibproto`. Both are in `make -C host check`.

## What's next

1. **Firmware on a board:** bring-up of `vibsensor/` (README there), SPI
   DMA for the FIFO, USB for development.
2. **The PC's side:** done, `tools/vibmon/` (README there).
3. **millweb:** done, the Measure page.
4. **The board:** placed ([hardware/vibsensor](../hardware/vibsensor/README.md)); routing is next.
