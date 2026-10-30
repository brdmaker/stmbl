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

CLI = os.environ.get("BRDROUTER", os.path.expanduser("~/BrdRoute/build/brdrouter_cli"))


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
    r = subprocess.run([CLI, dsn, ses] + list(args), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    sys.stdout.write("".join(l + "\n" for l in r.stdout.splitlines() if l.startswith(("Routing", "Done:", "Routed", "Redundancy", "Orphans", "[drc:"))))
    if r.returncode or not os.path.exists(ses):
        sys.stdout.write(r.stdout[-2000:])
        raise SystemExit("router failed")
    if not pcbnew.ImportSpecctraSES(board, ses):
        raise SystemExit("SES import failed")
    fill(board)
    pcbnew.SaveBoard(pcb, board)
    print("routed:", pcb)
    return 0
