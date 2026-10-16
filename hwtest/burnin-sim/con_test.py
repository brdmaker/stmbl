#!/usr/bin/env python3
"""LinuxCNC + the simulated drive + sserialterm (make test-console): a
headless DISPLAY program. The drive's console over smart-serial, through
the pty, with servoterm-cli as on a real drive's USB port:

1. about, a pin read, a pin write and read back;
2. --dump: the drive's whole flash config, identical to the file it booted;
3. while the axis moves (MDI rapids), console traffic: no following error,
   no fault, the answers right;
4. the capture scope: servoterm-cli sets it up (-c), arms it and waits
   (--capture) while a rapid starts; the CSV: 2048 rows, the speed crossing
   the trigger level at t = 0;
5. --update (in-band firmware update): refused while the drive is enabled,
   staged and applied once the machine is off.
"""
import os
import subprocess
import sys
import time

import hal
import linuxcnc

S = linuxcnc.stat()
C = linuxcnc.command()
HERE = os.path.dirname(os.path.abspath(__file__))
TTY = os.environ.get("CON_TTY", os.path.join(HERE, "drive-con"))
CLI = os.environ.get("SERVOTERM_CLI", "servoterm-cli")
results = []
INFO = []


def check(cond, name, detail=""):
    results.append((bool(cond), name, detail))


def poll():
    S.poll()
    return S


def wait(pred, timeout, step=0.01):
    t = time.time() + timeout
    while time.time() < t:
        if pred(poll()):
            return True
        time.sleep(step)
    return False


def cli(*args, timeout=60):
    r = subprocess.run([CLI, "-d", TTY, "-t", "1"] + list(args), capture_output=True, text=True, timeout=timeout)
    return r.returncode, r.stdout + r.stderr


def main():
    h = hal.component("con_test")
    h.ready()
    if not wait(lambda s: s.task_state != 0, 20):
        check(False, "LinuxCNC task up")
        return
    time.sleep(1.0)
    t0 = time.time()
    rc, out = cli("-c", "about", "-c", "pe0.p_in")
    INFO.append("# about + a pin over the console: %.1f s" % (time.time() - t0))
    check(rc == 0 and "STMBL simulated drive" in out and "pe0.p_in" in out,
          "servoterm-cli over the smart-serial console: about, a pin", out[-300:])
    rc, out = cli("-c", "pe0.idle = 21", "-c", "pe0.idle")
    check("pe0.idle = 21" in out.replace("21.000000", "21"), "  ...a pin written and read back", out[-200:])

    dump = os.path.join(HERE, "dump.txt")
    t0 = time.time()
    rc, out = cli("--dump", dump, timeout=120)
    conf = open(os.path.join(HERE, "sim.conf")).read()
    got = open(dump).read() if os.path.exists(dump) else ""
    norm = lambda s: [l.rstrip() for l in s.strip().splitlines()]
    INFO.append("# --dump: %d bytes in %.1f s" % (len(got), time.time() - t0))
    check(rc == 0 and norm(got) == norm(conf), "--dump: the drive's whole flash config, as the file it booted",
          "rc %d, %d vs %d lines" % (rc, len(norm(got)), len(norm(conf))))

    C.state(linuxcnc.STATE_ESTOP_RESET)
    C.state(linuxcnc.STATE_ON)
    ok = wait(lambda s: s.task_state == linuxcnc.STATE_ON, 5)
    time.sleep(3.0)
    C.mode(linuxcnc.MODE_MANUAL)
    C.wait_complete()
    C.home(-1)
    wait(lambda s: all(s.homed[j] for j in range(3)), 30)
    C.mode(linuxcnc.MODE_MDI)
    C.wait_complete()
    C.mdi("G0 X300")
    time.sleep(0.2)
    answers = 0
    for i in range(3):
        rc, out = cli("-c", "fault0.state", "-c", "about")
        answers += "STMBL simulated drive" in out
        C.mdi("G0 X%d" % (0 if i % 2 == 0 else 300))
    wait(lambda s: s.interp_state == linuxcnc.INTERP_IDLE and s.queue == 0, 20)
    s = poll()
    err = linuxcnc.error_channel().poll()
    check(ok and s.task_state == linuxcnc.STATE_ON and answers == 3 and not g_fault(),
          "console traffic while the axis moves: answers right, machine still on, no drive fault",
          "task %d answers %d err %s" % (s.task_state, answers, err))
    # the capture scope over the console
    csv = os.path.join(HERE, "capture.csv")
    t0 = time.time()
    p = subprocess.Popen([CLI, "-d", TTY, "-t", "1", "-c", "scope0.nch = 2", "-c", "scope0.ch0 = hv0.sim_vel",
                          "-c", "scope0.ch1 = hv0.iq_fb", "-c", "scope0.trig = hv0.sim_vel", "-c", "scope0.level = 50",
                          "-c", "scope0.pre = 0.25", "-c", "scope0.mode = 0", "--capture", csv, "--capture-timeout", "90"],
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    time.sleep(4.0)  # set up and armed
    C.mdi("G0 X%d" % (300 if poll().actual_position[0] < 150 else 0))
    out, _ = p.communicate(timeout=120)
    rows = [l.split(",") for l in open(csv).read().split()] if os.path.exists(csv) else []
    INFO.append("# capture: %s" % (out.strip().splitlines()[-1] if out.strip() else "-"))
    ok = p.returncode == 0 and len(rows) == 2049 and rows[0] == ["t", "hv0.sim_vel", "hv0.iq_fb"]
    cross = None
    if ok:
        data = [[float(x) for x in r] for r in rows[1:]]
        sgn = 1 if max(d[1] for d in data) > 50 else -1
        for i in range(1, len(data)):
            if sgn * data[i - 1][1] < 50 <= sgn * data[i][1] or data[i - 1][1] < 50 <= data[i][1]:
                cross = data[i][0]
                break
        INFO.append("# capture: %d rows, dt %g s, speed crosses 50 rad/s at t = %s s, %.1f s in all"
                    % (len(data), data[1][0] - data[0][0], cross, time.time() - t0))
    check(ok and cross is not None and abs(cross) <= 0.0004,
          "capture scope over the console: --capture CSV, 2048 samples, the trigger at t = 0", out[-300:])
    # in-band firmware update over the console: refused while the drive is
    # enabled, staged (and applied: the simulated bootloader copies it) once off
    img = os.path.join(HERE, "fw.bin")
    open(img, "wb").write(bytes((i * 131 + 7) & 0xff for i in range(3000)))
    rc, out = cli("--update", img)
    check(rc != 0 and "drive enabled" in out, "--update refused while the drive is enabled", out[-200:])
    C.state(linuxcnc.STATE_OFF)
    wait(lambda s: s.task_state == linuxcnc.STATE_OFF, 5)
    time.sleep(0.5)
    t0 = time.time()
    rc, out = cli("--update", img, timeout=120)
    INFO.append("# --update: 3000 bytes in %.1f s over the console" % (time.time() - t0))
    check(rc == 0 and "staged 3000 bytes" in out and "applied" in out,
          "--update over the smart-serial console: staged, CRC32 right, applied", out[-300:])
    INFO.append("# sserialterm: %d bytes from the drive, %d to it" % (hal.get_value("sserialterm.0.bytes-in"),
                                                                   hal.get_value("sserialterm.0.bytes-out")))


def g_fault():
    return hal.get_value("stmbl-sim.0.fault")


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        check(False, "exception: %r" % e)
    n_fail = sum(1 for r in results if not r[0])
    out = []
    for i, (ok, name, detail) in enumerate(results, 1):
        out.append("%s %d - %s" % ("ok" if ok else "not ok", i, name))
        if not ok and detail:
            out.append("#   %s" % (detail,))
    out.append("1..%d" % len(results))
    out.append("# %d/%d passed" % (len(results) - n_fail, len(results)))
    text = "\n".join(INFO + out) + "\n"
    sys.stdout.write(text)
    open("con_test.tap", "w").write(text)
    open("con_test.result", "w").write("%d\n" % n_fail)
