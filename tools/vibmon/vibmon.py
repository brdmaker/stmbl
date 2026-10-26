#!/usr/bin/env python3
"""vibmon: the PC's side of the measurement bus (docs/vibsensor.md §3).

  - at start, finds the sensors and gives them their numbers (vibaddr.py,
    sensors.json);
  - sends the machine state (rpm, feed, teeth, tool, flags) every 20 ms;
  - reads the sensors' features, summaries, events, spectra and console;
  - keeps each sensor's latest values and a trend (10 minutes at 10 Hz);
  - latches a crash until it is reset; a sensor silent for 0.5 s is lost;
  - optionally HAL pins (--hal): the machine state in, the alarms out;
  - a JSON socket for millweb (--sock): status and events out, commands in.

    vibmon.py --can can1 --config sensors.json --sock /tmp/vibmon.sock --hal
    vibmon.py --sim 2 --sock /tmp/vibmon.sock     simulated sensors (host/stmbl_host)

The HAL component "vibmon":
  in   spindle-rpm feed (float), teeth tool (s32), spindle-on running crash-reset (bit)
  out  crash lost (bit), level temp-level (s32): the worst of all sensors
       <position>.vel .rms .temp .ord1 .chat-f .chat-a (float), .crash .ok (bit)
Link vibmon.crash to a feed hold or into the E-stop chain (docs/vibsensor.md).
"""
import argparse
import collections
import json
import os
import queue
import socket
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import vibaddr  # noqa: E402
import vibproto as vp  # noqa: E402

FS = 26667.0  # the IIS3DWB's rate: a spectrum's bins are FS / fft_n apart


class SensorState:
    def __init__(self, entry):
        self.uid = entry["uid"]
        self.node = entry["node"]
        self.name = entry.get("name", "")
        self.position = entry.get("position")
        self.features = None
        self.summary = None
        self.spectrum = None   # {"seq", "df", "rms": [...]}
        self._spec = {}        # seq -> partial
        self.console = ""      # the console's output, the last 4 kB
        self.last_rx = None
        self.lost = True
        self.crash = False     # latched here until reset
        self.trend = collections.deque(maxlen=6000)

    @property
    def key(self):
        return self.position or ("node%d" % self.node)

    def status(self):
        return {"uid": self.uid, "node": self.node, "name": self.name, "position": self.position, "key": self.key,
                "lost": self.lost, "crash": self.crash, "features": self.features, "summary": self.summary}


class Monitor:
    def __init__(self, bus, config=None, config_path=None, clock=None, hal=None,
                 state_period=0.02, lost_after=0.5):
        self.bus = bus
        self.config_path = config_path
        self.config = config if config is not None else (vibaddr.load_config(config_path) if config_path else {"sensors": []})
        self.clock = clock or getattr(bus, "now", None) or time.monotonic
        self.hal = hal
        self.state_period, self.lost_after = state_period, lost_after
        self.addr = vibaddr.Addresser(bus)
        self.sensors = {}      # node -> SensorState
        self.machine = {"rpm": 0.0, "feed": 0.0, "teeth": 2, "tool": 0, "spindle": False, "running": False}
        self.crash = False     # any sensor's crash, latched
        self.events = []       # since the last take_events()
        self.last_state = -1e9
        self.con_seq = 0
        self.identifying = False
        self.t0 = self.clock()

    # ---------------------------------------------------------------- setup
    def start(self):
        report = self.addr.auto(self.config)
        self._save()
        self._rebuild()
        for uid, what in report:
            self._event({"type": "numbering", "uid": uid, "what": what})
        return report

    def _rebuild(self):
        old = self.sensors
        self.sensors = {}
        for e in self.config["sensors"]:
            s = old.get(e["node"]) if old.get(e["node"]) and old[e["node"]].uid == e["uid"] else SensorState(e)
            s.name, s.position, s.node = e.get("name", ""), e.get("position"), e["node"]
            self.sensors[e["node"]] = s

    def _save(self):
        if self.config_path:
            vibaddr.save_config(self.config_path, self.config)

    def assign(self, uid, name=None, position=None):
        """Name a sensor and say where it sits; written to it and to sensors.json."""
        e = next(x for x in self.config["sensors"] if x["uid"] == uid)
        if position is not None:
            e["position"] = position or None
        if name is not None:
            e["name"] = name
            u = bytes.fromhex(uid)
            self.addr.set(u, e["node"], name)
            self.addr.store(u)
        self._save()
        self._rebuild()

    def replace(self, old_uid, new_uid):
        self.addr.replace(self.config, old_uid, new_uid)
        self._save()
        self._rebuild()

    def rescan(self):
        return self.start()

    # ---------------------------------------------------------------- commands
    def set_machine(self, **kw):
        self.machine.update(kw)

    def console(self, node, line):
        for can_id, data in vp.console_frames(node, self.con_seq, line.rstrip("\n") + "\n"):
            self.bus.send(can_id, data)
            self.con_seq += 1

    def request_spectrum(self, node):
        self.console(node, "vnspec")

    def reset_crash(self, node=None):
        for n, s in self.sensors.items():
            if node in (None, n):
                s.crash = False
                self.console(n, "vib0.reset = 1")
        self.crash = any(s.crash for s in self.sensors.values())

    def identify(self, on):
        self.identifying = bool(on)
        self.bus.send(vibaddr.CMD_ID, vibaddr.fd_pad(bytes([vibaddr.IDENTIFY, 1 if on else 0])))

    # ---------------------------------------------------------------- the loop
    def poll(self, dt):
        """Send the machine state when due, take dt seconds of frames, watch."""
        if self.hal:
            self.hal.read(self)
        now = self.clock()
        if now - self.last_state >= self.state_period:
            m = self.machine
            flags = (vp.ST_SPINDLE if m["spindle"] else 0) | (vp.ST_RUNNING if m["running"] else 0)
            self.bus.send(*vp.state(int((now - self.t0) * 1000), m["rpm"], m["feed"], int(m["teeth"]), flags, int(m["tool"])))
            self.last_state = now
        for can_id, data in self.bus.recv(dt):
            self._frame(can_id, data)
        now = self.clock()
        for s in self.sensors.values():
            lost = s.last_rx is None or now - s.last_rx > self.lost_after
            if lost != s.lost:
                s.lost = lost
                self._event({"type": "lost" if lost else "back", "node": s.node, "key": s.key})
        if self.hal:
            self.hal.write(self)

    def _frame(self, can_id, d):
        if can_id == vibaddr.RSP_ID:
            if d and d[0] == vibaddr.TAP:
                uid = bytes(d[1:13]).hex()
                self._event({"type": "tap", "uid": uid, "node": d[13]})
            return
        k, node = vp.kind(can_id)
        s = self.sensors.get(node)
        if not k or not s:
            return
        s.last_rx = self.clock()
        if k == "features":
            s.features = vp.features(d)
            f = s.features
            s.trend.append((round(s.last_rx - self.t0, 3), max(f["vel"]), max(f["rms"]), f["temp"], self.machine["rpm"]))
        elif k == "summary":
            s.summary = vp.summary(d)
        elif k == "event":
            e = vp.event(d)
            if e["type"] == "crash":
                s.crash = self.crash = True
            self._event(dict(e, node=node, key=s.key, type="event:" + str(e["type"])))
        elif k == "spectrum":
            p = vp.spectrum(d)
            part = s._spec.setdefault(p["seq"], {"total": p["total"], "axis": p["axis"], "bins": {}})
            for i, b in enumerate(p["bins"]):
                if p["first"] + i < p["total"]:
                    part["bins"][p["first"] + i] = b
            if len(part["bins"]) == part["total"]:
                n = (part["total"] - 1) * 2
                s.spectrum = {"seq": p["seq"], "axis": part["axis"], "df": FS / n,
                              "rms": [part["bins"][k] for k in range(part["total"])]}
                s._spec.clear()
                self._event({"type": "spectrum", "node": node, "key": s.key})
        elif k == "console":
            s.console = (s.console + vp.console_text(d))[-4096:]

    def _event(self, e):
        e["t"] = round(self.clock() - self.t0, 3)
        self.events.append(e)
        del self.events[:-1000]

    def take_events(self):
        e, self.events = self.events, []
        return e

    # ---------------------------------------------------------------- reading
    def worst(self):
        f = [s.features for s in self.sensors.values() if s.features and not s.lost]
        return {"crash": self.crash, "lost": any(s.lost for s in self.sensors.values()),
                "level": max([x["level"] for x in f], default=0), "temp_level": max([x["temp_level"] for x in f], default=0)}

    def status(self):
        return {"machine": self.machine, "alarms": self.worst(), "identifying": self.identifying,
                "sensors": [s.status() for _, s in sorted(self.sensors.items())]}


# -------------------------------------------------------------------- HAL
class HalPins:
    """The vibmon HAL component (LinuxCNC's userspace hal module, or a fake
    with the same pins: FakeHal). Per-sensor pins exist for the sensors in
    sensors.json at start."""
    IN = (("spindle-rpm", "float"), ("feed", "float"), ("teeth", "s32"), ("tool", "s32"),
          ("spindle-on", "bit"), ("running", "bit"), ("crash-reset", "bit"))
    OUT = (("crash", "bit"), ("lost", "bit"), ("level", "s32"), ("temp-level", "s32"))
    PER = (("vel", "float"), ("rms", "float"), ("temp", "float"), ("ord1", "float"), ("chat-f", "float"),
           ("chat-a", "float"), ("crash", "bit"), ("ok", "bit"))

    def __init__(self, keys, comp=None):
        if comp is None:
            import hal
            comp = hal.component("vibmon")
            types = {"float": hal.HAL_FLOAT, "s32": hal.HAL_S32, "bit": hal.HAL_BIT}
            for n, t in self.IN:
                comp.newpin(n, types[t], hal.HAL_IN)
            for n, t in self.OUT:
                comp.newpin(n, types[t], hal.HAL_OUT)
            for k in keys:
                for n, t in self.PER:
                    comp.newpin("%s.%s" % (k, n), types[t], hal.HAL_OUT)
            comp.ready()
        self.comp = comp
        self.keys = set(keys)
        self._reset = False

    def read(self, mon):
        c = self.comp
        mon.set_machine(rpm=float(c["spindle-rpm"]), feed=float(c["feed"]), teeth=int(c["teeth"]) or 2,
                        tool=int(c["tool"]), spindle=bool(c["spindle-on"]), running=bool(c["running"]))
        r = bool(c["crash-reset"])
        if r and not self._reset:
            mon.reset_crash()
        self._reset = r

    def write(self, mon):
        c, w = self.comp, mon.worst()
        c["crash"], c["lost"], c["level"], c["temp-level"] = w["crash"], w["lost"], w["level"], w["temp_level"]
        for s in mon.sensors.values():
            if s.key not in self.keys:
                continue
            f, m = s.features or {}, s.summary or {}
            c[s.key + ".vel"] = max(f.get("vel", [0.0]))
            c[s.key + ".rms"] = max(f.get("rms", [0.0]))
            c[s.key + ".temp"] = f.get("temp", 0.0)
            c[s.key + ".ord1"] = (m.get("ord") or [0.0])[0]
            c[s.key + ".chat-f"] = m.get("chat_f", 0.0)
            c[s.key + ".chat-a"] = m.get("chat_a", 0.0)
            c[s.key + ".crash"] = s.crash
            c[s.key + ".ok"] = not s.lost


class FakeHal(dict):
    """A dict with the vibmon component's pins, for tests and --sim."""
    def __init__(self, keys):
        super().__init__()
        for n, t in HalPins.IN + HalPins.OUT:
            self[n] = 0
        for k in keys:
            for n, t in HalPins.PER:
                self["%s.%s" % (k, n)] = 0


# -------------------------------------------------------------------- JSON socket
class Server:
    """Line-delimited JSON on a Unix socket. Out: {"type": "status", ...}
    every 0.2 s and {"type": "event", ...} as they come; spectra and
    console text on request. In: {"cmd": ...} (see Server.do)."""
    def __init__(self, path):
        self.path = path
        try:
            os.unlink(path)
        except FileNotFoundError:
            pass
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.bind(path)
        self.sock.listen(4)
        self.clients = []
        self.lock = threading.Lock()
        self.cmds = queue.Queue()
        threading.Thread(target=self._accept, daemon=True).start()

    def _accept(self):
        while True:
            try:
                c, _ = self.sock.accept()
            except OSError:
                return
            with self.lock:
                self.clients.append(c)
            threading.Thread(target=self._read, args=(c,), daemon=True).start()

    def _read(self, c):
        buf = b""
        while True:
            try:
                d = c.recv(4096)
            except OSError:
                d = b""
            if not d:
                with self.lock:
                    if c in self.clients:
                        self.clients.remove(c)
                return
            buf += d
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                try:
                    self.cmds.put((c, json.loads(line)))
                except ValueError:
                    pass

    def send(self, msg, to=None):
        data = (json.dumps(msg) + "\n").encode()
        with self.lock:
            for c in ([to] if to else list(self.clients)):
                try:
                    c.sendall(data)
                except OSError:
                    if c in self.clients:
                        self.clients.remove(c)

    def do(self, mon):
        """Carry out the commands waiting (in the monitor's thread)."""
        while True:
            try:
                c, m = self.cmds.get_nowait()
            except queue.Empty:
                return
            cmd, node = m.get("cmd"), m.get("node")
            try:
                if cmd == "console":
                    mon.console(node, m["line"])
                elif cmd == "console_read":
                    s = mon.sensors[node]
                    self.send({"type": "console", "node": node, "text": s.console}, c)
                    s.console = ""
                elif cmd == "spectrum":
                    mon.request_spectrum(node)
                elif cmd == "get_spectrum":
                    self.send({"type": "spectrum", "node": node, "spectrum": mon.sensors[node].spectrum}, c)
                elif cmd == "trend":
                    self.send({"type": "trend", "node": node, "trend": list(mon.sensors[node].trend)}, c)
                elif cmd == "reset":
                    mon.reset_crash(node)
                elif cmd == "identify":
                    mon.identify(m.get("on", True))
                elif cmd == "assign":
                    mon.assign(m["uid"], m.get("name"), m.get("position"))
                elif cmd == "replace":
                    mon.replace(m["old"], m["new"])
                elif cmd == "rescan":
                    mon.rescan()
                elif cmd == "machine":  # without HAL: the machine state from a client
                    mon.set_machine(**{k: m[k] for k in mon.machine if k in m})
                else:
                    raise KeyError("unknown command %r" % cmd)
                self.send({"type": "ok", "cmd": cmd}, c)
            except Exception as e:  # a bad command answers, it doesn't stop the monitor
                self.send({"type": "error", "cmd": cmd, "error": str(e)}, c)

    def close(self):
        self.sock.close()
        try:
            os.unlink(self.path)
        except FileNotFoundError:
            pass


def run(mon, server=None, step=0.01, status_every=0.2, until=None):
    """The main loop: poll, commands, status and events to the clients."""
    last = -1e9
    while until is None or mon.clock() < until:
        mon.poll(step)
        if server:
            server.do(mon)
            for e in mon.take_events():
                server.send({"type": "event", "event": e})
            if mon.clock() - last >= status_every:
                server.send(dict(mon.status(), type="status"))
                last = mon.clock()


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--can", help="SocketCAN interface of the measurement bus (CAN FD)")
    ap.add_argument("--sim", type=int, help="this many simulated sensors instead (host/stmbl_host)")
    ap.add_argument("--config", default="sensors.json")
    ap.add_argument("--sock", help="JSON socket for millweb")
    ap.add_argument("--hal", action="store_true", help="the vibmon HAL component")
    a = ap.parse_args()
    if a.sim:
        import simbus
        bus = simbus.SimBus([simbus.SimSensor("%024x" % (0x5EA50000 + i)) for i in range(a.sim)])
    elif a.can:
        bus = vibaddr.SocketCanBus(a.can)
    else:
        ap.error("--can IFACE or --sim N")
    mon = Monitor(bus, config_path=a.config)
    for uid, what in mon.start():
        print("vibmon: %s %s" % (uid, what), flush=True)
    if a.hal:
        mon.hal = HalPins([s.key for s in mon.sensors.values()])
    server = Server(a.sock) if a.sock else None
    try:
        run(mon, server)
    except KeyboardInterrupt:
        pass
    finally:
        if server:
            server.close()


if __name__ == "__main__":
    main()
