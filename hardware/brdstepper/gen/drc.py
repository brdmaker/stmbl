"""Run KiCad's DRC on brdstepper.kicad_pcb (pcbnew's WriteDRCReport: KiCad 7's
kicad-cli has no DRC) and fail on anything but what a placed, unrouted board
has: unconnected items, and library warnings when KiCad's footprint libraries
are not configured (a headless install).

  python3 drc.py [REPORT]
"""
import collections
import os
import re
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
PCB = os.path.join(os.path.dirname(HERE), "brdstepper.kicad_pcb")
EXPECTED = {"unconnected_items", "lib_footprint_issues", "lib_footprint_mismatch"}


def main():
    rpt = sys.argv[1] if len(sys.argv) > 1 else "/tmp/brdstepper-drc.rpt"
    board = pcbnew.LoadBoard(PCB)
    pcbnew.WriteDRCReport(board, rpt, pcbnew.EDA_UNITS_MILLIMETRES, True)
    kinds = collections.Counter(re.findall(r"^\[(\w+)\]", open(rpt).read(), re.M))
    for k, n in sorted(kinds.items()):
        print("%-28s %4d%s" % (k, n, "" if k in EXPECTED else "  <-- not expected"))
    bad = sum(n for k, n in kinds.items() if k not in EXPECTED)
    print("report:", rpt)
    print("OK (only unrouted connections)" if not bad else "%d violations" % bad)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
