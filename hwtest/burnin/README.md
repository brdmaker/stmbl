# hwtest/burnin: LinuxCNC test machine for STMBL

LinuxCNC config for a burn-in / test bench: one STMBL drive on a Mesa 7c81,
reached through the Raspberry Pi's SPI. Copied from `docs/linuxcnc` in the
upstream stmbl checkout (`/home/cnc/stmbl`), including the local changes made
there. Changed since: `stmbl.ini` joint 0 / X axis limits raised to 500 mm/s
(3000 rpm motor speed) and 1000 mm/s^2, and the drive config for the motor
is in `ECMA-C30604FS/`.

| File | What |
|---|---|
| `stmbl.ini` | machine config: 3 joints, trivkins, 1 ms servo thread; X/joint 0 at 500 mm/s |
| `stmbl.hal` | HAL: hostmot2 via `hm2_rpspi`, joint 0 wired to `stbl.0.0` (STMBL over smart-serial) |
| `stmbl.org` | earlier `stmbl.hal`, without `debug_pin_descriptors=1` |
| `stmbl.var`, `stmbl.var.bak` | RS274NGC parameter file (LinuxCNC rewrites it at runtime) |
| `tool.tbl`, `halshow.preferences` | tool table, halshow UI state |

Notes:
- Only joint 0 is connected to a drive; joints 1 and 2 have no HAL nets.
- `[HOSTMOT2] DRIVER=hm2_rpspi"` has a stray quote. It is harmless, because
  `stmbl.hal` loads `hm2_rpspi` by name and never reads `DRIVER`.
- hostmot2 watchdog: 50 ms (`watchdog.timeout_ns 50000000`). It feeds the
  estop latch.

## Realtime setup (checked 2026-10-14)

| Item | Value |
|---|---|
| Board | Raspberry Pi 4 Model B Rev 1.4, 4 cores |
| Kernel | 6.1.54-rt15, PREEMPT_RT (`/sys/kernel/realtime` = 1) |
| LinuxCNC | 2.9.1, `linuxcnc-uspace` |
| CPU isolation | `isolcpus=2,3` (cores 2-3 reserved for realtime) |
| CPU frequency | `performance` governor, fixed at 1.8 GHz on all cores |
| RT throttling | 950000/1000000 us (kernel default) |
| Servo thread | 1 ms (`[EMCMOT] SERVO_PERIOD = 1000000`), no base thread |
| `processor.max_cstate=1` | on the kernel command line, but this is an x86 option and has no effect on the Pi |

Servo-thread latency, 65 s, idle machine (LinuxCNC not running, no load):

```
latency-histogram --nox --nobase --servo 1000000
servo min: -67.1 us  max: 68.6 us  sdev: 2.0 us
```

The worst case is about 69 us, roughly 7% of the 1 ms period. It appeared
once, about 9 s into the run, and the maximum did not change for the rest
of it. This was a short idle test only. A real burn-in check should run
for hours with load on the machine: GUI open, a G-code preview, network
and disk activity, for example `stress-ng` on the non-isolated cores 0-1.

To repeat it, with LinuxCNC stopped (`--nox` updates one status line, so
run it in a terminal):

```sh
latency-histogram --nox --nobase --servo 1000000 --sbinsize 1000 --sbins 500
halrun -U    # afterwards, if it was interrupted
```
