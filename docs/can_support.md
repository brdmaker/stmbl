# CAN support for stmbl and brdstepper (investigation)

Branch `can-support`, built on `brdstepper`. **Status:** an investigation;
nothing implemented yet except `tools/canbus_budget.py` (the bus timing
below).

**What it would mean, in short:**
- **Hardware:**
  - **brdstepper:** CAN FD is already in its plan (the CAN variant).
  - **stmbl v4.0:** the CAN controller's pins already reach a header;
    it needs only a small plug-on transceiver board.
- **Throughput:** CAN is slower than smart-serial for fast multi-axis
  loops. At a 1 kHz servo rate one bus carries 2 drives (classic CAN) to
  4–5 drives (CAN FD).
- **What it is good for:**
  - one daisy-chained cable through many drives (with power);
  - a much faster console and firmware update than smart-serial's;
  - hosts without a Mesa card;
  - auxiliary axes: gripper, tool changer, conveyor, spindle.
- **The big piece of work is LinuxCNC's side.** There is no CAN driver in
  LinuxCNC; a realtime HAL component on SocketCAN would have to be written.
- **Protocol recommendation:** a small stmbl protocol that carries the
  same process data as smart-serial, with CANopen-style identifiers.
  CiA 402 (CANopen drive profile) stays optional, for when other masters
  have to talk to the drives.

## 1. Hardware

### stmbl v4.0 (STM32F405)

**What the board has** (from `hw/kicad/v4.0/stmbl_4.0.net`):
- The F405's **CAN1** is routed: PD1 (CAN1_TX, U26 pin 82) and PD0
  (CAN1_RX, U26 pin 81).
- It goes as 3.3 V logic, through an ESD array (U20, USBLC6-4SC6), to the
  **2×6 header P10**.
- **There is no CAN transceiver on the board.**

| P10 | net |
|---|---|
| 1, 2, 3 | CMD_45, CMD_36, CMD_12 (the command interface lines) |
| 4 | NRST |
| 5 | **CAN_TX** (PD1) |
| 6 | SWDIO |
| 7 | **CAN_RX** (PD0) |
| 8, 9 | GND |
| 10 | SWCLK |
| 11 | +24V |
| 12 | +5V |

**What CAN needs on a v4.0 board:**
- **A plug-on board on P10:** a transceiver (TCAN1044V ✱: 5 V bus side
  from P10's +5V, 3.3 V logic), two connectors for daisy-chaining, and a
  120 Ω terminator on a jumper.
- **Clear of the other signals:** the board must leave SWD and the CMD
  lines alone. With the debug probe on the same header, use a pass-through
  header or a second small connector.

**Controller: bxCAN, classic CAN only** (no CAN FD), up to 1 Mbit/s:
- 3 transmit mailboxes, 2 receive FIFOs of 3, 28 filter banks;
- the StdPeriph driver is in the repo (`stm32f4xx_can.c`), and nothing in
  the firmware uses CAN yet.

### brdstepper (STM32G473)

- **Controller:** FDCAN1 (CAN FD, data phase up to 8 Mbit/s with a
  suitable transceiver ✱).
- **Pins and parts:** PB8 RX / PB9 TX, the transceiver's standby on PB13.
  The CAN variant's parts are in `docs/brdstepper_hw.md` §4.5 (TCAN1044V,
  two connectors for daisy-chaining, a terminator on a jumper).
- **Option bytes:** PB8 doubles as BOOT0, so the option bytes must ignore
  the pin (`make -C brdstepper option_bytes`).
- **Clock:** FDCAN takes 85 MHz from the PLL's Q output, already
  configured (`hw_clock_init`).

### Classic and FD on one bus

**A classic controller sees a CAN FD frame as an error** and destroys it
with error frames. stmbl v4.0 (bxCAN, not FD-tolerant) and brdsteppers
running CAN FD therefore cannot share a bus. Either:
- **all nodes classic:** brdstepper's FDCAN can run classic too, with the
  classic throughput (§2); or
- **two buses:** stmbl drives on a classic bus, brdsteppers on an FD bus.

## 2. Timing: how many drives per bus

`tools/canbus_budget.py` computes the frame times from the CAN bit layout
with worst-case stuff bits, and the drives that fit at 70 % bus load. Each
drive gets a command and a feedback frame every cycle, plus one SYNC frame
per cycle.

**Frame times** (worst case):

| frame | classic 1 Mbit/s | FD 1 / 2 Mbit/s | FD 1 / 5 Mbit/s | FD 1 / 8 Mbit/s |
|---|---|---|---|---|
| 8 bytes | 135 µs | | | |
| 16 bytes | | 130 µs | 73 µs | 58 µs |
| 64 bytes | | 373 µs | 170 µs | 119 µs |

**Drives per bus:**

| scheme | 2 kHz | 1 kHz | 500 Hz |
|---|---|---|---|
| classic 1 Mbit/s, 8 B command + 8 B feedback | 1 | 2 | 4 |
| FD 1/2 Mbit/s, 16 B + 16 B | 1 | 2 | 5 |
| FD 1/5 Mbit/s, 16 B + 16 B | 2 | 4 | 9 |
| FD 1/5 Mbit/s, one 64 B command frame per 4 drives | 2 | 4 | 12 |
| FD 1/8 Mbit/s, 16 B + 16 B | 2 | 5 | 11 |

**For comparison:** smart-serial gives every drive its own 2.5 Mbit/s
link, on one of a Mesa card's 2–8 ports. A 6-axis robot at 1 kHz is
- 6 ports of smart-serial,
- 2 CAN FD buses at 5 Mbit/s,
- or 3 classic CAN buses.

**Running the bus slower than the servo thread:**
- **Interpolation:** the drives already interpolate between commands
  (`vel_int0` takes position plus velocity). LinuxCNC's servo thread can
  stay at 1 kHz while the bus runs at 500 Hz: 9–12 drives on one FD bus.
- **The cost:** latency. A command reaches the motor about a bus cycle
  later. Following error at speed grows, unless LinuxCNC compensates.

## 3. LinuxCNC's side

**What exists** (forum and community sources, see Sources):
- There is no CAN or CANopen driver in LinuxCNC itself, and Mesa's
  hostmot2 has no CAN.
- A generic CiA 402 HAL component (`hal-cia402`) exists. It implements the
  drive state machine above a transport and is used mostly with EtherCAT
  (`lcec`); it supports CSP, and possibly CSV.
- Users report SocketCAN's realtime behaviour as a concern; no working
  SocketCAN CiA 402 setup in LinuxCNC was found.

**Host interfaces for a 1 kHz servo loop:**

| interface | fit |
|---|---|
| PCIe / M.2 CAN FD card with a SocketCAN driver (e.g. PEAK, Kvaser) ✱ | best: interrupt-driven, low latency. Under PREEMPT_RT the driver's IRQ thread needs a priority above the rest. |
| Raspberry Pi 4/5 + MCP2518FD on SPI ✱ | possible for a few drives; the SPI round trip adds latency. LinuxCNC runs on the Pi. |
| USB CAN adapters | not for the servo loop: USB polling adds up to 1 ms (full speed) of latency and jitter. Fine for setup, the console and updates. |
| a gateway (a Mesa smart-serial remote ↔ CAN) | timing stays in hostmot2. But a remote carries 96 / 224 bits, about 1–3 drives' worth, so it scales poorly. |

**The HAL component to write** (`stmblcan`, realtime, SocketCAN):
- **Per cycle:**
  1. receive what came (non-blocking);
  2. send SYNC and the commands;
  3. count timeouts.
- **Pins per drive:** the same as hostmot2's smart-serial `stbl` remote
  (pos_cmd, vel_cmd, enable, pos_fb, vel_fb, current, power, fault, in /
  out). Existing HAL files then change only by name.
- **Console ptys** as `sserialterm`'s: one per drive, from the same
  component.
- **Realtime:** SocketCAN's `send` / `recv` are syscalls in a uspace
  realtime thread. That works under PREEMPT_RT, but with the NIC's
  interrupt thread to tune. This needs measuring on real hardware early.

### Raspberry Pi with MCP2515s (no Mesa card)

**The setup:** LinuxCNC on a Raspberry Pi (PREEMPT_RT), 3 × MCP2515 on
SPI, one classic CAN bus each. There are two ways to the realtime thread.

**A. The kernel driver (`mcp251x`) and SocketCAN: not for the servo
loop.**
- **The path:** a frame arrives, the GPIO interrupt fires, the driver's
  threaded handler queues SPI transfers through the kernel SPI framework,
  and the network stack hands the frame to the socket.
- **The problem:** every step adds latency and jitter, and the MCP2515
  holds only 2 received frames. A late handler loses frames (`rx_over_errors`;
  see the HI-311x and MCP2518FD reports in Sources).
- **The tuning:** IRQ threads pinned to a core with raised priority.
  Possible, but every reboot and kernel update is a new measurement.
- **Good for:** setup tools (`candump`, `cansend`) when the machine is not
  running.

**B. A realtime component that owns the MCP2515s (recommended).**
- **Polled, once per servo cycle,** with no interrupts and no network
  stack. Each controller sees the same SPI sequence every cycle, so the
  timing is deterministic.
- **Precedent:** LinuxCNC's own Pi drivers do the same for Mesa SPI
  cards (`hm2_rpspi`).

**SPI access, in order of preference:**
- **`/dev/spidev` from the realtime thread:** works on the Pi 4 and Pi 5.
  Batch each controller's transfers into one `SPI_IOC_MESSAGE(n)` ioctl
  per phase, to pay the syscall cost a few times per cycle, not per
  transfer. Measure early.
- **Direct registers, as `hm2_rpspi`:** faster, but tied to the Pi model.
  hm2_rpspi covers the Pi 2/3 (BCM2835 SPI). The Pi 5's SPI sits in RP1
  behind PCIe and needs a new register driver (forum reports). Only if
  spidev proves too slow.

**The cycle per bus** (classic 1 Mbit/s, 2 drives, §2):

| when | SPI (MCP2515 instructions ✱) | bytes |
|---|---|---|
| start of the servo period | RX STATUS; READ RX BUFFER ×2 (clears the flags): the 2 drives' feedback from the last cycle | 3 + 2 × 14 |
| | LOAD TX BUFFER ×3: SYNC + the 2 commands; RTS | 3 × 14 + 1 |
| **sum** | | **~74** |

**SPI time:** 74 bytes at 10 MHz is ~60 µs per bus, ~180 µs for three,
plus chip-select gaps and ioctl overhead. That is ~25–40 % of a 1 ms servo
period: workable, to be measured.

**The rules this sets for the protocol:**
- **Receive buffers:** a bus may carry at most 2 frames toward the Pi per
  cycle (the MCP2515's 2 receive buffers, rollover on). So no separate
  heartbeat or console frames: everything rides in the one feedback frame
  per drive.
- **Transmit buffers:** 3 frames out per cycle (SYNC + 2 commands).
- **Classic CAN, 8 bytes:**
  - command: pos_cmd f32, vel_cmd 16-bit scaled, flags, 1 console byte;
  - feedback: pos_fb f32, vel_fb 16-bit, flags, 1 console byte;
  - current, power and temperatures in a rotating slot.
  - The console then runs at 1 byte per cycle (1 kB/s), as over
    smart-serial. A service mode, with the machine idle, can stream faster.
- **Latency:** a command takes effect, and feedback comes back, one cycle
  later, as with hostmot2.

**Capacity:** 3 buses × 2 drives at 1 kHz = 6 joints. A toolhead needs
- a 4th MCP2515,
- 500 Hz (4 drives per bus),
- or the wrist drive as its gateway (§6, now over CAN).

**Hardware to check on existing MCP2515 boards** ✱:
- **Crystal:** 1 Mbit/s needs 16 MHz. 8 MHz modules top out at 500 kbit/s:
  a bit needs at least 5 time quanta, and a quantum is at least 2 clock
  periods.
- **Transceiver:** a TJA1050 needs 5 V, and its RX output then drives the
  Pi's 3.3 V input. Use a 3.3 V transceiver, or level-shift RX.
- **Chip selects:** SPI0 has CE0 / CE1. The third controller needs SPI1
  or a GPIO chip select.
- **Kernel driver:** the `mcp251x` overlay must be off for controllers the
  realtime component owns.

## 4. Protocol

### Option A: CANopen, CiA 402 drive profile

- **Standard:** NMT, SYNC, PDOs for the process data, SDOs for
  configuration, heartbeat, EMCY. CiA 402 adds the drive state machine
  (controlword 0x6040 / statusword 0x6041), the modes (CSP 8, CSV 9,
  homing 6) and the objects (0x607A target position, 0x6064 actual
  position, 0x606C velocity, 0x6077 torque, ...).
- **For:** works with other CANopen masters (PLCs, CODESYS), standard
  tools, and `hal-cia402` for the state machine on LinuxCNC's side.
- **Against:**
  - **A stack:** CANopenNode (Apache-2.0) for classic CAN. CANopen FD
    (CiA 1301) is a different protocol, with less support.
  - **Configuration:** stmbl's dynamic config (templates, ~1000 HAL pins,
    the text console) has no natural place in an object dictionary.
  - **LinuxCNC's side is still missing:** NMT, SYNC, the PDO mapping and an
    SDO client all have to be written.

### Option B: stmbl over CAN (recommended first)

The smart-serial design moved onto CAN frames, with CANopen-style
identifiers, so a bus can be read in standard analyzers and later share a
bus with CANopen devices:

| COB-ID (11 bit) | frame | content |
|---|---|---|
| 0x080 | SYNC | from LinuxCNC every cycle: the drives sample feedback and apply the last command, and lock their loop to it |
| 0x200 + node | command | pos_cmd (f32), vel_cmd (f32), enable, out 0..3, index_enable; a console chunk (FD) |
| 0x180 + node | feedback | pos_fb, vel_fb, current, power, fault, in 0..3, index; a console chunk (FD) |
| 0x600 + node / 0x580 + node | console stream | classic CAN: the console in its own frames (7 bytes and a sequence byte) |
| 0x700 + node | heartbeat | state, at 10 Hz |

**Process data:** the same fields as smart-serial's table
(`tools/gentable.c`), so the drive side reuses `linrev` / `vel_int` /
`fault` / `pe` through the same templates (`link can` in place of
`link sserial`).

**Console:**
- **The link:** `sscon`'s alternating-bit protocol with chunks instead of
  single bytes. One frame per cycle carries 7 bytes (classic) or up to 48
  (FD).
- **Speed:** that is 7–48 kB/s at 1 kHz, against smart-serial's 1 kB/s.
- **What it gives:** `servoterm-cli --dump`, `--capture` and `--update`
  in seconds. An update of a 350 kB application takes ~10 s on FD, against
  ~15 min over smart-serial.

**Clock lock:** smart-serial's `clock_scale` already locks the drive's
5 kHz loop to the host's frame rate. SYNC gives the same reference, so the
drives sample and apply in step with LinuxCNC.

**Node IDs:** from the config (`can0.node = 3`), with a fallback for a
drive on its own (it answers on ID 127 until set). An LSS-style assignment
by serial number (the unit number smart-serial uses) can come later.

**Bootloader:** a CAN loader beside the smart-serial one
(`docs/sserial_bootloader.md`), using the same staged-update path (`fwup`).

**CiA 402 later:** option B's fields map onto CiA 402's objects, so a
CiA 402 PDO profile can be added for other masters without changing the
drive's internals.

## 5. What it would take

| piece | where | testable before hardware |
|---|---|---|
| CAN driver: bxCAN (F4) and FDCAN (G4): init, filters, interrupt queues | `src/`, `brdstepper/src/` | builds only |
| drive component `can` (the process data, SYNC lock, timeouts, the console chunks) | `shared/comps/can.c`, with a small per-board driver API | yes: a simulated bus in `host/` (several drives in one process, as the arm simulator does) |
| chunked console link (`sscon` with n bytes a frame) | `shared/sscon.c` | yes: `test_sscon` with frame loss |
| templates `can.txt` (as `sserial.txt`) | `conf/template/` | yes |
| LinuxCNC component `stmblcan` (SocketCAN, realtime) and its console ptys | `linuxcnc/stmblcan/` | partly: against Linux `vcan` if the kernel has it, else a socketpair stub |
| CAN bus model in the simulator (frame times from `canbus_budget.py`, losses) | `host/` | yes |
| bootloader's CAN loader | `bootloader/`, brdstepper's bootloader | the protocol part, like `lbp_dev` |
| stmbl v4.0 adapter board (transceiver on P10) | `hw/` | — |
| CiA 402 profile (optional) | on top | partly |

**Order:**
1. the shared protocol and the drive component with the simulated bus and
   tests;
2. the LinuxCNC component, measured early on a PCIe CAN FD card under
   PREEMPT_RT (the realtime risk);
3. the drivers on the boards;
4. the console and update over CAN;
5. the bootloaders.

## 6. Toolheads: what cobots do, and what fits here

### What the cobots do

| robot | tool flange | joints |
|---|---|---|
| Universal Robots e-Series | M8, 8 pins: 12 / 24 V, GND, 2 digital in, 2 digital out, and two analog inputs **or one RS-485** channel (Modbus RTU to grippers, e.g. Robotiq) | RS-485 between the control box and the joints, protocol undocumented (UR forum) |
| Doosan E-Series | M8, 8 pins: +24 V, GND, digital I/O, **RS-485 A / B** (pins 4 and 6) | — |
| Schneider Lexium Cobot | M8: 2 digital in, 2 out, 2 analog in; two outputs combinable into **RS-485** | — |
| Elephant myCobot Pro 450 | M8: 24 V and **RS-485** to the end effector | **CAN** port at the base |

**The pattern:**
- **Tool flange:** power, a few plain I/O, and one 2-wire half-duplex
  serial bus, all in an 8-pin M8. The bus runs at low rate, for grippers
  and sensors.
- **The arm itself:** the fast joint traffic stays inside it, on its own
  bus.
- **None of these uses CAN at the tool.** RS-485 Modbus is what the
  gripper makers support.

### What this means for an stmbl / brdstepper toolhead

| link | wires to the tool | nodes | for the toolhead |
|---|---|---|---|
| smart-serial (RS-422, full duplex) | 4 signal + power | 1 per port | a port and 4 wires per device: too many through a wrist |
| RS-485 half duplex (the cobots' way) | 2 signal + power | multi-drop, one master polling | standard grippers (Modbus RTU); our own devices would need a polled protocol |
| **CAN / CAN FD** | **2 signal + power** | multi-drop, arbitration, any node can talk | the same 4 wires as RS-485, plus event-driven, hot-plug friendly and error handling built in |

**CAN suits a toolhead well:**
- **Wires:** 4 contacts (24 V, GND, CANH, CANL) carry any number of
  devices. Examples: the gripper (a brdstepper), an I/O node (tool
  changer sensors, an air valve), a tool ID, a camera trigger.
- **Rate:** a gripper or tool axis rarely needs 1 kHz; 250–500 Hz with the
  drive's interpolation is plenty. One bus then has room to spare (§2).
- **Tool changing:**
  - **Changer contacts:** only those 4 have to pass through it, whatever
    the tool carries.
  - **Recognition:** a tool announces itself by heartbeat and node ID, and
    tools without electronics are simply absent.
  - **Termination:** put the termination on the wrist side, at the bus's
    fixed end, so the bus stays terminated with no tool fitted. Keep the
    tool's stub short: at 1 Mbit/s up to about 0.3 m, and less in a CAN FD
    data phase. A tool segment might run classic or 2 Mbit/s for margin.
- **Spare pins:** an M8 8-pin flange like the cobots' still leaves 4 pins:
  2 plain I/O (an e-stop or tool-present loop), plus RS-485 for a bought
  Modbus gripper.

### Where the toolhead bus meets LinuxCNC

1. **A CAN interface in the host** (§3). The toolhead devices are then
   ordinary `stmblcan` nodes. This is the most general option, but needs
   the realtime CAN component.
2. **The wrist drive as gateway.** Joint 6's drive (stmbl or brdstepper)
   keeps its smart-serial link to the Mesa card and is the CAN master of
   the toolhead behind it, forwarding the tool's data in its own frame.
   - **For:** no CAN in LinuxCNC at all, and the toolhead is in step with
     the servo thread.
   - **Against:** smart-serial has little room left (96 bits on older
     bitfiles, 224 on newer ones), enough for a gripper's command and
     feedback but not much more. A slow multiplexed slot (a different tool
     value each cycle), or the console channel, would carry the rest.
3. **RS-485 Modbus for bought grippers**, from a non-realtime HAL
   component, as the cobots do. This works for open / close / force
   commands, not for servo-rate control.

### The data budget through the wrist drive

**The limit** (hostmot2's source, `sserial.h`): `HM2_SSERIAL_NUMREGS` 7,
"224 bits of data per channel in sserialb". Older smart-serial firmware
stops at 96 bits. Which one a card has depends on its bitfile: check
yours.

**stmbl's frame today** (`tools/gentable.c`):

| direction | used | fields | free (96) | free (224) |
|---|---|---|---|---|
| to the drive | 80 bits | pos_cmd 32, vel_cmd 32, console 10, out 0..3, enable, index_enable | 16 | 144 |
| from the drive | 96 bits | pos_fb 32, vel_fb 32, current 8, power 8, console 10, in 0..3, fault, index | 0 | 128 |

**What the toolhead needs every servo period is small,** as long as its
fast loops close on the tool. The gripper's position and current loops
run in the gripper's own drive (a brdstepper on the tool), as the
joints' do. Through the wrist go only setpoints and status, for example:

| to the tool | bits | from the tool | bits |
|---|---|---|---|
| gripper target (0.01 mm, 16 bit) | 16 | gripper position | 16 |
| gripper force / current limit | 8 | gripper current (force) | 8 |
| gripper mode (move / grip / release / home) | 4 | status: gripped, moving, homed, fault, tool present | 8 |
| tool digital outputs (valves, lights) | 8 | tool digital inputs | 8 |
| | | tool ID | 8 |
| **sum** | **36** | **sum** | **48** |

- **With 224-bit firmware:** it fits with room to spare, in both
  directions.
- **With 96-bit firmware:** the drive-to-host direction is full today.
  48 bits have to come from compacting stmbl's own fields:
  - `vel_fb` 32 → 16 bits, scaled to the axis's top speed;
  - or `vel_fb` dropped altogether, since LinuxCNC can differentiate
    `pos_fb`.

**What doesn't need the cyclic frame:**
- **Rotating slot:** a 4-bit slot number and a 16-bit value, a different
  tool value each period. That gives 16 values, each at 1 kHz / 16 =
  60 Hz: temperatures, tool counters, sensor readings.
- **Console relay:** the 10-bit console channel already reaches the wrist
  drive. It can pass console lines on to the toolhead's nodes over CAN
  (`tool1 about`, `tool1 showconf`, `servoterm-cli --update` of a tool
  node). That covers configuration, tool data and firmware, without
  cyclic bits. Smart-serial's ~1 kB/s console is the bottleneck there,
  not CAN.
- **Tool change** (identify, load parameters, check the gripper) is
  event-driven: the console and slow slots suffice.

**When the wrist path is not enough:**
- **Signs:** several fast tool axes, a toolhead streaming sensor data
  (force/torque at a high rate), or a servo-rate loop closed in LinuxCNC
  through the tool.
- **Remedy:** the toolhead's CAN goes to the host directly (option 1), and
  the wrist drive only passes the wires through.

For the robot work on `claude/robot-dynamics` (the stmbl electric gripper,
the pneumatic tool changer), option 2 fits best. Joint 6's smart-serial
already runs, and the gripper and the tool changer's sensors would move
onto 4 wires through the wrist.

## 7. Decisions to make

1. **Use:**
   - (a) as many axes as possible at 1 kHz: smart-serial stays the main
     link, and CAN serves auxiliaries; or
   - (b) daisy-chained drives in tight places, at 500 Hz or with fewer
     axes per bus; or
   - (c) hosts without a Mesa card.
2. **Host:**
   - a PC with a PCIe CAN FD card, or
   - a Raspberry Pi with an MCP2518FD.
3. **Interoperability:** do other CANopen masters need to drive these?
   (That decides whether CiA 402 is needed now.)
4. **Buses:** stmbl v4.0 on classic CAN and brdstepper on FD, on separate
   buses; or everything classic.

## Sources

Raspberry Pi SPI and MCP2515:
- [hm2_rpspi(9): BCM2835 SPI from realtime, Pi 2/3](https://dyn.manpages.debian.org/trixie/linuxcnc-uspace/hm2_rpspi.9)
- [LinuxCNC forum: SPI on the Raspberry Pi 5](https://forum.linuxcnc.org/18-computer/51748-eoi-in-getting-spi-going-on-the-rpi5?start=10)
- [LinuxCNC forum: LinuxCNC on Raspberry Pi 5](https://forum.linuxcnc.org/9-installing-linuxcnc/50203-linuxcnc-on-raspberry-pi-5?start=70)
- [HI-311x CAN-over-SPI driver thread: RX FIFO filling, IRQ threads on another core](https://lkml.iu.edu/hypermail/linux/kernel/1703.1/01321.html)
- [MCP2518FD RX overflow under load (NXP community)](https://community.nxp.com/t5/i-MX-Processors/MCP2518FD-Rx-buffer-overflow-on-imx8mm/td-p/1237909)
- [mcp251x: re-check the interrupt flags after mailbox 0 (kernel log)](https://git.zx2c4.com/linux-dev/log/drivers/net/can/spi?showmsg=1)
- [MCP2515 family data sheet DS20001801K](https://cdn.sparkfun.com/assets/1/0/e/4/0/MCP2515-Family-Data-Sheet-DS20001801K.pdf)

Tool flanges and joint buses of cobots:
- [Universal Robots UR7e: tool connector (M8 8-pin, 2 AI or 1 RS-485)](https://www.universal-robots.com/products/ur7e-920/)
- [Universal Robots: tool connector electrical interfaces](https://www.universal-robots.com/ko/developer/hardware-and-motion/electrical-interfaces-tool-connector/)
- [ur_robot_driver: setting up the tool communication on an e-Series robot](https://docs.ros.org/en/iron/p/ur_robot_driver/setup_tool_communication.html)
- [UR forum: custom controller (RS-485 joint bus, protocol undocumented)](https://forum.universal-robots.com/t/using-ur-arm-with-custom-controller-and-power-supply/17535)
- [Doosan E-Series flange I/O (M8 pinout, RS-485 A/B)](https://manual.doosanrobotics.com/en/user/3.2.0/3.-E-Series/flange-i-o)
- [Schneider Lexium Cobot: electrical connections](https://product-help.schneider-electric.com/Lexium%20Cobot/V1.7/en/LXMCOBOT/LXMCOBOT/TPC_COBOT_ElecConnCobot-7430089A.html)
- [Elephant Robotics myCobot Pro 450 (RS-485 tool, CAN base port)](https://openelab.io/a/s/products/mycobot-pro-450)

LinuxCNC CAN / CiA 402 status, from forum and community pages:
- [hal-cia402 (CiA 402 HAL component)](https://gitee.com/linuxcnc/hal-cia402)
- [LinuxCNC forum: hal cia402 drive interface for CAN bus, EtherCAT](https://forum.linuxcnc.org/24-hal-components/41858-hal-cia402-drive-interface-for-can-bus-ethercat-etc?start=40)
- [LinuxCNC forum: separating CiA 402 logic from EtherCAT](https://forum.linuxcnc.org/ethercat/58434-separating-cia402-logic-from-ethercat-lcec-modular-adapter-drive-stub-valid)
- [LinuxCNC forum: Raspberry Pi talking CiA 402 over CAN (MCP2515)](https://www.forum.linuxcnc.org/38-general-linuxcnc-questions/44427-linuxcnc-on-raspberry-pi-talking-cia402-through-can-bus-using-mcp2515-chip)
- [LinuxCNC forum: a HAL component for CANopen communication](https://www.forum.linuxcnc.org/24-hal-components/27201-write-a-hal-componet-for-canopen-commnunication)

Hardware facts come from this repo (`hw/kicad/v4.0`) and
`docs/brdstepper_hw.md`. Frame timing comes from `tools/canbus_budget.py`
(ISO 11898-1 frame layout). ✱: product claims to check against their
datasheets.
