"""machine.json and what is made of it (machineconf.py), the bitstream loaders
and the Configure page's operations (supervisor.py), without LinuxCNC.
python3 -m unittest discover -s webui/mill/server/tests"""
import copy
import json
import os
import shutil
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import machineconf as mc  # noqa: E402
import supervisor  # noqa: E402

MILL = os.path.dirname(os.path.dirname(HERE))
TEST_MACHINE = os.path.join(MILL, "test", "machine", "machine.json")


def ini_sections(text):
    out, sec = {}, None
    for line in text.splitlines():
        line = line.strip()
        if line.startswith("["):
            sec = out.setdefault(line.strip("[]"), {})
        elif "=" in line and sec is not None and not line.startswith("#"):
            k, v = (x.strip() for x in line.split("=", 1))
            sec.setdefault(k, []).append(v)
    return out


class Machine(unittest.TestCase):
    def setUp(self):
        self.d = tempfile.mkdtemp(prefix="machineconf-test.")

    def tearDown(self):
        shutil.rmtree(self.d, ignore_errors=True)

    def gen(self, m):
        return mc.generate(mc.validate(m, self.d), self.d, "/mill")

    def test_default_is_valid_and_complete(self):
        m = mc.validate(copy.deepcopy(mc.DEFAULT))
        self.assertEqual([j["max"] for j in m["joints"]], [501, 501, 1])  # a joint's travel: the axis's and 1 mm
        f = self.gen(mc.DEFAULT)
        ini = ini_sections(f["mill.ini"])
        self.assertEqual(ini["KINS"]["KINEMATICS"], ["trivkins coordinates=XYZ"])
        self.assertEqual(ini["DISPLAY"]["DISPLAY"], ["/mill/millweb"])
        self.assertEqual(ini["MILLWEB"]["MACHINE_DIR"], [self.d])
        self.assertIn("net estop-loop iocontrol.0.user-enable-out => iocontrol.0.emc-enable-in", f["machine.hal"])

    def test_the_test_machine_is_test_sim(self):
        """test/machine/machine.json makes test/sim's INI, key for key (paths aside)."""
        f = self.gen(json.load(open(TEST_MACHINE)))
        got = ini_sections(f["mill.ini"])
        want = ini_sections(open(os.path.join(MILL, "test", "sim", "mill.ini")).read())
        for sec in ("TRAJ", "KINS", "AXIS_X", "AXIS_Y", "AXIS_Z", "JOINT_0", "JOINT_1", "JOINT_2", "JOINT_3", "EMCMOT"):
            for k, v in want[sec].items():
                norm = lambda xs: [float(x) if x.replace(".", "").lstrip("-").isdigit() else x.replace(" ", "") for x in xs]  # noqa: E731
                self.assertEqual(norm(got[sec].get(k, [])), norm(v), "%s %s" % (sec, k))
        for k in ("TOOLSETTER", "PIN", "AT_SPEED_PIN", "PORT"):
            self.assertEqual(got["MILLWEB"][k], want["MILLWEB"][k], k)
        self.assertEqual(got["EMC"]["MACHINE"], want["EMC"]["MACHINE"])

    def test_errors_all_at_once(self):
        m = copy.deepcopy(mc.DEFAULT)
        m["units"] = "inch"
        m["axes"]["X"]["min"] = 600
        m["joints"][0]["drive"] = {"type": "stmbl_sserial"}       # no Mesa board
        m["joints"][1]["home"] = {"method": "switch"}               # no switch, no speeds
        m["console"]["extra"] = {"BOGUS": 1}
        m["bitstream"] = {"loader": "fpga_manager", "file": "../etc/passwd"}
        with self.assertRaises(mc.Invalid) as cm:
            mc.validate(m, self.d)
        where = {w for w, _ in cm.exception.errors}
        self.assertTrue({"units", "axes.X", "joints.0.drive", "joints.1.home.switch", "joints.1.home.search_vel",
                         "joints.1.home.latch_vel", "console.extra.BOGUS", "bitstream.file"} <= where, where)

    def test_gantry_sequence_and_stmbl_drives(self):
        m = copy.deepcopy(mc.DEFAULT)
        m["board"] = {"type": "hm2_spix", "name": "7c81", "params": "spiclk_rate=35714", "config": "sserial_port_0=00000000"}
        m["joints"] = [
            {"axis": "X", "drive": {"type": "stmbl_sserial", "port": 0, "channel": 0}, "home": {"method": "index", "latch_vel": 5, "sequence": 1}},
            {"axis": "Y", "drive": {"type": "stmbl_sserial", "port": 0, "channel": 1}, "home": {"sequence": 2}},
            {"axis": "Y", "drive": {"type": "stmbl_sserial", "port": 0, "channel": 2}, "home": {"sequence": 2}},
            {"axis": "Z", "drive": {"type": "stmbl_sserial", "port": 0, "channel": 3, "console": False}},
        ]
        m["estop"] = {"input": "hm2_7c81.0.inm.00.input-00", "invert": True}
        f = self.gen(m)
        ini, hal = ini_sections(f["mill.ini"]), f["machine.hal"]
        self.assertEqual(ini["JOINT_1"]["HOME_SEQUENCE"], ["-2"])     # a gantry: synchronized
        self.assertEqual(ini["JOINT_0"]["HOME_USE_INDEX"], ["YES"])
        self.assertIn('loadrt hm2_spix config="sserial_port_0=00000000" spiclk_rate=35714', hal)
        self.assertIn("net j0-pos-fb hm2_7c81.0.stbl.0.0.pos_fb => joint.0.motor-pos-fb", hal)
        self.assertIn("net j0-index joint.0.index-enable <=> hm2_7c81.0.stbl.0.0.index_enable", hal)
        self.assertIn("loadrt sserialterm tty=%s/run/drive-x,%s/run/drive-y1,%s/run/drive-y2" % (self.d, self.d, self.d), hal)
        self.assertNotIn("drive-z", hal)
        self.assertEqual(ini["MILLWEB"]["DRIVE_LINK"][1], "Y1 %s/run/drive-y1 hm2_7c81.0.stbl.0.1" % self.d)
        self.assertEqual(len(ini["MILLWEB"]["DRIVE"]), 4)
        self.assertIn("net watchdog hm2_7c81.0.watchdog.has_bit => estop-latch.0.fault-in", hal)
        self.assertIn("net estop-raw hm2_7c81.0.inm.00.input-00 => estop-not.in", hal)
        order = [l.split()[1] for l in hal.splitlines() if l.startswith("addf ")]
        self.assertEqual(order[0], "hm2_7c81.0.read")
        self.assertEqual(order[-1], "hm2_7c81.0.write")
        self.assertLess(order.index("motion-controller"), order.index("sserialterm.0"))

    def test_bridge_type(self):
        """A fixed bridge over a table moving in Y (machines/cn720): one joint an axis, the type to the console."""
        m = json.load(open(os.path.join(MILL, "machines", "cn720", "machine.json")))
        f = self.gen(m)
        ini = ini_sections(f["mill.ini"])
        self.assertEqual(ini["MILLWEB"]["MACHINE_TYPE"], ["bridge"])
        self.assertEqual(ini["KINS"]["KINEMATICS"], ["trivkins coordinates=XYZ"])
        self.assertEqual(ini["EMCMOT"]["SERVO_PERIOD"], ["500000"])
        self.assertEqual(ini_sections(self.gen(mc.DEFAULT)["mill.ini"])["MILLWEB"]["MACHINE_TYPE"], ["gantry"])
        two = copy.deepcopy(mc.DEFAULT)
        two["type"] = "bridge"
        two["joints"].append(copy.deepcopy(two["joints"][1]))
        with self.assertRaises(mc.Invalid) as e:
            mc.validate(two)
        self.assertIn(("axes.Y", "a fixed-bridge machine moves Y with one joint (2 here: that's a gantry)"), e.exception.errors)
        bad = copy.deepcopy(mc.DEFAULT)
        bad["type"] = "router"
        with self.assertRaises(mc.Invalid) as e:
            mc.validate(bad)
        self.assertEqual([w for w, _ in e.exception.errors], ["type"])

    def test_screw_lead_and_ratio(self):
        """A stmbl joint with a lead: scale comps between LinuxCNC's mm and the drive's motor radians."""
        m = json.load(open(TEST_MACHINE))
        m["board"] = {"type": "hm2_eth", "name": "7i96s", "params": "board_ip=10.10.10.10"}
        for k, j in enumerate(m["joints"]):
            j["drive"] = {"type": "stmbl_sserial", "port": 0, "channel": k}
        m["joints"][0]["mech"] = {"lead": 10, "ratio": 2}
        m["joints"][3]["mech"] = {"lead": 5, "reverse": True}
        m["joints"][1]["mech"] = {"ratio": 3}  # no lead: none (the drive works in mm)
        hal = self.gen(m)["machine.hal"]
        self.assertIn("loadrt scale names=j0-cmd,j0-vel,j0-fb,j3-cmd,j3-vel,j3-fb", hal)
        k0 = 2 * 3.141592653589793 * 2 / 10
        self.assertIn("setp j0-cmd.gain %r" % k0, hal)
        self.assertIn("setp j0-fb.gain %r" % (1 / k0), hal)
        self.assertIn("setp j3-cmd.gain %r" % (-2 * 3.141592653589793 / 5), hal)
        self.assertIn("net j0-pos-cmd-rad j0-cmd.out => hm2_7i96s.0.stbl.0.0.pos_cmd", hal)
        self.assertIn("net j0-pos-fb j0-fb.out => joint.0.motor-pos-fb", hal)
        self.assertIn("net j1-pos-cmd joint.1.motor-pos-cmd => hm2_7i96s.0.stbl.0.1.pos_cmd", hal)
        lines = hal.splitlines()
        self.assertLess(lines.index("addf j0-fb servo-thread"), lines.index("addf motion-command-handler servo-thread"))
        self.assertGreater(lines.index("addf j0-cmd servo-thread"), lines.index("addf motion-controller servo-thread"))
        self.assertLess(lines.index("addf j0-cmd servo-thread"), lines.index("addf hm2_7i96s.0.write servo-thread"))
        m["joints"][0]["mech"] = {"lead": 0, "ratio": -1}
        with self.assertRaises(mc.Invalid) as cm:
            mc.validate(m, self.d)
        self.assertEqual({w for w, _ in cm.exception.errors}, {"joints.0.mech.lead", "joints.0.mech.ratio"})

    def test_spindle_pins_and_rack_changer(self):
        m = json.load(open(TEST_MACHINE))
        m["spindle"] = {"type": "hal", "max_rpm": 24000, "on": "hm2_7i96s.0.ssr.00.out-00", "speed": "pwmgen.0.value", "at_speed": "vfd.at-speed"}
        m["atc"] = {"type": "rack", "pockets": 6, "x": 600, "y": 400, "dx": 25, "dy": 0, "z_clear": -10, "z_pocket": -150, "slide": 20,
                    "unclamp": "hm2_7i96s.0.ssr.00.out-01", "released": "hm2_7i96s.0.inm.00.input-05"}
        f = self.gen(m)
        hal, ini, ngc = f["machine.hal"], ini_sections(f["mill.ini"]), f["toolchange.ngc"]
        self.assertIn("net spindle-on spindle.0.on => hm2_7i96s.0.ssr.00.out-00", hal)
        self.assertIn("net spindle-rpm spindle.0.speed-out-abs => pwmgen.0.value", hal)
        self.assertIn("net spindle-at-speed vfd.at-speed => spindle.0.at-speed", hal)
        self.assertNotIn("setp spindle.0.at-speed 1", hal)
        self.assertIn("num_dio=4", hal)
        self.assertIn("net atc-unclamp motion.digital-out-00 => hm2_7i96s.0.ssr.00.out-01", hal)
        self.assertIn("net atc-released hm2_7i96s.0.inm.00.input-05 => motion.digital-in-01", hal)
        self.assertEqual(ini["RS274NGC"]["REMAP"], ["M6 modalgroup=6 ngc=toolchange"])
        self.assertEqual(ini["RS274NGC"]["SUBROUTINE_PATH"], [os.path.join(self.d, "run")])
        # released has a sensor: waited for; clamped hasn't: the dwell
        self.assertIn("M66 P1 L3 Q3", ngc)
        self.assertNotIn("M66 P0", ngc)
        self.assertIn("G53 G0 X[600 + [#<_pk> - 1] * 25] Y[[400 + [#<_pk> - 1] * 0] - 20]", ngc)
        self.assertIn("[#<_current_tool> LE 6]", ngc)
        self.assertTrue(ngc.rstrip().endswith("M2"))
        m["atc"].update(unclamp="", z_pocket=0)
        m["spindle"] = {"type": "hal"}
        with self.assertRaises(mc.Invalid) as cm:
            mc.validate(m, self.d)
        self.assertEqual({w for w, _ in cm.exception.errors}, {"atc.unclamp", "atc.z_pocket", "spindle.on"})

    def test_example_machines(self):
        """machines/*: each valid and made; the test stand's racks scale its drives, its switches home it."""
        for d in sorted(os.listdir(os.path.join(MILL, "machines"))):
            f = self.gen(json.load(open(os.path.join(MILL, "machines", d, "machine.json"))))
            self.assertIn("[JOINT_0]", f["mill.ini"], d)
        f = self.gen(json.load(open(os.path.join(MILL, "machines", "teststand", "machine.json"))))
        k = 2 * 3.141592653589793 / 94.24778   # a 20-tooth module 1.5 pinion: 94.25 mm a turn
        self.assertIn("setp j1-cmd.gain %r" % k, f["machine.hal"])
        ini = ini_sections(f["mill.ini"])
        self.assertEqual(ini["MILLWEB"]["MACHINE_TYPE"], ["bridge"])
        self.assertEqual((ini["JOINT_1"]["HOME_OFFSET"], ini["JOINT_1"]["HOME_SEARCH_VEL"]), (["66"], ["10"]))   # Y homes at its +65 end
        self.assertIn("net j2-home-sw hm2_7i96s.0.inm.00.input-04 => joint.2.home-sw-in", f["machine.hal"])

    def test_lathe(self):
        """machines/emco_compact5: X Z, G18 G7 at the start, LATHE in [DISPLAY], a simulated spindle encoder."""
        m = json.load(open(os.path.join(MILL, "machines", "emco_compact5", "machine.json")))
        f = self.gen(m)
        ini, hal = ini_sections(f["mill.ini"]), f["machine.hal"]
        self.assertEqual(ini["DISPLAY"]["LATHE"], ["1"])
        self.assertNotIn("BACK_TOOL_LATHE", ini["DISPLAY"])
        self.assertTrue(ini["RS274NGC"]["RS274NGC_STARTUP_CODE"][0].startswith("G18 G7 "))
        self.assertEqual(ini["KINS"]["KINEMATICS"], ["trivkins coordinates=XZ"])
        self.assertEqual(ini["MILLWEB"]["MACHINE_TYPE"], ["lathe"])
        self.assertIn("net spindle-revs sim-spindle.position-fb => spindle.0.revs", hal)
        self.assertIn("net spindle-index spindle.0.index-enable <=> sim-spindle.index-enable", hal)
        m["lathe"] = {"post": "back", "diameter": False}
        m["spindle"] = {"type": "hal", "max_rpm": 3000, "on": "vfd.run", "speed": "vfd.rpm", "revs": "enc.position", "index": "enc.index-enable"}
        f = self.gen(m)
        ini, hal = ini_sections(f["mill.ini"]), f["machine.hal"]
        self.assertEqual(ini["DISPLAY"]["BACK_TOOL_LATHE"], ["1"])
        self.assertIn(" G8 ", ini["RS274NGC"]["RS274NGC_STARTUP_CODE"][0])
        self.assertIn("net spindle-revs enc.position => spindle.0.revs", hal)
        self.assertIn("net spindle-index spindle.0.index-enable <=> enc.index-enable", hal)
        self.assertNotIn("sim-spindle", hal)
        bad = copy.deepcopy(m)
        bad["axes"]["Y"] = {"min": 0, "max": 10, "max_vel": 10, "max_acc": 100}
        bad["joints"].append({"axis": "Y", "drive": {"type": "sim"}})
        bad["lathe"]["post"] = "side"
        bad["spindle"]["revs"] = ""
        bad["toolsetter"] = {"x": 0, "y": 0, "z_above": 0, "z_ref": 0, "travel": 10}
        with self.assertRaises(mc.Invalid) as cm:
            mc.validate(bad, self.d)
        self.assertEqual({w for w, _ in cm.exception.errors}, {"axes.Y", "lathe.post", "spindle.index", "toolsetter"})

    def test_turret_spindle(self):
        """A lathe's turret (sim: the command its feedback; stepper: a stepgen and its index) on M6, its fault in
        the E-STOP chain; an stmbl spindle drive in velocity mode through lathe.spindle."""
        m = json.load(open(os.path.join(MILL, "machines", "emco_compact5", "machine.json")))
        hal = self.gen(m)["machine.hal"]
        self.assertIn("loadrt lathe turret=1 spindle=0", hal)
        self.assertIn("net tool-change iocontrol.0.tool-change => lathe.turret.change", hal)
        self.assertIn("net tool-changed lathe.turret.changed => iocontrol.0.tool-changed", hal)
        self.assertIn("net turret-pos lathe.turret.pos-cmd => lathe.turret.pos-fb", hal)
        self.assertIn("net turret-index lathe.turret.sim-index => lathe.turret.index", hal)
        self.assertIn("net estop-loop-turret lathe.turret.chain-out => iocontrol.0.emc-enable-in", hal)
        self.assertIn("addf lathe.turret servo-thread", hal)
        self.assertNotIn("iocontrol.0.tool-change => iocontrol.0.tool-changed", hal)
        os.makedirs(os.path.join(self.d, "bitstreams"), exist_ok=True)
        open(os.path.join(self.d, "bitstreams", "ebaz4205.bin"), "wb").close()
        m = json.load(open(os.path.join(MILL, "machines", "emco_compact5", "machine-stmbl.json")))
        m["estop"] = {"input": "hm2_ebaz4205.0.gpio.011.in", "invert": True}
        m["lathe"]["turret"]["index_invert"] = True
        f = self.gen(m)
        hal, ini = f["machine.hal"], ini_sections(f["mill.ini"])
        self.assertIn("loadrt lathe turret=1 spindle=1", hal)
        self.assertIn("loadrt not names=estop-not,spindle-fault-not,turret-index-not", hal)  # one load for each module
        self.assertEqual(hal.count("loadrt not "), 1)
        self.assertIn("setp hm2_ebaz4205.0.stepgen.00.position-scale 3200", hal)
        self.assertIn("net turret-pos-cmd lathe.turret.pos-cmd => hm2_ebaz4205.0.stepgen.00.position-cmd", hal)
        self.assertIn("net turret-index-raw hm2_ebaz4205.0.gpio.010.in => turret-index-not.in", hal)
        self.assertIn("net estop-in estop-not.out => lathe.turret.chain-in", hal)
        self.assertIn("net estop-in-turret lathe.turret.chain-out => spindle-ok-and.in0", hal)
        self.assertIn("net estop-in-turret-spindle spindle-ok-and.out => estop-latch.0.ok-in", hal)
        self.assertIn("net spindle-fault hm2_ebaz4205.0.stbl.0.2.fault => spindle-fault-not.in", hal)
        self.assertIn("net spindle-vel-cmd lathe.spindle.vel-cmd => hm2_ebaz4205.0.stbl.0.2.vel_cmd", hal)
        self.assertIn("net spindle-revs lathe.spindle.revs => spindle.0.revs", hal)
        self.assertIn("net spindle-index spindle.0.index-enable <=> lathe.spindle.index-enable", hal)
        self.assertIn("net spindle-con-fb hm2_ebaz4205.0.stbl.0.2.con_fb => sserialterm.2.con-fb", hal)
        a = hal.index("addf hm2_ebaz4205.0.read"); b = hal.index("addf lathe.spindle"); c = hal.index("addf motion-controller")
        self.assertTrue(a < b < c)  # the drive's feedback into revs before motion
        self.assertTrue(hal.index("addf lathe.turret") < hal.index("addf hm2_ebaz4205.0.write"))
        self.assertTrue(hal.index("loadrt not ") < hal.index("net "))
        self.assertIn("S hm2_ebaz4205.0.stbl.0.2.current", " ".join(ini["MILLWEB"]["DRIVE"]))
        bad = copy.deepcopy(m)
        bad["lathe"]["turret"].update(stepgen="", index="", stations=40)
        bad["spindle"]["channel"] = 1  # joint 1's drive
        with self.assertRaises(mc.Invalid) as cm:
            mc.validate(bad, self.d)
        self.assertEqual({w for w, _ in cm.exception.errors}, {"lathe.turret.stepgen", "lathe.turret.index", "lathe.turret.stations", "spindle.type"})

    def test_can_drives(self):
        m = copy.deepcopy(mc.DEFAULT)
        m["can"] = {"spi": ["/dev/spidev0.0", "/dev/spidev0.1"], "discover": True, "spi_hz": 8000000}
        for i, (b, n) in enumerate(((0, 1), (0, 2), (1, 3))):
            m["joints"][i]["drive"] = {"type": "stmbl_can", "bus": b, "node": n}
        hal = self.gen(m)["machine.hal"]
        self.assertIn("loadrt stmblcan spi=/dev/spidev0.0,/dev/spidev0.1 nodes=1,2:3 discover=1 spi_hz=8000000", hal)
        self.assertIn("net j2-pos-cmd joint.2.motor-pos-cmd => stmblcan.1.3.pos-cmd", hal)
        self.assertIn("net j0-con-fb stmblcan.0.1.con-fb => sserialterm.0.con-fb", hal)

    def test_save_history_diff(self):
        mc.init(self.d)
        text, custom = mc.read(self.d)
        m = json.loads(text)
        m["name"] = "Renamed"
        new = json.dumps(m, indent=2)
        self.assertIn('+  "name": "Renamed"', mc.diff(text, new))
        hid = mc.save(self.d, new, "net a b\n", "/mill", "me", "rename")
        self.assertEqual(mc.read(self.d), (new + "\n", "net a b\n"))
        h = mc.history(self.d)
        self.assertEqual((h[0]["id"], h[0]["who"], h[0]["note"]), (hid, "me", "rename"))
        self.assertEqual(mc.history_get(self.d, hid)[1], "net a b\n")
        with self.assertRaises(mc.Invalid):
            mc.save(self.d, "{", "", "/mill")
        self.assertEqual(mc.read(self.d)[0], new + "\n")  # a bad save changes nothing
        mc.save(self.d, new, "", "/mill")
        self.assertFalse(os.path.exists(os.path.join(self.d, "custom.hal")))
        with self.assertRaises(ValueError):
            mc.history_get(self.d, "../../etc")


class MachineModel(unittest.TestCase):
    """machine.json's 3D model: its parts checked; the files in models/."""

    def setUp(self):
        self.d = tempfile.mkdtemp(prefix="machmodel-test.")
        mc.init(self.d)

    def tearDown(self):
        shutil.rmtree(self.d, ignore_errors=True)

    def test_parts_and_files(self):
        import base64
        ops = supervisor.ConfigOps(self.d)
        stl = base64.b64encode(b"\0" * 80 + b"\0\0\0\0").decode()
        self.assertIsNone(ops.model_save("frame.stl", stl))
        self.assertIn("file name", ops.model_save("../x.stl", stl))
        self.assertIn("file name", ops.model_save("x.step", stl))
        self.assertIn("6 MB", ops.model_save("big.stl", base64.b64encode(b"x" * (6 * 1024 * 1024 + 1)).decode()))
        self.assertEqual([x["name"] for x in ops.models()], ["frame.stl"])
        self.assertEqual(base64.b64decode(ops.model_get("frame.stl")["data"]), b"\0" * 84)
        m = copy.deepcopy(mc.DEFAULT)
        m["model"] = {"builtin": False, "parts": [{"file": "frame.stl", "moves": ""}, {"file": "frame.stl", "moves": "-X-Y", "pos": [1, 2, 3], "color": "#112233", "opacity": 0.5}]}
        mc.validate(m, self.d)
        for part, where in (({"file": "nope.stl"}, "file"), ({"file": "frame.stl", "moves": "XX"}, "moves"), ({"file": "frame.stl", "moves": "XQ"}, "moves"),
                            ({"file": "frame.stl", "pos": [1, 2]}, "pos"), ({"file": "frame.stl", "color": "red"}, "color"), ({"file": "frame.stl", "opacity": 2}, "opacity")):
            m2 = copy.deepcopy(m)
            m2["model"]["parts"] = [part]
            with self.assertRaises(mc.Invalid) as cm:
                mc.validate(m2, self.d)
            self.assertEqual(cm.exception.errors[0][0], "model.parts.0." + where, part)
        mc.save(self.d, json.dumps(m), "", MILL)
        self.assertIn("uses it", ops.model_delete("frame.stl"))
        m["model"]["parts"] = []
        mc.save(self.d, json.dumps(m), "", MILL)
        self.assertIsNone(ops.model_delete("frame.stl"))
        self.assertEqual(ops.models(), [])


class Bitstream(unittest.TestCase):
    def setUp(self):
        self.d = tempfile.mkdtemp(prefix="bitstream-test.")
        mc.init(self.d)
        open(os.path.join(self.d, "bitstreams", "top.bit.bin"), "wb").write(b"\x00\x01bitstream" * 100)
        self.logged = []

    def tearDown(self):
        shutil.rmtree(self.d, ignore_errors=True)
        for k in ("MILLWEB_SUDO", "MILLWEB_FPGA_SYSFS", "MILLWEB_FIRMWARE_DIR"):
            os.environ.pop(k, None)

    def machine(self, **bs):
        m = copy.deepcopy(mc.DEFAULT)
        m["bitstream"] = dict(m["bitstream"], file="top.bit.bin", **bs)
        return mc.validate(m, self.d)

    def test_fpga_manager_once_per_change(self):
        sysfs, fw = os.path.join(self.d, "sys"), os.path.join(self.d, "fw")
        os.makedirs(os.path.join(sysfs, "fpga0"))
        open(os.path.join(sysfs, "fpga0", "state"), "w").write("operating\n")
        os.environ.update(MILLWEB_SUDO="", MILLWEB_FPGA_SYSFS=sysfs, MILLWEB_FIRMWARE_DIR=fw)
        m = self.machine(loader="fpga_manager")
        note = supervisor.load_bitstream(m, self.d, self.logged.append)
        self.assertIn("loaded", note)
        name = open(os.path.join(sysfs, "fpga0", "firmware")).read()
        self.assertTrue(name.startswith("millweb-") and os.path.exists(os.path.join(fw, name)))
        self.assertEqual(open(os.path.join(sysfs, "fpga0", "flags")).read(), "0")
        self.assertIn("already loaded", supervisor.load_bitstream(m, self.d, self.logged.append))
        self.assertIn("loaded in", supervisor.load_bitstream(self.machine(loader="fpga_manager", reload="always"), self.d, self.logged.append))
        open(os.path.join(sysfs, "fpga0", "state"), "w").write("write error\n")
        with self.assertRaises(RuntimeError) as cm:
            supervisor.load_bitstream(self.machine(loader="fpga_manager", reload="always"), self.d, self.logged.append)
        self.assertIn("not operating", str(cm.exception))

    def test_command(self):
        m = self.machine(loader="command", command="cp {file} loaded.bin")
        supervisor.load_bitstream(m, self.d, self.logged.append)
        self.assertEqual(open(os.path.join(self.d, "loaded.bin"), "rb").read()[:11], b"\x00\x01bitstream")
        with self.assertRaises(RuntimeError):
            supervisor.load_bitstream(self.machine(loader="command", command="false {file}"), self.d, self.logged.append)

    def test_ops_bitstreams_and_check(self):
        ops = supervisor.ConfigOps(self.d)
        import base64
        self.assertIsNone(ops.bitstream_save("new.bit", base64.b64encode(b"x").decode()))
        self.assertIn("plain file name", ops.bitstream_save("../x", "eA=="))
        self.assertEqual([b["name"] for b in ops.bitstreams()], ["new.bit", "top.bit.bin"])
        text, _ = mc.read(self.d)
        m = json.loads(text)
        m["bitstream"] = {"loader": "command", "file": "new.bit", "command": "true {file}"}
        mc.save(self.d, json.dumps(m), "", MILL)
        self.assertIn("loads it", ops.bitstream_delete("new.bit"))
        r = ops.check(json.dumps(dict(m, units="inch")), "")
        self.assertFalse(r["ok"])
        self.assertEqual(r["errors"][0][0], "units")
        r = ops.check(json.dumps(m), "net x y\n")
        self.assertTrue(r["ok"])
        self.assertIn(os.path.join(self.d, "custom.hal"), r["files"]["mill.ini"])  # the preview loads the custom.hal to be
        self.assertNotIn("custom.hal", ops.check(json.dumps(m), "")["files"]["mill.ini"])
        self.assertIn("+net x y", r["diff"])


if __name__ == "__main__":
    unittest.main()
