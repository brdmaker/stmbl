"""dynwiz on the simulated machines (host/portal/machine and
machine/retrofit): the whole wizard through its command line, and what it
finds against what the portal files say (python3 -m unittest; ~2 min, the
machines in parallel). Needs host/portal/portal_sim built."""
import json
import math
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
WIZ = os.path.join(HERE, "..", "dynwiz")
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
MACH = os.path.join(ROOT, "host", "portal", "machine")
sys.path.insert(0, os.path.join(HERE, ".."))
import dynback  # noqa: E402

# name: portal file, extra arguments
RUNS = {
    "portal": ("portal.portal", ["--acc", "5000", "--vmax", "1600"]),
    "laser3015": ("retrofit/laser3015.portal", []),
    "cmm": ("retrofit/cmm.portal", []),
    "pcbdrill": ("retrofit/pcbdrill.portal", []),
    # the scales and currents can't see this light, stiff head (the wizard
    # says so); with a sensor on the head they can
    "dispenser": ("retrofit/dispenser.portal", []),
    "dispenser_tool": ("retrofit/dispenser.portal", ["--use-tool", "--fmin", "30"]),
    # the gantry yaw fight: the configs' fixes undone
    "laser_untuned": ("retrofit/laser3015.portal", ["--sim-line", "drv y1 conf0.cur_bw = 1000",
                                                    "--sim-line", "drv y2 conf0.cur_bw = 1000"]),
    "pcbdrill_untuned": ("retrofit/pcbdrill.portal", ["--sim-line", "drv y1 conf0.cur_bw = 1000",
                                                      "--sim-line", "drv y2 conf0.cur_bw = 1000"]),
}
STEPS = {"dispenser": ["mass", "modes", "write"], "dispenser_tool": ["modes", "circle", "verify"],
         "laser_untuned": ["mass"], "pcbdrill_untuned": ["cogging"]}

TMP = None
OUT = {}


def run_one(name):
    portal, extra = RUNS[name]
    sess = os.path.join(TMP, name + ".json")
    ini = os.path.join(TMP, name + ".ini")
    with open(ini, "w") as f:
        f.write("[EMC]\nMACHINE = %s\n\n[PCOMP]\nSHAPER = 0\nFX = 1\n\n[TRAJ]\nCOORDINATES = X Y Y\n" % name)
    log = []
    for step in STEPS.get(name, ["run"]):
        p = subprocess.run([sys.executable, WIZ, "-b", "sim", "--portal", os.path.join(MACH, portal), "-s", sess, "-y",
                            "--ini", ini, "--conf-out", TMP] + extra + [step], capture_output=True, text=True)
        log.append(p.stdout + p.stderr)
        if p.returncode:
            break
    return name, p.returncode, "".join(log), json.load(open(sess)) if os.path.exists(sess) else {}, open(ini).read()


def setUpModule():
    global TMP
    TMP = tempfile.mkdtemp(prefix="dynwiz-test.")
    with ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as ex:
        for name, rc, log, sess, ini in ex.map(run_one, RUNS):
            OUT[name] = {"rc": rc, "log": log, "s": sess.get("results", {}), "ini": ini}


def tearDownModule():
    shutil.rmtree(TMP, ignore_errors=True)


def truth(name):
    return dynback.SimBackend(os.path.join(MACH, RUNS[name][0])).truth


def wrap180(a):
    return (a + 180.0) % 360.0 - 180.0


class Machines(unittest.TestCase):
    def res(self, name):
        o = OUT[name]
        self.assertEqual(o["rc"], 0, o["log"][-3000:])
        return o["s"]

    def check_mass(self, name, tol):
        t = truth(name)
        r = self.res(name)["mass"]["axes"]
        y = t["bridge"] + t["carriage"] + t["head"]["m"]
        x = t["carriage"] + t["head"]["m"]
        self.assertAlmostEqual(r["y"], y, delta=tol * y, msg="%s: Y moves %.1f kg, true %.1f" % (name, r["y"], y))
        self.assertAlmostEqual(r["x"], x, delta=tol * x, msg="%s: X moves %.1f kg, true %.1f" % (name, r["x"], x))

    def check_modes(self, name, tol_end=0.12, tol_mid=0.08, tol_x=0.10):
        t = truth(name)["head"]
        m = self.res(name)["modes"]
        self.assertFalse(m.get("model_doubtful"), name)
        mo = m["model"]
        for k, tk, tol in (("fx", "fx", tol_x), ("fy_end", "fy_end", tol_end), ("fy_mid", "fy_mid", tol_mid)):
            self.assertAlmostEqual(mo[k], t[tk], delta=tol * t[tk], msg="%s: %s %.1f Hz, true %.1f" % (name, k, mo[k], t[tk]))
        # what the scales (or currents) showed against how the tool really rang
        for e in m["y"]:
            if e.get("tool") and e["tool"]["snr"] > 20:
                self.assertAlmostEqual(e["y"]["f"], e["tool"]["f"], delta=0.05 * e["tool"]["f"],
                                       msg="%s at X %.0f: %.1f Hz, the tool %.1f" % (name, e["X"], e["y"]["f"], e["tool"]["f"]))

    def check_clean(self, name):
        s = self.res(name)
        for k, mo in s.get("mass", {}).get("motors", {}).items():
            self.assertNotIn("oscillation", mo, "%s %s: %s" % (name, k, mo.get("oscillation")))
        for k, f in s.get("cogging", {}).items():
            if isinstance(f, dict):
                self.assertNotIn("oscillation", f, "%s %s" % (name, k))

    def test_portal(self):
        self.check_mass("portal", 0.05)
        self.check_modes("portal")
        self.check_clean("portal")
        v = self.res("portal")["verify"]
        self.assertLess(v["on"]["ring_um"], 0.7 * v["off"]["ring_um"])

    def test_laser(self):
        self.check_mass("laser3015", 0.06)
        self.check_modes("laser3015")
        self.check_clean("laser3015")
        s = self.res("laser3015")
        t = truth("laser3015")
        x0, y0 = t["origin"]
        for k, c in t["cogging"].items():
            f = s["cogging"][k]
            self.assertTrue(f["clear"], k)
            self.assertAlmostEqual(f["force"], c["a"], delta=0.1 * c["a"], msg=k)
            self.assertAlmostEqual(f["period"], c["period"], delta=0.01 * c["period"], msg=k)
            # the portal file's phase is at the plant's position (from the
            # origin); dynwiz reports it at LinuxCNC's
            ph = c["phase"] - 360.0 * (x0 if k == "x" else y0) / c["period"]
            self.assertLess(abs(wrap180(f["phase"] - ph)), 25.0, "%s: phase %.0f, expected %.0f" % (k, f["phase"], wrap180(ph)))
        v = s["verify"]
        self.assertLess(v["on"]["ring_um"], 0.5 * v["off"]["ring_um"])

    def test_cmm(self):
        self.check_mass("cmm", 0.05)
        self.check_modes("cmm")
        self.check_clean("cmm")
        s = self.res("cmm")
        self.assertAlmostEqual(s["modes"]["model"]["zeta"], truth("cmm")["head"]["zeta"], delta=0.01)
        v = s["verify"]
        self.assertLess(v["on"]["t_settle_ms"], 0.3 * v["off"]["t_settle_ms"])
        self.assertLess(v["on"]["ring_um"], 0.1 * v["off"]["ring_um"])

    def test_pcbdrill(self):
        self.check_mass("pcbdrill", 0.05)
        self.check_modes("pcbdrill")
        self.check_clean("pcbdrill")
        base = self.res("pcbdrill")["modes"]["base_hz"]
        self.assertTrue(any(abs(f - 18.0) < 2.0 for f in base), base)  # the frame on its feet (y 18 Hz)

    def test_dispenser(self):
        self.check_mass("dispenser", 0.06)
        self.check_clean("dispenser")
        s = self.res("dispenser")
        self.assertTrue(s["modes"].get("model_doubtful"), "the scales can't see the head: the model must be doubted")
        self.check_modes("dispenser_tool", tol_end=0.10, tol_mid=0.08, tol_x=0.10)

    def test_untuned_detected(self):
        lm = self.res("laser_untuned")["mass"]["motors"]
        for k in ("y1", "y2"):
            self.assertIn("oscillation", lm[k])
            self.assertGreater(lm[k]["oscillation"]["f"], 100)
        pc = self.res("pcbdrill_untuned")["cogging"]
        for k in ("y1", "y2"):
            self.assertIn("oscillation", pc[k])

    def test_write(self):
        ini = OUT["cmm"]["ini"]
        m = self.res("cmm")["modes"]["model"]
        self.assertIn("FX = %.1f" % m["fx"], ini)
        self.assertIn("FY_MID = %.1f" % m["fy_mid"], ini)
        self.assertIn("SHAPER = 1", ini)
        self.assertIn("RAILS = 1.1", ini)
        self.assertTrue([f for f in os.listdir(TMP) if f.startswith("cmm.ini.dynwiz-")], "no backup of the INI")
        # the doubtful model isn't written
        self.assertIn("FX = 1\n", OUT["dispenser"]["ini"])

    def test_write_confs(self):
        # nothing to change on the tuned machines
        self.assertFalse(self.res("pcbdrill").get("write", {}).get("confs"))
        self.assertFalse(os.path.exists(os.path.join(TMP, "pcbdrill_y.conf.dynwiz")))


if __name__ == "__main__":
    unittest.main()
