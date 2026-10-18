# hwtest/arcmate-sim: LinuxCNC drives the simulated ARC Mate

LinuxCNC 2.9 as the master of the simulated arm: `genserkins` (Craig DH,
from `host/arm/arcmate/arcmate.arm`), six angular joints, each on a
simulated stmbl drive over its simulated smart-serial link. The drives
and the arm (gears with backlash, friction, brakes, stops) run in
LinuxCNC's servo thread in the `stmbl_arm` module (`host/arm/lcnc`, built
on `host/arm/armcore.c`). **All arm values are guesses** until
`../arcmate/RECORD.md` has measurements.

```
LinuxCNC (genserkins, motion) --joint.N pos/vel/enable--> stmbl-arm.0.N --> drive N (real drive code)
        ^                                                       |              hv0 electrical model
        +----------------pos-fb / fault------------------------+              arm plant (all joints)
```

- Joint angles are genserkins' DH theta: the model angle + theta0 (J2:
  -90 at the home pose). Encoders are absolute; homing is
  `HOME_ABSOLUTE_ENCODER = 2` (no motion).
- Truth pins for tests: `stmbl-arm.0.N.q` (link angle), `.tau-ext` (Jᵀf of
  an applied force), `.tau-gear`, `.stop-hits`; an external force with
  `stmbl-arm.0.force-link` (1..6) and `.force-x/y/z` [N], `.force-rx/ry/rz`
  [m, link frame].

## Run

```sh
make -C ../../host/arm all                     # libstmbl_drive.so, arm_sim
make -C ../../host/arm/lcnc all install        # stmbl_arm.so -> LinuxCNC rtlib
make -C ../../linuxcnc/armcollide all install  # the collision detector
make -C ../../linuxcnc/armguard all install    # the collision reaction
make -C ../../linuxcnc/armgrip all install     # the gripper (test-grip)
make -C ../../linuxcnc/armkins all install     # kinematics with the TCP (test-tool)
make -C ../../linuxcnc/armtool all install     # the tool changer (test-tool)
make test        # headless: lcnc_test.py as the display, TAP output
make test-grip   # the same with the electric gripper (lcnc_grip_test.py)
make test-tool   # the pneumatic tool changer (lcnc_tool_test.py)
make test-quota  # power quotas: two drives per phase (lcnc_quota_test.py)
make run         # with AXIS
make hmi         # with the ARM-6 QtVCP screen (linuxcnc/arm6hmi), gripper and quotas
make test-hmi    # the screen's self-test on Xvfb (TAP + screenshot)
```

`make test` (as a non-root user; rtapi_app refuses root) copies this
config to /tmp, sets `DISPLAY = ./lcnc_test.py` and absolute paths, and
runs LinuxCNC: machine on, home, MDI lines through genserkins (with a
wrist reorientation), a rapid back, machine off. It checks the tool point,
LinuxCNC's joint following error, the true link angles against the
command, faults, stop hits, that the collision detector stays quiet, then
pushes the flange with 150 N mid-move: `armcollide` detects it (J3,
~13 ms), LinuxCNC goes to estop, the arm stops within 0.5 deg; estop reset
and machine on again; machine off: the brakes hold.

## The cell from Fusion 360

`tools/step2cell.py` turns a STEP export into collider boxes, one per
solid (body), placed in the robot's base frame:

```sh
python3 ../../tools/step2cell.py cell.step --base 1200 -350 0 0 0 90 --margin 10 \
        --min-size 20 > cell_obstacles.txt      # then paste/append into cell.txt
```

`--base`: where the robot base frame is in the Fusion design (origin in
mm, A B C in deg); `--margin` grows the boxes; `--min-size` drops screws
and the like. Each box is the smallest of a few orientations (the CAD axes,
the body's principal axes, frames from its largest flat faces: exact for
machined parts). The report on stderr gives each body's fill (body volume
/ box volume); a low one (an L bracket: 0.25) means the box is much larger
than the part - split the body in Fusion (Modify > Split Body) into
box-like pieces. Needs `pip install gmsh numpy` (OpenCascade reads STEP);
tested by `host/tests/robot/test_step2cell.py` in `make -C host check`.

## Collision prevention

`cell.txt` sets `prevent 10`: `armguard` stops the arm if it could not
stop 10 mm clear of the cell's obstacles or of itself (a protective stop,
then estop). `lcnc_test.py`: the same move that hits the fixture plate
with prevention off (`armguard.0.prevent-enable 0`, then backoff) is
stopped 9.5 mm before it with prevention on, and the arm may move away
afterwards. The soak runs with prevention on.

## Soak (`make soak`)

`soak.ngc` (lines along a cube, tool reorientation, circles in XY and XZ,
a welding weave, rapids; all Cartesian through genserkins) runs
`SOAK_CYCLES` times under `lcnc_soak.py`, then the error scenarios: a
declared payload (clean), an undeclared one (collision, estop), a push
mid-program (estop), a drive's smart-serial link lost (`stmbl-arm.0.J.
link-lost`: amplifier fault, machine off), an operator estop in the weave
(brakes hold), and a jog far past a soft limit (stops at the limit). As
`make test`: as a non-root user; TAP output.

The home orientation (flange pointing up) is A0 B0 C0: the flange frame is turned (J6 theta0 180) so that
C stays away from +-180, where LinuxCNC may report +180 after a stop and a
program's C-180 then turns J6 a full turn into its limit.

## Collision detection

`armcollide` (`linuxcnc/armcollide`) runs in the servo thread on the
drives' feedback and currents, with `[ARMCOLLIDE] MODEL` (here the arm
model with identification-sized errors, `model_perturbed.arm`). Its
`collision` (latched until the estop reset, `iocontrol.0.user-request-
enable` -> `armcollide.0.reset`) drops `iocontrol.0.emc-enable-in`: estop,
drives off, brakes on. A softer reaction (stop, compliant with gravity compensation, then
brakes) is the next step.

## The electric gripper (`make test-grip`)

The arm with the gripper (`host/arm/arcmate/arcmate_gripper.arm`: a 7th
simulated stmbl drive moving two fingers), `cell_grip.txt` (the gripper's
housing and two 2 kg parts, 30 mm wide) and `grip.hal`, a second HALFILE:
`armgrip` (`linuxcnc/armgrip`) runs the fingers and the payload check
and feeds armcollide's payload and mute pins. G-code:

```
M68 E0 Q30        ; part width [mm] (0: any)
M68 E1 Q2.0       ; its mass [kg] (0: unknown, weighed on the lift)
M68 E2 Q0.15      ; its centre of mass on the flange z [m]
M64 P0            ; close
M66 P0 L3 Q5      ; wait for "gripped" (armcollide's model has the part now)
G1 Z+150 F9000    ; lift: the payload check (M66 P1 L3 waits for it)
...
M65 P0            ; open: released, the model drops it
```

A grip fault drops the estop chain after armguard's (armgrip.0.chain-in
/ chain-out): 6 payload mismatch, 7 part lost, 8 wrong width or nothing
gripped (`armgrip.0.reason`). `lcnc_grip_test.py` picks box1, weighs it
on the lift (2.8 kg with model_perturbed.arm's floor: ok within 1 kg),
places and releases it, then lets box2 slip out mid-carry (estop, 7),
picks box1 declared as 4.5 kg (estop, 6) and closes on nothing (estop, 8).

`cell.txt`, `cell_grip.txt` and `cell_changer.txt` share
`cell_base.txt` (`include`): the arm's bodies, the guard's settings, the
fixture.

## The tool changer (`make test-tool`)

`host/arm/arcmate/arcmate_changer.arm` puts a pneumatic changer on the
flange. The tools are in `tools.txt`: 1 a bent-neck torch, 2 the electric
gripper. Each tool has its TCP, mass, bodies and a dock, with the flange
pointing forward and up (B 45). The config is arcmate.ini with `armkins`
as the kinematics, `cell_changer.txt`, `grip.hal` + `changer.hal`
(armtool) and M6 remapped to `toolchange.ngc`. `make test-tool` builds it
in /tmp; the genserkins DH setps live in `genserkins.hal`, which only the
genserkins configs load.

```
T2 M6        ; the gripper: toolchange.ngc puts the current tool back,
             ; picks tool 2, switches the kinematics to its TCP
G1 X1150 Y0 Z1100 A0 B45 C0 F6000   ; on the gripper's TCP
T1 M6        ; the torch
T0 M6        ; no tool
```

For the change, toolchange.ngc runs on the flange's kinematics (`M68 E3
Q0`). For the current tool, it reads the dock (`dockread.ngc`: armtool's
dock pins on analog-in 0..11), goes above the fork, down into the dock,
unlocks and backs out along the approach. For the new tool, it comes in
along the approach, locks (expecting the tool: M68 E4), lifts out of the
fork, then `M61 Q<tool>` and `M68 E3 Q<tool>` + `M66 E0 L0`. Its
M64/M65/M68 are immediate outputs, so an `M66 E0 L0` before each waits
for the motion to end first. `lcnc_tool_test.py` checks:
- the three changes: the right tool, the TCP, armcollide's tool mass;
- a TCP move, and a reorientation about the torch tip, which stays within
  0.06 mm;
- the tool slipping off mid-move: estop, reason 3;
- recovery onto the flange;
- an unlock away from the dock: refused, estop, reason 4.

## Power quotas (`make test-quota`)

Each drive reports its input power over smart-serial
(`stmbl-arm.0.J.power`; `hm2_*.stbl.*.power` on the real drives).
`powerquota.hal`, a HALFILE right after `arcmate.hal`, feeds the six into
`linuxcnc/powerquota`. It puts the feed scale on `motion.adaptive-feed`
(the program needs `M52 P1`) and goes into the estop chain after the
collision guard. `quota.txt` is the cell's supply: three phases, C 16 A
each, J1 J2 on L1, J3 J4 on L2, J5 J6 on L3.

`make test-quota` uses `quota_test.txt`. The simulated arm draws little
(about 15 W idle per drive, up to about 100 W for J1 + J2), so L1 is scaled
to a 0.17 A breaker that the test's sweeps overload about 3x.
- **M52 P1:** the sweeps are slowed (feed down to about 0.3). L1 works
  near its limit (damage about 0.43, under the 0.5 margin); L2 and L3 are
  untouched; no collision hit.
- **M52 P0:** the same sweeps at full speed heat L1 until the alarm stops
  the machine at damage 0.9, before the breaker would trip.

## The operator screen (`make hmi`, `make test-hmi`)

`linuxcnc/arm6hmi` is a QtVCP screen with `linuxcnc/vis3d` as the 3D view.
The setup:
- `make hmi` copies the config with the gripper and the power quotas;
- the screen goes into `qtvcp/screens/arm6`;
- `arm6_postgui.hal` nets the view's pins.

`make test-hmi` runs it on Xvfb with `ARM6HMI_TEST` set, and the screen tests
itself through its own buttons (16 checks, see `linuxcnc/arm6hmi/README.md`).
It needs `Xvfb` and PyQt5, QScintilla and PyOpenGL for LinuxCNC's Python.

## In a cloud session

LinuxCNC is not packaged for the cloud image (Ubuntu 24.04) and takes ~25
min to build, so a prebuilt one sits on the orphan branch `linuxcnc-bin`.
The SessionStart hook restores it in the background
(`linuxcnc_restore.sh`, log in /tmp/linuxcnc_restore.log): it unpacks to
`/home/user/linuxcnc-2.9`, installs the runtime packages, builds and
installs `stmbl_arm`, adds a python3 -> 3.12 shim (LinuxCNC's Python
modules are 3.12) and a user `lcnc`. Then:

```sh
su lcnc -c "export PATH=/home/user/linuxcnc-2.9/pybin:\$PATH HOME=/home/lcnc; \
            make -C /home/user/stmbl/hwtest/arcmate-sim test"
```
