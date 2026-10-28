"""millweb against a stand-in LinuxCNC (fake_linuxcnc): the protocol, the
control lease, the jog deadman, offsets, tools and programs.
python3 -m unittest discover -s webui/mill/server/tests"""
import asyncio
import json
import os
import shutil
import sys
import tempfile
import time
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
sys.path.insert(0, HERE)
import fake_linuxcnc as lc  # noqa: E402
import machine  # noqa: E402
import preview  # noqa: E402
import sampler  # noqa: E402
import drives  # noqa: E402
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
        self.tasks = [asyncio.ensure_future(self.hub.poll_loop()), asyncio.ensure_future(self.hub.slow_loop()), asyncio.ensure_future(self.hub.hal_loop())]
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
        # where the tool setter is, so the 3D view draws it where Measure goes
        self.assertEqual(info["toolsetter"], {"x": 980.0, "y": 20.0, "z_above": -40.0, "z_ref": -130.0, "travel": 100.0})
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
        self.assertFalse(r["ok"])  # no way out of NC_DIR
        self.assertFalse(os.path.exists(os.path.join(self.d, "evil.ngc")))
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

    def test_positions_follow_the_joints_until_homed(self):
        """LinuxCNC moves the axes' positions only once every joint is homed:
        while homing the console showed the axes standing (0), then jumping.
        Until all are homed each axis shows its joint (a gantry's first)."""
        lc.M.homed = [0, 0, 0, 1]                      # Z homed, Y homing
        lc.M.pos[:3] = [0.0, 0.0, -5.0]                # frozen in LinuxCNC
        lc.M.jpos = [1.5, 600.0, 600.0, -5.0]
        st = self.m.status()
        self.assertEqual(st["mach"], [1.5, 600.0, -5.0])
        self.assertEqual(st["pos"], [1.5 - 440.0, 600.0 - 460.0, -5.0 + 195.0])
        lc.M.homed = [1, 1, 1, 1]                      # all homed: the axes' own
        lc.M.pos[:3] = [0.0, 990.0, -5.0]
        self.assertEqual(self.m.status()["mach"], [0.0, 990.0, -5.0])

    async def test_folders_rename_delete(self):
        ws = await self.client()
        await self.ready(ws)
        nc = os.path.join(self.d, "nc")
        ok = lambda r: self.assertTrue(r["ok"], r)  # noqa: E731
        ok(await self.call(ws, "folder_create", name="parts"))
        ok(await self.call(ws, "folder_create", name="parts/bracket"))
        self.assertIn("no such folder", (await self.call(ws, "folder_create", name="nope/x"))["err"])
        ok(await self.call(ws, "program_save", name="parts/bracket/op1.ngc", text="G0 X1\nM2"))
        self.assertIn("no such folder", (await self.call(ws, "program_save", name="nope/a.ngc", text="M2"))["err"])
        self.assertIn("there already", (await self.call(ws, "program_save", name="parts/bracket/op1.ngc", text="M2", overwrite=False))["err"])
        r = await self.call(ws, "files")
        self.assertIn("parts/bracket/op1.ngc", [f["name"] for f in r["files"]])
        self.assertEqual(r["dirs"], ["parts", "parts/bracket"])
        # a program in a folder: get, load, its preview named by its path
        self.assertEqual((await self.call(ws, "program_get", name="parts/bracket/op1.ngc"))["program"]["name"], "parts/bracket/op1.ngc")
        ok(await self.call(ws, "program_load", name="parts/bracket/op1.ngc"))
        # rename and move; not the loaded one, nor its folder
        self.assertIn("loaded", (await self.call(ws, "program_rename", name="parts/bracket/op1.ngc", to="parts/op1.ngc"))["err"])
        self.assertIn("loaded", (await self.call(ws, "program_rename", name="parts", to="jobs"))["err"])
        ok(await self.call(ws, "program_save", name="parts/op2.ngc", text="M2"))
        ok(await self.call(ws, "program_rename", name="parts/op2.ngc", to="parts/bracket/op2-new.ngc"))
        self.assertTrue(os.path.exists(os.path.join(nc, "parts", "bracket", "op2-new.ngc")))
        self.assertIn("there already", (await self.call(ws, "program_rename", name="a.ngc", to="parts/bracket/op2-new.ngc"))["err"])
        self.assertIn("into itself", (await self.call(ws, "program_rename", name="parts", to="parts/bracket/x"))["err"])
        lc.M.file = ""  # nothing loaded
        ok(await self.call(ws, "program_rename", name="parts", to="jobs"))
        self.assertTrue(os.path.isdir(os.path.join(nc, "jobs", "bracket")))
        # delete: a program; a folder only when empty
        self.assertIn("isn't empty", (await self.call(ws, "folder_delete", name="jobs/bracket"))["err"])
        ok(await self.call(ws, "program_delete", name="jobs/bracket/op1.ngc"))
        ok(await self.call(ws, "program_delete", name="jobs/bracket/op2-new.ngc"))
        ok(await self.call(ws, "folder_delete", name="jobs/bracket"))
        self.assertFalse(os.path.exists(os.path.join(nc, "jobs", "bracket")))
        self.assertIn("not the program folder", (await self.call(ws, "folder_delete", name=""))["err"])
        # nothing outside the program folder, nothing hidden
        for bad in ("../x.ngc", "a/../../x.ngc", "/etc/x.ngc", ".hidden.ngc", "jobs/.git/x.ngc", "a\\..\\..\\x.ngc"):
            self.assertFalse((await self.call(ws, "program_save", name=bad, text="M2"))["ok"], bad)
        for bad in ("..", "../outside", ".git", "jobs/../.."):
            self.assertFalse((await self.call(ws, "folder_create", name=bad))["ok"], bad)
        os.symlink("/tmp", os.path.join(nc, "link"))  # a link out of the folder: not followed
        self.assertIn("outside", (await self.call(ws, "program_save", name="link/x.ngc", text="M2"))["err"])
        self.assertNotIn("link", (await self.call(ws, "files"))["dirs"])
        await ws.close()

    async def test_models_and_setup(self):
        import base64
        import struct
        ws = await self.client()
        await self.ready(ws)
        nc = os.path.join(self.d, "nc")
        ok = lambda r: self.assertTrue(r["ok"], r)  # noqa: E731
        # the default setup: the built-in vise and stock, before anything is saved
        st = await self.until(ws, lambda m: m["t"] == "setup")
        self.assertEqual([o["src"] for o in st["objects"]], ["builtin:vise", "builtin:stock"])
        # a binary STL (one triangle), an ASCII STL, an OBJ
        stl = b"\0" * 80 + struct.pack("<I", 1) + struct.pack("<12f", 0, 0, 1, 0, 0, 0, 10, 0, 0, 0, 20, 0) + b"\0\0"
        ascii_stl = b"solid x\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 1 0 0\nvertex 0 1 0\nendloop\nendfacet\nendsolid x\n"
        obj = b"v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"
        ok(await self.call(ws, "folder_create", name="fixtures"))
        for name, data in (("fixtures/plate.stl", stl), ("a.stl", ascii_stl), ("b.obj", obj)):
            ok(await self.call(ws, "model_save", name=name, data=base64.b64encode(data).decode()))
        self.assertIn("not an STL", (await self.call(ws, "model_save", name="bad.stl", data=base64.b64encode(b"hello").decode()))["err"])
        self.assertIn("not an OBJ", (await self.call(ws, "model_save", name="bad.obj", data=base64.b64encode(b"hello").decode()))["err"])
        self.assertIn("base64", (await self.call(ws, "model_save", name="c.stl", data="!!"))["err"])
        self.assertIn("3D models", (await self.call(ws, "model_save", name="c.ngc", data=base64.b64encode(stl).decode()))["err"])
        self.assertIn("up to 5 MB", (await self.call(ws, "model_save", name="big.stl", data=base64.b64encode(b"\0" * (5 * 1024 * 1024 + 1)).decode()))["err"])
        files = (await self.call(ws, "files"))["files"]
        self.assertEqual({f["name"]: f["kind"] for f in files if f["kind"] == "model"}, {"fixtures/plate.stl": "model", "a.stl": "model", "b.obj": "model"})
        r = await self.call(ws, "model_get", name="fixtures/plate.stl")
        self.assertEqual(base64.b64decode(r["data"]), stl)
        # the setup: saved, broadcast, checked
        objs = st["objects"] + [{"id": "p", "name": "plate", "role": "fixture", "src": "model:fixtures/plate.stl", "frame": "machine",
                                 "pos": [500, 500, -234], "rot": [0, 0, 90], "scale": 1, "look": "glass", "extra": "dropped"}]
        ok(await self.call(ws, "setup_save", objects=objs))
        st = await self.until(ws, lambda m: m["t"] == "setup" and len(m["objects"]) == 3)
        self.assertEqual(st["objects"][2]["look"], "glass")
        self.assertNotIn("extra", st["objects"][2])
        saved = json.load(open(os.path.join(self.d, "millweb-setup.json")))
        self.assertEqual(saved["objects"][2]["src"], "model:fixtures/plate.stl")
        for bad, why in (([{"src": "model:nope.stl"}], "no such model"), ([{"src": "file:/etc/passwd"}], "src"),
                         ([{"src": "model:../x.stl"}], "names"), ([{"src": "builtin:vise", "id": "a"}, {"src": "builtin:stock", "id": "a"}], "unique"),
                         ("x", "a list"), ([{"src": "builtin:vise", "pos": [1, 2]}], "three numbers")):
            r = await self.call(ws, "setup_save", objects=bad)
            self.assertFalse(r["ok"], bad)
            self.assertIn(why, r["err"])
        # a model in the setup can't be deleted or moved, nor its folder renamed
        self.assertIn("3D setup", (await self.call(ws, "program_delete", name="fixtures/plate.stl"))["err"])
        self.assertIn("3D setup", (await self.call(ws, "program_rename", name="fixtures/plate.stl", to="p.stl"))["err"])
        self.assertIn("3D setup", (await self.call(ws, "program_rename", name="fixtures", to="fx"))["err"])
        self.assertIn("extension", (await self.call(ws, "program_rename", name="a.stl", to="a.ngc"))["err"])
        ok(await self.call(ws, "program_rename", name="a.stl", to="fixtures/a2.stl"))
        ok(await self.call(ws, "program_delete", name="b.obj"))
        ok(await self.call(ws, "setup_save", objects=objs[:2]))
        ok(await self.call(ws, "program_delete", name="fixtures/plate.stl"))
        self.assertFalse(os.path.exists(os.path.join(nc, "fixtures", "plate.stl")))
        # a watching browser reads models and the setup, changes nothing
        b = await self.client()
        self.assertTrue((await self.call(b, "model_get", name="fixtures/a2.stl"))["ok"])
        self.assertIn("control", (await self.call(b, "setup_save", objects=[]))["err"])
        await b.close()
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
            f.write("".join("G1 X%d\n" % i for i in range(1, 25001)) + "M2\n")
        r = await self.call(ws, "program_lines", name="long.ngc", start=23999, count=3)  # a watcher may read
        self.assertTrue(r["ok"], r)
        self.assertEqual(r["lines"], ["G1 X23999", "G1 X24000", "G1 X24001"])
        self.assertEqual(r["nlines"], 25001)
        r = await self.call(ws, "program_lines", name="long.ngc", start=24999, count=5000)
        self.assertEqual(r["lines"], ["G1 X24999", "G1 X25000", "M2"])
        # the preview: a child process. Without LinuxCNC's gcode module it says
        # so; with it (an installed LinuxCNC) it reads the program
        r = await self.call(ws, "program_get", name="long.ngc")
        pg = r["program"]
        self.assertEqual((pg["nlines"], pg["line0"], len(pg["lines"])), (25001, 1, 2000))
        if pg["error"] and "gcode module" in pg["error"]:
            self.assertEqual(pg["path"], [])
        else:
            self.assertIsNone(pg["error"], pg["error"])
            self.assertEqual(pg["stats"]["moves"], 24999)  # the first G1 only sets where it starts
            self.assertEqual(pg["stats"]["ext"]["x"], [1, 25000])
        await ws.close()


class PostGui(unittest.TestCase):
    """[HAL] POSTGUI_HALFILE: millweb as the display loads it, as AXIS does
    (webui/mill/bench: the drive's phasing following error was never
    applied, a 1 mm limit tripped X while it autophased at enable)."""

    def test_runs_postgui_files_and_commands(self):
        d = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, d)
        ini = os.path.join(d, "m.ini")
        with open(ini, "w") as f:
            f.write("[HAL]\nHALFILE = m.hal\nPOSTGUI_HALFILE = post.hal\nPOSTGUI_HALFILE = x.tcl\n"
                    "POSTGUI_HALCMD = setp a.b 1\n")
        ran = []

        class R:
            def __init__(self, rc):
                self.returncode, self.stdout = rc, "" if rc == 0 else "no such pin"

        def run(argv, **kw):
            ran.append((argv, kw["cwd"]))
            return R(1 if "setp" in argv else 0)

        out = millweb.postgui(machine.Config(ini), run=run)
        self.assertEqual([a for a, _ in ran], [
            ["halcmd", "-i", ini, "-f", os.path.join(d, "post.hal")],
            ["haltcl", "-i", ini, os.path.join(d, "x.tcl")],
            ["halcmd", "-i", ini, "setp", "a.b", "1"]])
        self.assertTrue(all(cwd == d for _, cwd in ran))
        self.assertEqual([ok for ok, _ in out], [True, True, False])
        self.assertIn("no such pin", out[2][1])

    def test_none_without_postgui(self):
        d = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, d)
        ini = os.path.join(d, "m.ini")
        with open(ini, "w") as f:
            f.write("[HAL]\nHALFILE = m.hal\n")
        self.assertEqual(millweb.postgui(machine.Config(ini), run=None), [])


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


class HalShow(unittest.TestCase):
    """The HAL page's data: halcmd's show output parsed."""

    def test_parse_halcmd(self):
        pins = ("motmod float OUT 0 joint.0.motor-pos-cmd ==> j0\n"
                "motmod float IN 0 joint.0.motor-pos-fb <== j0\n"
                "iocontrol bit   IN FALSE iocontrol.0.emc-enable-in <== estop-loop\n"
                "motmod float IN 0 motion.analog-in-00\n")
        sigs = ("float  0  j0 <== joint.0.motor-pos-cmd ==> joint.0.motor-pos-fb\n"
                "bit    FALSE  estop-loop ==> iocontrol.0.emc-enable-in <== iocontrol.0.user-enable-out\n")
        params = "motmod s32   RW 66844 motion-command-handler.tmax\n"
        d = machine.parse_halcmd(pins, sigs, params)
        self.assertEqual(d["pins"][0], ["joint.0.motor-pos-cmd", "float", "OUT", "motmod", "0", "j0"])
        self.assertEqual(d["pins"][3][5], "")  # unconnected
        self.assertEqual(d["sigs"][1], ["estop-loop", "bit", "FALSE", "iocontrol.0.user-enable-out", ["iocontrol.0.emc-enable-in"]])
        self.assertEqual(d["params"][0], ["motion-command-handler.tmax", "s32", "RW", "motmod", "66844"])


class HalPage(Base):
    async def asyncSetUp(self):
        self.HAL = FakeHal({"joint.0.f-error": 0.25, "motion.analog-in-00": 0.0, "key": True})
        self.HAL.get_info_pins = lambda: [{"NAME": "joint.0.f-error", "VALUE": 0.25, "DIRECTION": 32, "TYPE": 2},
                                          {"NAME": "motion.analog-in-00", "VALUE": 0.0, "DIRECTION": 16, "TYPE": 2}]
        self.HAL.get_info_signals = lambda: [{"NAME": "j0", "VALUE": 0.0, "DRIVER": "joint.0.motor-pos-cmd", "TYPE": 2}]
        self.HAL.get_info_params = lambda: [{"NAME": "x.tmax", "VALUE": 5, "DIRECTION": 192}]
        await super().asyncSetUp()

    async def test_list_watch_and_no_setting_by_default(self):
        ws = await self.client()  # a watching browser may look
        r = await self.call(ws, "hal_list")
        self.assertTrue(r["ok"], r)
        self.assertIn(["joint.0.f-error", "float", "OUT", "", 0.25, ""], r["pins"])
        self.assertEqual(r["sigs"][0][3], "joint.0.motor-pos-cmd")
        self.assertEqual(r["params"][0][:3], ["x.tmax", "s32", "RW"])
        r = await self.call(ws, "hal_watch", names=["joint.0.f-error", "nope"])
        self.assertEqual(r["values"], {"joint.0.f-error": 0.25, "nope": None})
        self.HAL.pins["joint.0.f-error"] = 0.5
        await self.until(ws, lambda m: m["t"] == "hal" and m["v"]["joint.0.f-error"] == 0.5)
        self.assertEqual((await self.call(ws, "hal_values", names=["key"]))["values"], {"key": True})
        self.assertFalse(ws.hello["machine"]["hal_write"])
        await self.call(ws, "control", take=True)
        self.assertIn("HAL_WRITE", (await self.call(ws, "hal_set", name="motion.analog-in-00", value="1", kind="pin"))["err"])
        await self.call(ws, "hal_watch", names=[])
        self.assertNotIn(ws.hello["you"], self.hub.hal_watch)
        await ws.close()


class Sampler(unittest.TestCase):
    """The scope's sampling loop with a getter of its own."""

    def test_rate_frames_and_values(self):
        frames, t0 = [], [None]
        import math
        import struct
        import time

        def get(name):
            if t0[0] is None:
                t0[0] = time.perf_counter()
            if name == "bad":
                raise NameError(name)
            return True if name == "bit" else math.sin(time.perf_counter() - t0[0])
        sampler.sample_loop(get, ["a", "bit", "bad"], 1000.0, lambda h, d: frames.append((h, d)), duration=0.5)
        n = sum(h["n"] for h, _ in frames)
        self.assertGreater(n, 450)
        self.assertLess(n, 520)
        self.assertGreater(len(frames), 7)  # a frame every 50 ms
        h, d = frames[2]
        self.assertEqual(len(d), h["n"] * 3 * 4)
        rows = list(struct.iter_unpack("<3f", d))
        self.assertEqual(rows[0][1], 1.0)                  # a bit as 1
        self.assertTrue(math.isnan(rows[0][2]))           # a pin that can't be read: NaN, the others go on
        self.assertTrue(0 < rows[-1][0] < 1)
        self.assertAlmostEqual(frames[-2][0]["rate"], 1000, delta=60)


class Scope(Base):
    """scope_start: the sampler's frames to the browser; views for everyone."""

    async def asyncSetUp(self):
        await super().asyncSetUp()
        fake = os.path.join(self.d, "fake_sampler.py")
        open(fake, "w").write(
            "import sys, json, struct\nsys.path.insert(0, %r)\nimport sampler\n"
            "req = json.loads(sys.stdin.readline())\nout = sys.stdout.buffer\n"
            "def write(h, d):\n    h = (json.dumps(h) + '\\n').encode(); out.write(struct.pack('<I', len(h) + len(d)) + h + d); out.flush()\n"
            "sampler.sample_loop(lambda n: 2.5, req['names'], req['rate'], write)\n" % os.path.dirname(HERE))
        self.hub.sampler_cmd = [sys.executable, fake]

    async def test_scope_streams_and_stops(self):
        import base64
        import struct
        ws = await self.client()  # watching is enough: the scope only reads
        r = await self.call(ws, "scope_start", names=["a", "b"], rate=500)
        self.assertTrue(r["ok"], r)
        self.assertEqual(r["rate"], 500)
        m = await self.until(ws, lambda m: m["t"] == "scope" and m["n"] > 0)
        self.assertEqual(m["names"], ["a", "b"])
        self.assertEqual(struct.unpack("<2f", base64.b64decode(m["d"])[:8]), (2.5, 2.5))
        self.assertEqual((await self.call(ws, "scope_start", names=[]))["err"], "the scope needs a channel")
        proc = self.hub.scopes[ws.hello["you"]]
        await self.call(ws, "scope_stop")
        self.assertNotIn(ws.hello["you"], self.hub.scopes)
        self.assertIsNotNone(proc.returncode)
        # two at most; a browser that goes away stops its own
        others = [await self.client() for _ in range(3)]
        for o in others[:2]:
            self.assertTrue((await self.call(o, "scope_start", names=["a"]))["ok"])
        self.assertIn("two scopes", (await self.call(others[2], "scope_start", names=["a"]))["err"])
        await others[0].close()
        for _ in range(50):
            if len(self.hub.scopes) == 1:
                break
            await asyncio.sleep(0.05)
        self.assertEqual(len(self.hub.scopes), 1)
        self.assertTrue((await self.call(others[2], "scope_start", names=["a"]))["ok"])
        for o in others[1:]:
            await self.call(o, "scope_stop")
            await o.close()
        await ws.close()

    async def test_views_saved_for_everyone(self):
        a, b = await self.client(), await self.client()
        await self.until(a, lambda m: m["t"] == "views" and m["views"] == [])
        v = {"name": " axes ", "watch": [{"kind": "sig", "name": "j0"}, {"kind": "bogus", "name": "x"}, {"name": ""}], "scope": {"ch": [{"name": "j0"}], "rate": 1000}}
        r = await self.call(a, "views_save", views=[v])
        self.assertTrue(r["ok"], r)
        m = await self.until(b, lambda m: m["t"] == "views" and m["views"])
        self.assertEqual(m["views"], [{"name": "axes", "watch": [{"kind": "sig", "name": "j0"}, {"kind": "pin", "name": "x"}], "scope": {"ch": [{"name": "j0"}], "rate": 1000}}])
        self.assertEqual(self.m.views(), m["views"])
        self.assertIn("a view has a name", (await self.call(a, "views_save", views=[{"name": "  "}]))["err"])
        self.assertIn("too large", (await self.call(a, "views_save", views=[{"name": "x", "scope": {"s": "y" * 9000}}]))["err"])
        self.assertIn("at most 64", (await self.call(a, "views_save", views=[{"name": str(i)} for i in range(65)]))["err"])
        self.assertEqual(len(self.m.views()), 1)  # the bad ones changed nothing
        c = await self.client()
        await self.until(c, lambda m: m["t"] == "views" and m["views"] and m["views"][0]["name"] == "axes")
        for w in (a, b, c):
            await w.close()


class DriveConsole(unittest.TestCase):
    """drives.py: what may be sent, what the drive says, the card."""

    def test_classify(self):
        for cmd, kind in (("about", "read"), ("showconf", "read"), ("fault0.dc_volt", "read"), ("fault0", "write"), ("crash", "read"),
                          ("crash clear", "write"), ("conf0.r = 2", "write"), ("flashsaveconf", "danger"), ("jogl", "danger"),
                          ("reset", "danger"), ("fwbegin 1 2", "danger"), ("appendconf conf0.r = 1", "write")):
            self.assertEqual(drives.classify(cmd)[0], kind, cmd)
        self.assertIsNone(drives.classify("x" * 64)[0])
        self.assertIsNone(drives.classify("about\x1b")[0])
        self.assertIsNone(drives.classify("")[0])

    def test_lines_values_events_and_hidden_answers(self):
        out, ev = [], []
        d = drives.DriveLink("X", "/nonexistent", out=lambda l, t: out.append(t), event=lambda lvl, t: ev.append((lvl, t)))
        d.pending["fault0.state"] = 0
        d.feed(b"######## software info ########\nSTMBL v0.9.4 abc\nBranch master\nfault0.state = 3.000000\nfault0.dc_volt <= hv0.dc_volt = 323.5\n")
        d.feed(b"<font color='red'>ERROR:</font> Fault 5: position error\nINFO: Disabled \nfault0.fault = 5.0000")
        self.assertEqual(d.fw, "STMBL v0.9.4 abc")
        self.assertEqual(d.values["fault0.state"], 3.0)
        self.assertEqual(d.values["fault0.dc_volt"], 323.5)
        self.assertNotIn("fault0.state", "".join(out))       # millweb's own question
        self.assertIn("fault0.dc_volt <= hv0.dc_volt = 323.5", "".join(out))  # someone else's: shown
        self.assertNotIn("fault0.fault", "".join(out))        # no newline yet
        self.assertIn(("bad", "Drive X: Fault 5: position error"), ev)
        self.assertIn(("info", "Drive X: Disabled"), ev)
        d.feed(b"\n")
        st = d.status()
        self.assertEqual((st["state"], st["fault"], st["last"], st["dc"]), ("SOFT FAULT", "position error", "position error", 323.5))
        self.assertTrue(st["last_at"])
        self.assertEqual(st["link"], "missing" if d.state == "missing" else st["link"])

    def test_polls_over_a_pty_when_quiet(self):
        import pty
        import select
        master, slave = pty.openpty()
        path = os.ttyname(slave)
        d = drives.DriveLink("X", path, poll_ms=50)
        self.assertTrue(d.open())
        got = b""
        t = time.time()
        while time.time() - t < 1.0:  # about first, then the card's pins one at a time, each once answered or timed out
            d.tick()
            if select.select([master], [], [], 0.02)[0]:
                got += os.read(master, 1000)
                for line in got.split(b"\n")[:-1]:
                    name = line.strip().decode()
                    if name in drives.POLL and name in d.pending:
                        d.feed(("%s = 1.5\n" % name).encode())
                got = got.split(b"\n")[-1]
        self.assertTrue(d.fw is None)
        self.assertGreaterEqual(len(d.values), 5)
        self.assertEqual(d.status()["link"], "ok")
        # a browser's line goes out at once; the polling waits for a quiet console
        before = d.tx
        self.assertIsNone(d.user("showconf"))
        self.assertEqual(d.tx - before, len("showconf\n"))
        d.feed(b"conf0.r = 2\n")
        n = d.tx
        d.tick()
        self.assertEqual(d.tx, n, "no poll right after the drive spoke or a user wrote")
        d.detach("me", 60)
        self.assertIsNotNone(d.user("about"))
        self.assertEqual(d.status()["link"], "detached")
        self.assertTrue(d.attach())
        d.close()
        os.close(master)
        os.close(slave)


class DriveOps(Base):
    """drive_watch / drive_cmd / drive_detach: who may do what."""

    async def asyncSetUp(self):
        import pty
        self.master, self.slave = pty.openpty()
        self.INI_EXTRA = "DRIVE_LINK = X %s\nDRIVE_POLL_MS = 100000\n" % os.ttyname(self.slave)
        await super().asyncSetUp()
        self.tasks.append(asyncio.ensure_future(self.hub.drive_loop()))
        await asyncio.sleep(0.2)

    async def asyncTearDown(self):
        await super().asyncTearDown()
        os.close(self.master)
        os.close(self.slave)

    def drive_reads(self):
        import select
        out = b""
        while select.select([self.master], [], [], 0.1)[0]:
            out += os.read(self.master, 1000)
        return out.decode()

    async def test_reading_writing_and_detach(self):
        ws = await self.client()
        self.assertEqual(ws.hello["machine"]["drive_links"], ["X"])
        self.drive_reads()  # about at start
        r = await self.call(ws, "drive_watch", name="X")
        self.assertTrue(r["ok"], r)
        self.assertTrue((await self.call(ws, "drive_cmd", name="X", cmd="fault0.dc_volt"))["ok"])
        self.assertIn("fault0.dc_volt\n", self.drive_reads())
        os.write(self.master, b"fault0.dc_volt <= hv0.dc_volt = 320.0\n")
        m = await self.until(ws, lambda m: m["t"] == "dout" and "320.0" in m["text"])
        self.assertEqual(m["n"], "X")
        await asyncio.sleep(0.25)
        self.assertIn("DRIVE_WRITE", (await self.call(ws, "drive_cmd", name="X", cmd="conf0.r = 1"))["err"])
        self.hub.drive_write = True
        self.assertIn("control", (await self.call(ws, "drive_cmd", name="X", cmd="conf0.r = 1"))["err"])
        await self.call(ws, "control", take=True)
        await asyncio.sleep(0.25)
        self.assertTrue((await self.call(ws, "drive_cmd", name="X", cmd="conf0.r = 1"))["ok"])
        await asyncio.sleep(0.25)
        self.assertIn("confirm", (await self.call(ws, "drive_cmd", name="X", cmd="flashsaveconf"))["err"])
        await asyncio.sleep(0.25)
        self.assertTrue((await self.call(ws, "drive_cmd", name="X", cmd="flashsaveconf", sure=True))["ok"])
        sent = self.drive_reads()
        self.assertIn("conf0.r = 1\n", sent)
        self.assertIn("flashsaveconf\n", sent)
        self.assertIn("one line at a time", (await self.call(ws, "drive_cmd", name="X", cmd="about"))["err"] + (await self.call(ws, "drive_cmd", name="X", cmd="about"))["err"])
        await self.until(ws, lambda m: m["t"] == "ev" and "Drive X: flashsaveconf (by" in m["text"])
        self.assertTrue((await self.call(ws, "drive_detach", name="X", on=True))["ok"])
        self.assertIsNone(self.hub.links["X"].fd)
        self.assertTrue((await self.call(ws, "drive_detach", name="X", on=False))["ok"])
        self.assertIsNotNone(self.hub.links["X"].fd)
        m = await self.until(ws, lambda m: m["t"] == "drv")
        self.assertEqual(m["drives"][0]["n"], "X")
class Workspace(Base):
    INI_EXTRA = "WORKSPACE_X = 100 200\n"

    async def test_jog_workspace(self):
        """[MILLWEB] WORKSPACE_X: jogs stay inside it, programs may go beyond."""
        ws = await self.client()
        await self.call(ws, "control", take=True)
        for op, kw in (("estop", {"on": False}), ("power", {"on": True}), ("home", {"axis": "all"})):
            await self.call(ws, op, **kw)
        lc.M.pos[0] = 150.0
        r = await self.call(ws, "jog", axis="x", dir=1, inc=100, rate=600)  # shortened to the edge
        self.assertTrue(r["ok"], r)
        self.assertAlmostEqual(lc.M.pos[0], 200.0)
        r = await self.call(ws, "jog", axis="x", dir=1, inc=1, rate=600)  # at the edge: refused
        self.assertFalse(r["ok"])
        self.assertIn("workspace", r["err"])
        r = await self.call(ws, "jog", axis="x", dir=-1, inc=0, rate=600)  # continuous: to the other edge
        self.assertTrue(r["ok"], r)
        self.assertAlmostEqual(lc.M.pos[0], 100.0)
        self.assertTrue((await self.call(ws, "jog_stop", axis="x"))["ok"])
        lc.M.pos[0] = 50.0  # outside (a program took it there): only back towards it
        self.assertFalse((await self.call(ws, "jog", axis="x", dir=-1, inc=1, rate=600))["ok"])
        self.assertTrue((await self.call(ws, "jog", axis="x", dir=1, inc=1, rate=600))["ok"])
        self.assertAlmostEqual(lc.M.pos[0], 51.0)
        # Y has no workspace: the soft limits only, as before
        self.assertTrue((await self.call(ws, "jog", axis="y", dir=-1, inc=0.1, rate=600))["ok"])
        await ws.close()


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
