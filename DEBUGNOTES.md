# Debug notes

Running log of debug findings, gotchas, and open issues. See `CLAUDE.md` for
the rule on keeping this updated.

Entry format:

```
## YYYY-MM-DD  short title
- Area:     where it lives (file / subsystem)
- Status:   Open | Fixed | Workaround | Known | Note
- Symptom:  what was observed
- Cause:    root cause (if known)
- Fix:      what was done, or the idea for a fix
```

---

## 2026-05-24  ARM firmware build hangs in add_version_info.py
- Area:    `tools/add_version_info.py`, `tools/crc32.py`, `tools/elf.py` (POSTLD step)
- Status:  Workaround (Debian 12's binutils 2.40 builds fine; still hangs with 2.42)
- Symptom: `make` (and the bootloader/f3 sub-makes) hang after linking, in the
  CRC/version post-processing step. The python process spins at ~800 MB RSS and
  never finishes. The C compiles fine; only this post-link step hangs.
- Cause:   With modern binutils (2.42) the ELF places `.data`/`.bss` at their
  RAM LMA `0x20000000`, while flash sections start at `0x08000000`.
  `elf_to_bin()` walks sections in order and fills the gap between section LMAs
  with `0xFF`, producing a ~400 MB buffer; `CRC32.forge()`/`calc()` then iterate
  over it in pure python effectively forever.
- Fix:     Not fixed (we build the x86 host simulator only). Idea: in
  `elf_to_bin`/`add_version_info.py`, include only loadable flash sections
  (ALLOC+LOAD with LMA in the flash region) and skip RAM-LMA and debug sections,
  or cap/validate the computed image size before allocating.

## 2026-05-24  ILP32 sscanf corruption in hal.c on 64-bit hosts
- Area:    `shared/hal.c` (`hal_parse_`, `hal_linked_pins`)
- Status:  Fixed
- Symptom: In the x86 host build every link command (`a0.x = b0.y`) silently
  failed with "not found: 0.<pin>" — the sink component name parsed as empty.
- Cause:   `sscanf("%li"/"%lu", &v)` where `v` was `int32_t`/`uint32_t`. The
  firmware targets ARM (ILP32, `long` == 32 bit) so this is correct on-device,
  but on an LP64 host `%l*` writes 8 bytes into a 4-byte stack slot and clobbers
  adjacent locals.
- Fix:     Widened the affected locals (`sinki`, `sourcei`) to `long` /
  `unsigned long`. No effect on the 32-bit ARM target; correct on LP64.

## 2026-05-24  common.h GCC version guard vs macOS Clang
- Area:    `shared/common.h`, `host/host_compat.h`
- Status:  Fixed
- Symptom: Host `make` failed with `#error gcc to old (< 5.0)` in `common.h`.
- Cause:   Apple `cc` is Clang, which defines `__GNUC__` 4 for GCC compatibility;
  the firmware guard is meant for the ARM cross-GCC toolchain, not the host.
- Fix:     `host_compat.h` defines `STMBL_HOST`; `common.h` skips the check when
  that macro is set.

## 2026-05-24  newlib-only M_SQRT3 missing under glibc
- Area:    `shared/comps/psi.c` (`dq.c`, `idq.c` for the V3 build)
- Status:  Workaround
- Symptom: Host build failed: `'M_SQRT3' undeclared`.
- Cause:   `M_SQRT3`/`M_SQRT1_3` are newlib `math.h` extensions; glibc does not
  define them.
- Fix:     Provided in `host/host_compat.h`, force-included by the host Makefile.

## 2026-05-24  printf %lu / uint32_t format mismatches on LP64
- Area:    `shared/hal.c` and others (diagnostic prints)
- Status:  Known
- Symptom: A few diagnostic prints (hal stats, pointers via `%x`) can show
  wrong numbers in the host build.
- Cause:   Firmware printf formats assume 32-bit `long` (ARM). On LP64 the
  widths mismatch. This is read-only (not memory-unsafe), unlike the sscanf bug.
- Fix:     Suppressed with `-Wno-format` in the host build. Could be made fully
  portable with `PRIu32` etc. if desired; cosmetic only for now.

## 2026-05-24  No aligned closed-loop motor example yet
- Area:    `shared/comps/motsim.c`, host test assets
- Status:  Fixed (2026-10-14)
- Symptom: There is no closed-loop motor test asset (command velocity, watch it
  track).
- Cause:   `motsim` needs a commutation angle (`com_pos` = electrical rotor
  angle) for aligned FOC; with `com_pos` fixed the open-loop current command
  does not spin the rotor cleanly. Wiring a commutation/encoder source is TODO.
- Fix:     TODO — add a commutation source (or drive `com_pos` from rotor angle)
  and ship a `examples/motor.servoterm` closed-loop asset.
  Done another way: `host/examples/drive_jog.txt` (run by `make test`) boots
  the real drive config on the simulated drive (`--drive`, `host/hw/`), with
  the encoder, fb_switch commutation and the F3 current loop. It jogs both
  ways closed-loop.

## 2026-05-24  Scope channels are 8-bit (lossy) by design
- Area:    `shared/comps/term.c`, `host/servoterm.c`
- Status:  Note
- Symptom: Scope CSV values are quantized.
- Cause:   The wire protocol encodes each channel as one byte
  (`(value+offset)*gain+128`, clamped 1..254).
- Fix:     For exact numeric assertions, query pins directly (e.g. `motsim0.vel`
  returns exact text). Use the scope CSV for waveform-shape checks.

## 2026-10-14  servoterm-cli: first command lost / garbage right after open
- Area:    `servoterm-cli/servoterm-cli.c` (real drive over /dev/ttyACM0)
- Status:  Fixed
- Symptom: A command sent immediately after opening the port got no reply,
  and stray bytes (e.g. `}`, 0x80) were printed as text.
- Cause:   The drive's `term` only reads commands once `cdc_is_connected()`
  sees the host connection, which lags the open. The scope stream (0xFF + 8
  bytes, nonstop while `send_step > 0`) also usually starts mid-frame, so
  channel bytes before the first 0xFF look like text.
- Fix:     Wait 0.3 s after open and discard everything read in that window
  before sending the first command.

## 2026-10-14  Command lines >= 64 bytes jam the drive's USB input
- Area:    `shared/comps/term.c` (`rx_buf[64]`), `shared/ringbuf.c` (`rb_getline`)
- Status:  Fixed (firmware; servoterm-cli keeps its limits for older firmware)
- Symptom: After sending a command line of 64+ bytes (including `\n`), the
  drive ignores that line and every line after it.
- Cause:   `rb_getline` only returns a line that fits in `len`. Otherwise it
  undoes the read (`rb_undo`) and returns 0, so the too-long line stays at the
  head of the ring buffer for good. `call_cmd()` also caps the command name
  and argument at 64 chars each, and its argument stops at the first
  non-printable char (`%[ -~]`), so a tab cuts an `appendconf` line short.
- Fix:     rb_getline looks for the '\n' without consuming, so a partial line
  stays in place (no rb_undo). A line with no '\n' in len bytes is dropped up
  to and including its '\n' (ringbuf.skip), and rb_getline returns -1; term
  prints "error: line too long, max 63 chars, dropped". The old loop also
  consumed one byte more than it rewound. Tested on the simulated drive's
  pty: 90- and 250-char lines are dropped with the error, the next lines
  work, a 63-char line still works. `call_cmd()`'s 64-char caps on name and
  argument are unchanged, so servoterm-cli's 52-char appendconf limit stays.

## 2026-10-14  Drive loses bytes when a command arrives split across USB packets
- Area:    `shared/ringbuf.c` (`rb_putc` vs `rb_getc`/`rb_undo`), `src/usb_cdc.c`
- Status:  Fixed (firmware, not yet run on hardware; servoterm-cli workaround kept)
- Symptom: During `servoterm-cli --load`, one `appendconf` line got no ack.
  Its `\n` was lost, so the next command (`showconf`) was glued onto it:
  the drive answered `adding ...# append to configshowconf`.
- Cause:   `usb_rx_buf` is written from the USB interrupt (`VCP_DataRx` ->
  `rb_putc`) and read in the main loop (`rb_getline`), with no locking. The
  tool sent the text and the `\n` in two writes, i.e. two USB packets.
  `rb_getline` read the partial line, saw no `\n` and rewound with `rb_undo`.
  If the interrupt adds the `\n` during that read-modify-write of `pos`/`len`,
  the byte is lost or lands in the wrong slot.
- Fix:     Tool side: servoterm-cli sends each command and its `\n` in one
  write of at most 64 bytes, i.e. one full-speed USB packet. It sends a lone
  `\n` at connect to end any stale half-line. It also checks each
  `adding <line>` ack exactly. With these changes, load/verify/restore passed.
  Firmware fix: disable the USB IRQ around `rb_getline`, or make `rb_undo`
  unnecessary by peeking for `\n` before consuming.
  Fixed 2026-10-14: rb_getline no longer rewinds (see the 64-byte entry),
  and both cdc_getline()s keep the writer out while reading: the F4 masks
  OTG_FS_IRQn, the F3 (whose rx_buf is filled from the rt interrupt, ls)
  disables interrupts for the copy. rb_getc in src/syscalls.c `_read` is
  still unguarded; nothing calls it.

## 2026-10-14  id_mot template links non-existent pins (offset not fed back)
- Area:    `conf/template/id_mot.txt`, `shared/comps/idm.c`, `shared/comps/pid.c`
- Status:  Fixed
- Symptom: `id_mot.txt` had `pid0.l = idm0.load`. Neither pin exists (pid's
  offset pin is `o`, idm's is `offset`), so the drive rejects it with
  "not found: pid0.l". With a constant load, `conf0.o` came out wrong.
- Cause:   With `pid0.o` unlinked, idm0.offset is learned open-loop. It
  integrates `fb_torque_cmd`, and pid's integral term holds the load, so the
  estimate grows linearly forever. The printed value just reflects run time
  multiplied by load. In the host sim (motsim, load -0.3 Nm, true o = +0.3),
  it rose steadily to 0.657 at 120 s, and inertia came out 0.000341 against
  a true 0.0002.
- Fix:     `pid0.o = idm0.offset`. In the same sim: offset converges toward
  0.3 (0.267 at the end of the run), inertia 0.000215, and pid's integral
  drops from 0.30 to 0.03. With no load, results are the same as before.
  Regression asset: `host/examples/idm_load.hal`. Not yet run on hardware.

## 2026-10-14  idm: damping and friction not separable at cruise speed
- Area:    `shared/comps/idm.c`, `conf/template/id_mot.txt`, `id_sys.txt`
- Status:  Fixed (host sim and hardware, 2026-10-14)
- Symptom: Host sim, no load: idm gave d = 0.00106 and f = 0.072 against a
  true 0.002 / 0.05. With high damping (d = 0.01, f = 0.01) all four
  estimates diverged: f +600..1700%, j collapsed to its clamp, and position
  error reached 2.4 rad.
- Cause:   (1) Most of the run is at one speed, where `d*v + f*sign(v)` is
  one number: the sum is right, the split is not. (2) The adaptive
  estimates feed pid's feedforward directly, so an error in one (f taking
  over d's share) feeds into the others, and tracking degrades.
- Fix:     idm now prints a least-squares fit instead of the adaptive
  estimates. The adaptive estimates still drive the feedforward during
  the run.
  - `vel_steps` (default 5) cycles the stroke speed in state 1.3.
  - `ls_fb` (default 1) fits against measured `vel_fb`/`acc_fb`, linked to
    vel1 in id_mot.txt. vel1 is model-free there because `vel1.torque = 0`.
  - `ls_lp` (20 Hz) filters torque, acc, vel and sign(vel) identically
    before the fit. Without it, the noise of the differentiated acc biased
    j by -97%.
  - id_sys reports `j_sys = fit - idm0.j_mot` (linked to conf0.j).
  - If the fit is unusable (< 1000 samples, singular, or j <= 0), idm prints
    a red warning and falls back to the adaptive values.
  Sim results, five plants, 4096-count encoder plus vel estimator: j +0..13%,
  d -4..+4%, f 0%, o 0%. id_sys: j_sys +1%.
  Regression asset: `host/examples/idm_load.hal`.
  Remaining: j reads high. In the simulated drive this is confirmed as
  current-loop lag, see "idm inertia reads high by a constant" below. Not
  tested with sensor noise.
  Hardware, 2026-10-14 (firmware v0.9.18-224-g89e4d95d, bench motor,
  id_mot, about 2 min, no faults): fit j = 0.000096, d = 0 (clamped, the raw
  fit was negative), f = 0.0946, o = 0.0265. Adaptive estimates from the
  same run: j 0.000066, d 0.00047, f 0.0884, o 0.0250. The two methods agree
  roughly. The earlier conf (old idm) had j 0.000207, o 0.064. Negative d
  probably means friction drops with speed (Stribeck effect), which
  f + d*v cannot model. Unverified: no independent measurement of j.
  That run still had the wrong encoder resolution (16384 instead of 10000,
  see "Bench motor: encoder resolution misconfigured"). Rerun after the fix:
  j = 0.000044, f = 0.038, o = 0.0001; d = 0.0001 is from a separate speed
  sweep (20-157 rad/s), not from idm. These values are in
  `hwtest/burnin/ECMA-C30604FS/stmbl.conf`.

## 2026-10-14  autotune smaller issues (idpmsm, ids)
- Area:    `shared/comps/idpmsm.c`, `shared/comps/ids.c`
- Status:  Note
- Symptom/Cause:
  - FIXED idpmsm state 1.3 (L): resets `tmp0/tmp1` but not `tmp2/tmp3`. So
    on a second run in the same boot, the "low" averages start from the
    previous run's values.
  - FIXED idpmsm state 2.3 (mot_fb_offset): low-pass filters `-pos_fb`
    without handling angle wrap (now a sin/cos average; the noiseless sim
    can't show the flicker across +-pi, only that results don't change). If the locked rotor sits near ±pi, the average mixes
    +pi and -pi and the offset comes out wrong.
  - FIXED (now en_out) idpmsm state 2.3 writes `PIN(en) = 0`, but `en` is linked to
    `fault0.en_out` and gets overwritten next cycle, so it has no effect.
  - FIXED ids state 0 resets `step = 0.1`, `rep = 1` every time, so nrt_init's
    0.25 / 2 never apply. `rep` is not used anywhere.
  - idm: `inertia_sum/damping_sum/friction_sum/offset_sum` are computed but
    never used or printed. Only the adaptive estimates are reported.
- Fix:     2026-10-14: all but the idm sums fixed, see the FIXED items and
  their commits. ids: nrt_init now sets the step that runs (0.1) and
  state 0 no longer overwrites it, so `ids0.step` can be set. The idm sums
  stay: they are readable pins and cost nothing.

## 2026-10-14  Firmware build: duplicate shared/crc.c after the sys_id merge
- Area:    `Makefile` (SOURCES), merge `03a901d6`
- Status:  Fixed
- Symptom: `make` failed at link: `multiple definition of calc_crc8_table`
  and `calc_crc8`.
- Cause:   Both branches added `SOURCES += shared/crc.c`, and the merge kept
  both lines, so `crc.o` was linked twice.
- Fix:     Removed the duplicate line. Separately, the add_version_info.py
  hang (2026-05-24 entry) did not happen on the bench Pi (Debian 12,
  aarch64, gcc-arm-none-eabi 12.2.1, binutils 2.40, newlib 3.3.0). There,
  `.data` gets its flash LMA from the program headers, so `elf_to_bin`
  has no 0x08000000 -> 0x20000000 gap. Full `make all` takes about 30 s on
  4 cores. The hang seen with binutils 2.42 is still unexplained.

## 2026-10-14  Config/template lines >= 64 chars silently end hal_parse
- Area:    `shared/hal.c` (`findline`), `shared/config.c` (`link`), `src/config.c` (`loadconf`)
- Status:  Fixed (templates); Known (firmware limit)
- Symptom: On the drive, `link id_mot` applied only the first part of the
  template. `idm0.vel_fb`/`acc_fb` stayed unlinked, and `vel1.torque = 0`
  and `conf0.max_pos_error = 0` never applied. There was no error message.
- Cause:   `findline()` looks at most 64 chars ahead for `\n` and returns 0
  otherwise, which ends `hal_parse()`'s loop. A 72-char comment line added
  to id_mot.txt cut the template there. This affects templates (`link`)
  and the flash config at boot (`loadconf`). The host sim did not catch
  it: the test scripts had the template lines inlined, and the host build
  does not include `config.c`.
- Fix:     Shortened the comments. `tools/create_config.py` now fails the
  build on any template line over 63 chars. servoterm-cli `--load` already
  limits config lines to 52. Firmware idea: have findline() skip to the
  real `\n` instead of giving up after 64 chars.

## 2026-10-14  ids (id_pid) never tunes vel_bw: first cycle cost starts at min_cost
- Area:    `shared/comps/ids.c` (state 0 / 1.2)
- Status:  Fixed (sim and hardware)
- Symptom: On the bench motor, id_pid finished in ~25 s with
  `vel_bw = 70.000000`, exactly 0.7 (kd) times its starting value of 100.
- Cause:   State 0 set `cost = min_cost` instead of 0. The first cycle's cost
  is then min_cost_init plus the real cost, and min_cost stays
  min_cost_init. Whenever the real cost exceeds 0.2 * min_cost_init, the
  check `cost > min_cost * kt` fires on the first evaluation, so vel_bw is
  multiplied by kd and the search moves on without tuning it. In the host
  sim the first cycle is cheap, so it does not trigger (vel_bw 124 before
  and after the fix).
- Fix:     Start cost at 0. Rerun on the bench motor on 2026-10-14 with the
  fixed firmware, after the encoder-resolution fix: id_pid now tunes vel_bw,
  ending at pos_bw 42.8, vel_bw 500 and vel_d 1. The last two are marked
  "(cap)" in `hwtest/burnin/ECMA-C30604FS/stmbl.conf`: they are limits, not
  tuned optima.

## 2026-10-14  Bench motor: encoder resolution misconfigured (16384 vs 10000)
- Area:    drive config (`conf0.mot_fb_res`, `conf0.polecount`), `src/comps/enc_fb.c`
- Status:  Fixed (config)
- Symptom: Clicks / torque catches every 0.607 "rev". The + direction
  needed 2-5x the current of the - direction, with the voltage vector
  rotated up to ~68 deg off the q axis. A POS_ERROR fault at +157 rad/s.
  id_pmsm reported 8 pole pairs.
- Cause:   The motor has a 2500-line encoder (10000 counts/rev), but the
  config used the default 16384. `enc_fb` wraps the counter at `res` and
  re-zeroes `abs_pos` at each index pulse. With the wrong res, the
  commutation angle drifts ~42 deg electrical per revolution and jumps at
  every index pulse, once per real rev = 10000/16384 = 0.610 measured rev.
  The pole pairs read 5 * 16384/10000 = 8.2, rounded to 8.
  `enc_fb0.indexprint = 1` showed the counter at successive index pulses
  stepping by 10000 mod 16384.
- Fix:     `conf0.mot_fb_res = 10000`, `conf0.polecount = 5`, then reran
  id_pmsm (offset -0.631) and id_mot. Speed sweep +-20..157 rad/s is
  symmetric: torque 0.03-0.05 Nm in both directions, before up to 0.43 Nm
  in +. Hint: if id_pmsm's pole pairs are not a plausible integer for the
  motor, or catches repeat at a non-integer fraction of a rev, check
  mot_fb_res against the encoder datasheet.

## 2026-10-14  vel_int keeps an extrapolated overshoot when a move stops
- Area:    `shared/comps/vel_int.c` (sserial template: `vel_int0`)
- Status:  Fixed (simulated drive; not seen on hardware)
- Symptom: After a move stops, the drive holds a position offset from
  LinuxCNC's `pos_cmd`: 15 mrad (0.024 mm) after a 19.7 rad/s jog in
  `host/examples/drive_jog.txt`; LinuxCNC on the sim shows ~0.016 mm ferror
  after each G0. `pid0.pos_error` itself is ~0.
- Cause:   `vel_int` extrapolates `pos_in` by `vel_in` between updates and
  only resets to `pos_in` when it changes (`EDGE(p)`). If the last update of a
  move has vel != 0 and the next one repeats the same position with vel = 0,
  the extrapolation over that 1 ms (minus the 0.2 ms first cycle) stays.
- Fix:     A new command is now a change of pos_in or vel_in (the raw vel_in,
  before the watchdog zeroes it), kept per instance in the ctx. EDGE() kept
  its last value in a function-level static, shared by all vel_int
  instances. drive_jog.txt: standstill offset 0.024 mm -> 0.001 mm (one
  encoder count). The watchdog still stops extrapolating after wd.
  Bench, 2026-10-14, LinuxCNC + hm2 + the second drive, 8 moves up to
  500 mm/s: linrev0.cmd_out - vel_int0.pos_out at rest is 0 with v243 and
  also with v225 (old code). LinuxCNC ramps vel_cmd to ~0 before the last
  packet, so the stop-from-speed case only came from the sim's `lcnc`
  command. Rest ferror 0.000..0.003 mm with both.

## 2026-10-14  Simulated drive: what differs from the real one
- Area:    `host/hw/*.c`, `host/drive.c`, `host/lcnc/`
- Status:  Note
- Symptom: n/a. The sim runs the drive's own config and control comps, but:
  hv (F3) is modelled as curpid at 20 kHz (current and voltage mode) + a PMSM at 80 kHz, without PWM
  delay or dead time; current noise (`hv0.sim_cur_noise`) and the F3 packet
  delay (`hv0.sim_fb_delay`) are optional, off by default; the encoder is ideal
  (exact counts, index); no hv/thermal faults; `hv0.dc_cur` doesn't exist on
  the real hv either, so it is left out. `pe`/`velbuf`/`res` links fail the
  same way as on the drive ("not found").
- Cause:   by design.
- Fix:     n/a. Two gotchas from building it: the drive's HAL symbols
  (`hal_init`, ...) clash with LinuxCNC's, so `host/lcnc/Makefile` makes all
  but the `stmbl_sim_*` API local with objcopy; and the sim's printf buffer
  must hold a whole `showconf` (15 kB), or `servoterm-cli --dump` is cut off.

## 2026-10-14  linuxcnc started without a tty: output only in /tmp, deleted on success
- Area:    `/usr/bin/linuxcnc` (LinuxCNC 2.9.1), headless tests
- Status:  Known
- Symptom: `linuxcnc stmbl.ini > log` from a script leaves an empty log. With
  `$DISPLAY` set, a failed start pops the error-report dialog on the desktop.
- Cause:   Without a tty the script redirects to `/tmp/linuxcnc.{print,debug}.*`
  and only copies them to `~/linuxcnc_{print,debug}.txt` on failure. The
  DISPLAY program is started as `<DISPLAY> -ini <file>`.
- Fix:     For headless tests: `env -u DISPLAY linuxcnc ...`, a display program
  that ignores its arguments and writes its own result file.

## 2026-10-14  idm inertia reads high by a constant: current-loop lag
- Area:    `shared/comps/idm.c` (least-squares fit), `conf0.cur_bw`
- Status:  Fixed (simulated drive and bench)
- Symptom: `link id_mot` on the simulated ECMA motor (tuned config,
  `cur_bw` 1000) reports j = true + ~0.000045 kg*m^2 at any true j
  (0.00003 -> 0.000075, 0.0003 -> 0.000349). f, d and o come out right.
  The bench motor measured 0.000044 against a 0.000030 datasheet value.
- Cause:   The fit relates the commanded torque (`pid0.torque_cmd`) to the
  measured acceleration, but the real torque lags it (current loop
  bandwidth, one rt cycle to the F3). With `cur_bw` 4000 the error drops to
  +0.000013.
- Fix:     pmsm_ttc has `cur_fb` (linked to `hv0.iq_fb` in pmsm.txt) and
  `torque_fb` = cur_fb * 1.5 * polecount * psi. id_mot.txt links
  `idm0.torque_fb` to it; when that pin is linked, the fit uses it instead of
  the command (DC motors keep the command). Simulated drive, cur_bw 1000:
  j 0.00003 -> 0.000033, 0.000044 -> 0.000047, 0.0001 -> 0.000104,
  0.0003 -> 0.000303; d, f, o unchanged. Regression: drive_id_mot.txt.
  On hardware hv0.iq_fb is about one rt cycle older than in the sim (the F3
  packet), which may bias j slightly low; check on the bench.
  Bench, second drive (v243): j 0.000020 and 0.000027 at max_acc 250,
  0.000030 at 5000 (datasheet 0.000030). See the max_acc entry.

## 2026-10-14  Build warnings in our own code
- Area:    `src/comps/hv.c`, `stm32f303/src/comps/ls.c`, `f3_boot/src/main.c`,
  `shared/hal.c`, `shared/crc.c`, `shared/angle.c`, `src/comps/smartabs.c`,
  `src/comps/encs.c`
- Status:  Fixed
- Symptom: Firmware `make all` gave 32 warnings, host 5; none was a real bug.
- Cause:   Mostly GCC 12 object-size checks: the stmbl_talk CRC was given
  `&header.slave_addr` (1 byte) and reads whole words from there. Also
  `comp_by_name(NAME)` with string literals, non-const CRC8 args, an unused
  `volatile foo`, float math in `modi()`, pointer-to-int casts on LP64.
- Fix:     CRC start is now `(uint32_t *)&packet + 1` (the same address, since
  the 4-byte crc is the first field); `comp_by_name(const char *)`;
  `calc_crc8(const ...)`; `(void)CCR3` read; `modi()` is `v % m`; casts go
  through `uintptr_t`. Machine code is unchanged except `modi` (integer
  only now) and encs (the stack slot for foo is gone; the register read
  stays). Now 0 own warnings in all three builds. The 7 left are in `lib/`
  (CMSIS `sp` clobber, USB `packed` attribute, F3 HAL `CLEAR_BIT` on IDR)
  and are left alone.

## 2026-10-14  rb_read leaves pos at bufsize after a read to the end
- Area:    `shared/ringbuf.c` (`rb_read`)
- Status:  Fixed
- Symptom: none seen; found while reworking rb_getline.
- Cause:   `if(pos > bufsize)` instead of `>=`: a read that ends exactly at the
  end of the buffer leaves pos = bufsize. rb_read itself copes on the next
  call, but rb_getc then reads buf[bufsize], one byte past the buffer.
- Fix:     `>=`.

## 2026-10-14  idpmsm prints mot_fb_offset with the wrong sign when mot_fb_rev = 1
- Area:    `shared/comps/idpmsm.c` (state 2.5 print), `shared/comps/fb_switch.c`
- Status:  Fixed (simulated drive; not yet run on hardware)
- Symptom: Simulated drive, tuned bench config (mot_fb_rev = 1), id_pmsm at
  several encoder index positions, then the printed offset put in the
  config: commutation error -0.05, -0.44, +0.40, -2.44, -1.17 rad electrical
  depending on the index. The bench motor's -0.6308 happens to be about
  -pi/5, where the sign doesn't matter (offset = -offset mod 2pi/5).
- Cause:   idpmsm commutes with (pos_fb + offset) * pp, where pos_fb
  (fb_switch0.mot_abs_fb_no_offset) is already reversed. fb_switch reverses
  mot_fb_offset together with the position: (pos_fb - offset) * pp.
- Fix:     idpmsm gets `fb_rev` (id_pmsm.txt: conf0.mot_fb_rev) and prints the
  offset negated when it is set. Error is now 0.021 rad at every index
  position (the lock is held off by friction). Re-run id_pmsm on any motor
  with mot_fb_rev = 1 whose offset is not near a multiple of pi/polecount.

## 2026-10-14  idpmsm L reads about 4x high
- Area:    `shared/comps/idpmsm.c` (state 1.3)
- Status:  Fixed (simulated drive and bench motor)
- Symptom: Simulated drive, true L 0.023 H: id_pmsm printed l = 0.091. The
  bench motor read 0.023 against a 6.71 mH datasheet value (3.4x).
- Cause:   ud toggled every rt cycle, and L came from the mean currents of
  the two halves: L = dU / dI * period. That assumes the current is sampled
  exactly at the ripple's peaks, and even then dI is the peak-to-peak
  ripple, U*T/(2L), so the result is at least 2L. The sim samples 3/4 into
  the cycle, which halves the measured swing again: 4L. The real F3's
  sample timing gave 3.4x.
- Fix:     Square wave with `l_half` (8) rt cycles per half. From each pair of
  samples at the same voltage, l * di = period * (u - r * i_mean), so
  l = (u_hi - u_lo - r * (i_hi - i_lo)) / (slope_hi - slope_lo), with the
  voltage per sample taken from ud_fb (it comes with id_fb) and pairs near
  a switch skipped. Sample timing and feedback delay don't change slopes.
  Simulated drive, L 0.1 mH..100 mH (L/R 0.4..20 ms), 0..2 cycles delay,
  up to +-0.1 A noise: within 1.5% (0.2% for the bench motor). Full
  id_pmsm with delay and noise: `host/examples/drive_id_pmsm.txt`.
  Bench motor, 2026-10-14, firmware v0.9.18-243-g128b9b92: l = 0.00489
  (config had 0.0230 from the old method: 4.7x, as the sim predicts);
  r 2.337, polecount 5, offset equivalent to the config's, psi 0.04916.
  0.00489 lies between the datasheet's 6.71 mH and half of it, so the
  datasheet doesn't settle which it quotes. Second drive, same motor:
  l = 0.004911 (+0.5%), r 2.304, polecount 5, offset equivalent, psi 0.04979. The bench config has
  conf0.l = 0.023, so its current loop (kp = cur_bw * l) runs about 3x
  faster than cur_bw says; after re-measuring, raise cur_bw if needed.

## 2026-10-14  idm: bench motor's inertia barely observable at max_acc 250
- Area:    `shared/comps/idm.c` (`max_acc` default 250 rad/s^2)
- Status:  Open
- Symptom: Bench motor (datasheet j 0.000030 with brake), fit against
  measured torque. First drive: j 0.000008 and 0.000010 at the default
  max_acc 250; 0.000022 at 2000; 0.000027 at 5000 (but its DC-bus reading
  went wrong around then, which scales the F3's output). Second, healthy
  drive: 0.000020 and 0.000027 at 250; 0.000030 at 5000. f 0.032..0.042.
  The simulated motor gives 0.000033 at 250.
- Cause:   At 250 rad/s^2 this motor needs j*acc = 0.0075 Nm, against
  0.04 Nm friction and +-0.03 Nm sample-to-sample current scatter, so the
  fit is dominated by what f + d*v doesn't model (bearing catches, cogging,
  friction falling with speed: d comes out negative). The sim motor has
  none of that.
- Fix:     Idea: choose max_acc so that j*acc is several times f (e.g. from a
  first pass, or a larger default), and/or print the fit's residual so an
  unreliable j shows. 5000 was fine on the bench motor (no faults).

## 2026-10-14  USB text and scope frames mix when the drive's TX buffer overflows
- Area:    `src/syscalls.c` (`_write`), `src/usb_cdc.c` (`cdc_tx`), USB VCP
- Status:  Open
- Symptom: Scope bytes inside text: `[..] ?zy?L?{??conf0.j = ...` while
  logging 9-byte frames at 5 kHz (most frames were lost too), and 2 of 141
  `servoterm-cli --dump` reads with 9 text bytes missing or 1 replaced.
- Cause:   Probably `_write`/`cdc_tx` writing into APP_Rx_Buffer without
  checking how much is still unsent, so the write pointer laps unsent data
  when the host doesn't keep up. Not confirmed.
- Fix:     Idea: drop (or block for) data that doesn't fit, frame-atomic for
  scope frames. Meanwhile: log at `--step 5` or slower (1 kHz had no losses),
  and `--load` verifies by reading back, so a garbled read can't go to flash.

## 2026-10-18  setsserial could not flash through the smart-serial loader
- Area:    `shared/lbp_dev.c`, `bootloader/src/main.c` (sserial_loader),
  `servoterm-cli` (--update exit code); 7c81 card, SSLBP v43
- Status:  Fixed (bench)
- Symptom: (1) hostmot2 at 115200 found no remote: "found a" with a blank
  name, setsserial "Unable to find sserial remote". (2) Then every write of
  setsserial failed ("Error in sslbp_write_long/_byte/_double", "Error flag
  set after CMD Clear 1"), thousands of times, while the data still landed.
  (3) The erase-size read was dropped. (4) `servoterm-cli --update` exited 1
  after a good update.
- Cause:   (1) In setup mode (115200) hostmot2 keeps a remote only if the
  second byte of its card name is 'I'; the loader was "stbl". (2) SSLBP
  (source in Mesa's 7c81 package, hostmot2/source/sslbp.zip) waits for one
  reply byte to every write, the CRC of no data (0x00); the loader sent
  none to memory writes. (3) The loader dropped a partial request after
  1 ms of silence. (4) After applying, servoterm-cli read the port while
  the drive reset. The branch's host model had none of these: it followed
  hostmot2's C code, not the card's firmware. Found by running the
  loader's lbp_dev inside the application at 115200 with a byte log.
- Fix:     Name "sIbl" (HAL name ...sibl.0.0); a 0x00 reply to every write;
  local-write CRCs checked; 20 ms idle; no erase inside a commit (stale
  data fails it, erased afterwards); `make ssfw_install`; servoterm-cli
  skips the trailing read after applying. Bench: setsserial 23 blocks in
  67 s without errors after hardboot. Tests: test_bootloader (name rule,
  write replies, bad-CRC local write), hwtest/suite update_usb, setsserial.
  Also: inc/commandslist.h now depends on the Makefile (a removed source
  left a stale command list and a link error).

## 2026-10-17  DFU entry unreliable; stmbl bootloader's DFU path dead; hardboot locked up
- Area:    `src/main.c` (`bootloader`, `dfu_request_check`), `bootloader/src/main.c`,
  `src/config.c` (`hardboot`), `tools/bootloader.py`, `base.mak` (`btburn`)
- Status:  Fixed
- Symptom: (1) `bootloader` rebooted into the app instead of DFU in 10 of 20
  round trips (every other one). (2) With a bad app CRC, or the bootloader's
  magic word, the drive came up with no USB at all; an interrupted flash plus
  a power cycle needed BOOT0. (3) `hardboot` left USB enumerated and the
  console dead until a power cycle. (4) `btburn` waited a fixed 1 s for DFU.
- Cause:   (1) The app jumped into ST's ROM from the running firmware; after
  dfu-util's `:leave` (a jump into flash, no reset) the ROM went straight
  back to flash. (2) The stmbl bootloader jumped into the ROM without
  mapping the system memory at 0 (AN2606), so the ROM's USB interrupts used
  our vector table. (3) hardboot erases sector 4, which holds the vector
  table, with interrupts on: the next one jumped into blank flash. (4) And
  ST's ROM itself still goes back to flash in about 1 of 20 entries even
  from a clean reset (a counter shows the bootloader did take the request),
  most likely on a USB reset while the host enumerates it.
- Fix:     `bootloader` leaves a request word in CCM (0x1000FE00) and resets;
  the bootloader (v0.9.18-289+) takes it straight from reset, an older one
  passes it on to the app's SystemInit(), which jumps before any clock
  setup. The bootloader remaps system memory and counts requests (`about`).
  hardboot keeps interrupts off from the erase to the reset. bootloader.py
  waits for 0483:df11 and resends up to 10 times (first poll after 1.5 s;
  polling at once raised ROM bounces from ~2 to ~7 in 25); btburn waits for
  the device instead of a fixed sleep. Bench: 50/50 end-to-end entries, 3/3
  hardboot recoveries without BOOT0. Tests: hwtest/suite "bootloader: …"
  and "hardboot: …" (hw).

## 2026-10-17  USB console crawls after a smart-serial console session
- Area:    `shared/sscon.c` (`sscon_reset`), `src/syscalls.c` (`sscon_out`)
- Status:  Fixed
- Symptom: The 1 h soak, started right after a hwtest/suite run, failed in
  seconds: `hal` answers trickled in for seconds and ran into the next
  commands (console_no_answer, garbled HAL state). A config dump took 7 s
  instead of 2 and came back short.
- Cause:   `sscon0.active` was set by the first console byte from LinuxCNC
  and never cleared; a link timeout (LinuxCNC stopped) kept it. Every print
  then went into the 2 KB smart-serial ring, which nothing drained, and
  waited 100 ms per 8-byte chunk for room: ~80 bytes/s on USB too, until a
  reset. (64-byte chunks would have made it ~640 bytes/s.)
- Fix:     sscon_reset clears `active` and drops unsent output; it becomes
  active again when LinuxCNC sends. test_sscon covers it; hwtest/suite's
  post phase checks the USB console's speed after LinuxCNC exits.

## 2026-10-17  Autophasing kicked up to 1 mm, and failed silently against a stop
- Area:    `shared/comps/fb_switch.c` (autophasing), `shared/comps/fault.c`,
  real drive with the ECMA-C30604FS (2500-line incremental, no U/V/W)
- Status:  Fixed (sim and bench); a blocked rotor on the bench not yet tried
- Symptom: (1) Some boots: LinuxCNC machine on -> "joint 0 following error":
  pos_fb swung up to 0.9 mm within ~60 ms of each enable (two drives) until
  the encoder index was passed. (2) Against a mechanical stop, phasing
  "succeeded" with a wrong angle; once free the axis could not move or ran
  away (user's experience; reproduced in the sim: 172 deg off, fault 5).
- Cause:   The align method lets the rotor fall into a d current at an
  angle guessed from the boot's encoder zero: it moves up to half an
  electrical period (1 mm here), depending on where it sat at boot; if it
  cannot move, the guess stays. A faster correction (phase_gain 1000) ran
  away 39..45 mm on the bench.
- Fix:     Probe phasing (phase_mode 1, default; 0 = align, 2 = none for
  id_pmsm / rlpsij): 16 short current pulses round the circle, the
  movement each causes correlated with its angle (one side blocked only
  halves it); pulses grow until the rotor moves; the field turns between
  pulses instead of jumping (vel2 + cur_ind turned jumps into a wrong-way
  q current); the rotor's own movement stays in the feedback, the
  command's is absorbed. A check (+q / -q pulses) after either method:
  wrong way or no movement -> fault PHASING_ERROR (17), phase_fail says
  why. Sim: swing 0.03..0.06 mm (align 0.18..1.06), ~4 deg; stop on either
  side phases, blocked both ways faults. Bench: swing 0.11..0.19 mm (align
  0.47..1.12), LinuxCNC suite 15/15 (re-enable 0.11 mm). LinuxCNC side
  still allows PHASING_FERROR until homed (phasing_ferror.hal). Tests:
  host/tests/drive_phasing_*.test, console_link (jog while phasing).

## 2026-10-17  flashsaveconf stalls rt: RT_TOO_LONG after a config save
- Area:    `src/config.c` (`flashsaveconf`, `hardboot`)
- Status:  Fixed
- Symptom: `servoterm-cli --load` (which ends with flashsaveconf) or a bare
  flashsaveconf left the drive in HAL state RT_TOO_LONG, rt and frt stopped,
  until reset. Bench: 4 of 5 saves of the unchanged config, rt max ~122 us.
  This was the first drive's "three boots ended in RT_TOO_LONG" (2026-10-14).
- Cause:   The 16 KB sector erase stalls every flash fetch, interrupts
  included, for 0.25 s or more; afterwards the rt ISR finds its DMA flag set
  again and stops the HAL. Nothing stopped rt around the erase.
- Fix:     flashsaveconf refuses while fault0.en_out is set, stops rt/frt for
  the erase and write and starts them again (as `start`); hardboot stops the
  HAL first. Bench: 8 of 8 saves kept HAL_OK2, config unchanged. Test:
  hwtest/suite "flashsaveconf x3" and "--load" (hw; the sim has no stall).

## 2026-10-14  HV board reads the DC bus low (and RT_TOO_LONG stops)
- Area:    HV board DC sense (`hw/kicad/v4.0/hv_sense.sch`: R77, R78 249k,
  R79 3.9k into the F3 ADC; `stm32f303/src/comps/io.c` udc, fixed 3.3 V ref),
  `src/main.c` rt ISR overrun check
- Status:  Open (hardware suspected)
- Symptom: Bench, 2026-10-14 from about 19:30: the supply stayed at 300 V
  (user), but hv0.dc_volt fell from 296.9 to ~145 V and then wandered
  148..179 V with the drive disabled. Supply set to 200 V, meter at the
  drive's connector steady 200 V: hv0.dc_volt 137.5..139.2 V as a sawtooth
  (sinks ~1.5 V over 10..15 s, jumps back). Same readings with firmware
  v243, v248 (console changes reverted) and v225, so not firmware.
  pwm_volt follows the low reading, so the F3 outputs ~1.45x the commanded
  voltage. The RT_TOO_LONG stops in the same window were not boots: the
  drive was never reset, and `--load` writes to flash stalled rt (see
  "flashsaveconf stalls rt" below). Unrelated to this fault.
- Cause:   The input feeds the DC link directly (no precharge element), so
  the measurement is wrong: the ADC sees ~1.08 V instead of 1.55 V at 200 V.
  A high-side resistor (R77/R78) gone high, leakage across R79, or the F3's
  3.3 V analog supply off. Not measured yet.
- Fix:     Measure across R79 against GNDPWR (expect 200 V * 3.9k / 501.9k =
  1.55 V) and the F3's 3.3VP. No motor tests until the reading is right;
  bench results from before 19:30 (id_pmsm, first id_mot runs) were taken
  while it still read 296.9 V.

## 2026-10-14  Drive rebooted during id_mot and then hung (once)
- Area:    second bench drive, firmware v0.9.18-243-g128b9b92
- Status:  Open (cause not reproduced); the hang part is fixed
- Symptom: 30 s into id_mot (max_acc 250, state 1.3) the drive dropped off
  USB and re-enumerated (an F4 restart), then enumerated but answered
  nothing on the console until the user power-cycled it. LEDs normal after.
  The same run then passed 3 times (2 with a scope log, 1 polled), and
  id_mot / id_pmsm / LinuxCNC moves all ran without it again.
- Cause:   The hang fits a fault in the main loop: all fault handlers
  (`src/stm32f4xx_it.c`) called hal_error() and returned into the faulting
  instruction, which faulted again forever, while the USB IRQ (priority 15)
  kept the drive enumerated. No watchdog either (HAL_WATCHDOG is off). No
  firmware path resets the chip on its own, so the restart before it is
  still unexplained; a logic-supply dip or EMI reset is most likely.
- Fix:     Fault handlers now save a record in CCM RAM (fault, stacked PC/LR,
  CFSR/HFSR/BFAR, active HAL functions) and reset; `crash` prints it until
  power-off. After 3 crashes without 10 s of running, the drive boots in safe
  mode (no config, no drive components, console working); the next `reset`
  boots normally. Bench, with the `fault` command (now a real bus fault: it
  wrote to locked flash, which only set a flash error flag): the record
  showed BusFault at 0x30000000 with the PC in fault(); 3 quick faults gave
  safe mode, then `reset` a normal boot, config unchanged. Tests:
  hwtest/suite "faults: …" (hw only). If it recurs: run `crash` before
  power-cycling.

## 2026-10-14  stmbl_sim returned the previous packet's answer (1 ms extra lag)
- Area:    `host/lcnc/sim_api.c` (`stmbl_sim_update`), `host/lcnc/stmbl_sim.c`
- Status:  Fixed
- Symptom: LinuxCNC on the simulated drive: following error 0.84..1.0 mm at
  500 mm/s (LinuxCNC's own 1 mm limit tripped in the soak run); the real
  drive does 0.045 mm on the same cycle.
- Cause:   The module handed LinuxCNC the feedback from the previous exchange,
  then exchanged the new packet. The real drive answers each packet at once
  and hm2 reads that answer at the start of the next servo period, so the
  sim's feedback was one period (1 ms) staler: 0.5 mm at 500 mm/s.
- Fix:     Exchange, run the rt cycle in which sserial0 replies, then return
  that reply. The index handshake now compares against the current request.
  Soak cycle on the sim: ferror max 0.040 mm.

## 2026-10-14  hwtest/burnin-sim/sim.txt was cut short by a 64-char line
- Area:    `hwtest/burnin-sim/sim.txt`
- Status:  Fixed
- Symptom: The simulated bus stayed at 300 V although sim.txt set 297 V.
- Cause:   A comment line in sim.txt was 64 characters; hal_parse stops
  there (see "Config/template lines >= 64 chars"), so nothing after it was
  applied. sim.txt is not a template, so the build-time check doesn't see it.
- Fix:     Shorter lines (max 58), and the file now sets the bench motor as
  measured (l 0.0049, r 2.3, j 0.00003) where the config differs.

## 2026-10-14  rt worst case 168.5 us of 200 under LinuxCNC motion
- Area:    F4 rt loop (`hal` rt time), second bench drive, v0.9.18-243
- Status:  Open
- Symptom: First hardware soak run (LinuxCNC moves up to 500 mm/s, console
  checks between cycles): `hal` rt max 168.5 us of the 200 us period (84%).
  Idle and during id_mot it was 104..134 us.
- Cause:   About 119 us of normal rt work (adc0 44 us alone) plus the
  smart-serial frt (priority 0) preempting it: `src/comps/sserial.c`
  ProcessDataRPC busy-waits up to 22 us (28 us in all) for a packet's last
  bytes, once per LinuxCNC period; two packets in one rt cycle make ~170.
  The maxima were set while LinuxCNC started or stopped, not in motion.
  hal's rt time misses the ISR entry delay, and the RT_TOO_LONG check is
  "TCIF set again", so the true margin under LinuxCNC may be only ~2 us.
- Fix:     Not done. Ideas: answer the packet on the next frt tick instead of
  spinning; measure the real deadline from DMA2_Stream0 NDTR at ISR entry
  and exit; sum adc0's packed 16-bit samples with __UADD16 (~30 us less).

## 2026-10-14  rlpsij on the simulated drive: bad psi/J, overspeed in state 5
- Area:    `shared/comps/rlpsij.c`, tested via `servoterm-cli --autotune`
           against `stmbl_host --drive ECMA-C30604FS/stmbl.conf --pty`
- Status:  Fixed (see "rlpsij redesign" below)
- Symptom: True motor r=1.8, psi=0.06, pp=5, j=3e-5, f=0.02. rlpsij gives
  r=1.8005 and polecount=5 (good) but psi=0.026, j=0.0062, d<0. In state 5
  the rotor ran away to 535 rad/s (peak 711 with 1 ms sampling; motor
  max_vel 320 rad/s).
- Cause:   State 4's velocity loop (vel_kp=0.1, vel_ki=0.01) never reaches
  test_vel (rlpsij0.vel stayed ~0.2 rad/s vs 6.28), so the back-EMF fit
  runs near standstill. State 5 applied +-test_cur for 2 s each with no
  speed limit. J used acc = (raw - filtered vel) / period, ~90x too large.
- Fix:     State 5 now reverses torque at +-max_vel (new pin, default
  10 * test_vel, raw velocity) or after vel_time/2, brakes to standstill at
  the end, and computes j per segment as torque integral / velocity change.
  Drive sim peak 70 rad/s (was 535); motsim j 0.000997 for 0.001, 4.9e-5
  for 5e-5. j inherits state 4's psi error. A runaway guard (|vel| >
  2 * max_vel in states 4/5) switches the output off and sets state 7
  (aborted). TODO: state 4 velocity loop / psi.

## 2026-10-14  rlpsij ignores feedback direction (mot_fb_rev)
- Area:    `shared/comps/rlpsij.c`
- Status:  Open
- Symptom: With `rlpsij0.abs_pos_fb = fb_switch0.mot_abs_fb_no_offset` on a
  config with `conf0.mot_fb_rev = 1`, psi came out 0.0079 (true 0.06) and
  j negative; feeding the true shaft angle (`hv0.sim_pos`) gave 0.026.
- Cause:   rlpsij builds com_pos from abs_pos_fb * polecount and has no
  fb_rev input (idpmsm has `fb_rev`), so with reversed feedback the
  commutation turns the wrong way after state 3.
- Fix:     TODO: add an fb_rev pin to rlpsij (as idpmsm) or wire a
  direction-corrected position. Meanwhile this ran away to 518 rad/s in
  state 4; the runaway guard now aborts it (state 7), peak ~127 rad/s
  (trips at 2 * max_vel = 125.7).

## 2026-10-14  servoterm-cli autotune wrote first of duplicate conf0 keys
- Area:    `servoterm-cli/servoterm-cli.c` (`conf_patch`, `autotune_run`)
- Status:  Fixed
- Symptom: After `--autotune` + write, `conf0.j` stayed at the old value: the
  bench config lists `conf0.j` and `conf0.polecount` twice, and only the
  first line was replaced. Negative damping was also written.
- Cause:   loadconf applies lines in order, so the last duplicate wins.
- Fix:     conf_patch replaces the last line for the key, drops earlier
  duplicates, tags it `# autotune <date>`. Non-physical results (r, psi, j
  <= 0, f, d < 0, polecount < 1) are shown as IMPLAUSIBLE and not written.

## 2026-10-14  rlpsij redesign: 20 real motors identified
- Area:    `shared/comps/rlpsij.c`, `host/tests/motor_*.test`
           (`host/tests/gen_motor_tests.py`, motors from `conf/*.txt`)
- Status:  Fixed
- Symptom: On 20 motors (r 0.4-25.5 ohm, j 1.5e-6-3.5e-4, pp 1-5) the old
  rlpsij got r and polecount right but psi wrong on all 20 and j on 19;
  r = 0 on a 25.5 ohm motor; overshoot to 125 rad/s and aborts on very
  light rotors.
- Cause:   (1) state 1 integrator fixed at 10 V/(A*s): ~2.5 s to settle on
  25 ohm, longer than rl_time. (2) psi needed a speed loop with fixed gains
  (kp 0.1, ki 0.01) that cannot hold speed against friction (0.07 A vs
  0.2 A breakaway). (3) At full test_cur a light rotor gains ~240 rad/s
  inside the current loop's ~1 ms rise time. (4) Existing bug: friction was
  never converted from A to Nm and load was multiplied by kt twice.
- Fix:     Gain-scheduled state 1 (settles in ~50 ms for any r); l measured
  in state 2 (voltage square wave at standstill); state 4 = drive segments
  with adaptive current (start test_cur/8, segments >= 10 ms) + coasts,
  psi from a 2-parameter fit uq - r*iq = l'*diq/dt + psi*w_el over all
  samples, j/friction/load/damping from a 4-parameter momentum fit per
  segment; state 5 speed loop tuned from j/kt with the state 4 fit as
  feedforward. vel_kp/vel_ki replaced by vel_bw. New output com_vel for
  hv0.vel. Result: r, psi within 1%, j within 7.4%, friction within 9%,
  l within 7% where l/r >= 0.9 ms, peak speed <= 66 rad/s on all 20.

## 2026-10-14  curpid predictor needs hv0.vel during identification
- Area:    `host/hw/hv.c` (as `stm32f303` curpid), `rlpsij0.com_vel`
- Status:  Note
- Symptom: With rlpsij driving hv0 and hv0.vel unwired (0), the measured
  current fell short of the command at speed (p50b: 0.107 A for 0.375 A
  at 38 rad/s), and in "coasts" (q_cmd 0) held ~ +-friction current.
- Cause:   curpid predicts the next current from the applied voltage minus
  back-EMF computed from hv0.vel * hv0.psi; with vel 0 the predictor is off
  by back-EMF * dt / l. Normal templates wire hv0.vel = vel2.vel.
- Fix:     rlpsij outputs com_vel (electrical speed of com_pos); wire
  `hv0.vel = rlpsij0.com_vel` (host/tests/inc/rlpsij_hv0.txt). The fits
  use the measured current, so they were right anyway; the state 5 speed
  loop was not.

## 2026-10-14  Zero current at speed is not reachable with cur_ind = 0
- Area:    hv current loop (default `cur_ind = 0`, `cur_ff = 0`)
- Status:  Note
- Symptom: Commanding iq = 0 at speed (a "coast") gave -2.5 A of braking
  current on fanuc_a06b; the back-EMF ramps faster than the 1000 rad/s PI
  follows, and the braking it causes feeds the ramp (loop gain ~3.9).
- Cause:   No back-EMF feedforward by default (firmware ls.c defaults).
- Fix:     Identification must not assume i = 0 or settled current: rlpsij
  fits uq - r*iq = l'*diq/dt + psi*w_el over all samples instead.

## 2026-10-14  Identification limits: l at short l/r, damping, 6 decimals
- Area:    `rlpsij0.l`, `rlpsij0.damping`, console pin printing
- Status:  Known
- Symptom: l reads +22..+160% when l/r is 39-260 us; damping (1e-4) reads
  up to 38x off on some motors; j = 1.5e-6 prints as 0.000001.
- Cause:   l/r below ~1 rt period (200 us) is unobservable at 5 kHz.
  Damping torque at test_vel (0.0006 Nm) is lost against friction. The
  console prints pins with %f (6 decimals).
- Fix:     Tests check l only when l/r >= 0.5 ms and read j, l through
  `scale` x1e6. servoterm-cli does not write values < 1e-5 (LOW
  RESOLUTION) and notes the damping/l limits.

## 2026-10-14  pid template: vel2 torque feedforward polecount x too small
- Area:    `conf/template/pid.txt` (vel2), `shared/comps/vel.c`
- Status:  Fixed (sim; check on the bench)
- Symptom: Soak at top speed (288 rad/s, 3000 rad/s^2): festo ran away to
  ~570 rad/s, ferror 0.82 rad; iq_fb had the opposite sign of q_cmd.
  hv0.vel (= vel2.vel) was off from the true electrical speed by up to
  1100 rad/s.
- Cause:   vel2 observes fb_switch0.com_fb (electrical angle) but its
  torque feedforward acc = g * torque / j used j = conf0.j (mechanical)
  with g = 1, so it was polecount x too small. hv0.vel feeds the current
  loop's angle interpolation and, with cur_ind, its back-EMF feedforward.
  Gentle profiles (hwtest/soak: 628 rad/s^2) hide it.
- Fix:     `vel2.g = conf0.polecount` in pid.txt. Peak 306 rad/s, ferror
  0.127 rad (`host/tests/drive_highspeed.test`). mpid.txt's vel0 also
  observes com_fb with a torque input but sets neither j (default 1e-5) nor
  g; not changed (no test covers mpid) - Open.

## 2026-10-14  cur_ind = 0 loses current control at high electrical speed
- Area:    hv current loop, `conf0.cur_ind` (default 0 in all configs)
- Status:  Open
- Symptom: Bench motor (l 0.023, pp 5) at 288 rad/s: braking current
  requested, current stayed positive, voltage at the 164 V limit; speed
  overshoot to 388 rad/s, ferror 0.71 rad. With cur_ind = 1: 302 rad/s,
  0.12 rad.
- Cause:   Without back-EMF/cross-coupling feedforward, w_el * l (33 ohm at
  1440 rad/s) exceeds the PI's proportional gain cur_bw * l (23 V/A).
- Fix:     The soak sets conf0.cur_ind = 1. Consider defaulting cur_ind to 1
  (needs a correct hv0.psi and, now, the vel2.g fix) or documenting it for
  high-speed use.

## 2026-10-14  id_pid tunes some motors too soft / underdamped
- Area:    `shared/comps/ids.c` (id_pid), `conf/template/id_pid.txt`,
           `host` soak (`make soak`)
- Status:  Fixed (2 causes) / Known (3 motors)
- Symptom: With id_pid gains, 4 of 35 motors failed the soak: rx60_456
  (vel_bw 113), sgmph_01a1 (vel_bw 77, position error fault), syn_spindle
  (vel_bw 70, pos_bw 8.5, ferror 0.7 rad), syn_torque_14p (vel_bw 77,
  speed rings to 2x).
- Cause:   (1) The first cycle starts from rest and costs far more or less
  than later ones, and was compared with a fixed min_cost of
  (max_pos - min_pos)^2 / max_vel * 100 = 400: the spindle's 577 cut vel_bw
  at once; sgmph_01a1's and the torque motor's low first cycle made the
  first step up look 20% worse. (2) The test move is fixed (100 rad/s,
  1000 rad/s^2): the spindle needs 20 Nm (peak 14.4) and the torque motor
  can only reach ~33 rad/s, so the loop saturates and the cost measures
  that. Known: (3) id_pid supplies ideal velocity/acceleration
  feedforward (ff = 1), so only noise is left and on rx60_456 it rises
  with vel_bw past 133; through the real command path (LinuxCNC, vel0
  feedforward) vel_bw 500 is 4x better. ff = 0.5 reaches vel_bw 500 but
  drops pos_bw to ~8. (4) The search only lowers vel_d, from 10 to its
  bound 1, and never revisits a parameter: the torque motor ends
  underdamped (75% speed overshoot). (5) sgmph_01a1's rest error is
  stick-slip (friction / j = 40000 rad/s^2), not tuning.
- Fix:     (1) First cycle is a warm-up (`ids0.rep` cycles, default 1);
  the first scored cycle sets min_cost. (2) New inputs ids0.max_torque,
  ids0.j, ids0.mot_max_vel (linked to conf0.max_force, j, max_vel in
  id_pid.txt) limit the move to half the motor's acceleration and speed;
  `ids0.move_vel` / `move_acc` show what is used. Spindle now 500 / 32.2,
  soak passes (`host/tests/drive_id_pid.test`). sgmph_01a1 and the torque
  motor reach vel_bw 500. (3)-(5) are @todo in gen_soak_tests.py.

## 2026-10-14  stmbl_host script mode on a pipe: replies stay buffered
- Area:    `host/main.c` (`run_script`)
- Status:  Fixed
- Symptom: A program driving `stmbl_host --drive CONF` over stdin/stdout
  (the soak generator's id_pid tuning) waited forever for a reply.
- Cause:   Only serve mode set stdout unbuffered; in script mode stdout on
  a pipe is block-buffered, so replies sat in the buffer.
- Fix:     `run_script` flushes stdout after each line read from stdin
  (file scripts unchanged).

## 2026-10-15  pe (power estimation) was never loaded and computed wrong
- Area:    `shared/comps/pe.c`, `conf/template/pid.txt`, `mpid.txt`
- Status:  Fixed
- Symptom: The pid/mpid templates set `pe0.*`, but the component was never
  loaded. Its AC power was `ud*id + ud*id`, its copper loss `3 / 2 * ...`
  (integer division: 1), and ud / uq were wired to 0.
- Cause:   An unfinished component.
- Fix:     3/2 (ud id + uq iq) from hv0's dq feedback (amplitude-invariant
  Clarke); copper loss 1.5 R i^2; new `p_in` (input power: over the
  inverter efficiency, plus idle draw); loaded in pid / mpid. Checked
  against the simulated motor: 0.7 % under torque * speed + copper loss
  (host/tests/drive_power.test).

## 2026-10-15  Smart-serial table: a new field moves scale_address (300 -> 324)
- Area:    `src/comps/sserial.c`, `tools/gentable.c`
- Status:  Note
- Symptom: --
- Cause:   The descriptor table is generated by tools/gentable.c, and the
  global `scale` parameter's address follows the process data records.
  Adding `power` (signed, after `current`, so byte aligned) moved it from
  300 to 324. hostmot2 reads the table at start, so LinuxCNC needs no
  change, but the drive and the table must match.
- Fix:     Edit gentable.c and paste its output; host/tests/
  check_sserial_table.py fails if the C file and the generator disagree,
  or if a byte field is not byte aligned. Input data is 88 bits (power is 8
  bit, see below): 8 left under the 96 of older Mesa sserial firmware
  (sserialb: 224), for the console.

## 2026-10-15  host sim files: a line over 63 characters ends the parse
- Area:    `host/drive.c` (sim extra, `--sim` / stmbl_sim `sim=`)
- Status:  Known
- Symptom: Settings appended to hwtest/burnin-sim/sim.txt had no effect.
- Cause:   The drive's HAL parser stops at a line longer than 63
  characters, a comment line included. sim.txt says so; easy to miss.
- Fix:     Keep sim file lines short (sim_quota_test.txt).

## 2026-10-16  Smart-serial power in 8 bits: noise-shaped, 20 W a step
- Area:    `shared/sserial_q.h`, `src/comps/sserial.c`, `tools/gentable.c`
- Status:  Note
- Symptom: --
- Cause:   At 16 bits, power plus a console field no longer fit old Mesa
  bitfiles (96 bits a direction).
- Fix:     power is 8 bit signed, full scale 2560 W: hostmot2 decodes raw /
  128 * full scale, so 20 W a step exactly, readable to +-2540 W (stmbl's
  2 kW plus headroom). Rounding carries each frame's error into the next
  (ssq_encode), so one frame is within a step, but the average is the
  drive's: 15 W idle reads 0 / 20 W per frame and 14.6 W averaged; the
  fuse model and the energy meter integrate it (burn-in quota test
  unchanged: 1.186 vs 1.189 Wh). Read one frame of `power` as coarse; use
  `powerquota.0.total-power-avg` for a reading.

## 2026-10-16  tools/gentable.c crashed when no padding was needed
- Area:    `tools/gentable.c`
- Status:  Fixed
- Symptom: Adding the 10-bit console fields made gentable segfault.
- Cause:   The padding records were added with `if(bits % 8)
  ADD_PROCESS_VAR(...)` without braces, but ADD_PROCESS_VAR is several
  statements. Only the first was conditional; the bit counting ran anyway
  on an uninitialised table entry. It never showed while padding was
  always needed.
- Fix:     Braces. The console fields now fill 80 / 96 bits exactly.

## 2026-10-16  Console over smart-serial: hm2 pin names, CRC before the reply
- Area:    `src/comps/sserial.c`, `shared/sscon.c`, `linuxcnc/sserialterm`
- Status:  Note
- Symptom: --
- Cause:   hostmot2 names a pin after its record, with no direction
  prefix, so two records called `con` would collide. The firmware built
  its reply before checking the request's CRC.
- Fix:     Records `con_cmd` / `con_fb`, like pos_cmd / pos_fb. The CRC is
  checked first, and only a valid request steps the console. The F3 build
  shares term.c but has no console: weak references there.

## 2026-10-16  setsserial flash skips all-zero blocks
- Area:    `shared/lbp_flash.c`, `tools/mkssfw.py`
- Status:  Workaround
- Symptom: (host model) an all-zero 16 kB block in the image reads 0xFF
  on the device after `setsserial flash`.
- Cause:   setsserial skips blocks of zeros (neither erase nor write), and
  the device erases a whole sector at the first block written into it.
- Fix:     `tools/mkssfw.py` (`make ssfw`) refuses an application with an
  all-zero erase block. Otherwise the CRC check keeps the bootloader from
  starting it, which is safe but confusing. See
  docs/sserial_bootloader.md.

## 2026-10-16  Flash erase/program stalls the whole F4 (single bank)
- Area:    `bootloader/src/main.c`, `shared/comps/fwup.c`, `src/fwup_f4.c`,
  `src/config.c` (`flashsaveconf`, `hardboot`)
- Status:  Workaround
- Symptom: (design) an in-band update that erases staging sectors while the
  drive runs would stop the rt/frt interrupts for 1-2 s (128 kB sector).
  HAL would stop with RT_TOO_LONG or FRT_TOO_LONG, and hm2 would drop the
  smart-serial remote. During `setsserial flash`, an erase commit outlasts
  hostmot2's 1 s wait for the cookie ("Timeout waiting for CMD to clear").
- Cause:   STM32F405 and G431 have one flash bank. While it erases or
  programs, every instruction fetch from flash waits, including vector
  fetches and ISRs. Typical times: 16 kB 0.25 s, 128 kB 1 s (max 2 s), one
  word 16 us (max 100 us). `flashsaveconf` already stalls ~0.25 s today.
- Fix:     The application never erases staging; the bootloader leaves it
  blank (`fwst_boot`). fwup programs one word per rt period and only while
  `fwup0.en` (`fault0.en_out`) is 0. `hal.flash_grace` makes the rt/frt ISRs
  tolerate a late period after a word write instead of stopping HAL. The
  smart-serial loader erases the application area before answering
  anything (`lbpf_erase_all`), so erase commits find blank blocks
  (`lbpf_erase_lazy`). Only a re-run over stale data costs one timed-out
  setsserial run. Its RX uses DMA, so bytes received during a stall are
  kept. None of this has been tried on hardware yet.

## 2026-10-16  Console lines are 63 characters at most
- Area:    `shared/comps/term.c` (64-byte line buffers), `shared/commands.c`
  (`call_cmd`: `sscanf` into `char a[64]` without a width)
- Status:  Known
- Symptom: Longer console lines can't be sent. `call_cmd` also has no
  `sscanf` width, so a longer argument from another path would overflow
  `a[64]`.
- Cause:   Fixed 64-byte buffers.
- Fix:     fwdata lines carry 30 bytes each (60 characters, `FWUP_LINE`).
  Widths in `call_cmd`'s `sscanf` would be a cheap hardening.

## 2026-10-16  In-band update speed over the smart-serial console
- Area:    `servoterm-cli --update`, `shared/sscon.c`
- Status:  Note
- Symptom: Over sserialterm, 3 kB took 9.7 s in the LinuxCNC simulation
  (~350 B/s once running). A 345 kB application would take ~15 min. Over the
  simulator's USB-like pty it ran at ~4.5 kB/s (20 kB in 4.6 s).
- Cause:   The console moves one byte per servo period (1 kHz). A fwdata
  line is 61 characters for 30 image bytes, plus the replies. servoterm-cli
  keeps 3 lines in flight; sscon's RX buffer is 256 bytes.
- Fix:     Acceptable for unattended updates. A binary fwdata (no base64)
  or a wider console field would roughly halve the time.

## 2026-10-16  Capture scope: pre = 1 lost the trigger sample
- Area:    `shared/comps/scope.c` (`do_arm`)
- Status:  Fixed
- Symptom: With `scope0.pre = 1`, the dump said `pre=4096` (n = 4096), an
  index past the end, and the trigger sample was not in the capture.
  servoterm-cli's t = 0 pointed beyond the last row.
- Cause:   post = depth - pre*depth = 0, but the trigger sample itself
  counts as the first post sample.
- Fix:     post is at least 1: with pre = 1 the trigger sample is the last
  (index depth - 1). Auto mode (2) now re-arms after a dump like normal
  mode. Both are checked in `host/tests/test_scope.py`.

## 2026-10-19  fbtest Uno: bootloader loses sync when powered from fb1
- Area:    `hwtest/fbtest` (Arduino Uno on the drive's fb1 RJ45)
- Status:  Workaround
- Symptom: avrdude and a hand-written STK500 uploader both failed. Short
  optiboot commands worked; 128-byte page writes got no reply, page reads
  stopped early, stray `00` bytes appeared, and the loader stayed dead until
  the next reset. Nothing reached flash.
- Cause:   The Uno had fb1 pin 7 (5.19 V) on its 5V pin as well as USB power.
  With pin 7 removed it flashed first time. Not pinned down further: two
  supplies on the 5V net, or a ground/supply loop through the drive.
- Fix:     Power the fixture from USB only (`hwtest/fbtest/README.md`).
  `uno_flash.py` also waits 0.25 s after the reset before syncing (a byte
  sent while optiboot starts makes it reset itself). No automated test: a
  bench wiring fault.

## 2026-10-19  A digit in a comp name makes its pins unreachable
- Area:    `shared/hal.c` (console pin parser), `tools/create_hal_tbl.py`
- Status:  Fixed
- Symptom: a new comp `fb1_out` loaded and showed in `list`, but every
  `fb1_out0.<pin>` read or write answered `not found`.
- Cause:   the console parses `<comp><instance>.<pin>` with
  `%[a-zA-Z_]%li`: `fb1_out0` is comp `fb`, instance 1. (`hx711` is named
  `hx` for the same reason.)
- Fix:     the comp is `fbtx`; `create_hal_tbl.py` now refuses a comp name
  with anything but letters and `_`. Test: `host/tests/test_comp_names.py`
  (in `make -C host check`).

## 2026-10-16  brdstepper: TIM1 CH4 and FDCAN pins collide with USB on the G4
- Area:    `docs/brdstepper_hw.md` (pin plan)
- Status:  Note
- Symptom: On the 48-pin STM32G431/G473/G474, TIM1_CH4 exists only on PA11
  (USB D-), and FDCAN1 only on PA11/PA12 (USB) or PB8 (BOOT0)/PB9.
- Cause:   The pin multiplexing of the UFQFPN-48 package (ST pin data).
- Fix:     Coil B's second PWM input goes to TIM8_CH1 (PC6), synchronised
  to TIM1. CAN goes on PB8/PB9, with the option bytes set (nSWBOOT0 = 0,
  nBOOT0 = 1) so the pin's idle-high CAN RX does not start the ROM
  bootloader.


## 2026-10-16  DRV8876 IPROPI: low side only, one direction only
- Area:    brdstepper current sensing (`shared/bip_loop.c` `bip_sense`,
  `host/hw/hv.c` sim_sense, `docs/brdstepper_hw.md`)
- Status:  Workaround (datasheet-confirmed, simulated; to verify on the board)
- Symptom: (design) a single IPROPI sample cannot give a signed coil
  current, and a stepper's coil currents change sign every electrical turn.
- Cause:   DRV8876 datasheet SLVSDS7B 7.3.3.1: IPROPI mirrors the LOW-side
  MOSFETs, only for current flowing drain to source. Driving, it reads the
  current if it flows the way the bridge drives, else 0. In brake (both low
  sides on, slow decay) it reads the magnitude.
- Fix:     Sample twice per PWM period, in the drive window and in the brake
  window (forward = IN1 high and IN2 PWM, the bridge braking around the
  counter's top). Drive sample ~ brake sample means the current flows with
  the drive; drive 0 and brake > 0 means against it. If the drive window is
  too short to sample, take the sign from the current command. Below a
  noise floor, read 0: the rectified |noise| from the brake window otherwise
  biases the loop. That bias held the motor ~36 mrad (about a full step)
  off position before the floor was added to every branch. In simulation,
  with 5 mA noise, the result matches ideal sensing except at slow speed
  (detent-compensated ripple ~820 vs ~310 rad/s^2). Bidirectional shunt
  amplifiers would avoid all this, at the cost of parts and space.

## 2026-10-16  DRV8876 datasheet corrections (gain, offset, delays, IMODE)
- Area:    `brdstepper/inc/brd.h`, `brdstepper/comps/hv.c`, `shared/bip_loop.c`
  (bip_cal), `host/hw/hv.c`, `docs/brdstepper_hw.md` (parts)
- Status:  Fixed (simulated; to verify on the board)
- Symptom: Before the datasheet (SLVSDS7B), the design assumed AIPROPI
  450 uA/A, with 4.7 kOhm for 1.5 A: 7 V, past the ADC and the VREF clamp.
  In simulation at the datasheet's IPROPI offset limit (7.5 mA), the
  slow-speed ripple went from ~750 to ~9200 rad/s^2.
- Cause:   The datasheet gives AIPROPI 1000 uA/A, an offset of up to
  +-7.5 mA at low current, tPD 0.65 us, tDELAY 1.6 us, and RDS(on)
  0.35 + 0.35 Ohm. In the cycle-by-cycle IMODEs nFAULT also flags plain
  chopping. Input 00 is coast and 11 is brake.
- Fix:     RIPROPI 2.0 kOhm (1.65 A full scale) and an IPROPI cap of at most
  100 pF. PMODE high, IMODE open (fixed off-time, latched overcurrent,
  nFAULT for faults only). sense_window 0.12, max_duty 0.88. Disabled
  means coast (CCR > ARR), not brake. nSLEEP stays high and only pulses
  1.2 ms to clear a latched fault, then the driver gets 1.2 ms (tWAKE)
  before it is driven. An IPROPI offset calibration (bip_cal) runs at
  each enable: braking, rotor still, zero current. With it the
  worst-case ripple is ~1360 rad/s^2 (stepper_ipropi_worst.test). The
  simulator models the 0.7 Ohm bridge, the offset and the gain error.

## 2026-10-16  G4 flash: 64-bit units, each programmed once
- Area:    `shared/fwstage.c` (prog_size, the 24-byte header), `brdstepper/src/flash.c`
- Status:  Fixed
- Symptom: (design) the staging code programmed 32-bit words and later
  turned the header's state word from all ones to 0 in place. The G4
  programs 64-bit double words only, each once until erased (ECC), so
  neither works there.
- Cause:   Flash with ECC: a double word cannot be programmed twice.
- Fix:     fwst_t.prog_size (4 for the F4, 8 for the G4). Data goes in
  whole aligned units, the tail padded with 0xff. The header is 24 bytes:
  magic, size, CRC32, then a state unit of its own at offset 16, which
  stays erased until the bootloader marks the update done.
  `test_bootloader` checks it on a model of the G4 flash that refuses a
  second program of a unit.

## 2026-10-16  CAN on stmbl v4.0: no transceiver; classic and FD don't mix
- Area:    `hw/kicad/v4.0` (P10), `docs/can_support.md`
- Status:  Note
- Symptom: (investigation) the v4.0 schematic has CAN_TX / CAN_RX nets,
  but the board has no CAN transceiver.
- Cause:   The F405's CAN1 (PD1 TX, PD0 RX) only goes, through the ESD
  array U20, to the 2x6 header P10 (pins 5 and 7, next to SWD, CMD lines,
  +5V, +24V). bxCAN is classic CAN only, and not FD-tolerant: it errors
  CAN FD frames on the bus.
- Fix:     CAN on a v4.0 board needs a plug-on transceiver board on P10.
  stmbl v4.0 and FD brdsteppers need separate buses, or all classic.

## 2026-10-17  MCP2515 in the servo loop: filters, priorities, bus time
- Area:    `linuxcnc/stmblcan` (mcp2515.c, stmblcan_bus.c), `host/tests/test_stmblcan.c`
- Status:  Note (simulated; not yet on a Pi)
- Symptom: (simulation) without acceptance filters, a foreign device whose
  frames have higher priority than the feedback (0x050 vs 0x18x) took
  both MCP2515 receive buffers every period. No feedback got through, and
  the drives never became present. With filters, 3 such 8-byte frames a
  period still made drives miss periods.
- Cause:   The MCP2515 has 2 receive buffers, and the higher-priority
  frames reach them first. The 3-frame flood is bus time: 405 us on top of
  the period's own 605 us is more than 1 ms.
- Fix:     Accept only the expected feedback IDs (RXB0's two filters, with
  rollover, so both buffers serve them). Bus time can only be fixed by
  keeping foreign traffic off the servo buses, or giving it lower
  priority and less volume.

## 2026-10-17  stmblcan: a bus's period has to fit the MCP2515
- Area:    `linuxcnc/stmblcan` (stmblcan_bus_check), description files
- Status:  Note
- Symptom: (design) a description file can ask for more frames per period
  than the MCP2515 can hold: 2 drives plus a foreign I/O module need 4
  frames out and 3 in.
- Cause:   3 transmit buffers (SYNC included) and 2 receive buffers,
  serviced once per servo period.
- Fix:     stmblcan_bus_check refuses such a bus at load, with the reason.
  Frames with "every N" are staggered over the periods. Spread devices
  over the buses, or lower their rates.

## 2026-10-17  Index homing: the switched position reached the link a cycle late
- Area:    `conf/template/sserial.txt`, `can.txt`, `conf/haas_spindle_slip_uf.txt` (rt_prio), `shared/comps/idx_home.c`
- Status:  Fixed (simulated, both links; to be confirmed on a real drive)
- Symptom: Index homing (HOME_USE_INDEX) with joint.N.index-enable wired:
  the feedback that cleared index-enable still had the old position, and
  the next one was index-relative. LinuxCNC latched the home off by the
  whole switch (11 mm in host/tests/index_link.test), then the jump
  tripped the following error.
- Cause:   idx_home0 (rt_prio 2.3) ran after linrev0 (2.1), whose fb_in it
  feeds. index_clear reached the link comp (2.0) one rt cycle before the
  switched position did, and over smart-serial and CAN alike.
- Fix:     idx_home0.rt_prio = 2.05, so it runs before linrev0. Tests:
  host/tests/index_link.test (smart-serial), can_index.test (CAN).

## 2026-10-17  Index homing: the drive's loop kept the old command coordinates
- Area:    `shared/comps/idx_home.c` (cmd_offset), `shared/comps/linrev.c`, templates
- Status:  Fixed (simulated, both links; to be confirmed on a real drive)
- Symptom: After a correct index latch, the motor ran away from the
  command by fb_abs - fb (1 mm on the burn-in config: mot_fb_offset at
  10 mm/rev), which tripped the following error at FERROR 1.
- Cause:   At the index, idx_home switches the reported position from fb
  (raw encoder angle) to fb_abs (index-relative), and LinuxCNC moves its
  command into those coordinates. fb_switch still closes the position
  loop on the raw angle plus the offset taken at enable, so it read the
  moved command in the old coordinates.
- Fix:     idx_home0.cmd_offset = fb_abs - fb, active from the command that
  drops index_en (LinuxCNC re-based in that same packet).
  linrev0.cmd_offset takes it off cmd_out. When unconnected it is 0, so
  configs without index homing are unchanged. Tests as above
  (lcncindex.ferr), and hwtest/burnin-sim `make test-can` homes on the index.

## 2026-10-17  can comp: index_out gated by index_clear never finished homing
- Area:    `shared/comps/can.c`
- Status:  Fixed
- Symptom: Over CAN, the drive found the index (position switched) but
  index-enable never cleared; the following error tripped.
- Cause:   index_out = index_enable && !index_clear: idx_home saw index_en
  drop the cycle after it set index_clear, cleared it again, and the
  feedback went back to "searching".
- Fix:     index_out = index_enable, as src/comps/sserial.c does.

## 2026-10-17  stmblcan: vel_scale clips vel-cmd silently
- Area:    `linuxcnc/stmblcan` (vel-clipped pin), `can0.vel_scale`
- Status:  Note
- Symptom: The burn-in machine over CAN (rapids at 500 mm/s) tripped the
  following error on the first rapid.
- Cause:   vel-cmd goes over the wire as int16 with full scale vel_scale
  (default 100), so 500 was clipped to 100 and the drive's velocity
  feedforward was wrong.
- Fix:     Set can0.vel_scale above the fastest joint speed (the burn-in
  sim uses 600); stmblcan takes it from the drive (discover=1). The
  stmblcan.B.N.vel-clipped pin counts clipped periods, so check that it
  stays 0.

## 2026-10-18  No emergency braking: every fault coasted the motor
- Area:    `shared/comps/fault.c`, `qstop.c`, `dbrake.c`, `conf/template/pid.txt`, `pmsm.txt`
- Status:  Fixed (simulated; not yet on hardware)
- Symptom: Any fault, a lost link or a disable switched the power stage off
  at once: a moving axis coasted (burn-in axis from 300 mm/s: 26 mm,
  180 ms).
- Cause:   fault.c only had "outputs off". fault0.dc_brake was computed but
  unused.
- Fix:     Per-cause stop modes in fault.c (cmd_stop, en_stop, fb_stop,
  err_stop; stop_time):
  - qstop: a planned S-curve stop on the feedback (3 mm);
  - dbrake: a virtual resistor in voltage mode, needing no angle, for
    feedback faults (5 mm);
  - HV faults still coast.
  docs/emergency_stop.md; host/tests/estop_*.test.

## 2026-10-18  Link loss arrives as a disable first
- Area:    `shared/comps/fault.c`, sserial / can link components
- Status:  Fixed
- Symptom: With en_stop = 0, a lost link coasted although cmd_stop = 1.
- Cause:   On timeout the link component drops enable in the same cycle
  as it sets its error, so fault0 saw ENABLED -> DISABLED before the
  filtered CMD error.
- Fix:     Enable dropping while fault0.cmd_error is set counts as a link
  loss (cmd_stop).

## 2026-10-18  vel_int: the first command after a standstill was zeroed
- Area:    `shared/comps/vel_int.c`
- Status:  Fixed
- Symptom: (found through qstop's hold2) vel_int0.error flickered for one
  rt cycle at the start of every move after a standstill.
- Cause:   The watchdog test (counter > wd && v != 0) ran before the new
  command reset the counter. The counter runs on while the same packet
  repeats at a standstill, so the first moving packet had its velocity
  zeroed and was flagged.
- Fix:     A new command is never a timeout (!new_cmd in the test). It shifts
  timing by a cycle: index_link.test's jump limit is now 5 encoder counts
  (it was one), and it exposed the next entry.

## 2026-10-18  brdstepper: detent compensation stick-slips with worst-case IPROPI
- Area:    `shared/comps/stp_ttc.c` (detent), `host/tests/stepper_ipropi_worst*.test`
- Status:  Open
- Symptom: At 0.2 rev/s with the DRV8876's worst-case IPROPI errors,
  switching stp_ttc0.detent on mid-move stick-slips in most phases. The
  following error sits at ~0.055 rad with ~0.2 A, then jerks: ~10000
  rad/s^2 of acceleration span, against ~1360 when it doesn't.
- Cause:   Not found. It is phase-dependent: on the code before the
  emergency-braking branch, shifting the test's start by 1..7 rt cycles
  passed 2 of 6. The test passed by luck.
- Fix:     The check moved to stepper_ipropi_worst_detent.test (@todo). Look
  at the current loop's dead zone (B's negative offset reads 0) together
  with the detent compensation at low speed.

## 2026-10-18  Velocity observer lags at high decelerations (quick stop rings)
- Area:    `shared/comps/vel.c` (vel1), `shared/comps/qstop.c`
- Status:  Known
- Symptom: A quick stop near the motor's capability over-brakes and rings
  (burn-in motor from 188 rad/s at 80 %: down to -117 rad/s). pid's
  vel_fb ran ~2 ms behind the real velocity.
- Cause:   vel1 passes its torque feedforward through a 50 Hz high-pass
  (acc - lowpass(acc)). A sustained deceleration is tracked by its PLL
  (w = 1000) alone, which lags by about dec / w. LinuxCNC's moves (a few
  hundred rad/s^2) don't notice.
- Fix:     qstop brakes at 30 % of capability with a 3 ms S-curve by default
  (3.1 mm, 0.01 rad of reverse travel). Harder stops would need the
  observer to keep its torque feedforward during a stop.

## 2026-10-18  A failed encoder drives the motor for 1 ms before the fault
- Area:    `shared/comps/fault.c` (err_filter on mot_fb_error), `shared/comps/dbrake.c`
- Status:  Fixed (simulated)
- Symptom: (simulation) after enc_fb0.sim_fail at 188 rad/s, the 5 rt
  cycles until MOT_FB_ERROR commutated on the frozen angle: uq rose to
  114 V and the current to ~2 A in the wrong direction, before dbrake
  took over.
- Cause:   err_filter needs 5 cycles. Commutation keeps the last angle.
  Even a zero current command can't be held in that frame at speed: on
  rx60_123 the current still rose to 3 A.
- Fix:     dbrake0.en_out gates hv0.en: the bridge is off while the raw
  feedback error is set (fb_err0/1 = fault0.mot/com_fb_error), until the
  filter decides. A glitch just resumes.

## 2026-10-18  F3: the config index allowed one word past the array
- Area:    `stm32f303/src/comps/ls.c` (WRITE_CONF / READ_CONF)
- Status:  Fixed (compiles; not run on an F3)
- Symptom: none seen; found by reading the code.
- Cause:   `CLAMP(a, 0, sizeof(config) / 4)` lets a == 9 through, one past
  the 9 floats, so a WRITE_CONF there writes past `config`. Today's F4
  never sends 9, but an F4 with a longer config (another version) would.
- Fix:     `sizeof(config) / 4 - 1`. New F3 features must still not add
  config words without a version check: an older F3 has this bug. That is
  why brake_on_loss is a spare flag bit.

## 2026-10-18  F3 voltage mode clamps current by lowering the voltage
- Area:    `shared/comps/curpid.c` (VOLT_MODE), dynamic braking
- Status:  Fixed (simulated; needs the F3 firmware)
- Symptom: The ebrake suite: on manutec_r15_1 (0.4 ohm, 2 mH) the brake
  current went to 24 A against a 5.25 A limit.
- Cause:   Above max_cur, curpid scales the commanded voltage down (kci
  500). For u = -rv * i that is less resistance, so more current; the
  scale ran to 0, a plain short at the motor's short-circuit current.
  Keeping the brake at 0.9 * max_cur didn't help once a transient crossed
  max_cur.
- Fix:     curpid0.brake (on the F3: ls0.vbrake = flags.brake from the F4's
  dbrake, or the F3's own brake) applies only the voltage limit. An older
  F3 ignores the flag. host/hw/hv.c models the clamp and the flag.

## 2026-10-18  The F3 brakes when the F4 is lost (LV side gone)
- Area:    `stm32f303/src/comps/ls.c`, `shared/vbrake.h`, `src/comps/hv.c`, `shared/common.h`
- Status:  Fixed (simulated; F3 not yet run)
- Symptom: With the F4 crashed, reset or unpowered while enabled, the F3
  switched off on its packet timeout and the motor coasted (25 mm in the
  simulator).
- Cause:   Only the F4 knew how to stop. The HV side keeps running, though:
  it is powered from the DC bus (hv_supply.sch: LNK304, ACT4088).
- Fix:     The packet flag brake_on_loss (a spare bit, compatible both
  ways), set by the pmsm template. On timeout, if the last packet enabled
  the drive, the F3 runs the shared/vbrake.h brake itself until the current
  has died down (6 mm). host: hv0.sim_f4_dead, estop_f4dead*.test.

## 2026-10-18  A power cap on the dynamic brake can't work with a current limit
- Area:    `shared/vbrake.h`, `conf0.max_hv_power`
- Status:  Note
- Symptom: Capping the brake's bus power (1.5 rv i^2) at 20 W let the current
  reach 4.3 A against a 1.8 A target; applied only below the target, the
  brake chattered and nothing changed.
- Cause:   rv rises only to hold the current down, so any cap on the power it
  puts into the bus lets the current rise.
- Fix:     The current comes first (overcurrent destroys the stage at once);
  the bus has high_dc_volt, max_dc_volt and the F3's ABS_MAX_VOLT.
  conf0.max_hv_power limits the quick stop's regenerated power
  (qstop0.max_power), where it does work.

## 2026-10-18  Virtual-resistor brake unstable on low-inductance motors
- Area:    `shared/vbrake.h`
- Status:  Fixed (simulated)
- Symptom: The ebrake suite: on the Yaskawa r2aa06020f (2.7 ohm, 0.7 mH) the
  brake voltage flipped sign every rt cycle (+-164 V) and the current rang
  at ~15 A against 5.6 A.
- Cause:   u = -rv * i is applied a delay after i was measured (2 F4 rt
  periods). With L / R (0.26 ms) not much longer than that, an rv well above
  the winding's impedance is delayed feedback with a gain above 1.
- Fix:     rv stays below l / delay (dbrake0.l = conf0.l; the F3 uses its own
  period). If the current then can't be held (above 1.3 x the target for
  2 ms), the brake gives up and the outputs go off (gave_up): coasting is
  safe for the stage. In the suite only manutec_r15_1 with the F4's brake
  gives up.

## 2026-10-18  The brake's start as a short overshot the current
- Area:    `shared/vbrake.h`
- Status:  Fixed (simulated)
- Symptom: Up to twice the short-circuit current in the first ms of a
  dynamic brake (manutec 9 A, sr60_1 15 A for 5 / 11 A motors).
- Cause:   A sudden short has a transient of up to twice its steady current,
  and rv started at 0 and only rose once the current was over the target.
- Fix:     Start at rv = rn (pwm_volt / max_cur: about the target at full
  back-EMF) and relax towards the short while the current is below target.

## 2026-10-18  host build missed changes in shared/*.h
- Area:    `host/Makefile` (stmbl_host)
- Status:  Fixed
- Symptom: A change to shared/vbrake.h alone did not rebuild stmbl_host;
  tests ran the old code.
- Cause:   The stmbl_host rule depended on host/*.h and the sources, but not on
  the shared headers.
- Fix:     $(ROOT)/shared/*.h added to its prerequisites.

## 2026-10-18  The simulated F3 runs at 20 kHz, the real one at 15 kHz
- Area:    `host/hw/hv.c` (F3_PER_RT), `stm32f303/src/main.c` (hal_init 1/15000)
- Status:  Known
- Symptom: none.
- Cause:   The model steps the F3 current loop 4 times per 5 kHz rt cycle.
- Fix:     Keep in mind for rate-sensitive F3 code: the F3 brake's rv limit
  (l / 1.5 periods) is a little lower on the real F3.

## 2026-10-18  Simulated DC bus: rectified mains into the drive's own caps
- Area:    `host/hw/hv.c` (sim_supply, sim_mains, sim_cap, sim_chop_*), `host/sim_board.txt`
- Status:  Note
- Symptom: the old fixed 300 V supply soaked up any regeneration, so no
  over-voltage path was ever exercised.
- Cause:   model choice.
- Fix:     230 V 50 Hz through a diode bridge and 1 ohm into 540 uF, no
  supply caps; the F3 sees the bus through io.c's ~1.3 ms filter and trips
  at 410 V. A 47 ohm chopper at 345 / 335 V stands in for the planned supply
  card; without it the soak's manutec_r15_1 and sr8_0 and the spindle fail
  (regen pushes the bus past high_dc_volt, fault0 scales the current to 0,
  following error). Tests in `host/tests/bus/`. Not modelled: freewheel
  diodes charging the bus from back-EMF while the bridge is off.

## 2026-10-18  Without a chopper a hard stop overshoots the F3's 410 V
- Area:    DC bus, `shared/comps/fault.c` (max_dc_volt), supply card design
- Status:  Known
- Symptom: syn_spindle (829 J at speed) quick stop with no chopper: fault0
  trips HV_VOLT_ERROR at 380 V but the bus is at ~427 V when the bridge is
  off (`host/tests/bus/bus_regen_nochop.test`).
- Cause:   540 uF hold ~20 J between 323 and 410 V; braking at kilowatts
  the bus climbs ~40 V/ms, and the trip takes err_filter's 5 rt periods
  plus the F3's measurement filter.
- Fix:     hardware: a brake chopper (docs/emergency_stop.md). Idea for the
  firmware: limit regenerative torque only (fault0's dc_volt scale cuts
  motoring torque too, which doesn't lower the bus), or dissipate in the
  windings with d-axis current while the bus is high.

## 2026-10-18  dbrake gave up the current limit when the bus was high
- Area:    `shared/vbrake.h`
- Status:  Fixed
- Symptom: ebrake_syn_spindle_fb with the mains bus: 64 A against the 52 A
  limit.
- Cause:   above high_dc_volt the error was forced to -1 (towards the
  short) regardless of the current.
- Fix:     back off only while the current is below its target; if the bus
  still rises the F3 cuts off at 410 V and the motor coasts.
  `estop_bus_high.test` rewritten (it had checked the old give-up).

## 2026-10-18  Chopper and high_dc_volt below the +10 % mains peak
- Area:    `conf/template/conf.txt` (high_dc_volt 350, max_dc_volt 380), supply card
- Status:  Open
- Symptom: at 253 V mains (peak 356 V) the simulator's 345 V chopper burns
  ~460 W at standstill and the bus sits above high_dc_volt
  (`host/tests/bus/bus_mains_high.test`).
- Cause:   the template's limits are below 230 V +10 % rectified.
- Fix:     for the card: chopper ~375 / 365 V and the drives' high_dc_volt /
  max_dc_volt ~385 / 400 V (tested in bus_mains_high). Changing the template
  defaults would also apply to drives without a chopper; left to the user.

## 2026-10-18  ebrake suite: the spindle's quick stop rings on bench gains
- Area:    `host/tests/gen_ebrake_tests.py` (loop gains: the bench motor's)
- Status:  Open
- Symptom: ebrake_syn_spindle_link: the current swings 3 <-> 40 A for the
  whole stop (same with a stiff 300 V supply); with the mains bus that is
  80 V of bus swing. The checks pass (peaks below 52 A).
- Cause:   the generator doesn't use id_pid's gains; the soak's tuned gains
  (pos_bw 32, vel_d 4.1) settle in a few cycles.
- Fix:     idea: take the gains from the soak's id_pid result.

## 2026-10-18  fault0.last_fault doesn't keep an HV fault behind a link error
- Area:    `shared/comps/fault.c`
- Status:  Note
- Symptom: link lost, then the F3's over-voltage trip: fault0.fault and
  last_fault read CMD_ERROR afterwards; hv0.fault reads 0 once the F4 has
  disabled the F3 (io.c clears it).
- Cause:   the cmd_error filter rewrites the fault every cycle while the link
  is down.
- Fix:     tests read `hv0.sim_hv_fault` (kept). The stop logic is right
  (hv_fault ends the stop).

## 2026-10-18  Mains loss shows up as a following error
- Area:    `shared/comps/fault.c` (min_dc_volt 20 V)
- Status:  Note
- Symptom: mains gone at 188 rad/s and ~300 W: the bus falls ~2.5 V/ms; at
  ~190 V the burn-in motor can't hold speed; POS_ERROR, dynamic brake
  (`host/tests/bus/bus_mains_loss.test`).
- Cause:   no under-voltage stop above min_dc_volt.
- Fix:     fine as is (the dynamic brake needs no bus). Idea: start the quick
  stop on a falling bus while there is still voltage to brake with.

## 2026-10-18  LinuxCNC tests refuse to run as root
- Area:    `hwtest/burnin-sim` (make test-*), rtapi_app
- Status:  Workaround
- Symptom: make test-can fails with an empty linuxcnc.log.
- Cause:   rtapi_app: "Refusing to run as root without fallback UID"; with no
  tty linuxcnc sends its output to a temp file that it deletes.
- Fix:     run as the `lcnc` user: `chmod -R a+rX hwtest servoterm-cli;
  su lcnc -c 'cd .../hwtest/burnin-sim && make test-can'`.

## 2026-10-19  Probe phasing fails its check on a light rotor
- Area:    `shared/comps/fb_switch.c` (phase_mode 1), `conf/template/pid.txt`
- Status:  Fixed
- Symptom: merging emergency-braking, its generated suite enables each of
  the 35 motors of `motors.py` and phases with probe phasing (master's
  default since 2026-10-17). syn_servo_2kw (j 0.0012, f 0.9 Nm, 4 poles)
  at phase_cur 7.5 A: fault 17, phase_fail 2 (a check pulse moved the rotor
  the wrong way); 1 A and 4 A phased.
- Cause:   the same as "Probe phasing ~11 deg off with cur_ind" (below):
  the estimate was off, at a high current enough to fail the check. The
  bus and ebrake tests had also waited 1.5 s after enable, written for
  align phasing; probe phasing of the heavy rotors takes 2-4 s.
- Fix:     vel2.en = fb_switch0.state. Tests:
  `phasing_check_light_rotor.test`, `drive_phasing_cur_ind.test`. The bus
  tests and the ebrake and soak generators set a phasing current for the
  motor (25 % of max_ac_cur), wait 3 s and check `fault0.state == 1`.

## 2026-10-14  Encoder monitor (encmon) prototype
- Area:    `shared/comps/encmon.c`, `conf/template/encmon.txt`, fault code
           17 `FB_MON_ERROR` (`shared/common.h`, `fault.c` input
           `fb_mon_error`), sim fault injection in `host/hw/enc_fb.c`
           (`sim_freeze`, `sim_scale`, `sim_offset`); branch
           `claude/encoder-monitor`
- Status:  Note (prototype, sim only)
- Symptom: (design) An encoder that loses/gains counts or slips its
  coupling keeps the drive running with wrong position/commutation; the
  position loop cannot see it.
- Cause:   N/A.
- Fix:     encmon checks the dq voltage equation with the encoder's angle
  and speed. Lessons: use w differentiated from the encoder angle, not
  vel2.vel (the observer lags ~55 rad/s el in a 3000 rad/s^2 ramp: healthy
  ratio 0.30 -> 0.02), and rotate the back-EMF by delta = com_fb - hv0.pos
  (the current loop's frame). Bench motor at 100 rad/s: 2% counts lost ->
  trips in ~50 ms, 0.1 rad angle step in ~22 ms; frozen encoder: position
  error trips first. Floor: ~17 deg el angle error. Blind below min_bemf
  (5 V; bench motor ~21 rad/s). Model errors (r +40%, psi +-10%, l -50%)
  keep the healthy ratio <= 0.21 (trip 0.3). Soak (35 motors) never trips;
  longest time above 0.3 is 0.8 ms (syn_slow_lr, unramped velocity steps)
  vs trip_time 20 ms. Untested: hardware, out_rev = 1, field weakening.

## 2026-10-19  Encoder monitor against probe phasing: false residuals
- Area:    `shared/comps/encmon.c`, `conf/template/encmon.txt`
- Status:  Fixed
- Symptom: merging encoder-monitor into master (probe phasing default),
  `drive_encmon_ok` (healthy motor) peaked at a ratio of 0.99, then 0.56,
  against 0.1 expected and a 0.3 trip level.
- Cause:   1) `encmon0.en = fault0.en_out` checked while probing, when the
  field is turned away from the rotor on purpose. 2) Probe phasing left the
  commutation ~11 deg off on the bench motor in the sim (align: 0.3 deg)
  until the index; the jump to the absolute angle at the index then made
  a one-cycle speed spike in encmon's differentiated angle.
- Fix:     the template gates on `fault0.en_pid` (phased; not in the
  dynamic brake), `encmon0.com_src = fb_switch0.current_com_pos` (checks
  only on an absolute angle, 1..3), and 20 ms hold with the filters reset
  when the source changes. Test: `drive_encmon_ok.test`. The ~11 deg
  itself: "Probe phasing ~11 deg off with cur_ind" (fixed). Still: with no
  index or absolute feedback the monitor never checks (com_src stays 4);
  with an accurate probe that gate could be relaxed.

## 2026-10-19  Simulated encoder: summed rounding moved count edges
- Area:    `host/hw/enc_fb.c` (sim)
- Status:  Fixed
- Symptom: after merging encoder-monitor, `stepper.test`'s acceleration
  span 520 against < 400.
- Cause:   encoder-monitor's fault injection summed the angle every cycle
  (`mod(angle + d * scale)`); float rounding shifted count edges.
- Fix:     faults kept as an offset to the shaft angle, exact without
  one. Test: `stepper.test`.

## 2026-10-19  In-band update silently not applied (staging header layout)
- Area:    `shared/fwstage.c/.h` (fwup, bootloader)
- Status:  Fixed
- Symptom: on the bench, `make update` of the integration build staged and
  "applied" (CRC right, `fw ok reset`), the drive reset in ~4 s and ran the
  old application. Three times. The first update, from v318, had worked.
- Cause:   the brdstepper branch (G4: 8-byte units) moved the pending
  header to 24 bytes with the state at 16 for every flash. The drive's
  bootloader (v312) reads the old 16-byte one, found nothing pending and
  erased the staging area. Every drive with an older bootloader would have
  lost in-band updates once it ran this application.
- Fix:     4-byte units keep the 16-byte layout (state at 12); only 8-byte
  units use 24. Test: `test_bootloader` "F4 pending mark in the layout of
  the bootloaders in the field". Recovered the bench drive with
  `make ssburn` (the loader writes the application directly).

## 2026-10-19  Phasing suite: one align check move at 0.40 mm; a non-text console byte
- Area:    `hwtest/phasing` on v0.9.18-343 (emergency braking merged)
- Status:  Note
- Symptom: 1) align (the old method, compared only) at 4.4 mm: check move
  error 0.40 mm (limit 0.3; all others <= 0.156 mm), no fault. Not seen
  again in 16 positions 0..4.5 mm (32/32) or on master. 2) the first run
  stopped on a 0x80 byte in a USB console answer (UnicodeDecodeError);
  not seen in the two runs after.
- Cause:   not known. 1) perhaps the index found during the check move
  (the angle jumps from align's estimate to the absolute one).
- Fix:     `phasing.py` logs non-text console bytes ("console: bytes that
  are not text") instead of stopping. Watch both.

## 2026-10-19  Probe phasing ~11 deg off with cur_ind
- Area:    `conf/template/pid.txt` (vel2), `shared/comps/fb_switch.c`
- Status:  Fixed
- Symptom: in the sim, probe phasing on the bench motor ended 11.4..12.0
  deg off with conf0.cur_ind = 1 from every start position (0.6 deg with
  cur_ind = 0); the encoder monitor saw it as a bad encoder.
- Cause:   vel2 (hv0.pos, hv0.vel from com_fb) followed the probe's field
  turns between pulses: up to ~94 rad/s with the rotor still, ~5 rad/s left
  when the next pulse started. cur_ind feeds that speed forward (uq +=
  vel * psi): up to 0.15 A of q current between pulses against a 0.125 A
  pulse, -0.04 A during it, so every pulse pushed ~17 deg off its angle,
  always in the turn's direction.
- Fix:     vel2.en = fb_switch0.state (0 while phasing or disabled): the
  angle passes straight through, the speed is 0. `fault0.en_pid` was tried
  first: id_pid runs without the pid, and the soak got worse. Test:
  `drive_phasing_cur_ind.test` (com_fb 1.6965 against 1.5 +- 0.035 before).
  The bench drive runs cur_ind 0, so its phasing was not affected;
  configs with cur_ind 1 were. Not yet run on hardware.

## 2026-10-19  Host soak: four motors failed since probe phasing became default
- Area:    `host/tests/gen_soak_tests.py`
- Status:  Fixed
- Symptom: `make -C host soak` on master: fanuc_a06b and weiler_z (id_pid
  did not finish, position error), sr8_0 and syn_slow_lr (following
  error); not noticed when probe phasing became the default (2026-10-17).
- Cause:   the profile started 1.5 s after enable; probe phasing of the
  heavy rotors takes longer, at the bench drive's phase_cur (1 A) up to ~4 s.
- Fix:     phase_cur at 25 % of the motor's current (id_pid too), 3 s,
  `fault0.state[0] == 1` checked. Soak 381/385 (4 known).

## 2026-10-19  Align phasing refused at its dead point (phasing suite)
- Area:    `shared/comps/fb_switch.c` (phase_mode 0), `hwtest/phasing`
- Status:  Known
- Symptom: phasing suite on v0.9.18-345, align at 0.55 mm: fault 17,
  phase_fail 2, swing 0.003 mm. The same position passed in other runs.
- Cause:   align pulls the rotor onto the field; started with the field
  ~180 deg from the rotor's d axis there is no torque, the rotor stays and
  align takes a 180 deg wrong angle. The check after phasing sees the
  rotor go the wrong way and refuses: what it is there for. Probe
  phasing (the default) has no dead point.
- Fix:     `phasing.py` counts an align refusal with phase_fail 2 and less
  than `--dead-swing` (0.01 mm) movement as "ok (refused)", like the
  brake-held cases.

## 2026-10-19  7C81 "sserialx8" firmware: smart-serial only on P7
- Area:    Mesa 7C81 FPGA firmware (bench card)
- Status:  Note
- Symptom: the user's new build (sserialx8_latest.bit, TopGCSPIHostMot2,
  6slx9tqg144, 2026/10/19 17:19) wrote and verified, and readhmid showed 1
  SSerial with 8 channels, all on P7 (DB25: RX0-3 P7-01..04, TX0-3
  P7-05..08, RX4-7 P7-09..15, TX4-7 P7-17..23, TXEn7 P7-25 only). P5/P6
  (the RJ45 ports with the on-board RS-422 parts, where the drive is
  cabled) are unassigned; no StepGen, QCount or PWM.
- Cause:   that firmware's pin file.
- Fix:     not testable on the bench as wired (needs RS-422 transceivers
  on P7, or channels 0/1 on P5/P6 as in 7c81_5abobx2d). The card is back
  on 7c81_5abobx2d.bit (verified, readhmid identical to before; LinuxCNC
  link test ok). mesaflash cannot back up a 7C81's flash; restore by
  writing the bitfile. Files: /home/cnc/stmbl-firmware/mesa-7c81-20261019-1742

## 2026-10-20  Emergency braking on hardware; test notes
- Area:    `hwtest/suite` ebrake phase, `src/comps/hv.c` (drop_link)
- Status:  Note
- Symptom: results in `docs/emergency_stop.md` "On hardware" (all four
  causes at 50/150/300 mm/s pass; 300 mm/s: 1.46 / 1.49 / 0.55 / 0.75 mm).
  Getting there:
  1) a scope trigger on `fault0.en` never fired on machine off; the
     drive's own `fault0.state` does (fall through 0.5; faults rise
     through 2.5).
  2) `vel1.vel` goes to 0 when the dynamic brake starts (en_fb off): the
     test measures from `enc_fb0.pos`, unwrapped.
  3) link loss by `halcmd stop`: the card's watchdog bit and hostmot2
     stopped the port ("too many errors"); the machine did not come back
     on. `sserial.port-0.run = 0` drops the link the same way for the
     drive and LinuxCNC recovers.
  4) after the port restarts LinuxCNC once reported "Exceeded POSITIVE
     soft limit (1000)" (drive disabled then: a junk position read, no
     motion); the test then re-homes.
  5) with the F3 link dropped the F4 reports the HV timeout, LinuxCNC
     switches off: the drive goes to state 0, so that case triggers on
     `hv0.drop_link`.
  6) the 0x80 console byte again (`fault0.state = 1.00\x80000`, USB, while
     the scope was set up): the suite logs such bytes now
     (`console_bad_bytes`). Open: where it comes from.
- Fix:     see above; the phase is the regression test for the braking.

## 2026-10-20  An input linked to an input reads a stale value until relink
- Area:    `shared/hal.c` (PIN() reads source->value, one level), console
- Status:  Known (procedure)
- Symptom: on the bench, encmon loaded at runtime (`stop`, `link encmon`,
  `start`) tripped at once (fault 18, ratio up to 2.2); its residual rotated
  at the electrical frequency. A scope channel on `encmon0.pos` recorded 0,
  and a scope trigger on `fault0.en` never fired.
- Cause:   `encmon0.com = hv0.pos` links to hv0.pos, itself an input
  (linked to dbrake0.pos_out): PIN() follows one link, so encmon read
  hv0.pos's own stale value. `relink` flattens such chains; the boot does
  it after loadconf, the host tests do it, my runtime load did not. Scope
  channels and triggers linked to inputs have the same problem.
- Fix:     load at runtime with `stop`, `link X`, `relink`, `start`; link
  scope channels/triggers to outputs (or relink after). With relink the
  monitor's angle input is dbrake0.pos_out.

## 2026-10-20  Encoder monitor: the real F3's frame leads hv0.pos (0.12 ms)
- Area:    `shared/comps/encmon.c`, `conf/template/encmon.txt`
- Status:  Fixed (Open: the q residual)
- Symptom: relinked, the monitor's ratio sat at 0.24 (max 0.275, trip 0.3)
  above 250 rad/s on the bench motor: ud -43 V where the model expected
  ~-10 V at 300 rad/s.
- Cause:   the F3 reports ud/uq/id/iq in a frame ahead of hv0.pos (it
  extrapolates the angle between packets, its readings come a packet
  late). The simulated F3 reports in hv0.pos's frame: the sim never showed
  it.
- Fix:     `encmon0.delay` (template 0.12 ms): delta += w * delay. Bench,
  speed above 250 rad/s: delay 0 ratio 0.24 (max 0.275), 0.12 ms 0.10
  (max 0.14), 0.20 ms 0.10 but err_d +6 V (over). A first offline fit said
  0.32 ms; it ignored the lead already in the angle chain and over-turned
  (err_d +10 V). The sim's encmon tests and soak set delay = 0.
  `max_ratio` is the wrong margin measure: one sample at the instant the
  monitor turns on (bemf at min_bemf, 5 V) reached 0.27-0.30; a trip needs
  20 ms above thres (`max_timer`, 0 in every bench run). Open: err_q -6.7 V
  (~10 % of the back-EMF) at speed: dead-time loss or psi a little high.
  Regression: the hardware soak with encmon loaded (soak.py records
  encmon_max_ratio / encmon_max_timer_s).

## 2026-10-21  LinuxCNC 2.9.10 and hm2_spix on the bench
- Area:    Pi 4 + Mesa 7C81 (sserial_p5p6_x8 firmware), `hwtest/burnin/stmbl.ini`
- Status:  Note
- Symptom: upgraded 2.9.1 -> 2.9.10 (apt, linuxcnc.org; 2.9.1 is gone from
  the repo, the upgrade is one-way). sserialterm and stmbl_sim rebuilt.
  hm2_rpspi on 2.9.10: full suite 33/33, drive rt max 165 us (as on 2.9.1).
  hm2_spix:
  - spiclk_rate=50000 refused ("outside acceptable range (7..50000 kHz)"),
    hostmot2 does not load, LinuxCNC stops (errors only in
    ~/linuxcnc_debug.txt).
  - 41666 for both: "MISO input is bit-shifted by one bit. SPI read clock
    frequency probably too high", no board.
  - 41666 write / 31250 read and 35714 work.
  - Pi side, steady state (tmax cleared, 10 s, two runs each): servo thread
    67-72 us (max 175-192) at 35.7 MHz against hm2_rpspi 74-76 us (max
    190-270) at 31.25 MHz; hm2 write 24 against 28 us, read 34 against 36.
- Cause:   card/cabling limit on reads; hm2_spix's queueing gains ~5-10 %
  on a Pi 4.
- Fix:     DRIVER = hm2_spix, DRIVER_PARAMS = spiclk_rate=35714
  spiclk_rate_rd=31250 (one step below each limit, as its manual advises).
  Test: hwtest/suite "Pi side" timing test (host_* measurements).

## 2026-10-21  Align phasing: the rotor drifts when align lets go (current offset)
- Area:    `stm32f303/src/comps/io.c` (current offsets), `shared/cur_offset.h`
- Status:  Fixed
- Symptom: after LinuxCNC 2.9.10 / hm2_spix, the phasing suite's align
  cases failed at most positions: swing 0.3-0.7 mm, check move error up to
  0.8 mm, some fault 17 (check the wrong way). Probe and brake cases fine.
- Cause:   not LinuxCNC, hm2_spix, the Mesa firmware or the drive firmware
  (A/B of each). A capture of align's hand-over to its check: with zero
  current commanded the bare motor (nothing coupled) ran 0.425 mm (15 deg)
  and stopped. The F3 calibrates its current offsets once at HV power-up;
  with the bridge off it read ~0.09 A (id -0.08, iq +0.05): commanding 0 A
  then drives a real 0.09 A DC, a fixed field the frictionless rotor rolls
  to. Probe phasing never holds and releases at 0 A. Drift grows with
  time/temperature (worse after a 2.7 h soak). `--index-jump` shows
  align's angle itself is fine (-8..+13 deg).
- Fix:     the F3 trims its offsets while the bridge is off (after 0.1 s,
  tau 1 s, readings above 0.5 A ignored): `shared/cur_offset.h`,
  `host/tests/test_cur_offset.c`. Also the align check starts at
  phase_cur / 8 and grows (drive_phasing_align_gentle.test).
  Bench, v0.9.18-356: offset reading with the bridge off 0.09 -> ~0.01 A;
  phasing suite 36/36 (align swing <= 0.018 mm, move error <= 0.115 mm).

## 2026-10-21  Preview: the gcode module segfaults on a T word without LinuxCNC running
- Area:    `webui/mill/server/preview.py` (LinuxCNC 2.9 `gcode.parse`)
- Status:  Known
- Symptom: `gcode.parse` of a program with `T3` (or `M6 T3`) crashes the
  Python process (SIGSEGV) when LinuxCNC isn't running; without T words it works.
- Cause:   since 2.9 the interpreter reads tool data from LinuxCNC's tool
  data shared memory, not from the canon's `get_tool`.
- Fix:     preview only while LinuxCNC runs (millweb always does). Unit tests
  of the preview avoid T words; the live test covers them.

## 2026-10-21  Preview canon: external length units 1.0 on an mm machine; tool numbers from pockets
- Area:    `webui/mill/server/preview.py`
- Status:  Fixed
- Symptom: the preview came out 25.4x too large and offset; the tool change
  named tool 2 for T3.
- Cause:   `get_external_length_units` must return stat.linear_units (1.0
  for mm), as rs274's StatMixin does: the canon works in inches either way.
  `change_tool` gets the tool table row ("pocket", from 1), not the T number.
- Fix:     1.0; the backend passes the tool table's row order.

## 2026-10-21  Probing from a remote console: probe trips and probe travel
- Area:    `webui/mill/server/machine.py` (cmd_probe, cmd_tool_measure), `webui/mill/test/sim/mill.hal`
- Status:  Fixed
- Symptom: (1) "Probe tripped during non-probe move" ended a program; (2)
  "Probe move would exceed Z's negative limit": the tool setter never touched;
  (3) a probe result of 0 right after the move.
- Cause:   (1) the simulated probe tripped whenever the tool was below the
  stock top, cutting included; a real probe isn't in the spindle then, and
  LinuxCNC aborts a move the probe trips in. (2) LinuxCNC refuses a G38 whose
  end is beyond the soft limit, even if it would trip before. (3)
  stat.probed_position can lag the interpreter going idle.
- Fix:     the sim's probe trips only during probe moves (motion-type 5) and
  on the tool tip (joint Z less motion.tooloffset.z); millweb shortens the
  travel to what the soft limit leaves, and waits up to 1 s for the result.

## 2026-10-21  millweb: an MDI move reports run "mdi", not "run"
- Area:    `webui/mill/server/machine.py` (status), clients
- Status:  Note
- Symptom: a client that waited for run != "run" after MDI went on while the
  machine still moved.
- Fix:     wait for interp "idle" (the live test does); run is "mdi" during MDI.

## 2026-10-21  LinuxCNC: "machine off" reads back as STATE_ESTOP_RESET
- Area:    `webui/mill/server/machine.py` (cmd_power)
- Status:  Fixed
- Symptom: switching the machine off answered "LinuxCNC didn't switch the
  machine off", though it was off.
- Cause:   after `state(STATE_OFF)` stat.task_state reads STATE_ESTOP_RESET
  (2), never STATE_OFF (3); millweb waited for STATE_OFF.
- Fix:     off means `task_state != STATE_ON`; the fake LinuxCNC now models it.

## 2026-10-21  `T0 M6 G43` keeps the previous tool's length offset
- Area:    `webui/mill/server/machine.py` (cmd_tool_load)
- Status:  Fixed
- Symptom: after unloading (T0), the DRO still subtracted the old tool's length.
- Cause:   with tool 0 in the spindle LinuxCNC leaves G43's offset as it was.
- Fix:     `T0 M6 G49` for an empty spindle.

## 2026-10-21  Probing: motion stops past the trip point
- Area:    `webui/mill/server/machine.py` (cmd_probe)
- Status:  Fixed
- Symptom: touch plate set G54 Z 25 µm low; the retract ended 25 µm short.
- Cause:   at F300 the axis stops a little after the probe trips (the
  stop ramp and a servo period); G10 L20 and the relative retract worked from
  where it stopped.
- Fix:     both from stat.probed_position: G10 L2 with the trip point, the
  retract by clearance + (trip − stop). The setter already used the trip point.

## 2026-10-21  millweb: E-STOP and Stop waited behind long commands
- Area:    `webui/mill/server/millweb.py`, `machine.py`
- Status:  Fixed
- Symptom: during a tool measurement (~30 s) a browser's E-STOP or Stop got
  no answer until it finished; after Stop the measurement went on moving.
- Cause:   every command ran in one worker thread; sequences only waited for
  the interpreter to go idle, which an abort also does.
- Fix:     E-STOP, Stop and jog stop run in their own thread on their own
  `linuxcnc.command()`; a stop counter ends any sequence (`mdi(..., since=)`
  returns "stopped"). Unit and backend tests check both.

## 2026-10-21  Console (live): multi-point moves refused after the first line
- Area:    `webui/mill/js/live.js` (moveTo)
- Status:  Fixed
- Symptom: Go to zero / Park on the machine: "a program or MDI is running".
- Cause:   each point went as its own MDI without waiting for the one before.
- Fix:     `mdi` takes `wait` (reply when done); moveTo waits per point and
  checks the travel first, as the simulation does.

## 2026-10-21  Console (live): the Dynamics page stopped the render loop
- Area:    `webui/mill/js/app.js` (renderUi, frame)
- Status:  Fixed
- Symptom: opening Dynamics on a live machine froze the DRO and the 3D view.
- Cause:   live mode has no power estimate (pTot null); `pTot.toFixed` threw
  inside requestAnimationFrame's callback, which then never asked again.
- Fix:     guard pTot; frame() catches and logs a render error and keeps going.

## 2026-10-21  Tests: loading a HAL component twice aborts LinuxCNC silently
- Area:    `webui/mill/test/backend_suite.py`, `harness.py`
- Status:  Note
- Symptom: a test profile's LinuxCNC exited at once with an empty log.
- Cause:   the extra HAL lines loaded `and2`, which mill.hal already loads;
  the linuxcnc script's own log file is deleted on exit.
- Fix:     another component (`or2`). To see why, run `linuxcnc` by hand in
  the kept directory (`--keep`).

## 2026-10-21  millweb: a program aborted by an error reports "done"
- Area:    `webui/mill/server/machine.py` (status), `millweb.py`
- Status:  Open
- Symptom: run-from-line onto an arc ("Radius to end of arc differs") showed
  DONE and the event "Program finished".
- Cause:   run is "done" when the interpreter goes idle after running in auto;
  stat has no flag for an abort by error, and errors arrive on the slower
  error channel.
- Fix:     not yet; the error itself is in the log as a fault.

## 2026-10-21  Tests: a 7 MB text over the WebSocket is over 8 MB as JSON
- Area:    `webui/mill/test/backend_suite.py`
- Status:  Note
- Symptom: a "too large" upload test got no reply.
- Cause:   newlines double in JSON; the frame passed the 8 MB limit and the
  server closed the connection (as designed).
- Fix:     the test sends 7 MB without escapes, and checks the close separately.

## 2026-10-21  millweb: a big program's preview stopped LinuxCNC
- Area:    `webui/mill/server/preview.py`, `millweb.py`
- Status:  Fixed
- Symptom: loading a 300 000-line adaptive program (arcs) ended millweb, and
  LinuxCNC with it (millweb is its display).
- Cause:   past 400 000 segments the canon's check_abort() said stop;
  `gcode.parse` then raises **KeyboardInterrupt** ("Load aborted"), which is
  not an Exception: it left the worker thread and stopped the event loop.
- Fix:     the preview runs in a child process (`preview.py` reading a JSON
  request); check_abort only enforces `PREVIEW_TIMEOUT`, and that
  KeyboardInterrupt is caught as "truncated". A crash in the interpreter now
  costs a preview, not the machine.

## 2026-10-21  millweb: big previews stalled the server for seconds
- Area:    `webui/mill/server/millweb.py`, `preview.py`, `js/live.js`, `js/app.js`
- Status:  Fixed
- Symptom: 20 000 G83 holes: 380 000 segments, a 26 MB message, pings
  unanswered for 5.2 s, 300 MB RSS; the poll loop awaited the preview, so the
  status stopped meanwhile; previews of one file ran twice (get, then load);
  the listing stopped at line 50 000.
- Cause:   the interpreter's Python canon holds the GIL; the whole path went
  out as JSON lists of 10 numbers, encoded on the event loop.
- Fix:     child process; the drawn path merges neighbouring moves past
  `PREVIEW_SEGS` (100 000) while stats (travel, lengths, time, tools) cover
  every move; 7 numbers per segment chained from the last; the message is
  assembled as text; a cache; the load's preview in the background; the
  listing in windows (`program_lines`, an index of every 1000th line).
  A million lines: under 8 MB, pings under 0.5 s (make test-stress).

## 2026-10-21  The preview child segfaults on a T word without stat()
- Area:    `webui/mill/server/preview.py` (main)
- Status:  Fixed
- Symptom: every preview in the child process died with SIGSEGV (exit -11)
  in LinuxCNC's tooldata library.
- Cause:   as in the entry on the gcode module and T words: the tool data's
  shared memory is mapped by the `linuxcnc` module's stat().poll() (creating
  the stat isn't enough: tested); the fresh child had never polled (millweb's
  own process polls 30 times a second).
- Fix:     the child creates a linuxcnc.stat() and polls it once before parsing.

## 2026-10-21  Tests: `pkill -f PATTERN` kills the shell that runs it
- Area:    test tooling
- Status:  Note
- Symptom: the command running `pkill -f stress_suite.py` exited 144 and the
  edit after it never ran.
- Cause:   the pattern is in that shell's own command line.
- Fix:     `pkill -f "[s]tress_suite"`, or kill by pid.

## 2026-10-21  LinuxCNC: feed override above MAX_FEED_OVERRIDE is ignored, silently
- Area:    `webui/mill/server/machine.py` (cmd_override), `js/live.js`, `test/sim/mill.ini`
- Status:  Fixed
- Symptom: at 200 % feed override a program of short moves ran at exactly
  its F: 3000 mm/min, 71 lines/s, the same as at 100 %; stat.feedrate read 2.0.
- Cause:   milltask clamps the override to `[DISPLAY] MAX_FEED_OVERRIDE`
  (default 1.0) but stat reports the value commanded; the console's slider
  went to 150 %.
- Fix:     millweb reads MAX_FEED_OVERRIDE / MAX_SPINDLE_OVERRIDE /
  MIN_SPINDLE_OVERRIDE, clamps to them and sends them in hello; the page sets
  its sliders' ranges from them. test/sim allows 200 %: 6000 mm/min, 135 lines/s.

## 2026-10-21  Console: a long program never showed (Math.min(...array))
- Area:    `webui/mill/js/app.js` (plan)
- Status:  Fixed
- Symptom: with a 62 000-segment path the page stayed on "—", 0 lines;
  "RangeError: Maximum call stack size exceeded".
- Cause:   the extents were `Math.min(...xs)` over every segment's ends:
  spreading ~125 000 arguments overflows the stack.
- Fix:     a loop.

## 2026-10-21  Console: recolouring the whole path every frame while running
- Area:    `webui/mill/js/app.js` (colorPath), `js/live.js` (step)
- Status:  Fixed
- Symptom: a running program with a long path made the page sluggish.
- Cause:   the "done" boundary moves every frame, and each move recoloured
  and re-uploaded every segment (after a linear search); live step searched
  the segments from the start each frame.
- Fix:     a binary search for the boundary, only the segments between the
  old and new boundary recoloured and uploaded (updateRange); step resumes
  from the last segment. ~3 ms of script a frame with 62 000 segments.
  Frames per second in the test container (software WebGL, 4 shared cores)
  stay ~5 at 1600x1000 with LinuxCNC running, 11 with nothing loaded: the
  rasteriser, not the program.

## 2026-10-21  millweb: loading the same file again kept the old preview
- Area:    `webui/mill/server/millweb.py` (op_program_load)
- Status:  Fixed
- Symptom: a program edited on disk and loaded again showed its old path.
- Cause:   previews were sent only when LinuxCNC's file name changed.
- Fix:     every load sends its preview (the cache makes an unchanged file cheap).

## 2026-10-21  LinuxCNC: status without the work offset after a fast start
- Area:    `webui/mill/server/machine.py` (resync_offsets), `millweb.py`, `js/live.js`
- Status:  Workaround
- Symptom: `make test-live` failed "the DRO reaches X10 Y20 Z5" every time,
  run directly it passed: the DRO showed machine coordinates (X450 for G54 X10).
- Cause:   when commands come within a second or so of LinuxCNC starting
  (the scenario starts the moment millweb's port opens), stat can stay at
  g5x_index 0 (not a valid system) with offsets 0, though the interpreter
  uses G54 = 440 (the move lands right). A fresh linuxcnc.stat() in another
  process reads the same: every display would be wrong. A move or "G54"
  doesn't republish it; selecting another system does. 5 of 6 fast starts
  with node's bin first in PATH (it shifts the start-up timing), 0 of 6 with
  1 s to settle.
- Fix:     millweb sees g5x_index 0 while on, and once homed and idle (and
  before any MDI) selects another system and then the startup one (param
  5220), logging it; `st.wcs_ok` false meanwhile, and the page's DRO says
  "G5x?". Repaired within 1.5 s of homing in every case tried. Root cause in
  LinuxCNC's task start-up not chased further.

## 2026-10-22  webui/mill: test/sim's program was never committed (*.ngc ignored)
- Area:    `.gitignore` (`*.ngc`), `webui/mill/.gitignore`, `webui/mill/test/sim/nc_files/`
- Status:  Fixed
- Symptom: in a fresh clone of claude/mill-webui, three suites failed: no
  `bracket-op1.ngc` in test/sim/nc_files (the directory wasn't in the branch).
- Cause:   the root .gitignore ignores `*.ngc`; `git add -A` skipped the file
  silently, and every run here used the working tree's copy.
- Fix:     `!test/sim/nc_files/*.ngc` in webui/mill/.gitignore; the program
  committed. Check new test data with `git status --ignored`.

## 2026-10-22  Console (simulation): a slow frame stepped over M1
- Area:    `webui/mill/js/app.js` (tick)
- Status:  Fixed
- Symptom: "M1 optional stop holds" failed 2 runs in 4 once the 3D view
  drew more; the simulated program ran through its M1.
- Cause:   M3, M5, M8, M9 and M1 were applied only for the segment a frame
  landed on (tool changes for every one passed). A frame step longer than
  an M-code's 50 ms segment jumped over it: frame-rate dependent, so a slow
  tablet would skip an M1 too.
- Fix:     every segment passed in the frame is applied; an M1 stops the
  program at its line whatever the step. Tests that jump the program clock
  past M1 now switch it off first.

## 2026-10-22  Suites on the target board: installed LinuxCNC, RAM, gcode present
- Area:    `webui/mill/test/harness.py`, `stress_suite.py`, `server/tests/test_millweb.py`
- Status:  Fixed
- Symptom: (board report) the harness always sourced $LINUXCNC/scripts/rip-
  environment and the stress suite looked for $LINUXCNC/nc_files/3D_Chips.ngc,
  neither of which an installed LinuxCNC has; a unit test needed the gcode
  module absent; the stress suite read the million-line file in one go.
- Fix:     run-in-place tree only when it has rip-environment, else the
  installed linuxcnc on PATH; 3D_Chips.ngc from the tree, the package's
  ncfiles or ~/linuxcnc/nc_files, else that case is skipped; the unit test
  accepts both (with gcode it checks the parsed program, which then needed
  its M2); the listing check reads lines with islice.

## 2026-10-22  File page: a long folder ran under the next panel
- Area:    `webui/mill/index.html` (.files, .page-pane > .panel)
- Status:  Fixed
- Symptom: with ~9 programs, clicks on the lower rows went to the Preflight
  panel's heading (Playwright: "<h2> ... intercepts pointer events").
- Cause:   page panels are flex items that shrink; the list overflowed its
  panel and lay under the next one.
- Fix:     page panels don't shrink (the page scrolls); the list scrolls inside
  itself past ~half the screen. The live GUI suite checks a folder of 14.

## 2026-10-22  Remote test runs: a board keeps its state between runs
- Area:    `webui/mill/test/gui/live_suite.js`, `wsclient.js`
- Status:  Note
- Symptom: a second remote run of the live GUI suite failed: an upload asked
  "Replace?", and the DRO was off by a tool length.
- Cause:   the harness starts every local run afresh; a served board keeps its
  files and LinuxCNC its loaded tool.
- Fix:     cases delete their own leftovers first; the baseline unloads the
  tool. Also found: an upload's file-list broadcast can arrive before its
  reply, so the page now selects the upload itself.

## 2026-10-22  The first page on a Pi took 16 s; the live suite's first case failed
- Area:    `webui/mill/index.html` (loading screen), `js/live.js` (boot), `test/gui/runner.js`, `live_suite.js`
- Status:  Fixed
- Symptom: (board report) "connects" failed every time: its first page
  needed 16.4 s to show LIVE with the machine on, against a 15 s limit; a
  second page took 3.5 s. millweb answered a connection in 10-20 ms and the
  page's 750 KB came in 0.6 s: the time was Chromium's first start on the Pi.
- Fix:     the suites warm the browser with one page before the first case
  (its time printed), and give a page 30 s. The console shows a loading
  screen until its first frame (what it is doing; which file failed if a
  script didn't load). The page also waited only 2.5 s for a machine before
  falling back to the simulation, which a cold browser could miss: 6 s now,
  and if the machine answers after the fallback the page reloads once into it.
- Found:   the loading screen's first version caught resource errors too: offline
  (no Google Fonts) it marked itself failed and stayed over a console that had
  started. Only scripts count now, and the first drawn frame always removes it.
- Also:    "flood on" in the live suite failed now and then: it read the
  page's button the moment the suite's own connection saw flood on; the page
  hears it over its own connection a moment later. It waits for the page now.

## 2026-10-22  Debug page scope: a killed sampler hung all of HAL
- Area:    webui/mill (millweb scope, server/sampler.py)
- Status:  Fixed
- Symptom: after a few scope start/stops, millweb stopped answering (page
  loads timed out), `halcmd` hung even under `timeout`, and LinuxCNC didn't
  shut down. Needed kill -9 of rtapi_app/linuxcncsvr and `ipcrm` of the
  lcnc shared memory.
- Cause:   millweb stopped the sampler with SIGKILL. `hal.get_value` holds
  HAL's mutex in shared memory; a process killed inside it never releases
  it, and every HAL user then spins on it. The killed process's HAL
  component also stays registered.
- Fix:     the sampler takes SIGTERM as a flag (the get in progress finishes),
  leaves its loop and calls `comp.exit()`; millweb sends SIGTERM and waits
  3 s, killing only as a last resort (logged). The backend suite starts and
  stops an 8-channel 2 kHz scope 30 times, then checks HAL answers and no
  `millweb-scope-*` component is left. The same applies to any HAL user:
  never SIGKILL one that may be inside a HAL call (millweb's own `halcmd`
  calls are killed only after a 10 s timeout).
- Also:    the sampler's frame header had a "t" (time) that the relay's
  {"t": "scope"} overwrote; it is "t0" now.

## 2026-10-22  Drives tab: a drive's fault is gone before its card asks
- Area:    webui/mill (server/drives.py)
- Status:  Note
- Symptom: a position-error fault on the simulated drive never showed as
  SOFT FAULT on the card: it read DISABLED, with last_fault set.
- Cause:   the drive faults, LinuxCNC sees amp-fault and drops enable at
  once, and the drive goes SOFT_FAULT -> DISABLED within a servo period or
  two. millweb asks the card's values one at a time over the ~1 kB/s
  console (a full card every ~2 s), so it rarely catches the fault state.
- Fix:     the drive prints "ERROR: Fault n: text" on the fault. millweb
  reads that line, logs it, and keeps it as the card's "last fault" with
  its time. The polled fault0.fault/state still show a fault that stays.
- Also:    millweb holds the drive's pty for good (a pty nobody reads drops
  the drive's output), so servoterm-cli can't share it; the card's Detach
  lets go of it for a while. A pin read is a prefix match on the drive
  ("fault0.en" prints five pins): millweb asks full names and parses every
  "name [<= source] = value" line it sees, whoever asked.

## 2026-10-22  Tablets: double-tap zoom, text selection, squeezed panels
- Area:    webui/mill/index.html (CSS), js/app.js, test/gui/tablet_suite.js
- Status:  Fixed (iPad Safari itself not tested here: no WebKit)
- Symptom: (iPad report) a quick second tap zoomed the page; taps on some
  buttons selected text. The tablet survey also found: held sideways
  (1080x746) the Program and MDI panels squeezed to 72 px each, Preflight
  under MDI's Send; the scope's mode buttons squeezed to 2 px high; the
  header on two rows with E-STOP alone on the second; upright, the header
  (and its E-STOP) scrolled out of sight; 26-64 controls per page under
  30 px.
- Cause:   no touch-action (Safari double-tap zoom applies to buttons too);
  text selectable everywhere; grid/flex items with min-height 0 inside a
  fixed-height page shrink instead of letting the page scroll.
- Fix:     `* { touch-action: manipulation }`, `user-select: none` on the
  body (text kept in G-code, inputs, logs, the drive console), no
  contextmenu on the jog pad; `.lower` min-height auto and the Debug tabs
  `flex: none` in one column; a one-row header at 961-1440 px; a floating
  E-STOP when the header's is out of sight; larger targets under
  `(pointer: coarse)`. tablet_suite.js checks it at 12 sizes; the same
  suite run on the previous version fails on every page.
- Note:    Chromium's iPad emulation doesn't do Safari's double-tap zoom,
  so the suite checks the CSS that prevents it, not the zoom itself.

## 2026-10-23  millweb-supervisor: LinuxCNC's output, restarts, a refused config
- Area:    webui/mill/server/supervisor.py
- Status:  Note
- Symptom: logs/linuxcnc.log held only the supervisor's own lines; neither
  LinuxCNC's nor millweb's output.
- Cause:   the `linuxcnc` script sends its output to /tmp/linuxcnc.print.*
  and .debug.* when stdout isn't a terminal, and deletes them at exit
  (on an error it copies them to ~/linuxcnc_*.txt).
- Fix:     the supervisor runs linuxcnc on a pty and copies what comes out
  into the log (DISPLAY unset: no X error window either).
- Also:    a restart is millweb quitting with run/request = restart (LinuxCNC
  follows its display); each start gets a fresh millweb, so nothing
  reattaches to a new HAL or NML. Before quitting millweb stops its scope
  samplers (else their HAL components hold up LinuxCNC's exit). A config
  that never reached "running" isn't retried (its error doesn't go away);
  the error shown is the first HAL file line from the log
  ("custom.hal:1: Pin 'nosuch.pin' does not exist"), not the script's
  generic "terminated with an error". A quick restart (~2 s) can bring the
  page back to the new millweb without passing the supervisor: the
  supervisor's state is in millweb's hello and pushed on change.

## 2026-10-23  Drives with older firmware faulted; HV and LV firmware could differ unseen
- Area:    shared/version_number.h, src/comps/hv.c, host/hw/hv.c, shared/commands.c, tools/stmblconf.py, webui/mill (driveconf.py)
- Status:  Fixed (the mechanism; not yet run on hardware)
- Symptom: (bench report) fatal faults on drives that didn't have the latest
  firmware; nothing said which firmware a drive ran, and `about` printed the
  HV image the F4 carries, not what the HV board runs.
- Cause:   one product version (0.9.18) for every build; no check between
  firmware, HV board, config and console.
- Fix:     STMBL_FW_VERSION (1), in both images; the HV board reports its own
  in the state rotation (an older one: a rotation without the word = 0) and
  the drive faults HV_VERSION_ERROR on a mismatch (hv0.fw_check = 0 to
  allow); `version` command; configs carry #stmbl_conf_version; invertible
  conf/patches with tools/stmblconf.py; the console shows versions, refuses
  machine on with another firmware, and backs up / patches / restores
  configs over the drive's console (docs/firmware_versions.md).
- Note:    version_info_t's product_name sits at byte 152 of the struct
  (0x188 + 152 in an image), fw_magic at 196, fw_version at 200.

## 2026-10-23  brdstepper KiCad project: what KiCad 7 headless can't do, and the board's room
- Area:    hardware/brdstepper/gen (gen.py, check.py, pcb.py, drc.py)
- Status:  Workaround
- Symptom: `kicad-cli sch erc` / `pcb drc` don't exist (KiCad 7.0.11, the
  apt version here; the KiCad 8 PPA is blocked). `ZONE_FILLER.Fill`
  segfaults in a headless pcbnew; `FOOTPRINT.Flip` segfaults on a footprint
  not yet on a board; `SaveBoard` rewrites the .kicad_pro and drops its net
  classes and sheet list.
- Cause:   ERC/DRC came to kicad-cli in KiCad 8; the others are pcbnew 7's
  Python bindings.
- Fix:     check.py checks KiCad's own netlist against parts.py plus ERC's
  pin-type rules; drc.py runs DRC through pcbnew.WriteDRCReport; the GND
  plane is left unfilled; footprints are added to the board before they are
  flipped; pcb.py writes its net classes back after saving.
- Note:    the *_ThermalVias QFN footprints have a copper pad on the far
  side as well: a QFN on the bottom puts copper on the top. And 28 x 28 mm
  with both variants' connectors (USB-C, 3 GH, XH 3) and the M2.5 holes'
  6 mm courtyards is full: J3 is pads and J7 the 1.27 mm 5-pin (both
  options the document allows), U4 turned 90° so J2's pins pass beside it.

## 2026-10-25  vibration sensor: a strong tone's skirt read as chatter
- Area:    shared/vibdsp.c (vib_peak), shared/comps/vib.c (chat_f, chat_a)
- Status:  Fixed
- Symptom: (simulator) milling without chatter still reported chat_a
  0.29 g at 436 Hz: next to the 400 Hz tooth passing.
- Cause:   the search skipped bins within 3 of a spindle harmonic, but then
  summed +-3 bins around the bin it found, which reached back into the
  excluded tone's Hann main lobe.
- Fix:     exclude +-4 bins, and leave excluded bins out of the sum. Chatter
  within 4 bins of a spindle harmonic (52 Hz at fft_n 2048, 26 Hz at
  4096) is not separated from it.

## 2026-10-25  The console prints every pin a name starts with
- Area:    the console (hal_parse), host tests (@expect PIN[k])
- Status:  Note
- Symptom: `vib0.crash` printed crash_g, crash_reset, crash and crash_peak,
  so a test's crash_peak[1] was the value printed with the crash query.
- Cause:   pin queries match by prefix.
- Fix:     name pins so that an alarm's name is no prefix of others
  (vib0.crash, shock, shock_lim, reset).

## 2026-10-25  vibsensor: 4096-point FFTs and the HAL's pools didn't fit 128 KB
- Area:    vibsensor/ (Makefile HAL_SIZES), shared/comps/vib.c, shared/vibbuf.h, shared/comps/vnode.c
- Status:  Fixed
- Symptom: the first link overflowed RAM by 58 KB.
- Cause:   the FFT input double buffer (32 KB) plus a copy for the FFT
  (16 KB) and im (16 KB), a 4096-sample vibbuf (24 KB), a spectrum copy
  and frame array for vnspec (13 KB), and the HAL's default pools
  (1024 pins, 16 KB of contexts: 26 KB).
- Fix:     the FFT works in the collected block; vibbuf 1024 samples
  (38 ms; rt drains it every 0.2 ms); vnspec builds frames one at a time;
  the sensor's HAL pools -DHAL_MAX_PINS=320 -DHAL_MAX_COMPS=8
  -DHAL_MAX_CTX=4096; config text 6 KB, Tx ring 64. Now 108 KB.
- Note:    a component name must not contain a digit (the console reads
  `tmp1170.temp` as comp tmp, instance 1170): the TMP117's component is
  `thermo`.

## 2026-10-26  Vibration levels flipped at a threshold, and tripped on the filters' start
- Area:    shared/comps/vib.c (level, temp_level), host/tests/vib/levels.test
- Status:  Fixed
- Symptom: (vibmon --sim) a sensor at 4.4 mm/s, next to vel_warn 4.5, sent
  a level event most reports, each a line in millweb's event log. With
  hysteresis added, a steady 11.04 mm/s then stayed at alarm (11.2): the
  first report after start had passed it.
- Cause:   no hysteresis; and the high pass and velocity filters ring for a
  few hundred ms after start (gravity's 1 g step on z).
- Fix:     a level falls back only 10 % below its threshold (temperature
  2 degC); levels are judged from 0.5 s after the filters were (re)made.

## 2026-10-26  rlpsij: a light rotor with a slow l/r gets j and friction wrong
- Area:    shared/comps/rlpsij.c (state 4), host/tests/motor_sew_cmp40m.test
- Status:  Open
- Symptom: the SEW CMP40M (r 9.96 ohm, l 22.75 mH, j 1.8e-5 kg m^2,
  psi 0.1455, 3 pole pairs) identifies r, l, psi and the pole count right,
  but j 24 % high and friction at half. With 3x the inertia every value is
  within tolerance.
- Cause:   (likely) at test_cur the rotor reaches max_vel in under 1 ms,
  less than the electrical time constant l / r = 2.3 ms: the drive
  segments are mostly current rise, and the momentum fit trades j against
  friction.
- Fix:     not yet; the test carries @todo. Closed-loop soak of the motor
  passes (gen_soak_tests.py --only sew_cmp40m). On the machine the gear
  unit and the load add inertia.

## 2026-10-26  kigen: GND in a wide-clearance net class fails fine-pitch parts
- Area:    hardware/kigen (sch.py net classes), hardware/vibsensor/gen/parts.py
- Status:  Note
- Symptom: DRC on the vibration sensor's board: four clearance errors
  between the IIS3DWB's own pads (0.5 mm pitch, 0.15 mm apart).
- Cause:   GND was in the "Power" net class (0.2 mm clearance), and LGA /
  QFN pads of the same part sit closer than that.
- Fix:     a board's NETCLASS_POWER (gen/parts.py) leaves GND out where
  fine-pitch parts have ground pads (vibsensor); brdstepper keeps its
  classes (no violation there).


## 2026-10-26  kigen: connector rows at 45 degrees collide in a chamfered tail
- Area:    hardware/vibsensor/gen/board.py (two M8 sockets IN / OUT)
- Status:  Note
- Symptom: J1 / J3 rotated 45 degrees to face the tail's chamfers gave 26
  courtyard violations; placed near the corner, "overlaps where fixed".
- Cause:   kigen's courtyards are axis-aligned boxes, so a 45-degree row
  grows its box into the M2 hole and the other row; its corner crossed
  the tail chamfer.
- Fix:     the lead pads as rows along the long edges at (-9.4, +-7.35),
  rotation 90; the socket's leads bend to them inside the potting.

## 2026-10-27  SEW CMP40M resolver cable: pairs found by resistance
- Area:    conf/sew_cmp40m.txt (RH1M resolver on res_fb0)
- Status:  Note
- Symptom: SEW's green resolver cable cut off its D-sub; which pair is
  excitation, sin and cos.
- Cause:   colours only; SEW's tables disagree on pins.
- Fix:     pairs by D-sub pins (3/8, 1/6, 2/7) and resistance: the odd one
  out is the excitation (grey/pink 77.4 ohm), sin and cos match
  (green/yellow 67.5, blue/red 67.3). Wiring to the RJ45 in the config's
  comments. The KTY84 is on brown/white (576 ohm at ~20 C), not
  violet/black: that pair reads 1 ohm, a bridge at the motor end.

## 2026-10-27  servoterm-cli ignored SIGTERM in interactive mode
- Area:    servoterm-cli/servoterm-cli.c (the interactive select loop)
- Status:  Fixed
- Symptom: `kill` on an interactive servoterm-cli did nothing; a test
  driving it through a pty hung until its timeout.
- Cause:   the loop retried select() on EINTR without looking at stop_req,
  which the signal handler sets.
- Fix:     the loop runs while !stop_req. Found by
  host/tests/test_servoterm_ident.py (drive name in the prompt).

## 2026-10-27  millweb: a moving-table machine in the 3D view
- Area:    webui/mill/js/app.js (buildBuiltin, workG), machmodel.js
- Status:  Note
- Symptom: the 3D view drew everything in the work's frame with a gantry
  moving over it; a fixed-bridge machine (CN 720) would show its base and
  bridge sliding in Y.
- Cause:   one frame for both layouts.
- Fix:     machine.json `type` (gantry | bridge). For a bridge the view keeps
  the base and bridge still (the tool at y 0) and moves workG (fixtures,
  toolpath, trail, travel box) and the built-in table with -Y. Anything that
  turns a view position back into machine Y must add workG.position.y (the
  follow camera does).

## 2026-10-28  millweb 3D: see-through objects flicker where two surfaces meet
- Area:    webui/mill/js/setup3d.js (applyLooks, stableOrder), machmodel.js
- Status:  Fixed
- Symptom: in see-through mode (fixtures / workpiece "glass") the view
  flickered where two surfaces lie at the same level (the stock on the
  vise, the vise's jaws on its base), changing as the view turned.
- Cause:   see-through materials don't write depth, so three.js sorts them
  by their objects' distance every frame; two at the same level swap places
  as the camera moves, and the blend changes with the order.
- Fix:     every see-through mesh gets its own fixed renderOrder (per object,
  per mesh inside it), and a polygon offset pushing it back (never forward,
  so the toolpath on the stock top stays in front). Machine model parts
  with opacity < 1 get a fixed order behind the setup's.

## 2026-10-28  millweb 3D: "orange sparks" at some view angles only
- Area:    webui/mill/js/app.js (scene setup: trail, chips)
- Status:  Fixed
- Symptom: the tool threw orange sparks at some view angles, none at others.
- Cause:   the chip particles (and the tool trail) are buffers rewritten
  every frame, but their bounding spheres were computed once (the chips' at
  Z -5000, all parked there). three.js culled them by that stale sphere: they
  showed only when it happened to be in view.
- Fix:     frustumCulled = false on both; the chips are aluminium-grey now.
  Any geometry whose positions change per frame needs this (or
  computeBoundingSphere after each change).

## 2026-10-28  LinuxCNC 2.9: an NGC-only M6 remap for a rack changer
- Area:    webui/mill/server/machineconf.py (toolchange_ngc, atc)
- Status:  Note
- Symptom: -
- Cause:   -
- Fix:     `REMAP=M6 modalgroup=6 ngc=toolchange` without a Python prolog
  works: the sub reads #<_current_tool> / #<_selected_tool>, ends with
  `M61 Q#<_selected_tool>`, and M70 / M72 keep the modal state (G43 stays as
  it was). M66 L3 sets #5399 to -1 on a timeout (abort then). M0 inside the
  remap pauses for a manual change (resume continues). motion's digital I/O
  needs `num_dio` on motmod. Checked in LinuxCNC 2.9 (sim joints, 4 pockets,
  slide-in): T2, T3 swapped through the rack, T7 by hand.

## 2026-10-28  backend_suite: "X moves through the drive" fails now and then
- Area:    webui/mill/test/backend_suite.py (supervisor: an stmbl drive on X)
- Status:  Open
- Symptom: once in a full `make test-backend` run: joint.0.motor-pos-fb
  599.990417 vs stmbl-sim.0.pos_fb 599.990173; the case alone passes.
- Cause:   it reads both pins in one hal_values call and wants them equal,
  but the drive may still be settling and the two are read a servo cycle
  apart.
- Fix:     idea: wait for inpos, or compare within 1 µm.

## 2026-10-28  OpenSCAD: "may not be a valid 2-manifold" from a tangent counterbore
- Area:    hardware/teststand/teststand.scad (the motor brackets)
- Status:  Fixed
- Symptom: the X and Z motor brackets exported with "Object may not be a
  valid 2-manifold"; removing the motor's cuts didn't help.
- Cause:   a bolt's counterbore (Ø16) exactly tangent to another face (a
  gusset's side, the plate's bottom edge): CGAL leaves an edge shared by
  two solids.
- Fix:     moved the bolts so nothing is tangent. Also: OpenSCAD 2021's
  ASCII STL of printed threads is huge (64 MB for the set); `--export-format
  binstl` and 12 slices a pitch make it 11 MB.
