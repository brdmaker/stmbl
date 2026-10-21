# pcomp: physics-based command conditioning for a portal machine

A LinuxCNC realtime component between motion and the drives. It shapes the planner's X, Y commands with a model of the machine, so that the **tool point**, not the carriage, follows the program at high acceleration. The code is `shared/pcomp/pcomp.c`, the same file `host/portal/portal_sim` uses. The why and the numbers are in `hwtest/portal-sim/README.md`.

The whole picture (the drive settings, tuning, measuring the machine, testing): [docs/motion_control.md](../../docs/motion_control.md).

What it does each servo period:
1. **ZV input shaper** at the head's modes. The x mode is fixed; the y mode is the model's at the carriage's X (the bridge bends most at mid-span). The same filter goes on both axes, so the path keeps its shape.
2. **A moving average** (`jerk-ms`, 20 ms), so LinuxCNC's acceleration steps become ramps.
3. **Acceleration leads** on the smoothed command: out = s + a·(1/ω² − τ² − σ²/2).
   - The head bends against the acceleration: +a/ω².
   - The drives' steady lag behaves like a filter: −τ²·a.
   - The filters pull the path inside curves: −σ²/2·a.
4. **The velocity** to the drives is the smoothed path's central difference. A backward difference would be half a period late, which is v·dt/2 of extra servo lag (0.8 mm at 1.6 m/s with 1 ms).

It also does two things for LinuxCNC:
- **Feedback re-referencing.** The drives' feedback goes back to motion as fb + (command in now − command out the period before). Motion's following-error check then sees the drives' own error, not the conditioning's delay (about 24 ms, 38 mm at 1.6 m/s) or its leads.
  - The pairing matters: pcomp reads the drives' feedback before they run, so it shows where they got to with pcomp's *previous* output. Motion compares that feedback with *this* period's command.
  - Either other pairing leaves v·dt in the following error (1 mm at 1 m/s with a 1 ms servo period), and LinuxCNC aborted on it while decelerating out of rapids.
- **I/O delay.** The `dio` and `s32` channels delay by the same amount, so a laser, spindle or dispense valve acts where the tool is.

```
loadrt pcomp
addf pcomp.0 servo-thread       # after motion-controller, before the drives

net x-cmd   joint.0.motor-pos-cmd  => pcomp.0.x-cmd
net y-cmd   joint.1.motor-pos-cmd  => pcomp.0.y-cmd
net x-vcmd  joint.0.vel-cmd        => pcomp.0.x-vel-cmd
net y-vcmd  joint.1.vel-cmd        => pcomp.0.y-vel-cmd
net x-drv   pcomp.0.x-out          => <X drive pos-cmd>
net x-dvel  pcomp.0.x-vel          => <X drive vel-cmd>
net y-drv   pcomp.0.y-out          => <Y1 drive pos-cmd> <Y2 drive pos-cmd>
net y-dvel  pcomp.0.y-vel          => <Y1 vel-cmd> <Y2 vel-cmd>
net x-fb    <X drive pos-fb>       => pcomp.0.x-fb
net y1-fb   <Y1 drive pos-fb>      => pcomp.0.y1-fb
net y2-fb   <Y2 drive pos-fb>      => pcomp.0.y2-fb
net x-mfb   pcomp.0.x-fb-out       => joint.0.motor-pos-fb
net y1-mfb  pcomp.0.y1-fb-out      => joint.1.motor-pos-fb
net y2-mfb  pcomp.0.y2-fb-out      => joint.2.motor-pos-fb
net amp-en  joint.0.amp-enable-out => pcomp.0.enable ...
net laser   motion.digital-out-00  => pcomp.0.dio-in-0
net laser-d pcomp.0.dio-out-0      => <the laser's gate>
```

`hwtest/portal-sim/portal_pcomp.hal` is a complete example, with the model in the INI's `[PCOMP]`.

**Wire `enable` to the amplifiers' enable.** While they're off, motion makes its command follow the feedback; through the conditioning's delay that loop would wind up (it did, at startup, when the absolute scales came in). On enable it starts again from standstill.

## Pins

| Pin | Dir | Meaning |
|---|---|---|
| `x-cmd`, `y-cmd` | in | motion's command [mm] |
| `x-vel-cmd`, `y-vel-cmd` | in | its velocity, passed on while nothing is on |
| `x-out`, `y-out`, `x-vel`, `y-vel` | out | to the drives |
| `x-fb`, `y1-fb`, `y2-fb` | in | the drives' feedback |
| `x-fb-out`, `y1-fb-out`, `y2-fb-out` | out | to motion's `motor-pos-fb` |
| `enable` | in | the amplifiers' enable |
| `shaper`, `defl`, `shrink`, `servo` | in | the parts of the conditioning |
| `jerk-ms` | in | the moving average [ms] (20) |
| `fx`, `fy-end`, `fy-mid`, `zeta` | in | the head's modes [Hz] and damping (0.03) |
| `rails`, `x-center` | in | rail spacing [m], machine X of the bridge's centre [mm] |
| `servo-tau2` | in | the drives' lag per acceleration [s²] |
| `delay` | out | the conditioning's mean delay [ms] |
| `dio-in-0..3` → `dio-out-0..3`, `s32-in-0` → `s32-out-0` | | delayed by `delay` |

Change the model and the switches at standstill.

## Getting the model

On the machine, with the drives tuned:
- **Head modes:** a 1 mm step of each axis at standstill. Fit the ring-down frequency from the scale, or from the motor current alone (no extra sensor). Repeat at a few X positions and fit the beam model 1/f² = 1/f_end² + (1/f_mid² − 1/f_end²)(1 − u²), u = 2(X − x_center)/rails.
- **τ²:** run a test circle (R20 at 0.5 m/s) with everything else on, and adjust τ² until the circle's mean radius is right.

In the simulator the step test finds the y mode within 3–6 %, and the result is robust to about 10 % model error.
