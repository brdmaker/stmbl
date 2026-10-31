#!/usr/bin/env python3
"""LinuxCNC runs linuxcnc/dynwiz on the simulated portal: a headless DISPLAY.

    [DISPLAY] DISPLAY = ./dynwiz_display.py   (make dynwiz sets this)
    DYNWIZ_STEPS="run"  DYNWIZ_OUT=dir

Estop reset, machine on, home (absolute scales: no motion), then
`dynwiz -b lcnc -y STEPS` against this LinuxCNC (its output in
OUT/dynwiz.log, the session in OUT/dynwiz.json), then LinuxCNC shuts down.
"""
import os
import subprocess
import sys
import time

import linuxcnc

HERE = os.path.dirname(os.path.abspath(__file__))
WIZ = os.path.join(HERE, "..", "..", "linuxcnc", "dynwiz", "dynwiz")
S = linuxcnc.stat()
C = linuxcnc.command()


def wait(pred, timeout):
    t = time.time() + timeout
    while time.time() < t:
        S.poll()
        if pred(S):
            return True
        time.sleep(0.05)
    return False


def main():
    ini = sys.argv[sys.argv.index("-ini") + 1] if "-ini" in sys.argv else os.environ["INI_FILE_NAME"]
    out = os.environ.get("DYNWIZ_OUT", ".")
    steps = os.environ.get("DYNWIZ_STEPS", "run").split()
    log = open(os.path.join(out, "dynwiz.log"), "w")
    if not wait(lambda s: s.task_state != 0, 20):
        log.write("no LinuxCNC\n")
        return
    C.state(linuxcnc.STATE_ESTOP_RESET)
    C.state(linuxcnc.STATE_ON)
    wait(lambda s: s.task_state == linuxcnc.STATE_ON, 5)
    time.sleep(0.3)
    C.mode(linuxcnc.MODE_MANUAL)
    C.wait_complete()
    C.teleop_enable(0)
    C.home(-1)
    wait(lambda s: all(s.homed[j] for j in range(s.joints)), 10)
    C.teleop_enable(1)
    for st in steps:
        p = subprocess.run([sys.executable, WIZ, "-b", "lcnc", "--ini", ini, "-y", "-s", os.path.join(out, "dynwiz.json"),
                            "--conf-out", out, st], stdout=log, stderr=subprocess.STDOUT)
        log.write("== %s: exit %d\n" % (st, p.returncode))
        log.flush()
        if p.returncode:
            break
    C.state(linuxcnc.STATE_OFF)
    time.sleep(0.2)


if __name__ == "__main__":
    main()
