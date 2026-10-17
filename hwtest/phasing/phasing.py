#!/usr/bin/env python3
"""
Autophasing suite for a real STMBL drive (hm2 smart-serial, no LinuxCNC).

For each case: move the rotor to a position, disable, reset the drive over
USB (a fresh boot: the incremental encoder counts from 0 again, so the
phasing starts from a new angle error), start a fresh HAL session, enable,
and record the phasing: the rotor's swing, how long it took, the result
(fault, fb_switch0.phase_fail, the probe current and pulse length). Then
check the angle is right: a move out and back with velocity feedforward
must stay within --max-error and fault-free, and the holding torque at
rest is recorded.

Cases per position: free; brake held closed while phasing (io0.out0 = 0),
released for the check move; and with --align the align method
(phase_mode 0) for comparison.

Results: runs/<date-time>/summary.md, cases.csv, phasing.log.
The motor moves (by --step mm per position, and the check move); the brake
is held closed only during the phasing, by a RAM setting a reset clears.
"""
import argparse
import csv
import datetime
import json
import os
import re
import signal
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
CLI = os.path.join(REPO, "servoterm-cli", "servoterm-cli")
HAL_FILE = os.path.join(HERE, "phasing.hal")
WORKER = os.path.join(HERE, "phasing_worker.py")


def now():
    return datetime.datetime.now().strftime("%H:%M:%S")


class Suite:
    def __init__(self, a):
        self.a = a
        self.dir = os.path.join(HERE, "runs", datetime.datetime.now().strftime("%Y%m%d-%H%M%S"))
        os.makedirs(self.dir)
        self.log = open(os.path.join(self.dir, "phasing.log"), "w")
        self.rows = []
        self.hal = None

    def note(self, text):
        self.log.write("%s %s\n" % (now(), text))
        self.log.flush()

    def cli(self, *cmds, quiet=0.3, timeout=30):
        args = [CLI, "-d", self.a.usb, "-t", str(quiet)] + sum([["-c", c] for c in cmds], [])
        try:
            r = subprocess.run(args, capture_output=True, text=True, timeout=timeout)
            out = r.stdout + r.stderr
        except subprocess.TimeoutExpired:
            out = "timeout"
        self.note("$ %s\n%s" % (" ; ".join(cmds), out.strip()))
        return out

    def pins(self, *names):
        out = self.cli(*names)
        vals = {}
        for l in out.splitlines():
            m = re.match(r"\s*(\w+\d+\.\w+)(?: <= \S+)? = (-?[0-9.]+)", l)
            if m:
                vals[m.group(1)] = float(m.group(2))
        return vals

    # --- the drive and the HAL session

    def reset_drive(self):
        self.cli("reset", quiet=0.2, timeout=10)
        end = time.time() + 5
        while time.time() < end and os.path.exists(self.a.usb):  # gone
            time.sleep(0.1)
        end = time.time() + 30
        while time.time() < end and not os.path.exists(self.a.usb):
            time.sleep(0.2)
        time.sleep(3.0)  # booted
        return os.path.exists(self.a.usb)

    def hal_start(self):
        self.hal_stop()
        self.hal = subprocess.Popen(["halrun", "-f", HAL_FILE], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                    start_new_session=True)
        end = time.time() + 20
        while time.time() < end:
            if subprocess.run(["halcmd", "getp", self.a.prefix + "pos_fb"], capture_output=True).returncode == 0:
                time.sleep(1.0)
                return True
            time.sleep(0.3)
        return False

    def hal_stop(self):
        subprocess.run(["halrun", "-U"], capture_output=True)
        if self.hal and self.hal.poll() is None:
            try:
                os.killpg(self.hal.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                self.hal.wait(5)
            except subprocess.TimeoutExpired:
                os.killpg(self.hal.pid, signal.SIGKILL)
        self.hal = None
        time.sleep(1.0)

    def worker(self, *args, timeout=120):
        r = subprocess.run([sys.executable, WORKER, self.a.prefix] + [str(x) for x in args],
                           capture_output=True, text=True, timeout=timeout)
        self.note("worker %s -> %s %s" % (" ".join(str(x) for x in args), r.stdout.strip(), r.stderr.strip()[-300:]))
        try:
            return json.loads(r.stdout.strip().splitlines()[-1])
        except (ValueError, IndexError):
            return {"error": (r.stderr or r.stdout).strip()[-200:]}

    # --- one case

    def case(self, pos, kind):
        """kind: probe, brake (probe with the brake held), align"""
        a = self.a
        row = {"pos_mm": round(pos, 3), "case": kind}
        if not self.reset_drive():
            row.update(result="FAIL", why="drive did not come back after reset")
            return row
        setup = ["fault0.max_pos_error = %g" % a.max_pos_error]
        setup.append("fb_switch0.phase_mode = %d" % (0 if kind == "align" else 1))
        if kind == "brake":
            setup.append("io0.out0 = 0")  # brake closed, unlinked from fault0.mot_brake
        self.cli(*setup)
        if not self.hal_start():
            row.update(result="FAIL", why="HAL session did not come up")
            return row
        # phase: enable and watch until the drive is enabled or faults
        r = self.worker("phase", a.phase_timeout)
        st = self.pins("fault0.state", "fault0.fault", "fb_switch0.phase_fail", "fb_switch0.phase_amp",
                       "fb_switch0.phase_len", "fb_switch0.current_com_pos")
        row.update(swing_mm=r.get("swing"), phase_s=r.get("time"), state=st.get("fault0.state"),
                   fault=st.get("fault0.fault"), phase_fail=st.get("fb_switch0.phase_fail"),
                   amp=st.get("fb_switch0.phase_amp"), len_s=st.get("fb_switch0.phase_len"),
                   com_pos=st.get("fb_switch0.current_com_pos"))
        if "error" in r or st.get("fault0.state") != 1:
            ok = kind == "brake" and st.get("fault0.fault") == 17  # refusing is fine with the brake on
            row.update(result="ok (refused)" if ok else "FAIL",
                       why=r.get("error") or "not enabled: fault %s, phase_fail %s" % (st.get("fault0.fault"), st.get("fb_switch0.phase_fail")))
            self.worker("disable")
            self.hal_stop()
            return row
        if kind == "brake":
            self.cli("io0.out0 = 1")  # release for the check move
            time.sleep(0.5)
        # angle check: hold torque at rest, then a move out and back
        hold = self.pins("pid0.torque_cmd").get("pid0.torque_cmd")
        m = self.worker("move", a.check_move, a.check_vel)
        st2 = self.pins("fault0.state", "fault0.fault")
        row.update(hold_nm=hold, move_err_mm=m.get("max_err"), after_fault=st2.get("fault0.fault"))
        good = "error" not in m and st2.get("fault0.state") == 1 and (m.get("max_err") or 1e9) <= a.max_error
        if good and r.get("swing") is not None and kind != "align" and r["swing"] > a.max_swing:
            good = False
            row["why"] = "swing %.3f mm > %.3f" % (r["swing"], a.max_swing)
        row["result"] = "ok" if good else "FAIL"
        if not good and "why" not in row:
            row["why"] = m.get("error") or "after the move: state %s fault %s, error %s mm" % (
                st2.get("fault0.state"), st2.get("fault0.fault"), m.get("max_err"))
        # move on to the next position, then disable
        self.worker("move", a.step, a.check_vel, "stay")
        self.worker("disable")
        self.hal_stop()
        return row

    def run(self):
        a = self.a
        kinds = ["probe"] + (["brake"] if not a.no_brake else []) + (["align"] if a.align else [])
        pos = 0.0
        for k in range(a.positions):
            for kind in kinds:
                row = self.case(pos, kind)
                self.rows.append(row)
                print("%s pos %+6.2f mm %-6s %-13s swing %s mm, phasing %s s, fault %s/%s, move error %s mm %s" % (
                    now(), pos, kind, row.get("result"), fmt(row.get("swing_mm")), fmt(row.get("phase_s"), 1),
                    fmt(row.get("fault"), 0), fmt(row.get("phase_fail"), 0), fmt(row.get("move_err_mm")),
                    ("(" + row["why"] + ")") if row.get("why") else ""), flush=True)
                self.report()
            pos += a.step  # the last case moved the rotor on
        self.cli("reset", quiet=0.2)  # defaults back (phase_mode, brake link, max_pos_error)
        return self.report(final=True)

    def report(self, final=False):
        keys = ["pos_mm", "case", "result", "swing_mm", "phase_s", "state", "fault", "phase_fail", "amp", "len_s",
                "com_pos", "hold_nm", "move_err_mm", "after_fault", "why"]
        with open(os.path.join(self.dir, "cases.csv"), "w", newline="") as f:
            w = csv.DictWriter(f, keys, extrasaction="ignore")
            w.writeheader()
            w.writerows(self.rows)
        fails = [r for r in self.rows if r.get("result") == "FAIL"]
        md = ["# Phasing suite: %s" % os.path.basename(self.dir), "",
              "Result: **%s** (%d cases, %d failed)%s" % ("PASS" if not fails and self.rows else "FAIL", len(self.rows),
                                                          len(fails), "" if final else " (running)"), ""]
        for kind in ("probe", "brake", "align"):
            rs = [r for r in self.rows if r["case"] == kind and r.get("swing_mm") is not None]
            if rs:
                sw = [r["swing_mm"] for r in rs]
                ts = [r["phase_s"] for r in rs if r.get("phase_s") is not None]
                me = [r["move_err_mm"] for r in rs if r.get("move_err_mm") is not None]
                md.append("- **%s**: swing %.3f..%.3f mm, phasing %.1f..%.1f s, check move error up to %s mm" % (
                    kind, min(sw), max(sw), min(ts or [0]), max(ts or [0]), fmt(max(me) if me else None)))
        md += ["", "| pos mm | case | result | swing mm | phasing s | fault | phase_fail | pulse A / s | hold Nm | move error mm | note |",
               "|---|---|---|---|---|---|---|---|---|---|---|"]
        for r in self.rows:
            md.append("| %s | %s | %s | %s | %s | %s | %s | %s / %s | %s | %s | %s |" % (
                r["pos_mm"], r["case"], r.get("result"), fmt(r.get("swing_mm")), fmt(r.get("phase_s"), 1),
                fmt(r.get("fault"), 0), fmt(r.get("phase_fail"), 0), fmt(r.get("amp"), 2), fmt(r.get("len_s")),
                fmt(r.get("hold_nm"), 4), fmt(r.get("move_err_mm")), r.get("why", "")))
        open(os.path.join(self.dir, "summary.md"), "w").write("\n".join(md) + "\n")
        return 1 if fails or not self.rows else 0


def fmt(v, d=3):
    return "-" if v is None else ("%.*f" % (d, v))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--usb", default="/dev/ttyACM0")
    p.add_argument("--prefix", default="hm2_7c81.0.stbl.0.0.", help="hm2 pin prefix of the drive")
    p.add_argument("--positions", type=int, default=12)
    p.add_argument("--step", type=float, default=0.55,
                   help="mm between positions (0.55: not a fraction of the 2 mm electrical period, so they spread)")
    p.add_argument("--align", action="store_true", help="also the align method at each position")
    p.add_argument("--no-brake", action="store_true", help="skip the brake-closed cases")
    p.add_argument("--max-swing", type=float, default=0.5, help="mm, probe phasing")
    p.add_argument("--max-error", type=float, default=0.3, help="mm, during the check move")
    p.add_argument("--check-move", type=float, default=5.0, help="mm out and back")
    p.add_argument("--check-vel", type=float, default=20.0, help="mm/s")
    p.add_argument("--max-pos-error", type=float, default=1.0, help="drive's fault0.max_pos_error during the suite [rad]")
    p.add_argument("--phase-timeout", type=float, default=20.0, help="s")
    a = p.parse_args()
    if subprocess.run(["pgrep", "-x", "rtapi_app"], capture_output=True).returncode == 0 or \
       subprocess.run(["pgrep", "-x", "milltask"], capture_output=True).returncode == 0:
        print("LinuxCNC or a HAL session is running; stop it first (halrun -U)")
        return 2
    s = Suite(a)
    print("phasing suite: %s" % s.dir)
    try:
        rc = s.run()
    finally:
        s.hal_stop()
    print("summary: %s/summary.md" % s.dir)
    return rc


if __name__ == "__main__":
    sys.exit(main())
