# armguard: collision reaction for a serial arm

A LinuxCNC realtime module (uspace), between motion and the drives.
Normally it passes motion's joint commands to the drives and the drives'
feedback to motion. When `armcollide` reports a collision it takes the
joints over (`shared/robot/guard.c`):

1. **stop**: decelerate from the commanded speed to standstill, all joints
   together, within the stop decelerations (`astop`);
2. **back off** 20 mm (`backoff MM T`): a straight retract of the flange,
   orientation kept, back along the path if it was moving, else along the
   contact force estimated from the collision residuals. A tool pressed
   against the obstacle comes free instead of being clamped there by the
   brakes;
3. **hold** 0.1 s, then **estop** (`armguard.0.estop`, latched until the
   operator's reset): drives off, brakes on.

While it commands the drives it feeds motion its own command as feedback
(no following error); the estop aborts motion, which resyncs to the true
feedback when it is enabled again. Machine protection, not a safety
function.

```
loadrt armguard model=model.arm cell=cell.txt
addf armguard.0 servo-thread            # after motion-controller
net ... joint.N.motor-pos-cmd => armguard.0.N.motion-cmd
net ... joint.N.vel-cmd       => armguard.0.N.motion-vel
net ... armguard.0.N.cmd      => <drive N pos-cmd>
net ... armguard.0.N.vel      => <drive N vel-cmd>
net ... <drive N pos-fb>      => armguard.0.N.drive-fb
net ... armguard.0.N.fb       => joint.N.motor-pos-fb
net ... armcollide.0.N.residual => armguard.0.N.residual
net ... armcollide.0.collision  => armguard.0.collision
net ... motion.motion-enabled   => armguard.0.enable
net ... armguard.0.estop        => (estop chain: emc-enable-in = NOT estop)
net ... iocontrol.0.user-request-enable => armguard.0.reset
```

`model`: the `dh` lines (the detector's model file). `cell`: the arm's
bodies and the cell's obstacles (`shared/robot/geom.h`) and the guard's
settings. Example and test: `hwtest/arcmate-sim` (`cell.txt`,
`lcnc_test.py`: the wrist runs into a fixture plate, backs off, the tool
ends 18 mm clear with no force).
