# hwtest/portal-sim: how fast can LinuxCNC drive a portal, and how accurately?

LinuxCNC 2.9 drives the simulated portal from `host/portal`:
- a 1 × 1 m gantry on Baldor ironless linear motors (Y1, Y2: LMCF08F; X: LMCF08E), each on a simulated stmbl drive in the servo thread;
- the frame on its feet, the bridge racking, the tool head on springs (28–55 Hz), and the DC bus from 230 V mains.

LinuxCNC sees a trivkins XYY gantry (joint 0 = X, joints 1 and 2 = Y) with absolute scales.

| File | What |
|---|---|
| `portal.ini`, `portal.hal` | the config; a HAL `sampler` records every servo cycle while a program runs |
| `portal_run.py` | headless display: runs programs, records them (`halsampler`), notes aborts and recovers |
| `gen_path.py` | the benchmark path at a given feed, and its geometry |
| `bench.py` | the sweep: accelerations × feeds, scored against the program (`results.md`, `results.json`) |
| `pareto.py` | the fastest run that meets each accuracy target |
| `replay_study.py` | replays a recording through `portal_sim` with `pcomp`'s physics-based command conditioning |

```sh
make -C ../../host/portal/lcnc all install   # stmbl_portal.so into LinuxCNC
make test     # the path at 12 and 36 m/min, 0.5 g (~3 min)
make bench    # the full sweep (~40 min)
```

Run these as a non-root user. `bench.py` needs numpy, so it runs under the system python3, and it puts LinuxCNC's python on the PATH for the display.

## The benchmark path

About 9 m of cutting in 4,200 G1 segments, with G64 P0.02 blending. Rapids (G0) run at the 1.6 m/s limit.

| Feature | Exercises |
|---|---|
| square spiral | long lines, 90° corners |
| gear, 30 teeth from 1 mm segments | sharp corners every few mm |
| wavy hatch, 0.4 mm segments | the planner handles at most about one segment per servo cycle |
| circles R100 / R20 / R5 (G2) | centripetal acceleration |
| 12-point star | acute corners |

The score is the **path error**: the true tool point, measured relative to the frame (the work), against the programmed geometry, while cutting. The 99th percentile (p99) and the max are reported, exactly, from a spatial index over every segment.

## Results: as LinuxCNC runs it

Fastest feed at each acceleration (the axes' MAX_ACCELERATION; joints 10 % more):

| accel | program time | cutting feed reached | path p99 / max | following error | head swing | frame on its feet | force on the floor |
|---|---|---|---|---|---|---|---|
| 0.05 g | 165 s | 3.6 m/min | 24 / 33 µm | 0.02 mm | 59 µm | 33 µm | 80 N |
| 0.1 g | 116 s | 5.2 m/min | 47 / 69 µm | 0.04 mm | 119 µm | 57 µm | 137 N |
| 0.25 g | 75 s | 8 m/min | 208 / 239 µm | 0.12 mm | 374 µm | 0.2 mm | 480 N |
| 0.5 g | 54 s | 11 m/min | 260 / 566 µm | 0.24 mm | 765 µm | 0.4 mm | 1 kN |
| 1 g | 38 s | 16 m/min | 612 / 925 µm | 0.46 mm | 1.36 mm | 1.1 mm | 2.7 kN |
| 2 g | aborts after 0.4 s: Y2 following error (0.6 mm > MIN_FERROR at low speed) | | | | | | |

- **Acceleration sets the cycle time, not F.** Above about 24 m/min the program takes the same time. At 0.25 g the cutting feed reached is 8 m/min whatever F says, because corners, short segments and arcs keep the planner on its acceleration limit.
- **The head is most of the error.** On the same machine made rigid (`portal_rigid.portal`), the p99 is 56 / 119 / 247 µm at 0.25 / 0.5 / 1 g, so the structure accounts for about three quarters of it. LinuxCNC's trapezoids switch acceleration on instantly (infinite jerk), and every corner kicks the head's mode.
- **The following error is not the accuracy.** It is set by the rapids (0.46 mm at 1 g, for every F), while the path error comes from the head and the servo's response at the segment junctions.
- **The motors are not the limit.** Peak current is under 40 % of the drives' limit at 1 g.

**Fastest run per accuracy target** (pareto.py):

| target, p99 | fastest | time |
|---|---|---|
| 25 µm | 0.05 g, F24 | 165 s |
| 50 µm | 0.1 g, F48 | 116 s |
| 200 µm | 0.25 g, F6 | 115 s |
| 500 µm | 0.5 g, F72 | 54 s |

## Better accuracy at speed: physics-based command conditioning

`host/portal/pcomp.c` sits between the planner and the drives and conditions the command with a model of the machine. Three error mechanisms all look the same at low frequency, a position error proportional to the acceleration:

| Mechanism | Error | Correction |
|---|---|---|
| the head lags its carriage (inertia) | −a/ω², ω the head's mode at the carriage's X | + a/ω² |
| a smoothing filter shrinks the path toward the inside of curves | + ½σ²·a | − ½σ²·a |
| the drives' steady lag (no planner acceleration feedforward over smart-serial) | + τ²·a | − τ²·a |

So the conditioning is:
1. A ZV shaper at the head's x and y modes. The y frequency is the model's at the current X. The same filter goes on both axes, so the path keeps its shape.
2. A 20 ms moving average, so the acceleration ramps instead of stepping. A lead term on a stepping acceleration would jump the command.
3. The leads a·(1/ω² − τ² − σ²/2), from the smoothed command.

It adds about 24 ms of delay, once. A controller delays its laser or spindle I/O by the same amount (`pcomp` reports it).

**Replayed on LinuxCNC's own recordings** (the same planner output and cycle time), path p99 / max in µm. "Conditioned" is the shaper, the 20 ms average and the leads; the better of with or without the servo term (it helps at 0.25 g, is neutral at 1 g):

| accel | as LinuxCNC ran it | ZV shaper | conditioned | conditioned, model 10 % off |
|---|---|---|---|---|
| 0.05 g | 24 / 32 | 29 / 37 | 22 / 30 | 21 / 28 |
| 0.1 g | 49 / 70 | 45 / 57 | 33 / 42 | 32 / 41 |
| 0.25 g | 218 / 249 | 107 / 126 | 44 / 66 | 62 / 78 |
| 0.5 g | 259 / 588 | 212 / 243 | 76 / 112 | 87 / 138 |
| 1 g | 618 / 924 | 415 / 470 | 195 / 251 | 212 / 319 |

- **Same accuracy, much faster.** With conditioning, 0.25 g reaches what took 0.05–0.1 g before, so a 50 µm job takes 75 s instead of 116 s, and 0.5 g holds 76 µm.
- **It beats a stiffer machine.** At 1 g it does better than the same machine made rigid without conditioning (247 µm).
- **Each part is needed.** The shaper alone rounds corners and shrinks circles. The leads alone, on LinuxCNC's stepping acceleration, jump the command and double the current.
- **Steady circles show the servo term best.** On R20 at 0.5 m/s (12.6 m/s²) the contour error goes 366 → 66 µm (`host/portal/tests/portal_pcomp.test`).

**Identifying the model on the machine.**
- A 1 mm step at standstill, then the ring-down read from the scale *or the motor current* alone, finds the head's y mode within 3–6 % at each X. Fitting the beam model to five positions gives 46 / 27 Hz (rails / mid-span; true 55 / 28).
- The x step reads 33 Hz against the head's own 45 Hz: the mode as coupled to the servo-held 5 kg carriage.
- Compensating with the identified model: 224 µm p99 at 1 g (true model 195 µm; as recorded 618 µm).
- τ² comes from a test circle: run it with the conditioning on and adjust τ² until the mean radius is right (1.45e-5 s² here).

## Open points

- **The remaining error at 1 g** (about 200 µm) is transient: the servo's response at segment junctions, and racking (about 100 µrad). Next steps:
  - racking / load-split feedforward per Y motor from the carriage position;
  - planner acceleration feedforward to the drives.
- **A LinuxCNC component:** `pcomp` as a HAL component between motion and the drives, with motion's following-error check moved to the conditioned command. Then the 2 g runs and closed-loop checks on the real path.
- **Jerk-limited planning** would remove the cause instead of shaping after it.
- **All machine numbers are guesses** for a light laser / router portal. Measure the real machine's modes (the step test above) before trusting the absolute values.
