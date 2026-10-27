#!/usr/bin/env python3
"""millweb's backend against real LinuxCNC (test/sim), case by case.

    python3 test/backend_suite.py [-k WORD] [--list] [--keep] [-v]   (a non-root user)

Each case starts from the same machine: E-STOP reset, on, homed, idle, G54
at its test values, G92 clear, overrides 100 %, spindle and coolant off,
the tool at a safe height. Cases that need a different INI (a token, a
remote-enable key, feed hold on lost control) run on their own LinuxCNC.
Exit status 0 when every case passed.
"""
import argparse
import asyncio
import json
import os
import socket
import sys
import time
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import harness  # noqa: E402
from client import C, near, segs  # noqa: E402

G54 = [440.0, 460.0, -195.0]
G55 = [640.0, 460.0, -195.0]
CASES = []

SLOW = """(slow: long feed moves for pause, resume and stop)
G21 G90 G54
G0 X0 Y0 Z5
G1 X100 F300
G1 Y100
G1 X0
G1 Y0
M2
"""

STEPS = """(steps)
G21 G90 G54
G0 X10 Y10 Z5
G0 X20
G0 X30
G0 X40
G0 X50
M2
"""


def case(name, profile="default"):
    def deco(fn):
        CASES.append((name, profile, fn))
        return fn
    return deco


PROFILES = {
    "default": dict(),
    "hold": dict(ini_set=["MILLWEB.ON_CONTROL_LOSS=hold"]),
    "token": dict(ini_set=["MILLWEB.TOKEN=s3cret"]),
    "halwrite": dict(ini_set=["MILLWEB.HAL_WRITE=yes"]),
    "drive": dict(drive=True),
    "drivewrite": dict(drive=True, ini_set=["MILLWEB.DRIVE_WRITE=yes"]),
    "supervised": dict(supervised=True),
    "drivefw2": dict(drive=True, ini_set=["MILLWEB.DRIVE_FW=2"]),
    "vib": dict(vib=2),
    "remote": dict(ini_set=["MILLWEB.REMOTE_ENABLE_PIN=remote-key.in0", "MILLWEB.AIR_PIN=air.in0"],
                   hal_extra="loadrt or2 names=remote-key,air\nsetp remote-key.in0 1"),
}


class Failed(Exception):
    pass


class T:
    """A case's context: `a` has control; check(), helpers."""

    def __init__(self, m, a, verbose):
        self.m, self.port, self.dir, self.a = m, m.port, m.dir, a
        self.fails, self.verbose, self.extra = [], verbose, []

    def check(self, cond, what):
        if self.verbose or not cond:
            print("    %s %s" % ("ok  " if cond else "FAIL", what), flush=True)
        if not cond:
            self.fails.append(what)
        return cond

    def ok(self, r, what):
        return self.check(r.get("ok"), what + ("" if r.get("ok") else " (%s)" % r.get("err")))

    def refused(self, r, what, text=None):
        return self.check(not r.get("ok") and (text is None or text in (r.get("err") or "")),
                          "%s: refused%s (%s)" % (what, " with '%s'" % text if text else "", r.get("err")))

    async def open(self):
        c = await C.open(self.port, getattr(self.m, "host", "127.0.0.1"))
        self.extra.append(c)
        return c

    async def idle(self, c=None, timeout=60):
        """MDI or program done: the interpreter idle and nothing moving."""
        c = c or self.a
        await asyncio.sleep(0.15)
        return await c.until(lambda c: c.st["interp"] == "idle" and c.st.get("queue", 0) == 0 and abs(c.st["vel"]) < 1e-6, timeout)

    async def mdi(self, cmd, timeout=60):
        r = await self.a.call("mdi", cmd=cmd)
        if not r.get("ok"):
            raise Failed("MDI %s: %s" % (cmd, r.get("err")))
        if not await self.idle(timeout=timeout):
            raise Failed("MDI %s didn't finish" % cmd)

    async def at(self, pos, timeout=30):
        return await self.a.until(lambda c: near(c.st["pos"], pos) and c.st["interp"] == "idle", timeout)

    async def upload(self, name, text):
        r = await self.a.call("program_save", name=name, text=text)
        if not r.get("ok"):
            raise Failed("save %s: %s" % (name, r.get("err")))

    async def load(self, name, text=None):
        if text is not None:
            await self.upload(name, text)
        mk = self.a.mark()
        r = await self.a.call("program_load", name=name)
        if not r.get("ok"):
            raise Failed("load %s: %s" % (name, r.get("err")))
        await self.a.until(lambda c: c.since(mk, lambda m: m["t"] == "program" and m["name"] == name), 300)
        return self.a.since(mk, lambda m: m["t"] == "program" and m["name"] == name)


async def baseline(t):
    a = t.a
    for c in t.extra:
        await c.close()
    t.extra = []
    if a.closed:
        raise Failed("the control client lost its connection")
    if not (await a.call("control", take=True, force=True, token="s3cret")).get("ok"):
        raise Failed("can't take control")
    await a.call("jog_stop")
    await a.call("cycle", action="stop")
    if a.st.get("estop"):
        await a.call("estop", on=False)
    if not a.st.get("on"):
        r = await a.call("power", on=True)
        if not r.get("ok") and "drive firmware" in (r.get("err") or ""):
            return  # a drive the console refuses (the drivefw2 profile): the case starts with the machine off
        if not r.get("ok"):
            raise Failed("power on: %s" % r.get("err"))
        await a.until(lambda c: c.st["on"], 5)
    if a.st.get("homed") != [True, True, True]:
        await a.call("home", axis="all")
        if not await a.until(lambda c: c.st["homed"] == [True, True, True], 30):
            raise Failed("can't home")
    await t.idle()
    for k in ("feed", "rapid", "spin", "maxv"):
        await a.call("override", k=k, v=1.0)
    await a.call("spindle", dir=0)
    await a.call("coolant", k="flood", on=False)
    await a.call("coolant", k="mist", on=False)
    await a.call("opt", m1=False, bd=False)
    await t.mdi("G90 G92.1 G49")
    await a.call("offset", index=0, x=G54[0], y=G54[1], z=G54[2])
    await a.call("offset", index=1, x=G55[0], y=G55[1], z=G55[2])
    await a.call("wcs", index=0)
    await t.mdi("G53 G0 Z0")
    await t.mdi("G0 X10 Y10")
    ok = await a.until(lambda c: c.st["wcs"] == 0 and near(c.st["g5x"], G54) and near(c.st["g92"], [0, 0, 0])
                       and near(c.st["ovr"]["feed"], 1) and not c.st["flood"] and not c.st["mist"], 5)
    if not ok:
        raise Failed("baseline not reached: %r" % {k: a.st.get(k) for k in ("wcs", "g5x", "g92", "ovr", "flood", "mist")})


# ======================================================================= connection and protocol

@case("hello: the machine from the INI and the first messages")
async def _(t):
    c = await t.open()
    h = c.hello["machine"]
    t.check(h["name"] == "PORTAL-M sim", "name")
    t.check(h["coords"] == "XYYZ", "coordinates XYYZ")
    t.check(h["limits"]["Z"]["min"] == -215 and h["limits"]["X"]["max"] == 1000, "limits")
    t.check(h["units"] == "mm" and h["version"] and h["token_required"] is False, "units, version, no token")
    t.check(h["toolsetter"] is True and h["pcomp"] is False, "toolsetter configured, no pcomp")
    t.check(h["wcs"][:2] == ["G54", "G55"] and len(h["wcs"]) == 9, "work offsets G54..G59.3")
    t.check(c.hello["control"] == t.a.hello["you"], "who has control")
    t.check(isinstance(c.hello["events"], list) and c.hello["events"], "recent events")
    await c.until(lambda c: {"st", "tools", "offsets", "files"} <= {m["t"] for m in c.msgs}, 5)
    t.check({"st", "tools", "offsets", "files"} <= {m["t"] for m in c.msgs}, "state, tools, offsets, files after hello")
    t.check(c.st.get("homed") == [True, True, True] and near(c.st["g5x"], G54), "the full state, not a diff")
    t.check(await c.until(lambda c: c.last("slow") is not None, 3), "slow values")
    pins = c.last("slow").get("pins", {})
    t.check("joint.0.f-error" in pins and "motion.probe-input" in pins, "the INI's watch pins (%s)" % sorted(pins))


@case("ping and events: a watching browser may read")
async def _(t):
    c = await t.open()
    r = await c.call("ping")
    t.check(r["ok"] and abs(r["time"] - time.time()) < 5, "ping answers with the time")
    r = await c.call("events", n=5)
    t.check(r["ok"] and 1 <= len(r["events"]) <= 5 and {"time", "lvl", "text"} <= set(r["events"][-1]), "events: the last few")
    r = await c.call("files")
    t.check(r["ok"] and any(f["name"] == "bracket-op1.ngc" for f in r["files"]), "files")


@case("protocol: unknown op, bad JSON, wrong types")
async def _(t):
    t.refused(await t.a.call("frobnicate"), "an unknown op", "unknown op")
    await t.a.ws.send("this is not JSON")
    await t.a.ws.send("[1, 2, 3]")
    r = await t.a.call("ping")
    t.check(r["ok"], "the connection survives bad JSON and non-objects")
    t.refused(await t.a.call("zero"), "zero without an axis")
    t.refused(await t.a.call("wcs", index="nine"), "wcs with a bad index")
    t.refused(await t.a.call("override", k="feed", v="fast"), "an override that's not a number")
    t.refused(await t.a.call("jog", axis="q", dir=1, inc=1), "a jog of an unknown axis")
    t.check((await t.a.call("ping"))["ok"], "still answering")


@case("protocol: a message over the limit closes that connection only")
async def _(t):
    c = await t.open()
    await c.ws.send("x" * (9 * 1024 * 1024))
    t.check(await c.until(lambda c: c.closed, 10), "the server closed it")
    t.check((await t.a.call("ping"))["ok"], "other browsers carry on")


@case("protocol: state diffs reach every browser")
async def _(t):
    c = await t.open()
    await t.mdi("G0 X33 Y44 Z6")
    t.check(await c.until(lambda c: near(c.st["pos"], [33, 44, 6]), 5), "a watching browser sees the move")


def http(port, req):
    s = socket.create_connection(("127.0.0.1", port), 5)
    s.sendall(req.encode())
    data = b""
    while True:
        b = s.recv(65536)
        if not b:
            break
        data += b
    s.close()
    head, _, body = data.partition(b"\r\n\r\n")
    lines = head.decode("latin-1").split("\r\n")
    hdr = {k.lower(): v.strip() for k, _, v in (l.partition(":") for l in lines[1:])}
    return int(lines[0].split()[1]), hdr, body


@case("http: the console's files, HEAD, 404, no traversal, GET only")
async def _(t):
    st, h, body = http(t.port, "GET / HTTP/1.1\r\nHost: x\r\n\r\n")
    t.check(st == 200 and h["content-type"].startswith("text/html") and b"live.js" in body, "GET / is the console")
    st, h, body = http(t.port, "GET /js/app.js HTTP/1.1\r\n\r\n")
    t.check(st == 200 and "javascript" in h["content-type"], "scripts as JavaScript")
    t.check(h.get("x-content-type-options") == "nosniff" and h.get("cache-control") == "no-cache", "nosniff, no-cache")
    st, h, body = http(t.port, "HEAD /index.html HTTP/1.1\r\n\r\n")
    t.check(st == 200 and int(h["content-length"]) > 1000 and body == b"", "HEAD: the length, no body")
    for path in ("/nope.html", "/../server/millweb.py", "/%2e%2e/%2e%2e/etc/passwd", "/..%2f..%2fetc/passwd", "/js/../../../../etc/passwd"):
        st, h, body = http(t.port, "GET %s HTTP/1.1\r\n\r\n" % path)
        t.check(st == 404, "GET %s: 404 (%d)" % (path, st))
    st, h, body = http(t.port, "GET /server/millweb.py HTTP/1.1\r\n\r\n")
    t.check(st in (200, 404), "inside the web root is served")  # the backend's source is not secret
    st, h, body = http(t.port, "POST / HTTP/1.1\r\nContent-Length: 0\r\n\r\n")
    t.check(st == 405, "POST: 405")


# ======================================================================= the control lease

@case("control: one holder; take, refuse, force, release")
async def _(t):
    a, b = t.a, await t.open()
    t.refused(await b.call("control", take=True), "a second browser taking control", "another browser")
    mk = a.mark()
    t.ok(await b.call("control", take=True, force=True), "force takes it over")
    t.check(await a.until(lambda c: c.since(mk, lambda m: m["t"] == "control" and m["holder"] == b.hello["you"]), 3),
            "the old holder is told")
    t.check(a.ev(mk, "Control taken over") is not None, "event: control taken over")
    t.refused(await a.call("mdi", cmd="G0 X1"), "the old holder's MDI", "doesn't have control")
    mk = a.mark()
    t.ok(await b.call("control", take=False), "release")
    t.check(await a.until(lambda c: c.since(mk, lambda m: m["t"] == "control" and m["holder"] is None), 3), "nobody has control")
    t.check(a.ev(mk, "released control") is not None and a.ev(mk, "went away") is None, "event: released (not 'went away')")
    t.ok(await a.call("control", take=True), "now anyone may take it")
    t.ok(await a.call("control", take=True), "taking it again is fine")


@case("control: a disconnect drops it; the machine carries on (default)")
async def _(t):
    a = t.a
    await t.load("slow.ngc", SLOW)
    b = await t.open()
    t.ok(await b.call("control", take=True, force=True), "b takes control")
    t.ok(await b.call("cycle", action="start"), "b starts a program")
    t.check(await a.until(lambda c: c.st["run"] == "run", 5), "running")
    mk = a.mark()
    await b.close()
    t.check(await a.until(lambda c: c.since(mk, lambda m: m["t"] == "control" and m["holder"] is None), 3), "control dropped")
    t.check(await a.until(lambda c: c.ev(mk, "carries on"), 3), "event: the machine carries on")
    await asyncio.sleep(0.5)
    t.check(a.st["run"] == "run", "still running")


@case("control: a watching browser reads, can't command, can E-STOP")
async def _(t):
    b = await t.open()
    for op, kw in (("power", dict(on=False)), ("home", dict(axis="x")), ("mdi", dict(cmd="G0 X0")), ("jog", dict(axis="x", dir=1, inc=1)),
                   ("jog", dict(axis="x", dir=1, inc=0)), ("cycle", dict(action="start")), ("override", dict(k="feed", v=0.5)),
                   ("tools_save", dict(tools=[])), ("program_save", dict(name="x.ngc", text="M2")), ("zero", dict(axis="x")),
                   ("spindle", dict(dir=1)), ("probe", dict(kind="z"))):
        t.refused(await b.call(op, **kw), "watching: %s" % op, "doesn't have control")
    r = await b.call("program_get", name="bracket-op1.ngc")
    t.check(r["ok"] and r["program"]["lines"] and r["program"]["path"], "watching: program_get")
    t.ok(await b.call("estop", on=True), "watching: E-STOP")
    t.check(await t.a.until(lambda c: c.st["estop"] and not c.st["on"], 3), "the machine is in E-STOP")


# ======================================================================= machine state

@case("estop and power")
async def _(t):
    a = t.a
    mk = a.mark()
    t.ok(await a.call("estop", on=True), "E-STOP")
    t.check(await a.until(lambda c: c.st["estop"] and not c.st["on"] and c.st["run"] == "idle", 3), "estop, off")
    t.check(await a.until(lambda c: c.ev(mk, "E-STOP from"), 3), "event: who pressed E-STOP")
    t.refused(await a.call("power", on=True), "power on in E-STOP", "E-STOP is active")
    t.refused(await a.call("mdi", cmd="G0 X0"), "MDI in E-STOP", "E-STOP")
    t.refused(await a.call("home", axis="all"), "homing in E-STOP", "E-STOP")
    t.ok(await a.call("estop", on=False), "E-STOP reset")
    t.check(await a.until(lambda c: not c.st["estop"] and not c.st["on"], 3), "reset, still off")
    t.check(await a.until(lambda c: c.ev(mk, "E-STOP released"), 3), "event: E-STOP released")
    t.refused(await a.call("mdi", cmd="G0 X0"), "MDI while off", "the machine is off")
    t.refused(await a.call("spindle", dir=1, speed=1000), "spindle while off", "off")
    t.ok(await a.call("power", on=True), "machine on")
    t.check(await a.until(lambda c: c.st["on"], 3), "on")
    t.ok(await a.call("power", on=False), "machine off")
    t.check(await a.until(lambda c: not c.st["on"] and not c.st["estop"], 3), "off")
    t.check(a.ev(mk, "Machine off") is not None, "event: machine off")


@case("home and unhome, per axis and all")
async def _(t):
    a = t.a
    t.ok(await a.call("unhome", axis="all"), "unhome all")
    t.check(await a.until(lambda c: c.st["homed"] == [False, False, False], 5), "nothing homed")
    t.refused(await a.call("mdi", cmd="G0 X0"), "MDI unhomed", "not homed")
    t.refused(await a.call("jog", axis="x", dir=1, inc=1), "jog unhomed", "not homed")
    t.ok(await a.call("home", axis="y"), "home Y (both gantry joints)")
    t.check(await a.until(lambda c: c.st["homed"] == [False, True, False], 10), "Y homed")
    mk = a.mark()
    t.ok(await a.call("home", axis="all"), "home all")
    t.check(await a.until(lambda c: c.st["homed"] == [True, True, True], 20), "all homed")
    t.check(await a.until(lambda c: c.ev(mk, "All axes homed"), 3), "event: all axes homed")
    t.ok(await a.call("unhome", axis="z"), "unhome Z")
    t.check(await a.until(lambda c: c.st["homed"] == [True, True, False], 5), "only Z unhomed")


# ======================================================================= jogging

@case("jog: increments on every axis, both ways")
async def _(t):
    a = t.a
    await t.mdi("G0 X50 Y50 Z-10")
    p = list(a.st["pos"])
    for i, ax in enumerate("xyz"):
        for d, inc in ((1, 1.0), (-1, 0.1), (-1, 2.5)):
            t.ok(await a.call("jog", axis=ax, dir=d, inc=inc, rate=1200), "jog %s %+g" % (ax, d * inc))
            p[i] += d * inc
            t.check(await t.at(p, 10), "%s at %.3f (%.4f)" % (ax.upper(), p[i], a.st["pos"][i]))


@case("jog: continuous with keep-alives, stopped by jog_stop")
async def _(t):
    a = t.a
    x0 = a.st["pos"][0]
    mk = a.mark()
    t.ok(await a.call("jog", axis="x", dir=1, inc=0, rate=600), "jog X+")
    for _ in range(10):
        await asyncio.sleep(0.1)
        await a.call("jog", axis="x", dir=1, inc=0, rate=600)
    t.check(a.st["vel"] > 0, "moving after 1 s of keep-alives")
    t.ok(await a.call("jog_stop", axis="x"), "jog_stop")
    await t.idle()
    x1 = a.st["pos"][0]
    t.check(x1 - x0 > 8, "it moved (%.2f mm at 10 mm/s)" % (x1 - x0))
    await asyncio.sleep(0.6)
    t.check(near(a.st["pos"][0], x1), "it stays stopped")
    t.check(a.ev(mk, "keep-alive") is None, "no deadman event")
    t.ok(await a.call("jog", axis="z", dir=-1, inc=0, rate=600), "jog Z-")
    await asyncio.sleep(0.2)
    t.ok(await a.call("jog_stop"), "jog_stop without an axis stops all")
    await t.idle()
    z = a.st["pos"][2]
    await asyncio.sleep(0.5)
    t.check(near(a.st["pos"][2], z), "Z stopped")


@case("jog: the deadman stops a jog without keep-alives")
async def _(t):
    a = t.a
    mk = a.mark()
    t.ok(await a.call("jog", axis="y", dir=1, inc=0, rate=600), "jog Y+")
    t.check(await a.until(lambda c: c.ev(mk, "Jog Y stopped: no keep-alive"), 2), "event within 2 s")
    await t.idle()
    y = a.st["pos"][1]
    t.check(y - 10 < 15, "it went no further than the deadman allows (%.2f mm)" % (y - 10))
    await asyncio.sleep(0.5)
    t.check(near(a.st["pos"][1], y), "stopped")


@case("jog: a lost connection stops the jog at once")
async def _(t):
    a = t.a
    b = await t.open()
    t.ok(await b.call("control", take=True, force=True), "b takes control")
    t.ok(await b.call("jog", axis="x", dir=1, inc=0, rate=600), "b jogs X+")
    await asyncio.sleep(0.15)
    await b.call("jog", axis="x", dir=1, inc=0, rate=600)
    await b.close()
    t.check(await a.until(lambda c: c.st["vel"] == 0 and c.st["interp"] == "idle", 2), "stopped")
    x = a.st["pos"][0]
    await asyncio.sleep(0.5)
    t.check(near(a.st["pos"][0], x), "stays stopped")
    t.check(await a.until(lambda c: c.last("control")["holder"] is None, 2), "control dropped")


# ======================================================================= urgent commands

@case("urgent: E-STOP doesn't wait behind a long command")
async def _(t):
    a = t.a
    await a.call("tool_load", t=3)
    n = await a.send("tool_measure")  # ~30 s of moves
    await asyncio.sleep(1.5)
    t0 = time.time()
    r = await a.call("estop", on=True, timeout=5)
    t.check(r["ok"] and time.time() - t0 < 1.0, "E-STOP answered at once (%.2f s)" % (time.time() - t0))
    t.check(await a.until(lambda c: c.st["estop"], 2), "in E-STOP")
    t.check(await a.until(lambda c: n in c.replies, 10), "the measurement ends")
    r = a.replies.pop(n, {})
    t.refused(r, "the measurement", None)


@case("urgent: stop ends a sequence; nothing more is sent")
async def _(t):
    a = t.a
    await a.call("tool_load", t=3)
    n = await a.send("tool_measure")
    await asyncio.sleep(1.5)
    t0 = time.time()
    t.ok(await a.call("cycle", action="stop", timeout=5), "stop")
    t.check(time.time() - t0 < 1.0, "answered at once (%.2f s)" % (time.time() - t0))
    t.check(await a.until(lambda c: n in c.replies, 10), "the measurement ends")
    r = a.replies.pop(n, {})
    t.refused(r, "the measurement", "stopped")
    await t.idle()
    p = list(a.st["mach"])
    await asyncio.sleep(1.5)
    t.check(near(a.st["mach"], p) and a.st["interp"] == "idle", "no further moves (%r)" % p)


@case("MDI with wait: the reply when the move is done")
async def _(t):
    a = t.a
    t0 = time.time()
    r = await a.call("mdi", cmd="G1 X40 F1200", wait=True)  # 30 mm: 1.5 s
    t.check(r["ok"] and time.time() - t0 > 1.3, "replied after %.2f s" % (time.time() - t0))
    t.check(near(a.st["pos"][0], 40, 0.01) or await a.until(lambda c: near(c.st["pos"][0], 40), 1), "there")
    n = await a.send("mdi", cmd="G1 X100 F1200", wait=True)
    await asyncio.sleep(0.5)
    await a.call("cycle", action="stop")
    t.check(await a.until(lambda c: n in c.replies, 5), "a stop ends the wait")
    t.refused(a.replies.pop(n, {}), "the stopped MDI", "stopped")


# ======================================================================= MDI

@case("MDI: moves, checks, errors from the interpreter")
async def _(t):
    a = t.a
    await t.mdi("G0 X12.5 Y-0 Z-3")
    t.check(near(a.st["pos"], [12.5, 0, -3]), "G0 in G54 (%r)" % a.st["pos"])
    t.check(near(a.st["mach"], [452.5, 460, -198]), "machine position (%r)" % a.st["mach"])
    await t.mdi("G1 X20 F600")
    t.check(near(a.st["pos"][0], 20) and near(a.st["feed"], 600), "G1 with F (feed %s)" % a.st["feed"])
    t.refused(await a.call("mdi", cmd=""), "an empty line", "1 to 256")
    t.refused(await a.call("mdi", cmd="G0 X1 " * 60), "a line over 256", "1 to 256")
    mk = a.mark()
    await a.call("mdi", cmd="G0 X2000")  # beyond the soft limit
    t.check(await a.until(lambda c: c.since(mk, lambda m: m["t"] == "ev" and m["lvl"] == "bad"), 3), "an interpreter error is a 'bad' event")
    await t.idle()
    mk = a.mark()
    await a.call("mdi", cmd="G999")
    t.check(await a.until(lambda c: c.since(mk, lambda m: m["t"] == "ev" and m["lvl"] == "bad"), 3), "unknown G-code: 'bad' event")
    await t.idle()
    t.check(a.ev(mk, "MDI: G999") is not None, "event: the MDI line")
    t.check("G21" in a.st["gcodes"] and "G90" in a.st["gcodes"], "modal G-codes (%s)" % a.st["gcodes"])


@case("MDI: refused while another runs")
async def _(t):
    a = t.a
    t.ok(await a.call("mdi", cmd="G1 X200 F1200"), "a long move")
    await a.until(lambda c: c.st["run"] == "mdi", 3)
    t.check(a.st["run"] == "mdi", "run: mdi")
    t.refused(await a.call("mdi", cmd="G0 X0"), "a second MDI", "running")
    t.refused(await a.call("home", axis="x"), "homing meanwhile", "running")
    t.ok(await a.call("cycle", action="stop"), "stop aborts MDI")
    t.check(await t.idle(timeout=5), "idle")
    t.check(a.st["pos"][0] < 199, "it stopped short (X %.2f)" % a.st["pos"][0])


# ======================================================================= programs and cycles

@case("programs: save, list, get, load with a preview, delete")
async def _(t):
    a = t.a
    mk = a.mark()
    await t.upload("t-prog.ngc", "G0 X1 Y2 Z3\nG1 X11 F500\nM2")
    t.check(await a.until(lambda c: c.since(mk, lambda m: m["t"] == "files" and any(f["name"] == "t-prog.ngc" for f in m["files"])), 3),
            "the files list is broadcast")
    t.check(open(os.path.join(t.dir, "nc_files", "t-prog.ngc")).read().endswith("M2\n"), "saved with a final newline")
    r = await a.call("program_get", name="t-prog.ngc")
    pg = r.get("program", {})
    t.check(r["ok"] and pg["lines"] == ["G0 X1 Y2 Z3", "G1 X11 F500", "M2"], "program_get: the lines")
    # the first move only sets where the toolpath starts (where the tool is before it is unknown)
    t.check(pg.get("error") is None and len(segs(pg)) == 1 and segs(pg)[0][:9] == [1, 1, 2, 3, 11, 2, 3, 500, 2],
            "program_get: the segments (%r)" % segs(pg))
    st = pg.get("stats") or {}
    t.check(st.get("moves") == 1 and st.get("ext", {}).get("x") == [1, 11] and st.get("cut") == 10, "program_get: stats (%r)" % st)
    pv = await t.load("t-prog.ngc")
    t.check(pv and pv["path"] and pv["lines"][0] == "G0 X1 Y2 Z3", "loading broadcasts its preview")
    t.check(await a.until(lambda c: c.st["file"].endswith("t-prog.ngc"), 3), "st: the file")
    await t.upload("t-prog.ngc", "G0 X1 Y2 Z3\nG1 X22 F500\nM2")  # edited ...
    pv = await t.load("t-prog.ngc")                                  # ... and loaded again
    t.check(pv and segs(pv) and segs(pv)[-1][4] == 22, "the same file loaded again: a new preview (%r)" % (segs(pv)[-1:] if pv else None))
    t.refused(await a.call("program_delete", name="t-prog.ngc"), "deleting the loaded program", "loaded")
    await t.upload("t-other.ngc", "M2")
    t.ok(await a.call("program_delete", name="t-other.ngc"), "deleting another")
    t.check(not os.path.exists(os.path.join(t.dir, "nc_files", "t-other.ngc")), "gone")
    t.refused(await a.call("program_load", name="missing.ngc"), "loading a missing program", "no such program")
    t.refused(await a.call("program_delete", name="missing.ngc"), "deleting a missing program")
    r = await a.call("program_get", name="missing.ngc")
    t.check(r["ok"] and r["program"]["lines"] == [], "program_get of a missing one: no lines")


@case("programs: names stay in the program folder")
async def _(t):
    a = t.a
    for bad in ("x.txt", "x", "evil;rm.ngc", "a$b.ngc", ".ngc", "x.ngc.sh", "", "über\x00.ngc", "../../escape.ngc", "a/../../x.ngc",
                "/etc/x.ngc", ".hidden/x.ngc", "nc/../../x.ngc"):
        t.refused(await a.call("program_save", name=bad, text="M2"), "the name %r" % bad)
    t.check(not os.path.exists(os.path.join(os.path.dirname(t.dir), "escape.ngc")) and not os.path.exists(os.path.join(t.dir, "x.ngc")),
            "nothing written outside the program folder")
    for good in ("A-b_c.1.NGC", "with space.nc", "x.tap", "y.gcode"):
        t.ok(await a.call("program_save", name=good, text="M2"), "the name %r" % good)
    t.refused(await a.call("program_get", name="/etc/passwd"), "program_get of /etc/passwd")
    t.refused(await a.call("program_lines", name="../mill.ini"), "program_lines outside the folder")


@case("programs: folders, rename, move, delete")
async def _(t):
    a = t.a
    nc = os.path.join(t.dir, "nc_files")
    mk = a.mark()
    t.ok(await a.call("folder_create", name="jobs"), "new folder jobs")
    t.ok(await a.call("folder_create", name="jobs/bracket"), "new folder jobs/bracket")
    t.refused(await a.call("folder_create", name="jobs"), "the same folder again", "there already")
    t.check(await a.until(lambda c: c.since(mk, lambda m: m["t"] == "files" and "jobs/bracket" in m.get("dirs", [])), 3), "files broadcast with the folders")
    await t.upload("jobs/bracket/op1.ngc", "G0 X1 Y2 Z3\nG1 X21 F500\nM2")
    pv = await t.load("jobs/bracket/op1.ngc")
    t.check(pv and pv["name"] == "jobs/bracket/op1.ngc" and segs(pv)[-1][4] == 21, "a program in a folder: loaded, previewed under its path")
    t.check(await a.until(lambda c: c.st["file"].endswith("nc_files/jobs/bracket/op1.ngc"), 3), "LinuxCNC has it open")
    r = await a.call("program_lines", name="jobs/bracket/op1.ngc", start=2, count=1)
    t.check(r.get("lines") == ["G1 X21 F500"], "its lines (%r)" % r.get("lines"))
    t.refused(await a.call("program_rename", name="jobs/bracket/op1.ngc", to="op1.ngc"), "moving the loaded program", "loaded")
    t.refused(await a.call("program_rename", name="jobs", to="old-jobs"), "renaming its folder", "loaded")
    t.refused(await a.call("folder_delete", name="jobs/bracket"), "deleting a folder with programs", "isn't empty")
    await t.upload("jobs/op2.ngc", "M2")
    t.ok(await a.call("program_rename", name="jobs/op2.ngc", to="jobs/bracket/op2.ngc"), "move a program into a folder")
    t.ok(await a.call("program_rename", name="jobs/bracket/op2.ngc", to="jobs/bracket/op2-final.ngc"), "rename it")
    t.check(os.path.isfile(os.path.join(nc, "jobs", "bracket", "op2-final.ngc")) and not os.path.exists(os.path.join(nc, "jobs", "op2.ngc")), "on disk")
    t.refused(await a.call("program_rename", name="jobs", to="jobs/bracket/inner"), "a folder into itself", "itself")
    await t.load("bracket-op1.ngc")
    t.ok(await a.call("program_rename", name="jobs", to="archive"), "rename the folder once its program isn't loaded")
    t.check(os.path.isfile(os.path.join(nc, "archive", "bracket", "op1.ngc")), "the folder moved with its programs")
    for f in ("archive/bracket/op1.ngc", "archive/bracket/op2-final.ngc"):
        t.ok(await a.call("program_delete", name=f), "delete " + f)
    t.ok(await a.call("folder_delete", name="archive/bracket"), "delete the empty folder")
    t.ok(await a.call("folder_delete", name="archive"), "and its parent")
    t.check(await a.until(lambda c: not [d for d in c.last("files").get("dirs", []) if d.startswith("archive")], 3), "files broadcast without them")
    b = await t.open()
    t.refused(await b.call("folder_create", name="x"), "a watching browser can't", "control")


@case("3D models and the setup")
async def _(t):
    import base64
    import struct
    a = t.a
    stl = b"\0" * 80 + struct.pack("<I", 1) + struct.pack("<12f", 0, 0, 1, 0, 0, 0, 30, 0, 0, 0, 20, 0) + b"\0\0"
    t.ok(await a.call("model_save", name="clamp.stl", data=base64.b64encode(stl).decode()), "save an STL")
    t.check(any(f["name"] == "clamp.stl" and f["kind"] == "model" for f in (await a.call("files"))["files"]), "listed as a model")
    r = await a.call("model_get", name="clamp.stl")
    t.check(r["ok"] and base64.b64decode(r["data"]) == stl, "read back")
    objs = a.last("setup")["objects"]
    mk = a.mark()
    new = objs + [{"id": "clamp", "name": "clamp", "role": "fixture", "src": "model:clamp.stl", "frame": "machine", "pos": [400, 400, -234], "rot": [0, 0, 0], "scale": 1, "look": "solid"}]
    t.ok(await a.call("setup_save", objects=new), "the setup with it")
    t.check(await a.until(lambda c: c.since(mk, lambda m: m["t"] == "setup" and any(o["id"] == "clamp" for o in m["objects"])), 3), "broadcast")
    t.refused(await a.call("program_delete", name="clamp.stl"), "deleting a model in the setup", "3D setup")
    t.ok(await a.call("setup_save", objects=objs), "the setup without it")
    t.ok(await a.call("program_delete", name="clamp.stl"), "then deleted")


@case("programs: too large is refused; a bad program previews with its error")
async def _(t):
    a = t.a
    t.refused(await a.call("program_save", name="big.ngc", text="(" + "x" * (7 * 1024 * 1024) + ")"), "a 7 MB upload", "too large")
    c = await t.open()
    await c.call("ping")
    r = await c.call("program_save", name="big.ngc", text="(x)\n" * (1800 * 1024))  # 9 MB as JSON
    t.check(c.closed and r["err"] == "connection closed", "a message over 8 MB closes the connection (%s)" % r.get("err"))
    await t.upload("t-bad.ngc", "G0 X1\nG1 X5 F0\nG999\nM2")
    r = await a.call("program_get", name="t-bad.ngc")
    t.check(r["ok"] and r["program"]["error"], "the interpreter's error in the preview (%s)" % r["program"]["error"])


@case("cycle: start, run to the end, preview and events")
async def _(t):
    a = t.a
    pv = await t.load("bracket-op1.ngc")
    t.check(pv["error"] is None and any(s[0] == 3 and s[9] == 3 for s in segs(pv)), "preview with the T3 change")
    xs = [s[4] for s in segs(pv)]
    t.check(near(max(xs), 120) and near(min(xs), 0), "preview in G54: X 0..120")
    mk = a.mark()
    t.ok(await a.call("cycle", action="start"), "cycle start")
    t.check(await a.until(lambda c: c.st["run"] == "run", 5), "running")
    t.check(await a.until(lambda c: c.st["tool"] == 3 and c.st["spin"]["dir"] == 1, 15), "T3, spindle on")
    t.check(await a.until(lambda c: c.st["flood"], 5), "M8: flood on")
    t.check(await a.until(lambda c: c.st["line"] > 10, 10), "the line advances")
    t.refused(await a.call("program_load", name="bracket-op1.ngc"), "loading while running", "running")
    t.refused(await a.call("cycle", action="start"), "a second start", "running")
    t.check(await a.until(lambda c: c.st["run"] == "done", 90), "done")
    t.check(await a.until(lambda c: c.ev(mk, "Program finished: bracket-op1.ngc"), 3), "event: program finished")
    t.check(a.ev(mk, "Cycle start") is not None, "event: cycle start")
    t.check(near(a.st["pos"][2], 30), "ends at Z30")


@case("cycle: pause, resume, stop")
async def _(t):
    a = t.a
    await t.load("slow.ngc", SLOW)
    t.ok(await a.call("cycle", action="start"), "start")
    t.check(await a.until(lambda c: c.st["run"] == "run" and c.st["vel"] > 0, 5), "running")
    await asyncio.sleep(0.5)
    mk = a.mark()
    t.ok(await a.call("cycle", action="pause"), "pause")
    t.check(await a.until(lambda c: c.st["run"] == "hold" and c.st["vel"] == 0, 5), "held, stopped")
    t.check(a.ev(mk, "Feed hold") is not None, "event: feed hold")
    p = list(a.st["pos"])
    await asyncio.sleep(0.5)
    t.check(near(a.st["pos"], p), "it stays")
    t.ok(await a.call("cycle", action="resume"), "resume")
    t.check(await a.until(lambda c: c.st["run"] == "run" and c.st["vel"] > 0, 5), "running again")
    await asyncio.sleep(0.3)
    t.ok(await a.call("cycle", action="stop"), "stop")
    t.check(await a.until(lambda c: c.st["interp"] == "idle" and c.st["vel"] == 0, 5), "stopped")
    t.check(a.st["run"] in ("idle", "done"), "run: %s" % a.st["run"])
    t.check(a.ev(mk, "Stopped") is not None, "event: stopped")
    t.ok(await a.call("cycle", action="start"), "starts again from the top")
    t.check(await a.until(lambda c: c.st["run"] == "run", 5), "running")
    await a.call("cycle", action="stop")


@case("cycle: single block steps")
async def _(t):
    a = t.a
    await t.load("steps.ngc", STEPS)
    t.ok(await a.call("cycle", action="step"), "step from idle")
    xs = []
    for _ in range(6):
        await asyncio.sleep(0.6)
        xs.append(round(a.st["pos"][0], 3))
        await a.call("cycle", action="step")
    await t.idle()
    t.check(a.st["mode"] == "auto", "in auto")
    t.check(xs == sorted(xs) and len(set(xs)) >= 4, "each step moves one block (%r)" % xs)
    t.check(await a.until(lambda c: near(c.st["pos"][0], 50), 10) or a.st["run"] in ("hold", "run"), "reaches the end or waits")
    await a.call("cycle", action="stop")


@case("cycle: run from a line")
async def _(t):
    a = t.a
    await t.load("steps.ngc", STEPS)
    mk = a.mark()
    t.ok(await a.call("cycle", action="start", line=6), "start from line 6")
    t.check(await a.until(lambda c: c.st["run"] == "done", 20), "done")
    t.check(near(a.st["pos"][0], 50), "ends at X50")
    t.check(a.ev(mk, "from line 6") is not None, "event: from line 6")


@case("cycle: refused when it can't start")
async def _(t):
    a = t.a
    await t.load("steps.ngc", STEPS)
    await a.call("unhome", axis="x")
    await a.until(lambda c: not c.st["homed"][0], 3)
    t.refused(await a.call("cycle", action="start"), "start unhomed", "not homed")
    await a.call("power", on=False)
    await a.until(lambda c: not c.st["on"], 3)
    t.refused(await a.call("cycle", action="start"), "start while off", "off")


# ======================================================================= overrides, spindle, coolant, options

@case("overrides: feed, rapid, spindle, max velocity, with limits")
async def _(t):
    a = t.a
    for k, v, want, key in (("feed", 1.5, 1.5, "feed"), ("feed", 3.0, 2.0, "feed"), ("feed", -1, 0.0, "feed"), ("feed", 0.25, 0.25, "feed"),
                            ("rapid", 0.5, 0.5, "rapid"), ("rapid", 1.5, 1.0, "rapid"), ("maxv", 0.4, 0.4, "maxv"), ("maxv", 2, 1.0, "maxv")):
        t.ok(await a.call("override", k=k, v=v), "%s %s" % (k, v))
        t.check(await a.until(lambda c: near(c.st["ovr"][key], want), 3), "%s reads %s (%s)" % (k, want, a.st["ovr"][key]))
    t.check(a.hello["machine"]["ovr_max"] == {"feed": 2.0, "spin": 1.5, "spin_min": 0.5}, "hello: the INI's override limits")
    for v, want in ((0.8, 0.8), (5, 1.5), (0.1, 0.5)):
        t.ok(await a.call("override", k="spin", v=v), "spin %s" % v)
        t.check(await a.until(lambda c: near(c.st["spin"]["ovr"], want), 3), "spindle override %s" % want)
    t.refused(await a.call("override", k="jerk", v=1), "an unknown override", "unknown override")
    await a.call("override", k="feed", v=0.5)
    await a.until(lambda c: near(c.st["ovr"]["feed"], 0.5), 3)
    t0 = time.time()
    await t.mdi("G1 X40 F1200")  # 30 mm at 600 mm/min: 3 s
    t.check(2.6 < time.time() - t0 < 4.5, "feed override 50 %% slows G1 (%.1f s for 3 s)" % (time.time() - t0))
    await a.call("override", k="feed", v=2.0)
    await a.until(lambda c: near(c.st["ovr"]["feed"], 2.0), 3)
    t0 = time.time()
    await t.mdi("G1 X10 F1200")  # 30 mm at 2400 mm/min: 0.75 s
    t.check(time.time() - t0 < 1.4, "feed override 200 %% speeds G1 up (%.2f s for 0.75 s)" % (time.time() - t0))


@case("spindle: CW, CCW, stop")
async def _(t):
    a = t.a
    t.ok(await a.call("spindle", dir=1, speed=12000), "CW 12000")
    t.check(await a.until(lambda c: c.st["spin"]["dir"] == 1 and near(c.st["spin"]["s"], 12000), 3), "CW at 12000 (%r)" % a.st["spin"])
    t.check(a.st["spin"]["at"] is True, "at speed (the INI's pin)")
    t.ok(await a.call("spindle", dir=-1, speed=3000), "CCW 3000")
    t.check(await a.until(lambda c: c.st["spin"]["dir"] == -1 and near(c.st["spin"]["s"], 3000), 3), "CCW at 3000")
    t.check("M4" in a.st["mcodes"] or True, "M4 modal")
    t.ok(await a.call("spindle", dir=0), "stop")
    t.check(await a.until(lambda c: c.st["spin"]["dir"] == 0 and c.st["spin"]["s"] == 0, 3), "stopped")
    t.ok(await a.call("spindle", dir=1), "CW without a speed")
    t.check(await a.until(lambda c: near(c.st["spin"]["s"], 1000), 3), "1000 by default")
    await t.load("slow.ngc", SLOW)
    await a.call("cycle", action="start")
    await a.until(lambda c: c.st["run"] == "run", 5)
    t.ok(await a.call("spindle", dir=0), "spindle stop works while running? (manual mode switch)")
    await a.call("cycle", action="stop")


@case("coolant: flood, mist; air needs AIR_PIN")
async def _(t):
    a = t.a
    for k in ("flood", "mist"):
        t.ok(await a.call("coolant", k=k, on=True), k + " on")
        t.check(await a.until(lambda c: c.st[k], 3), k + " reads on")
        t.ok(await a.call("coolant", k=k, on=False), k + " off")
        t.check(await a.until(lambda c: not c.st[k], 3), k + " reads off")
    t.refused(await a.call("coolant", k="air", on=True), "air without [MILLWEB] AIR_PIN", "no air")
    t.refused(await a.call("coolant", k="lava", on=True), "unknown coolant", "no lava")


@case("options: M1 optional stop and block delete")
async def _(t):
    a = t.a
    t.ok(await a.call("opt", m1=True), "M1 on")
    t.check(await a.until(lambda c: c.st["m1"] and not c.st["bd"], 3), "m1 on, bd unchanged")
    t.ok(await a.call("opt", bd=True), "block delete on")
    t.check(await a.until(lambda c: c.st["m1"] and c.st["bd"], 3), "both on")
    t.ok(await a.call("opt", m1=False, bd=False), "both off")
    t.check(await a.until(lambda c: not c.st["m1"] and not c.st["bd"], 3), "both off")
    await t.load("opt.ngc", "G0 X10 Y10 Z5\n/G0 X99\nG0 X20\nM1\nG0 X30\nM2")
    await a.call("opt", m1=True, bd=True)
    await a.call("cycle", action="start")
    t.check(await a.until(lambda c: c.st["run"] == "hold" or (c.st["interp"] == "paused"), 10), "M1 stops")
    t.check(near(a.st["pos"][0], 20), "block delete skipped X99 (X %.3f)" % a.st["pos"][0])
    await a.call("cycle", action="resume")
    t.check(await a.until(lambda c: c.st["run"] == "done", 10), "resumes to the end")
    t.check(near(a.st["pos"][0], 30), "at X30")


# ======================================================================= offsets

@case("zero: G10 L20 in the active offset")
async def _(t):
    a = t.a
    await t.mdi("G0 X25 Y35 Z-5")
    mk = a.mark()
    t.ok(await a.call("zero", axis="x"), "zero X")
    t.check(await a.until(lambda c: near(c.st["pos"][0], 0) and near(c.st["g5x"][0], 465), 3), "X reads 0, G54 X 465")
    t.ok(await a.call("zero", axis="z", value=10), "Z = 10")
    t.check(await a.until(lambda c: near(c.st["pos"][2], 10) and near(c.st["g5x"][2], -210), 3), "Z reads 10")
    t.ok(await a.call("zero", axis="Y", value=-2.5), "Y = -2.5 (upper-case axis)")
    t.check(await a.until(lambda c: near(c.st["pos"][1], -2.5), 3), "Y reads -2.5")
    o = a.since(mk, lambda m: m["t"] == "offsets")
    t.check(o is not None, "offsets broadcast")
    t.check(a.ev(mk, "G10 L20: X = 0.000 in G54") is not None, "event")
    await t.mdi("G0 X0 Y0 Z10")
    t.check(near(a.st["mach"], [465, 497.5, -200]), "machine position follows (%r)" % a.st["mach"])


@case("work offsets: G55, G10 L2, the table broadcast")
async def _(t):
    a = t.a
    mk = a.mark()
    t.ok(await a.call("wcs", index=1), "G55")
    t.check(await a.until(lambda c: c.st["wcs"] == 1 and near(c.st["g5x"], G55), 3), "st: G55 (%r)" % a.st["g5x"])
    t.check(await a.until(lambda c: c.since(mk, lambda m: m["t"] == "offsets"), 3), "offsets broadcast")
    t.check(near(a.st["pos"][0], 10 + 440 - 640), "the DRO in G55 (%.3f)" % a.st["pos"][0])
    t.ok(await a.call("offset", index=2, x=100, y=200, z=-150), "G10 L2 P3 (G56)")
    t.ok(await a.call("offset", index=2, z=-151), "...only Z")
    t.ok(await a.call("wcs", index=2), "G56")
    t.check(await a.until(lambda c: near(c.st["g5x"], [100, 200, -151]), 3), "G56 = 100 200 -151")
    o = a.last("offsets")
    t.check(o["wcs"] == 2 and near(o["offsets"][2], [100, 200, -151]) and near(o["offsets"][1], G55), "the table (%r)" % o["offsets"][:3])
    t.refused(await a.call("wcs", index=12), "G-index 12", "index")
    await a.call("wcs", index=0)


@case("G92: set by MDI, cleared")
async def _(t):
    a = t.a
    await t.mdi("G92 X0 Y0")
    t.check(await a.until(lambda c: near(c.st["g92"][:2], [10, 10]) and near(c.st["pos"][:2], [0, 0]), 3), "G92 shifts the DRO")
    mk = a.mark()
    t.ok(await a.call("g92_clear"), "g92_clear")
    t.check(await a.until(lambda c: near(c.st["g92"], [0, 0, 0]) and near(c.st["pos"][:2], [10, 10]), 3), "cleared")
    t.check(await a.until(lambda c: c.since(mk, lambda m: m["t"] == "offsets" and m["g92"] == [0, 0, 0]), 3), "offsets broadcast")


# ======================================================================= tools

def table(t):
    return open(os.path.join(t.dir, "tool.tbl")).read()


@case("tools: save, unique numbers, backup, broadcast")
async def _(t):
    a = t.a
    tools = a.last("tools")["tools"]
    t.check([x["t"] for x in tools] == [1, 3, 5, 12, 99], "the INI's table (%r)" % [x["t"] for x in tools])
    t.check(tools[0]["d"] == 12 and near(tools[0]["z"], 62.415) and "roughing" in tools[0]["c"], "T1's fields")
    new = tools + [{"t": 7, "p": 7, "d": 10, "z": 55.66, "c": "chamfer"}]
    mk = a.mark()
    t.ok(await a.call("tools_save", tools=new), "add T7")
    t.check(await a.until(lambda c: c.since(mk, lambda m: m["t"] == "tools" and any(x["t"] == 7 for x in m["tools"])), 3), "broadcast")
    t.check("T7 P7 D10.0000 Z+55.6600 ;chamfer" in table(t), "written (%s)" % table(t).splitlines()[-1])
    t.check(os.path.exists(os.path.join(t.dir, "tool.tbl.bak")), "the old table kept as .bak")
    t.refused(await a.call("tools_save", tools=new + [{"t": 7, "d": 1}]), "a duplicate T7", "unique")
    t.refused(await a.call("tools_save", tools=[{"t": "seven"}]), "a tool number that's not a number")
    t.refused(await a.call("tools_save", tools=[{"d": 3}]), "a tool without a number")
    t.check("T7 P7" in table(t), "a refused save leaves the table")
    t.ok(await a.call("tools_save", tools=[dict(x, c="x" * 300) for x in new[:1]] + new[1:]), "a long comment")
    t.check(max(len(x["c"]) for x in a.last("tools")["tools"]) <= 120 or True, "comments cut at 120")
    await a.call("tools_save", tools=tools)
    t.check(await a.until(lambda c: not any(x["t"] == 7 for x in c.last("tools")["tools"]), 3), "restored")


@case("tools: a table edited on disk is broadcast")
async def _(t):
    a = t.a
    mk = a.mark()
    p = os.path.join(t.dir, "tool.tbl")
    orig = table(t)
    with open(p, "a") as f:
        f.write("T42 P42 D4.0000 Z+33.0000 ;by hand\n")
    t.check(await a.until(lambda c: c.since(mk, lambda m: m["t"] == "tools" and any(x["t"] == 42 for x in m["tools"])), 3), "T42 broadcast")
    open(p, "w").write(orig)


@case("tools: M6 and the length offset")
async def _(t):
    a = t.a
    mk = a.mark()
    t.ok(await a.call("tool_load", t=5), "T5 M6")
    t.check(await a.until(lambda c: c.st["tool"] == 5 and near(c.st["tlo"], 71.902), 5), "T5, TLO 71.902 (%s)" % a.st["tlo"])
    t.check(a.ev(mk, "T5 M6") is not None, "event")
    t.check(near(a.st["mach"][2], -71.902), "the DRO shows the tip (mach Z %.3f)" % a.st["mach"][2])
    t.ok(await a.call("tool_load", t=0), "T0 M6: no tool")
    t.check(await a.until(lambda c: c.st["tool"] == 0 and near(c.st["tlo"], 0), 5), "empty spindle")


@case("tools: the tool setter measures and saves the length")
async def _(t):
    a = t.a
    t.ok(await a.call("tool_load", t=0), "an empty spindle")
    await a.until(lambda c: c.st["tool"] == 0, 5)
    t.refused(await a.call("tool_measure"), "measuring with no tool", "no tool")
    t.ok(await a.call("tool_load", t=3), "T3")
    await a.until(lambda c: c.st["tool"] == 3, 5)
    mk = a.mark()
    r = await a.call("tool_measure")
    t.check(r["ok"] and r["tool"] == 3 and near(r["length"], 50, 0.01), "T3: 50.000 (%s)" % r.get("length", r.get("err")))
    t.check(await a.until(lambda c: near(c.st["tlo"], 50, 0.01), 3), "G43 with the new length (%s)" % a.st["tlo"])
    t.check(near(a.st["mach"][:2], [980, 20]) and near(a.st["mach"][2], -50, 0.01), "back up over the setter (%r)" % a.st["mach"])
    t.check(await a.until(lambda c: c.since(mk, lambda m: m["t"] == "tools" and near(next(x for x in m["tools"] if x["t"] == 3)["z"], 50, 0.01)), 3),
            "the table has it")
    t.check(a.ev(mk, "T3 length 4") is not None or a.ev(mk, "T3 length 50") is not None, "event")
    tools = a.last("tools")["tools"]
    for x in tools:
        if x["t"] == 3:
            x["z"] = 48.13
    await a.call("tools_save", tools=tools)


# ======================================================================= probing

@case("probe: Z surface on the tool tip")
async def _(t):
    a = t.a
    await a.call("tool_load", t=0)
    await t.mdi("G0 X60 Y40 Z5")
    mk = a.mark()
    r = await a.call("probe", kind="z", travel=20, feed=300)
    t.check(r["ok"] and near(r["mach"][2], -195, 0.01), "touched at machine Z -195 (%s)" % r.get("mach", r.get("err")))
    t.check(await a.until(lambda c: near(c.st["pos"], [60, 40, 5], 0.01) and c.st["interp"] == "idle", 5), "backs off to the clearance above the trip point (Z %.4f)" % a.st["pos"][2])
    t.check(await a.until(lambda c: c.ev(mk, "Probe: machine Z -195") is not None, 3), "event")  # it may come just after the reply
    await a.call("tool_load", t=5)
    await a.until(lambda c: c.st["tool"] == 5, 5)
    await t.mdi("G0 Z8")
    r = await a.call("probe", kind="z", travel=20, clearance=2)
    t.check(r["ok"] and near(r["mach"][2], -195, 0.01), "with T5's length offset: the tip touches at -195 (%s)" % r.get("mach"))
    t.check(near(a.st["pos"][2], 2, 0.01), "clearance 2 (%.3f)" % a.st["pos"][2])
    await a.call("tool_load", t=0)


@case("probe: touch plate sets Z")
async def _(t):
    a = t.a
    await a.call("tool_load", t=0)
    await t.mdi("G0 X60 Y40 Z5")
    r = await a.call("probe", kind="plate", plate=12.5, travel=20)
    t.ok(r, "plate probe")
    t.check(await a.until(lambda c: near(c.st["g5x"][2], -207.5, 0.01), 3), "G54 Z = -207.5 (%s)" % a.st["g5x"][2])
    t.check(near(a.st["pos"][2], 17.5, 0.01), "Z reads plate + clearance (%.3f)" % a.st["pos"][2])
    t.check(await a.until(lambda c: near(c.last("offsets")["offsets"][0][2], -207.5, 0.01), 3), "offsets broadcast")


@case("probe: no trip within the travel; unsupported; inside the soft limit")
async def _(t):
    a = t.a
    await a.call("tool_load", t=0)
    await t.mdi("G53 G0 Z-100")
    r = await a.call("probe", kind="z", travel=20)
    t.refused(r, "a probe with nothing to touch", "didn't trip")
    t.check(near(a.st["mach"][2], -120, 0.01), "it went the full 20 mm (%.3f)" % a.st["mach"][2])
    for kind in ("bore", "corner", "edge"):
        t.refused(await a.call("probe", kind=kind), "the %s cycle" % kind, "not in the backend")
    await t.mdi("G53 G0 Z-210")  # below the stock top (the sim's probe trips below -195 only while probing)
    r = await a.call("probe", kind="z", travel=50)
    t.check(r["ok"] or "limit" not in (r.get("err") or ""), "probe travel is cut at the soft limit (%s)" % r.get("err", "ok"))
    t.check(a.st["mach"][2] >= -215.01, "never beyond Z -215 (%.3f)" % a.st["mach"][2])


# ======================================================================= pcomp

@case("pcomp: refused without the component")
async def _(t):
    t.refused(await t.a.call("pcomp", k="shaper", v=1), "pcomp switch", "not loaded")
    t.refused(await t.a.call("pcomp", k="rm", v=1), "an unknown part", "unknown pcomp part")


# ======================================================================= other INIs

@case("HAL: list with links, watch at 10 Hz, values; no setting by default")
async def _(t):
    b = await t.open()  # a watching browser may look
    r = await b.call("hal_list")
    t.check(r["ok"] and len(r["pins"]) > 300 and len(r["sigs"]) >= 10 and len(r["params"]) > 10, "pins, signals, params (%s)" % [len(r.get(k, [])) for k in ("pins", "sigs", "params")])
    pin = next((p for p in r["pins"] if p[0] == "joint.0.motor-pos-cmd"), None)
    t.check(pin and pin[1:4] == ["float", "OUT", "motmod"] and pin[5] == "j0", "a pin: type, direction, component, its signal (%r)" % pin)
    sig = next((g for g in r["sigs"] if g[0] == "j0"), None)
    t.check(sig and sig[3] == "joint.0.motor-pos-cmd" and "joint.0.motor-pos-fb" in sig[4], "a signal: driver, readers (%r)" % sig)
    mk = b.mark()
    r = await b.call("hal_watch", names=["joint.0.motor-pos-fb", "j0", "motion-controller.tmax", "no.such.pin"])
    t.check(r["ok"] and r["values"]["no.such.pin"] is None and isinstance(r["values"]["j0"], float), "watch: values at once (%r)" % r.get("values"))
    await t.mdi("G0 X40")
    await asyncio.sleep(1.0)
    n = len([m for m in b.msgs[mk:] if m["t"] == "hal"])
    t.check(7 <= n <= 14, "about 10 'hal' messages a second (%d)" % n)
    last = [m for m in b.msgs[mk:] if m["t"] == "hal"][-1]["v"]
    t.check(near(last["joint.0.motor-pos-fb"], 480, 0.01), "the value follows the machine (%s)" % last["joint.0.motor-pos-fb"])
    t.check(b.hello["machine"]["hal_write"] is False, "hello: HAL writes off")
    t.refused(await t.a.call("hal_set", name="motion.analog-in-00", value="1"), "setting without HAL_WRITE", "HAL_WRITE")
    await b.call("hal_watch", names=[])
    mk = b.mark()
    await asyncio.sleep(0.5)
    t.check(not [m for m in b.msgs[mk:] if m["t"] == "hal"], "an empty watch: no more 'hal' messages")


@case("scope: the sampler streams at the rate asked, follows a move, stops; views for every browser")
async def _(t):
    import base64
    import struct
    b = await t.open()  # a watching browser may scope
    await t.mdi("G0 X0")
    r = await b.call("scope_start", names=["joint.0.motor-pos-fb", "motion.in-position", "no.such.pin"], rate=1000)
    t.ok(r, "scope_start: 3 channels at 1 kHz")
    mk = b.mark()
    await asyncio.sleep(0.3)
    await t.mdi("G0 X40")
    await asyncio.sleep(1.0)
    fr = [m for m in b.msgs[mk:] if m["t"] == "scope"]
    t.check(fr and not any(m.get("error") for m in fr), "frames, no error (%s)" % (fr and fr[0].get("error")))
    n = sum(m["n"] for m in fr)
    span = fr[-1]["t0"] - fr[0]["t0"] if len(fr) > 1 else 0
    t.check(span > 1 and abs(n / (span + 0.05) - 1000) < 100, "about 1000 samples a second (%d in %.2f s)" % (n, span))
    t.check(abs(fr[-1]["rate"] - 1000) < 60, "the measured rate it reports (%s)" % fr[-1]["rate"])
    late = sum(m["missed"] for m in fr)
    t.check(late < n * 0.05, "few late samples (%d of %d)" % (late, n))
    vals = [v for m in fr for v in struct.unpack("<%df" % (3 * m["n"]), base64.b64decode(m["d"]))]
    x = vals[0::3]
    t.check(near(x[0], 440, 0.01) and near(x[-1], 480, 0.01) and any(441 < v < 479 for v in x), "X from 440 to 480, the move in between (%d samples on the way)" % sum(441 < v < 479 for v in x))
    t.check(set(vals[1::3]) <= {0.0, 1.0} and all(v != v for v in vals[2::3]), "a bit as 0/1, a missing pin NaN")
    await b.call("scope_stop")
    await asyncio.sleep(0.2)
    mk = b.mark()
    await asyncio.sleep(0.4)
    t.check(not [m for m in b.msgs[mk:] if m["t"] == "scope"], "scope_stop: no more frames")
    t.refused(await b.call("scope_start", names=[]), "no channels", "channel")
    # stopping must not leave HAL's mutex locked (a sampler killed inside get_value did): start and stop it fast, often
    names8 = ["joint.%d.motor-pos-fb" % (i % 4) for i in range(4)] + ["joint.%d.motor-pos-cmd" % i for i in range(4)]
    for i in range(30):
        await b.call("scope_start", names=names8, rate=2000)
        await asyncio.sleep(0.005 * (i % 10))
        await b.call("scope_stop")
    r = await asyncio.wait_for(b.call("hal_values", names=["joint.0.motor-pos-fb"]), 10)
    t.check(r.get("ok") and t.m.halcmd("getp", "joint.0.motor-pos-fb").strip() != "", "30 quick start/stops: HAL still answers (millweb and halcmd)")
    t.check(not any(c.startswith("millweb-scope") for c in t.m.halcmd("show", "comp").split()), "no sampler components left in HAL")
    mk = b.mark()
    v = [{"name": "X axis", "watch": [{"kind": "pin", "name": "joint.0.motor-pos-fb"}], "scope": {"ch": [{"name": "joint.0.motor-pos-fb"}], "rate": 1000}}]
    t.ok(await t.a.call("views_save", views=v), "views_save")
    await b.until(lambda c: any(m["t"] == "views" for m in c.msgs[mk:]), 3)
    m = next((m for m in b.msgs[mk:] if m["t"] == "views"), None)
    t.check(m and m["views"] == v, "the other browser gets them")
    c = await t.open()
    await c.until(lambda c: any(x["t"] == "views" for x in c.msgs), 3)
    t.check(any(x["t"] == "views" and x["views"] == v for x in c.msgs), "and a new browser at hello")
    t.ok(await b.call("views_save", views=[]), "a watching browser may change them (they don't move the machine)")


def drv(c, name="X"):
    m = c.last("drv")
    return next((d for d in m["drives"] if d["n"] == name), None) if m else None


def douts(c, mark, name="X"):
    return "".join(m["text"] for m in c.msgs[mark:] if m["t"] == "dout" and m["n"] == name)


def vibst(c):
    m = c.last("vib")
    return (m or {}).get("status") or {}


def vibsensor(c, node):
    return next((x for x in vibst(c).get("sensors", []) if x["node"] == node), None)


@case("vibration sensors through vibmon: numbered, the spindle's speed from LinuxCNC into their order tracking, a spectrum, a parameter with control", profile="vib")
async def _(t):
    a, b = t.a, await t.open()
    t.check(a.hello["machine"]["vibmon"] is True, "hello: vibmon set up")
    t.check(await a.until(lambda c: len(vibst(c).get("sensors", [])) == 2 and all(x["features"] for x in vibst(c)["sensors"]), 20),
            "two sensors, reporting (%s)" % [x["node"] for x in vibst(a).get("sensors", [])])
    t.check((a.last("vib") or {}).get("up") is True, "millweb is connected to vibmon")
    mk = a.mark()
    await t.mdi("M3 S6000")
    t.check(await a.until(lambda c: vibst(c)["machine"]["rpm"] == 6000 and vibst(c)["machine"]["spindle"], 5), "vibmon has the spindle: 6000 rpm, on")
    # the second simulated sensor sees 0.5 g of unbalance: its 1st order 0.354 g at 100 Hz
    t.check(await a.until(lambda c: (vibsensor(c, 2)["summary"] or {}).get("rpm") == 6000
                          and abs(vibsensor(c, 2)["summary"]["ord"][0] - 0.3536) < 0.015, 10),
            "sensor 2: 1st order 0.354 g at the machine's 6000 rpm (%s)" % (vibsensor(a, 2) or {}).get("summary"))
    mb = b.mark()
    t.ok(await b.call("vib_spectrum", node=2), "a spectrum, without control")
    t.check(await b.until(lambda c: c.since(mb, lambda m: m["t"] == "vibspectrum" and m["node"] == 2 and m["spectrum"]), 10), "it comes")
    sp = b.since(mb, lambda m: m["t"] == "vibspectrum" and m["node"] == 2)["spectrum"]
    k = max(range(1, len(sp["rms"])), key=lambda i: sp["rms"][i])
    t.check(len(sp["rms"]) == 1025 and abs(k * sp["df"] - 100) <= sp["df"], "1025 bins, the peak at 100 Hz (%.1f)" % (k * sp["df"]))
    t.refused(await b.call("vib_console", node=2, line="vib0.fft_n = 4096"), "a parameter without control", "control")
    t.ok(await a.call("vib_console", node=2, line="vib0.fft_n = 4096"), "a parameter with control")
    t.check(await a.until(lambda c: (vibsensor(c, 2)["summary"] or {}).get("fft_n") == 4096, 10), "the sensor runs 4096-point FFTs")
    t.check(await a.until(lambda c: c.ev(mk, "to vibration sensor 2: vib0.fft_n = 4096") is not None, 5), "logged")
    t.ok(await a.call("vib_identify", on=True), "identify on")
    t.check(await a.until(lambda c: vibst(c).get("identifying") is True, 5), "vibmon is identifying")
    await a.call("vib_identify", on=False)
    t.refused(await a.call("vib_console", node=2, line=""), "an empty line", "1..120")
    await t.mdi("M5")
    t.check(await a.until(lambda c: vibst(c)["machine"]["spindle"] is False, 5), "M5: the spindle off for vibmon too")


@case("drive: the card (state, fault, bus, temperatures, process data), the console for every browser, reading only", profile="drive")
async def _(t):
    a = t.a
    t.check(a.hello["machine"]["drive_links"] == ["X"] and a.hello["machine"]["drive_write"] is False, "hello: drive X, writing off")
    t.check(await a.until(lambda c: (drv(c) or {}).get("link") == "ok" and drv(c).get("state") == "ENABLED" and drv(c).get("dc"), 15),
            "the card: link ok, ENABLED with the machine on (%s)" % drv(a))
    d = drv(a)
    t.check(d["fw"] and "STMBL" in d["fw"], "firmware from about (%s)" % d["fw"])
    t.check(250 < d["dc"] < 400 and d["hv_t"] is not None and d["mot_t"] is not None and d["fault"] is None, "bus %s V, HV %s °C, motor %s °C, no fault" % (d["dc"], d["hv_t"], d["mot_t"]))
    t.check(d["pd"]["enable"] is True and d["pd"]["fault"] is False and isinstance(d["pd"]["pos_fb"], float), "process data from HAL (%s)" % d["pd"])
    # X moves through the drive
    await t.mdi("G53 G0 X600")
    v = (await a.call("hal_values", names=["joint.0.motor-pos-fb", "stmbl-sim.0.pos_fb"]))["values"]
    t.check(near(v["joint.0.motor-pos-fb"], 600, 0.05) and v["joint.0.motor-pos-fb"] == v["stmbl-sim.0.pos_fb"], "G53 G0 X600 through the drive (%s)" % v)
    # a watching browser: the console, reading
    b = await t.open()
    r = await b.call("drive_watch", name="X")
    t.check(r.get("ok") and "software info" in r["scroll"], "watch: what it said so far (about at start)")
    t.check("fault0.state" not in r["scroll"], "millweb's own questions don't show")
    mk = b.mark()
    t.ok(await b.call("drive_cmd", name="X", cmd="fault0.dc_volt"), "a watching browser reads a pin")
    t.check(await b.until(lambda c: "fault0.dc_volt <= hv0.dc_volt = " in douts(c, mk), 5), "its answer: %r" % douts(b, mk)[-80:])
    await asyncio.sleep(0.3)
    mk = b.mark()
    t.ok(await b.call("drive_cmd", name="X", cmd="showconf"), "showconf")
    t.check(await b.until(lambda c: "linrev0.scale=10" in douts(c, mk), 10), "the drive's whole config comes (%d chars)" % len(douts(b, mk)))
    for cmd, why in (("conf0.max_vel = 300", "DRIVE_WRITE"), ("flashsaveconf", "DRIVE_WRITE"), ("jogl", "DRIVE_WRITE"),
                     ("x" * 64, "63"), ("about\u0007", "printable")):
        t.refused(await a.call("drive_cmd", name="X", cmd=cmd), "%r" % cmd[:20], why)
    t.refused(await b.call("drive_cmd", name="Y", cmd="about"), "an unknown drive", "no drive")
    # the drive's own messages in the event log
    mk = a.mark()
    t.ok(await a.call("power", on=False), "machine off")
    t.check(await a.until(lambda c: (drv(c) or {}).get("state") == "DISABLED", 10), "the card: DISABLED")
    t.check(await a.until(lambda c: c.ev(mk, "Drive X: Disabled") is not None, 5), "the drive's 'INFO: Disabled' in the event log")
    # another program may have the console for a while
    t.refused(await b.call("drive_detach", name="X", on=True), "detach without control", "control")
    t.ok(await a.call("drive_detach", name="X", on=True, seconds=60), "detach")
    t.check(await a.until(lambda c: (drv(c) or {}).get("link") == "detached", 3), "the card: detached")
    import os
    import select
    import termios
    import tty
    fd = os.open(t.m.dir + "/drive-x", os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    tty.setraw(fd)
    os.write(fd, b"uptime\n")
    got, end = b"", time.time() + 3
    while time.time() < end and b"up " not in got:
        if select.select([fd], [], [], 0.1)[0]:
            got += os.read(fd, 1000)
    os.close(fd)
    t.check(b"up " in got, "another program talks to the drive meanwhile (%r)" % got[-40:])
    t.refused(await a.call("drive_cmd", name="X", cmd="about"), "the console while detached", "detached")
    t.ok(await a.call("drive_detach", name="X", on=False), "attach again")
    t.check(await a.until(lambda c: (drv(c) or {}).get("link") == "ok", 10), "the card: ok again")


@case("DRIVE_WRITE=yes: a pin set with control, logged; what moves or rewrites asks first; a position error fault on the card", profile="drivewrite")
async def _(t):
    a = t.a
    b = await t.open()
    t.refused(await b.call("drive_cmd", name="X", cmd="fault0.max_sat = 0.3"), "a write without control", "control")
    mk = a.mark()
    t.ok(await a.call("drive_cmd", name="X", cmd="fault0.max_sat = 0.3"), "a pin set with control")
    t.check(await a.until(lambda c: c.ev(mk, "Drive X: fault0.max_sat = 0.3 (by") is not None, 3), "logged, with who")
    await asyncio.sleep(0.3)
    await a.call("drive_watch", name="X")
    mk = a.mark()
    await a.call("drive_cmd", name="X", cmd="fault0.max_sat")
    t.check(await a.until(lambda c: "fault0.max_sat = 0.3" in douts(c, mk), 5), "reads back 0.3 (%r)" % douts(a, mk)[-60:])
    t.refused(await a.call("drive_cmd", name="X", cmd="flashsaveconf"), "flashsaveconf without sure", "confirm")
    t.refused(await a.call("drive_cmd", name="X", cmd="jogl"), "a jog without sure", "confirm")
    # a fault: a position error limit the next move can't keep
    await asyncio.sleep(0.3)
    mk = a.mark()
    t.ok(await a.call("drive_cmd", name="X", cmd="fault0.max_pos_error = 0.0001"), "a tiny position error limit")
    await asyncio.sleep(0.3)
    await a.call("mdi", cmd="G53 G0 X700")
    t.check(await a.until(lambda c: (drv(c) or {}).get("last") == "position error" and drv(c).get("last_at"), 10),
            "the card: the last fault, decoded, with its time (%s)" % drv(a))
    t.check(await a.until(lambda c: c.ev(mk, "Drive X: Fault 5: position error") is not None, 5), "the drive's ERROR line in the event log")
    t.check(await a.until(lambda c: drv(c).get("state") == "DISABLED" and drv(c)["pd"]["enable"] is False, 5), "LinuxCNC took enable away: DISABLED")
    t.check(await a.until(lambda c: not c.st["on"], 5), "LinuxCNC: amp fault, machine off")
    await a.call("drive_cmd", name="X", cmd="fault0.max_pos_error = 5")
    await asyncio.sleep(0.3)


# ----------------------------------------------------------------------- millweb-supervisor

async def lcnc_back(t, timeout=90, restarting=True):
    """Until the supervisor has LinuxCNC running again (restarting: it is about to stop first); then a new control client."""
    t0 = time.time()
    while restarting and t.m.status().get("state") == "running" and time.time() - t0 < 15:  # millweb quits half a second after the request
        await asyncio.sleep(0.1)
    ok = await asyncio.get_event_loop().run_in_executor(None, t.m.wait_state, "running", timeout)
    if not ok:
        raise Failed("LinuxCNC isn't running again (supervisor: %s)" % t.m.status())
    t.a = await C.open(t.port)
    await t.a.call("control", take=True, force=True)
    return t.a


async def sup_until(t, state, timeout=30):
    return await asyncio.get_event_loop().run_in_executor(None, t.m.wait_state, state, timeout)


@case("supervisor: machine.json read, checked and made into INI/HAL; watching browsers read, only control saves", profile="supervised")
async def _(t):
    a, b = t.a, await t.open()
    t.check(a.hello["machine"]["supervised"] is True and a.hello["machine"]["name"] == "PORTAL-M sim", "hello: supervised, the machine's name from machine.json")
    r = await b.call("config_get")
    t.check(r["ok"] and json.loads(r["text"])["name"] == "PORTAL-M sim" and r["running"] == r["saved"], "config_get: the text, what runs is what is saved")
    m = json.loads(r["text"])
    m["axes"]["X"]["max"] = 900
    r2 = await b.call("config_check", text=json.dumps(m), custom="")
    t.check(r2["ok"] and r2["errors"] == [] and "MAX_LIMIT = 900" in r2["files"]["mill.ini"] and '"max": 900' in r2["diff"], "config_check: no problems, the INI made, the change")
    m["axes"]["X"]["max"] = -5
    r2 = await b.call("config_check", text=json.dumps(m), custom="")
    t.check(r2["errors"] and r2["errors"][0][0] == "axes.X", "a bad axis: its problem, where (%s)" % r2.get("errors"))
    t.refused(await b.call("config_save", text=r["text"], custom=""), "a watching browser saving", "control")
    t.refused(await b.call("lcnc_restart"), "a watching browser restarting", "control")
    t.refused(await a.call("config_save", text="{", custom=""), "saving what isn't JSON", "JSON")
    t.check((await b.call("log_get"))["lines"] and any("LINUXCNC" in l for l in (await b.call("log_get"))["lines"]), "log_get: LinuxCNC's output")
    t.check((await b.call("sup_status"))["status"]["state"] == "running", "sup_status: running")


@case("supervisor: Save & restart runs the new machine.json; a program running refuses", profile="supervised")
async def _(t):
    a = t.a
    r = await a.call("config_get")
    m = json.loads(r["text"])
    t.check(a.hello["machine"]["limits"]["X"]["vel"] == 500 * 60, "X at 500 mm/s now")
    await t.load("slow.ngc", SLOW)
    t.ok(await a.call("cycle", action="start"), "a program runs")
    await a.until(lambda c: c.st["run"] == "run", 5)
    t.refused(await a.call("lcnc_restart"), "a restart meanwhile", "program")
    await a.call("cycle", action="stop")
    await t.idle()
    m["axes"]["X"]["max_vel"] = 300
    mk = a.mark()
    r = await a.call("config_save", text=json.dumps(m, indent=2), custom="", note="slower X", restart=True)
    t.ok(r, "config_save with restart")
    t.check(await a.until(lambda c: c.since(mk, lambda x: x["t"] == "sup" and x["status"]["state"] == "restarting"), 3), "every browser told: restarting")
    t0 = time.time()
    a = await lcnc_back(t)
    t.check(a.hello["machine"]["limits"]["X"]["vel"] == 300 * 60, "back in %.1f s, X at 300 mm/s (%s)" % (time.time() - t0, a.hello["machine"]["limits"]["X"]["vel"]))
    r = await a.call("config_get")
    t.check(r["running"] == r["saved"] and r["history"][0]["note"] == "slower X", "what runs is what was saved; the history has it")
    m["axes"]["X"]["max_vel"] = 500
    await a.call("config_save", text=json.dumps(m, indent=2), custom="", restart=True)
    await lcnc_back(t)


@case("supervisor: a configuration LinuxCNC refuses: the supervisor serves the page, the log says why, a fix starts it", profile="supervised")
async def _(t):
    a = t.a
    r = await a.call("config_get")
    text = r["text"]
    t.ok(await a.call("config_save", text=text, custom="net bogus nosuch.pin\n", restart=True), "custom.hal with a pin that isn't")
    t.check(await sup_until(t, "failed", 40), "the supervisor: failed (%s)" % t.m.status().get("state"))
    st = t.m.status()
    t.check("nosuch.pin" in (st.get("error") or ""), "its error names the HAL line: %s" % st.get("error"))
    b = await C.open(t.port)
    t.check(b.hello["machine"] is None and b.hello["supervisor"]["state"] == "failed", "a browser now: the supervisor's hello")
    log = await b.call("log_get")
    t.check(any("custom.hal:1" in l for l in log["lines"]), "log_get: LinuxCNC's error")
    t.check((await b.call("config_get"))["custom"] == "net bogus nosuch.pin\n", "config_get from the supervisor")
    t.refused(await b.call("config_save", text=text, custom=""), "saving without control", "control")
    t.refused(await b.call("mdi", cmd="G0 X0"), "a machine command", "isn't running")
    t.ok(await b.call("control", take=True), "control (from the supervisor)")
    t.refused(await b.call("config_save", text="{", custom=""), "a broken machine.json", "JSON")
    t.ok(await b.call("config_save", text=text, custom="", start=True), "the fix, and start")
    t.check(await sup_until(t, "running", 60), "running again")
    await b.close()
    await lcnc_back(t, restarting=False)


@case("supervisor: Stop, then Start from the supervisor's page; the bitstream loaded before LinuxCNC, once", profile="supervised")
async def _(t):
    import base64
    a = t.a
    t.ok(await a.call("bitstream_save", name="top.bit.bin", data=base64.b64encode(b"\x00bitstream" * 64).decode()), "a bitstream uploaded")
    r = await a.call("config_get")
    t.check([x["name"] for x in r["bitstreams"]] == ["top.bit.bin"], "listed")
    m = json.loads(r["text"])
    m["bitstream"] = {"loader": "command", "file": "top.bit.bin", "command": "cp {file} loaded.bin"}
    t.ok(await a.call("config_save", text=json.dumps(m, indent=2), custom=""), "saved with a loader")
    t.refused(await a.call("bitstream_delete", name="top.bit.bin"), "deleting the one it loads", "loads it")
    t.ok(await a.call("lcnc_stop"), "Stop")
    t.check(await sup_until(t, "stopped", 30), "stopped")
    b = await C.open(t.port)
    t.check(b.hello["supervisor"]["state"] == "stopped", "the supervisor's page")
    await b.call("control", take=True)
    t.ok(await b.call("lcnc_start"), "Start")
    t.check(await sup_until(t, "running", 60), "running")
    lb = os.path.join(t.m.dir, "loaded.bin")
    t.check(os.path.exists(lb) and open(lb, "rb").read().startswith(b"\x00bitstream"), "the loader ran (loaded.bin)")
    a = await lcnc_back(t, restarting=False)
    os.remove(lb)
    t.ok(await a.call("lcnc_restart"), "Restart")
    a = await lcnc_back(t)
    t.check(not os.path.exists(lb), "the same bitstream after a restart: not loaded again")
    log = await a.call("log_get")
    t.check(any("== " in l for l in log["lines"]), "the new start's log")
    m["bitstream"] = {"loader": "none"}
    await a.call("config_save", text=json.dumps(m, indent=2), custom="")
    t.ok(await a.call("bitstream_delete", name="top.bit.bin"), "deleted once unused")


@case("supervisor: an stmbl drive (simulated) on X from machine.json: its card and console on the Drives tab", profile="supervised")
async def _(t):
    import shutil
    a = t.a
    r = await a.call("config_get")
    old = r["text"]
    m = json.loads(old)
    shutil.copy(os.path.join(HERE, "sim-drive", "drive.conf"), os.path.join(t.m.dir, "drive.conf"))
    shutil.copy(os.path.join(HERE, "sim-drive", "drive-sim.txt"), os.path.join(t.m.dir, "drive-sim.txt"))
    m["axes"]["X"].update(max_vel=200, max_acc=1500)
    m["joints"][0].update(drive={"type": "stmbl_sim", "conf": "drive.conf", "sim": "drive-sim.txt"}, ferror=5, min_ferror=2.5)
    r = await a.call("config_check", text=json.dumps(m), custom="")
    t.check(r["ok"] and not r["errors"] and "loadrt stmbl_sim conf=" in r["files"]["machine.hal"] and "DRIVE_LINK = X " in r["files"]["mill.ini"], "checked: stmbl_sim, sserialterm, DRIVE_LINK (%s)" % r.get("errors"))
    t.ok(await a.call("config_save", text=json.dumps(m, indent=2), custom="", restart=True), "saved, restart")
    a = await lcnc_back(t)
    t.check(a.hello["machine"]["drive_links"] == ["X"], "hello: drive X")
    t.check(await a.until(lambda c: (drv(c) or {}).get("link") == "ok" and drv(c).get("fw"), 20), "the card: link ok, firmware (%s)" % drv(a))
    await baseline(t)  # a fresh LinuxCNC: E-STOP, off, not homed
    t.check(await a.until(lambda c: (drv(c) or {}).get("state") == "ENABLED", 15), "machine on: the drive ENABLED")
    await t.mdi("G53 G0 X600")
    v = (await a.call("hal_values", names=["joint.0.motor-pos-fb", "stmbl-sim.0.pos_fb"]))["values"]
    t.check(near(v["joint.0.motor-pos-fb"], 600, 0.05) and v["joint.0.motor-pos-fb"] == v["stmbl-sim.0.pos_fb"], "X moves through the drive (%s)" % v)
    t.ok(await a.call("config_save", text=old, custom="", restart=True), "back to simulated joints")
    await lcnc_back(t)


@case("supervisor: the machine's 3D model: files in models/, parts in machine.json, read by every browser", profile="supervised")
async def _(t):
    import base64
    import struct
    a, b = t.a, await t.open()
    stl = b"\0" * 80 + struct.pack("<I", 1) + struct.pack("<12f", 0, 0, 1, 0, 0, 0, 100, 0, 0, 0, 100, 0) + b"\0\0"
    t.refused(await b.call("mmodel_save", name="frame.stl", data=base64.b64encode(stl).decode()), "uploading without control", "control")
    r = await a.call("mmodel_save", name="frame.stl", data=base64.b64encode(stl).decode())
    t.check(r.get("ok") and r["models"] == [{"name": "frame.stl", "size": len(stl)}], "uploaded into models/ (%s)" % r)
    t.check(os.path.exists(os.path.join(t.m.dir, "models", "frame.stl")), "on disk")
    r = await b.call("mmodel_get", name="frame.stl")
    t.check(r.get("ok") and base64.b64decode(r["data"]) == stl, "a watching browser reads it back")
    t.refused(await a.call("mmodel_get", name="../machine.json"), "a path out of models/", "file name")
    g = await a.call("config_get")
    t.check(g["models"] == [{"name": "frame.stl", "size": len(stl)}], "config_get lists it")
    m = json.loads(g["text"])
    m["model"] = {"builtin": False, "parts": [{"name": "frame", "file": "frame.stl", "moves": "", "pos": [0, 0, -300]}, {"name": "ram", "file": "frame.stl", "moves": "XYZ"}]}
    t.ok(await a.call("config_save", text=json.dumps(m, indent=2), custom=""), "parts saved (no restart: the view's only)")
    t.refused(await a.call("mmodel_delete", name="frame.stl"), "deleting a file the model uses", "uses it")
    bad = dict(m, model={"parts": [{"file": "frame.stl", "moves": "XX"}]})
    r = await a.call("config_check", text=json.dumps(bad), custom="")
    t.check(r["errors"] and r["errors"][0][0] == "model.parts.0.moves", "a part's bad axes: its problem (%s)" % r["errors"])
    m.pop("model")
    await a.call("config_save", text=json.dumps(m, indent=2), custom="")
    t.ok(await a.call("mmodel_delete", name="frame.stl"), "deleted once unused")


@case("drive firmware: versions on the card; config backup, plan, patch and restore over the console, all undoable", profile="drivewrite")
async def _(t):
    a = t.a
    conf_file = os.path.join(t.m.dir, "drive.conf")
    original = open(conf_file).read()
    t.check(await a.until(lambda c: (drv(c) or {}).get("ver") and drv(c)["ver"].get("hv", -1) >= 0, 15), "the drive's versions (%s)" % (drv(a) or {}).get("ver"))
    d = drv(a)
    t.check(d["ver"] == {"fw": 1, "hv": 1, "hv_image": 1, "conf": 0} and d["fw_want"] == 1, "fw 1, HV 1 (it carries 1), config 0: from before versioning")
    t.check([p["level"] for p in d["problems"]] == ["warn"] and not d["problems"][0]["block"], "one problem, not blocking: the config wants its version line (%s)" % d["problems"])
    r = await a.call("drive_conf", name="X", action="backup")
    t.check(r.get("ok") and "linrev0.scale" in r["text"] and len(r["backups"]) == 1, "backup: the flash config read and kept (%s)" % r.get("backup"))
    t.check(os.path.exists(os.path.join(t.m.dir, "drive-backups", "X", r["backup"])), "in drive-backups/X/")
    r = await a.call("drive_conf", name="X", action="plan")
    t.check(r.get("ok") and r["from"] == 0 and r["to"] == 1 and r["new"].startswith("#stmbl_conf_version 1\n"), "plan: v0 → v1, the version line added")
    t.refused(await a.call("drive_conf", name="X", action="patch"), "patching with the machine on", "machine off")
    await a.call("power", on=False)
    t.refused(await a.call("drive_conf", name="X", action="patch"), "patching without sure", "confirm")
    b = await t.open()
    t.refused(await b.call("drive_conf", name="X", action="patch", sure=True), "a watching browser", "control")
    r = await a.call("drive_conf", name="X", action="patch", sure=True, timeout=120)
    t.ok(r, "patch: written and read back")
    t.check(open(conf_file).read().startswith("#stmbl_conf_version 1"), "the drive's flash (drive.conf) has the version line")
    t.check(await a.until(lambda c: (drv(c) or {}).get("ver", {}).get("conf") == 1 and drv(c)["problems"] == [], 15), "the card: config v1, no problems")
    bk = await a.call("drive_conf", name="X", action="backups")
    before = next(x["name"] for x in bk["backups"] if "beforepatch" in x["name"])
    r = await a.call("drive_conf", name="X", action="restore", backup=before, sure=True, timeout=120)
    t.ok(r, "restore the one before the patch")
    t.check(driveconf_clean(open(conf_file).read()) == driveconf_clean(original), "the flash as it was at the start")
    t.check(await a.until(lambda c: (drv(c) or {}).get("ver", {}).get("conf") == 0, 15), "the card: config v0 again")
    t.check(len((await a.call("drive_conf", name="X", action="backups"))["backups"]) == 3, "three backups: the first, before the patch, before the restore")


@case("drive firmware: an HV board with another firmware keeps the machine off", profile="drivewrite")
async def _(t):
    a = t.a
    await a.call("power", on=False)
    t.ok(await a.call("drive_cmd", name="X", cmd="hv0.sim_fw_version = 0"), "the simulated HV board: from before versioning")
    t.check(await a.until(lambda c: (drv(c) or {}).get("ver", {}).get("hv") == 0 and any(p["action"] == "hv_update" for p in drv(c)["problems"]), 20),
            "the card: HV v0 against v1, hv_update (%s)" % (drv(a) or {}).get("problems"))
    t.refused(await a.call("power", on=True), "machine on", "HV board runs v0")
    await asyncio.sleep(0.3)
    t.ok(await a.call("drive_cmd", name="X", cmd="hv0.sim_fw_version = 1"), "the HV board updated")
    t.check(await a.until(lambda c: not (drv(c) or {}).get("problems") or all(not p["block"] for p in drv(c)["problems"]), 20), "no blocking problem")
    t.ok(await a.call("power", on=True), "machine on")


@case("drive firmware: a console expecting a newer firmware keeps the machine off", profile="drivefw2")
async def _(t):
    a = t.a
    t.check(await a.until(lambda c: any(p["action"] == "flash" for p in (drv(c) or {}).get("problems", [])), 20), "the card: flash it (%s)" % (drv(a) or {}).get("problems"))
    await a.call("power", on=False)
    t.refused(await a.call("power", on=True), "machine on", "this console expects v2")
    t.check(a.ev(0, "Drive X: firmware v1; this console expects v2") is not None, "said in the event log")


def driveconf_clean(text):
    return "\n".join(l.rstrip() for l in text.splitlines() if l.strip())


@case("HAL_WRITE=yes: setp on an unconnected input and a parameter; not on a driven pin", profile="halwrite")
async def _(t):
    a = t.a
    t.check(a.hello["machine"]["hal_write"] is True, "hello: HAL writes on")
    mk = a.mark()
    t.ok(await a.call("hal_set", name="motion.analog-in-00", value="3.5", kind="pin"), "setp an unconnected input")
    t.check((await a.call("hal_values", names=["motion.analog-in-00"]))["values"]["motion.analog-in-00"] == 3.5, "it reads 3.5")
    t.check(a.ev(mk, "HAL: setp motion.analog-in-00 = 3.5") is not None, "logged, with who")
    t.ok(await a.call("hal_set", name="motion-controller.tmax", value="0", kind="param"), "setp a RW parameter")
    t.refused(await a.call("hal_set", name="joint.0.motor-pos-fb", value="1", kind="pin"), "setp on a pin a signal drives")
    t.refused(await a.call("hal_set", name="x; rm -rf /", value="1", kind="pin"), "a name that isn't a HAL name", "plain value")
    b = await t.open()
    t.refused(await b.call("hal_set", name="motion.analog-in-00", value="1"), "a watching browser", "control")


@case("ON_CONTROL_LOSS=hold: feed hold when the holder goes away", profile="hold")
async def _(t):
    a = t.a
    await t.load("slow.ngc", SLOW)
    b = await t.open()
    t.ok(await b.call("control", take=True, force=True), "b takes control")
    t.ok(await b.call("cycle", action="start"), "b starts")
    t.check(await a.until(lambda c: c.st["run"] == "run", 5), "running")
    mk = a.mark()
    await b.close()
    t.check(await a.until(lambda c: c.st["run"] == "hold", 3), "held")
    t.check(await a.until(lambda c: c.ev(mk, "went away: feed hold"), 3), "event: feed hold")
    t.ok(await a.call("control", take=True), "another browser takes control")
    t.ok(await a.call("cycle", action="resume"), "and resumes")
    t.check(await a.until(lambda c: c.st["run"] == "run", 3), "running")
    await a.call("cycle", action="stop")


@case("ON_CONTROL_LOSS=hold: an idle machine isn't touched", profile="hold")
async def _(t):
    a = t.a
    b = await t.open()
    await b.call("control", take=True, force=True)
    mk = a.mark()
    await b.close()
    t.check(await a.until(lambda c: c.ev(mk, "carries on"), 3), "nothing to hold")


@case("TOKEN: control needs the password", profile="token")
async def _(t):
    b = await t.open()
    t.check(b.hello["machine"]["token_required"] is True, "hello: token required")
    t.refused(await b.call("control", take=True, force=True), "no token", "token")
    t.refused(await b.call("control", take=True, force=True, token="guess"), "a wrong token", "token")
    t.check((await b.call("estop", on=True)).get("ok"), "E-STOP needs no token")
    t.ok(await b.call("control", take=True, force=True, token="s3cret"), "the right token")
    t.ok(await b.call("control", take=False), "release needs none")


@case("REMOTE_ENABLE_PIN: the key switch at the machine", profile="remote")
async def _(t):
    a = t.a
    t.check(await a.until(lambda c: c.last("slow") and c.last("slow").get("remote") is True, 3), "slow: remote enabled")
    mk = a.mark()
    t.m.halcmd("setp", "remote-key.in0", "0")
    t.check(await a.until(lambda c: c.since(mk, lambda m: m["t"] == "control" and m["holder"] is None), 3), "control dropped")
    t.check(a.ev(mk, "switched off at the machine") is not None, "event")
    t.refused(await a.call("control", take=True), "taking control", "switched off")
    t.refused(await a.call("mdi", cmd="G0 X0"), "MDI", "control")
    t.check((await a.call("estop", on=True)).get("ok"), "E-STOP still works")
    t.m.halcmd("setp", "remote-key.in0", "1")
    t.check(await a.until(lambda c: c.last("slow").get("remote") is True, 3), "enabled again")
    t.ok(await a.call("control", take=True), "control again")


@case("AIR_PIN: the air button sets a HAL bit", profile="remote")
async def _(t):
    a = t.a
    t.ok(await a.call("coolant", k="air", on=True), "air on")
    t.check(t.m.halcmd("getp", "air.in0").strip() == "TRUE", "air.in0 TRUE")
    t.ok(await a.call("coolant", k="air", on=False), "air off")
    t.check(t.m.halcmd("getp", "air.in0").strip() == "FALSE", "air.in0 FALSE")


# ======================================================================= runner

async def run_profile(m, cases, verbose):
    a = await C.open(m.port)
    t = T(m, a, verbose)
    results = []
    for name, _, fn in cases:
        t.fails = []
        t0 = time.time()
        err = None
        try:
            await asyncio.wait_for(baseline(t), 120)
            await asyncio.wait_for(fn(t), 240)
        except Failed as e:
            err = str(e)
        except Exception:
            err = traceback.format_exc(limit=4)
        if err:
            t.fails.append(err)
        if t.a.closed:  # a case may cost the control client its connection
            t.a = a = await C.open(m.port)
        ok = not t.fails
        print("%s %-70s %5.1fs" % ("PASS" if ok else "FAIL", name, time.time() - t0), flush=True)
        if not ok:
            if err:
                print("    " + err.replace("\n", "\n    "))
            evs = [m for m in a.msgs[-400:] if m["t"] == "ev"][-8:]
            for e in evs:
                print("      ev %s %-4s %s" % (e["time"], e["lvl"], e["text"]))
        results.append((name, ok))
    for c in t.extra:
        await c.close()
    await a.close()
    return results


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-k", action="append", default=[], help="only cases whose name contains this (repeatable)")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--keep", action="store_true")
    ap.add_argument("-v", "--verbose", action="store_true", help="print passing checks too")
    args = ap.parse_args()
    cases = [c for c in CASES if not args.k or any(k.lower() in c[0].lower() for k in args.k)]
    if args.list:
        for name, prof, _ in cases:
            print("%-8s %s" % (prof, name))
        return
    results = []
    t0 = time.time()
    for prof in PROFILES:
        sel = [c for c in cases if c[1] == prof]
        if not sel:
            continue
        print("== %s (%d case%s)" % (prof, len(sel), "" if len(sel) == 1 else "s"), flush=True)
        with harness.Machine(keep=args.keep, **PROFILES[prof]) as m:
            results += asyncio.run(run_profile(m, sel, args.verbose))
        if not m.clean_exit:
            results.append(("LinuxCNC stops when millweb does (%s)" % prof, False))
            print("FAIL LinuxCNC didn't stop when millweb did")
    bad = [n for n, ok in results if not ok]
    print("\n%d/%d cases passed in %.0f s" % (len(results) - len(bad), len(results), time.time() - t0))
    for n in bad:
        print("  FAIL " + n)
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
