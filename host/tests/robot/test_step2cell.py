#!/usr/bin/env python3
"""Tests for tools/step2cell.py (TAP): a STEP file made here with gmsh's
OpenCascade kernel (a box turned 30 deg about z and 10 about x, a cylinder
along y, an L-shaped bracket, a small screw), imported with a robot base
frame offset and turned 90 deg: centres, sizes, orientation, fill, the
--min-size and --margin options. Skips (TAP SKIP) without gmsh."""
import math
import os
import subprocess
import sys
import tempfile

HOST = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TOOL = os.path.join(HOST, "..", "tools", "step2cell.py")
n, fails = 0, 0


def ok(cond, name, detail=""):
    global n, fails
    n += 1
    print("%s %d - step2cell: %s" % ("ok" if cond else "not ok", n, name))
    if not cond:
        fails += 1
        if detail:
            print("#   %s" % detail)


try:
    import gmsh
    import numpy as np
except ImportError:
    print("1..0 # SKIP gmsh / numpy not installed (pip install gmsh numpy)")
    sys.exit(0)

sys.path.insert(0, os.path.dirname(TOOL))
import step2cell as sc  # noqa: E402

d = tempfile.mkdtemp()
step = os.path.join(d, "cell.step")
gmsh.initialize(["-v", "0"])
gmsh.option.setNumber("General.Terminal", 0)
gmsh.model.add("t")
occ = gmsh.model.occ
b = occ.addBox(-200, -100, -50, 400, 200, 100)          # 400 x 200 x 100 mm
occ.rotate([(3, b)], 0, 0, 0, 1, 0, 0, math.radians(10))
occ.rotate([(3, b)], 0, 0, 0, 0, 0, 1, math.radians(30))
occ.translate([(3, b)], 1000, 200, 300)
cy = occ.addCylinder(500, -550, 500, 0, 300, 0, 50)       # r 50, 300 along y, centre y -400
l1 = occ.addBox(0, 0, 0, 300, 40, 40)
l2 = occ.addBox(0, 0, 0, 40, 40, 300)
lb, _ = occ.fuse([(3, l1)], [(3, l2)])
occ.translate(lb, -300, 600, 0)
sc_ = occ.addCylinder(800, 800, 0, 0, 0, 12, 3)           # a screw: 6 x 12 mm
occ.synchronize()
so = os.dup(1)  # OpenCascade's STEP writer talks on stdout
os.dup2(os.open(os.devnull, os.O_WRONLY), 1)
gmsh.write(step)
os.dup2(so, 1)
gmsh.finalize()

base = [100, 0, 0, 0, 0, 90]  # robot base at x = 100 mm, turned 90 deg about z


def run(*extra):
    r = subprocess.run([sys.executable, TOOL, step, "--base"] + [str(v) for v in base] + list(extra),
                       capture_output=True, text=True)
    boxes = []
    for line in r.stdout.splitlines():
        w = line.split()
        if w and w[0] == "obstacle":
            boxes.append((w[1], [float(x) for x in w[3:12]], float(line.split("fill")[1])))
    return r, boxes


r, boxes = run()
ok(r.returncode == 0 and len(boxes) == 4, "4 solids -> 4 boxes", r.stderr[-300:])


def to_robot(p):  # CAD point [mm] -> robot base frame [m]
    Rb = sc.R_from_rpy(*[math.radians(v) for v in base[3:]])
    return (Rb.T @ (np.array(p) - np.array(base[:3]))) / 1000.0


def find(center):
    c = to_robot(center)
    return min(boxes, key=lambda bx: np.linalg.norm(np.array(bx[1][:3]) - c))


# the turned box: centre, sizes and its axes (any order / sign)
bx = find([1000, 200, 300])
ok(np.allclose(bx[1][:3], to_robot([1000, 200, 300]), atol=1e-3), "turned box: centre", bx)
ok(np.allclose(sorted(bx[1][3:6]), [0.05, 0.1, 0.2], atol=1e-3), "turned box: half sizes 200/100/50 mm", bx)
R = sc.R_from_rpy(*[math.radians(v) for v in bx[1][6:9]])
Rcad = sc.R_from_rpy(0, 0, math.radians(30)) @ sc.R_from_rpy(math.radians(10), 0, 0)
Rrob = sc.R_from_rpy(*[math.radians(v) for v in base[3:]]).T @ Rcad
long_axis = R[:, int(np.argmax(bx[1][3:6]))]
ok(abs(abs(np.dot(long_axis, Rrob[:, 0])) - 1) < 1e-3, "turned box: long side along the solid's", long_axis)
ok(bx[2] > 0.98, "turned box: fill ~1 (%.2f)" % bx[2])

cyl = find([500, -400, 500])
ok(np.allclose(sorted(cyl[1][3:6]), [0.05, 0.05, 0.15], atol=1.5e-3), "cylinder: box 100 x 100 x 300 mm", cyl)
ok(0.7 < cyl[2] < 0.82, "cylinder: fill ~ pi/4 (%.2f)" % cyl[2])

lb_ = find([-150, 620, 150])
ok(lb_[2] < 0.35, "L bracket: low fill flagged (%.2f)" % lb_[2])
ok("low: consider splitting" in r.stderr, "  ...reported for splitting")

# --min-size drops the screw; --margin grows the boxes
r2, boxes2 = run("--min-size", "20", "--margin", "10")
ok(len(boxes2) == 3, "--min-size 20 drops the 12 mm screw", [b[0] for b in boxes2])
b2 = min(boxes2, key=lambda x: np.linalg.norm(np.array(x[1][:3]) - np.array(bx[1][:3])))
ok(np.allclose(sorted(b2[1][3:6]), [0.06, 0.11, 0.21], atol=1e-3), "--margin 10 grows each side by 10 mm", b2)

# the lines load into the geometry (C parser): via arm_sim's clearance
cell = os.path.join(d, "cell.txt")
open(cell, "w").write(r.stdout)
script = os.path.join(d, "s.txt")
open(script, "w").write("".join(l + "\n" for l in r.stdout.splitlines() if l.startswith("obstacle")) +
                        "print clear\n")
arm = os.path.join(HOST, "arm", "arm_sim")
if os.access(arm, os.X_OK):
    o = subprocess.run([arm, os.path.join(HOST, "arm", "arcmate", "arcmate.arm"), script],
                       capture_output=True, text=True).stdout
    ok("ERROR" not in o and "arm.clear =" in o, "the box lines load into arm_sim's geometry", o[-200:])
print("1..%d" % n)
print("# %d/%d passed" % (n - fails, n))
sys.exit(1 if fails else 0)
