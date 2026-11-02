#!/usr/bin/env python3
"""stman's board: stmbl v4.0's LV placement kept, the new circuits in the freed lower half.

  python3 gen/pcb.py            writes stman.kicad_pcb (run gen/build.py first: it reads the schematic's netlist)

- **Outline:** stmbl v4.0's, so the v4 case fits, without the slots that isolated
  the HV half (stman's lower half is on the drive's ground like the rest).
- **stmbl's parts:** every part stman keeps is stmbl's own footprint, copied from
  hw/kicad/v4.0/stmbl_4.0.kicad_pcb where stmbl placed it (side, rotation,
  position), with stman's nets. Fiducials and holes in the LV half come along.
- **New parts:** connectors and the main ICs at the positions in FIXED; their
  passives and the rest next to the parts they share nets with
  (hardware/kigen/place.py's search), in the free room.
- **Isolation barrier** (smart-serial): a no-copper rule area along the line
  between the cable side (J3, U8, U9 and their parts, top right) and the
  rest; U200 (ISO7331FC) straddles it where stmbl's U10 was.

Placement only: nothing is routed (stmbl's tracks belonged to its nets; most
of them changed).
"""
import json
import os
import subprocess
import sys
import tempfile

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.dirname(HERE)
REPO = os.path.dirname(os.path.dirname(OUT))
sys.path.insert(0, os.path.join(REPO, "hardware", "kigen"))
from sexp import parse, find, findall  # noqa: E402

ORIG = os.path.join(REPO, "hw", "kicad", "v4.0", "stmbl_4.0.kicad_pcb")
FPDIRS = {"stmbl": os.path.join(REPO, "hw", "kicad", "lib", "stmbl.pretty")}
KICAD_FP = "/usr/share/kicad/footprints"
LAYERS = 4  # stmbl's LV is 2 layers; stman's precision analog wants a ground plane (In1)

# new parts: (side, rotation, x, y) of their courtyard's centre, in stmbl's board coordinates (mm)
FIXED = {
    "U200": ("F", 0, 166.5, 60.5),          # the isolator, in stmbl's U10 place, across the barrier
    # analog: terminals on the lower left edge (where stmbl's motor terminals were), DACs and filter beside them
    "J5": ("F", 270, 113.5, 118.5), "J6": ("F", 270, 113.5, 139.5),
    "U40": ("F", 0, 127.0, 111.5), "U41": ("F", 0, 127.0, 121.5), "U42": ("F", 0, 138.0, 116.5),
    "U56": ("F", 0, 153.0, 111.0),           # NMA1215 ±15 V
    "U45": ("F", 0, 134.5, 131.4),           # AD7606
    # drive I/O: terminals along the bottom edge, the switches above them
    "J7": ("F", 0, 141.2, 145.0), "J8": ("F", 0, 179.1, 145.0),
    "U50": ("F", 0, 158.0, 128.0), "U51": ("F", 0, 176.0, 128.0),
    "U55": ("F", 0, 168.0, 117.0), "U57": ("F", 0, 178.0, 117.0),
}
# terminal blocks: their courtyard flush with an edge after placing ("left" x / "bottom" y)
FLUSH = {"J5": ("left", 110.05), "J6": ("left", 110.05), "J7": ("bottom", 149.7), "J8": ("bottom", 149.7)}
MOVE = {"LOGO1": ("F", 0, 188.0, 112.0)}   # stmbl's logo: out of the terminals' way
# new parts that connect to the MCU: placed by their other mates (the MCU's room is taken)
NOT_ANCHOR = {"U26", "U23"}
# the isolation barrier (stmbl coordinates): from the top edge down past U200, then across to the right edge
BARRIER = [(166.5, 49.75), (166.5, 67.2), (198.0, 67.2)]
BARRIER_W = 1.0
CABLE_SIDE = {"J3", "U8", "U9", "R62", "R63", "R64", "R65", "R66", "R67", "C53", "C54", "R31", "C17", "L7", "C112",
              "R101", "R102", "JP200", "JP201", "R260", "R261", "R262", "R263"}
CABLE_BOX = (167.5, 50.5, 197.5, 66.6)   # where the new cable-side parts may go


def mm(v):
    return pcbnew.FromMM(v)


# search offsets around a part's anchor, nearest first (0.5 mm grid, up to 50 x 35 mm away)
OFFSETS = sorted((abs(i * 0.5) + abs(j * 0.5), i * 0.5, j * 0.5) for i in range(-100, 101) for j in range(-70, 71))


def netlist():
    with tempfile.TemporaryDirectory() as d:
        out = os.path.join(d, "n.net")
        subprocess.run(["kicad-cli", "sch", "export", "netlist", "-o", out, os.path.join(OUT, "stman.kicad_sch")],
                       check=True, stdout=subprocess.DEVNULL)
        nl = parse(open(out).read())
    comps = []
    for c in findall(find(nl, "components"), "comp"):
        sp = find(c, "sheetpath")
        comps.append({"ref": find(c, "ref")[1], "value": find(c, "value")[1], "fp": find(c, "footprint")[1],
                      "path": find(sp, "tstamps")[1] + str(find(c, "tstamps")[1]).split()[0]})  # a multi-unit part lists one per unit
    pins = {}
    for n in findall(find(nl, "nets"), "net"):
        name = find(n, "name")[1]
        for x in findall(n, "node"):
            pins[(find(x, "ref")[1], find(x, "pin")[1])] = name
    return comps, pins


def keep_edge(d):
    """stmbl's outline without the HV/LV slots (and the slot open to the right edge)"""
    xs = [pcbnew.ToMM(d.GetStart().x), pcbnew.ToMM(d.GetEnd().x)]
    ys = [pcbnew.ToMM(d.GetStart().y), pcbnew.ToMM(d.GetEnd().y)]
    return not (min(xs) >= 115 and 96 <= min(ys) and max(ys) <= 111.5)


def courtyard(fp):
    xs, ys = [], []
    for g in fp.GraphicalItems():
        if g.GetLayer() in (pcbnew.F_CrtYd, pcbnew.B_CrtYd):
            b = g.GetBoundingBox()
            xs += [b.GetLeft(), b.GetRight()]
            ys += [b.GetTop(), b.GetBottom()]
    if not xs:
        b = fp.GetBoundingBox(False, False)
        xs, ys = [b.GetLeft(), b.GetRight()], [b.GetTop(), b.GetBottom()]
    return (pcbnew.ToMM(min(xs)), pcbnew.ToMM(min(ys)), pcbnew.ToMM(max(xs)), pcbnew.ToMM(max(ys)))


def far_pads(fp):
    """through-hole pads take room on the other side too"""
    out = []
    for p in fp.Pads():
        if p.GetAttribute() in (pcbnew.PAD_ATTRIB_PTH, pcbnew.PAD_ATTRIB_NPTH):
            bb = p.GetBoundingBox()
            out.append((pcbnew.ToMM(bb.GetLeft()) - 0.3, pcbnew.ToMM(bb.GetTop()) - 0.3,
                        pcbnew.ToMM(bb.GetRight()) + 0.3, pcbnew.ToMM(bb.GetBottom()) + 0.3))
    return out


def load_fp(fpid):
    lib, name = fpid.split(":")
    path = FPDIRS.get(lib, os.path.join(KICAD_FP, lib + ".pretty"))
    fp = pcbnew.FootprintLoad(path, name)
    if fp is None:
        raise SystemExit("no footprint %s" % fpid)
    fp.SetFPID(pcbnew.LIB_ID(lib, name))
    return fp


def inside(r, edge_box, holes):
    x0, y0, x1, y1 = r
    ex0, ey0, ex1, ey1 = edge_box
    if x0 < ex0 or y0 < ey0 or x1 > ex1 or y1 > ey1:
        return False
    return not any(x0 < h[2] and x1 > h[0] and y0 < h[3] and y1 > h[1] for h in holes)


def main():
    comps, pins = netlist()
    orig = pcbnew.LoadBoard(ORIG)
    ofps = {f.GetReference(): f for f in orig.GetFootprints()}
    board = pcbnew.BOARD()
    board.SetCopperLayerCount(LAYERS)
    ds, ods = board.GetDesignSettings(), orig.GetDesignSettings()
    ds.m_MinClearance = mm(0.15)
    ds.m_TrackMinWidth = mm(0.15)
    tb = board.GetTitleBlock()
    tb.SetTitle("stman: stmbl with an analog output")
    tb.SetRevision("0.1")
    tb.SetCompany("stmbl")
    tb.SetComment(0, "stmbl v4.0 LV placement kept; new parts in the lower half (hardware/stman/gen/pcb.py). Not routed.")
    # ---- outline
    for d in orig.GetDrawings():  # new shapes of the same geometry (a Duplicate() from the other board corrupts the heap)
        if d.GetLayer() == pcbnew.Edge_Cuts and keep_edge(d):
            e = pcbnew.PCB_SHAPE(board)
            if d.GetShape() == pcbnew.SHAPE_T_ARC:
                e.SetShape(pcbnew.SHAPE_T_ARC)
                e.SetArcGeometry(d.GetStart(), d.GetArcMid(), d.GetEnd())
            else:
                e.SetShape(pcbnew.SHAPE_T_SEGMENT)
                e.SetStart(d.GetStart())
                e.SetEnd(d.GetEnd())
            e.SetLayer(pcbnew.Edge_Cuts)
            e.SetWidth(mm(0.1))
            board.Add(e)
    seg = pcbnew.PCB_SHAPE(board)
    seg.SetShape(pcbnew.SHAPE_T_SEGMENT)
    seg.SetStart(pcbnew.VECTOR2I(mm(198.0), mm(101.0)))
    seg.SetEnd(pcbnew.VECTOR2I(mm(198.0), mm(106.0)))
    seg.SetLayer(pcbnew.Edge_Cuts)
    seg.SetWidth(mm(0.1))
    board.Add(seg)
    # ---- nets
    nets = {}
    for n in sorted(set(pins.values())):
        if n.startswith("unconnected-"):
            continue
        nets[n] = pcbnew.NETINFO_ITEM(board, n)
        board.Add(nets[n])
    # ---- parts
    occ = {"F": [], "B": []}
    placed, new = {}, []
    for c in comps:
        ref = c["ref"]
        if ref in ofps and ofps[ref].GetFPID().GetLibItemName() != c["fp"].split(":")[-1]:
            fp = load_fp(c["fp"])                # stman changes the footprint (C102 0603 -> 0805): stmbl's place
            fp.SetPosition(ofps[ref].GetPosition())
            if ofps[ref].IsFlipped():
                fp.Flip(fp.GetPosition(), False)
            fp.SetOrientation(ofps[ref].GetOrientation())
        elif ref in ofps:                       # stmbl's footprint where stmbl has it
            fp = pcbnew.FOOTPRINT(ofps[ref])
        else:
            fp = load_fp(c["fp"])
            new.append(ref)
        fp.SetReference(ref)
        fp.SetValue(c["value"])
        fp.SetPath(pcbnew.KIID_PATH(c["path"]))
        board.Add(fp)
        for pad in fp.Pads():  # after Add: a copied pad still points at the other board's nets until then
            n = pins.get((ref, pad.GetNumber()))
            if n in nets:
                pad.SetNet(nets[n])
            else:
                pad.SetNetCode(0)
        placed[ref] = fp
        if ref in ofps:
            side = "B" if fp.IsFlipped() else "F"
            occ[side].append(courtyard(fp))
            occ["F" if side == "B" else "B"] += far_pads(fp)
    # fiducials, holes and the logo of stmbl's LV half (on the board only, not in the schematic)
    for ref, f in ofps.items():
        y = pcbnew.ToMM(f.GetPosition().y)
        if ref not in placed and y < 100 and (ref.startswith("REF") or ref.startswith("H") or not f.Pads()):
            if ref.startswith("REF") or ref.startswith("H"):
                g = pcbnew.FOOTPRINT(f)
                if ref == "REF1":  # under U200 now: beside it
                    g.SetPosition(pcbnew.VECTOR2I(mm(157.0), mm(66.0)))
                board.Add(g)
                occ["B" if g.IsFlipped() else "F"].append(courtyard(g))
    # new parts: fixed ones first, then the rest beside what they connect to
    edge_box = (98.6, 50.3, 197.4, 149.2)
    holes = [(97.0, 90.5, 110.6, 106.0)]          # the outline's step at the lower left
    net_of = {}
    for (r, p), n in pins.items():
        net_of.setdefault(r, set()).add(n)
    big = {"GND", "+3V3", "+5V", "+12V", "+24V", "+15VA", "-15VA", "SS_GND", "SS_5V"}

    def try_put(fp, side, rot, x, y):
        if fp.IsFlipped() != (side == "B"):
            fp.Flip(fp.GetPosition(), False)
        fp.SetOrientationDegrees(rot)
        fp.SetPosition(pcbnew.VECTOR2I(mm(x), mm(y)))
        r = courtyard(fp)
        fr = far_pads(fp)
        return r, fr

    def free(side, r, fr, box):
        if not inside(r, box, holes):
            return False
        for a in occ[side]:
            if r[0] < a[2] and r[2] > a[0] and r[1] < a[3] and r[3] > a[1]:
                return False
        other = "B" if side == "F" else "F"
        for q in fr:
            for a in occ[other]:
                if q[0] < a[2] and q[2] > a[0] and q[1] < a[3] and q[3] > a[1]:
                    return False
        return True

    def take(side, r, fr):
        occ[side].append(r)
        occ["B" if side == "F" else "F"] += fr

    for ref, (side, rot, x, y) in MOVE.items():
        fp = placed[ref]
        r, fr = try_put(fp, side, rot, x, y)
        take(side, r, fr)
    for ref, (side, rot, cx, cy) in FIXED.items():
        fp = placed[ref]
        r, fr = try_put(fp, side, rot, 0.0, 0.0)          # the courtyard's centre at (cx, cy)
        x, y = cx - (r[0] + r[2]) / 2, cy - (r[1] + r[3]) / 2
        if ref in FLUSH:
            how, at = FLUSH[ref]
            if how == "left":
                x = at - r[0]
            else:
                y = at - r[3]
        r, fr = try_put(fp, side, rot, x, y)
        take(side, r, fr)
    todo = [r for r in new if r not in FIXED]
    done = set(placed) - set(todo)
    unplaced = []

    def mates_of(ref):
        return [o for o in done if o != ref and o not in NOT_ANCHOR and (net_of.get(o, set()) & net_of.get(ref, set())) - big]

    while todo:
        # next: the part with the most placed mates (its signal nets), the larger first
        todo.sort(key=lambda r: (-len(mates_of(r)), -(lambda b: (b[2] - b[0]) * (b[3] - b[1]))(courtyard(placed[r]))))
        ref = todo.pop(0)
        fp = placed[ref]
        mates = mates_of(ref)
        pts = [((courtyard(placed[o])[0] + courtyard(placed[o])[2]) / 2, (courtyard(placed[o])[1] + courtyard(placed[o])[3]) / 2)
               for o in mates] or [(150.0, 125.0)]
        ax, ay = sum(p[0] for p in pts) / len(pts), sum(p[1] for p in pts) / len(pts)
        box = CABLE_BOX if ref in CABLE_SIDE else edge_box
        shapes = []
        for side in ("F", "B"):
            for rot in (0, 90):
                r, fr = try_put(fp, side, rot, 0.0, 0.0)
                shapes.append((side, rot, r, fr, 3.0 if side == "B" else 0.0))
        best = None
        for d, dx, dy in OFFSETS:
            if best and d >= best[0]:
                break
            for side, rot, r0, fr0, pen in shapes:
                if best and d + pen >= best[0]:
                    continue
                x, y = ax + dx, ay + dy
                r = (r0[0] + x, r0[1] + y, r0[2] + x, r0[3] + y)
                fr = [(q[0] + x, q[1] + y, q[2] + x, q[3] + y) for q in fr0]
                if free(side, r, fr, box) and not crosses_barrier(r, ref):
                    best = (d + pen, side, rot, x, y)
        done.add(ref)
        if best:
            _, side, rot, x, y = best
            r, fr = try_put(fp, side, rot, x, y)
            take(side, r, fr)
        else:
            unplaced.append(ref)
            try_put(fp, "F", 0, 205.0, 55.0 + 3 * len(unplaced))
    # ---- the isolation barrier: a rule area without copper, and a silkscreen line
    for (x0, y0), (x1, y1) in zip(BARRIER, BARRIER[1:]):
        z = pcbnew.ZONE(board)
        z.SetIsRuleArea(True)
        z.SetDoNotAllowCopperPour(True)
        z.SetDoNotAllowTracks(True)
        z.SetDoNotAllowVias(True)
        z.SetDoNotAllowPads(False)
        z.SetDoNotAllowFootprints(False)
        z.SetLayerSet(pcbnew.LSET.AllCuMask())
        h = BARRIER_W / 2
        ol = z.Outline()
        ol.NewOutline()
        for px, py in ((min(x0, x1) - h, min(y0, y1) - h), (max(x0, x1) + h, min(y0, y1) - h),
                       (max(x0, x1) + h, max(y0, y1) + h), (min(x0, x1) - h, max(y0, y1) + h)):
            ol.Append(mm(px), mm(py))
        z.SetZoneName("isolation barrier")
        board.Add(z)
        for layer in (pcbnew.F_SilkS, pcbnew.B_SilkS):
            s = pcbnew.PCB_SHAPE(board)
            s.SetShape(pcbnew.SHAPE_T_SEGMENT)
            s.SetStart(pcbnew.VECTOR2I(mm(x0), mm(y0)))
            s.SetEnd(pcbnew.VECTOR2I(mm(x1), mm(y1)))
            s.SetLayer(layer)
            s.SetWidth(mm(0.2))
            board.Add(s)
    t = pcbnew.PCB_TEXT(board)
    t.SetText("SMART-SERIAL (CABLE SIDE)")
    t.SetPosition(pcbnew.VECTOR2I(mm(181.0), mm(68.8)))
    t.SetLayer(pcbnew.F_SilkS)
    t.SetTextSize(pcbnew.VECTOR2I(mm(0.8), mm(0.8)))
    board.Add(t)
    # ---- the ground plane (unfilled: press B in the editor)
    z = pcbnew.ZONE(board)
    z.SetLayer(pcbnew.In1_Cu)
    z.SetNet(nets["GND"])
    ol = z.Outline()
    ol.NewOutline()
    for px, py in ((98.5, 50.2), (166.0, 50.2), (166.0, 67.7), (197.5, 67.7), (197.5, 149.3), (110.5, 149.3), (110.5, 106.0),
                   (98.5, 91.0)):
        ol.Append(mm(px), mm(py))
    z.SetZoneName("GND plane (board side)")
    z.SetIsFilled(False)
    board.Add(z)
    # every courtyard against every other on its side
    allfp = list(board.GetFootprints())
    clash = []
    for i, a in enumerate(allfp):
        ra, sa = courtyard(a), a.IsFlipped()
        for b2 in allfp[i + 1:]:
            if b2.IsFlipped() != sa:
                continue
            rb = courtyard(b2)
            if ra[0] < rb[2] - 0.01 and ra[2] > rb[0] + 0.01 and ra[1] < rb[3] - 0.01 and ra[3] > rb[1] + 0.01:
                clash.append("%s/%s" % (a.GetReference(), b2.GetReference()))
    new_set = set(new)
    mine = [c for c in clash if set(c.split("/")) & new_set]
    print("courtyard overlaps involving new parts: %s; stmbl's own (as in stmbl v4.0): %d" % (" ".join(mine) or "none", len(clash) - len(mine)))
    path = os.path.join(OUT, "stman.kicad_pcb")
    pro = os.path.join(OUT, "stman.kicad_pro")
    mine = json.load(open(pro))
    pcbnew.SaveBoard(path, board)
    saved = json.load(open(pro))
    for k in ("sheets", "schematic"):
        if k in mine:
            saved[k] = mine[k]
    with open(pro, "w") as f:
        json.dump(saved, f, indent=2)
        f.write("\n")
    print("stmbl parts in place: %d; new: %d placed, %d without room: %s" % (
        len(placed) - len(new), len(new) - len(unplaced), len(unplaced), " ".join(unplaced) or "-"))


def crosses_barrier(r, ref):
    """a part (not U200) may not lie across the barrier"""
    if ref == "U200":
        return False
    for (x0, y0), (x1, y1) in zip(BARRIER, BARRIER[1:]):
        h = BARRIER_W / 2 + 0.3
        bx0, bx1, by0, by1 = min(x0, x1) - h, max(x0, x1) + h, min(y0, y1) - h, max(y0, y1) + h
        if r[0] < bx1 and r[2] > bx0 and r[1] < by1 and r[3] > by0:
            return True
    return False


if __name__ == "__main__":
    main()
