#!/usr/bin/env python3
"""The test stand's demo drawing (demo.ngc): a border, circles, a diamond, a spiral and STMBL.
Every axis moves in both directions, at several feeds, with arcs, so a run shows the drives,
the switches (home first) and the planner working together. The pen touches the paper at Z -40."""
import math
import sys

DOWN, UP = -40.0, -34.0
FONT = {  # strokes in a box 0.6 wide, 1 high
    "S": [[(0.6, 0.85), (0.45, 1), (0.15, 1), (0, 0.85), (0, 0.62), (0.15, 0.5), (0.45, 0.5), (0.6, 0.38), (0.6, 0.15), (0.45, 0), (0.15, 0), (0, 0.15)]],
    "T": [[(0, 1), (0.6, 1)], [(0.3, 1), (0.3, 0)]],
    "M": [[(0, 0), (0, 1), (0.3, 0.5), (0.6, 1), (0.6, 0)]],
    "B": [[(0, 0), (0, 1), (0.4, 1), (0.55, 0.88), (0.55, 0.62), (0.4, 0.5), (0, 0.5)], [(0.4, 0.5), (0.6, 0.38), (0.6, 0.12), (0.45, 0), (0, 0)]],
    "L": [[(0, 1), (0, 0), (0.55, 0)]],
}
out = []
g = out.append
f = lambda v: ("%.3f" % v).rstrip("0").rstrip(".")  # noqa: E731


def stroke(pts, feed):
    g("G0 X%s Y%s" % (f(pts[0][0]), f(pts[0][1])))
    g("G1 Z%s F600" % f(DOWN))
    for x, y in pts[1:]:
        g("G1 X%s Y%s F%d" % (f(x), f(y), feed))
    g("G0 Z%s" % f(UP))


def circle(cx, cy, r, feed, ccw=False):
    g("G0 X%s Y%s" % (f(cx + r), f(cy)))
    g("G1 Z%s F600" % f(DOWN))
    g("%s X%s Y%s I%s J0 F%d" % ("G3" if ccw else "G2", f(cx + r), f(cy), f(-r), feed))
    g("G0 Z%s" % f(UP))


g("(stmbl test stand: the demo drawing; paper 130 x 140 on the table, the pen touching it at Z %s)" % f(DOWN))
g("G21 G90 G17 G40 G49 G94 G64 P0.05")
g("G0 Z%s" % f(UP))
g("(a border, the whole travel but a margin)")
stroke([(-52, -62), (52, -62), (52, 62), (-52, 62), (-52, -62)], 1500)
g("(three circles, both ways round, three feeds)")
for r, feed, ccw in ((12, 600, False), (24, 1500, True), (36, 3000, False)):
    circle(0, 10, r, feed, ccw)
g("(a diamond in them)")
stroke([(0, 10 - 33), (33, 10), (0, 10 + 33), (-33, 10), (0, 10 - 33)], 2400)
g("(a spiral out from the middle: a smooth path of short lines)")
pts = []
for i in range(0, 721, 6):
    a = math.radians(i)
    r = 1 + 9 * i / 720.0
    pts.append((r * math.cos(a), 10 + r * math.sin(a)))
stroke(pts, 1200)
g("(STMBL under them)")
x0, h = -33.0, 12.0
for k, ch in enumerate("STMBL"):
    for s in FONT[ch]:
        stroke([(x0 + k * 13.5 + px * h, -55 + py * h) for px, py in s], 1200)
g("G0 Z0")
g("G0 X0 Y0")
g("M2")
text = "\n".join(out) + "\n"
open(sys.argv[1] if len(sys.argv) > 1 else "demo.ngc", "w").write(text)
