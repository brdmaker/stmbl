# stmbl — agent rules

## Debug notes (keep DEBUGNOTES.md current)

Whenever you discover a debug finding — a root cause, a non-obvious gotcha, a
surprising behavior, a workaround — or an open issue while working in this repo,
record it in `DEBUGNOTES.md` at the repo root. Follow the entry format already
in that file: a dated heading plus `Area`, `Status`
(`Open | Fixed | Workaround | Known | Note`), `Symptom`, `Cause`, and `Fix`.

- Keep entries concise (a few lines each).
- When the state of an existing issue changes, update its `Status` in place
  (e.g. `Open` -> `Fixed`) instead of adding a duplicate entry.
- Do this as part of the work, not only when asked.

## Regression tests (one for every bug)

Every bug you fix or find gets an automated test that fails without the fix
and passes with it, committed together with the fix.

- Use the lowest level that reproduces the bug:
  1. host unit test (`host/tests/test_*.c`) or `host/tests/*.test` with
     `@expect`, run by `make -C host check`;
  2. LinuxCNC on the simulated drive (`hwtest/burnin-sim`, `make test-*`);
  3. real hardware: `hwtest/suite` (`run_hwtest.sh hw`), `hwtest/soak` for
     faults that need hours, or `hwtest/fbtest` for feedback-port hardware.
- Where practical, show the test failing first: run it before the fix, or
  against the drive or build that still has the bug.
- Name the test in the bug's `DEBUGNOTES.md` entry (`Fix:` line). If no
  automated test is possible (e.g. a hardware fault), say why there.
- Do this as part of the work, not only when asked.

## Host simulator

The native x86 build of the control core lives in `host/` (see
`host/README.md`). Prefer it for running and testing control logic. After
changing control code run `make -C host check`, and add a `host/tests/*.test`
with `@expect` checks for behaviour you fix or add. `make -C host soak` runs
every motor in closed loop through a motion profile (~1 min). The ARM
firmware builds with `make all` at the repo root on the bench Pi (binutils
2.40); with binutils 2.42 a post-link tool hangs, see `DEBUGNOTES.md`.

## Flashing hardware

Flash the F4 application over **smart-serial during development**, using
both paths so they stay tested in real use; **USB DFU is only the backup.**
Always use the repo's make targets, never your own `dfu-util`, `st-flash`,
`setsserial` or `tools/bootloader.py` commands.

| Target | How | When |
|---|---|---|
| `make update` | in-band: the running application stages the image over its console, the bootloader copies it (`servoterm-cli --update`). `DEV=` the console: `/dev/ttyACM0` (USB, ~90 s) or sserialterm's pty (the smart-serial console, ~14 min) | the usual way |
| `make ssburn` | recovery path: `sserialboot`, then LinuxCNC's `setsserial` through the bootloader's smart-serial loader (`tools/ssflash.sh`, `--in-loader` for a drive already in its loader). LinuxCNC and halrun must not run | alternate it with `make update` (e.g. every other flash) |
| `make btburn` | F4 application over USB DFU (ST's ROM) | backup only: firmware without fwup / the loader, a smart-serial flash failed, or a change to the update path itself is being brought up |
| `make boot_btburn`, `make f3_btburn`, `make f3_boot_btburn` | F4 bootloader, F3 application, F3 bootloader over DFU | always (setsserial never writes block 0, the bootloader) |
| `make all_btburn`, `make f3_all_btburn` | whole-chip images (`f4.bin`, `f3.bin`) | |
| `make flash`, `boot_flash`, `f3_flash`, ... | the same over ST-Link (`st-flash`) | |

- **Drive disabled:** `fwup` and `sserialboot` refuse while it is enabled;
  flash writes under motion are not supported.
- **Use the smart-serial console** for `make update` now and then too, not
  only USB, so that path is exercised as well.
- **A failed smart-serial flash** is a bug like any other: DEBUGNOTES entry
  and regression test (see above). Recover with `make btburn`: a drive
  stuck in its loader falls back to DFU 30 s after power-up without
  smart-serial traffic.
- **Older versions:** check out that commit (for example in a `git worktree`)
  and run the same target there.
- **Backups:** reading flash (`dfu-util -U`) is not flashing and is fine.
  Before flashing, back up the drive's config (`servoterm-cli --dump`) and
  confirm the motor is safe to move; after flashing, check `about` and that
  the config is unchanged.
