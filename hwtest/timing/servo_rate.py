#!/usr/bin/env python3
"""How fast can the servo thread run on this controller? Measures LinuxCNC's
servo thread and hostmot2's read and write times at several periods, on any
hostmot2 front end: the Pi 4 + 7C81 over SPI, a Pi 5 / CM4 with a PCIe Mesa
card (hm2_pci), a Zynq board with hostmot2 in its FPGA (hm2_soc_ol).
docs/servo-timing.md explains the budget.

    servo_rate.py [--driver NAME --params "..."] [--config "..."] [--periods US,US..]
                  [--seconds S] [--no-motion] [--exchange-us US] [--out FILE]

    # the bench's SPI card (hwtest/burnin/stmbl.ini [HOSTMOT2]):
    servo_rate.py
    # a PCIe card on a Pi 5 / CM4:
    servo_rate.py --driver hm2_pci --config "num_encoders=0 num_pwmgens=0 num_stepgens=0 sserial_port_0=00000000"
    # a Zynq (bitstream loaded first, e.g. webui/mill/server/fpga_load.py):
    servo_rate.py --driver hm2_soc_ol --config "num_encoders=0 sserial_port_0=00000000"

Per period: a HAL session like LinuxCNC's (motmod makes the servo thread:
board read, motion, board write; --no-motion: a bare thread), the drives on
smart-serial exchanging packets but NOT enabled (no motor moves; their
enable pins stay 0). After S seconds (60): the thread's and the read/write
functions' average and largest time [us], the smart-serial fault/error
counters' change, rtapi's "unexpected realtime delay" messages. Verdict:
the largest servo time plus the smart-serial exchange (--exchange-us, 150)
must fit in the period: "fits" under 80 %, "tight" under 100 %, else "no".

Run as the machine's user, LinuxCNC stopped (it runs halrun -U first).
Writes a markdown table to --out (servo_rate.md) with the platform's facts
(CPU, kernel, isolcpus, governor).
"""
import argparse
import os
import platform
import re
import signal
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))
import hm2_driver  # noqa: E402


def sh(*cmd, timeout=10):
    return subprocess.run(list(cmd), capture_output=True, text=True, timeout=timeout)


def getp(name):
    r = sh("halcmd", "getp", name)
    try:
        return int(float(r.stdout.strip()))
    except ValueError:
        return None


def facts():
    def read(p):
        try:
            return open(p).read().strip()
        except OSError:
            return "?"
    model = read("/proc/device-tree/model").replace("\x00", "")
    if model == "?":
        m = re.search(r"model name\s*:\s*(.*)", read("/proc/cpuinfo"))
        model = m.group(1) if m else platform.machine()
    cmd = read("/proc/cmdline")
    iso = " ".join(w for w in cmd.split() if w.split("=")[0] in ("isolcpus", "nohz_full", "rcu_nocbs", "irqaffinity"))
    gov = read("/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor" % (os.cpu_count() - 1))
    rt = "PREEMPT_RT" if "PREEMPT_RT" in read("/sys/kernel/realtime") + platform.version() else "not RT"
    return "%s; %d CPUs; kernel %s (%s); %s; governor %s; RTAPI_CPU_NUMBER %s" % (
        model, os.cpu_count(), platform.release(), rt, iso or "no isolcpus", gov, os.environ.get("RTAPI_CPU_NUMBER", "-"))


def counters():
    """smart-serial fault / error counters: {pin: value}"""
    out = sh("halcmd", "show", "pin").stdout + sh("halcmd", "show", "param").stdout
    vals = {}
    for l in out.splitlines():
        f = l.split()
        for i, w in enumerate(f):
            if re.search(r"(fault|error)[-_.]?count|\.fault-count|\.error-count", w) and i >= 1:
                try:
                    vals[w] = float(f[i - 1] if f[i - 1] not in ("IN", "OUT", "RW", "RO", "I/O") else f[i - 2])
                except (ValueError, IndexError):
                    pass
    return vals


def board_prefix():
    for l in sh("halcmd", "show", "funct").stdout.splitlines():
        m = re.search(r"(hm2_\w+\.\d+)\.read\b", l)
        if m:
            return m.group(1)
    return None


def run_period(a, period_us, workdir):
    sh("halrun", "-U", timeout=30)
    ns = int(period_us * 1000)
    params = (a.params + (' config="%s"' % a.config if a.config else "")).strip()
    hal = os.path.join(workdir, "rate.hal")
    lines = []
    if a.motion:
        lines += ["loadrt trivkins", "loadrt tpmod", "loadrt homemod",  # as the linuxcnc script loads them (2.9)
                  "loadrt motmod servo_period_nsec=%d num_joints=5" % ns]
    else:
        lines += ["loadrt threads name1=servo-thread period1=%d" % ns]
    if a.driver != "none":
        lines += ["loadrt hostmot2", "loadrt %s %s" % (a.driver, params)]
    lines += ["loadusr -w sleep 100000"]
    open(hal, "w").write("\n".join(lines) + "\n")
    log = open(os.path.join(workdir, "halrun-%d.log" % period_us), "w")
    p = subprocess.Popen(["halrun", "-f", hal], stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
    res = {"period": period_us}
    try:
        end = time.time() + 30
        while time.time() < end and sh("halcmd", "show", "thread").stdout.find("servo-thread") < 0:
            time.sleep(0.3)
        # the thread comes with motmod, before the board's driver has found its
        # smart-serial remotes (seconds): wait for the board's functions too
        board = None
        while a.driver != "none" and time.time() < end and not board:
            board = board_prefix()
            if not board:
                time.sleep(0.3)
        if a.driver != "none" and not board:
            res["error"] = "no hostmot2 board (see %s)" % log.name
            return res
        if a.motion:
            sh("halcmd", "addf", "motion-command-handler", "servo-thread")
            sh("halcmd", "addf", "motion-controller", "servo-thread")
        if board:
            sh("halcmd", "addf", board + ".read", "servo-thread", "1")
            sh("halcmd", "addf", board + ".write", "servo-thread")
        sh("halcmd", "start")
        time.sleep(3.0)  # past start-up
        parts = {"servo": "servo-thread"}
        if board:
            parts.update(read=board + ".read", write=board + ".write")
        for n in parts.values():
            sh("halcmd", "setp", n + ".tmax", "0")
        c0 = counters()
        samples = {k: [] for k in parts}
        end = time.time() + a.seconds
        while time.time() < end:
            for k, n in parts.items():
                v = getp(n + ".time")
                if v is not None:
                    samples[k].append(v)
            time.sleep(0.2)
        for k, n in parts.items():
            s = samples[k]
            res[k + "_avg"] = sum(s) / len(s) / 1000.0 if s else None
            t = getp(n + ".tmax")
            res[k + "_max"] = t / 1000.0 if t is not None else None
        c1 = counters()
        res["sserial_errors"] = sum(max(0.0, c1.get(k, 0) - c0.get(k, 0)) for k in c1)
        res["board"] = board or "-"
    finally:
        sh("halcmd", "stop")
        sh("halrun", "-U", timeout=30)
        if p.poll() is None:
            try:
                os.killpg(p.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
        log.close()
    text = open(log.name).read()
    res["rt_delays"] = len(re.findall(r"[Uu]nexpected realtime delay", text))
    res["hm2_errors"] = len(re.findall(r"hm2.*(?:[Ee]rror|[Tt]imeout)", text))
    return res


def verdict(r, exchange):
    if r.get("servo_max") is None:
        return "-"
    used = (r["servo_max"] + exchange) / r["period"]
    return "%s (%d %%)" % ("fits" if used < 0.8 else "tight" if used < 1.0 else "no", round(used * 100))


def main():
    drv, prm = hm2_driver.driver()
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--driver", default=drv, help="hm2_spix, hm2_rpspi, hm2_pci, hm2_soc_ol, hm2_eth ... (none: the thread only)")
    ap.add_argument("--params", help="driver parameters (default: stmbl.ini's for the SPI driver)")
    ap.add_argument("--config", default="sserial_port_0=00000000", help="hostmot2 config string")
    ap.add_argument("--periods", default="1000,500,333,250", help="servo periods [us]")
    ap.add_argument("--seconds", type=float, default=60.0)
    ap.add_argument("--no-motion", dest="motion", action="store_false")
    ap.add_argument("--exchange-us", type=float, default=150.0, help="the smart-serial exchange's time [us]")
    ap.add_argument("--out", default="servo_rate.md")
    a = ap.parse_args()
    if a.params is None:
        a.params = prm if a.driver == drv else ""
    if os.geteuid() == 0:
        sys.exit("servo_rate: not as root (rtapi_app refuses); run as the machine's user")
    info = facts()
    print(info)
    rows = []
    d = tempfile.mkdtemp(prefix="servo-rate.")  # kept: the halrun logs
    if True:
        for pu in (float(x) for x in a.periods.split(",")):
            r = run_period(a, pu, d)
            rows.append(r)
            if "error" in r:
                print("%6.0f us: %s" % (pu, r["error"]))
                continue
            f = lambda k: "%.1f" % r[k] if r.get(k) is not None else "-"
            print("%6.0f us (%4.0f Hz): servo %s / %s  read %s / %s  write %s / %s  sserial err %d  rt delays %d  -> %s" % (
                pu, 1e6 / pu, f("servo_avg"), f("servo_max"), f("read_avg"), f("read_max"), f("write_avg"), f("write_max"),
                r["sserial_errors"], r["rt_delays"], verdict(r, a.exchange_us)), flush=True)
    md = ["# Servo rate on this controller\n\n%s\n\nDriver `%s %s`, config `%s`, %s, %g s per period. Times in us (average / largest).\n\n" % (
        info, a.driver, a.params, a.config, "with motion" if a.motion else "bare thread", a.seconds),
        "| period | rate | servo | read | write | sserial errors | rt delays | + %g us exchange |\n|---|---|---|---|---|---|---|---|\n" % a.exchange_us]
    for r in rows:
        if "error" in r:
            md.append("| %.0f us | %.0f Hz | %s | | | | | |\n" % (r["period"], 1e6 / r["period"], r["error"]))
            continue
        f = lambda k: "%.1f / %.1f" % (r[k + "_avg"], r[k + "_max"]) if r.get(k + "_avg") is not None else "-"
        md.append("| %.0f us | %.0f Hz | %s | %s | %s | %d | %d | %s |\n" % (
            r["period"], 1e6 / r["period"], f("servo"), f("read"), f("write"), r["sserial_errors"], r["rt_delays"], verdict(r, a.exchange_us)))
    open(a.out, "w").write("".join(md))
    print("\n" + a.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
