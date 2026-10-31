#!/usr/bin/env python3
"""How fast can the portal run the benchmark path and keep a given
accuracy? From bench.py's results (one or more results.json):

    pareto.py results.json [more.json ..] [--tol 25,50,100,200,500]

For each tolerance [um] it picks the fastest finished run (program time)
whose path error (the true tool point against the program, while cutting)
stays within it: by the 99th percentile, and by the maximum. It prints the
run's acceleration and feed, and the cutting feed reached. It also prints
the trade-off curve: per acceleration, the shortest time and its error.
"""
import argparse
import json


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("results", nargs="+")
    ap.add_argument("--tol", default="25,50,100,200,500")
    a = ap.parse_args()
    rows = []
    for f in a.results:
        for r in json.load(open(f)):
            r["src"] = f
            rows.append(r)
    done = [r for r in rows if r.get("status") == "done" and "time" in r]
    print("| tolerance [um] | by | fastest run: accel [g], F [m/min] | time [s] | cutting feed reached [m/min] | p99 / max [um] |")
    print("|---|---|---|---|---|---|")
    for tol in [float(t) for t in a.tol.split(",")]:
        for key, label in (("path_p99", "p99"), ("path_max", "max")):
            ok = [r for r in done if r[key] <= tol]
            if not ok:
                print("| %g | %s | none | | | |" % (tol, label))
                continue
            b = min(ok, key=lambda r: r["time"])
            cut = b["feed"] / 1000 * b["feed_pct"] / 100
            print("| %g | %s | %.2f g, %g | %.1f | %.1f | %.0f / %.0f |" % (
                tol, label, b["accel"] / 9810, b["feed"] / 1000, b["time"], cut, b["path_p99"], b["path_max"]))
    print()
    print("| accel [g] | shortest time [s] at F [m/min] | p99 / max [um] | aborted |")
    print("|---|---|---|---|")
    for acc in sorted({r["accel"] for r in rows}):
        rs = [r for r in done if r["accel"] == acc]
        ab = sum(1 for r in rows if r["accel"] == acc and r.get("status") != "done")
        if rs:
            b = min(rs, key=lambda r: r["time"])
            print("| %.2f | %.1f at %g | %.0f / %.0f | %d |" % (acc / 9810, b["time"], b["feed"] / 1000, b["path_p99"], b["path_max"], ab))
        else:
            print("| %.2f | - | - | %d |" % (acc / 9810, ab))


if __name__ == "__main__":
    main()
