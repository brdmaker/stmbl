"""kigen.place: a board (<name>.kicad_pcb) from the schematic's netlist (run
the schematic first), placed by the board's rules in gen/board.py:

  OUTLINE        ("rect", half) or ("circle", radius), centred; mm
  LAYERS         copper layers
  RULES          {design setting: mm}, e.g. {"m_MinClearance": 0.127}
  TITLE          the board's title
  FIXED          {ref: (side, rotation, x, y)}: put there first (edge
                 connectors, holes, sensors)
  OVERHANG       {ref: mm} a courtyard may stand past the edge (default
                 -0.05: inside, so copper keeps its edge clearance)
  TOP_ONLY, BOTTOM_FIRST, EARLY   sets of refs: the side they go on first,
                 and which are placed before the rest (largest first)
  KEEPOUT        {ref: half}: stay out of the square of that half size
                 around the centre
  ANCHOR         {ref: (x, y)}: start the search there instead of at the
                 centre of the parts it shares nets with
  BIG_NETS       nets that don't pull parts together (GND, supplies)
  PLANES         [(net, layer name)]: unfilled zones over the whole board
  draw(board, h) optional: extra drawings (h: helpers mm, OX, OY, pcbnew)

Every footprint gets its nets and its link to the schematic symbol, so
"Update PCB from schematic" works on it. Every part not fixed goes to the
nearest free spot (courtyards may not overlap; through-hole pins and
far-side pads take room on the other side) next to the parts it shares nets
with. A part that finds no room is left beside the board and listed. Nothing
is routed.

  kigen.py pcb <board dir>
"""
import json
import math
import os
import subprocess
import sys
import tempfile

import pcbnew

from sexp import parse, find, findall

FPDIR = os.environ.get("KICAD7_FOOTPRINT_DIR", "/usr/share/kicad/footprints")
OX, OY = 100.0, 100.0          # board centre in KiCad coordinates (mm)
STEP = 0.25
GAP = 0.0                      # extra space between courtyards
# set by run() from the board's rules (gen/board.py)
OUT = PROJECT = B = None
HALF = 0.0
FIXED = OVERHANG = KEEPOUT = ANCHOR = {}
TOP_ONLY = BOTTOM_FIRST = EARLY = BIG_NETS = set()


def mm(v):
    return pcbnew.FromMM(v)


def netlist():
    with tempfile.TemporaryDirectory() as d:
        out = os.path.join(d, "n.net")
        subprocess.run(["kicad-cli", "sch", "export", "netlist", "-o", out, os.path.join(OUT, PROJECT + ".kicad_sch")],
                       check=True, stdout=subprocess.DEVNULL)
        nl = parse(open(out).read())
    comps = []
    for c in findall(find(nl, "components"), "comp"):
        sp = find(c, "sheetpath")
        comps.append({"ref": find(c, "ref")[1], "value": find(c, "value")[1], "fp": find(c, "footprint")[1],
                      "path": find(sp, "tstamps")[1] + find(c, "tstamps")[1]})
    pins = {}
    for n in findall(find(nl, "nets"), "net"):
        name = find(n, "name")[1].lstrip("/")
        for x in findall(n, "node"):
            pins[(find(x, "ref")[1], find(x, "pin")[1])] = name
    return comps, pins


def courtyard(fp):
    bb = None
    for g in fp.GraphicalItems():
        if g.GetLayer() in (pcbnew.F_CrtYd, pcbnew.B_CrtYd):
            b = g.GetBoundingBox()
            if bb is None:
                bb = b
            else:
                bb.Merge(b)
    if bb is None:
        bb = fp.GetBoundingBox(False, False)
    p = fp.GetPosition()
    return ((bb.GetLeft() - p.x) / 1e6, (bb.GetTop() - p.y) / 1e6, (bb.GetRight() - p.x) / 1e6, (bb.GetBottom() - p.y) / 1e6)


def other_side(fp):
    """The room a part takes on the far side of the board, relative to its
    position, as rectangles: a mounting hole all of its courtyard; else each
    pad that reaches the far copper (through-hole pins, the TC2030's guide
    holes, the copper under a thermal-via pad), with 0.3 mm around."""
    if not fp.Pads():
        return [courtyard(fp)]
    far = pcbnew.F_Cu if fp.IsFlipped() else pcbnew.B_Cu
    pos = fp.GetPosition()
    out = []
    for p in fp.Pads():
        if p.IsOnLayer(far) or p.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH:
            bb = p.GetBoundingBox()
            out.append(((bb.GetLeft() - pos.x) / 1e6 - 0.3, (bb.GetTop() - pos.y) / 1e6 - 0.3,
                        (bb.GetRight() - pos.x) / 1e6 + 0.3, (bb.GetBottom() - pos.y) / 1e6 + 0.3))
    return out


def orient(fp, side, rot):
    fp.SetPosition(pcbnew.VECTOR2I(mm(OX), mm(OY)))
    if fp.IsFlipped() != (side == "B"):
        fp.Flip(fp.GetPosition(), False)
    fp.SetOrientationDegrees(rot)
    return courtyard(fp)


class Board:
    def __init__(self):
        self.occ = {"F": [], "B": []}

    def free(self, side, r, ref):
        x0, y0, x1, y1 = r
        lim = HALF + OVERHANG.get(ref, 0.5 if ref is None else -0.05)  # in from the edge: copper keeps 0.25 mm
        if x0 < -lim - 1e-6 or y0 < -lim - 1e-6 or x1 > lim + 1e-6 or y1 > lim + 1e-6:
            return False
        if B.OUTLINE[0] == "circle" and max(math.hypot(x, y) for x in (x0, x1) for y in (y0, y1)) > lim + 1e-6:
            return False  # a round board: every corner inside
        k = KEEPOUT.get(ref)
        if k and x0 < k and x1 > -k and y0 < k and y1 > -k:
            return False
        for a in self.occ[side]:
            if x0 < a[2] + GAP and x1 > a[0] - GAP and y0 < a[3] + GAP and y1 > a[1] - GAP:
                return False
        return True

    def take(self, sides, r):
        for s in sides:
            self.occ[s].append(r)


def circle(board, r, layer, x=0.0, y=0.0):
    c = pcbnew.PCB_SHAPE(board)
    c.SetShape(pcbnew.SHAPE_T_CIRCLE)
    c.SetCenter(pcbnew.VECTOR2I(mm(OX + x), mm(OY + y)))
    c.SetEnd(pcbnew.VECTOR2I(mm(OX + x + r), mm(OY + y)))
    c.SetLayer(layer)
    c.SetWidth(mm(0.1))
    board.Add(c)


def outline_points():
    if B.OUTLINE[0] == "circle":
        return [(HALF * math.cos(2 * math.pi * i / 72), HALF * math.sin(2 * math.pi * i / 72)) for i in range(72)]
    return [(-HALF, -HALF), (HALF, -HALF), (HALF, HALF), (-HALF, HALF)]


def run(proj_dir, parts, board_mod):
    global OUT, PROJECT, B, HALF, FIXED, OVERHANG, KEEPOUT, ANCHOR, TOP_ONLY, BOTTOM_FIRST, EARLY, BIG_NETS
    OUT, PROJECT, B = proj_dir, parts.PROJECT, board_mod
    HALF = B.OUTLINE[1]
    FIXED, OVERHANG = B.FIXED, getattr(B, "OVERHANG", {})
    KEEPOUT, ANCHOR = getattr(B, "KEEPOUT", {}), getattr(B, "ANCHOR", {})
    TOP_ONLY, BOTTOM_FIRST = getattr(B, "TOP_ONLY", set()), getattr(B, "BOTTOM_FIRST", set())
    EARLY, BIG_NETS = getattr(B, "EARLY", set()), getattr(B, "BIG_NETS", set())
    comps, pins = netlist()
    board = pcbnew.BOARD()
    board.SetCopperLayerCount(B.LAYERS)
    ds = board.GetDesignSettings()
    for k, v in B.RULES.items():
        if hasattr(ds, k):
            setattr(ds, k, mm(v))
    tb = board.GetTitleBlock()
    tb.SetTitle("%s: %s" % (PROJECT, B.TITLE))
    tb.SetRevision(parts.REV)
    tb.SetCompany("stmbl")
    tb.SetComment(0, "Generated by hardware/kigen (gen/board.py): placement only, not routed")

    if B.OUTLINE[0] == "circle":
        circle(board, HALF, pcbnew.Edge_Cuts)
    else:
        edge = pcbnew.PCB_SHAPE(board)
        edge.SetShape(pcbnew.SHAPE_T_RECT)
        edge.SetStart(pcbnew.VECTOR2I(mm(OX - HALF), mm(OY - HALF)))
        edge.SetEnd(pcbnew.VECTOR2I(mm(OX + HALF), mm(OY + HALF)))
        edge.SetLayer(pcbnew.Edge_Cuts)
        edge.SetWidth(mm(0.1))
        board.Add(edge)
    if hasattr(B, "draw"):
        B.draw(board, sys.modules[__name__])

    nets = {}
    for n in sorted(set(pins.values())):
        if n.startswith("unconnected-"):
            continue
        nets[n] = pcbnew.NETINFO_ITEM(board, n)
        board.Add(nets[n])

    fps = {}
    for c in comps:
        lib, name = c["fp"].split(":")
        fp = pcbnew.FootprintLoad(os.path.join(FPDIR, lib + ".pretty"), name)
        if fp is None:
            raise SystemExit("%s: no footprint %s" % (c["ref"], c["fp"]))
        fp.SetFPID(pcbnew.LIB_ID(lib, name))
        fp.SetReference(c["ref"])
        fp.SetValue(c["value"])
        fp.SetPath(pcbnew.KIID_PATH(c["path"]))
        # too dense for references on the silkscreen: they stay on the fab layers
        fp.Reference().SetVisible(False)
        for g in fp.GraphicalItems():
            if isinstance(g, pcbnew.PCB_TEXT) and g.GetText() == "${REFERENCE}":
                g.SetVisible(True)
        missing = []
        for pad in fp.Pads():
            num = pad.GetNumber()
            if not num:
                continue
            n = pins.get((c["ref"], num))
            if n and n in nets:
                pad.SetNet(nets[n])
            elif (c["ref"], num) not in pins and num not in getattr(B, "PAD_NOPIN", {}).get(c["ref"], ()):
                missing.append(num)
        # thermal-via pads carry the exposed pad's number; nothing else should be without a pin
        if missing:
            print("note: %s pads without a symbol pin: %s" % (c["ref"], " ".join(sorted(set(missing)))))
        board.Add(fp)
        fps[c["ref"]] = fp
    sym_pins = {}
    for (ref, num) in pins:
        sym_pins.setdefault(ref, set()).add(num)
    for ref, fp in fps.items():
        pads = {p.GetNumber() for p in fp.Pads()}
        lost = sym_pins.get(ref, set()) - pads
        if lost:
            raise SystemExit("%s: symbol pins with no pad: %s" % (ref, sorted(lost)))

    b = Board()
    placed = {}
    for ref, (side, rot, x, y) in FIXED.items():
        fp = fps[ref]
        r = orient(fp, side, rot)
        fp.SetPosition(pcbnew.VECTOR2I(mm(OX + x), mm(OY + y)))
        rr = (x + r[0], y + r[1], x + r[2], y + r[3])
        if not b.free(side, rr, ref):
            print("warning: %s overlaps where it is fixed" % ref)
        b.take((side,), rr)
        for o in other_side(fp):
            b.take(("B" if side == "F" else "F",), (x + o[0], y + o[1], x + o[2], y + o[3]))
        placed[ref] = (x, y, side)

    def area(ref):
        r = orient(fps[ref], "F", 0)
        return (r[2] - r[0]) * (r[3] - r[1])

    rest = sorted((r for r in fps if r not in placed), key=lambda r: (0 if r in TOP_ONLY else 1 if r in EARLY else 2, -area(r), r))
    net_of = {}
    for (ref, num), n in pins.items():
        net_of.setdefault(ref, set()).add(n)
    grid = [i * STEP - HALF for i in range(int(2 * HALF / STEP) + 1)]
    unplaced = []
    for ref in rest:
        fp = fps[ref]
        if ref in ANCHOR:
            ax, ay = ANCHOR[ref]
        else:
            mates = [placed[o] for o in placed if not o.startswith("H") and (net_of.get(o, set()) & net_of.get(ref, set())) - BIG_NETS]
            if not mates:
                mates = [placed[o] for o in placed if not o.startswith("H") and net_of.get(o, set()) & net_of.get(ref, set())]
            ax = sum(m[0] for m in mates) / len(mates) if mates else 0.0
            ay = sum(m[1] for m in mates) / len(mates) if mates else 0.0
        sides = ["F"] if ref in TOP_ONLY else ["B", "F"] if ref in BOTTOM_FIRST else ["F", "B"]
        shapes = []
        for si, side in enumerate(sides):
            for rot in (0, 90):
                r = orient(fp, side, rot)
                shapes.append((side, rot, r, other_side(fp), si * 2.0))
        cands = []
        for side, rot, r, o, pen in shapes:
            # the grid, and every spot flush against a part or an edge already there
            xs, ys = set(grid), set(grid)
            for a in b.occ[side] + [(-HALF, -HALF, -HALF, -HALF), (HALF, HALF, HALF, HALF)]:
                xs.update((round(a[2] - r[0], 3), round(a[0] - r[2], 3)))
                ys.update((round(a[3] - r[1], 3), round(a[1] - r[3], 3)))
            for k in (KEEPOUT.get(ref),):
                if k:
                    xs.update((round(k - r[0], 3), round(-k - r[2], 3)))
                    ys.update((round(k - r[1], 3), round(-k - r[3], 3)))
            for x in xs:
                for y in ys:
                    if abs(x) <= HALF and abs(y) <= HALF:
                        cands.append((math.hypot(x - ax, y - ay) + pen, x, y, side, rot, r, o))
        cands.sort(key=lambda c: c[0])
        for _, x, y, side, rot, r, o in cands:
            rr = (x + r[0], y + r[1], x + r[2], y + r[3])
            far = "B" if side == "F" else "F"
            oo = [(x + q[0], y + q[1], x + q[2], y + q[3]) for q in o]
            if b.free(side, rr, ref) and all(b.free(far, q, None) for q in oo):
                orient(fp, side, rot)
                fp.SetPosition(pcbnew.VECTOR2I(mm(OX + x), mm(OY + y)))
                b.take((side,), rr)
                for q in oo:
                    b.take((far,), q)
                placed[ref] = (x, y, side)
                break
        else:
            unplaced.append(ref)
    # what found no room waits beside the board
    px, py = HALF + 6.0, -HALF
    for ref in unplaced:
        fp = fps[ref]
        r = orient(fp, "F", 0)
        if py - r[1] + r[3] > HALF + 20:
            px, py = px + 12.0, -HALF
        fp.SetPosition(pcbnew.VECTOR2I(mm(OX + px - r[0]), mm(OY + py - r[1])))
        py += r[3] - r[1] + 1.0

    # planes: not filled here, ZONE_FILLER crashes in headless pcbnew 7 (press B in the editor)
    for net, layer in getattr(B, "PLANES", []):
        zone = pcbnew.ZONE(board)
        zone.SetLayer(getattr(pcbnew, layer.replace(".", "_")))
        zone.SetNet(nets[net])
        zone.SetIsFilled(False)
        ol = zone.Outline()
        ol.NewOutline()
        for x, y in outline_points():
            ol.Append(mm(OX + x), mm(OY + y))
        zone.SetLocalClearance(mm(0.2))
        zone.SetMinThickness(mm(0.2))
        board.Add(zone)

    path = os.path.join(OUT, PROJECT + ".kicad_pcb")
    pro = os.path.join(OUT, PROJECT + ".kicad_pro")
    mine = json.load(open(pro))
    pcbnew.SaveBoard(path, board)
    # saving rewrites the project file: keep the schematic's net classes and sheets
    saved = json.load(open(pro))
    for k in ("net_settings", "sheets", "schematic"):
        saved[k] = mine[k]
    with open(pro, "w") as f:
        json.dump(saved, f, indent=2)
        f.write("\n")
    top = sum(1 for r in placed.values() if r[2] == "F")
    print("placed %d parts (%d top, %d bottom), %d without room: %s" % (
        len(placed), top, len(placed) - top, len(unplaced), " ".join(unplaced) or "-"))
    return 0

