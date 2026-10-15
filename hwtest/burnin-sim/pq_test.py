#!/usr/bin/env python3
"""LinuxCNC + the simulated drive + powerquota: a headless DISPLAY program
(make test-quota). The drive's input power (sserial power, pe0.p_in)
feeds one quota sized below what fast back-and-forth moves draw.

1. With adaptive feed on (M52 P1): the moves are slowed, the fuse's
   damage stays under the margin, no alarm; the cycles take longer.
2. Without (M52 P0): the same moves at full speed heat the fuse until the
   alarm stops the machine (estop) before the fuse would blow; an estop
   reset clears it once the fuse has cooled below the trip level.
PROBE=1: only print the power the moves draw (to size a quota).
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


def cycles(n, timeout):
    """n back-and-forth rapids over 400 mm; samples while they run"""
    st = {"pmax": 0.0, "psum": 0.0, "k": 0, "smin": 1.0, "dmax": 0.0, "estop": False, "t": 0.0}
    C.mode(linuxcnc.MODE_MDI)
    C.wait_complete()
    t0 = time.time()
    for _ in range(n):
        for x in (400, 0):
            C.mdi("G0 X%d" % x)
            ts = time.time()  # until it moves (the interpreter is idle before motion starts)
            while abs(poll().actual_position[0] - x) > 0.01 and abs(S.current_vel) < 1e-6 and time.time() - ts < 1:
                time.sleep(0.002)
            while True:
                s = poll()
                p = g("stmbl-sim.0.power")
                st["pmax"] = max(st["pmax"], p)
                st["psum"] += p
                st["k"] += 1
                st["smin"] = min(st["smin"], g("powerquota.0.feed-scale"))
                st["dmax"] = max(st["dmax"], g("powerquota.0.L1.damage"))
                if s.task_state != linuxcnc.STATE_ON:
                    st["estop"] = True
                    st["t"] = time.time() - t0
                    return st
                if (s.interp_state == linuxcnc.INTERP_IDLE and s.queue == 0 and s.inpos and
                        abs(s.actual_position[0] - x) < 0.05) or time.time() - t0 > timeout:
                    break
                time.sleep(0.005)
    st["t"] = time.time() - t0
    return st


def main():
    h = hal.component("pq_test")
    h.ready()
    if not wait(lambda s: s.task_state != 0, 20):
        check(False, "LinuxCNC task up")
        return
    C.state(linuxcnc.STATE_ESTOP_RESET)
    C.state(linuxcnc.STATE_ON)
    check(wait(lambda s: s.task_state == linuxcnc.STATE_ON, 5), "machine on")
    time.sleep(3.0)  # drive enabled, phased
    C.mode(linuxcnc.MODE_MANUAL)
    C.wait_complete()
    C.home(-1)
    wait(lambda s: all(s.homed[j] for j in range(3)), 30)
    idle = g("stmbl-sim.0.power")
    INFO.append("# drive power at rest %.1f W" % idle)
    if os.environ.get("PROBE"):
        mdi("M52 P0")
        st = cycles(5, 60)
        INFO.append("# full speed: power max %.0f W, mean %.0f W, 5 cycles %.2f s, estop %s, drive fault %d, x %.1f" % (
            st["pmax"], st["psum"] / max(st["k"], 1), st["t"], st["estop"], g("stmbl-sim.0.fault"),
            poll().actual_position[0]))
        e = linuxcnc.error_channel()
        for _ in range(5):
            m = e.poll()
            if m:
                INFO.append("# error: %s" % (m[1],))
        return
    check(10 < idle < 40, "the drive reports its input power over smart-serial (%.1f W at rest)" % idle)

    hal.set_p("powerquota.0.meter-reset", "1")
    time.sleep(0.05)
    hal.set_p("powerquota.0.meter-reset", "0")
    mdi("M52 P1")
    st = cycles(12, 120)
    avg, peak, wh = g("powerquota.0.total-power-avg"), g("powerquota.0.total-power-peak"), g("powerquota.0.energy")
    INFO.append("# meters: avg %.1f W, peak %.1f W, energy %.4f Wh" % (avg, peak, wh))
    check(idle - 1 < avg < st["pmax"] and peak >= st["pmax"] - 1 and wh > idle * st["t"] / 3600.0 * 0.9,
          "meters: total-power-avg between rest and peak, total-power-peak, energy (%.3f Wh) counting" % wh)
    INFO.append("# protected: power max %.0f W, mean %.0f W, feed min %.2f, damage max %.2f, %.1f s" % (
        st["pmax"], st["psum"] / max(st["k"], 1), st["smin"], st["dmax"], st["t"]))
    check(not st["estop"] and st["smin"] < 0.8, "M52 P1: the moves are slowed (feed scale down to %.2f)" % st["smin"])
    check(st["dmax"] < 0.55 and not g("powerquota.0.alarm"),
          "  ...the fuse stays under the margin (damage max %.2f), no alarm" % st["dmax"])

    mdi("M52 P0")
    st2 = cycles(40, 120)
    INFO.append("# unprotected: power max %.0f W, mean %.0f W, damage max %.2f, estop %s after %.1f s" % (
        st2["pmax"], st2["psum"] / max(st2["k"], 1), st2["dmax"], st2["estop"], st2["t"]))
    check(st2["estop"] and g("powerquota.0.alarm") and 0.89 < st2["dmax"] < 0.95,
          "M52 P0, full speed: the fuse heats up; the alarm stops the machine at damage %.2f, before it blows"
          % st2["dmax"])
    hot = g("powerquota.0.alarm")
    C.state(linuxcnc.STATE_ESTOP_RESET)
    time.sleep(0.3)
    check(hot and g("powerquota.0.alarm"), "  ...an estop reset does not clear it while the fuse is that hot")
    time.sleep(5.0)  # cooling (tau 300 s): just under the trip level
    C.state(linuxcnc.STATE_ESTOP)
    C.state(linuxcnc.STATE_ESTOP_RESET)
    time.sleep(0.3)
    check(not g("powerquota.0.alarm") and g("powerquota.0.L1.damage") < 0.9,
          "  ...a little cooler, it does (damage %.3f)" % g("powerquota.0.L1.damage"))
    hal.set_p("powerquota.0.meter-reset", "1")
    time.sleep(0.05)
    check(g("powerquota.0.energy") < 0.001 and g("powerquota.0.total-power-peak") < 40,
          "meter-reset clears energy and peak")
    hal.set_p("powerquota.0.meter-reset", "0")


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
    open("pq_test.tap", "w").write(text)
    open("pq_test.result", "w").write("%d\n" % n_fail)
