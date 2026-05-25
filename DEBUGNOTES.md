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

## 2026-05-25  HAL pin connection syntax: = not <=
- Area:    `shared/hal.c` (`hal_parse_`), `host/tests/`
- Status:  Note
- Symptom: Pin connections in HAL scripts must use `sink = source` syntax.
  The `<=` notation (e.g. `sim1.amp <= sim0.amp`) is display-only output
  format from `hal_print_pin`; passing it as a command is silently ignored.
- Cause:   The sscanf pattern in case 3 of `hal_parse_` uses ` = ` as the
  separator; `<` is not consumed so the parse falls through to pin query.
- Fix:     Documented here and corrected in all test files and scripts.

## 2026-05-25  hal_print_pin bypasses debug_level
- Area:    `shared/hal.c` (`hal_print_pin`, `hal_parse_` case 3)
- Status:  Known
- Symptom: Pin query output (case 3 search-comps path) prints via
  `hal_print_pin` which calls printf directly, not gated by `debug_level`.
  Appears in unit test output even with `hal_set_debug_level(2)`.
- Cause:   `hal_print_pin` was written before the debug_level system.
- Fix:     Cosmetic only; does not affect test results. Could add a
  `if(hal.debug_level < 1)` guard to `hal_print_pin` if output noise
  becomes a problem.

## 2026-05-25  sserial_host frt race: pd_cmd consumed before pin writes
- Area:    `host/sserial_host.c`, `host/tests/test_sserial.c`
- Status:  Fixed
- Symptom: test_sserial tests 17/18/20 (process_data pos_fb/vel_fb checks)
  failed ~10% of runs with wrong pin values (0.0 or stale value).
- Cause:   stmbl_host is single-threaded: it's either in `run_cycles` (frt
  loop) or reading from stdin (fgets loop). HAL pin assignments from the
  pipe are processed only in the fgets loop, not during run_cycles. If the
  parent wrote pd_cmd to the socket before the previous step's run_cycles
  completed, frt_func consumed pd_cmd with the old pin values (before
  pos_fb=1.5 / pos_fb=9.875 were seen). Race window: ~3-11 residual frt
  calls at ~1µs each; parent write latency ~5-6µs → ~10% failure rate.
- Fix:     Added `HAL_PIN(pd_arm)` to sserial_host. frt_func checks
  `PIN(pd_arm) > 0.5` before processing ProcessDataRPC and immediately
  resets it to 0 after. The test sets pd_arm=1.0 via sim_cmd before each
  lbp_process_data call; this goes through the pipe so it is processed in
  the fgets loop, meaning pd_arm can only be 1 during the intended step.

## 2026-05-25  Virtual motor drive via sserial
- Area:    `host/tests/test_sserial_motor.c`, `shared/comps/veltopos.c`
- Status:  Note
- Symptom: (design note) sserial_host wired to veltopos integrator; vel_cmd from
  LBP master drives veltopos, pos_fb fed back over sserial.
- Cause:   N/A — this is an integration demonstration.
- Fix:     HAL wiring: `veltopos0.vel = sserial_host0.vel_cmd` and
  `sserial_host0.pos_fb = veltopos0.pos`. Timing: sserial reads pos_fb
  during FRT call 1 of each step (before that step's RT cycles), so
  pos_fb lags by one step (3 RT cycles = 0.6 ms). With vel=10 rad/s and
  polecount=1 each step adds 0.006 rad; stopping (vel=0) holds position
  confirmed by comparing consecutive exchange responses.

## 2026-05-25  HAL component names must not contain digits
- Area:    `shared/hal.c` (`comp_inst_by_name`, `pin_inst_by_name`)
- Status:  Note
- Symptom: Loading a component whose name contains an embedded digit (e.g.
  `ls_f3_host`) succeeds and shows up in `hal`, but every pin query/set for
  that component returns "not found".
- Cause:   The HAL parser uses `%[^0-9]` to extract the component name from a
  token like `ls_f3_host0.r`; it stops at the first digit, reading `"ls_f"`
  and treating `"3"` as the instance number — finding neither.
- Fix:     Renamed `ls_f3_host` → `ls_host` (no embedded digit). The rule:
  digits in HAL component base names are forbidden; only the trailing
  instance number (appended automatically) may be a digit.

## 2026-05-25  Two-process dual-CPU simulation: SIGPIPE and stdout race
- Area:    `host/hv_lv_host.c`, `host/ls_host.c`, `host/main.c`,
           `host/tests/test_hv_link.c`
- Status:  Fixed
- Symptom: (1) F4 process produced 0 bytes of stdout output in the test.
  (2) Even after fixing (1), test_hv_link was flaky: 9/9 some runs, 5/9 others.
- Cause:   (1) If F3 (the faster process) exits first, F4 receives SIGPIPE on
  its next `write()` to the socket and dies immediately without flushing
  buffered stdio output.
  (2) Both processes' commands were pre-loaded into their stdin pipes, so the
  OS could schedule F4's Phase 3 (read replies) before F3's Phase 2 (send
  replies), leaving id_fb at 0 instead of 2.5.
- Fix:     (1) Added `signal(SIGPIPE, SIG_IGN)` + `setvbuf(stdout,NULL,_IONBF,0)`
  to `main()` in main.c. SIGPIPE now returns EPIPE from write() (already
  ignored), and unbuffered stdout makes output available immediately.
  (2) test_hv_link.c redesigned with stdout-marker synchronisation: each phase
  sends a "time" command and uses `poll()`+`read()` to block until "sim time:"
  appears in that process's stdout — guaranteeing strict phase ordering.

## 2026-05-25  WRITE_CONF bootstrap: ls_host zeros out pre-set config pins
- Area:    `host/ls_host.c`
- Status:  Fixed
- Symptom: `test_motor_hv` — `motsim0.iq`, `id`, `vel` all NaN. Test sets
  `ls_host0.l = 0.001` before `start`, but `motsim` sees `mot_l = 0 → ÷0 → NaN`.
- Cause:   `ls_host.nrt_init` zeros `ctx->config`, and `rt_func` propagates all
  9 config slots (`PIN(l) = ctx->config.pins.l`) on every received packet.
  The first packet carries conf_addr=0 (r), so `ctx->config.pins.l` is still 0.
  That overwrites the user-set `ls_host0.l = 0.001` one RT cycle in.
- Fix:     Added `rt_start` to `ls_host` that copies all 9 HAL pin values into
  `ctx->config` at start time. The first packet's propagation then writes the
  user-set value (l=0.001) rather than zero. Subsequent WRITE_CONF slots from
  F4 update the values as they arrive.

## 2026-05-25  get_pin finds stale value from HAL wiring echo lines
- Area:    `host/tests/test_motor_hv.c`
- Status:  Fixed
- Symptom: Test 6 (`motsim0.vel > 0`) always failed even though torque was
  flowing and velocity should have been ≈0.03 rad/s.
- Cause:   HAL link commands (`motsim0.com_vel = motsim0.vel`) echo an "OK"
  confirmation line: `"OK motsim0.com_vel <= motsim0.vel = 0.000000"`. This
  string contains the pattern `"motsim0.vel = "` at the old (zero) value. Since
  it appears in the accumulated buffer before the final explicit pin query,
  `strstr` returned the stale match.
- Fix:     `get_pin` in test_motor_hv.c now finds the LAST occurrence of the
  pattern (loops over all `strstr` hits, keeps the final one).

## 2026-05-25  Scope channels are 8-bit (lossy) by design
- Area:    `shared/comps/term.c`, `host/servoterm.c`
- Status:  Note
- Symptom: Scope CSV values are quantized.
- Cause:   The wire protocol encodes each channel as one byte
  (`(value+offset)*gain+128`, clamped 1..254).
- Fix:     For exact numeric assertions, query pins directly (e.g. `motsim0.vel`
  returns exact text). Use the scope CSV for waveform-shape checks.
