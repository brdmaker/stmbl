#!/usr/bin/env python3
"""Sample runs on the simulated Scripta CN 720 (mill_sim): the same moves
on the resolvers or 10 um scales, at a 1 or 2 kHz servo thread.

  python3 runs.py [--only NAME,...] [--variants a,b] [-j N] [--md FILE]

Prints one table per scenario (values in um unless named otherwise), and
with --md writes them as markdown. Each run is a mill_sim script; see
SCENARIOS for what they do and which values they report.
"""
import argparse
import concurrent.futures as cf
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

VARIANTS = {  # name: (machine file, servo ms, extra script lines)
    "resolver 1k": ("machine/cn720.mill", 1, ""),
    "resolver 2k": ("machine/cn720.mill", 0.5, ""),
    "scales 1k": ("machine/cn720_scales.mill", 1, ""),
    "scales 2k": ("machine/cn720_scales.mill", 0.5, ""),
    "blend 1k": ("machine/cn720_blend.mill", 1, ""),
    "blend 2k": ("machine/cn720_blend.mill", 0.5, ""),
}

START = """lcnc 1
run 0.3
move {t0} {x0} {y0} -20
run 0.3
"""  # the quintic's peak speed is 1.875 x the mean: t0 keeps it under 15 m/min

# name: (title, script after START, values to print, start x y)
SCENARIOS = {
    "hold": ("Standing still, 1 s", "mark\nrun 1\n", "rms_etool max_etool rms_ez max_ez", (350, 215)),
    "circle10_3": ("Circle R 10 mm at 3 m/min (0.03 g), 2nd and 3rd turn",
                   "circle 1.257 350 215 10 1\nmark\ncircle 2.513 350 215 10 2\n", "max_econt rms_econt max_yb max_hx", (360, 215)),
    "circle10_6": ("Circle R 10 mm at 6 m/min (0.1 g)",
                   "circle 0.628 350 215 10 1\nmark\ncircle 1.257 350 215 10 2\n", "max_econt rms_econt max_yb max_hx", (360, 215)),
    "circle10_12": ("Circle R 10 mm at 12 m/min (0.4 g)",
                    "circle 0.314 350 215 10 1\nmark\ncircle 0.628 350 215 10 2\n", "max_econt rms_econt max_yb max_hx", (360, 215)),
    "circle2_3": ("Circle R 2 mm at 3 m/min (0.35 g): a small pocket",
                  "circle 0.251 350 215 2 1\nmark\ncircle 0.503 350 215 2 2\n", "max_econt rms_econt max_yb max_hx", (352, 215)),
    "square": ("20 mm square, exact stop at 1 g per side (15.5 m/min peak)",
               "mark\nsquare 0.103 350 215 20\nrun 0.1\n", "max_econt rms_econt max_yb max_hx", (340, 205)),
    "square_05g": ("20 mm square, exact stop at 0.5 g (11 m/min peak)",
                   "mark\nsquare 0.146 350 215 20\nrun 0.1\n", "max_econt rms_econt max_yb max_hx", (340, 205)),
    "square_s": ("20 mm square, exact stop, S-curve sides in the same time (1.1 g peak, jerk-limited)",
                 "mark\nsquare 0.103 350 215 20 1\nrun 0.1\n", "max_econt rms_econt max_yb max_hx", (340, 205)),
    "line": ("X line 300 mm at 3 m/min (feed), 0.25 g ramps",
             "mark\ntmove 6.67 500 215 -20\n", "rms_etool rms_econt max_econt rms_ferr_x", (200, 215)),
    "rapid_y": ("Y rapid 300 mm at 27 m/min, 1 g ramps (0.71 s), then where the tool is after 10 / 30 / 100 ms",
                "trapfrac 0.0634\nmark\ntmove 0.712 350 380 -20\nprint etool\nrun 0.01\nprint etool\nrun 0.02\nprint etool\nrun 0.07\nprint etool\n",
                "max_yb max_econt", (350, 80)),
    "lead": ("X 600 mm with a 5 K warm screw (57 um/m too long), the error at the end",
             "lead x 57.5\ntrapfrac 0.1\nmark\ntmove 3 650 215 -20\nrun 0.3\n", "etool tx cx", (50, 215)),
    "cut": ("Steady 20 N cutting force on the tool in X and Y (thin chips)",
            "cut 20 20 0\nrun 0.5\nmark\nrun 0.5\n", "etool max_hx max_hy", (350, 215)),
}


def run_one(variant, scen):
    mfile, servo, extra = VARIANTS[variant]
    title, body, values, (x0, y0) = SCENARIOS[scen]
    freq = int(round(1000 / servo))
    t0 = max(0.4, 1.875 * ((x0 - 350) ** 2 + (y0 - 215) ** 2) ** 0.5 / 250.0)
    script = "servo %g\ndrv all vel_int0.cmd_freq = %d\n%s" % (servo, freq, extra) + START.format(t0=t0, x0=x0, y0=y0) + body + "print fault %s\n" % values
    p = subprocess.run([os.path.join(HERE, "mill_sim"), os.path.join(HERE, mfile)], input=script, capture_output=True, text=True, cwd=HERE, timeout=600)
    out = {}
    seq = []
    for line in p.stdout.splitlines():
        m = re.match(r"mill\.(\w+) = (\S+)", line)
        if m:
            out[m.group(1)] = float(m.group(2))
            seq.append((m.group(1), float(m.group(2))))
    if "etool" in values.split() or scen == "rapid_y":
        out["_seq"] = [v for k, v in seq if k == "etool"]
    if p.returncode or "ERROR" in p.stdout:
        out["_err"] = (p.stdout + p.stderr)[-400:]
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only")
    ap.add_argument("--variants")
    ap.add_argument("-j", type=int, default=os.cpu_count() or 2)
    ap.add_argument("--md")
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
        title, _, values, _ = SCENARIOS[s]
        cols = values.split()
        if s == "rapid_y":
            cols = ["etool@0", "etool@10ms", "etool@30ms", "etool@100ms"] + cols
        print("\n## %s: %s" % (s, title))
        md.append("\n**%s**\n\n| variant | %s |\n|---|%s\n" % (title, " | ".join(cols), "---|" * len(cols)))
        print("%-17s" % "variant" + "".join("%14s" % c for c in cols))
        for v in vars_:
            r = jobs[(s, v)].result()
            if "_err" in r or r.get("fault"):
                print("%-17s FAILED %s" % (v, r.get("_err", "fault")))
                md.append("| %s | failed |\n" % v)
                continue
            row = []
            for c in cols:
                if c.startswith("etool@"):
                    seqv = r.get("_seq", [])
                    i = ["etool@0", "etool@10ms", "etool@30ms", "etool@100ms"].index(c)
                    row.append(seqv[i] if i < len(seqv) else float("nan"))
                else:
                    row.append(r.get(c, float("nan")))
            print("%-17s" % v + "".join("%14.4g" % x for x in row))
            md.append("| %s | %s |\n" % (v, " | ".join("%.3g" % x for x in row)))
    if a.md:
        with open(a.md, "w") as f:
            f.write("".join(md))
    return 0


if __name__ == "__main__":
    sys.exit(main())
