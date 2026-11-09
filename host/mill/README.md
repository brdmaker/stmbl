# mill_sim: a fixed-bridge mill on three simulated stmbl drives

The simulator for mills. `machine/bridge.mill` is its generic machine: a fixed bridge with a moving table, ball
screws and servos, 700 × 430 mm travel, 200 mm Z, 210 kg. Its data are those of the Scripta CN 720 conversion;
that machine's own models, its studies and the HSM 400U's five-axis models live in brdmot (`sim/mill`). The same
simulator runs five-axis trunnion machines (B/C or A/C through LinuxCNC's `xyz*-trt-kins`).

The motor data comes from `conf/sew_cmp40m.txt` on the mill branch. The
frame's numbers are estimates.

```
make                    # mill_sim (and ../arm/libstmbl_drive.so)
make check              # tests/*.test (also in make -C .. check)
./mill_sim machine/bridge.mill SCRIPT
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
- `bridge.mill`: resolvers only (semi-closed loop).
- `bridge_blend.mill`: the loops run on the resolvers, and a correction pulls them to the scales over 0.2 s. This is the new `fb_switch0.joint_tc`.

## Tests

| test | checks |
|---|---|
| `mill_move` | Z hangs on its brake, then on the gravity feed-forward; a move on every axis follows within 15 µm and lands within 3 µm |
| `mill_brake` | the drive off: the brake holds Z where it was |
| `mill_circle` | circles at 6 and 12 m/min, 2 kHz, within their limits |
| `mill_lead`, `mill_lead_resolver` | the warm screw: 37 µm on the resolver, under 8 µm with the blended scale |
| `mill_bandwidth` | vel_bw 1200: the Y screw mode goes unstable and trips |

## In LinuxCNC (lcnc/)

`millcore.c` holds the drives, the plant and the machine file. Two front ends use it:
- `mill_sim`: the scripts above.
- `lcnc/stmbl_mill.c`: a LinuxCNC realtime module, `loadrt stmbl_mill machine=... lib=...`, with per-joint smart-serial pins and the simulation's truth (tool-x/y/z, head-x/y, bridge-y). The servo thread's period is the drives' packet rate.

brdmot's `hwtest/cn720-sim` runs LinuxCNC's real planner on the CN 720. On circles, a square and a line, 2 kHz again cuts the path error by about a quarter: circle R 10 at 6 m/min 8.4 → 6.9 µm, the square 27.5 → 20.7 µm. See its README.

## Not modelled yet

- **LinuxCNC's planner:** mill_sim's scripts use exact-stop moves; brdmot's hwtest/cn720-sim runs the real planner.
- **Cutting:** only a steady force, with no chip-load dynamics or chatter.
- **The screw:** no backlash (nuts assumed preloaded), and no whirl at speed.
