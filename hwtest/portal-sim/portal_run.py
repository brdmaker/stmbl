#!/usr/bin/env python3
"""LinuxCNC runs programs on the simulated portal: a headless DISPLAY.

    [DISPLAY] DISPLAY = ./portal_run.py   (bench.py and make test set this)
    PORTAL_PROGRAMS="a.ngc b.ngc ..."  PORTAL_OUT=dir

Estop reset, machine on, home (absolute scales: no motion), then each
program in turn: the HAL sampler (portal.hal) records every servo cycle
while it runs (halsampler -> OUT/NAME.samples, columns as portal.hal's
sampler pins, after halsampler's sample number), and OUT/NAME.json gets
how it ended: done, or aborted with LinuxCNC's errors (a following error,
a drive fault). After an abort the machine is switched on again and brought
to the middle slowly before the next program. Then LinuxCNC shuts down.
"""
import json
import os
import subprocess
import time

import hal
import linuxcnc

S = linuxcnc.stat()
C = linuxcnc.command()
E = linuxcnc.error_channel()


def poll():
    S.poll()
    return S


def wait(pred, timeout, step=0.02):
    t = time.time() + timeout
    while time.time() < t:
        if pred(poll()):
            return True
        time.sleep(step)
    return False


def errors():
    out = []
    while True:
        e = E.poll()
        if not e:
            return out
        out.append(e[1].strip())


def machine_on():
    C.state(linuxcnc.STATE_ESTOP_RESET)
    C.state(linuxcnc.STATE_ON)
    if not wait(lambda s: s.task_state == linuxcnc.STATE_ON, 5):
        return False
    time.sleep(0.3)  # the drives enabled
    if not all(poll().homed[j] for j in range(3)):
        C.mode(linuxcnc.MODE_MANUAL)
        C.wait_complete()
        C.teleop_enable(0)
        C.home(-1)
        if not wait(lambda s: all(s.homed[j] for j in range(3)), 10):
            return False
    return True


def mdi(cmd, timeout=60):
    C.mode(linuxcnc.MODE_MDI)
    C.wait_complete()
    C.mdi(cmd)
    time.sleep(0.1)
    return wait(lambda s: s.interp_state == linuxcnc.INTERP_IDLE and s.queue == 0, timeout)


def run(path, out):
    name = os.path.splitext(os.path.basename(path))[0]
    res = {"program": name, "status": "done", "errors": []}
    errors()  # drop old ones
    with open(os.path.join(out, name + ".samples"), "w") as f:
        rec = subprocess.Popen(["halsampler", "-c", "0", "-t"], stdout=f)
        time.sleep(0.2)
        hal.set_p("sampler.0.enable", "1")
        C.mode(linuxcnc.MODE_AUTO)
        C.wait_complete()
        C.program_open(path)
        C.wait_complete()
        t0 = time.time()
        C.auto(linuxcnc.AUTO_RUN, 0)
        time.sleep(0.3)
        while True:
            s = poll()
            if s.task_state != linuxcnc.STATE_ON:
                res["status"] = "aborted"
                break
            if s.interp_state == linuxcnc.INTERP_IDLE and s.queue == 0:
                break
            if time.time() - t0 > 900:
                res["status"] = "timeout"
                C.abort()
                break
            time.sleep(0.02)
        res["wall"] = time.time() - t0
        time.sleep(0.2)
        hal.set_p("sampler.0.enable", "0")
        time.sleep(0.3)
        rec.terminate()
        rec.wait()
    res["errors"] = errors()
    if res["errors"] and res["status"] == "done":
        res["status"] = "error"
    json.dump(res, open(os.path.join(out, name + ".json"), "w"))
    if res["status"] != "done":  # back on, slowly to the middle
        time.sleep(0.5)
        if machine_on():
            mdi("G90 G1 X500 Y500 F3000")
    return res


def main():
    h = hal.component("portal_run")  # hal.set_p needs a component
    h.ready()
    out = os.environ.get("PORTAL_OUT", ".")
    progs = os.environ.get("PORTAL_PROGRAMS", "").split()
    if not wait(lambda s: s.task_state != 0, 20) or not machine_on():
        json.dump({"status": "no machine", "errors": errors()}, open(os.path.join(out, "startup.json"), "w"))
        return
    mdi("G90 G1 X500 Y500 F3000")
    for p in progs:
        r = run(os.path.abspath(p), out)
        print("portal_run: %s %s %.1f s %s" % (r["program"], r["status"], r.get("wall", 0), "; ".join(r["errors"])), flush=True)
    C.state(linuxcnc.STATE_OFF)
    time.sleep(0.2)


if __name__ == "__main__":
    main()
