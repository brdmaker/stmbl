#!/usr/bin/env python3
"""step2cell: a STEP file (e.g. exported from Fusion 360) to collider boxes
for the arm's collision model (shared/robot/geom.h obstacle lines).

Every solid in the file becomes one oriented box: the smallest of a few
candidate orientations (the CAD axes, the solid's principal axes, frames
from its largest flat faces - exact for machined parts). The boxes are
placed in the robot's base frame (--base: where the robot base is in the
CAD file) and written as

    obstacle NAME box cx cy cz hx hy hz a b c     (m; deg, R = Rz(c) Ry(b) Rx(a))

ready for a cell file (hwtest/arcmate-sim/cell.txt; stmbl_arm and armguard
read them). A box is a simplification: the report gives each solid's fill
(solid volume / box volume); a low fill (an L-shaped bracket, a ring)
means the box is much bigger than the part - split the body in Fusion
(e.g. Modify > Split Body) so each piece is box-like.

usage: step2cell.py FILE.step [--base X Y Z A B C] [--margin MM]
                    [--skip REGEX] [--only REGEX] [--min-size MM] [-o OUT]
  --base     the robot base frame in the CAD file: origin [mm] and A B C
             [deg] (default: the CAD origin and axes)
  --margin   grow every box by MM on each side (default 0)
  --skip / --only   select solids by name (regular expressions)
  --min-size drop solids whose largest box side is under MM (screws...)

Needs the gmsh Python module (pip install gmsh numpy): its OpenCascade
kernel reads STEP; Fusion 360 writes mm, converted to m here.
"""

import argparse
import math
import re
import sys

import numpy as np


def rpy_from_R(R):
    """genserkins' A B C [rad] of R = Rz(c) Ry(b) Rx(a)"""
    cb = math.hypot(R[0, 0], R[1, 0])
    b = math.atan2(-R[2, 0], cb)
    if cb > 1e-9:
        return math.atan2(R[2, 1], R[2, 2]), b, math.atan2(R[1, 0], R[0, 0])
    return 0.0, b, math.atan2(-R[0, 1], R[1, 1])


def R_from_rpy(a, b, c):
    ca, sa, cb, sb, cc, sc = math.cos(a), math.sin(a), math.cos(b), math.sin(b), math.cos(c), math.sin(c)
    return np.array([[cc * cb, cc * sb * sa - sc * ca, cc * sb * ca + sc * sa],
                     [sc * cb, sc * sb * sa + cc * ca, sc * sb * ca - cc * sa],
                     [-sb, cb * sa, cb * ca]])


def box_in(R, pts):
    """box of the points in frame R (columns = axes): centre, half sizes, volume"""
    loc = pts @ R
    lo, hi = loc.min(axis=0), loc.max(axis=0)
    half = (hi - lo) / 2.0
    return R @ ((lo + hi) / 2.0), half, float(np.prod(2.0 * half))


def right_handed(R):
    if np.linalg.det(R) < 0:
        R = R.copy()
        R[:, 2] = -R[:, 2]
    return R


def frame_from_normals(n1, n2):
    a = n1 / np.linalg.norm(n1)
    b = n2 - a * np.dot(a, n2)
    if np.linalg.norm(b) < 1e-6:
        return None
    b /= np.linalg.norm(b)
    return np.column_stack([a, b, np.cross(a, b)])


def fit_box(pts, normals):
    """smallest box among the candidate orientations"""
    cands = [np.eye(3)]
    c = pts - pts.mean(axis=0)
    _, _, vt = np.linalg.svd(c, full_matrices=False)
    cands.append(right_handed(vt.T))
    for i in range(len(normals)):
        for j in range(i + 1, len(normals)):
            F = frame_from_normals(normals[i], normals[j])
            if F is not None:
                cands.append(F)
        F = frame_from_normals(normals[i], np.array([0.0, 0.0, 1.0]))  # a face and the up axis
        if F is not None:
            cands.append(F)
    best = None
    for R in cands:
        R = right_handed(R)
        ctr, half, vol = box_in(R, pts)
        if best is None or vol < best[3] - 1e-12:
            best = (R, ctr, half, vol)
    # refine: small turns about each axis (a solid with no flat faces)
    R, ctr, half, vol = best
    for _ in range(3):
        for ax in range(3):
            for ang in np.radians([-10, -5, -2, -1, 1, 2, 5, 10]):
                k = np.zeros(3)
                k[ax] = ang
                K = np.array([[0, -k[2], k[1]], [k[2], 0, -k[0]], [-k[1], k[0], 0]])
                Rr = R @ (np.eye(3) + math.sin(ang) / ang * K + (1 - math.cos(ang)) / ang ** 2 * K @ K)
                c2, h2, v2 = box_in(Rr, pts)
                if v2 < vol - 1e-12:
                    R, ctr, half, vol = Rr, c2, h2, v2
    return R, ctr, half, vol


def read_step(path):
    """[(name, surface points [mm], flat face normals sorted by area, volume [mm^3])]"""
    import gmsh
    import os
    so = os.dup(1)  # OpenCascade's STEP reader talks on stdout
    os.dup2(os.open(os.devnull, os.O_WRONLY), 1)
    gmsh.initialize(["-v", "0"])
    gmsh.option.setNumber("General.Terminal", 0)
    gmsh.option.setString("Geometry.OCCTargetUnit", "MM")
    gmsh.option.setNumber("Geometry.OCCImportLabels", 1)
    gmsh.model.add("cell")
    try:
        gmsh.model.occ.importShapes(path)
    except Exception as e:  # not a STEP file, unreadable
        gmsh.finalize()
        sys.stdout.flush()
        os.dup2(so, 1)
        raise SystemExit("step2cell: cannot read %s: %s" % (path, e))
    gmsh.model.occ.synchronize()
    vols = gmsh.model.getEntities(3)
    # a coarse surface mesh is enough: the box only needs the extremes
    xmin, ymin, zmin, xmax, ymax, zmax = gmsh.model.getBoundingBox(-1, -1)
    size = max(xmax - xmin, ymax - ymin, zmax - zmin)
    gmsh.option.setNumber("Mesh.MeshSizeMax", size / 20.0 if size > 0 else 1.0)
    gmsh.option.setNumber("Mesh.MeshSizeFromCurvature", 12)
    gmsh.model.mesh.generate(2)
    out = []
    for dim, tag in vols:
        name = gmsh.model.getEntityName(dim, tag) or ""
        name = name.split("/")[-1] if name else "solid%d" % tag
        pts, normals = [], []
        for d2, s in gmsh.model.getBoundary([(dim, tag)], oriented=False, recursive=False):
            _, coord, _ = gmsh.model.mesh.getNodes(2, abs(s), includeBoundary=True)
            pts.append(np.array(coord).reshape(-1, 3))
            if gmsh.model.getType(2, abs(s)) == "Plane":
                lo, hi = gmsh.model.getParametrizationBounds(2, abs(s))
                n = gmsh.model.getNormal(abs(s), [(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2])
                area = gmsh.model.occ.getMass(2, abs(s))
                normals.append((area, np.array(n[:3])))
        normals.sort(key=lambda x: -x[0])
        P = np.vstack(pts) if pts else np.zeros((0, 3))
        out.append((name, P, [n for _, n in normals[:4]], gmsh.model.occ.getMass(dim, tag)))
    gmsh.finalize()
    sys.stdout.flush()
    os.dup2(so, 1)
    return out


def clean(name):
    s = re.sub(r"[^A-Za-z0-9_]+", "_", name).strip("_") or "solid"
    return s[:15]


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("step")
    ap.add_argument("--base", type=float, nargs=6, default=[0, 0, 0, 0, 0, 0],
                    metavar=("X", "Y", "Z", "A", "B", "C"))
    ap.add_argument("--margin", type=float, default=0.0)
    ap.add_argument("--skip")
    ap.add_argument("--only")
    ap.add_argument("--min-size", type=float, default=0.0)
    ap.add_argument("-o", "--out")
    args = ap.parse_args(argv)

    bx, by, bz, ba, bb, bc = args.base
    Rb = R_from_rpy(math.radians(ba), math.radians(bb), math.radians(bc))
    tb = np.array([bx, by, bz])
    lines = ["# GENERATED by tools/step2cell.py from %s" % args.step,
             "# base frame in the CAD file: %s mm, %s deg; margin %g mm" % (
                 " ".join("%g" % v for v in args.base[:3]), " ".join("%g" % v for v in args.base[3:]),
                 args.margin),
             "# solid: fill = solid volume / box volume (low: split the body in CAD)"]
    report, used = [], set()
    for name, P, normals, vol in read_step(args.step):
        if args.only and not re.search(args.only, name):
            continue
        if args.skip and re.search(args.skip, name):
            continue
        if len(P) < 4:
            report.append("%s: no geometry, skipped" % name)
            continue
        P = (P - tb) @ Rb          # into the robot base frame (mm)
        normals = [Rb.T @ n for n in normals]
        R, ctr, half, bvol = fit_box(P, normals)
        if 2 * half.max() < args.min_size:
            report.append("%s: %.0f mm, under --min-size, skipped" % (name, 2 * half.max()))
            continue
        half = half + args.margin
        fill = vol / bvol if bvol > 0 else 0.0
        a, b, c = (math.degrees(v) for v in rpy_from_R(R))
        nm = clean(name)
        k = 2
        while nm in used:
            nm = clean(name)[:12] + "_%d" % k
            k += 1
        used.add(nm)
        lines.append("obstacle %s box %.4f %.4f %.4f %.4f %.4f %.4f %.3f %.3f %.3f   # fill %.2f" % (
            nm, ctr[0] / 1e3, ctr[1] / 1e3, ctr[2] / 1e3, half[0] / 1e3, half[1] / 1e3, half[2] / 1e3, a, b, c,
            fill))
        report.append("%s -> %s: %.0f x %.0f x %.0f mm, fill %.2f%s" % (
            name, nm, 2 * half[0], 2 * half[1], 2 * half[2], fill,
            "  (low: consider splitting it)" if fill < 0.5 else ""))
    text = "\n".join(lines) + "\n"
    if args.out:
        open(args.out, "w").write(text)
    else:
        sys.stdout.write(text)
    for r in report:
        print("step2cell: " + r, file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
