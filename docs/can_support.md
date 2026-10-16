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

## 6. Decisions to make

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
