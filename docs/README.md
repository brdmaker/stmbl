# stmbl docs

The user manual (AsciiDoc) is in `src/`: getting started, wiring and
pinout, supply, motor and feedback setup, tuning, commands, servoterm.
`linuxcnc/` holds an example LinuxCNC configuration.

Smart-serial features (branches `sserial_console`, `sserial_bootloader`):

| doc | what |
|---|---|
| [sserial_console.md](sserial_console.md) | the drive's console over smart-serial: setup with `sserialterm`, the frame and link protocol, limits, troubleshooting |
| [capture_scope.md](capture_scope.md) | `scope0`: capture in the drive, download over any console; pins, triggers, modes, dump format |
| [sserial_bootloader.md](sserial_bootloader.md) | firmware updates: in-band over the console (`fwup`, `servoterm-cli --update`) and recovery with `setsserial flash`; flash layout, the single-bank stall |

Tools: `servoterm-cli/README.md`, `linuxcnc/sserialterm/README.md`.
Simulator and tests: `host/README.md` (`make -C host check`).
Findings and open issues: `DEBUGNOTES.md`.
