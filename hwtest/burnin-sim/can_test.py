#!/usr/bin/env python3
"""LinuxCNC + the simulated drive over CAN (make test-can): a headless
DISPLAY program. can.hal: the real stmblcan component (spi=sim) on the
simulated drive's simulated MCP2515, in LinuxCNC's servo thread.

1. at load stmblcan asked the drive for its description: the node is
   present, the slot values (supply, temperature) arrive;
2. machine on, joint 0 homed on the encoder index (stmblcan clears
   index-enable when the drive reports it found), rapids X 0 <-> 300 at 500 mm/s: no following error
   (FERROR 1 mm), no fault, the position arrives, the bus counters clean
   (no frame late, no RX overflow, no feedback missed while moving), and
   the velocity never clipped (vel_scale: the drive's, 600, from discovery);
3. the console over CAN: servoterm-cli through sserialterm on the node's
   con pins, about and a pin, also while the axis moves;
4. the drive's power and current while it moves come over the slots.
"""
import os
import subprocess
import sys
import time

import hal
import linuxcnc

S = linuxcnc.stat()
C = linuxcnc.command()
HERE = os.path.dirname(os.path.abspath(__file__))
TTY = os.environ.get("CON_TTY", os.path.join(HERE, "drive-con"))
CLI = os.environ.get("SERVOTERM_CLI", "servoterm-cli")
N = "stmblcan.0.1."
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


def cli(*args, timeout=60):
    r = subprocess.run([CLI, "-d", TTY, "-t", "1"] + list(args), capture_output=True, text=True, timeout=timeout)
    return r.returncode, r.stdout + r.stderr


def g(name):
    return hal.get_value(name)


def main():
    h = hal.component("can_test")
    h.ready()
    if not wait(lambda s: s.task_state != 0, 20):
        check(False, "LinuxCNC task up")
        return
    time.sleep(1.0)
    check(g(N + "present") and g("stmblcan.0.ok"), "stmblcan: the drive found at load (discover=1), present")
    vbus, temp = g(N + "vbus"), g(N + "temp")
    INFO.append("# slots at rest: vbus %.1f V, temp %.1f C" % (vbus, temp))
    check(abs(vbus - 297) < 3 and 0 < temp < 100, "slot values over CAN: supply and temperature",
          "vbus %g temp %g" % (vbus, temp))

    rc, out = cli("-c", "about", "-c", "can0.syncs")
    check(rc == 0 and "STMBL simulated drive" in out and "can0.syncs" in out,
          "servoterm-cli over the console over CAN: about, a pin", out[-300:])

    C.state(linuxcnc.STATE_ESTOP_RESET)
    C.state(linuxcnc.STATE_ON)
    ok = wait(lambda s: s.task_state == linuxcnc.STATE_ON, 5)
    time.sleep(3.0)  # the drive phases
    C.mode(linuxcnc.MODE_MANUAL)
    C.wait_complete()
    C.home(-1)
    homed = wait(lambda s: all(s.homed[j] for j in range(3)), 30)
    s = poll()
    INFO.append("# index homing: X %.4f after it, index-enable %d" % (s.joint_actual_position[0], g(N + "index-enable")))
    check(homed and not g(N + "index-enable") and abs(s.joint_actual_position[0]) < 0.05,
          "joint 0 homed on the encoder index over CAN: index-enable cleared by stmblcan",
          "homed %s X %g" % (s.homed[:3], s.joint_actual_position[0]))
    C.mode(linuxcnc.MODE_MDI)
    C.wait_complete()
    missed0 = g(N + "missed")
    late0, ovf0 = g("stmblcan.0.tx-late"), g("stmblcan.0.rx-overflow")
    pmax = imax = 0.0
    answers = 0
    for i, x in enumerate((300, 0, 300, 0)):
        C.mdi("G0 X%d" % x)
        if i == 1:
            rc, out = cli("-c", "fault0.state", "-c", "about")
            answers += "STMBL simulated drive" in out
        t = time.time() + 10
        while time.time() < t:
            s = poll()
            pmax = max(pmax, abs(g(N + "power")))
            imax = max(imax, abs(g(N + "current")))
            if s.interp_state == linuxcnc.INTERP_IDLE and s.queue == 0 and abs(s.joint[0]["velocity"]) < 1e-6:
                break
            time.sleep(0.005)
        time.sleep(0.2)
        s = poll()
        if abs(s.joint_actual_position[0] - x) > 0.01:
            break
    s = poll()
    err = linuxcnc.error_channel().poll()
    INFO.append("# after 4 rapids: X %.4f, task %d, missed %d, tx-late %d, rx-overflow %d, error %s"
                % (s.joint_actual_position[0], s.task_state, g(N + "missed") - missed0,
                   g("stmblcan.0.tx-late") - late0, g("stmblcan.0.rx-overflow") - ovf0, err))
    check(ok and s.task_state == linuxcnc.STATE_ON and abs(s.joint_actual_position[0]) < 0.01 and not g(N + "fault"),
          "rapids X 0 <-> 300 at 500 mm/s over CAN: no following error, no fault, back at 0",
          "task %d X %g err %s" % (s.task_state, s.joint_actual_position[0], err))
    check(g(N + "vel-clipped") == 0, "vel-cmd within the drive's vel_scale (discovered): never clipped",
          "%d periods clipped" % g(N + "vel-clipped"))
    check(g(N + "missed") == missed0 and g("stmblcan.0.tx-late") == late0 and g("stmblcan.0.rx-overflow") == ovf0,
          "bus counters while moving: no feedback missed, no frame late, no RX overflow")
    check(answers == 1, "console over CAN while the axis moves: answers right")
    INFO.append("# slots while moving: power up to %.0f W, current up to %.1f A" % (pmax, imax))
    # unloaded motor: ~0.2 A peaks; the slots sample each value every 32 ms
    check(pmax > 20 and imax > 0.05, "slot values while moving: power and current", "%g W %g A" % (pmax, imax))
    INFO.append("# sserialterm over CAN: %d bytes from the drive, %d to it"
                % (g("sserialterm.0.bytes-in"), g("sserialterm.0.bytes-out")))


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
    open("can_test.tap", "w").write(text)
    open("can_test.result", "w").write("%d\n" % n_fail)
