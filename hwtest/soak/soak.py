#!/usr/bin/env python3
"""
Long-term reliability (soak) run for an STMBL drive, simulated or real.

Runs a G-code cycle over and over through LinuxCNC (motion), and between
cycles checks the drive over its USB console: HAL state and rt load,
uptime (reboots), faults, DC bus, temperatures, position error, and every
few cycles the config flash (read back and compared) and the console's
long-line handling. Anything unusual is an event; by default the first
event that is a failure switches the machine off and ends the run.

Started by run_soak.sh as LinuxCNC's DISPLAY program (motion), or on its own
with --mode console (no LinuxCNC: health and console checks only).
Results go to the run directory: summary.md, events.log, health.csv,
cycles.csv, console.log. `touch <run dir>/STOP` ends a run cleanly.
"""
import argparse
import csv
import datetime
import json
import os
import re
import signal
import sys
import time

import serial

# --------------------------------------------------------------------------
# drive console


class DriveConsole:
    """Persistent connection to the drive's USB console (or the sim's pty).

    Splits the byte stream into text lines and scope frames (0xFF + 8 bytes,
    0xFE reset), as servoterm-cli does, and keeps text the drive sends on its
    own (INFO / ERROR lines) for the caller to pick up.
    """

    def __init__(self, port, log):
        self.port = port
        self.log = log  # file for all console text
        self.ser = None
        self.scope_left = 0
        self.line = b""
        self.lines = []  # complete text lines not yet consumed
        self.unsolicited = []
        self.last_text = 0.0  # time of the last text byte (scope frames don't count)

    def open(self, wait=30.0):
        end = time.time() + wait
        while True:
            try:
                self.ser = serial.Serial(self.port, 115200, timeout=0.05)
                time.sleep(0.3)
                self.ser.write(b"\n")  # end a stale half line
                self._read(0.5)
                self.lines = []
                return True
            except (serial.SerialException, OSError):
                self.ser = None
                if time.time() > end:
                    return False
                time.sleep(1.0)

    def close(self):
        if self.ser:
            try:
                self.ser.close()
            except Exception:
                pass
        self.ser = None

    def _feed(self, data):
        for b in data:
            if self.scope_left:
                self.scope_left -= 1
            elif b == 0xFF:
                self.scope_left = 8
            elif b == 0xFE:
                pass
            elif b == 0x0A:
                self.last_text = time.time()
                text = self.line.decode(errors="replace").rstrip("\r")
                self.line = b""
                self.log.write("%s %s\n" % (now(), text))
                self.lines.append(text)
            else:
                self.line += bytes([b])
                self.last_text = time.time()

    def _read(self, quiet, total=5.0):
        """Read until no text arrived for `quiet` s (at most `total` s).

        The drive streams scope frames all the time, so only text counts."""
        end = time.time() + total
        self.last_text = time.time()
        while time.time() < end:
            data = self.ser.read(4096)
            if data:
                self._feed(data)
            if time.time() - self.last_text > quiet:
                break

    def command(self, cmd, quiet=0.25, total=5.0):
        """Send one command line, return the text lines that came back.

        Raises ConsoleError when the port is gone or nothing came back."""
        if not self.ser:
            raise ConsoleError("not connected")
        try:
            self.lines = []
            self.log.write("%s >> %s\n" % (now(), cmd))
            self.ser.write((cmd + "\n").encode())
            self._read(quiet, total)
        except (serial.SerialException, OSError) as e:
            self.close()
            raise ConsoleError("port: %s" % e)
        out = []
        for l in self.lines:
            if re.match(r"\s*(INFO|ERROR|WARNING)", l):
                self.unsolicited.append(l.strip())
            out.append(l)
        self.lines = []
        if not out:
            raise ConsoleError("no answer to %r" % cmd)
        return out

    def poll_unsolicited(self):
        if self.ser:
            try:
                self._read(0.05, 0.2)
            except (serial.SerialException, OSError):
                self.close()
        for l in self.lines:
            if re.match(r"\s*(INFO|ERROR|WARNING)", l):
                self.unsolicited.append(l.strip())
        self.lines = []
        u, self.unsolicited = self.unsolicited, []
        return u


class ConsoleError(Exception):
    pass


def pin_values(lines):
    """{'fault0.state': 1.0, ...} from 'name = value' / 'name <= src = value'."""
    out = {}
    for l in lines:
        m = re.match(r"\s*(\w+\d+\.\w+)(?: <= \S+)? = (-?[0-9.]+|nan|-?inf)", l)
        if m:
            try:
                out[m.group(1)] = float(m.group(2))
            except ValueError:
                pass
    return out


# --------------------------------------------------------------------------
# helpers


def now():
    return datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]


FAULTS = ["NO_ERROR", "CMD_ERROR", "MOT_FB_ERROR", "COM_FB_ERROR", "JOINT_FB_ERROR",
          "POS_ERROR", "SAT_ERROR", "MOT_TEMP_ERROR", "HV_CRC_ERROR", "HV_TIMEOUT_ERROR",
          "HV_TEMP_ERROR", "HV_VOLT_ERROR", "HV_FAULT_ERROR", "HV_CURRENT_OFFSET_FAULT",
          "HV_OVERCURRENT_RMS", "HV_OVERCURRENT_PEAK", "HV_OVERCURRENT_HW", "PHASING_ERROR"]


def fault_name(code):
    c = int(code)
    return FAULTS[c] if 0 <= c < len(FAULTS) else "fault %d" % c


class Run:
    def __init__(self, args):
        self.args = args
        self.dir = args.out
        os.makedirs(self.dir, exist_ok=True)
        self.t0 = time.time()
        self.console_log = open(os.path.join(self.dir, "console.log"), "a", buffering=1)
        self.events_f = open(os.path.join(self.dir, "events.log"), "a", buffering=1)
        self.health_f = open(os.path.join(self.dir, "health.csv"), "a", newline="", buffering=1)
        self.cycles_f = open(os.path.join(self.dir, "cycles.csv"), "a", newline="", buffering=1)
        self.health_w = csv.writer(self.health_f)
        self.cycles_w = csv.writer(self.cycles_f)
        self.health_w.writerow(["time", "elapsed_s", "cycle", "hal_state", "rt_us", "rt_max_us",
                                "frt_max_us", "uptime_s", "fault_state", "fault", "dc_volt",
                                "hv_temp", "mot_temp", "pos_error"])
        self.cycles_w.writerow(["time", "cycle", "duration_s", "ferror_max_mm", "ferror_rest_mm",
                                "vel_max", "result"])
        self.events = []  # (time, kind, failure, text)
        self.cycles = 0
        self.health_n = 0
        self.stats = {}
        self.baseline = {}
        self.stop_reason = None
        self.config_checks = [0, 0]  # ok, bad
        self.console_checks = [0, 0]
        self.recoveries = 0

    def event(self, kind, text, failure=True):
        e = (now(), kind, failure, text)
        self.events.append(e)
        self.events_f.write("%s %-22s %s %s\n" % (e[0], kind, "FAIL" if failure else "info", text))
        print("%s %s %s" % (e[0], kind, text), flush=True)
        return failure

    def stat(self, name, v):
        if v is None:
            return
        s = self.stats.setdefault(name, [v, v])
        s[0] = min(s[0], v)
        s[1] = max(s[1], v)

    def elapsed(self):
        return time.time() - self.t0

    def summary(self, final=False):
        a = self.args
        fails = [e for e in self.events if e[2]]
        kinds = {}
        for e in self.events:
            kinds[e[1]] = kinds.get(e[1], 0) + 1
        lines = ["# Soak run: %s" % os.path.basename(self.dir), ""]
        lines.append("| | |")
        lines.append("|---|---|")
        lines.append("| Target | %s, mode %s, console %s |" % (a.target, a.mode, a.port))
        lines.append("| Started | %s |" % datetime.datetime.fromtimestamp(self.t0).strftime("%Y-%m-%d %H:%M:%S"))
        lines.append("| Duration | %.2f h%s |" % (self.elapsed() / 3600, "" if final else " (running)"))
        lines.append("| Cycles | %d |" % self.cycles)
        lines.append("| Health checks | %d |" % self.health_n)
        lines.append("| Config read-back checks | %d ok, %d bad |" % tuple(self.config_checks))
        lines.append("| Console long-line checks | %d ok, %d bad |" % tuple(self.console_checks))
        lines.append("| Fault recoveries | %d |" % self.recoveries)
        if fails:
            result = "FAIL"
        elif not final:
            result = "running"
        elif self.stop_reason_is_failure():
            result = "STOPPED (%s)" % self.stop_reason
        else:
            result = "PASS"
        lines.append("| Result | **%s** |" % result)
        if self.stop_reason:
            lines.append("| Stopped by | %s |" % self.stop_reason)
        lines.append("")
        shown = {k: v for k, v in self.baseline.items() if not k.startswith("_")}
        if shown:
            lines.append("Baseline: " + ", ".join("%s %s" % (k, v) for k, v in shown.items()))
            lines.append("")
        if self.stats:
            lines.append("| Value | min | max |")
            lines.append("|---|---|---|")
            for k in sorted(self.stats):
                lines.append("| %s | %.4g | %.4g |" % (k, self.stats[k][0], self.stats[k][1]))
            lines.append("")
        lines.append("Events: " + (", ".join("%s x%d" % (k, n) for k, n in sorted(kinds.items())) or "none"))
        lines.append("")
        for e in self.events[-30:]:
            lines.append("- %s **%s** %s%s" % (e[0], e[1], "" if e[2] else "(info) ", e[3]))
        open(os.path.join(self.dir, "summary.md"), "w").write("\n".join(lines) + "\n")
        json.dump({"cycles": self.cycles, "elapsed_s": self.elapsed(), "events": self.events,
                   "stats": self.stats, "stop_reason": self.stop_reason, "result": result},
                  open(os.path.join(self.dir, "summary.json"), "w"), indent=1)

    def stop_reason_is_failure(self):
        return self.stop_reason not in (None, "time limit", "cycle limit", "STOP file", "signal")


# --------------------------------------------------------------------------
# checks


def health_check(run, con, cycle):
    """One round of drive health readings. Returns True if all is well."""
    a = run.args
    ok = True
    try:
        hal = con.command("hal", quiet=0.3)
        up = con.command("uptime")
        pins = pin_values(con.command("fault0.state") + con.command("fault0.fault") +
                          con.command("hv0.dc_volt") + con.command("hv0.hv_temp") +
                          con.command("iit0.temp") + con.command("pid0.pos_error"))
    except ConsoleError as e:
        run.event("console_no_answer", str(e))
        return False
    run.health_n += 1
    text = "\n".join(hal)
    m = re.search(r"HAL state:\s*(\w+)", text)
    hal_state = m.group(1) if m else "?"
    m = re.search(r"rt time:\s*([0-9.]+)\(max ([0-9.]+)\)us/([0-9.]+)us", text)
    rt, rt_max, rt_period = (float(m.group(1)), float(m.group(2)), float(m.group(3))) if m else (None, None, None)
    m = re.search(r"frt time:\s*[0-9.]+\(max ([0-9.]+)\)us", text)
    frt_max = float(m.group(1)) if m else None
    m = re.search(r"up (\d+):(\d+):(\d+)", "\n".join(up))
    uptime = int(m.group(1)) * 3600 + int(m.group(2)) * 60 + int(m.group(3)) if m else None

    fs, fc = pins.get("fault0.state"), pins.get("fault0.fault")
    dc, hvt, mt, pe = (pins.get("hv0.dc_volt"), pins.get("hv0.hv_temp"),
                       pins.get("iit0.temp"), pins.get("pid0.pos_error"))
    run.health_w.writerow([now(), "%.1f" % run.elapsed(), cycle, hal_state, rt, rt_max, frt_max, uptime,
                           fs, fc, dc, hvt, mt, pe])
    run.stat("rt_max_us", rt_max)
    run.stat("dc_volt", dc)
    run.stat("hv_temp", hvt)
    run.stat("mot_temp", mt)

    if hal_state != "HAL_OK2":
        ok = run.event("hal_state", "%s\n%s" % (hal_state, text))
    if rt_max is not None and rt_period and rt_max > a.rt_max_frac * rt_period:
        if run.baseline.get("rt_warned") is None:
            run.baseline["rt_warned"] = rt_max
            run.event("rt_load_high", "rt max %.1f us of %.0f us" % (rt_max, rt_period), failure=False)
    if uptime is not None:
        last = run.baseline.get("_uptime")
        if last is not None and uptime + 2 < last:
            ok = run.event("drive_reboot", "uptime %d s after %d s" % (uptime, last))
        run.baseline["_uptime"] = uptime
    if fs is not None and fs == 3.0 and not (a.mode == "console" and fc == 1.0):
        ok = run.event("drive_fault", fault_name(fc))
    if dc is not None:
        base = run.baseline.setdefault("dc_volt", round(dc, 1))
        if abs(dc - base) > a.dc_tol * base:
            ok = run.event("dc_volt", "%.1f V, baseline %.1f V (tolerance %.0f%%)" % (dc, base, a.dc_tol * 100))
    if hvt is not None and hvt > a.max_hv_temp:
        ok = run.event("hv_temp", "%.1f C > %.1f C" % (hvt, a.max_hv_temp))
    if mt is not None and mt > a.max_mot_temp:
        ok = run.event("mot_temp", "%.1f C > %.1f C" % (mt, a.max_mot_temp))
    return ok


def read_config(con):
    lines = con.command("showconf", quiet=0.6, total=10.0)
    return "\n".join(lines).strip()


def config_check(run, con):
    try:
        conf = read_config(con)
    except ConsoleError as e:
        run.event("console_no_answer", str(e))
        return False
    base = run.baseline.get("_config")
    if base is None:
        run.baseline["_config"] = conf
        open(os.path.join(run.dir, "config-baseline.conf"), "w").write(conf + "\n")
        return True
    if conf == base:
        run.config_checks[0] += 1
        return True
    run.config_checks[1] += 1
    n = len([f for f in os.listdir(run.dir) if f.startswith("config-diff")])
    open(os.path.join(run.dir, "config-diff-%d.conf" % n), "w").write(conf + "\n")
    # one more read: a garbled transfer vs a changed flash
    try:
        again = read_config(con)
    except ConsoleError:
        again = None
    if again == base:
        return run.event("config_readback", "read %d differed, re-read matches (transfer)" % n, failure=run.args.strict_console)
    return run.event("config_changed", "config read %d differs twice from the baseline" % n)


def console_check(run, con):
    """A line too long for the drive's 64-byte buffer, then a normal command."""
    try:
        out = con.command("conf0.r " + "x" * 80, quiet=0.3)
        good = con.command("conf0.r")
    except ConsoleError as e:
        run.event("console_no_answer", str(e))
        return False
    dropped = any("too long" in l for l in out)
    answered = any(re.match(r"\s*conf0\.r = ", l) for l in good)
    if dropped and answered:
        run.console_checks[0] += 1
        return True
    run.console_checks[1] += 1
    return run.event("console_long_line", "dropped msg %s, next command answered %s" % (dropped, answered))


# --------------------------------------------------------------------------
# LinuxCNC


class Machine:
    def __init__(self, run):
        import linuxcnc
        self.lc = linuxcnc
        self.s = linuxcnc.stat()
        self.c = linuxcnc.command()
        self.e = linuxcnc.error_channel()
        self.run = run

    def errors(self):
        out = []
        while True:
            m = self.e.poll()
            if not m:
                return out
            out.append(m[1])

    def on(self):
        self.c.state(self.lc.STATE_ESTOP_RESET)
        self.c.wait_complete()
        self.c.state(self.lc.STATE_ON)
        self.c.wait_complete()
        time.sleep(1.0)
        self.s.poll()
        return self.s.task_state == self.lc.STATE_ON

    def home(self):
        self.s.poll()
        if all(self.s.homed[i] for i in range(self.s.joints)):
            return True
        self.c.mode(self.lc.MODE_MANUAL)
        self.c.wait_complete()
        self.c.home(-1)
        self.c.wait_complete()
        end = time.time() + 30
        while time.time() < end:
            self.s.poll()
            if all(self.s.homed[i] for i in range(self.s.joints)):
                return True
            time.sleep(0.1)
        return False

    def off(self):
        try:
            self.c.abort()
            self.c.wait_complete()
            self.c.state(self.lc.STATE_OFF)
            self.c.wait_complete()
        except Exception:
            pass

    def cycle(self, program, timeout):
        """Run the program once. Returns (ok, ferror_max, vel_max, msgs)."""
        self.c.mode(self.lc.MODE_AUTO)
        self.c.wait_complete()
        self.c.program_open(program)
        self.c.wait_complete()
        self.c.auto(self.lc.AUTO_RUN, 0)
        fmax = vmax = 0.0
        t0 = time.time()
        started = False
        msgs = []
        while time.time() - t0 < timeout:
            self.s.poll()
            j = self.s.joint[0]
            fmax = max(fmax, abs(j["ferror_current"]))
            vmax = max(vmax, abs(j["velocity"]))
            msgs += self.errors()
            if self.s.task_state != self.lc.STATE_ON:
                return False, fmax, vmax, msgs + ["machine went off"]
            if self.s.interp_state != self.lc.INTERP_IDLE:
                started = True
            elif started or time.time() - t0 > 2.0:
                return True, fmax, vmax, msgs
            time.sleep(0.02)
        return False, fmax, vmax, msgs + ["cycle timeout"]

    def ferror_rest(self):
        self.s.poll()
        return abs(self.s.joint[0]["ferror_current"])


# --------------------------------------------------------------------------
# main loops


def should_stop(run):
    a = run.args
    if os.path.exists(os.path.join(run.dir, "STOP")):
        return "STOP file"
    if a.hours and run.elapsed() > a.hours * 3600:
        return "time limit"
    if a.cycles and run.cycles >= a.cycles:
        return "cycle limit"
    if STOP_SIGNAL[0]:
        return "signal"
    return None


STOP_SIGNAL = [False]


def unsolicited(run, con):
    failed = False
    for l in con.poll_unsolicited():
        if l.startswith("ERROR"):
            failed |= run.event("drive_message", l)
        else:
            run.event("drive_message", l, failure=False)
    return failed


def checks_between(run, con, cycle):
    a = run.args
    ok = health_check(run, con, cycle)
    if a.config_every and cycle % a.config_every == 0:
        ok &= config_check(run, con)
    if a.console_every and cycle % a.console_every == 0:
        ok &= console_check(run, con)
    ok &= not unsolicited(run, con)
    return ok


def reconnect(run, con):
    con.close()
    if con.open(wait=run.args.reconnect_wait):
        run.event("console_reconnected", con.port, failure=False)
        return True
    return False


def run_console(run, con):
    """No LinuxCNC: health and console checks on an idle drive."""
    a = run.args
    cycle = 0
    config_check(run, con)  # baseline
    while not run.stop_reason:
        cycle += 1
        run.cycles = cycle
        ok = checks_between(run, con, cycle)
        if not con.ser and not reconnect(run, con):
            run.stop_reason = "console lost"
            break
        if not ok and a.on_fault == "stop":
            run.stop_reason = "failure"
            break
        run.summary()
        end = time.time() + a.interval
        while time.time() < end and not run.stop_reason:
            run.stop_reason = should_stop(run)
            time.sleep(0.5)
        run.stop_reason = run.stop_reason or should_stop(run)


def run_motion(run, con):
    a = run.args
    m = Machine(run)
    for msg in m.errors():
        run.event("lcnc_message", msg, failure=False)
    if not m.on():
        run.stop_reason = "machine on failed"
        run.event("machine_on", "LinuxCNC did not reach STATE_ON: %s" % m.errors())
        return
    time.sleep(1.0)
    unsolicited(run, con)
    if not m.home():
        run.stop_reason = "homing failed"
        run.event("homing", "not homed after 30 s")
        m.off()
        return
    config_check(run, con)  # baseline
    health_check(run, con, 0)
    while not run.stop_reason:
        run.stop_reason = should_stop(run)
        if run.stop_reason:
            break
        cycle = run.cycles + 1
        t0 = time.time()
        ok, fmax, vmax, msgs = m.cycle(a.program, a.cycle_timeout)
        time.sleep(a.settle)
        frest = m.ferror_rest()
        run.cycles = cycle
        run.stat("ferror_max_mm", fmax)
        run.stat("ferror_rest_mm", frest)
        run.stat("vel_max", vmax)
        for msg in msgs:
            if "Unexpected realtime delay" in msg:
                # the Pi's servo thread was late once; LinuxCNC carries on.
                # Recorded (it shows up as a step in ferror), not a drive fault.
                run.event("host_rt_delay", msg.splitlines()[0], failure=False)
            else:
                ok &= not run.event("lcnc_message", msg)
        if fmax > a.max_ferror:
            ok &= not run.event("ferror", "%.4f mm > %.4f mm in cycle %d" % (fmax, a.max_ferror, cycle))
        if frest > a.max_ferror_rest:
            ok &= not run.event("ferror_rest", "%.4f mm > %.4f mm after cycle %d" % (frest, a.max_ferror_rest, cycle))
        ok &= checks_between(run, con, cycle)
        run.cycles_w.writerow([now(), cycle, "%.2f" % (time.time() - t0), "%.4f" % fmax, "%.4f" % frest,
                               "%.1f" % vmax, "ok" if ok else "FAIL"])
        if not con.ser and not reconnect(run, con):
            m.off()
            run.stop_reason = "console lost"
            break
        if not ok:
            m.off()
            if a.on_fault == "stop" or run.recoveries >= a.max_recoveries:
                run.stop_reason = "failure" if a.on_fault == "stop" else "too many recoveries"
                break
            run.recoveries += 1
            time.sleep(3.0)
            if not (m.on() and m.home()):
                run.stop_reason = "recovery failed"
                run.event("recovery", "machine on / home failed")
                break
            run.event("recovery", "machine back on (%d)" % run.recoveries, failure=False)
        run.summary()
    m.off()


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--target", default="hw", help="label for the report: sim or hw")
    p.add_argument("--mode", choices=["motion", "console"], default="motion")
    p.add_argument("--port", default="/dev/ttyACM0", help="drive console (sim: the pty link)")
    p.add_argument("--out", required=True, help="run directory")
    p.add_argument("--program", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "soak.ngc"))
    p.add_argument("--hours", type=float, default=0, help="stop after this long (0: no limit)")
    p.add_argument("--cycles", type=int, default=0, help="stop after this many cycles (0: no limit)")
    p.add_argument("--interval", type=float, default=10.0, help="console mode: seconds between checks")
    p.add_argument("--config-every", type=int, default=10, help="config read-back every N cycles (0: off)")
    p.add_argument("--console-every", type=int, default=0,
                   help="long-line console check every N cycles (0: off; needs firmware >= v0.9.18-243)")
    p.add_argument("--on-fault", choices=["stop", "continue"], default="stop")
    p.add_argument("--max-recoveries", type=int, default=10)
    p.add_argument("--max-ferror", type=float, default=0.5, help="mm, during a cycle")
    p.add_argument("--max-ferror-rest", type=float, default=0.02, help="mm, at rest after a cycle")
    p.add_argument("--dc-tol", type=float, default=0.05, help="DC bus tolerance against its first reading")
    p.add_argument("--max-hv-temp", type=float, default=70.0)
    p.add_argument("--max-mot-temp", type=float, default=80.0)
    p.add_argument("--rt-max-frac", type=float, default=0.9, help="note rt max above this part of the period")
    p.add_argument("--settle", type=float, default=0.5, help="s at rest before the rest error is read")
    p.add_argument("--cycle-timeout", type=float, default=300.0)
    p.add_argument("--reconnect-wait", type=float, default=30.0)
    p.add_argument("--strict-console", action="store_true",
                   help="treat a garbled config transfer (re-read fine) as a failure")
    a = p.parse_args(os.environ.get("SOAK_ARGS", "").split() if len(sys.argv) <= 1 else None)

    signal.signal(signal.SIGTERM, lambda *_: STOP_SIGNAL.__setitem__(0, True))
    signal.signal(signal.SIGINT, lambda *_: STOP_SIGNAL.__setitem__(0, True))

    run = Run(a)
    open(os.path.join(run.dir, "args.json"), "w").write(json.dumps(vars(a), indent=1))
    con = DriveConsole(a.port, run.console_log)
    if not con.open(wait=a.reconnect_wait):
        run.stop_reason = "console not found"
        run.event("console_open", "cannot open %s" % a.port)
        run.summary(final=True)
        return 2
    try:
        about = con.command("about", quiet=0.4)
        fw = next((l.split()[2] for l in about if l.startswith("STMBL v")), None)
        if fw is None:  # the simulated drive
            fw = next((l.strip() for l in about if l.startswith("STMBL")), "?")
        run.baseline["firmware"] = fw
    except ConsoleError as e:
        run.event("console_no_answer", str(e))
    run.event("start", "target %s mode %s firmware %s" % (a.target, a.mode, run.baseline.get("firmware")),
              failure=False)
    try:
        if a.mode == "console":
            run_console(run, con)
        else:
            run_motion(run, con)
    except Exception as e:  # keep the report
        run.stop_reason = "runner error"
        run.event("runner_error", repr(e))
        raise
    finally:
        run.event("end", run.stop_reason or "?", failure=False)
        run.summary(final=True)
        con.close()
    return 1 if [e for e in run.events if e[2]] else 0


if __name__ == "__main__":
    sys.exit(main())
