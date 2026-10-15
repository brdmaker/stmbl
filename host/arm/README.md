# arm_sim: six simulated drives on a simulated arm

`arm_sim` runs one simulated stmbl drive per joint and couples them to a
rigid-body arm with elastic, backlashed gears (`plant.c`, dynamics from
`shared/robot`). Each drive is its own copy of `libstmbl_drive.so`, loaded
with `dlmopen` into a separate namespace, so the firmware HAL (one set of
globals) needs no change. The drive runs its real config with
`hv0.sim_ext = 1`: the electrical side (current loop, back-EMF) stays in
the drive, the rotor and everything behind it are in the plant.

```
make                 # libstmbl_drive.so, arm_sim
make check           # tests/*.test (also part of make -C .. check)
./arm_sim arcmate/arcmate.arm SCRIPT
```

Per 200 µs rt cycle: rotor angle and speed into each drive (`hv0.sim_pos`,
`hv0.sim_vel`), one drive tick, the mean electrical torque back
(`hv0.sim_te_int`), then the plant in four 50 µs steps. Per 1 ms the
LinuxCNC side sends each drive its joint command over the simulated
smart-serial link, in joint degrees (`sserial` scale = 360 / ratio), with a
per-joint offset set when it first sees the drive (absolute encoders).

The plant, per joint: gear spring and damper with backlash between the
motor (through the transmission matrix, gear ratio and wrist coupling) and
the link; gearbox friction (Coulomb, Stribeck, viscous) on the link, rotor
bearing friction and the motor brake on the rotor, both applied so they
stop motion but never reverse it; hard stops; an external force on any
link. Encoders are absolute (`enc_fb0.sim_absolute = 1`), as FANUC's.

`arcmate/` is the ARC Mate A05B-1203-B255 with **guessed** values (see
`hwtest/arcmate/`); replace them as `RECORD.md` fills up. Script commands
are listed at the top of `arm_sim.c`.

Tests: `arm_move` (hold, a 1.5 s move of all joints, brakes on disable),
`arm_energy` (energy conserved in a free fall through the backlash),
`arm_force` (gear torques change by -Jᵀf under a push), `arm_brake`
(brakes hold; without J2's the arm falls onto its stop).

## Collision detector

`detector FILE` runs `shared/robot/collide.c` (the code of
`linuxcnc/armcollide`) every 1 ms on what LinuxCNC sees: each drive's
position and speed feedback and motor current. `arcmate/model_exact.arm`
is the plant's own model; `arcmate/model_perturbed.arm`
(`gen_model_perturbed.py`) has identification-sized errors (masses ±10 %,
centres of mass 1-2 cm off, inertias +15 %, rotor inertias +10 %, friction
±20 %) and is what the tests and `hwtest/arcmate-sim` use.

Residual floor over `tests/inc_motion_suite.txt` [Nm] (perturbed model):

| | J1 | J2 | J3 | J4 | J5 | J6 |
|---|---|---|---|---|---|---|
| moving (> 0.1 rad/s) | 9 | 11 | 8 | 1.7 | 1.8 | 0.5 |
| near standstill | 19 | 25 | 20 | 3.4 | 2.7 | 1.4 |

Near standstill gearbox stiction holds an unknown part of the load, so the
thresholds have a stiction band (`thresh J thr0 thr_v thr_s vs`), set to
~1.5 x these.

Sensitivity, force on the flange (100 mm out on its z), detection time
(`detected joint`, ms after the push; `-` = not detected within 0.3 s):

| | 20 N | 40 N | 80 N | 120 N | 300 N |
|---|---|---|---|---|---|
| moving, down | J3 16 | J3 11 | J3 8 | J3 7 | J3 5 |
| moving, +x | J2 47 | J5 17 | J5 8 | J5 6 | J5 4 |
| moving, +y | J4 15 | J4 8 | J4 5 | J4 4 | J4 3 |
| at rest (home), down | - | J3 78 | J3 19 | J3 14 | J3 9 |
| at rest (home), +x | J5 137 | J5 12 | J5 6 | J5 5 | J5 3 |
| at rest (home), +y | J4 84 | J4 15 | J4 7 | J4 6 | J4 4 |

These are simulation numbers: ideal current measurement, no torque
ripple, a friction model of the same form as the plant's. The real arm
will need larger thresholds; Phase 7 of `hwtest/arcmate/MEASUREMENT_PLAN.md`
measures its floor.

## Cartesian moves

`lmove`/`tlmove` (straight lines with reorientation), `arc` and `weave`
(welding) command the flange in mm and genserkins' A B C [deg]; every 1 ms
the pose is turned into joints by `shared/robot/kin.c` (damped least
squares from the previous joints), as LinuxCNC's genserkins does in world
mode. The kinematics agree with LinuxCNC's genserkins to 1e-5 mm / deg
(`host/tests/robot/genserkins_ref.txt`, recorded from
`hwtest/arcmate-sim`). A Cartesian move that would exceed a joint's speed
limit is aborted (`limitabort`), as LinuxCNC does. The flange frame is
turned (J6 theta0 180) so that the home orientation (flange up) is A0 B0 C0: away from the
+-180 deg wrap of C.

## Soak (`make -C .. soak`)

`tests/gen_arm_soak.py` generates 24 scenarios into `host/gen/arm_soak`,
each with the collision detector on `model_perturbed.arm` and LinuxCNC's
reactions emulated (amp fault, smart-serial link loss, following error ->
machine off; collision -> estop):

| Movement (nothing may fire) | Errors (the expected reaction) |
|---|---|
| joint suite, smooth | pushes on links 3/4/6 mid-move: estop within 50 ms |
| LinuxCNC jogs and trapezoids | 150 N at rest: estop |
| random joint moves (seeded) | 5 N while moving: no reaction (below sensitivity) |
| Cartesian lines (a cube) | undeclared 5 kg payload: flagged |
| tool reorientation | link loss: machine off in 3 ms, < 5 mm off path |
| circles in three planes | DC bus loss: drive fault, brakes hold |
| welding weaves, 10 mm/s | overspeed: drive fault |
| pick-and-place, end pose repeats | J2 into its hard stop: detected |
| declared 5 kg payload | unreachable target: move aborted, machine on |
| 0.3 A current noise | operator estop mid-weave; brake failure: falls |

Movement checks: no machine off, no fault, no stop hit, no IK or limit
abort, joint speeds under the motors' limits, following error, Cartesian
path error (commanded vs the true tool point: ~1.5 mm is the gears' sag).
`SOAK_CYCLES=N` repeats the movement. Limits of the detector found on the
way: 1 A of current noise trips the wrist (50:1 gears); an undeclared
payload is a collision.

## Collision reaction (`guard on`)

With `guard on` a detected collision does not estop at once: the guard
(`shared/robot/guard.c`, the code of `linuxcnc/armguard`) stops the arm,
backs the flange off 20 mm (`backoff MM T`) - back along the path if it
was moving, else along the contact force estimated from the residuals -
and then estops. The plant's obstacles (`obstacle ...` lines, contact
100 N/mm) push back on the arm's bodies (`body ...` in `arcmate.arm`), so
a tool pressed against them is really under tension: `arm_backoff.test`
(guard off: estop, the brakes hold the wrist pressed into the wall at
~190 N) against `arm_backoff_guard.test` (guard on: no force left, ~15 mm
clear) and `arm_backoff_slow.test` (direction from the contact force).

## Collision prevention (`prevent MM`)

`prevent MM` (after the `obstacle` lines: it takes the cell as defined so
far) runs the guard's check every 1 ms on the command about to go out:
could the arm still stop (`astop J DEG_S2`, its own synchronised stop)
without coming within MM of an obstacle or of itself? If not, and it is
approaching, it stops now and estops (off_reason 5). Tests:
`arm_prevent.test` (a line at the wall: stopped ~10 mm before it, no
contact), `arm_prevent_free.test` (a line 30 mm from the wall runs
through; after a stop the arm may move away, not closer),
`arm_prevent_self.test` (the wrist folded onto the forearm). The soak runs
every movement scenario with prevention on and has prevent_* scenarios.

Found on the way: with the guessed bodies the forearm folds onto the
lower arm from q3 ~ 85 deg (the guessed J3 limit, 160, allows poses that
cannot exist; the real stop will tell); reorienting the tool by 25 deg at
(850, 0, 1150) brings the torch within 5 mm of the forearm.
