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
| `server/driveconf.py` | the drives' firmware versions against this console's; their configs read, backed up, patched and restored over the console |
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
| `type` | `gantry` (default: the bridge moves in Y over a fixed table; a Y pair allowed) or `bridge` (a fixed bridge, the table moves in Y, one joint an axis: the Scripta CN 720, `machines/cn720/machine.json`). The console draws that machine: its view keeps the bridge still and moves the table, the work and the toolpath with −Y. Or `lathe` (X and Z: the Emco Compact 5, `machines/emco_compact5`; see *Lathes*). In the INI as `[MILLWEB] MACHINE_TYPE` |
| `lathe` | a lathe's: `post` (`front`: the tools between the operator and the spindle's axis, +X toward the operator; `back`), `diameter` (X in diameter, G7, from the start; default true). In the INI: `[DISPLAY] LATHE`, `BACK_TOOL_LATHE`, `RS274NGC_STARTUP_CODE = G18 G7 …`. `turret` (see *Lathes*): `type` `none`, `sim`, `stepper` (`stepgen`: its HAL prefix, `steps` a turret turn, `index`: the once-a-turn pulse's pin, `index_invert`), `stations`, and its moves in turret turns: `offset` (station 1 after the index), `overtravel`, `lock`, `speed`, `lock_speed`, `acc`, `dwell`, `timeout`, `slip` |
| `board` | `type`: `none`, `hm2_eth`, `hm2_spix`, `hm2_rpspi`, `hm2_soc_ol`; `name` (in its pins: `7i96s` for `hm2_7i96s.0…`), `params` (the driver's: `board_ip=…`, `spiclk_rate=…`), `config` (`config="…"`), `watchdog_ms` |
| `bitstream` | `loader`: `none`, `fpga_manager`, `command`; `file` (in `bitstreams/`), `fpga` (`fpga0`), `command` (with `{file}`), `reload`: `changed` or `always` |
| `axes` | `X`, `Y`, `Z` …: `min`, `max`, `max_vel`, `max_acc` (mm, mm/s, mm/s²) |
| `joints` | a list: `axis`; `drive`: `{type: sim}` (command = feedback), `{type: stmbl_sserial, port, channel}` (on the Mesa board's smart-serial), `{type: stmbl_can, bus, node}` (stmblcan; `can.spi` lists the buses), `{type: stmbl_sim, conf, sim}` (the simulated drive, one at most), with `console` (its console on the Drives tab, default yes); `mech`: `lead` (mm a screw turn), `ratio` (motor turns a screw turn: 2 for a 2:1 belt), `reverse` (a stmbl drive with a lead gets `scale` comps between LinuxCNC's mm and its motor radians; no lead: the drive works in mm itself); `ferror`, `min_ferror`; `home`: `method` (`absolute`, `immediate`, `switch` with `switch`/`search_vel`/`latch_vel`/`offset`, `index` with `latch_vel`/`offset`), `pos`, `sequence` (two joints on an axis: a gantry, homed in step); `min`, `max`, `max_vel`, `max_acc` (default: the axis's, travel ±1 mm, speeds ×1.1) |
| `can` | `spi` (a device a bus, or `sim`), `discover`, `spi_hz` |
| `estop` | `input` (a HAL pin, true while the chain is closed), `invert`; a Mesa board's watchdog also trips it; no input and no board: a loop |
| `probe` | `type`: `none`, `input` (`input`, `invert`), `sim` (trips below `sim_z`) |
| `spindle` | `type`: `none`, `sim`, `hal` (its pins: `on`, `forward`, `reverse`, `speed` (rpm, `spindle.0.speed-out-abs`), `at_speed` (in; blank: at speed at once), and its encoder for threading and G95: `revs` (spindle.0.revs, turns), `index` (index-enable), `speed_in` (turns/s)); `max_rpm`, `min_rpm`. A simulated spindle on a lathe gets a simulated encoder (`sim_spindle`), so G33 and G76 run. Or `stmbl_sserial` (`port`, `channel`) / `stmbl_can` (`bus`, `node`): an stmbl drive in velocity mode through `lathe.spindle` (linuxcnc/lathe), `ratio` motor turns a spindle turn, `index_sensor` (a once-a-turn sensor's pin; blank: the motor's zero angle), `console`; its fault opens the E-STOP chain; on the Drives page as `S` |
| `atc` | `type`: `none` or `rack`: M6 is remapped to `run/toolchange.ngc`, made from these: tool n in pocket n (`pockets`), pocket 1 at `x`, `y`, the others `dx`, `dy` apart; `z_clear`, `z_pocket`, `slide` (the holder enters its fork along +Y; 0: from above), `feed`; pins `unclamp` (the drawbar, motion's digital out 0), `air` (out 1), `clamped`, `released` (sensors, in 0 and 1: waited for up to `timeout` s; blank: `dwell`). A tool beyond the pockets is changed by hand (a message and a pause) |
| `toolsetter` | `x`, `y`, `z_above`, `z_ref`, `travel` (the Tools page's Measure), or null |
| `display` | `max_feed_override`, `max_spindle_override`, `min_spindle_override` |
| `stmbl` | the drives' firmware: `fw` (null: this checkout's `STMBL_FW_VERSION`), `block` (a mismatch keeps the machine off; default true) |
| `model` | the machine in the 3D view: `builtin` (the built-in machine of `type`, default on), `parts`: `{name, file (in models/, STL or OBJ), moves ("" fixed, "Y", "XY", "XYZ", "-X" against X for a moving table …), pos, rot (°), scale, color, opacity}`, each placed as it is with the machine at X0 Y0 Z0 and the tool tip at the origin. A lathe's: the spindle's axis along X, the chuck's face at X0, `tip` where the tool's tip is in the files, `moves` `Z`, `XZ` or `S` (turns with the spindle); `tools/step2parts.py` makes them from a STEP assembly |
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
- **Machine, Axes & joints, Hardware, Spindle, Tools & probe, I/O, Console:** machine.json in forms (a joint per card; the drive's and the homing's fields follow their type; a joint's screw lead and ratio show its mm a motor turn and the motor's rpm at top speed). Blank fields take the default shown. Tools & probe holds the probe, the tool setter and the rack tool changer.
- **HAL pins:** each pin field has **Pins…**, a pin browser: LinuxCNC's pins while it runs (with their values and signals), else the names the board would give (said so), only the direction that fits (an input from the machine: pins that drive; an output: pins that are driven); type to filter, Enter or a click picks.
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
| `PCOMP` | pcomp.0 | pcomp's HAL prefix; its switches are on the Dynamics page when it is loaded |
| `[DISPLAY] MAX_FEED_OVERRIDE`, `MAX_SPINDLE_OVERRIDE`, `MIN_SPINDLE_OVERRIDE` | 1.0, 1.0, 0.5 | the sliders' ranges: LinuxCNC clamps the overrides to these (and still reads back the value asked for) |
| `SETUP` | `millweb-setup.json` beside the INI | the 3D setup (fixtures and workpieces in the view) |
| `CPUS` | | millweb, its threads and its preview processes on these cores only (`0`, `0,1`, `0-1`): keep them off LinuxCNC's real-time core |
| `PREVIEW_NICE` | 10 | the preview process's niceness (lower priority than millweb and LinuxCNC) |
| `HAL_HZ` | 10 | the Debug page's watch rate |
| `HAL_WRITE` | no | `yes`: the Debug page can set unconnected input pins, writable parameters and undriven signals |
| `DRIVE_LINK` (repeat) | | an stmbl drive for the Drives tab: `name tty [hal-prefix]`, e.g. `X /tmp/drive-x hm2_7i96s.0.stbl.0.0` (its console through `sserialterm`, or its USB port; the prefix of its smart-serial pins) |
| `DRIVE_WRITE` | no | `yes`: with control, the Drives tab's console may set pins and run commands, not only read |
| `DRIVE_FW` | this checkout's `STMBL_FW_VERSION` | the stmbl firmware version the console expects of its drives (docs/firmware_versions.md) |
| `DRIVE_FW_BLOCK` | yes | a drive with another firmware (or HV board, or a config needing a patch) keeps the machine off; `no`: only said |
| `DRIVE_POLL_MS` | 250 | a drive's card values are asked one at a time, at most this often, while its console is quiet |
| `VIBMON` | | vibmon's socket (tools/vibmon): the Measure page, the vibration sensors (docs/vibsensor.md) |
| `VIBMON_STATE` | yes | send vibmon the machine state (spindle speed, feed, tool and its teeth from a "3-flute" in the tool's comment) five times a second; `no` when vibmon reads it from HAL (`--hal`) |
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
| `slow` | 2 Hz | HAL pins, drives, load, bus, yaw, contour, pcomp's switches, the remote-enable key; an arm's `arm`: its components' pins (see *Arms*) |
| `program` | when a program is loaded | name; `nlines` and the first 2000 `lines` (from `line0`); `path`: `[kind, x, y, z, feed, line, tool]` per segment, each from where the one before ended (`start`), from LinuxCNC's interpreter (kind 0 rapid, 1 feed, 2 probe, 3 tool change), in the active work coordinates; `stats` over every move: `moves`, `shown`, `stride` (past `PREVIEW_SEGS` the drawn path merges neighbouring moves), `ext` (travel), `cut` and `rapid` lengths, `time`, `tools`; `error`, `truncated` |
| `tools`, `offsets`, `files` | on change | the tool table, G54…G59.3 and G92, the program folder (`files` with their paths and `kind`: program or model, `dirs`) |
| `setup` | on connect, on change | the 3D setup's `objects` |
| `control` | on change | who has control |
| `ev` | as they happen | `{time, lvl, text}`: LinuxCNC's error channel, state changes, commands |
| `reply` | to each request | `{id, ok, err}`, plus a result (probe, tool setter, preview) |

**From the browser:** `{"op": ..., "id": n, ...}`:
- `control` {take, force, token};
- `estop` {on} (any browser), `power` {on};
- `home` / `unhome` {axis | all | J<n> (a joint)};
- `jog` {axis, dir, inc (0: continuous), rate}, `jog_stop` {axis}: an axis (X..W the machine has: world mode) or a joint `J<n>` (joint mode, before homing too), rate in units (mm or °) a minute;
- `teach_record` {rate}: an arm's Record point (see *Arms*);
- `arm_model`: an arm's 3D model files (see *Arms*; any browser);
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
- `drive_watch` {name | null}: a drive's console in this browser (the reply has what it said so far, `scroll`; then `dout` messages `{n, text}`); `drive_cmd` {name, cmd, sure}: a line to its console (reading for everyone; the rest with control and `DRIVE_WRITE`, and what moves, restarts or rewrites the drive with `sure`); `drive_detach` {name, on, seconds}: the tty left to other programs for a while (with control). `drive_conf` {name, action: backups | backup | plan | patch | restore | reset, backup, to, sure}: a drive's flash config (driveconf.py). A card has `ver` {fw, hv, hv_image, conf}, `fw_want`, `problems` [{level, block, action, text}], `job`. `drv` messages (2 Hz, when changed): each drive's card, `{n, link: ok | silent | missing | detached, state, fault, last, last_at, dc, hv_t, mot_t, ac, perr, sat, fw, rx, tx, age, pd: {enable, fault, pos_fb, vel_fb, current, power}}`;
- `config_get`, `config_check` {text, custom}, `config_save` {text, custom, note, restart}, `config_version` {version}, `bitstream_save` {name, data}, `bitstream_delete` {name}, `mmodel_save` {name, data}, `mmodel_get` {name}, `mmodel_delete` {name} (the 3D model's files), `log_get` {which, n}, `sup_status`, `lcnc_restart`, `lcnc_stop` (with millweb-supervisor; `sup` messages carry its state). While LinuxCNC is down the supervisor answers these (`lcnc_start` for Start; its hello has `machine: null` and `supervisor`);
- `views_save` {views}: the Debug page's views for every browser (then a `views` message to all; also sent at hello). A view is `{name, watch: [{kind, name}], scope: {ch, rate, tdiv, mode, trig}}`;
- `setup_save` {objects}: the 3D setup (each `{id, name, role, src: builtin:vise | builtin:stock | model:<path>, frame: work | machine, pos, rot, scale, look: solid | glass | hidden}`);
- `program_lines` {name, start, count ≤ 5000}: a window of a long program's listing;
- `ping`, `events` {n} (the last n events);
- `pcomp` {k, v}.

## Arms

A LinuxCNC config whose kinematics are an arm's (`genserkins`, `armkins`, `pumakins`, ...; or `[MILLWEB] MACHINE_TYPE = arm`)
is an arm to millweb (`hwtest/arcmate-sim`: the ARC Mate with its guard, gripper and tool changer). Its joints are not axes:
- **`hello`:** `machine.type` `arm`, and `machine.arm`:
  - `joints`, `axes` (`XYZABC`);
  - `jlim`: each joint's min, max, vel (per minute), acc, type, from `[JOINT_n]`; `alim`: the axes';
  - `teach`: the teach file's name; `grip` {open, close}: the gripper's MDI (`[ARM6HMI] GRIP_OPEN` / `GRIP_CLOSE`, default `M65 P0` / `M64 P0`); `tool_ready` (`[ARM6HMI] TOOL_READY`).
- **`st`:** `homed` (all three: all joints homed), and:
  - `ax`, `axm`: every axis in the work offset and in machine coordinates;
  - `j`, `jh`, `jfe`: each joint's position (LinuxCNC's joint `input`, °), homed, following error;
  - `jmode`: `joint` or `world` (teleop).
- **Jog:** `J<n>` jogs a joint in joint mode (before homing too), X..C the axes in world mode (homed); millweb switches LinuxCNC's mode first and waits for motion to take it (up to 1 s). Home a joint with `home` `J<n>`.
- **`teach_record` {rate}:** the arm's position as `G1 X.. Y.. Z.. A.. B.. C.. F<rate>` at the end of the teach file (`[MILLWEB] TEACH_FILE`, else `[ARM6HMI] TEACH_FILE`, else `teach.ngc` in the program folder); the reply has the line and the number of points, and the file list is sent again.
- **`arm_model`:** `{files: {name: text | {b64: STL}}, model, cell, tools}`: the 3D model (`[MILLWEB] ARM_MODEL`, else `[ARM6HMI] MODEL`, else linuxcnc/vis3d's `arm6_changer.vis` with a tool changer, `arm6_lcnc.vis` without), every file it `include`s or loads (`part … mesh FILE.stl`), armguard's cell (`[ARMGUARD] CELL`) and the tool table (`[STMBL_ARM] TOOLS`) with theirs. Names are paths from the model's folder; in the texts each include and mesh names its file as `@<name>` (the files vis3d resolves from the including file's folder).
- **`slow`'s `arm`:** each loaded component's pins, `-` as `_`:
  - `guard` (armguard.0: state, reason, clearance);
  - `collide` (armcollide.0: collision);
  - `grip` (armgrip.0: state, gap_fb, payload_mass, close);
  - `tool` (armtool.0: mounted, fault, reason, valve);
  - `changer` (stmbl-arm.0.changer: locked, unlocked, present, id);
  - `quota` (powerquota.0: total_power, feed_scale, limiting);
  - `kins_tool` (armkins.tool).

**The console on an arm** (`js/arm.js`, `js/armmodel.js`; linuxcnc/arm6hmi is the same as a QtVCP screen):
- **The 3D view:** the arm from its model files, posed from LinuxCNC's joints (Craig DH as genserkins: the browser's forward kinematics give LinuxCNC's own X Y Z A B C, `test/armmodel_test.js`). The tools on the flange or in their docks (with their stands), the cell's obstacles see-through, green while the guard is clear, amber under twice its limit, red stopped. The tool point marked; the program's path and the trail as a mill's.
- **The DRO:** X Y Z A B C (the tool point).
- **Joints panel** (in place of the spindle's): each joint within its limits (LinuxCNC's `[JOINT_n]`), its angle, following error, homed, REF homes it. Below:
  - the guard: ARMED / NEAR / STOP and why, the clearance against its limit;
  - the gripper: its state and gap, Open / Close (the MDI of `[ARM6HMI] GRIP_OPEN` / `GRIP_CLOSE`);
  - the power quota;
  - **Record point** (`teach_record`).
- **JOG:** JOINT (J1..J6, joint mode, before homing too) or WORLD (X..C, homed), the increments or CONT (hold the key).
- **Tools page:** the tool changer's tools, IN DOCK / ON FLANGE, **Mount** (`T<n> M6`, after `[ARM6HMI] TOOL_READY` when set), **Put the tool back** (`T0 M6`); the lock, its sensors, the kinematics' tool point, a fault and its reason.
- **The simulation:** machine.json's `type: arm` gives the ARC Mate with its tool changer (`js/arm_demo.js`, made by `tools/gen_arm_demo.js` from linuxcnc/vis3d and hwtest/arcmate-sim), a pick-and-place program; its joints follow the program's tool point by IK (orientation held), the jog pad's joints by forward kinematics.
- **Tests:** `make test-gui-arm` runs `test/gui/arm_suite.js` against LinuxCNC with hwtest/arcmate-sim and its tool changer (`harness.py --arm`): the model and panels, homing, a joint and a world jog, MDI, the guard, Record point, three tool changes with armtool, LinuxCNC and the kinematics agreeing, the gripper.

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
- **Firmware versions** (docs/firmware_versions.md): each card shows the drive's firmware, its HV board's and its config's version against the one this console expects, and what is wrong: another firmware (flash it), an HV board on another one (`hv_update`), a config for another version (patch it). Those keep the machine off (`DRIVE_FW_BLOCK`). **Config…** opens a drive's flash config: *Back up now*, *Patch for vN…* (each step shown, then written, read back, the one before kept), *Restore* a backup, *Reset drive*. Backups are kept in `drive-backups/<drive>/` (the machine folder, or the INI's); writing needs control, `DRIVE_WRITE` and the machine off.
- **Detach:** millweb keeps the tty open (a pty nobody reads drops what the drive says), so `servoterm-cli -d /tmp/drive-x` can't use it meanwhile. **Detach** (with control) lets go of it for 10 minutes or until **Attach**, e.g. for `servoterm-cli --dump` before flashing.
- **The link is slow:** a card's values are asked one at a time, only when the console is quiet (`DRIVE_POLL_MS`), so a full card takes about 2 s and typing goes first. A fault that clears within that time (LinuxCNC takes the enable away at once) still shows as the last fault, from the drive's own message.

**Views** keep the watch list and the scope's set-up under a name. **Save view** saves the current ones (an existing name is replaced); a click on a view, or **Alt+1…9**, opens it; **×** deletes it. On a machine they are kept for every browser (`[MILLWEB] DEBUG_VIEWS`, no control needed: they don't touch the machine); in the simulation they are kept in this browser.

The structure comes from `halcmd show` (millweb runs in LinuxCNC's environment), the values from the `hal` module.

## The Measure page

The vibration sensors (docs/vibsensor.md) through vibmon (`tools/vibmon`,
`[MILLWEB] VIBMON`).

- **The bar:** vibmon's link, the alarms (crash, a sensor lost, vibration
  and temperature levels), the machine state the sensors get (rpm, teeth,
  feed). Buttons: **Reset crash**, **Identify**, **Rescan**. A crash or an
  alarm level turns the MEASURE button red on every page.
- **A card per sensor:**
  - velocity (mm/s, ISO's measure), coloured by its level, with a two-minute
    trend;
  - acceleration RMS per axis, peak, kurtosis, temperature;
  - the spindle's 1× 2× 3× orders, tooth passing, chatter (frequency and
    level), the bearing band.
- **Spectrum:** a sensor's spectrum on request or every second, over 1 kHz,
  5 kHz or all. Markers show the spindle's orders, tooth passing and the
  chatter peak; hovering reads a frequency and level.
- **Sensors:** each one's unique ID, node, name and position.
  - **Identify,** then tap a sensor on the machine: its row lights up; give
    it a name and a position and **Save** (kept in the sensor and in
    vibmon's sensors.json).
  - **replaces…** hands a missing sensor's place to a new one.
- **Sensor console:** a line to a sensor (`vib0.fft_n = 4096`,
  `vib0.shock_lim = 10`, `appendconf …`, `flashsaveconf`) and its answer.

**Who may do what:** reading, the spectrum and the trend are open to
everyone. The console, Reset, Identify, naming and Rescan need control, and
console lines are logged. Crashes, lost sensors and level changes go into
the event log.

**Simulation:** four simulated sensors on a spindle at 9000 rpm.

**Ops:**
- `vib_spectrum` {node}, `vib_trend` {node}, `vib_console_read` {node}:
  reading.
- `vib_console` {node, line}, `vib_reset` {node | null}, `vib_identify` {on},
  `vib_assign` {uid, name, position}, `vib_replace` {old, new},
  `vib_rescan`: with control.

**Messages:**
- `vib` {up, status}: vibmon's status, every 0.2 s;
- `vibev` {event};
- `vibspectrum` {node, spectrum: {df, rms}};
- `vibtrend`, `vibconsole`.

## Lathes

`type: "lathe"` makes the console a lathe's (from LinuxCNC's `[DISPLAY] LATHE` when it runs):
- **DRO:** X as a diameter (Ø, the radius below it) in G7, as a radius in G8, following LinuxCNC's modal state; Z; no Y. Positions stay radii, as LinuxCNC keeps them.
- **The view:** the spindle's axis along the screen, the chuck turning with the stock in it, the saddle and cross slide following the tool. XZ is the operator's view (+X down for a front post, up for a back one). The path is drawn over the stock and mirrored across the axis, faint: the part's section.
- **Jog:** left/right Z, up/down X (up: away from the operator on a front post).
- **Tools:** the turret's tools with X and Z offsets, nose radius, orientation (Q 1–9, LinuxCNC's: Q2 an outside turning tool, Q3 a boring bar, Q6 a thread or parting tool across, Q7 a drill on the axis, as AXIS draws them), the insert's shape, a grooving insert's width and the cutting edge's length (the tool database), drawn as the insert and its holder. tool.tbl keeps X, Q, I, J.
- **Wizards:** facing, turning (roughing passes to an allowance, then a finish), threading (G76), grooving and parting off. The programs are G18 G7 G95, at constant cutting speed (G96) where it fits.
- **Preview:** LinuxCNC's interpreter with the INI's startup code (X words as diameters), its time at the spindle's rpm in G95, from the cutting speed at the diameter in G96, and at the pitch on G33/G76 lines.
- **Probe and Dynamics:** not on a lathe.

**The tool database.** LinuxCNC's tool table has no place for a tool's flutes or its insert, so millweb keeps them in a tag at the end of the tool's comment, `;6 mm endmill {fl=3 cl=18}` (LinuxCNC keeps comments when it writes the table, a touch-off too). The Tools page edits them:
- **`fl`:** flutes (a mill's Tools page; the spectrum's tooth-pass line uses it).
- **`ins`:** a lathe insert's shape: `80`, `55`, `35` (rhombus), `60` (a thread), `groove`, `round`, `drill`.
- **`w`:** a grooving insert's width.
- **`cl`:** the cutting edge's length (a mill's: the flute length).
- **A mill's:** `ty` its type (`flat`, `ball`, `bull`, `drill`, `vee` (a chamfer or engraving cutter), `probe`; blank: guessed from the comment), `cr` a bull nose's corner radius, `ang` a drill's or a vee's point angle (118°, 90° when blank), `so` its stick-out (the tip to the holder), `hd` the holder's Ø. Blank: flutes 2–4 × Ø long, the stick-out the flutes and a Ø more, a holder 20 mm wider than the tool.

**Material removal** (`js/stock.js`, the browser's; no load on the board):
- **The stock:** Ø, a tube's bore, the length out of the chuck, its face's Z in the work's coordinates, the chuck's Ø. Set on the File page's Preflight card and kept with the 3D setup (`lathe` in millweb-setup.json).
- **The model:** the stock in 0.05 mm slices along Z, each a ring. Each move sweeps the tool's insert outline through it, from the tool table as AXIS draws it: orientation, nose radius, front and back angles (I, J), else the insert's shape. Exact for turned shapes, nose radius and fillets included. A thread can't be cut into a turned section: a 60° insert's passes along Z keep the major Ø and note the thread's depth (drawn as a saw).
- **The view:** the stock cut as the program runs (and as LinuxCNC runs it, line by line), coloured by the tool that cut each surface. A part cut off stays where it was. "Show the finished part" runs it all.
- **The check** (Preflight, every program shown on the File page, and each program loaded: a warning when it finds a crash risk):
  - G0 into the stock;
  - the tool or its holder into the chuck;
  - the holder in the part (a cut deeper than the insert reaches);
  - a cut deeper than 70 % of the insert's edge;
  - an outside tool past the axis;
  - a part off (where, and how long).

  Each finding gives its line (a tap shows it in the listing) and a mark in the view. Also shown: the volume taken, cutting against air time, each tool's material removal rate and deepest cut, the threads, the part.
- **Not modelled:** the jaws' shape (the chuck is a cylinder from its face), a holder other than a 12 mm shank (a Ø10 bar for an inside tool), tool deflection, chip load. A program the preview draws with merged moves (past 100 000) is checked on the merged path.

**The turret** (`lathe.turret`, linuxcnc/lathe): a stepper on step/dir (a stepgen in position mode), one index pulse a turret turn, locked by running backwards onto its pawl, as on the Emco Compact 5. M6 turns it; T only names the tool:
- **Homing:** the first change after a start or an E-STOP turns forward up to 1.5 turns to find the index (station 1 is `offset` after it).
- **A change:** forward to the station (never backwards: that is the lock) and `overtravel` past it, back by `overtravel + lock` at `lock_speed` onto the pawl, a `dwell`, done. Tool *n* is on station (*n* − 1) mod `stations` + 1. Every tool used needs a line in tool.tbl: LinuxCNC refuses a T it doesn't have.
- **Faults:** the index seen elsewhere than it should be (`slip`: lost steps, a stall), no index while homing, a change longer than `timeout`. Any of these opens the E-STOP chain; the reset after it homes the turret again on the next change.
- **The console:** the Tools page says the station, homed or not, and the fault.

**AC servos everywhere** (`machines/emco_compact5/machine-stmbl.json`): stmbl drives on X, Z and the spindle on smart-serial, the turret's stepper on a stepgen, and an EBAZ4205 (Zynq) card running hostmot2 (`hm2_soc_ol`, its bitstream through `fpga_manager`). The board's name, its stepgen and GPIO numbers, and the X/Z leads in the file are placeholders: set them to the card's bitstream and the machine. The spindle drive's config is velocity mode, as `conf/haas_spindle.txt` (`pid0.pos_en = 0`).

`machines/emco_compact5` is an example: simulated joints, spindle (with an encoder: G76 threads) and turret.
Its 3D model comes from the machine's STEP file through `tools/step2parts.py`; see its README.

**Two lathes in one machine** (a twin-spindle lathe such as the CTX TWIN, a controller each side) are
not in yet. The plan: each side its own LinuxCNC and millweb. A part's hand-over is a handshake in
M-code subroutines on both sides:
- **Signals:** "at transfer position", "clamped", "released", as a few 24 V lines between the two I/O cards, or a network link.
- **Moves:** each side waits for the other (M66) and moves (M64/M65).
- **Simplest transfer:** both spindles stopped and oriented (M19).
- **Synchronous transfer:** the receiving spindle's drive follows the giving one's encoder.

## Messages, progress, run statistics

- **Warnings and faults** come up as a large card over the page (warnings go after 10 s, faults stay until a tap or a key; the card takes no clicks, so what is under it still works). The footer's line, now larger, opens the last message again; the Status page has them all.
- **Uploading and loading a program** show a bar: reading the file, sending it (from the socket's buffer), LinuxCNC loading it and making the preview, then drawing it. A long path is turned into the 3D view in slices, the page staying alive between them.
- **STATS** takes the MDI / JOG place while a cycle runs and gives it back after: run time (and on hold), pace against the estimate, G-code lines a second and lines run, maximum and mean velocity, contour error, the largest following error of each joint (and the gantry's Y1−Y2 skew), spindle load. The last run's numbers stay under STATS until the next run or Reset.

## Material removal (a mill)

`js/mstock.js`, in the browser (no load on the board), the lathe's (`js/stock.js`) counterpart:
- **The stock:** the 3D setup's workpiece: the built-in block, or a model's triangles (between their highest and lowest point at each place). A height map over it, 0.25 mm cells (coarser past 250 000 cells). The fixtures (the vise's base and jaws, models) as a height map of their own.
- **The cut:** each move lowers the cells under the tool to its lowest point over them. It is exact for a flat end mill on any straight move, ramps included, and sampled for ball, bull nose, drill and vee profiles. Arcs come as short moves.
- **The view:** the stock cut as the program runs (the simulation's and LinuxCNC's), coloured by the tool that cut each place. It replaces the block while there is something cut (a click on it selects the workpiece). It is drawn coarser than it is cut (at most 40 000 points, fewer on a device that draws slowly); "Show the finished part" runs it all.
- **The check:** on the File page's Preflight (Material removal), for every program shown, and with a warning when a loaded program has a crash risk:
  - G0 into the stock;
  - a cut deeper than the flutes (the shank in the material);
  - the holder in the stock (the stick-out too short);
  - the cutter or holder into a fixture;
  - a probe into the stock off a probing move;
  - cutting after M5 (the simulation's programs; LinuxCNC's preview doesn't say);
  - through the stock's bottom (a note).

  Repeats are one finding with their count; a tap shows the line, and marks sit in the view. Also shown: the volume, cutting against air time, each tool's removal rate, deepest cut and chip load (from its flutes and the spindle's speed, when the program gives it).
- **Against the finished part:** a model in the setup with the role *finished part* (the File page's **+ part**, or the object's panel), placed like the stock, see-through. After the whole run its top is compared with the stock's, cell by cell:
  - a **gouge** (the stock below the part by more than ±0.05 mm) is a crash risk, given with the line and tool that cut it;
  - **stock left** is its volume and thickness;
  - the share within ±0.05.

  A wall inside a cell counts as neither: a gouge is measured from the part's lowest point among a cell and its neighbours, stock left from the highest. **Colour against the part** shows the run's result red where it gouges (deeper red to 0.5 mm), blue where stock is left (to 2 mm), green within the tolerance.
- **Not modelled:** undercuts (a height map: 3 axes), the spindle nose and the head above the holder, the setup's rotation (G10 R), tool deflection. A program drawn with merged moves (past 100 000) is checked on the merged path.

## The 3D setup

What stands on the table in the 3D view: the built-in vise and stock by default, and
models from the program folder. millweb keeps it (`[MILLWEB] SETUP`, default
`millweb-setup.json` beside the INI), so every browser shows the same; changing it
needs control.
- **Click** an object in the view to select it (a drag still orbits): it lights up, and its panel shows its **role** (fixture or workpiece), where it is **placed in** (the work offset: it moves with G5x, G92 and the rotation; or the machine), its position, rotation (X, Y, Z in degrees) and **scale** (25.4 for a model drawn in inches).
- **Top at work zero** puts its top at work Z0 and its front-left corner at X0 Y0; **On the table** sets its bottom on the table; **Turn 90°**, **Tip 90°**; **Remove**.
- Each object is solid, see-through or hidden; the **fixtures** and **workpiece** buttons above the view do the same for all of them at once.
- A new workpiece goes to the work zero, a new fixture onto the table under it.
- **Tools:** on the Tools page each tool can have its own model (the tool and its holder in one STL or OBJ, its origin at the cutting tip, the spindle along +Z), uploaded there or chosen from the program folder's models, shifted, turned and scaled; the 3D view draws it in place of the plain cylinder. Kept with the setup (`tools`).


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
