#!/usr/bin/env python3
"""kigen: KiCad 7 projects made from a parts list (hardware/<board>/gen/).

  kigen.py sch    <board dir>   symbols, project, schematic (gen/parts.py)
  kigen.py check  <board dir>   the schematic's netlist against parts.py
  kigen.py pcb    <board dir>   the board, placed (gen/board.py)
  kigen.py route  <board dir>   autoroute with BrdRoute (route.py), import the result
  kigen.py drc    <board dir>   KiCad's DRC: only unrouted connections allowed
  kigen.py bom    <board dir>   <name>-bom.csv
  kigen.py doc    <board dir>   doc/<name>-schematic.pdf, doc/<name>-placement.png

A board directory has gen/parts.py (PROJECT, SHEETS, ... see sch.py) and,
for pcb, gen/board.py (see place.py). Used by hardware/brdstepper and
hardware/vibsensor.
"""
import importlib.util
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)


def load(board_dir, name):
    path = os.path.join(board_dir, "gen", name + ".py")
    spec = importlib.util.spec_from_file_location(os.path.basename(board_dir) + "_" + name, path)
    mod = importlib.util.module_from_spec(spec)
    sys.path.insert(0, os.path.dirname(path))
    spec.loader.exec_module(mod)
    return mod


def main():
    if len(sys.argv) < 3 or (len(sys.argv) > 3 and sys.argv[1] != "route"):
        raise SystemExit(__doc__)
    cmd, d = sys.argv[1], os.path.abspath(sys.argv[2])
    parts = load(d, "parts")
    if cmd == "sch":
        import sch
        sch.run(d, parts)
    elif cmd == "check":
        import check
        return check.run(d, parts)
    elif cmd == "pcb":
        import place
        return place.run(d, parts, load(d, "board"))
    elif cmd == "route":
        import route
        return route.run(d, parts, sys.argv[3:])
    elif cmd == "drc":
        import drc
        return drc.run(os.path.join(d, parts.PROJECT + ".kicad_pcb"), "/tmp/%s-drc.rpt" % parts.PROJECT)
    elif cmd == "bom":
        import bom
        bom.run(d, parts)
    elif cmd == "doc":
        os.makedirs(os.path.join(d, "doc"), exist_ok=True)
        subprocess.run(["kicad-cli", "sch", "export", "pdf", "-o", os.path.join(d, "doc", parts.PROJECT + "-schematic.pdf"),
                        os.path.join(d, parts.PROJECT + ".kicad_sch")], check=True, stdout=subprocess.DEVNULL)
        subprocess.run([os.path.join(HERE, "render.sh"), os.path.join(d, parts.PROJECT + ".kicad_pcb"),
                        os.path.join(d, "doc", parts.PROJECT + "-placement.png")], check=True)
        print("wrote", os.path.join(d, "doc"))
    else:
        raise SystemExit(__doc__)
    return 0


if __name__ == "__main__":
    sys.exit(main())
