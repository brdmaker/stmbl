"""Generate brdstepper.kicad_pcb from the schematic's netlist (run gen.py first).

The board: 28 x 28 mm, four layers, M2.5 holes on the NEMA11's 23 mm square,
U4 (the encoder) on the bottom, centred on the shaft. Every footprint gets its
nets and its link to the schematic symbol, so "Update PCB from schematic"
works on it.

Placement is a starting point, not a layout: the edge connectors and the
encoder are put where docs/brdstepper_hw.md says; every other part goes to
the nearest free spot (courtyards may not overlap) next to the parts it
shares nets with. A part that finds no room is left beside the board and
listed. Nothing is routed; In1.Cu carries a GND plane.

  python3 pcb.py [OUTDIR]
"""
import json
import math
import os
import subprocess
import sys
import tempfile

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from sexp import parse, find, findall  # noqa: E402

OUT = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.dirname(HERE)
FPDIR = os.environ.get("KICAD7_FOOTPRINT_DIR", "/usr/share/kicad/footprints")
OX, OY = 100.0, 100.0          # board centre in KiCad coordinates (mm)
HALF = 14.0                    # 28 x 28 mm
HOLE = 11.5                    # M2.5 at 23 mm
STEP = 0.25
GAP = 0.0                      # extra space between courtyards

# where the edge connectors and the encoder go: (side, rotation, x, y) of the
# footprint origin. Rotation 0 has the mating face of J1 and the GH connectors
# toward +y; their front is put on the board edge.
FIXED = {
    "H1": ("F", 0, -HOLE, -HOLE), "H2": ("F", 0, HOLE, -HOLE),
    "H3": ("F", 0, -HOLE, HOLE), "H4": ("F", 0, HOLE, HOLE),
    "U4": ("B", 90, 0.0, 0.0),                                 # on the shaft axis (turned: J2's pins pass beside it)
    "J2": ("F", 90, -4.3, 3.0),                                # supply, beside the shaft: DC only (see README)
    "J1": ("F", 180, 0.0, -HALF + 3.1),                        # USB-C, north edge ("PCB Edge" at +3.1)
    "J4": ("F", 90, HALF - 3.2, 0.0),                          # smart-serial, east edge
    "J5": ("F", 270, -HALF + 3.2, 0.0),                        # CAN, west edge
    "J6": ("F", 0, 0.0, HALF - 3.2),                           # CAN, south edge
}
OVERHANG = {"J1": 1.2, "J4": 0.05, "J5": 0.05, "J6": 0.05, "H1": 0.5, "H2": 0.5, "H3": 0.5, "H4": 0.5}         # mm a connector's courtyard may stand past the edge

# parts that stay on top: the bottom faces the motor, a few mm away
TOP_ONLY = {"L1", "C1", "C2", "D2", "J2", "J3"}
BOTTOM_FIRST = {"U1", "U8", "U9"}  # thin, leaving the top to what must be there
EARLY = {"U2", "U3"}             # the bridges: kept off the centre, and their thermal pads reach both sides
# parts carrying the switched coil current stay out of a square around the
# shaft (§8: their field reaches the sensor); the supply (DC) parts may come
# closer, but not over the sensor itself
KEEPOUT = {r: 5.5 for r in ("U2", "U3", "C23", "C24", "C27", "C28")}
KEEPOUT["J3"] = 4.0            # the motor-lead pads
KEEPOUT.update({r: 3.0 for r in ("L1", "U6", "C1", "C2", "C3", "C4", "C5", "D2")})

# a few parts start from a chosen spot instead of the centre of their nets
ANCHOR = {"U1": (0.0, -7.0), "J3": (6.0, 2.0), "U2": (6.0, -4.0), "U3": (6.0, 4.0),
          "U8": (7.0, -6.0), "U9": (-7.0, 0.0), "J7": (-4.0, -6.0), "L1": (-8.0, 4.0), "U6": (-8.0, 8.0)}
BIG_NETS = {"GND", "3V3", "VM", "5V", "VDDA"}


def mm(v):
    return pcbnew.FromMM(v)


def netlist():
    with tempfile.TemporaryDirectory() as d:
        out = os.path.join(d, "n.net")
        subprocess.run(["kicad-cli", "sch", "export", "netlist", "-o", out, os.path.join(OUT, "brdstepper.kicad_sch")],
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


def main():
    comps, pins = netlist()
    board = pcbnew.BOARD()
    board.SetCopperLayerCount(4)
    ds = board.GetDesignSettings()
    for k, v in (("m_MinClearance", 0.127), ("m_TrackMinWidth", 0.127), ("m_ViasMinSize", 0.45), ("m_MinThroughDrill", 0.2),
                 ("m_CopperEdgeClearance", 0.25), ("m_HoleToHoleMin", 0.25),
                 ("m_HoleClearance", 0.15)):  # the USB4105 footprint has 0.194 mm between its own hole and pad
        if hasattr(ds, k):
            setattr(ds, k, mm(v))
    tb = board.GetTitleBlock()
    tb.SetTitle("brdstepper: closed-loop NEMA11 stepper drive")
    tb.SetRevision("0.1")
    tb.SetCompany("stmbl")
    tb.SetComment(0, "Generated by gen/pcb.py: placement only, not routed")

    edge = pcbnew.PCB_SHAPE(board)
    edge.SetShape(pcbnew.SHAPE_T_RECT)
    edge.SetStart(pcbnew.VECTOR2I(mm(OX - HALF), mm(OY - HALF)))
    edge.SetEnd(pcbnew.VECTOR2I(mm(OX + HALF), mm(OY + HALF)))
    edge.SetLayer(pcbnew.Edge_Cuts)
    edge.SetWidth(mm(0.1))
    board.Add(edge)
    # the shaft and the magnet under U4
    for r, layer in ((3.0, pcbnew.B_Fab), (0.6, pcbnew.Dwgs_User)):
        c = pcbnew.PCB_SHAPE(board)
        c.SetShape(pcbnew.SHAPE_T_CIRCLE)
        c.SetCenter(pcbnew.VECTOR2I(mm(OX), mm(OY)))
        c.SetEnd(pcbnew.VECTOR2I(mm(OX + r), mm(OY)))
        c.SetLayer(layer)
        c.SetWidth(mm(0.1))
        board.Add(c)

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
            elif (c["ref"], num) not in pins and not (c["ref"].startswith("U") and num in ("17", "49")):
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

    # GND plane on In1.Cu
    zone = pcbnew.ZONE(board)
    zone.SetLayer(pcbnew.In1_Cu)
    zone.SetNet(nets["GND"])
    zone.SetIsFilled(False)
    ol = zone.Outline()
    ol.NewOutline()
    for x, y in ((-HALF, -HALF), (HALF, -HALF), (HALF, HALF), (-HALF, HALF)):
        ol.Append(mm(OX + x), mm(OY + y))
    zone.SetLocalClearance(mm(0.2))
    zone.SetMinThickness(mm(0.2))
    board.Add(zone)
    # not filled here: ZONE_FILLER crashes in headless pcbnew 7 (press B in the editor)

    path = os.path.join(OUT, "brdstepper.kicad_pcb")
    pro = os.path.join(OUT, "brdstepper.kicad_pro")
    mine = json.load(open(pro))
    pcbnew.SaveBoard(path, board)
    # saving rewrites the project file: keep gen.py's net classes and sheets
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


if __name__ == "__main__":
    sys.exit(main())
