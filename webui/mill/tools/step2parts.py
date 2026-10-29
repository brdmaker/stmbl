#!/usr/bin/env python3
"""A machine's STEP assembly -> STL parts for millweb's 3D view (machine.json's model.parts), grouped by what
they ride on. OpenCASCADE's tessellator (the OCP wheel: pip install cadquery-ocp) and the assembly's names.

  python3 step2parts.py machine.stp rules.json outdir/

rules.json:
  frame       {"origin": [x, y, z], "x": [..], "y": [..], "z": [..]}: the view's origin and axes in the
              STEP file's coordinates (a lathe: the spindle's axis on x through the chuck's face, y = -X)
  deflection  the tessellation's chord error [mm] (default 0.8: a view, not a print)
  skip        substrings of a part's path (lower case): not drawn (fasteners); "keep": paths it spares
  skip_leaf   substrings ("contains") or exact names ("exact") of a part's own name: not drawn (what is
              inside covers and housings)
  groups      [{"name": "xz", "match": [substrings]}, ...]: the first that matches; else "default"
  split       [{"match": substring, "group": name, "inside": [axis, lo, hi]}]: a part that matches goes to
              the group when its box lies within lo..hi on that STEP axis (0 x, 1 y, 2 z), else the default
Writes <prefix>_<group>.stl (binary, in the view's frame) and parts.json (each group's parts, size, box)."""
import json
import os
import struct
import sys

from OCP.BRep import BRep_Tool
from OCP.BRepMesh import BRepMesh_IncrementalMesh
from OCP.IFSelect import IFSelect_RetDone
from OCP.OCP.collections import Sequence_TDF_Label
from OCP.STEPCAFControl import STEPCAFControl_Reader
from OCP.TCollection import TCollection_ExtendedString
from OCP.TDataStd import TDataStd_Name
from OCP.TDF import TDF_Label
from OCP.TDocStd import TDocStd_Document
from OCP.TopAbs import TopAbs_FACE, TopAbs_REVERSED
from OCP.TopExp import TopExp_Explorer
from OCP.TopLoc import TopLoc_Location
from OCP.TopoDS import TopoDS
from OCP.XCAFDoc import XCAFDoc_DocumentTool


def leaves_of(path):
    """[(the part's path of names, its located shape)] from the STEP's assembly tree."""
    doc = TDocStd_Document(TCollection_ExtendedString("doc"))
    r = STEPCAFControl_Reader()
    r.SetNameMode(True)
    if r.ReadFile(path) != IFSelect_RetDone:
        raise SystemExit("%s: not a STEP file it can read" % path)
    r.Transfer(doc)
    st = XCAFDoc_DocumentTool.ShapeTool_s(doc.Main())

    def name(lab):
        a = TDataStd_Name()
        return a.Get().ToExtString() if lab.FindAttribute(TDataStd_Name.GetID_s(), a) else ""

    out = []

    def walk(lab, loc, names):
        ref = TDF_Label()
        if st.IsReference_s(lab) and st.GetReferredShape_s(lab, ref):
            loc = loc.Multiplied(st.GetLocation_s(lab))
            names = names + [name(lab)]
            lab = ref
        if st.IsAssembly_s(lab):
            comps = Sequence_TDF_Label()
            st.GetComponents_s(lab, comps)
            for i in range(1, comps.Length() + 1):
                walk(comps.Value(i), loc, names)
        else:
            sh = st.GetShape_s(lab)
            out.append(("/".join(names + [name(lab)]), sh.Located(loc.Multiplied(sh.Location()))))

    roots = Sequence_TDF_Label()
    st.GetFreeShapes(roots)
    for i in range(1, roots.Length() + 1):
        walk(roots.Value(i), TopLoc_Location(), [name(roots.Value(i))])
    return out


def triangles(shape, deflection):
    """The shape's triangles in the STEP file's coordinates."""
    BRepMesh_IncrementalMesh(shape, deflection, False, 0.6, True)
    tris = []
    ex = TopExp_Explorer(shape, TopAbs_FACE)
    while ex.More():
        f = TopoDS.Face(ex.Current())
        ex.Next()
        loc = TopLoc_Location()
        t = BRep_Tool.Triangulation_s(f, loc)
        if t is None:
            continue
        trsf = loc.Transformation()
        pts = [t.Node(i).Transformed(trsf) for i in range(1, t.NbNodes() + 1)]
        rev = f.Orientation() == TopAbs_REVERSED
        for i in range(1, t.NbTriangles() + 1):
            a, b, c = t.Triangle(i).Get()
            if rev:
                b, c = c, b
            tris.append([(pts[k - 1].X(), pts[k - 1].Y(), pts[k - 1].Z()) for k in (a, b, c)])
    return tris


def classify(path, box, rules):
    low = path.lower()
    leaf = low.split("/")[-1]
    keep = [k.lower() for k in rules.get("keep", [])]
    if any(k in low for k in rules.get("skip", [])) and not any(k in low for k in keep):
        return None
    sl = rules.get("skip_leaf", {})
    if any(k in leaf for k in sl.get("contains", [])) or leaf in sl.get("exact", []):
        return None
    for s in rules.get("split", []):
        if s["match"] in low:
            ax, lo, hi = s["inside"]
            return s["group"] if box[ax] >= lo and box[ax + 3] <= hi else rules.get("default", "fixed")
    for g in rules.get("groups", []):
        if any(k in low for k in g["match"]):
            return g["name"]
    return rules.get("default", "fixed")


def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    src, rules_path, out = sys.argv[1:]
    rules = json.load(open(rules_path))
    fr = rules["frame"]
    o, ex, ey, ez = fr["origin"], fr["x"], fr["y"], fr["z"]
    dot = lambda p, e: (p[0] - o[0]) * e[0] + (p[1] - o[1]) * e[1] + (p[2] - o[2]) * e[2]  # noqa: E731
    prefix = rules.get("prefix", "part")
    os.makedirs(out, exist_ok=True)
    groups, info = {}, {}
    for path, shape in leaves_of(src):
        tris = triangles(shape, rules.get("deflection", 0.8))
        if not tris:
            continue
        pts = [p for t in tris for p in t]
        box = [min(p[k] for p in pts) for k in range(3)] + [max(p[k] for p in pts) for k in range(3)]
        g = classify(path, box, rules)
        if g is None:
            continue
        groups.setdefault(g, []).extend([[(dot(p, ex), dot(p, ey), dot(p, ez)) for p in t] for t in tris])
        info.setdefault(g, {"parts": []})["parts"].append(path.split("/")[-1])
    for g, tris in groups.items():
        fn = "%s_%s.stl" % (prefix, g)
        with open(os.path.join(out, fn), "wb") as f:
            f.write(("%s: %s" % (prefix, g)).encode()[:80].ljust(80, b" "))
            f.write(struct.pack("<I", len(tris)))
            for a, b, c in tris:
                f.write(struct.pack("<12fH", 0, 0, 0, *a, *b, *c, 0))
        pts = [p for t in tris for p in t]
        info[g].update(file=fn, triangles=len(tris), bytes=os.path.getsize(os.path.join(out, fn)),
                       box=[[round(min(p[k] for p in pts), 1), round(max(p[k] for p in pts), 1)] for k in range(3)])
        print("%-8s %4d parts %7d triangles %5.1f MB  %s" % (g, len(info[g]["parts"]), len(tris), info[g]["bytes"] / 1e6, fn))
    json.dump(info, open(os.path.join(out, "parts.json"), "w"), indent=1)


if __name__ == "__main__":
    main()
