"""Route a board with BrdRoute (the Specctra autorouter) and bring the result
back into the .kicad_pcb:

  kigen.py route <board dir> [router args...]

Exports <name>.dsn with pcbnew, runs `brdrouter_cli` (BRDROUTER env var, or
the BrdRoute checkout next to this repository), imports its .ses, fills the zones and saves
the board. Needs the router built (cmake --build build --target brdrouter_cli).
Routing is deterministic: the same board gives the same copper.
"""
import json
import os
import shutil
import subprocess
import sys

import pcbnew

import plane
from sexp import parse, find, findall

HERE = os.path.dirname(os.path.abspath(__file__))
# BRDROUTER, or a BrdRoute checkout next to this repository
CLI = os.environ.get("BRDROUTER", os.path.join(HERE, "..", "..", "..", "BrdRoute", "build", "brdrouter_cli"))


def import_ses(board, path):
    """Wires and vias of a Specctra session into the board. (pcbnew's own
    ImportSpecctraSES needs the editor, not a headless Python.) The SES is
    in um, y down; the board's origin is the DSN's."""
    ses = parse(open(path).read())
    res = find(find(ses, "routes"), "resolution")
    unit = {"um": 1000.0, "mm": 1e6}.get(str(res[1]), 1000.0) / float(res[2]) if len(res) > 2 else 1000.0
    tracks = vias = 0
    for net in findall(find(find(ses, "routes"), "network_out"), "net"):
        ni = board.FindNet(str(net[1]))
        if ni is None:
            raise SystemExit("SES net %s is not on the board" % net[1])
        for w in findall(net, "wire"):
            path_ = find(w, "path")
            via = find(w, "via")
            if path_ is not None:
                layer = board.GetLayerID(str(path_[1]))
                width = int(float(path_[2]) * unit)
                pts = [(int(float(path_[i]) * unit), -int(float(path_[i + 1]) * unit)) for i in range(3, len(path_) - 1, 2)]
                for a, b in zip(pts, pts[1:]):
                    if a == b:
                        continue
                    t = pcbnew.PCB_TRACK(board)
                    t.SetStart(pcbnew.VECTOR2I(*a))
                    t.SetEnd(pcbnew.VECTOR2I(*b))
                    t.SetLayer(layer)
                    t.SetWidth(width)
                    t.SetNet(ni)
                    board.Add(t)
                    tracks += 1
            elif via is not None:
                # padstack name Via[0-3]_<pad>:<drill>_um
                pad, drill = str(via[1]).split("_")[1].split(":")
                v = pcbnew.PCB_VIA(board)
                v.SetViaType(pcbnew.VIATYPE_THROUGH)
                v.SetPosition(pcbnew.VECTOR2I(int(float(via[2]) * unit), -int(float(via[3]) * unit)))
                v.SetWidth(int(float(pad) * 1000))
                v.SetDrill(int(float(drill) * 1000))
                v.SetNet(ni)
                board.Add(v)
                vias += 1
    return tracks, vias


def inset_boundary(dsn, edge):
    """The router does not keep copper off the board edge: shrink the DSN's
    rectangular boundary by the edge clearance (um)."""
    import re
    text = open(dsn).read()
    m = re.search(r"\(boundary\s*\(path pcb 0([^)]*)\)", text)
    if not m:
        return
    v = [float(t) for t in m.group(1).split()]
    xs, ys = v[0::2], v[1::2]
    x0, x1, y0, y1 = min(xs) + edge, max(xs) - edge, min(ys) + edge, max(ys) - edge
    if len(v) != 10 or len(set(xs)) != 2 or len(set(ys)) != 2:
        return  # not a rectangle: left alone
    pts = [(x1, y0), (x0, y0), (x0, y1), (x1, y1), (x1, y0)]
    new = "".join(" %g %g" % p for p in pts)
    open(dsn, "w").write(text[:m.start(1)] + new + text[m.end(1):])


def first_via_only(dsn):
    """KiCad lists the board's default via after the net classes' (here 0.6 mm
    after 0.5 mm): the router picks the larger. Keep the net classes' one."""
    import re
    text = open(dsn).read()
    text = re.sub(r'(\n\s*\(via ("[^"]+"|\S+)) (?:"[^"]+"|[^\s()]+)+\)', r"\1)", text, count=1)
    open(dsn, "w").write(text)


def load_bare(path):
    """The board without its copper (tracks, vias, fills). pcbnew 7's Python
    crashes when tracks are removed from a loaded board, so they are cut out
    of the file text."""
    import tempfile
    import re
    text = open(path).read()
    # top-level segments, vias, arcs and the zones' filled polygons, whatever the indentation
    pat = re.compile(r"\n[ \t]*\((?:segment|via|arc|filled_polygon)[\s(]")
    while True:
        m = pat.search(text)
        if not m:
            break
        i = m.start() + 1 + len(text[m.start() + 1:]) - len(text[m.start() + 1:].lstrip(" \t"))
        text = text[:m.start()] + text[plane._balanced(text, i):]
    with tempfile.TemporaryDirectory() as d:
        tmp = os.path.join(d, os.path.basename(path))
        open(tmp, "w").write(text)
        pro = path[:-len(".kicad_pcb")] + ".kicad_pro"   # the design rules and net classes live in the project
        if os.path.exists(pro):
            shutil.copy(pro, tmp[:-len(".kicad_pcb")] + ".kicad_pro")
        return pcbnew.LoadBoard(tmp)


def fill(board):
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())


def run(proj_dir, parts, bmod, args):
    pcb = os.path.join(proj_dir, parts.PROJECT + ".kicad_pcb")
    dsn = os.path.join(proj_dir, parts.PROJECT + ".dsn")
    ses = os.path.join(proj_dir, parts.PROJECT + ".ses")
    board = load_bare(pcb)  # a fresh route: no old copper
    ds = board.GetDesignSettings()
    for k, v in getattr(bmod, "RULES", {}).items():   # the board's design rules (kigen sch does not write them)
        if hasattr(ds, k):
            setattr(ds, k, pcbnew.FromMM(v))
    planes = getattr(bmod, "PLANES", [])
    half = bmod.OUTLINE[1]
    for net, layer in planes:  # the plane's net is connected by a via per pad, before the router starts
        placed, failed = plane.fanout(board, net, half)
        print("%s plane on %s: %d vias%s" % (net, layer, placed, ", no room for " + " ".join(failed) if failed else ""))
    if not pcbnew.ExportSpecctraDSN(board, dsn):
        raise SystemExit("DSN export failed")
    inset_boundary(dsn, board.GetDesignSettings().m_CopperEdgeClearance / 1000.0)
    first_via_only(dsn)
    plane.prepare_dsn(dsn, planes)
    if os.path.exists(ses):
        os.remove(ses)
    r = subprocess.run([CLI, dsn, ses] + list(args), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    lines = r.stdout.replace("\r", "\n").splitlines()
    keys = ("Done:", "Routed ", "Redundancy:", "Orphans ", "[drc:post")
    sys.stdout.write("".join(l.replace("[batch_autorouter] ", "") + "\n" for l in lines if any(k in l for k in keys)))
    # the router exits non-zero when connections stay unrouted: what matters is that it wrote a session
    if not os.path.exists(ses):
        sys.stdout.write(r.stdout[-2000:])
        raise SystemExit("router failed (exit %d)" % r.returncode)
    # a session that repeats the wiring the router was given (the plane fanout) replaces it
    sesnets = {str(n[1]) for n in findall(find(find(parse(open(ses).read()), "routes"), "network_out"), "net")}
    if any(net in sesnets for net, _ in planes):
        board = load_bare(pcb)
    print("imported %d track segments, %d vias" % import_ses(board, ses))
    fill(board)
    pro = pcb[:-len(".kicad_pcb")] + ".kicad_pro"
    mine = json.load(open(pro))
    pcbnew.SaveBoard(pcb, board)
    # saving rewrites the project file: keep the schematic's net classes and sheets
    saved = json.load(open(pro))
    for k in ("net_settings", "sheets", "schematic"):
        saved[k] = mine[k]
    # the design rules of gen/board.py (the project file kigen sch writes has none; a headless save does not add them)
    names = {"m_MinClearance": "min_clearance", "m_TrackMinWidth": "min_track_width", "m_ViasMinSize": "min_via_diameter",
             "m_MinThroughDrill": "min_through_hole_diameter", "m_CopperEdgeClearance": "min_copper_edge_clearance",
             "m_HoleToHoleMin": "min_hole_to_hole", "m_HoleClearance": "min_hole_clearance"}
    rules = saved.setdefault("board", {}).setdefault("design_settings", {}).setdefault("rules", {})
    for k, v in getattr(bmod, "RULES", {}).items():
        if k in names:
            rules[names[k]] = v
    with open(pro, "w") as f:
        json.dump(saved, f, indent=2)
        f.write("\n")
    print("routed:", pcb)
    return 0
