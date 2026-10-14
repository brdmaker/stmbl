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
  voltage. In the same window three boots ended in HAL state RT_TOO_LONG
  within seconds (rt max 134 us of 200; the USB IRQ, priority 15, can't
  delay the rt ISR, priority 2).
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
- Status:  Open (not reproduced)
- Symptom: 30 s into id_mot (max_acc 250, state 1.3) the drive dropped off
  USB and re-enumerated (an F4 restart), then enumerated but answered
  nothing on the console until the user power-cycled it. LEDs normal after.
  The same run then passed 3 times (2 with a scope log, 1 polled), and
  id_mot / id_pmsm / LinuxCNC moves all ran without it again.
- Cause:   Unknown. A HardFault would fit the hang: hal_error() returns from
  the handler, so the faulting instruction re-runs; but that doesn't
  explain the restart before it. A logic-supply dip would explain both.
  The first drive's DC-sense fault on the same bench makes the setup
  (supply, grounding, USB, encoder cable) a suspect too.
- Fix:     If it recurs: note the time, read `hal` (state and error_info) before
  power-cycling, and log the logic supply.

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
- Status:  Note
- Symptom: First hardware soak run (LinuxCNC moves up to 500 mm/s, console
  checks between cycles): `hal` rt max 168.5 us of the 200 us period (84%).
  Idle and during id_mot it was 104..134 us. The first drive's RT_TOO_LONG
  stops are recorded above.
- Cause:   Not known which rt function peaks; `hal` lists per-function max.
- Fix:     The soak suite logs rt max every cycle and notes it above 90% of
  the period (`rt_load_high`); run it for hours and compare per-function
  maxima from `console.log`.

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
