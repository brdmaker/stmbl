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

Setup (branch `setup-wizard`):

| doc | what |
|---|---|
| [setup_wizard.md](setup_wizard.md) | `servoterm-cli --wizard`: sets up a drive and its motor step by step from the nameplate, measures the rest, wiring help, version check; the library for a GUI; tested on 35 simulated motors |

brdstepper (branch `brdstepper`):

| doc | what |
|---|---|
| [brdstepper_hw.md](brdstepper_hw.md) | closed-loop NEMA11 stepper drive: MCU choice, pin plan, smart-serial / CAN variants, USB, parts list, software plan (a new target in this repo, reusing `shared/`) |

CAN (branch `can-support`):

| doc | what |
|---|---|
| [can_support.md](can_support.md) | what CAN support would mean for stmbl and brdstepper: hardware, bus timing (`tools/canbus_budget.py`), LinuxCNC's side, protocol options, work plan |

Tools: `servoterm-cli/README.md`, `linuxcnc/sserialterm/README.md`.
Simulator and tests: `host/README.md` (`make -C host check`).
Findings and open issues: `DEBUGNOTES.md`.
