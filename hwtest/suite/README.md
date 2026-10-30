# hwtest/suite: hardware test suite

A pass/fail check of a whole drive in about 5 minutes: firmware, console,
config flash, capture scope, crash handling, and under LinuxCNC the
smart-serial link, console and power field, enable cycles and motion. The
same tests run on the real drive and on the simulated one, so a failure can
be tried on the sim first.

```sh
cd hwtest/suite
./run_hwtest.sh hw --dc-volt 200      # the real drive: /dev/ttyACM0 + hm2
./run_hwtest.sh sim                   # the simulated drive
./run_hwtest.sh hw --phase-only drive # only the drive phase (no motion)
./run_hwtest.sh hw --only swing,motion  # tests whose name contains these
```

**On `hw` the motor moves** (X ±300 mm at up to 500 mm/s, the
`hwtest/burnin` machine) and the drive is crashed and reset on purpose.
The machine must be safe to move. LinuxCNC and halrun must not be running.

Setup once (as for `hwtest/burnin-sim`):
```sh
make -C servoterm-cli
make -C linuxcnc/sserialterm all && sudo make -C linuxcnc/sserialterm install
make -C host && make -C host/lcnc && sudo make -C host/lcnc install   # sim only
```
The drive needs firmware with the smart-serial console (con_cmd / con_fb,
v0.9.18-274 or later).

## Phases

**drive**: no LinuxCNC. The drive's USB console (sim: `stmbl_host --drive
… --pty`, booted from a copy of `hwtest/burnin-sim/sim.conf`).

| Test | Checks |
|---|---|
| identify | `about`: firmware (and HV board) version |
| HAL | `HAL_OK2`, rt functions running, rt max below 90% of 200 µs |
| readings | DC bus above `--min-dc` (and within 5% of `--dc-volt`), HV temperature 5–70 °C, fault state disabled or CMD error |
| console | unknown command → `not found`; a line split over two USB writes is reassembled; a 70-character line is dropped with an error and the next command works |
| config dumps | `--dumps` (5) dumps byte-identical; the first is the baseline for every later comparison |
| config load | `--load` of the baseline plus a comment line: flash verified, HAL still running; the original loaded back, byte-identical |
| flashsaveconf | 3 saves, HAL still `HAL_OK2` after each (rt is stopped around the erase) |
| capture (USB) | a forced capture, 2 × 2048 samples, CRCs ok, DC bus samples plausible |
| reset | a RAM-only pin is back at its default, uptime restarts (hw), HAL running, config unchanged |
| fault record (hw) | `fault` → the drive resets, `crash` shows a bus fault at 0x30000000 and a PC in `fault()` |
| bootloader (hw) | 5 × `tools/bootloader.py` → ROM DFU → a 4-byte read with `:leave` → the app is back, HAL running; ROM bounces retried and counted |
| hardboot (hw) | `hardboot` erases the app → the stmbl bootloader's CRC check gives DFU over USB (no BOOT0) → `make btburn` restores it; only when the drive runs the repo's current build |
| update over USB (hw) | `servoterm-cli --update obj_app/stmbl.bin`: staged, applied, the bootloader copies it, the app is back with the same config, `servoterm-cli` exits 0; only when the drive runs the repo's build |
| setsserial recovery (hw) | `hardboot`, then 15 s later hostmot2 at 115200 and `setsserial flash …sibl.0.0 stmbl/STMBL_SS.BIN`: every block written, no SSLBP error, the app starts with the same config; needs `make ssfw_install` of the repo's build |
| safe mode (hw) | 3 crashes in a row → safe mode (no rt functions, console working); `reset` → normal boot, config unchanged; the record is cleared |
| watchdog (hw) | `wdtest main`, then `wdtest irq` → the watchdog resets the drive; `crash` says Watchdog with the command, `errlog` has the hang and a watchdog boot; config unchanged |

**lcnc**: LinuxCNC headless with this script as its DISPLAY, on a copy of
`hwtest/burnin` (hm2 + `hw_console.hal`) or `hwtest/burnin-sim`
(`stmbl_sim` + `console.hal`). The USB console stays the reference; the
smart-serial console is the pty `<run>/drive-con`.

| Test | Checks |
|---|---|
| link | LinuxCNC up, `about` over smart-serial, `sserialterm.0.active`, no drive fault |
| console pin | a pin written over smart-serial reads back there and over USB |
| console dump | `--dump` over smart-serial identical to the USB baseline |
| console long line | a 70-character line dropped with the error, the next works |
| power | the hm2 `power` pin within 25 W of the drive's `pe0.p_in` (8-bit field, 20 W steps) |
| machine on | the drive enables, the motor moves less than `--max-swing` (0.1 mm) at the enable, homes, no fault |
| enable cycles | machine off/on × 5: the drive follows (state 0 / 1), no kick, no fault |
| motion | `hwtest.ngc` (rapids ±300 mm, feeds, reversals, small steps): following error ≤ `--max-ferror` (0.5 mm), at rest ≤ `--max-ferror-rest` (0.02 mm), 500 mm/s reached both ways, no LinuxCNC error, drive still enabled |
| motion + console | the same, with a smart-serial `--dump` and queries running; dump identical, answers right |
| capture (smart-serial) | armed on `vel1.vel` rising through 50 rad/s, a rapid: 2 × 2048 samples, the crossing at t = 0 |
| power under motion | the power pin's peak during the program; pin and drive agree at rest |
| rt load | `HAL_OK2` under LinuxCNC; rt / frt maxima recorded; FAIL above `--rt-fail` (97%) |
| end | machine off disables the drive; no reboot during the phase, no crash record, config unchanged |

**ebrake** (hw, only with `--phase-only ebrake`; **the axis moves at up to
300 mm/s and is stopped on purpose**): emergency braking
(`docs/emergency_stop.md`). For each speed in `EBRAKE_SPEEDS` (50,150,300
mm/s) and each cause in `EBRAKE_KINDS` (off, link, fb, f3): a G1 move from
X-250, the cause fired at speed past X-100, the stop captured by the
drive's scope (the encoder's own angle, q current, DC bus, `fault0.stop`).

```sh
./run_hwtest.sh hw --phase-only ebrake
EBRAKE_SPEEDS=50 EBRAKE_KINDS=off,fb ./run_hwtest.sh hw --phase-only ebrake
```

| Cause | Fired by | Checks |
|---|---|---|
| off | LinuxCNC machine off | stop mode 1 (quick stop) |
| link | `<board>.sserial.port-0.run = 0` for 2 s | stop mode 1 |
| fb | `fault0.mot_fb_error = 1`, relinked to `enc_fb0.error` after | stop mode 2 (dynamic brake) |
| f3 | `hv0.drop_link = 1.5` (the F4 sends the F3 nothing; the F3 brakes on its own) | stopped |

Every case: at speed at the trigger (70 % or more), standing still within
the capture, peak current at most 1.3 × `--ebrake-max-cur` (10 A), the bus
below `--ebrake-max-dc` (380 V); then machine on, homed, a move, no crash
record. Distances and times go to the summary.

**post** (hw): after LinuxCNC has exited, a USB `--dump` must take no
longer than 1.5 × the one before LinuxCNC. The smart-serial link has timed
out by then; a console still treating it as active slowed all output.

On a following error or a failed machine-on, the drive's commutation,
feedback and controller pins go to `diag.txt`.

**Use one console at a time.** The drive sends all its output to both
consoles, so USB traffic while a smart-serial command runs ends up in its
answer (and the other way round). The tests keep to that.

## Results

`runs/<target>-<date>-<time>/` (git-ignored):

| File | Content |
|---|---|
| `summary.md` | PASS / FAIL, every test with time and detail, measurements (rt, following error, dump times, power, enable swing, …) |
| `results.tap` | TAP |
| `results-drive.json`, `results-lcnc.json`, `results-post.json` | per phase, machine-readable |
| `hwtest.log` | every console command and answer, with time |
| `diag.txt` | drive state at a failure |
| `*.conf`, `*.csv` | dumps and captures |

For hours of motion use `hwtest/soak`; for a board-level test of a
feedback port (no motor) `hwtest/fbtest`.
