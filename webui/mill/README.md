# webui/mill: a browser console for a headless milling machine

The operator console for a gantry mill on stmbl drives and LinuxCNC, built
as a web app. The machine runs headless: LinuxCNC and a small backend on
the controller, and the operator uses a browser on any tablet or PC on the
shop network.

**Status:** the front end is complete as a concept. It runs on its own:
everything is simulated in the page, including the machine, a program,
offsets, tools, probing and the dynamics. The backend below is the plan.

```sh
xdg-open webui/mill/index.html              # straight from disk
python3 -m http.server -d webui/mill 8080   # or served: http://<host>:8080/
```

It has no build step and needs no internet:
- **three.js** (r128, MIT) is copied under `vendor/three` with its licence.
- **Fonts** come from Google Fonts when the browser has internet; offline the page falls back to the system fonts.

| File | What |
|---|---|
| `index.html` | The whole app: HTML, CSS and the JS, including the simulated machine |
| `vendor/three/` | three.js r128 and its OrbitControls (MIT) |

## The pages

| Page | What |
|---|---|
| MAIN | The machine in 3D: frame, rails, bridge, carriage, ram, spindle, tool, the stock in its fixture, the travel limits. The toolpath is coloured by predicted path error, feed or tool, and the deflection is drawn exaggerated. Below: the G-code (run from a line), MDI with history, jog |
| FILE | Programs, each with a preflight: travel, tools, time, predicted path error |
| WIZARDS | Facing, bolt circle and circular pocket generate G-code |
| OFFSETS | G54–G59.3 and G92, a touch plate |
| TOOLS | The tool table: load, measure on the setter, add, delete |
| PROBE | Eight probing cycles; set the offset from the result |
| DYNAMICS | Command conditioning (pcomp), the head's modes, the gantry link, spindle vibration, drives, power, the dynwiz model |
| STATUS | Events, machine counters, a HAL pin watch |
| SETTINGS | Units, display options, M1 / block delete / lift on hold, the keys |

On every page the right column holds:
- the DRO, with zero, touch-off and home per axis;
- the modal G-codes;
- spindle, coolant and the overrides;
- cycle start, hold, single block and stop.

## The backend (plan)

One Python process on the controller between LinuxCNC and the browsers.
It is a LinuxCNC user interface like AXIS or QtVCP, with a web socket
where they have a window.

```
browser (any number)  <── HTTPS + WebSocket ──>  millweb (Python, asyncio)  ──>  LinuxCNC (task, motion, HAL)
                                                   │  linuxcnc.stat / command / error_channel
                                                   │  hal: a userspace component "millweb" (postgui HAL)
                                                   │  halsampler (servo-rate data) · gcode module (preview)
                                                   └  files: nc_files, tool.tbl, the INI, dynwiz.json
```

### Process and start-up
- **How it starts:** `millweb` is the INI's display program (`[DISPLAY] DISPLAY = millweb`), the way `hwtest/portal-sim/portal_run.py` drives LinuxCNC headless. LinuxCNC starts it and stops with it, and a systemd unit starts LinuxCNC at boot.
- **Where it runs:** it is ordinary user-space Python, not realtime, and nothing it does can delay the servo thread.
- **What it needs:** the `linuxcnc` and `hal` modules and `aiohttp` (`python3-aiohttp` on Debian). Configuration lives in the INI's `[MILLWEB]`: port, certificate, users, the nc_files directory, the HAL pins it shows.

### Status out
- **Machine state:** `linuxcnc.stat()` polled at 30–50 Hz: position, homed, state, mode, interpreter state, current line, overrides, spindle, coolant, tool, offsets, G-codes.
- **HAL values:** a userspace HAL component, `millweb`, with input pins netted in a postgui HAL file, like `linuxcnc/arm6hmi`'s. That covers the drives' current, power and fault, `pcomp.0.*`, the gantry link's `yaw`/`active`, `powerquota.0.*`, the spindle load and the DC bus.
- **What goes to the browser:** only what changed since the last message, as JSON. Three rates:
  - fast, about 30 Hz: DRO, following error, feed, spindle;
  - slow, about 2 Hz: drives, power, pins;
  - on change: files, tool table, offsets, events.
- **Servo-rate data** (contour error, bridge yaw, the spindle current for its spectrum) comes from a HAL `sampler` read with `halsampler`, the same way `linuxcnc/dynwiz` records. The backend reduces it: decimated series, and an FFT of the spindle current at 2 Hz. A browser never gets 1 kHz raw data unless a scope view asks for it.
- **Events:** `error_channel` and the backend's own events, kept in a ring buffer (a log file too) and replayed to a browser that connects later.

### Commands in

The JSON messages are `{"op": ..., ...}`, each answered with ok or a reason:

| op | LinuxCNC |
|---|---|
| `estop`, `power` | `C.state(STATE_ESTOP / ESTOP_RESET / ON / OFF)` |
| `home`, `unhome` | `C.home(j)` in manual mode |
| `jog` | `C.jog(JOG_INCREMENT / JOG_CONTINUOUS / JOG_STOP, ...)`; continuous jog is a deadman, below |
| `mdi` | `C.mode(MODE_MDI)`, `C.mdi(text)` |
| `cycle` | `C.auto(AUTO_RUN, line)`, `AUTO_PAUSE / RESUME / STEP`, `C.abort()` |
| `override` | `C.feedrate`, `rapidrate`, `spindleoverride`, `maxvel` |
| `spindle`, `coolant` | `C.spindle(...)`, `C.flood / mist`; air as a HAL bit |
| `offset` | G10 L2 / L20 through MDI; G92 / G92.1 |
| `tool` | edit `tool.tbl`, then `C.load_tool_table()`; M6 / M61 through MDI; measure is an O-word subroutine |
| `probe` | O-word subroutines with G38.x, as QtDragon's probe screens use; the results come back through `#5061`… parameters |
| `program` | list, upload, delete, load (`C.program_open`), preflight |
| `wizard` | facing, bolt circle, pocket: the generators of `index.html` move to the server, write a file and load it |
| `pcomp` | `hal.set_p("pcomp.0.shaper", ...)` at standstill |
| `dynwiz` | runs `linuxcnc/dynwiz` as a subprocess, streams its output and loads `dynwiz.json` |

### Preview geometry
- **When a program loads,** the backend parses it with LinuxCNC's own interpreter (the `gcode` module with a canon object, as AXIS and QtVCP do).
- **It sends** the line segments with their line numbers, feeds, tools and kinds, as one binary message (Float32Array).
- **It computes** the path-error colour the same way pcomp does, from the machine model in `[PCOMP]`, so the browser draws what the controller predicts.

### Safety

The web page is a convenience, not a safety function:
- **The E-STOP chain is hardware.** It runs through stmbl's fault inputs and the drives' quick stop, as it does today. The page's E-STOP button only requests `STATE_ESTOP`.
- **One browser has control at a time.** The others view only. Control is a lease you take, and a local "remote enable" key switch on a HAL input can forbid it.
- **Continuous jog needs a heartbeat:** a message at least every 100 ms, or the backend sends `JOG_STOP`. A lost connection stops a jog at once.
- **A program keeps running** if the controlling browser goes away (configurable: feed hold instead). Nobody else can take control without the lease expiring, or someone confirming at the machine.
- **Access:** HTTPS with a login (operator, viewer, integrator roles) and rate limits. Keep it on the shop network or behind a VPN; never put the machine directly on the internet.
- **Checks before LinuxCNC:** commands are validated first (limits, mode, machine on). LinuxCNC still checks everything again.

### The front end's side
- **`index.html` splits along one seam:** its simulated machine becomes `sim.js`, and a `ws.js` speaks the protocol above.
- **Both implement the same interface:** a stream of state updates in, commands out. The UI code doesn't know which one it has.
- **Opened from disk, or with `?sim`,** it keeps working as now, as a demo or for training. Served by `millweb`, it drives the machine.

### Testing without hardware
- **The integration test** is `hwtest/portal-sim` (on the portal-dynamics branch): real LinuxCNC on the simulated portal, with the simulated stmbl drives in the servo thread. `millweb` runs as its display program.
- **The browser side:** headless Chromium (pre-installed for Playwright) drives the page: home, MDI, run, hold, probe. The test reads the DRO and the event log.
- **The protocol** gets unit tests against a fake `linuxcnc` module.
