# hwtest/soak: long-term reliability runs

A soak run moves the burn-in machine's X axis through `soak.ngc` over and
over (LinuxCNC, AUTO mode, about 36 s per cycle). Between cycles it checks
the drive over its USB console. The same runner works with the simulated
drive (`hwtest/burnin-sim`, `stmbl_sim`) and with the real one
(`hwtest/burnin`, hm2 + smart-serial), so a run on the sim and a run on the
bench can be compared line by line.

```sh
cd hwtest/soak
./run_soak.sh sim --hours 8                      # simulated drive
./run_soak.sh hw  --hours 8 --console-every 10   # real drive on /dev/ttyACM0
./run_soak.sh console --hours 2                  # idle drive, no LinuxCNC
touch runs/<run>/STOP                            # end a run cleanly
```

A slower motor or an unreliable USB console: `SOAK_MAX_VEL=300` caps the
machine's top speed (mm/s) in the run's copy of the ini, `SOAK_CONSOLE=sserial`
(hw) checks the drive over the smart-serial console (sserialterm in
LinuxCNC's HAL) instead of USB. The SEW CMP40M on the 202 V bench:
`SOAK_MAX_VEL=300 SOAK_CONSOLE=sserial ./run_soak.sh hw --hours 4`.

`run_soak.sh` starts LinuxCNC headless, from a copy of the machine config in
the run directory, with `soak.py` in place of the GUI. Nothing appears on the
desktop. For `sim`, that copy also holds the simulated drive's flash, so a
run never changes `hwtest/burnin-sim/sim.conf`. LinuxCNC and halrun must not
already be running.

## What is checked

| When | Check | Event (FAIL unless noted) |
|---|---|---|
| During each cycle | LinuxCNC following error, 50 Hz | `ferror` over `--max-ferror` (0.5 mm); LinuxCNC errors (`lcnc_message`), for example its own following-error limit. LinuxCNC's "Unexpected realtime delay" (the Pi's servo thread was late) is `host_rt_delay`, info only. |
| After each cycle | following error at rest after `--settle` (0.5 s) | `ferror_rest` over `--max-ferror-rest` (0.02 mm) |
| After each cycle | `hal`: HAL state, rt load and worst case | `hal_state` not `HAL_OK2` (with the full `hal` output); `rt_load_high` (info) above 90% of the period |
| After each cycle | `uptime` | `drive_reboot` when it goes backwards |
| After each cycle | `fault0.state`, `fault0.fault` | `drive_fault` with the fault name |
| After each cycle | `hv0.dc_volt` | `dc_volt` more than `--dc-tol` (5%) off the first reading |
| After each cycle | `hv0.hv_temp`, `iit0.temp` | over `--max-hv-temp` (70 °C), `--max-mot-temp` (80 °C) |
| Any time | text the drive sends on its own | `ERROR …` lines; `INFO …` lines are logged as info |
| Any time | console answers | `console_no_answer`; USB gone: reconnect for 30 s, else the run ends |
| Every `--config-every` (10) cycles | `showconf` compared with the first read | `config_changed` if it differs twice; one bad read that a re-read fixes is `config_readback` (a transfer error; info unless `--strict-console`) |
| Every `--console-every` cycles (off) | an 88-character line, then `conf0.r` | `console_long_line` unless the drive drops the line with its error and still answers. Needs firmware v0.9.18-243 or later. |

On a failure the machine is switched off and the run ends (`--on-fault stop`,
the default). With `--on-fault continue` it switches back on, homes, and goes
on, up to `--max-recoveries` (10) times. Failures are still counted and still
make the result FAIL.

`--mode console` (`run_soak.sh console`) skips LinuxCNC and repeats the
health and console checks every `--interval` (10) s on an idle drive. There
the drive's `CMD error` (fault 1) is expected and ignored.

## Results

`runs/<target>-<date>-<time>/`, git-ignored:

| File | Content |
|---|---|
| `summary.md` | result (PASS / FAIL / STOPPED), counts, min/max of every reading, the last events. Rewritten after every cycle, so it survives a crash. |
| `summary.json` | the same, machine-readable |
| `events.log` | every event with time, kind, FAIL/info |
| `cycles.csv` | per cycle: duration, max and rest following error, top speed, ok/FAIL |
| `health.csv` | per check: HAL state, rt time and max, uptime, fault, DC bus, temperatures, position error |
| `console.log` | every console line, with time, including the commands sent |
| `config-baseline.conf`, `config-diff-N.conf` | the config at the start, and any read that differed |
| `runner.log`, `linuxcnc.log`, `linuxcnc_*.txt` | runner output; LinuxCNC's own logs when it failed |
| `lcnc/` | the machine config the run used |

## Notes

- **The cycle:** `soak.ngc` stays within ±300 mm and the machine limits (500 mm/s,
  1000 mm/s²). It covers long rapids, feeds from 2 to 500 mm/s, 20 short
  reversals, 0.05 mm steps, a dwell and a blended zig-zag. Adjust it, or pass
  another program with `--program`.
- **Safety on hardware:** the motor runs for as long as the run lasts.
  Check that it is safe to move, that the supply is steady, and keep an eye on
  the temperatures in `summary.md`.
- **The simulated motor** is the bench motor as measured (see
  `hwtest/burnin-sim/sim.txt`). With it, sim and bench give similar following
  errors: 0.040 against 0.045 mm in the first runs.
- **Console traffic:** the runner holds the console for the whole run, so
  servoterm-cli can't be used at the same time. Read `console.log` instead.

First runs, 2026-10-14, 2 cycles each: sim PASS (ferror max 0.041 mm); real
drive (v0.9.18-243, 200 V) PASS (ferror max 0.045 mm, rt max 168.5 µs of 200,
HV temperature 30 → 35.5 °C).
