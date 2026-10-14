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

## Host simulator

The native x86 build of the control core lives in `host/` (see
`host/README.md`). Prefer it for running and testing control logic. The ARM
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
