#!/usr/bin/env python3
"""machines/excitech (the router) in LinuxCNC, made and run by millweb-supervisor: its rack's M6.

    python3 test/harness.py --machine excitech -- python3 test/router_live.py

Machine on, homed; T2 M6: the empty spindle to pocket 2 (machine X 300, Y 1290, Z -150) and out;
T5 M6: T2 back into its fork, T5 out of fork 5; the hello says router with the rack."""
import asyncio
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from client import C  # noqa: E402

fails = []


def check(cond, what):
    print(("ok   " if cond else "FAIL ") + what, flush=True)
    if not cond:
        fails.append(what)


async def main():
    c = await C.open(int(os.environ["MILLWEB_PORT"]))
    info = c.hello["machine"]
    check(info["type"] == "router", "hello: a router")
    check(info["rack"] and info["rack"]["pockets"] == 6 and info["rack"]["y"] == 1290, "hello: the 6-fork rack at Y 1290")
    check(info["limits"]["X"]["max"] == 1200 and info["limits"]["Y"]["max"] == 1320, "travel 1200 x 1320")
    check(info["spindle_max"] == 24000, "spindle 24 000 min-1")
    for op, kw in (("control", {"take": True}), ("estop", {"on": False}), ("power", {"on": True}), ("home", {"axis": "all"})):
        r = await c.call(op, **kw)
        check(r["ok"], "%s %s" % (op, r.get("err", "")))
    check(await c.until(lambda c: all(c.st["homed"]), 30), "homed")
    seen = []  # machine positions while changing (the st messages' machine position)

    async def change(n):
        mark = c.mark()
        r = await c.call("mdi", cmd="T%d M6" % n)
        check(r["ok"], "T%d M6 sent %s" % (n, r.get("err", "")))
        ok = await c.until(lambda c: c.st.get("tool") == n and c.st.get("interp") == "idle", 90)
        if not ok:
            print("  st:", {k: c.st.get(k) for k in ("tool", "interp", "run", "mode", "pos", "mpos")}, flush=True)
            print("  events:", [m.get("text") for m in c.msgs[mark:] if m["t"] in ("ev", "err", "msg")][:8], flush=True)
        pos = [m["mach"] for m in c.msgs[mark:] if m["t"] == "st" and m.get("mach")]
        seen.append(pos)
        return ok

    check(await change(2), "T2 in the spindle")
    check(await change(5), "T5 in the spindle (T2 back in fork 2)")
    at = lambda ps, x, y, z: any(abs(p[0] - x) < 1 and abs(p[1] - y) < 1 and abs(p[-1] - z) < 1 for p in ps)  # noqa: E731
    check(at(seen[0], 300, 1290, -150), "T2 M6: down into fork 2 (machine X 300 Y 1290 Z -150)")
    check(at(seen[1], 300, 1290, -150) and at(seen[1], 750, 1290, -150), "T5 M6: T2 into fork 2, T5 out of fork 5 (X 750)")
    await c.call("control", take=False)
    await c.close()
    print("%d checks failed" % len(fails) if fails else "all passed", flush=True)
    return 1 if fails else 0


sys.exit(asyncio.run(main()))
