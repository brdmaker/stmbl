# lathe: a lathe's turret and its stmbl spindle

A LinuxCNC realtime module (uspace), loaded by millweb's machine.hal for a lathe
(`loadrt lathe turret=1 spindle=1`). The logic is `lathe_logic.c`, tested on the host
(`make -C ../../host check`: test_lathe); `test/turret.hal` with `test/run_hal.py` runs the module
in halrun with a simulated turret.

```
make            # lathe.so (MODINC: LinuxCNC's Makefile.modinc)
make install    # into LinuxCNC's module directory
```

## lathe.turret

A stepper (step/dir: a stepgen in position mode) turns the turret; one index pulse a turret turn
gives its angle; it locks by running backwards onto its pawl. Positions are turret turns.

| pin | | |
|---|---|---|
| `change`, `tool` (s32), `changed` | in, in, out | iocontrol's tool-change, tool-prep-number, tool-changed |
| `index` | in | the index pulse (one a turn) |
| `pos-cmd`, `pos-fb` | out, in | the stepgen's position-cmd and position-fb (turns) |
| `enable`, `enable-out` | in, out | motion enabled; the stepgen's enable |
| `chain-in`, `chain-out` | in, out | the E-STOP chain through it: a fault opens it |
| `reset` | in | a rising edge clears a fault (and unhomes) |
| `homed`, `busy`, `fault`, `station`, `state`, `error` | out | its state; `error` 1 no index, 2 slip, 3 timeout |
| `stations`, `offset`, `overtravel`, `lock`, `speed`, `lock-speed`, `acc`, `dwell`, `timeout`, `slip` | in | its set-up |
| `sim-index`, `sim-index-at` | out, in | a simulated index (from `pos-fb`) |

A change: unhomed, forward up to 1.5 turns to the index; forward to the station's angle plus
`overtravel`; back by `overtravel + lock` at `lock-speed`; `dwell`; `changed` until `change` drops.

## lathe.spindle

An stmbl drive in velocity mode (the drive's `pid0.pos_en = 0`, as `conf/haas_spindle.txt`).

| pin | | |
|---|---|---|
| `speed-rps`, `on` | in | spindle.0.speed-out-rps, spindle.0.on |
| `vel-cmd`, `pos-cmd` | out | to the drive: motor rad/s, and the integrated angle (wrapped ±π) |
| `pos-fb`, `vel-fb` | in | from the drive: motor rad (wrapped or not), rad/s |
| `revs`, `speed-fb` | out | spindle.0.revs (turns since the index), spindle.0.speed-in (turns/s) |
| `index-enable` | io | spindle.0.index-enable: cleared at the index |
| `index-sensor`, `use-sensor` | in | a once-a-turn sensor; without it, the motor's zero angle (right only when `ratio` is whole) |
| `at-speed`, `at-speed-tol` | out, in | spindle.0.at-speed: within the tolerance (a fraction) |
| `ratio` | in | motor turns a spindle turn |
