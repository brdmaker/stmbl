#!/usr/bin/env python3
"""LinuxCNC runs test programs on the simulated Scripta CN 720 (host/mill,
stmbl_mill) and checks how well the tool follows.

    mill_test.py [--servo MS,MS..] [--machine NAME,NAME..] [--out DIR] [--no-check]

For each servo period (1: 1 kHz, 0.5: 2 kHz) and machine file (cn720:
resolvers; cn720_blend: 10 um scales blended in; cn720_scales: position
loops on the scales) a copy of this config in /tmp runs the programs below
(mill_run.py records every servo cycle). Per program, while cutting (feed
moves at the cutting depth):

  path      the true tool (relative to the work) against the programmed
            geometry: max and 99th percentile [um] (G64 P0.01 allows 10 um)
  servo     the tool against LinuxCNC's own commanded path (25 ms either
            side): what the drives and the mechanics add, max / rms [um]
  ferror    LinuxCNC's largest joint following error [um]
  I         the largest motor current [A] (the drives allow 8.5)
  head      the spindle tip's largest deflection [um]; bridge: its sway [um]

Results: DIR/results.md. Unless --no-check, it fails (exit 1) when a
program didn't finish or a value is over its bound (BOUNDS).
Not as root (rtapi_app refuses); needs host/arm/libstmbl_drive.so and
stmbl_mill installed (make -C ../../host/mill/lcnc all install).
"""
import argparse
import json
import math
import os
import re
import shutil
import subprocess
import sys
import tempfile

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
LINUXCNC = os.environ.get("LINUXCNC", "/home/user/linuxcnc-2.9")
COLS = "cx cy cz tx ty tz fe0 fe1 fe2 i0 i1 i2 hx hy yb vel mt".split()
CX, CY, R10, R2, ZCUT = 350.0, 215.0, 10.0, 2.0, -25.0

HEAD = "G21 G90 G17 G64 P0.01\nG0 Z-20\n"
TAIL = "G0 Z-20\nM2\n"
PROGRAMS = {  # name: (G-code body, geometry, what it is)
    "circle10_F6000": ("G0 X360 Y215\nG1 Z-25 F1000\nG1 X360 Y215 F6000\nG2 X360 Y215 I-10 J0\nG2 X360 Y215 I-10 J0\n",
                       ("circle", R10), "circle R 10 mm at 6 m/min, 2 turns"),
    "circle10_F12000": ("G0 X360 Y215\nG1 Z-25 F1000\nG1 X360 Y215 F12000\nG2 X360 Y215 I-10 J0\nG2 X360 Y215 I-10 J0\n",
                        ("circle", R10), "circle R 10 mm at 12 m/min (0.4 g)"),
    "circle2_F3000": ("G0 X352 Y215\nG1 Z-25 F1000\nG1 X352 Y215 F3000\nG2 X352 Y215 I-2 J0\nG2 X352 Y215 I-2 J0\n",
                      ("circle", R2), "circle R 2 mm at 3 m/min (a small pocket)"),
    "square20_F15000": ("G0 X340 Y205\nG1 Z-25 F1000\nG61\nG1 X360 Y205 F15000\nG1 X360 Y225\nG1 X340 Y225\nG1 X340 Y205\nG64 P0.01\n",
                        ("square", 20.0), "20 mm square, exact stop (G61), 15 m/min, 0.5 g"),
    "line_F3000": ("G0 X200 Y215\nG1 Z-25 F1000\nG1 X500 Y215 F3000\n",
                   ("line",), "X line 300 mm at 3 m/min"),
}
# (metric, bound): a run fails over it
BOUNDS = {"path_max": 40.0, "servo_max": 40.0, "ferror": 100.0, "gaps": 0}


def setup(d, servo, machine):
    for f in ("mill.ini", "mill.hal", "mill_run.py", "tool.tbl", "mill.var"):
        shutil.copy(os.path.join(HERE, f), d)
    ini = open(os.path.join(d, "mill.ini")).read()
    ini = ini.replace("DISPLAY = axis", "DISPLAY = ./mill_run.py")
    ini = ini.replace("FILE = ../../host/mill/machine/cn720.mill", "FILE = %s/host/mill/machine/%s.mill" % (ROOT, machine))
    ini = ini.replace("LIB = ../../host/arm/libstmbl_drive.so", "LIB = %s/host/arm/libstmbl_drive.so" % ROOT)
    ini = re.sub(r"(?m)^SERVO_PERIOD = .*", "SERVO_PERIOD = %d" % round(servo * 1e6), ini)
    open(os.path.join(d, "mill.ini"), "w").write(ini)
    progs = []
    for name, (body, _, _) in PROGRAMS.items():
        p = os.path.join(d, name + ".ngc")
        open(p, "w").write(HEAD + body + TAIL)
        progs.append(p)
    return progs


def geom_dist(g, x, y):
    if g[0] == "circle":
        return np.abs(np.hypot(x - CX, y - CY) - g[1])
    if g[0] == "line":
        return np.abs(y - CY)
    a = g[1] / 2  # square about (CX, CY): distance to its outline
    dx, dy = np.abs(x - CX) - a, np.abs(y - CY) - a
    out = np.hypot(np.maximum(dx, 0), np.maximum(dy, 0))
    return np.where((dx > 0) | (dy > 0), out, np.minimum(-dx, -dy))


def servo_dist(c, t, w):
    """the tool's distance from the commanded polyline within w samples either side"""
    n = len(t)
    best = np.full(n, np.inf)
    for k in range(-w, w):
        i = np.arange(max(0, -k), min(n, n - k - 1))
        a, b = c[i + k], c[i + k + 1]
        d = b - a
        l2 = np.maximum((d * d).sum(1), 1e-18)
        u = np.clip(((t[i] - a) * d).sum(1) / l2, 0, 1)
        e = np.hypot(*(t[i] - a - u[:, None] * d).T)
        best[i] = np.minimum(best[i], e)
    return best


def analyse(path, res, g, servo):
    out = {"status": res.get("status", "not run"), "errors": res.get("errors", [])}
    try:  # halsampler may start with "overrun" (its FIFO filled before it read)
        rows = [l.split() for l in open(path)]
        a = np.array([list(map(float, r)) for r in rows if len(r) == len(COLS) + 1 and r[0].isdigit()], ndmin=2)
    except (OSError, ValueError):
        return out
    if a.size == 0:
        return out
    S = {c: a[:, i + 1] for i, c in enumerate(COLS)}  # column 0: halsampler's sample number
    cut = np.isin(S["mt"], (2, 3)) & (np.abs(S["tz"] - ZCUT) < 0.05) & (np.abs(S["cz"] - ZCUT) < 0.01)
    if cut.sum() < 10:
        return out
    n = a[:, 0][cut]
    out["gaps"] = int((np.diff(n) > 1).sum())  # lost samples while cutting (an overrun)
    pe = geom_dist(g, S["tx"][cut], S["ty"][cut]) * 1000
    c, t = np.c_[S["cx"], S["cy"]], np.c_[S["tx"], S["ty"]]
    se = servo_dist(c, t, int(round(0.025 / (servo * 1e-3))))[cut] * 1000
    out.update(path_max=pe.max(), path_p99=np.percentile(pe, 99), servo_max=se.max(), servo_rms=math.sqrt((se ** 2).mean()),
               ferror=max(np.abs(S["fe%d" % j]).max() for j in range(3)) * 1000, imax=max(np.abs(S["i%d" % j]).max() for j in range(3)),
               head=max(np.abs(S["hx"]).max(), np.abs(S["hy"]).max()), bridge=np.abs(S["yb"]).max(), time=len(S["mt"][cut]) * servo * 1e-3)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--servo", default="1,0.5")
    ap.add_argument("--machine", default="cn720")
    ap.add_argument("--out")
    ap.add_argument("--no-check", action="store_true")
    a = ap.parse_args()
    out = a.out or tempfile.mkdtemp(prefix="mill-test.")
    os.makedirs(out, exist_ok=True)
    md, fails = ["# hwtest/mill-sim results\n"], []
    for machine in a.machine.split(","):
        for servo in map(float, a.servo.split(",")):
            d = tempfile.mkdtemp(prefix="mill-sim.")
            progs = setup(d, servo, machine)
            env = dict(os.environ, MILL_PROGRAMS=" ".join(progs), MILL_OUT=d)
            cmd = ("export PATH=%s/pybin:$PATH; . %s/scripts/rip-environment >/dev/null && cd %s && "
                   "timeout 300 linuxcnc mill.ini > linuxcnc.log 2>&1") % (LINUXCNC, LINUXCNC, d)
            subprocess.run(["bash", "-c", cmd], env=env)
            title = "%s, servo %g ms (%d Hz)" % (machine, servo, round(1000 / servo))
            print("\n## " + title + "  (" + d + ")")
            md.append("\n**%s**\n\n| program | status | path max / p99 | servo max / rms | ferror | I max [A] | head | bridge |\n|---|---|---|---|---|---|---|---|\n" % title)
            for p in progs:
                name = os.path.splitext(os.path.basename(p))[0]
                try:
                    res = json.load(open(os.path.join(d, name + ".json")))
                except OSError:
                    res = {"status": "not run"}
                r = analyse(os.path.join(d, name + ".samples"), res, PROGRAMS[name][1], servo)
                if "path_max" in r:
                    line = "%-16s %-7s path %6.1f / %5.1f  servo %6.1f / %5.2f  ferror %6.1f  I %4.2f  head %5.1f  bridge %5.1f" % (
                        name, r["status"], r["path_max"], r["path_p99"], r["servo_max"], r["servo_rms"], r["ferror"], r["imax"], r["head"], r["bridge"])
                    md.append("| %s | %s | %.1f / %.1f | %.1f / %.2f | %.1f | %.2f | %.1f | %.1f |\n" % (
                        name, r["status"], r["path_max"], r["path_p99"], r["servo_max"], r["servo_rms"], r["ferror"], r["imax"], r["head"], r["bridge"]))
                else:
                    line = "%-16s %-7s %s" % (name, r["status"], "; ".join(r.get("errors", [])))
                    md.append("| %s | %s | | | | | | |\n" % (name, r["status"]))
                print(line, flush=True)
                if r["status"] != "done" or "path_max" not in r:
                    fails.append("%s %s: %s" % (title, name, r["status"]))
                for k, b in BOUNDS.items():
                    if r.get(k, 0) > b:
                        fails.append("%s %s: %s %.1f > %.1f" % (title, name, k, r[k], b))
    open(os.path.join(out, "results.md"), "w").write("".join(md))
    print("\nresults: %s/results.md" % out)
    if fails and not a.no_check:
        print("FAILED:\n  " + "\n  ".join(fails))
        return 1
    print("ok" if not fails else "(not checked)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
