"""Induction motors for gen_acim_tests.py: T equivalent circuit per phase
(star equivalent, dq amplitudes), leakage split evenly between stator and
rotor. Values are typical for small 230 V 50 Hz motors of the given power and
pole count; the nameplate (rated speed, torque, current) is computed from the
circuit at rated power, as a real motor's plate reflects its parameters.
"""

import cmath
import math

# name, rated power [W], pole pairs, rs, rr [ohm], lm, ll [H], j [kg m^2],
# rated voltage [V, phase peak], rated frequency [Hz]
MOTORS = [
    ("acim_100w_4p", 100, 2, 40.0, 39.0, 1.2, 0.15, 0.00015, 188, 50),
    ("acim_180w_6p", 180, 3, 14.0, 14.0, 0.75, 0.10, 0.0006, 188, 50),
    ("acim_370w_4p", 370, 2, 9.5, 8.5, 0.55, 0.045, 0.0012, 188, 50),
    ("acim_750w_2p", 750, 1, 4.5, 3.8, 0.40, 0.025, 0.0008, 188, 50),
]


def steady(m, u, we, wr):
    """Torque [Nm] and stator current amplitude [A] at voltage u, electrical
    supply / rotor speeds we, wr."""
    _, _, pp, rs, rr, lm, ll, _, _, _ = m
    s = (we - wr) / we
    zm = 1j * we * lm
    zr = rr / s + 1j * we * ll / 2
    i = u / (rs + 1j * we * ll / 2 + zm * zr / (zm + zr))
    ir = i * zm / (zm + zr)
    return 1.5 * abs(ir) ** 2 * rr / s / (we / pp), abs(i)


def nameplate(m):
    """Rated mechanical speed [rad/s], torque [Nm], slip [pu], current [A
    amplitude] at rated power, voltage and frequency."""
    name, p_n, pp, rs, rr, lm, ll, j, u_n, f_n = m
    we = 2 * math.pi * f_n
    lo, hi = 0.3 * we / pp, we / pp * (1 - 1e-6)
    for _ in range(80):
        w = (lo + hi) / 2
        t, _ = steady(m, u_n, we, w * pp)
        if t * w > p_n:
            lo = w
        else:
            hi = w
    t, i = steady(m, u_n, we, lo * pp)
    return lo, t, 1 - lo / (we / pp), i
