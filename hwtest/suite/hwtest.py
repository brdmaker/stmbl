#!/usr/bin/env python3
"""
Hardware test suite for an STMBL drive, real (hw) or simulated (sim).

Two phases, started by run_hwtest.sh:

  drive  no LinuxCNC; the drive's USB console (sim: stmbl_host --pty).
         Identity, HAL health, readings, console line handling, config
         dump / load / flash save, capture scope, reset, and on hw the
         fault record and safe mode.
  lcnc   LinuxCNC's DISPLAY program (hm2 + sserialterm, or stmbl_sim).
         The smart-serial link and console, the power field, machine on
         and enable cycles, a motion program with following-error limits,
         console traffic and a scope capture while moving, rt load.
  report merges both phases into summary.md and results.tap.

Each test is a function that raises Fail (or returns a detail string).
Results go to the run directory: results-<phase>.json, hwtest.log.
"""
import argparse
import datetime
import json
import os
import re
import subprocess
import sys
import threading
import time
import traceback

import math

import serial

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import hm2_driver  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
CLI = os.path.join(REPO, "servoterm-cli", "servoterm-cli")

TESTS = []  # (phase, name, targets, func)


def test(phase, name, targets=("hw", "sim")):
    def reg(f):
        TESTS.append((phase, name, targets, f))
        return f
    return reg


class Fail(Exception):
    pass


class Skip(Exception):
    pass


def need(cond, msg):
    if not cond:
        raise Fail(msg)


def now():
    return datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]


# --------------------------------------------------------------------------
# context: ports, logging, measurements


class Ctx:
    def __init__(self, a):
        self.a = a
        self.dir = a.out
        self.log = open(os.path.join(a.out, "hwtest.log"), "a")
        self.meas = {}  # name -> value, for the report
        self.lock = threading.Lock()
        self.baseline_conf = None
        conf = os.path.join(a.out, "baseline.conf")
        if os.path.exists(conf):
            self.baseline_conf = open(conf).read()

    def note(self, text):
        with self.lock:
            self._note(text)

    def _note(self, text):
        self.log.write("%s %s\n" % (now(), text))
        self.log.flush()

    def m(self, name, value):
        self.meas[name] = value

    def cli(self, port, *args, quiet=None, timeout=120):
        """servoterm-cli on a port; returns (rc, output)."""
        q = quiet if quiet is not None else (1.0 if port == self.a.con else 0.5)
        cmd = [CLI, "-d", port, "-t", str(q)] + list(args)
        self.note("$ " + " ".join(cmd[1:]))
        try:
            r = subprocess.run(cmd, capture_output=True, timeout=timeout, cwd=self.dir)
        except subprocess.TimeoutExpired:
            self.note("  timeout")
            return -1, "timeout"
        raw = r.stdout + r.stderr
        # a byte that is not text (0x80 in place of a digit, seen on the bench
        # 2026-10-19/20) is logged, not fatal
        out = raw.decode("utf-8", errors="backslashreplace")
        if "\\x" in out:
            self.note("  console: bytes that are not text: %r" % raw[-400:])
            self.meas["console_bad_bytes"] = self.meas.get("console_bad_bytes", 0) + 1
        for l in out.splitlines()[:60]:
            self.note("  " + l)
        return r.returncode, out

    def pins(self, port, *names, **kw):
        rc, out = self.cli(port, *sum([["-c", n] for n in names], []), **kw)
        return pin_values(out)

    def raw(self, port, chunks, gap=0.05, wait=2.0):
        """Write raw byte chunks (with gaps), return the text that came back."""
        s = serial.Serial(port, 115200, timeout=0.05)
        try:
            time.sleep(0.3)
            s.reset_input_buffer()
            for c in chunks:
                s.write(c)
                time.sleep(gap)
            data = b""
            end = time.time() + wait
            while time.time() < end:
                data += s.read(4096)
        finally:
            s.close()
        text = strip_scope(data).decode(errors="replace")
        self.note("raw %r -> %r" % (chunks, text[-300:]))
        return text

    def wait_port(self, port, gone_first=False, timeout=30):
        if gone_first:
            end = time.time() + 5
            while time.time() < end and os.path.exists(port):
                time.sleep(0.1)
        end = time.time() + timeout
        while time.time() < end:
            if os.path.exists(port):
                time.sleep(2.5)  # let the drive boot
                return True
            time.sleep(0.2)
        return False


def strip_scope(data):
    """Drop the drive's scope frames (0xFF + 8 bytes, 0xFE) from a USB stream."""
    out = bytearray()
    skip = 0
    for b in data:
        if skip:
            skip -= 1
        elif b == 0xFF:
            skip = 8
        elif b != 0xFE:
            out.append(b)
    return bytes(out)


def pin_values(text):
    out = {}
    for l in text.splitlines():
        m = re.match(r"\s*(?:OK )?(\w+\d+\.\w+)(?: <= \S+)? = (-?[0-9.]+|nan|-?inf)", l)
        if m:
            try:
                out[m.group(1)] = float(m.group(2))
            except ValueError:
                pass
    return out


def hal_info(ctx, port):
    rc, out = ctx.cli(port, "-c", "hal")
    state = re.search(r"HAL state:\s+(\S+)", out)
    rt = re.search(r"^rt time: ([0-9.]+)\(max ([0-9.]+)\)us/([0-9.]+)us", out, re.M)
    frt = re.search(r"^frt time: ([0-9.]+)\(max ([0-9.]+)\)us/([0-9.]+)us", out, re.M)
    ticks = re.search(r"active rt funcs\((\d+)\)", out)
    return {
        "state": state.group(1) if state else None,
        "rt": float(rt.group(1)) if rt else None,
        "rt_max": float(rt.group(2)) if rt else None,
        "rt_period": float(rt.group(3)) if rt else None,
        "frt_max": float(frt.group(2)) if frt else None,
        "rt_funcs": int(ticks.group(1)) if ticks else None,
        "text": out,
    }


def uptime_s(ctx, port):
    rc, out = ctx.cli(port, "-c", "uptime")
    m = re.search(r"up (\d+):(\d+):(\d+)", out)
    return int(m.group(1)) * 3600 + int(m.group(2)) * 60 + int(m.group(3)) if m else None


def dump(ctx, port, name):
    path = os.path.join(ctx.dir, name)
    if os.path.exists(path):
        os.remove(path)
    rc, out = ctx.cli(port, "--dump", path, timeout=180)
    return open(path).read() if rc == 0 and os.path.exists(path) else None


def baseline(ctx):
    """The drive's config as dumped at the start (dumped now if not yet)."""
    if ctx.baseline_conf is None:
        c = dump(ctx, ctx.a.usb, "baseline.conf")
        need(c, "cannot dump the baseline config")
        ctx.baseline_conf = c
    return ctx.baseline_conf


def norm(conf):
    return [l.rstrip() for l in conf.strip().splitlines()]


# --------------------------------------------------------------------------
# drive phase: USB console, no LinuxCNC


@test("drive", "identify: about answers with the firmware version")
def t_identify(ctx):
    rc, out = ctx.cli(ctx.a.usb, "-c", "about")
    m = re.search(r"STMBL v\S+ (\S+)", out) or re.search(r"(STMBL simulated drive.*)", out)
    need(rc == 0 and m, "no version in: %r" % out[-200:])
    ctx.m("firmware", m.group(1).strip())
    hv = re.search(r"STMBL-hv-f303 v\S+ (\S+)", out)
    if hv:
        ctx.m("hv_firmware", hv.group(1))
    return ctx.meas["firmware"]


@test("drive", "HAL running (HAL_OK2), rt within its period")
def t_hal(ctx):
    h = hal_info(ctx, ctx.a.usb)
    need(h["state"] == "HAL_OK2", "HAL state %s" % h["state"])
    need(h["rt_funcs"], "no rt functions running (safe mode?)")
    if h["rt_max"] is not None:
        ctx.m("idle_rt_us", h["rt"])
        ctx.m("idle_rt_max_us", h["rt_max"])
        need(h["rt_max"] < 0.9 * h["rt_period"], "rt max %.1f us of %.0f" % (h["rt_max"], h["rt_period"]))
        return "rt %.1f us, max %.1f of %.0f" % (h["rt"], h["rt_max"], h["rt_period"])
    return "%d rt functions (no timing on the sim)" % h["rt_funcs"]


@test("drive", "readings: DC bus, temperatures, fault state plausible")
def t_readings(ctx):
    p = ctx.pins(ctx.a.usb, "hv0.dc_volt", "hv0.hv_temp", "iit0.temp", "fault0.state", "fault0.fault")
    for k in ("hv0.dc_volt", "hv0.hv_temp", "fault0.state", "fault0.fault"):
        need(k in p, "no %s" % k)
    dc = p["hv0.dc_volt"]
    ctx.m("dc_volt", dc)
    ctx.m("hv_temp", p["hv0.hv_temp"])
    need(dc > ctx.a.min_dc, "DC bus %.1f V below %.0f V" % (dc, ctx.a.min_dc))
    if ctx.a.dc_volt:
        need(abs(dc - ctx.a.dc_volt) < 0.05 * ctx.a.dc_volt, "DC bus %.1f V, expected %.0f V +-5%%" % (dc, ctx.a.dc_volt))
    need(5 < p["hv0.hv_temp"] < 70, "HV temperature %.1f C" % p["hv0.hv_temp"])
    # no LinuxCNC: disabled (0) or the CMD error (state 3, fault 1) is expected
    need(p["fault0.state"] in (0, 3) and p["fault0.fault"] in (0, 1),
         "fault state %g fault %g" % (p["fault0.state"], p["fault0.fault"]))
    return "DC %.1f V, HV %.1f C, motor %.1f C" % (dc, p["hv0.hv_temp"], p.get("iit0.temp", float("nan")))


@test("drive", "uid: lv and hv CPU unique IDs reported; the USB serial matches the lv ID", targets=("hw",))
def t_uid(ctx):
    rc, out = ctx.cli(ctx.a.usb, "-c", "uid")
    lv = re.search(r"lv uid: ([0-9a-f]{24})", out)
    hv = re.search(r"hv uid: ([0-9a-f]{24})", out)
    usb = re.search(r"usb serial: ([0-9A-F]{12})", out)
    need(lv and usb, "no lv uid / usb serial: %r" % out[-200:])
    need(hv, "no hv uid (HV firmware too old? run hv_update): %r" % out[-200:])
    need(lv.group(1) != hv.group(1), "lv and hv IDs are the same")
    r = subprocess.run(["lsusb", "-v", "-d", "0483:5740"], capture_output=True, text=True)
    m = re.search(r"iSerial\s+\d+\s+(\S+)", r.stdout)
    if m:
        need(m.group(1) == usb.group(1), "USB descriptor serial %s, drive says %s" % (m.group(1), usb.group(1)))
    ctx.m("lv_uid", lv.group(1))
    ctx.m("hv_uid", hv.group(1))
    return "lv %s, hv %s" % (lv.group(1), hv.group(1))


@test("drive", "console: unknown command answers 'not found'")
def t_unknown(ctx):
    rc, out = ctx.cli(ctx.a.usb, "-c", "hwtest_no_such_pin")
    need("not found" in out, out[-200:])


@test("drive", "console: a line split over two USB writes is put back together")
def t_split(ctx):
    text = ctx.raw(ctx.a.usb, [b"\n", b"hv0.dc", b"_volt\n"], gap=0.05)
    need(re.search(r"hv0\.dc_volt = [0-9.]+", text), "answer: %r" % text[-200:])


@test("drive", "console: a 70-character line is dropped with an error, the next works")
def t_long_line(ctx):
    text = ctx.raw(ctx.a.usb, [b"\n", b"x" * 70 + b"\n", b"hv0.dc_volt\n"], gap=0.1)
    need("too long" in text, "no error for the long line: %r" % text[-200:])
    need(re.search(r"hv0\.dc_volt = [0-9.]+", text), "next command not answered: %r" % text[-200:])


@test("drive", "config: repeated dumps are identical")
def t_dumps(ctx):
    confs = []
    t0 = time.time()
    for i in range(ctx.a.dumps):
        c = dump(ctx, ctx.a.usb, "dump-%d.conf" % i)
        need(c, "dump %d failed" % i)
        confs.append(c)
    ctx.m("usb_dump_s", (time.time() - t0) / len(confs))
    bad = [i for i, c in enumerate(confs) if c != confs[0]]
    need(not bad, "dumps %s differ from the first" % bad)
    ctx.baseline_conf = confs[0]
    open(os.path.join(ctx.dir, "baseline.conf"), "w").write(confs[0])
    return "%d dumps, %d lines, %.1f s each" % (len(confs), len(norm(confs[0])), ctx.meas["usb_dump_s"])


@test("drive", "config: --load writes and verifies flash, HAL keeps running, original restored")
def t_load(ctx):
    baseline(ctx)
    orig = os.path.join(ctx.dir, "baseline.conf")
    test = os.path.join(ctx.dir, "load-test.conf")
    open(test, "w").write(ctx.baseline_conf.rstrip("\n") + "\n# hwtest %s\n" % time.strftime("%Y%m%d-%H%M%S"))
    rc, out = ctx.cli(ctx.a.usb, "--load", test, timeout=180)
    need(rc == 0, "--load of the test config failed: %s" % out[-300:])
    got = dump(ctx, ctx.a.usb, "after-load.conf")
    need(got and norm(got) == norm(open(test).read()), "flash does not hold the test config")
    h = hal_info(ctx, ctx.a.usb)
    need(h["state"] == "HAL_OK2", "HAL state after the flash write: %s" % h["state"])
    rc, out = ctx.cli(ctx.a.usb, "--load", orig, timeout=180)
    need(rc == 0, "restoring the original failed: %s" % out[-300:])
    got = dump(ctx, ctx.a.usb, "after-restore.conf")
    need(got == baseline(ctx), "flash differs from the original after restore")
    h = hal_info(ctx, ctx.a.usb)
    need(h["state"] == "HAL_OK2", "HAL state after restoring: %s" % h["state"])


@test("drive", "config: flashsaveconf x3 keeps the HAL running (rt stopped around the erase)")
def t_flashsave(ctx):
    for i in range(3):
        rc, out = ctx.cli(ctx.a.usb, "-c", "flashloadconf", "-c", "flashsaveconf", quiet=1.5)
        need("bytes written" in out, "save %d: %s" % (i, out[-200:]))
        h = hal_info(ctx, ctx.a.usb)
        need(h["state"] == "HAL_OK2", "save %d: HAL state %s" % (i, h["state"]))
    got = dump(ctx, ctx.a.usb, "after-save.conf")
    need(got == baseline(ctx), "flash changed")


@test("drive", "capture scope over USB: forced capture, 2 x 2048 samples, CRCs ok")
def t_capture_usb(ctx):
    csv = os.path.join(ctx.dir, "capture-usb.csv")
    rc, out = ctx.cli(ctx.a.usb, "-c", "scope0.nch = 2", "-c", "scope0.ch0 = hv0.dc_volt", "-c", "scope0.ch1 = hv0.hv_temp",
                      "-c", "scope0.trig = hv0.dc_volt", "-c", "scope0.level = 100000", "-c", "scope0.mode = 2",
                      "--capture", csv, "--capture-timeout", "60", timeout=120)
    need(rc == 0 and os.path.exists(csv), out[-300:])
    rows = open(csv).read().split()
    need(len(rows) == 2049 and rows[0] == "t,hv0.dc_volt,hv0.hv_temp", "%d rows, header %r" % (len(rows), rows[:1]))
    dc = [float(r.split(",")[1]) for r in rows[1:]]
    need(min(dc) > ctx.a.min_dc, "DC samples down to %.1f V" % min(dc))
    m = re.search(r"in ([0-9.]+) s", out)
    return "DC %.1f..%.1f V%s" % (min(dc), max(dc), ", %s s" % m.group(1) if m else "")


@test("drive", "reset: the drive reboots, comes back running with the same config")
def t_reset(ctx):
    # a pin set in RAM only must be back at its default after the reboot
    ctx.cli(ctx.a.usb, "-c", "scope0.level = 77.25")
    ctx.cli(ctx.a.usb, "-c", "reset", quiet=0.3, timeout=20)
    need(ctx.wait_port(ctx.a.usb, gone_first=ctx.a.target == "hw"), "console did not come back")
    p = ctx.pins(ctx.a.usb, "scope0.level")
    need(p.get("scope0.level") not in (None, 77.25), "RAM pin kept its value: no reboot")
    up = uptime_s(ctx, ctx.a.usb)
    if ctx.a.target == "hw":  # the sim's clock runs on through a reset
        need(up is not None and up < 30, "uptime %s after reset" % up)
    h = hal_info(ctx, ctx.a.usb)
    need(h["state"] == "HAL_OK2" and h["rt_funcs"], "HAL state %s, %s rt funcs" % (h["state"], h["rt_funcs"]))
    got = dump(ctx, ctx.a.usb, "after-reset.conf")
    need(got == baseline(ctx), "config differs after reset")
    return "up %d s" % up


def fault_and_wait(ctx):
    ctx.cli(ctx.a.usb, "-c", "fault", quiet=0.3, timeout=20)
    need(ctx.wait_port(ctx.a.usb, gone_first=True), "console did not come back after the fault")


@test("drive", "faults: a bus fault resets the drive, `crash` records it", targets=("hw",))
def t_fault_record(ctx):
    ctx.cli(ctx.a.usb, "-c", "crash clear")
    fault_and_wait(ctx)
    rc, out = ctx.cli(ctx.a.usb, "-c", "crash", "-c", "uptime")
    need("crashes since power-on: 1" in out, "record: %s" % out[-300:])
    need("bfar 0x30000000" in out, "bus fault address not recorded: %s" % out[-300:])
    pc = re.search(r"pc (0x[0-9a-f]+)", out)
    elf = os.path.join(REPO, "obj_app", "stmbl.elf")
    where = ""
    if pc and os.path.exists(elf):
        r = subprocess.run(["arm-none-eabi-addr2line", "-f", "-e", elf, pc.group(1)], capture_output=True, text=True)
        where = " ".join(r.stdout.split())
        need(where.startswith("fault "), "PC %s resolves to %s, not fault()" % (pc.group(1), where))
    h = hal_info(ctx, ctx.a.usb)
    need(h["state"] == "HAL_OK2" and h["rt_funcs"], "HAL after the reset: %s" % h["state"])
    return "pc in %s" % where if where else "recorded"


@test("drive", "faults: 3 crashes in a row boot safe mode, `reset` boots normally", targets=("hw",))
def t_safe_mode(ctx):
    for i in range(2):  # the record test left one
        fault_and_wait(ctx)
    rc, out = ctx.cli(ctx.a.usb, "-c", "crash")
    need("safe mode" in out, "not in safe mode: %s" % out[-300:])
    h = hal_info(ctx, ctx.a.usb)
    need(h["rt_funcs"] == 0, "safe mode runs %s rt functions" % h["rt_funcs"])
    ctx.cli(ctx.a.usb, "-c", "reset", quiet=0.3, timeout=20)
    need(ctx.wait_port(ctx.a.usb, gone_first=True), "console did not come back")
    h = hal_info(ctx, ctx.a.usb)
    need(h["state"] == "HAL_OK2" and h["rt_funcs"], "normal boot: HAL %s, %s rt funcs" % (h["state"], h["rt_funcs"]))
    got = dump(ctx, ctx.a.usb, "after-safe.conf")
    need(got == baseline(ctx), "config differs after safe mode")
    ctx.cli(ctx.a.usb, "-c", "crash clear")


def errlog_entries(ctx):
    rc, out = ctx.cli(ctx.a.usb, "-c", "errlog all")
    return [l for l in out.splitlines() if re.match(r"\s*\d+\s+boot ", l)]


@test("drive", "watchdog: a hung main loop or interrupts reset the drive, `crash` and `errlog` name it", targets=("hw",))
def t_watchdog(ctx):
    took = []
    for how in ("main", "irq"):
        ctx.cli(ctx.a.usb, "-c", "crash clear")
        before = len(errlog_entries(ctx))
        t0 = time.monotonic()
        ctx.cli(ctx.a.usb, "-c", "wdtest " + how, quiet=0.3, timeout=20)
        need(ctx.wait_port(ctx.a.usb, gone_first=True), "wdtest %s: the drive didn't reset" % how)
        took.append(time.monotonic() - t0)
        rc, out = ctx.cli(ctx.a.usb, "-c", "crash")
        need("last reset: watchdog" in out and "last: Watchdog" in out and "console command: wdtest" in out,
             "wdtest %s: record: %s" % (how, out[-300:]))
        new = errlog_entries(ctx)[before:]
        need(any("HANG: watchdog reset cmd wdtest" in l for l in new), "wdtest %s: errlog: %s" % (how, new))
        need(any("boot: watchdog" in l for l in new), "wdtest %s: no watchdog boot entry: %s" % (how, new))
        h = hal_info(ctx, ctx.a.usb)
        need(h["state"] == "HAL_OK2" and h["rt_funcs"], "wdtest %s: HAL after the reset: %s" % (how, h["state"]))
    ctx.cli(ctx.a.usb, "-c", "crash clear")
    got = dump(ctx, ctx.a.usb, "after-watchdog.conf")
    need(got == baseline(ctx), "config differs after the watchdog resets")
    return "back after %.1f / %.1f s" % tuple(took)


def usb_answers(port):
    import serial
    s = serial.Serial(port, 115200, timeout=0.2)
    try:
        s.reset_input_buffer()
        for _ in range(3):
            s.write(b"hv0.dc_volt\n")
            t, b = time.monotonic(), b""
            while time.monotonic() - t < 1.5:
                b += s.read(4096)
                if b"hv0.dc_volt" in b and b"=" in b:
                    return True
        return False
    finally:
        s.close()


@test("drive", "usb: 1000 port closes in the middle of output leave the console sending", targets=("hw",))
def t_usb_stall(ctx):
    # ST's CDC went idle right after a ZLP; the next transfer went over it and
    # the console never sent again, input still worked (DEBUGNOTES 2026-10-29,
    # stuck after 350 rounds before the fix).
    import random
    import serial
    rng = random.Random(1)
    for i in range(1000):
        s = serial.Serial(ctx.a.usb, 115200, timeout=0.01)
        s.write(rng.choice([b"list\n", b"help\n", b"show hv0\n", b"about\n"]))
        time.sleep(rng.uniform(0.002, 0.08))
        s.read(rng.randint(0, 3000))
        s.close()
        if i % 50 == 49:
            need(usb_answers(ctx.a.usb), "console stopped sending after %d rounds" % (i + 1))
    rc, out = ctx.cli(ctx.a.usb, "-c", "about")
    m = re.search(r"usb tx stalls healed: (\d+)", out)
    need(m, "about has no healed count: %s" % out[-300:])
    ctx.m("usb_tx_healed", int(m.group(1)))
    return "healed %s" % m.group(1)


def dfu_present():
    r = subprocess.run(["dfu-util", "-l"], capture_output=True, text=True)
    return "0483:df11" in r.stdout


def dfu_requests(ctx):
    rc, out = ctx.cli(ctx.a.usb, "-c", "about")
    m = re.search(r"DFU requests taken by the bootloader since power-on: (\d+)", out)
    return int(m.group(1)) if m else None


def wait_for(cond, timeout, step=0.25):
    end = time.time() + timeout
    while time.time() < end:
        if cond():
            return True
        time.sleep(step)
    return False


@test("drive", "bootloader: tools/bootloader.py enters DFU, leave boots the app, x5", targets=("hw",))
def t_dfu_round_trips(ctx):
    # The app's `bootloader` used to jump into the ROM itself: half the
    # entries just rebooted the app, and the stmbl bootloader's own path
    # never brought up USB. ST's ROM still goes back to the app now and
    # then; bootloader.py retries.
    c0 = dfu_requests(ctx)
    for i in range(5):
        r = subprocess.run([sys.executable, os.path.join(REPO, "tools", "bootloader.py"), "dfu-util"],
                           capture_output=True, text=True, timeout=120)
        ctx.note("bootloader.py: %s" % r.stdout.strip())
        need(r.returncode == 0 and dfu_present(), "round trip %d: no DFU: %s" % (i, r.stdout.strip()[-200:]))
        out = os.path.join(ctx.dir, "dfu-%d.bin" % i)
        if os.path.exists(out):
            os.remove(out)
        r = subprocess.run(["dfu-util", "-d", "0483:df11", "-a", "0", "-s", "0x08000000:4:leave", "-U", out],
                           capture_output=True, text=True, timeout=30)
        need(r.returncode == 0 and os.path.getsize(out) == 4, "round trip %d: DFU read / leave failed" % i)
        need(ctx.wait_port(ctx.a.usb, timeout=30), "round trip %d: the app did not come back" % i)
    c1 = dfu_requests(ctx)
    h = hal_info(ctx, ctx.a.usb)
    need(h["state"] == "HAL_OK2" and h["rt_funcs"], "HAL after the round trips: %s" % h["state"])
    if c0 is not None and c1 is not None:
        ctx.m("dfu_rom_bounces", c1 - c0 - 5)
        return "5 round trips, %d ROM bounce(s) retried" % (c1 - c0 - 5)
    return "5 round trips"


def drive_version(ctx):
    if not ctx.meas.get("firmware"):
        rc, out = ctx.cli(ctx.a.usb, "-c", "about")
        m = re.search(r"STMBL v\S+ (\S+)", out)
        if m:
            ctx.m("firmware", m.group(1))
    return ctx.meas.get("firmware") or ""


def build_version():
    try:
        data = open(os.path.join(REPO, "obj_app", "stmbl.bin"), "rb").read()
    except OSError:
        return None
    m = re.search(rb"v\d+\.\d+\.\d+-\d+-g[0-9a-f]+(?:-dirty)?", data)
    return m.group(0).decode() if m else None


@test("drive", "hardboot: an erased app gives DFU from the stmbl bootloader, btburn restores it", targets=("hw",))
def t_hardboot(ctx):
    fw = drive_version(ctx)
    built = build_version()
    if not built or built != fw:
        raise Skip("the drive runs %s, the repo build is %s (btburn would change it)" % (fw, built))
    ctx.cli(ctx.a.usb, "-c", "hardboot", quiet=0.3, timeout=20)
    # the bootloader first waits 30 s in its smart-serial loader for setsserial
    need(wait_for(dfu_present, 45), "no DFU after hardboot (bootloader CRC path)")
    r = subprocess.run(["make", "-C", REPO, "btburn"], capture_output=True, text=True, timeout=600)
    ctx.note(r.stdout[-2000:] + r.stderr[-1000:])
    need(r.returncode == 0, "make btburn failed: %s" % (r.stdout + r.stderr)[-300:])
    need(ctx.wait_port(ctx.a.usb, timeout=30), "the app did not come back after btburn")
    rc, out = ctx.cli(ctx.a.usb, "-c", "about")
    need(fw in out, "firmware after restore: %s" % out[-200:])
    got = dump(ctx, ctx.a.usb, "after-hardboot.conf")
    need(got == baseline(ctx), "config differs after hardboot / btburn")


@test("drive", "update over USB: servoterm-cli --update stages, the bootloader copies, the app is back", targets=("hw",))
def t_update_usb(ctx):
    fw = drive_version(ctx)
    built = build_version()
    if not built or built != fw:
        raise Skip("the drive runs %s, the repo build is %s" % (fw, built))
    image = os.path.join(REPO, "obj_app", "stmbl.bin")
    t0 = time.time()
    # a long quiet time: after the last line the drive checks the image
    # (fwend busy) before it answers
    rc, out = ctx.cli(ctx.a.usb, "--update", image, quiet=5, timeout=600)
    need(rc == 0 and "applied" in out, "--update: %s" % out[-300:])
    need(ctx.wait_port(ctx.a.usb, gone_first=True, timeout=60), "the app did not come back after the update")
    rc, out = ctx.cli(ctx.a.usb, "-c", "about")
    need(fw in out, "firmware after the update: %s" % out[-200:])
    got = dump(ctx, ctx.a.usb, "after-update.conf")
    need(got == baseline(ctx), "config differs after the update")
    ctx.m("update_usb_s", time.time() - t0)
    return "%.0f s" % (time.time() - t0)


SSFW = "/lib/firmware/stmbl/STMBL_SS.BIN"


@test("drive", "recovery: hardboot, then setsserial flash through the bootloader's smart-serial loader", targets=("hw",))
def t_setsserial(ctx):
    # The loader must be found at 115200 (card name sIbl), answer every
    # write with 0x00 and keep commits under SSLBP's 50 ms (DEBUGNOTES).
    fw = drive_version(ctx)
    built = build_version()
    if not built or built != fw:
        raise Skip("the drive runs %s, the repo build is %s" % (fw, built))
    try:
        img = open(SSFW, "rb").read()
    except OSError:
        raise Skip("no %s (make ssfw_install)" % SSFW)
    if built.encode() not in img:
        raise Skip("%s is not the repo build (make ssfw_install)" % SSFW)
    hal = os.path.join(ctx.dir, "setsserial.hal")
    open(hal, "w").write('loadrt hostmot2 sserial_baudrate=115200\n' + hm2_driver.loadrt() + '\n'
                         'loadrt setsserial cmd="flash %ssibl.0.0 stmbl/STMBL_SS.BIN"\n'
                         % ctx.a.prefix.replace("stbl.0.0.", ""))
    ctx.cli(ctx.a.usb, "-c", "hardboot", quiet=0.3, timeout=20)
    need(wait_for(lambda: not os.path.exists(ctx.a.usb), 10), "hardboot did not reset the drive")
    t_boot = time.time()
    # One attempt, once the loader is surely ready: the bootloader may first
    # erase a staging area, then the loader erases the application area
    # before it answers. A hostmot2 started earlier finds no remote (it then
    # also turns the channel's CRC off in the card for its 8i20 search; the
    # loader ignores requests without CRC). Within the loader's 30 s, after
    # which it falls back to DFU.
    time.sleep(max(0.0, t_boot + 15 - time.time()))
    t0 = time.time()
    r = subprocess.run(["halrun", "-f", hal], capture_output=True, text=True, timeout=400)
    out = r.stdout + r.stderr
    subprocess.run(["halrun", "-U"], capture_output=True)
    open(os.path.join(ctx.dir, "setsserial.log"), "w").write(out)
    wrote = out.count("Wrote block")
    errors = len(re.findall(r"Error in sslbp|Synch failed|Flash Failed", out))
    need("Flash Success" in out and wrote > 0 and errors == 0,
         "setsserial: %d blocks written, %d errors (setsserial.log)" % (wrote, errors))
    need(ctx.wait_port(ctx.a.usb, timeout=30), "the app did not start after setsserial")
    rc, o = ctx.cli(ctx.a.usb, "-c", "about")
    need(fw in o, "firmware after setsserial: %s" % o[-200:])
    got = dump(ctx, ctx.a.usb, "after-setsserial.conf")
    need(got == baseline(ctx), "config differs after setsserial")
    ctx.m("setsserial_s", time.time() - t0)
    return "%d blocks in %.0f s, no errors" % (wrote, time.time() - t0)


# --------------------------------------------------------------------------
# lcnc phase: LinuxCNC display program


class Lcnc:
    def __init__(self, ctx):
        import hal
        import linuxcnc
        self.hal = hal
        self.comp = hal.component("hwtest")
        self.comp.ready()
        self.lc = linuxcnc
        self.s = linuxcnc.stat()
        self.c = linuxcnc.command()
        self.e = linuxcnc.error_channel()
        self.ctx = ctx
        self.msgs = []

    def poll(self):
        self.s.poll()
        return self.s

    def wait(self, pred, timeout, step=0.02):
        end = time.time() + timeout
        while time.time() < end:
            if pred(self.poll()):
                return True
            time.sleep(step)
        return False

    def errors(self):
        out = []
        while True:
            m = self.e.poll()
            if not m:
                break
            out.append(m[1])
        for m in out:
            self.ctx.note("lcnc: %s" % m)
        self.msgs += out
        return out

    def drive_errors(self):
        """LinuxCNC messages except the Pi's own late servo cycles."""
        return [m for m in self.errors() if "Unexpected realtime delay" not in m]

    def pin(self, name):
        return self.hal.get_value(self.ctx.a.prefix + name)

    def on(self):
        """Machine on. self.swing: the largest |pos_fb - pos_cmd| (mm) at the
        drive in the 0.8 s after the enable (normally ~0.01 mm)."""
        self.c.state(self.lc.STATE_ESTOP_RESET)
        self.c.wait_complete()
        self.c.state(self.lc.STATE_ON)
        t0 = time.time()
        self.swing = 0.0
        while time.time() - t0 < 0.8:
            self.swing = max(self.swing, abs(self.pin("pos_fb") - self.pin("pos_cmd")))
            time.sleep(0.0005)
        self.c.wait_complete()
        return self.wait(lambda s: s.task_state == self.lc.STATE_ON, 5)

    def off(self):
        try:
            self.c.abort()
            self.c.wait_complete()
            self.c.state(self.lc.STATE_OFF)
            self.c.wait_complete()
        except Exception:
            pass

    def home(self):
        if all(self.poll().homed[j] for j in range(self.s.joints)):
            return True
        self.c.mode(self.lc.MODE_MANUAL)
        self.c.wait_complete()
        self.c.home(-1)
        self.c.wait_complete()
        return self.wait(lambda s: all(s.homed[j] for j in range(s.joints)), 30)

    def mdi(self, cmd, timeout=30):
        self.c.mode(self.lc.MODE_MDI)
        self.c.wait_complete()
        self.c.mdi(cmd)
        time.sleep(0.1)
        return self.wait(lambda s: s.interp_state == self.lc.INTERP_IDLE and s.queue == 0, timeout)

    def run_program(self, path, during=None, timeout=300):
        """Run a G-code file; call during() repeatedly while it runs.
        Returns (ok, ferror_max_mm, vel_max_pos, vel_max_neg)."""
        self.c.mode(self.lc.MODE_AUTO)
        self.c.wait_complete()
        self.c.program_open(path)
        self.c.wait_complete()
        self.c.auto(self.lc.AUTO_RUN, 0)
        fmax = vpos = vneg = 0.0
        t0 = time.time()
        started = False
        while time.time() - t0 < timeout:
            s = self.poll()
            j = s.joint[0]
            fmax = max(fmax, abs(j["ferror_current"]))
            vpos = max(vpos, j["velocity"])
            vneg = min(vneg, j["velocity"])
            if s.task_state != self.lc.STATE_ON:
                return False, fmax, vpos, vneg
            if s.interp_state != self.lc.INTERP_IDLE:
                started = True
            elif started or time.time() - t0 > 2.0:
                return True, fmax, vpos, vneg
            if during:
                during()
            time.sleep(0.02)
        return False, fmax, vpos, vneg

    def ferror(self):
        return abs(self.poll().joint[0]["ferror_current"])


def ready(ctx):
    """Machine on and homed (for tests run on their own with --only)."""
    L = lcnc(ctx)
    if L.poll().task_state != L.lc.STATE_ON:
        need(L.on(), "machine on failed: %s" % L.errors())
        time.sleep(2.0)
    need(L.home(), "not homed")
    return L


def lcnc(ctx):
    if not hasattr(ctx, "_lcnc"):
        ctx._lcnc = Lcnc(ctx)
    return ctx._lcnc


@test("lcnc", "LinuxCNC up; smart-serial link carries the drive's console")
def t_link(ctx):
    L = lcnc(ctx)
    need(L.wait(lambda s: s.task_state != 0, 30), "LinuxCNC task not up")
    time.sleep(2.0)
    ctx.m("uptime_start", uptime_s(ctx, ctx.a.usb))
    rc, out = ctx.cli(ctx.a.con, "-c", "about")
    need(rc == 0 and "STMBL" in out, "no answer over smart-serial: %s" % out[-300:])
    need(L.hal.get_value("sserialterm.0.active"), "sserialterm.0.active is 0")
    p = ctx.pins(ctx.a.usb, "fault0.fault")
    need(p.get("fault0.fault") == 0, "drive fault %s with LinuxCNC running" % p.get("fault0.fault"))


@test("lcnc", "smart-serial console: pin write and read back")
def t_con_pin(ctx):
    rc, out = ctx.cli(ctx.a.con, "-c", "scope0.level = 12.5", "-c", "scope0.level")
    need(re.search(r"scope0\.level = 12\.5", out), out[-200:])
    p = ctx.pins(ctx.a.usb, "scope0.level")  # the same pin seen over USB
    need(p.get("scope0.level") == 12.5, "USB reads %s" % p.get("scope0.level"))


@test("lcnc", "smart-serial console: --dump identical to the USB baseline")
def t_con_dump(ctx):
    baseline(ctx)
    t0 = time.time()
    got = dump(ctx, ctx.a.con, "con-dump.conf")
    ctx.m("sserial_dump_s", time.time() - t0)
    need(got == baseline(ctx), "smart-serial dump differs from the USB one")
    return "%.1f s" % ctx.meas["sserial_dump_s"]


@test("lcnc", "smart-serial console: a 70-character line is dropped, the next works")
def t_con_long(ctx):
    import select
    fd = os.open(ctx.a.con, os.O_RDWR | os.O_NOCTTY)
    try:
        import tty
        tty.setraw(fd)
        os.write(fd, b"x" * 70 + b"\nhv0.dc_volt\n")
        data = b""
        end = time.time() + 4
        while time.time() < end:
            r, _, _ = select.select([fd], [], [], 0.1)
            if r:
                data += os.read(fd, 4096)
    finally:
        os.close(fd)
    text = data.decode(errors="replace")
    ctx.note("long line over smart-serial: %r" % text)
    need("too long" in text and re.search(r"hv0\.dc_volt = [0-9.]+", text), text[-200:])


@test("lcnc", "power field: the hm2 power pin agrees with the drive's pe0.p_in")
def t_power(ctx):
    L = lcnc(ctx)
    p = ctx.pins(ctx.a.usb, "pe0.p_in")
    hm = L.pin("power")
    need("pe0.p_in" in p, "no pe0.p_in")
    ctx.m("power_idle_w", hm)
    need(0 <= hm < 2000, "power pin %s" % hm)
    need(abs(hm - p["pe0.p_in"]) <= 25, "power pin %.1f W, drive %.1f W" % (hm, p["pe0.p_in"]))
    return "pin %.0f W, drive %.1f W" % (hm, p["pe0.p_in"])


DIAG_PINS = ["fault0.state", "fault0.fault", "fb_switch0.mot_state", "fb_switch0.com_state",
             "fb_switch0.com_fb", "fb_switch0.mot_fb_no_offset", "fb_switch0.offset", "enc_fb0.state",
             "pid0.pos_error", "pid0.torque_cmd", "pid0.vel_cmd", "hv0.iq_fb", "hv0.id_fb", "hv0.pos",
             "hv0.vel", "vel2.vel", "vel1.vel", "hv0.dc_volt"]


def diag(ctx, why):
    """Record the drive's commutation / feedback / controller state."""
    rc, out = ctx.cli(ctx.a.usb, *sum([["-c", n] for n in DIAG_PINS], []))
    ctx.note("DIAG (%s):\n%s" % (why, out))
    open(os.path.join(ctx.dir, "diag.txt"), "a").write("== %s %s\n%s\n" % (now(), why, out))


def check_swing(ctx, L, where):
    ctx.m("enable_swing_max_mm", max(ctx.meas.get("enable_swing_max_mm", 0.0), L.swing))
    if L.swing > ctx.a.max_swing:
        diag(ctx, "enable swing %.3f mm %s" % (L.swing, where))
        raise Fail("%s: the motor swung %.3f mm at enable (limit %.2f; see diag.txt)" % (where, L.swing, ctx.a.max_swing))


@test("lcnc", "machine on: drive enables (phasing swing within limits), homes, no fault")
def t_on(ctx):
    L = lcnc(ctx)
    ok = L.on()
    if not ok:
        diag(ctx, "machine on failed")
    check_swing(ctx, L, "machine on")
    need(ok, "LinuxCNC did not reach STATE_ON: %s" % L.errors())
    time.sleep(2.0)
    need(L.home(), "not homed")
    p = ctx.pins(ctx.a.usb, "fault0.state", "fault0.fault")
    need(p.get("fault0.state") == 1, "drive state %s fault %s" % (p.get("fault0.state"), p.get("fault0.fault")))
    need(not L.pin("fault"), "fault pin set")
    errs = L.drive_errors()
    need(not errs, "LinuxCNC: %s" % errs)


def ini_value(ctx, section, key):
    try:
        text = open(os.path.join(ctx.dir, "lcnc", "stmbl.ini")).read()
    except OSError:
        return None
    m = re.search(r"^\[%s\](.*?)(?=^\[|\Z)" % re.escape(section), text, re.S | re.M)
    v = re.search(r"^%s\s*=\s*(\S+)" % key, m.group(1), re.M) if m else None
    return float(v.group(1)) if v else None


@test("lcnc", "ferror limit: PHASING_FERROR until homed and right after an enable, FERROR once settled")
def t_phasing_ferror(ctx):
    # The drive autophases at each enable until it has seen the encoder
    # index; phasing_ferror.hal keeps LinuxCNC's limit above that swing.
    L = ready(ctx)
    relaxed, normal = ini_value(ctx, "JOINT_0", "PHASING_FERROR"), ini_value(ctx, "JOINT_0", "FERROR")
    hold = ini_value(ctx, "JOINT_0", "PHASING_TIME")
    if relaxed is None:
        return "skipped: no [JOINT_0]PHASING_FERROR in the config"
    ferr = lambda: L.hal.get_value("ini.0.ferror")
    time.sleep(hold + 0.5)
    settled = ferr()
    need(abs(settled - normal) < 1e-6, "homed and enabled %.1f s: limit %.3f, expected %.3f" % (hold + 0.5, settled, normal))
    L.off()
    time.sleep(0.5)
    need(abs(ferr() - relaxed) < 1e-6, "disabled: limit %.3f, expected %.3f" % (ferr(), relaxed))
    need(L.on(), "machine on failed: %s" % L.errors())
    early = ferr()
    check_swing(ctx, L, "re-enable")
    time.sleep(hold + 0.5)
    late = ferr()
    need(abs(early - relaxed) < 1e-6 and abs(late - normal) < 1e-6,
         "after the enable: %.3f, then %.3f; expected %.3f, then %.3f" % (early, late, relaxed, normal))
    return "limit %.1f mm while phasing, %.1f mm settled; swing %.3f mm" % (relaxed, normal, L.swing)


@test("lcnc", "enable cycles: machine off/on x5, the drive follows, no fault")
def t_enable_cycles(ctx):
    L = lcnc(ctx)
    for i in range(5):
        L.off()
        time.sleep(1.0)
        p = ctx.pins(ctx.a.usb, "fault0.state")
        need(p.get("fault0.state") == 0, "cycle %d off: drive state %s" % (i, p.get("fault0.state")))
        ok = L.on()
        if not ok:
            diag(ctx, "enable cycle %d failed" % i)
        check_swing(ctx, L, "enable cycle %d" % i)
        need(ok, "cycle %d: machine on failed: %s" % (i, L.errors()))
        time.sleep(1.5)
        p = ctx.pins(ctx.a.usb, "fault0.state", "fault0.fault")
        need(p.get("fault0.state") == 1, "cycle %d on: drive state %s fault %s" % (i, p.get("fault0.state"), p.get("fault0.fault")))
    need(L.home(), "not homed")
    errs = L.drive_errors()
    need(not errs, "LinuxCNC: %s" % errs)


def motion_checks(ctx, ok, fmax, vpos, vneg, tag):
    L = lcnc(ctx)
    time.sleep(0.5)
    frest = L.ferror()
    ctx.m(tag + "_ferror_max_mm", fmax)
    ctx.m(tag + "_ferror_rest_mm", frest)
    ctx.m(tag + "_vel_mm_s", [vneg, vpos])
    need(ok, "program did not finish: %s" % L.errors())
    need(fmax <= ctx.a.max_ferror, "following error %.3f mm > %.3f" % (fmax, ctx.a.max_ferror))
    need(frest <= ctx.a.max_ferror_rest, "rest error %.4f mm > %.4f" % (frest, ctx.a.max_ferror_rest))
    need(vpos > 0.9 * ctx.a.vel and -vneg > 0.9 * ctx.a.vel,
         "top speed +%.0f / %.0f mm/s, expected %.0f both ways" % (vpos, vneg, ctx.a.vel))
    errs = L.drive_errors()
    need(not errs, "LinuxCNC: %s" % errs)
    p = ctx.pins(ctx.a.usb, "fault0.state", "fault0.fault")
    need(p.get("fault0.state") == 1, "drive state %s fault %s after the program" % (p.get("fault0.state"), p.get("fault0.fault")))
    return "ferror max %.3f mm, rest %.4f mm, speed %+.0f/%+.0f mm/s" % (fmax, frest, vpos, vneg)


@test("lcnc", "motion program: following error within limits, full speed both ways")
def t_motion(ctx):
    L = ready(ctx)
    ok, fmax, vpos, vneg = L.run_program(os.path.join(HERE, "hwtest.ngc"))
    return motion_checks(ctx, ok, fmax, vpos, vneg, "motion")


@test("lcnc", "smart-serial console traffic and a --dump while the program runs")
def t_motion_console(ctx):
    # One console at a time: both get all of the drive's output, so USB
    # traffic now would end up in the smart-serial answers and back.
    L = ready(ctx)
    base = baseline(ctx)
    answers = [0, 0]
    dumped = [None]
    stop = threading.Event()

    def traffic():  # in a thread, so the motion sampling keeps its pace
        dumped[0] = dump(ctx, ctx.a.con, "moving-con.conf")
        while not stop.is_set():
            rc, out = ctx.cli(ctx.a.con, "-c", "hv0.dc_volt", "-c", "fault0.state", quiet=0.3, timeout=20)
            answers[0] += 1
            answers[1] += bool(re.search(r"fault0\.state = 1", out))
            stop.wait(0.3)
    th = threading.Thread(target=traffic)
    th.start()
    try:
        ok, fmax, vpos, vneg = L.run_program(os.path.join(HERE, "hwtest.ngc"))
    finally:
        stop.set()
        th.join(60)
    detail = motion_checks(ctx, ok, fmax, vpos, vneg, "motion_console")
    need(dumped[0] == base, "smart-serial dump during motion differs from the baseline")
    need(answers[0] >= 3 and answers[1] == answers[0], "smart-serial answers right: %d of %d" % (answers[1], answers[0]))
    return "dump + %d queries; %s" % (answers[0], detail)


@test("lcnc", "capture scope over smart-serial: trigger on a rapid at t = 0")
def t_capture_motion(ctx):
    L = ready(ctx)
    need(L.mdi("G0 X-300", 30), "move to X-300")
    csv = os.path.join(ctx.dir, "capture-motion.csv")
    p = subprocess.Popen([CLI, "-d", ctx.a.con, "-t", "1", "-c", "scope0.nch = 2", "-c", "scope0.ch0 = vel1.vel",
                          "-c", "scope0.ch1 = hv0.iq_fb", "-c", "scope0.trig = vel1.vel", "-c", "scope0.level = 50",
                          "-c", "scope0.edge = 0", "-c", "scope0.pre = 0.25", "-c", "scope0.mode = 0",
                          "--capture", csv, "--capture-timeout", "90"],
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, cwd=ctx.dir)
    time.sleep(5.0)  # set up and armed over the slow link
    need(L.mdi("G0 X300", 30), "rapid to X300")
    out, _ = p.communicate(timeout=150)
    ctx.note(out)
    need(p.returncode == 0 and os.path.exists(csv), out[-300:])
    rows = [r.split(",") for r in open(csv).read().split()]
    need(len(rows) == 2049 and rows[0] == ["t", "vel1.vel", "hv0.iq_fb"], "%d rows %r" % (len(rows), rows[:1]))
    data = [[float(x) for x in r] for r in rows[1:]]
    cross = next((data[i][0] for i in range(1, len(data)) if data[i - 1][1] < 50 <= data[i][1]), None)
    need(cross is not None and abs(cross) <= 0.0005, "speed crosses 50 rad/s at t = %s" % cross)
    ctx.m("capture_vel_max", max(d[1] for d in data))
    need(L.mdi("G0 X0", 30), "back to X0")
    return "crossing at t = %g s, peak %.0f rad/s" % (cross, ctx.meas["capture_vel_max"])


@test("lcnc", "power field under motion: rises while moving, agrees with the drive")
def t_power_motion(ctx):
    L = ready(ctx)
    peak = [0.0]

    def during():
        peak[0] = max(peak[0], L.pin("power"))
    ok, fmax, vpos, vneg = L.run_program(os.path.join(HERE, "hwtest.ngc"), during=during)
    need(ok, "program did not finish")
    ctx.m("power_peak_w", peak[0])
    need(peak[0] >= ctx.meas.get("power_idle_w", 0), "power peak %.0f W below idle" % peak[0])
    p = ctx.pins(ctx.a.usb, "pe0.p_in")
    need(abs(L.pin("power") - p.get("pe0.p_in", -999)) <= 25, "pin %.0f W, drive %s W at rest" % (L.pin("power"), p.get("pe0.p_in")))
    return "peak %.0f W during the program" % peak[0]


@test("lcnc", "rt load under LinuxCNC: HAL_OK2, worst case recorded")
def t_rt(ctx):
    h = hal_info(ctx, ctx.a.usb)
    need(h["state"] == "HAL_OK2", "HAL state %s" % h["state"])
    if h["rt_max"] is None:
        return "no timing on the sim"
    ctx.m("lcnc_rt_max_us", h["rt_max"])
    ctx.m("lcnc_frt_max_us", h["frt_max"])
    need(h["rt_max"] < ctx.a.rt_fail * h["rt_period"], "rt max %.1f us of %.0f" % (h["rt_max"], h["rt_period"]))
    note = " (above %.0f%%: see DEBUGNOTES rt worst case)" % (ctx.a.rt_note * 100) if h["rt_max"] > ctx.a.rt_note * h["rt_period"] else ""
    return "rt max %.1f us, frt max %.1f us%s" % (h["rt_max"], h["frt_max"] or 0, note)


@test("lcnc", "Pi side: servo thread and the card's read/write times (SPI driver)", targets=("hw",))
def t_host_timing(ctx):
    """LinuxCNC's own timing of the servo thread and hostmot2's read and
    write functions (rtapi clocks: ns on the Pi), recorded to compare SPI
    drivers and clocks (hwtest/burnin/stmbl.ini [HOSTMOT2])."""
    board = ctx.a.prefix.split(".")[0] + ".0"
    def getp(name):
        r = subprocess.run(["halcmd", "getp", name], capture_output=True, text=True, timeout=10)
        try:
            return int(r.stdout.strip())
        except ValueError:
            return None

    # .time is the last period's: average 50 samples over ~5 s; .tmax is
    # the largest since it was last cleared: cleared here, read after 10 s
    # of running (start-up excluded)
    for part in ("servo-thread", board + ".read", board + ".write"):
        subprocess.run(["halcmd", "setp", part + ".tmax", "0"], timeout=10)
    time.sleep(10.0)
    vals = {}
    for part in ("servo-thread", board + ".read", board + ".write"):
        samples = []
        for _ in range(50):
            v = getp(part + ".time")
            if v is not None:
                samples.append(v)
            time.sleep(0.1 / 3)
        vals[part + ".time"] = int(sum(samples) / len(samples)) if samples else None
        vals[part + ".tmax"] = getp(part + ".tmax")
    ctx.note("host timing %r" % vals)
    drv = "%s %s" % hm2_driver.driver()
    for k, v in vals.items():  # host_servo_tmax, host_read_time, ...
        part, what = k.rsplit(".", 1)
        ctx.m("host_%s_%s" % ("servo" if part == "servo-thread" else part.rsplit(".", 1)[-1], what), v)
    ctx.m("host_driver", drv)
    need(all(v is not None for v in vals.values()), "missing: %r" % vals)
    us = lambda v: v / 1000.0
    return "%s: servo %.0f us (max %.0f), read %.0f (max %.0f), write %.0f (max %.0f)" % (
        drv, us(vals["servo-thread.time"]), us(vals["servo-thread.tmax"]), us(vals[board + ".read.time"]),
        us(vals[board + ".read.tmax"]), us(vals[board + ".write.time"]), us(vals[board + ".write.tmax"]))


@test("lcnc", "end: machine off disables the drive; no reboot, no crash, config unchanged")
def t_end(ctx):
    L = lcnc(ctx)
    L.off()
    time.sleep(1.0)
    p = ctx.pins(ctx.a.usb, "fault0.state")
    need(p.get("fault0.state") == 0, "drive state %s after machine off" % p.get("fault0.state"))
    up = uptime_s(ctx, ctx.a.usb)
    start = ctx.meas.get("uptime_start")
    need(up is not None and start is not None and up >= start, "uptime %s, at the start %s: rebooted" % (up, start))
    if ctx.a.target == "hw":
        rc, out = ctx.cli(ctx.a.usb, "-c", "crash")
        need("no crash" in out, out[-200:])
    got = dump(ctx, ctx.a.usb, "end.conf")
    need(got == baseline(ctx), "config changed")


# --------------------------------------------------------------------------
# ebrake phase: deliberate stops while moving (docs/emergency_stop.md). Run
# on its own: run_hwtest.sh hw --phase-only ebrake [--ebrake-speeds ...].
# Each case: rapid to the start, a G1 move at the speed, the cause fired at
# cruise, the drive's scope captures it (all over USB: arm, cause, download).

MM_PER_RAD = 10.0 / (2.0 * math.pi)  # the burn-in axis: 10 mm a revolution

# The trigger is the drive's own state (fault0.state: 0 disabled, 1
# enabled, 3 soft fault, 4 hard fault): leaving ENABLED is when the stop
# starts, whatever the cause.
EBRAKE = {  # kind: (what, trigger pin, edge 0 rising / 1 falling, level, stop mode)
    "off":  ("machine off (enable dropped)", "fault0.state", 1, 0.5, 1),
    "link": ("smart-serial link lost (the card's port stopped)", "fault0.state", 0, 2.5, 1),
    "fb":   ("feedback fault (fault0.mot_fb_error forced)", "fault0.state", 0, 2.5, 2),
    # the F4 reports the HV timeout and LinuxCNC switches off (state 0):
    # trigger on the drop itself
    "f3":   ("the F4 gone for the F3 (hv0.drop_link)", "hv0.drop_link", 0, 0.5, 0),
}


def ebrake_case(ctx, kind, v):
    what, trig, edge, level, mode = EBRAKE[kind]
    a = ctx.a
    board = a.prefix.split(".")[0] + ".0"
    L = ready(ctx)
    need(L.mdi("G0 X-250", 30), "rapid to X-250")
    time.sleep(0.5)
    csv = os.path.join(ctx.dir, "ebrake-%s-%g.csv" % (kind, v))
    # 4 channels x 1024 samples, every 3rd rt period: 0.61 s, 10 % before
    rc, out = ctx.cli(a.usb, "-c", "scope0.nch = 4", "-c", "scope0.ch0 = enc_fb0.pos", "-c", "scope0.ch1 = hv0.iq_fb",
                      "-c", "scope0.ch2 = hv0.dc_volt", "-c", "scope0.ch3 = fault0.stop", "-c", "scope0.div = 3",
                      "-c", "scope0.trig = %s" % trig, "-c", "scope0.edge = %d" % edge,
                      "-c", "scope0.level = %g" % level, "-c", "scope0.pre = 0.1", "-c", "scope0.mode = 0",
                      "-c", "scopearm", "-c", "scopestat")
    need("armed" in out, "scope not armed: %s" % out[-200:])
    L.c.mode(L.lc.MODE_MDI)
    L.c.wait_complete()
    L.c.mdi("G1 X250 F%d" % int(v * 60))
    # fire at cruise, past X-100 (stopping takes millimetres; 350 mm left)
    fired = L.wait(lambda s: s.joint[0]["velocity"] > 0.97 * v and s.position[0] > -100, 20, 0.001)
    need(fired, "never at %g mm/s past X-100" % v)
    x0 = L.poll().position[0]
    if kind == "off":
        L.c.state(L.lc.STATE_OFF)
    elif kind == "link":  # no more packets, as a pulled cable; LinuxCNC runs on
        subprocess.run(["halcmd", "setp", board + ".sserial.port-0.run", "0"], timeout=10)
    elif kind == "fb":
        ctx.cli(a.usb, "-c", "fault0.mot_fb_error = 1", quiet=0.2)
    elif kind == "f3":
        ctx.cli(a.usb, "-c", "hv0.drop_link = 1.5", quiet=0.2)
    st = L.poll().task_state
    p = ctx.pins(a.usb, "fault0.state", "fault0.fault", "fault0.en", "fault0.stop", "fault0.stop_timer",
                 "scope0.state", "vel1.vel", quiet=0.2)
    ctx.note("fired %s at X%.1f: LinuxCNC task_state %s, drive %r" % (kind, x0, st, p))
    time.sleep(2.0)
    # put things back
    if kind == "link":
        subprocess.run(["halcmd", "setp", board + ".sserial.port-0.run", "1"], timeout=10)
        time.sleep(2.0)
    if kind == "fb":
        ctx.cli(a.usb, "-c", "fault0.mot_fb_error = enc_fb0.error", quiet=0.2)
    rc, out = ctx.cli(a.usb, "--capture", csv, "--no-arm", "--capture-timeout", "20", timeout=120)
    need(rc == 0 and os.path.exists(csv), "capture: %s" % out[-300:])
    rows = [r.split(",") for r in open(csv).read().split()]
    d = [[float(x) for x in r] for r in rows[1:]]
    t = [r[0] for r in d]
    # the encoder's own angle (enc_fb0.pos, -pi..pi), unwrapped: it keeps
    # counting whatever the drive's state (vel1 stops with the feedback)
    ang = [d[0][1]]
    for r in d[1:]:
        ang.append(ang[-1] + math.remainder(r[1] - ang[-1], 2.0 * math.pi))
    w = [0.0] + [(ang[i] - ang[i - 1]) / (t[i] - t[i - 1]) for i in range(1, len(t))]
    w = [sum(w[max(1, i - 2):i + 3]) / len(w[max(1, i - 2):i + 3]) if i else 0.0 for i in range(len(w))]  # 5 samples
    i0 = next(i for i in range(len(t)) if t[i] >= 0.0)
    w0 = abs(sum(w[i0 - 10:i0]) / 10.0)  # just before the trigger (the encoder may count down)
    # distance from the trigger until the speed is (and stays) below 2 rad/s
    dist, stop_t = 0.0, None
    for i in range(i0 + 1, len(t)):
        if abs(w[i]) < 2.0 and all(abs(x) < 2.0 for x in w[i:i + 20]):
            stop_t = t[i]
            dist = ang[i] - ang[i0]
            break
    ipk = max(abs(r[2]) for r in d[i0:])
    upk = max(r[3] for r in d[i0:])
    modes = sorted(set(int(round(r[4])) for r in d[i0:]))
    mm = abs(dist) * MM_PER_RAD
    tag = "%s_%g" % (kind, v)
    ctx.m("ebrake_%s_mm" % tag, mm)
    ctx.m("ebrake_%s_ms" % tag, stop_t * 1000 if stop_t else None)
    ctx.m("ebrake_%s_ipk" % tag, ipk)
    ctx.m("ebrake_%s_udc" % tag, upk)
    detail = "%.0f rad/s: %.2f mm in %s ms, peak %.1f A, bus %.0f V, stop modes %s" % (
        w0, mm, "%.0f" % (stop_t * 1000) if stop_t else "?", ipk, upk, modes)
    # LinuxCNC halts its command with a machine off: the motor may slow first
    need(abs(w0) > 0.7 * v / MM_PER_RAD, "not at speed at the trigger (%.0f rad/s) " % w0 + detail)
    need(stop_t is not None, "not standing still within the capture: " + detail)
    need(ipk <= 1.3 * a.ebrake_max_cur, "current above 1.3 x %g A: " % a.ebrake_max_cur + detail)
    need(upk < a.ebrake_max_dc, "bus above %g V: " % a.ebrake_max_dc + detail)
    if mode:
        need(mode in modes, "the drive did not run stop mode %d: " % mode + detail)
    # the drive comes back: machine on again, a short move, no crash record
    L.errors()
    ok = False
    for _ in range(5):  # the port may take a moment to come back
        if L.on():
            ok = True
            break
        ctx.note("machine on: task_state %s, %s" % (L.poll().task_state, L.errors()))
        time.sleep(1.0)
    need(ok, "machine on after the stop failed: %s" % L.errors())
    time.sleep(1.0)
    need(L.home(), "not homed after the stop")
    need(L.mdi("G0 X0", 30), "move after the stop")
    rc, out2 = ctx.cli(a.usb, "-c", "crash")
    need("no crash" in out2, "crash record: " + out2[-200:])
    return detail


def ebrake_tests():
    for v in [float(x) for x in EBRAKE_SPEEDS]:
        for kind in EBRAKE_KINDS:
            def f(ctx, kind=kind, v=v):
                return ebrake_case(ctx, kind, v)
            f.__name__ = "t_ebrake_%s_%g" % (kind, v)
            test("ebrake", "ebrake %s at %g mm/s: %s" % (kind, v, EBRAKE[kind][0]), targets=("hw",))(f)


EBRAKE_SPEEDS = (os.environ.get("EBRAKE_SPEEDS") or "50,150,300").split(",")
EBRAKE_KINDS = (os.environ.get("EBRAKE_KINDS") or "off,link,fb,f3").split(",")
ebrake_tests()


# --------------------------------------------------------------------------
# post phase: after LinuxCNC exited


@test("post", "after LinuxCNC: the USB console at full speed (smart-serial link timed out)")
def t_post_speed(ctx):
    # The console's smart-serial side must go quiet when the link times
    # out; it used to stay active and slow every print to ~80 bytes/s.
    time.sleep(1.0)
    drive = os.path.join(ctx.dir, "results-drive.json")
    ref = json.load(open(drive))["meas"].get("usb_dump_s") if os.path.exists(drive) else None
    t0 = time.time()
    got = dump(ctx, ctx.a.usb, "post.conf")
    dt = time.time() - t0
    ctx.m("post_usb_dump_s", dt)
    need(got == baseline(ctx), "config dump after LinuxCNC differs or failed (%.1f s)" % dt)
    limit = 1.5 * ref if ref else 4.0
    need(dt <= limit, "USB dump took %.1f s, %.1f s before LinuxCNC" % (dt, ref or 0))
    h = hal_info(ctx, ctx.a.usb)
    need(h["state"] == "HAL_OK2", "HAL state %s" % h["state"])
    return "dump %.1f s (before LinuxCNC %.1f s)" % (dt, ref or 0)


# --------------------------------------------------------------------------
# runner and report


def run_phase(ctx, phase):
    a = ctx.a
    results = []
    for ph, name, targets, f in TESTS:
        if ph != phase:
            continue
        if a.target not in targets:
            results.append({"name": name, "status": "skip", "detail": "not on %s" % a.target, "s": 0})
            continue
        if a.only and not any(o in f.__name__ or o in name for o in a.only.split(",")):
            continue
        ctx.note("=== %s" % name)
        t0 = time.time()
        try:
            detail = f(ctx) or ""
            status = "ok"
        except Fail as e:
            status, detail = "fail", str(e)
        except Skip as e:
            status, detail = "skip", str(e)
        except Exception as e:
            status, detail = "fail", "exception: %r" % e
            ctx.note(traceback.format_exc())
        dt = time.time() - t0
        ctx.note("--- %s %s (%.1f s) %s" % (status, name, dt, detail))
        print("%-4s %-75s %5.1fs  %s" % (status, name[:75], dt, detail), flush=True)
        results.append({"name": name, "status": status, "detail": detail, "s": round(dt, 1)})
        if status == "fail" and phase == "lcnc" and a.target == "hw" and "LinuxCNC up" in name:
            break  # nothing else can work without the link
    out = {"phase": phase, "target": a.target, "results": results, "meas": ctx.meas}
    json.dump(out, open(os.path.join(ctx.dir, "results-%s.json" % phase), "w"), indent=1)
    return results


def report(a):
    phases = []
    for ph in ("drive", "lcnc", "ebrake", "post"):
        p = os.path.join(a.out, "results-%s.json" % ph)
        if os.path.exists(p):
            phases.append(json.load(open(p)))
    results = [(p["phase"], r) for p in phases for r in p["results"]]
    meas = {}
    for p in phases:
        meas.update(p["meas"])
    n_fail = sum(1 for _, r in results if r["status"] == "fail")
    n_ok = sum(1 for _, r in results if r["status"] == "ok")
    tap = []
    for i, (ph, r) in enumerate(results, 1):
        word = {"ok": "ok", "fail": "not ok", "skip": "ok"}[r["status"]]
        skip = " # SKIP %s" % r["detail"] if r["status"] == "skip" else ""
        tap.append("%s %d - [%s] %s%s" % (word, i, ph, r["name"], skip))
        if r["status"] == "fail":
            tap.append("#   %s" % r["detail"])
    tap.append("1..%d" % len(results))
    tap.append("# %d passed, %d failed, %d skipped" % (n_ok, n_fail, len(results) - n_ok - n_fail))
    open(os.path.join(a.out, "results.tap"), "w").write("\n".join(tap) + "\n")
    md = ["# Hardware test: %s" % os.path.basename(a.out), "",
          "Result: **%s** (%d passed, %d failed, %d skipped)" % ("PASS" if n_fail == 0 and results else "FAIL",
                                                                n_ok, n_fail, len(results) - n_ok - n_fail), "",
          "| | phase | test | time | detail |", "|---|---|---|---|---|"]
    for ph, r in results:
        md.append("| %s | %s | %s | %.1f s | %s |" % (r["status"], ph, r["name"], r["s"], str(r["detail"]).replace("|", "/")))
    md += ["", "## Measurements", "", "| | |", "|---|---|"]
    for k, v in meas.items():
        md.append("| %s | %s |" % (k, ("%.4g" % v) if isinstance(v, float) else v))
    open(os.path.join(a.out, "summary.md"), "w").write("\n".join(md) + "\n")
    print("\n".join(tap[-1:]))
    return 1 if n_fail or not results else 0


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--phase", choices=["drive", "lcnc", "ebrake", "post", "report"], required=True)
    p.add_argument("--target", choices=["hw", "sim"], default="hw")
    p.add_argument("--out", required=True, help="run directory")
    p.add_argument("--usb", default="/dev/ttyACM0", help="the drive's USB console (sim: its pty)")
    p.add_argument("--con", default="", help="the smart-serial console pty (sserialterm)")
    p.add_argument("--prefix", default="hm2_7c81.0.stbl.0.0.", help="LinuxCNC pin prefix of the drive")
    p.add_argument("--only", default="", help="comma-separated parts of test names to run")
    p.add_argument("--dumps", type=int, default=5)
    p.add_argument("--min-dc", type=float, default=24.0, help="V, lowest plausible DC bus")
    p.add_argument("--dc-volt", type=float, default=0.0, help="V, expected DC bus (+-5%%; 0: not checked)")
    p.add_argument("--vel", type=float, default=500.0, help="mm/s, the program's top speed")
    p.add_argument("--max-ferror", type=float, default=0.5, help="mm during motion")
    p.add_argument("--max-swing", type=float, default=1.5,
                   help="mm, motor movement at enable (autophasing; below [JOINT_0]PHASING_FERROR)")
    p.add_argument("--max-ferror-rest", type=float, default=0.02, help="mm at rest")
    p.add_argument("--rt-note", type=float, default=0.85, help="note rt max above this part of the period")
    p.add_argument("--rt-fail", type=float, default=0.97, help="fail rt max above this part of the period")
    p.add_argument("--ebrake-max-cur", type=float, default=10.0, help="A, the drive's max_ac_cur (ebrake phase)")
    p.add_argument("--ebrake-max-dc", type=float, default=380.0, help="V, highest DC bus during a stop")
    a = p.parse_args(os.environ.get("HWTEST_ARGS", "").split() if len(sys.argv) <= 1 else None)
    os.makedirs(a.out, exist_ok=True)
    if a.phase == "report":
        return report(a)
    ctx = Ctx(a)
    results = run_phase(ctx, a.phase)
    if a.phase in ("lcnc", "ebrake"):
        try:
            lcnc(ctx).off()
        except Exception:
            pass
    return 1 if any(r["status"] == "fail" for r in results) else 0


if __name__ == "__main__":
    sys.exit(main())
