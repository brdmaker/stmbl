#!/usr/bin/env python3
"""LinuxCNC runs the simulated ARC Mate with the electric gripper: a
headless DISPLAY program (make test-grip: arcmate.ini with
arcmate_gripper.arm, cell_grip.txt and grip.hal).

1. Pick and place: box1 (2 kg, 30 mm) gripped at A with M64 / M66 (width
   measured, armcollide's model takes the declared 2 kg), lifted (the
   payload check: ok), carried, placed at B and released (the model drops
   it). No collision, no fault.
2. The part slips out mid-carry (stmbl-arm.0.grip.drop): LOST, estop
   (armgrip reason 7).
3. A part declared 4.5 kg that weighs 2: the lift's check flags it, estop
   (reason 6).
4. Closing on nothing with a part expected: estop (reason 8).

Prints TAP and writes lcnc_grip_test.tap / .result.
"""
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


def mdi(cmd, timeout=30.0):
    """run one MDI line; True when done without an estop"""
    C.mode(linuxcnc.MODE_MDI)
    C.wait_complete()
    C.mdi(cmd)
    t = time.time() + timeout
    time.sleep(0.05)
    while time.time() < t:
        s = poll()
        if s.task_state != linuxcnc.STATE_ON:
            return False
        if s.interp_state == linuxcnc.INTERP_IDLE and s.queue == 0:
            return True
        time.sleep(0.01)
    return False


def machine_on():
    C.state(linuxcnc.STATE_ESTOP_RESET)
    C.state(linuxcnc.STATE_ON)
    ok = wait(lambda s: s.task_state == linuxcnc.STATE_ON, 5)
    time.sleep(0.5)
    return ok


def watch(cmd, timeout, inject=None):
    """an MDI move, sampling; inject(t) may act during it. Returns the
    samples' collision count, estop seen, armgrip reason"""
    C.mode(linuxcnc.MODE_MDI)
    C.wait_complete()
    C.mdi(cmd)
    t0 = time.time()
    coll, estop = 0, False
    while time.time() - t0 < timeout:
        s = poll()
        if inject:
            inject(time.time() - t0)
        coll += int(g("armcollide.0.collision"))
        if s.task_state != linuxcnc.STATE_ON:
            estop = True
            break
        if time.time() - t0 > 0.2 and s.interp_state == linuxcnc.INTERP_IDLE and s.queue == 0:
            break
        time.sleep(0.01)
    return coll, estop, g("armgrip.0.reason")


def pick(x, y, z, side, width, mass, cz=0.15):
    """approach from `side` mm along y, close on the part; True if gripped"""
    steps = ["G90 G0 X%.1f Y%.1f Z%.1f A0 B0 C0" % (x, y + side, z), "G1 Y%.1f F3000" % y,
             "M68 E0 Q%g" % width, "M68 E1 Q%g" % mass, "M68 E2 Q%g" % cz, "M64 P0", "M66 P0 L3 Q5"]
    for st in steps:
        if not mdi(st):
            s = poll()
            INFO.append("# pick at %g %g %g: '%s' failed (task %d, guard reason %d, grip state %d, pose %s)" % (
                x, y, z, st, s.task_state, g("armguard.0.reason"), g("armgrip.0.state"),
                " ".join("%.1f" % v for v in s.actual_position[:6])))
            return False
    if not g("armgrip.0.gripped"):
        INFO.append("# pick at %g %g %g: state %d, width %.2f, gap %.2f" % (
            x, y, z, g("armgrip.0.state"), g("armgrip.0.width"), g("stmbl-arm.0.grip.pos-fb")))
    return g("armgrip.0.gripped")


def main():
    h = hal.component("lcnc_grip_test")
    h.ready()
    if not wait(lambda s: s.task_state != 0, 20):
        check(False, "LinuxCNC task up")
        return
    check(machine_on(), "machine on")
    C.mode(linuxcnc.MODE_MANUAL)
    C.wait_complete()
    C.home(-1)
    check(wait(lambda s: all(s.homed[j] for j in range(6)), 10), "all joints homed (absolute)")
    C.teleop_enable(1)
    C.wait_complete()
    check(abs(g("stmbl-arm.0.grip.pos-fb") - 75) < 0.5 and g("armgrip.0.state") == 0,
          "gripper open at 75 mm, no fault", g("stmbl-arm.0.grip.pos-fb"))

    # 1. pick at A, lift (the check), carry, place at B, release
    gripped = pick(850, -50, 1150, -100, 30, 2.0)
    w = g("armgrip.0.width")
    check(gripped and abs(w - 30) < 0.5, "box1 gripped, width %.2f mm" % w)
    check(g("armcollide.0.payload-mass") == 2.0 and g("stmbl-arm.0.grip.held") == 0,
          "  ...armcollide's model has the declared 2 kg; the plant holds it")
    coll, estop, _ = watch("G1 Z1300 F9000", 10)
    time.sleep(0.2)
    m = g("armgrip.0.pay-meas")
    INFO.append("# box1 weighed on the lift: %.2f kg" % m)
    check(not estop and g("armgrip.0.pay-ok") and abs(m - 2.0) < 1.0,
          "  ...lifted: payload check ok (%.2f kg)" % m, "pay-state %d" % g("armgrip.0.pay-state"))
    c2, e2, _ = watch("G1 Y250 F9000", 15)
    ok = not e2 and mdi("G1 Z1150 F6000") and mdi("M65 P0") and mdi("M66 P0 L4 Q5")
    time.sleep(1.5)
    check(ok and g("armgrip.0.state") == 0 and g("stmbl-arm.0.grip.held") == -1,
          "  ...placed at B and released", "state %d" % g("armgrip.0.state"))
    check(g("armcollide.0.payload-mass") == 0.0, "  ...the model dropped the payload")
    ok = mdi("G1 Y350 F3000")
    check(ok and coll + c2 == 0 and g("armcollide.0.collision") == 0 and g("armgrip.0.fault") == 0,
          "  ...no collision, no gripper fault through the cycle")

    # 2. box2 slips out on the way
    gripped = pick(850, 100, 1200, -100, 30, 2.0)
    check(gripped, "box2 gripped")
    coll, estop, _ = watch("G1 Z1350 F9000", 10)
    t_drop = []

    def drop(t):
        if t > 0.5 and not t_drop:
            hal.set_p("stmbl-arm.0.grip.drop", "1")
            t_drop.append(t)
    coll, estop, reason = watch("G1 X650 F6000", 15, drop)
    time.sleep(0.3)
    check(estop and reason == 7 and g("armgrip.0.state") == 6, "  ...slips out mid-carry: lost, estop (reason %d)"
          % reason, "estop %s state %d" % (estop, g("armgrip.0.state")))
    hal.set_p("stmbl-arm.0.grip.drop", "0")

    # 3. box1 at B, declared 4.5 kg
    check(machine_on() and g("armgrip.0.fault") == 0, "machine on again, gripper fault cleared")
    ok = mdi("M65 P0") and mdi("M66 P0 L4 Q1")
    time.sleep(1.5)
    gripped = pick(850, 250, 1150, 100, 30, 4.5)
    check(gripped and g("armcollide.0.payload-mass") == 4.5, "box1 gripped, declared 4.5 kg")
    coll, estop, reason = watch("G1 Z1300 F9000", 10)
    time.sleep(0.3)
    check(estop and g("armgrip.0.reason") == 6 and g("armgrip.0.pay-state") == 4,
          "  ...the lift weighs %.2f kg: mismatch, estop (reason %d)" % (g("armgrip.0.pay-meas"),
                                                                        g("armgrip.0.reason")),
          "estop %s coll %d" % (estop, coll))

    # 4. nothing there
    check(machine_on(), "machine on again")
    ok = mdi("M65 P0") and mdi("M66 P0 L4 Q1")
    time.sleep(1.5)
    ok = mdi("G0 X700 Y0 Z1150") and mdi("M68 E0 Q30") and mdi("M64 P0")
    wait(lambda s: s.task_state != linuxcnc.STATE_ON, 4)
    check(poll().task_state != linuxcnc.STATE_ON and g("armgrip.0.reason") == 8 and g("armgrip.0.state") == 4,
          "closing on nothing: empty, estop (reason %d)" % g("armgrip.0.reason"))
    machine_on()
    mdi("M65 P0")
    time.sleep(2.0)
    C.state(linuxcnc.STATE_OFF)
    time.sleep(0.5)


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        check(False, "exception: %r" % e)
    n_fail = 0
    out = []
    for i, (ok, name, detail) in enumerate(results, 1):
        out.append("%s %d - %s" % ("ok" if ok else "not ok", i, name))
        if not ok:
            n_fail += 1
            if detail:
                out.append("#   %s" % (detail,))
    out.append("1..%d" % len(results))
    out.append("# %d/%d passed" % (len(results) - n_fail, len(results)))
    text = "\n".join(INFO + out) + "\n"
    sys.stdout.write(text)
    sys.stdout.flush()
    open("lcnc_grip_test.tap", "w").write(text)
    open("lcnc_grip_test.result", "w").write("%d\n" % n_fail)
