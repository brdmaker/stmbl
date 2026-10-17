#!/usr/bin/env python3
"""HAL side of the phasing suite, one action per call, in the running HAL
session (phasing.hal). Prints one JSON line.

  phasing_worker.py PREFIX phase TIMEOUT    enable at the current position;
                                            swing and time until enabled
  phasing_worker.py PREFIX move MM VEL [stay]  out by MM and back (stay: only out)
                                            with velocity feedforward
  phasing_worker.py PREFIX disable
"""
import json
import subprocess
import sys
import time

import hal

P = sys.argv[1]
CLI = __import__("os").path.join(__import__("os").path.dirname(__import__("os").path.abspath(__file__)),
                                 "..", "..", "servoterm-cli", "servoterm-cli")
c = hal.component("phasing_worker")
c.ready()


def pin(n):
    return hal.get_value(P + n)


def setp(n, v):
    hal.set_p(P + n, str(v))


def drive_state():
    out = subprocess.run([CLI, "-t", "0.2", "-c", "fault0.state"], capture_output=True, text=True).stdout
    for l in out.splitlines():
        if "fault0.state" in l:
            return float(l.split("=")[-1])
    return None


act = sys.argv[2]
if act == "phase":
    timeout = float(sys.argv[3])
    fb = pin("pos_fb")
    setp("pos_cmd", fb)
    setp("vel_cmd", 0)
    time.sleep(0.3)
    setp("enable", 1)
    t0 = time.time()
    swing = 0.0
    done = None
    last_poll = 0.0
    while time.time() - t0 < timeout:
        swing = max(swing, abs(pin("pos_fb") - fb))
        if pin("fault"):
            break
        if time.time() - last_poll > 0.5:  # the drive's own state, over USB
            last_poll = time.time()
            st = drive_state()
            if st == 1.0:
                done = time.time() - t0
                break
            if st == 3.0:
                break
        time.sleep(0.001)
    t_end = time.time() + 0.5  # the swing right after it is enabled
    while time.time() < t_end:
        swing = max(swing, abs(pin("pos_fb") - fb))
        time.sleep(0.001)
    print(json.dumps({"swing": round(swing, 4), "time": round(done, 2) if done is not None else None}))
elif act == "move":
    dist, vel = float(sys.argv[3]), float(sys.argv[4])
    stay = len(sys.argv) > 5 and sys.argv[5] == "stay"
    start = pin("pos_cmd")
    legs = [dist] if stay else [dist, -dist]
    err = 0.0
    x = start
    for leg in legs:
        n = max(int(abs(leg) / vel / 0.001), 1)
        v = leg / (n * 0.001)
        for i in range(n):
            x += leg / n
            setp("vel_cmd", v)
            setp("pos_cmd", x)
            err = max(err, abs(pin("pos_fb") - x))
            time.sleep(0.001)
        setp("vel_cmd", 0)
        t_end = time.time() + 0.3
        while time.time() < t_end:
            err = max(err, abs(pin("pos_fb") - x))
            time.sleep(0.001)
    print(json.dumps({"max_err": round(err, 4), "fault": bool(pin("fault"))}))
elif act == "disable":
    setp("enable", 0)
    setp("vel_cmd", 0)
    time.sleep(0.3)
    print(json.dumps({"ok": True}))
