# webui/mill: a browser console for a headless milling machine

The operator console for a gantry mill on stmbl drives and LinuxCNC, as a
web app. The machine runs headless: LinuxCNC and **millweb** run on the
controller, and the operator uses a browser on any tablet or PC on the shop
network.

The console runs two ways:
- **Live:** served by `millweb`, it shows the real machine and drives it.
- **Simulated:** opened from disk, with `?sim`, or when no millweb answers within 6 s (if one answers later, the page reloads into it). It runs a built-in machine and program, for demos and training.

Until it has drawn, the page shows a loading screen saying what it is doing (loading, connecting to the machine), or which file didn't load.

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
| `js/setup3d.js` | the 3D setup: fixtures and workpieces, STL/OBJ reading, selecting in the view, the object panel |
| `js/hal.js` | the Debug page's HAL tab: pins, signals and parameters, the watch list |
| `js/drives.js` | the Debug page's Drives tab: a card per stmbl drive and its console |
| `js/machmodel.js` | the machine's own 3D model in the view: parts that move with the axes |
| `js/config.js` | the Configure page: machine.json in forms and as text, Check, history, Restart |
| `js/scope.js` | the Debug page's scope (triggering, drawing, cursor), its tabs and saved views |
| `js/live.js` | the live machine: the WebSocket client behind the same actions; decides live or simulated at start |
| `millweb` | the backend's launcher: LinuxCNC's display program |
| `millweb-supervisor` | runs LinuxCNC for a machine folder (machine.json), again and again; `millweb-supervisor.service` for systemd |
| `server/millweb.py` | the hub: browsers, control lease, jog deadman, status diffs, events, previews |
| `server/machine.py` | LinuxCNC: status, commands, tool table, offsets, programs, probing |
| `server/drives.py` | the stmbl drives' consoles: the tty, the values the cards ask for, what browsers may send |
| `server/supervisor.py` | millweb-supervisor: the bitstream, the INI and HAL, LinuxCNC's runs; the page while LinuxCNC is down |
| `server/machineconf.py` | machine.json: checking it, the INI and HAL made of it, saving with a history |
| `server/fpga_load.py` | loads a bitstream through Linux's FPGA manager (root, via sudo) |
| `server/sampler.py` | the scope's sampler: HAL values at a steady rate, in a process of its own |
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

## A machine in one file: millweb-supervisor

The usual way: LinuxCNC isn't started by hand with a hand-written INI. A
service, `millweb-supervisor`, runs it for a **machine folder**, again and
again, and the console configures it:

    millweb-supervisor /home/lcnc/machine          (in LinuxCNC's environment, as the machine's user)

Each start:
1. `machine.json` is checked and made into `run/mill.ini` and `run/machine.hal` (and `custom.hal`, if there is one, loaded after it);
2. the **FPGA bitstream** is loaded, if machine.json names one: with Linux's FPGA manager (a Zynq's programmable logic: `bitstreams/<file>.bit.bin`, through `server/fpga_load.py` with `sudo -n`), or with a command of your own (`{file}` is the bitstream). With `reload: changed` it is loaded when it changed or after a boot, else at every start;
3. `linuxcnc run/mill.ini` runs, millweb as its display, until it ends.

**Restart LinuxCNC** (the Configure page; with control, not while a program
runs) makes millweb quit; LinuxCNC follows; the supervisor starts it again
with the saved machine.json. The page reconnects by itself, in 2–3 s with the
test machine. **Stop** leaves LinuxCNC down until **Start**.

**While LinuxCNC is down** (restarting, stopped, or failed) the supervisor
serves the console on the same port: the header says *LINUXCNC FAILED* (or
stopped), and only the Configure page works: the error, the start's log,
machine.json to fix, Start. A configuration LinuxCNC refuses isn't retried
(the error names the HAL line); one that ran before and then ended is
started again (three quick failures in a row: it waits for Start).

**The machine folder:**

| | |
|---|---|
| `machine.json` | the machine (below); the Configure page edits it |
| `custom.hal` | more HAL, after the generated (the Configure page's Custom HAL) |
| `models/` | the machine's 3D model files (the 3D model tab; up to 6 MB each) |
| `bitstreams/` | FPGA bitstreams (uploaded on the page, up to 6 MB; larger ones copied in) |
| `history/` | every saved machine.json and custom.hal (and the file as it was before the first save) |
| `run/` | made at each start: `mill.ini`, `machine.hal`; `status.json` (the supervisor's state); the drives' console ptys |
| `logs/` | `linuxcnc.log`: LinuxCNC's and millweb's output, the last start (`.1`, `.2` the ones before) |
| `tool.tbl`, `linuxcnc.var`, `nc_files/`, `millweb-setup.json`, `debug-views.json` | kept across starts |

`millweb-supervisor DIR --init` makes a folder with a default machine.json
(three simulated joints). `make supervised` runs `test/machine` (the test
machine as a machine.json) to try it.

**machine.json** (everything optional has a default; the Configure page shows
it):

| key | what |
|---|---|
| `name`, `units` (mm), `servo_period_ns` | |
| `board` | `type`: `none`, `hm2_eth`, `hm2_spix`, `hm2_rpspi`, `hm2_soc_ol`; `name` (in its pins: `7i96s` for `hm2_7i96s.0…`), `params` (the driver's: `board_ip=…`, `spiclk_rate=…`), `config` (`config="…"`), `watchdog_ms` |
| `bitstream` | `loader`: `none`, `fpga_manager`, `command`; `file` (in `bitstreams/`), `fpga` (`fpga0`), `command` (with `{file}`), `reload`: `changed` or `always` |
| `axes` | `X`, `Y`, `Z` …: `min`, `max`, `max_vel`, `max_acc` (mm, mm/s, mm/s²) |
| `joints` | a list: `axis`; `drive`: `{type: sim}` (command = feedback), `{type: stmbl_sserial, port, channel}` (on the Mesa board's smart-serial), `{type: stmbl_can, bus, node}` (stmblcan; `can.spi` lists the buses), `{type: stmbl_sim, conf, sim}` (the simulated drive, one at most), with `console` (its console on the Drives tab, default yes); `ferror`, `min_ferror`; `home`: `method` (`absolute`, `immediate`, `switch` with `switch`/`search_vel`/`latch_vel`/`offset`, `index` with `latch_vel`/`offset`), `pos`, `sequence` (two joints on an axis: a gantry, homed in step); `min`, `max`, `max_vel`, `max_acc` (default: the axis's, travel ±1 mm, speeds ×1.1) |
| `can` | `spi` (a device a bus, or `sim`), `discover`, `spi_hz` |
| `estop` | `input` (a HAL pin, true while the chain is closed), `invert`; a Mesa board's watchdog also trips it; no input and no board: a loop |
| `probe` | `type`: `none`, `input` (`input`, `invert`), `sim` (trips below `sim_z`) |
| `spindle` | `type`: `none`, `sim`; `max_rpm` |
| `toolsetter` | `x`, `y`, `z_above`, `z_ref`, `travel` (the Tools page's Measure), or null |
| `display` | `max_feed_override`, `max_spindle_override`, `min_spindle_override` |
| `model` | the machine in the 3D view: `builtin` (the built-in gantry, default on), `parts`: `{name, file (in models/, STL or OBJ), moves ("" fixed, "Y", "XY", "XYZ", "-X" against X for a moving table …), pos, rot (°), scale, color, opacity}`, each placed as it is with the machine at X0 Y0 Z0 and the tool tip at the origin |
| `console` | millweb: `port`, `host`, `token`, `on_control_loss`, `hal_write`, `drive_write`, `cpus`, `pins` (the Status page's), `extra` (more `[MILLWEB]` keys) |

The stmbl drives get their `DRIVE` and `DRIVE_LINK` lines (the Drives tab)
and a `sserialterm` console each, made from the joints.

**As a service:** `millweb-supervisor.service` (edit the user and paths;
it runs as the machine's user: LinuxCNC refuses root). For the FPGA manager,
a sudoers line lets that user load bitstreams and nothing else:

    lcnc ALL=(root) NOPASSWD: /usr/bin/python3 /home/lcnc/stmbl/webui/mill/server/fpga_load.py *

(`MILLWEB_SUDO` changes the `sudo -n` prefix; empty runs it directly.)

**The Configure page** (also without the supervisor: then it says so and
the INI is configured by hand):
- **Machine, Axes & joints, Hardware, I/O, Console:** machine.json in forms (a joint per card; the drive's and the homing's fields follow their type). Blank fields take the default shown.
- **3D model:** the machine in the 3D view from its own files: upload STL or OBJ parts, say what each rides on (the frame fixed, a gantry's bridge with Y, the X carriage with X and Y, the Z ram with all three, a moving table against X and Y), place, turn, scale and colour them, and switch the built-in gantry off. The 3D view sits beside the form and shows each change at once; Save keeps it for every browser. A CAD export of the machine at its home position, in mm, lines up with one offset (where the tool tip is).
- **Custom HAL**, **machine.json** (as text; the forms and the text are the same file).
- **Check** asks millweb to check machine.json and make its INI and HAL: problems are listed (a click goes to the field, marked), **Generated** shows the files and the changes against the saved ones.
- **Save** (asks, naming the settings changed; a note goes into the history), **Save & restart**, **Discard**; **History**: every version, compared with the editor or loaded into it; **Log**: the last three starts.
- Saving, uploading bitstreams and restarting need control; reading doesn't. Changes are in the event log with who made them.

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
| `WORKSPACE_X`, `WORKSPACE_Y`, `WORKSPACE_Z` | | `min max` (machine): jogs stay inside this box (an increment is shortened to its edge, a continuous jog stops there; from outside only back towards it); programs and MDI still have the whole travel. None: the soft limits |
| `PCOMP` | pcomp.0 | pcomp's HAL prefix; its switches are on the Dynamics page when it is loaded |
| `[DISPLAY] MAX_FEED_OVERRIDE`, `MAX_SPINDLE_OVERRIDE`, `MIN_SPINDLE_OVERRIDE` | 1.0, 1.0, 0.5 | the sliders' ranges: LinuxCNC clamps the overrides to these (and still reads back the value asked for) |
| `SETUP` | `millweb-setup.json` beside the INI | the 3D setup (fixtures and workpieces in the view) |
| `CPUS` | | millweb, its threads and its preview processes on these cores only (`0`, `0,1`, `0-1`): keep them off LinuxCNC's real-time core |
| `PREVIEW_NICE` | 10 | the preview process's niceness (lower priority than millweb and LinuxCNC) |
| `HAL_HZ` | 10 | the Debug page's watch rate |
| `HAL_WRITE` | no | `yes`: the Debug page can set unconnected input pins, writable parameters and undriven signals |
| `DRIVE_LINK` (repeat) | | an stmbl drive for the Drives tab: `name tty [hal-prefix]`, e.g. `X /tmp/drive-x hm2_7i96s.0.stbl.0.0` (its console through `sserialterm`, or its USB port; the prefix of its smart-serial pins) |
| `DRIVE_WRITE` | no | `yes`: with control, the Drives tab's console may set pins and run commands, not only read |
| `DRIVE_POLL_MS` | 250 | a drive's card values are asked one at a time, at most this often, while its console is quiet |
| `DEBUG_VIEWS` | `debug-views.json` beside the INI | the Debug page's saved views |
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
| `tools`, `offsets`, `files` | on change | the tool table, G54…G59.3 and G92, the program folder (`files` with their paths and `kind`: program or model, `dirs`) |
| `setup` | on connect, on change | the 3D setup's `objects` |
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
- `files`, `program_get` / `program_save` / `program_delete` / `program_load` {name[, text, overwrite]}: names are paths in the program folder (`jobs/acme/op1.ngc`);
- `program_rename` {name, to}: a program, model or folder renamed or moved; `folder_create` / `folder_delete` {name} (only an empty folder);
- `model_save` {name, data (base64), overwrite}, `model_get` {name}: 3D models (`.stl`, `.obj`);
- `hal_list`, `hal_values` {names}, `hal_watch` {names} (then `hal` messages `{v: {name: value}}` at `HAL_HZ`), `hal_set` {name, value, kind: pin | param | sig} (with `HAL_WRITE`);
- `scope_start` {names (≤ 8), rate (10–2000 Hz)}, `scope_stop`: then `scope` messages every 50 ms, `{names, n, rate (measured), missed (late samples), t0, d}` with `d` the n×channels samples as base64 little-endian float32, row by row (two browsers may scope at once);
- `drive_watch` {name | null}: a drive's console in this browser (the reply has what it said so far, `scroll`; then `dout` messages `{n, text}`); `drive_cmd` {name, cmd, sure}: a line to its console (reading for everyone; the rest with control and `DRIVE_WRITE`, and what moves, restarts or rewrites the drive with `sure`); `drive_detach` {name, on, seconds}: the tty left to other programs for a while (with control). `drv` messages (2 Hz, when changed): each drive's card, `{n, link: ok | silent | missing | detached, state, fault, last, last_at, dc, hv_t, mot_t, ac, perr, sat, fw, rx, tx, age, pd: {enable, fault, pos_fb, vel_fb, current, power}}`;
- `config_get`, `config_check` {text, custom}, `config_save` {text, custom, note, restart}, `config_version` {version}, `bitstream_save` {name, data}, `bitstream_delete` {name}, `mmodel_save` {name, data}, `mmodel_get` {name}, `mmodel_delete` {name} (the 3D model's files), `log_get` {which, n}, `sup_status`, `lcnc_restart`, `lcnc_stop` (with millweb-supervisor; `sup` messages carry its state). While LinuxCNC is down the supervisor answers these (`lcnc_start` for Start; its hello has `machine: null` and `supervisor`);
- `views_save` {views}: the Debug page's views for every browser (then a `views` message to all; also sent at hello). A view is `{name, watch: [{kind, name}], scope: {ch, rate, tdiv, mode, trig}}`;
- `setup_save` {objects}: the 3D setup (each `{id, name, role, src: builtin:vise | builtin:stock | model:<path>, frame: work | machine, pos, rot, scale, look: solid | glass | hidden}`);
- `program_lines` {name, start, count ≤ 5000}: a window of a long program's listing;
- `ping`, `events` {n} (the last n events);
- `pcomp` {k, v}.

## Programs

The File page (connected to a machine) shows the program folder (`[MILLWEB] NC_DIR`,
else `[DISPLAY] PROGRAM_PREFIX`) a folder at a time, with the path above it:
- **Upload**: the button, or G-code files dropped on the list, into the folder shown. Asks before replacing a program. Up to 6 MB each; larger ones go into the program folder directly (network share, USB).
- **New folder**, **Rename / move** (a new name, or a path such as `jobs/acme/op1.ngc`; a program keeps its extension), **Delete** (asks first; a folder only when empty).
- Selecting a program shows its preflight; **Load** opens it in LinuxCNC.
- **3D models** of fixtures and workpieces (STL binary or ASCII, OBJ; up to 5 MB) upload the same way. Selecting one shows its size and triangle count; **+ fixture** / **+ workpiece** adds it to the 3D setup. A model the setup uses can't be deleted, renamed or moved.

## The Debug page

HAL debugging in the browser, as LinuxCNC's `halshow`, `halmeter` and `halscope`, and the stmbl drives' own state and console, on three tabs with saved views above them.

**HAL tab**
- **Left:** every pin, signal and parameter, grouped by component, with a filter (name, component or signal). Each shows its type, direction, value (the rows on screen refresh once a second) and what it is linked to. **Refresh** reads the HAL again.
- **Click** one to watch it: on the right its value comes 10 times a second (`[MILLWEB] HAL_HZ`), bits as lights, numbers with a trend of the last 20 s. Below, its details: a pin's signal with that signal's driver and readers, a signal's driver and readers, each a link. The watch list is kept in the browser; it streams only while the page is open. **∿** puts a watched one on the scope.
- **Setting values** (`halcmd setp` / `sets`) is off. With `[MILLWEB] HAL_WRITE = yes` and control taken, an unconnected input pin, a writable parameter or a signal nothing drives can be set; each change is in the event log with who made it. It changes the running machine: a debugging aid, not for production.

**Scope tab**
- Up to 8 channels (pins, signals or parameters), sampled at 100–2000 Hz, 1 ms to 2 s a division. Each channel has its colour, units per division and the value at the middle line; **auto** sets them from what is shown (the first capture does it by itself).
- **Modes:** *auto* (a screen even without a trigger), *normal* (only on a trigger), *single* (one capture, then stop), *roll* (a moving strip). The trigger is a channel crossing a level, rising or falling, at a position on the screen.
- **Cursor:** hover the screen for the time from the trigger and each channel's value.
- The scope runs on when you change tab or page: arm a single capture, go and jog, come back to it. **Stop** stops it, and it stops when the browser goes.
- **How it samples:** millweb starts `server/sampler.py`, a process per scoping browser that reads the values with the `hal` module at the rate set, and streams them to the browser, which does the triggering and drawing. It runs in userspace. It is steady to a fraction of a millisecond, and the info line shows the measured rate and late samples. It is not halscope's real-time sampling, which is locked to the servo thread: for events within a servo period, use halscope.

**Drives tab** (for stmbl drives, `[MILLWEB] DRIVE_LINK`)
- **A card per drive:** its state (DISABLED, ENABLED, PHASING, SOFT/HARD FAULT), the fault decoded, or the last one with its time, DC bus voltage, power stage and motor temperatures, AC current, position error and saturation (asked over the drive's console), and its smart-serial process data from HAL (enable, fault bit, position, velocity, current, power). The footer says how old the values are and how many bytes went each way. The drive's own messages (`INFO: Enabled`, `ERROR: Fault 5: position error`, warnings) go into the event log.
- **The console:** the drive's terminal as over USB, through its smart-serial link with `sserialterm` (docs/sserial_console.md, about 1 kB/s): `about`, `showconf`, `list`, a pin's name (`fault0.dc_volt`, or a prefix, `fault0`), ↑/↓ for the last lines. Every browser on the tab sees the same console. Reading is open to everyone. Setting pins and other commands need `[MILLWEB] DRIVE_WRITE = yes` and control, and are logged with who sent them; the page asks before anything that moves the motor (`jogl`, `jogr`), stops or restarts the drive (`start`, `stop`, `reset`, `bootloader`) or rewrites its flash or firmware (`flashsaveconf`, `fw*`). The drive takes 63 characters a line.
- **Detach:** millweb keeps the tty open (a pty nobody reads drops what the drive says), so `servoterm-cli -d /tmp/drive-x` can't use it meanwhile. **Detach** (with control) lets go of it for 10 minutes or until **Attach**, e.g. for `servoterm-cli --dump` before flashing.
- **The link is slow:** a card's values are asked one at a time, only when the console is quiet (`DRIVE_POLL_MS`), so a full card takes about 2 s and typing goes first. A fault that clears within that time (LinuxCNC takes the enable away at once) still shows as the last fault, from the drive's own message.

**Views** keep the watch list and the scope's set-up under a name. **Save view** saves the current ones (an existing name is replaced); a click on a view, or **Alt+1…9**, opens it; **×** deletes it. On a machine they are kept for every browser (`[MILLWEB] DEBUG_VIEWS`, no control needed: they don't touch the machine); in the simulation they are kept in this browser.

The structure comes from `halcmd show` (millweb runs in LinuxCNC's environment), the values from the `hal` module.

## The 3D setup

What stands on the table in the 3D view: the built-in vise and stock by default, and
models from the program folder. millweb keeps it (`[MILLWEB] SETUP`, default
`millweb-setup.json` beside the INI), so every browser shows the same; changing it
needs control.
- **Click** an object in the view to select it (a drag still orbits): it lights up, and its panel shows its **role** (fixture or workpiece), where it is **placed in** (the work offset: it moves with G5x, G92 and the rotation; or the machine), its position, rotation (X, Y, Z in degrees) and **scale** (25.4 for a model drawn in inches).
- **Top at work zero** puts its top at work Z0 and its front-left corner at X0 Y0; **On the table** sets its bottom on the table; **Turn 90°**, **Tip 90°**; **Remove**.
- Each object is solid, see-through or hidden; the **fixtures** and **workpiece** buttons above the view do the same for all of them at once.
- A new workpiece goes to the work zero, a new fixture onto the table under it.


## On a tablet

The console is laid out for tablets as well as desktops, tested at the usual
iPad (mini, 10.2", Air, Pro 11" and 12.9") and 10" Android sizes, both ways
round (`make test-gui-tablet`):
- **Held sideways** (961–1440 px): the three columns as on a desktop, the header on one row with the E-STOP at its right end (the status chips wrap among themselves, the program name shortens, the clock goes below 1280 px), a lower nav rail on short screens. The main page scrolls to show the program and MDI at full height.
- **Upright** (up to 960 px): one column, the nav as a row, the page scrolls. When the header's E-STOP scrolls out of sight a **floating E-STOP** appears at the bottom right.
- **Touch:** controls are at least 30 px (larger buttons, segments, inputs and sliders on a touch screen). A quick second tap doesn't zoom (`touch-action: manipulation`; pinch zoom still works), and taps or holds on controls don't select text or open the callout (a held jog button did on an iPad). G-code, inputs, the event log and the drive console stay selectable. A second tap on a selected folder opens it (no double-click on a tablet).

## Safety, as built

- **The E-STOP chain is hardware**: stmbl's fault inputs and the drives' quick stop. The page's E-STOP only asks LinuxCNC for `STATE_ESTOP`, and any browser, watching or not, may press it.
- **One browser has control** at a time, the others watch. Control goes away when its browser disconnects. `REMOTE_ENABLE_PIN` forbids remote control at the machine, and `TOKEN` asks for a password.
- **Continuous jogging needs a keep-alive:** a message at least every 300 ms, or millweb stops the jog. A lost connection stops it at once.
- **E-STOP, Stop and jog stop never wait** behind another command (a tool measurement takes 30 s): they go to LinuxCNC from their own thread on their own command channel. After a Stop or an E-STOP, a running sequence (tool setter, probe, a multi-point move) sends nothing more.
- **Checks before LinuxCNC:** commands are checked (E-STOP, machine on, homed, idle) first, and LinuxCNC checks everything again.
- **A program can't take millweb down.** The preview interprets in a child process (killed after `PREVIEW_TIMEOUT`); the server keeps answering while it works.
- **The work offset shown is LinuxCNC's.** When LinuxCNC's status comes up without it (a fast start, DEBUGNOTES), the DRO says "G5x?" and millweb has LinuxCNC publish it again once the machine is homed and idle, and before any MDI.
- **Probe moves stay inside the soft limits.** LinuxCNC refuses a G38 move whose end lies beyond them, so millweb shortens the travel to what's left.
- **Program files** stay inside the program folder: paths of plain names (no `..`, nothing hidden, no links out), at most 8 folders deep, G-code extensions only. The loaded program, and the folder it is in, can't be renamed, moved or deleted.
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
| `make test-gui-tablet` | every page at iPad and Android tablet sizes, both ways round, as a touch screen: nothing to scroll sideways, every control tappable and 30 px or more, the header and E-STOP, touch CSS (`TABLET_URL=` for a machine) | 2 min | node, Playwright |
| `make test-backend` | the protocol case by case on real LinuxCNC (`test/sim`) | 7 min | LinuxCNC, a non-root user |
| `make test-gui-live` | the console on real LinuxCNC through its own controls | 3 min | all of the above |
| `make test-live` | one end-to-end scenario: the protocol, then the page | 1 min | all of the above |
| `make test-stress` | large programs: a million lines, 2 million moves, 20 000 peck cycles, arcs, real CAM output (inches) | 10 min | all of the above |
| `make test-config` | the Configure page and restarts: test/machine run by millweb-supervisor (backend cases and the page) | 2 min | all of the above |
| `make test-drive` | the Drives tab on LinuxCNC with a simulated stmbl drive on X (`harness.py --drive`: `stmbl_sim` closing joint 0's loop, `sserialterm`) | 1 min | all of the above, `stmbl_sim.so` and `sserialterm.so` installed |
| `make check-all` | unit, simulated, tablet, backend and live GUI suites | 17 min | |

**On a small board** (LinuxCNC installed from packages, little RAM): the harness
takes a run-in-place tree from `LINUXCNC=path` (or `~/linuxcnc-2.9`), else the
installed `linuxcnc` on PATH. The stress suite finds LinuxCNC's `3D_Chips.ngc` in
the tree, `/usr/share/linuxcnc/ncfiles` or `~/linuxcnc/nc_files` (else that case
is skipped), and reads its million-line program a line at a time.
The suites' temporary machines go where `TMPDIR` points (the stress programs are
~45 MB): set it off a small `/tmp` tmpfs. millweb itself doesn't stage uploads in
`/tmp`; it writes them beside their final name in the program folder.

**The heavy suites from another computer.** Chromium and the stress programs are
too much for a small board next to LinuxCNC. Run the test machine on the board and
the suites on a PC:

```sh
# on the board: the test machine (test/sim) on the network; STRESS=1 writes the
# stress programs there too (generated on the board: nothing big crosses the network)
make -C webui/mill serve-test PORT=8080 STRESS=1        # Ctrl-C stops it
# on the PC (node, Playwright, Chromium; no LinuxCNC needed)
make -C webui/mill test-gui-live REMOTE=board:8080
make -C webui/mill test-stress   REMOTE=board:8080
```

Remote, the suites check the machine's files through the protocol instead of on
disk, skip the case that would stop millweb, clean up what earlier runs left on the
board, and refuse any machine but the test one (`PORTAL-M sim`): they take control,
home, jog and run programs. The light suites (`make test`, `make test-backend`)
run on the board itself.

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
tablet widths; the loading screen (shown while loading, a failed script named); over HTTP without millweb (the simulation after 6 s) and
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
