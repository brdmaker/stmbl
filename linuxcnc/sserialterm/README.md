# sserialterm: stmbl drive consoles over smart-serial

Full documentation: [docs/sserial_console.md](../../docs/sserial_console.md)
(protocol, frame, setup, limits, troubleshooting) and
[docs/capture_scope.md](../../docs/capture_scope.md) (the capture scope).

Drives in tight places don't need a USB cable for setup. The drive's
console (`show`, `about`, pin reads and writes, `showconf`,
`flashsaveconf`, ...) runs inside its smart-serial process data, and
`sserialterm` turns it into a pseudo-terminal on the LinuxCNC machine.
`servoterm-cli` (or any terminal program) then talks to the drive as over
USB, while LinuxCNC runs, in estop or while machining.

```
loadrt sserialterm tty=/tmp/drive-x,/tmp/drive-y
addf sserialterm.0 servo-thread
net x-con-fb  hm2_7i96s.0.stbl.0.0.con_fb => sserialterm.0.con-fb
net x-con-cmd sserialterm.0.con-cmd       => hm2_7i96s.0.stbl.0.0.con_cmd
```
```
servoterm-cli -d /tmp/drive-x -c about
servoterm-cli -d /tmp/drive-x --dump drive-x.conf
```

How it works (`shared/sscon.h`):
- **The fields:** `con_cmd` (to the drive) and `con_fb` (from it), a
  10-bit field each way: a byte, the sender's toggle and an ack. hostmot2
  shows them as raw u32 pins (smart-serial stream type).
- **Resends:** alternating-bit protocol both ways. A lost or repeated
  frame (CRC error, a skipped channel) delays a byte; it is never lost or
  doubled. A full receiver doesn't ack, which is flow control.
- **Speed:** one byte per servo period each way, about 1 kB/s at 1 kHz. A
  1 kB config `--dump` takes about 3 s.
- **When the drive talks:** output goes over the link only once LinuxCNC's
  side has sent something, so nothing piles up while nobody listens. The
  drive's link state resets when the smart-serial link times out (LinuxCNC
  restart).

The frame cost: 10 bits each way. The drive's input data is 96 bits, the
limit of older Mesa smart-serial bitfiles.

## Capture scope

`scope0` (`shared/comps/scope.c`, loaded by the pid / mpid templates and
idle until armed) records up to 8 pins into the drive's own memory, 4096
samples shared between the channels. It records at the 5 kHz loop, or the
20 kHz one with `scope0.fast = 1`, every `div`-th period. Arm it, trigger
it, then download over the slow console:
```
servoterm-cli -d /tmp/drive-x -c "scope0.nch = 2" -c "scope0.ch0 = hv0.iq_fb" \
    -c "scope0.ch1 = vel1.vel" -c "scope0.trig = vel1.vel" -c "scope0.level = 50" \
    -c "scope0.pre = 0.25" --capture step.csv
```
- **Trigger:** source pin, `level`, `edge` (0 rising, 1 falling, 2 either),
  `pre` (share before the trigger).
- **Modes:** `mode` 0 single, 1 normal (re-armed after each dump), 2 auto
  (as normal, and forced after a capture length without a trigger).
- **Commands:** `scopearm`, `scopetrig` (force), `scopestat`, `scopedump`.
  The dump holds each channel's source and range, then the samples as 16
  bit in base64 lines with a CRC-16 each.
- **`servoterm-cli --capture FILE`:** arms (unless `--no-arm`), waits,
  downloads, checks the CRCs and writes CSV: `t` (0 at the trigger) and a
  column per channel, named after its source.
- **Speed:** 2 × 2048 samples take about 14 s over smart-serial (USB works
  the same way, faster).
- **Unattended capture:** armed in the drive's config (`scope0.arm = 1`,
  e.g. triggered by `fault0.state`), the scope holds what happened around
  the first event until someone downloads it.

Pins per drive N (in `tty=` order):
- `con-fb` (u32 in), `con-cmd` (u32 out);
- `bytes-in`, `bytes-out` (u32): bytes delivered each way;
- `active` (the drive has answered), `client` (a program has the pty open).

A pty with nobody attached drops what the drive says.

Tests:
- `make -C ../../host check`: `test_sscon` (lost and repeated frames both
  ways, flow control, restart) and `sserial_console.test` (the simulated
  drive's console from the host's LinuxCNC emulation, a whole `showconf`).
- `make -C ../../hwtest/burnin-sim test-console`: LinuxCNC with the
  simulated drive and this component; servoterm-cli over the pty: `about`,
  pins, `--dump` identical to the drive's config, console traffic while the
  axis moves, and a `--capture` of a rapid (trigger at t = 0, CRCs).
- `scope_capture.test` and `scope_console.test` (host): triggered, forced
  and normal-mode captures, and a dump over the console.
- `console_link.test` (host): an over-long line, link loss mid-reply and a
  reconnect, traffic while enabled and moving.
- `test_scope.py` (host): every scope setting, checked on the decoded
  samples (edges, div, pre 0 / 1, modes, the arm pin, 8 channels, fast).
