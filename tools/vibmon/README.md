# vibmon: the vibration sensors' PC side

The machine's PC on the measurement bus (docs/vibsensor.md §3).

| file | what |
|---|---|
| `vibmon.py` | the monitor: numbering at start, machine state out, frames in, trends, crash latch, watchdog, HAL pins, JSON socket |
| `vibaddr.py` | node numbers: discovery by unique ID, automatic assignment, tap-to-name, replace (sensors.json) |
| `vibproto.py` | the frames (as `shared/vibproto.h`) |
| `simbus.py` | a bus of simulated sensors: `host/stmbl_host` processes running the sensor's own components |
| `test_vibaddr.py`, `test_vibmon.py` | tests; both run in `make -C host check` |

## Running

```sh
# on the machine: CAN FD on can1 (1 Mbit/s, 5 Mbit/s data)
ip link set can1 type can bitrate 1000000 dbitrate 5000000 fd on && ip link set can1 up
tools/vibmon/vibmon.py --can can1 --config /etc/vibmon/sensors.json --sock /run/vibmon.sock --hal

# without sensors: two simulated ones
make -C host stmbl_host
tools/vibmon/vibmon.py --sim 2 --config /tmp/sensors.json --sock /tmp/vibmon.sock
```

## HAL

With `--hal` it is the userspace component `vibmon`:

| pins | |
|---|---|
| `vibmon.spindle-rpm`, `feed`, `teeth`, `tool`, `spindle-on`, `running` (in) | the machine state the sensors get |
| `vibmon.crash` (out) | any sensor's crash, latched until `vibmon.crash-reset` (in) rises |
| `vibmon.lost` (out) | a sensor silent for 0.5 s |
| `vibmon.level`, `temp-level` (out) | the worst sensor's: 0 ok, 1 warn, 2 alarm |
| `vibmon.<position>.vel`, `.rms`, `.temp`, `.ord1`, `.chat-f`, `.chat-a`, `.crash`, `.ok` (out) | per sensor; position from sensors.json (`node<N>` until it has one) |

**Wiring:**

```
net spindle-rpm  spindle.0.speed-out-abs  => vibmon.spindle-rpm
net vib-crash    vibmon.crash              => halui.feed-hold      # or into the E-stop chain
```

The stop is in software (sensor → bus → vibmon → HAL), a few ms ✱. Per-sensor
pins exist for the sensors in sensors.json at start; restart vibmon after
naming new ones.

## The JSON socket

`--sock` takes line-delimited JSON.

**Out:**
- `{"type": "status", "machine", "alarms", "identifying", "sensors": [...]}`
  every 0.2 s;
- `{"type": "event", "event": {...}}` as events come. An event's `type` is
  one of:
  - `event:crash`, `event:level`, `event:temp`;
  - `lost`, `back`;
  - `tap`;
  - `spectrum` (one has arrived);
  - `numbering`.

**In:** a command, answered with `{"type": "ok"}` or `{"type": "error"}`.

| command | does |
|---|---|
| `{"cmd": "console", "node": 1, "line": "vib0.fft_n = 4096"}` | a console line to a sensor |
| `{"cmd": "console_read", "node": 1}` | its output since the last read |
| `{"cmd": "spectrum", "node": 1}` | ask for the spectrum |
| `{"cmd": "get_spectrum", "node": 1}` | the last one received |
| `{"cmd": "trend", "node": 1}` | `[t, vel, rms, temp, rpm]` points, 10 Hz, 10 minutes |
| `{"cmd": "reset", "node": null}` | clear the crash latch (one sensor or all) |
| `{"cmd": "identify", "on": true}` | tap-to-name on: taps come as `tap` events |
| `{"cmd": "assign", "uid": "…", "name": "spindle", "position": "spindle"}` | name a sensor and set its position |
| `{"cmd": "replace", "old": "…", "new": "…"}` | a new sensor takes a missing one's place |
| `{"cmd": "rescan"}` | the numbering again |
| `{"cmd": "machine", "rpm": 6000, "teeth": 2}` | the machine state without HAL |
