#!/usr/bin/env python3
"""Sample runs on the simulated Mikron HSM 400U retrofit (mill_sim,
machine/hsm400u*.mill): its Siemens servos with EnDat on stmbl drives, the
glass scales, the A/C trunnion through xyzac-trt-kins.

  python3 hsm_runs.py [--only NAME,...] [--variants a,b] [-j N] [--md FILE] [--no-sweep]

Prints one table per scenario (um, arcsec, A, unless named otherwise) and the
rapid sweep (the fastest X rapid each bus allows); with --md writes them as
markdown. Every number rests on GUESSED machine data (machine/hsm400u/):
the comparisons between variants mean more than the absolute values.
"""
import argparse
import concurrent.futures as cf
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

VARIANTS = {  # name: (machine file, servo ms)
    "EnDat 1k": ("machine/hsm400u.mill", 1),
    "EnDat 2k": ("machine/hsm400u.mill", 0.5),
    "scales 1k": ("machine/hsm400u_scales.mill", 1),
    "scales 2k": ("machine/hsm400u_scales.mill", 0.5),
    "blend 1k": ("machine/hsm400u_blend.mill", 1),
    "EnDat 380V 1k": ("machine/hsm400u_380v.mill", 1),
}

# every scenario starts here: enabled, then a slow move to its start pose (X Y Z A C)
START = "lcnc 1\nrun 0.3\nmove 2.5 {0} {1} {2} {3} {4}\nrun 0.4\n"

# name: (title, start pose, script, values printed)
SCENARIOS = {
    "hold": ("Standing still, 1 s (the tip in the work)", (200, 225, -100, 0, 0),
             "mark\nrun 1\n", "rms_ework max_ework rms_etool"),
    "rapid_x": ("X rapid 380 mm at 25 m/min, 0.1 trapezoid ramps (~0.7 g)", (10, 225, -100, 0, 0),
                "trapfrac 0.1\nmark\ntmove 1.013 390 225 -100 0 0\nrun 0.2\n", "max_ferr_x max_cur_x max_econt etool"),
    "circle25_5": ("XY circle R 25 mm at 5 m/min, 2nd and 3rd turn", (225, 225, -100, 0, 0),
                   "circle 1.885 200 225 25 1\nmark\ncircle 3.770 200 225 25 2\n", "max_econt rms_econt max_hx max_hy"),
    "circle10_10": ("XY circle R 10 mm at 10 m/min (0.28 g)", (210, 225, -100, 0, 0),
                    "circle 0.377 200 225 10 1\nmark\ncircle 0.754 200 225 10 2\n", "max_econt rms_econt max_hx max_hy"),
    "square": ("20 mm square, exact stop at 0.5 g per side", (190, 215, -100, 0, 0),
               "mark\nsquare 0.146 200 225 20\nrun 0.1\n", "max_econt rms_econt max_hx max_hy"),
    "warm": ("X 300 mm with a 5 K warm screw (57.5 um/m), the error at the end", (50, 225, -100, 0, 0),
             "lead x 57.5\ntrapfrac 0.1\nmark\ntmove 3 350 225 -100 0 0\nrun 0.5\n", "etool tx cx"),
    "rtcp": ("Tool centre point: A 0 -> 30, C 0 -> 90 in 3 s, the tip 30 / 20 mm off the axes (in the work)", (230, 245, -150, 0, 0),
             "mark\nrtcp 3 30 90\nrun 0.3\n", "max_ework max_econtw ework max_ferr_a max_ferr_c"),
    "wline": ("Five-axis line: the tip 40 mm along the work's X while A 0 -> 20, C 0 -> 60, 2 s", (210, 225, -150, 0, 0),
              "mark\nwmove 2 250 225 -150 20 60\nrun 0.3\n", "max_ework max_econtw ework"),
    "a90": ("A to 90 deg (the unbalance at its largest), held 1 s: the tip 100 mm from the A axis", (200, 225, -150, 0, 0),
            "move 4 200 225 -150 90 0\nrun 0.5\nmark\nrun 1\n", "max_ferr_a max_cur_a max_ework"),
    "cut": ("Steady 100 N cutting force on the tool in X and Y", (200, 225, -100, 0, 0),
            "cut 100 100 0\nrun 0.5\nmark\nrun 0.5\n", "etool max_hx max_hy"),
}


def run_script(mfile, servo, script):
    freq = int(round(1000 / servo))
    script = "servo %g\ndrv all vel_int0.cmd_freq = %d\n" % (servo, freq) + script
    p = subprocess.run([os.path.join(HERE, "mill_sim"), os.path.join(HERE, mfile)], input=script, capture_output=True, text=True,
                       cwd=HERE, timeout=900)
    out = {}
    for line in p.stdout.splitlines():
        m = re.match(r"mill\.(\w+) = (\S+)", line)
        if m:
            out[m.group(1)] = float(m.group(2))
    if p.returncode or "ERROR" in p.stdout:
        out["_err"] = (p.stdout + p.stderr)[-400:]
    return out


def run_one(variant, scen):
    mfile, servo = VARIANTS[variant]
    _, pose, body, values = SCENARIOS[scen]
    return run_script(mfile, servo, START.format(*pose) + body + "print fault %s\n" % values)


def rapid_ok(mfile, v):
    """X 380 mm at v m/min (0.1 trapezoid): (no fault, max following error um)"""
    T = 380.0 / (v * 1000 / 60) / 0.9
    r = run_script(mfile, 1, START.format(10, 225, -100, 0, 0) + "trapfrac 0.1\nmark\ntmove %.4f 390 225 -100 0 0\nrun 0.2\nprint fault max_ferr_x\n" % T)
    return not r.get("fault", 1) and "_err" not in r, r.get("max_ferr_x", float("nan"))


def sweep(md):
    """the fastest X rapid without a fault (or 100 um of following error), per bus"""
    print("\n## sweep: the fastest X rapid (380 mm, 0.1 trapezoid ramps)")
    md.append("\n**The fastest X rapid (380 mm, 0.1 trapezoid ramps): no fault, following error under 100 um**\n\n| bus | m/min | its following error [um] | the next step |\n|---|---|---|---|\n")
    speeds = [20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40]
    for name, mfile in (("~325 V (230 V mains rectified)", "machine/hsm400u.mill"), ("380 V (stiff)", "machine/hsm400u_380v.mill")):
        with cf.ThreadPoolExecutor(len(speeds)) as ex:
            res = dict(zip(speeds, ex.map(lambda v: rapid_ok(mfile, v), speeds)))
        best = None
        for v in speeds:
            ok, fe = res[v]
            if ok and fe < 100:
                best = v
            else:
                break
        nxt = speeds[speeds.index(best) + 1] if best is not None and best != speeds[-1] else None
        nx = ("%d m/min: %s" % (nxt, "fault" if not res[nxt][0] else "%.0f um" % res[nxt][1])) if nxt else "-"
        print("%-32s %s m/min (%.1f um)   next %s" % (name, best, res[best][1] if best else float("nan"), nx))
        md.append("| %s | %s | %.1f | %s |\n" % (name, best, res[best][1] if best else float("nan"), nx))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only")
    ap.add_argument("--variants")
    ap.add_argument("-j", type=int, default=os.cpu_count() or 2)
    ap.add_argument("--md")
    ap.add_argument("--no-sweep", action="store_true")
    a = ap.parse_args()
    scens = a.only.split(",") if a.only else list(SCENARIOS)
    vars_ = a.variants.split(",") if a.variants else list(VARIANTS)
    jobs = {}
    with cf.ThreadPoolExecutor(a.j) as ex:
        for s in scens:
            for v in vars_:
                jobs[(s, v)] = ex.submit(run_one, v, s)
    md = []
    for s in scens:
        title, _, _, values = SCENARIOS[s]
        cols = values.split()
        print("\n## %s: %s" % (s, title))
        md.append("\n**%s**\n\n| variant | %s |\n|---|%s\n" % (title, " | ".join(cols), "---|" * len(cols)))
        print("%-16s" % "variant" + "".join("%14s" % c for c in cols))
        for v in vars_:
            r = jobs[(s, v)].result()
            if "_err" in r or r.get("fault"):
                print("%-16s FAILED %s" % (v, r.get("_err", "fault")))
                md.append("| %s | failed (fault) |\n" % v)
                continue
            row = [r.get(c, float("nan")) for c in cols]
            print("%-16s" % v + "".join("%14.4g" % x for x in row))
            md.append("| %s | %s |\n" % (v, " | ".join("%.3g" % x for x in row)))
    if not a.no_sweep and not a.only:
        sweep(md)
    if a.md:
        with open(a.md, "w") as f:
            f.write("".join(md))
    return 0


if __name__ == "__main__":
    sys.exit(main())
