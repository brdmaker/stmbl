"""tests/motors.py against conf/*.txt: a motor-table row named after a config
(conf/fanuc_a2-3000.txt -> fanuc_a2_3000) holds that config's r, l, psi,
polecount, j and max_ac_cur, so the autotune, soak and stop tests run the
motor the config describes. python3 host/tests/test_motor_table.py"""
import glob
import os
import re
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
from motors import MOTORS  # noqa: E402

# rows that keep other values on purpose, with the reason
KEEP = {
    # catalog values; the conf holds the bench commissioning (its header)
    "sew_cmp40m",
}


def conf_values(path):
    v = {}
    with open(path) as f:
        for line in f:
            m = re.match(r"\s*conf0\.(\w+)\s*=\s*([-+0-9.eE]+)", line)
            if m:
                v[m.group(1)] = float(m.group(2))
    if "psi" not in v and {"max_force", "polecount", "max_ac_cur"} <= set(v):
        # motors.py: psi from the torque constant at peak current
        v["psi"] = v["max_force"] / (1.5 * v["polecount"] * v["max_ac_cur"])
    return v


class MotorTable(unittest.TestCase):
    def test_rows_match_their_configs(self):
        confs = {os.path.basename(f)[:-4].replace("-", "_"): f
                 for f in glob.glob(os.path.join(ROOT, "conf", "*.txt"))}
        checked = 0
        for name, r, l, psi, pp, j, imax in MOTORS:
            if name not in confs or name in KEEP:
                continue
            v = conf_values(confs[name])
            want = {"r": r, "l": l, "psi": psi, "polecount": pp, "j": j, "max_ac_cur": imax}
            for k, w in want.items():
                self.assertIn(k, v, "%s: conf has no conf0.%s" % (name, k))
                self.assertAlmostEqual(v[k], w, delta=0.01 * abs(w) + 1e-9,
                                       msg="%s: motors.py %s %g, conf %g" % (name, k, w, v[k]))
            checked += 1
        self.assertGreater(checked, 25)

    def test_fanuc_a2_3000_is_in_the_table(self):
        self.assertIn("fanuc_a2_3000", [m[0] for m in MOTORS])


class FanucA2(unittest.TestCase):
    """conf/fanuc_a2-3000.txt's [calc] lines follow from its datasheet lines
    (B-65142E/02: Kt 0.67 Nm/A rms, Is 2.9 A rms, tt 20 min, Im 32 A pk)."""

    def setUp(self):
        self.v = conf_values(os.path.join(ROOT, "conf", "fanuc_a2-3000.txt"))
        with open(os.path.join(ROOT, "conf", "fanuc_a2-3000.txt")) as f:
            for line in f:
                m = re.match(r"\s*iit0\.(\w+)\s*=\s*([-+0-9.eE]+)", line)
                if m:
                    self.v["iit_" + m.group(1)] = float(m.group(2))

    def test_psi_from_kt(self):
        kt_pk = 0.67 / 2 ** 0.5
        self.assertAlmostEqual(self.v["psi"], kt_pk / (1.5 * self.v["polecount"]), delta=0.001)

    def test_max_force_is_what_max_ac_cur_makes(self):
        t = 1.5 * self.v["polecount"] * self.v["psi"] * self.v["max_ac_cur"]
        self.assertAlmostEqual(self.v["max_force"], t, delta=0.05)

    def test_iit_continuous_current_is_is(self):
        # iit: steady temperature high_temp at max_cur / cur_boost
        self.assertAlmostEqual(self.v["max_ac_cur"] / self.v["iit_cur_boost"], 2.9 * 2 ** 0.5, delta=0.05)

    def test_iit_time_constant_is_tt(self):
        # iit.c: tau = max_time x cur_boost^2 x (high-amb) / (max-amb), 50/70 by default
        tau = self.v["iit_max_time"] * self.v["iit_cur_boost"] ** 2 * 50.0 / 70.0
        self.assertAlmostEqual(tau, 20 * 60, delta=12)

    def test_current_stays_off_the_demagnetizing_limit(self):
        # an over-current trip at 1.3 x max_ac_cur stays below Im 32 A pk
        self.assertLess(1.3 * self.v["max_ac_cur"], 32 / 2)


if __name__ == "__main__":
    unittest.main()
