#!/usr/bin/env python3
"""LinuxCNC with the pneumatic tool changer on the simulated ARC Mate: a
headless DISPLAY program (make test-tool: arcmate.ini with
arcmate_changer.arm, cell_changer.txt, grip.hal + changer.hal, armkins
as the kinematics, M6 remapped to toolchange.ngc).

1. T2 M6: the gripper picked from its dock; armtool sees it locked and
   identified, armkins runs on its TCP, armcollide has its mass; a move
   on the TCP lands there.
2. T1 M6: the gripper back, the torch (bent neck) picked; a reorientation
   about the TCP keeps the tip in place.
3. T0 M6: the torch back, no tool.
4. The tool comes off mid-move: estop (armtool reason 3). Put right: the
   flange's kinematics (M68 E3 Q0), M61 Q0.
5. Unlock requested away from the dock: refused, estop (reason 4), the
   tool stays on.
No collision hit through the changes.
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


def mdi(cmd, timeout=60.0):
    """one MDI line; True when done with the machine still on"""
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


def state(label):
    INFO.append("# %s: mounted %d kins %d plant %d spindle %d fault %d pos %s" % (
        label, g("armtool.0.mounted"), g("armkins.tool"), g("stmbl-arm.0.changer.plant-tool"),
        poll().tool_in_spindle, g("armtool.0.reason"),
        " ".join("%.1f" % v for v in S.actual_position[:6])))


def tool_is(n):
    return (g("armtool.0.mounted") == n and g("armkins.tool") == n and g("stmbl-arm.0.changer.plant-tool") == n
            and poll().tool_in_spindle == n and g("armtool.0.fault") == 0)


def main():
    h = hal.component("lcnc_tool_test")
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
    check(g("armtool.0.mounted") == 0 and g("armtool.0.fault") == 0, "no tool, no changer fault")
    coll = 0

    # 1. the gripper
    ok = mdi("G90 G1 X900 Y300 Z1150 A0 B45 C0 F6000") and mdi("T2 M6")
    state("T2 M6")
    check(ok and tool_is(2), "T2 M6: gripper locked, identified, on its TCP")
    check(abs(g("armcollide.0.tool-mass") - 1.8) < 1e-9, "  ...armcollide's model has its 1.8 kg")
    ok = mdi("G1 X1150 Y0 Z1100 A0 B45 C0 F6000")
    p = poll().actual_position
    check(ok and abs(p[0] - 1150) < 0.2 and abs(p[2] - 1100) < 0.2, "  ...a move on its TCP (%.2f %.2f %.2f)"
          % (p[0], p[1], p[2]))
    coll += g("armcollide.0.collision")

    # 2. the torch
    ok = mdi("T1 M6")
    state("T1 M6")
    check(ok and tool_is(1), "T1 M6: gripper back in its dock, torch on")
    ok = mdi("G1 X1150 Y0 Z1150 A0 B45 C0 F6000")
    p0 = list(poll().actual_position[:3])
    ok = ok and mdi("G1 A15 B30 C10 F2000")
    p1 = list(poll().actual_position[:3])
    d = max(abs(a - b) for a, b in zip(p0, p1))
    check(ok and d < 0.2, "  ...reoriented about the torch tip: it stays within %.3f mm" % d)
    coll += g("armcollide.0.collision")

    # 3. no tool
    ok = mdi("T0 M6")
    state("T0 M6")
    check(ok and tool_is(0), "T0 M6: torch back, no tool")
    check(coll == 0 and g("armcollide.0.collision") == 0, "  ...no collision hit through the changes")

    # 4. the tool comes off mid-move
    ok = mdi("T2 M6")
    C.mode(linuxcnc.MODE_MDI)
    C.wait_complete()
    C.mdi("G1 X1150 Y0 Z1100 A0 B45 C0 F3000")
    time.sleep(0.5)
    hal.set_p("stmbl-arm.0.changer.slip", "1")
    lost = wait(lambda s: s.task_state != linuxcnc.STATE_ON, 3)
    state("slip")
    check(ok and lost and g("armtool.0.reason") == 3, "tool 2 comes off mid-move: estop (reason %d)"
          % g("armtool.0.reason"))
    hal.set_p("stmbl-arm.0.changer.slip", "0")
    ok = machine_on() and mdi("M68 E3 Q0") and mdi("M66 E0 L0") and mdi("M61 Q0") and mdi("M65 P1")
    time.sleep(0.3)
    state("recovered")
    check(ok and tool_is(0), "  ...put right: no tool, the flange's kinematics")

    # 5. unlock away from the dock
    ok = mdi("T1 M6") and mdi("G1 X1150 Y0 Z1150 A0 B45 C0 F6000")
    C.mode(linuxcnc.MODE_MDI)
    C.wait_complete()
    C.mdi("M65 P1")
    stopped = wait(lambda s: s.task_state != linuxcnc.STATE_ON, 3)
    time.sleep(0.3)
    state("unlock")
    check(ok and stopped and g("armtool.0.reason") == 4 and g("stmbl-arm.0.changer.plant-tool") == 1 and
          g("armtool.0.valve"), "unlock away from the dock: refused, estop (reason %d), the torch stays on"
          % g("armtool.0.reason"))
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
    open("lcnc_tool_test.tap", "w").write(text)
    open("lcnc_tool_test.result", "w").write("%d\n" % n_fail)
