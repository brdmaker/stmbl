# arm_sim: six simulated drives on a simulated arm

`arm_sim` runs one simulated stmbl drive per joint and couples them to a
rigid-body arm with elastic, backlashed gears (`plant.c`, dynamics from
`shared/robot`). Each drive is its own copy of `libstmbl_drive.so`, loaded
with `dlmopen` into a separate namespace, so the firmware HAL (one set of
globals) needs no change. The drive runs its real config with
`hv0.sim_ext = 1`: the electrical side (current loop, back-EMF) stays in
the drive, the rotor and everything behind it are in the plant.

```
make                 # libstmbl_drive.so, arm_sim
make check           # tests/*.test (also part of make -C .. check)
./arm_sim arcmate/arcmate.arm SCRIPT
```

Per 200 µs rt cycle: rotor angle and speed into each drive (`hv0.sim_pos`,
`hv0.sim_vel`), one drive tick, the mean electrical torque back
(`hv0.sim_te_int`), then the plant in four 50 µs steps. Per 1 ms the
LinuxCNC side sends each drive its joint command over the simulated
smart-serial link, in joint degrees (`sserial` scale = 360 / ratio), with a
per-joint offset set when it first sees the drive (absolute encoders).

The plant, per joint: gear spring and damper with backlash between the
motor (through the transmission matrix, gear ratio and wrist coupling) and
the link; gearbox friction (Coulomb, Stribeck, viscous) on the link, rotor
bearing friction and the motor brake on the rotor, both applied so they
stop motion but never reverse it; hard stops; an external force on any
link. Encoders are absolute (`enc_fb0.sim_absolute = 1`), as FANUC's.

`arcmate/` is the ARC Mate A05B-1203-B255 with **guessed** values (see
`hwtest/arcmate/`); replace them as `RECORD.md` fills up. Script commands
are listed at the top of `arm_sim.c`.

Tests: `arm_move` (hold, a 1.5 s move of all joints, brakes on disable),
`arm_energy` (energy conserved in a free fall through the backlash),
`arm_force` (gear torques change by -Jᵀf under a push), `arm_brake`
(brakes hold; without J2's the arm falls onto its stop).
