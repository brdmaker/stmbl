# armtool: the pneumatic tool changer's monitor

A LinuxCNC realtime module (uspace), for machine protection (not a safety
function). It drives the changer's lock valve from the program's request
and watches the sensors: "locked" / "unlocked" on the piston, "present"
(a tool plate on the master) and the tool's coded ID. From these it
knows the mounted tool. The code is `shared/robot/tool.c` (`rb_changer`),
the same `host/arm/arm_sim` runs; all pins are listed at the top of
`armtool.c`, and the wiring is in `hwtest/arcmate-sim/changer.hal`.

```
loadrt armtool model=/path/model.arm tools=/path/tools.txt
addf armtool.0 servo-thread
```

- **The mounted tool** goes to armcollide (`tool-mass`, `tool-cx/-cy/-cz`:
  a payload beside the gripped part's) and to armguard (`tool`: the tool's
  `body ... tool N` lines count).
- **Docks** for the M6 routine: `dock-select` (M68 E5) picks a tool, and
  `dock-0..11` give its dock pose, the bare flange's approach point and
  the slide-out point with the tool on. Wired to motion's analog inputs,
  they are read with `M66 E<n> L0`.
- **Faults** (latched until `reset`, `reason`; `chain-out` drops the estop
  chain):
  1. the lock sensors did not follow the valve within 0.5 s;
  2. it locked onto another tool than `expect` (M68 E4), or onto none;
  3. the mounted tool's lock, presence or ID went away (10 ms);
  4. unlock was requested with a tool on, away from its dock. The valve
     stays locked, so the tool does not fall;
  5. the arm moved (more than 0.1 deg in 50 ms) with the kinematics on
     another tool's TCP. Standing still it may be put right: after a lost
     tool, `M68 E3 Q0`, `M66 E0 L0`, `M61 Q0`.

  On reset, the mounted tool is whatever the sensors show, so a lost tool
  is gone.
