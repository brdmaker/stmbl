# hwtest/hsm400u-sim: the Mikron HSM 400U retrofit in LinuxCNC (simulated)

LinuxCNC 2.9 with `xyzac-trt-kins` drives the simulated HSM 400U
(`host/mill`, [HSM400U.md](../../host/mill/HSM400U.md)): the `stmbl_mill`
module runs five simulated stmbl drives and the machine in the servo
thread. The drives carry the machine's Siemens servos (EnDat), as the
simulation guesses them, on stmbl's ~325 V bus. X Y Z run on ball screws
(`hsm400u_scales` / `_blend`: with the glass scales), and A/C on gears with
A's unbalance.

- **Kinematics:** `hsm.hal` sets the pivots and the tool length
  (`xyzac-trt-kins.*-rot-point`, `y/z-offset`, `tool-offset`) to the machine
  file's `trt` line. World mode is the tool centre point: the programs are in
  the work's coordinates.
- **Homing:** absolute encoders, `HOME_ABSOLUTE_ENCODER`, so homing needs no
  motion.
- **Limits:** joint speeds stay within the bus's limit: X Y Z 470 mm/s
  (28 m/min), A 85 deg/s, C 125 deg/s.

```
make test       # rtcp, wline, circle at 1 and 2 kHz (hsm_test.py)
make test-all   # also with the scales blended: results.md
make run        # AXIS
```

## The programs

| program | what it does |
|---|---|
| `rtcp` | The tip holds on a point 30 / 20 mm off the axes while A tilts 0 → 30 and C turns 0 → 90 → 180. |
| `wline` | The tip runs 40 mm along X while A goes 0 → 20 and C 0 → 60. |
| `circle` | The table holds at A 20, C 45 while the tip cuts a 10 mm circle in the work. All five joints move. |

`geom` is the simulated tip, in the work's frame, against the programmed
point, line or circle. `servo` is the tip against LinuxCNC's own command
(its joint commands through the same kinematics).

## Results

[results.md](results.md):
- The tip stays within 10 µm of the geometry at 1 kHz and within 6 µm at
  2 kHz.
- Linear following errors stay under 61 µm, rotary under 37 arcsec.
- No program faults.

All of it rests on guessed machine data.
