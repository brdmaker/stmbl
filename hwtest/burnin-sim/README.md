# hwtest/burnin-sim: the burn-in machine with a simulated drive

This is `../burnin/` with the Mesa 7c81 and the real STMBL drive replaced by
`stmbl_sim`, a LinuxCNC realtime component built from `host/lcnc/`. It runs
the drive's own control code (`shared/` and the `conf/template/` config
templates) against a simulated ECMA-C30604FS motor. It runs inside LinuxCNC's
servo thread, in lockstep with LinuxCNC.

You can use it to try a drive config, LinuxCNC settings, G-code or an
autotune before any of it touches the motor. No hardware is needed: the Pi
on its own is enough.

```
LinuxCNC (AXIS, motion) --pos_cmd/vel_cmd/enable--> stmbl-sim.0 --> drive firmware code
          ^                                              |          (pid, fb_switch, pmsm, ...)
          +-------------pos_fb/vel_fb/fault--------------+          simulated F3 + motor + encoder
                                                         |
                               /tmp/stmbl-sim (pty) <----+  drive's USB console: servoterm-cli
```

## Contents

- [Setup (once)](#setup-once)
- [Run it with LinuxCNC](#run-it-with-linuxcnc)
- [The drive console](#the-drive-console)
- [The drive config (simulated flash)](#the-drive-config-simulated-flash)
- [The simulated motor](#the-simulated-motor)
- [Trying an autotune](#trying-an-autotune)
- [Without LinuxCNC](#without-linuxcnc)
- [Back to the real drive](#back-to-the-real-drive)
- [Troubleshooting](#troubleshooting)
- [Differences from the real drive](#differences-from-the-real-drive)

## Setup (once)

```sh
cd ~/stmbl_brdmaker
make -C servoterm-cli                  # the console client
make -C host                           # stmbl_host, the simulator without LinuxCNC
make -C host/lcnc                      # stmbl_sim.so, the LinuxCNC module
sudo make -C host/lcnc install         # -> /usr/lib/linuxcnc/modules/stmbl_sim.so
```

Rebuild and reinstall the module after changing anything in `shared/`,
`conf/template/` or `host/`. A running LinuxCNC keeps the old module until you
restart it.

## Run it with LinuxCNC

```sh
linuxcnc ~/stmbl_brdmaker/hwtest/burnin-sim/stmbl.ini
```

In AXIS, as on the real machine:

1. **Estop off (F1), then machine on (F2).** The simulated drive phases the
   motor (about 0.5 s) and enables. Its console prints
   `INFO: Phasing` and `INFO: Enabled`.
2. **Home all.** No home switches are configured, so homing completes at once.
3. **Jog or MDI X.** Only X / joint 0 has a drive. Y and Z are in the config
   but have nothing connected. Limits, as in `../burnin/`: 500 mm/s
   (3000 rpm at 10 mm/rev) and 1000 mm/s².

Machine off disables the drive. A drive fault (for example a following error
over `conf0.max_pos_error`) reaches LinuxCNC through `stmbl-sim.0.fault` and
`joint.0.amp-fault-in`, as with the real drive. Toggle machine on to clear it.

The simulated drive's HAL pins, for halshow or halscope:

| Pin | Dir | What |
|---|---|---|
| `stmbl-sim.0.pos_cmd`, `vel_cmd` | in | position [mm] and velocity [mm/s] command |
| `stmbl-sim.0.pos_fb`, `vel_fb` | out | feedback, as the drive reports it |
| `stmbl-sim.0.current` | out | motor current `hv0.iq_fb` [A] |
| `stmbl-sim.0.enable` | in | drive enable |
| `stmbl-sim.0.fault` | out | drive fault |
| `stmbl-sim.0.index_enable` | io | index homing handshake (not wired in this config; `can.hal` homes on the index over CAN) |
| `stmbl-sim.0.out-00..03` | in | drive outputs `sserial0.out0..3` (`out-03` is the brake release) |
| `stmbl-sim.0.in-00..03` | out | drive inputs |
| `stmbl-sim.0.scale` | in | `sserial0.scale` (this config overrides it with `linrev0.scale = 10`) |

## The drive console

While LinuxCNC runs, the drive's USB console is the pty `/tmp/stmbl-sim`. It
takes the same commands and gives the same output as `/dev/ttyACM0` on the
real drive:

```sh
cd ~/stmbl_brdmaker/servoterm-cli
S="./servoterm-cli -d /tmp/stmbl-sim"

$S -c "fault0.state" -c "fault0.fault"      # 0 disabled, 1 enabled, 3 fault
$S -c "pid0.pos_error" -c "vel1.vel" -c "hv0.iq_fb"
$S -c "hv0.sim_vel" -c "hv0.sim_torque"     # the simulated motor itself
$S -c "fb_switch0.current_com_pos"          # 4 autophased, 1 absolute (after index)
$S -c hal                                   # HAL state and the rt component list
```

Scope logging works the same way. For example, to log while you jog in AXIS
(Ctrl-C stops it):

```sh
$S -C "pid0.pos_error:1000,vel1.vel:0.35,pid0.torque_cmd:250,hv0.iq_fb:20" \
   --step 5 -s run.csv --log 0 > run.txt
```

The console is free whenever no client has it open, so there's no need to
stop LinuxCNC to use it.

## The drive config (simulated flash)

`sim.conf` is the simulated drive's flash. The drive boots from it when
LinuxCNC starts, and again on `reset`. It started as a copy of the tuned
bench config `../burnin/ECMA-C30604FS/stmbl.conf`.

- **Edit it while LinuxCNC is stopped,** or write it over the console as with
  the real drive:
  ```sh
  $S --dump my.conf                   # what is in flash now
  $S --load my.conf -c reset          # write, verify, reboot the drive
  ```
  `--load` also saves the previous contents as `conf-backup-*.txt`, as on the
  real drive.
- **The real drive's config is never touched.** `flashsaveconf` writes
  `sim.conf` and nothing else.
- **To start over** from the tuned bench config:
  ```sh
  cp ../burnin/ECMA-C30604FS/stmbl.conf sim.conf
  ```
- **The 63-character line limit applies,** as on the drive: a line of 64
  characters or more silently ends the config there.

## The simulated motor

After `sim.conf`, the drive loads `host/sim_board.txt`. It links the
simulated motor and encoder to `conf0`, so by default the motor is exactly
what the config says. Then `sim.txt` (in this folder) is parsed, and anything
set there wins. Use it to simulate a motor that differs from what the drive
believes:

```sh
# sim.txt examples
hv0.sim_j = 0.00003      # datasheet inertia instead of conf0.j
hv0.sim_load = -0.2      # constant load torque [Nm], e.g. gravity
hv0.sim_brake = 1.3      # holding brake [Nm], engaged while io0.out0 = 0
hv0.sim_mains = 211      # mains [V rms]: the bench's 297 V bus
hv0.sim_chop_r = 0       # no brake chopper on the supply
```

| Pin | Default (from `sim_board.txt`) | What |
|---|---|---|
| `hv0.sim_r`, `sim_l` | `conf0.r`, `conf0.l` | phase resistance [Ω] and inductance [H] |
| `hv0.sim_psi`, `sim_pp` | `conf0.psi`, `conf0.polecount` | flux linkage [V·s/rad], pole pairs |
| `hv0.sim_j` | `conf0.j` | rotor inertia [kg·m²] |
| `hv0.sim_d`, `sim_f` | `conf0.d`, `conf0.f` | damping [Nm/(rad/s)], coulomb friction [Nm] |
| `hv0.sim_load` | 0 | external load torque [Nm] |
| `hv0.sim_brake` | 0 (no brake) | brake torque [Nm]; `sim_brake_release` follows `io0.out0` |
| `hv0.sim_supply` | 1 | DC bus: 1 rectified mains into the drive's own caps (no supply caps), 0 a stiff `sim_udc` |
| `hv0.sim_mains`, `sim_mains_hz` | 230, 50; 211 in `sim.txt` | mains [V rms, Hz]; 0 V: mains lost. The bus at rest is the peak less two diodes |
| `hv0.sim_mains_r` | 1 | mains, rectifier and wiring resistance [Ω] |
| `hv0.sim_cap` | 0.00054 | the drive's bus capacitance [F] (as `pe0.cap`) |
| `hv0.sim_bus_load` | 2 | constant load on the bus [W] (the HV board's own supply) |
| `hv0.sim_chop_r`, `sim_chop_on`, `sim_chop_off` | 47, 345, 335 | the supply card's brake chopper: the resistor [Ω] across the bus above `on` until below `off` [V]; 0 Ω: none |
| `hv0.sim_udc` | 300 | `sim_supply = 0` (and the stepper): the supply [V] |
| `hv0.sim_phase_rev` | `conf0.out_rev` | motor phase order |
| `hv0.sim_fb_delay` | 0 | extra delay of the current/voltage feedback [rt cycles, 0..4], like the F3 packet |
| `hv0.sim_cur_noise` | 0 | current measurement noise, uniform ± [A] |
| `enc_fb0.sim_rev` | `conf0.mot_fb_rev` | encoder count direction |
| `enc_fb0.sim_index` | `conf0.mot_fb_offset` | index position. A different value makes `conf0.mot_fb_offset` wrong for this motor. |

The encoder resolution is `enc_fb0.res`, which the config sets from
`conf0.mot_fb_res`. Bus state: `hv0.sim_vbus` (on the caps; `hv0.dc_volt`
is the F3's filtered measurement), `sim_ibus` (into the bridge, < 0
regenerating), `sim_irect`, `sim_chop_e` [J], `sim_hv_fault` (the F3's own
over-voltage trip at 410 V, kept). Read-only state: `hv0.sim_pos`, `sim_vel`, `sim_acc`,
`sim_torque`, `sim_id`, `sim_iq`.

You can also set these pins at runtime from the console, for example
`$S -c "hv0.sim_load = -0.3"`; a `reset` restores `sim.txt`. Like any
`sim.txt` line, each must stay under 64 characters.

## Trying an autotune

The `id_*` templates run on the simulated drive as on the real one, and
`stmbl_host` runs them much faster than real time. A 2½-minute `id_mot` run
takes about 5 s. Example: identify a motor whose inertia and friction differ
from the config:

```sh
cd ~/stmbl_brdmaker/host
cp ../hwtest/burnin/ECMA-C30604FS/stmbl.conf /tmp/id.conf
echo "link id_mot" >> /tmp/id.conf
printf 'hv0.sim_j = 0.00003\nhv0.sim_f = 0.02\n' > /tmp/id_sim.txt
printf 'fault0.cmd_error = 0\nfault0.en = 0\nrun 0.5\nfault0.en = 1\nrun 200\n' > /tmp/id.hal
./stmbl_host --drive /tmp/id.conf --sim /tmp/id_sim.txt /tmp/id.hal | grep "append"
```

```
conf0.j = 0.000072 # append to config
conf0.o = -0.000015 # append to config
conf0.d = 0.000109 # append to config
conf0.f = 0.019951 # append to config
```

`fault0.cmd_error = 0` and `fault0.en = 1` stand in for LinuxCNC, which isn't
running here. The friction, damping and offset results are right. The inertia
reads high by a roughly constant 0.000045 kg·m², because of the lag between
commanded and actual torque at `cur_bw` 1000. That is an `idm` issue the sim
uncovered (see `DEBUGNOTES.md`), not a sim error.

## Without LinuxCNC

`host/stmbl_host --drive` runs the same simulated drive on its own. See
`host/README.md` for details.

```sh
cd ~/stmbl_brdmaker/host
# scripted, as fast as the CPU allows; 'lcnc <en> <vel> [pos]' plays LinuxCNC
./stmbl_host --drive ../hwtest/burnin-sim/sim.conf --sim ../hwtest/burnin-sim/sim.txt examples/drive_jog.txt
# real time, console on a pty; shows CMD error, as a drive with no LinuxCNC does
./stmbl_host --drive ../hwtest/burnin-sim/sim.conf --pty /tmp/stmbl-sim
```

`make -C host test` runs `host/examples/drive_jog.txt` against the tuned bench
config as a regression test.

## Back to the real drive

The two configs differ only in the HAL file:

| | `../burnin/stmbl.hal` | `stmbl_sim.hal` |
|---|---|---|
| drive | `loadrt hostmot2`, `loadrt hm2_rpspi`, `hm2_7c81.0.read/write` | `loadrt stmbl_sim ...`, `addf stmbl-sim.0` |
| pins | `hm2_7c81.0.stbl.0.0.*` | `stmbl-sim.0.*` (same names) |
| estop | latched by the hostmot2 watchdog | follows the GUI |

A config that runs well here can go to the real drive with
`servoterm-cli --load sim.conf -c reset` on `/dev/ttyACM0`. Check it first:
`sim.txt` settings don't go with it, and real hardware has delays, noise and
limits that the sim doesn't (see below).

## Troubleshooting

| Symptom | Fix |
|---|---|
| `dlopen: .../stmbl_sim.so: cannot open shared object file` | The module isn't installed: `sudo make -C host/lcnc install` |
| `stmbl_sim: cannot start the drive` | `sim.conf` or `sim.txt` is missing, or the `TTY` directory isn't writable. Paths in `[STMBL_SIM]` are relative to this folder. |
| `servoterm-cli: cannot open /tmp/stmbl-sim` | LinuxCNC isn't running; the pty exists only while it does |
| Drive shows fault 1 (`CMD error`) | LinuxCNC isn't sending, or the machine was never switched on. Machine on clears it. |
| `LinuxCNC is still running` after a crash | `halrun -U` |
| Changes in `shared/` don't show up | Rebuild and reinstall the module, then restart LinuxCNC |
| Odd drive behaviour after experiments | `cp ../burnin/ECMA-C30604FS/stmbl.conf sim.conf` |

## Differences from the real drive

- **Ideal hardware:** the encoder counts exactly. The current loop (the F3's
  curpid at 20 kHz) has no PWM delay or dead time, and the HV board never
  faults. Current noise and feedback delay are off by default
  (`hv0.sim_cur_noise`, `hv0.sim_fb_delay`). Following error and current ripple are lower than on
  hardware; use the sim to check logic and setup, not to judge final tuning
  margins.
- **Command link:** the smart-serial exchange happens once per servo period,
  with the same one-period feedback delay as hm2. Bit timing and CRC errors
  are not simulated.
- **One drive per LinuxCNC instance:** the drive's HAL is a single global.
- **Template links:** links the real drive also fails on (`pe0.*`,
  `velbuf0`, `res0.*`, `hv0.dc_cur`) print `not found` here too.

Measured on the Pi 4 with LinuxCNC 2.9.1, 2026-10-14. AXIS was replaced by a
script that drove the `linuxcnc` Python module:

| Check | Result |
|---|---|
| Machine on | drive phases and enables, no fault |
| G0 ±100 mm, G1 X200 F30000 | 500 mm/s and 1000 mm/s² reached; max following error 0.39 mm (while accelerating), about 0.016 mm at rest. The rest offset is a `vel_int` effect, see `DEBUGNOTES.md`. |
| Encoder index | found; commutation switches from autophasing to absolute |
| `--load` + `reset` over the pty | 36 lines written and verified; the drive reboots with the new config |
| Servo-thread time | about 41 µs per 1 ms period (sim, motion and planner), worst case 172 µs |

## Power quotas (`make test-quota`)

The drive reports its input power over smart-serial (`stmbl-sim.0.power`;
on the real drive `hm2_7c81.0.stbl.0.0.power`). `powerquota.hal` (a second
HALFILE) feeds it to `linuxcnc/powerquota`, which slows the motion through
adaptive feed before the supply's fuse could blow. `quota.txt` is the
burn-in machine's supply (C 10 A, 230 V). `make test-quota` runs
`pq_test.py` headless with `quota_test.txt`, a 0.48 A quota that the
moves overload 3x, and `sim_quota_test.txt`, a load on the simulated motor
(about 300 W at 500 mm/s). With M52 P1 the moves are slowed (feed down to
0.55, fuse damage 0.34). Without it, the alarm estops the machine at
damage 0.9, and an estop reset clears it once the fuse model has cooled
a little. Needs `stmbl_sim.so` and `powerquota.so` installed (see
`linuxcnc/powerquota/README.md`).

## The console over smart-serial (`make test-console`)

The drive's console also runs inside its smart-serial process data
(`con_cmd` / `con_fb`, about 1 kB/s). `console.hal` (a second HALFILE)
connects it to `linuxcnc/sserialterm`, which serves it as a pty
(`[SSERIALTERM]TTY`). `servoterm-cli -d <that pty>` then works as on the
drive's USB port, with no USB cable. `make test-console` runs `con_test.py`
headless:
- `about`, pin reads and writes;
- `--dump` identical to the drive's config;
- console traffic while the axis moves.

## Over CAN (`make test-can`)

`can.hal` in place of `stmbl_sim.hal`: the drive links `can`
(`conf/template/can.txt`) instead of `sserial`. It sits on a simulated
CAN bus with a simulated MCP2515 (`stmbl_sim can=1`). LinuxCNC's end is
the real `stmblcan` component (`spi=sim`), as on a Raspberry Pi with
MCP2515s (`linuxcnc/stmblcan/README.md`). The console goes over CAN too
(`sserialterm` on the node's `con-cmd` / `con-fb`).

To run it by hand:
- set `HALFILE = can.hal`;
- add `[SSERIALTERM]TTY = /tmp/drive-con`;
- in `sim.conf`, change `link sserial` to `link can` and add
  `can0.vel_scale = 600`.

`vel-cmd` goes over the wire as 16 bits with that full scale. At the
default 100 the 500 mm/s rapids are clipped (`stmblcan.0.1.vel-clipped`
counts them) and the first one trips the following error.

`make test-can` runs `can_test.py` headless and checks:
- discovery at load;
- slot values (supply, temperature, power, current);
- joint 0 homed on the encoder index (`HOME_USE_INDEX`, index-only);
- rapids at 500 mm/s with FERROR 1;
- clean bus counters;
- `servoterm-cli` over the CAN console, also while the axis moves.

Needs `stmbl_sim.so`, `stmblcan.so` and `sserialterm.so` installed.
