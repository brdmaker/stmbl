# stmblcan: stmbl drives over CAN from LinuxCNC on a Raspberry Pi

MCP2515 CAN controllers on the Pi's SPI, driven directly from the servo
thread: polled once per period, no interrupts, no SocketCAN in the loop.
Design and timing: [docs/can_support.md](../../docs/can_support.md).

**Status:**
- **Driver and bus logic:** tested on the host against simulated MCP2515s
  and drives (`make -C ../../host check`: `test_stmblcan`).
- **The component:** builds against LinuxCNC 2.9 and loads.
- **Not yet run:** on a Pi with real MCP2515s, nor with drive firmware
  speaking the protocol (`shared/canproto.h`).

## Setup on the Pi (Pi 4, 3 × MCP2515, 16 MHz crystals)

The device tree overlay binds the MCP2515s to the kernel's `mcp251x`
driver: spi0, chip selects on GPIO 13 / 6 / 5, interrupts on GPIO 21 / 26
/ 19. Keep it. `spi_takeover.sh` hands the chips to `spidev` for LinuxCNC
and back afterwards, so `candump` still works when LinuxCNC isn't running:

```
sudo ./spi_takeover.sh take       # can0..2 down, /dev/spidev0.0 .. 0.2
linuxcnc my-machine.ini
sudo ./spi_takeover.sh give       # back to can0..2
```

**SPI clock:**
- The overlay's `spi-max-frequency` (5 MHz) only applies to the kernel
  driver. stmblcan sets its own (`spi_hz`, 10 MHz, the MCP2515's maximum).
- On the Pi 4 the SPI clock divides the core clock, so fix it in
  `config.txt` (e.g. `core_freq_min=500`). A changing core clock changes
  the SPI clock.
- Check with an oscilloscope, and at 5 MHz if the wiring is long.

## HAL

```
loadrt stmblcan spi=/dev/spidev0.0,/dev/spidev0.1,/dev/spidev0.2 nodes=1,2:3,4:5,6
addf stmblcan.read  servo-thread      # first
addf motion-command-handler servo-thread
addf motion-controller servo-thread
...
addf stmblcan.write servo-thread      # last

net j0-pos-cmd joint.0.motor-pos-cmd => stmblcan.0.1.pos-cmd
net j0-pos-fb  stmblcan.0.1.pos-fb   => joint.0.motor-pos-fb
net j0-enable  joint.0.amp-enable-out => stmblcan.0.1.enable
net j0-fault   stmblcan.0.1.fault     => joint.0.amp-fault-in
```

**Parameters:**

| parameter | default | meaning |
|---|---|---|
| `spi` | — | one spidev per bus, comma separated |
| `nodes` | — | node IDs per bus: up to 2 a bus (the MCP2515's receive buffers), `:` between buses |
| `spi_hz` | 10000000 | SPI clock |
| `bitrate` | 1000000 | CAN bit rate (1 Mbit/s needs 16 MHz crystals) |
| `osc` | 16000000 | the MCP2515 crystal |
| `vel_scale` | 100 | full scale of `vel-cmd` / `vel-fb` (16 bit on the wire) |
| `timeout` | 10 | periods without feedback before a node is not present (and faults) |
| `desc` | — | a description file per bus, `,` between buses (empty for none) |
| `discover` | 0 | 1: ask each drive in `nodes` for its description at load |

**Pins:**
- **Per node** (`stmblcan.B.N.*`, bus B, node ID N):
  - in: `pos-cmd`, `vel-cmd`, `enable`, `index-enable`, `out-0..3`, `con-cmd`;
  - out: `pos-fb`, `vel-fb`, `fault` (also when not present), `index`,
    `in-0..3`, `current`, `power`, `temp`, `vbus` (32 periods apart),
    `present`, `missed`, `con-fb`.
- **Per bus** (`stmblcan.B.*`): `ok`, `tx-late`, `rx-overflow`, `foreign`,
  `tec`, `rec`.

**Console:** `con-cmd` / `con-fb` are the console's 10-bit field, as
`con_cmd` / `con_fb` on smart-serial. Link them to `sserialterm` for a pty
per drive; `servoterm-cli` then works as over USB.

## Pin inventory: discovery and description files

**One text format** (`candesc.h`) describes a device, whether it comes
from the device or from a file:
- **`discover=1`:** each drive in `nodes` is asked for its description at
  load (0x600 / 0x580 + node). A stmbl drive answers `device stbl`,
  `profile stmbl1`, `vel_scale ...`, and its pins are made from that.
- **`desc=bus0.desc`:**
  - **Devices without our firmware:** their frames and fields, as in
    `examples/bus0.desc`. Every field becomes a pin,
    `stmblcan.B.<device>.<field>`: out for frames from the device, in for
    frames to it.
  - **Drives pinned down:** with `discover=1` as well, a drive must
    describe itself as the file does, or the load is refused.
- **Errors:** a description error is reported with file, line and reason,
  e.g. `bus0.desc:5: type: bit, uN, sN, f32: q7`.

**The budget check:** a bus whose period does not fit the MCP2515 is
refused at load. That means at most 3 frames out (commands, the devices'
tx frames, SYNC) and 2 in, in the worst period. Frames with `every N`
are staggered over periods.

For example, 1 drive plus the I/O module fits, while 2 drives plus it
need 4 frames out and 3 in, and are refused.

## Per servo period (each bus)

| when | SPI | CAN |
|---|---|---|
| `stmblcan.read` | READ STATUS; READ RX BUFFER × up to 2 | — |
| `stmblcan.write` | LOAD TX BUFFER × 3 (2 commands, SYNC), RTS | commands, SYNC; the drives answer with feedback |

That is 3 SPI calls and ~73 bytes per bus: ~175 µs of transfer at 10 MHz
for three buses, plus ioctl overhead (to measure on the Pi). CAN time is
~605 µs of every 1 ms (worst-case stuffing).

**Receive filters:** only the expected feedback IDs are accepted. A
foreign device on the bus can't fill the receive buffers, but it still
takes bus time.
