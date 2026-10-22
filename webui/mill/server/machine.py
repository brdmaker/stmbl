"""millweb's side of LinuxCNC: the status as a compact dict, the commands
the console sends, the files it reads and writes. Uses LinuxCNC's python
module (`linuxcnc`, and `hal` for pins); tests pass a stand-in.

Machines in mm (linear units 1). Axes X Y Z; a gantry's second Y joint is
shown as Y2 (from [TRAJ] COORDINATES / [KINS]).
"""
import os
import re
import shutil
import threading
import time

AXES = "XYZABCUVW"


def read_ini(path):
    """{section: {key: [values]}}: every value of a repeated key."""
    out, sec = {}, None
    with open(path, errors="replace") as f:
        for line in f:
            s = line.strip()
            if not s or s[0] in "#;":
                continue
            if s.startswith("[") and "]" in s:
                sec = s[1:s.index("]")]
                out.setdefault(sec, {})
                continue
            if "=" in s and sec is not None:
                k, v = s.split("=", 1)
                out[sec].setdefault(k.strip(), []).append(v.strip())
    return out


class Config:
    def __init__(self, ini_path):
        self.path = os.path.abspath(ini_path)
        self.dir = os.path.dirname(self.path)
        self.ini = read_ini(ini_path)

    def get(self, sec, key, default=None):
        v = self.ini.get(sec, {}).get(key)
        return v[0] if v else default

    def all(self, sec, key):
        return list(self.ini.get(sec, {}).get(key, []))

    def num(self, sec, key, default):
        try:
            return float(self.get(sec, key, default))
        except (TypeError, ValueError):
            return default

    def file(self, sec, key, default=None):
        v = self.get(sec, key, default)
        if not v:
            return None
        return os.path.normpath(os.path.join(self.dir, os.path.expanduser(v)))


def parse_tool_table(path):
    """tool.tbl: T P D Z ;comment -> [{t, p, d, z, c}]"""
    tools = []
    try:
        with open(path, errors="replace") as f:
            for line in f:
                body, _, comment = line.partition(";")
                w = dict(re.findall(r"([TPDZXYR])\s*([-+]?\d*\.?\d+)", body.upper()))
                if "T" not in w:
                    continue
                tools.append({"t": int(float(w["T"])), "p": int(float(w.get("P", w["T"]))),
                              "d": float(w.get("D", 0)), "z": float(w.get("Z", 0)), "c": comment.strip()})
    except OSError:
        pass
    return tools


def write_tool_table(path, tools):
    if os.path.exists(path):
        shutil.copy(path, path + ".bak")
    with open(path + ".tmp", "w") as f:
        for t in sorted(tools, key=lambda t: t["t"]):
            f.write("T%d P%d D%.4f Z%+.4f ;%s\n" % (t["t"], t["p"], t["d"], t["z"], t.get("c", "").replace("\n", " ")))
    os.replace(path + ".tmp", path)


def var_value(path, n, default=None):
    """One parameter from the parameter file (5220: the coordinate system at startup)."""
    try:
        with open(path) as f:
            for line in f:
                w = line.split()
                if len(w) >= 2 and w[0] == str(n):
                    return float(w[1])
    except (OSError, ValueError):
        pass
    return default


def parse_var_offsets(path):
    """G54..G59.3 [x, y, z] from the parameter file (5221 + 20 i ...)."""
    vals = {}
    try:
        with open(path) as f:
            for line in f:
                w = line.split()
                if len(w) >= 2:
                    try:
                        vals[int(w[0])] = float(w[1])
                    except ValueError:
                        pass
    except OSError:
        pass
    return [[vals.get(5221 + 20 * i + k, 0.0) for k in range(3)] for i in range(9)]


class Machine:
    """The LinuxCNC end: status(), events(), and the commands (in one worker
    thread: they may wait for LinuxCNC)."""

    WCS = ["G54", "G55", "G56", "G57", "G58", "G59", "G59.1", "G59.2", "G59.3"]

    def __init__(self, cfg, linuxcnc, hal=None):
        self.cfg, self.lc, self.hal = cfg, linuxcnc, hal
        self.s = linuxcnc.stat()        # the poller's
        self.ws = linuxcnc.stat()       # the worker's
        self.c = linuxcnc.command()
        # E-STOP, stop and jog stop come from their own thread, on their own
        # channel, so they never wait behind a long command (DEBUGNOTES)
        self.cu = linuxcnc.command()
        self.us = linuxcnc.stat()
        self.stops = 0                  # counts stops: a sequence that sees it change gives up
        self.e = linuxcnc.error_channel()
        self.lock = threading.Lock()
        coords = (cfg.get("TRAJ", "COORDINATES") or cfg.get("KINS", "COORDINATES") or "X Y Z").replace(" ", "").upper()
        m = re.search(r"coordinates=([A-Za-z]+)", cfg.get("KINS", "KINEMATICS", "") or "")
        if m:
            coords = m.group(1).upper()
        self.coords = coords
        # joints per axis letter, in order (a gantry: Y -> [1, 2])
        self.joints = {}
        for j, a in enumerate(coords):
            self.joints.setdefault(a, []).append(j)
        self.nc_dir = cfg.file("MILLWEB", "NC_DIR") or cfg.file("DISPLAY", "PROGRAM_PREFIX") or os.path.join(cfg.dir, "nc_files")
        self.tool_file = cfg.file("EMCIO", "TOOL_TABLE") or os.path.join(cfg.dir, "tool.tbl")
        self.var_file = cfg.file("RS274NGC", "PARAMETER_FILE") or os.path.join(cfg.dir, "linuxcnc.var")
        self.offsets = parse_var_offsets(self.var_file)
        self.g92 = [0.0, 0.0, 0.0]
        self.maxv = cfg.num("TRAJ", "MAX_LINEAR_VELOCITY", cfg.num("TRAJ", "MAX_VELOCITY", 100.0))
        # LinuxCNC clamps the overrides to these, though stat reads back what was asked (DEBUGNOTES)
        self.ovr_max = {"feed": cfg.num("DISPLAY", "MAX_FEED_OVERRIDE", 1.0),
                        "spin": cfg.num("DISPLAY", "MAX_SPINDLE_OVERRIDE", 1.0),
                        "spin_min": cfg.num("DISPLAY", "MIN_SPINDLE_OVERRIDE", 0.5)}
        ts = (cfg.get("MILLWEB", "TOOLSETTER") or "").split()
        self.toolsetter = [float(v) for v in ts] if len(ts) >= 4 else None  # X Y Z_above Z_ref [travel]
        self.pins = cfg.all("MILLWEB", "PIN")
        self.drives = [d.split() for d in cfg.all("MILLWEB", "DRIVE")]  # name current [power] [fault]
        self.sig = {k: cfg.get("MILLWEB", k) for k in ("SPINDLE_LOAD_PIN", "BUS_PIN", "YAW_PIN", "CONTOUR_PIN", "AT_SPEED_PIN", "REMOTE_ENABLE_PIN", "AIR_PIN")}
        self.pcomp = cfg.get("MILLWEB", "PCOMP", "pcomp.0")
        self.last_run = "idle"
        self.wcs_unknown = False
        self.notes = []                 # for the event log, from the worker thread
        self.done_flag = False
        self.jog_active = {}

    # -------------------------------------------------------------- info
    def info(self):
        cfg = self.cfg
        lim = {}
        for a in "XYZ":
            sec = "AXIS_" + a
            lim[a] = {"min": cfg.num(sec, "MIN_LIMIT", 0.0), "max": cfg.num(sec, "MAX_LIMIT", 0.0),
                      "vel": cfg.num(sec, "MAX_VELOCITY", 0.0) * 60, "acc": cfg.num(sec, "MAX_ACCELERATION", 0.0)}
        return {"name": cfg.get("EMC", "MACHINE", "LinuxCNC"), "coords": self.coords, "limits": lim,
                "maxv": self.maxv * 60, "units": "mm", "nc_dir": self.nc_dir,
                "spindle_max": cfg.num("SPINDLE_0", "MAX_FORWARD_VELOCITY", 24000.0),
                "jog_default": cfg.num("DISPLAY", "DEFAULT_LINEAR_VELOCITY", 30.0) * 60,
                "toolsetter": bool(self.toolsetter), "pcomp": self._pin_exists(self.pcomp + ".shaper"),
                "drives": [d[0] for d in self.drives], "pins": self.pins, "wcs": self.WCS, "ovr_max": self.ovr_max}

    def _pin_exists(self, name):
        if not self.hal or not name:
            return False
        try:
            self.hal.get_value(name)
            return True
        except Exception:
            return False

    def _pin(self, name):
        if not self.hal or not name:
            return None
        try:
            v = self.hal.get_value(name)
            return v if isinstance(v, bool) else round(float(v), 6)
        except Exception:
            return None

    # -------------------------------------------------------------- status
    def status(self):
        lc, s = self.lc, self.s
        s.poll()
        on = s.task_state == lc.STATE_ON
        estop = s.task_state == lc.STATE_ESTOP
        homed = [bool(all(s.homed[j] for j in self.joints.get(a, [0]))) for a in "XYZ"]
        g5x, g92, tlo = s.g5x_offset, s.g92_offset, s.tool_offset
        act = s.actual_position
        # the tool tip in the active work coordinates (as AXIS's relative DRO)
        import math
        x, y, z = act[0] - g5x[0] - tlo[0], act[1] - g5x[1] - tlo[1], act[2] - g5x[2] - tlo[2]
        if s.rotation_xy:
            t = math.radians(-s.rotation_xy)
            x, y = x * math.cos(t) - y * math.sin(t), x * math.sin(t) + y * math.cos(t)
        x, y, z = x - g92[0], y - g92[1], z - g92[2]
        wi = max(0, s.g5x_index - 1)
        self.wcs_unknown = on and s.g5x_index == 0  # see resync_offsets
        self.offsets[wi] = [g5x[0], g5x[1], g5x[2]]  # what LinuxCNC has is the truth
        self.g92 = [g92[0], g92[1], g92[2]]
        interp = {lc.INTERP_IDLE: "idle", lc.INTERP_READING: "reading", lc.INTERP_PAUSED: "paused", lc.INTERP_WAITING: "waiting"}.get(s.interp_state, "idle")
        mode = {lc.MODE_MANUAL: "manual", lc.MODE_AUTO: "auto", lc.MODE_MDI: "mdi"}.get(s.task_mode, "manual")
        if not on:
            run = "idle"
        elif interp == "idle":
            run = "done" if self.last_run in ("run", "hold", "step") and mode == "auto" else ("done" if self.done_flag else "idle")
        elif mode == "auto":
            run = "hold" if (s.paused or interp == "paused") else "run"
        else:
            run = "mdi"
        self.done_flag = run == "done"
        self.last_run = run
        fe = {}
        for a in "XYZ":
            js = self.joints.get(a, [])
            for k, j in enumerate(js):
                if j < len(s.joint):
                    fe[a + (str(k + 1) if len(js) > 1 else "")] = round(s.joint[j]["ferror_current"] * 1000, 2)
        sp = s.spindle[0] if s.spindle else {}
        spd = sp.get("speed", 0.0) or 0.0
        at = self._pin(self.sig["AT_SPEED_PIN"])
        return {
            "estop": estop, "on": on, "homed": homed, "mode": mode, "interp": interp, "run": run,
            "line": s.current_line, "mline": s.motion_line, "file": s.file,
            "pos": [round(x, 4), round(y, 4), round(z, 4)],
            "mach": [round(act[0] - tlo[0], 4), round(act[1] - tlo[1], 4), round(act[2] - tlo[2], 4)],
            "wcs": wi, "g5x": [round(g5x[i], 4) for i in range(3)], "g92": [round(g92[i], 4) for i in range(3)],
            "rot": round(s.rotation_xy, 4), "tool": s.tool_in_spindle, "tlo": round(tlo[2], 4), "wcs_ok": s.g5x_index > 0,
            "dtg": [round(v, 4) for v in s.dtg[:3]], "fe": fe, "vel": round(s.current_vel * 60, 1),
            "feed": round(s.settings[1], 1),
            "spin": {"dir": sp.get("direction", 0), "s": round(abs(spd), 1), "enabled": bool(sp.get("enabled", 0)),
                     "ovr": round(sp.get("override", 1.0), 3), "at": bool(at) if at is not None else None},
            "ovr": {"feed": round(s.feedrate, 3), "rapid": round(s.rapidrate, 3), "maxv": round(s.max_velocity * 60 / max(self.maxv * 60, 1e-9), 3)},
            "flood": bool(s.flood), "mist": bool(s.mist), "m1": bool(s.optional_stop), "bd": bool(s.block_delete),
            "gcodes": " ".join("G%g" % (g / 10.0) for g in s.gcodes[1:] if g >= 0),
            "mcodes": " ".join("M%d" % m for m in s.mcodes[1:] if m >= 0),
            "probe": {"tripped": bool(s.probe_tripped), "pos": [round(v, 4) for v in s.probed_position[:3]]},
            "queue": s.queue,
        }

    def slow(self):
        out = {"pins": {p: self._pin(p) for p in self.pins}}
        out["drives"] = [{"n": d[0], "cur": self._pin(d[1]) if len(d) > 1 else None, "pwr": self._pin(d[2]) if len(d) > 2 else None,
                          "fault": self._pin(d[3]) if len(d) > 3 else None} for d in self.drives]
        for k, key in (("load", "SPINDLE_LOAD_PIN"), ("bus", "BUS_PIN"), ("yaw", "YAW_PIN"), ("contour", "CONTOUR_PIN"), ("remote", "REMOTE_ENABLE_PIN")):
            out[k] = self._pin(self.sig[key])
        if out["pins"] == {}:
            del out["pins"]
        if self.info_pcomp():
            out["pcomp"] = {k: self._pin("%s.%s" % (self.pcomp, k)) for k in ("shaper", "defl", "shrink", "servo", "jerk-ms", "fx", "fy-end", "fy-mid", "delay")}
        return out

    def info_pcomp(self):
        return self._pin_exists(self.pcomp + ".shaper")

    def events(self):
        out = []
        while self.notes:
            out.append(("warn", self.notes.pop(0)))
        while True:
            m = self.e.poll()
            if not m:
                return out
            kind, text = m
            lvl = "bad" if kind in (self.lc.NML_ERROR, self.lc.OPERATOR_ERROR) else "info"
            out.append((lvl, text.strip()))

    # -------------------------------------------------------------- helpers (worker thread)
    def _wait_idle(self, timeout=30.0):
        t = time.time() + timeout
        while time.time() < t:
            self.ws.poll()
            if self.ws.interp_state == self.lc.INTERP_IDLE and self.ws.queue == 0:
                return True
            time.sleep(0.02)
        return False

    def _probed(self, timeout=1.0):
        """After a G38.x: (tripped, machine position), once motion has
        published it (it can lag the interpreter going idle)."""
        t = time.time() + timeout
        while True:
            self.ws.poll()
            pos = list(self.ws.probed_position[:3])
            if self.ws.probe_tripped and any(abs(v) > 1e-9 for v in pos):
                return True, pos
            if time.time() > t:
                return bool(self.ws.probe_tripped), pos
            time.sleep(0.02)

    def _travel_down(self, want):
        """A probe move may not end beyond the Z soft limit (LinuxCNC refuses
        it): the travel there is, at most `want`."""
        self.ws.poll()
        zmin = self.cfg.num("AXIS_Z", "MIN_LIMIT", -1e9)
        room = self.ws.position[2] - zmin - 0.01
        return max(0.0, min(float(want), room))

    def _mode(self, m):
        self.ws.poll()
        if self.ws.task_mode != m:
            self.c.mode(m)
            self.c.wait_complete(2.0)

    def _ready(self, need_homed=True, need_idle=True):
        lc = self.lc
        self.ws.poll()
        if self.ws.task_state == lc.STATE_ESTOP:
            return "E-STOP is active"
        if self.ws.task_state != lc.STATE_ON:
            return "the machine is off"
        if need_homed and not all(self.ws.homed[j] for j in range(self.ws.joints)):
            return "not homed"
        if need_idle and self.ws.interp_state != lc.INTERP_IDLE:
            return "a program or MDI is running"
        return None

    def resync_offsets(self):
        """LinuxCNC's status can come up without the interpreter's work offset
        (g5x_index 0, offsets 0) when commands arrive just after it starts;
        a move then uses the real offset while every display shows machine
        coordinates (DEBUGNOTES). Selecting another coordinate system and
        then the startup one makes it publish them. Machine on, homed, idle."""
        self.ws.poll()
        if self.ws.task_state != self.lc.STATE_ON or self.ws.g5x_index != 0 or self._ready():
            return None
        start = int(var_value(self.var_file, 5220, 1.0) or 1)
        start = start if 1 <= start <= 9 else 1
        self._mode(self.lc.MODE_MDI)
        for i in (2 if start == 1 else 1, start):
            self.c.mdi(self.WCS[i - 1])
            time.sleep(0.05)
            self._wait_idle(5)
        self.ws.poll()
        return "LinuxCNC's status had no work offset: re-read (%s)" % self.WCS[self.ws.g5x_index - 1] if self.ws.g5x_index else None

    def mdi(self, text, wait=False, timeout=60.0, since=None):
        """One MDI line; with wait, until it is done. `since`: the stop count
        the calling sequence started at; after a stop it sends nothing more."""
        since = self.stops if since is None else since
        if self.stops != since:
            return "stopped"
        err = self._ready()
        if err:
            return err
        note = self.resync_offsets()
        if note:
            self.notes.append(note)
        self._mode(self.lc.MODE_MDI)
        self.c.mdi(text)
        if wait:
            time.sleep(0.05)
            if not self._wait_idle(timeout):
                return "timed out: " + text
            if self.stops != since:
                return "stopped"
        return None

    # -------------------------------------------------------------- commands
    def _wait_state(self, pred, timeout=2.0, s=None):
        s = s or self.ws
        t = time.time() + timeout
        while time.time() < t:
            s.poll()
            if pred(s.task_state):
                return True
            time.sleep(0.02)
        return False

    # urgent (their own thread): E-STOP, stop, jog stop
    def cmd_estop(self, on):
        lc = self.lc
        if on:
            self.stops += 1
            self.cu.state(lc.STATE_ESTOP)
            return None
        self.cu.state(lc.STATE_ESTOP_RESET)
        if not self._wait_state(lambda s: s != lc.STATE_ESTOP, s=self.us):
            return "E-STOP is still active (the hardware chain?)"
        return None

    def cmd_stop(self):
        self.stops += 1
        self.cu.abort()
        return None

    def cmd_power(self, on):
        lc = self.lc
        self.ws.poll()
        if on and self.ws.task_state == lc.STATE_ESTOP:
            return "E-STOP is active"
        self.c.state(lc.STATE_ON if on else lc.STATE_OFF)
        # off reads back as STATE_ESTOP_RESET, not STATE_OFF (DEBUGNOTES)
        if not self._wait_state(lambda s: s == lc.STATE_ON if on else s != lc.STATE_ON):
            return "LinuxCNC didn't switch the machine %s" % ("on" if on else "off")
        return None

    def cmd_home(self, axis):
        err = self._ready(need_homed=False)
        if err:
            return err
        self._mode(self.lc.MODE_MANUAL)
        self.c.teleop_enable(0)
        self.c.wait_complete(1.0)
        if axis in (None, "all"):
            self.c.home(-1)
        else:
            for j in self.joints.get(axis.upper(), []):
                self.c.home(j)
        return None

    def cmd_unhome(self, axis):
        err = self._ready(need_homed=False)
        if err:
            return err
        self._mode(self.lc.MODE_MANUAL)
        self.c.teleop_enable(0)
        self.c.wait_complete(1.0)
        for j in (range(self.ws.joints) if axis in (None, "all") else self.joints.get(axis.upper(), [])):
            self.c.unhome(j)
        return None

    def cmd_jog(self, axis, direction, inc, rate):
        """inc > 0: one increment; inc == 0: continuous (stopped by jog_stop
        or by millweb's deadman)."""
        lc = self.lc
        err = self._ready()
        if err:
            return err
        a = "XYZ".index(axis.upper())
        self._mode(lc.MODE_MANUAL)
        self.ws.poll()
        if self.ws.motion_mode != lc.TRAJ_MODE_TELEOP:
            self.c.teleop_enable(1)
            self.c.wait_complete(1.0)
        vel = abs(rate) / 60.0
        if inc > 0:
            self.c.jog(lc.JOG_INCREMENT, False, a, vel * (1 if direction > 0 else -1), inc)
        else:
            self.c.jog(lc.JOG_CONTINUOUS, False, a, vel * (1 if direction > 0 else -1))
            self.jog_active[a] = True
        return None

    def cmd_jog_stop(self, axis=None):
        for a in ([("XYZ".index(axis.upper()))] if axis else list(self.jog_active)):
            self.cu.jog(self.lc.JOG_STOP, False, a)
            self.jog_active.pop(a, None)
        return None

    def cmd_cycle(self, action, line=0):
        lc = self.lc
        if action == "stop":
            return self.cmd_stop()
        if action == "pause":
            self.c.auto(lc.AUTO_PAUSE)
            return None
        if action == "resume":
            self.c.auto(lc.AUTO_RESUME)
            return None
        self.ws.poll()
        if action == "step" and self.ws.task_mode == lc.MODE_AUTO and self.ws.interp_state != lc.INTERP_IDLE:
            self.c.auto(lc.AUTO_STEP)
            return None
        err = self._ready()
        if err:
            return err
        if not self.ws.file:
            return "no program loaded"
        self._mode(lc.MODE_AUTO)
        if action == "step":
            self.c.auto(lc.AUTO_STEP)
        else:
            self.c.auto(lc.AUTO_RUN, int(line or 0))
        return None

    def cmd_override(self, k, v):
        v = max(0.0, float(v))
        if k == "feed":
            self.c.feedrate(min(v, self.ovr_max["feed"]))
        elif k == "rapid":
            self.c.rapidrate(min(v, 1.0))
        elif k == "spin":
            self.c.spindleoverride(max(self.ovr_max["spin_min"], min(v, self.ovr_max["spin"])), 0)
        elif k == "maxv":
            self.c.maxvel(min(v, 1.0) * self.maxv)
        else:
            return "unknown override " + k
        return None

    def cmd_spindle(self, direction, speed=None):
        lc = self.lc
        err = self._ready(need_homed=False, need_idle=False)
        if err:
            return err
        self._mode(lc.MODE_MANUAL)
        if direction == 0:
            self.c.spindle(lc.SPINDLE_OFF, 0)
        else:
            self.c.spindle(lc.SPINDLE_FORWARD if direction > 0 else lc.SPINDLE_REVERSE, float(speed or 1000), 0)
        return None

    def cmd_coolant(self, k, on):
        lc = self.lc
        if k == "flood":
            self.c.flood(lc.FLOOD_ON if on else lc.FLOOD_OFF)
        elif k == "mist":
            self.c.mist(lc.MIST_ON if on else lc.MIST_OFF)
        elif k == "air" and self.hal and self.sig["AIR_PIN"]:
            self.hal.set_p(self.sig["AIR_PIN"], "1" if on else "0")
        else:
            return "no " + k
        return None

    def cmd_opt(self, m1=None, bd=None):
        if m1 is not None:
            self.c.set_optional_stop(bool(m1))
        if bd is not None:
            self.c.set_block_delete(bool(bd))
        return None

    def cmd_zero(self, axis, value=0.0):
        return self.mdi("G10 L20 P0 %s%.4f" % (axis.upper(), float(value)), wait=True, timeout=5)

    def cmd_wcs(self, index):
        return self.mdi(self.WCS[int(index)], wait=True, timeout=5)

    def cmd_offset(self, index, x=None, y=None, z=None):
        words = "".join(" %s%.4f" % (a, float(v)) for a, v in (("X", x), ("Y", y), ("Z", z)) if v is not None)
        err = self.mdi("G10 L2 P%d%s" % (int(index) + 1, words), wait=True, timeout=5)
        if not err:
            for k, v in enumerate((x, y, z)):
                if v is not None:
                    self.offsets[int(index)][k] = float(v)
        return err

    def cmd_g92_clear(self):
        return self.mdi("G92.1", wait=True, timeout=5)

    def cmd_tool_load(self, t):
        # T0: an empty spindle; G43 would keep the old tool's offset (DEBUGNOTES)
        return self.mdi("T%d M6 %s" % (int(t), "G43" if int(t) else "G49"), wait=True, timeout=120)

    def tools(self):
        return parse_tool_table(self.tool_file)

    def cmd_tools_save(self, tools):
        err = self._ready(need_homed=False)
        if err:
            return err
        clean = []
        for t in tools:
            clean.append({"t": int(t["t"]), "p": int(t.get("p", t["t"])), "d": float(t.get("d", 0)), "z": float(t.get("z", 0)),
                          "c": str(t.get("c", ""))[:120]})
        if len({t["t"] for t in clean}) != len(clean):
            return "tool numbers must be unique"
        write_tool_table(self.tool_file, clean)
        self.c.load_tool_table()
        return None

    def cmd_tool_measure(self):
        """On the tool setter: Z up, over the setter, probe down; the tool's
        length from where it touched ([MILLWEB] TOOLSETTER = X Y Z_ABOVE Z_REF [TRAVEL])."""
        if not self.toolsetter:
            return "no tool setter configured ([MILLWEB] TOOLSETTER)"
        x, y, zab, zref = self.toolsetter[:4]
        travel = self.toolsetter[4] if len(self.toolsetter) > 4 else 100.0
        self.ws.poll()
        t = self.ws.tool_in_spindle
        if t <= 0:
            return "no tool in the spindle"
        since = self.stops
        for line in ("G49", "G53 G0 Z0", "G53 G0 X%.4f Y%.4f" % (x, y), "G53 G0 Z%.4f" % zab):
            err = self.mdi(line, wait=True, timeout=120, since=since)
            if err:
                return err
        travel = self._travel_down(travel)
        for line in ("G91 G38.2 Z-%.4f F300" % travel, "G90"):
            err = self.mdi(line, wait=True, timeout=120, since=since)
            if err:
                return err
        tripped, pos = self._probed()
        if not tripped:
            return "the setter didn't trip within %.0f mm" % travel
        zt = pos[2]
        length = round(zt - zref, 4)
        tools = self.tools()
        for tl in tools:
            if tl["t"] == t:
                tl["z"] = length
        self.mdi("G53 G0 Z0", wait=True, since=since)
        err = self.cmd_tools_save(tools)
        if not err:
            self.mdi("G43", wait=True)
        return err or {"tool": t, "length": length}

    def cmd_probe(self, op, travel=20.0, feed=300.0, plate=0.0, clearance=5.0):
        """Z surface or touch plate: probe down from here."""
        if op not in ("z", "plate"):
            return "the %s cycle is not in the backend yet (Z surface and touch plate are)" % op
        travel = self._travel_down(travel)
        since = self.stops
        for line in ("G91 G38.2 Z-%.4f F%.1f" % (travel, float(feed)), "G90"):
            err = self.mdi(line, wait=True, timeout=120, since=since)
            if err:
                return err
        tripped, pm = self._probed()
        if not tripped:
            return "the probe didn't trip within %.1f mm" % float(travel)
        s = self.ws
        s.poll()
        # motion stops a little past the trip point (DEBUGNOTES): offsets and
        # the retract go from where it tripped, not from where it stopped
        back = float(clearance) + pm[2] - s.position[2]
        pm[2] -= s.tool_offset[2]  # the tool tip, as the DRO's machine Z
        res = {"mach": [round(v, 4) for v in pm]}
        if op == "plate":
            z = pm[2] - s.g92_offset[2] - float(plate)
            err = self.mdi("G10 L2 P%d Z%.4f" % (max(1, s.g5x_index), z), wait=True, since=since)
            if err:
                return err
        self.mdi("G91 G0 Z%.4f" % back, wait=True, since=since)
        self.mdi("G90", wait=True)
        return res

    # programs: paths relative to the program folder ("parts/op1.ngc"), in it only
    PART = re.compile(r"^[\w\- ][\w.\- ]*$")  # a file or folder name: no "..", nothing hidden
    PROGRAM = re.compile(r"\.(ngc|nc|tap|gcode)$", re.I)
    MAX_DEPTH, MAX_ENTRIES = 8, 5000

    def _safe(self, name, folder=False):
        """The absolute path of a program (or, folder=True, a folder; "" is the
        program folder itself), from its path in the program folder."""
        rel = str(name).replace("\\", "/").strip("/")
        parts = [p for p in rel.split("/") if p] if rel else []
        if not folder and not parts:
            raise ValueError("no program name")
        if len(parts) > self.MAX_DEPTH or any(not self.PART.match(p) or len(p) > 120 for p in parts):
            raise ValueError("names: letters, digits, space . _ - (not starting with a dot), folders separated by /")
        if not folder and not self.PROGRAM.search(parts[-1]):
            raise ValueError("programs end in .ngc, .nc, .tap or .gcode")
        root = os.path.realpath(self.nc_dir)
        full = os.path.join(root, *parts)
        real = os.path.realpath(full)
        if real != root and not real.startswith(root + os.sep):
            raise ValueError("outside the program folder")
        return full

    def relpath(self, path):
        """A path's name in the program folder, or its base name when it is elsewhere."""
        root = os.path.realpath(self.nc_dir)
        real = os.path.realpath(path)
        return os.path.relpath(real, root).replace(os.sep, "/") if real.startswith(root + os.sep) else os.path.basename(path)

    def programs(self):
        """Every program in the program folder and its folders (hidden ones and
        links out of it left out): files [{name, size, mtime}], dirs [name]."""
        files, dirs = [], []
        root = os.path.realpath(self.nc_dir)
        for top, dnames, fnames in os.walk(root):
            depth = 0 if top == root else os.path.relpath(top, root).count(os.sep) + 1
            dnames[:] = sorted(d for d in dnames if self.PART.match(d) and depth < self.MAX_DEPTH
                               and os.path.realpath(os.path.join(top, d)).startswith(root + os.sep))
            rel = "" if top == root else os.path.relpath(top, root).replace(os.sep, "/") + "/"
            dirs += [rel + d for d in dnames]
            for n in sorted(fnames):
                if self.PART.match(n) and self.PROGRAM.search(n):
                    try:
                        st = os.stat(os.path.join(top, n))
                    except OSError:
                        continue
                    files.append({"name": rel + n, "size": st.st_size, "mtime": int(st.st_mtime)})
            if len(files) + len(dirs) > self.MAX_ENTRIES:
                break
        return files, dirs

    def program_text(self, name):
        with open(self._safe(name), errors="replace") as f:
            return f.read()

    def _loaded(self):
        self.ws.poll()
        return os.path.realpath(self.ws.file) if self.ws.file else None

    def cmd_program_save(self, name, text, overwrite=True):
        p = self._safe(name)
        if not os.path.isdir(os.path.dirname(p)):
            if os.path.dirname(p) != os.path.realpath(self.nc_dir):
                return "no such folder"
            os.makedirs(self.nc_dir, exist_ok=True)
        if not overwrite and os.path.exists(p):
            return "a program of that name is there already"
        with open(p + ".tmp", "w") as f:
            f.write(text if text.endswith("\n") else text + "\n")
        os.replace(p + ".tmp", p)
        return None

    def cmd_program_delete(self, name):
        p = self._safe(name)
        if self._loaded() == os.path.realpath(p):
            return "that program is loaded"
        if not os.path.isfile(p):
            return "no such program"
        os.remove(p)
        return None

    def cmd_rename(self, name, to):
        """A program or a folder to a new name or place (in the program folder);
        a program keeps a program's extension."""
        folder = os.path.isdir(self._safe(name, folder=True))
        src, dst = self._safe(name, folder), self._safe(to, folder)
        if src == os.path.realpath(self.nc_dir):
            return "not the program folder itself"
        if not os.path.exists(src):
            return "no such " + ("folder" if folder else "program")
        if os.path.exists(dst):
            return "%s is there already" % to
        if not os.path.isdir(os.path.dirname(dst)):
            return "no such folder: %s" % os.path.dirname(str(to))
        if folder and (dst + os.sep).startswith(os.path.realpath(src) + os.sep):
            return "not into itself"
        loaded = self._loaded()
        if loaded and (loaded == os.path.realpath(src) or loaded.startswith(os.path.realpath(src) + os.sep)):
            return "the loaded program is " + ("in it" if folder else "that one")
        os.rename(src, dst)
        return None

    def cmd_folder_create(self, name):
        p = self._safe(name, folder=True)
        if os.path.exists(p):
            return "%s is there already" % name
        if not os.path.isdir(os.path.dirname(p)):
            return "no such folder: %s" % os.path.dirname(str(name))
        os.mkdir(p)
        return None

    def cmd_folder_delete(self, name):
        p = self._safe(name, folder=True)
        if p == os.path.realpath(self.nc_dir):
            return "not the program folder itself"
        if not os.path.isdir(p):
            return "no such folder"
        if os.listdir(p):
            return "the folder isn't empty"
        os.rmdir(p)
        return None

    def cmd_program_load(self, name):
        err = self._ready(need_homed=False)
        if err:
            return err
        p = self._safe(name)
        if not os.path.exists(p):
            return "no such program"
        self._mode(self.lc.MODE_AUTO)
        self.c.program_open(p)
        self.c.wait_complete(5.0)
        return None

    def cmd_pcomp(self, k, v):
        if k not in ("shaper", "defl", "shrink", "servo", "jerk-ms"):
            return "unknown pcomp part"
        if not self.info_pcomp():
            return "pcomp is not loaded"
        self.hal.set_p("%s.%s" % (self.pcomp, k), str(v))
        return None
