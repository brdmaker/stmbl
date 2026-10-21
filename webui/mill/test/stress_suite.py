#!/usr/bin/env python3
"""Large programs through millweb and LinuxCNC (test/sim).

    python3 test/stress_suite.py [-k WORD] [--keep] [-v] [--no-browser]   (a non-root user)

The programs (bigcode.py, and LinuxCNC's own 3D_Chips.ngc) are written into
the program folder, as a large program arrives over the network or a USB
stick: too large to upload through the page. Each case checks one layer:
the preview (in a child process; its time, its size, the whole program's
travel and time), that the server keeps answering while it works, the
listing's line windows, the cache, LinuxCNC running a program of short
moves, run-from-line a million lines in, and the page in Chromium.
"""
import argparse
import asyncio
import json
import os
import shutil
import subprocess
import sys
import time
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import harness  # noqa: E402
from backend_suite import T, Failed, baseline  # noqa: E402
from client import C, near, segs, wsserver  # noqa: E402

wsserver.MAX_MESSAGE = 64 << 20  # this client takes previews whole

PROGRAMS = [  # name, kind, size for bigcode.py
    ("surface-1m.ngc", "surface", 1000000),
    ("surface-20k.ngc", "surface", 20000),
    ("adaptive-300k.ngc", "adaptive", 300000),
    ("drill-20k.ngc", "drill", 20000),
    ("loop-2m.ngc", "loop", 2000),
]
CASES = []


def case(name):
    def deco(fn):
        CASES.append((name, fn))
        return fn
    return deco


class Pinger:
    """Pings every 100 ms from a second browser: the server's worst answer time."""

    def __init__(self, port):
        self.port, self.worst, self.task = port, 0.0, None

    async def __aenter__(self):
        self.c = await C.open(self.port)
        self.task = asyncio.ensure_future(self.run())
        return self

    async def run(self):
        while True:
            t = time.time()
            await self.c.call("ping", timeout=120)
            self.worst = max(self.worst, time.time() - t)
            await asyncio.sleep(0.1)

    async def __aexit__(self, *a):
        self.task.cancel()
        await self.c.close()


def rss():
    pids = harness.millweb_pids()
    if not pids:
        return 0
    for line in open("/proc/%d/status" % pids[0]):
        if line.startswith("VmRSS"):
            return int(line.split()[1]) // 1024
    return 0


async def get(t, name):
    """program_get: (the program, seconds, MB), with the server's worst ping meanwhile."""
    async with Pinger(t.port) as p:
        t0 = time.time()
        n = await t.a.send("program_get", name=name)
        await t.a.until(lambda c: n in c.replies, 400)
        dt = time.time() - t0
        await asyncio.sleep(0.3)
    r = t.a.replies.pop(n, {"ok": False, "err": "no reply"})
    if not r.get("ok"):
        raise Failed("program_get %s: %s" % (name, r.get("err")))
    pg = r["program"]
    mb = len(json.dumps(pg)) / 1e6
    st = pg.get("stats") or {}
    print("    %-18s %6.1f s  %5.1f MB  %8s moves, %6d drawn (every %d)  %s lines  ping worst %.2f s  RSS %d MB" % (
        name, dt, mb, "{:,}".format(st.get("moves", 0)), st.get("shown", 0), st.get("stride", 0),
        "{:,}".format(pg.get("nlines", 0)), p.worst, rss()), flush=True)
    return pg, dt, mb, p.worst


def check_preview(t, pg, mb, worst, moves_min, ext=None):
    st = pg.get("stats") or {}
    t.check(pg.get("error") is None, "no interpreter error (%s)" % pg.get("error"))
    t.check(st.get("moves", 0) >= moves_min, "every move counted (%s ≥ %s)" % (st.get("moves"), moves_min))
    t.check(st.get("shown", 0) <= 100000 and len(segs(pg)) <= 100002, "the drawn path ≤ 100 000 segments (%d)" % len(segs(pg)))
    t.check(mb < 8, "the message under 8 MB (%.1f)" % mb)
    t.check(worst < 0.5, "the server answers meanwhile: worst ping %.2f s < 0.5 s" % worst)
    p = segs(pg)
    t.check(all(a[4:7] == b[1:4] for a, b in zip(p, p[1:])), "the drawn path is connected")
    if ext:
        for a, (lo, hi) in ext.items():
            e = st.get("ext", {}).get(a, [None, None])
            t.check(e[0] is not None and near(e[0], lo, 0.05) and near(e[1], hi, 0.05), "%s travel %r ≈ [%s, %s]" % (a.upper(), e, lo, hi))
    t.check(bool(harness.millweb_pids()), "millweb still running")


# ======================================================================= previews

@case("3D_Chips.ngc (LinuxCNC's own, inches): previews in mm")
async def _(t):
    pg, dt, mb, worst = await get(t, "3D_Chips.ngc")
    check_preview(t, pg, mb, worst, 4000)
    e = pg["stats"]["ext"]
    t.check(e["x"][1] - e["x"][0] > 50, "inches came out as mm (X span %.1f mm)" % (e["x"][1] - e["x"][0]))
    t.check(dt < 10, "in %.1f s" % dt)


@case("surface-1m: a million lines of 3D finishing")
async def _(t):
    pg, dt, mb, worst = await get(t, "surface-1m.ngc")
    check_preview(t, pg, mb, worst, 990000, {"x": (0, 120), "y": (0, 80)})
    st = pg["stats"]
    t.check(st["stride"] >= 8, "drawn: every %dth point" % st["stride"])
    t.check(pg["nlines"] == 1001661 and len(pg["lines"]) == 2000 and pg["line0"] == 1, "the first 2000 of 1 001 661 lines")
    t.check(st["time"] > 3600, "the time over every move (%.0f s)" % st["time"])
    t.check(dt < 120, "in %.0f s" % dt)
    t0 = time.time()
    r = await t.a.call("program_get", name="surface-1m.ngc")
    t.check(r["ok"] and time.time() - t0 < 3, "asked again: from the cache (%.2f s)" % (time.time() - t0))


@case("adaptive-300k: arcs at several depths")
async def _(t):
    pg, dt, mb, worst = await get(t, "adaptive-300k.ngc")
    check_preview(t, pg, mb, worst, 300000, {"x": (10, 107.6)})
    t.check(pg["stats"]["ext"]["z"][0] >= -20.01, "deepest Z −20")


@case("drill-20k: 20 000 G83 peck cycles")
async def _(t):
    pg, dt, mb, worst = await get(t, "drill-20k.ngc")
    check_preview(t, pg, mb, worst, 20000 * 5, {"x": (2, 117.523), "y": (2, 77.688)})  # a 0.664 mm pitch
    t.check(pg["stats"]["ext"]["z"][0] >= -6.01, "holes to Z−6")
    t.check(pg["truncated"] is None and dt < 60, "the whole program in %.0f s" % dt)


@case("loop-2m: 25 lines, 2 million moves (O-word loops)")
async def _(t):
    pg, dt, mb, worst = await get(t, "loop-2m.ngc")
    st = pg.get("stats") or {}
    t.check(pg.get("error") is None, "no error (%s)" % pg.get("error"))
    t.check(st.get("moves", 0) >= 2000000 or pg.get("truncated"), "all 2 000 000 moves, or stopped at the time limit (%s)" % pg.get("truncated"))
    t.check(st.get("shown", 0) <= 100000 and mb < 8, "a bounded path (%d, %.1f MB)" % (st.get("shown", 0), mb))
    t.check(worst < 0.5, "the server answers meanwhile: worst ping %.2f s" % worst)
    t.check(bool(harness.millweb_pids()), "millweb still running")


# ======================================================================= listing, loading

@case("listing: line windows deep into a million lines")
async def _(t):
    for start in (1, 500000, 1001000):
        t0 = time.time()
        r = await t.a.call("program_lines", name="surface-1m.ngc", start=start, count=2000)
        dt = time.time() - t0
        want = open(os.path.join(t.dir, "nc_files", "surface-1m.ngc")).read().splitlines()[start - 1:start + 1999] if start == 500000 else None
        t.check(r["ok"] and r["nlines"] == 1001661, "lines from %d (%.2f s)" % (start, dt))
        if want is not None:
            t.check(r["lines"] == want, "the right lines")
        t.check(dt < 3, "quick (%.2f s)" % dt)
    r = await t.a.call("program_lines", name="surface-1m.ngc", start=1001660, count=10)
    t.check(r["lines"] == ["M30", "%"], "the last lines (%r)" % r["lines"])


@case("load: the preview goes out in the background; status keeps flowing")
async def _(t):
    a = t.a
    mk = a.mark()
    async with Pinger(t.port) as p:
        t0 = time.time()
        r = await a.call("program_load", name="adaptive-300k.ngc")
        t.check(r["ok"] and time.time() - t0 < 5, "loaded at once (%.1f s)" % (time.time() - t0))
        await t.mdi("G0 X20 Y20")  # the machine answers while the preview runs
        t.check(near(a.st["pos"][0], 20), "an MDI move meanwhile")
        t.check(await a.until(lambda c: c.since(mk, lambda m: m["t"] == "program" and m["name"] == "adaptive-300k.ngc"), 300), "the preview arrives")
    t.check(p.worst < 0.5, "worst ping %.2f s" % p.worst)


@case("load another program while a preview runs: the newer one wins")
async def _(t):
    a = t.a
    mk = a.mark()
    await a.call("program_load", name="loop-2m.ngc")
    await asyncio.sleep(1)
    await a.call("program_load", name="surface-20k.ngc")
    t.check(await a.until(lambda c: c.since(mk, lambda m: m["t"] == "program" and m["name"] == "surface-20k.ngc"), 120), "the second one's preview")
    await asyncio.sleep(3)
    last = a.last("program")
    t.check(last["name"] == "surface-20k.ngc", "and it stays the one shown (%s)" % last["name"])


# ======================================================================= running

@case("run: 20 000 short moves to the end")
async def _(t):
    a = t.a
    await t.load("surface-20k.ngc")
    await a.call("override", k="feed", v=2.0)
    mk = a.mark()
    t0 = time.time()
    t.ok(await a.call("cycle", action="start"), "start")
    vel, lines = [], []
    while time.time() - t0 < 600 and a.st["run"] != "done":
        await asyncio.sleep(0.5)
        vel.append(a.st["vel"])
        lines.append(a.st["line"])
    dt = time.time() - t0
    t.check(a.st["run"] == "done" and a.ev(mk, "Program finished") is not None, "finished in %.0f s" % dt)
    moving = sorted(v for v in vel[2:-2] if v > 0)
    med = moving[len(moving) // 2] if moving else 0
    print("    feed: median %.0f mm/min (programmed 3000, override 200 %%), %.0f s" % (med, dt), flush=True)
    t.check(med > 5500, "the planner keeps the feed along 0.7 mm moves: median %.0f of 6000 mm/min" % med)
    t.check(lines == sorted(lines), "the line only goes forward")
    t.check(dt < 240, "the time a 13.9 m path at 6000 mm/min takes, give or take the ends of the rows (%.0f s)" % dt)


@case("run from line 900 000 of a million")
async def _(t):
    a = t.a
    await t.load("surface-1m.ngc")
    await a.call("override", k="feed", v=2.0)
    t0 = time.time()
    t.ok(await a.call("cycle", action="start", line=900000), "start from line 900 000")
    t.check(await a.until(lambda c: c.st["run"] == "run" and c.st["vel"] > 0, 120), "moving after %.1f s (the interpreter skips 900 000 lines)" % (time.time() - t0))
    t.check(await a.until(lambda c: c.st["line"] >= 900000, 10), "at line %d" % a.st["line"])
    await asyncio.sleep(3)
    l1 = a.st["line"]
    t.check(l1 > 900000, "advancing (line %d)" % l1)
    t.ok(await a.call("cycle", action="stop"), "stop")
    t.check(await a.until(lambda c: c.st["interp"] == "idle", 5), "stopped")


@case("the page: a million-line program in Chromium")
async def _(t):
    if t.no_browser:
        return
    await t.load("surface-1m.ngc")
    await t.a.call("override", k="feed", v=2.0)
    t.ok(await t.a.call("cycle", action="start", line=500000), "the machine runs from line 500 000")
    t.check(await t.a.until(lambda c: c.st["line"] >= 500000, 120), "at line %d" % t.a.st["line"])
    env = dict(os.environ, NODE_PATH=os.environ.get("NODE_PATH", "/opt/node22/lib/node_modules"))
    p = await asyncio.create_subprocess_exec("node", os.path.join(HERE, "gui", "stress_page.js"), str(t.port), env=env,
                                             stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.STDOUT)
    out, _ = await asyncio.wait_for(p.communicate(), 600)
    for line in out.decode().splitlines():
        print("    " + line, flush=True)
        if line.startswith("FAIL"):
            t.fails.append(line[5:])
    t.check(p.returncode == 0, "the page's checks")
    await t.a.call("cycle", action="stop")


# ======================================================================= runner

def write_programs(d):
    nc = os.path.join(d, "nc_files")
    for name, kind, n in PROGRAMS:
        subprocess.run([sys.executable, os.path.join(HERE, "bigcode.py"), kind, str(n), os.path.join(nc, name)], check=True)
    chips = os.path.join(harness.LINUXCNC, "nc_files", "3D_Chips.ngc")
    if os.path.exists(chips):
        shutil.copy(chips, nc)


async def run(m, cases, verbose, no_browser):
    a = await C.open(m.port)
    t = T(m, a, verbose)
    t.no_browser = no_browser
    results = []
    for name, fn in cases:
        t.fails = []
        t0 = time.time()
        err = None
        print("---- " + name, flush=True)
        try:
            await asyncio.wait_for(baseline(t), 120)
            await asyncio.wait_for(fn(t), 900)
        except Failed as e:
            err = str(e)
        except Exception:
            err = traceback.format_exc(limit=4)
        if err:
            t.fails.append(err)
            print("    " + err.replace("\n", "\n    "))
        if not harness.millweb_pids():
            t.fails.append("millweb died")
        ok = not t.fails
        print("%s %-66s %6.1fs" % ("PASS" if ok else "FAIL", name, time.time() - t0), flush=True)
        results.append((name, ok))
        if not harness.millweb_pids():
            break
        if t.a.closed:
            t.a = a = await C.open(m.port)
    await a.close()
    return results


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-k", action="append", default=[])
    ap.add_argument("--keep", action="store_true")
    ap.add_argument("-v", "--verbose", action="store_true")
    ap.add_argument("--no-browser", action="store_true")
    args = ap.parse_args()
    cases = [c for c in CASES if not args.k or any(k.lower() in c[0].lower() for k in args.k)]
    t0 = time.time()
    with harness.Machine(keep=args.keep) as m:
        print("writing the programs ...", flush=True)
        write_programs(m.dir)
        results = asyncio.run(run(m, cases, args.verbose, args.no_browser))
    bad = [n for n, ok in results if not ok]
    if not m.clean_exit:
        bad.append("LinuxCNC stops when millweb does")
    print("\n%d/%d cases passed in %.0f s" % (len(results) - len(bad), len(results), time.time() - t0))
    for n in bad:
        print("  FAIL " + n)
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
