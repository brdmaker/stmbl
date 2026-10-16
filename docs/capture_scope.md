# Capture scope (`scope0`)

The drive records up to 8 pins into its own memory, at the full loop rate,
around a trigger. The capture is downloaded afterwards over any console:
the slow smart-serial console ([sserial_console.md](sserial_console.md))
or USB.

It differs from the streaming scope (`term`, servoterm's `-s`), which sends
8 channels at 8 bits live over USB. This scope:
- captures first and sends afterwards, so it works over a ~1 kB/s link;
- keeps 32-bit values, downloaded at 16 bits over each channel's range;
- can wait unattended for an event (a fault) and hold what happened around
  it.

The `pid` and `mpid` templates load it: `scope0`, at rt and frt priority
15. It costs almost nothing until it is armed.

## Quick start

```
servoterm-cli -d /tmp/drive-x \
    -c "scope0.nch = 2" -c "scope0.ch0 = hv0.iq_fb" -c "scope0.ch1 = vel1.vel" \
    -c "scope0.trig = vel1.vel" -c "scope0.level = 50" -c "scope0.pre = 0.25" \
    --capture step.csv
```
servoterm-cli sets the pins, then:
1. arms the scope;
2. waits for the capture (`--capture-timeout`, default 60 s);
3. downloads it, checking every line's CRC;
4. writes `step.csv`.

The CSV has columns `t,vel1.vel,...`, with `t` in seconds and 0 at the
trigger sample. Move the axis meanwhile (MDI, jog); the speed crossing
50 rad/s upward triggers it.

By hand, on any console:
```
scope0.nch = 2
scope0.ch0 = hv0.iq_fb
...
scopearm
scopestat              -> scope: armed, 2 channels x 2048 samples, dt 0.0002 s
scopestat              -> scope: done, ...
scopedump
```

## Pins

| pin | default | meaning |
|---|---|---|
| `ch0` … `ch7` | — | the channels: link any pins. Only the first `nch` are recorded. |
| `nch` | 4 | channels, 1..8 (clamped). The memory holds 4096 samples in all: `depth = 4096 / nch` per channel. |
| `div` | 1 | a sample every `div` periods (≥ 1) |
| `fast` | 0 | 0: the rt loop (5 kHz, 0.2 ms); 1: the frt loop (20 kHz, 50 µs) |
| `trig` | — | the trigger source (link a pin) |
| `level` | 0.5 | the trigger level |
| `edge` | 0 | 0 rising, 1 falling, 2 either |
| `pre` | 0.1 | share of the capture before the trigger, 0..1 |
| `mode` | 0 | 0 single, 1 normal, 2 auto (see below) |
| `arm` | 0 | a rising edge (0 → >0) arms. Its level does nothing. |
| `state` | 0 | out: 0 idle, 1 armed, 2 triggered, 3 done |
| `depth` | — | out: samples per channel of the current capture |
| `dt` | — | out: seconds between samples (`period × div`) |

Settings are taken **at arming**. Changing `nch`, `div`, `pre` or the
period while armed takes effect at the next arm.

**Capture length** = `depth × dt`, for example:

| nch | fast | div | depth | dt | length |
|---|---|---|---|---|---|
| 1 | 0 | 1 | 4096 | 0.2 ms | 0.82 s |
| 2 | 0 | 1 | 2048 | 0.2 ms | 0.41 s |
| 4 | 0 | 1 | 1024 | 0.2 ms | 0.20 s |
| 2 | 0 | 10 | 2048 | 2 ms | 4.1 s |
| 1 | 1 | 1 | 4096 | 50 µs | 0.20 s |
| 8 | 1 | 2 | 512 | 0.1 ms | 51 ms |

## Triggering

- **Evaluation:** the trigger is evaluated on the recorded samples, every
  `div`-th period.
- **A crossing:**
  - rising: the previous sample is below `level` and this one is at or
    above it;
  - falling: the previous sample is above `level` and this one is at or
    below it;
  - either: both count.
- **The trigger sample** is the first sample past the crossing. In the dump
  it sits at index `pre`; servoterm-cli puts t = 0 there.
- **Pre-trigger fill:** before it can trigger, the scope records the
  pre-trigger share first (`pre × depth` samples). A crossing earlier than
  that is ignored, so the capture always has its full history.
- **Limits of `pre`:**
  - `pre = 0`: the trigger sample is the first sample.
  - `pre = 1`: the trigger sample is the last one, index `depth − 1`.
- **`scopetrig`** forces the trigger at the next sample, in any mode.

### Modes

| mode | after the trigger | without a trigger | after `scopedump` |
|---|---|---|---|
| 0 single | done, stays done | waits forever | stays done; a second dump repeats the capture |
| 1 normal | done | waits forever | re-armed: catches the next event |
| 2 auto | done | forced after one capture length of armed time | re-armed |

Auto mode in time: armed, the scope fills the pre-trigger part, then waits
one capture length (`depth` samples). Then it forces the trigger and
records the post-trigger part. With 2 channels at 5 kHz that is
0.1 + 0.41 + 0.31 = 0.82 s from arming to done.

### Unattended: catching the first fault

Put the setup into the drive's config, so it is armed from boot:
```
scope0.nch = 4
scope0.ch0 = hv0.iq_fb
scope0.ch1 = hv0.id_fb
scope0.ch2 = vel1.vel
scope0.ch3 = hv0.dc_volt
scope0.trig = fault0.state
scope0.level = 2.5
scope0.pre = 0.8
scope0.arm = 1
```
`fault0.state` goes to 3 (SOFT_FAULT) or 4 (HARD_FAULT), above 2.5. The
scope keeps the 0.16 s before the fault and 0.04 s after it, until someone
runs `servoterm-cli --capture fault.csv --no-arm` (which only waits and
downloads). The `arm` pin arms on its rising edge, which happens once at
config load.

## Commands

| command | answer |
|---|---|
| `scopearm` | `scope: armed` (armed at the next sample) |
| `scopetrig` | (none) the trigger is forced at the next sample |
| `scopestat` | `scope: <idle\|armed\|triggered\|done>, N channels x D samples, dt T s` |
| `scopedump` | the capture (below), or `scope: no capture (<state>)` |

## The dump format

```
scope begin nch=2 n=2048 dt=0.0002 pre=512
scope ch0 hv0.iq_fb min=-0.412 max=1.873
scope ch1 vel1.vel min=0 max=48.27
scope data <base64> <crc16>
...                                      (171 lines here)
scope end lines=171
```
- **`begin`:** channels, samples per channel, the interval, and the index
  of the trigger sample.
- **`chN`:** the channel's source pin (`comp<inst>.<pin>`, or `scope0.chN`
  when unlinked) and its range over the capture.
- **`data`:** the samples, oldest first, channels interleaved (time major).
  - Each sample is a little-endian u16: `q = round((v − min) / (max − min) × 65535)`,
    so the value is `v = min + q / 65535 × (max − min)`. A constant
    channel is all 0, so v = min.
  - 24 samples (48 bytes) per line, base64, then the CRC-16/CCITT-FALSE
    (poly 0x1021, init 0xFFFF) of the 48 raw bytes, in hex.
- **`end`:** the line count, so a reader can tell the dump is complete.

**Resolution:** 1/65535 of each channel's range. That is 3 × 10⁻⁵ of a ±1
signal; the test measures the decoded sine against the ideal one to within
10⁻³.

**Size:** 4096 samples are ~171 lines, ~13 kB. That is ~14 s over the
smart-serial console, under a second over USB.

## servoterm-cli `--capture FILE`

| option | |
|---|---|
| `--capture FILE` | after the `-c` commands: arm, wait for `done`, dump, check, write CSV |
| `--no-arm` | don't arm: wait for a capture armed elsewhere (unattended fault capture) |
| `--capture-timeout SEC` | how long to wait for `done` (default 60) |

servoterm-cli checks every line's CRC and the line count, and fails
otherwise. Its CSV has a header `t,<ch0 source>,...` and one row per sample.

## Memory and cost

- **Memory:** 4096 floats (16 kB) of RAM in the drive.
- **CPU:** a check per sample while armed (or a few multiplications per
  channel), and nothing while idle.
- **Priority:** at rt/frt priority 15 it records after the control
  components of the same period. A pin it reads shows that period's value.

## Tests

`host/tests/test_scope.py` (20 checks) decodes every capture the way
servoterm-cli does and checks the samples. It uses sim0's 50 Hz sine and
its square as known signals:
- **Format:** 2 × 2048 at 0.2 ms, 512 before the trigger, sources named,
  every CRC right.
- **Trigger edges:** rising, falling and either, each with the crossing at
  the trigger sample.
- **Timing and values:**
  - the time base (the sine measures 50 Hz) and the amplitude;
  - two channels in phase;
  - 16-bit quantisation against the ideal sine.
- **Settings:** `div = 4`; `pre = 0` and `pre = 1`; 8 channels, unlinked
  channels named and constant, `nch` clamped; `fast` with `div = 2`.
- **Modes and arming:**
  - single, normal and auto, including auto forced after a capture length
    and the re-arms;
  - arming on the `arm` pin's edge, not its level;
  - `scopedump` before done refused; `scopetrig`; `scopestat`.

Also:
- `host/tests/scope_capture.test`: a capture triggered by the motor's
  speed, a forced trigger in normal mode.
- `host/tests/scope_console.test`: a 4-channel dump over the smart-serial
  console.
- `make -C hwtest/burnin-sim test-console`: `servoterm-cli --capture` in
  LinuxCNC during a rapid (2048 rows, the speed crossing at t = 0).
