#!/usr/bin/env python3
"""The test stand's collision check: every moving group against the others at the travel's corners,
and the parts of a group against each other. Prints what touches (its box); exit 1 if anything does."""
import itertools, os, re, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
GROUPS = [("Y", "F"), ("X", "F"), ("S", "F"), ("X", "Y"), ("S", "Y"), ("S", "X")]
INSIDE = [("base", "beam"), ("beam", "xbr"), ("xbr", "xmot"), ("base", "ymot"), ("beam", "xsw"), ("base", "ysw"), ("xbr", "xsw"),
          ("car", "gibx"), ("car", "gibz"), ("car", "zbr"), ("zbr", "zmot"), ("car", "zmot"), ("car", "zsw"), ("zbr", "zsw"),
          ("table", "giby"), ("beam", "gibx"), ("base", "giby")]
POS = [(x, y, z) for x in (-35, 35) for y in (-40, 40) for z in (0, -40)] + [(0, 0, -20)]

def run(a, b, x=0, y=0, z=0):
    with tempfile.TemporaryDirectory() as d:
        out = os.path.join(d, "i.stl")
        p = subprocess.run(["openscad", "-o", out, "-D", 'A="%s"' % a, "-D", 'B="%s"' % b, "-D", "X=%g" % x, "-D", "Y=%g" % y, "-D", "Z=%g" % z,
                            os.path.join(HERE, "check.scad")], capture_output=True, text=True, timeout=1800)
        if "top level object is empty" in p.stderr or not os.path.exists(out):
            return None
        xs, ys, zs = [], [], []
        for m in re.finditer(r"vertex\s+(\S+)\s+(\S+)\s+(\S+)", open(out).read()):
            xs.append(float(m.group(1))); ys.append(float(m.group(2))); zs.append(float(m.group(3)))
        if not xs:
            return None
        box = [(min(v), max(v)) for v in (xs, ys, zs)]
        return None if min(hi - lo for lo, hi in box) < 0.05 else box   # touching faces only: no volume

jobs = [(a, b, 0, 0, 0) for a, b in INSIDE] + [(a, b) + p for a, b in GROUPS for p in POS]
bad = 0
with ThreadPoolExecutor(os.cpu_count() or 4) as ex:
    for j, r in zip(jobs, ex.map(lambda j: run(*j), jobs)):
        if r:
            bad += 1
            print("TOUCH %s/%s at X%g Y%g Z%g: x %.1f..%.1f y %.1f..%.1f z %.1f..%.1f" % (j + tuple(v for lo_hi in r for v in lo_hi)), flush=True)
print("%d checks, %d touch" % (len(jobs), bad))
sys.exit(1 if bad else 0)
