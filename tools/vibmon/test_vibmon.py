#!/usr/bin/env python3
"""vibmon against simulated sensors that run the sensor's own code
(simbus.py: host/stmbl_host with vib, vnode and the board's template):
numbering, tap-to-name, the machine state into the sensors' order
tracking, features and summaries, HAL pins, the spectrum, a parameter over
the console, a crash and its reset, a lost sensor, the JSON socket.

  python3 tools/vibmon/test_vibmon.py      (host/stmbl_host built)
"""
import json
import os
import socket
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import simbus  # noqa: E402
import vibmon  # noqa: E402

fails = checks = 0


def check(ok, what):
    global fails, checks
    checks += 1
    print("%s %d - %s" % ("ok" if ok else "not ok", checks, what))
    if not ok:
        fails += 1


def near(a, b, tol):
    return a is not None and abs(a - b) <= tol


def main():
    if not os.path.exists(simbus.HOST):
        print("1..0 # SKIP host/stmbl_host not built")
        return 0
    d = tempfile.mkdtemp()
    A, B = "5ea500000000000000000a01", "5ea500000000000000000b02"
    sa, sb = simbus.SimSensor(A), simbus.SimSensor(B)
    bus = simbus.SimBus([sa, sb])
    try:
        mon = vibmon.Monitor(bus, config_path=os.path.join(d, "sensors.json"))
        rep = mon.start()
        check(sorted(s.node for s in mon.sensors.values()) == [1, 2], "start: two sensors numbered 1, 2 (%s)" % rep)
        na = next(s.node for s in mon.sensors.values() if s.uid == A)
        nb = next(s.node for s in mon.sensors.values() if s.uid == B)

        # tap each to say where it is
        for sim, uid, name, pos in ((sa, A, "spindle", "spindle"), (sb, B, "X axis", "x")):
            mon.identify(True)
            mon.poll(0.4)  # past the tap detector's 0.3 s
            mon.take_events()
            sim.cmd("vibsim0.crash_g = 3")
            sim.cmd("vibsim0.crash = 1")
            mon.poll(0.05)
            taps = [e for e in mon.take_events() if e["type"] == "tap"]
            mon.identify(False)
            check(len(taps) == 1 and taps[0]["uid"] == uid, "tap names the %s sensor" % pos)
            mon.assign(uid, name, pos)
        cfg = json.load(open(os.path.join(d, "sensors.json")))
        check(sorted(e["position"] for e in cfg["sensors"]) == ["spindle", "x"], "sensors.json has the positions")
        mon.reset_crash()  # the taps were knocks: clear what they latched
        mon.poll(0.3)
        mon.take_events()

        keys = ["spindle", "x"]
        hal = vibmon.FakeHal(keys)
        mon.hal = vibmon.HalPins(keys, comp=hal)
        hal["spindle-rpm"], hal["teeth"], hal["spindle-on"] = 6000.0, 2, 1
        sa.cmd("vibsim0.rpm = 6000")
        sa.cmd("vibsim0.unb = 1")
        sb.cmd("vibsim0.feed = 7500")
        sb.cmd("vibsim0.screw_a = 0.2")
        vibmon.run(mon, until=mon.clock() + 1.5)
        s1, s2 = mon.sensors[na], mon.sensors[nb]
        check(s1.summary and near(s1.summary["rpm"], 6000, 1), "the sensor got the rpm from the machine state")
        check(near(s1.summary["ord"][0], 0.7071, 0.015), "spindle: 1st order 0.707 g (%.4f)" % s1.summary["ord"][0])
        check(near(hal["spindle.ord1"], 0.7071, 0.015), "HAL spindle.ord1")
        check(near(hal["spindle.vel"], 11.04, 0.25), "HAL spindle.vel 11.04 mm/s (%.2f)" % hal["spindle.vel"])
        check(hal["level"] == 1 and hal["x.ok"] and hal["spindle.ok"], "HAL level 1 (warn), both ok")
        ev = [e for e in mon.events if e["type"] == "event:level"]
        check(any(e["key"] == "spindle" and e["value"] == 1 for e in ev), "a level event from the spindle")
        check(len(s1.trend) >= 10, "a trend (%d points)" % len(s1.trend))

        mon.request_spectrum(na)
        mon.poll(0.05)
        sp = s1.spectrum
        if sp:
            k = max(range(len(sp["rms"])), key=lambda i: sp["rms"][i])
        check(sp and len(sp["rms"]) == 1025 and near(k * sp["df"], 100, sp["df"]),
              "spectrum: 1025 bins, the peak at 100 Hz")

        mon.console(na, "vib0.fft_n = 4096")
        mon.poll(0.5)
        check(s1.summary["fft_n"] == 4096, "a parameter over the console: fft_n 4096")

        sa.cmd("vibsim0.crash_g = 20")
        sa.cmd("vibsim0.crash = 1")
        mon.poll(0.05)
        check(hal["crash"] and hal["spindle.crash"] and not hal["x.crash"], "crash: HAL vibmon.crash, spindle.crash")
        hal["crash-reset"] = 1
        mon.poll(0.05)
        hal["crash-reset"] = 0
        mon.poll(0.2)
        check(not hal["crash"] and sa.query("vib0.crash") == 0, "crash-reset clears it, on the sensor too")

        sb.alive = False  # unplugged
        mon.poll(0.7)
        check(hal["lost"] and not hal["x.ok"] and hal["spindle.ok"], "a silent sensor is lost after 0.5 s")
        sb.alive = True
        mon.poll(0.3)
        check(not hal["lost"] and hal["x.ok"], "and back")

        # the JSON socket
        path = os.path.join(d, "vibmon.sock")
        srv = vibmon.Server(path)
        c = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        c.connect(path)
        c.settimeout(5)
        vibmon.run(mon, srv, until=mon.clock() + 0.05)  # the server has the client
        c.sendall(b'{"cmd": "trend", "node": %d}\n{"cmd": "nonsense"}\n' % na)
        vibmon.run(mon, srv, until=mon.clock() + 0.3)
        f = c.makefile()
        msgs = []
        while not any(m.get("type") == "error" for m in msgs):
            msgs.append(json.loads(f.readline()))
        st = [m for m in msgs if m["type"] == "status"]
        check(st and len(st[-1]["sensors"]) == 2 and st[-1]["sensors"][0]["features"], "socket: status with both sensors")
        tr = [m for m in msgs if m["type"] == "trend"]
        check(tr and len(tr[0]["trend"]) > 10, "socket: trend")
        check(any(m["type"] == "error" and m["cmd"] == "nonsense" for m in msgs), "socket: a bad command answers an error")
        c.close()
        srv.close()
    finally:
        bus.close()
    print("1..%d" % checks)
    print("# %d/%d passed" % (checks - fails, checks))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
