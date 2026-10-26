# Motion control with stmbl and LinuxCNC: tuning, compensation, testing

How to get accuracy at speed out of stmbl drives under LinuxCNC. It covers two drive settings that fix a velocity estimation lag and a feedforward gap, `pcomp` (model-based command conditioning in LinuxCNC), measuring the machine's model, and testing all of it in simulation. The numbers come from the simulated portal (`host/portal`, `hwtest/portal-sim`): a 1 × 1 m gantry on Baldor linear motors with a flexible tool head.

## Contents

- [What limits accuracy at speed](#what-limits-accuracy-at-speed)
- [Drive settings](#drive-settings)
- [Tuning procedure](#tuning-procedure)
- [pcomp: model-based command conditioning](#pcomp-model-based-command-conditioning)
- [Gantry-aware control](#gantry-aware-control)
- [Measuring the machine](#measuring-the-machine)
- [Testing in simulation](#testing-in-simulation)
- [Results](#results)
- [Pitfalls](#pitfalls)

## What limits accuracy at speed

Most of the error at speed is a position error proportional to acceleration. It comes from four sources.

**The drive's velocity estimate lags under acceleration.** The observer `vel1`, which feeds the speed loop, reports 2·d·a/w too little while accelerating: 1.8 ms × a with the defaults w 1000, d 0.9.
- In the speed loop this works as positive acceleration feedback. The configured `vel_bw` isn't the real bandwidth: 300 behaves like about 650.
- Every configuration becomes unstable at vel_bw = w/(2d) ≈ 556.

**No acceleration feedforward over smart-serial, and a late velocity.**
- LinuxCNC's `joint.N.vel-cmd` is the average over the last servo period, so it is half a period late.
- The sserial template has no acceleration feedforward: `vel0` is off, and when on it gives a 50 Hz-filtered derivative.

**The structure.** LinuxCNC's planner switches acceleration on instantly (infinite jerk), and every corner kicks the machine's resonances.
- On the simulated portal, the tool head on its carriage (28–55 Hz) bends a/ω² under acceleration and rings afterwards. That is three quarters of the path error at 0.25–1 g.

**Machine-specific effects** in the simulated portal:
- the gantry's racking: the two Y motors' loops against the bridge's yaw;
- the frame rocking on its feet;
- iron-core cogging;
- guide friction;
- the DC bus: braking energy raises it, and the voltage limits speed.

The drive settings fix the first two; pcomp handles the third, using a model of the machine.

## Drive settings

Both are opt-in. Without them a drive behaves exactly as before.

```
vel_int0.mode = 1     # command interpolation and acceleration feedforward
vel1.bc = 1           # velocity observer without the lag
vel1.bc_lp = 0        # optional low-pass of the correction [Hz] (0: none)
```

**Use them together.** The observer's lag had been standing in for the missing acceleration feedforward: either one alone makes tracking worse.

### `vel_int0.mode = 1` (`shared/comps/vel_int.c`)

LinuxCNC's planner moves with constant acceleration between its updates. So from the last three position packets the drive recovers the velocity and acceleration at the newest one exactly:

    a = (d1 − d0) / T²,   v = d1/T + a·T/2,   T = 1 / vel_int0.cmd_freq

d1 and d0 are the last two position steps. Between packets the command is interpolated quadratically, and the acceleration goes through `rev0.out_dd` to `pid0.acc_ext_cmd`.
- `cmd_freq` must be LinuxCNC's servo rate (default 1000 Hz).
- A packet with vel 0, or no motion, means stop.
- The watchdog (`wd`) holds the position when packets stop.

In mode 0 (the default) `acc_out` passes `vel0.acc` through, so configs with `vel0.en = 1` keep working.

### `vel1.bc = 1` (`shared/comps/vel.c`)

Adds the observer's proportional correction to the reported velocity, so it is the derivative of the observer's own position estimate. The 2·d·a/w lag disappears. `vel_bw` then is the real bandwidth, and the w/(2d) ceiling no longer applies.

`bc_lp` filters the correction if encoder quantization makes it noisy; 300 Hz cost nothing in the tests.

### Linear motors: `conf0.j` is the whole moving mass

The observer predicts acceleration from torque / `conf0.j`. On a rotary motor j is the rotor, and the load sits behind a coupling. A linear motor's coil is bolted to its load, so:

    conf0.j = m_moving · (p / 2π)²,   conf0.j_sys = 0

Here p is the electrical period (one "revolution"); `host/README.md` ("Linear motors") has the conversions. With only the coil in j, the loop rings at 15–25 Hz while moving.

### Cogging (iron-core linear motors)

`pmsm_ttc` (in the `pmsm` template) already compensates one harmonic of cogging: `pmsm_ttc0.ac` (amplitude [Nm]), `pc` (phase), `nc` (harmonic per electrical period), and `g` (gain, 1 to apply). To identify it, run a slow constant-speed move: the current then shows the cogging as a ripple at the magnet pitch.

## Tuning procedure

For each drive, after the motor is identified (setup wizard or `rlpsij`):

1. **Set the inertia.** For a linear motor, `conf0.j` is the moving mass (above); for a rotary motor, the motor in `j` and the load in `j_sys`.
2. **Turn both settings on:** `vel_int0.mode = 1` and `vel1.bc = 1`.
3. **Pick the gains.** Start at pos_bw 60, vel_bw 400–600. With `bc` on, vel_bw is the real bandwidth. If you come from a tuning without it, the old vel_bw behaved like vel_bw/(1 − 2d·vel_bw/w): about twice the number for typical values.
4. **Judge the tuning while moving, not at standstill.** Guide friction acts as heavy damping around zero speed, so a loop that rings in motion can hold perfectly still. Run constant-speed moves and acceleration steps, and watch `pid0.pos_error` and the current.
5. **Raise vel_bw until the first mechanical resonance shows** (ringing at a fixed frequency after acceleration steps), then back off 30 %. On a gantry this is often the racking mode: the two Y loops against the bridge's yaw. In the yaw each Y motor moves only `2 J / L²` of the bridge, a quarter or less of the mass it is tuned for, so its loop is that much faster there. With the default current loop (`cur_bw` 1000 rad/s) four of the five simulated gantries fought at 160–235 Hz with vel_bw 500: raise `conf0.cur_bw` to 2000 or lower vel_bw (`host/portal/machine/retrofit/README.md`). `dynwiz mass` and `dynwiz cogging` detect it.
6. **Then pos_bw:** typically vel_bw/5 to vel_bw/4.

## pcomp: model-based command conditioning

`linuxcnc/pcomp` is a realtime component between motion and the drives (code in `shared/pcomp/pcomp.c`). Its README has the pins and the wiring; `hwtest/portal-sim/portal_pcomp.hal` is a complete example.

Every servo period it does four things:
1. **ZV input shaper** at the head's modes: x fixed, y at the carriage's X (beam model). The same filter goes on X and Y, so the path keeps its shape.
2. **A moving average** (`jerk-ms`, 20 ms), so the acceleration ramps instead of stepping.
3. **Acceleration leads:** out = s + a·(1/ω² − τ² − σ²/2).
   - The head bends against the acceleration: +a/ω².
   - The drives' lag: −τ²·a (about 0 with the drive settings above, so `servo 0`).
   - The filters' shrink inside curves: −σ²/2·a.
4. **Velocity:** the central difference of the smoothed path.

For LinuxCNC:
- **Feedback re-referencing:** motion gets fb + (command in now − command out the period before). Its following-error check then sees the drives' own error.
- **I/O delay:** the `dio`/`s32` channels delay laser or spindle signals by the conditioning's delay (about 24 ms), so they act where the tool is.
- **Enable:** `enable` must be the amplifiers' enable.

Usage in an INI (`[PCOMP]`, as `hwtest/portal-sim/portal.ini`):

```
[PCOMP]
SHAPER = 1
DEFL = 1
SHRINK = 1
SERVO = 0          # with vel_int0.mode 1 and vel1.bc 1 the drives don't lag
JERK_MS = 20
FX = 45            # the head's modes [Hz]: from the step test below
FY_END = 55
FY_MID = 28
ZETA = 0.03
RAILS = 1.2        # [m]
X_CENTER = 500     # [mm]
SERVO_TAU2 = 0
```

The model only has to be roughly right: with all frequencies 10 % off the result lost about 10 %.

## Gantry-aware control

Two Y drives move one bridge, each with its own scale and loop, tuned for half the moving mass. The bridge has two modes:
- **Common:** Y1 and Y2 together. Each motor moves its share of the mass, as tuned.
- **Yaw:** Y1 against Y2. Each motor moves only `2 J / L²`, a quarter or less of its share (laser3015: 18 of 73 kg; pcbdrill: 4.7 of 21.5 kg).

In the yaw each velocity loop is therefore four times as fast as tuned. Unless guide friction damps it, it meets the current loop: Y1 and Y2 fight at 160–235 Hz, at up to 10 A. A single drive can't tell the two modes apart.

**The gantry link** (`shared/comps/gantry.c`) lets it. The two Y drives exchange their position error and velocity, and each feeds its pid a blend of its own and the other's:

```
v' = v + (1 - r_vel)/2 (v_other - v)      e' = e + (1 - r_pos)/2 (e_other - e)
```

- **The common mode** (the mean) is unchanged.
- **The yaw** gets `r_vel` times the velocity gain. With `r_vel ≈ m_yaw / m_side` its loop is tuned for its own inertia.
- **`r_pos`** (default 1) keeps the yaw's position stiffness, which holds the bridge square.
- **Errors, not positions,** are exchanged, so the two scales' offsets don't matter.

```
load gantry
gantry0.rt_prio = 7
gantry0.cmd = reslimit0.pos_out
gantry0.pos_in = fb_switch0.pos_fb
gantry0.vel_in = vel1.vel
pid0.pos_fb = gantry0.pos_out
pid0.vel_fb = gantry0.vel_out
gantry0.en = fault0.en_pid
gantry0.r_vel = 0.4
```

In the simulator (`glink DELAY EVERY` in a portal file; `machine/retrofit/*_glink.portal`, at the default gains):

| machine | without the link | with it (`glink 3 2`) |
|---|---|---|
| laser3015 | Y1/Y2 ±4 A at 200 Hz, rack 85 urad | 1.0 A (friction), rack 7–10 urad |
| cmm | 10 A at 208 Hz, rack 118 urad | 0.01 A, rack 2.4 urad |
| pcbdrill | 7 A at 227 Hz after reversing | 0.00 A |
| laser3015, vel_bw 1000 | SAT_ERROR | stable (r_vel 0.5) |

- **Stability rule:** keep the yaw's velocity bandwidth `vel_bw · r_vel / (m_yaw / m_side)` below about 1.8 × `cur_bw`.
- **The link must be fast:** at most about 1 ms. At 2 ms the fight starts to return; at 3 ms, as a relay through LinuxCNC and smart-serial would take, it is back in full (`tests/portal_gantry_delay.test`).
- **So it has to be drive to drive.** The intended transport is classic CAN between the two Y drives: stmbl v4's P10 has CAN1, and needs a transceiver board (can-support branch). An 8-byte frame each way every 400 µs is 68 % of 1 Mbit/s, with about 600 µs latency. That transport isn't written yet: without a link (`rx_ok` 0) the component passes the feedback through unchanged.
- **Until then:** `conf0.cur_bw = 2000` or a lower vel_bw (`machine/retrofit/README.md`). dynwiz detects the fight.

**What doesn't help:**
- **The pid's inertia split** (`j_mot`/`j_sys`/`j_lpf`) can't tell the modes apart.
- **A per-side load-split lead in pcomp:** the stiff Y drives already hold the carriage's quasi-static load split, and the rack that is left is dynamic (DEBUGNOTES).

## Measuring the machine

**`linuxcnc/dynwiz` does all of this from the console.** It moves the machine through LinuxCNC and records every servo cycle. It finds:
- the masses (`conf0.j`);
- the head's modes along the bridge;
- `tau2`;
- cogging;
- loops at their limit.

It then verifies pcomp and writes `[PCOMP]` and suggested drive configs. Each step runs on its own: `dynwiz mass`, `dynwiz modes`, ..., or all of them with `dynwiz run`. Its README covers setting up a machine (`[DYNWIZ]`), safety and reading the results. The rest of this section is what it does, by hand.

### Head modes: the step test

1. Stand the axis still, with the drives tuned.
2. Command a 1 mm step in about 15 ms (MDI `G91 G1 X1 F4000`, or from HAL).
3. Record the drive's position feedback, or just its current, for 0.4 s at the servo rate: `halsampler` with `sampler`, as `hwtest/portal-sim/portal.hal` does.
4. Fit the ring-down frequency: the peak of the spectrum 40–380 ms after the step.

The head's swing pushes the carriage back, so the scale and the current both show it, and no extra sensor is needed. In the simulator the Y frequency comes out within 3–6 % at every position.

For a mode that varies along the bridge, repeat at 5 X positions and fit the beam model:

    1/f² = 1/f_end² + (1/f_mid² − 1/f_end²)(1 − u²),   u = 2(X − x_center)/rails

That is a straight line in (1, 1 − u²), so least squares fits it.

The X step finds the mode as coupled to the servo-held carriage, which can be lower than the head's own (33 vs 45 Hz in the simulator). Compensating with the identified model gave 224 µm at 1 g, against 195 µm with the true one.

### Servo lag: the circle test

Only needed when the drive settings can't be used. Run a circle (R20 at 0.5 m/s: 12.6 m/s²) with the rest of pcomp on. Then set τ² = −(mean radius − R) / a_centripetal, and repeat once.

### What to record during any test

All in one HAL `sampler` row per servo cycle:
- motion's command (`joint.N.motor-pos-cmd`);
- the drives' feedback and current;
- `motion.motion-type`, delayed through `pcomp.0.s32-in-0` when pcomp is used;
- any machine-side measurement, such as an accelerometer.

## Testing in simulation

Everything below runs without hardware. The drives are the real drive code (`libstmbl_drive.so`); the machines are simulated plants.

Besides the portal (`host/portal/machine/portal.portal`) there are four imaginary retrofit machines in `host/portal/machine/retrofit`:
- a 3 × 1.5 m laser on iron-core motors;
- a dispenser with one Y motor and an idler rail;
- a CMM on air bearings;
- a 2 g PCB driller.

Their README has what each needed. `linuxcnc/dynwiz`'s `make test-sim` runs the wizard on all of them and checks what it finds against their files; `make -C hwtest/portal-sim dynwiz` runs it through LinuxCNC.

### Host suite

```sh
make -C host check        # all host tests (695): motors, autotune, arm, portal
python3 host/tests/run_tests.py host/tests/acc_ff_linear.test
python3 host/tests/run_tests.py host/portal/tests/*.test
```

| Test | Checks |
|---|---|
| `host/tests/acc_ff_linear.test` | linear axis at 2 m/s² with `vel_int0.mode 1` and `vel1.bc 1`: the acceleration recovered exactly (209.4 rad/s²), position error < 1.2 mrad (stock: 3.7) |
| `host/portal/tests/portal_momentum.test` | the multibody model: momentum conserved with a free frame |
| `portal_move.test` | a fast diagonal: following and contour error, head ringing, frame motion, force on the floor |
| `portal_racking.test` | the load split between Y1 and Y2 with the carriage near either rail |
| `portal_bus.test` | the DC bus rising on braking, capacitance, the chopper |
| `portal_pcomp.test` | pcomp on a circle: the contour error drops about 9× |
| `portal_idler.test` | one Y motor with an idler rail: the bridge twists |

`stmbl_host`'s `lcnc EN VEL POS ACC` plays LinuxCNC's planner, with constant acceleration and `vel_cmd` as LinuxCNC sends it. Use it to test any drive config:

```sh
cd host
./stmbl_host --drive examples/linear_axis.conf --sim examples/linear_axis_sim.txt <<EOF
vel_int0.mode = 1
vel1.bc = 1
lcnc 0 0
run 0.5
lcnc 1 0
run 1.5
lcnc 1 0 0 2000
run 0.15
pid0.pos_error
EOF
```

### portal_sim: a machine on three drives

```sh
make -C host/portal
cd host/portal
./portal_sim machine/portal.portal SCRIPT
```

- **The machine:** `machine/portal.portal` (format at the top of `portal_sim.c`). Variants: `portal_rigid.portal`; `drive y2 none` for one Y motor with an idler rail; `cogging ...` for iron core.
- **Script commands:** `move`, `tmove`, `circle`, `replay` (a LinuxCNC recording), `drv` (any drive pin), `comp ...` (pcomp), `print`, `log` and `watch`.
- **What it reports:** following and contour error, the true tool point, racking, frame motion, feet force, currents and bus voltage.

### LinuxCNC on the simulated portal

```sh
make -C host/portal/lcnc all install
make -C linuxcnc/pcomp all install
cd hwtest/portal-sim
make test            # the benchmark path at 12 and 36 m/min, 0.5 g (~3 min)
make test-pcomp      # the same with pcomp
make bench           # accelerations x feeds (~40 min): results/results.md
make bench-pcomp
python3 pareto.py results/results.json        # fastest run per accuracy target
python3 replay_study.py <run>.samples         # a recording replayed with pcomp variants
```

- **The scoring:** `bench.py` scores the true tool point against the programmed geometry, while cutting: p99 and max path error. It also reports following error, current, racking, frame motion and bus voltage.
- **Running it:** LinuxCNC must run as a non-root user. In a cloud session, see `hwtest/arcmate-sim/README.md` ("In a cloud session") and `DEBUGNOTES.md` (headless LinuxCNC, Python versions).

## Results

Simulated portal, CAM-like benchmark path (9 m, 4,200 segments, G64 P0.02), 72 m/min. Each acceleration has the same cycle time across all columns:

| accel | time | stock drive | stock + pcomp | new drive settings | new + pcomp |
|---|---|---|---|---|---|
| 0.25 g | 75 s | 208 µm | 48 µm | 194 µm | **26 µm** |
| 0.5 g | 54 s | 260 µm | 93 µm | 277 µm | **48 µm** |
| 1 g | 38 s | 612 µm | 210 µm | 781 µm | **137 µm** |

These are 99th-percentile path errors.
- **With both, a 50 µm job runs at 0.5 g in 54 s.** Before, it needed 0.1 g and 116 s.
- **The drive settings alone barely change the path error, and at 1 g it gets worse.** The axes follow almost exactly (following error 0.46 → 0.12 mm at 1 g), so the head's swing shows in full; the old servo lag had partly hidden it. pcomp then compensates the head.

## Pitfalls

- **Holding still proves nothing about stability.** Judge tuning in motion.
- **Linear motors:** `conf0.j` is the moving mass.
- **A component between motion and the drives has three traps:**
  - its enable must follow the amplifiers', or the follow-the-feedback loop winds up;
  - the velocity must not be a backward difference;
  - feedback re-referencing must pair *this* period's command with the *previous* output.
- **Make sure the speed limit is real.** The motor's back-EMF at speed must stay below the DC bus. In the simulator, Y tops out at 1.8 m/s on 230 V mains.
- **Braking needs capacitance or a chopper.** A fast stop lifted the simulated bus from 325 to 373 V on the drives' own 1.4 mF. The chopper must switch on below the drives' `conf0.max_dc_volt` (380 V), or they trip first.
- **Gantry yaw.** Each Y loop is tuned for its share of the mass, but in the bridge's yaw it moves much less: Y1 and Y2 can fight at 150–250 Hz. Raise `cur_bw` or lower `vel_bw` (Tuning procedure, step 5).
- **The scales may not see the head.** A light, stiff head barely moves a servo-held carriage, and the frame on its feet or the position loop's own settling can ring louder. `dynwiz modes` sets aside a base mode, marks a fit that doesn't bend like a bridge as doubtful, and can use a sensor on the head (`--use-tool`).
- **Two LinuxCNC HAL gotchas:**
  - A HAL pin can be on only one signal: join the existing signal rather than netting the pin again.
  - The `sampler` records from load time unless `sampler.0.enable` is set to 0.
- **When headless `linuxcnc` fails silently:** read `~/linuxcnc_debug.txt`.
