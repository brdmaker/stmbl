# portal_sim: a simulated portal machine on three simulated stmbl drives

A 1 × 1 m gantry with linear motors.
- **Y:** the bridge on two rails, one Baldor LMCF08F per side (Y1, Y2).
- **X:** the carriage, an LMCF08E on the bridge.
- **Head:** the tool head on the carriage.

Each drive is a copy of the real drive code (`libstmbl_drive.so`, as in `host/arm`). The mechanics are a planar multibody model with the parts per-axis tuning doesn't see:

- **The frame** (600 kg) stands on leveling feet: springs and dampers to the floor in x, y and yaw (modes 12 / 10 / 14 Hz). Every motor force pushes the frame back. The model gives the frame's motion and the force on the floor.
- **The bridge racks:** Y1 and Y2 are separate loops, and the bridge's yaw is held only by the guide blocks' moment stiffness. The carriage's position shifts the load between Y1 and Y2.
- **The head** sits on springs on the carriage: 45 Hz in x, and in y 55 Hz at the rails falling to 28 Hz at mid-span, where the bridge bends most. Damping is 3 %.
- **Guide friction:** Coulomb and viscous, per side.
- **The DC bus** is shared: 230 V mains through a diode bridge, the drives' own 3 × 470 µF, and an optional brake chopper. Braking energy can't go back to the mains.

Machine numbers are guesses for a light laser / router portal (`machine/portal.portal`). Motor numbers are Baldor's catalog (`docs/baldor_linear.md`).

```
make                 # portal_sim (and ../arm/libstmbl_drive.so)
make check           # tests/*.test (also part of make -C .. check)
./portal_sim machine/portal.portal SCRIPT
make -C lcnc all install   # the LinuxCNC module (hwtest/portal-sim)
```

The portal file format and the script commands are at the top of `portal_sim.c`. Scripts play LinuxCNC (1 ms servo, smooth or trapezoidal moves, circles). They print following errors, the tool point and its contour error, racking, frame motion, feet force, currents and bus voltage, and maxima of all of them. `log` writes them every 1 ms and `watch` adds any drive pin.

## The model

Generalized coordinates: the frame Xf, Yf, ψ; the bridge y and its yaw θ; the carriage x; the head's deflection hx, hy.
- The motor and guide forces act between bodies, on the relative coordinates.
- M(q) and the velocity terms come from the bodies' positions by numerical Jacobians, solved every 50 µs step: M q̈ = Q − Σ m Jᵀ J̇ q̇.
- So the reactions on the frame and the coupling between the axes are not added by hand; they follow from the equations.

Checked by `portal_momentum.test`: with the frame free (no feet) and no friction, the total momentum stays below 1e-4 kg m/s while the frame recoils 20 mm.

A linear motor runs as a rotary one with one revolution per electrical period (60.96 mm):
- position [rad] = s · 2π/p;
- force = torque · 2π/p;
- LinuxCNC works in mm along each scale (1 µm absolute scales).

## What the simulation found

**`conf0.j` is the whole moving mass on a linear motor.** The drive's velocity observer (`vel1`) predicts acceleration from torque / `conf0.j`.
- With `j` = the coil (1.5 kg on X) and the rest in `j_sys`, the observer expects 8 × too much acceleration. The loop then rings at 15–25 Hz during moves, with up to 2 mm of following error.
- With `j` = the moving mass, cruise error drops from about 5 to 0.4 µm rms, and settling from 200 to 40–70 ms.
- See `DEBUGNOTES.md`.

**Holding hides an unstable loop.** At standstill the guides' Coulomb friction acts as huge damping, so a loop that rings while moving holds quietly. Judge the tuning on a moving axis.

**The racking mode limits the Y gains.** Each Y drive is tuned for its 21 kg share. For the racking motion a motor sees only about 5 kg (the bridge's yaw inertia over (L/2)²), so the same gains are about 4 × too hot for that mode:
- vel_bw 300 / pos_bw 60: about 30 µrad of racking;
- higher gains: a 0.6–1.3 mrad oscillation (`jammed` above 2 mrad: a real bridge would bind).

A cross-coupled gantry controller, with separate loops for the mean and the difference of Y1/Y2, would lift this limit.

**The motors' speed limit is the bus voltage.** The back-EMF ψω reaches the bus' Udc/√3 at about 1.8 m/s on Y (LMCF08F, ψ 0.96) and 3.4 m/s on X (LMCF08E, ψ 0.53). Beyond that the drive can't follow, and it trips with `POS_ERROR` (fault 5) at 1.5 rad (14.6 mm). A sagging bus lowers the limit further.

**Braking fills the bus.** With only the drives' own 1410 µF, a fast diagonal stop lifts the bus from 325 to 373 V, 7 V under the drives' 380 V trip.
- 10 mF keeps it at 325 V.
- A chopper at 370 V clips it (`portal_bus.test`).
- Very hard stops regenerate less: at peak current the ironless coils (10.5 Ω per phase on Y) burn the braking energy themselves (1.5 R I²).

**The head is the first accuracy limit.** At 0.5 g the head bends 0.16 mm at mid-span statically. LinuxCNC's trapezoids switch acceleration in steps (infinite jerk), and every corner or wiggle rings the 28 Hz mode. On CAM paths the head swings up to 0.5–0.8 mm, even at 12 m/min (`hwtest/portal-sim`).

## Tests

| Test | Checks |
|---|---|
| `portal_momentum` | free frame, no friction: momentum conserved; the frame recoils against the moves |
| `portal_move` | a 550 × 550 mm diagonal at 0.7 g: following error, contour error < 0.1 mm, the head rings, the frame moves, the floor sees the force, arrives within 5 µm |
| `portal_racking` | the carriage near Y1, Y2, in the middle: the near motor carries more; the bridge racks; symmetric in the middle |
| `portal_bus` | braking raises the bus close to the trip with 1.4 mF, not with 10 mF; the chopper clips it |
