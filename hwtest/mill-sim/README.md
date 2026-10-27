# hwtest/mill-sim: LinuxCNC on the simulated Scripta CN 720

LinuxCNC drives the simulated CN 720 conversion from `host/mill`:
- **Layout:** a fixed bridge with the table moving in Y; trivkins XYZ, one joint per axis. Machine Y is the tool relative to the table.
- **Drives:** three simulated stmbl drives with SEW CMP40M servos on ball screws. The `stmbl_mill` module runs them and the machine's mechanics in the servo thread: base on its feet, swaying bridge, the spindle tip's modes, Z on its brake.
- **Limits:** travels X 0..700, Y 0..430, Z −200..0. Speeds 30 m/min on X and Y, 15 m/min on Z; 5 m/s² (0.5 g).

```
make -C ../../host/mill/lcnc all install   # the module (and libstmbl_drive.so)
make test        # 1 and 2 kHz on the resolvers, checked (~1 min)
make test-all    # also blended scales and scales: results.md (~3 min)
make run         # LinuxCNC with AXIS
```

Run it as a user other than root (rtapi_app refuses root), for example
`su lcnc -c "TMPDIR=/tmp make test"`.

**Choosing the setup in `mill.ini`:**
- `[MILL] FILE` picks the machine file:
  - `cn720.mill`: resolvers;
  - `cn720_blend.mill`: 10 µm scales blended in with `fb_switch0.joint_tc`;
  - `cn720_scales.mill`: the position loops close on the scales.
- `[EMCMOT] SERVO_PERIOD` 500000 runs a 2 kHz servo thread. `stmbl_mill` sets the drives' `vel_int0.cmd_freq` from the thread's period. A real drive needs `vel_int0.cmd_freq = 2000` in its config for 2 kHz.

**How the test works:** `mill_test.py` copies the config to /tmp per variant and runs five programs through `mill_run.py`, a headless display. The HAL sampler records every servo cycle. While cutting (feed moves at Z −25), it measures:
- **path:** the true tool, relative to the work, against the programmed geometry.
- **servo:** the tool against LinuxCNC's own commanded path.
- **following error:** LinuxCNC's largest, plus the motor current, the head's deflection and the bridge's sway.

## Results

All in µm (results.md has every run). Path error, max / 99th percentile:

| program | resolver 1 kHz | resolver 2 kHz | blend 2 kHz | scales 2 kHz |
|---|---|---|---|---|
| circle R 10 at 6 m/min | 8.4 / 6.2 | 6.9 / 4.5 | 10.0 / 6.2 | 9.4 / 7.4 |
| circle R 10 at 12 m/min | 15.0 / 11.8 | 10.5 / 8.8 | 13.8 / 11.6 | 13.9 / 11.8 |
| circle R 2 at 3 m/min | 8.1 / 7.3 | 6.7 / 5.9 | 11.5 / 11.0 | 12.7 / 10.5 |
| 20 mm square, G61, 15 m/min | 27.5 / 20.9 | 20.7 / 17.8 | 22.7 / 15.2 | 27.5 / 17.9 |
| X line at 3 m/min | 2.3 / 2.2 | 1.0 / 1.0 | 4.1 / 4.0 | 3.4 / 3.4 |

What the results show:
- **2 kHz:** about a quarter less path error on every program. It agrees with host/mill's scripted runs.
- **Scales:** blended scales, and scales closing the loop, cost a few µm on these short moves. Their gain is absolute accuracy over the travel, like a warm screw, which these programs don't test (host/mill: `lead`).
- **Following error:** 25–60 µm, which is the drives' lag (the 1 mm limit is far off).
- **Motor current:** at most 1.3 A of the drives' 8.5 A.
- **The square:** its corners swing the spindle tip up to 40 µm and the bridge up to 24 µm. That's the path's largest error at 0.5 g.

**The test's bounds** (`mill_test.py`):
- path error and servo error under 40 µm;
- following error under 100 µm;
- no samples lost.

## Notes

- **A metric artifact:** at the first cutting samples of a program, the servo error can read up to 20 µm. Around the plunge the XY command path is still nearly a point, so the window's polyline is short. Read the path error for the machine's accuracy there.
- **Homing:** homing is `HOME_ABSOLUTE_ENCODER`, which needs no motion. The drives report absolute positions, as the machine file's `home` sets them.
