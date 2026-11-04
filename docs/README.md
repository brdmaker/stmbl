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

brdstepper (branch `brdstepper`):

| doc | what |
|---|---|
| [brdstepper_hw.md](brdstepper_hw.md) | closed-loop NEMA11 stepper drive: MCU choice, pin plan, smart-serial / CAN variants, USB, parts list, software plan (a new target in this repo, reusing `shared/`) |

CAN (branch `can-support`):

| doc | what |
|---|---|
| [can_support.md](can_support.md) | what CAN support would mean for stmbl and brdstepper: hardware, bus timing (`tools/canbus_budget.py`), LinuxCNC's side, protocol options, work plan |

Supply:

| doc | what |
|---|---|
| [psu.md](psu.md) | moved to `brdmaker/brdmot` (`psu/`): the pack power supply for single-phase 230 V |

FANUC motors on hand:

| doc | what |
|---|---|
| [fanuci.md](fanuci.md) | `fanuci`: experimental probe for FANUC 1-pair (αi / βi) serial pulsecoders on FB0, how to find the protocol on the bench and turn it into a driver |
| [fanuc/](fanuc/README.md) | moved to the private repository `brdmaker/brdmot`: the FANUC motors on hand, their encoders, the `encf` bench test, FANUC manuals as text |

Tools: `servoterm-cli/README.md`, `linuxcnc/sserialterm/README.md`.
Simulator and tests: `host/README.md` (`make -C host check`).
Findings and open issues: `DEBUGNOTES.md`.
