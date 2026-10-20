#!/usr/bin/env python3
"""Speed sweep of the simulated portal under LinuxCNC: how fast does the
benchmark path (gen_path.py) run, and what breaks first?

    bench.py [--accel A,A..] [--feeds F,F..] [--vmax V] [--out DIR] [--quick]
             [--portal FILE]   another machine (default host/portal/machine/portal.portal)
    bench.py --reanalyse DIR..   score finished runs again (their /tmp dirs)

For each acceleration limit A [mm/s^2] (the axes' and joints' MAX_ACCELERATION,
TRAJ's too) a copy of this config in /tmp runs the path at each feed F
[mm/min] (portal_run.py records every servo cycle). Per run:

  time       program time and cutting time [s]; feed: the cutting feed
             reached (cut length / cutting time) as % of F
  path       the true tool point (relative to the frame, i.e. the work)
             against the programmed geometry, while cutting: max and 99th
             percentile [um] (G64 P0.02 allows 20 um of blending)
  servo      the tool point against LinuxCNC's own commanded path: what the
             drives and the mechanics add [um]
  ferror     LinuxCNC's largest joint following error [mm]
  I/Imax     the largest motor current over the drive's limit; sat: time
             share above 97 % of it
  rack       the bridge's largest yaw [urad]; frame: the frame's largest
             motion on its feet [um]; feet: the largest force on the
             floor [N]; head: the head's largest deflection [um]
  bus        the DC bus' lowest / highest voltage [V]

Results: DIR/results.json and DIR/results.md (a table per acceleration).
--quick: one acceleration, two feeds (make test).
"""
import argparse
import glob
import json
import math
import os
import re
import shutil
import subprocess
import sys
import tempfile

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
LINUXCNC = os.environ.get("LINUXCNC", "/home/user/linuxcnc-2.9")
COLS = "cx cy tx ty fe0 fe1 fe2 i0 i1 i2 rack frx fry ftx fty hx hy bus vel xfb mt".split()
IMAX = (9.2, 7.8, 7.8)  # X, Y1, Y2 drives' max_ac_cur


PORTAL = os.path.join(ROOT, "host", "portal", "machine", "portal.portal")


def setup(d, accel, vmax):
    for f in ("portal.ini", "portal.hal", "portal_run.py", "tool.tbl", "portal.var"):
        shutil.copy(os.path.join(HERE, f), d)
    ini = open(os.path.join(d, "portal.ini")).read()
    ini = ini.replace("DISPLAY = axis", "DISPLAY = ./portal_run.py")
    ini = ini.replace("FILE = ../../host/portal/machine/portal.portal", "FILE = %s" % PORTAL)
    ini = ini.replace("LIB = ../../host/arm/libstmbl_drive.so", "LIB = %s/host/arm/libstmbl_drive.so" % ROOT)
    ini = re.sub(r"(?m)^(MAX_(LINEAR_)?ACCELERATION|DEFAULT_LINEAR_ACCELERATION) = .*", lambda m: "%s = %g" % (m.group(1), accel), ini)
    ini = re.sub(r"(?m)^(MAX_(LINEAR_)?VELOCITY) = .*", lambda m: "%s = %g" % (m.group(1), vmax), ini)
    # the joints get 10 % more than the axes (LinuxCNC's usual margin)
    parts = re.split(r"(?m)^(\[JOINT_\d\])", ini)
    for i in range(1, len(parts), 2):
        parts[i + 1] = re.sub(r"(?m)^MAX_ACCELERATION = .*", "MAX_ACCELERATION = %g" % (accel * 1.1), parts[i + 1])
        parts[i + 1] = re.sub(r"(?m)^MAX_VELOCITY = .*", "MAX_VELOCITY = %g" % (vmax * 1.1), parts[i + 1])
    open(os.path.join(d, "portal.ini"), "w").write("".join(parts))


def seg_dist(p, a, b):
    """distance of points p (n,2) from segments a-b (n,2 each)"""
    d = b - a
    l2 = np.maximum((d * d).sum(1), 1e-18)
    u = np.clip(((p - a) * d).sum(1) / l2, 0, 1)
    q = a + d * u[:, None]
    return np.hypot(*(p - q).T)


FEATURES = ["spiral", "gear", "hatch", "hatch", "hatch", "hatch", "circle R100", "circle R20", "circle R5", "star"]


def path_error(tool, geom, cell=2.0):
    """distance of each tool point from the programmed polylines (exact:
    every segment within reach, from a grid of cell [mm] over them), and
    the polyline (feature) it is nearest to"""
    a = np.concatenate([np.array(pl[:-1]) for pl in geom])
    b = np.concatenate([np.array(pl[1:]) for pl in geom])
    owner = np.concatenate([np.full(len(pl) - 1, i) for i, pl in enumerate(geom)])
    grid = {}
    lo = np.floor(np.minimum(a, b) / cell).astype(int)
    hi = np.floor(np.maximum(a, b) / cell).astype(int)
    for k in range(len(a)):
        for gx in range(lo[k, 0], hi[k, 0] + 1):
            for gy in range(lo[k, 1], hi[k, 1] + 1):
                grid.setdefault((gx, gy), []).append(k)
    out = np.empty(len(tool))
    feat = np.zeros(len(tool), int)
    cells = np.floor(tool / cell).astype(int)
    for n, p in enumerate(tool):
        cx, cy = cells[n]
        best, bk, r = np.inf, 0, 1
        while True:  # rings of cells until the best is closer than the ring
            ks = [k for gx in range(cx - r, cx + r + 1) for gy in range(cy - r, cy + r + 1) for k in grid.get((gx, gy), ())]
            if ks:
                ks = np.unique(ks)
                d = seg_dist(np.repeat(p[None], len(ks), 0), a[ks], b[ks])
                i = int(np.argmin(d))
                if d[i] < best:
                    best, bk = d[i], ks[i]
            if best <= r * cell or r > 64:
                break
            r *= 2
        out[n], feat[n] = best, owner[bk]
    return out, feat


def servo_error(tool, cmd, w=60):
    """distance of each tool point from the commanded path around it"""
    n = len(tool)
    best = np.full(n, np.inf)
    idx = np.arange(n)
    for off in range(-w, w):
        i0, i1 = np.clip(idx + off, 0, n - 1), np.clip(idx + off + 1, 0, n - 1)
        best = np.minimum(best, seg_dist(tool, cmd[i0], cmd[i1]))
    return best


def analyse(samples, res, geom, cut_len, feed):
    try:  # halsampler is stopped mid-line: keep the complete rows
        rows = [l.split() for l in open(samples).read().splitlines()]
        a = np.array([r for r in rows if len(r) == len(COLS) + 1], float).reshape(-1, len(COLS) + 1)
    except (ValueError, OSError):
        a = np.zeros((0, len(COLS) + 1))
    m = {"status": res.get("status", "?"), "errors": res.get("errors", [])}
    if len(a) < 10:
        m["status"] = m["status"] + " (no samples)"
        return m
    c = {k: a[:, i + 1] for i, k in enumerate(COLS)}
    mt = c["mt"].astype(int)
    moving = np.nonzero(mt > 0)[0]
    cut = (mt == 2) | (mt == 3)
    m["time"] = (moving[-1] - moving[0] + 1) / 1000.0 if len(moving) else 0.0
    m["cut_time"] = cut.sum() / 1000.0
    m["feed_pct"] = 100.0 * (cut_len / max(m["cut_time"], 1e-9)) / (feed / 60.0)
    tool = np.stack([c["tx"], c["ty"]], 1)
    cmd = np.stack([c["cx"], c["cy"]], 1)
    sv = servo_error(tool, cmd) * 1000
    pe, feat = path_error(tool[cut], geom) if cut.any() else (np.zeros(1), np.zeros(1, int))
    pe *= 1000
    m["path_max"], m["path_p99"] = float(pe.max()), float(np.percentile(pe, 99))
    m["features"] = {}  # p99 path error per feature [um]
    for i, name in enumerate(FEATURES):
        sel = np.array([FEATURES[f] == name for f in feat])
        if sel.any():
            m["features"][name] = float(np.percentile(pe[sel], 99))
    m["worst"] = max(m["features"], key=m["features"].get) if m["features"] else ""
    m["servo_max"], m["servo_p99"] = float(sv[cut].max()) if cut.any() else 0.0, float(np.percentile(sv[cut], 99)) if cut.any() else 0.0
    m["ferror"] = float(max(abs(c["fe0"]).max(), abs(c["fe1"]).max(), abs(c["fe2"]).max()))
    ratio = [abs(c[k]) / lim for k, lim in zip(("i0", "i1", "i2"), IMAX)]
    m["i_ratio"] = float(max(r.max() for r in ratio))
    m["sat_pct"] = float(100.0 * max((r > 0.97).mean() for r in ratio))
    m["rack"] = float(abs(c["rack"]).max())
    m["frame"] = float(max(abs(c["frx"]).max(), abs(c["fry"]).max()))
    m["feet"] = float(max(abs(c["ftx"]).max(), abs(c["fty"]).max()))
    m["head"] = float(max(abs(c["hx"]).max(), abs(c["hy"]).max()))
    m["bus_min"], m["bus_max"] = float(c["bus"].min()), float(c["bus"].max())
    return m


def run_accel(accel, feeds, vmax, out, geom_file, cut_len):
    d = tempfile.mkdtemp(prefix="portal-bench.")
    setup(d, accel, vmax)
    progs = []
    for f in feeds:
        p = os.path.join(d, "path_F%d.ngc" % f)
        subprocess.run([sys.executable, os.path.join(HERE, "gen_path.py"), str(f), p], check=True, stdout=subprocess.DEVNULL)
        progs.append(p)
    env = dict(os.environ, PORTAL_PROGRAMS=" ".join(progs), PORTAL_OUT=d)
    # without a terminal linuxcnc writes its output to /tmp/linuxcnc.print.*
    # and on an error copies it to ~/linuxcnc_print.txt / linuxcnc_debug.txt
    # (pybin: python3 as LinuxCNC's modules want it, for portal_run.py)
    cmd = ("export PATH=%s/pybin:$PATH; . %s/scripts/rip-environment >/dev/null && cd %s && "
           "timeout %d linuxcnc portal.ini > linuxcnc.log 2>&1") % (LINUXCNC, LINUXCNC, d, 120 + sum(9000 / f * 60 * 3 for f in feeds))
    subprocess.run(["bash", "-c", cmd], env=env)
    geom = json.load(open(geom_file))
    rows = []
    for f, p in zip(feeds, progs):
        name = os.path.splitext(os.path.basename(p))[0]
        try:
            res = json.load(open(os.path.join(d, name + ".json")))
        except OSError:
            res = {"status": "not run", "errors": []}
        m = analyse(os.path.join(d, name + ".samples"), res, geom, cut_len, f)
        m.update(accel=accel, feed=f, dir=d)
        rows.append(m)
        print(fmt_row(m), flush=True)
    return rows


HEAD = ("| F [m/min] | status | time [s] | feed % | path max / p99 [um] | servo max / p99 [um] | ferror [mm] | "
        "I/Imax | sat % | rack [urad] | frame [um] | feet [N] | head [um] | bus [V] | worst feature (p99 um) |")


def fmt_row(m):
    if "time" not in m:
        return "| %g | %s | | | | | | | | | | | | | |" % (m["feed"] / 1000, m["status"])
    st = m["status"] if m["status"] == "done" else "%s: %s" % (m["status"], "; ".join(m["errors"])[:60])
    return ("| %g | %s | %.1f | %.0f | %.0f / %.0f | %.0f / %.0f | %.3f | %.2f | %.1f | %.0f | %.0f | %.0f | %.0f | %.0f-%.0f | %s %.0f |" % (
        m["feed"] / 1000, st, m["time"], m["feed_pct"], m["path_max"], m["path_p99"], m["servo_max"], m["servo_p99"],
        m["ferror"], m["i_ratio"], m["sat_pct"], m["rack"], m["frame"], m["feet"], m["head"], m["bus_min"], m["bus_max"],
        m["worst"], m["features"].get(m["worst"], 0)))


def main():
    global PORTAL
    ap = argparse.ArgumentParser()
    ap.add_argument("--accel", default="2500,5000,10000,20000")
    ap.add_argument("--feeds", default="6000,12000,24000,36000,48000,72000")
    ap.add_argument("--vmax", type=float, default=1600)
    ap.add_argument("--out", default=os.path.join(HERE, "results"))
    ap.add_argument("--quick", action="store_true")
    ap.add_argument("--portal", default=PORTAL)
    ap.add_argument("--reanalyse", nargs="+")
    a = ap.parse_args()
    PORTAL = os.path.abspath(a.portal)
    if a.quick:
        a.accel, a.feeds = "5000", "12000,36000"
    os.makedirs(a.out, exist_ok=True)
    geom_file = os.path.join(a.out, "path_geom.json")
    info = subprocess.run([sys.executable, os.path.join(HERE, "gen_path.py"), "1000", os.path.join(a.out, "path.ngc"), geom_file],
                          check=True, capture_output=True, text=True).stdout
    geom = json.load(open(geom_file))
    cut_len = sum(math.dist(p, q) for pl in geom for p, q in zip(pl, pl[1:]))
    print("# " + info.strip(), flush=True)
    allrows = []
    md = ["# Portal speed sweep", "", "Path: %s" % info.strip().split(": ", 1)[1], ""]
    if a.reanalyse:
        for d in a.reanalyse:
            acc = float(re.search(r"(?m)^MAX_LINEAR_ACCELERATION = (.*)", open(os.path.join(d, "portal.ini")).read()).group(1))
            print("# acceleration %g mm/s^2 (%.2f g): %s" % (acc, acc / 9810, d), flush=True)
            print(HEAD, flush=True)
            rows = []
            for p in sorted(glob.glob(os.path.join(d, "path_F*.ngc")), key=lambda p: int(re.search(r"F(\d+)", p).group(1))):
                name = os.path.splitext(os.path.basename(p))[0]
                f = int(re.search(r"F(\d+)", p).group(1))
                try:
                    res = json.load(open(os.path.join(d, name + ".json")))
                except OSError:
                    res = {"status": "not run", "errors": []}
                m = analyse(os.path.join(d, name + ".samples"), res, geom, cut_len, f)
                m.update(accel=acc, feed=f, dir=d)
                rows.append(m)
                print(fmt_row(m), flush=True)
            allrows += rows
            md += ["## %g mm/s^2 (%.2f g)" % (acc, acc / 9810), "", HEAD, "|" + "---|" * 15] + [fmt_row(r) for r in rows] + [""]
        a.accel = ""
    for acc in [float(x) for x in a.accel.split(",") if x]:
        print("# acceleration %g mm/s^2 (%.2f g), vmax %g mm/s" % (acc, acc / 9810, a.vmax), flush=True)
        print(HEAD, flush=True)
        rows = run_accel(acc, [int(f) for f in a.feeds.split(",")], a.vmax, a.out, geom_file, cut_len)
        allrows += rows
        md += ["## %g mm/s^2 (%.2f g)" % (acc, acc / 9810), "", HEAD, "|" + "---|" * 15] + [fmt_row(r) for r in rows] + [""]
    json.dump(allrows, open(os.path.join(a.out, "results.json"), "w"), indent=1)
    open(os.path.join(a.out, "results.md"), "w").write("\n".join(md) + "\n")
    if a.quick:  # make test: both runs finish, the slow one within 0.5 mm of the program
        bad = [r for r in allrows if r["status"] != "done"]
        slow = allrows[0]
        ok = not bad and slow.get("path_p99", 1e9) < 500 and slow.get("ferror", 1e9) < 0.5
        print("# quick check: %s" % ("ok" if ok else "FAILED"))
        sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
