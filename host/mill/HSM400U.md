# Mikron HSM 400U retrofit: a rough simulation

A first look, before any hardware work, at retrofitting a Mikron HSM 400U
(5-axis HSC machine with an iTNC 530 and a B/C trunnion, Siemens servos,
glass scales on X Y Z; not the linear-motor LP) to stmbl drives and
LinuxCNC. The spindle keeps its own drive, so it isn't simulated here.

**Everything machine-specific is a guess** until the nameplates, the iTNC's
machine parameters and a few measurements replace it
(`machine/hsm400u/`). Read the results as comparisons between variants and
as orders of magnitude, not as the machine's numbers.

```
make                                         # mill_sim (see README.md)
./mill_sim machine/hsm400u.mill SCRIPT       # scripts as for the CN 720, plus the five-axis commands
python3 hsm_runs.py [--md FILE]              # the runs below (~5 min)
```

## The model

`mill_sim` with five simulated stmbl drives (`mplant.c`, `millcore.c`):

| part | as simulated | to replace with |
|---|---|---|
| motors (all five) | Siemens 1FK6-class, wound for a 600 V DC link: 8 poles, psi 0.214 (Kt 1.29 Nm/A peak), R 1.2 ohm, L 9 mH, 15 A peak, 3000 rpm (`1fk6.conf`) | each nameplate: type, M0, I0, n_N, kE; the catalogue's R, L, J |
| motor encoders | EnDat (EQN 1325-like) as an absolute 2^18 encoder | the order code: EnDat 2.1 or 2.2, resolution |
| bus | stmbl's: 230 V mains rectified, ~325 V, sagging under load (`hv0.sim_supply`) | - |
| X Y Z | 32 x 16 mm ball screws, direct; Y carries the table with the trunnion (350 kg), X the carriage with Z and the spindle (215 kg), Z 95 kg on gravity feed-forward and a brake. Travels from the manual: X 400, Y 240, Z 350 | the manual says toothed belts to the screws: the belt ratio and the screw pitch; masses |
| scales | glass scales (LS 4x6-like) read at 0.1 um by the drive's second encoder input | the scales' model numbers (incremental 1 Vpp or absolute EnDat) |
| B | the trunnion's tilt about Y, +/-110 deg (the manual), through a 1:120 gear (stiffness 1.5e6 Nm/rad, 4 kg m^2 turning), an unbalance of 2 kg m | the drive (the manual's 150 rpm and absolute encoders "on the rotor" suggest a direct drive), the trunnion's mass and balance |
| C | the rotary table through a 1:80 gear (8e5 Nm/rad, 0.4 kg m^2) | the same (the manual: 250 rpm) |
| kinematics | LinuxCNC 2.9's `xyzbc-trt-kins` (its trtfuncs.c), `trtbc 200 160 -450 0 -80 100`: the C axis at the table centre, X 200 / Y 160 (the manual's travels about it: X +/-200, Y -160 / +80); the pallet face at Z -350 in world coordinates (the manual's 100 mm least spindle-to-table distance [uncertain]); the B axis 80 mm below the pallet face, crossing the C axis [guess]; a 100 mm tool | MP950 (the C pivot, cycle 307's log) and the B pivot from the iTNC; then measure them |
| frame | the CN 720's layout (table on Y, a bridge with X, Z) with heavier, stiffer parts, head modes 120-180 Hz | weigh, tap-test |

What the five-axis part adds: the rotary axes' gears (motor, torsional
stiffness, the load's inertia and friction, A's unbalance torque), and
judging the tool **in the work's frame**: the simulated tool position and
table angles through the same transform LinuxCNC uses (`trtbc` mirrors
`xyzbc-trt-kins`, `trt` `xyzac-trt-kins`), against the
commanded joints through it. `wmove` and `rtcp` command tool-centre-point
moves the way LinuxCNC's world mode does (the joints by the inverse
kinematics every servo period).

## Results

`ework`: the tool tip against the command in the work's frame (lag
included); `econtw`: its distance from the commanded path; `econt`: the XY
contour error; um, arcsec, A. Variants: the position loops on the motors'
EnDat, on the scales, or blended (the loops on the EnDat, pulled to the
scales over 0.5 s); 1 or 2 kHz servo; the stmbl bus or a stiff 380 V one.

**Standing still, 1 s (the tip in the work)**

| variant | rms_ework | max_ework | rms_etool |
|---|---|---|---|
| EnDat 1k | 0.117 | 0.13 | 0.127 |
| EnDat 2k | 0.168 | 0.183 | 0.127 |
| scales 1k | 0.0214 | 0.0274 | 0.0355 |
| scales 2k | 0.0634 | 0.0771 | 0.0316 |
| blend 1k | 0.0313 | 0.0377 | 0.0421 |
| EnDat 380V 1k | 0.117 | 0.13 | 0.127 |

**X rapid 380 mm at 25 m/min, 0.1 trapezoid ramps (~0.7 g)**

| variant | max_ferr_x | max_cur_x | max_econt | etool |
|---|---|---|---|---|
| EnDat 1k | 31.8 | 6.06 | 1.12 | 0.147 |
| EnDat 2k | 48.2 | 5.54 | 1.56 | 0.164 |
| scales 1k | 37.5 | 6.11 | 1.27 | 0.0242 |
| scales 2k | 52.8 | 5.6 | 1.26 | 0.0121 |
| blend 1k | 34.2 | 6.03 | 0.968 | 0.248 |
| EnDat 380V 1k | 29.4 | 6.06 | 1.12 | 0.148 |

**XY circle R 25 mm at 5 m/min, 2nd and 3rd turn**

| variant | max_econt | rms_econt | max_hx | max_hy |
|---|---|---|---|---|
| EnDat 1k | 3.01 | 0.336 | 1.07 | 0.087 |
| EnDat 2k | 2.15 | 0.325 | 0.798 | 0.103 |
| scales 1k | 2.84 | 0.401 | 1.07 | 0.0898 |
| scales 2k | 2.05 | 0.419 | 0.806 | 0.106 |
| blend 1k | 3.09 | 0.406 | 1.07 | 0.088 |
| EnDat 380V 1k | 2.99 | 0.336 | 1.06 | 0.086 |

**XY circle R 10 mm at 10 m/min (0.28 g)**

| variant | max_econt | rms_econt | max_hx | max_hy |
|---|---|---|---|---|
| EnDat 1k | 16.8 | 1.76 | 7.84 | 0.227 |
| EnDat 2k | 7.25 | 1.24 | 3.18 | 0.382 |
| scales 1k | 14.3 | 3.82 | 7.81 | 0.239 |
| scales 2k | 6.14 | 3.84 | 3.2 | 0.394 |
| blend 1k | 16.8 | 1.77 | 7.82 | 0.229 |
| EnDat 380V 1k | 16.8 | 1.76 | 7.87 | 0.235 |

**20 mm square, exact stop at 0.5 g per side**

| variant | max_econt | rms_econt | max_hx | max_hy |
|---|---|---|---|---|
| EnDat 1k | 20.2 | 2.81 | 14.6 | 2.37 |
| EnDat 2k | 23.2 | 2.88 | 13.2 | 2.11 |
| scales 1k | 26.3 | 3.41 | 14.8 | 2.42 |
| scales 2k | 27.2 | 3.49 | 13.4 | 2.16 |
| blend 1k | 20.1 | 2.79 | 14.6 | 2.37 |
| EnDat 380V 1k | 20.2 | 2.81 | 14.6 | 2.38 |

**X 300 mm with a 5 K warm screw (57.5 um/m), the error at the end**

| variant | etool | tx | cx |
|---|---|---|---|
| EnDat 1k | 20.1 | 350 | 350 |
| EnDat 2k | 20.1 | 350 | 350 |
| scales 1k | 0.0568 | 350 | 350 |
| scales 2k | 0.0489 | 350 | 350 |
| blend 1k | 1.11 | 350 | 350 |
| EnDat 380V 1k | 20.1 | 350 | 350 |

**Tool centre point: B 0 -> 30, C 0 -> 90 in 3 s, the tip 30 / 20 mm off the C axis, 50 mm above the pallet (in the work)**

| variant | max_ework | max_econtw | ework | max_ferr_b | max_ferr_c |
|---|---|---|---|---|---|
| EnDat 1k | 6.76 | 6.76 | 2.63 | 5.11 | 7.76 |
| EnDat 2k | 6.61 | 6.61 | 2.51 | 5.09 | 12.8 |
| scales 1k | 6.4 | 6.4 | 1.54 | 5.15 | 7.78 |
| scales 2k | 6.36 | 6.36 | 1.46 | 5.1 | 12.6 |
| blend 1k | 6.65 | 6.65 | 1.8 | 5.15 | 7.78 |
| EnDat 380V 1k | 6.76 | 6.76 | 2.62 | 5.11 | 7.76 |

**Five-axis line: the tip 40 mm along the work's X while B 0 -> 20, C 0 -> 60, 2 s**

| variant | max_ework | max_econtw | ework |
|---|---|---|---|
| EnDat 1k | 17.5 | 7.18 | 3.02 |
| EnDat 2k | 7.09 | 7.09 | 2.89 |
| scales 1k | 17.9 | 6.22 | 1.91 |
| scales 2k | 6.22 | 6.22 | 1.9 |
| blend 1k | 17.9 | 6.57 | 2.13 |
| EnDat 380V 1k | 17.5 | 7.17 | 3.02 |

**B to 90 deg (the unbalance at its largest), held 1 s**

| variant | max_ferr_b | max_cur_b | max_ework |
|---|---|---|---|
| EnDat 1k | 1.24 | 0.146 | 3.7 |
| EnDat 2k | 1.21 | 0.151 | 3.62 |
| scales 1k | 1.24 | 0.146 | 3.76 |
| scales 2k | 1.24 | 0.151 | 3.7 |
| blend 1k | 1.24 | 0.146 | 3.76 |
| EnDat 380V 1k | 1.24 | 0.146 | 3.7 |

**Steady 100 N cutting force on the tool in X and Y**

| variant | etool | max_hx | max_hy |
|---|---|---|---|
| EnDat 1k | 10.9 | 5.89 | 7.55 |
| EnDat 2k | 10.9 | 5.89 | 7.55 |
| scales 1k | 10.3 | 5.89 | 7.55 |
| scales 2k | 10.3 | 5.89 | 7.55 |
| blend 1k | 10.4 | 5.89 | 7.55 |
| EnDat 380V 1k | 10.9 | 5.89 | 7.55 |

**The fastest X rapid (380 mm, 0.1 trapezoid ramps): no fault, following error under 100 um**

| bus | m/min | its following error [um] | the next step |
|---|---|---|---|
| ~325 V (230 V mains rectified) | 28 | 28.2 | 30 m/min: 257 um |
| 380 V (stiff) | 36 | 56.7 | 38 m/min: fault |


## What it says

1. **The bus limits the rapids.** The Siemens motors are wound for a 600 V DC
   link; on stmbl's ~325 V the voltage runs out at about 1750 rpm: X (16 mm
   screw) rapids top out at **~28 m/min**, at 30 the axis lags by a quarter
   of a millimetre and above that the drive trips. A 380 V bus (the most an
   stmbl HV board takes; it trips at 410 V) gives ~36 m/min. The original
   drives on 600 V would reach the motors' 3000 rpm (48 m/min). For a 400U
   that mostly cuts at moderate feeds this may be acceptable; it is the
   first thing to check against the real kE.
   **The owner limits feed and rapids to 8000 mm/min**, well under the
   ~28 m/min the ~325 V bus allows: the bus is no obstacle then, and a
   380 V bus isn't needed. The acceleration limit still matters.
2. **C (and B) too:** C's 90 deg in 1 s (169 deg/s peak) already needs
   2250 motor rpm through 1:80 and trips; C's rapid is about 150 deg/s.
3. **The glass scales are worth keeping.** A 5 K warm X screw puts the tool
   20 um off at 300 mm on the EnDat alone; on the scales 0.06 um, blended
   1.1 um. Read at 0.1 um they don't hunt the way the CN 720's 10 um scales
   did, but they let the screw's compliance into the loop: the small circle
   at 10 m/min gets a worse rms (3.8 vs 1.8 um). Blending gives both, as on
   the CN 720.
4. **2 kHz helps contours:** the R 10 circle at 10 m/min 17 -> 8 um, the
   five-axis line 17 -> 7 um.
5. **Five-axis works as LinuxCNC does it:** a tool-centre-point turn of the
   table (B 30 deg, C 90 deg in 3 s) keeps the tip within ~7 um; B holds its
   unbalance at 90 deg within ~1 arcsec.
6. **The frame:** 100 N on the tool deflects it ~11 um, mostly the head.

## To make it the real machine

- Motor nameplates (or the iTNC's motor table): kE decides the rapids.
- The iTNC 530's machine parameters (back them up before anything is
  removed): axis directions, scale signal periods, reference marks, the
  rotary axes' gear ratios and the kinematics' pivot distances (MP950 for
  C; the B pivot), and the machine datum, to convert them to the
  simulation's frame (C axis at X 200 / Y 160).
- The scales' and EnDat encoders' model numbers.
- Masses (weigh), the head's and the frame's modes (tap test).

The same machine runs under LinuxCNC (`hwtest/hsm400u-sim`).
