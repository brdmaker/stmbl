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
- Status:  Workaround (does not happen with Debian 12's binutils 2.40, see 2026-10-14)
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
- Status:  Open
- Symptom: There is no closed-loop motor test asset (command velocity, watch it
  track).
- Cause:   `motsim` needs a commutation angle (`com_pos` = electrical rotor
  angle) for aligned FOC; with `com_pos` fixed the open-loop current command
  does not spin the rotor cleanly. Wiring a commutation/encoder source is TODO.
- Fix:     TODO — add a commutation source (or drive `com_pos` from rotor angle)
  and ship a `examples/motor.servoterm` closed-loop asset.

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
- Status:  Known
- Symptom: After sending a command line of 64+ bytes (including `\n`), the
  drive ignores that line and every line after it.
- Cause:   `rb_getline` only returns a line that fits in `len`. Otherwise it
  undoes the read (`rb_undo`) and returns 0, so the too-long line stays at the
  head of the ring buffer for good. `call_cmd()` also caps the command name
  and argument at 64 chars each, and its argument stops at the first
  non-printable char (`%[ -~]`), so a tab cuts an `appendconf` line short.
- Fix:     Not fixed in firmware. `servoterm-cli --load` rejects config lines
  over 52 chars (63 - len("appendconf ")) and turns tabs into spaces. A
  firmware fix would discard the line when the buffer is full and no `\n`
  has come.

## 2026-10-14  Drive loses bytes when a command arrives split across USB packets
- Area:    `shared/ringbuf.c` (`rb_putc` vs `rb_getc`/`rb_undo`), `src/usb_cdc.c`
- Status:  Workaround
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
- Status:  Fixed (host sim; not yet run on hardware)
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
  Remaining: j reads 5-13% high (possibly current-loop lag between commanded
  and actual torque; not checked). Not tested with sensor noise.
  Hardware, 2026-10-14 (firmware v0.9.18-224-g89e4d95d, bench motor,
  id_mot, about 2 min, no faults): fit j = 0.000096, d = 0 (clamped, the raw
  fit was negative), f = 0.0946, o = 0.0265. Adaptive estimates from the
  same run: j 0.000066, d 0.00047, f 0.0884, o 0.0250. The two methods agree
  roughly. The earlier conf (old idm) had j 0.000207, o 0.064. Negative d
  probably means friction drops with speed (Stribeck effect), which
  f + d*v cannot model. Unverified: no independent measurement of j.

## 2026-10-14  autotune smaller issues (idpmsm, ids)
- Area:    `shared/comps/idpmsm.c`, `shared/comps/ids.c`
- Status:  Note
- Symptom/Cause:
  - idpmsm state 1.3 (L): resets `tmp0/tmp1` but not `tmp2/tmp3`. So on a
    second run in the same boot, the "low" averages start from the previous
    run's values.
  - idpmsm state 2.3 (mot_fb_offset): low-pass filters `-pos_fb` without
    handling angle wrap. If the locked rotor sits near ±pi, the average mixes
    +pi and -pi and the offset comes out wrong.
  - idpmsm state 2.3 writes `PIN(en) = 0`, but `en` is linked to
    `fault0.en_out` and gets overwritten next cycle, so it has no effect.
  - ids state 0 resets `step = 0.1`, `rep = 1` every time, so nrt_init's
    0.25 / 2 never apply. `rep` is not used anywhere.
  - idm: `inertia_sum/damping_sum/friction_sum/offset_sum` are computed but
    never used or printed. Only the adaptive estimates are reported.
- Fix:     None yet. Each is small. Check in the host sim before changing.

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
- Status:  Fixed (sim regression unchanged; hardware rerun pending)
- Symptom: On the bench motor, id_pid finished in ~25 s with
  `vel_bw = 70.000000`, exactly 0.7 (kd) times its starting value of 100.
- Cause:   State 0 set `cost = min_cost` instead of 0. The first cycle's cost
  is then min_cost_init plus the real cost, and min_cost stays
  min_cost_init. Whenever the real cost exceeds 0.2 * min_cost_init, the
  check `cost > min_cost * kt` fires on the first evaluation, so vel_bw is
  multiplied by kd and the search moves on without tuning it. In the host
  sim the first cycle is cheap, so it does not trigger (vel_bw 124 before
  and after the fix).
- Fix:     Start cost at 0.

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
