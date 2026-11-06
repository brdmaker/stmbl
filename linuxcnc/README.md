# LinuxCNC side of the stmbl drive

What LinuxCNC needs to talk to stmbl drives. Machine-level parts (robot
kinematics, the lathe, powerquota, the GUI) are in the private repository
`brdmaker/brdmot`, which uses this one as a submodule.

| folder | what |
|---|---|
| [config/](config/) | example LinuxCNC configuration: one drive over smart-serial on a Mesa card (`stmbl.ini`, `stmbl.hal`) |
| [sserialterm/](sserialterm/README.md) | `sserialterm`: the drives' consoles over smart-serial, as ptys for `servoterm-cli` ([docs/sserial_console.md](../docs/sserial_console.md)) |
| [stmblcan/](stmblcan/README.md) | `stmblcan`: drives over CAN (MCP2515 on a Raspberry Pi's SPI) ([docs/can_support.md](../docs/can_support.md)) |

Simulated drives for LinuxCNC, built from the host simulator (`host/`):

| module | what |
|---|---|
| `host/lcnc` → `stmbl_sim.so` | the simulated drive as a realtime module, on the smart-serial or CAN path; configs in `hwtest/burnin-sim` |
| `host/arm/lcnc`, `host/portal/lcnc`, `host/mill/lcnc` | six-axis arm, gantry and mill plants with their drives |

Build any of them with `make && sudo make install` in its folder (LinuxCNC
2.9 uspace, `Makefile.modinc`). Build outputs (`*.so`, `*.tmp`, `*.ver`)
are ignored by git.
