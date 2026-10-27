#!/usr/bin/env python3
"""LinuxCNC + the simulated ARC Mate + powerquota (make test-quota): a
headless DISPLAY program. The six drives' input power (smart-serial
power) feeds three quotas, two drives per phase (quota_test.txt: L1 for
J1 + J2 sized below what big sweeps draw, L2 / L3 roomy).

1. M52 P1: sweeps between two poses at full feed. L1 warms up, the moves
   are slowed (feed scale < 1), L1's damage stays under the margin, L2 /
   L3 untouched, no alarm; the arm keeps its path (no collision hit).
2. M52 P0: the same sweeps at full speed heat L1 until the alarm stops
   the machine (estop) before the fuse would blow.
PROBE=1: only print the power per phase (to size the quotas).
"""
import os
import sys
import time

import hal
import linuxcnc

S = linuxcnc.stat()
C = linuxcnc.command()
results = []
INFO = []
P1 = "X800 Y-150 Z1250 A0 B0 C0"
P2 = "X1050 Y300 Z1050 A0 B0 C0"


def check(cond, name, detail=""):
    results.append((bool(cond), name, detail))


def poll():
    S.poll()
    return S


def wait(pred, timeout, step=0.01):
    t = time.time() + timeout
    while time.time() < t:
        if pred(poll()):
            return True
        time.sleep(step)
    return False


def g(pin):
    return hal.get_value(pin)


def mdi(cmd, timeout=60.0):
    C.mode(linuxcnc.MODE_MDI)
    C.wait_complete()
    C.mdi(cmd)
    time.sleep(0.05)
    return wait(lambda s: s.task_state != linuxcnc.STATE_ON or
                (s.interp_state == linuxcnc.INTERP_IDLE and s.queue == 0), timeout)


def sweeps(n, timeout):
    st = {"p": [0.0] * 3, "pmax": [0.0] * 3, "k": 0, "smin": 1.0, "d": [0.0] * 3, "estop": False, "coll": 0}
    t0 = time.time()
    C.mode(linuxcnc.MODE_MDI)
    C.wait_complete()
    for i in range(2 * n):
        C.mdi("G1 %s F15000" % (P2 if i % 2 == 0 else P1))
        time.sleep(0.05)
        while True:
            s = poll()
            for k, q in enumerate(("L1", "L2", "L3")):
                p = g("powerquota.0.%s.power" % q)
                st["p"][k] += p
                st["pmax"][k] = max(st["pmax"][k], p)
                st["d"][k] = max(st["d"][k], g("powerquota.0.%s.damage" % q))
            st["k"] += 1
            st["smin"] = min(st["smin"], g("powerquota.0.feed-scale"))
            st["coll"] += g("armcollide.0.collision")
            if s.task_state != linuxcnc.STATE_ON:
                st["estop"] = True
                st["t"] = time.time() - t0
                return st
            if (s.interp_state == linuxcnc.INTERP_IDLE and s.queue == 0) or time.time() - t0 > timeout:
                break
            time.sleep(0.005)
    st["t"] = time.time() - t0
    return st


def fmt(st):
    k = max(st["k"], 1)
    return "power max/mean L1 %.0f/%.0f L2 %.0f/%.0f L3 %.0f/%.0f W; damage %.2f %.2f %.2f; feed min %.2f; %.1f s" % (
        st["pmax"][0], st["p"][0] / k, st["pmax"][1], st["p"][1] / k, st["pmax"][2], st["p"][2] / k,
        st["d"][0], st["d"][1], st["d"][2], st["smin"], st["t"])


def main():
    h = hal.component("lcnc_quota_test")
    h.ready()
    if not wait(lambda s: s.task_state != 0, 20):
        check(False, "LinuxCNC task up")
        return
    C.state(linuxcnc.STATE_ESTOP_RESET)
    C.state(linuxcnc.STATE_ON)
    check(wait(lambda s: s.task_state == linuxcnc.STATE_ON, 5), "machine on")
    time.sleep(0.5)
    C.mode(linuxcnc.MODE_MANUAL)
    C.wait_complete()
    C.home(-1)
    wait(lambda s: all(s.homed[j] for j in range(6)), 10)
    C.teleop_enable(1)
    C.wait_complete()
    idle = [g("stmbl-arm.0.%d.power" % j) for j in range(6)]
    check(all(10 < p < 40 for p in idle), "the six drives report their power (%s W at rest)"
          % " ".join("%.0f" % p for p in idle))
    mdi("G90 G1 %s F6000" % P1)
    if os.environ.get("PROBE"):
        mdi("M52 P0")
        INFO.append("# full speed: " + fmt(sweeps(4, 120)))
        return

    mdi("M52 P1")
    st = sweeps(10, 300)
    INFO.append("# protected: " + fmt(st))
    check(not st["estop"] and st["smin"] < 0.8, "M52 P1: the sweeps are slowed (feed scale down to %.2f)"
          % st["smin"])
    check(st["d"][0] > 0.2 and st["d"][0] < 0.55 and not g("powerquota.0.alarm"),
          "  ...L1 (J1 + J2) works near its limit, under the margin (damage max %.2f), no alarm" % st["d"][0])
    check(st["d"][1] < 0.01 and st["d"][2] < 0.01, "  ...L2 and L3 untouched")
    check(st["coll"] == 0, "  ...no collision hit (the path kept)")

    mdi("M52 P0")
    st2 = sweeps(40, 300)
    INFO.append("# unprotected: " + fmt(st2))
    check(st2["estop"] and g("powerquota.0.alarm") and g("powerquota.0.alarm-quota") == 0 and
          0.89 < st2["d"][0] < 0.95,
          "M52 P0, full speed: L1 heats up; the alarm stops the machine at damage %.2f, before it blows"
          % st2["d"][0])


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        check(False, "exception: %r" % e)
    n_fail = sum(1 for r in results if not r[0])
    out = []
    for i, (ok, name, detail) in enumerate(results, 1):
        out.append("%s %d - %s" % ("ok" if ok else "not ok", i, name))
        if not ok and detail:
            out.append("#   %s" % (detail,))
    out.append("1..%d" % len(results))
    out.append("# %d/%d passed" % (len(results) - n_fail, len(results)))
    text = "\n".join(INFO + out) + "\n"
    sys.stdout.write(text)
    open("lcnc_quota_test.tap", "w").write(text)
    open("lcnc_quota_test.result", "w").write("%d\n" % n_fail)
