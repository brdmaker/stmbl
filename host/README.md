# stmbl host simulator

A native (x86) build of the stmbl control core, for running and testing the
control logic without STM32 hardware. It compiles the real HAL engine and
control components (`shared/hal.c`, `shared/comps/*.c`) with the host compiler
and replaces the STM32 low-level layer (timer ISRs, ADC, USB) with a software
tick loop and a stdio-based console.

Nothing here is firmware-specific to build: it needs only `gcc`, `make`, and
`python3` (the same python codegen the firmware uses).

## How it maps to the firmware

| On the device                                   | On the host                                  |
|-------------------------------------------------|----------------------------------------------|
| Control components in `shared/comps/*.c`        | Same files, compiled unchanged               |
| HAL engine `shared/hal.c`                       | Same file                                    |
| `hal_run_rt` @5 kHz, `hal_run_frt` @20 kHz ISRs | Software tick loop (`tick_rt_cycle`)         |
| `hal_run_nrt` @~1 kHz main loop                 | Interleaved during `run`/`step` (serve mode) |
| `term` over USB CDC (commands + scope)          | `term` over stdin/stdout via a CDC shim      |
| SysTick / watchdog                              | Stubs in `main.c`                            |

The component model is unchanged: components expose float "pins", are scheduled
by `rt_prio`/`frt_prio`, and are wired together with link commands — exactly as
on the drive.

## Layout

```
host/
  main.c            host runner: console verbs, script / serve / drive modes
  drive.c           the simulated drive: hw stubs, tick loop, flash config, boot/reset
  cdc_host.c        host implementation of the firmware CDC (USB serial) API -> stdio or FIFOs
  pty.c             the simulated drive's console pty and flash file (background thread)
  hw/               stand-ins for the drive hardware comps (hv, enc_fb, sserial, adc, io)
  sim_board.txt     template that links the simulated motor/encoder to conf0
  lcnc/             stmbl_sim, the simulated drive as a LinuxCNC realtime module
  host_compat.h     newlib-ism shims (e.g. M_SQRT3), force-included by the Makefile
  shim/
    usbd_cdc_if.h   shadows inc/usbd_cdc_if.h so 'term' builds without the USB stack
  servoterm.c       headless QtServoterm-style client (spawns the sim, demuxes scope)
  Makefile          builds stmbl_host + servoterm, runs codegen from ../tools
  examples/         test assets (*.hal = deterministic console; *.servoterm = protocol + scope;
                    drive_*.txt = scripts for a simulated drive, run with --drive)
  tests/            self-checking suite: *.test scripts with @expect checks, run_tests.py;
                    motors.py + gen_motor_tests.py (autotune), gen_soak_tests.py (soak)
  gen/              generated tables/headers (git-ignored)
```

Two binaries are produced:

- **`stmbl_host`** — the simulator (the core).
- **`servoterm`** — the command-line client (talks to the core).

## Build

```sh
cd host
make            # builds ./stmbl_host and ./servoterm
make check      # runs the test suite (tests/*.test), fails on any unmet check
make soak       # soak: every motor, id_pid-tuned, through a motion profile
make test       # make check, then every examples/*.hal, *.servoterm and drive_*.txt
make clean
```

## Running the core directly

### Deterministic script mode

```sh
./stmbl_host examples/pid.hal     # run a script
./stmbl_host                      # interactive, reads commands from stdin
```

Time advances only via the explicit `step`/`run` commands, so runs are exactly
reproducible. `term`/scope is **not** active in this mode — use it for unit-style
tests that query pin values directly.

### Serve mode (device wire protocol)

```sh
./stmbl_host --serve
```

Loads `term0` and speaks the same byte stream as the USB serial link: command
lines in on stdin, text responses plus binary scope frames out on stdout. Still
deterministic (time advances only on `run`/`step`). Normally you drive this
through `servoterm` rather than by hand.

## Console command language

Commands are plain text lines (handled by `hal_parse`). The main ones:

| Command                         | Effect                                              |
|---------------------------------|-----------------------------------------------------|
| `load <comp>`                   | instantiate a component (e.g. `load pid` -> `pid0`) |
| `<comp><n>.<pin>`               | print a pin (prefix match, e.g. `sim0.sin`)         |
| `<comp><n>.<pin> = <number>`    | set a pin                                           |
| `<comp><n>.<pin> = <c><n>.<pin>`| link a pin to another pin (sink = source)           |
| `<comp><n>.rt_prio = <p>`       | schedule the comp in the rt loop (p>0, lower first) |
| `<comp><n>.frt_prio = <p>`      | schedule the comp in the frt loop                   |
| `start` / `stop`                | start/stop the rt/frt system                        |
| `relink`                        | resolve chained links (a=b, b=c => a=c)             |
| `list` / `show` / `show_hal`    | inspect instances / available comps / wiring        |
| `help`                          | list all commands                                   |

Host-only verbs added by the simulator:

| Command       | Effect                                                  |
|---------------|---------------------------------------------------------|
| `step <n>`    | advance `n` rt cycles (rt = 0.2 ms)                     |
| `run <s>`     | advance `s` seconds of simulated time                   |
| `time`        | print simulated time                                    |
| `nrt`         | run one non-realtime pass                               |
| `exit`/`quit` | quit                                                    |

Typical setup order: `load` comps -> set `rt_prio`/`frt_prio` -> link and set
pins -> `start` -> `step`/`run` -> query pins.

## servoterm

A headless client modelled on [QtServoterm](https://github.com/STMBL/QtServoterm)
with no GUI. It spawns `stmbl_host --serve`, forwards commands, and splits the
reply stream into text (printed) and oscilloscope frames (decoded to CSV).

```sh
./servoterm [--sim PATH] [--scope FILE] [SCRIPT]
  --sim PATH     path to the simulator (default ./stmbl_host)
  --scope FILE   write decoded scope samples to FILE as CSV
  SCRIPT         file of commands to send; if omitted, reads stdin
```

Examples:

```sh
# capture a sine to CSV
./servoterm --scope sine.csv examples/scope.servoterm

# interactive session
./servoterm
```

### Scope protocol

The `term` component streams 8 channels. On the wire (identical to the device):

```
0xFF, ch0, ch1, ... ch7     one frame, each channel byte in 1..254
0xFE                        scope reset
```

Each channel byte is `(value + offset) * gain + 128` on the device side and is
decoded by servoterm as `(byte - 128) / 128.0`. Wire a channel and set its gain
before starting, e.g.:

```
term0.wave0 = sim0.sin      # channel 0 follows a pin
term0.gain0 = 100
term0.send_step = 50        # sample every 50 rt cycles (10 ms)
```

The CSV has a header `frame,ch0..ch7` and one row per frame.

## Writing test assets

- **`*.hal`** — deterministic console scripts run by `stmbl_host` directly. Best
  for exact assertions: set inputs, `step`/`run`, then query pins (exact text).
  See `examples/pid.hal`, `examples/signal.hal`.
- **`*.servoterm`** — scripts run through `servoterm` (serve mode). Best for
  protocol/scope captures. See `examples/scope.servoterm`.

Both are byte-for-byte reproducible, so captured output (and `--scope` CSVs) can
be compared against golden files in CI.

## Test suite (`tests/`)

`make check` runs every `tests/*.test` through `tests/run_tests.py` and prints
TAP (`ok N - ...` / `not ok N - ...`, then `# passed/total`). It exits non-zero
if any check fails. Run some of them with
`python3 tests/run_tests.py [-v] tests/rlpsij_*.test` (`-v` also shows the
values that passed).

A `.test` file is an ordinary `stmbl_host` script plus `@` directives:

```
# rlpsij state 5 stays bounded on a light rotor.
@args --drive ../hwtest/burnin/ECMA-C30604FS/stmbl.conf
@include tests/inc/rlpsij_hv0.txt        # shared wiring (tests/inc/ is not run)
rlpsij0.abs_pos_fb = hv0.sim_pos
relink
start
rlpsij0.en = 1
@sample hv0.sim_vel 0.001 14             # run 14 s, print the pin every 1 ms
rlpsij0.state
@expect rlpsij0.state == 6               # last printed value
@expect rlpsij0.r ~ 1.8 2%               # tolerance: N% or absolute
@expect hv0.sim_vel[1] > 0               # [k]: k-th printed value, [-1] last
@expect max|hv0.sim_vel| < 80            # also: max PIN, min PIN
```

Other directives: `@timeout SEC` (default 120) and `@reject REGEX` (fails if
any output line matches). Paths are relative to `host/`. A check fails if the
pin was never printed or is nan, and the test also checks that `stmbl_host`
exits cleanly. Peak checks need `@sample`: a coarse sample misses peaks (a
0.1 s trace showed 69 rad/s where the 1 ms one shows 127).

The drive tests `@include` the `examples/drive_*.txt` scripts, so their
documented expectations are checked rather than just printed.

`tests/motor_*.test` run the `rlpsij` autotune on the 35 motors in
`tests/motors.py`: 30 from `conf/*.txt` (r 0.4-25.5 ohm, j 1.5e-6-4e-4 kg*m^2,
polecount 1-5) and 5 synthetic corners (2 kW servo, 14 pole-pair torque motor,
spindle, micro servo, l/r = 200 ms) on the simulated drive. They check that it
finishes and finds r (3%), polecount, psi (5%), j (10%) and friction (15%), and
l (10%) where l / r >= 0.5 ms, with the speed kept under 80 rad/s. They are
generated: edit `tests/motors.py` and run `python3 tests/gen_motor_tests.py`.
Small j and l are read through a `scale` comp (x 1e6), because the console
prints 6 decimals.

`tests/drive_highspeed.test` runs a motor to 288 rad/s at 3000 rad/s^2 and
back (regression test for `vel2.g` in `conf/template/pid.txt`).

## Soak suite (`make soak`)

`make soak` runs every motor in `tests/motors.py` in closed loop on the
simulated drive, the way a machine runs: pid + pmsm + encoder, commanded
through the simulated LinuxCNC link (`lcnc`). For each motor,
`tests/gen_soak_tests.py` first tunes the position/velocity loop with the
drive's own `id_pid` and then writes a soak test that uses the tuned gains.
The tests are generated into `gen/soak` (they are ~60k lines each).

```sh
make soak                   # 1 cycle per motor, ~1 min
make soak SOAK_CYCLES=20    # longer
python3 tests/soak_report.py gen/soak/soak_<motor>.test   # where it went wrong
```

One cycle (~25 s simulated), scaled to the motor: top speed vmax = half the
voltage-limited speed (capped at 0.9 * max_vel), acceleration amax = 40% of
peak torque / j (capped at 3000 rad/s^2):

| Section | Motion |
|---|---|
| hold, creep | standstill; +-2% vmax |
| trapezoid | to +-vmax at amax, 1 s cruise, stop |
| medium | four 5 rad moves at vmax/2, amax/2 |
| reversals | 10 reversals between +-vmax/2 at amax |
| sines | 1, 5, 20 Hz position sines within vmax/2 and 0.8 amax |
| micro | 20 alternating 0.01 rad steps |
| vsteps | unramped velocity steps of +-10% vmax |
| cruise | 2 s at +-vmax |
| enable | disable/enable at standstill, a move, back to the start |

Checks, with the limits of `hwtest/soak`: following error under 0.5 mm in
motion and 0.02 mm at rest (0.314 / 0.0126 rad at 10 mm/rev), no drive fault
or `ERROR` line, drive enabled at the end, smart-serial link without errors,
speed within 1.2 x vmax, phase current within max_ac_cur.
`conf0.cur_ind = 1` is set: without back-EMF feedforward the current loop
loses control at high electrical speed (see `DEBUGNOTES.md`).

Three motors are known to fail (see `DEBUGNOTES.md`, "id_pid tunes some
motors too soft"): rx60_456 (id_pid tunes it soft), syn_torque_14p
(underdamped), sgmph_01a1 (stick-slip at rest). They are marked `@todo` in
the generator: TAP prints them as `not ok ... # TODO`, and they do not fail
the run. `tests/drive_id_pid.test` checks id_pid itself on the spindle.

## Simulated drive

`--drive` boots a whole simulated drive the way `src/main.c` boots the real
one: it loads `term`, then `flashloadconf`, `loadconf`, `relink` and `start`,
using a drive config file as the flash. The config's templates (`link pid`,
`link pmsm`, `link enc_fb0`, `link sserial`, ...) come from `conf/template/`,
so the drive's real config runs unchanged, with the same 63-character line
limit.

```sh
./stmbl_host --drive ../hwtest/burnin/ECMA-C30604FS/stmbl.conf examples/drive_jog.txt
./stmbl_host --drive CONF --pty /tmp/stmbl-sim     # real time, console on a pty
../servoterm-cli/servoterm-cli -d /tmp/stmbl-sim -c "fault0.state"
```

The hardware the templates link to is simulated by stand-ins in `hw/`. They
use the same component and pin names as `src/comps/`:

| Stand-in | Simulates |
|---|---|
| `hw/hv.c` | HV board (F3) and motor: the curpid current loop at 20 kHz and a PMSM model at 80 kHz. The `sim_*` pins hold the motor (r, l, psi, pole pairs, j, d, f, load, brake) and the supply voltage. |
| `hw/enc_fb.c` | quadrature encoder on the simulated shaft, with an index pulse: state 1 (incremental) until the index, then 3 |
| `hw/sserial.c` | smart-serial link to LinuxCNC; process data in `sim_io.h`, with timeout to `CMD error` as on the drive |
| `hw/adc.c`, `hw/io.c` | pins only |
| `src/comps/conf.c` | unchanged |

After the config, `sim_board.txt` (built in as the template `sim_board`)
links the simulated motor to `conf0`: `hv0.sim_r = conf0.r`, and so on. The
motor is then exactly what the config says; the encoder index sits where
`conf0.mot_fb_offset` puts it, and direction follows `conf0.mot_fb_rev` and
`conf0.out_rev`. To simulate a motor that differs from the config (for example
to test an autotune), override those pins with `--sim FILE`.

- **Without `--pty`:** the script drives the clock with `step` and `run`.
  `lcnc <enable> [vel_cmd [pos_cmd]]` plays LinuxCNC: every 1 ms it sends the
  held command and advances `pos_cmd` by `vel_cmd`. `lcnc` on its own prints
  the feedback.
- **With `--pty`:** the drive runs in real time, 1 ms per ms, and its USB
  console is the pty. `reset` reboots the simulated drive, and
  `flashsaveconf` writes back to CONF, so `servoterm-cli --load ... -c reset`
  works as on the drive. Nothing plays LinuxCNC, so the drive shows
  `CMD error`, as a real drive does without LinuxCNC.

## LinuxCNC module (`lcnc/`)

`stmbl_sim.so` is the simulated drive as a LinuxCNC (uspace) realtime
component. It runs in the servo thread, in lockstep with LinuxCNC: each
period it exchanges the smart-serial process data, then runs the drive for
that period (five 200 µs rt cycles at 1 kHz). Its pins mirror a real drive's
`hm2_<board>.0.stbl.0.0.*`: `pos_cmd`, `vel_cmd`, `pos_fb`, `vel_fb`,
`current`, `enable`, `fault`, `index_enable`, `out-00..03`, `in-00..03`, and
`scale`.

```sh
cd lcnc && make && sudo make install   # -> /usr/lib/linuxcnc/modules/
```

```
loadrt stmbl_sim conf=sim.conf sim=sim.txt tty=/tmp/stmbl-sim
addf stmbl-sim.0 servo-thread
```

`../hwtest/burnin-sim/` is a complete LinuxCNC config built on it. On the Pi 4
the whole servo thread (sim, motion and planner) takes about 41 µs per 1 ms,
with a worst case of 172 µs.

- **One drive per LinuxCNC:** the drive's HAL is a single global.
- **Symbol hiding:** the drive's HAL symbols (`hal_init`, `hal_start`, ...)
  share names with LinuxCNC's, so the Makefile links the drive into
  `stmbl_core.o` and makes every symbol local except the `stmbl_sim_*` API in
  `lcnc/stmbl_sim_api.h`.
- **No blocking in the servo thread:** the console runs over lock-free FIFOs,
  and a separate thread serves the pty and writes the flash file.

## Limitations

- Scope channels are 8-bit and therefore lossy — for exact numeric checks query
  pins directly; use scope for waveform shape.
- The build includes the V4 `SHARED_COMPS` set (the portable control components
  plus `term`). The hardware components (`io4`, `hv`, ADC, encoders, sserial,
  ...) are replaced by the stand-ins in `hw/`. These model an ideal encoder and
  have no PWM delay, dead time, current noise or HV faults; see
  `../DEBUGNOTES.md`.
- See `../DEBUGNOTES.md` for known issues (including why the ARM cross-build
  currently hangs).
