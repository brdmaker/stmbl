#!/usr/bin/env python3
"""LinuxCNC drives the simulated ARC Mate: a headless DISPLAY program.

    linuxcnc -d arcmate.ini   with [DISPLAY] DISPLAY = ./lcnc_test.py
    (make test does this with a copy of arcmate.ini)

Estop reset, machine on, home (absolute encoders: no motion), then MDI moves
in Cartesian space through genserkins: a straight line, a move with a
wrist reorientation, a joint move, back home; machine off at the end.
While it moves it samples, every servo-ish period LinuxCNC lets a user
program see (~10 ms): LinuxCNC's joint following error, the true link
angles of the simulation (stmbl-arm.0.J.q) against LinuxCNC's joint
command, faults and hard stop hits.

Prints TAP ("ok N - ...") and exits; LinuxCNC then shuts down. Limits are
for the guessed arm (host/arm/arcmate) and the drives' default gains.
"""
import math
import sys
import time

import hal
import linuxcnc

S = linuxcnc.stat()
C = linuxcnc.command()
results = []
INFO = []  # "# ..." lines for the report


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


class Sampler:
    """max |joint ferror| [deg], max |command - true link angle| [deg]"""

    def __init__(self):
        self.ferr = [0.0] * 6
        self.link = [0.0] * 6
        self.faults = 0
        self.collisions = 0

    def sample(self):
        s = poll()
        for j in range(6):
            js = s.joint[j]
            self.ferr[j] = max(self.ferr[j], abs(js["ferror_current"]))
            q = hal.get_value("stmbl-arm.0.%d.q" % j)
            cmd = hal.get_value("joint.%d.motor-pos-cmd" % j)
            self.link[j] = max(self.link[j], abs(cmd - q))
            self.faults += int(hal.get_value("stmbl-arm.0.%d.fault" % j))
        self.collisions += int(hal.get_value("armcollide.0.collision"))

    def run_until_done(self, timeout):
        t = time.time() + timeout
        time.sleep(0.05)
        while time.time() < t:
            self.sample()
            s = poll()
            if s.interp_state == linuxcnc.INTERP_IDLE and s.queue == 0 and s.inpos:
                return True
            time.sleep(0.01)
        return False


def mdi(cmd, timeout=30.0):
    C.mode(linuxcnc.MODE_MDI)
    C.wait_complete()
    C.mdi(cmd)
    smp = Sampler()
    ok = smp.run_until_done(timeout)
    return ok, smp


def main():
    h = hal.component("lcnc_test")  # hal.get_value needs a component
    h.ready()
    if not wait(lambda s: s.task_state != 0, 20):
        check(False, "LinuxCNC task up")
        return
    C.state(linuxcnc.STATE_ESTOP_RESET)
    C.state(linuxcnc.STATE_ON)
    check(wait(lambda s: s.task_state == linuxcnc.STATE_ON, 5), "machine on")
    time.sleep(0.5)  # drives enabled, brakes released
    C.mode(linuxcnc.MODE_MANUAL)
    C.wait_complete()
    C.home(-1)
    check(wait(lambda s: all(s.homed[j] for j in range(6)), 10), "all joints homed (absolute)")
    C.teleop_enable(1)
    C.wait_complete()

    s = poll()
    home_joints = list(s.joint_actual_position[:6])
    home_pos = list(s.actual_position[:6])
    INFO.append("# home joints %s" % ["%.3f" % v for v in home_joints])
    INFO.append("# home pose   %s" % ["%.2f" % v for v in home_pos])
    check(abs(home_joints[1] + 90) < 0.1 and abs(home_joints[4] + 90) < 0.1,
          "LinuxCNC joints at the arm's home pose (DH theta)", home_joints)

    # straight line in X and Z, orientation kept
    x, y, z, a, b, c = home_pos
    ok, smp = mdi("G90 G1 X%.3f Y%.3f Z%.3f A%.3f B%.3f C%.3f F3000" % (x + 150, y, z - 100, a, b, c))
    time.sleep(0.3)  # settle
    s = poll()
    p = s.actual_position
    check(ok, "MDI line done")
    err = math.sqrt((p[0] - x - 150) ** 2 + (p[1] - y) ** 2 + (p[2] - z + 100) ** 2)
    check(err < 0.05, "line: tool point within 0.05 mm of target (%.4f mm)" % err)
    check(max(smp.ferr) < 0.2, "line: LinuxCNC joint ferror < 0.2 deg (%.3f)" % max(smp.ferr))
    check(max(smp.link) < 0.3, "line: true link angle within 0.3 deg of command (%.3f)" % max(smp.link))
    check(smp.faults == 0, "line: no drive fault")
    check(smp.collisions == 0, "line: no collision detected")

    # reorient the wrist about A and B while moving
    ok, smp = mdi("G1 X%.3f Y%.3f Z%.3f A%.3f B%.3f C%.3f F2000" % (x + 50, y + 100, z, a + 20, b - 15, c))
    check(ok, "MDI line with reorientation done")
    check(max(smp.ferr) < 0.3, "reorient: LinuxCNC joint ferror < 0.3 deg (%.3f)" % max(smp.ferr))
    check(smp.faults == 0, "reorient: no drive fault")
    check(smp.collisions == 0, "reorient: no collision detected")

    # back home in joint space (G53-free: MDI in world, then joint jog home)
    ok, smp = mdi("G0 X%.3f Y%.3f Z%.3f A%.3f B%.3f C%.3f" % tuple(home_pos))
    s = poll()
    j_err = max(abs(s.joint_actual_position[j] - home_joints[j]) for j in range(6))
    check(ok and j_err < 0.05, "rapid back home: joints within 0.05 deg (%.4f)" % j_err)
    check(max(smp.ferr) < 0.5, "rapid: LinuxCNC joint ferror < 0.5 deg (%.3f)" % max(smp.ferr))
    check(smp.collisions == 0, "rapid: no collision detected")
    hits = sum(int(hal.get_value("stmbl-arm.0.%d.stop-hits" % j)) for j in range(6))
    check(hits == 0, "no hard stop hit")

    # collision: 150 N down on the flange in the middle of a line; the
    # detector drops emc-enable-in: LinuxCNC goes to estop, drives off,
    # brakes on
    check(hal.get_value("armcollide.0.armed"), "collision detector armed")
    C.mode(linuxcnc.MODE_MDI)
    C.wait_complete()
    C.mdi("G1 X%.3f Y%.3f Z%.3f A%.3f B%.3f C%.3f F3000" % (x + 200, y - 100, z - 100, a, b, c))
    time.sleep(1.0)
    q_force = [hal.get_value("stmbl-arm.0.%d.q" % j) for j in range(6)]
    hal.set_p("stmbl-arm.0.force-z", "-150")
    hal.set_p("stmbl-arm.0.force-rz", "0.1")
    hal.set_p("stmbl-arm.0.force-link", "6")
    t0 = time.time()
    seen = []  # the joint, caught while collision is up (estop clears it)
    t_end = t0 + 2.0
    while time.time() < t_end and not seen:
        if hal.get_value("armcollide.0.collision"):
            seen = [hal.get_value("armcollide.0.joint"), time.time() - t0]
        time.sleep(0.0005)
    hit = wait(lambda s: s.task_state == linuxcnc.STATE_ESTOP, 2.0, 0.001)
    dt = time.time() - t0
    check(seen, "collision: detected (joint %s, %.0f ms wall clock after the push)" %
          (seen[0] if seen else "-", seen[1] * 1000 if seen else 0))
    check(hit, "collision: LinuxCNC in estop %.0f ms (wall clock) after the push" % (dt * 1000))
    time.sleep(0.5)
    hal.set_p("stmbl-arm.0.force-link", "0")
    q_stop = [hal.get_value("stmbl-arm.0.%d.q" % j) for j in range(6)]
    moved = max(abs(q_stop[j] - q_force[j]) for j in range(6))
    check(moved < 5.0, "collision: arm stopped within 5 deg of where it was pushed (%.2f)" % moved)
    C.state(linuxcnc.STATE_ESTOP_RESET)
    C.state(linuxcnc.STATE_ON)
    check(wait(lambda s: s.task_state == linuxcnc.STATE_ON, 5), "machine on again after the collision")
    time.sleep(0.5)

    # collision with the fixture plate (cell.txt, face at y = -350 mm):
    # armguard stops, backs off 20 mm along the path, then estop. The tool
    # is free (no contact force) and held clear.
    ok, smp = mdi("G0 X%.3f Y%.3f Z%.3f A%.3f B%.3f C%.3f" % (850, -150, 1150, a, b, c))
    check(ok and hal.get_value("stmbl-arm.0.clearance") > 30, "next to the fixture, clear (%.1f mm)"
          % hal.get_value("stmbl-arm.0.clearance"))
    C.mdi("G1 Y-400 F3000")
    t0 = time.time()
    fmax, states = 0.0, set()
    while time.time() - t0 < 4.0:
        fmax = max(fmax, hal.get_value("stmbl-arm.0.contact-force"))
        states.add(hal.get_value("armguard.0.state"))
        if poll().task_state == linuxcnc.STATE_ESTOP:
            break
        time.sleep(0.001)
    time.sleep(0.5)
    f_end, clr = hal.get_value("stmbl-arm.0.contact-force"), hal.get_value("stmbl-arm.0.clearance")
    check(poll().task_state == linuxcnc.STATE_ESTOP, "fixture: collision -> estop")
    check(2 in states, "  ...after a backoff (guard states seen %s)" % sorted(states))
    check(fmax > 50, "  ...the wrist did hit the fixture (peak %.0f N)" % fmax)
    check(f_end == 0 and clr > 10, "  ...tool free: no contact force (%.1f N), %.1f mm clear" % (f_end, clr))
    C.state(linuxcnc.STATE_ESTOP_RESET)
    C.state(linuxcnc.STATE_ON)
    check(wait(lambda s: s.task_state == linuxcnc.STATE_ON, 5), "machine on again after the backoff")
    time.sleep(0.5)

    # machine off: brakes hold
    q0 = [hal.get_value("stmbl-arm.0.%d.q" % j) for j in range(6)]
    C.state(linuxcnc.STATE_OFF)
    time.sleep(1.0)
    q1 = [hal.get_value("stmbl-arm.0.%d.q" % j) for j in range(6)]
    d = max(abs(q1[j] - q0[j]) for j in range(6))
    check(d < 0.05, "machine off: brakes hold the arm (moved %.4f deg)" % d)


if __name__ == "__main__":
    try:
        main()
    except Exception as e:  # report, and let LinuxCNC shut down
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
    open("lcnc_test.tap", "w").write(text)  # LinuxCNC keeps the display's stdout
    open("lcnc_test.result", "w").write("%d\n" % n_fail)
