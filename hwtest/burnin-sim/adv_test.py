#!/usr/bin/env python3
"""tools/sserial_advance.py on the simulated drive (make test-advance): a
headless DISPLAY program. The machine on and homed, the tool runs a back
and forth program on X and fits LinuxCNC's following error:

1. with the drive's pos_advance as configured (conf/template/sserial.txt,
   0.0006): a result, inside 0..2 ms;
2. pos_advance set 0.5 ms longer over the drive's console: the drive now
   reports its position 0.5 ms ahead, and the tool must say so (its k
   about -0.5 ms, its new value back where 1. put it).

This is the whole chain of the tool: LinuxCNC's stat, a program in auto,
the fit (whose math host/tests/test_sserial_advance.py checks alone).
"""
import os
import subprocess
import sys
import time
import types

import hal
import linuxcnc

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.environ.get("STMBL_TOOLS", os.path.join(HERE, "..", "..", "tools")))
import sserial_advance as sa  # noqa: E402

S = linuxcnc.stat()
C = linuxcnc.command()
TTY = os.environ.get("CON_TTY", os.path.join(HERE, "stmbl-sim"))
CLI = os.environ.get("SERVOTERM_CLI", "servoterm-cli")
results, INFO = [], []


def check(cond, name, detail=""):
    results.append((bool(cond), name, detail))


def wait(pred, timeout, step=0.01):
    t = time.time() + timeout
    while time.time() < t:
        S.poll()
        if pred(S):
            return True
        time.sleep(step)
    return False


def cli(*args, timeout=60):
    r = subprocess.run([CLI, "-d", TTY, "-t", "1"] + list(args), capture_output=True, text=True, timeout=timeout)
    return r.returncode, r.stdout + r.stderr


def measure(advance):
    prog = os.path.join(HERE, "adv_move.ngc")
    rows = sa.log(types.SimpleNamespace(joint=0, program=prog, time=0))
    t = [r[0] for r in rows]
    r = sa.fit(t, [x[1] for x in rows], [x[2] for x in rows], advance)
    INFO.extend("# " + s for s in r["lines"])
    return r


def main():
    h = hal.component("adv_test")
    h.ready()
    if not wait(lambda s: s.task_state != 0, 20):
        check(False, "LinuxCNC task up")
        return
    with open(os.path.join(HERE, "adv_move.ngc"), "w") as f:
        f.write("G90 G0 X0\n" + "G1 X300 F24000\nG1 X0\n" * 4 + "M2\n")
    rc, out = cli("-c", "sserial0.pos_advance")
    INFO.append("# drive: " + out.strip().replace("\n", " | ")[-120:])
    C.state(linuxcnc.STATE_ESTOP_RESET)
    C.state(linuxcnc.STATE_ON)
    on = wait(lambda s: s.task_state == linuxcnc.STATE_ON, 5)
    time.sleep(3.0)  # the drive phases
    C.mode(linuxcnc.MODE_MANUAL)
    C.wait_complete()
    C.home(-1)
    homed = wait(lambda s: all(s.homed[j] for j in range(3)), 30)
    check(on and homed, "machine on and homed")

    base = 0.0006
    r1 = measure(base)
    check(0 <= r1["new"] <= 0.002, "configured pos_advance: the tool gives a value inside 0..2 ms",
          "new %.5f" % r1["new"])

    rc, out = cli("-c", "sserial0.pos_advance = %g" % (base + 0.0005), "-c", "sserial0.pos_advance")
    check(rc == 0 and "0.0011" in out, "pos_advance 0.5 ms longer over the drive console", out[-200:])
    r2 = measure(base + 0.0005)
    check(abs(r2["k"] + 0.0005) < 0.00005, "0.5 ms too long: the tool finds it (k about -0.5 ms)",
          "k %.1f us" % (r2["k"] * 1e6))
    check(abs(r2["new"] - r1["new"]) < 0.00005, "and gives back the same pos_advance as before",
          "%.5f against %.5f" % (r2["new"], r1["new"]))
    cli("-c", "sserial0.pos_advance = %g" % base)


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
    open("adv_test.tap", "w").write(text)
    open("adv_test.result", "w").write("%d\n" % n_fail)
    C.state(linuxcnc.STATE_ESTOP)
