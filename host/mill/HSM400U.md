# Mikron HSM 400U retrofit: a rough simulation

A first look, before any hardware work, at retrofitting a Mikron HSM 400U
(5-axis HSC machine with an iTNC 520, Siemens servos with EnDat encoders,
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
| X Y Z | 32 x 16 mm ball screws; Y carries the table with the trunnion (350 kg), X the carriage with Z and the spindle (215 kg), Z 95 kg on gravity feed-forward and a brake | screw pitch and size, masses, travels |
| scales | glass scales (LS 4x6-like) read at 0.1 um by the drive's second encoder input | the scales' model numbers (incremental 1 Vpp or absolute EnDat) |
| A | the trunnion's tilt through a 1:120 gear (stiffness 1.5e6 Nm/rad, 4 kg m^2 turning), an unbalance of 2 kg m | gear ratio and type, the trunnion's mass and balance |
| C | the rotary table through a 1:80 gear (8e5 Nm/rad, 0.4 kg m^2) | the same |
| kinematics | LinuxCNC 2.9's `xyzac-trt-kins` (its trtfuncs.c): the A axis at Y 225, Z -250, the C axis at X 200, its table 80 mm above A, a 100 mm tool | the pivot distances (the iTNC's kinematics table holds them) |
| frame | the CN 720's layout (table on Y, a bridge with X, Z) with heavier, stiffer parts, head modes 120-180 Hz | weigh, tap-test |

What the five-axis part adds: the rotary axes' gears (motor, torsional
stiffness, the load's inertia and friction, A's unbalance torque), and
judging the tool **in the work's frame**: the simulated tool position and
table angles through the same transform LinuxCNC uses, against the
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
| EnDat 1k | 0.173 | 0.184 | 0.156 |
| EnDat 2k | 0.164 | 0.174 | 0.156 |
| scales 1k | 0.035 | 0.0471 | 0.0334 |
| scales 2k | 0.0324 | 0.0447 | 0.0334 |
| blend 1k | 0.0358 | 0.0476 | 0.0339 |
| EnDat 380V 1k | 0.172 | 0.185 | 0.156 |

**X rapid 380 mm at 25 m/min, 0.1 trapezoid ramps (~0.7 g)**

| variant | max_ferr_x | max_cur_x | max_econt | etool |
|---|---|---|---|---|
| EnDat 1k | 31.8 | 6.06 | 1.12 | 0.176 |
| EnDat 2k | 48.2 | 5.54 | 1.57 | 0.186 |
| scales 1k | 37.5 | 6.11 | 1.27 | 0.0206 |
| scales 2k | 52.8 | 5.6 | 1.26 | 0.0195 |
| blend 1k | 34.2 | 6.03 | 0.968 | 0.245 |
| EnDat 380V 1k | 29.4 | 6.06 | 1.13 | 0.173 |

**XY circle R 25 mm at 5 m/min, 2nd and 3rd turn**

| variant | max_econt | rms_econt | max_hx | max_hy |
|---|---|---|---|---|
| EnDat 1k | 3 | 0.373 | 1.06 | 0.0883 |
| EnDat 2k | 2.36 | 0.362 | 0.792 | 0.105 |
| scales 1k | 2.84 | 0.405 | 1.07 | 0.0913 |
| scales 2k | 2.08 | 0.424 | 0.8 | 0.107 |
| blend 1k | 3.08 | 0.439 | 1.07 | 0.0899 |
| EnDat 380V 1k | 2.99 | 0.373 | 1.06 | 0.0882 |

**XY circle R 10 mm at 10 m/min (0.28 g)**

| variant | max_econt | rms_econt | max_hx | max_hy |
|---|---|---|---|---|
| EnDat 1k | 16.8 | 1.78 | 7.82 | 0.227 |
| EnDat 2k | 7.93 | 1.3 | 3.17 | 0.389 |
| scales 1k | 14.4 | 3.79 | 7.8 | 0.238 |
| scales 2k | 6.19 | 3.81 | 3.19 | 0.403 |
| blend 1k | 16.8 | 1.81 | 7.81 | 0.228 |
| EnDat 380V 1k | 16.8 | 1.78 | 7.85 | 0.233 |

**20 mm square, exact stop at 0.5 g per side**

| variant | max_econt | rms_econt | max_hx | max_hy |
|---|---|---|---|---|
| EnDat 1k | 20.1 | 2.81 | 14.6 | 2.38 |
| EnDat 2k | 23 | 2.88 | 13.2 | 2.12 |
| scales 1k | 27.5 | 3.46 | 14.7 | 2.44 |
| scales 2k | 27.4 | 3.53 | 13.4 | 2.17 |
| blend 1k | 20 | 2.8 | 14.6 | 2.38 |
| EnDat 380V 1k | 20.1 | 2.81 | 14.6 | 2.39 |

**X 300 mm with a 5 K warm screw (57.5 um/m), the error at the end**

| variant | etool | tx | cx |
|---|---|---|---|
| EnDat 1k | 20.1 | 350 | 350 |
| EnDat 2k | 20.1 | 350 | 350 |
| scales 1k | 0.0581 | 350 | 350 |
| scales 2k | 0.05 | 350 | 350 |
| blend 1k | 1.11 | 350 | 350 |
| EnDat 380V 1k | 20.1 | 350 | 350 |

**Tool centre point: A 0 -> 30, C 0 -> 90 in 3 s, the tip 30 / 20 mm off the axes (in the work)**

| variant | max_ework | max_econtw | ework | max_ferr_a | max_ferr_c |
|---|---|---|---|---|---|
| EnDat 1k | 4.99 | 4.99 | 0.888 | 5.11 | 7.76 |
| EnDat 2k | 5 | 5 | 0.934 | 5.09 | 12.8 |
| scales 1k | 5.35 | 5.35 | 1.05 | 5.15 | 7.78 |
| scales 2k | 5.24 | 5.24 | 0.996 | 5.1 | 12.6 |
| blend 1k | 5.67 | 5.67 | 1.28 | 5.15 | 7.78 |
| EnDat 380V 1k | 4.99 | 4.99 | 0.883 | 5.11 | 7.76 |

**Five-axis line: the tip 40 mm along the work's X while A 0 -> 20, C 0 -> 60, 2 s**

| variant | max_ework | max_econtw | ework |
|---|---|---|---|
| EnDat 1k | 15.5 | 6.01 | 0.603 |
| EnDat 2k | 6.05 | 6.05 | 0.634 |
| scales 1k | 16.3 | 6.52 | 0.793 |
| scales 2k | 6.52 | 6.52 | 0.777 |
| blend 1k | 16.3 | 6.73 | 1.02 |
| EnDat 380V 1k | 15.5 | 6.02 | 0.603 |

**A to 90 deg (the unbalance at its largest), held 1 s: the tip 100 mm from the A axis**

| variant | max_ferr_a | max_cur_a | max_ework |
|---|---|---|---|
| EnDat 1k | 1.24 | 0.146 | 1.28 |
| EnDat 2k | 1.21 | 0.151 | 1.22 |
| scales 1k | 1.24 | 0.146 | 1.11 |
| scales 2k | 1.24 | 0.151 | 1.09 |
| blend 1k | 1.24 | 0.146 | 1.1 |
| EnDat 380V 1k | 1.24 | 0.146 | 1.28 |

**Steady 100 N cutting force on the tool in X and Y**

| variant | etool | max_hx | max_hy |
|---|---|---|---|
| EnDat 1k | 11 | 5.87 | 7.52 |
| EnDat 2k | 11 | 5.87 | 7.52 |
| scales 1k | 10.2 | 5.87 | 7.52 |
| scales 2k | 10.2 | 5.87 | 7.52 |
| blend 1k | 10.4 | 5.87 | 7.52 |
| EnDat 380V 1k | 11 | 5.87 | 7.52 |

**The fastest X rapid (380 mm, 0.1 trapezoid ramps): no fault, following error under 100 um**

| bus | m/min | its following error [um] | the next step |
|---|---|---|---|
| ~325 V (230 V mains rectified) | 28 | 28.2 | 30 m/min: 257 um |
| 380 V (stiff) | 36 | 56.9 | 38 m/min: fault |


## What it says

1. **The bus limits the rapids.** The Siemens motors are wound for a 600 V DC
   link; on stmbl's ~325 V the voltage runs out at about 1750 rpm: X (16 mm
   screw) rapids top out at **~28 m/min**, at 30 the axis lags by a quarter
   of a millimetre and above that the drive trips. A 380 V bus (the most an
   stmbl HV board takes; it trips at 410 V) gives ~36 m/min. The original
   drives on 600 V would reach the motors' 3000 rpm (48 m/min). For a 400U
   that mostly cuts at moderate feeds this may be acceptable; it is the
   first thing to check against the real kE.
2. **C (and A) too:** C's 90 deg in 1 s (169 deg/s peak) already needs
   2250 motor rpm through 1:80 and trips; C's rapid is about 150 deg/s.
3. **The glass scales are worth keeping.** A 5 K warm X screw puts the tool
   20 um off at 300 mm on the EnDat alone; on the scales 0.06 um, blended
   1.1 um. Read at 0.1 um they don't hunt the way the CN 720's 10 um scales
   did, but they let the screw's compliance into the loop: the small circle
   at 10 m/min gets a worse rms (3.8 vs 1.8 um). Blending gives both, as on
   the CN 720.
4. **2 kHz helps contours:** the R 10 circle at 10 m/min 17 -> 8 um, the
   five-axis line 15 -> 6 um.
5. **Five-axis works as LinuxCNC does it:** a tool-centre-point turn of the
   table (A 30 deg, C 90 deg in 3 s) keeps the tip within ~5 um; A holds its
   unbalance at 90 deg within ~1 arcsec.
6. **The frame:** 100 N on the tool deflects it ~11 um, mostly the head.

## To make it the real machine

- Motor nameplates (or the iTNC's motor table): kE decides the rapids.
- The iTNC 520's machine parameters (back them up before anything is
  removed): axis directions, scale signal periods, reference marks, the
  rotary axes' gear ratios and the kinematics' pivot distances.
- The scales' and EnDat encoders' model numbers.
- Masses (weigh), the head's and the frame's modes (tap test).

The same machine runs under LinuxCNC (`hwtest/hsm400u-sim`).
