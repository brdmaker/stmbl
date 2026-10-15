# FANUC ARC Mate (A05B-1203-B255) — measurement plan

Goal: everything needed to run the arm on six stmbl drives with LinuxCNC as
master (`genserkins`), and to build the model-based collision detection
(momentum observer, machine protection only). No data exists for this arm,
so everything is measured. Results go into `RECORD.md` (one section per
axis), raw logs into `logs/` (CSV, file name `<phase>_<axis>_<what>.csv`).

Nameplate: ARC Mate, TYPE A05B-1203-B255, F-16140, NO. R93…, DATE 1993.4,
FANUC LTD.

The phases build on each other: 0 → 1 → 2 → 3/4/5 → 6 → 7. Phases 1–2 need
only one drive at a time; 4–7 need all six (LinuxCNC as the logger).

---

## Equipment

| Item | Used in |
|---|---|
| Hoist or sling + wooden blocks to support the upper arm | 0, everything with J2/J3 brakes released |
| Digital inclinometer, 0.05° or better (two, if possible) | 2, 5 |
| Dial indicator 0.01 mm + magnetic base | 3 |
| Luggage scale / spring scale, or a load cell (0–50 kg) | 3, 7 |
| Known weights (e.g. 2 × 5 kg) and a bolt-on bar for the flange | 5, 6 |
| LCR meter, multimeter, oscilloscope (2 ch) | 1 |
| Cordless drill + coupling, to backdrive a motor off the arm | 1 |
| Tape measure, caliper, a sharp pointer for the flange, a fixed sharp point | 2 |
| IR thermometer (or the motor thermistors, if fitted) | 4 |
| Foam block / padded obstacle | 7 |

## Safety rules (all phases)

- Bolt the base to something rigid; a falling 1990s arm is the main hazard.
- **J2/J3 fall when their brakes release.** Support the upper arm (sling or
  blocks) before releasing a brake or before the first enable of a new
  axis, and keep the support until the axis has held position under
  control.
- Hardwired E-stop that cuts the drives' HV and drops the brakes,
  independent of LinuxCNC and of the drives' software.
- Brake voltage: measure the coil resistance and read the motor label
  before powering a brake. Older FANUC motors used 90 V DC brakes, newer
  ones 24 V; do not guess.
- First moves of every axis: low `max_ac_cur`, low `max_vel`, the drive's
  position-error limit tight, nobody inside the reach.
- Back up each drive config (`servoterm-cli --dump`) before changes; flash
  only with the repo's make targets (see `CLAUDE.md`).

---

## Phase 0 — inventory (no power)

For every axis J1–J6, record in `RECORD.md`:

1. Motor nameplate (A06B-…), encoder label (A860-… or other), photo of each.
2. Connector pinouts: power U/V/W/PE, brake, encoder, thermal switch.
3. Brake: present?, coil resistance, label voltage.
4. Mechanical: hard stops (positions roughly), over-travel switches,
   **zero-position witness marks** on the castings (FANUC arms have them),
   cable routing limits (J1/J4/J6 may have range limited by the dress pack).
5. J2 gravity balancer (spring cylinder)? Present / absent, mounting points.
6. Wrist layout: which motors sit where, belt/gear trains to J4–J6 —
   this tells whether the wrist axes are **coupled** (Phase 2.3).
7. Encoder battery / absolute backup: present? (multi-turn position needs it)

## Phase 1 — each motor (one drive)

Off the arm where it comes off easily (wrist motors usually do); on the arm
otherwise, with the joint in a gravity-neutral position (J1 anywhere; J2/J3
supported). On the arm, the identified inertia and friction include the
reflected joint — that is fine, note which it was.

1. **R, L**: LCR meter line-line at 1 kHz (L at two rotor positions,
   min/max → Ld/Lq), then `rlpsij` on the drive (r, l, polecount). Compare.
2. **Back-EMF (psi)**: off the arm, spin with the drill at a steady speed,
   scope line-line voltage: psi = V_ll,peak / (√3 · ω_el). Cross-check with
   `rlpsij` psi.
3. **Encoder**: protocol (works with `encf`?), counts/rev, multi-turn
   counter, battery backup kept over power-off.
4. **Commutation offset** (`conf0.mot_fb_offset`) and direction
   (`mot_fb_rev`): `rlpsij` / `id_pmsm`.
5. **Motor inertia, friction, damping**: `rlpsij` (off the arm: motor
   alone; on the arm: motor + joint, use `rlpsij0.max_travel` to stay
   inside the joint range — see host/README "Linear motors").
6. **Brake**: release voltage, holding current, release and engage time
   (scope the coil current and the encoder).
7. **Thermal sensor**: type (PTC / NC switch / KTY), resistance cold.
8. Save the drive config: `hwtest/arcmate/conf/J<n>.txt`.

## Phase 2 — gear ratios and geometry (one drive per axis)

2.1 **Gear ratio** per joint: move the joint by a large known angle and
read the motor encoder (turns + counts).
- J2, J3, J5: inclinometer on the link, two positions ≥ 60° apart.
- J1, J4, J6: full joint turn if the range allows (witness mark to
  witness mark), else a pointer + protractor/laser at a known radius.
- ratio = Δmotor_angle / Δjoint_angle; expect non-integer (e.g. 121:1).
  Repeat in both directions; the difference is backlash (Phase 3 measures it
  properly). Record the sign (motor + → joint +/−).

2.2 **Zero positions**: align each witness mark, record the motor
multi-turn position. This becomes LinuxCNC's joint zero
(`HOME_ABSOLUTE_ENCODER`).

2.3 **Wrist coupling**: move one wrist motor (J4, then J5) alone and
measure all wrist joint angles. If J5/J6 move when only the J4 motor
turns, record the coupling matrix (joint = C · motor); LinuxCNC needs it as
a small HAL comp before kinematics.

2.4 **Link lengths, first pass**: tape/caliper between joint axes, with
the arm at zero. Enough for `genserkins` to run; Phase 2.5 refines them.

2.5 **Kinematic calibration** (after Phase 4 has all axes running):
pointer on the flange, touch one fixed sharp point from 20–30 different
arm poses; record joint angles. Least squares for the DH lengths/offsets
and joint zero offsets so all poses give the same point. Repeat with a
second point 300+ mm away to fix the scale.

## Phase 3 — backlash and stiffness per joint

The drive sees only the motor encoder, so backlash is measured outside.

1. Servo on, holding position (stiff gains). Dial indicator on the link at
   a known radius r (as far out as practical), measuring tangentially.
2. Load the link with the spring scale at radius r_f: 0 → +F → 0 → −F → 0
   in steps (e.g. 0, 20, 50, 100 N each way). Record deflection.
3. The deflection-vs-torque loop gives the **lost motion** (width at zero
   torque, backlash) and the **stiffness** (slope) per joint.
4. Gravity axes: gravity preloads the gears, so also note the arm pose;
   backlash shows up where the gravity torque crosses zero (link vertical).

Why it matters: at a reversal the motor crosses the backlash unloaded and
then picks the load up — the collision detector sees a torque step there
and must not trip on it (Phase 7 thresholds around reversals).

## Phase 4 — friction per joint (all axes under LinuxCNC)

1. Warm-up: 20–30 min of continuous motion, all axes. Friction of geared
   joints changes a lot with grease temperature; do every sweep **cold**
   (first thing in the day) and **warm**, and log temperature.
2. Constant-velocity sweeps over the same joint range, both directions, at
   log-spaced speeds (e.g. 0.5, 1, 2, 5, 10, 20, 50, 100 % of the joint's
   max speed). Log joint position, velocity and motor current at 1 kHz
   (`halsampler`). Use only the constant-velocity part of each sweep.
3. Per speed: friction = (I₊ − I₋)/2 at the same position (gravity
   cancels), times kt. Fit Coulomb + viscous + Stribeck (low-speed bump):
   τ_f(v) = sign(v)·(Fc + (Fs − Fc)·e^(−|v|/vs)) + Fv·v.
4. Position dependence: the same sweep gives friction vs angle at each speed
   (gear eccentricity, often a once-per-output-turn ripple). Record its size.

## Phase 5 — gravity, balancer, torque constant (J2, J3, J5)

1. From the Phase 4 sweeps: gravity torque = (I₊ + I₋)/2 · kt per position.
2. J2 and J3 sweeps at several angles of the other one (J3 at −45/0/+45°
   while sweeping J2, and the reverse): gravity of J2 depends on J3.
3. Fit τ_g = a·sin(q) + b·cos(q) (+ coupled terms) per joint; a J2
   balancer shows as an extra position-dependent term (spring force through
   its lever) — fit its constant and preload if present.
4. **Torque constant through the gear** (kt · ratio · efficiency): bolt a
   known mass at a known radius on the flange, repeat a J2/J3 slow sweep;
   the current difference with/without mass vs the computed m·g·r torque
   gives the real joint-side torque per amp. Do it at two masses.

## Phase 6 — full dynamic identification

1. Excitation trajectories: per joint a sum of 5 sines (Fourier series),
   within joint limits, max speed and acceleration; all joints together,
   period 10–20 s, repeated 5×. Design them in the simulator first (they
   must not hit limits or each other).
2. Log q and motor current, all joints, 1 kHz. Velocity and acceleration
   from the known trajectory (and from the encoders, zero-phase filtered).
3. Least squares for the robot's base parameters (link masses × COM, inertias,
   motor inertias, friction) — the dynamics are linear in them.
4. Validate on a different trajectory: predicted vs measured current,
   residual RMS per joint. Repeat with the known payload from Phase 5.

## Phase 7 — collision detector baseline

1. Normal motion: hours of representative paths (welding-like moves,
   rapids, reversals) with the observer running, detector **not armed**.
   Per joint: residual mean, std, max, vs speed and near reversals → the
   thresholds (constant + speed/acceleration dependent + reversal window).
2. Sensitivity: slow moves into a padded obstacle and pulls with the spring
   scale/load cell at the flange, known force and direction. Record the
   force at which each joint's residual crosses its threshold, and the
   time from contact to trip.
3. Reaction test: stop → compliant (gravity compensated) 100–300 ms →
   brakes. Check the arm backs off the obstacle and J2/J3 do not sag.
4. Repeat 1–3 warm and cold.

---

## What gets prepared in the simulator meanwhile

- Six-joint arm model in `host/` (gravity, coupling, gear ratio, friction
  with Stribeck, backlash, optional balancer, external force on a link),
  with the parameters as unknowns.
- The analysis scripts for every phase above (ratio, friction fit, gravity
  fit, kinematic calibration, base-parameter least squares), each tested on
  synthetic logs from the simulator with known "true" values: a script that
  cannot recover the simulator's parameters will not recover the arm's.
- Excitation trajectory design (limits checked in simulation).
- LinuxCNC config with six simulated drives, `genserkins`, and halsampler
  logging in the same CSV format as the real runs.
