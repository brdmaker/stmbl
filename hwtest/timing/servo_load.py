#!/usr/bin/env python3
"""The servo thread under a real five-axis load: LinuxCNC with the HSM 400U's
kinematics (xyzbc-trt-kins, the pivots of brdmot's hwtest/hsm400u-sim), five joints
looped back (motor-pos-cmd -> motor-pos-fb: no motors, no drives enabled),
the hostmot2 card's read and write in the thread with the drives exchanging
smart-serial packets, and programs running in a loop: tool-centre-point
moves, a five-axis line, a circle at B 20 / C 45, and a dense circle of
0.05 mm lines at 8000 mm/min with line joining off (the planner's worst case).
servo_rate.py measures the thread idle; this one under motion.

    servo_load.py [--driver NAME --params "..."] [--config "..."] [--periods US,US..]
                  [--seconds S] [--out FILE]

    # the Zynq (bitstream loaded first):
    servo_load.py --driver hm2_soc_ol --config "num_encoders=0 num_pwmgens=0 num_stepgens=0 sserial_port_0=00000000"
    # LinuxCNC's own load only, no card:
    servo_load.py --driver none

Per period (default 1000, 500, 333, 250 us; 300 s each): the servo thread's
average and largest time, the card's read and write, programs completed,
rtapi's "unexpected realtime delay" messages, smart-serial errors in the
log. Writes servo_load.md. Run as the machine's user with LinuxCNC stopped;
needs `linuxcnc` (installed, or a run-in-place tree via LINUXCNC=path).

Internally the same file is LinuxCNC's DISPLAY (--display): machine on,
home (absolute: no motion), the programs until the time is up, then exit.
"""
import argparse
import json
import math
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

HERE = os.path.abspath(os.path.dirname(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))

TRT = "x-rot-point 200|y-rot-point 160|z-rot-point -450|x-offset 0|z-offset -80|tool-offset 100"  # hsm400u-sim/hsm.hal

HEAD = "G21 G90 G17 G64 P0.01\nG0 Z-120\nG0 B0 C0\n"
TAIL = "G0 Z-120\nG0 B0 C0\nM2\n"
PROGRAMS = {  # brdmot's hwtest/hsm400u-sim/hsm_test.py's, and a dense circle (hsm_blocks.py)
    "rtcp": "G0 X230 Y180\nG1 Z-300 F1000\nG1 B30 C90 F1800\nG1 B0 C180 F1800\n",
    "wline": "G0 X210 Y160\nG1 Z-300 F1000\nG1 X250 B20 C60 F600\n",
    "circle": "G0 B20 C45\nG0 X240 Y160\nG1 Z-300 F1000\nG2 X240 Y160 I-10 J0 F3000\n",
}


def dense_circle(seg=0.05, r=20.0, cx=230.0, cy=160.0):
    n = int(round(2 * math.pi * r / seg))
    pts = ["G1 X%.4f Y%.4f" % (cx + r * math.cos(2 * math.pi * k / n), cy + r * math.sin(2 * math.pi * k / n)) for k in range(n + 1)]
    return "G21 G90 G17 G64 P0.01 Q0\nG0 Z-120\nG0 B0 C0\nG0 X%.4f Y%.4f\nG1 Z-300 F1000\nF8000\n" % (cx + r, cy) + "\n".join(pts[1:]) + "\n" + TAIL


INI = """[EMC]
VERSION = 1.1
MACHINE = servo_load (HSM 400U kinematics, joints looped back)
[DISPLAY]
DISPLAY = {display}
CYCLE_TIME = 0.100
POSITION_OFFSET = RELATIVE
POSITION_FEEDBACK = ACTUAL
MAX_LINEAR_VELOCITY = 133.333
[RS274NGC]
PARAMETER_FILE = load.var
[EMCMOT]
EMCMOT = motmod
COMM_TIMEOUT = 1.0
SERVO_PERIOD = {ns}
[TASK]
TASK = milltask
CYCLE_TIME = 0.010
[HAL]
HALFILE = load.hal
[TRAJ]
COORDINATES = X Y Z B C
LINEAR_UNITS = mm
ANGULAR_UNITS = degree
DEFAULT_LINEAR_VELOCITY = 50
MAX_LINEAR_VELOCITY = 133.333
DEFAULT_LINEAR_ACCELERATION = 4000
MAX_LINEAR_ACCELERATION = 4000
[EMCIO]
EMCIO = io
CYCLE_TIME = 0.100
TOOL_TABLE = tool.tbl
[KINS]
KINEMATICS = xyzbc-trt-kins
JOINTS = 5
[AXIS_X]
MIN_LIMIT = -400
MAX_LIMIT = 800
MAX_VELOCITY = 450
MAX_ACCELERATION = 4000
[AXIS_Y]
MIN_LIMIT = -400
MAX_LIMIT = 640
MAX_VELOCITY = 450
MAX_ACCELERATION = 4000
[AXIS_Z]
MIN_LIMIT = -800
MAX_LIMIT = 200
MAX_VELOCITY = 450
MAX_ACCELERATION = 4000
[AXIS_B]
MIN_LIMIT = -110
MAX_LIMIT = 110
MAX_VELOCITY = 80
MAX_ACCELERATION = 600
[AXIS_C]
MIN_LIMIT = -99999
MAX_LIMIT = 99999
MAX_VELOCITY = 120
MAX_ACCELERATION = 900
"""
JOINT = """[JOINT_{j}]
TYPE = {typ}
MIN_LIMIT = {lo}
MAX_LIMIT = {hi}
MAX_VELOCITY = {v}
MAX_ACCELERATION = {a}
FERROR = 1
MIN_FERROR = 0.2
HOME = 0
HOME_SEQUENCE = 0
HOME_ABSOLUTE_ENCODER = 2
"""
JOINTS = [("LINEAR", -1, 401, 470, 4400), ("LINEAR", -1, 241, 470, 4400), ("LINEAR", -351, 1, 470, 4400),
          ("ANGULAR", -110, 110, 85, 660), ("ANGULAR", -99999, 99999, 125, 1000)]


def sh(*cmd, timeout=15):
    return subprocess.run(list(cmd), capture_output=True, text=True, timeout=timeout)


def getp(name):
    r = sh("halcmd", "getp", name)
    try:
        return int(float(r.stdout.strip()))
    except ValueError:
        return None


def probe_board(driver, params):
    """the card's HAL prefix (hm2_xxx.0), from a short halrun session"""
    sh("halrun", "-U", timeout=30)
    d = tempfile.mkdtemp(prefix="servo-load-probe.")
    hal = os.path.join(d, "p.hal")
    open(hal, "w").write("loadrt hostmot2\nloadrt %s %s\nshow funct\n" % (driver, params))
    out = sh("halrun", "-f", hal, timeout=60).stdout
    sh("halrun", "-U", timeout=30)
    m = re.search(r"(hm2_\w+\.\d+)\.read\b", out)
    return m.group(1) if m else None


def write_config(d, a, period_us, board):
    ns = int(round(period_us * 1000))
    # LinuxCNC runs DISPLAY's first word with "-ini FILE" right after it: a wrapper
    disp = os.path.join(d, "display.sh")
    open(disp, "w").write('#!/bin/sh\nexec "%s" "%s" --display "$@"\n' % (sys.executable, os.path.abspath(__file__)))
    os.chmod(disp, 0o755)
    ini = INI.format(display=disp, ns=ns)
    for j, (typ, lo, hi, v, acc) in enumerate(JOINTS):
        ini += JOINT.format(j=j, typ=typ, lo=lo, hi=hi, v=v, a=acc)
    open(os.path.join(d, "load.ini"), "w").write(ini)
    hal = ["loadrt [KINS]KINEMATICS",
           "loadrt [EMCMOT]EMCMOT servo_period_nsec=[EMCMOT]SERVO_PERIOD num_joints=[KINS]JOINTS"]
    hal += ["setp xyzbc-trt-kins.%s" % p.replace(" ", " ", 1) for p in TRT.split("|")]
    if board:
        params = (a.params + (' config="%s"' % a.config if a.config else "")).strip()
        hal += ["loadrt hostmot2", "loadrt %s %s" % (a.driver, params), "addf %s.read servo-thread" % board]
    hal += ["addf motion-command-handler servo-thread", "addf motion-controller servo-thread"]
    if board:
        hal += ["addf %s.write servo-thread" % board]
    for j in range(5):
        hal += ["net j%d-pos joint.%d.motor-pos-cmd => joint.%d.motor-pos-fb" % (j, j, j)]
    hal += ["net estop-loop iocontrol.0.user-enable-out => iocontrol.0.emc-enable-in",
            "net tool-prep-loop iocontrol.0.tool-prepare => iocontrol.0.tool-prepared",
            "net tool-change-loop iocontrol.0.tool-change => iocontrol.0.tool-changed"]
    open(os.path.join(d, "load.hal"), "w").write("\n".join(hal) + "\n")
    open(os.path.join(d, "load.var"), "w").write("")
    open(os.path.join(d, "tool.tbl"), "w").write("T1 P1 Z100 D6 ;100 mm tool\n")
    progs = []
    for name, body in PROGRAMS.items():
        p = os.path.join(d, name + ".ngc")
        open(p, "w").write(HEAD + body + TAIL)
        progs.append(p)
    p = os.path.join(d, "dense.ngc")
    open(p, "w").write(dense_circle())
    progs.append(p)
    return progs


# --- LinuxCNC's DISPLAY: runs the programs, measures, exits

def display():
    import linuxcnc
    S, C, E = linuxcnc.stat(), linuxcnc.command(), linuxcnc.error_channel()
    out, secs, board = os.environ["LOAD_OUT"], float(os.environ["LOAD_SECONDS"]), os.environ.get("LOAD_BOARD", "")
    progs = os.environ["LOAD_PROGRAMS"].split()
    res = {"programs": 0, "errors": []}

    def wait(pred, t):
        end = time.time() + t
        while time.time() < end:
            S.poll()
            if pred(S):
                return True
            time.sleep(0.02)
        return False

    def errs():
        while True:
            e = E.poll()
            if not e:
                return
            res["errors"].append(e[1].strip()[:200])

    try:
        C.state(linuxcnc.STATE_ESTOP_RESET)
        C.state(linuxcnc.STATE_ON)
        if not wait(lambda s: s.task_state == linuxcnc.STATE_ON, 10):
            raise RuntimeError("machine did not turn on")
        C.mode(linuxcnc.MODE_MANUAL)
        C.teleop_enable(0)
        C.home(-1)
        if not wait(lambda s: all(s.homed[j] for j in range(s.joints)), 20):
            raise RuntimeError("not homed")
        C.mode(linuxcnc.MODE_MDI)
        wait(lambda s: s.task_mode == linuxcnc.MODE_MDI, 5)
        C.mdi("T1 M6 G43")
        wait(lambda s: s.interp_state == linuxcnc.INTERP_IDLE, 10)
        time.sleep(1.0)
        parts = ["servo-thread"] + ([board + ".read", board + ".write"] if board else [])
        for p in parts:
            sh("halcmd", "setp", p + ".tmax", "0")
        samples = {p: [] for p in parts}
        end = time.time() + secs
        i = 0
        while time.time() < end:
            C.mode(linuxcnc.MODE_AUTO)
            wait(lambda s: s.task_mode == linuxcnc.MODE_AUTO, 5)
            C.program_open(progs[i % len(progs)])
            C.auto(linuxcnc.AUTO_RUN, 0)
            wait(lambda s: s.interp_state != linuxcnc.INTERP_IDLE, 5)
            while time.time() < end + 120:
                S.poll()
                if S.interp_state == linuxcnc.INTERP_IDLE:
                    break
                for p in parts:
                    v = getp(p + ".time")
                    if v is not None:
                        samples[p].append(v)
                time.sleep(0.2)
            errs()
            if S.task_state != linuxcnc.STATE_ON:
                raise RuntimeError("machine went off: %s" % "; ".join(res["errors"][-3:]))
            res["programs"] += 1
            i += 1
        for p in parts:
            k = "servo" if p == "servo-thread" else p.rsplit(".", 1)[-1]
            s = samples[p]
            res[k + "_avg"] = sum(s) / len(s) / 1000.0 if s else None
            t = getp(p + ".tmax")
            res[k + "_max"] = t / 1000.0 if t is not None else None
    except Exception as e:  # noqa: BLE001 - reported in the results
        errs()
        res["error"] = str(e)
    json.dump(res, open(os.path.join(out, "result.json"), "w"))
    try:
        C.state(linuxcnc.STATE_ESTOP)
    except Exception:  # noqa: BLE001
        pass
    return 0


# --- the runner

def facts():
    try:
        import servo_rate
        return servo_rate.facts()
    except Exception:  # noqa: BLE001
        return os.uname().release


def main():
    if "--display" in sys.argv:
        return display()
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--driver", default="none", help="hm2_soc_ol, hm2_pci, hm2_spix ... (none: LinuxCNC only)")
    ap.add_argument("--params", default="", help="driver parameters")
    ap.add_argument("--config", default="sserial_port_0=00000000", help="hostmot2 config string")
    ap.add_argument("--periods", default="1000,500,333,250")
    ap.add_argument("--seconds", type=float, default=300.0)
    ap.add_argument("--exchange-us", type=float, default=150.0)
    ap.add_argument("--out", default="servo_load.md")
    ap.add_argument("--keep", action="store_true", help="keep the run folders (logs)")
    a = ap.parse_args()
    if os.geteuid() == 0:
        sys.exit("servo_load: not as root; run as the machine's user")
    sys.path.insert(0, HERE)
    info = facts()
    print(info)
    board = None
    if a.driver != "none":
        board = probe_board(a.driver, (a.params + (' config="%s"' % a.config if a.config else "")).strip())
        if not board:
            sys.exit("servo_load: %s found no hostmot2 board (bitstream loaded? config?)" % a.driver)
        print("board", board)
    lcnc = os.environ.get("LINUXCNC")
    pre = ". %s/scripts/rip-environment >/dev/null && " % lcnc if lcnc else ""
    rows = []
    for pu in (float(x) for x in a.periods.split(",")):
        d = tempfile.mkdtemp(prefix="servo-load.")
        progs = write_config(d, a, pu, board)
        env = dict(os.environ, LOAD_OUT=d, LOAD_SECONDS=str(a.seconds), LOAD_BOARD=board or "", LOAD_PROGRAMS=" ".join(progs))
        cmd = "%scd %s && timeout %d linuxcnc load.ini > linuxcnc.log 2>&1" % (pre, d, int(a.seconds + 300))
        subprocess.run(["bash", "-c", cmd], env=env)
        try:
            r = json.load(open(os.path.join(d, "result.json")))
        except (OSError, ValueError):
            r = {"error": "no result (see %s/linuxcnc.log)" % d}
        log = open(os.path.join(d, "linuxcnc.log"), errors="replace").read()
        r["rt_delays"] = len(re.findall(r"[Uu]nexpected realtime delay", log))
        r["sserial_errors"] = len(re.findall(r"sserial.*(?:[Ee]rror|[Ff]ault|[Tt]imeout)", log))
        r["period"] = pu
        rows.append(r)
        f = lambda k: "%.1f / %.1f" % (r[k + "_avg"], r[k + "_max"]) if r.get(k + "_avg") is not None else "-"
        used = (r["servo_max"] + a.exchange_us) / pu if r.get("servo_max") is not None else None
        r["verdict"] = "-" if used is None else "%s (%d %%)" % ("fits" if used < 0.8 else "tight" if used < 1.0 else "no", round(used * 100))
        print("%6.0f us (%4.0f Hz): servo %s  read %s  write %s  programs %d  rt delays %d  sserial errors %d  -> %s%s" % (
            pu, 1e6 / pu, f("servo"), f("read"), f("write"), r.get("programs", 0), r["rt_delays"], r["sserial_errors"], r["verdict"],
            ("  ERROR " + r["error"]) if r.get("error") else ""), flush=True)
        if not a.keep and not r.get("error"):
            shutil.rmtree(d, ignore_errors=True)
    md = ["# Servo thread under a five-axis load\n\n%s\n\nDriver `%s %s`, config `%s`, board %s; %g s per period; "
          "xyzbc-trt-kins, 5 joints looped back, programs rtcp / wline / circle / dense (0.05 mm lines, F8000, Q0). "
          "Times in us (average / largest).\n\n" % (info, a.driver, a.params, a.config, board or "none", a.seconds),
          "| period | rate | servo | read | write | programs | rt delays | sserial errors | + %g us exchange |\n"
          "|---|---|---|---|---|---|---|---|---|\n" % a.exchange_us]
    for r in rows:
        f = lambda k: "%.1f / %.1f" % (r[k + "_avg"], r[k + "_max"]) if r.get(k + "_avg") is not None else "-"
        md.append("| %.0f us | %.0f Hz | %s | %s | %s | %d | %d | %d | %s |\n" % (
            r["period"], 1e6 / r["period"], f("servo") if not r.get("error") else r["error"][:60], f("read"), f("write"),
            r.get("programs", 0), r["rt_delays"], r["sserial_errors"], r.get("verdict", "-")))
    open(a.out, "w").write("".join(md))
    print("\n" + a.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
