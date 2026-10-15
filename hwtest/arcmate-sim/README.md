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
make test        # headless: lcnc_test.py as the display, TAP output
make run         # with AXIS
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

## Soak (`make soak`)

`soak.ngc` (lines along a cube, tool reorientation, circles in XY and XZ,
a welding weave, rapids; all Cartesian through genserkins) runs
`SOAK_CYCLES` times under `lcnc_soak.py`, then the error scenarios: a
declared payload (clean), an undeclared one (collision, estop), a push
mid-program (estop), a drive's smart-serial link lost (`stmbl-arm.0.J.
link-lost`: amplifier fault, machine off), an operator estop in the weave
(brakes hold), and a jog far past a soft limit (stops at the limit). As
`make test`: as a non-root user; TAP output.

Tool down is A0 B0 C0: the flange frame is turned (J6 theta0 180) so that
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
