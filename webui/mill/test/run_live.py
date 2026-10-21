#!/usr/bin/env python3
"""millweb against real LinuxCNC: test/sim (no hardware) with millweb as
its display program, driven over the WebSocket the way a browser does.

    python3 webui/mill/test/run_live.py [--keep] [--shot FILE]   (as a non-root user)

Copies test/sim to a temporary directory, starts LinuxCNC there headless,
runs the scenario, takes a screenshot of the live console with headless
Chromium if --shot is given, and shuts LinuxCNC down by stopping millweb.
Exit status 0 when every check passed.
"""
import argparse
import asyncio
import os
import re
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
MILL = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(MILL, "server"))
import wsserver  # noqa: E402

LINUXCNC = os.environ.get("LINUXCNC", "/home/user/linuxcnc-2.9")
CHROME = os.environ.get("CHROME", "/opt/pw-browsers/chromium-1194/chrome-linux/chrome")
fails = []


def check(cond, what):
    print(("ok   " if cond else "FAIL ") + what, flush=True)
    if not cond:
        fails.append(what)


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def ok(r):
    """A reply's ok; its reason printed when it failed."""
    if not r.get("ok"):
        print("      (%s)" % r.get("err"), flush=True)
    return bool(r.get("ok"))


class C:
    """A test client: replies by id, everything else kept in order."""

    def __init__(self, ws):
        self.ws, self.n, self.st, self.msgs = ws, 0, {}, []

    @classmethod
    async def open(cls, port):
        c = cls(await wsserver.connect("127.0.0.1", port))
        c.hello = await c.ws.recv_json(10)
        c.task = asyncio.ensure_future(c.reader())
        c.replies = {}
        return c

    async def reader(self):
        while True:
            m = await self.ws.recv_json(3600)
            if m is None:
                return
            if m["t"] == "st":
                self.st.update(m)
            if m["t"] == "reply":
                self.replies[m["id"]] = m
            self.msgs.append(m)

    async def call(self, op, timeout=130, **kw):
        self.n += 1
        n = self.n
        await self.ws.send_json({"op": op, "id": n, **kw})
        end = time.time() + timeout
        while n not in self.replies and time.time() < end:
            await asyncio.sleep(0.02)
        return self.replies.pop(n, {"ok": False, "err": "no reply"})

    async def until(self, pred, timeout=15.0):
        end = time.time() + timeout
        while time.time() < end:
            if pred(self):
                return True
            await asyncio.sleep(0.05)
        return False

    def seen(self, pred):
        return next((m for m in self.msgs if pred(m)), None)


async def scenario(port, shot):
    a = await C.open(port)
    check(a.hello["machine"]["name"] == "PORTAL-M sim", "hello: the machine's name from the INI")
    check(a.hello["machine"]["coords"] == "XYYZ", "hello: a gantry's coordinates XYYZ")
    r = await a.call("power", on=True)
    check(not r["ok"] and "control" in r["err"], "without control, commands are refused")
    check(ok(await a.call("control", take=True)), "take control")
    check(ok(await a.call("estop", on=False)), "E-STOP reset")
    check(ok(await a.call("power", on=True)), "machine on")
    check(await a.until(lambda c: c.st.get("on")), "the machine is on")
    check(ok(await a.call("home", axis="all")), "home all")
    check(await a.until(lambda c: c.st.get("homed") == [True, True, True]), "all axes homed")

    # MDI and the DRO
    check(ok(await a.call("mdi", cmd="G0 X10 Y20 Z5")), "MDI G0 X10 Y20 Z5")
    check(await a.until(lambda c: c.st.get("pos") and abs(c.st["pos"][0] - 10) < 1e-3 and abs(c.st["pos"][2] - 5) < 1e-3 and c.st.get("interp") == "idle"),
          "the DRO reaches X10 Y20 Z5 (G54)")
    check(abs(a.st["mach"][0] - 450) < 1e-3, "machine X = 450 (G54 X440)")
    check(ok(await a.call("zero", axis="x", value=0)), "zero X (G10 L20)")
    check(await a.until(lambda c: abs(c.st["pos"][0]) < 1e-3 and abs(c.st["g5x"][0] - 450) < 1e-3), "X reads 0, G54 X = 450")
    await a.call("offset", index=0, x=440.0)
    check(await a.until(lambda c: abs(c.st["g5x"][0] - 440) < 1e-3), "G10 L2 back to G54 X440")

    # jog: an increment, then a continuous jog the deadman stops
    y0 = a.st["pos"][1]
    check(ok(await a.call("jog", axis="y", dir=1, inc=1.0, rate=600)), "jog Y +1 mm")
    check(await a.until(lambda c: abs(c.st["pos"][1] - (y0 + 1)) < 1e-3), "Y moved 1 mm")
    x0 = a.st["pos"][0]
    check(ok(await a.call("jog", axis="x", dir=1, inc=0, rate=600)), "continuous jog X+")
    for _ in range(5):
        await asyncio.sleep(0.1)
        await a.call("jog", axis="x", dir=1, inc=0, rate=600)
    await asyncio.sleep(1.0)  # no keep-alive: millweb stops it
    x1 = a.st["pos"][0]
    await asyncio.sleep(0.5)
    check(x1 > x0 + 3 and abs(a.st["pos"][0] - x1) < 1e-3, "the jog moved, then the deadman stopped it (%.2f mm)" % (x1 - x0))
    check(a.seen(lambda m: m["t"] == "ev" and "keep-alive" in m["text"]) is not None, "event: jog stopped without keep-alive")

    # a program: upload, load, preview from the interpreter, run to the end
    text = open(os.path.join(HERE, "sim", "nc_files", "bracket-op1.ngc")).read().replace("BRACKET AL6082 OP1", "UPLOADED")
    check(ok(await a.call("program_save", name="uploaded.ngc", text=text)), "upload a program")
    check(ok(await a.call("program_load", name="uploaded.ngc")), "load it")
    check(await a.until(lambda c: c.seen(lambda m: m["t"] == "program" and m["name"] == "uploaded.ngc") is not None), "its preview arrives")
    pv = a.seen(lambda m: m["t"] == "program" and m["name"] == "uploaded.ngc") or {"segs": [], "error": "no preview"}
    segs = pv["segs"]
    check(pv["error"] is None, "the interpreter read it without error (%s)" % pv["error"])
    check(any(s[0] == 3 and s[9] == 3 for s in segs), "the preview has the T3 tool change")
    arcs = [s for s in segs if s[0] == 1 and 50 <= s[8] <= 60]
    check(len(segs) > 30, "arcs come as short segments (%d segments)" % len(segs))
    xs = [s[4] for s in segs] or [0]
    check(abs(max(xs) - 120) < 1e-3 and abs(min(xs)) < 1e-3, "preview in G54 coordinates: X 0 … 120")
    check(ok(await a.call("cycle", action="start")), "cycle start")
    check(await a.until(lambda c: c.st.get("run") == "run", 5), "running")
    check(await a.until(lambda c: c.st.get("tool") == 3, 10), "T3 in the spindle")
    check(await a.until(lambda c: c.st.get("spin", {}).get("dir") == 1, 10), "spindle on (M3)")
    if shot:
        await asyncio.sleep(1.5)
        screenshot(port, shot)
    check(await a.until(lambda c: c.st.get("run") == "done", 60), "the program finished")
    check(a.seen(lambda m: m["t"] == "ev" and "Program finished" in m["text"]) is not None, "event: program finished")
    check(abs(a.st["pos"][2] - 30) < 1e-3, "ends at Z30")

    # probing: Z surface (the probe trips at machine Z -195 = G54 Z0)
    await a.call("mdi", cmd="G0 X60 Y40 Z5")
    await a.until(lambda c: abs(c.st["pos"][2] - 5) < 1e-3 and c.st["interp"] == "idle")
    r = await a.call("probe", kind="z", travel=20, feed=300)
    check(ok(r) and abs(r["mach"][2] + 195) < 0.01, "probe Z: touched at machine Z %.4f" % r.get("mach", [0, 0, 0])[2])
    r = await a.call("probe", kind="bore")
    check(not r["ok"], "unsupported probe cycles are refused with a reason")

    # tools: edit the table, change tools, measure on the setter
    tools = [t for t in a.seen(lambda m: m["t"] == "tools")["tools"]]
    tools.append({"t": 7, "p": 7, "d": 10, "z": 55.66, "c": "10 mm chamfer mill"})
    check(ok(await a.call("tools_save", tools=tools)), "tool table: add T7")
    check(await a.until(lambda c: any(t["t"] == 7 for m in c.msgs if m["t"] == "tools" for t in m["tools"])), "the new table is broadcast")
    check(ok(await a.call("tool_load", t=7)), "T7 M6")
    check(await a.until(lambda c: c.st.get("tool") == 7), "T7 in the spindle")
    r = await a.call("tool_measure")
    check(ok(r) and abs(r.get("length", 0) - 50.0) < 0.01, "tool setter: T7 length %.4f (touched -195, ref -245)" % r.get("length", 0))

    # a second browser watches; it can press E-STOP, nothing else
    b = await C.open(port)
    check(b.hello["control"] == a.hello["you"], "the second browser sees who has control")
    r = await b.call("mdi", cmd="G0 X0")
    check(not r["ok"], "the watching browser can't send MDI")
    check(ok(await b.call("estop", on=True)), "the watching browser can press E-STOP")
    check(await a.until(lambda c: c.st.get("estop") is True and c.st.get("on") is False), "E-STOP: the machine is off")
    await b.ws.close()
    await a.ws.close()
    if fails:  # what the machine said
        for m in a.msgs:
            if m["t"] == "ev":
                print("  ev %s %-4s %s" % (m["time"], m["lvl"], m["text"]))


def screenshot(port, path):
    if not os.path.exists(CHROME):
        print("(no chromium: no screenshot)")
        return
    cmd = [CHROME, "--headless", "--no-sandbox", "--use-angle=swiftshader", "--enable-unsafe-swiftshader", "--hide-scrollbars",
           "--window-size=1600,1000", "--timeout=6000", "--screenshot=" + path, "http://127.0.0.1:%d/#main" % port]
    subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=60)
    print("screenshot:", path, flush=True)


def millweb_pids():
    out = []
    for p in os.listdir("/proc"):
        if p.isdigit():
            try:
                cmd = open("/proc/%s/cmdline" % p, "rb").read().split(b"\0")
            except OSError:
                continue
            if any(c.endswith(b"webui/mill/millweb") for c in cmd) and int(p) != os.getpid():
                out.append(int(p))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--keep", action="store_true", help="keep the temporary directory")
    ap.add_argument("--shot", help="a screenshot of the live console while the program runs")
    ap.add_argument("--browser", nargs="?", const="", help="also drive the page in headless Chromium (Playwright); "
                    "optionally a screenshot file")
    args = ap.parse_args()
    if os.geteuid() == 0:
        sys.exit("run as a non-root user (LinuxCNC's rtapi_app refuses root)")
    d = tempfile.mkdtemp(prefix="millweb-live.")
    shutil.copytree(os.path.join(HERE, "sim"), d, dirs_exist_ok=True)
    port = free_port()
    ini = open(os.path.join(d, "mill.ini")).read()
    ini = re.sub(r"(?m)^DISPLAY = .*", "DISPLAY = " + os.path.join(MILL, "millweb"), ini)
    ini = re.sub(r"(?m)^PORT = .*", "PORT = %d" % port, ini)
    ini = re.sub(r"(?m)^HOST = .*", "HOST = 127.0.0.1", ini)
    open(os.path.join(d, "mill.ini"), "w").write(ini)
    sh = ("export PATH=%s/pybin:$PATH; . %s/scripts/rip-environment >/dev/null; cd %s && exec linuxcnc mill.ini > linuxcnc.log 2>&1"
          % (LINUXCNC, LINUXCNC, d))
    lc = subprocess.Popen(["bash", "-c", sh], start_new_session=True)
    try:
        t = time.time() + 60
        while time.time() < t:
            try:
                socket.create_connection(("127.0.0.1", port), 1).close()
                break
            except OSError:
                time.sleep(0.5)
        else:
            print(open(os.path.join(d, "linuxcnc.log")).read()[-3000:])
            sys.exit("millweb didn't come up")
        print("millweb up on port %d (%s)" % (port, d), flush=True)
        asyncio.run(asyncio.wait_for(scenario(port, args.shot), 300))
        if args.browser is not None:
            env = dict(os.environ, NODE_PATH=os.environ.get("NODE_PATH", "/opt/node22/lib/node_modules"))
            cmd = ["node", os.path.join(HERE, "browser_live.js"), str(port)] + ([args.browser] if args.browser else [])
            p = subprocess.run(cmd, env=env, timeout=180)
            if p.returncode:
                fails.append("the browser test")
    finally:
        for p in millweb_pids():
            try:
                os.kill(p, signal.SIGTERM)
            except OSError:
                pass
        try:
            lc.wait(40)
        except subprocess.TimeoutExpired:
            os.killpg(lc.pid, signal.SIGKILL)
            fails.append("LinuxCNC didn't shut down when millweb stopped")
        if fails or args.keep:
            print("(kept %s)" % d)
        else:
            shutil.rmtree(d, ignore_errors=True)
    print("%d check(s) failed" % len(fails) if fails else "all checks passed", flush=True)
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
