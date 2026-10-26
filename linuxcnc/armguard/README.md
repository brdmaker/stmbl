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

## Collision prevention (`prevent MM` in the cell)

Every servo period, before motion's command is passed on, the guard asks:
*if the arm started its stop now, would it keep the margin?* Its stop is
known (all joints together within `astop`), so it checks the clearance of
the arm's bodies to the cell's obstacles and to each other along that stop
path (4 points). If the arm would come within `MM` and is approaching
(the stop path is closer than the arm now; moving away is always
allowed), it stops now, holds, and requests estop: a protective stop
(`armguard.0.reason` 2), no backoff, no contact.

Pins: `armguard.0.prevent-enable` (in, on by default when `prevent > 0`),
`armguard.0.clearance` and `.clearance-stop` (out, mm).

The check is honest about the stop: a fast joint near an obstacle or a
fold of the arm itself is stopped if its stop could not end clear, even
if the planned path turns away later. Set `astop` to the decelerations the
drives can really do in a stop (the simulated cell: 2 x the joints'
MAX_ACCELERATION); too gentle values make prevention stop early. The
bodies must cover the real arm and tool (with some margin); bodies that
overlap by construction need `selfignore`.
