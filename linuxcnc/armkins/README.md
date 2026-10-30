# armkins: arm kinematics with the tool changer's TCP

A LinuxCNC kinematics module (uspace) for a 6-joint serial arm in
genserkins' convention: Craig DH, joints = DH theta [deg], A B C with
R = Rz(C) Ry(B) Rx(A). With tool 0 it is genserkins: the existing torch
test passes unchanged with it. The Cartesian position is the TCP of the
tool on the tool changer, from the tool table (`shared/robot/tool.h`:
`tcp x y z a b c` per tool, a full 6D offset, so a bent torch neck or an
angled gripper works). The code is `shared/robot/kin.c` / `tool.c`, the
same `host/arm/arm_sim` runs.

```
[KINS]
KINEMATICS = armkins model=/path/model.arm tools=/path/tools.txt
```

`model`: a file with the arm's `dh` lines (armcollide's model file).

**Tool switching** uses LinuxCNC 2.9's switchable kinematics: the
switchkins type is the tool ID. Wire `motion.analog-out-03 =>
motion.switchkins-type`; in G-code `M68 E3 Q<tool>` then `M66 E0 L0`, which
resyncs the interpreter. Motion recomputes the Cartesian position from
the joints at the switch, so the reported position jumps to the new TCP
and the next move starts from it. `hwtest/arcmate-sim/toolchange.ngc` (the
M6 remap) switches to 0 for the change and to the new tool after it.

Pins: `armkins.tool` (s32, the TCP's tool now), `armkins.ik-fails` (s32).
The inverse kinematics is damped least squares from the current joints,
which gives the nearest solution.
