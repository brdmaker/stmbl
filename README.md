# linuxcnc-bin: prebuilt LinuxCNC for the cloud sessions

Orphan branch (no shared history with the code): only a prebuilt LinuxCNC,
so that a fresh Claude Code cloud container can run `hwtest/arcmate-sim`
(LinuxCNC + the simulated arm on simulated stmbl drives) without a ~25 min
build. Normal clones and work never fetch it.

- `linuxcnc-2.9-uspace.tar.xz`: LinuxCNC 2.9.10 (branch 2.9; the bench runs
  2.9.1, same HAL API), uspace, run-in-place, built for Ubuntu 24.04 x86_64
  with Python 3.12. Unpacks to `/home/user/linuxcnc-2.9` (its paths are
  absolute). Runtime only: no docs, no build objects; `src/Makefile.modinc`
  and `include/` are in, to build modules (host/arm/lcnc).
- `build_linuxcnc.sh`: exactly how it was built.

Restore with `hwtest/arcmate-sim/linuxcnc_restore.sh` on the code branch
(fetches this branch, unpacks, installs the runtime packages, sets up a
python3 -> 3.12 shim and a non-root user). Useless on the Pi (arm64,
Debian): install LinuxCNC there as usual.
