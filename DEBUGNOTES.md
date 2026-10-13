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
- Status:  Open
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
- Area:    `shared/comps/idm.c`
- Status:  Note
- Symptom: In the host sim (no load), idm gives d = 0.00106 / f = 0.072
  against a true 0.002 / 0.05.
- Cause:   Most of the run (state 1.3, 90 s) is at constant max_vel, where
  `d*v + f*sign(v)` is a single number. The sum is right
  (0.072 + 25*0.00106 = 0.098 against a true 0.100), but how it is split
  between d and f is not well determined. Only the 20 s ramp-up (state 1.2)
  runs at several speeds. The offset also adapts slowly (li = 0.01), and
  reaches only about 89% within the run.
- Fix:     Idea: spend more of the run at several speeds, or fit d/f by
  least squares from the `*_sum` values idm already collects. Also consider
  a smaller `li` or a longer run.

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
