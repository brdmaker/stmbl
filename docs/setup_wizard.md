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
- [Holding brake](#holding-brake)
- [Motor temperature sensor](#motor-temperature-sensor)
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

On real hardware the smart-serial console needs no LinuxCNC:
`tools/sscon/sscon.sh` runs the Mesa driver and sserialterm in a HAL
session and gives `/tmp/stmbl-con`. Put `console.sserial = 1` in the
session: every step's config then keeps `load sserial` (its console only),
so the resets after a config write don't take the console away. The script
raises the port's fault limit (the flash erase stops the drive's rt for up
to 0.5 s) and restarts the port when it stops. Use it when USB's console
is unreliable under motion (the SEW bench, DEBUGNOTES 2026-10-29).

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
| 5 | `motor` | nameplate: rated and peak current (A rms), rated torque, top speed; every one may be "unknown". A holding brake: its voltage, current, which output (DIO0–2), its engage time. The temperature sensor (none, KTY84-130, PT1000, PTC, NTC, thermal switch) with warning and trip temperatures. A vertical axis (gravity pulls) |
| 6 | `feedback` | incremental encoder, sin/cos, resolver (pole count), Tamagawa smartabs, Mitsubishi, Sanyo Denki, FANUC 4-wire serial absolute (αA64 type, multi-turn), FANUC 2-wire (βiA128) |
| 7 | `wiring` | power, motor, brake and feedback connector pinout (the tables in `docs/src/Getting Started.adoc`), then the user wires it. The brake directly on its output when it is 24 V DC and takes at most 1 A (with a flyback diode), else through a relay. The temperature sensor to the HV board's motor temperature connector. Where the temperature sensor is measured (`temp.input`): the HV board's input, or a feedback port's pin 7 (`fb1`; `fb0` with a resolver) after opening its solder jumper, see [motor_temp_lv.md](motor_temp_lv.md) |
| 8 | `base` | a first config (no command link), drive reset; checks the HV bus voltage, that the temperature sensor reads the room's temperature (a PTC or switch: not tripped), and that there is no fault |
| 9 | `fbcheck` | the shaft turned by hand: encoder resolution and counting direction (below). A brake is released for it (`fault0.brake_release`); on a vertical axis only once the load is off or supported |
| 10 | `gate` | before anything moves: motor uncoupled or the axis clear; how far it may turn |
| 11 | `electrical` | `id_pmsm`: resistance, inductance, pole pairs, phase order, commutation offset, flux; cross-checked against the nameplate |
| 12 | `encoder` | the feedback checked open loop: a d current turns the rotor one turn each way (at most the allowed travel), slowly, and the feedback must follow the field every cycle (below) |
| 13 | `mechanical` | the torque probe, then `id_mot` twice: inertia, friction, damping |
| 14 | `tuning` | `id_pid`: position and velocity loop gains |
| 15 | `brake` | the holding brake (below): released by the drive, holds, no sag at disable. Horizontal axes: automatic (the drive pushes against it, below); vertical: by hand |
| 16 | `limits` | current (peak, and continuous for the thermal model `iit0`), torque, speed (nameplate and bus voltage); the axis's top speed |
| 17 | `link` | LinuxCNC over smart-serial (with the command connector's pinout and the units per motor turn) or CAN, or none yet |
| 18 | `verify` | the final config: the drive enables, holds, makes ±0.5 rad steps; checks it moved and the position error is small |
| 19 | `absolute` | a multi-turn absolute encoder (FANUC) with the smart-serial link: the final config links `mturn` and the encoder's multi-turn template; this step checks that the drive took the encoder's turn count (`mturn0.state` 1), warns of a lost count or a battery warning, and gives LinuxCNC's settings ([absolute_encoders.md](absolute_encoders.md)). Other feedback: skipped |
| 20 | `save` | `stmbl-<time>.conf` and `stmbl-setup-report-<time>.txt` |

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
  - Both runs wait until the commutation is known (an incremental encoder
    phases at enable) and settled, and start where the motor stands.
  - The report gives each run's largest following error. **The runs used to
    lose the track:** a Yaskawa SGMAH did on the bench (10000 rad/s², a lot
    of current), and in the simulator every motor did in the gentle run.
    The fit uses measured torque and motion, so its result stayed right, but
    on a large motor or a fixed axis that is dangerous. The torque probe and
    what follows from it (below) keep them on track now (DEBUGNOTES
    2026-11-16). Still available, off: `idm`'s following-error limit
    (`max_err`) and S-curve moves (`max_jerk`, they bias the friction fit).
    `idm`'s adaptive inertia
    sets the loop's gain during the runs. Its change is limited to
    `j_rate` (1/s) times itself per second: it used to collapse within
    milliseconds at the second run's acceleration, which tripped the
    SGMAH.
  - **Before the runs, a torque probe** (`idjf`, `conf/template/id_jf.txt`):
    torque pulses with the encoder only watching, guarded by
    0.3 × the peak torque and 6 rad of travel. Its inertia, friction and
    offset seed `idm`. `idm`'s loops are stiff in proportion to its
    inertia, and its friction is fed forward. Started blind (a small
    inertia, no friction) they were soft against friction.
  - **With the probe's values both runs then use:**
    - stiffer loops (`pos_bw` 15, `vel_bw` 150);
    - the inertia held, not adapted;
    - the current loop's r and back-EMF feedforward.

    Without the feedforward the torque fell 20–35 % short while the speed
    changed, and only an inflated inertia made up for it (the adaptive
    estimate drifted 2–3× high).
  - **Results:**

    | | gentle run | fast run |
    |---|---|---|
    | simulator, summed largest errors over 32 motors: blind start | 100 rad | 35 rad |
    | the same, probe seeding only | 21 rad | 25 rad |
    | the same, all of the above | 3 rad | 16 rad |
    | SGMAH on the bench: blind start | 1.47 rad | 1.03 rad |
    | SGMAH: all of the above | 0.055 rad | 0.54 rad |

    In the simulator every fast run stays within 0.6 rad except on two
    motors where the probe's j is 1.6× / 2.1× off (1.5 / 3.0 rad). The
    probe's j is within 16 % on 32 of 35 motors and its friction within 8 %.
    On the SGMAH: j 1.27e-4 (id_mot 1.0e-4), friction 0.016 Nm.
  - **If the probe finds nothing usable**, `id_mot` starts blind as before:
    idm's soft adaptive loops, no feedforward. The probe's smallest pulse
    scale is 1 % of the peak torque on a small motor (at most idjf's
    0.01 Nm): on a 4e-7 kg m² rotor 0.01 Nm had turned it past the travel
    in one pulse.
- **The encoder check** (`encoder`, `conf/template/enc_check.txt`,
  `shared/comps/encchk.c`), before any loop closes on the feedback:
  - a d current (`id_pmsm`'s test current) turns the rotor like a stepper,
    0.5 turn/s, one turn each way (at most the allowed travel), through
    `dbrake0`, so the fault comp still switches it off;
  - every cycle the feedback, followed through the turns, is compared with
    the field (electrical); the rotor lags by its load angle, a few
    degrees. Lost or extra counts, a wrong resolution or pole pair count,
    the wrong direction or a glitch frame make the gap grow: above
    0.5 rad electrical the current goes off and the step fails;
  - it must count ±1 turn within 2 %.

  SGMAH on the bench: +1.001 / −0.999 turns, at most 10° electrical
  behind, no CRC error; with the direction set wrong it stopped after about
  5° of travel. Tests: `host/tests/drive_enc_check*.test` (sound, wrong
  resolution, wrong direction), run_wizard_sim.py (`enc_check`).
- **`id_pid`** tunes the loops on the measured motor.
- **Current feedforward:** from `id_pid` on (once `r`, `psi` and `j` are
  known) the config sets `conf0.cur_ff = 1` and `conf0.cur_ind = 1`. The
  current loop then feeds forward the resistive drop and the back-EMF
  instead of making its integrator follow every speed change. SGMAH,
  tuned loop, 200 rad/s at 10000 rad/s²: the torque reaches 87 % of the
  command in the ramps instead of 43 %, and the following error is
  0.48 rad instead of 0.56. In `id_mot` it is on only after the torque
  probe (above): with the blind start's soft loops it made the tracking
  worse (SGMAH fast run 1.8 rad instead of 1.0, most simulated motors
  lost the track).

## Holding brake

**Wiring and config.** The brake goes on DIO0, 1 or 2. Each output
switches to GND with a 24 V pin beside it; 1 A, 2 A absolute.
- **Directly** when it is 24 V DC and takes at most 1 A, with a flyback
  diode across the coil.
- **Through a relay** otherwise.

The config gets `io0.outN = fault0.mot_brake` (the output on, brake
released, while the drive is enabled) and `fault0.brake_time`.
`brake_time` is how long the drive keeps holding the axis after telling
the brake to grip, at a disable or after a stop, so a vertical axis doesn't
drop while the brake closes (docs/emergency_stop.md).

**The `brake` step** checks it on the drive:
1. **Released by the drive:** with the drive off, the drive releases the
   brake (`fault0.brake_release`), and the shaft must turn freely by hand.
   Moving the motor with the drive enabled proves nothing: its peak torque
   is often above the brake's.
2. **(Vertical axis) The load goes back on.** The measurements before ran
   without it: gravity would read as friction and offset, and the motor
   turns several turns.
3. **Holds with the drive off:** the user tries to turn the shaft. On a
   vertical axis it is also watched for 2 s.
4. **No sag at disable:**
   - The drive holds the axis, is disabled, and the position is watched.
   - `brake_time` starts at the datasheet's engage time × 1.5 + 20 ms
     (0.15 s if unknown).
   - If the axis moves more than 0.01 rad, it is raised (× 1.5 + 30 ms) and
     the test is repeated.
   - On a horizontal axis nothing pulls, so the datasheet value stays.

Positions are read from the feedback's own pin (`enc_fb0.pos`, ...):
`fb_switch0.pos_fb` stands still while the drive is disabled.


**Automatic test (horizontal axes).** No hand at the shaft: the drive holds
its position, the command steps 0.3 rad (released) or 0.5 rad (closed) away
and the torque limit (`pid0.max_torque`, `neg_min_torque`) rises in 40
steps, 0.1 s each, until the shaft moves 0.02 rad. Released, it must turn
below half the rated torque (friction); closed (the brake output forced off
while enabled), the torque it slips at, both ways, up to 2.5x the rated
torque (not the peak: a stalled motor at peak current heats fast). It must
hold at least the rated torque. The position loop stops the shaft at the
command, so a slipping brake can't let it run. The session gets
`brake.release_torque`, `brake.slip_pos`, `brake.slip_neg` (-1: no slip up
to `brake.test_torque`). `brake.manual = 1` asks for the hand checks
instead; a vertical axis always gets them (a slipping brake would drop the
load). On the SEW CMP40M (BP brake, 0.95 Nm): turns at 0.05 Nm released,
slips at 1.15..1.20 Nm closed.
## Motor temperature sensor

**In the drive.** The HV board measures the sensor's resistance
(`hv0.mot_temp`, ohms). `mtemp0` (`shared/comps/mtemp.c`, curves in
`shared/mtemp.h`) turns it into degC for `conf0.mot_temp_type`:

| type | sensor | reads |
|---|---|---|
| 0 | none (default) | the thermal model `iit0` alone, as before |
| 1 | KTY84-130 | degC (datasheet table: 581 ohm at 20 degC, 1000 at 100) |
| 2 | PT1000 | degC (IEC 60751) |
| 3 | PTC (DIN 44081) | tripped above `conf0.mot_temp_ptc_r` (1330 ohm) |
| 4 | NTC | degC (`conf0.mot_temp_ntc_r25`, `conf0.mot_temp_ntc_b`) |
| 5 | NC thermal switch | tripped when open |

- `fault0.mot_temp` is `mtemp0.temp`, the higher of the sensor and the
  model. Above `conf0.high_mot_temp` the current is derated, above
  `conf0.max_mot_temp` (5 s) the drive stops: `MOT_TEMP_ERROR` (7).
- A tripped PTC or switch, and an open or shorted wire (`mtemp0.err` 1
  / 2, after 0.1 s), read as the trip temperature + 10 degC: the drive
  stops.
- A sensor of another type often reads as a short or an open wire (a
  KTY84 is too low for a PT1000), otherwise as an implausible
  temperature.

**In the wizard.**
1. `motor`: the type (`motor.temp_sensor`), for an NTC its R25 and B, for
   the reading types the warning and trip temperatures (`temp.warn`,
   `temp.trip`, 110 / 130 degC).
2. `wiring`: the sensor's two wires to the HV board's 2-pin motor
   temperature connector. That input is on the HV side, at the DC bus'
   potential.
3. `base`: with the motor cold, the reading must be 5..45 degC (a PTC or
   switch: not tripped), and not open or shorted. Otherwise the step
   stops and says which. It runs before the fault check, because a bad
   sensor also faults the drive.
4. The config gets `conf0.mot_temp_type`, the NTC's data, and
   `conf0.high_mot_temp` / `max_mot_temp`.

**In the simulator.** `hv0.sim_temp` (winding degC), `sim_temp_sensor`
(the fitted type), `sim_temp_wire` (1 open, 2 short), `sim_temp_tnat` (a
PTC's or switch's trip temperature). Tests:
`host/tests/mot_temp_sensor.test` (every type, a mismatched type),
`mot_temp_fault.test` (an open wire and an overheated winding stop the
drive), and `run_wizard_sim.py` (a sensor on 3 of 4 motors, plus a wrong
type and an open wire, which have to stop the wizard at `base`).

**Measurement resolution** (from the F3's conversion in
`stm32f303/src/comps/io.c`, not checked on a bench): about 0.8 degC per
ADC count for a KTY84 at 20 degC, about 1 degC for a PT1000, better for
an NTC near 25 degC. Enough for a trip, not for a thermometer.

## Firmware version check

The wizard is built against one configuration interface: the template
names, the `conf0` pins, and the identification components' pins and
states. `shared/setup_api.h` numbers it (`STMBL_SETUP_API`), and the
drive prints it in `about` ("setup api 4"; 2 added the motor
temperature sensor, 3 the identification's following-error pins and its
wait for the commutation, 4 the encoder check (`enc_check`, `encchk`) and
the torque probe (`id_jf`, `idjf`)).

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
- With a holding brake on every third motor (1.5× rated torque, 40 ms),
  and a vertical axis on every sixth (a 30 % hanging load; its datasheet
  engage time is too short, 10 ms against 60): the brake holds, there is
  no sag at disable, and on the vertical axes the wizard raised
  `brake_time` above the brake's real delay.
- `hv0.sim_brake_delay` gives the simulated brake its delay, and a closed
  brake stops `hv0.sim_turn`.
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
- **`hv`, `out_rev = 1`:** the F4 flipped the q current and the angle
  for the F3, not the speed; the F3's current predictor then regulated the
  wrong current, short in proportion to the speed (a third of it at
  25 rad/s on one motor). Fixed in 2026-11.
- **`idm`, `ids`:** they moved while an incremental encoder was still
  phasing, and started from their own position rather than the motor's.
  They now wait for the commutation (`ready`, `settle`) and start where the
  motor stands.
- **`fault`:** `brake_time` (above), so a vertical axis doesn't drop
  while the brake closes.
- **`mtemp`:** the motor's temperature sensor (above). Before it, the
  HV board's sensor reading went nowhere: the over-temperature check saw
  only the thermal model.
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
