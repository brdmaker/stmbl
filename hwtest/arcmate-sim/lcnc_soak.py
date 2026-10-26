#!/usr/bin/env python3
"""LinuxCNC soak of the simulated ARC Mate: a headless display program.

    make soak [SOAK_CYCLES=N]    (copies this config to /tmp, DISPLAY = this)

1. soak.ngc N times (Cartesian lines, reorientation, circles in XY and XZ,
   a welding weave, rapids, all through genserkins): the program must
   finish every cycle with no collision, fault, stop hit or machine off;
   LinuxCNC's joint following error and the true link angles against the
   command are bounded; the end pose repeats.
Then the error scenarios, each from machine on at the end of a cycle:
2. payload 5 kg declared to armcollide: a cycle runs clean
3. the same payload undeclared: armcollide flags it, estop
4. 150 N push on the flange mid-program: estop
5. drive 3 loses its smart-serial link mid-program: amp fault, machine off
6. operator estop in the middle of the weave: the brakes hold the arm
7. a jog of J2 far beyond its soft limit: LinuxCNC stops at the limit, the
   arm never reaches its hard stop
Prints TAP (and writes lcnc_soak.tap / lcnc_soak.result).
"""
import math
import os
import sys
import time

import hal
import linuxcnc

S = linuxcnc.stat()
C = linuxcnc.command()
E = linuxcnc.error_channel()
CYCLES = int(os.environ.get("SOAK_CYCLES", "1"))
NGC = os.path.abspath("soak.ngc")
results, info = [], []


def check(cond, name, detail=""):
    results.append((bool(cond), name, detail))


def poll():
    S.poll()
    return S


def wait(pred, timeout, step=0.01):
    end = time.time() + timeout
    while time.time() < end:
        if pred(poll()):
            return True
        time.sleep(step)
    return False


def hv(name):
    return hal.get_value(name)


def setp(name, v):
    hal.set_p(name, str(v))


def errors():
    out = []
    while True:
        e = E.poll()
        if not e:
            return out
        out.append(e[1])


def machine_on():
    C.state(linuxcnc.STATE_ESTOP_RESET)
    C.state(linuxcnc.STATE_ON)
    ok = wait(lambda s: s.task_state == linuxcnc.STATE_ON, 5)
    time.sleep(0.6)  # drives enabled, detector armed
    C.mode(linuxcnc.MODE_MANUAL)
    C.wait_complete()
    if not all(poll().homed[j] for j in range(6)):
        C.home(-1)
        wait(lambda s: all(s.homed[j] for j in range(6)), 10)
    C.teleop_enable(1)
    C.wait_complete()
    return ok


class Monitor:
    def __init__(self):
        self.ferr = [0.0] * 6
        self.link = [0.0] * 6
        self.coll = 0
        self.fault = 0
        self.coll_joint = 0
        self.t_coll = None

    def sample(self, t0=None):
        s = poll()
        for j in range(6):
            self.ferr[j] = max(self.ferr[j], abs(s.joint[j]["ferror_current"]))
            self.link[j] = max(self.link[j], abs(hv("joint.%d.motor-pos-cmd" % j) - hv("stmbl-arm.0.%d.q" % j)))
            self.fault += int(hv("stmbl-arm.0.%d.fault" % j))
        if hv("armcollide.0.collision") and not self.coll:
            self.coll = 1
            self.coll_joint = hv("armcollide.0.joint")
            self.t_coll = time.time() - t0 if t0 else None
        return s


def run_program(timeout, during=None):
    """run soak.ngc once; during(t) is called while it runs; returns
    (finished normally, monitor, final task state)"""
    s = poll()
    info.append("# run from joints %s, pose %s" % (["%.1f" % v for v in s.joint_actual_position[:6]],
                                                 ["%.1f" % v for v in s.actual_position[:6]]))
    C.mode(linuxcnc.MODE_AUTO)
    C.wait_complete()
    C.program_open(NGC)
    C.auto(linuxcnc.AUTO_RUN, 0)
    m = Monitor()
    t0 = time.time()
    started = False
    while time.time() - t0 < timeout:
        s = m.sample()
        if s.interp_state != linuxcnc.INTERP_IDLE:
            started = True
        if during:
            during(time.time() - t0, m)
        if s.task_state != linuxcnc.STATE_ON:
            return False, m, s.task_state
        if started and s.interp_state == linuxcnc.INTERP_IDLE and s.queue == 0:
            e = errors()
            if e:
                info.append("#   errors: %s" % e[:3])
            return not e, m, s.task_state
        time.sleep(0.005)
    return False, m, poll().task_state


def hits():
    return sum(int(hv("stmbl-arm.0.%d.stop-hits" % j)) for j in range(6))


def main():
    h = hal.component("lcnc_soak")
    h.ready()
    wait(lambda s: s.task_state != 0, 20)
    check(machine_on(), "machine on, homed")
    errors()

    # 1. the program, N cycles
    ends = []
    for k in range(CYCLES):
        ok, m, st = run_program(240)
        s = poll()
        ends.append(list(s.actual_position[:3]))
        check(ok, "cycle %d: program finished (state %d)" % (k + 1, st))
        check(m.coll == 0 and m.fault == 0, "cycle %d: no collision, no drive fault" % (k + 1),
              "coll %d joint %d fault %d" % (m.coll, m.coll_joint, m.fault))
        check(max(m.ferr) < 0.5, "cycle %d: LinuxCNC joint ferror < 0.5 deg (%.3f)" % (k + 1, max(m.ferr)))
        check(max(m.link) < 1.0, "cycle %d: link angles within 1 deg of command (%.3f)" % (k + 1, max(m.link)))
        if not ok:
            info.append("# errors: %s" % errors())
            machine_on()
    check(hits() == 0, "program: no hard stop hit")
    if len(ends) > 1:
        d = max(math.dist(ends[0], e) for e in ends[1:])
        check(d < 0.05, "program: the end pose repeats within 0.05 mm (%.4f)" % d)

    # 2. payload declared
    for k, v in zip(("mass", "cx", "cy", "cz"), (5, 0, 0, 0.05)):
        setp("stmbl-arm.0.payload-" + k, v)
        setp("armcollide.0.payload-" + k, v)
    time.sleep(0.5)
    ok, m, st = run_program(240)
    check(ok and m.coll == 0, "payload 5 kg declared: program runs clean (coll %d)" % m.coll)

    # 3. payload undeclared
    setp("armcollide.0.payload-mass", 0)
    ok, m, st = run_program(240)
    est = wait(lambda s: s.task_state == linuxcnc.STATE_ESTOP, 1.0)
    check(not ok and m.coll and est, "payload undeclared: collision flagged (joint %d), estop" % m.coll_joint)
    setp("stmbl-arm.0.payload-mass", 0)
    check(machine_on(), "machine on again")

    # 4. push mid-program
    state = {"t": None}

    def push(t, m):
        if t > 4.0 and state["t"] is None:
            setp("stmbl-arm.0.force-z", -150)
            setp("stmbl-arm.0.force-rz", 0.1)
            setp("stmbl-arm.0.force-link", 6)
            state["t"] = time.time()
    ok, m, st = run_program(240, push)
    setp("stmbl-arm.0.force-link", 0)
    est = wait(lambda s: s.task_state == linuxcnc.STATE_ESTOP, 1.0)
    check(not ok and m.coll and est,
          "push 150 N mid-program: collision (joint %d), estop" % m.coll_joint)
    check(hits() == 0, "push: no hard stop hit")
    check(machine_on(), "machine on again")

    # 5. link loss
    state = {"t": None}

    def lose(t, m):
        if t > 4.0 and state["t"] is None:
            setp("stmbl-arm.0.2.link-lost", 1)
            state["t"] = time.time()
    ok, m, st = run_program(240, lose)
    errs = errors()
    setp("stmbl-arm.0.2.link-lost", 0)
    check(not ok and st != linuxcnc.STATE_ON, "drive 3 link lost mid-program: machine off (state %d)" % st)
    check(any("amplifier fault" in e for e in errs), "  ...as an amplifier fault on joint 2", errs[:3])
    check(hits() == 0, "link loss: no hard stop hit")
    time.sleep(1.0)
    check(machine_on(), "machine on again (link back)")

    # 6. operator estop in the weave
    state = {"t": None, "q": None}

    def estop(t, m):  # in the weave: x 780..880 at y ~ -150, z 1100
        s = poll()
        x, y, z = s.actual_position[:3]
        if state["t"] is None and 800 < x < 860 and abs(y + 150) < 6 and abs(z - 1100) < 1:
            C.state(linuxcnc.STATE_ESTOP)
            state["t"] = time.time()
            state["q"] = [hv("stmbl-arm.0.%d.q" % j) for j in range(6)]
    ok, m, st = run_program(240, estop)
    time.sleep(1.0)
    q1 = [hv("stmbl-arm.0.%d.q" % j) for j in range(6)]
    d = max(abs(a - b) for a, b in zip(q1, state["q"] or q1))
    est = wait(lambda s: s.task_state == linuxcnc.STATE_ESTOP, 1.0)
    check(state["t"] is not None and est, "operator estop in the weave")
    check(state["t"] is not None and d < 0.5, "  ...the brakes stop the arm within 0.5 deg (%.3f)" % d)
    check(machine_on(), "machine on again")

    # 7. soft limits
    C.teleop_enable(0)
    C.wait_complete()
    C.jog(linuxcnc.JOG_INCREMENT, 1, 1, 30.0, 200.0)
    still, t_end = 0, time.time() + 20  # wait until J2 has stood for 0.5 s
    while time.time() < t_end and still < 50:
        s = poll()
        still = still + 1 if abs(s.joint[1]["velocity"]) < 1e-9 else 0
        time.sleep(0.01)
    s = poll()
    info.append("# soft-limit jog: state %d, errors %s" % (s.task_state, errors()[:2]))
    lim = s.joint[1]["max_position_limit"]
    check(s.joint_actual_position[1] <= lim + 0.01 and hits() == 0,
          "jog J2 200 deg up: stops at its soft limit %.1f (at %.2f), no stop hit"
          % (lim, s.joint_actual_position[1]))
    C.state(linuxcnc.STATE_OFF)


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        check(False, "exception: %r" % e)
    out, n_fail = [], 0
    for i, (ok, name, detail) in enumerate(results, 1):
        out.append("%s %d - %s" % ("ok" if ok else "not ok", i, name))
        if not ok:
            n_fail += 1
            if detail:
                out.append("#   %s" % (detail,))
    out.append("1..%d" % len(results))
    out.append("# %d/%d passed" % (len(results) - n_fail, len(results)))
    text = "\n".join(info + out) + "\n"
    sys.stdout.write(text)
    open("lcnc_soak.tap", "w").write(text)
    open("lcnc_soak.result", "w").write("%d\n" % n_fail)
