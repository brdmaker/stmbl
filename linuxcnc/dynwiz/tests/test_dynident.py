"""dynident on synthetic signals with known answers (python3 -m unittest)."""
import math
import os
import random
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import dynident as di  # noqa: E402

DT = 0.001


class Ringdown(unittest.TestCase):
    def test_damped_sine_on_a_settling_servo(self):
        rnd = random.Random(1)
        f, z = 31.0, 0.04
        w = 2 * math.pi * f
        y = []
        for k in range(360):
            t = k * DT
            y.append(0.02 * math.exp(-z * w * t) * math.sin(w * math.sqrt(1 - z * z) * t + 0.3)
                     + 0.005 * math.exp(-t / 0.03) + 0.0002 * rnd.gauss(0, 1))
        r = di.ringdown(y, DT, 5, 200)
        self.assertAlmostEqual(r["f"], f * math.sqrt(1 - z * z), delta=0.3)
        self.assertAlmostEqual(r["zeta"], z, delta=0.01)
        self.assertGreater(r["snr"], 10)


class Beam(unittest.TestCase):
    def test_exact_model(self):
        fe, fm, L, xc = 55.0, 28.0, 1.2, 500.0
        pts = []
        for X in (100, 300, 500, 700, 900):
            u = 2 * (X - xc) / (L * 1000)
            pts.append((X, 1 / math.sqrt(1 / fe ** 2 + (1 / fm ** 2 - 1 / fe ** 2) * (1 - u * u))))
        e, m, err = di.beam_fit(pts, xc, L)
        self.assertAlmostEqual(e, fe, places=6)
        self.assertAlmostEqual(m, fm, places=6)
        self.assertLess(err, 1e-6)


def trapezoid(d, a, v, dt=DT):
    """A trapezoid move of d [mm] at a [mm/s^2], v [mm/s]: positions."""
    ta = v / a
    if d < v * ta:
        ta = math.sqrt(d / a)
        v = a * ta
    tc = (d - v * ta) / v
    T = 2 * ta + tc
    out = []
    for k in range(int(T / dt) + 1):
        t = k * dt
        if t < ta:
            s = 0.5 * a * t * t
        elif t < ta + tc:
            s = 0.5 * a * ta * ta + v * (t - ta)
        else:
            u = T - t
            s = d - 0.5 * a * u * u
        out.append(s)
    out.append(d)
    return out


class Mass(unittest.TestCase):
    def test_force_model(self):
        m, kf, fc, b = 21.0, 148.8, 25.0, 30.0
        p = []
        x = 0.0
        for d, a in ((200, 5000), (-200, 5000), (200, 10000), (-200, 10000)):
            seg = trapezoid(abs(d), a, 800)
            p += [x + math.copysign(s, d) for s in seg]
            x = p[-1]
            p += [x] * 100
        v, acc = di.derivatives(p, DT)
        cur = [(m * acc[k] / 1000 + (fc * (1 if v[k] > 0 else -1) if abs(v[k]) > 1 else 0) + b * v[k] / 1000) / kf + 0.05
               for k in range(len(p))]
        r = di.mass_fit(p, cur, DT)
        self.assertAlmostEqual(r["k_a"] * kf, m, delta=0.1)
        self.assertAlmostEqual(r["k_c"] * kf, fc, delta=0.5)
        self.assertAlmostEqual(r["i0"], 0.05, delta=0.005)


class Cogging(unittest.TestCase):
    def test_ripple(self):
        per, amp, ph = 22.86, 0.2, 40.0
        pos, cur, sg = [], [], []
        for k in range(4000):
            for s in (1, -1):
                p = 100 + 0.05 * k if s > 0 else 300 - 0.05 * k
                pos.append(p)
                sg.append(s)
                cur.append(0.15 * s + amp * math.sin(2 * math.pi * p / per + math.radians(ph)) + 0.001 * p)
        r = di.cogging_fit(pos, cur, sg)
        self.assertAlmostEqual(r["period"], per, delta=0.05)
        self.assertAlmostEqual(r["amp"], amp, delta=0.005)
        self.assertAlmostEqual(r["phase"], ph, delta=2.0)


class Circle(unittest.TestCase):
    def test_lag(self):
        R, v, tau2 = 20.0, 500.0, 1.5e-5
        a = v * v / R
        rf = R - tau2 * a
        cx, cy, fx, fy = [], [], [], []
        for k in range(1000):
            th = v / R * k * DT
            cx.append(R * math.cos(th))
            cy.append(R * math.sin(th))
            fx.append(rf * math.cos(th - 0.01))
            fy.append(rf * math.sin(th - 0.01))
        r = di.circle_lag(cx, cy, fx, fy, DT, (0, 0))
        self.assertAlmostEqual(r["tau2"], tau2, delta=1e-7)


class Moves(unittest.TestCase):
    def test_runs(self):
        c = [0] * 10 + [0.1 * k for k in range(1, 11)] + [1.0] * 10
        self.assertEqual(di.moves(c, DT), [(9, 20)])


if __name__ == "__main__":
    unittest.main()
