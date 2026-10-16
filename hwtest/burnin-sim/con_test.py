#!/usr/bin/env python3
"""LinuxCNC + the simulated drive + sserialterm (make test-console): a
headless DISPLAY program. The drive's console over smart-serial, through
the pty, with servoterm-cli as on a real drive's USB port:

1. about, a pin read, a pin write and read back;
2. --dump: the drive's whole flash config, identical to the file it booted;
3. while the axis moves (MDI rapids), console traffic: no following error,
   no fault, the answers right.
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


def main():
    h = hal.component("con_test")
    h.ready()
    if not wait(lambda s: s.task_state != 0, 20):
        check(False, "LinuxCNC task up")
        return
    time.sleep(1.0)
    t0 = time.time()
    rc, out = cli("-c", "about", "-c", "pe0.p_in")
    INFO.append("# about + a pin over the console: %.1f s" % (time.time() - t0))
    check(rc == 0 and "STMBL simulated drive" in out and "pe0.p_in" in out,
          "servoterm-cli over the smart-serial console: about, a pin", out[-300:])
    rc, out = cli("-c", "pe0.idle = 21", "-c", "pe0.idle")
    check("pe0.idle = 21" in out.replace("21.000000", "21"), "  ...a pin written and read back", out[-200:])

    dump = os.path.join(HERE, "dump.txt")
    t0 = time.time()
    rc, out = cli("--dump", dump, timeout=120)
    conf = open(os.path.join(HERE, "sim.conf")).read()
    got = open(dump).read() if os.path.exists(dump) else ""
    norm = lambda s: [l.rstrip() for l in s.strip().splitlines()]
    INFO.append("# --dump: %d bytes in %.1f s" % (len(got), time.time() - t0))
    check(rc == 0 and norm(got) == norm(conf), "--dump: the drive's whole flash config, as the file it booted",
          "rc %d, %d vs %d lines" % (rc, len(norm(got)), len(norm(conf))))

    C.state(linuxcnc.STATE_ESTOP_RESET)
    C.state(linuxcnc.STATE_ON)
    ok = wait(lambda s: s.task_state == linuxcnc.STATE_ON, 5)
    time.sleep(3.0)
    C.mode(linuxcnc.MODE_MANUAL)
    C.wait_complete()
    C.home(-1)
    wait(lambda s: all(s.homed[j] for j in range(3)), 30)
    C.mode(linuxcnc.MODE_MDI)
    C.wait_complete()
    C.mdi("G0 X300")
    time.sleep(0.2)
    answers = 0
    for i in range(3):
        rc, out = cli("-c", "fault0.state", "-c", "about")
        answers += "STMBL simulated drive" in out
        C.mdi("G0 X%d" % (0 if i % 2 == 0 else 300))
    wait(lambda s: s.interp_state == linuxcnc.INTERP_IDLE and s.queue == 0, 20)
    s = poll()
    err = linuxcnc.error_channel().poll()
    check(ok and s.task_state == linuxcnc.STATE_ON and answers == 3 and not g_fault(),
          "console traffic while the axis moves: answers right, machine still on, no drive fault",
          "task %d answers %d err %s" % (s.task_state, answers, err))
    INFO.append("# sserialterm: %d bytes from the drive, %d to it" % (hal.get_value("sserialterm.0.bytes-in"),
                                                                   hal.get_value("sserialterm.0.bytes-out")))
    C.state(linuxcnc.STATE_OFF)


def g_fault():
    return hal.get_value("stmbl-sim.0.fault")


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
    open("con_test.tap", "w").write(text)
    open("con_test.result", "w").write("%d\n" % n_fail)
