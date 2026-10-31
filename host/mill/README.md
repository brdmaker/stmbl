# mill_sim: a fixed-bridge mill on three simulated stmbl drives

A model of the Scripta CN 720 engraver converted into a small HSC
aluminium machine (DATRON-style: small tools, thin chips, high feeds):
- **Frame and travels:** the stock CN 720, with 700 × 430 mm travel, 200 mm Z, 210 kg in total.
- **Axes:** ball screws driven by SEW CMP40M servos without their gearboxes.
- **Spindle:** an ISO20 ATC spindle.

**Five axes:** the same simulator runs a Mikron HSM 400U retrofit with its A/C trunnion through LinuxCNC's
`xyzac-trt-kins` (`machine/hsm400u*.mill`, `hsm_runs.py`): see [HSM400U.md](HSM400U.md).

The motor data comes from `conf/sew_cmp40m.txt` on the mill branch. The
frame's numbers are estimates. Weigh the table, the X carriage and the Z
slide, and tap-test the head and the bridge, then put the measured values
into `machine/cn720_frame.inc`.

```
make                    # mill_sim (and ../arm/libstmbl_drive.so)
make check              # tests/*.test (also in make -C .. check)
./mill_sim machine/cn720.mill SCRIPT
python3 runs.py         # the sample runs below (RESULTS.md: all of them)
```

The machine file format and the script commands are at the top of
`mill_sim.c`. Scripts play LinuxCNC: a servo period of 1 ms, or `servo 0.5`
for 2 kHz.

## The model (mplant.c)

| part | moves | model |
|---|---|---|
| base | on its feet, x and y (18 / 15 Hz) | takes every reaction |
| table (Y, 40 kg) | on the base; machine Y is the tool relative to the table | pushes the base back |
| bridge | fixed by columns that bend in y (70 Hz with all it carries) | sways when the base is shaken |
| X carriage (12 kg) with the Z slide | along the bridge | |
| Z slide and spindle (15 kg) | vertically, under gravity | brake (`brake z`) clamps the rotor while the drive is off |
| spindle tip (5 kg modal) | springs to the carriage (x) and bridge (y), 120 / 100 Hz with Z up, 90 / 70 Hz with Z down | |

Each axis is modelled the same way:
- **Drive train:** the motor's rotor, with the coupling and the screw's own inertia, drives the nut through a spring. Its stiffness is the screw's E·A/L, with L from the fixed bearing to the nut, so it changes along the travel, in series with the nut, bearing and coupling.
- **Friction:** Coulomb and viscous, on both the motor side and the guides.
- **Lead error:** `lead x 57.5` makes the nut travel 57.5 µm/m more than the motor's turns say. That stands for the screw's lead error, or its growth when warm: 11.5 µm/m per K, so 57.5 µm/m is 5 K.

The tool's position relative to the work:
- **X:** carriage + head swing (hx);
- **Y:** machine Y + bridge sway + head swing (hy);
- **Z:** the slide.

**Feedback:**
- **Resolver:** each motor has one, simulated as a 14-bit encoder with one count of jitter (`rnoise`).
- **Linear scale (optional):** `scale x 10` feeds a 10 µm scale to the drive's `enc_fb1`. The drive's config (`scale10.conf`) uses it as the joint feedback.

The packet from LinuxCNC reaches the drive 50 µs after it is sent (`latency`). The drive takes it at its next 200 µs cycle. At 2 kHz that is alternately 2 and 3 cycles apart, as on the real link.

**Variants:**
- `cn720.mill`: resolvers only (semi-closed loop).
- `cn720_scales.mill`: the position loops close on the 10 µm scales, the velocity loops on the resolvers.
- `cn720_blend.mill`: the loops run on the resolvers, and a correction pulls them to the scales over 0.2 s. This is the new `fb_switch0.joint_tc`.

## Results

The full tables are in RESULTS.md. `econt` is the tool's distance from the commanded path: errors along the path don't count, only those across it. Values are in µm.

| move | resolver 1 kHz | resolver 2 kHz | scales 2 kHz | blend 2 kHz |
|---|---|---|---|---|
| circle R 10 at 6 m/min, max / rms | 12.1 / 1.8 | 8.6 / 1.5 | 9.9 / 4.0 | 10.3 / 3.7 |
| circle R 10 at 12 m/min (0.4 g), max / rms | 16.0 / 4.9 | 10.8 / 3.4 | 11.8 / 5.3 | 14.9 / 4.8 |
| circle R 2 at 3 m/min (small pocket), max / rms | 13.3 / 2.3 | 9.6 / 1.9 | 13.8 / 4.3 | 11.2 / 3.7 |
| X line at 3 m/min, rms | 0.91 | 0.29 | 2.9 | 0.14 |
| 20 mm square at 1 g, exact stop, max | 35.9 | 39.6 | 34.3 | 36.2 |
| the same, S-curve sides, same time | 19.6 | 17.8 | 21.2 | 17.0 |
| the same at 0.5 g | 22.5 | 22.6 | 28.2 | 25.0 |
| X at 650 mm, screw 5 K warm (tool − command) | 38.3 | 37.1 | 4.9 | 4.4 |
| 20 N cutting force (tool − command) | 16.5 | 15.4 | 23.4 | 16.1 |

**What they say:**
1. **2 kHz servo:**
   - **Contours:** the error on circles and small pockets falls by about a quarter to a third.
   - **Lag:** the tool trails the command by 3 µm instead of 20 µm at 3 m/min, because the drive's command is only half as old.
   - **Packet timing:** at 2 kHz the packets reach the drive 2 and 3 drive cycles apart. That makes a 1 kHz sawtooth of up to v × 100 µs in the drive's command, but the motor and mechanics filter it to under 0.02 µm at the tool. No firmware change is needed for it.
2. **Corners:** the S-curve halves the corner error and the head's swing (52 → 23 µm) at the same cycle time. LinuxCNC's trapezoids switch acceleration in one step, which rings the 90–120 Hz head. A jerk-limited planner, or `pcomp`'s shaper in front of the drives, does more for corners than the servo rate does.
3. **10 µm scales as the position feedback:**
   - **Hunting:** the loop hunts within a count, so contours get worse (rms 4 instead of 1.5 µm) and the tool stands 3–4 µm off at rest.
   - **What they're for:** absolute accuracy. A 5 K warm X screw puts the tool 38 µm off at 650 mm on the resolver, and under 5 µm with the scale.
4. **Blend (`fb_switch0.joint_tc`):**
   - **Lines and accuracy:** it combines both strengths on lines and on absolute accuracy: the best line (0.14 µm) and the warm screw within 4.4 µm.
   - **Curves:** with a 0.2 s time constant it follows the screw's wind-up on curves, so circles still get 3.7 µm rms. With 3 s (`drv all fb_switch0.joint_tc = 3`), circles are nearly as good as the resolver's (R 10 at 6 m/min: 2.0 µm rms), but after the 600 mm move with the warm screw the tool is still 23.5 µm off 0.3 s later. It settles over the next few seconds.
   - **The split:** put the lead error, which is fixed along the screw, in LinuxCNC's screw compensation table, measured once with the scale. A blend of 2–3 s then only follows temperature.
5. **The screws limit the loop:** `vel_bw` 500 rad/s (80 Hz) is the most the Y table allows (40 kg on about 45 N/µm). At 800 it rings, and at 1200 the drive trips (`mill_bandwidth.test`). `pos_bw` 60 gives the best contours. These are the configs' defaults.
6. **The frame limits the cut:** at 20 N the tool deflects 15–16 µm, almost all of it in the head (7.5 µm in x, 10.8 µm in y). Thin chips keep forces low. A stiffer head pays off more than the servo does.

## Tests

| test | checks |
|---|---|
| `mill_move` | Z hangs on its brake, then on the gravity feed-forward; a move on every axis follows within 15 µm and lands within 3 µm |
| `mill_brake` | the drive off: the brake holds Z where it was |
| `mill_circle` | circles at 6 and 12 m/min, 2 kHz, within their limits |
| `mill_lead`, `mill_lead_resolver` | the warm screw: 37 µm on the resolver, under 8 µm with the blended scale |
| `mill_bandwidth` | vel_bw 1200: the Y screw mode goes unstable and trips |
| `hsm_rtcp` | the HSM 400U: a tool-centre-point turn of the table and a five-axis line keep the tip within 10 / 15 um in the work; A holds its unbalance at 90 deg |
| `hsm_rapid` | the HSM 400U's 600 V motors on stmbl's ~325 V bus: X at 28 m/min follows, at 34 m/min the drive trips |

## In LinuxCNC (lcnc/, hwtest/mill-sim)

`millcore.c` holds the drives, the plant and the machine file. Two front ends use it:
- `mill_sim`: the scripts above.
- `lcnc/stmbl_mill.c`: a LinuxCNC realtime module, `loadrt stmbl_mill machine=... lib=...`, with per-joint smart-serial pins and the simulation's truth (tool-x/y/z, head-x/y, bridge-y). The servo thread's period is the drives' packet rate.

`hwtest/mill-sim` runs LinuxCNC's real planner on it. On circles, a square and a line, 2 kHz again cuts the path error by about a quarter: circle R 10 at 6 m/min 8.4 → 6.9 µm, the square 27.5 → 20.7 µm. See its README.

## Not modelled yet

- **LinuxCNC's planner:** mill_sim's scripts use exact-stop moves; hwtest/mill-sim runs the real planner.
- **Cutting:** only a steady force, with no chip-load dynamics or chatter.
- **The screw:** no backlash (nuts assumed preloaded), and no whirl at speed.
