# Drive console over smart-serial

The drive's text console runs inside the smart-serial process data, so a
drive in a tight place needs no USB cable for setup:
`show`, `about`, pin reads and writes, `showconf`, `flashsaveconf`, the
capture scope, firmware updates. On the LinuxCNC machine, the
`sserialterm` component turns each drive's console into a pseudo-terminal.
`servoterm-cli` (or any terminal program) talks to that pty exactly as to
the drive's USB port. This works while LinuxCNC runs, in estop and while
machining.

```
 drive                         Mesa card              LinuxCNC host
 term ─ sscon0 ─ con_fb ──────────────────► hm2 pin ─ sserialterm ─ pty ─ servoterm-cli
             └── con_cmd ◄──────────────── hm2 pin ◄─┘
```

Related docs:
- [capture_scope.md](capture_scope.md): the capture scope, downloaded over
  this console.
- `linuxcnc/sserialterm/README.md`: the component, in short.
- On branch `sserial_bootloader`, `sserial_bootloader.md`: firmware updates
  over this console.

## Quick start

1. **Drive firmware** with the console (this branch): `make all`, then
   flash as usual (see `CLAUDE.md`).
2. **The component:**
   ```
   make -C linuxcnc/sserialterm all install
   ```
3. **HAL**, one `tty=` entry per drive, in the order of the `net`s below:
   ```
   loadrt sserialterm tty=/tmp/drive-x,/tmp/drive-y
   addf sserialterm.0 servo-thread
   net x-con-fb  hm2_7i96s.0.stbl.0.0.con_fb  => sserialterm.0.con-fb
   net x-con-cmd sserialterm.0.con-cmd        => hm2_7i96s.0.stbl.0.0.con_cmd
   net y-con-fb  hm2_7i96s.0.stbl.0.1.con_fb  => sserialterm.1.con-fb
   net y-con-cmd sserialterm.1.con-cmd        => hm2_7i96s.0.stbl.0.1.con_cmd
   ```
   The hm2 names follow `hm2_<board>.<n>.stbl.<port>.<channel>.<pin>`;
   `halcmd show pin hm2 | grep con_` lists them.
4. **Talk to the drive:**
   ```
   servoterm-cli -d /tmp/drive-x -c about
   servoterm-cli -d /tmp/drive-x -c "fault0.state"
   servoterm-cli -d /tmp/drive-x --dump drive-x.conf     # back up the config
   servoterm-cli -d /tmp/drive-x --load drive-x.conf     # write and verify it
   screen /tmp/drive-x                                   # interactive
   ```

## The frame

The console takes 10 bits each way in the process data. The rest of the
frame is unchanged, apart from the 8-bit power field (`power`):

| to the drive (80 bits) | from the drive (96 bits) |
|---|---|
| `pos_cmd` float 32 | `pos_fb` float 32 |
| `vel_cmd` float 32 | `vel_fb` float 32 |
| **`con_cmd` 10** | `current` 8, `power` 8 |
| `out_0..3`, `enable`, `index_enable` 6 | **`con_fb` 10** |
| | `in_0..3`, `fault`, `index_enable` 6 |

The drive's input is 96 bits, the limit of older Mesa smart-serial
bitfiles (newer "sserialb" ones allow 224). `tools/gentable.c` generates
the descriptor tables in `src/comps/sserial.c`;
`host/tests/check_sserial_table.py` checks them. The console fields have
the smart-serial *stream* type, which hostmot2 shows as raw u32 pins.

**Field layout** (`shared/sscon.h`):

| bits | meaning |
|---|---|
| 0–7 | a data byte |
| 8 | the sender's toggle: flips with each new byte |
| 9 | the ack: the toggle of the last byte this side took from the other |

## The link protocol (`shared/sscon.c`)

The protocol is an alternating bit, with one byte in flight each way.
Both ends run the same code: the drive (`sscon0`, in the `sserial`
component) and LinuxCNC (`sserialterm`).

- **Sending:** a byte goes out with a new toggle, and is repeated every
  frame until the other side's ack equals that toggle.
- **Receiving:** a byte is taken when its toggle differs from the last one
  taken and there is room; taking it changes the ack. A full receiver does
  not ack, which is the flow control.
- **Lost or repeated frames** (CRC errors, hostmot2 skipping a channel)
  delay bytes. They never lose or double them. `test_sscon` loses 10 % and
  30 % of frames.
- **Byte 0 is never sent.** A side that restarted reads as all zero, so it
  offers nothing, and a 0 left over from the other side's restart is
  dropped.
- **Speed:** one byte per servo period each way, so ~1 kB/s at a 1 kHz
  servo thread. A 1 kB `--dump` takes ~3 s.
- **Buffers:**
  - Drive: transmit 2048 bytes (`SSCON_TXSIZE`), receive 256
    (`SSCON_RXSIZE`).
  - The host simulator builds with a 32 kB transmit ring, because it cannot
    wait.

**Restarts:**
- **The drive's side:** when the smart-serial link times out (LinuxCNC
  stopped, cable out), the drive resets its link state (`sscon_reset`).
- **LinuxCNC's side:** it starts afresh with the component.
- **After a reconnect:** the first command answers whole, and no left-over
  bytes of an old reply arrive (`console_link.test`).

**When the drive talks:** the drive sends console output over the link
only once the LinuxCNC side has sent it something (`active`). So nothing
piles up while nobody listens.

## The drive side

- **Output:** `src/syscalls.c`'s `_write` copies everything the firmware
  prints into `sscon0` as well as to USB.
  - Both consoles see all output: a command typed on USB also answers on
    the smart-serial console, and the other way round.
  - In thread mode, a print waits up to 100 ms for room in the ring.
    Inside an interrupt it does not wait, and what does not fit is dropped.
- **Input:** `shared/comps/term.c` reads whole lines from `sscon0` and runs
  them like USB lines.
  - **63 characters at most** (a 64-byte buffer).
  - A longer line is dropped whole, and the next one works
    (`console_link.test`).
- **Link handling:** the `sserial` component (`src/comps/sserial.c`):
  - runs `sscon_frame` once per received process data frame, only when the
    frame's CRC is right;
  - resets the console on a link timeout.
- **The F3 build** has no console: `term.c` refers to it weakly.

## The LinuxCNC side: `sserialterm`

`linuxcnc/sserialterm/sserialterm.c`. One instance per `tty=` entry.

| pin | dir | type | meaning |
|---|---|---|---|
| `sserialterm.N.con-fb` | in | u32 | from hm2's `con_fb` |
| `sserialterm.N.con-cmd` | out | u32 | to hm2's `con_cmd` |
| `sserialterm.N.bytes-in` | out | u32 | bytes received from the drive |
| `sserialterm.N.bytes-out` | out | u32 | bytes sent to the drive |
| `sserialterm.N.active` | out | bit | the drive has answered |
| `sserialterm.N.client` | out | bit | a program has the pty open |

- **The ptys:** each `tty=` path is a symlink to a pty that a non-realtime
  thread serves.
- **A pty with nobody attached drops what the drive says.** Attach
  first, then send.
- **The function** must run in the servo thread that also runs hm2's
  read and write.

## servoterm-cli over the pty

Everything `servoterm-cli` does over USB works over the pty, at console
speed:

| option | what | time over smart-serial |
|---|---|---|
| `-c CMD` (repeatable), SCRIPT | commands | the reply's length at ~1 kB/s, plus the `-w` gap (50 ms) |
| `--dump FILE` / `--load FILE` | the flash config | ~3 s for 1 kB; `--load` sends a line at a time and verifies, so longer |
| `--capture FILE` | the capture scope (see [capture_scope.md](capture_scope.md)) | ~14 s for 2 × 2048 samples |

## Limits and gotchas

- **Line length:** 63 characters per line, both for typed commands and for
  `appendconf` in `--load` (servoterm-cli checks).
- **Shared output:** both consoles share the drive's output (see above). A
  long `showconf` on USB also costs console time on the link.
- **Byte 0** cannot be sent. The console carries text and base64, so this
  never matters.
- **Pin names:** hostmot2 builds pin names from the descriptor's record
  names; the console's are `con_cmd` and `con_fb`. Names must be unique
  per remote: an early version named both `con` and hm2 refused to load.
- **Throughput** scales with the servo rate: at a 2 kHz thread, ~2 kB/s.

## Troubleshooting

| symptom | check |
|---|---|
| `sserialterm.N.active` stays 0 | is the drive's firmware new enough (`con_fb` pin present in `halcmd show pin hm2`)? are the `net`s the right way round? |
| `active` 1, but no answer | `client` 1? Lines need a newline. A line over 63 characters is dropped. |
| answers stop after a LinuxCNC restart | the drive resets on the link timeout; send a command again |
| `--dump` incomplete | `-t` (quiet time) too short for the slow link: try `-t 2` |
| bytes-in counts, text garbled | two programs on one pty |

## Tests

| test | covers |
|---|---|
| `host/tests/test_sscon.c` (8) | the link layer: 1000 bytes each way, 10 % / 30 % frame loss, both ways at once, flow control, an over-long line, a restart |
| `host/tests/sserial_console.test` | the simulated drive's console from the host's LinuxCNC emulation (`con`, `conread`): `about`, pins, a whole `showconf` |
| `host/tests/console_link.test` (10) | an over-long line dropped, link loss in the middle of a reply and a reconnect (`lcncstop`, `conreset`), console traffic while the drive is enabled and moving, USB and console output shared |
| `host/tests/check_sserial_table.py` | the descriptor tables, field sizes and the 96-bit limit |
| `make -C hwtest/burnin-sim test-console` | LinuxCNC with the simulated drive and `sserialterm`; servoterm-cli over the pty: `about`, pins, `--dump` identical to the booted config, traffic while the axis moves, `--capture` |

All of this runs in simulation. In the LinuxCNC simulation the drive is the
`stmbl_sim` module, so hostmot2's decoding of the stream fields is not
exercised yet. The first bench test should check that `sserialterm` works
on the real `hm2_…con_fb` / `con_cmd` pins.
