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
  3. real hardware: `hwtest/suite` (`run_hwtest.sh hw`), or `hwtest/soak` for
     faults that need hours.
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

Always flash with the repo's official make targets, never with your own
`dfu-util`, `st-flash` or `tools/bootloader.py` commands:

| Target | Flashes |
|---|---|
| `make btburn` | F4 application (`obj_app/stmbl.dfu` at 0x08010000) over USB DFU; builds first and enters the bootloader itself |
| `make boot_btburn`, `make f3_btburn`, `make f3_boot_btburn` | F4 bootloader, F3 application, F3 bootloader |
| `make all_btburn`, `make f3_all_btburn` | whole-chip images (`f4.bin`, `f3.bin`) |
| `make flash`, `boot_flash`, `f3_flash`, ... | the same over ST-Link (`st-flash`) |

To flash an older version, check out that commit (for example in a
`git worktree`) and run the same target there. Reading flash for a backup
(`dfu-util -U`) is not flashing and is fine. Before flashing, back up the
drive's config (`servoterm-cli --dump`) and confirm the motor is safe to
move; after flashing, check `about` and that the config is unchanged.
