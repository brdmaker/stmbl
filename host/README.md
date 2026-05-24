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
  main.c            host runner: hw stubs, tick loop, console verbs, script + serve modes
  cdc_host.c        host implementation of the firmware CDC (USB serial) API -> stdio
  host_compat.h     newlib-ism shims (e.g. M_SQRT3), force-included by the Makefile
  shim/
    usbd_cdc_if.h   shadows inc/usbd_cdc_if.h so 'term' builds without the USB stack
  servoterm.c       headless QtServoterm-style client (spawns the sim, demuxes scope)
  Makefile          builds stmbl_host + servoterm, runs codegen from ../tools
  examples/         test assets (*.hal = deterministic console; *.servoterm = protocol + scope)
  gen/              generated tables/headers (git-ignored)
```

Two binaries are produced:

- **`stmbl_host`** — the simulator (the core).
- **`servoterm`** — the command-line client (talks to the core).

## Build

```sh
cd host
make            # builds ./stmbl_host and ./servoterm
make test       # runs every examples/*.hal and examples/*.servoterm
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

## Limitations

- Scope channels are 8-bit and therefore lossy — for exact numeric checks query
  pins directly; use scope for waveform shape.
- The build includes the V4 `SHARED_COMPS` set (the portable control components
  plus `term`); hardware components (`io4`, `hv`, ADC, encoders, sserial, ...)
  are not part of the host build.
- See `../DEBUGNOTES.md` for known issues (including why the ARM cross-build
  currently hangs).
