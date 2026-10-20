#!/usr/bin/env python3
"""The benchmark toolpath for the portal: CAM-like G-code at a given feed,
and its cutting geometry (for the path error).

    gen_path.py FEED_MM_MIN OUT.ngc [GEOM.json]

Features (machine X, Y in mm, the work area 0..1000):
  square spiral   long lines and 90 degree corners: acceleration, blending
  gear outline    30 teeth from 1 mm segments, sharp tooth corners
  wavy hatch      lines with a 2 mm sine wiggle from 0.4 mm segments: the
                  planner handles about one segment per servo cycle, so this
                  caps the feed at 0.4 mm/ms = 24 m/min
  circles         G2, R 100 / 20 / 5: centripetal acceleration v^2/R
  star            12 points, acute corners
Rapids (G0) between features at the machine's maximum speed. G64 P0.02:
LinuxCNC may blend corners within 20 um of the program.
"""
import json
import math
import sys


def square_spiral(x0, y0, size, step, loops):
    pts = []
    for i in range(loops):
        a, b = x0 + i * step, y0 + i * step
        s = size - 2 * i * step
        pts += [(a, b), (a + s, b), (a + s, b + s), (a, b + s), (a, b + step if i + 1 < loops else b)]
    return pts


def gear(cx, cy, r, teeth, depth, seg):
    pts = []
    n = teeth * 4
    for k in range(n + 1):  # tooth: flat top, flank down, root, flank up
        a0 = 2 * math.pi * k / n
        rr = r if k % 4 in (0, 1) else r - depth
        a1 = 2 * math.pi * (k + 1) / n
        r1 = r if (k + 1) % 4 in (0, 1) else r - depth
        steps = max(1, int(abs(rr * (a1 - a0)) / seg)) if k < n else 1
        for s in range(steps):
            u = s / steps
            a = a0 + (a1 - a0) * u
            rad = rr if rr == r1 else rr
            pts.append((cx + rad * math.cos(a), cy + rad * math.sin(a)))
        if k < n and rr != r1:  # the flank: radial, from rr to r1
            pts.append((cx + r1 * math.cos(a1), cy + r1 * math.sin(a1)))
    return pts


def wavy_hatch(x0, x1, y0, rows, pitch, amp, wave, seg):
    paths = []
    for i in range(rows):
        y = y0 + i * pitch
        xs = [x0 + k * seg for k in range(int((x1 - x0) / seg) + 1)]
        if i % 2:
            xs = xs[::-1]
        paths.append([(x, y + amp * math.sin(2 * math.pi * (x - x0) / wave)) for x in xs])
    return paths


def star(cx, cy, r_out, r_in, n):
    pts = []
    for k in range(2 * n + 1):
        r = r_out if k % 2 == 0 else r_in
        a = math.pi / 2 + math.pi * k / n
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def arc_points(cx, cy, r, a0, turns, step=0.05):
    n = max(8, int(2 * math.pi * r * turns / step))
    return [(cx + r * math.cos(a0 - 2 * math.pi * turns * k / n), cy + r * math.sin(a0 - 2 * math.pi * turns * k / n))
            for k in range(n + 1)]


def main():
    feed = float(sys.argv[1])
    out = sys.argv[2]
    geom_out = sys.argv[3] if len(sys.argv) > 3 else None
    g = ["(portal benchmark, F%.0f)" % feed, "G21 G90 G17 G94", "G64 P0.02", "F%.0f" % feed]
    geom = []  # cutting polylines

    def poly(pts):
        g.append("G0 X%.4f Y%.4f" % pts[0])
        for p in pts[1:]:
            g.append("G1 X%.4f Y%.4f" % p)
        geom.append(pts)

    def circle(cx, cy, r, turns):
        a0 = math.pi  # start at the left of the centre, clockwise
        g.append("G0 X%.4f Y%.4f" % (cx - r, cy))
        g.append("G2 X%.4f Y%.4f I%.4f J0 P%d" % (cx - r, cy, r, turns))
        geom.append(arc_points(cx, cy, r, a0, turns))

    poly(square_spiral(80, 80, 300, 40, 3))
    poly(gear(700, 250, 160, 30, 12, 1.0))
    for p in wavy_hatch(80, 400, 600, 4, 40, 2.0, 8.0, 0.4):
        poly(p)
    circle(700, 700, 100, 1)
    circle(700, 700, 20, 2)
    circle(700, 700, 5, 3)
    poly(star(700, 700, 200, 130, 12))
    g += ["G0 X500 Y500", "M2"]
    open(out, "w").write("\n".join(g) + "\n")
    if geom_out:
        json.dump(geom, open(geom_out, "w"))
    cut = sum(math.dist(a, b) for pts in geom for a, b in zip(pts, pts[1:]))
    segs = sum(1 for l in g if l.startswith("G1"))
    print("%s: %d lines, %d G1 segments, %.2f m of cutting" % (out, len(g), segs, cut / 1000))


if __name__ == "__main__":
    main()
