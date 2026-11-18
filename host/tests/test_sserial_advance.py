#!/usr/bin/env python3
"""tools/sserial_advance.py's fit on synthetic runs: back-and-forth moves
with trapezoid profiles, a following error of a known delay * vel (plus an
acceleration term, an offset and noise, sampled at LinuxCNC's ~1 kHz task
rate with jitter). The fit must find the delay. Prints TAP."""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))
import sserial_advance as sa  # noqa: E402

n = fails = 0


def ok(c, name):
    global n, fails
    n += 1
    fails += not c
    print("%s %d - sserial_advance: %s" % ("ok" if c else "not ok", n, name))


def run(delay, b=2e-6, c=1e-4, noise=2e-6, seed=1, vmax=50.0, amax=2000.0):
    rng = np.random.default_rng(seed)
    t = np.cumsum(rng.uniform(0.0008, 0.0012, 8000))  # ~1 kHz, jittered
    # back and forth: accelerate, cruise 0.3 s, decelerate, reverse
    period = 2 * (2 * vmax / amax + 0.3)
    ph = np.mod(t, period) / period
    ta = vmax / amax / period  # acceleration share of the period
    cr = 0.3 / period
    v = np.zeros_like(t)
    for s, start in ((1, 0.0), (-1, 0.5)):
        p = ph - start
        v += s * np.where((p >= 0) & (p < ta), p / ta,
                          np.where((p >= ta) & (p < ta + cr), 1.0,
                                   np.where((p >= ta + cr) & (p < 2 * ta + cr), (2 * ta + cr - p) / ta, 0.0))) * vmax
    acc = np.gradient(v, t)
    fe = delay * v + b * acc + c + rng.normal(0, noise, len(t))
    return t, fe, v


for delay in (200e-6, -150e-6, 30e-6):
    t, fe, v = run(delay)
    r = sa.fit(t, fe, v, 0.0006)
    ok(abs(r["k"] - delay) < 3e-6, f"finds a {delay * 1e6:+.0f} us delay (got {r['k'] * 1e6:+.1f})")
    ok(abs(r["new"] - (0.0006 + delay)) < 3e-6, f"new pos_advance = old + {delay * 1e6:+.0f} us")

t, fe, v = run(5e-6)
r = sa.fit(t, fe, v, 0.0006)
ok(any("keep the present value" in s for s in r["lines"]), "under 20 us: says keep it")
t, fe, v = run(-900e-6)
r = sa.fit(t, fe, v, 0.0006)
ok(any(s.startswith("warning: outside") for s in r["lines"]), "a result below 0: warns")
ok(any(s.startswith("sserial0.pos_advance = ") for s in r["lines"]), "prints the config line")

for args, why in (((np.arange(50.0), np.zeros(50), np.ones(50)), "too few samples"),
                  ((np.arange(500.0), np.zeros(500), np.zeros(500)), "joint never moved")):
    try:
        sa.fit(*args, 0.0006)
        ok(False, f"{why}: refused")
    except ValueError:
        ok(True, f"{why}: refused")

print("1..%d" % n)
sys.exit(1 if fails else 0)
