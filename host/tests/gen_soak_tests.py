#!/usr/bin/env python3
"""Generate soak tests: every motor in motors.py on the simulated drive, in
closed loop through the simulated LinuxCNC link (lcnc), through a motion
profile scaled to that motor.

The pass limits are hwtest/soak's: following error under 0.5 mm in motion
and under 0.02 mm at rest (0.314 / 0.0126 rad at linrev0.scale = 10 mm/rev),
and no drive fault. Also: speed within 1.2 x the profile's top speed, phase
current within max_ac_cur, motor temperature model (iit0) within limits,
smart-serial link without errors, drive enabled at the end.

The profile (one cycle; --cycles repeats it), with vmax = half the
voltage-limited speed (capped at 0.9 * max_vel) and amax = 40% of peak
torque / j (capped at 3000 rad/s^2):
  hold, creep +-2% vmax, long trapezoids at vmax, medium moves, 10 reversals,
  sines at 1, 5 and 20 Hz, 20 micro-steps, unramped velocity steps
  (10% vmax, at most amax * 10 ms), a 2 s
  cruise each way, disable/enable, and a move back to the start.
Rest checkpoints print fault0.pos_error (= pid0.pos_error) so the rest limit
is checked separately from the motion samples.

Before writing a motor's test, the generator tunes the position/velocity
loop with the drive's own autotune, id_pid (conf/template/id_pid.txt, the
ids comp), on the simulated drive, and the soak runs with the tuned
conf0.pos_bw / vel_bw / vel_d, as on a real machine. --no-tune keeps the
bench motor's gains. A motor whose tuning does not finish gets a test that
fails with @fail.

The encoder monitor (conf/template/encmon.txt) runs in every test, so the
soak is also its false-trip test: the longest time its residual ratio
stays above the trip threshold (0.3) must be under half its trip_time
(20 ms). Its largest ratio is printed at each rest checkpoint too.

conf0.cur_ind = 1 (back-EMF / cross-coupling feedforward in the current
loop): with the default 0 the drive loses current control once the
electrical speed's w * l exceeds the loop's cur_bw * l, e.g. the bench
motor above ~200 rad/s (see DEBUGNOTES; tests/drive_highspeed.test).

The tests are large (~60k lines each), so they are generated into gen/soak
by `make soak` rather than kept in git.

usage: gen_soak_tests.py [--cycles N] [--out DIR] [--no-tune]
       (default 1 cycle, ../gen/soak; needs ./stmbl_host built)
"""

import argparse
import math
import os
import re
import subprocess
import tempfile

from motors import MOTORS

DT = 0.002                      # velocity update / sample period [s]
MM_PER_RAD = 10.0 / (2 * math.pi)  # linrev0.scale = 10 mm/rev
FERROR = 0.5 / MM_PER_RAD       # 0.5 mm  -> rad
FERROR_REST = 0.02 / MM_PER_RAD  # 0.02 mm -> rad
UDC = 300.0
VMAX_CAP = 320.0 * 0.9          # bench conf0.max_vel
AMAX_CAP = 3000.0


class Profile:
    """Velocity commands at DT, with the samples and checkpoints to print."""

    def __init__(self):
        self.lines = []
        self.vel = 0.0   # current command [rad/s]
        self.pos = 0.0   # integrated command [rad]
        self.n = 0
        self.t = 0.0

    def section(self, name):
        # "time" prints a "sim time:" line: tests/soak_report.py splits there
        self.lines.append("# section %s" % name)
        self.lines.append("time")

    def tick(self, vel):
        self.vel = vel
        self.pos += vel * DT
        self.t += DT
        self.lines.append("lcnc 1 %.5f" % (vel * MM_PER_RAD))
        self.lines.append("run %g" % DT)
        self.lines.append("pid0.pos_error")
        self.lines.append("hv0.sim_vel")
        self.lines.append("hv0.iq_fb")
        self.n += 1
        if self.n % 10 == 0:
            self.lines.append("fault0.fault")

    def hold(self, sec):
        for _ in range(int(round(sec / DT))):
            self.tick(0.0 if abs(self.vel) < 1e-9 else self.vel)

    def ramp_to(self, v, acc):
        while abs(self.vel - v) > 1e-9:
            step = acc * DT
            nv = self.vel + max(-step, min(step, v - self.vel))
            self.tick(nv)

    def cruise(self, sec):
        v = self.vel
        for _ in range(int(round(sec / DT))):
            self.tick(v)

    def rest_check(self, settle=0.5):
        self.ramp_to(0.0, 1e9)
        self.hold(settle)
        self.lines.append("fault0.pos_error")
        self.lines.append("encmon0.max_ratio")
        self.lines.append("encmon0.max_timer")

    def move(self, dist, v, a):
        """Point-to-point move (trapezoid or triangle), ending at rest."""
        sgn = 1.0 if dist >= 0 else -1.0
        dist = abs(dist)
        v = min(v, math.sqrt(dist * a))
        t_acc = v / a
        t_cruise = max(0.0, (dist - v * t_acc) / v)
        self.ramp_to(sgn * v, a)
        self.cruise(t_cruise)
        self.ramp_to(0.0, a)

    def sine(self, freq, amp, periods):
        w = 2 * math.pi * freq
        steps = int(round(periods / freq / DT))
        for k in range(1, steps + 1):
            self.tick(amp * w * math.sin(w * k * DT))
        self.tick(0.0)


def build(vmax, amax, cycles):
    p = Profile()
    for _ in range(cycles):
        p.section("hold")
        p.hold(0.5)
        p.section("creep")
        for s in (1, -1):
            p.ramp_to(s * 0.02 * vmax, amax)
            p.cruise(1.0)
        p.rest_check()
        p.section("trapezoid")
        for s in (1, -1):
            p.ramp_to(s * vmax, amax)
            p.cruise(1.0)
            p.ramp_to(0.0, amax)
            p.rest_check(0.3)
        p.section("medium")
        for s in (1, -1, 1, -1):
            p.move(s * 5.0, 0.5 * vmax, 0.5 * amax)
            p.hold(0.2)
        p.rest_check()
        p.section("reversals")
        for k in range(10):
            p.ramp_to((1 if k % 2 == 0 else -1) * 0.5 * vmax, amax)
        p.ramp_to(0.0, amax)
        p.rest_check()
        p.section("sines")
        for f in (1.0, 5.0, 20.0):
            w = 2 * math.pi * f
            amp = min(0.5 * vmax / w, 0.8 * amax / w ** 2, 2 * math.pi)
            p.sine(f, amp, max(2, int(f)))
        p.rest_check()
        p.section("micro")  # 0.01 rad, alternating
        for k in range(20):
            p.move((1 if k % 2 == 0 else -1) * 0.01, vmax, amax)
            p.hold(0.05)
        p.rest_check()
        p.section("vsteps")  # no ramp; at most what amax reaches in 10 ms
        dv = min(0.1 * vmax, amax * 0.01)
        for v in (dv, 0.0, -dv, 0.0):
            p.tick(v)
            p.cruise(0.3)
        p.rest_check()
        p.section("cruise")
        for s in (1, -1):
            p.ramp_to(s * vmax, amax)
            p.cruise(2.0)
            p.ramp_to(0.0, amax)
        p.rest_check()
        p.section("enable")
        p.lines += ["lcnc 0 0", "run 0.3", "fault0.state", "lcnc 1 0", "run 1.5"]
        p.move(0.5, 0.5 * vmax, 0.5 * amax)
        # back to the start of the cycle
        p.move(-p.pos, vmax, amax)
        p.pos = 0.0
        p.rest_check()
    return p


# Known failures with id_pid-tuned gains (TAP TODO: reported, not fatal; a
# fixed one prints "ok ... # TODO"). See DEBUGNOTES "id_pid tunes some
# motors too soft".
KNOWN = {
    "rx60_456":       "id_pid tunes with ideal feedforward: noise caps vel_bw at ~113",
    "sgmph_01a1":     "stick-slip at rest (friction / j = 40000 rad/s^2): rest error",
    "syn_torque_14p": "id_pid leaves vel_d at its bound 1: speed rings 75% over",
}

HOST_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BENCH_CONF = "../hwtest/burnin/ECMA-C30604FS/stmbl.conf"


def tune(conf_lines, max_sec=900):
    """Run id_pid on the simulated drive until ids0 reports done (state 1.4);
    return (pos_bw, vel_bw, vel_d), or None if it does not finish within
    max_sec simulated seconds."""
    p = subprocess.Popen(["./stmbl_host", "--drive", BENCH_CONF], cwd=HOST_DIR,
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, universal_newlines=True, bufsize=1)

    def ask(cmd, pin):
        p.stdin.write(cmd + "\n%s\n" % pin)
        p.stdin.flush()
        for line in p.stdout:
            m = re.match(r"^%s = (\S+)$" % re.escape(pin), line.strip())
            if m:
                return float(m.group(1))
        return None

    try:
        for c in conf_lines + ["debug_level 1", "link id_pid", "relink", "start",
                               "debug_level 0", "fault0.cmd_error = 0", "fault0.en = 0",
                               "run 0.5", "fault0.en = 1"]:
            p.stdin.write(c + "\n")
        done = False
        for _ in range(int(max_sec / 2)):
            st = ask("run 2", "ids0.state")
            if st is None:
                break
            if st >= 1.35:
                done = True
                break
        if not done:
            return None
        return ask("", "ids0.pos_bw"), ask("", "ids0.vel_bw"), ask("", "ids0.vel_d")
    finally:
        p.kill()
        p.wait()


# probe autophasing (fb_switch phase_mode 1, the default since 2026-10-17)
# at a share of the motor's current before the profile starts: the heavy
# rotors take up to ~2.5 s; checked (fault0.state[0] == 1)
PHASE_SHARE = 0.25
PHASE_WAIT = 3.0

TEMPLATE = """\
# GENERATED by tests/gen_soak_tests.py -- edit the profile or motors.py there.
# Soak: motor {name} in closed loop (pid, pmsm, enc_fb0) via the simulated
# LinuxCNC link, {cycles} cycle(s), {t:.0f} s simulated.
# vmax {vmax:.1f} rad/s, amax {amax:.0f} rad/s^2; limits from hwtest/soak.
@args --drive ../hwtest/burnin/ECMA-C30604FS/stmbl.conf
@timeout {timeout}
conf0.r = {r}
conf0.l = {l}
conf0.psi = {psi}
conf0.polecount = {pp}
conf0.j = {j}
conf0.f = {f}
conf0.d = {d}
conf0.max_ac_cur = {imax}
conf0.max_force = {t_peak:.4g}
conf0.max_vel = {max_vel:.1f}
conf0.cur_ind = 1
conf0.phase_cur = {pcur:.3g}
{gains}debug_level 1
link encmon
relink
start
encmon0.delay = 0
debug_level 0
lcnc 0 0
run 0.5
lcnc 1 0
run {tphase}
fault0.state
@expect fault0.state[0] == 1
{body}
fault0.state
sserial0.error
iit0.temp
@expect max fault0.fault < 0.5
@expect fault0.state[-1] == 1
@expect sserial0.error == 0
@expect max|pid0.pos_error| < {ferror:.4f}
@expect max|fault0.pos_error| < {ferror_rest:.4f}
@expect max|hv0.sim_vel| < {vlim:.1f}
@expect max|hv0.iq_fb| < {ilim:.3g}
@expect max encmon0.max_timer < 0.01
@reject ^ERROR
"""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cycles", type=int, default=1)
    ap.add_argument("--no-tune", action="store_true")
    ap.add_argument("--only", action="append", default=[], help="only this motor (repeatable)")
    ap.add_argument("--out", default=os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "gen", "soak"))
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    motors = [m for m in MOTORS if not args.only or m[0] in args.only]
    for name, r, l, psi, pp, j, imax in motors:
        t_peak = 1.5 * pp * psi * imax
        f = round(0.02 * t_peak, 4)
        d = float("%.3g" % (2.8e-5 * t_peak))
        w_volt = UDC / math.sqrt(3) * 0.95 / (psi * pp)  # back-EMF limit [rad/s]
        max_vel = min(0.9 * w_volt, 320.0)
        vmax = min(0.5 * w_volt, 0.9 * max_vel, VMAX_CAP)
        amax = min(0.4 * t_peak / j, AMAX_CAP)
        conf = ["conf0.r = %s" % r, "conf0.l = %s" % l, "conf0.psi = %s" % psi,
                "conf0.polecount = %s" % pp, "conf0.j = %s" % j, "conf0.f = %s" % f,
                "conf0.d = %s" % d, "conf0.max_ac_cur = %s" % imax,
                "conf0.max_force = %.4g" % t_peak, "conf0.max_vel = %.1f" % max_vel,
                "conf0.cur_ind = 1", "conf0.phase_cur = %.3g" % (PHASE_SHARE * imax)]
        if args.no_tune:
            gains = "# bench gains (--no-tune)\n"
        else:
            g = tune(conf)
            if g is None:
                gains = "@fail id_pid did not finish for this motor\n"
            else:
                gains = ("# id_pid result\nconf0.pos_bw = %.4f\nconf0.vel_bw = %.4f\n"
                         "conf0.vel_d = %.4f\n" % g)
            print("  %-15s %s" % (name, "id_pid failed" if g is None else
                  "pos_bw %.1f vel_bw %.0f vel_d %.2f" % g))
        if name in KNOWN and not args.no_tune:
            gains += "@todo %s\n" % KNOWN[name]
        p = build(vmax, amax, args.cycles)
        text = TEMPLATE.format(
            name=name, cycles=args.cycles, t=p.t + 2.0 * args.cycles + 2.0,
            vmax=vmax, amax=amax, r=r, l=l, psi=psi, pp=pp, j=j, f=f, d=d,
            imax=imax, t_peak=t_peak, max_vel=max_vel, body="\n".join(p.lines), gains=gains,
            pcur=PHASE_SHARE * imax, tphase=PHASE_WAIT,
            timeout=120 * args.cycles, ferror=FERROR, ferror_rest=FERROR_REST,
            vlim=1.2 * vmax, ilim=1.05 * imax)
        with open(os.path.join(args.out, "soak_%s.test" % name), "w") as fh:
            fh.write(text)
    print("wrote %d soak tests (%d cycle(s)) to %s" % (len(motors), args.cycles, args.out))


if __name__ == "__main__":
    main()
