"""Route a board with BrdRoute (the Specctra autorouter) and bring the result
back into the .kicad_pcb:

  kigen.py route <board dir> [router args...]

Exports <name>.dsn with pcbnew, runs `brdrouter_cli` (BRDROUTER env var, or
~/BrdRoute/build/brdrouter_cli), imports its .ses, fills the zones and saves
the board. Needs the router built (cmake --build build --target brdrouter_cli).
Routing is deterministic: the same board gives the same copper.
"""
import os
import subprocess
import sys

import pcbnew

from sexp import parse, find, findall

CLI = os.environ.get("BRDROUTER", os.path.expanduser("~/BrdRoute/build/brdrouter_cli"))


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


def fill(board):
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())


def run(proj_dir, parts, args):
    pcb = os.path.join(proj_dir, parts.PROJECT + ".kicad_pcb")
    dsn = os.path.join(proj_dir, parts.PROJECT + ".dsn")
    ses = os.path.join(proj_dir, parts.PROJECT + ".ses")
    board = pcbnew.LoadBoard(pcb)
    for t in list(board.GetTracks()):  # a fresh route: drop the old copper
        board.Remove(t)
    if not pcbnew.ExportSpecctraDSN(board, dsn):
        raise SystemExit("DSN export failed")
    inset_boundary(dsn, board.GetDesignSettings().m_CopperEdgeClearance / 1000.0)
    r = subprocess.run([CLI, dsn, ses] + list(args), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    sys.stdout.write("".join(l + "\n" for l in r.stdout.splitlines() if l.startswith(("Routing", "Done:", "Routed", "Redundancy", "Orphans", "[drc:"))))
    if r.returncode or not os.path.exists(ses):
        sys.stdout.write(r.stdout[-2000:])
        raise SystemExit("router failed")
    print("imported %d track segments, %d vias" % import_ses(board, ses))
    fill(board)
    pcbnew.SaveBoard(pcb, board)
    print("routed:", pcb)
    return 0
