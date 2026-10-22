#!/usr/bin/env python3
"""The setup wizard (servoterm-cli --wizard, docs/setup_wizard.md) end to end
against the simulated drive: every motor in motors.py, each with an encoder
variant (line count, counting direction, phase order, index position), from
a blank drive to the saved config. The simulated motor is fixed by a --sim
file (it doesn't follow what the wizard writes into conf0); the wizard's
answers come from an answers file, and the hand turning the shaft is
hv0.sim_turn.

Checks per motor (TAP): the wizard finishes; the encoder's resolution and
direction, the phase order and the pole pairs exactly; r, l, psi within a
few percent; the commutation offset to the same electrical angle as the
index; j and f roughly (the console prints 6 decimals: small inertias lose
resolution); the final config's verify step moved and held.

usage: run_wizard_sim.py [-j N] [--speed N] [--only NAME[,NAME]] [--keep]
"""

import argparse
import math
import os
import shutil
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
HOST = os.path.dirname(HERE)
ROOT = os.path.dirname(HOST)
STMBL_HOST = os.path.join(HOST, "stmbl_host")
CLI = os.path.join(ROOT, "servoterm-cli", "servoterm-cli")

sys.path.insert(0, HERE)
from motors import MOTORS  # noqa: E402

# lines, encoder counting reversed, phases swapped, index angle [rad]
VARIANTS = [
    (2500, 1, 0, -0.630835),
    (1000, 0, 0, 1.2),
    (5000, 0, 1, 2.5),
    (500, 1, 1, -2.0),
    (1024, 0, 1, 0.3),
    (2048, 1, 0, -1.1),
]

UDC = 230.0 * math.sqrt(2)

# Known measurement limits (TAP TODO: reported, not fatal), by motor and
# the check's first word; see DEBUGNOTES "setup wizard".
KNOWN = {
    ("rx60_456", "j"): "idm: friction 10x the inertia's torque, j high",
    ("fanuc_b05_6000", "j"): "idm: friction 0.4 Nm vs 0.25 Nm inertia torque at the most it can accelerate, j 2.3x",
    ("sgmph_01a1", "j"): "idm: j 1.5e-6 with friction 0.03 Nm, j 2.4x",
    ("r2aa04003f", "id.l"): "idpmsm: L/R 55 us, a few rt cycles: L 2.7x",
    ("r2aa04003f", "f"): "idm: j 2.5e-6: friction -27 % (just outside 25 %)",
    ("sr60_4", "id.l"): "idpmsm: L/R 40 us: L +15 %",
}


def nameplate(r, l, psi, pp, j, imax):
    """What a builder reads off the plate: rms currents, torque, speed."""
    peak = imax / math.sqrt(2)
    rated = peak / 3.0
    kt = 1.5 * pp * psi * math.sqrt(2)  # Nm / A rms
    w_volt = UDC / math.sqrt(3) * 0.95 / (psi * pp)
    rpm = min(0.8 * w_volt, 6000 * 2 * math.pi / 60) * 60 / (2 * math.pi)
    return rated, peak, kt * rated, round(rpm, -1)


def case(i, motor):
    name, r, l, psi, pp, j, imax = motor
    lines, rev, prev, index = VARIANTS[i % len(VARIANTS)]
    t_peak = 1.5 * pp * psi * imax
    f = round(0.02 * t_peak, 4)
    d = float("%.3g" % (2.8e-5 * t_peak))
    rated_torque = nameplate(r, l, psi, pp, j, imax)[2]
    # every 3rd motor a holding brake (1.5 x rated torque); every 6th a
    # vertical axis (a hanging load of 30 % rated torque) whose datasheet
    # engage time is too short (10 ms, really 60 ms): the wizard has to find
    # brake_time itself
    brake = i % 3 == 2
    vertical = i % 6 == 5
    return dict(name=name, r=r, l=l, psi=psi, pp=pp, j=j, imax=imax, f=f, d=d, lines=lines, rev=rev,
                prev=prev, index=index, brake=brake, vertical=vertical,
                brake_torque=1.5 * rated_torque, brake_delay=0.06 if vertical else 0.04,
                engage_ms=10 if vertical else 40, load=0.3 * rated_torque if vertical else 0.0)


def sim_file(c):
    return "\n".join([
        "hv0.sim_r = %g" % c["r"], "hv0.sim_l = %g" % c["l"], "hv0.sim_psi = %g" % c["psi"],
        "hv0.sim_pp = %d" % c["pp"], "hv0.sim_j = %g" % c["j"], "hv0.sim_d = %g" % c["d"],
        "hv0.sim_f = %g" % c["f"], "hv0.sim_phase_rev = %d" % c["prev"],
        "hv0.sim_chop_on = 375", "hv0.sim_chop_off = 365",
        "enc_fb0.sim_res = %d" % (4 * c["lines"]), "enc_fb0.sim_rev = %d" % c["rev"],
        "enc_fb0.sim_index = %g" % c["index"],
        "hv0.sim_brake = %g" % (c["brake_torque"] if c["brake"] else 0.0),
        "hv0.sim_brake_delay = %g" % c["brake_delay"], ""])


def answers(c, wait):
    rated, peak, torque, rpm = nameplate(c["r"], c["l"], c["psi"], c["pp"], c["j"], c["imax"])
    return "\n".join([
        "safety.ok = y", "supply.kind = 230", "supply.chopper = y", "supply.chopper_on = 375",
        "supply.max_hv_power = 1000",
        "motor.rated_cur = %.3g" % rated, "motor.peak_cur = %.3g" % peak,
        "motor.rated_torque = %.3g" % torque, "motor.max_rpm = %g" % rpm,
        "motor.brake = %s" % ("y" if c["brake"] else "n"), "axis.vertical = %s" % ("y" if c["vertical"] else "n"),
        "brake.voltage = 24", "brake.current = 0.5", "brake.output = 0", "brake.engage_ms = %g" % c["engage_ms"],
        "fbcheck.secured = y",
        # the load goes on for the brake step (and stays); the hand can't turn a closed brake
        "brake.loaded = %s" % ("send hv0.sim_load = %.4g; wait 0.2" % -c["load"] if c["vertical"] else "done"),
        "brake.release_turn = send hv0.sim_turn = 1.5; wait %g" % wait,
        "brake.hold_turn = send hv0.sim_turn = 1; wait %g" % wait,
        "feedback.type = enc", "wiring.done = done",
        "fbcheck.turn = send hv0.sim_turn = 19; wait %g" % wait,
        "gate.free = y", "gate.turns = 2", "link.kind = sserial", "link.scale = 10", ""])


def read_state(path):
    st = {}
    if os.path.exists(path):
        for line in open(path):
            if "=" in line and not line.startswith("#"):
                k, v = line.split("=", 1)
                st[k.strip()] = v.strip()
    return st


def num(st, key):
    try:
        return float(st[key])
    except (KeyError, ValueError):
        return float("nan")


def run_case(c, speed, keep):
    d = tempfile.mkdtemp(prefix="wiz-%s-" % c["name"])
    open(os.path.join(d, "drive.conf"), "w").close()
    open(os.path.join(d, "sim.txt"), "w").write(sim_file(c))
    open(os.path.join(d, "answers.txt"), "w").write(answers(c, 4.0 / speed + 0.5))
    tty = os.path.join(d, "tty")
    sim = subprocess.Popen([STMBL_HOST, "--drive", "drive.conf", "--sim", "sim.txt", "--pty", tty,
                            "--speed", str(speed)], cwd=d, stdout=open(os.path.join(d, "sim.log"), "w"),
                           stderr=subprocess.STDOUT)
    for _ in range(50):
        if os.path.exists(tty):
            break
        time.sleep(0.1)
    t0 = time.time()
    try:
        p = subprocess.run([CLI, "-d", tty, "--wizard", "--session", "session.txt", "--answers", "answers.txt",
                            "--out", d, "--time-scale", str(speed)], cwd=d, stdin=subprocess.DEVNULL,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=1800)
        out, rc = p.stdout.decode(errors="replace"), p.returncode
    except subprocess.TimeoutExpired as e:
        out, rc = (e.stdout or b"").decode(errors="replace") + "\nTIMEOUT", -1
    finally:
        sim.terminate()
        sim.wait()
    open(os.path.join(d, "wizard.log"), "w").write(out)
    st = read_state(os.path.join(d, "session.txt"))
    if not keep and rc == 0:
        shutil.rmtree(d)
    return c, rc, out, st, d, time.time() - t0


def checks(c, rc, out, st):
    pp = c["pp"]
    res = []

    def chk(cond, what):
        res.append((bool(cond), what))

    def rel(key, truth, tol):
        v = num(st, key)
        chk(abs(v - truth) <= tol * abs(truth), "%s = %.4g, true %.4g (%.0f %%)" % (key, v, truth, 100 * tol))

    last = [l for l in out.splitlines() if l.startswith("ERROR")]
    chk(rc == 0, "wizard finished" + ("" if rc == 0 else ": " + (last[-1] if last else "rc %d" % rc)))
    chk(num(st, "feedback.counts") == 4 * c["lines"], "counts %s, true %d" % (st.get("feedback.counts"),
                                                                            4 * c["lines"]))
    chk(num(st, "feedback.rev") == c["rev"], "encoder direction %s, true %d" % (st.get("feedback.rev"), c["rev"]))
    chk(num(st, "id.out_rev") == c["prev"], "phase order %s, true %d" % (st.get("id.out_rev"), c["prev"]))
    chk(num(st, "id.pp") == pp, "pole pairs %s, true %d" % (st.get("id.pp"), pp))
    rel("id.r", c["r"], 0.03)
    rel("id.l", c["l"], 0.08)
    rel("id.psi", c["psi"], 0.05)
    off = num(st, "id.offset")
    e = (off - c["index"]) * pp
    e = math.remainder(e, 2 * math.pi) if not math.isnan(e) else float("nan")
    chk(abs(e) < 0.2, "commutation offset %.4f: %.3f rad electrical from the index" % (off, e))
    j = num(st, "mech.j")
    chk(abs(j - c["j"]) <= max(0.25 * c["j"], 2e-6), "j = %.3g, true %.3g (25 %% or 2e-6)" % (j, c["j"]))
    f = num(st, "mech.f")
    chk(abs(f - c["f"]) <= max(0.25 * c["f"], 2e-3), "f = %.3g, true %.3g (25 %%)" % (f, c["f"]))
    chk(num(st, "verify.step") < 0.05, "verify: position error after the steps %s rad" % st.get("verify.step"))
    if c["brake"]:
        chk(num(st, "brake.hold_drift") < 0.05, "brake holds with the drive off: moved %s rad" %
            st.get("brake.hold_drift"))
        chk(num(st, "brake.drop") < 0.01, "no sag at disable: %s rad" % st.get("brake.drop"))
        if c["vertical"]:
            chk(num(st, "brake.time") > c["brake_delay"], "brake_time %s s > the brake's %g s (found by the wizard)" %
                (st.get("brake.time"), c["brake_delay"]))
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-j", type=int, default=os.cpu_count() or 2)
    ap.add_argument("--speed", type=int, default=10)
    ap.add_argument("--only", default="")
    ap.add_argument("--keep", action="store_true", help="keep every case's directory")
    a = ap.parse_args()
    for exe in (STMBL_HOST, CLI):
        if not os.access(exe, os.X_OK):
            sys.exit("missing %s: make -C host; make -C servoterm-cli" % exe)
    only = [s for s in a.only.split(",") if s]
    cases = [case(i, m) for i, m in enumerate(MOTORS) if not only or m[0] in only]
    n = failed = 0
    with ThreadPoolExecutor(max_workers=a.j) as ex:
        for c, rc, out, st, d, dt in ex.map(lambda c: run_case(c, a.speed, a.keep), cases):
            print("# %s: %d lines, rev %d, phases %s, index %.2f%s (%.0f s)%s" % (
                c["name"], c["lines"], c["rev"], "swapped" if c["prev"] else "as wired", c["index"],
                ", vertical with brake" if c["vertical"] else ", brake" if c["brake"] else "", dt,
                "" if rc == 0 and not a.keep else ", in " + d))
            for good, what in checks(c, rc, out, st):
                n += 1
                known = KNOWN.get((c["name"], what.split()[0]))
                failed += not good and not known
                print("%s %d - %s: %s%s" % ("ok" if good else "not ok", n, c["name"], what,
                                            " # TODO " + known if known else ""))
            sys.stdout.flush()
    print("1..%d\n# %d/%d passed (known limits as TODO)" % (n, n - failed, n))
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
