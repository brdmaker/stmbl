# webui/mill: a browser console for a headless milling machine

The operator console for a gantry mill on stmbl drives and LinuxCNC, as a
web app. The machine runs headless: LinuxCNC and **millweb** run on the
controller, and the operator uses a browser on any tablet or PC on the shop
network.

The console runs two ways:
- **Live:** served by `millweb`, it shows the real machine and drives it.
- **Simulated:** opened from disk, with `?sim`, or when no millweb answers within 2.5 s. It runs a built-in machine and program, for demos and training.

```sh
make -C webui/mill demo        # the console alone, simulated: http://localhost:8000/
make -C webui/mill sim         # LinuxCNC (test/sim, no hardware) with millweb: http://localhost:8080/
make -C webui/mill test        # millweb against a stand-in LinuxCNC (~3 s)
make -C webui/mill test-live   # against real LinuxCNC and a headless browser (~1 min, non-root)
```

Nothing to build and nothing to install beyond LinuxCNC:
- **millweb** is Python 3, standard library only, plus LinuxCNC's own `linuxcnc`, `hal`, `gcode` and `rs274` modules. It runs on a Raspberry Pi too.
- **The page** needs no internet: three.js r128 (MIT) is under `vendor/three` with its licence. Fonts come from Google Fonts when online and fall back to system fonts.

| Path | What |
|---|---|
| `index.html` | the console's markup and style |
| `js/app.js` | the console: pages, DRO, 3D machine, plots, and the built-in simulated machine |
| `js/live.js` | the live machine: the WebSocket client behind the same actions; decides live or simulated at start |
| `millweb` | the backend's launcher: LinuxCNC's display program |
| `server/millweb.py` | the hub: browsers, control lease, jog deadman, status diffs, events, previews |
| `server/machine.py` | LinuxCNC: status, commands, tool table, offsets, programs, probing |
| `server/preview.py` | a program's toolpath from LinuxCNC's interpreter (the `gcode` module, as AXIS previews) |
| `server/wsserver.py` | HTTP(S) and WebSocket on asyncio (RFC 6455), standard library only |
| `server/tests/` | unit tests against `fake_linuxcnc.py` |
| `test/sim/` | a LinuxCNC config without hardware: a gantry mill (X Y Y Z), loopback joints, a probe, a tool changer |
| `test/run_live.py`, `test/browser_live.js` | the integration test: real LinuxCNC, the protocol, then a headless Chromium on the page |
| `deploy/linuxcnc-millweb.service` | systemd: LinuxCNC with millweb at boot |

## Running it on a machine

1. **Make millweb the display.** In the machine's INI:

   ```ini
   [DISPLAY]
   DISPLAY = /path/to/stmbl/webui/mill/millweb
   PROGRAM_PREFIX = ~/linuxcnc/nc_files

   [MILLWEB]
   PORT = 8080
   ```

   LinuxCNC starts millweb and stops when it stops. Alternatively, run `webui/mill/millweb --ini machine.ini` beside a LinuxCNC that already has another display.
2. **Start it at boot:** `deploy/linuxcnc-millweb.service` (systemd; set the user and the INI path).
3. **Open** `http://<controller>:8080/` in any browser on the shop network. **Take control** (the header button), release E-STOP, switch the machine on, home.

**`[MILLWEB]` keys**

| Key | Default | What |
|---|---|---|
| `PORT`, `HOST` | 8080, 0.0.0.0 | where it listens |
| `CERT`, `KEY` | | HTTPS (and WSS) with this certificate |
| `TOKEN` | | required to take control (asked for in the browser) |
| `REMOTE_ENABLE_PIN` | | a HAL bit (a key switch at the machine): while false, nobody can take control, and control is dropped |
| `ON_CONTROL_LOSS` | continue | `hold`: feed hold when the controlling browser disconnects during a program |
| `POLL_HZ` | 30 | status rate |
| `NC_DIR` | `[DISPLAY] PROGRAM_PREFIX` | the program folder (browsers list, upload, load, delete) |
| `TOOLSETTER` | | `X Y Z_ABOVE Z_REF [TRAVEL]` (machine): tool length = touched Z − Z_REF |
| `PIN` (repeat) | | HAL pins for the Status page's watch |
| `DRIVE` (repeat) | | `NAME CURRENT_PIN [POWER_PIN] [FAULT_PIN]` for the Dynamics page's drive table |
| `SPINDLE_LOAD_PIN`, `BUS_PIN`, `YAW_PIN`, `CONTOUR_PIN`, `AT_SPEED_PIN`, `AIR_PIN` | | the Dynamics page's live values; `AIR_PIN`: a bit the Air button sets |
| `PCOMP` | pcomp.0 | pcomp's HAL prefix; its switches are on the Dynamics page when it is loaded |

## The protocol

JSON over the WebSocket at `/ws`.

**From the server** (`t` = type):

| t | When | What |
|---|---|---|
| `hello` | on connect | `you` (this browser's id), `machine` (name, coordinates, limits, units, program folder, drives, pins, token required), `control` (the holder), recent `events` |
| `st` | at `POLL_HZ`, only the fields that changed | estop, on, homed, mode, interp, run (idle / run / hold / mdi / done), line, file, `pos` (the tool tip in the active work offset), `mach`, wcs, g5x, g92, tool, tlo, following error per joint, velocity, spindle, overrides, coolant, M1 / block delete, G- and M-codes, probe |
| `slow` | 2 Hz | HAL pins, drives, load, bus, yaw, contour, pcomp's switches, the remote-enable key |
| `program` | when a program is loaded | name, lines, `segs` `[kind, x0, y0, z0, x, y, z, feed, line, tool]` from LinuxCNC's interpreter (kind 0 rapid, 1 feed, 2 probe, 3 tool change), in the active work coordinates |
| `tools`, `offsets`, `files` | on change | the tool table, G54…G59.3 and G92, the program folder |
| `control` | on change | who has control |
| `ev` | as they happen | `{time, lvl, text}`: LinuxCNC's error channel, state changes, commands |
| `reply` | to each request | `{id, ok, err}`, plus a result (probe, tool setter, preview) |

**From the browser:** `{"op": ..., "id": n, ...}`:
- `control` {take, force, token};
- `estop` {on} (any browser), `power` {on};
- `home` / `unhome` {axis | all};
- `jog` {axis, dir, inc (0: continuous), rate}, `jog_stop`;
- `mdi` {cmd};
- `cycle` {action: start | pause | resume | step | stop, line};
- `override` {k: feed | rapid | spin | maxv, v}, `spindle` {dir, speed}, `coolant` {k, on}, `opt` {m1, bd};
- `zero` {axis, value}, `wcs` {index}, `offset` {index, x, y, z}, `g92_clear`;
- `tool_load` {t}, `tools_save` {tools}, `tool_measure`, `probe` {kind: z | plate, travel, feed, plate, clearance};
- `files`, `program_get` / `program_save` / `program_delete` / `program_load` {name[, text]};
- `pcomp` {k, v}.

## Safety, as built

- **The E-STOP chain is hardware**: stmbl's fault inputs and the drives' quick stop. The page's E-STOP only asks LinuxCNC for `STATE_ESTOP`, and any browser, watching or not, may press it.
- **One browser has control** at a time, the others watch. Control goes away when its browser disconnects. `REMOTE_ENABLE_PIN` forbids remote control at the machine, and `TOKEN` asks for a password.
- **Continuous jogging needs a keep-alive:** a message at least every 300 ms, or millweb stops the jog. A lost connection stops it at once.
- **Checks before LinuxCNC:** commands are checked (E-STOP, machine on, homed, idle) first, and LinuxCNC checks everything again.
- **Probe moves stay inside the soft limits.** LinuxCNC refuses a G38 move whose end lies beyond them, so millweb shortens the travel to what's left.
- **Program files** stay inside the program folder: base names only, G-code extensions only.
- **Access:** keep the machine on the shop network or behind a VPN. Use `CERT`/`KEY` for HTTPS and `TOKEN` for a password. There are no user accounts or roles yet.

## Not yet

- **Probe cycles:** Z surface, touch plate and the tool setter are live. The page's other cycles (corners, edges, bores, bosses, edge angle) run only in the simulation.
- **Dynamics page:** live values come from configured pins. The model and verify cards still show the simulation's numbers. Running dynwiz from the page is not wired up.
- **Units:** mm machines only.
- **Tool table:** wear and life are not kept.
- **Preview:** a program with a T word only previews while LinuxCNC runs, because the 2.9 interpreter reads tool data from LinuxCNC's shared memory (DEBUGNOTES).
- **Remote enable:** the key-switch pin is read, but no hardware example is wired.

## Testing

**`make test`** runs the protocol against `server/tests/fake_linuxcnc.py`:
- `hello` and the static files (no path traversal);
- the control lease, and E-STOP from a watching browser;
- E-STOP, power, homing, MDI, G10 L20 and the overrides;
- the jog deadman;
- the tool table (backup, unique numbers), program names, upload and load, probing.

**`make test-live`** runs `test/run_live.py`, about 50 checks against real LinuxCNC on `test/sim`, with millweb as its display:
- the INI's name and coordinates;
- the lease, power and homing;
- MDI and the DRO, G10 L20 / L2, increment and continuous jogs (the deadman stops it);
- upload and load; the interpreter's preview (the tool change, arcs as segments, G54 coordinates);
- the run to the end: the tool change, M3;
- probing at the stock top (machine Z −195, on the tool tip with its length offset);
- the tool table, M6, the tool setter (length 50.000);
- a second browser that watches, can't send MDI, and can press E-STOP.

Then `test/browser_live.js` (Playwright, headless Chromium) uses the page's own controls:
- take control, E-STOP reset, machine on, home all;
- MDI, with the DRO following;
- a jog from the pad;
- a wizard's program saved and loaded on the machine, the File page and its preflight;
- no script errors.

`--shot FILE` / `--browser FILE` take screenshots.
