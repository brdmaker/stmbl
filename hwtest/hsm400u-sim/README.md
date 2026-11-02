# hwtest/hsm400u-sim: the Mikron HSM 400U retrofit in LinuxCNC (simulated)

LinuxCNC 2.9 with `xyzbc-trt-kins` drives the simulated HSM 400U
(`host/mill`, [HSM400U.md](../../host/mill/HSM400U.md)): the `stmbl_mill`
module runs five simulated stmbl drives and the machine in the servo
thread. The drives carry the machine's Siemens servos (EnDat), as the
simulation guesses them, on stmbl's ~325 V bus. X Y Z run on ball screws
(`hsm400u_scales` / `_blend`: with the glass scales), and the B/C trunnion
(B tilts about Y, +/-110 deg) on gears with B's unbalance.

- **Kinematics:** `hsm.hal` sets the pivots and the tool length
  (`xyzbc-trt-kins.*-rot-point`, `x/z-offset`, `tool-offset`) to the machine
  file's `trtbc` line. World mode is the tool centre point: the programs are
  in the work's coordinates.
- **Layout (the manual's travels):** X 0..400 and Y 0..240 with the table
  centre (the C axis) at X 200, Y 160; Z -350..0, the pallet face at Z -350
  in world coordinates; B +/-110. The B axis 80 mm below the pallet face is
  a guess: replace it, and the C pivot, with the iTNC's MP950 and B pivot.
- **World Z is the gauge line, not the tip:** at B 0 the kinematics give
  world = joints, and `tool-offset` only moves the pivot. In a real config,
  `tool-offset` follows the tool length and the programs use G43.
- **Homing:** absolute encoders, `HOME_ABSOLUTE_ENCODER`, so homing needs no
  motion.
- **Limits:** feed and rapids at most 8000 mm/min (`MAX_LINEAR_VELOCITY =
  133.333`), the owner's limit. The joints keep the bus's limit (X Y Z
  470 mm/s = 28 m/min, A 85 deg/s, C 125 deg/s), which five-axis moves
  can need for the tip's 8000 mm/min.

```
make test       # rtcp, wline, circle at 1 and 2 kHz (hsm_test.py)
make test-all   # also with the scales blended: results.md
make blocks     # block rate and accuracy against the iTNC 530: blocks.md
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
