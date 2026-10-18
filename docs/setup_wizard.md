# Setup wizard

Setting up an stmbl drive by hand means knowing which templates to link,
which `conf0` values matter, the order of `id_pmsm`, `id_mot` and `id_pid`,
and how to check the result. The setup wizard does that step by step:

```sh
servoterm-cli -d /dev/ttyACM0 --wizard
```

It asks only what a machine builder can read off the motor's nameplate
and measures everything else on the drive. There is no motor database. It
also gives wiring help, writes the config, checks it, and saves a copy
and a report. It is a module of servoterm-cli (`servoterm-cli/setup/`),
written as a library, so a GUI can be built on the same functions later.

## Contents

- [Running it](#running-it)
- [The steps](#the-steps)
- [What it measures, and how](#what-it-measures-and-how)
- [Firmware version check](#firmware-version-check)
- [Session, resume, answers file](#session-resume-answers-file)
- [The library (for a GUI)](#the-library-for-a-gui)
- [Testing in the simulator](#testing-in-the-simulator)
- [Firmware changes it needed](#firmware-changes-it-needed)
- [Limits and open points](#limits-and-open-points)

## Running it

```sh
make -C servoterm-cli
servoterm-cli/servoterm-cli -d /dev/ttyACM0 --wizard           # USB
servoterm-cli/servoterm-cli -d /tmp/drive-x --wizard           # smart-serial console (sserialterm)
```

| Option | Default | What |
|---|---|---|
| `--session FILE` | `stmbl-setup-session.txt` | everything asked and measured; run again to resume |
| `--answers FILE` | (none) | answers from a file instead of the terminal (below) |
| `--redo STEP` | | forget STEP and the steps after it, then run them again |
| `--wizard-step STEP` | | run only STEP |
| `--out DIR` | `.` | where the backup, the final config and the report go |
| `--time-scale N` | 1 | the simulator's `--speed N`: shortens the wizard's waits |
| `-v` | | show the drive's own text |

Answer a question with its number or id. `?` means unknown (where that's
allowed), Enter takes the default, and `q` stops; the session is kept.

## The steps

| # | Step | What happens |
|---|---|---|
| 1 | `version` | `about`: the firmware's setup API must match the wizard's (below) |
| 2 | `backup` | the drive's config to `stmbl-backup-<time>.txt` (`--load` it to go back) |
| 3 | `safety` | what will move; e-stop within reach, motor fastened |
| 4 | `supply` | 230 V / 120 V mains or DC; a brake chopper and its threshold; the braking power the drive may feed back (`conf0.max_hv_power`) |
| 5 | `motor` | nameplate: rated and peak current (A rms), rated torque, top speed, holding brake; every one may be "unknown" |
| 6 | `feedback` | incremental encoder, sin/cos, resolver (pole count), Tamagawa smartabs, Mitsubishi, Sanyo Denki |
| 7 | `wiring` | power, motor, brake and feedback connector pinout (the tables in `docs/src/Getting Started.adoc`), then the user wires it |
| 8 | `base` | a first config (no command link), drive reset; checks the HV bus voltage and that there is no fault |
| 9 | `fbcheck` | the shaft turned by hand: encoder resolution and counting direction (below) |
| 10 | `gate` | before anything moves: motor uncoupled or the axis clear; how far it may turn |
| 11 | `electrical` | `id_pmsm`: resistance, inductance, pole pairs, phase order, commutation offset, flux; cross-checked against the nameplate |
| 12 | `mechanical` | `id_mot` twice: inertia, friction, damping |
| 13 | `tuning` | `id_pid`: position and velocity loop gains |
| 14 | `limits` | current (peak, and continuous for the thermal model `iit0`), torque, speed (nameplate and bus voltage); the axis's top speed |
| 15 | `link` | LinuxCNC over smart-serial (with the command connector's pinout and the units per motor turn) or CAN, or none yet |
| 16 | `verify` | the final config: the drive enables, holds, makes ±0.5 rad steps; checks it moved and the position error is small |
| 17 | `save` | `stmbl-<time>.conf` and `stmbl-setup-report-<time>.txt` |

The config it writes:
- `link pid`, `link pmsm`, the feedback's template, and in the end the
  command link.
- The measured `conf0` values, each tagged with its source
  (`# id_pmsm`, `# setup: measured`, ...).
- The limits: `conf0.max_ac_cur` is the peak current's amplitude, √2 ×
  the nameplate's rms, capped at the HV board's 25 A.
  `iit0.cur_boost` = peak / rated.
- With a chopper, `conf0.high_dc_volt` / `max_dc_volt` are set above it
  (docs/emergency_stop.md).

## What it measures, and how

**Encoder resolution, by hand.** For an incremental encoder the drive's
counter is set to wrap at 65536 (`enc_fb0.res`), and the encoder prints
the counter at every index pulse (`enc_fb0.indexprint`, `cnt = N`). The
user turns the shaft three turns in the direction they call forward.
Consecutive index counts then differ by exactly the counts per
revolution, and the sign of the difference is the counting direction
(`conf0.mot_fb_rev`). Nothing has to be known about the encoder. Without
an index pulse the wizard asks for the line count and takes the direction
from a quarter turn.

**The motor, on the drive.**
- **`id_pmsm` measures the electrical values:**
  - R and L with a d-axis current. The test current is half the rated
    current, between 0.2 and 8 A.
  - The pole pairs and the phase order with a rotating field.
  - The commutation offset with the field held still.
  - The flux (psi) with the motor turning on its own.
- **The nameplate cross-check:** the torque constant 1.5·pp·psi·√2 is
  compared with rated torque / rated current. Off by more than 2× is an
  error: a wrong pole count or encoder resolution. More than 30 % is a
  warning.
- **`id_mot` runs twice:**
  - First at its safe defaults (250 rad/s², 25 rad/s).
  - Then at the acceleration that takes 30 % of the peak torque for the
    first run's inertia, and faster.
  - On light motors with much friction the first run reads the inertia
    up to 4× too high (the friction's sign changes often at low speed,
    and the fit puts the misfit into j); the second run gets within
    ~20–30 %.
- **`id_pid`** tunes the loops on the measured motor.

## Firmware version check

The wizard is built against one configuration interface: the template
names, the `conf0` pins, and the identification components' pins and
states. `shared/setup_api.h` numbers it (`STMBL_SETUP_API`), and the
drive prints it in `about` ("setup api 1").

| Drive | Wizard |
|---|---|
| no "setup api" line | refuses: the firmware is older than the wizard; flash it (`make btburn`) after a config backup |
| another number | refuses: flash the firmware from the same source, or use the wizard that came with it |
| same number, other git version | warns and goes on |

Bump `STMBL_SETUP_API` when a template, pin or state the wizard uses
changes incompatibly.

## Session, resume, answers file

**The session file:** every answer and measurement is a `key = value`
line (`supply.bus = 325`, `id.r = 2.155`, `done.electrical = 1`). It is
saved after every step. Run the wizard again and it resumes at the first
unfinished step. The version is checked again every time, since it may be
another drive now.

**An answers file** uses the same keys:
```
supply.kind = 230
supply.chopper = y
motor.rated_cur = 2.6
feedback.type = enc
gate.turns = 2
fbcheck.turn = send hv0.sim_turn = 19; wait 4
```
- **What answers what:**
  - Choices take the option's id (`enc`, `sserial`) or number.
  - Numbers take a number or `?`; yes/no questions take `y` or `n`.
  - A hand action takes `done`, or what the simulator should do:
    `send` a console line, `wait` seconds.
- **Questions it doesn't answer:** the terminal is asked if there is one;
  otherwise the default is used, and with no default the wizard stops.
- **Uses:** setting up identical machines, and the simulator tests.

## The library (for a GUI)

`servoterm-cli/setup/`, plain C, no globals; each part is its own module:

| Module | What |
|---|---|
| `su_ui.h` | the UI as callbacks: `say`, `choose` (ids + names), `number` (with "unknown"), `confirm`, `action` (a hand action, with the drive link), `progress`. Every question has a key. |
| `su_state.[ch]` | the session: key/value store, load/save/parse |
| `su_link.[ch]` | the drive's console: open, send, query, `get` (`getf`: full precision), `set`, `reset` and reconnect; scope frames dropped |
| `su_conf.[ch]` | config text: normalize, validate (the drive's 52-char lines, 15 KiB), set/get/unset pins, link templates; read/write the drive's flash with verify |
| `su_version.[ch]` | parse `about`, check against the wizard's API and git version |
| `su_motor.[ch]` | nameplate + identified values → limits; plausibility and the torque-constant cross-check |
| `su_feedback.[ch]` | feedback types (template, position pin, how the resolution is found); index counts → counts per revolution and direction |
| `su_wiring.[ch]` | connector tables as text |
| `su_steps.[ch]` | the steps and the engine: `su_wizard_run`, `su_wizard_step`, `su_wizard_redo`, `su_build_config` (the config for any state, without a drive) |
| `su_cli_ui.[ch]` | the terminal and answers-file front-end (servoterm-cli only) |

A GUI:
1. Implements `su_ui_t`. Dialogs, a form keyed by the question keys, a
   wiring picture for `SU_MSG_WIRING`.
2. Opens a `su_link_t`.
3. Runs `su_wizard_run`, or single steps with a step list beside them.

## Testing in the simulator

**What the simulator gained:**
- `stmbl_host --pty LINK --speed N` runs the drive N× faster than real
  time.
- `hv0.sim_turn` is a hand on the shaft: it turns it by that angle at
  1 rev/s.
- `enc_fb0.sim_res` gives the encoder its own resolution, wrapped by the
  drive's counter. `enc_fb0.indexprint` prints the index counts as on the
  drive.
- A `--sim` file fixes the true motor. It doesn't follow `conf0`, so the
  identification really has to find it.

```sh
make -C servoterm-cli check     # the library's unit tests (67)
make -C host wizard-smoke       # the bench motor through the whole wizard (also in make -C host check)
make -C host wizard             # every motor in tests/motors.py (35), ~8 min on 4 cores
```

**What `wizard` checks:**
- Every motor goes from a blank drive to the saved config, each with an
  encoder variant: 500 to 5000 lines, either counting direction, either
  phase order, the index anywhere.
- Exact: the counts, the encoder direction, the phase order and the pole
  pairs.
- Within a few percent: R, L and psi.
- The commutation offset to the index's electrical angle.
- j and f within 25 %, and the verify step.
- Measurement limits it knows about are marked TODO in
  `tests/run_wizard_sim.py`:
  - three light motors with heavy friction, whose inertia stays 1.3–2.4×
    high;
  - two motors with L/R of 40–55 µs, whose L reads high.

## Firmware changes it needed

Running all 35 motors through the identification found problems in the
firmware's own tools (DEBUGNOTES):
- **`idpmsm`, pole pairs:** the rotating field started at full speed; a
  heavy rotor swung instead of following, and the pole pairs came out
  wrong on 5 motors. The field now ramps up over 1 s, and pp = the field's
  travel / the rotor's travel.
- **`idpmsm`, flux:** its speed loop, a slow integrator, overshot to the
  voltage limit: 700 rad/s on a light motor. It is now bounded: current
  until `test_vel`, none until 70 % of it. psi comes from the coasting
  parts, as the voltage's integral over the rotor's travel.
- **`id_pmsm` template:** `hv0.vel = 0`. The F3 advanced the angle by the
  normal commutation velocity, which scales with a pole count that is
  still a guess there.
- **`idm`:** its inertia floor was 5e-6; a 4e-7 motor got gains 500× too
  high and saturated. Now 1e-7, and the wizard starts the adaptive inertia
  low.
- **The console:**
  - `getf <pin>` prints a pin in full precision; `%f` printed a micro
    motor's inertia as 0.000000.
  - `about` prints the setup API.

## Limits and open points

- **Not run on hardware yet.** Everything above ran against the
  simulator.
- **PMSM only.** DC, induction and stepper motors are not in the wizard
  yet.
- **Feedback types:** only the incremental encoder is simulated and
  tested end to end. The others link their templates and check the
  direction by hand, untested.
- **Inertia:** up to 2.4× high on light motors with much friction
  (idm). The loops still tune and verify.
- **Inductance:** inductance on motors with L/R below ~50 µs reads high
  (idpmsm's L step).
- **CAN wiring:** the wizard points to docs/can_support.md; v4 has no CAN
  connector.
- **Linear motors, gantries, absolute homing:** not covered yet.
