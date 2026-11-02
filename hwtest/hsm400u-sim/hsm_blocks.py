#!/usr/bin/env python3
"""Block rate and contour accuracy of LinuxCNC + stmbl on the simulated
Mikron HSM 400U, for comparison with the original iTNC 530 (block processing
time 3.6 ms standard, 0.5 ms with software option 2).

    hsm_blocks.py [--servo MS,MS..] [--seg MM,MM..] [--tol MM,MM..] [--feed MM_MIN] [--out DIR]

Each program is a circle R 20 mm in XY cut as straight lines of one length
(as CAM output is) at the owner's 8000 mm/min, under G64 P<tol>. Per run:
  lines/s   the steady block rate: the median tip speed over the segment length
  feed      the median tip speed [mm/min] (the programmed feed if the control
            keeps up with the blocks)
  path      the tip's largest distance from the programmed polyline [um]
            (the planner's blending, G64 P, plus the servo error)
  servo     the tip against LinuxCNC's own commanded path [um] (the drives)
  time      cutting time for the circle [s]

Writes DIR/blocks.md. Not as root; needs numpy and stmbl_mill installed.
"""
import argparse
import json
import math
import os
import subprocess
import tempfile

import numpy as np

import hsm_test as H

R, CX, CY = 20.0, 230.0, 160.0  # the circle 30 mm off the C axis (X 200, Y 160)


def program(seg, tol, feed, q=None):
    n = max(8, int(round(2 * math.pi * R / seg)))
    pts = [(CX + R * math.cos(2 * math.pi * k / n), CY + R * math.sin(2 * math.pi * k / n)) for k in range(n + 1)]
    body = ["G21 G90 G17 G64 P%g" % tol + ("" if q is None else " Q%g" % q), "G0 Z-120", "G0 B0 C0", "G0 X%.4f Y%.4f" % pts[0], "G1 Z%g F1000" % H.ZCUT, "F%g" % feed]
    body += ["G1 X%.4f Y%.4f" % p for p in pts[1:]]
    body += ["G0 Z-120", "M2", ""]
    return "\n".join(body), np.array(pts), n


def poly_dist(tip, pts):
    """each tip point's distance from the closed polyline pts (XY), in the cut plane"""
    best = np.full(len(tip), np.inf)
    a, b = pts[:-1], pts[1:]
    d = b - a
    l2 = (d * d).sum(1)
    for i in range(0, len(tip), 2000):  # chunks: tip x segments
        t = tip[i:i + 2000, None, :2]
        u = np.clip(((t - a) * d).sum(2) / l2, 0, 1)
        e = np.linalg.norm(t - a - u[..., None] * d, axis=2).min(1)
        best[i:i + 2000] = np.hypot(e, tip[i:i + 2000, 2] - H.ZCUT)
    return best


def analyse(path, pts, servo):
    rows = [l.split() for l in open(path)]
    a = np.array([list(map(float, r)) for r in rows if len(r) == len(H.COLS) + 1 and r[0].isdigit()], ndmin=2)
    S = {c: a[:, i + 1] for i, c in enumerate(H.COLS)}
    cmd = H.trt_fwd(np.c_[S["j0"], S["j1"], S["j2"], S["j3"], S["j4"]])
    tip = np.c_[S["wx"], S["wy"], S["wz"]]
    cut = (np.abs(cmd[:, 2] - H.ZCUT) < 0.01) & (np.abs(tip[:, 2] - H.ZCUT) < 0.1)
    dt = servo * 1e-3
    v = np.r_[0, np.linalg.norm(np.diff(tip[:, :2], axis=0), axis=1) / dt]
    moving = cut & (v > 1.0)
    se = H.servo_dist(cmd, tip, int(round(0.025 / dt)))[moving] * 1000
    return dict(feed=float(np.median(v[moving])) * 60, path=float(poly_dist(tip[moving], pts).max()) * 1000,
                servo=float(se.max()), time=float(moving.sum() * dt))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--servo", default="1,0.5")
    ap.add_argument("--seg", default="1,0.5,0.2,0.1,0.067,0.05,0.02")
    ap.add_argument("--tol", default="0.01")
    ap.add_argument("--q", type=float, help="G64 Q (LinuxCNC merges collinear-within-Q lines; default Q = P, 0 = off)")
    ap.add_argument("--feed", type=float, default=8000)
    ap.add_argument("--machine", default="hsm400u")
    ap.add_argument("--out")
    a = ap.parse_args()
    out = a.out or tempfile.mkdtemp(prefix="hsm-blocks.")
    os.makedirs(out, exist_ok=True)
    segs = [float(s) for s in a.seg.split(",")]
    md = ["# HSM 400U simulation: block rate and contour accuracy\n\n"
          "Circle R %g mm as straight lines at F%g, LinuxCNC 2.9 + stmbl (simulated), machine `%s`.\n" % (R, a.feed, a.machine)]
    for servo in map(float, a.servo.split(",")):
        for tol in map(float, a.tol.split(",")):
            d = tempfile.mkdtemp(prefix="hsm-blocks-run.")
            H.setup(d, servo, a.machine)
            progs, polys = [], {}
            for seg in segs:
                name = "seg%g" % seg
                text, pts, n = program(seg, tol, a.feed, a.q)
                p = os.path.join(d, name + ".ngc")
                open(p, "w").write(text)
                progs.append(p)
                polys[name] = (seg, pts, n)
            env = dict(os.environ, MILL_PROGRAMS=" ".join(progs), MILL_OUT=d, MILL_RECOVER="G90 G0 Z-120")
            cmd = ("export PATH=%s/pybin:$PATH; . %s/scripts/rip-environment >/dev/null && cd %s && "
                   "timeout 600 linuxcnc hsm.ini > linuxcnc.log 2>&1") % (H.LINUXCNC, H.LINUXCNC, d)
            subprocess.run(["bash", "-c", cmd], env=env)
            title = "servo %g ms (%d Hz), G64 P%g%s" % (servo, round(1000 / servo), tol, "" if a.q is None else " Q%g" % a.q)
            print("\n## %s  (%s)" % (title, d))
            md.append("\n**%s**\n\n| segment [mm] | lines | status | feed [mm/min] | lines/s | path max [um] | servo max [um] | time [s] |\n"
                      "|---|---|---|---|---|---|---|---|\n" % title)
            for name, (seg, pts, n) in polys.items():
                try:
                    st = json.load(open(os.path.join(d, name + ".json"))).get("status", "?")
                    r = analyse(os.path.join(d, name + ".samples"), pts, servo)
                except (OSError, ValueError, KeyError, IndexError) as e:
                    print("%-8s failed: %s" % (name, e))
                    md.append("| %g | %d | failed | | | | | |\n" % (seg, n))
                    continue
                lps = r["feed"] / 60 / seg
                print("seg %-6g %5d lines  %-5s feed %6.0f  %6.0f lines/s  path %6.1f  servo %5.1f  %.2f s" % (
                    seg, n, st, r["feed"], lps, r["path"], r["servo"], r["time"]), flush=True)
                md.append("| %g | %d | %s | %.0f | %.0f | %.1f | %.1f | %.2f |\n" % (seg, n, st, r["feed"], lps, r["path"], r["servo"], r["time"]))
    open(os.path.join(out, "blocks.md"), "w").write("".join(md))
    print("\nresults: %s/blocks.md" % out)


if __name__ == "__main__":
    main()
