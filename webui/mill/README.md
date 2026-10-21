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
make -C webui/mill test        # millweb against a stand-in LinuxCNC (~5 s)
make -C webui/mill check-all   # every suite: unit, simulated GUI, backend and GUI on real LinuxCNC (~15 min, non-root)
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
| `test/harness.py` | LinuxCNC on a copy of `test/sim` with millweb, for the suites (`harness.py -- CMD` runs CMD beside it) |
| `test/client.py` | a protocol client for the Python suites |
| `test/backend_suite.py` | the backend, case by case, on real LinuxCNC |
| `test/gui/` | the GUI suites (Playwright): `runner.js`, `sim_suite.js` (simulated), `live_suite.js` (real LinuxCNC), `wsclient.js` |
| `test/run_live.py`, `test/browser_live.js` | the end-to-end scenario: the protocol, then a headless Chromium on the page |
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
| `[DISPLAY] MAX_FEED_OVERRIDE`, `MAX_SPINDLE_OVERRIDE`, `MIN_SPINDLE_OVERRIDE` | 1.0, 1.0, 0.5 | the sliders' ranges: LinuxCNC clamps the overrides to these (and still reads back the value asked for) |
| `PREVIEW_SEGS` | 100000 | at most this many segments drawn; a longer program's path is merged to fit (its stats still cover every move) |
| `PREVIEW_TIMEOUT` | 120 | seconds of interpreting for a preview; then it shows what was read and says so |

## The protocol

JSON over the WebSocket at `/ws`.

**From the server** (`t` = type):

| t | When | What |
|---|---|---|
| `hello` | on connect | `you` (this browser's id), `machine` (name, coordinates, limits, units, program folder, drives, pins, token required), `control` (the holder), recent `events` |
| `st` | at `POLL_HZ`, only the fields that changed | estop, on, homed, mode, interp, run (idle / run / hold / mdi / done), line, file, `pos` (the tool tip in the active work offset), `mach`, wcs, g5x, g92, tool, tlo, following error per joint, velocity, spindle, overrides, coolant, M1 / block delete, G- and M-codes, probe |
| `slow` | 2 Hz | HAL pins, drives, load, bus, yaw, contour, pcomp's switches, the remote-enable key |
| `program` | when a program is loaded | name; `nlines` and the first 2000 `lines` (from `line0`); `path`: `[kind, x, y, z, feed, line, tool]` per segment, each from where the one before ended (`start`), from LinuxCNC's interpreter (kind 0 rapid, 1 feed, 2 probe, 3 tool change), in the active work coordinates; `stats` over every move: `moves`, `shown`, `stride` (past `PREVIEW_SEGS` the drawn path merges neighbouring moves), `ext` (travel), `cut` and `rapid` lengths, `time`, `tools`; `error`, `truncated` |
| `tools`, `offsets`, `files` | on change | the tool table, G54…G59.3 and G92, the program folder |
| `control` | on change | who has control |
| `ev` | as they happen | `{time, lvl, text}`: LinuxCNC's error channel, state changes, commands |
| `reply` | to each request | `{id, ok, err}`, plus a result (probe, tool setter, preview) |

**From the browser:** `{"op": ..., "id": n, ...}`:
- `control` {take, force, token};
- `estop` {on} (any browser), `power` {on};
- `home` / `unhome` {axis | all};
- `jog` {axis, dir, inc (0: continuous), rate}, `jog_stop`;
- `mdi` {cmd, wait} (wait: the reply when the line is done);
- `cycle` {action: start | pause | resume | step | stop, line};
- `override` {k: feed | rapid | spin | maxv, v}, `spindle` {dir, speed}, `coolant` {k, on}, `opt` {m1, bd};
- `zero` {axis, value}, `wcs` {index}, `offset` {index, x, y, z}, `g92_clear`;
- `tool_load` {t}, `tools_save` {tools}, `tool_measure`, `probe` {kind: z | plate, travel, feed, plate, clearance};
- `files`, `program_get` / `program_save` / `program_delete` / `program_load` {name[, text]};
- `program_lines` {name, start, count ≤ 5000}: a window of a long program's listing;
- `ping`, `events` {n} (the last n events);
- `pcomp` {k, v}.

## Safety, as built

- **The E-STOP chain is hardware**: stmbl's fault inputs and the drives' quick stop. The page's E-STOP only asks LinuxCNC for `STATE_ESTOP`, and any browser, watching or not, may press it.
- **One browser has control** at a time, the others watch. Control goes away when its browser disconnects. `REMOTE_ENABLE_PIN` forbids remote control at the machine, and `TOKEN` asks for a password.
- **Continuous jogging needs a keep-alive:** a message at least every 300 ms, or millweb stops the jog. A lost connection stops it at once.
- **E-STOP, Stop and jog stop never wait** behind another command (a tool measurement takes 30 s): they go to LinuxCNC from their own thread on their own command channel. After a Stop or an E-STOP, a running sequence (tool setter, probe, a multi-point move) sends nothing more.
- **Checks before LinuxCNC:** commands are checked (E-STOP, machine on, homed, idle) first, and LinuxCNC checks everything again.
- **A program can't take millweb down.** The preview interprets in a child process (killed after `PREVIEW_TIMEOUT`); the server keeps answering while it works.
- **The work offset shown is LinuxCNC's.** When LinuxCNC's status comes up without it (a fast start, DEBUGNOTES), the DRO says "G5x?" and millweb has LinuxCNC publish it again once the machine is homed and idle, and before any MDI.
- **Probe moves stay inside the soft limits.** LinuxCNC refuses a G38 move whose end lies beyond them, so millweb shortens the travel to what's left.
- **Program files** stay inside the program folder: base names only, G-code extensions only.
- **Access:** keep the machine on the shop network or behind a VPN. Use `CERT`/`KEY` for HTTPS and `TOKEN` for a password. There are no user accounts or roles yet.

## Not yet

- **Probe cycles:** Z surface, touch plate and the tool setter are live. The page's other cycles (corners, edges, bores, bosses, edge angle) run only in the simulation.
- **Dynamics page:** live values come from configured pins. The model and verify cards still show the simulation's numbers. Running dynwiz from the page is not wired up.
- **Units:** mm machines only.
- **Tool table:** wear and life are not kept.
- **Preview:** a program with a T word only previews while LinuxCNC runs, because the 2.9 interpreter reads tool data from LinuxCNC's shared memory (DEBUGNOTES).
- **Uploads** through the page stop at 6 MB; larger programs go into the program folder directly (network share, USB), as CAM output for a big part usually does.
- **Remote enable:** the key-switch pin is read, but no hardware example is wired.

## Testing

| Make target | What | Time | Needs |
|---|---|---|---|
| `make test` | unit tests: millweb against `server/tests/fake_linuxcnc.py` | 5 s | Python |
| `make test-gui-sim` | every page and control of the console on its simulated machine | 4 min | node, Playwright |
| `make test-backend` | the protocol case by case on real LinuxCNC (`test/sim`) | 7 min | LinuxCNC, a non-root user |
| `make test-gui-live` | the console on real LinuxCNC through its own controls | 3 min | all of the above |
| `make test-live` | one end-to-end scenario: the protocol, then the page | 1 min | all of the above |
| `make test-stress` | large programs: a million lines, 2 million moves, 20 000 peck cycles, arcs, real CAM output (inches) | 10 min | all of the above |
| `make check-all` | the first four | 15 min | |

`K=word` runs only the cases whose name contains it, `SHOTS=dir` keeps a
screenshot of each failed GUI case: `make test-gui-sim K=offsets SHOTS=/tmp/shots`.
Each suite prints `PASS`/`FAIL` per case, the failed checks, and for a
failed backend case the machine's last events.

**The harness** (`test/harness.py`) copies `test/sim` to a temporary
directory, sets millweb as its display on a free port, starts LinuxCNC
headless, and stops it by stopping millweb (LinuxCNC follows its display).
`--ini-set SECTION.KEY=VALUE` changes the INI, `--keep` keeps the directory;
`harness.py -- CMD` runs CMD with `MILLWEB_PORT` and `MILLWEB_DIR` set.

**Unit tests** (`make test`, a stand-in LinuxCNC; moves finish at once):
- `hello` and the static files (no path traversal);
- the control lease, E-STOP from a watching browser, releasing control;
- E-STOP, power (off reads back as `STATE_ESTOP_RESET`), homing, MDI, G10 L20, overrides;
- the jog deadman; the tool table, program names, upload, load, probing, `T0` unloading;
- `ping`, `events`, unknown ops, bad JSON, a crashing handler (still a reply);
- E-STOP and Stop answered at once while a tool measurement waits, and the measurement sends nothing more;
- `TOKEN`, `ON_CONTROL_LOSS = hold`, `REMOTE_ENABLE_PIN` and `AIR_PIN` (a stand-in `hal`).

**Backend suite** (`make test-backend`, 48 cases on real LinuxCNC). Each case
starts from the same machine: E-STOP reset, on, homed, idle, G54 and G55 at
their test values, G92 clear, overrides 100 %, spindle and coolant off, the
tool at a safe height.
- **Connection:** `hello` (the INI's name, coordinates, limits, pins), `ping`, `events`, unknown ops, bad JSON and wrong types, a message over 8 MB (only that connection closes), state diffs to every browser.
- **HTTP:** the console's files, `HEAD`, 404, five path-traversal attempts, `POST` refused.
- **Control:** take, refuse, force, release; a disconnect drops it while the program carries on; a watching browser is refused twelve kinds of command, may read programs and press E-STOP.
- **Machine:** E-STOP and power and what they block; homing per axis and all; unhomed blocks MDI and jogs.
- **Jogging:** increments on every axis both ways; continuous with keep-alives stopped by `jog_stop`; the deadman; a lost connection.
- **Urgent commands:** E-STOP and Stop answered within a second during a tool measurement, which then sends nothing more; MDI with `wait`.
- **MDI:** moves in G54 and machine coordinates, F, too long or empty lines, interpreter errors as events; refused while another runs.
- **Programs:** save, list, get, load with the interpreter's preview, delete (not the loaded one); file names stay in the program folder (eight bad names); 7 MB refused; a program with an error previews with it.
- **Cycles:** run to the end (tool change, M3, M8, events); pause, resume, stop, start again; single block; run from a line; refused unhomed or off.
- **Overrides, spindle, coolant, options:** all four overrides and their limits; feed override 50 % takes twice as long; CW, CCW, stop, at speed; flood, mist, air; M1 and block delete in a program.
- **Offsets:** G10 L20 per axis, G55, G10 L2 per axis, G92 and clearing it.
- **Tools:** save with a backup, unique numbers, bad entries, a table edited on disk; M6 and its length offset; the tool setter (50.000 mm).
- **Probing:** Z surface with and without a length offset, the retract from the trip point; the touch plate sets Z from the trip point; nothing to touch; unsupported cycles; the soft limit.
- **Other INIs**, each on its own LinuxCNC: `ON_CONTROL_LOSS = hold` (holds a running program, leaves an idle one), `TOKEN`, `REMOTE_ENABLE_PIN` (a HAL key switch drops control, E-STOP still works), `AIR_PIN`.

**GUI suite, simulated** (`make test-gui-sim`, 32 cases; `index.html` from
disk, each case on a fresh page): boot; every page, remembered across a
reload, `#hash`; cycle start, hold, resume, single block, stop, run to the
end, run from a line, M1, confirm before start, the feed-hold lift; E-STOP and
power by button and F1/F2 and what they block; homing; DRO modes, ZERO, SET
(dialog, Esc, cancel), zero all, go to zero, park; MDI (moves, G55, T M6, M3,
M4, M5, M7, M8, M9, G10, beyond the travel, history, Ctrl+M); jog increments,
continuous, keyboard, soft limit, rate; overrides and reset; spindle and
coolant; the File page (preflight, load, Edit); the three wizards; Offsets
(edit, use, set here, G92, touch plate); Tools (select, load, add, delete,
edit, measure); Probe (all cycles, apply, the tool setter); Dynamics (pcomp
switches, readouts, four plots drawn, re-measure); Status (filter, clear,
pins, stats); Settings (inches, decimals, exaggeration, switches); the 3D
view (cameras, colours, layers, orbiting); no sideways scrolling at phone and
tablet widths; over HTTP without millweb (the simulation after 2.5 s) and
with `?sim`. Any script error fails the case.

**GUI suite, live** (`make test-gui-live`, 15 cases on real LinuxCNC, each
from the backend suite's starting state): connecting (LIVE, the INI's name,
pins, tool table); taking control while a second browser watches, is refused
MDI, presses E-STOP, takes over; E-STOP, power and homing; MDI with the DRO,
machine coordinates and modal codes; pad and keyboard jogs and a continuous
jog the keep-alive sustains; ZERO, SET, zero all, go to zero, park, and a move
beyond the travel refused; the Offsets page; the Tools page down to
`tool.tbl` and the tool setter; probing with T99 and applying it; the File
page from the machine's interpreter, load, cycle buttons, run from a line to
the end; a wizard's program saved and loaded; overrides, spindle, coolant, M1
and block delete as the machine reports them; changes made by another browser
showing; no script errors on any page; finally millweb stopping (OFFLINE).

**Stress** (`make test-stress`, `test/stress_suite.py`): `test/bigcode.py`
writes CAM-style programs into the program folder: a 1 001 661-line 3D raster
finish (27 MB), 300 000 lines of adaptive clearing with G2/G3, 20 000 G83
peck cycles, and 25 lines of O-word loops that make 2 million moves; with
LinuxCNC's own `3D_Chips.ngc` (inches). Each is previewed: no error, every
move counted, the travel right, at most 100 000 segments drawn, the message
under 8 MB, the server answering pings within 0.5 s meanwhile, millweb
alive. Then: line windows at 500 000 and the end of the million; the cache;
a load answering at once while its preview runs, and a newer load winning;
20 000 short moves run to the end (the median feed is printed); run from
line 900 000; and the page in Chromium while the machine runs the million
from line 500 000 (drawn, the listing following the running line, the
scripts' time per frame, heap, the preflight).

**End-to-end** (`make test-live`): about 50 checks through one session, then
`test/browser_live.js` on the page. `--shot FILE` / `--browser FILE` take
screenshots.
