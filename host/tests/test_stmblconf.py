"""tools/stmblconf.py: drive configs up and down firmware versions
(docs/firmware_versions.md). python3 host/tests/test_stmblconf.py"""
import glob
import os
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import stmblconf as sc  # noqa: E402

CONF = """link pid
link pmsm
conf0.r = 2.15 # id_pmsm 2026-10-14
conf0.max_sat = 0.2
io0.out0 = fault0.mot_brake
linrev0.scale=10
"""
PATCHES = {
    2: {"version": 2, "description": "fault0.mot_brake is fault0.brake now", "ops": [{"op": "rename", "from": "fault0.mot_brake", "to": "fault0.brake"}]},
    3: {"version": 3, "description": "a higher saturation limit; the new template", "ops": [
        {"op": "set", "pin": "conf0.max_sat", "from": 0.2, "to": 0.3},
        {"op": "set", "pin": "conf0.new_pin", "from": None, "to": 5},
        {"op": "line", "from": "link pmsm", "to": "link pmsm2"}]},
}


def norm(t):
    return "\n".join(l.rstrip() for l in t.strip().splitlines())


class StmblConf(unittest.TestCase):
    def test_marker(self):
        self.assertEqual(sc.version_of(CONF), 0)
        t = sc.with_version(CONF, 7)
        self.assertTrue(t.startswith("#stmbl_conf_version 7\n"))
        self.assertEqual(sc.version_of(t), 7)
        self.assertEqual(sc.version_of(sc.with_version(t, 9)), 9)
        self.assertEqual(norm(sc.with_version(t, 0)), norm(CONF))

    def test_up_and_back_down(self):
        up, steps = sc.migrate(CONF, 3, PATCHES)
        self.assertEqual(sc.version_of(up), 3)
        self.assertIn("io0.out0 = fault0.brake", up)
        self.assertIn("conf0.max_sat = 0.3", up)
        self.assertIn("conf0.new_pin = 5", up)
        self.assertIn("link pmsm2", up)
        self.assertIn("conf0.r = 2.15 # id_pmsm 2026-10-14", up)  # comments stay
        self.assertEqual([s["version"] for s in steps], [1, 2, 3])
        self.assertTrue(any("fault0.brake" in c for c in steps[1]["changes"]))
        down, steps = sc.migrate(up, 0, PATCHES)
        self.assertEqual(norm(down), norm(CONF))
        self.assertEqual([s["direction"] for s in steps], ["down"] * 3)

    def test_a_value_set_by_hand_is_kept(self):
        t = CONF.replace("conf0.max_sat = 0.2", "conf0.max_sat = 0.25")
        up, steps = sc.migrate(t, 3, PATCHES)
        self.assertIn("conf0.max_sat = 0.25", up)
        self.assertTrue(any("kept" in c for c in steps[2]["changes"]))
        self.assertEqual(norm(sc.migrate(up, 0, PATCHES)[0]), norm(t))

    def test_a_rename_onto_a_pin_in_use_refuses(self):
        with self.assertRaises(sc.Conflict):
            sc.migrate(CONF + "fault0.brake = 1\n", 2, PATCHES)

    def test_patch_checks(self):
        for bad in ({"version": 0}, {"version": 2, "ops": [{"op": "drop"}]}, {"version": 2, "ops": [{"op": "rename", "from": "x", "to": "y0.z"}]},
                    {"version": 2, "ops": [{"op": "line"}]}):
            with self.assertRaises(ValueError):
                sc.check_patch(bad)

    def test_the_repos_patches_undo_on_every_config(self):
        """conf/patches up to this firmware and back down: every config in conf/ unchanged."""
        patches = sc.load_patches()
        top = sc.fw_version()
        self.assertGreaterEqual(top, 1)
        self.assertTrue(set(patches) <= set(range(1, top + 1)), "a patch for a version this firmware isn't yet")
        files = glob.glob(os.path.join(ROOT, "conf", "*.txt")) + glob.glob(os.path.join(ROOT, "hwtest", "*", "*", "stmbl.conf"))
        self.assertGreater(len(files), 20)
        for f in files:
            text = open(f).read()
            up, _ = sc.migrate(text, top, patches)
            self.assertEqual(sc.version_of(up), top, f)
            self.assertEqual(norm(sc.migrate(up, 0, patches)[0]), norm(text), f)

    def test_cli(self):
        import tempfile
        d = tempfile.mkdtemp()
        p = os.path.join(d, "x.conf")
        open(p, "w").write(CONF)
        self.assertEqual(sc.main(["upgrade", p]), 0)
        self.assertEqual(sc.version_of(open(p).read()), sc.fw_version())
        self.assertTrue(os.path.exists(p + ".v0.bak"))


if __name__ == "__main__":
    unittest.main()
