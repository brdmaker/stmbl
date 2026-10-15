# armcollide: model-based collision detection for a serial arm

A LinuxCNC realtime module (uspace) for **machine protection** (not a
safety function: no certified, dual-channel monitoring). It compares the
joint torques measured through the drives' motor currents with the arm's
dynamic model, using a generalised momentum observer (De Luca): no
acceleration is needed, and the residual estimates the external torque on
each joint. A residual over its joint's threshold latches `collision`.

The code is `shared/robot/collide.c` (and `robot.c`), the same that
`host/arm/arm_sim` runs in its tests; this file is only the HAL wrapper.

```
loadrt armcollide model=/path/model.arm
addf armcollide.0 servo-thread
net ... joint.N.motor-pos-fb  => armcollide.0.N.pos      # deg, genserkins' DH theta
net ... <drive N vel-fb>      => armcollide.0.N.vel      # deg/s
net ... <drive N current>     => armcollide.0.N.current  # A (iq)
net ... motion.motion-enabled => armcollide.0.enable
net ... armcollide.0.collision => (estop chain, or a reaction component)
net ... iocontrol.0.user-request-enable => armcollide.0.reset   # estop reset clears it
```

Outputs: `armcollide.0.N.residual` and `.threshold` [Nm], `.collision`
(latched until `.reset`, so an estop request lasts until the operator
resets: LinuxCNC's I/O controller samples `emc-enable-in` only every
`[EMCIO] CYCLE_TIME`), `.joint` (first joint over its threshold, 1-based),
`.armed` (hold-off after enable over). Inputs also `.payload-mass`,
`.payload-cx/-cy/-cz`: the payload on the flange; an undeclared one looks
like a collision.

## The model file

The arm file format of `host/arm` (see `shared/robot/collide.h`): Craig DH
as genserkins, link mass / centre of mass / inertia, motor (gear ratio,
rotor inertia), wrist coupling, gearbox friction, torque constants
(`kt J 1.5*pole_pairs*psi`), thresholds and the observer gain. For the
real arm it comes from identification (`hwtest/arcmate/MEASUREMENT_PLAN.md`
Phases 4-6); thresholds from Phase 7 (residual floor of normal work).

## Thresholds

Per joint: `thr0 + thr_v |qd| + thr_s exp(-(qd/vs)^2)`. Near standstill,
gearbox stiction carries an unknown part (up to the breakaway friction)
of the load, so the residual there can reach that much without any
contact: the stiction band `thr_s` covers it, and the detector is less
sensitive at rest than in motion. Set `thr0` from the residual floor while
moving, `thr_s` from the floor near standstill, both with margin.
