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
firmware cross-build currently hangs in a post-link tool — see `DEBUGNOTES.md`
before attempting `make` at the repo root.
