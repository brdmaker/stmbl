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

## 2026-10-29  Wizard over smart-serial: getf missed answers on the slow console
- Area:    `servoterm-cli/setup/su_link.c` (`su_link_get`)
- Status:  Fixed
- Symptom: Over the smart-serial console (~1 kB/s) steps failed with "no
  answer from the drive for fb_switch0.pos_fb" after a reset, though the
  drive answered every try (strace).
- Cause:   su_link_get stopped at the first newline: the previous answer's
  tail, with only the start of its own ("fb_switch0.pos_fb = -"). All
  three tries failed so. USB delivers an answer in one burst.
- Fix:     Read until the pin's own line is complete. Test: servoterm-cli
  `make check`, "get: the last answer's tail first..." (fails without).
  Also: the host simulator lacked `confcrc` (the config write's checks,
  every wizard sim case failed), and its brake followed `io0.out0` through
  a link the boot's relink had collapsed to fault0.mot_brake, so a console
  override of the output didn't reach it: `io0.out0_state` now (host/hw/io.c).

## 2026-10-29  SEW CMP40M: the KTY wiring couples PWM into the resolver
- Area:    bench wiring; HV board motor temperature input (`mtemp`, hv0.mot_temp)
- Status:  Workaround (KTY disconnected, mot_temp_type 0 for commissioning)
- Symptom: Wizard step 12 stopped with fault 2 (resolver amplitude < 0.15).
  Enabled at 0 A, no motion: res0.amp fell from 0.259 to ~0.20 and jumped
  0.16..0.25; the KTY read 46..62 degC on a cold motor. res0.phase 0.85 is
  already the best (swept disabled and enabled).
- Cause:   The KTY goes to the HV board, whose ground is the HV side's. Its
  wires run with the resolver's (SEW signal plug), so the inverter's
  common-mode swing reaches the resolver inputs. With the KTY unplugged,
  enabled at 0 A reads 0.259 +-0.001, the same as disabled.
- Fix:     For now none: the thermal model only. Options: run the KTY wires
  apart from the resolver's (or in the power cable), earth the HV supply's
  DC-, or read the KTY on the LV side. FB1 can't take a resolver as is (no
  excitation; 1 of 10 ADC samples), see the session of 2026-10-29.

## 2026-10-29  USB console stops sending, input still works (was: "F4 hung after enable")
- Area:    `lib/STM32_USB_Device_Library-1.2.0/Class/cdc/src/usbd_cdc_core.c`,
  `src/usb_cdc.c`, `servoterm-cli/setup/su_link.c`
- Status:  Open (the stall under current; the wizard reconnects, smart-serial
  avoids it)
- Symptom: `/dev/ttyACM0` stays, the console sends nothing, the drive runs
  on and still takes input. Only under large motor currents: the wizard's
  id_mot (10000 rad/s^2) stalled it every run; 2400 queries over 27 min of
  soak motion (628 rad/s^2) never did.
- Cause:   (1) Fixed: ST's CDC went idle right after arming a ZLP (stuck
  after 350 port closes mid-output); transfers also never end on a full
  packet now. (2) Under current: a transfer stays armed (DIEPTSIZ 1 packet,
  0 bytes left, FIFO empty) and the host stops polling it; an abort, FIFO
  flush and resend don't revive it, a re-enumeration does. Interference on
  the USB link suspected (the Pi 4's xHCI and the bench's grounding).
- Fix:     The drive heals a lost DataIn, and aborts and resends a transfer
  the host polls (NAKs) but never gets. The wizard reopens its device when
  it goes away (by-id path, 15 s). A re-enumeration after "DTR set, no
  progress for 200 ms" was tried and removed: it also fired on plain flow
  control (a reader that paused 8 s, the host's tty full), looped (a bus
  reset left the CDC state busy, 45 in 10 min; usbd_cdc_Init clears it now)
  and left the console one packet behind (every soak answer one command
  late). From the drive's side a host that stopped polling looks like flow
  control. usbmon (/dev/usbmon1) saw EPIPE on the bulk IN during a soak.
  Tests: hwtest/suite `usb: 1000 port closes...`, servoterm-cli "reconnect:
  ..." (fails without). Tuning over USB came out lower (pos_bw 11.3 vs 35.4
  over smart-serial); the smart-serial config stays on the drive.
  `usbdiag` puts the state in `errlog`. `tools/ssflash.sh` finds the drive
  by its by-id link (it came back as ttyACM1, ssburn failed on ttyACM0).

## 2026-10-29  Wizard: a lost deleteconf doubled the config; messages broke acks
- Area:    `servoterm-cli/setup/su_conf.c` (`su_conf_write`)
- Status:  Fixed
- Symptom: The flash config held its 21 lines twice (two hv comps, see
  below); later "no ack for line 18 (appendconf ...)" with the drive's fault
  messages in the answers.
- Cause:   Any output counted as the answer to `deleteconf` (which prints
  nothing), so a lost one appended to the old RAM text; the ack had to be
  the first line of the answer; "Fault 9: HV timeout error" in the
  flashsaveconf answer read as a failure; read-back kept message lines.
- Fix:     RAM size 0 checked after deleteconf (`confcrc`), acks searched
  in the answer, the RAM size checked before flashsaveconf, drive messages
  dropped from the read-back. Test: servoterm-cli `make check`, "conf write:
  ..." on a fake drive (fails 3 of 4 without the fix). Over smart-serial the
  read-back starts with a blank line ("verify failed: flash differs"):
  blank lines are ignored in the compare (same test, fails without).

## 2026-10-29  Doubled config in flash: a second hv comp kills the HV link
- Area:    `servoterm-cli/setup/su_conf.c` (`su_conf_write`), `src/config.c`
- Status:  Open (drive repaired; how the text got doubled is not known)
- Symptom: SEW commissioning, wizard step 12 onward: endless "Fault 9: HV
  timeout", the F3 sends nothing (hv0.crc_error 0, fw_version -1, dc_volt 0)
  across LV and 24 V power cycles. HV board LED: 1 blink per 1.2 s (F3 app
  running, fault 1 = no valid packet from the F4). The wizard's own
  `--dump`/`--load` fail in the fault flood.
- Cause:   The flash config held its 21 lines twice. Every `link pid` loads
  a full set of comps, so the drive ran `hv0` and `hv1` (and pid1, fault1, ...)
  on the one F3 UART; the F3 never sees a valid packet, and it only starts
  sending after one. Which write doubled it is open: `su_conf_write` takes
  any output as the answer to `deleteconf`, so a lost `deleteconf` appends to
  the old RAM text. Note `showconf` prints the flash text, not RAM
  (`confcrc` prints the RAM size).
- Fix:     Rewrote one copy (deleteconf, confcrc size 0, appendconf, size
  check, flashsaveconf, showconf compare), reset: link up, 202 V, no fault.
  Ideas: confirm `deleteconf` (RAM size 0) before appending; refuse a second
  instance of the hardware comps (hv, adc, io) in `load_comp`. No test yet.

## 2026-10-26  The System card's buttons did nothing (system updates, first releases)
- Area:    `webui/mill/js/sysupdate.js`, `webui/mill/server/sysupdate.py`
- Status:  Fixed
- Symptom: On the EBAZ4205 images 2026.10.26.1/.2 the System card showed the
  version but Check, Upgrade and Restart did nothing; the page got no answer.
- Cause:   Three things. The card called `Live.sys` (the API is `Live.L`); a
  first fix used the global `LIVE`, which is still null when the hello arrives,
  so every page load threw (the whole live suite failed on it). And the
  replies merged the `{"t": "sys", ...}` snapshot into `{"t": "reply", ...}`,
  so the reply's type was overwritten and the page never matched it.
- Fix:     `Live.L.sys`; replies carry `status`/`check` only. Tests:
  `server/tests/test_sysupdate.py` (`test_replies_dont_carry_a_message_type`),
  `test/gui/live_suite.js` "system updates: ..." with
  `MILLWEB_UPDATE_CMD=test/fake-update-helper`.

## 2026-10-26  Live GUI suite on the EBAZ4205: some cases fail only in the full run
- Area:    `webui/mill/test/gui/live_suite.js` (remote, Chromium on a Pi 400 against the board)
- Status:  Open
- Symptom: In a full `make test-gui-live REMOTE=board:8080` run, "file page" (run
  with the cycle buttons, file management), "3D models" and "scope" failed; run
  alone (`-k`) the same cases pass, with and without the system-update change
  (the branch without it failed "3D models" alone once, too).
- Cause:   Not known; timing under load (a slow board and a software-rendering
  browser) is the likely one.
- Fix:     Open: find the waits that are too short in those cases.

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

## 2026-10-15  Arm: start-up phasing under gravity load trips POS_ERROR
- Area:    drive commutation at enable (`fb_switch`, incremental encoder),
           `host/arm` (simulated ARC Mate)
- Status:  Known
- Symptom: Simulated arm at a raised pose, no friction: J2's drive faulted
  (fault 5) right after enable, and the brake then let the arm sag.
- Cause:   With an incremental encoder (enc_fb state 1) the drive phases
  (fb_switch current_com_pos 4) by moving the motor; on a gravity-loaded
  joint with the brake released the arm moves during phasing.
- Fix:     Absolute encoders (FANUC serial pulse coders: commutation known
  at power-on); sim `enc_fb0.sim_absolute = 1`. On the real arm with an
  incremental encoder, phase only with the arm supported or at a
  gravity-neutral pose.

## 2026-10-15  Several simulated drives in one process: dlmopen
- Area:    `host/arm` (arm_sim, libstmbl_drive.so)
- Status:  Note
- Symptom: The firmware HAL is one set of globals: one drive per process.
- Cause:   by design (one drive per MCU).
- Fix:     Build the drive as `libstmbl_drive.so` (only `stmbl_sim_*`
  exported) and load it once per drive with `dlmopen(LM_ID_NEWLM)`: each
  copy has its own globals, no source change. No threads in the copies
  (boot with `stmbl_sim_boot`, not `stmbl_sim_open`); flush the copy's own
  stdout via `stmbl_sim_cmd`.

## 2026-10-15  LinuxCNC in the cloud container: build and run notes
- Area:    `hwtest/arcmate-sim`, branch `linuxcnc-bin`
- Status:  Workaround
- Symptom: (1) LinuxCNC master (2.10-pre) changed the HAL C API
  (`hal_float_t` pins gone, `hal_real_t` refs), so `stmbl_sim`/`stmbl_arm`
  do not build; (2) configure picks python3.13 while Boost.Python is 3.12;
  (3) the 2.9 build wants `a2x` (asciidoc/docbook) for man pages;
  (4) `rtapi_app` refuses to run as root; (5) without a tty `linuxcnc`
  writes all output to /tmp/linuxcnc.debug.* (and ~/linuxcnc_debug.txt),
  and a DISPLAY program's stdout is lost; (6) it needs
  `configs/common/linuxcnc.nml`.
- Cause:   as listed.
- Fix:     Build branch 2.9 (the bench runs 2.9.1); `PYTHON_BIN=python3.12
  ./configure ... --disable-gtk2`; a stub a2x on PATH; run as user `lcnc`;
  the test display writes its TAP to a file; pack `configs/common`. See
  `build_linuxcnc.sh` on `linuxcnc-bin` and `hwtest/arcmate-sim/README.md`.

## 2026-10-15  Collision residual at standstill: gearbox stiction
- Area:    `shared/robot/collide.c`, `linuxcnc/armcollide`
- Status:  Known
- Symptom: Simulated ARC Mate, detector with the exact model: the
  residual is ~1-4 Nm while moving but 15-25 Nm on J1-J3 at standstill,
  with no contact.
- Cause:   Stiction in the gears holds an unknown part (up to the
  breakaway friction) of the load once a joint stops; the motor torque the
  servo leaves is not the model's.
- Fix:     Thresholds with a stiction band near zero speed (`thresh J thr0
  thr_v thr_s vs`); the detector is less sensitive at rest. Set it from the
  measured floor (MEASUREMENT_PLAN Phase 7).

## 2026-10-15  arm_sim / stmbl_arm: joint offsets taken from the first packet
- Area:    `host/arm/armcore.c` (absolute-encoder offsets)
- Status:  Fixed
- Symptom: LinuxCNC's joint feedback off by up to one motor turn / ratio
  (J2: 1.06 deg) from the true motor angle, depending on start-up order.
- Cause:   The offset (joint = feedback + off) was set at the first
  exchange, where the drive still reports position 0.
- Fix:     Set it at the 5th exchange.

## 2026-10-15  Collision detector false trips under LinuxCNC's trapezoids
- Area:    `shared/robot/collide.c` (thresholds)
- Status:  Fixed
- Symptom: Fine with smooth test moves, but LinuxCNC joint jogs (full
  300 deg/s^2 from standstill, jerk steps) tripped it on the simulated arm.
- Cause:   Three effects of the jerk: (1) model errors (inertia +-15 %)
  scale with the dynamic torque; (2) through elastic gears the rotor runs
  ahead of the link (~arm_jj * qdd, 26 Nm on J1 for ~15 ms) and torque
  coupled from other joints' accelerations hits joints held by stiction;
  (3) the gears ring at ~20 Hz afterwards, longer than the observer's
  1/k. And the residual lags the torque by 1/k, so a threshold following
  the torque instantly drops when the acceleration reverses.
- Fix:     Threshold terms thr_rel * peak(max(|tau - g|, |(M qdd)_j|)) and
  thr_rot * peak(arm_jj |qdd|), peaks decaying over `envelope` (0.15 s).
  Test moves now include LinuxCNC's trapezoids (`jog`, `tmove`).

## 2026-10-15  genserkins: tool orientation at C = +-180 turns J6 a full turn
- Area:    `hwtest/arcmate-sim` (LinuxCNC 2.9 genserkins), arm flange frame
- Status:  Workaround
- Symptom: After a stop LinuxCNC reported the pose with C = +180 (was
  -180); the next G1 ... C-180 is a 360 deg move of C: "Linear move would
  exceed joint 5's negative limit".
- Cause:   A/B/C come from the rotation matrix: at +-180 the sign is
  arbitrary, and LinuxCNC treats C as a linear axis.
- Fix:     Keep working orientations away from +-180: the flange frame is
  turned (J6 theta0 180): the home orientation is A0 B0 C0.

## 2026-10-15  Collision estop request missed by LinuxCNC's I/O controller
- Area:    `linuxcnc/armcollide`, estop chain in `hwtest/arcmate-sim`
- Status:  Fixed
- Symptom: A collision during a program ended with the machine off (task
  state 2), not estop.
- Cause:   `collision` cleared as soon as the drives were disabled: the
  drop of `emc-enable-in` lasted a few ms; iocontrol samples it every
  [EMCIO] CYCLE_TIME (100 ms).
- Fix:     `collision` latched until `armcollide.0.reset` (wired to
  `iocontrol.0.user-request-enable`, the estop reset).

## 2026-10-15  Arm geometry: bodies two links apart overlap by design
- Area:    `shared/robot/geom.c` (self-clearance), `host/arm/arcmate`
- Status:  Note
- Symptom: Self-clearance -92 mm at the home pose.
- Cause:   Only bodies on neighbouring links are skipped (selfskip 1); the
  pedestal (link 0) and the upper arm (link 2) overlap at the shoulder, the
  forearm (3) and the wrist (5) at the wrist.
- Fix:     `selfignore 0 2`, `selfignore 3 4` in the arm's geometry; check a
  new arm's self-clearance at a few poses before relying on it.

## 2026-10-15  Collision prevention: stop-path check, findings
- Area:    `shared/robot/guard.c` (rb_guard_watch), arm soak
- Status:  Note
- Symptom: With prevention on, random joint moves within the joint limits
  were stopped (protective stop) though the true clearance stayed > 50 mm.
- Cause:   (1) The check uses the guard's real stop: with the default 5
  rad/s^2 the fast wrist would coast ~33 deg, into the fold of the torch on
  the forearm. (2) The random targets themselves folded the forearm onto
  the lower arm: with the guessed bodies that happens from q3 ~ 85 deg,
  inside the guessed J3 limit (160).
- Fix:     astop per joint = what the drives can do in a stop (sim: 2 x
  MAX_ACCELERATION); random targets keep q3 <= 60, q2 + q3 <= 110,
  |q5| <= 100. On the real arm: measure J3's range (the hard stop will be
  earlier) and the bodies; check self-clearance over the range.

## 2026-10-15  Collision detector: 20 % model error too tight for fast multi-joint moves
- Area:    `host/arm/arcmate/model_exact.arm` (threl)
- Status:  Fixed
- Symptom: A false collision in a long random-move soak (J2 residual 58 Nm
  against 55 Nm) with J2 and J3 both near 0.7 of their speed limits.
- Cause:   The joint torque swings by ~300 Nm within ~70 ms there (elastic
  gears, coupling); 20 % of the dynamic torque left too little margin.
- Fix:     threl 0.3 on J1-J3 (worst residual/threshold 0.90 over 36 random
  moves); detection tests unchanged.

## 2026-10-15  sserial position feedback is raw while the drive is disabled
- Area:    `shared/comps/sserial.c`, `fb_switch`; `host/arm/arm_sim.c` (gripper)
- Status:  Known
- Symptom: The simulated gripper drive faulted with POS_ERROR at enable, and
  closing reported "gripped" at once, at 78 mm.
- Cause:   While disabled, `fb_switch` makes the drive's command track
  LinuxCNC's command, not the motor, and sserial's pos_fb is the raw
  `mot_fb_no_offset` plus revolutions (linrev) scaled by the `scale` LinuxCNC
  sends. arm_sim sent the gripper's command and scale only while enabled, so
  the scale was 0 while disabled. At enable the raw encoder fraction (9.4
  turns) then appeared as a position jump. The arm joints only escaped
  because their motors sit at whole turns at home.
- Fix:     LinuxCNC (and arm_sim) must keep sending pos_cmd = feedback and
  the scale while disabled. The gripper block in arm_sim's servo now runs
  before the `!en` return.

## 2026-10-15  stmbl config files take no comments
- Area:    drive config (`host/arm/arcmate/gripper.conf`), `shared/config.c`
- Status:  Known
- Symptom: `conf0.max_sat = 0` in gripper.conf had no effect
  (`conf0.max_sat` read back 0.2), so the gripper still saturation-faulted.
- Cause:   `#` comment lines in the config break the boot's parsing of the
  lines after them. Nothing is reported, since the boot echo is silenced
  in armcore.
- Fix:     Keep configs comment-free and put notes in the `.tool` / `.arm`
  file. armcore strips comments only from the sim file.

## 2026-10-15  Gripper drive: holding a part is saturation, squeeze vs max_pos_error
- Area:    `host/arm/arcmate/gripper.conf`, `shared/robot/gripper.c`
- Status:  Note
- Symptom: Gripping faulted the drive after 0.2 s with SAT_ERROR (fault 6).
  A 4 mm squeeze gave POS_ERROR (fault 5).
- Cause:   A gripper holds its fingers at the current limit (the grip force)
  for as long as it holds a part, and the default `max_sat` is 0.2 s. A
  squeeze is a following error: 4 mm of gap is 0.5 motor turns (8 mm/turn),
  over the stock `max_pos_error` of 1.5 rad (1.9 mm).
- Fix:     gripper.conf sets `conf0.max_sat = 0`. Squeeze is 1.2 mm and
  lost_tol 0.8 mm: a part slipping out still lets the fingers close past
  lost_tol (LOST).

## 2026-10-15  Payload check: no measurement at standstill; accuracy = model floor
- Area:    `shared/robot/gripper.c` (rb_grip_payload)
- Status:  Note
- Symptom: Measured after the grip at standstill, a 2 kg part read 4.2 kg
  (5.7 kg relative to a pre-grip baseline). The 6-DOF flange wrench gave
  2.6 kg even with the exact model.
- Cause:   1. At standstill, static friction takes an unknown share of a new
  load: J2's gear torque rose 2.2 Nm of the ~12 Nm expected.
  2. The flange wrench has 6 unknowns, so every joint's model error becomes
  mass (J5's 0.8 Nm alone reads 0.8 kg).
  3. On the lift itself the residual floor (J3 ~2 Nm exact, ~5 Nm perturbed)
  is a constant offset: +0.3 / +0.8 kg for any mass from 0.5 to 4 kg.
- Fix:     On a grip the model takes the declared part at once. The mass is
  checked while moving: a 1-parameter least-squares fit (r = dm * gravity
  torque per kg at the part's centre of mass) over the joints faster than
  0.15 rad/s (3x the Stribeck speed). Tolerance is 1 kg; a smaller error
  stays under the collision thresholds anyway.

## 2026-10-15  arcmate.arm split into arm + tool; readers without `include`
- Area:    `host/arm/arcmate/*.arm`, `*.tool`, `host/tests/robot/test_robot.c`
- Status:  Note
- Symptom: The genserkins cross-check in test_robot failed (no DH read) once
  arcmate.arm became `include arcmate_base.arm` + `include torch.tool`.
- Cause:   armcore supports `include`, but test_robot's DH reader and
  `rb_collide_load` (a `detector` file) do not.
- Fix:     test_robot reads arcmate_base.arm. Detector models stay
  self-contained (model_exact.arm, model_perturbed.arm).

## 2026-10-15  Gripper housing vs forearm: self-clearance at low poses
- Area:    `hwtest/arcmate-sim/cell_grip.txt`, `host/arm/arcmate/gripper.tool`
- Status:  Known (guessed geometry)
- Symptom: In lcnc_grip_test, a G0 to the flange at (850, 50, 1050) mm,
  flange up, was stopped by collision prevention (armguard reason 2).
- Cause:   With the flange up, lower poses tilt the forearm. The gripper
  housing (r 45 mm, vs the torch's 25 mm) then comes within the 10 mm
  margin of the forearm capsule. Both bodies are guesses.
- Fix:     The test's second part sits higher (flange at z 1200). Re-check
  once the real housing and forearm are measured.

## 2026-10-15  Tool changer docks: flange-up hits the forearm, flange-forward is the wrist singularity
- Area:    `host/arm/arcmate/tools.txt`, `arcmate_changer.arm`
- Status:  Known (guessed geometry)
- Symptom: Flange-up docks at z 1050 caused protective stops: the forearm
  vs the changer plate, with self-clearance 10 mm at z 1000 and negative
  at 950. With docks flange-forward (B 90), J4 overspeed-faulted (off 1)
  moving to a part.
- Cause:   Flange up at low poses tilts the forearm towards the plate.
  Flange forward with the forearm level is J5 = 0, the wrist singularity:
  a small Cartesian change spins J4 and J6.
- Fix:     Docks and working poses at B 45 (J5 -41 to -66 deg, clearance
  56 mm or more). Changer plate r 40 mm. Re-check with the real geometry.

## 2026-10-15  LinuxCNC 2.9: tool TCP through switchable kinematics works
- Area:    `linuxcnc/armkins`, `hwtest/arcmate-sim/toolchange.ngc`
- Status:  Note
- Symptom: genserkins has no tool orientation, and a TCP change mid-program
  would make the Cartesian position jump.
- Cause:   --
- Fix:     armkins exports kinematicsSwitchable() / kinematicsSwitch(type)
  with type = tool ID. `M68 E3 Q<tool>` drives motion.switchkins-type, then
  `M66 E0 L0`: motion recomputes the Cartesian position from the joints
  and the interpreter resyncs (checked: the position jumps to the TCP,
  later moves are clean, back to 0 too). M64/M65/M68 are immediate (not
  queued with motion), so each needs an `M66 E0 L0` before it, or the lock
  would open while the arm still moves into the dock.

## 2026-10-15  armtool: kinematics check tripped on the settling at enable
- Area:    `linuxcnc/armtool/armtool.c`
- Status:  Fixed
- Symptom: After a lost tool, the machine went straight back to estop at
  enable (reason 5), so the kinematics could not be put back to the flange.
- Cause:   "Moving" was any joint over 0.01 deg/s; the settling after
  enable exceeds that.
- Fix:     Moving = a joint over 0.1 deg in 50 ms. Standing still, the
  operator can set `M68 E3 Q0`, `M66 E0 L0`, `M61 Q0`.
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

## 2026-10-23  webui/mill/bench: a 1 mm following error tripped X at enable
- Area:    webui/mill/server/millweb.py, webui/mill/bench (phasing_ferror.hal, mill.hal)
- Status:  Fixed
- Symptom: Switching the bench mill on, X jerked (the drive autophasing)
  and LinuxCNC stopped with "joint 0 following error" (highmark 1.17 mm).
- Cause:   The larger limit while the drive autophases (PHASING_FERROR 2 mm,
  hwtest/burnin's phasing_ferror.hal) is a POSTGUI_HALFILE: it links the
  ini.0.* pins LinuxCNC makes after the HALFILEs. AXIS loads those files;
  millweb, as the display, didn't, so ini.0.ferror stayed 1 mm. Loaded
  by hand, its `loadrt and2` then failed: under uspace a module loads
  once, and mill.hal already loads and2 for the probe.
- Fix:     millweb runs [HAL] POSTGUI_HALFILE (.tcl through haltcl) and
  POSTGUI_HALCMD as AXIS does, after it starts, and reports each in the
  event log (a failure as an error). The bench loads both and2 instances
  in mill.hal. Test: test_millweb.PostGui. On the bench: ini.0.ferror 2
  until X is homed and enabled 1.5 s, then 1.

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

## 2026-10-23  hm2_spix: a cold start read the card bit-shifted at 31.25 MHz
- Area:    `hwtest/burnin/stmbl.ini` [HOSTMOT2] DRIVER_PARAMS (Pi 4 + 7C81)
- Status:  Workaround
- Symptom: after a power-up, LinuxCNC with the burn-in config failed:
  "hm2_spix: SPI0/CE0: MISO input is bit-shifted by one bit. SPI read clock
  frequency probably too high", "Invalid cookie", no board. The same 35.7 /
  31.25 MHz had worked for days.
- Cause:   the read margin at 31.25 MHz is thin (reads failed at 41.7);
  a cold board or supply shifts it. Not pinned down.
- Fix:     25 MHz both ways (`spiclk_rate=25000 spiclk_rate_rd=25000`), also
  in webui/mill/bench. The Pi-side timing test (hwtest/suite) shows the cost.

## 2026-10-23  Index homing again: the index reported at once, without moving
- Area:    `shared/comps/idx_home.c`
- Status:  Fixed
- Symptom: On the bench mill (webui/mill/bench) homing X a second time
  "homed" it where it stood: index-enable dropped 50 ms after it was set,
  the joint never crept to the index (the true index was 3 mm away).
- Cause:   idx_home finds the index as fb_abs crossing between quadrants 1
  and 4, comparing each sample with lastq. A new request compared its
  first sample with the lastq of the request before, which stopped just
  past the index (quadrant 1 or 4): standing anywhere in the other of the
  two, that was a crossing. Also: waitabs (an index requested before the
  drive had seen one) was never cleared, so it would fire every later
  request at once.
- Fix:     A new request starts lastq at its own first sample; waitabs is
  cleared when it fires or the request goes away. Tests:
  host/tests/index_rehome.test (fails before: found after 3 periods),
  index_rehome_creep.test.

## 2026-10-14  --sim file ignored after a comment line
- Area:    `host/drive.c` (drive_boot, --sim)
- Status:  Fixed
- Symptom: A `--sim` file starting with a `#` comment had no effect at all.
- Cause:   The file was passed to the console as one string; the comment
  stripper cut everything from the first `#` to the end, i.e. every line.
- Fix:     Parse the --sim text line by line, stripping comments per line.

## 2026-10-14  Linear scale with periodic reference marks breaks commutation
- Area:    `src/comps/enc_fb.c` (index latch), linear motors
- Status:  Known
- Symptom: Linear axis (60 mm electrical period) on a scale with a reference
  mark every 20 mm: absolute commutation from the first mark is right, then
  the axis loses force and trips POS_ERROR after the next mark (sim:
  `host/tests/linear_periodic_marks.test`).
- Cause:   enc_fb re-latches `absoffset` on every index pulse while
  `en_index` is set; only marks a whole electrical period apart land at the
  angle `conf0.mot_fb_offset` was measured for, the others are +-120 deg off.
- Fix:     Use a single (selectable) reference mark, e.g. the RGH22 reference
  actuator, or marks at a multiple of the electrical period. A firmware option
  to latch only the first index would also cover it.

## 2026-10-14  Sim: encoder drifted from the true position over long travel
- Area:    `host/hw/hv.c` (PMSM integrator)
- Status:  Fixed
- Symptom: Linear axis round trip to -485 mm and back: encoder at 0, carriage
  (`sim_abs_pos`) at +0.06 mm; reference marks moved against the counts.
- Cause:   `sim_pos` (what enc_fb counts) was a wrapped float sum at 80 kHz,
  `sim_abs_pos` a separate double sum; float rounding drifted between them.
- Fix:     `sim_pos` is now derived from the double position every step.

## 2026-10-14  rlpsij wanders on a limited stroke
- Area:    `shared/comps/rlpsij.c` (states 3-5)
- Status:  Fixed
- Symptom: Linear axis (60 mm period): the id moved the carriage 0..+208 mm,
  all one way; started near an end it would hit the stop.
- Cause:   State 4 always starts driving +, and its coasts are long on a low
  friction axis (~95 mm from 0.6 m/s); state 5 steps + first.
- Fix:     `rlpsij0.max_travel` (rad, 0 = off): segments head back past half
  of it, a coast heading away brakes, state 5 runs towards the start, abort
  beyond it. 100 mm limit: -39..+59 mm used, same results.

## 2026-10-14  No acceleration feedforward with LinuxCNC over sserial
- Area:    `conf/template/sserial.txt`, `conf/template/pid.txt`
- Status:  Known
- Symptom: Linear axis at 10 m/s^2: following error ~0.4 mm with bench gains
  even though j is exact; `pid0.ff_torque_cmd` holds only friction/damping.
- Cause:   sserial.txt sets `vel0.en = 0`; `pid0.acc_ext_cmd = vel0.acc` is
  then 0, so j * acc comes from the feedback loops. Enabling vel0 helps
  little (0.37 -> 0.30 mm): its acc is pos_error * w^2 of 1 kHz steps.
- Fix:     Open. Higher cur_bw/vel_bw (id_pid) gets it under 0.17 mm in sim. A
  proper fix is an acc from the commanded velocity (vel_int) or from LinuxCNC.


## 2026-10-17  Baldor linear motors: phase colours depend on the drive
- Area:    `docs/baldor_linear.md` (Baldor LMCF / LMBL, MN1800 cables)
- Status:  Note
- Symptom: (documentation) the same AY1763A00 cable lists two phase
  colourings: LinDrive/MintDrive (U red, V white, W black) and "trap"
  drives (U black, V red, W white).
- Cause:   Baldor matched the phase order to the Hall order of each drive.
- Fix:     On stmbl, find the phase order by scoping the back-EMF against
  Hall 1/2/3, not from the colours (docs/baldor_linear.md, "Bring-up
  checks").

## 2026-10-18  enc_fb: input filter set before the timer clock (fast scales)
- Area:    `src/comps/enc_fb.c` hw_init (F4, FB0 quadrature on TIM4 for v4.0)
- Status:  Fixed (compiles for the F4; not yet run on a drive)
- Symptom: none seen; found by reading the code. Relevant for fast linear
  scales: 1 µm at 4–5 m/s is 4–5 M counts/s.
- Cause:   Three problems:
  - TIM_ICInit (ICFilter = 0xF) ran before
    RCC_APB1PeriphClockCmd(FB0_ENC_TIM_RCC), so its writes were lost
    unless res.c had already enabled TIM4. The inputs ran unfiltered.
  - `TIM_Channel_1 | TIM_Channel_2` is TIM_Channel_2 (Channel_1 is 0), so
    TIM_ICInit only ever reached B.
  - Its BothEdge polarity sets CC2NP, which TIM_EncoderInterfaceConfig
    leaves set. Encoder mode must not use that.
  Just moving the clock enable would have applied a 3 µs filter to B
  only (0xF: fDTS/32, N = 8: about 0.65 m/s at 1 µm), in both-edges mode.
- Fix:     Enable the clock first and drop TIM_ICInit. After
  TIM_EncoderInterfaceConfig, write IC1F/IC2F in CCMR1 from the new pin
  `enc_fb0.input_filter` (set at load; default 3 = 8 samples at 84 MHz,
  ~0.1 µs; 0 = off). The counting direction (B inverted) is unchanged.
  Still to check on a drive: counts against distance at speed, and the
  direction.

## 2026-10-18  No emergency braking: every fault coasts the motor
- Area:    `shared/comps/fault.c`, F3 `stm32f303/src/comps/io.c` / `ls.c`
- Status:  Fixed (by "No emergency braking: every fault coasted the motor"
  above: qstop, dbrake, the F3 braking on its own; the claude/linear-motor
  branch's note, merged after)
- Symptom: (code review, for linear axes at speed) a fault or a lost link
  leaves a moving axis to coast into its end stop.
- Cause:   Every fault (CMD_ERROR after about 5 rt cycles of lost
  smart-serial/CAN, a feedback error, over-voltage, ...) goes to
  SOFT_FAULT. That sets en_out = 0, and the F3 clears HV_EN: all switches
  off. The F3 does the same if the F4's packets stop (ls.c, after 5
  missed). mot_brake drops (the holding brake engages, if one is fitted;
  linear motors usually have none). Regen while decelerating can only
  clamp current (high_dc_volt) or trip HV_VOLT_ERROR. fault0.dc_brake is
  computed but used nowhere, and the F3's BRK output (io0.brk, with
  brk_present detection) is not linked by any template.
- Fix:     Ideas: a controlled quick stop on CMD_ERROR while the feedback is
  good; dynamic braking (all low sides on) when control is lost but the
  power stage works; a chopper on BRK driven by dc_brake; and, for a dead
  drive, an external normally-closed contactor shorting the phases through
  resistors, plus shock absorbers at the ends.

## 2026-10-26  Linear axis: id_pid tunes lower on master
- Area:    `shared/comps/ids.c` (id_pid), host simulator, `host/tests/gen_linear_soak.py`
- Status:  Open
- Symptom: Merging claude/linear-motor into master: id_pid on the simulated
  linear axis (examples/linear_axis.conf) now ends at pos_bw 20, vel_bw 77,
  vel_d 1.75; on the branch it reached pos_bw 101, vel_bw 1000, vel_d 1. With
  master's gains the linear soak's following error is 0.33 rad (3 mm) against
  a 0.021 limit; with the branch's gains it passes 12/12 on master.
- Cause:   Not found. ids.c is the same on both; the first scored cycle
  already costs more than kt x the warm-up's, so vel_bw is cut at once
  (100 x kd). Master's plant and loop differ since the branch: probe
  autophasing (phase_mode 1), vel2.en = fb_switch0.state, qstop / dbrake in
  pid.txt, the simulated motor's coulomb friction hold, the mains-fed bus
  (sim_supply 0 changes nothing). No linear motor to compare on hardware.
- Fix:     gen_linear_soak.py uses the branch's gains (--tune runs id_pid).
  The linear tests wait 3 s for probe phasing (it takes ~2 s here).

## 2026-10-14  uf (V/f) template: slip from |i|, stale angle, voltage derating
- Area:    `conf/template/uf.txt`, `shared/comps/uf2.c` (comp `uf`),
           `conf/template/sserial_uf.txt`, `conf/asyc_uf_3kw.txt`
- Status:  Open (uf unchanged; acvf avoids all of these)
- Symptom: On the simulated 100 W induction motor: +1.1% speed at no load,
  -32% at rated load (50 Hz), stall at 25 Hz with half load, then the
  rotor driven backwards and a motor overtemperature fault.
- Cause:   Slip compensation scales with |i|, which counts the magnetizing
  current as load and has no resistive-drop boost. hv0.vel is not set, so
  between 5 kHz packets the F3 holds a stale angle. uf0.scale =
  fault0.scale scales the voltage: thermal derating collapses the flux, a
  hot motor cannot restart. sserial_uf reports the command back as
  vel_fb and no pos_fb. asyc_uf_3kw.txt sets ramp0.max_speed, which does
  not exist (ramp0.max_vel), and never ramp0.en.
- Fix:     New acvf (see "acvf / idacim"). Fixing uf in place would change
  existing machines; left as is.

## 2026-10-14  acvf / idacim: induction motor V/f and autotune
- Area:    `shared/comps/acvf.c`, `shared/comps/idacim.c`, templates acvf,
           sserial_acvf, id_acim; sim `hv0.sim_type = 1`; branch
           `claude/acim-conveyor`
- Status:  Note (sim only)
- Symptom: (design notes) Found while building it on the sim:
- Cause:   (1) Slip compensation is positive feedback: filtered at 20 Hz it
  hunts above ~100 rad/s (5 Hz now). (2) r * id compensation hunts on the
  larger motors (370 W: 17, 750 W: 79 rad/s peak to peak) where plain V/f
  is stable or marginal: active damping (torque high-pass on the frequency,
  damp 0.5..4) stops it. (3) At zero frequency u = r * id holds any dc
  current forever: faded in only above 3 * u_boost. (4) idacim's rotor
  step: the current takes ~20 ms to settle, so a fit that assumes constant
  current read tau_r +19%; the fit now uses the measured current.
  (5) Without cur_bw (uf-derived templates leave it 0) current mode gives
  no voltage: id_acim sets hv0.cur_bw.
- Fix:     4 motors (100 W 4p .. 750 W 2p): autotune rs 0.5%, rr/lm/tau_r
  ~3%, leakage ~6% (sigma*ls +5%: the rotor flux partly follows the square
  wave); speed within 1% with the autotuned values, travel within 1%.
  Untested: hardware, regenerative overvoltage on fast stops, field
  weakening, the ARM build.

## 2026-10-14  Test infra: float sim position, HAL query prefixes, link echoes
- Area:    `host/hw/hv.c` (sim_abs_pos), `host/tests/run_tests.py`
- Status:  Fixed / Note
- Symptom: The belt travel check failed by 2% only late in a run; queries
  showed "wrong" values in index-based checks.
- Cause:   sim_abs_pos summed in float: near 2000 rad one ulp is 2.4e-4
  rad against ~1.8e-3 rad per 12.5 us step, a biased rounding of ~7% per
  add. A console query prints every pin with that prefix (acvf0.vel also
  prints vel_cmd, vel_n, vel_sync), and the boot's link echo "OK sink <=
  source = v" is parsed as a value of sink, shifting [k] indices.
- Fix:     sim_abs_pos accumulates in double. Index linked pins from the
  end ([-1]); avoid pin names that prefix others (acvf: blocked,
  block_time, jtimer).

## 2026-10-26  acvf: the current limit never acted under master's F3 current clamp
- Area:    `shared/comps/acvf.c` (induction V/f), F3 `curpid.c` voltage mode, host `hw/hv.c`
- Status:  Fixed (simulated; no induction motor on the bench)
- Symptom: Merging claude/acim-conveyor into master, acim_100w_4p_speed: a
  belt jammed with 4x rated friction stalled, but acvf0.blocked (sserial
  in2) never came: acvf0.lim stayed 0, the frequency kept rising (vel_sync
  115 rad/s, u_cmd 178 V) with the rotor at 0.
- Cause:   acvf pulls the frequency back only while the current is above
  max_cur. Since the branch was made the F3's current loop in voltage mode
  (curpid.c, and the simulator's stand-in) scales its voltage down above
  max_cur itself, so the current sits at max_cur and never above it: the
  V/f limit, and with it the blocked-belt detection, never engaged. The
  same on a real drive.
- Fix:     acvf0.lim_share (0.9): the limit acts at 0.9 x max_cur x scale,
  below the F3's clamp. Test: acim_100w_4p_speed.test (blocked == 1 at the
  jam; fails without it). The induction model now uses the simulator's
  mech_step (friction, brake, end stops) and a stiff sim_udc bus.

## 2026-10-15  stmbl_arm hung in its first servo call after a struct change
- Area:    `host/arm/lcnc/Makefile`, `host/lcnc/stmbl_sim_api.h`
- Status:  Fixed
- Symptom: After master's `power` field came in, every arm LinuxCNC config
  failed with "USRMOT: ERROR: command timeout". A bare halrun showed the
  servo thread's time stuck at 0: stmbl-arm.0 never returned.
- Cause:   `stmbl_sim_pd_t` grew a field. armcore.o and stmbl_arm_rt.o did
  not depend on stmbl_sim_api.h, so an incremental build kept the old
  layout in them while libstmbl_drive.so had the new one.
- Fix:     Both now depend on stmbl_sim_api.h. After a header change in
  host/lcnc, `make clean` is the safe way.

## 2026-10-18  vis3d: offscreen GL here, and what core GL leaves out
- Area:    `linuxcnc/vis3d` (vis3d_render, tests)
- Status:  Note
- Symptom: Mesa's surfaceless EGL platform offered no EGLConfig at all, so
  eglCreateContext failed; PyQt5's xcb plugin wouldn't load ("could not
  load the Qt platform plugin xcb even though it was found"); the TCP trail
  drawn as GL_LINE_STRIP was a hair-thin line.
- Cause:   surfaceless contexts need EGL_KHR_no_config_context; the PyQt5
  wheel's xcb plugin wants libxcb-icccm4, -image0, -keysyms1,
  -render-util0, -xinerama0, -xkb1 and libxkbcommon-x11-0; core profiles
  have no wide lines (glLineWidth > 1).
- Fix:     create the context with EGL_NO_CONFIG_KHR and render into an
  FBO; install those packages for `make qt-check`; draw the trail as a
  ribbon facing the camera.

## 2026-10-18  armguard clearance is mm; clearance-stop is not a limit
- Area:    `linuxcnc/armguard` pins, `linuxcnc/arm6hmi`, `linuxcnc/vis3d/qtvcp/vis3d_widget.py`
- Status:  Fixed
- Symptom: the first ARM-6 screen showed "clearance 33797 mm" and a wrong guard
  state.
- Cause:   two mistakes in the screen:
  - it read `.clearance` as metres, but armguard gives mm;
  - it used `.clearance-stop` as the limit, but that pin is the clearance
    along the stop path. The limit is `prevent MM` in the cell file.
- Fix:     the screen and the widget take `armguard.0.state` (non-zero:
  stopped, `.reason` 1 collision / 2 protective stop) plus `.clearance` in mm
  against the cell's `prevent` (parsed with its includes; INI `PREVENT_MM`
  overrides it). NEAR is shown below twice the limit.

## 2026-10-18  QtVCP joint jog on genserkins: warning but it works
- Area:    QtVCP `Action.DO_JOG`, `linuxcnc/arm6hmi`
- Status:  Known
- Symptom: joint jogging logs "Joint jogging not supported for non-identity
  kinematics".
- Cause:   QtVCP warns for any non-identity kinematics. The jog is still sent
  in joint mode (teleop off) and moves the joint.
- Fix:     none needed; arm6hmi's self-test jogs J1 +10 deg through it.

## 2026-10-18  LinuxCNC stat.joint has max_ferror, not ferror_limit
- Area:    `linuxcnc/arm6hmi/arm6_handler.py`
- Status:  Fixed
- Symptom: the screen's 100 ms refresh silently stopped (nothing updated past
  the first joint).
- Cause:   `stat.joint[N]["ferror_limit"]` doesn't exist in 2.9 (the keys are
  `ferror_current`, `ferror_highmark`, `max_ferror`, `min_ferror`). A QTimer
  slot that raises dies quietly in QtVCP.
- Fix:     use `max_ferror`. `_update` now wraps the refresh in try/except and
  logs the traceback once.

## 2026-10-18  Incremental jog cancelled by the button release
- Area:    `linuxcnc/arm6hmi/arm6_handler.py` (jog rows)
- Status:  Fixed
- Symptom: an incremental jog (e.g. 10 deg) moved only a fraction of a degree.
- Cause:   the release handler sent a jog stop, which also aborts an
  incremental jog in progress.
- Fix:     on release, stop only in continuous mode.

## 2026-10-18  QtVCP in the cloud image needs PyQt5 for python3.12
- Area:    `hwtest/arcmate-sim` `make hmi` / `test-hmi`, cloud session
- Status:  Workaround
- Symptom: `qtvcp` fails to import PyQt5 (LinuxCNC's modules are python3.12;
  Ubuntu's python3-pyqt5 doesn't match it). Then the xcb platform plugin
  fails to load.
- Cause:   `linuxcnc_restore.sh` installs no Qt.
- Fix:     install by hand:
  - `python3.12 -m pip install --break-system-packages PyQt5 QScintilla PyOpenGL`
  - `apt-get install xvfb libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-randr0 libxcb-render-util0 libxcb-shape0 libxcb-xinerama0 libxcb-xkb1 libxkbcommon-x11-0`

  Idea: add both to the restore script.

## 2026-10-19  A HAL pin sits on one signal: postgui nets join the existing one
- Area:    `linuxcnc/arm6hmi/arm6_changer_postgui.hal`, `hwtest/arcmate-sim/changer.hal`
- Status:  Fixed
- Symptom: with the tool changer config LinuxCNC quit while starting the
  screen, showing only its error dialog (show_errors.tcl). `test-hmi` hung
  until its timeout with an empty log.
- Cause:   `net arm6-tool armtool.0.mounted => arm6.vis3d.tool` in the postgui
  file: `changer.hal` already has `armtool.0.mounted` on the signal
  `tool-mounted` ("Pin ... was already linked"). The message is only in
  /tmp/linuxcnc.debug.*, which is deleted when the dialog closes.
- Fix:     `net tool-mounted => arm6.vis3d.tool`. To read such errors, cat
  /tmp/linuxcnc.print.* and /tmp/linuxcnc.debug.* while the dialog is up.

## 2026-10-19  Checking a world jog right after a joint jog: use the commanded position
- Area:    `linuxcnc/arm6hmi/arm6_selftest.py` (jog pad)
- Status:  Fixed
- Symptom: X -10 mm from the jog pad measured -9.905 mm, now and then (the
  step timed out).
- Cause:   the start X came from `actual_position` right after a J2 +10 deg
  jog. The simulated arm's feedback was still settling, and a J2 move
  changes X a lot. The jog itself was exact.
- Fix:     wait 0.6 s after the joint jog, then check the world jog on
  `stat.position` (commanded): -10.000 mm.

## 2026-10-19  Long status labels set the window's minimum width
- Area:    `linuxcnc/arm6hmi` (subtitle, events, footer)
- Status:  Fixed
- Symptom: the screen grew to 1836 px, then 2490 px wide (wider than the
  1600 px Xvfb screen). The subtitle showed armkins' whole
  `model=... tools=...` line.
- Cause:   a QLabel's minimum width is its text's width.
- Fix:     `setMinimumWidth(40)` on the status labels, so they are clipped
  rather than widening the window. The subtitle shows only the kinematics
  module. (`QSizePolicy.Ignored` instead squeezed them to nothing next to a
  stretch.) Now 1448 px.


## 2026-10-14  --sim file ignored after a comment line
- Area:    `host/drive.c` (drive_boot, --sim)
- Status:  Fixed
- Symptom: A `--sim` file starting with a `#` comment had no effect at all.
- Cause:   The file was passed to the console as one string; the comment
  stripper cut everything from the first `#` to the end, i.e. every line.
- Fix:     Parse the --sim text line by line, stripping comments per line.

## 2026-10-14  Linear scale with periodic reference marks breaks commutation
- Area:    `src/comps/enc_fb.c` (index latch), linear motors
- Status:  Known
- Symptom: Linear axis (60 mm electrical period) on a scale with a reference
  mark every 20 mm: absolute commutation from the first mark is right, then
  the axis loses force and trips POS_ERROR after the next mark (sim:
  `host/tests/linear_periodic_marks.test`).
- Cause:   enc_fb re-latches `absoffset` on every index pulse while
  `en_index` is set; only marks a whole electrical period apart land at the
  angle `conf0.mot_fb_offset` was measured for, the others are +-120 deg off.
- Fix:     Use a single (selectable) reference mark, e.g. the RGH22 reference
  actuator, or marks at a multiple of the electrical period. A firmware option
  to latch only the first index would also cover it.

## 2026-10-14  Sim: encoder drifted from the true position over long travel
- Area:    `host/hw/hv.c` (PMSM integrator)
- Status:  Fixed
- Symptom: Linear axis round trip to -485 mm and back: encoder at 0, carriage
  (`sim_abs_pos`) at +0.06 mm; reference marks moved against the counts.
- Cause:   `sim_pos` (what enc_fb counts) was a wrapped float sum at 80 kHz,
  `sim_abs_pos` a separate double sum; float rounding drifted between them.
- Fix:     `sim_pos` is now derived from the double position every step.

## 2026-10-14  rlpsij wanders on a limited stroke
- Area:    `shared/comps/rlpsij.c` (states 3-5)
- Status:  Fixed
- Symptom: Linear axis (60 mm period): the id moved the carriage 0..+208 mm,
  all one way; started near an end it would hit the stop.
- Cause:   State 4 always starts driving +, and its coasts are long on a low
  friction axis (~95 mm from 0.6 m/s); state 5 steps + first.
- Fix:     `rlpsij0.max_travel` (rad, 0 = off): segments head back past half
  of it, a coast heading away brakes, state 5 runs towards the start, abort
  beyond it. 100 mm limit: -39..+59 mm used, same results.

## 2026-10-14  No acceleration feedforward with LinuxCNC over sserial
- Area:    `conf/template/sserial.txt`, `conf/template/pid.txt`
- Status:  Known
- Symptom: Linear axis at 10 m/s^2: following error ~0.4 mm with bench gains
  even though j is exact; `pid0.ff_torque_cmd` holds only friction/damping.
- Cause:   sserial.txt sets `vel0.en = 0`; `pid0.acc_ext_cmd = vel0.acc` is
  then 0, so j * acc comes from the feedback loops. Enabling vel0 helps
  little (0.37 -> 0.30 mm): its acc is pos_error * w^2 of 1 kHz steps.
- Fix:     Open. Higher cur_bw/vel_bw (id_pid) gets it under 0.17 mm in sim. A
  proper fix is an acc from the commanded velocity (vel_int) or from LinuxCNC.


## 2026-10-17  Baldor linear motors: phase colours depend on the drive
- Area:    `docs/baldor_linear.md` (Baldor LMCF / LMBL, MN1800 cables)
- Status:  Note
- Symptom: (documentation) the same AY1763A00 cable lists two phase
  colourings: LinDrive/MintDrive (U red, V white, W black) and "trap"
  drives (U black, V red, W white).
- Cause:   Baldor matched the phase order to the Hall order of each drive.
- Fix:     On stmbl, find the phase order by scoping the back-EMF against
  Hall 1/2/3, not from the colours (docs/baldor_linear.md, "Bring-up
  checks").

## 2026-10-18  enc_fb: input filter set before the timer clock (fast scales)
- Area:    `src/comps/enc_fb.c` hw_init (F4, FB0 quadrature on TIM4 for v4.0)
- Status:  Fixed (compiles for the F4; not yet run on a drive)
- Symptom: none seen; found by reading the code. Relevant for fast linear
  scales: 1 µm at 4–5 m/s is 4–5 M counts/s.
- Cause:   Three problems:
  - TIM_ICInit (ICFilter = 0xF) ran before
    RCC_APB1PeriphClockCmd(FB0_ENC_TIM_RCC), so its writes were lost
    unless res.c had already enabled TIM4. The inputs ran unfiltered.
  - `TIM_Channel_1 | TIM_Channel_2` is TIM_Channel_2 (Channel_1 is 0), so
    TIM_ICInit only ever reached B.
  - Its BothEdge polarity sets CC2NP, which TIM_EncoderInterfaceConfig
    leaves set. Encoder mode must not use that.
  Just moving the clock enable would have applied a 3 µs filter to B
  only (0xF: fDTS/32, N = 8: about 0.65 m/s at 1 µm), in both-edges mode.
- Fix:     Enable the clock first and drop TIM_ICInit. After
  TIM_EncoderInterfaceConfig, write IC1F/IC2F in CCMR1 from the new pin
  `enc_fb0.input_filter` (set at load; default 3 = 8 samples at 84 MHz,
  ~0.1 µs; 0 = off). The counting direction (B inverted) is unchanged.
  Still to check on a drive: counts against distance at speed, and the
  direction.

## 2026-10-18  No emergency braking: every fault coasts the motor
- Area:    `shared/comps/fault.c`, F3 `stm32f303/src/comps/io.c` / `ls.c`
- Status:  Known
- Symptom: (code review, for linear axes at speed) a fault or a lost link
  leaves a moving axis to coast into its end stop.
- Cause:   Every fault (CMD_ERROR after about 5 rt cycles of lost
  smart-serial/CAN, a feedback error, over-voltage, ...) goes to
  SOFT_FAULT. That sets en_out = 0, and the F3 clears HV_EN: all switches
  off. The F3 does the same if the F4's packets stop (ls.c, after 5
  missed). mot_brake drops (the holding brake engages, if one is fitted;
  linear motors usually have none). Regen while decelerating can only
  clamp current (high_dc_volt) or trip HV_VOLT_ERROR. fault0.dc_brake is
  computed but used nowhere, and the F3's BRK output (io0.brk, with
  brk_present detection) is not linked by any template.
- Fix:     Ideas: a controlled quick stop on CMD_ERROR while the feedback is
  good; dynamic braking (all low sides on) when control is lost but the
  power stage works; a chopper on BRK driven by dc_brake; and, for a dead
  drive, an external normally-closed contactor shorting the phases through
  resistors, plus shock absorbers at the ends.

## 2026-10-20  Linear motors: conf0.j must be the whole moving mass
- Area:    drive config (`conf0.j`, `vel1` observer), `host/portal/machine/*.conf`
- Status:  Fixed (portal configs); Known for any linear-motor config
- Symptom: on the simulated portal the axes rang at 15-25 Hz while moving,
  with up to 2 mm of following error. Raising pos_bw above about 40 made it
  a limit cycle. Holding position was quiet.
- Cause:   `vel1.j = conf0.j`: the velocity observer predicts acceleration
  from torque / conf0.j. With j = the coil (1.5 kg on X) and the load in
  j_sys, it expects 8 x too much acceleration; its velocity lags and
  overshoots (23.6 vs 25.9 rad/s seen). On a rotary motor j is the rotor
  and the load hangs on a coupling; a linear motor's coil is the load.
- Fix:     j = the whole moving mass (as rotary: m (p/2pi)^2), j_sys = 0.
  Cruise error dropped from about 5 to 0.4 um rms, and stable gains went up
  to pos_bw 150 / vel_bw 1000 on a rigid load.

## 2026-10-20  A loop that rings in motion can hold quietly (friction damps it)
- Area:    servo tuning, `host/portal`
- Status:  Note
- Symptom: gains with a 20 Hz limit cycle in motion held position within
  2 um at standstill.
- Cause:   the guides' Coulomb friction acts as very heavy damping around
  zero speed; in motion it is a constant force and damps nothing.
- Fix:     judge tuning with the axis moving (constant speed and after
  acceleration steps), not on a held position.

## 2026-10-20  No acceleration feedforward over smart-serial by default
- Area:    `conf/template/sserial.txt` (`vel0.en = 0`)
- Status:  Known
- Symptom: following error during acceleration of a/(pos_bw vel_bw)
  (1.2 mm at 0.9 g, pos_bw 25, vel_bw 300).
- Cause:   pid0.acc_ext_cmd comes from vel0 (the derivative of the
  command), which the sserial template disables.
- Fix:     `vel0.en = 1` in the config gives it (the portal's configs do).
  It is filtered (vel0.lp 50 Hz) and helps about 25 %; feedforward of the
  planner's own acceleration would help more.

## 2026-10-20  Gantry: racking limits the Y gains
- Area:    `host/portal` (Y1/Y2 as separate drives)
- Status:  Known
- Symptom: Y gains above vel_bw 300 / pos_bw 60 made the bridge oscillate
  in yaw (0.6-1.3 mrad); without the guides' yaw stiffness it ran away.
- Cause:   each Y drive is tuned for its 21 kg share of the bridge. In the
  racking mode a motor moves only the bridge's yaw inertia over (L/2)^2,
  about 5 kg: the same gains are 4 x too high there.
- Fix:     tune Y for the racking mode, or a cross-coupled gantry loop
  (mean and difference of Y1/Y2 controlled separately).

## 2026-10-20  sampler.0.enable defaults to on: halsampler records from startup
- Area:    `hwtest/portal-sim/portal.hal`
- Status:  Fixed
- Symptom: the benchmark's samples started at LinuxCNC's start: 500 mm of
  "following error" (command 0 before homing) and a program time that
  included the startup.
- Cause:   `sampler.N.enable` is true when loaded.
- Fix:     `setp sampler.0.enable 0` in the HAL file; portal_run.py sets it
  while a program runs. Also: axis.x.pos-cmd read 0 here, so the HAL file
  records joint.N.motor-pos-cmd (trivkins, absolute scales: the same).

## 2026-10-20  Headless linuxcnc: its output goes to /tmp/linuxcnc.print.*
- Area:    LinuxCNC's `scripts/linuxcnc`, `hwtest/portal-sim/bench.py`
- Status:  Note
- Symptom: `linuxcnc portal.ini > log` exited 255 with an empty log.
- Cause:   without a terminal the script redirects itself into
  /tmp/linuxcnc.print.* / linuxcnc.debug.* and deletes them at exit. On an
  error without X it copies them to ~/linuxcnc_print.txt and
  ~/linuxcnc_debug.txt (here: a pin already on a signal in portal.hal).
- Fix:     read ~/linuxcnc_debug.txt after a failed headless start.

## 2026-10-20  Acceleration leads: which way each one points
- Area:    `host/portal/pcomp.c` (command conditioning)
- Status:  Note
- Symptom: two of the three leads made the path worse at first (a circle
  shrank further: -401 instead of -236 um).
- Cause:   sign. A smoothing filter, and a servo that lags under
  acceleration, behave like an average along the path: they end up
  displaced towards the acceleration (inside a curve), s = x + s2/2 a. The
  head's lag is inertial: it bends against the acceleration.
- Fix:     out = s + a (1/w^2 - tau2 - s2/2). Leads also need a smooth
  acceleration: on LinuxCNC's stepping acceleration (even after a ZV
  shaper) they jump the command by K da (0.5 mm) and double the current;
  a moving average after the shaper turns the steps into ramps.

## 2026-10-20  halsampler stopped mid-line: numpy.loadtxt rejects the whole file
- Area:    `hwtest/portal-sim/bench.py`
- Status:  Fixed
- Symptom: every run but the first per LinuxCNC session scored "no samples".
- Cause:   portal_run.py terminates halsampler, which leaves a partial last
  line; loadtxt raises, and the analysis read that as an empty file.
- Fix:     keep only rows with all the columns.

## 2026-10-20  A command filter between motion and the drives: three traps
- Area:    `linuxcnc/pcomp` (any component that changes the command after motion)
- Status:  Fixed
- Symptom: LinuxCNC with pcomp: amplifier faults at machine on; following
  error aborts out of rapids, even at 0.25 g (1.1 mm, proportional to v).
- Cause:   three, one after the other:
  1. Wind-up. With the drives off, motion makes its command follow the
     feedback. pcomp hands back fb + (in - out), and through the filter's
     delay that loop integrates; at startup (absolute scales: 0 -> 500 mm)
     it wound up.
  2. Stale velocity. A backward difference of the output is half a servo
     period late: v dt/2 of drive lag (0.8 mm at 1.6 m/s).
  3. Feedback pairing. pcomp reads the drives' feedback before they run (it
     shows where they got to with pcomp's previous output), and motion
     compares it with this period's command. fb + (in - out) of the same
     period, or of the period before, both leave v dt (1 mm at 1 m/s).
- Fix:     1. enable = the amplifiers' enable; restart at standstill.
  2. the shaped path's central difference (one period back) + the lead's
  rate. 3. fb_out = fb + (in now - out the period before).
  The servo lag constant tau2 had to be recalibrated after 2 (1.45e-5 ->
  2.15e-5 s^2): the stale velocity had been part of it.

## 2026-10-21  Velocity observer lags under acceleration (2 d a / w); vel_bw ceiling 556
- Area:    `shared/comps/vel.c` (vel1: pid0.vel_fb), every drive
- Status:  Workaround (opt-in `vel1.bc = 1`; default unchanged)
- Symptom: the portal and the linear axis held a position error
  proportional to acceleration that tuning couldn't remove; acceleration
  feedforward made circles worse; vel_bw above about 550 was unstable on
  every load; the vel_bw a config said wasn't the loop's real bandwidth.
- Cause:   the observer's torque model only acts above `lp` (50 Hz). A
  steady acceleration a is tracked with an error a/w^2, and the reported
  velocity (acc_sum, without the proportional correction 2 d w e) lags the
  true one by 2 d a / w (1.8 ms x a at w 1000, d 0.9). In the speed loop
  that is positive acceleration feedback: the effective inertia is
  j (1 - 2 d vel_bw / w), so the real bandwidth is vel_bw / (1 - 2 d vel_bw / w)
  (650 for a configured 300), unstable at vel_bw = w / 2d = 556. It also
  stands in for missing acceleration feedforward, which is why feedforward
  alone looked harmful.
- Fix:     `vel1.bc = 1` adds the correction to the reported velocity
  (`vel1.bc_lp` [Hz] filters it against quantization). Use it together with
  acceleration feedforward (`vel_int0.mode = 1`) and retune: vel_bw is then
  the real bandwidth. Portal: following error 115 -> 7 um at 0.7 g;
  linear axis at 2 m/s^2: 3.7 -> 0.5 mrad (`tests/acc_ff_linear.test`). With
  bc on by default the host suite's motors all still pass; only configs
  tuned with the lag in place (portal, two arm detector thresholds) move,
  so it stays opt-in.

## 2026-10-21  LinuxCNC's vel-cmd is half a servo period late; acc feedforward over sserial
- Area:    `shared/comps/vel_int.c`, `conf/template/sserial.txt`
- Status:  Fixed (opt-in `vel_int0.mode = 1`)
- Symptom: velocity feedforward lagged; no acceleration feedforward over
  smart-serial (vel0 is off in sserial.txt; on, it is a filtered derivative).
- Cause:   joint.N.vel-cmd is the average over the last period. LinuxCNC's
  planner moves with constant acceleration between updates.
- Fix:     mode 1: velocity and acceleration from the last three positions
  (exact for the planner: a = (d1 - d0)/T^2, v = d1/T + a T/2, T =
  1/cmd_freq), quadratic interpolation, acc_out -> rev0.out_dd ->
  pid0.acc_ext_cmd. Mode 0 passes vel0.acc through, as before.

## 2026-10-21  vel_int mode 1: a jump while standing kicks the next move (POS_ERROR)
- Area:    `shared/comps/vel_int.c` (mode 1)
- Status:  Fixed
- Symptom: on some portal origins (X 400 and 450, not 420 or 500) the
  first move faulted with POS_ERROR at once; the drive ran backwards.
- Cause:   standing still, LinuxCNC's packets repeat and aren't "new", so
  the history p2 kept whatever came before the stop. After a jump in the
  command while stopped (here portal_sim's unsynced first command, the raw
  scale, then the offset one), the next move's first packet computed
  a = (d1 - jump) / T^2, about 2.7e6 rad/s^2.
- Fix:     a stop (v = 0 or no change) sets p2 = p: the history is
  "standing at p". `host/tests/acc_ff_jump.test` (fails before).

## 2026-10-21  portal_sim sent an unsynced command before the scale offsets were set
- Area:    `host/portal/portalcore.c` (`ptcore_set_cmd`)
- Status:  Fixed
- Symptom: as above; with mode 0 harmless.
- Cause:   until the offsets were known (5 exchanges), the command was the
  raw machine position (400 mm), then about 0: a jump LinuxCNC never sends.
- Fix:     until synced, each drive gets its own feedback as command (as
  LinuxCNC follows the feedback while disabled).

## 2026-10-21  portal files: `cogging` lines never parsed; fault_code crashed without a Y2 drive
- Area:    `host/portal/portalcore.c`, `host/portal/portal_sim.c`
- Status:  Fixed
- Symptom: `cogging y1 28 22.86 30` -> "bad line"; `print fault_code` on a
  file with only `drive y2 none` segfaulted.
- Cause:   the number parser started at the motor's name (n = 0 numbers);
  fault_code called the unloaded drive's `pin()` (NULL).
- Fix:     parse the numbers after the name; skip drives that never
  loaded. `host/portal/tests/portal_retrofit*.test`.

## 2026-10-21  Gantry: Y1 and Y2 fight at 160-235 Hz in the bridge's yaw
- Area:    gantry drive tuning (`conf0.vel_bw`, `conf0.cur_bw`), `host/portal/machine/retrofit`
- Status:  Fixed in simulation with the gantry link (`shared/comps/gantry.c`);
  Workaround on hardware until the drive-to-drive link exists (cur_bw 2000
  or a lower vel_bw)
- Symptom: Y1 and Y2 currents opposite, +-4 to +-7 A at 160-235 Hz, while
  moving (laser3015, cmm) or after reversing at slow speed (pcbdrill); with
  one Y motor, against the bridge's twist (dispenser). Masses, modes, the
  portal's own machine fine.
- Cause:   each Y drive is tuned for its share of the moving mass; in the
  yaw (Y1 against Y2) it moves only 2 J / L^2 of the bridge (18 of 73 kg on
  the laser, 4.7 of 21.5 kg on the driller): the velocity loop is 4-5x as
  fast there and meets the default current loop (1000 rad/s). Guide
  friction damps it on some machines (it appeared without friction on the
  laser: 11.5 A).
- Fix:     `conf0.cur_bw = 2000`, or `conf0.vel_bw` 200-300. dynwiz's
  `mass` and `cogging` steps detect it (a sustained line in the current)
  and `write` proposes vel_bw x 0.6. The real fix: the gantry link (next
  entries): the two Y drives exchange position error and velocity, and
  each feeds its pid the yaw at r_vel ~ m_yaw / m_side of the velocity
  gain. The fight is gone at the default gains on laser3015, cmm and
  pcbdrill (cmm: 10 A at 208 Hz -> 0.01 A, rack 118 -> 2.4 urad), and
  vel_bw can double. `host/portal/tests/portal_gantry*.test`.

## 2026-10-21  Brake chopper above the drives' max_dc_volt never helps
- Area:    `host/portal` `bus` lines, real supplies
- Status:  Note
- Symptom: HV_VOLT_ERROR at 2 g with a chopper set to switch on at 390 V.
- Cause:   `conf0.max_dc_volt` is 380 (conf/template/conf.txt): the
  drives trip before the chopper conducts.
- Fix:     chopper on below it (360 V in the retrofit files); on a real
  machine, check the resistor module's threshold against max_dc_volt.

## 2026-10-21  Moving mass from the current: a ringing head biases a per-sample fit
- Area:    `linuxcnc/dynwiz/dynident.py` (`mass_fit`)
- Status:  Fixed
- Symptom: X masses 14 % high (cmm) or 18 % low (pcbdrill); dynwiz would
  have proposed a wrong conf0.j.
- Cause:   the head swinging on its springs adds a force that, over the
  short accelerations (36 ms at 2 g), sits at whatever phase it has.
- Fix:     fit the impulse: both sides summed over each move's samples;
  the head's part then is its momentum, which stays small. All five
  simulated machines within 3 %.

## 2026-10-21  Step-response modes: the base and the servo look like the head
- Area:    `linuxcnc/dynwiz` (`modes`)
- Status:  Workaround
- Symptom: the strongest ring after a step was 17 Hz at the bridge's ends
  (pcbdrill: its frame on its feet) or 15 Hz everywhere (dispenser: the
  position loop's own settling, pos_bw 100 rad/s), not the head.
- Cause:   the scales are on the frame and only see the head through its
  reaction on the servo-held carriage; a light, stiff head (1.2 kg at
  85-140 Hz on 1.8 kg) barely shows at all.
- Fix:     the three strongest peaks per position; a frequency within 3 %
  at every position is the base's and is left out; the current is used
  where it is clearer than the scale; a fit that doesn't bend like a bridge
  is marked doubtful and not written; `--use-tool` for a sensor on the head.

## 2026-10-21  HAL sampler: a second instance needs its own depth
- Area:    LinuxCNC `sampler` (2.9)
- Status:  Note
- Symptom: `loadrt sampler depth=20000 cfg=...,...` then `addf sampler.1`:
  "function 'sampler.1' not found".
- Cause:   depth is per instance; an instance without one isn't created.
- Fix:     `depth=20000,20000`.

## 2026-10-21  Gantry link: the partner's data must arrive within ~1 ms
- Area:    `shared/comps/gantry.c`, `host/portal` (`glink DELAY EVERY`)
- Status:  Known
- Symptom: with the link's latency at 2 ms the fight came back at the
  bridge's middle; at 3 ms (a relay through LinuxCNC and smart-serial at
  1 kHz) it is as bad as without, SAT_ERROR at 4 ms.
- Cause:   the partner's velocity feeds the fast loop, where the fight is
  (200 Hz: 2 ms is 144 degrees).
- Fix:     a drive-to-drive link, not LinuxCNC: classic CAN between the two
  Y drives (stmbl v4 P10: CAN1 without a transceiver, can-support branch),
  an 8-byte frame each way every 400 us (68 % of 1 Mbit/s), ~600 us
  latency: simulated as `glink 3 2`, stable. Stability with the link: the
  yaw's velocity bandwidth vel_bw r_vel / (m_yaw / m_side) up to about
  1.8 cur_bw.

## 2026-10-21  Gantry: what didn't help (inertia split, load-split lead)
- Area:    `pid0.j_mot / j_sys / j_lpf`, pcomp
- Status:  Note
- Symptom/Cause: (1) the pid's frequency split of the inertia (j_mot = the
  yaw's mass, j_sys the rest, above j_lpf only j_mot) can't tell the yaw
  from the common mode, and its low-pass sits at the speed loop's crossover:
  worse, POS_ERROR. (2) A per-side lead in pcomp for the carriage's load
  split (Y1/Y2 apart by G (X - x0)/L a): rack grew with either sign. The
  stiff Y drives already hold the quasi-static split (16 urad at 1 g with
  the carriage off-centre); the rack left is dynamic.
- Fix:     none needed; the gantry link instead. Removed from pcomp.

## 2026-10-26  Portal: the linear motors run out of bus voltage at 1.3 m/s
- Area:    host/portal (claude/portal-dynamics), host `hw/hv.c` (the F3's voltage limit)
- Status:  Open (simulation; no portal hardware)
- Symptom: Merging claude/portal-dynamics into master: portal_move's tool
  path error 177 um (limit 100, the branch 60), the frame moving 562 um (114);
  portal_racking's Y1 current 1.06 x (limit 1.05); portal_momentum's second
  move ends elsewhere. Marked @todo.
- Cause:   Master's simulated F3 limits the output voltage vector to what the
  bus can make (vbus / sqrt 3); the branch's limited ud and uq one by one, up
  to 1.34 x that. The Baldor LMCF08F at 1.3 m/s needs ~130 V back-EMF plus
  R i (10.5 ohm) against 188 V from a 325 V bus: the moves are voltage
  limited. Without the vector limit the merge reproduces the branch's numbers
  (60 um, 114 um). portal_momentum faults (POS_ERROR) on its second move on
  both; it only checks momentum and frame travel, and master brakes after the
  fault where the branch coasted.
- Fix:     Not yet: slower or lower-acceleration moves, a higher bus (a PFC
  front end, 400 V), or motors with a lower Kf. Then drop the @todo lines.
- Also:    linuxcnc/dynwiz `make test-sim` (not in host check): 4 of 8 fail
  on the branch itself on the bench Pi (dispenser: "the scales can't see the
  head"; pcbdrill, write_confs: "math domain error"; write); after the merge
  test_portal fails instead of test_write ("math domain error", likely the
  same voltage limit). `make test` (dynident) 6/6.

## 2026-10-26  sserial pos_advance is 0.2 ms long since idx_home runs before linrev
- Area:    `conf/template/sserial.txt` (sserial0.pos_advance 0.0008), idx_home rt_prio
- Status:  Open (needs the bench)
- Symptom: The portal simulation's following error (LinuxCNC's command
  minus the drive's reported position) grew from ~8 to ~228 um at 0.7 g on
  master; the motion itself was the same.
- Cause:   sserial0.pos_fb is extrapolated by pos_advance (0.8 ms) for the
  link's latency. Since idx_home0.rt_prio went to 2.05 (2026-10-17, index
  homing) linrev reports the position one rt cycle (0.2 ms) fresher, so the
  same advance now leads by 0.2 ms: at 1 m/s that reads as 0.2 mm.
- Fix:     0.0006 restores the portal's numbers in simulation. Not changed:
  it alters every drive's reported position; check it on the bench first
  (hwtest soak's ferror_max at 500 mm/s should drop by ~0.1 mm).
- Also:    2026-10-27, the simulated CN 720 (host/mill, ball screws, SEW
  CMP40M) merged with master: mill_move's following error 16.9 / 17.2 um
  against 15 (the branch: under 15); with pos_advance 0.0006 10/10. Marked
  @todo there too.

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


## 2026-10-26  rlpsij: a light rotor with a slow l/r gets j and friction wrong
- Area:    shared/comps/rlpsij.c (state 4), host/tests/motor_sew_cmp40m.test
- Status:  Fixed
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

## 2026-10-27  mill_sim: a 2 kHz servo thread looked jittery, the harness wasn't
- Area:    host/mill/mill_sim.c (servo_period, tool_mm)
- Status:  Fixed
- Symptom: at 2 kHz the tool seemed to ripple 5.3 um rms against the command
  at 6.3 m/min (1 kHz: 0.3 um), exactly v x 100 us / 2.
- Cause:   the harness measured the tool after 2 or 3 rt cycles (0.5 ms is 2.5
  of them) but compared it with the command at the nominal time; it also
  handed the drive each packet at the rt cycle before its send time.
- Fix:     packets arrive `latency` (50 us) after their send time, taken at
  the next rt cycle; the tool is moved back along its velocity to LinuxCNC's
  time. The real ripple: 0.27 um at 1 and 2 kHz. A vel_int option that puts
  the packets on the servo grid changed nothing and was dropped.

## 2026-10-27  mill_sim: a second feedback read a cycle late
- Area:    host/mill/mill_sim.c (tick: enc_fb1.sim_pos)
- Status:  Fixed
- Symptom: with a linear scale blended into the loop, a Y rapid at 27 m/min
  ended 85 um off; the scale lagged the motor by 0.056 rad at speed.
- Cause:   hv0 moves the simulated rotor over the rt cycle before enc_fb0
  reads it (the drive sees the end of the cycle); the harness gave enc_fb1
  the load's position at the start: 200 us apart.
- Fix:     the scale is read at the end of the cycle too (s + v RT). Any
  simulated second feedback needs the same.

## 2026-10-27  Ball-screw axes: the screw sets the velocity bandwidth
- Area:    host/mill (CN 720 model), pid0.vel_bw
- Status:  Note
- Symptom: vel_bw 800 rad/s rings, 1200 trips (Y: the 40 kg table on about
  45 N/um, motor side ~20 kg equivalent).
- Cause:   the screw-load mode between motor and table.
- Fix:     vel_bw 500, pos_bw 60 (host/mill/machine/x.conf ...). Retune with
  the real masses; tap-test the screws' modes.

## 2026-10-27  A 10 um scale in the position loop hunts; blend it instead
- Area:    shared/comps/fb_switch.c (joint_tc), host/mill
- Status:  Note
- Symptom: position loop on a 10 um scale: contours 2-3 x worse than on the
  resolver, the tool 3-4 um off at rest (it hunts within a count).
- Cause:   the scale's quantization in a loop of pos_bw 60 and the screw's
  wind-up between the motor and the scale.
- Fix:     fb_switch0.joint_tc > 0: the loop on the motor's feedback, pulled
  to the scale over joint_tc. 0.2 s: a warm screw's 38 um error -> 4 um,
  circles 3.7 um rms; 3 s: circles as the resolver's, slow on lead error.
  Lead error belongs in LinuxCNC's screw compensation, measured once with
  the scale; the blend then only follows temperature.

## 2026-10-27  halsampler's first line can be "overrun"
- Area:    hwtest/mill-sim/mill_test.py (any halsampler recording)
- Status:  Workaround
- Symptom: numpy.loadtxt failed on every program's recording but the first;
  the runs looked like they had no samples.
- Cause:   halsampler prints "overrun" when the sampler's FIFO filled before
  it read (samples from before the enable); it is a line of text first.
- Fix:     read only the rows with all the columns, and count gaps in the
  sample numbers while cutting (mill_test.py fails a run with gaps).

## 2026-10-27  stmbl_mill: the drives' cmd_freq follows the servo thread
- Area:    host/mill/millcore.c (mccore_servo), real drives' configs
- Status:  Note
- Symptom: a 2 kHz servo thread with vel_int0.cmd_freq left at 1000 makes
  the drive's command interpolation (vel_int mode 1) take the packets for
  twice as far apart: its velocity comes out half, its acceleration a
  quarter.
- Cause:   cmd_freq is a setting, not measured.
- Fix:     the simulation sets it from the period; on a real drive put
  vel_int0.cmd_freq = 2000 in the config for a 2 kHz thread.

- Cause:   not the l/r (that guess was wrong: the drive current adapts down
  to 0.32 A and segments last ~40 ms). Each segment lasted between SEG_MIN
  and 4 * SEG_MIN, so drive_cur never changed and state 4 settled into a
  limit cycle of identical segments: the momentum fit had only two distinct
  equations (drive, coast, mirrored) for four unknowns, and |v| always
  within max_vel / 2..max_vel made friction and damping 0.994 correlated.
  It traded friction into damping; state 5 then used that friction as
  feedforward, the rotor stuck at test_vel / 10, and read friction low too.
- Fix:     every other pair of drive segments stops at max_vel / 2 (only
  full-speed segments adapt drive_cur). j +5 %, friction +2 % (was +24 %,
  -55 %); the other motors' friction errors shrink (rx60_456 -9 -> +1 %,
  p50b -8 -> -0.5 %). Test: motor_sew_cmp40m.test (no longer @todo).

## 2026-10-26  rlpsij state 5: a start-up kick winds the speed integrator
- Area:    shared/comps/rlpsij.c (state 5)
- Status:  Fixed
- Symptom: with the state 4 fix above alone, p50b, weiler_x and
  syn_servo_2kw read friction 25-60 % low; which motors fail depends on
  the direction state 4's brake ends in.
- Cause:   in the simulated drive the current loop barely follows the brake
  (iq ~0 for a -1.5 A command) and overshoots when state 5 starts (0.9 A for
  a 0.1 A command): the rotor kicks to ~20 rad/s, and the speed integrator
  (gain jq * vel_bw^2 / 4, slow) winds ~-0.13 A while it slows down. At
  test_vel / 10 the rotor then stands still with negative current, and the
  phase reads a wrong friction current.
- Fix:     state 5 integrates the speed error only within test_vel / 2 of
  the command or below test_vel / 2 (breakaway). Tests:
  motor_p50b.test, motor_weiler_x.test, motor_syn_servo_2kw.test fail
  without it (with the state 4 fix in).

## 2026-10-26  make check: test_step2cell.py needs libGLU (gmsh)
- Area:    host/tests/robot/test_step2cell.py, cloud containers
- Status:  Note
- Symptom: `make -C host check` stops at test_step2cell.py with
  "OSError: libGLU.so.1: cannot open shared object file"; the .test suite
  after it never runs.
- Cause:   the gmsh Python package loads libGLU, which the container lacks.
- Fix:     install libglu1-mesa, or run the remaining check steps by hand
  (`make -s -C arm all; make -s -C portal all; python3 tests/run_tests.py
  tests/*.test tests/ebrake/*.test tests/bus/*.test arm/tests/*.test
  portal/tests/*.test`).

## 2026-10-18  idpmsm: pole pairs wrong on heavy rotors
- Area:    `shared/comps/idpmsm.c` (state 2.2)
- Status:  Fixed
- Symptom: setup wizard over 35 simulated motors: 5 got 9-12 pole pairs
  (syn_spindle, syn_servo_2kw, manutec_r15_1, p50b, weiler_x).
- Cause:   the rotating field started at full test_vel; a rotor heavy for
  the test current can't pull in and swings; pp was a filtered ratio of
  instantaneous speeds, biased by the swinging.
- Fix:     the field ramps up over 1 s; pp = the field's travel / the
  rotor's over the last 2 s. All 35 right.

## 2026-10-18  idpmsm: the flux step spun light motors to the voltage limit
- Area:    `shared/comps/idpmsm.c` (state 3.2), `conf/template/id_pmsm.txt`
- Status:  Fixed
- Symptom: manutec_r15_1 (j 3.5e-4, 0.8 A) ran up to 700 rad/s (6700 rpm)
  during the psi measurement; with a wrong conf0.polecount it kept being
  driven with no current command.
- Cause:   (1) the speed loop was a slow integrator (error limited to
  test_vel / 100): huge overshoot. (2) id_pmsm left hv0.vel on the normal
  commutation chain, scaled by conf0.polecount (still a guess there): the
  F3 advanced the angle by it between packets and spun the field itself.
- Fix:     bang-bang: test_cur below test_vel, no current until 0.7
  test_vel; psi = the integral of uq - r iq over the rotor's travel while
  coasting (vel1 reads ~1 % high while slowing: 2 % in psi). The template
  sets hv0.vel = 0. drive_id_pmsm.test: psi -1.3 % (2 % tolerance).

## 2026-10-18  idm: micro motors saturate; inertia high with heavy friction
- Area:    `shared/comps/idm.c`, the setup wizard's mechanical step
- Status:  Workaround (inertia), Fixed (micro motors)
- Symptom: syn_micro (j 4e-7) faulted SAT_ERROR in id_mot. Light motors with
  friction large against the inertia's torque read j 1.3-7x high.
- Cause:   idm's adaptive inertia (which sets the loop gains) had a floor
  of 5e-6: gains 12x too high. At idm's 25 rad/s the friction's sign
  changes often and the least-squares fit puts the misfit into j.
- Fix:     floor 1e-7 and the wizard starts the inertia low; the wizard runs
  idm twice, the second time at 30 % of peak torque for the first (high)
  estimate and faster: within 25 % except fanuc_b05_6000 (2.3x) and
  sgmph_01a1 (2.4x), kept as TODO in host/tests/run_wizard_sim.py.

## 2026-10-18  The console prints pins with 6 decimals
- Area:    `shared/hal.c` (hal_print_pin: %f)
- Status:  Workaround
- Symptom: a micro motor's inertia (4e-7) read as 0.000000; 4.4e-5 as
  0.000044 (2 significant digits).
- Cause:   %f.
- Fix:     new console command `getf <pin>` (%.9g), used by the setup
  wizard. The normal print is unchanged (servoterm and scripts read it).

## 2026-10-18  idpmsm: inductance high when L/R is a few rt cycles
- Area:    `shared/comps/idpmsm.c` (L step)
- Status:  Known
- Symptom: r2aa04003f (L/R 55 us) L 2.7x high, sr60_4 (40 us) +15 %.
- Cause:   the square-wave L step resolves the current slope over a few
  5 kHz samples; with L/R near the period the slope isn't linear.
- Fix:     none yet; the wizard warns when L/R < 50 us.

## 2026-10-18  setup wizard: notes
- Area:    `servoterm-cli/setup/`, docs/setup_wizard.md
- Status:  Note
- Symptom: -
- Cause:   -
- Fix:     Gotchas found building it: idpmsm resets r/l/psi/pp to defaults
  when disabled (state 0): read results before disabling. A pin query
  prints every pin with that prefix (conf0.r also prints conf0.rt_prio):
  match the whole name. The scope stream (term0.send_step) fills the
  buffers while the wizard waits for the user: it sets send_step = 0. The
  host simulator's sim_board links the simulated motor to conf0, so
  identification tests need a --sim file with fixed values.

## 2026-10-18  A vertical axis drops while the holding brake closes
- Area:    `shared/comps/fault.c` (mot_brake, en_out)
- Status:  Fixed
- Symptom: burn-in axis, a 50 ms brake, 0.5 Nm hanging load: disabled at
  standstill it fell 18.7 rad (3 turns) before the brake gripped.
- Cause:   the outputs went off in the same cycle as the brake command;
  brakes take tens of ms to grip.
- Fix:     fault0.brake_time: the brake grips while the quick stop (or the
  dynamic brake) still holds the axis, then the outputs go off. Default 0
  (old behaviour); the setup wizard sets it and raises it until there is no
  sag. tests/ebrake/estop_brake_time.test.

## 2026-10-18  fb_switch0.pos_fb stands still while the drive is disabled
- Area:    `shared/comps/fb_switch.c` (fault0.en_fb)
- Status:  Known
- Symptom: the wizard's brake step saw no movement with the drive off
  although the axis fell 3 rad.
- Cause:   fb_switch only updates with en_fb.
- Fix:     read the feedback's own pin (enc_fb0.pos, res0.pos, ...) to watch
  a disabled axis.

## 2026-10-18  "The motor moves when enabled" doesn't prove the brake releases
- Area:    setup wizard, brake step
- Status:  Note
- Symptom: a brake on the wrong output (never released) passed a 0.3 rad
  move test.
- Cause:   the motor's peak torque (3x rated) is above a typical brake's
  (1.5x rated): it drags the closed brake.
- Fix:     the release test is by hand with the drive off and
  fault0.brake_release = 1: the shaft must turn freely.

## 2026-10-27  Motor temperature sensor: the HV board's reading went nowhere
- Area:    shared/comps/mtemp.c, conf/template/{pid,mpid,uf}.txt, setup wizard
- Status:  Fixed
- Symptom: a motor's KTY / PTC on the HV board's temperature connector
  protected nothing: fault0.mot_temp was linked to iit0.temp, the thermal
  model; hv0.mot_temp (the sensor's resistance in ohms) was only shown.
- Cause:   no conversion from ohms to degC, and no sensor type setting.
- Fix:     mtemp0 converts for conf0.mot_temp_type (0 none, 1 KTY84-130,
  2 PT1000, 3 PTC, 4 NTC, 5 NC switch); fault0.mot_temp = max(sensor,
  model); open / shorted wire, tripped PTC / switch stop the drive.
  Default 0 keeps the old behaviour. The wizard asks the type and checks
  the cold reading (setup api 2).

## 2026-10-27  The motor temperature input is on the HV side
- Area:    HV board, motor temperature connector; SEW CMP feedback cables
- Status:  Known
- Symptom: none yet; a wiring hazard. SEW (and others) run the KTY pair in
  the feedback cable next to the resolver.
- Cause:   the F3 reads the sensor (stm32f303/src/comps/io.c), on the HV
  side at the DC bus' potential; the resolver / encoder is on the LV side.
  Moved to the HV board, the KTY pair sits at bus potential beside LV
  pairs, separated only by the wires' insulation.
- Fix:     keep the KTY pair's ends insulated and away from the feedback
  RJ45 (the wizard's wiring text says so). Not checked on hardware: a
  bench reading of the input (ohms vs a resistor) is still to do.

## 2026-10-27  Wizard: a bad temperature sensor faulted the drive before the check
- Area:    servoterm-cli/setup/su_steps.c (step_base)
- Status:  Fixed
- Symptom: with an open sensor wire or the wrong type, the base step said
  only "The drive reports fault 7 with the first config."
- Cause:   a broken sensor reads as trip + 10 degC, so fault0 stops the
  drive (MOT_TEMP_ERROR) within 5 s; the fault check ran first.
- Fix:     the sensor check runs before the fault check and says which
  (open, short / another type, implausible reading).

## 2026-10-27  HAL_MAX_COMPS (32) full: a test's second extra comp didn't load
- Area:    shared/hal.h; host/tests/motor_*.test (generated)
- Status:  Fixed
- Symptom: after mtemp joined the pid template, 23 motor tests failed with
  "scale1.out0 was never printed": `load scale` twice, the second missing.
- Cause:   the drive config already used 30 of 32 comp instances; mtemp
  made 31, the tests' two scales 33. `hal` shows it: "HAL_MAX_COMPS 31/32".
- Fix:     HAL_MAX_COMPS 40 (~0.5 KB RAM on the F4). A user's config
  loading extra comps would have hit the same limit.

## 2026-10-27  call_cmd: a long command argument overflowed the stack
- Area:    `shared/commands.c` (call_cmd), every console (drive, F3, host, vibration sensor)
- Status:  Fixed
- Symptom: tests/vib/node.test failed on the bench Pi (32-bit ARM), also on
  claude/mill-webui alone: the sensor never took its node number; on x86 it
  passed.
- Cause:   call_cmd split a line with sscanf(" %[a-zA-Z_0-9] %[ -~]") into
  64-byte buffers, unbounded: the SET frame's 67-character argument ran past
  `a` on the stack. Where it landed decided whether anything broke.
- Fix:     bounded fields (%63, %127; `a` is 128 bytes). Test:
  tests/vib/node.test (fails on ARM without it).

## 2026-10-28  The console showed no motion while homing, then a jump
- Area:    webui/mill/server/machine.py (status: pos, mach)
- Status:  Fixed
- Symptom: Homing on the bench GUI: the positions stood still (0) while
  the axes homed, and jumped to the home positions at the end.
- Cause:   millweb showed LinuxCNC's axis positions (stat.actual_position),
  which LinuxCNC's motion updates only once every joint is homed; homing
  runs in joint mode, so they stood still until the last joint was done.
- Fix:     until every joint is homed each axis shows its joint's position
  (stat.joint_actual_position; a gantry's first joint). Each axis still
  steps once when it is homed: from where it started (0) to machine
  coordinates. Test: test_millweb.Protocol.test_positions_follow_the_joints_until_homed.

## 2026-10-28  Bench: the tool setter never tripped; the 3D view drew it elsewhere
- Area:    webui/mill/server/machineconf.py (simulated probe), bench/mill.hal, test/sim/mill.hal; machine.py (hello), js/app.js, js/live.js
- Status:  Fixed
- Symptom: Measure on the Tools page went down to Z -215 over the setter at
  X 250 Y 20 and stopped "without making contact"; the 3D view showed the
  setter at X 980.
- Cause:   (1) the simulated probe compared joint.N.motor-pos-cmd (the motor's
  count from where it started) with its trip height, not the machine
  position: Z's start sat 20 mm below the switch, so at the bottom of travel
  the motor read -195 and the tip never got below it; with random starts
  (sim_at random) anything. (2) The console only learnt *whether* there is
  a setter; the 3D models put it at fixed places (X 980 Y 20, or the bridge
  table's corner).
- Fix:     the probe takes joint.N.pos-fb (machine coordinates); hello's
  machine.toolsetter is {x, y, z_above, z_ref, travel} and both models and the
  demo place the setter there. On the bench: trips at Z -195.0 over X 250 Y
  20 (T1 50 mm). Tests: test_hello_and_static (the setter's position),
  backend_suite's hello check; the probe wiring by hand (no simulated-probe
  test with an offset start yet).
- Then:    2026-10-28, the simulated probe touches only the stock and the
  setter: the stock's box from the 3D setup's workpieces (millweb sets
  probe-sx/sy/sz at 2 Hz), the setter within 10 mm of its X Y at z_ref, and
  the tool tip from each tool's true length (SIM_TOOLS / the table's when
  first used), not the active offset (Measure cancels it). Bench: T1 62.408
  (true 62.415), T3 48.123 (48.13), over the stock Z -195.03, beside it no
  contact. Tests: backend_suite "the simulated probe touches the setup's
  stock and the tool setter, nothing else", "the tool setter measures"
  (T3 51.25, its table 48.13), test_sim_probe, test_sim_probe_stock_box_and_tool_lengths.

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

## 2026-10-29  A new drive's erased config flash: servoterm-cli --load aborted
- Area:    src/config.c (showconf, flashloadconf, conf_flash_text), host/drive.c, shared/confident.h (conf_text)
- Status:  Fixed
- Symptom: The first config load into a new drive (blank F4, flashed over
  DFU): "no ack for line 1 (appendconf #stmbl_conf_version 1), got:
  stmbl_conf_version 1", load aborted.
- Cause:   The config sector had never been written: erased flash, all 0xFF
  with no terminating 0. showconf printed it as a string (on past the
  sector until some 0 byte), flashloadconf copied 15 KB of it into the
  unterminated RAM config. servoterm-cli's backup read it with showconf, took
  a 0xFF for a scope frame's marker and swallowed the next 8 bytes: the
  first ack's "adding #".
- Fix:     conf_text(): erased flash (first byte 0xFF) is an empty config, for
  the firmware and the simulator; showconf prints at most the config's size,
  flashloadconf terminates. Test: host/tests/test_servoterm_ident.py
  test_load_into_erased_flash (fails without it, as on the drive).

## 2026-10-29  After make btburn the drive came back in DFU at every start
- Area:    bootloader/src/main.c (the DFU request word in CCM), src (the app's `bootloader` command, SystemInit)
- Status:  Open
- Symptom: The new drive (USB 207D35A45631): make btburn (bootloader.py's
  `bootloader`, then DFU, download, leave to 0x08010000): back in ROM DFU
  0.45 s later; a leave to 0x08000000: DFU again at once; the next leave to
  0x08000000 booted the application. Flash read back exactly the build.
- Cause:   Not found. Fits the request word (0xD0F0B007 at 0x1000FE00)
  surviving into the application, whose SystemInit takes a request an
  older bootloader passed on, until a start through the bootloader cleared
  it.
- Fix:     Recovery: dfu-util -a 0 -s 0x08000000:4:leave -U <file> (a 4-byte
  read, then leave) until it starts. Next: reproduce on a drive with the
  debugger, check whether the bootloader's clear reaches CCM before the ROM
  jump.

## 2026-10-29  Bench: a new drive dropped off USB for good
- Area:    hardware (drive with USB serial 207D35A45631)
- Status:  Known
- Symptom: After its first flash it ran, answered `about`, then disconnected
  46 s after a start and never enumerated again (neither as STMBL nor DFU).
- Fix:     None: the user found the board faulty and replaced it.

## 2026-10-29  hv_update fails at CRC_CHECK with an old F3 bootloader (v0.9.18-184)
- Area:    f3_boot (on the HV board), src/comps/hv.c (hv_update)
- Status:  Workaround
- Symptom: A drive from v0.9.18-207 updated to master: hv_update went
  through ERASE_FLASH and SEND_APP, then FLASH_FAILED at CRC_CHECK, twice;
  afterwards the F3 stayed in its bootloader (fault 9, HV values frozen).
- Cause:   The image was written exactly (the F3's flash read back equals
  obj_hvf3/hvf3.bin, whose STM32 CRC is 0), but that board's F3 bootloader,
  v0.9.18-184, rejected it, at the update and at boot. Its source reads the
  same as today's; the binary on the board computes the CRC differently.
- Fix:     Replace the F3 bootloader: DC bus off, USB on the HV board's own
  micro-USB (P11; its GND is the HV side's, VBUS not connected), JP1 (bottom
  side beside P11, BOOT0 to 3.3 V) shorted while an isolated 24..48 V powers
  the HV input, then make f3_all_btburn. Afterwards the F3 ran master (hv=1).
  Drives with an F3 bootloader older than ~v0.9.18-207 may need this.

## 2026-10-30  hardware/kigen: headless pcbnew 7 gotchas (route step)
- Area:    hardware/kigen (route.py, place.py)
- Status:  Workaround
- Symptom: `board.Remove(track)` on a loaded board segfaults later; `pcbnew.ImportSpecctraSES`
  returns False (needs the editor); BOX2I.Merge in a Python loop corrupts the heap.
- Cause:   pcbnew 7's SWIG ownership; the SES importer is tied to the GUI frame.
- Fix:     route.py cuts tracks out of the file text before loading (`load_bare`) and imports
  the SES itself (`import_ses`). No test: needs KiCad; `make -C hardware/brdstepper_mesa pcb route`
  exercises it and DRC checks the result.
