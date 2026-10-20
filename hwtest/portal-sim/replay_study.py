#!/usr/bin/env python3
"""What does physics-based command conditioning buy? Replays one LinuxCNC
recording of the benchmark (a bench.py run's .samples) through portal_sim
(host/portal) with and without pcomp's input shaper and head deflection
feedforward, and scores the tool point against the programmed path as
bench.py does.

    replay_study.py SAMPLES [--geom path_geom.json] [--portal FILE] [--variants 0,5]

The servo variant's tau2 (2.15e-5 s^2) is calibrated on a circle: R20 at
0.5 m/s with the rest of the conditioning on, until the tool's mean radius
is 20 mm (host/portal/tests/portal_pcomp.test).

The planner's commands are the recorded ones, so the cycle time is the
same as LinuxCNC's (the shaper adds its delay, ~30 ms, once).
"""
import argparse
import json
import os
import subprocess
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import bench  # noqa: E402

SIM = os.path.join(ROOT, "host", "portal", "portal_sim")
PORTAL = os.path.join(ROOT, "host", "portal", "machine", "portal.portal")

VARIANTS = [
    ("as recorded (no conditioning)", []),
    ("ZV shaper", ["comp shaper 1"]),
    ("ZV + jerk filter 10 ms", ["comp shaper 1", "comp jerk 10"]),
    ("ZV + jerk 10 ms + shrink comp", ["comp shaper 1", "comp jerk 10", "comp shrink 1"]),
    ("ZV + jerk 10 ms + shrink + deflection ff", ["comp shaper 1", "comp jerk 10", "comp shrink 1", "comp defl 1"]),
    ("ZV + jerk 20 ms + shrink + deflection ff", ["comp shaper 1", "comp jerk 20", "comp shrink 1", "comp defl 1"]),
    ("ZV + jerk 20 ms + shrink + deflection ff + servo lag", ["comp shaper 1", "comp jerk 20", "comp shrink 1", "comp defl 1",
                                                             "comp servo 2.15e-5"]),
    ("as above (10 ms), model 10 % stiff", ["comp model %g %g %g %g", "comp shaper 1", "comp jerk 10", "comp shrink 1", "comp defl 1"]),
]


def run(samples, lines, portal, log):
    script = "\n".join(["run 0.05", "lcnc 1", "run 0.2"] + lines + ["log " + log, "replay " + samples, "log off",
                        "print max_rack max_hx max_hy max_cur_y1 max_cur_x fault comp_delay"]) + "\n"
    out = subprocess.run([SIM, portal], input=script, capture_output=True, text=True).stdout
    vals = {}
    for l in out.splitlines():
        if l.startswith("portal."):
            k, v = l[7:].split(" = ")
            vals[k] = float(v)
    return vals


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("samples")
    ap.add_argument("--geom", default=None)
    ap.add_argument("--portal", default=PORTAL)
    ap.add_argument("--variants", default=None, help="indices into VARIANTS (default: all)")
    a = ap.parse_args()
    geom_file = a.geom or os.path.join(os.path.dirname(a.samples), "path_geom.json")
    if not os.path.exists(geom_file):
        geom_file = "/tmp/path_geom.json"
        subprocess.run([sys.executable, os.path.join(HERE, "gen_path.py"), "1000", "/tmp/path_geom.ngc", geom_file],
                       check=True, stdout=subprocess.DEVNULL)
    geom = json.load(open(geom_file))
    # the recording's motion types: which samples were cutting
    rows = [l.split() for l in open(a.samples).read().splitlines()]
    mt = np.array([int(float(r[-1])) for r in rows if len(r) == len(bench.COLS) + 1])
    first, last = np.nonzero(mt)[0][[0, -1]]
    first, last = max(first - 200, 0), min(last + 500, len(mt) - 1)
    cut_rec = (mt[first:last + 1] == 2) | (mt[first:last + 1] == 3)
    # the model's frequencies (for the "wrong model" variant): the plant's, 10 % high
    P = {}
    for l in open(a.portal):
        t = l.split("#")[0].split()
        if t and t[0] == "head":
            P = dict(fx=float(t[3]), fye=float(t[4]), fym=float(t[5]), z=float(t[6]))
    print("| variant | path p99 / max [um] | rack [urad] | head x / y [um] | peak I Y1 / X [A] | delay [ms] | fault |")
    print("|---|---|---|---|---|---|---|")
    pick = [VARIANTS[int(i)] for i in a.variants.split(",")] if a.variants else VARIANTS
    for name, lines in pick:
        lines = [l % (P["fx"] * 1.1, P["fye"] * 1.1, P["fym"] * 1.1, P["z"]) if "%" in l else l for l in lines]
        log = "/tmp/replay_study.log"
        v = run(os.path.abspath(a.samples), lines, a.portal, log)
        lg = np.loadtxt(log, ndmin=2)
        lg = lg[-len(cut_rec):]  # the replay itself (after the move to its start)
        tool = lg[:, 3:5]
        # the conditioning delays the motion: the cutting (laser, spindle)
        # is delayed as much, so the recording's cutting samples shift too
        shift = int(round(v.get("comp_delay", 0)))
        cut = np.roll(cut_rec, shift)
        cut[:shift] = False
        pe, _ = bench.path_error(tool[cut], geom)
        pe *= 1000
        print("| %s | %.0f / %.0f | %.0f | %.0f / %.0f | %.1f / %.1f | %.0f | %d |" % (
            name, np.percentile(pe, 99), pe.max(), v.get("max_rack", 0), v.get("max_hx", 0), v.get("max_hy", 0),
            v.get("max_cur_y1", 0), v.get("max_cur_x", 0), shift, v.get("fault", 0)), flush=True)


if __name__ == "__main__":
    main()
