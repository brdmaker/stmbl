"""dynwiz backends: how the wizard moves the machine and records it.

Both run a list of moves from the machine's current position and return a
recording at the servo rate (1 kHz):

  t cx cy      the planner's command [s, mm]
  x y1 y2      the scales [mm] (y2: None for a machine with one Y scale)
  ix iy1 iy2   the motors' current [A] (None where not wired)
  tx ty        the true tool point [mm] (simulation only, else None)

Moves: ("to", X, Y, F)  ("rel", "x"|"y", d, F)  ("dwell", s)
       ("circle", CX, CY, R, F, turns)        F in mm/min.

  SimBackend    host/portal/portal_sim on a portal file: no LinuxCNC, fast
                (about 7x real time), for development and tests
  LcncBackend   a running LinuxCNC through its python module (MDI/AUTO)
                and a HAL sampler read with halsampler: the real machine,
                or hwtest/portal-sim's simulated one
"""
import math
import os
import re
import shutil
import subprocess
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))

# the drives' fault codes (shared/common.h)
FAULTS = {1: "CMD_ERROR", 2: "MOT_FB_ERROR", 3: "COM_FB_ERROR", 4: "JOINT_FB_ERROR",
          5: "POS_ERROR (following error)", 6: "SAT_ERROR (current at its limit too long)", 7: "MOT_TEMP_ERROR",
          8: "HV_CRC_ERROR", 9: "HV_TIMEOUT_ERROR", 10: "HV_TEMP_ERROR",
          11: "HV_VOLT_ERROR (DC bus over/under voltage: braking energy? a brake chopper)", 12: "HV_FAULT_ERROR"}

COLS = ("t", "cx", "cy", "x", "y1", "y2", "ix", "iy1", "iy2", "tx", "ty")


class Rec(dict):
    """A recording: column name -> list (or None)."""

    dt = 0.001

    def n(self):
        return len(self["t"])

    def y(self):  # the gantry's Y: the mean of its scales
        if self.get("y2") is None:
            return self["y1"]
        return [0.5 * (a + b) for a, b in zip(self["y1"], self["y2"])]


def trap_time(d, a, v):
    """A trapezoid over d at acceleration a, speed v: (total time, the share
    of it spent accelerating at each end)."""
    d = abs(d)
    if d <= 0:
        return 0.0, 0.25
    ta = v / a
    if d < v * ta:  # a triangle
        ta = math.sqrt(d / a)
        return 2 * ta, 0.5
    T = d / v + ta
    return T, ta / T


def kf_from_conf(conf, period_mm):
    """Force constant [N/A] of a linear (or pinion-driven) motor from its
    drive config: 3/2 polecount psi, per revolution = one period."""
    psi = float(conf.get("conf0.psi", "nan"))
    pp = float(conf.get("conf0.polecount", "1"))
    return 1.5 * pp * psi * 2 * math.pi / (period_mm / 1000.0)


def read_conf(path):
    """A drive config (servoterm-cli --dump, or conf/*.txt): pin = value."""
    out = {}
    for line in open(path):
        line = line.split("#", 1)[0].strip()
        m = re.match(r"^(\S+)\s*=\s*(\S+)", line)
        if m:
            out[m.group(1)] = m.group(2)
    return out


def read_ini(path):
    """A LinuxCNC INI file: {section: {key: value}} (the first of repeated keys)."""
    out, sec = {}, None
    for line in open(path):
        s = line.strip()
        if not s or s[0] in "#;":
            continue
        if s.startswith("["):
            sec = s.strip("[]")
            out.setdefault(sec, {})
            continue
        if "=" in s and sec:
            k, v = s.split("=", 1)
            out[sec].setdefault(k.strip(), v.split("#", 1)[0].strip())
    return out


class Machine:
    """What the wizard knows of the machine before it measures."""

    def __init__(self):
        self.xlim = (0.0, 1000.0)  # where it may go [mm]
        self.ylim = (0.0, 1000.0)
        self.acc = {"x": 5000.0, "y": 5000.0}  # the axes' limits [mm/s^2]
        self.vmax = {"x": 1000.0, "y": 1000.0}  # [mm/s]
        self.rails = 1.2  # Y rail spacing [m]
        self.x_center = 500.0  # machine X of the bridge's centre [mm]
        self.motors = {}  # "x"|"y1"|"y2" -> {"kf": N/A, "period": mm, "conf": path or None}
        self.idler = False  # one Y motor, the other side an idler rail
        self.has_tool = False
        self.name = "machine"


class SimBackend:
    """portal_sim on a portal file. Each recording is a fresh simulation that
    starts at rest at the current position."""

    def __init__(self, portal, sim=None, acc=None, vmax=None, extra=()):
        self.extra = list(extra)  # portal_sim lines before the drives are enabled (what-if settings)
        self.portal = os.path.abspath(portal)
        self.sim = sim or os.path.join(ROOT, "host", "portal", "portal_sim")
        if not os.access(self.sim, os.X_OK):
            raise RuntimeError("%s not built: make -C host/portal" % self.sim)
        self.comp = []  # portal_sim comp lines for the next recordings
        m = Machine()
        m.name = os.path.splitext(os.path.basename(portal))[0]
        m.has_tool = True
        d = os.path.dirname(self.portal)
        self.home = None
        self.truth = {}
        for line in open(self.portal):
            if line.startswith("#@dynwiz"):  # the limits LinuxCNC's INI would have: #@dynwiz ACC_X ACC_Y VMAX
                w = line.split()
                m.acc = {"x": float(w[1]), "y": float(w[2])}
                m.vmax = {"x": float(w[3]), "y": float(w[3])}
                continue
            w = line.split("#", 1)[0].split()
            if not w:
                continue
            if w[0] == "drive" and len(w) >= 3 and w[2] == "none":
                m.motors.pop(w[1], None)
                m.idler = True
            elif w[0] == "drive" and len(w) >= 5:
                conf = os.path.join(d, w[2])
                c = read_conf(conf)
                m.motors[w[1]] = {"kf": kf_from_conf(c, float(w[4])), "period": float(w[4]), "conf": conf,
                                  "pp": float(c.get("conf0.polecount", 1))}
            elif w[0] == "stroke":
                ymin, ymax, xmin, xmax = map(float, w[1:5])
                # the end stops; the wizard keeps 10 mm off them
                m.ylim, m.xlim = (ymin + 10, ymax - 10), (xmin + 10, xmax - 10)
            elif w[0] == "origin":
                m.x_center = float(w[1])
                self.truth["origin"] = (float(w[1]), float(w[2]))
            elif w[0] == "rails":
                m.rails = float(w[1])
            elif w[0] == "home":
                self.home = (float(w[1]), float(w[2]))
            elif w[0] == "head":
                self.truth["head"] = dict(zip(("m", "j", "fx", "fy_end", "fy_mid", "zeta"), map(float, w[1:7])))
            elif w[0] in ("bridge", "carriage"):
                self.truth[w[0]] = float(w[1])
            elif w[0] == "cogging":
                self.truth.setdefault("cogging", {})[w[1]] = dict(zip(("a", "period", "phase"), map(float, w[2:5])))
            elif w[0] in ("guidey", "guidex", "guidey2"):
                self.truth[w[0]] = (float(w[1]), float(w[2]))
        if acc:
            m.acc = {"x": acc, "y": acc}
        if vmax:
            m.vmax = {"x": vmax, "y": vmax}
        if self.home is None:
            self.home = (0.5 * sum(m.xlim), 0.5 * sum(m.ylim))
        self.m = m
        self.pos = list(self.home)
        self.last_out = ""

    def describe(self):
        return "simulation: %s (portal_sim)" % self.portal

    def position(self):
        return tuple(self.pos)

    def set_comp(self, model):
        """pcomp on (model: dict as the session's) or off (None)."""
        if model is None:
            self.comp = []
            return
        self.comp = ["comp model %g %g %g %g" % (model["fx"], model["fy_end"], model["fy_mid"], model["zeta"]),
                     "comp jerk %g" % model.get("jerk_ms", 20), "comp shaper 1", "comp defl 1", "comp shrink 1"]
        if model.get("servo_tau2"):
            self.comp.append("comp servo %g" % model["servo_tau2"])
        # pcomp's beam model uses the portal's own rails and x0; a model
        # identified elsewhere would need them too (it is the plant's here)

    def record(self, moves):
        a = self.m.acc
        v = self.m.vmax
        x, y = self.pos
        lines = ["home %.4f %.4f" % (x, y)] + self.extra + ["run 0.05", "lcnc 1", "run 0.3",
                 # enabled where the machine is (it settled a little): back to the point
                 "tmove 0.3 %.4f %.4f" % (x, y), "run 0.3"] + self.comp
        fd, logf = tempfile.mkstemp(prefix="dynwiz.", suffix=".log")
        os.close(fd)
        lines.append("log %s" % logf)
        lines.append("run 0.05")
        for mv in moves:
            if mv[0] == "dwell":
                lines.append("run %g" % mv[1])
            elif mv[0] in ("to", "rel"):
                if mv[0] == "rel":
                    nx, ny = (x + mv[2], y) if mv[1] == "x" else (x, y + mv[2])
                    f = mv[3]
                else:
                    nx, ny, f = mv[1], mv[2], mv[3]
                d = math.hypot(nx - x, ny - y)
                if d == 0:
                    continue
                # LinuxCNC's limits along the move: the tighter axis
                ux, uy = abs(nx - x) / d, abs(ny - y) / d
                acc = min(a["x"] / ux if ux else 1e18, a["y"] / uy if uy else 1e18)
                vel = min(f / 60.0, v["x"] / ux if ux else 1e18, v["y"] / uy if uy else 1e18)
                T, frac = trap_time(d, acc, vel)
                T = math.ceil(T * 1000.0) / 1000.0
                lines += ["trapfrac %.6f" % frac, "tmove %.3f %.5f %.5f" % (T, nx, ny)]
                x, y = nx, ny
            elif mv[0] == "circle":
                _, cx, cy, r, f, turns = mv
                sx, sy = cx + r, cy
                d = math.hypot(sx - x, sy - y)
                if d > 0:
                    T, frac = trap_time(d, min(a.values()), min(v.values()) / 2)
                    lines += ["trapfrac %.6f" % frac, "tmove %.3f %.5f %.5f" % (math.ceil(T * 1000) / 1000, sx, sy), "run 0.2"]
                vel = f / 60.0
                lines.append("circle %.4f %.4f %.4f %.4f %g" % (2 * math.pi * r * turns / vel, cx, cy, r, turns))
                x, y = sx, sy
        lines += ["log off", "print fault fault_code jammed hits"]
        p = subprocess.run([self.sim, self.portal], input="\n".join(lines) + "\n", capture_output=True, text=True)
        self.last_out = p.stdout + p.stderr
        if p.returncode != 0 or "ERROR" in self.last_out:
            os.unlink(logf)
            raise RuntimeError("portal_sim failed:\n" + self.last_out[-2000:])
        vals = dict(re.findall(r"portal\.(\w+) = (\S+)", self.last_out))
        if float(vals.get("fault", 0)) != 0:
            os.unlink(logf)
            code = int(float(vals.get("fault_code", 0)))
            raise RuntimeError("a drive faulted: %s (%d)" % (FAULTS.get(code, "?"), code))
        rec = Rec((c, []) for c in COLS)
        for line in open(logf):
            w = line.split()
            if len(w) < 11:
                continue
            for i, c in enumerate(COLS):
                rec[c].append(float(w[[0, 1, 2, 5, 6, 7, 8, 9, 10, 3, 4][i]]))
        os.unlink(logf)
        if self.m.idler:
            rec["iy2"] = None
        self.pos = [x, y]
        return rec


class LcncBackend:
    """A running LinuxCNC (the machine on, homed, idle). Its INI's [DYNWIZ]
    section says where the signals are (README.md):

      JOINT_X = 0, JOINT_Y1 = 1, JOINT_Y2 = 2    joints (JOINT_Y2 empty: one Y)
      CUR_X, CUR_Y1, CUR_Y2   the drives' current pins (hm2_*.stbl.N.current)
      FB_X, FB_Y1, FB_Y2      the scales, if not joint.N.motor-pos-fb (behind
                              pcomp: the drives' own pos-fb; an idler's scale)
      TOOL_X, TOOL_Y          a tool point measurement, if there is one
      KF_X, KF_Y1, KF_Y2      force constants [N/A]; or CONF_* (servoterm-cli
                              --dump of the drive) and PERIOD_* [mm]
      SAMPLER                 the sampler instance to use (when the config
                              loads sampler itself; it needs 11 float pins)
      PCOMP                   pcomp's HAL prefix (pcomp.0) if it is loaded
      RAILS, X_CENTER         as [PCOMP]'s
    """

    NCOL = 11

    def __init__(self, ini):
        import hal  # noqa: F401  (LinuxCNC's python: fail early)
        import linuxcnc
        self.lc = linuxcnc
        self.ini_path = os.path.abspath(ini)
        self.ini = read_ini(ini)
        dz = self.ini.get("DYNWIZ", {})
        pc = self.ini.get("PCOMP", {})
        m = Machine()
        m.name = self.ini.get("EMC", {}).get("MACHINE", "machine")
        ax, ay = self.ini.get("AXIS_X", {}), self.ini.get("AXIS_Y", {})
        m.xlim = (float(ax.get("MIN_LIMIT", 0)) + 5, float(ax.get("MAX_LIMIT", 0)) - 5)
        m.ylim = (float(ay.get("MIN_LIMIT", 0)) + 5, float(ay.get("MAX_LIMIT", 0)) - 5)
        m.acc = {"x": float(ax.get("MAX_ACCELERATION", 1000)), "y": float(ay.get("MAX_ACCELERATION", 1000))}
        m.vmax = {"x": float(ax.get("MAX_VELOCITY", 100)), "y": float(ay.get("MAX_VELOCITY", 100))}
        m.rails = float(dz.get("RAILS", pc.get("RAILS", 1.0)))
        m.x_center = float(dz.get("X_CENTER", pc.get("X_CENTER", 0.5 * sum(m.xlim))))
        self.joint = {k: dz.get("JOINT_" + k.upper(), d) for k, d in (("x", "0"), ("y1", "1"), ("y2", "2"))}
        m.idler = self.joint["y2"] == ""
        here = os.path.dirname(self.ini_path)
        for k in ("x", "y1", "y2"):
            K = k.upper()
            kf = dz.get("KF_" + K)
            conf = dz.get("CONF_" + K)
            per = float(dz.get("PERIOD_" + K, 0) or 0)
            if conf:
                conf = os.path.join(here, conf)
            if not kf and conf and per:
                kf = kf_from_conf(read_conf(conf), per)
            if self.joint[k] != "" or kf:
                m.motors[k] = {"kf": float(kf) if kf else None, "period": per or None, "conf": conf}
        if m.idler:
            m.motors.pop("y2", None)
        self.src = []
        for k in ("x", "y1", "y2"):
            j = self.joint[k]
            self.src.append(dz.get("FB_" + k.upper(), "joint.%s.motor-pos-fb" % j if j != "" else ""))
        self.cmd_src = ["joint.%s.motor-pos-cmd" % self.joint["x"], "joint.%s.motor-pos-cmd" % self.joint["y1"]]
        self.cur_src = [dz.get("CUR_" + k, "") for k in ("X", "Y1", "Y2")]
        self.tool_src = [dz.get("TOOL_X", ""), dz.get("TOOL_Y", "")]
        m.has_tool = all(self.tool_src)
        self.sampler = dz.get("SAMPLER")
        self.pcomp = dz.get("PCOMP", "")
        self.m = m
        self.S = linuxcnc.stat()
        self.C = linuxcnc.command()
        self.E = linuxcnc.error_channel()
        self.comp_saved = None
        if self.pcomp and not self.halcmd("show", "pin", self.pcomp + ".shaper", check=False).count(self.pcomp + ".shaper"):
            self.pcomp = ""  # [DYNWIZ] PCOMP, but this HAL file doesn't load it
        self._hal_setup()
        # the wizard measures the machine as it is: pcomp off (but for
        # verify), and back as it was when the wizard ends
        if self.pcomp:
            import atexit
            saved = self.comp_state()
            atexit.register(self.restore_comp, saved)
            self.set_comp(None)

    def describe(self):
        return "LinuxCNC: %s (%s)" % (self.m.name, self.ini_path)

    # -- HAL --------------------------------------------------------------
    def halcmd(self, *args, check=True):
        p = subprocess.run(["halcmd"] + list(args), capture_output=True, text=True)
        if check and p.returncode != 0:
            raise RuntimeError("halcmd %s: %s" % (" ".join(args), (p.stdout + p.stderr).strip()))
        return p.stdout

    def signal_of(self, pin):
        """The signal a pin is on, or None."""
        out = self.halcmd("show", "pin", pin)
        for line in out.splitlines():
            w = line.split()
            if pin in w:
                i = w.index(pin)
                if i + 2 < len(w) and w[i + 1] in ("==>", "<==", "<=>"):
                    return w[i + 2]
                return None
        raise RuntimeError("no HAL pin %s" % pin)

    def _hal_setup(self):
        """A sampler with our columns: x y1 y2 scales, their currents, X and Y
        commands, the tool point (unwired ones read 0)."""
        if self.sampler is None:
            comps = self.halcmd("show", "comp")
            if re.search(r"\bsampler\b", comps):
                raise RuntimeError("the config loads sampler already: give the wizard its own instance "
                                   "(add ,%s to its cfg=) and set [DYNWIZ] SAMPLER (README.md)" % ("f" * self.NCOL))
            self.halcmd("loadrt", "sampler", "depth=20000", "cfg=" + "f" * self.NCOL)
            self.halcmd("addf", "sampler.0", "servo-thread")
            self.sampler = "0"
        self.halcmd("setp", "sampler.%s.enable" % self.sampler, "0")
        cols = self.cmd_src + self.src + self.cur_src + self.tool_src  # 2 + 3 + 3 + 2 = 10
        self.layout = ("cx", "cy", "x", "y1", "y2", "ix", "iy1", "iy2", "tx", "ty")
        for i, pin in enumerate(cols):
            sp = "sampler.%s.pin.%d" % (self.sampler, i)
            self.halcmd("unlinkp", sp, check=False)
            if not pin:
                continue
            sig = self.signal_of(pin)
            if sig is None:
                sig = "dynwiz-%d" % i
                self.halcmd("net", sig, pin)
            self.halcmd("linksp", sig, sp)

    # -- LinuxCNC ---------------------------------------------------------
    def poll(self):
        self.S.poll()
        return self.S

    def wait(self, pred, timeout):
        t = time.time() + timeout
        while time.time() < t:
            if pred(self.poll()):
                return True
            time.sleep(0.02)
        return False

    def errors(self):
        out = []
        while True:
            e = self.E.poll()
            if not e:
                return out
            out.append(e[1].strip())

    def ready(self):
        s = self.poll()
        lc = self.lc
        if s.task_state != lc.STATE_ON:
            return "the machine is not on"
        if not all(s.homed[j] for j in range(s.joints)):
            return "not homed"
        if s.interp_state != lc.INTERP_IDLE:
            return "a program is running"
        return None

    def position(self):
        s = self.poll()
        p = s.actual_position  # machine coordinates
        return (p[0], p[1])

    def set_comp(self, model):
        if not self.pcomp:
            if model is not None:
                raise RuntimeError("no pcomp in this config ([DYNWIZ] PCOMP)")
            return
        P = self.pcomp
        if model is None:
            for k in ("shaper", "defl", "shrink", "servo"):
                self.halcmd("setp", "%s.%s" % (P, k), "0")
            self.halcmd("setp", "%s.jerk-ms" % P, "0")
            return
        for k, v in (("fx", model["fx"]), ("fy-end", model["fy_end"]), ("fy-mid", model["fy_mid"]),
                     ("zeta", model["zeta"]), ("rails", model["rails"]), ("x-center", model["x_center"]),
                     ("servo-tau2", model.get("servo_tau2", 0)), ("jerk-ms", model.get("jerk_ms", 20))):
            self.halcmd("setp", "%s.%s" % (P, k), "%g" % v)
        for k in ("shaper", "defl", "shrink"):
            self.halcmd("setp", "%s.%s" % (P, k), "1")
        self.halcmd("setp", "%s.servo" % P, "1" if model.get("servo_tau2") else "0")

    def comp_state(self):
        if not self.pcomp:
            return None
        out = {}
        for k in ("shaper", "defl", "shrink", "servo", "jerk-ms"):
            out[k] = self.halcmd("getp", "%s.%s" % (self.pcomp, k)).strip()
        return out

    def restore_comp(self, st):
        if st:
            for k, v in st.items():
                self.halcmd("setp", "%s.%s" % (self.pcomp, k), "1" if v == "TRUE" else "0" if v == "FALSE" else v)

    def gcode(self, moves, off=(0.0, 0.0)):
        """The moves as a program; positions are machine coordinates, the
        program's are less the active offsets (G5x + G92), rotation none."""
        g = ["G21 G90 G61 G17", "G40 G49 G94"]
        for mv in moves:
            if mv[0] == "dwell":
                g.append("G4 P%.3f" % mv[1])
            elif mv[0] == "to":
                g.append("G1 X%.4f Y%.4f F%.1f" % (mv[1] - off[0], mv[2] - off[1], mv[3]))
            elif mv[0] == "rel":
                g.append("G91 G1 %s%.4f F%.1f" % (mv[1].upper(), mv[2], mv[3]))
                g.append("G90")
            elif mv[0] == "circle":
                _, cx, cy, r, f, turns = mv
                g.append("G1 X%.4f Y%.4f F%.1f" % (cx + r - off[0], cy - off[1], min(f, 3000)))
                g.append("G4 P0.2")
                g.append("G2 X%.4f Y%.4f I%.4f J0 P%d F%.1f" % (cx + r - off[0], cy - off[1], -r, int(turns), f))
        g.append("M2")
        return "\n".join(g) + "\n"

    def record(self, moves):
        why = self.ready()
        if why:
            raise RuntimeError(why)
        lc = self.lc
        d = tempfile.mkdtemp(prefix="dynwiz.")
        prog = os.path.join(d, "dynwiz.ngc")
        s = self.poll()
        if abs(s.rotation_xy) > 1e-9:
            raise RuntimeError("a rotated coordinate system (G10 L2 R) is active")
        off = (s.g5x_offset[0] + s.g92_offset[0], s.g5x_offset[1] + s.g92_offset[1])
        open(prog, "w").write(self.gcode(moves, off))
        samples = os.path.join(d, "rec.samples")
        self.errors()
        with open(samples, "w") as f:
            rec = subprocess.Popen(["halsampler", "-c", self.sampler, "-t"], stdout=f)
            time.sleep(0.2)
            self.halcmd("setp", "sampler.%s.enable" % self.sampler, "1")
            try:
                self.C.mode(lc.MODE_AUTO)
                self.C.wait_complete()
                self.C.program_open(prog)
                self.C.wait_complete()
                self.C.auto(lc.AUTO_RUN, 0)
                time.sleep(0.3)
                status = "done"
                while True:
                    s = self.poll()
                    if s.task_state != lc.STATE_ON:
                        status = "aborted (machine off)"
                        break
                    if s.interp_state == lc.INTERP_IDLE and s.queue == 0:
                        break
                    time.sleep(0.02)
            except KeyboardInterrupt:
                self.C.abort()
                status = "interrupted"
            time.sleep(0.2)
            self.halcmd("setp", "sampler.%s.enable" % self.sampler, "0")
            time.sleep(0.3)
            rec.terminate()
            rec.wait()
        errs = self.errors()
        self.C.mode(lc.MODE_MANUAL)
        if status != "done" or errs:
            raise RuntimeError("%s %s" % (status, "; ".join(errs)))
        out = Rec((c, []) for c in COLS)
        for line in open(samples):
            w = line.split()
            if len(w) != self.NCOL + 1:  # halsampler's number, then the pins; a cut-off last line
                continue
            vals = [float(v) for v in w[1:]]
            out["t"].append(len(out["t"]) * 0.001)
            for i, c in enumerate(self.layout):
                out[c].append(vals[i])
        shutil.rmtree(d, ignore_errors=True)
        for c, src in (("y2", self.src[2]), ("ix", self.cur_src[0]), ("iy1", self.cur_src[1]), ("iy2", self.cur_src[2]),
                       ("tx", self.tool_src[0]), ("ty", self.tool_src[1])):
            if not src:
                out[c] = None
        return out
