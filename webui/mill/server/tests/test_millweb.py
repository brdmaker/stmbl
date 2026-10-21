"""millweb against a stand-in LinuxCNC (fake_linuxcnc): the protocol, the
control lease, the jog deadman, offsets, tools and programs.
python3 -m unittest discover -s webui/mill/server/tests"""
import asyncio
import json
import os
import shutil
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
sys.path.insert(0, HERE)
import fake_linuxcnc as lc  # noqa: E402
import machine  # noqa: E402
import preview  # noqa: E402
import millweb  # noqa: E402
import wsserver  # noqa: E402

INI = """[EMC]
MACHINE = test-mill
[TRAJ]
COORDINATES = X Y Y Z
MAX_LINEAR_VELOCITY = 100
[KINS]
KINEMATICS = trivkins coordinates=XYYZ kinstype=BOTH
JOINTS = 4
[AXIS_X]
MIN_LIMIT = 0
MAX_LIMIT = 1000
[AXIS_Y]
MIN_LIMIT = 0
MAX_LIMIT = 1000
[AXIS_Z]
MIN_LIMIT = -200
MAX_LIMIT = 0
[EMCIO]
TOOL_TABLE = tool.tbl
[RS274NGC]
PARAMETER_FILE = sim.var
[MILLWEB]
NC_DIR = nc
TOOLSETTER = 980 20 -40 -130 100
"""


class FakeHal:
    """hal.get_value / set_p over a dict of pins."""

    def __init__(self, pins):
        self.pins = pins

    def get_value(self, name):
        if name not in self.pins:
            raise NameError(name)
        return self.pins[name]

    def set_p(self, name, v):
        self.pins[name] = v


class Base(unittest.IsolatedAsyncioTestCase):
    INI_EXTRA = ""   # more [MILLWEB] keys
    HAL = None

    async def asyncSetUp(self):
        lc.M.reset()
        lc.M.g5x[:3] = [440.0, 460.0, -195.0]  # what LinuxCNC read from sim.var
        self.d = tempfile.mkdtemp(prefix="millweb-test.")
        open(os.path.join(self.d, "m.ini"), "w").write(INI + self.INI_EXTRA)
        open(os.path.join(self.d, "tool.tbl"), "w").write("T3 P3 D6.0 Z+48.13 ;6 mm endmill\nT5 P5 D5 Z71.9 ;drill\n")
        open(os.path.join(self.d, "sim.var"), "w").write("5221 440.0\n5222 460.0\n5223 -195.0\n")
        os.makedirs(os.path.join(self.d, "nc"))
        open(os.path.join(self.d, "nc", "a.ngc"), "w").write("G0 X1\nM2\n")
        cfg = machine.Config(os.path.join(self.d, "m.ini"))
        self.m = machine.Machine(cfg, lc, self.HAL)
        self.hub = millweb.Hub(self.m, cfg, asyncio.Event())
        self.srv = await wsserver.Server(millweb.WEBROOT, self.hub.handle).serve("127.0.0.1", 0)
        self.port = self.srv.sockets[0].getsockname()[1]
        self.tasks = [asyncio.ensure_future(self.hub.poll_loop()), asyncio.ensure_future(self.hub.slow_loop())]
        await asyncio.sleep(0.1)

    async def asyncTearDown(self):
        self.hub.quit.set()
        for t in self.tasks:
            t.cancel()
        self.srv.close()
        shutil.rmtree(self.d, ignore_errors=True)

    async def client(self):
        ws = await wsserver.connect("127.0.0.1", self.port)
        ws.n = 0
        ws.inbox = []
        hello = await ws.recv_json()
        self.assertEqual(hello["t"], "hello")
        ws.hello = hello
        return ws

    async def call(self, ws, op, **kw):
        ws.n += 1
        await ws.send_json({"op": op, "id": ws.n, **kw})
        while True:
            m = await ws.recv_json()
            if m["t"] == "reply" and m["id"] == ws.n:
                return m
            ws.inbox.append(m)

    async def ready(self, ws):
        await self.call(ws, "control", take=True)
        for op, kw in (("estop", {"on": False}), ("power", {"on": True}), ("home", {"axis": "all"})):
            r = await self.call(ws, op, **kw)
            self.assertTrue(r["ok"], (op, r))

    async def until(self, ws, pred, timeout=3.0):
        loop = asyncio.get_event_loop()
        end = loop.time() + timeout
        for m in ws.inbox:
            if pred(m):
                return m
        while loop.time() < end:
            m = await ws.recv_json(end - loop.time())
            ws.inbox.append(m)
            if pred(m):
                return m
        raise AssertionError("no such message")


class Protocol(Base):
    async def test_hello_and_static(self):
        ws = await self.client()
        info = ws.hello["machine"]
        self.assertEqual(info["name"], "test-mill")
        self.assertEqual(info["coords"], "XYYZ")
        self.assertEqual(info["limits"]["Z"]["min"], -200)
        st = await self.until(ws, lambda m: m["t"] == "st" and "estop" in m)
        self.assertTrue(st["estop"])
        tools = await self.until(ws, lambda m: m["t"] == "tools")
        self.assertEqual([t["t"] for t in tools["tools"]], [3, 5])
        self.assertEqual(tools["tools"][0]["c"], "6 mm endmill")
        off = await self.until(ws, lambda m: m["t"] == "offsets")
        self.assertEqual(off["offsets"][0], [440.0, 460.0, -195.0])
        files = await self.until(ws, lambda m: m["t"] == "files")
        self.assertEqual(files["files"][0]["name"], "a.ngc")
        # the page itself
        r, w = await asyncio.open_connection("127.0.0.1", self.port)
        w.write(b"GET / HTTP/1.1\r\nHost: x\r\n\r\n")
        body = await r.read()
        self.assertIn(b"200 OK", body.split(b"\r\n")[0])
        self.assertIn(b"Portal Mill Console", body)
        w.close()
        r, w = await asyncio.open_connection("127.0.0.1", self.port)
        w.write(b"GET /../server/millweb.py HTTP/1.1\r\n\r\n")
        self.assertIn(b"404", (await r.read()).split(b"\r\n")[0])
        w.close()
        await ws.close()

    async def test_control_lease(self):
        a, b = await self.client(), await self.client()
        r = await self.call(a, "power", on=True)
        self.assertFalse(r["ok"])
        self.assertIn("control", r["err"])
        self.assertTrue((await self.call(a, "control", take=True))["ok"])
        r = await self.call(b, "control", take=True)
        self.assertFalse(r["ok"])
        self.assertIn("another browser", r["err"])
        # anyone may press E-STOP
        self.assertTrue((await self.call(b, "estop", on=True))["ok"])
        # the holder leaves: control is free
        await a.close()
        await asyncio.sleep(0.2)
        self.assertTrue((await self.call(b, "control", take=True))["ok"])
        await b.close()

    async def test_machine_flow(self):
        ws = await self.client()
        await self.call(ws, "control", take=True)
        self.assertTrue((await self.call(ws, "estop", on=False))["ok"])
        self.assertTrue((await self.call(ws, "power", on=True))["ok"])
        r = await self.call(ws, "mdi", cmd="G0 X10")
        self.assertFalse(r["ok"])
        self.assertIn("not homed", r["err"])
        self.assertTrue((await self.call(ws, "home", axis="all"))["ok"])
        st = await self.until(ws, lambda m: m["t"] == "st" and m.get("homed") == [True, True, True])
        self.assertTrue((await self.call(ws, "mdi", cmd="G0 X10 Y20"))["ok"])
        st = await self.until(ws, lambda m: m["t"] == "st" and "pos" in m and m["pos"][0] == 10.0)
        self.assertEqual(st["pos"][1], 20.0)
        self.assertEqual(st["mach"][0], 450.0)  # G54 X440
        # zero X here: G10 L20
        self.assertTrue((await self.call(ws, "zero", axis="x", value=0))["ok"])
        self.assertIn("G10 L20 P0 X0.0000", lc.M.mdi_log)
        off = await self.until(ws, lambda m: m["t"] == "offsets" and abs(m["offsets"][0][0] - 450.0) < 1e-6)
        # overrides, spindle, coolant
        self.assertTrue((await self.call(ws, "override", k="feed", v=0.8))["ok"])
        self.assertTrue((await self.call(ws, "spindle", dir=1, speed=12000))["ok"])
        self.assertTrue((await self.call(ws, "coolant", k="flood", on=True))["ok"])
        st = await self.until(ws, lambda m: m["t"] == "st" and m.get("flood") is True)
        self.assertEqual(lc.M.spindle["speed"], 12000)
        self.assertEqual(lc.M.feedrate, 0.8)
        await ws.close()

    async def test_jog_deadman(self):
        ws = await self.client()
        await self.call(ws, "control", take=True)
        for op, kw in (("estop", {"on": False}), ("power", {"on": True}), ("home", {"axis": "all"})):
            await self.call(ws, op, **kw)
        self.assertTrue((await self.call(ws, "jog", axis="x", dir=1, inc=0, rate=600))["ok"])
        self.assertIn(0, lc.M.jogging)
        for _ in range(4):  # keep-alive
            await asyncio.sleep(0.1)
            await self.call(ws, "jog", axis="x", dir=1, inc=0, rate=600)
        self.assertIn(0, lc.M.jogging)
        await asyncio.sleep(0.6)  # silence: the deadman stops it
        self.assertNotIn(0, lc.M.jogging)
        ev = await self.until(ws, lambda m: m["t"] == "ev" and "keep-alive" in m["text"])
        # an increment
        self.assertTrue((await self.call(ws, "jog", axis="y", dir=-1, inc=0.1, rate=600))["ok"])
        self.assertAlmostEqual(lc.M.pos[1], -0.1)
        await ws.close()

    async def test_tools_programs_probe(self):
        ws = await self.client()
        await self.call(ws, "control", take=True)
        for op, kw in (("estop", {"on": False}), ("power", {"on": True}), ("home", {"axis": "all"})):
            await self.call(ws, op, **kw)
        tools = [{"t": 3, "p": 3, "d": 6, "z": 48.2, "c": "endmill"}, {"t": 7, "p": 7, "d": 10, "z": 55.6, "c": "chamfer"}]
        self.assertTrue((await self.call(ws, "tools_save", tools=tools))["ok"])
        txt = open(os.path.join(self.d, "tool.tbl")).read()
        self.assertIn("T7 P7 D10.0000 Z+55.6000 ;chamfer", txt)
        self.assertTrue(os.path.exists(os.path.join(self.d, "tool.tbl.bak")))
        r = await self.call(ws, "tools_save", tools=tools + [tools[0]])
        self.assertFalse(r["ok"])
        # programs
        r = await self.call(ws, "program_save", name="../evil.ngc", text="M2")
        self.assertTrue(r["ok"])  # the name is reduced to its base name, inside NC_DIR
        self.assertTrue(os.path.exists(os.path.join(self.d, "nc", "evil.ngc")))
        r = await self.call(ws, "program_save", name="x.sh", text="M2")
        self.assertFalse(r["ok"])
        self.assertTrue((await self.call(ws, "program_save", name="b.ngc", text="G0 X5\nM2"))["ok"])
        self.assertTrue((await self.call(ws, "program_load", name="b.ngc"))["ok"])
        pv = await self.until(ws, lambda m: m["t"] == "program")
        self.assertEqual(pv["name"], "b.ngc")
        self.assertEqual(pv["lines"], ["G0 X5", "M2"])
        # probe: Z surface
        r = await self.call(ws, "probe", kind="z", travel=20)
        self.assertTrue(r["ok"], r)
        self.assertEqual(r["mach"][2], -12.5)
        r = await self.call(ws, "probe", kind="bore")
        self.assertFalse(r["ok"])
        # tool load
        self.assertTrue((await self.call(ws, "tool_load", t=7))["ok"])
        st = await self.until(ws, lambda m: m["t"] == "st" and m.get("tool") == 7)
        await ws.close()


    async def test_ping_events_unknown_and_bugs(self):
        ws = await self.client()
        r = await self.call(ws, "ping")  # no control needed
        self.assertTrue(r["ok"])
        self.assertIn("time", r)
        r = await self.call(ws, "events", n=2)
        self.assertTrue(r["ok"])
        self.assertTrue(1 <= len(r["events"]) <= 2)
        await ws.send("not json")
        await ws.send("[1]")
        await self.call(ws, "control", take=True)
        r = await self.call(ws, "frobnicate")
        self.assertFalse(r["ok"])
        self.assertIn("unknown op", r["err"])
        r = await self.call(ws, "wcs", index=12)  # an IndexError inside: still a reply
        self.assertFalse(r["ok"])

        async def boom(c, m):
            raise RuntimeError("bug")
        self.hub.op_ping = boom
        r = await self.call(ws, "ping")
        self.assertFalse(r["ok"])
        self.assertIn("internal error", r["err"])
        await self.until(ws, lambda m: m["t"] == "ev" and "ping failed" in m["text"])
        await ws.close()

    async def test_power_off_and_release(self):
        ws = await self.client()
        await self.ready(ws)
        r = await self.call(ws, "power", on=False)  # reads back as ESTOP_RESET
        self.assertTrue(r["ok"], r)
        self.assertEqual(lc.M.task_state, lc.STATE_ESTOP_RESET)
        self.assertTrue((await self.call(ws, "control", take=False))["ok"])
        await self.until(ws, lambda m: m["t"] == "ev" and "released control" in m["text"])
        await ws.close()

    async def test_status_without_work_offset_is_resynced(self):
        ws = await self.client()
        await self.ready(ws)
        lc.M.g5x_index = 0  # LinuxCNC's status after a too-early start: no work offset
        await self.until(ws, lambda m: m["t"] == "st" and m.get("wcs_ok") is False)
        r = await self.call(ws, "mdi", cmd="G0 X1")
        self.assertTrue(r["ok"], r)
        i = lc.M.mdi_log.index("G0 X1")
        self.assertEqual(lc.M.mdi_log[i - 2:i], ["G55", "G54"])  # another system, then the startup one
        self.assertEqual(lc.M.g5x_index, 1)
        await self.until(ws, lambda m: m["t"] == "ev" and "re-read (G54)" in m["text"])
        await ws.close()

    async def test_tool_zero_unloads(self):
        ws = await self.client()
        await self.ready(ws)
        await self.call(ws, "tool_load", t=3)
        self.assertIn("T3 M6 G43", lc.M.mdi_log)
        await self.call(ws, "tool_load", t=0)
        self.assertIn("T0 M6 G49", lc.M.mdi_log)
        await ws.close()

    async def test_urgent_commands_dont_wait(self):
        ws = await self.client()
        await self.ready(ws)
        await self.call(ws, "tool_load", t=3)
        for op, kw, why in (("estop", {"on": True}, "stopped"), ("cycle", {"action": "stop"}, "stopped")):
            if op == "cycle":
                await self.ready(ws)
            lc.M.slow = True
            ws.n += 1
            n = ws.n
            await ws.send_json({"op": "tool_measure", "id": n})  # waits on the busy interpreter
            await asyncio.sleep(0.3)
            t0 = asyncio.get_event_loop().time()
            r = await self.call(ws, op, **kw)
            self.assertTrue(r["ok"])
            self.assertLess(asyncio.get_event_loop().time() - t0, 0.5, op + " waited")
            lc.M.slow = False
            m = await self.until(ws, lambda m: m["t"] == "reply" and m["id"] == n)
            self.assertFalse(m["ok"])
            self.assertIn(why, m["err"])
            # nothing more was sent after the stop
            self.assertFalse(any(x.startswith("G53 G0 X") for x in lc.M.mdi_log[lc.M.mdi_log.index("G49"):][2:]))
            lc.M.mdi_log.clear()
        await ws.close()


    async def test_long_program_lines_and_preview_message(self):
        ws = await self.client()
        p = os.path.join(self.d, "nc", "long.ngc")
        with open(p, "w") as f:
            f.write("".join("G1 X%d\n" % i for i in range(1, 25001)))
        r = await self.call(ws, "program_lines", name="long.ngc", start=23999, count=3)  # a watcher may read
        self.assertTrue(r["ok"], r)
        self.assertEqual(r["lines"], ["G1 X23999", "G1 X24000", "G1 X24001"])
        self.assertEqual(r["nlines"], 25000)
        r = await self.call(ws, "program_lines", name="long.ngc", start=24999, count=5000)
        self.assertEqual(r["lines"], ["G1 X24999", "G1 X25000"])
        # the preview: a child process; without LinuxCNC's gcode module it says so
        r = await self.call(ws, "program_get", name="long.ngc")
        pg = r["program"]
        self.assertEqual((pg["nlines"], pg["line0"], len(pg["lines"]), pg["path"]), (25000, 1, 2000, []))
        self.assertIn("gcode module", pg["error"])
        await ws.close()


class Decimation(unittest.TestCase):
    """The preview's canon on its own: stats over every move, a bounded path."""

    def test_merges_past_the_cap_and_counts_everything(self):
        c = preview.Canon(max_segs=1000, rapid=6000, acc=1000)
        c.feedrate = 600.0
        c._seg(0, (0.0,) * 9)  # the first move only sets where the path starts
        n = 10000
        for i in range(1, n + 1):  # a zig-zag of 10 000 feed moves, 0.1 mm each in X
            c.lineno = i
            c._seg(1, (i * 0.1 / 25.4, (i % 2) * 0.05 / 25.4, -0.5 / 25.4) + (0.0,) * 6)
        c.change_tool(1)
        self.assertEqual(c.n, n)
        self.assertLessEqual(len(c.segs), 1001)
        self.assertGreaterEqual(c.stride, 8)
        self.assertEqual(c.segs[-1][0], 3)           # a tool change is never merged
        self.assertEqual(c.segs[-2][8], n)           # the last drawn segment ends on the last line
        self.assertAlmostEqual(c.segs[-2][4], n * 0.1, 6)
        self.assertAlmostEqual(c.hi_ext[0], n * 0.1, 6)
        self.assertGreater(c.cut, n * 0.1)           # every move's length, not the merged ones'
        for a, b in zip(c.segs, c.segs[1:]):         # still one connected path
            self.assertEqual(a[4:7], b[1:4])

    def test_alternating_rapid_and_feed_still_shrinks(self):
        c = preview.Canon(max_segs=1000)
        c.feedrate = 450.0
        c._seg(0, (0.0,) * 9)
        for i in range(20000):  # peck drilling: down at feed, up at rapid, over at rapid
            x = (i // 4) * 0.5 / 25.4
            c.lineno = i
            c._seg(1 if i % 4 == 1 else 0, (x, 0.0, (-6.0 if i % 4 == 1 else 2.0) / 25.4) + (0.0,) * 6)
        self.assertLessEqual(len(c.segs), 1001)
        self.assertLess(c.stride, 64)
        self.assertAlmostEqual(c.lo_ext[2], -6.0, 6)
        for a, b in zip(c.segs, c.segs[1:]):
            self.assertEqual(a[4:7], b[1:4])


class Token(Base):
    INI_EXTRA = "TOKEN = s3cret\n"

    async def test_token(self):
        ws = await self.client()
        self.assertTrue(ws.hello["machine"]["token_required"])
        self.assertIn("token", (await self.call(ws, "control", take=True))["err"])
        self.assertIn("token", (await self.call(ws, "control", take=True, token="x"))["err"])
        self.assertTrue((await self.call(ws, "estop", on=True))["ok"])  # E-STOP needs none
        self.assertTrue((await self.call(ws, "control", take=True, token="s3cret"))["ok"])
        await ws.close()


class HoldOnLoss(Base):
    INI_EXTRA = "ON_CONTROL_LOSS = hold\n"

    async def test_hold(self):
        a, b = await self.client(), await self.client()
        await self.ready(a)
        lc.M.task_mode = lc.MODE_AUTO
        lc.M.interp_state = lc.INTERP_READING
        await self.until(b, lambda m: m["t"] == "st" and m.get("run") == "run")
        await a.close()
        await self.until(b, lambda m: m["t"] == "ev" and "feed hold" in m["text"])
        self.assertTrue(lc.M.paused)
        await b.close()


class RemoteKey(Base):
    INI_EXTRA = "REMOTE_ENABLE_PIN = key.in\nAIR_PIN = air.in\n"

    async def asyncSetUp(self):
        self.HAL = FakeHal({"key.in": True, "air.in": False})
        await super().asyncSetUp()

    async def test_remote_key(self):
        ws = await self.client()
        await self.ready(ws)
        self.assertTrue((await self.call(ws, "coolant", k="air", on=True))["ok"])
        self.assertEqual(self.HAL.pins["air.in"], "1")
        self.HAL.pins["key.in"] = False
        await self.until(ws, lambda m: m["t"] == "control" and m["holder"] is None)
        self.assertIn("switched off", (await self.call(ws, "control", take=True))["err"])
        self.assertIn("control", (await self.call(ws, "mdi", cmd="G0 X1"))["err"])
        self.assertTrue((await self.call(ws, "estop", on=True))["ok"])
        ws.inbox.clear()
        self.HAL.pins["key.in"] = True
        await self.until(ws, lambda m: m["t"] == "slow" and m.get("remote") is True)
        self.assertTrue((await self.call(ws, "control", take=True))["ok"])
        await ws.close()


if __name__ == "__main__":
    unittest.main()
