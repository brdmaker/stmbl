#!/usr/bin/env python3
"""LinuxCNC runs five-axis programs on the simulated Mikron HSM 400U retrofit
(host/mill, machine/hsm400u*.mill, stmbl_mill) through xyzac-trt-kins, and
checks how well the tool tip follows in the work.

    hsm_test.py [--servo MS,MS..] [--machine NAME,NAME..] [--out DIR] [--no-check]

Programs (in the work's coordinates: LinuxCNC's world mode is the tool centre
point):
  rtcp      the tip on a point 30 / 20 mm off the axes while A 0 -> 30 and
            C 0 -> 90, then back to A 0 at C 180 (30 deg/s)
  wline     the tip 40 mm along X at 600 mm/min while A 0 -> 20, C 0 -> 60
  circle    A 20, C 45 held: a circle R 10 mm at 3 m/min in the work's XY
            (every joint moves)

Per program, while cutting (feed moves at Z -150 in the work):
  geom      the simulated tip (stmbl-mill.0.work-*) against the programmed
            point / line / circle: max and 99th percentile [um]
  servo     the tip against LinuxCNC's own commanded path (its joint
            commands through the same kinematics, 25 ms either side) [um]
  ferror    the largest linear joint following error [um] and rotary [arcsec]
  I         the largest motor current [A] (the drives allow 15)

Results: DIR/results.md. Unless --no-check, exit 1 when a program didn't
finish or a value is over its bound. Not as root; needs numpy, stmbl_mill
installed (make -C ../../host/mill/lcnc all install).
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
COLS = "j0 j1 j2 j3 j4 wx wy wz fe0 fe1 fe2 fe3 fe4 i0 i1 i2 i3 i4 mt".split()
TRT = dict(xr=200.0, yr=225.0, zr=-250.0, dy=0.0, dz=80.0, tool=100.0)  # as hsm.hal and the machine file's trt line
ZCUT = -150.0

HEAD = "G21 G90 G17 G64 P0.01\nG0 Z-120\nG0 A0 C0\n"
TAIL = "G0 Z-120\nG0 A0 C0\nM2\n"
PROGRAMS = {  # name: (G-code body, geometry, what it is)
    "rtcp": ("G0 X230 Y245\nG1 Z-150 F1000\nG1 A30 C90 F1800\nG1 A0 C180 F1800\n", ("point", (230.0, 245.0, ZCUT)),
             "tool centre point: the tip held while A 0 -> 30, C 0 -> 90 -> 180"),
    "wline": ("G0 X210 Y225\nG1 Z-150 F1000\nG1 X250 A20 C60 F600\n", ("line", (210.0, 225.0, ZCUT), (250.0, 225.0, ZCUT)),
              "five-axis line: X 40 mm at 600 mm/min while A 0 -> 20, C 0 -> 60"),
    "circle": ("G0 A20 C45\nG0 X240 Y225\nG1 Z-150 F1000\nG2 X240 Y225 I-10 J0 F3000\n", ("circle", (230.0, 225.0, ZCUT), 10.0),
               "A 20, C 45: circle R 10 mm at 3 m/min in the work"),
}
BOUNDS = {"geom_max": 40.0, "servo_max": 40.0, "ferror": 100.0, "gaps": 0}


def trt_fwd(j):
    """xyzac-trt-kins forward (LinuxCNC 2.9 trtfuncs.c), j: N x 5 -> N x 3"""
    a, c = np.radians(j[:, 3]), np.radians(j[:, 4])
    dy, dz = TRT["dy"], TRT["dz"] + TRT["tool"]
    X, Y, Z = j[:, 0] - TRT["xr"], j[:, 1] - dy - TRT["yr"], j[:, 2] - dz - TRT["zr"]
    return np.c_[np.cos(c) * X + np.sin(c) * np.cos(a) * Y + np.sin(c) * np.sin(a) * Z + np.sin(c) * dy + TRT["xr"],
                 -np.sin(c) * X + np.cos(c) * np.cos(a) * Y + np.cos(c) * np.sin(a) * Z + np.cos(c) * dy + TRT["yr"],
                 -np.sin(a) * Y + np.cos(a) * Z + dz + TRT["zr"]]


def setup(d, servo, machine):
    for f in ("hsm.ini", "hsm.hal", "tool.tbl", "hsm.var"):
        shutil.copy(os.path.join(HERE, f), d)
    shutil.copy(os.path.join(HERE, "..", "mill-sim", "mill_run.py"), d)
    ini = open(os.path.join(d, "hsm.ini")).read()
    ini = ini.replace("DISPLAY = axis", "DISPLAY = ./mill_run.py")
    ini = ini.replace("FILE = ../../host/mill/machine/hsm400u.mill", "FILE = %s/host/mill/machine/%s.mill" % (ROOT, machine))
    ini = ini.replace("LIB = ../../host/arm/libstmbl_drive.so", "LIB = %s/host/arm/libstmbl_drive.so" % ROOT)
    ini = re.sub(r"(?m)^SERVO_PERIOD = .*", "SERVO_PERIOD = %d" % round(servo * 1e6), ini)
    open(os.path.join(d, "hsm.ini"), "w").write(ini)
    progs = []
    for name, (body, _, _) in PROGRAMS.items():
        p = os.path.join(d, name + ".ngc")
        open(p, "w").write(HEAD + body + TAIL)
        progs.append(p)
    return progs


def geom_dist(g, p):
    if g[0] == "point":
        return np.linalg.norm(p - np.array(g[1]), axis=1)
    if g[0] == "line":
        a, b = np.array(g[1]), np.array(g[2])
        d = b - a
        u = np.clip(((p - a) @ d) / (d @ d), 0, 1)
        return np.linalg.norm(p - a - u[:, None] * d, axis=1)
    c, r = np.array(g[1]), g[2]  # a circle in the plane z = c[2]
    return np.hypot(np.hypot(p[:, 0] - c[0], p[:, 1] - c[1]) - r, p[:, 2] - c[2])


def servo_dist(c, t, w):
    """the tip's distance from the commanded polyline within w samples either side (3D)"""
    n = len(t)
    best = np.full(n, np.inf)
    for k in range(-w, w):
        i = np.arange(max(0, -k), min(n, n - k - 1))
        a, b = c[i + k], c[i + k + 1]
        d = b - a
        l2 = np.maximum((d * d).sum(1), 1e-18)
        u = np.clip(((t[i] - a) * d).sum(1) / l2, 0, 1)
        best[i] = np.minimum(best[i], np.linalg.norm(t[i] - a - u[:, None] * d, axis=1))
    return best


def analyse(path, res, g, servo):
    out = {"status": res.get("status", "not run"), "errors": res.get("errors", [])}
    try:
        rows = [l.split() for l in open(path)]
        a = np.array([list(map(float, r)) for r in rows if len(r) == len(COLS) + 1 and r[0].isdigit()], ndmin=2)
    except (OSError, ValueError):
        return out
    if a.size == 0:
        return out
    S = {c: a[:, i + 1] for i, c in enumerate(COLS)}
    cmd = trt_fwd(np.c_[S["j0"], S["j1"], S["j2"], S["j3"], S["j4"]])
    tip = np.c_[S["wx"], S["wy"], S["wz"]]
    cut = np.isin(S["mt"], (2, 3)) & (np.abs(cmd[:, 2] - ZCUT) < 0.01) & (np.abs(tip[:, 2] - ZCUT) < 0.1)
    if cut.sum() < 10:
        return out
    out["gaps"] = int((np.diff(a[:, 0][cut]) > 1).sum())
    ge = geom_dist(g, tip[cut]) * 1000
    se = servo_dist(cmd, tip, int(round(0.025 / (servo * 1e-3))))[cut] * 1000
    out.update(geom_max=ge.max(), geom_p99=np.percentile(ge, 99), servo_max=se.max(), servo_rms=math.sqrt((se ** 2).mean()),
               ferror=max(np.abs(S["fe%d" % j]).max() for j in range(3)) * 1000,
               ferror_rot=max(np.abs(S["fe%d" % j]).max() for j in (3, 4)) * 3600,
               imax=max(np.abs(S["i%d" % j]).max() for j in range(5)), time=cut.sum() * servo * 1e-3)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--servo", default="1,0.5")
    ap.add_argument("--machine", default="hsm400u")
    ap.add_argument("--out")
    ap.add_argument("--no-check", action="store_true")
    a = ap.parse_args()
    out = a.out or tempfile.mkdtemp(prefix="hsm-test.")
    os.makedirs(out, exist_ok=True)
    md, fails = ["# hwtest/hsm400u-sim results\n"], []
    for machine in a.machine.split(","):
        for servo in map(float, a.servo.split(",")):
            d = tempfile.mkdtemp(prefix="hsm400u-sim.")
            progs = setup(d, servo, machine)
            env = dict(os.environ, MILL_PROGRAMS=" ".join(progs), MILL_OUT=d, MILL_RECOVER="G90 G0 Z-120")
            cmd = ("export PATH=%s/pybin:$PATH; . %s/scripts/rip-environment >/dev/null && cd %s && "
                   "timeout 300 linuxcnc hsm.ini > linuxcnc.log 2>&1") % (LINUXCNC, LINUXCNC, d)
            subprocess.run(["bash", "-c", cmd], env=env)
            title = "%s, servo %g ms (%d Hz)" % (machine, servo, round(1000 / servo))
            print("\n## " + title + "  (" + d + ")")
            md.append("\n**%s**\n\n| program | status | geom max / p99 [um] | servo max / rms [um] | ferror [um] | rotary [arcsec] | I max [A] |\n"
                      "|---|---|---|---|---|---|---|\n" % title)
            for p in progs:
                name = os.path.splitext(os.path.basename(p))[0]
                try:
                    res = json.load(open(os.path.join(d, name + ".json")))
                except OSError:
                    res = {"status": "not run"}
                r = analyse(os.path.join(d, name + ".samples"), res, PROGRAMS[name][1], servo)
                if "geom_max" in r:
                    line = "%-8s %-7s geom %6.1f / %5.1f  servo %6.1f / %5.2f  ferror %6.1f  rot %6.1f\"  I %5.2f" % (
                        name, r["status"], r["geom_max"], r["geom_p99"], r["servo_max"], r["servo_rms"], r["ferror"], r["ferror_rot"], r["imax"])
                    md.append("| %s | %s | %.1f / %.1f | %.1f / %.2f | %.1f | %.1f | %.2f |\n" % (
                        name, r["status"], r["geom_max"], r["geom_p99"], r["servo_max"], r["servo_rms"], r["ferror"], r["ferror_rot"], r["imax"]))
                else:
                    line = "%-8s %-7s %s" % (name, r["status"], "; ".join(r.get("errors", [])))
                    md.append("| %s | %s | | | | | |\n" % (name, r["status"]))
                print(line, flush=True)
                if r["status"] != "done" or "geom_max" not in r:
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
