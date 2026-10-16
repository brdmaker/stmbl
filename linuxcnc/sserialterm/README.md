# sserialterm: stmbl drive consoles over smart-serial

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
  pins, `--dump` identical to the drive's config, and console traffic while
  the axis moves.
