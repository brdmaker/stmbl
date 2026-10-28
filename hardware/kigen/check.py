"""Check the generated schematic: what KiCad reads from it is what parts.py says.

KiCad 7's kicad-cli has no ERC, so this does the checks that matter here on the
netlist kicad-cli exports:
  - every net has exactly the pins parts.py gives it (labels and wires join
    what they should, and nothing else);
  - every pin parts.py leaves out is on no net (it carries a no-connect flag);
  - no net of one pin;
  - pin types: a net with a power input has a power output or a PWR_FLAG; no
    two power outputs, no two outputs, no output on a power output.

  kigen.py check <board dir>
"""
import os
import subprocess
import sys
import tempfile

from sexp import parse, find, findall

PROJ, parts = None, None  # set by run()


def netlist():
    with tempfile.TemporaryDirectory() as d:
        out = os.path.join(d, "n.net")
        subprocess.run(["kicad-cli", "sch", "export", "netlist", "-o", out, os.path.join(PROJ, parts.PROJECT + ".kicad_sch")],
                       check=True, stdout=subprocess.DEVNULL)
        return open(out).read()


def pwr_flags():
    """The nets a PWR_FLAG sits on (netlists leave power symbols out): the
    global label at the end of each flag's pin stub."""
    sch = parse(open(os.path.join(PROJ, parts.PROJECT + ".kicad_sch")).read())
    labels = {(round(float(find(l, "at")[1]), 2), round(float(find(l, "at")[2]), 2)): l[1] for l in findall(sch, "label")}
    out = []
    for s in findall(sch, "symbol"):
        if find(s, "lib_id")[1] == "power:PWR_FLAG":
            at = find(s, "at")
            out.append(labels.get((round(float(at[1]), 2), round(float(at[2]) + 2.54, 2))))
    return out


def run(proj_dir, parts_mod):
    global PROJ, parts
    PROJ, parts = proj_dir, parts_mod
    nl = parse(netlist())
    want = {}
    nc = set()
    refs = set()
    for plist in parts.SHEETS.values():
        for ref, lib_id, value, fp, nets, note in plist:
            refs.add(ref)
            for pin, net in nets.items():
                want.setdefault(net, set()).add((ref, pin))
    flagged = pwr_flags()
    if set(flagged) != set(parts.PWR_FLAGS):
        errs0 = ["PWR_FLAGs on %s, parts.py says %s" % (sorted(flagged), parts.PWR_FLAGS)]
    else:
        errs0 = []
    got, types = {}, {}
    for n in findall(find(nl, "nets"), "net"):
        name = find(n, "name")[1].lstrip("/")
        nodes = findall(n, "node")
        got[name] = {(find(x, "ref")[1], find(x, "pin")[1]) for x in nodes}
        types[name] = [(find(x, "ref")[1], find(x, "pin")[1], find(x, "pintype")[1]) for x in nodes]
    comps = {find(c, "ref")[1] for c in findall(find(nl, "components"), "comp")}
    errs = errs0
    if comps != refs:
        errs.append("parts: missing %s, extra %s" % (sorted(refs - comps), sorted(comps - refs)))
    for name, pins in want.items():
        if got.get(name) != pins:
            errs.append("net %s: want %s, got %s" % (name, sorted(pins), sorted(got.get(name, ()))))
    for name, pins in got.items():
        if name.startswith("unconnected-"):
            continue  # a pin with a no-connect flag, alone on its net
        if name not in want:
            errs.append("net %s is not in parts.py: %s" % (name, sorted(pins)))
        if len(pins) < 2:
            errs.append("net %s has one pin" % name)
    for name, ts in types.items():
        if name.startswith("unconnected-"):
            continue
        kinds = [t.split("+")[0] for _, _, t in ts]
        drivers = [k for k in kinds if k == "power_out"] + (["flag"] if name in flagged else [])
        if "power_in" in kinds and not drivers:
            errs.append("net %s: power input with no power output or PWR_FLAG" % name)
        if len(drivers) > 1 and not all(r.startswith("#FLG") for r, _, t in ts if t == "power_out"):
            real = [(r, p) for r, p, t in ts if t == "power_out" and not r.startswith("#FLG")]
            if len(real) > 1:
                errs.append("net %s: power outputs %s" % (name, real))
        outs = [(r, p) for r, p, t in ts if t.split("+")[0] == "output"]
        if len(outs) > 1:
            errs.append("net %s: outputs %s" % (name, outs))
        if outs and [1 for r, p, t in ts if t == "power_out" and not r.startswith("#FLG")]:
            errs.append("net %s: an output on a power output" % name)
        flags = [1] if name in flagged else []
        real = [(r, p) for r, p, t in ts if t == "power_out" and not r.startswith("#FLG")]
        if flags and real:
            errs.append("net %s: PWR_FLAG and a power output %s" % (name, real))
    nets = [n for n in got if not n.startswith("unconnected-")]
    print("%d parts, %d nets, %d unconnected pins (no-connect)" % (len(comps), len(nets), len(got) - len(nets)))
    for e in errs:
        print("ERROR", e)
    print("OK" if not errs else "%d errors" % len(errs))
    return 1 if errs else 0

