"""Run KiCad's DRC on a board (pcbnew's WriteDRCReport: KiCad 7's kicad-cli
has no DRC) and fail on anything but what a placed, unrouted board has:
unconnected items, library warnings when KiCad's footprint libraries are not
configured (a headless install), and silkscreen overlaps (warnings).

  kigen.py drc <board dir>
"""
import collections
import re

import pcbnew

# unrouted; KiCad's libraries not configured (headless); footprint outlines
# touching on the silkscreen (a warning, cleaned up in the layout)
EXPECTED = {"unconnected_items", "lib_footprint_issues", "lib_footprint_mismatch", "silk_overlap"}


def run(pcb, rpt):
    board = pcbnew.LoadBoard(pcb)
    pcbnew.WriteDRCReport(board, rpt, pcbnew.EDA_UNITS_MILLIMETRES, True)
    kinds = collections.Counter(re.findall(r"^\[(\w+)\]", open(rpt).read(), re.M))
    for k, n in sorted(kinds.items()):
        print("%-28s %4d%s" % (k, n, "" if k in EXPECTED else "  <-- not expected"))
    bad = sum(n for k, n in kinds.items() if k not in EXPECTED)
    print("report:", rpt)
    print("OK (only unrouted connections)" if not bad else "%d violations" % bad)
    return 1 if bad else 0

