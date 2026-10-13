# Delta ECMA-C30604FS on STMBL V4

Drive config (`stmbl.conf`) for the burn-in bench motor, as dumped from the
drive on 2026-10-14. Firmware: v0.9.18-225-gc2914ca7. DC bus: ~297 V.

## Motor

From the nameplate and the Delta ASDA-B2 manual. The spec table there covers
the C1/C2 versions of this frame; this C3 is the older ASDA-AB variant.

| | |
|---|---|
| Model code | ECMA-**C**: 220 V / 3000 rpm, **3**: 2500 ppr encoder, **06**: 60 mm, **04**: 400 W, **F**: keyway, with brake, no oil seal, **S**: standard shaft |
| Rated | 400 W, 3000 rpm, 1.27 Nm, 2.6 A rms, label voltage 110 V |
| Max speed (B2 table) | 5000 rpm; not verified for the C3, so this config stays at 3000 rpm |
| Poles | 10 (5 pole pairs) |
| Encoder | 2500 lines = **10000 counts/rev** incremental, with index |
| Brake | yes, on `io0.out0` (`fault0.mot_brake`) |
| Datasheet Kt / Ke | 0.49 Nm/A rms / 17.4 mV/rpm |
| Datasheet rotor inertia | 0.277e-4 kg m^2 (0.30e-4 with brake) |

## Config values and where they came from

| Value | Setting | Source |
|---|---|---|
| encoder | `mot_fb_res = 10000`, `mot_fb_rev = 1` | model code; confirmed with `enc_fb0.indexprint` (counter steps 10000 per index) |
| poles | `polecount = 5` | id_pmsm |
| commutation | `mot_fb_offset = -0.630835` | id_pmsm |
| electrical | `r = 2.15`, `l = 0.023`, `psi = 0.0474` | id_pmsm. psi agrees with the datasheet Kt/Ke within 1-3%; r and l do not match the datasheet (cable, definition, rough L method) |
| mechanical | `j = 0.000044`, `f = 0.038`, `o = 0.0001` | id_mot (least-squares fit). j is ~47% above the datasheet with-brake value |
| damping | `d = 0.0001` | speed sweep 20-157 rad/s. Slightly high at 3000 rpm; ~0.00005 fits better |
| loop | `pos_bw = 42.8`, `vel_bw = 500`, `vel_d = 1` | id_pid (vel_bw and vel_d at their caps) |
| limits | `max_vel = 320` (~3050 rpm), `max_force = 3`, `max_ac_cur = 10` | rated speed plus margin for position correction |
| LinuxCNC | `linrev0.scale = 10` (10 mm per motor rev) | matches `../stmbl.ini` (500 mm/s = 3000 rpm) |

Two early lines (`conf0.j = 0.000026`, `conf0.polecount = 4`) are overridden
by later lines in the same file; they are kept so the file matches the
drive byte for byte.

## Measured performance (LinuxCNC run, 2026-10-14)

- Reached 3028 rpm; measured speed 1.003 x command at top speed
- Torque at constant speed 0.034-0.056 Nm, the same in both directions
- Following error at constant speed 9-15 mrad rms (0.015-0.024 mm);
  max 0.2 mm during acceleration
- No faults. Small random feedback-torque bumps (~0.05 Nm, ~1 per 3 revs,
  not angle-locked), consistent with the worn bearing that can be heard
  when the shaft is turned by hand

## Loading

```sh
cd servoterm-cli
./servoterm-cli --load ../hwtest/burnin/ECMA-C30604FS/stmbl.conf -c reset
```

The old config used the default 16384 counts/rev and 8 "pole pairs". That
caused a click once per revolution, extra torque in one direction, and a
following-error fault at speed. See DEBUGNOTES.md (2026-10-14).
