#!/usr/bin/env python3
"""Check stman's schematic: what KiCad reads from it (kicad-cli netlist) is the design.

  python3 gen/check.py

1. **stmbl's circuits kept:** every net of the stmbl v4.0 LV board (its
   netlist, hw/kicad/v4.0/stmbl_4.0.net) among the parts stman keeps is still
   one net, unless a pin on it is one stman changes (CHANGED). Known quirks of
   that 2018 netlist are listed in KNOWN.
2. **stman's nets:** every pin of the new parts (newsheets.new()) is on the
   net it should be, together with the MCU pins (build.MCU) and the CMD
   port's isolator wiring (CMD_NETS); different nets stay different.
"""
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.dirname(HERE)
REPO = os.path.dirname(os.path.dirname(OUT))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, "hardware", "kigen"))
import build  # noqa: E402
import newsheets  # noqa: E402

# the CMD port: U200 ISO7331C between the MCU and the transceivers U8 (receive) and U9 (send) on the cable side
CMD_NETS = {
    "SS_TX": [("U200", "3"), ("U26", "23")], "SS_TXE": [("U200", "4"), ("U26", "93")],
    "SS_RX": [("U200", "5"), ("U26", "69")],
    "SS_TXD": [("U200", "14"), ("U9", "4")], "SS_TXDE": [("U200", "13"), ("U9", "2"), ("U9", "3"), ("R102", "1")],
    "SS_RXD": [("U200", "12"), ("U8", "1"), ("U8", "4")],
    "SS_5V": [("U200", "16"), ("U200", "10"), ("L7", "2"), ("JP200", "2")],
    "SS_VCC": [("L7", "1"), ("U8", "8"), ("U9", "8")],
    "SS_GND": [("U200", "9"), ("U200", "15"), ("U8", "5"), ("U9", "5"), ("JP201", "2")],
}
# reverse polarity and the FB port polyfuses (in series where stmbl had a wire)
SERIES = {
    "UI_IN": [("P3", "1"), ("D200", "2")], "+24V": [("D200", "1"), ("D22", "1"), ("C143", "1")],
    "FB0_PIN7": [("J2", "7"), ("F200", "1")], "FB0_SUP": [("F200", "2"), ("C114", "1"), ("JP2", "2")],
    "FB1_PIN7": [("J1", "7"), ("F201", "1")], "FB1_SUP": [("F201", "2"), ("C117", "1"), ("JP5", "2")],
}
# 2018 netlist quirks: a net there that is two in the drawing, or an op-amp's supply pin listed per unit
# stmbl nets stman splits on purpose: the CMD port's +5F and shield become the cable side's SS_5V and SS_SHIELD
SPLIT = {"+5F", "GNDD"}
KNOWN = {"/ctrl/f4/A_5V", "/ctrl/f4/A_24V", "Net-(C36-Pad2)", "Net-(C37-Pad2)", "Net-(U6-Pad4)", "Net-(U7-Pad4)",
         "Net-(U6-Pad8)", "Net-(U7-Pad8)", "GND"}


def netlist():
    with tempfile.TemporaryDirectory() as d:
        out = os.path.join(d, "n.net")
        subprocess.run(["kicad-cli", "sch", "export", "netlist", "-o", out, os.path.join(OUT, "stman.kicad_sch")],
                       check=True, stdout=subprocess.DEVNULL)
        t = open(out).read()
    nets = {}
    for blk in t.split("(net (code")[1:]:
        name = re.search(r'\(name "([^"]*)"\)', blk).group(1)
        nets[name] = set(re.findall(r'\(node \(ref "([^"]*)"\) \(pin "([^"]*)"', blk))
    return nets


def legacy_nets(keep):
    t = open(os.path.join(REPO, "hw", "kicad", "v4.0", "stmbl_4.0.net")).read()
    t = t[t.index("(nets"):]
    out = {}
    for blk in t.split("(net (code")[1:]:
        name = re.search(r'\(name ("[^"]*"|[^)\s]*)\)', blk).group(1).strip('"')
        s = {(r, p) for r, p in re.findall(r"\(node \(ref ([^)]*)\) \(pin ([^)]*)\)", blk) if keep(r)}
        if s:
            out[name] = s
    return out


def main():
    nets = netlist()
    where = {}
    for n, pins in nets.items():
        for x in pins:
            where[x] = n
    errs = []
    # ---- stman's nets
    want = {}
    for title, groups in newsheets.new().values():
        for gtitle, parts in groups:
            for ref, lib_id, value, fp, pins, note in parts:
                for pin, net in pins.items():
                    want.setdefault(net, set()).add((ref, pin))
    for pin, net in build.MCU.items():
        want.setdefault(net, set()).add(("U26", pin))
    for net, pins in list(CMD_NETS.items()) + list(SERIES.items()):
        want.setdefault(net, set()).update(pins)
    power = {"GND": "GND", "+3V3": "+3V3", "+5V": "+5V", "+12V": "+12V", "+24V": "+24V"}
    got_of = {}
    for net, pins in want.items():
        actual = {where.get(x) for x in pins}
        if None in actual:
            errs.append("%s: not on any net: %s" % (net, sorted(x for x in pins if x not in where)))
            continue
        if len(actual) > 1:
            errs.append("%s: split over %s" % (net, sorted(actual)))
            continue
        got_of[net] = actual.pop()
    inv = {}
    for net, a in got_of.items():
        inv.setdefault(a, []).append(net)
    for a, ns in inv.items():
        if len(ns) > 1:
            errs.append("joined: %s are one net (%s)" % (ns, a))
    # ---- stmbl's circuits kept
    kept = {}
    for r, p in where:
        kept.setdefault(r, set()).add(p)
    changed = {("U26", p) for p in build.MCU} | {x for pins in list(CMD_NETS.values()) + list(SERIES.values()) for x in pins}
    for net, pins in legacy_nets(lambda r: r in kept).items():
        if net in KNOWN or net in SPLIT:
            continue
        here = {x for x in pins if x in where}
        if not here or here & changed:
            continue
        actual = {where[x] for x in here}
        if len(actual) > 1:
            errs.append("stmbl net %s split: %s" % (net, sorted(actual)))
    print("%d nets, %d parts checked; %s" % (len(nets), len(kept), "%d problem(s)" % len(errs) if errs else "OK"))
    for e in errs:
        print("  " + e)
    return 1 if errs else 0


if __name__ == "__main__":
    sys.exit(main())
