"""millweb's side of LinuxCNC: the status as a compact dict, the commands
the console sends, the files it reads and writes. Uses LinuxCNC's python
module (`linuxcnc`, and `hal` for pins); tests pass a stand-in.

Machines in mm (linear units 1). Axes X Y Z; a gantry's second Y joint is
shown as Y2 (from [TRAJ] COORDINATES / [KINS]).
"""
import base64
import json
import math
import os
import re
import shutil
import subprocess
import threading
import time

AXES = "XYZABCUVW"
# kinematics of an arm: its joints are not axes (millweb: machine type "arm")
def axis_rot(i, deg):
    """3x3 rotation about x (0), y (1) or z (2) by deg"""
    c, s = math.cos(math.radians(deg)), math.sin(math.radians(deg))
    return [[[1, 0, 0], [0, c, -s], [0, s, c]], [[c, 0, s], [0, 1, 0], [-s, 0, c]], [[c, -s, 0], [s, c, 0], [0, 0, 1]]][i]


def mat_mul(a, b):
    return [[sum(a[r][k] * b[k][c] for k in range(3)) for c in range(3)] for r in range(3)]


def rpy_matrix(a, b, c):
    """an arm's A B C [deg] as genserkins: R = Rz(C) Ry(B) Rx(A)"""
    return mat_mul(axis_rot(2, c), mat_mul(axis_rot(1, b), axis_rot(0, a)))


def rpy_of(R):
    """R's A B C [deg] (shared/robot/kin.c rb_rpy_from_R)"""
    cb = math.hypot(R[0][0], R[1][0])
    b = math.atan2(-R[2][0], cb)
    if cb > 1e-9:
        a, c = math.atan2(R[2][1], R[2][2]), math.atan2(R[1][0], R[0][0])
    else:
        a, c = 0.0, math.atan2(-R[0][1], R[1][1])
    return [math.degrees(a), math.degrees(b), math.degrees(c)]


def unwrap_deg(x, ref):
    """x moved by whole turns to within 180° of ref (LinuxCNC moves A B C the way the numbers go)"""
    return x + 360.0 * round((ref - x) / 360.0)


ARM_KINS = ("genserkins", "armkins", "pumakins", "scarakins", "genhexkins", "pentakins", "rosekins", "5axiskins")
ARM_PINS = {  # what the console shows of an arm, if loaded (linuxcnc/armguard, armcollide, armgrip, armtool, powerquota)
    "guard": ("armguard.0.", ("state", "reason", "clearance")),
    "collide": ("armcollide.0.", ("collision",)),
    "grip": ("armgrip.0.", ("state", "gap-fb", "payload-mass", "close")),
    "tool": ("armtool.0.", ("mounted", "fault", "reason", "valve")),
    "changer": ("stmbl-arm.0.changer.", ("locked", "unlocked", "present", "id")),
    "quota": ("powerquota.0.", ("total-power", "feed-scale", "limiting")),
}


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


# millweb's own tool data, a tag at the end of tool.tbl's comment (LinuxCNC keeps comments when it writes the
# table): fl flutes; a lathe's ins (the insert: 80, 55, 35, 60 a thread, groove, round, drill), w its width
# (a grooving insert's), cl its cutting edge length (a mill's: the flute length); a mill's ty (its type: flat, ball,
# bull, drill, vee, probe), cr (a bull nose's corner radius), ang (a drill's or a vee's point angle), so (its
# stick-out: the tip to the holder), hd (the holder's Ø)
TOOL_TAG = {"fl": int, "ins": str, "w": float, "cl": float, "ty": str, "cr": float, "ang": float, "so": float, "hd": float}
INSERTS = ("80", "55", "35", "60", "groove", "round", "drill")
MILL_TYPES = ("flat", "ball", "bull", "drill", "vee", "probe")


def split_comment(comment):
    """'6 mm endmill {fl=3}' -> ('6 mm endmill', {'fl': 3}); unknown or bad keys dropped."""
    m = re.search(r"\s*\{([^{}]*)\}\s*$", comment)
    if not m:
        return comment.strip(), {}
    out = {}
    for k, _, v in (x.partition("=") for x in m.group(1).split()):
        try:
            if k in TOOL_TAG and v:
                out[k] = TOOL_TAG[k](float(v)) if TOOL_TAG[k] is int else TOOL_TAG[k](v)
        except ValueError:
            pass
    return comment[:m.start()].strip(), out


def join_comment(c, t):
    tag = " ".join("%s=%s" % (k, ("%g" % t[k]) if isinstance(t[k], float) else t[k]) for k in TOOL_TAG if t.get(k) not in (None, ""))
    return (c + " {%s}" % tag) if tag else c


LATHE_STOCK = {"d": (0.5, 2000), "bore": (0, 2000), "stick": (1, 3000), "face": (-500, 500), "chuckD": (10, 3000)}


def lathe_check(o):
    """A lathe's stock in the setup: d (Ø), bore, stick (out of the chuck), face (its Z), chuckD; ValueError else."""
    if not isinstance(o, dict):
        raise ValueError("a lathe's stock: d, bore, stick, face, chuckD")
    out = {}
    for k, (lo, hi) in LATHE_STOCK.items():
        if k in o:
            v = o[k]
            if isinstance(v, bool) or not isinstance(v, (int, float)) or not lo <= v <= hi:
                raise ValueError("the stock's %s: %g to %g" % (k, lo, hi))
            out[k] = float(v)
    if out.get("bore", 0) >= out.get("d", 32):
        raise ValueError("the stock's bore: less than its diameter")
    return out


def parse_tool_table(path):
    """tool.tbl: T P D Z ;comment -> [{t, p, d, z, c}]; a lathe's also X (its X offset), Q (the
    orientation, 1-9), I and J (the front and back angles): x, q, i, j; the comment's tag: fl, ins, w, cl"""
    tools = []
    try:
        with open(path, errors="replace") as f:
            for line in f:
                body, _, comment = line.partition(";")
                w = dict(re.findall(r"([TPDZXYRQIJ])\s*([-+]?\d*\.?\d+)", body.upper()))
                if "T" not in w:
                    continue
                c, tag = split_comment(comment)
                t = {"t": int(float(w["T"])), "p": int(float(w.get("P", w["T"]))),
                     "d": float(w.get("D", 0)), "z": float(w.get("Z", 0)), "c": c}
                t.update(tag)
                for k in "XQIJ":
                    if k in w:
                        t[k.lower()] = int(float(w[k])) if k == "Q" else float(w[k])
                tools.append(t)
    except OSError:
        pass
    return tools


def write_tool_table(path, tools):
    if os.path.exists(path):
        shutil.copy(path, path + ".bak")
    with open(path + ".tmp", "w") as f:
        for t in sorted(tools, key=lambda t: t["t"]):
            lathe = "".join((" X%+.4f" % t["x"] if "x" in t else "") + (" Q%d" % t["q"] if t.get("q") else "") +
                            (" I%.1f" % t["i"] if "i" in t else "") + (" J%.1f" % t["j"] if "j" in t else ""))
            f.write("T%d P%d%s D%.4f Z%+.4f ;%s\n" % (t["t"], t["p"], lathe, t["d"], t["z"], join_comment(t.get("c", "").replace("\n", " "), t)))
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


DEFAULT_SETUP = [
    {"id": "vise", "name": "Vise", "role": "fixture", "src": "builtin:vise", "frame": "work",
     "pos": [0.0, 0.0, -25.0], "rot": [0.0, 0.0, 0.0], "scale": 1.0, "look": "solid"},
    {"id": "stock", "name": "Stock 120 x 80 x 25", "role": "workpiece", "src": "builtin:stock", "frame": "work",
     "pos": [0.0, 0.0, 0.0], "rot": [0.0, 0.0, 0.0], "scale": 1.0, "look": "solid"},
]


def model_check(name, data):
    """None when data looks like the STL or OBJ its name says, else why not."""
    if name.lower().endswith(".stl"):
        if len(data) >= 84 and len(data) == 84 + 50 * int.from_bytes(data[80:84], "little"):
            return None  # binary STL: header, count, 50 bytes a triangle
        head = data[:2048].decode("latin-1").lower()
        if head.lstrip().startswith("solid") and "facet" in data[:1 << 20].decode("latin-1").lower():
            return None  # ASCII STL
        return "not an STL file (neither binary nor ASCII)"
    text = data[:1 << 20].decode("latin-1")
    if re.search(r"(?m)^v\s+[-+\d.eE]+\s+[-+\d.eE]+\s+[-+\d.eE]+", text) and re.search(r"(?m)^f\s", data.decode("latin-1")):
        return None
    return "not an OBJ file (no vertices or faces)"


def setup_check(objects, model_path):
    """The setup's objects, checked and normalised: raises ValueError with the reason."""
    if not isinstance(objects, list) or len(objects) > 64:
        raise ValueError("the setup: a list of at most 64 objects")
    out, ids = [], set()
    for o in objects:
        if not isinstance(o, dict):
            raise ValueError("a setup object is a dictionary")
        src = str(o.get("src", ""))
        if src.startswith("model:"):
            if not os.path.isfile(model_path(src[6:])):
                raise ValueError("no such model: %s" % src[6:])
        elif src not in ("builtin:vise", "builtin:stock"):
            raise ValueError("an object's src: builtin:vise, builtin:stock or model:<path>")
        num = lambda v, lim: max(-lim, min(lim, float(v))) if math.isfinite(float(v)) else 0.0  # noqa: E731
        c = {"id": str(o.get("id", ""))[:40] or "o%d" % len(out), "name": str(o.get("name", ""))[:80],
             "role": o.get("role") if o.get("role") in ("fixture", "workpiece", "part") else "fixture", "src": src,
             "frame": o.get("frame") if o.get("frame") in ("work", "machine") else "work",
             "pos": [num(v, 10000) for v in list(o.get("pos", (0, 0, 0)))[:3]],
             "rot": [num(v, 360) for v in list(o.get("rot", (0, 0, 0)))[:3]],
             "scale": max(0.001, min(1000.0, float(o.get("scale", 1)))),
             "look": o.get("look") if o.get("look") in ("solid", "glass", "hidden") else "solid"}
        if len(c["pos"]) != 3 or len(c["rot"]) != 3:
            raise ValueError("pos and rot: three numbers each")
        if c["id"] in ids:
            raise ValueError("object ids must be unique")
        ids.add(c["id"])
        out.append(c)
    return out


def tools_check(tools, model_path):
    """The tools' 3D models {"<tool number>": {src: model:<path>, z, rot, scale}}, checked: ValueError with the reason.
    A model is drawn with the tool's tip at its origin and the spindle along +Z; z shifts it, rot turns it."""
    if not isinstance(tools, dict) or len(tools) > 200:
        raise ValueError("the tools' models: at most 200")
    out = {}
    for k, t in tools.items():
        if not str(k).isdigit() or not 0 < int(k) < 100000 or not isinstance(t, dict):
            raise ValueError("a tool's model: under its tool number")
        src = str(t.get("src", ""))
        if not src.startswith("model:") or not os.path.isfile(model_path(src[6:])):
            raise ValueError("T%s: no such model: %s" % (k, src[6:] or "(none)"))
        num = lambda v, lim: max(-lim, min(lim, float(v))) if math.isfinite(float(v)) else 0.0  # noqa: E731
        rot = [num(v, 360) for v in list(t.get("rot", (0, 0, 0)))[:3]]
        if len(rot) != 3:
            raise ValueError("T%s: rot is three numbers" % k)
        out[str(int(k))] = {"src": src, "z": num(t.get("z", 0), 1000), "rot": rot, "scale": max(0.001, min(1000.0, float(t.get("scale", 1))))}
    return out


HAL_TYPES = {1: "bit", 2: "float", 3: "s32", 4: "u32"}
HAL_DIRS = {16: "IN", 32: "OUT", 48: "IO", 64: "RO", 192: "RW"}


def parse_halcmd(pins_text, sigs_text, params_text):
    """`halcmd -s show pin / sig / param` into {pins, sigs, params}:
    pins [name, type, dir, owner, value, signal or ""], sigs [name, type,
    value, driver or "", readers], params [name, type, dir, owner, value]."""
    pins, sigs, params = [], [], []
    for line in pins_text.splitlines():
        w = line.split()
        if len(w) >= 5:
            pins.append([w[4], w[1], w[2], w[0], w[3], w[6] if len(w) >= 7 and w[5] in ("<==", "==>", "<=>") else ""])
    for line in sigs_text.splitlines():
        w = line.split()
        if len(w) >= 3:
            driver, readers = "", []
            for arrow, pin in zip(w[3::2], w[4::2]):
                if arrow == "<==":
                    driver = pin
                else:
                    readers.append(pin)
            sigs.append([w[2], w[0], w[1], driver, readers])
    for line in params_text.splitlines():
        w = line.split()
        if len(w) >= 5:
            params.append([w[4], w[1], w[2], w[0], w[3]])
    return {"pins": pins, "sigs": sigs, "params": params}


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
        # an arm (genserkins, armkins, ...): joints that are not axes; the axes it has (X Y Z A B C), in AXES order
        kins = ((cfg.get("KINS", "KINEMATICS", "") or "").split() or [""])[0]
        self.arm = (cfg.get("MILLWEB", "MACHINE_TYPE", "") or "").lower() == "arm" or kins in ARM_KINS
        self.axes = "".join(a for a in AXES if a in coords)
        self.njoints = int(cfg.num("KINS", "JOINTS", len(coords)))
        if self.arm:
            self.joints = {}  # no joint is an axis
        self.teach_file = cfg.file("MILLWEB", "TEACH_FILE") or cfg.file("ARM6HMI", "TEACH_FILE") or os.path.join(self.nc_dir_guess(cfg), "teach.ngc")
        self.nc_dir = cfg.file("MILLWEB", "NC_DIR") or cfg.file("DISPLAY", "PROGRAM_PREFIX") or os.path.join(cfg.dir, "nc_files")
        self.setup_file = cfg.file("MILLWEB", "SETUP") or os.path.join(cfg.dir, "millweb-setup.json")
        self.views_file = cfg.file("MILLWEB", "DEBUG_VIEWS") or os.path.join(cfg.dir, "debug-views.json")
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
        self.enable_pin = cfg.get("MILLWEB", "ENABLE_PIN", "") or ""  # an arm's enabling switch: set while released (motion.jog-inhibit)
        self.last_run = "idle"
        self.wcs_unknown = False
        self.notes = []                 # for the event log, from the worker thread
        self.done_flag = False
        self.jog_active = {}

    @staticmethod
    def nc_dir_guess(cfg):
        return cfg.file("MILLWEB", "NC_DIR") or cfg.file("DISPLAY", "PROGRAM_PREFIX") or os.path.join(cfg.dir, "nc_files")

    # -------------------------------------------------------------- an arm
    def arm_info(self):
        """An arm's joints and axes (their limits from the INI), the files the console draws it from."""
        cfg = self.cfg
        jl = []
        for j in range(self.njoints):
            sec = "JOINT_%d" % j
            jl.append({"min": cfg.num(sec, "MIN_LIMIT", -180.0), "max": cfg.num(sec, "MAX_LIMIT", 180.0),
                       "vel": cfg.num(sec, "MAX_VELOCITY", 0.0) * 60, "acc": cfg.num(sec, "MAX_ACCELERATION", 0.0),
                       "type": (cfg.get(sec, "TYPE", "ANGULAR") or "ANGULAR").upper()})
        al = {a: {"min": cfg.num("AXIS_" + a, "MIN_LIMIT", 0.0), "max": cfg.num("AXIS_" + a, "MAX_LIMIT", 0.0),
                  "vel": cfg.num("AXIS_" + a, "MAX_VELOCITY", 0.0) * 60} for a in self.axes}
        return {"joints": self.njoints, "axes": self.axes, "jlim": jl, "alim": al, "teach": os.path.basename(self.teach_file),
                "grip": {"open": cfg.get("ARM6HMI", "GRIP_OPEN", "M65 P0"), "close": cfg.get("ARM6HMI", "GRIP_CLOSE", "M64 P0")},
                "tool_ready": cfg.get("ARM6HMI", "TOOL_READY", "") or "", "enable_switch": bool(self.enable_pin),
                "home": self.home_joints(),
                "angular_vel": cfg.num("DISPLAY", "DEFAULT_ANGULAR_VELOCITY", 20.0) * 60}

    def arm_model_paths(self):
        """(the 3D model, armguard's cell, the tool table): [MILLWEB] ARM_MODEL (else [ARM6HMI] MODEL, else
        linuxcnc/vis3d's arm6_changer.vis with a tool changer, arm6_lcnc.vis without), [ARMGUARD] CELL,
        [STMBL_ARM] TOOLS."""
        cfg = self.cfg
        model = cfg.file("MILLWEB", "ARM_MODEL") or cfg.file("ARM6HMI", "MODEL")
        tools = cfg.file("STMBL_ARM", "TOOLS")
        if not model:
            repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
            vis = cfg.file("ARM6HMI", "VIS3D") or os.path.join(repo, "linuxcnc", "vis3d")
            model = os.path.join(vis, "models", "arm6_changer.vis" if self._pin_exists("armtool.0.mounted") or tools else "arm6_lcnc.vis")
        return model, cfg.file("ARMGUARD", "CELL"), tools

    def arm_model(self):
        """The arm's model files for the browser: {"files": {name: text, or {"b64": STL}}, "model", "cell", "tools"},
        each name its path relative to the model's folder; in the texts, each include and mesh names its file as
        @<name> (vis3d resolves them from the including file's folder: the same files). Only what the INI names and
        what those files include or load."""
        model, cell, tools = self.arm_model_paths()
        if not model or not os.path.isfile(model):
            raise ValueError("no arm model: %s ([MILLWEB] ARM_MODEL)" % model)
        root = os.path.dirname(os.path.realpath(model))
        files, total = {}, [0]
        rel = lambda p: os.path.relpath(os.path.realpath(p), root).replace(os.sep, "/")  # noqa: E731

        def add(path, depth=0):
            name = rel(path)
            if name in files or depth > 8:
                return
            if path.lower().endswith(".stl"):
                data = open(path, "rb").read()
                total[0] += len(data)
                if total[0] > 40 * 1024 * 1024:
                    raise ValueError("the arm's model files: over 40 MB")
                files[name] = {"b64": base64.b64encode(data).decode()}
                return
            text = open(path, errors="replace").read()
            if len(text) > 2 * 1024 * 1024:
                raise ValueError("%s: over 2 MB" % name)
            out, refs = [], []
            for line in text.splitlines():
                w = line.split("#", 1)[0].split()
                ref = w[1] if len(w) >= 2 and w[0] == "include" else w[3] if len(w) >= 4 and w[0] == "part" and w[2] == "mesh" else None
                if ref:
                    p = ref if os.path.isabs(ref) else os.path.join(os.path.dirname(path), ref)
                    if os.path.isfile(p):  # the browser has names, not paths: @ and the file's name
                        line = line.replace(ref, "@" + rel(p), 1)
                        refs.append(p)
                out.append(line)
            files[name] = "\n".join(out) + "\n"
            for p in refs:
                add(p, depth + 1)
        add(model)
        for p in (cell, tools):
            if p and os.path.isfile(p):
                add(p)
        return {"files": files, "model": rel(model), "cell": rel(cell) if cell and os.path.isfile(cell) else None,
                "tools": rel(tools) if tools and os.path.isfile(tools) else None}

    def arm_slow(self):
        """The arm's components' pins (those loaded)."""
        out = {}
        for k, (pre, pins) in ARM_PINS.items():
            if self._pin_exists(pre + pins[0]):
                out[k] = {p.replace("-", "_"): self._pin(pre + p) for p in pins}
        kt = self._pin("armkins.tool")
        if kt is not None:
            out["kins_tool"] = kt
        if self.enable_pin:
            out["enable"] = {"held": not self.enable_released()}
        return out

    def enable_released(self):
        """an arm's enabling switch: released (no switch: never)"""
        return bool(self.enable_pin) and bool(self._pin(self.enable_pin))

    TEACH_HEAD = "(the teach points: the console's Record point adds a G1 line before the M2)\nG21 G90\n"

    def teach_points(self):
        """the teach file's points (its G0/G1 lines), in order"""
        try:
            with open(self.teach_file) as fh:
                return [x.strip() for x in fh if re.match(r"^G[01] ", x)]
        except FileNotFoundError:
            return []

    def _teach_write(self, pts):
        """the teach file as a program: its header, the points, M2"""
        os.makedirs(os.path.dirname(self.teach_file) or ".", exist_ok=True)
        tmp = self.teach_file + ".tmp"
        with open(tmp, "w") as fh:
            fh.write(self.TEACH_HEAD + "".join(p + "\n" for p in pts) + "M2\n")
        os.replace(tmp, self.teach_file)

    def cmd_teach_record(self, rate=None):
        """Record point: the arm's position (X Y Z A B C in the work coordinates, as a program runs it) as a G1
        line after the teach file's last point."""
        self.ws.poll()
        s = self.ws
        p = [s.actual_position[i] - s.g5x_offset[i] - s.g92_offset[i] for i in range(len(AXES))]
        f = int(rate or self.cfg.num("DISPLAY", "DEFAULT_LINEAR_VELOCITY", 50.0) * 60)
        line = "G1 " + " ".join("%s%.3f" % (a, p[AXES.index(a)]) for a in self.axes) + " F%d" % f
        pts = self.teach_points() + [line]
        self._teach_write(pts)
        return {"line": line, "points": len(pts), "file": os.path.basename(self.teach_file)}

    def cmd_teach(self, action, index=None):
        """the teach points: list, delete (index), up / down (index: moved one place), clear"""
        pts = self.teach_points()
        if action != "list":
            if action == "clear":
                pts = []
            elif action in ("delete", "up", "down"):
                if not isinstance(index, int) or not 0 <= index < len(pts):
                    raise ValueError("no point %r" % (index,))
                if action == "delete":
                    del pts[index]
                else:
                    k = index + (-1 if action == "up" else 1)
                    if 0 <= k < len(pts):
                        pts[index], pts[k] = pts[k], pts[index]
            else:
                raise ValueError("teach: list, delete, up, down or clear")
            self._teach_write(pts)
        return {"points": pts, "file": os.path.basename(self.teach_file)}

    def home_joints(self):
        """the arm's home pose (joint values) for Go home: [ARM6HMI] HOME_JOINTS, else none"""
        v = (self.cfg.get("ARM6HMI", "HOME_JOINTS", "") or "").split()
        try:
            q = [float(x) for x in v]
        except ValueError:
            return None
        return q if len(q) == self.njoints else None

    def cmd_arm_home(self, rate=None):
        """Go home: every joint to its home value at once (joint mode, one increment each, their speeds such that
        they arrive together, at most the jog rate [°/min] and each joint's MAX_VELOCITY)"""
        q = self.home_joints()
        if q is None:
            return "no home pose ([ARM6HMI] HOME_JOINTS, a value a joint)"
        if self.enable_released():
            return "the enabling switch is released: hold it to move"
        err = self._ready()
        if err:
            return err
        self._mode(self.lc.MODE_MANUAL)
        err = self._motion_mode(True)
        if err:
            return err
        self.ws.poll()
        cur = [self.ws.joint[j]["input"] for j in range(self.njoints)]
        d = [q[j] - cur[j] for j in range(self.njoints)]
        vmax = [min(abs(rate or 1e9) / 60.0, self.cfg.num("JOINT_%d" % j, "MAX_VELOCITY", 30.0)) for j in range(self.njoints)]
        t = max([abs(d[j]) / max(vmax[j], 1e-6) for j in range(self.njoints)] + [1e-6])
        for j in range(self.njoints):
            if abs(d[j]) > 1e-4:
                self.c.jog(self.lc.JOG_INCREMENT, True, j, max(abs(d[j]) / t, 1e-3) * (1 if d[j] > 0 else -1), abs(d[j]))
        return None

    # -------------------------------------------------------------- info
    def info(self):
        cfg = self.cfg
        lim = {}
        for a in "XYZ":
            sec = "AXIS_" + a
            lim[a] = {"min": cfg.num(sec, "MIN_LIMIT", 0.0), "max": cfg.num(sec, "MAX_LIMIT", 0.0),
                      "vel": cfg.num(sec, "MAX_VELOCITY", 0.0) * 60, "acc": cfg.num(sec, "MAX_ACCELERATION", 0.0)}
        mtype = (cfg.get("MILLWEB", "MACHINE_TYPE", "gantry") or "gantry").lower()
        if (cfg.get("DISPLAY", "LATHE", "0") or "0").strip().lower() in ("1", "yes", "true"):
            mtype = "lathe"
        if self.arm:
            mtype = "arm"
        return {"name": cfg.get("EMC", "MACHINE", "LinuxCNC"), "coords": self.coords, "limits": lim,
                "type": mtype if mtype in ("gantry", "bridge", "lathe", "arm") else "gantry",
                "arm": self.arm_info() if self.arm else None,
                "lathe": {"post": "back" if (cfg.get("DISPLAY", "BACK_TOOL_LATHE", "0") or "0").strip().lower() in ("1", "yes", "true") else "front"}
                if mtype == "lathe" else None,
                "maxv": self.maxv * 60, "units": "mm", "nc_dir": self.nc_dir,
                "spindle_max": cfg.num("SPINDLE_0", "MAX_FORWARD_VELOCITY", 24000.0),
                "jog_default": cfg.num("DISPLAY", "DEFAULT_LINEAR_VELOCITY", 30.0) * 60,
                "toolsetter": bool(self.toolsetter), "pcomp": self._pin_exists(self.pcomp + ".shaper"),
                "drives": [d[0] for d in self.drives], "pins": self.pins, "wcs": self.WCS, "ovr_max": self.ovr_max,
                "hal_write": (cfg.get("MILLWEB", "HAL_WRITE", "no") or "no").lower() in ("yes", "1", "true")}

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
        if self.arm:  # its joints aren't axes: homed when all are
            homed = [bool(all(s.homed[j] for j in range(self.njoints)))] * 3
        else:
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
            "rot": round(s.rotation_xy, 4), "tool": s.tool_in_spindle, "tlo": round(tlo[2], 4), "tlox": round(tlo[0], 4), "wcs_ok": s.g5x_index > 0,
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
            **(self.arm_status(s) if self.arm else {}),
        }

    def arm_status(self, s):
        """An arm's: every axis (work and machine), every joint (position, homed, following error), the jog mode."""
        g5x, g92 = s.g5x_offset, s.g92_offset
        ix = [AXES.index(a) for a in self.axes]
        js = s.joint[:self.njoints]
        return {"ax": [round(s.actual_position[i] - g5x[i] - g92[i], 4) for i in ix],
                "axm": [round(s.actual_position[i], 4) for i in ix],
                "j": [round(j["input"], 4) for j in js], "jh": [bool(j["homed"]) for j in js],
                "jfe": [round(j["ferror_current"], 5) for j in js],
                "jmode": {self.lc.TRAJ_MODE_TELEOP: "world", self.lc.TRAJ_MODE_FREE: "joint"}.get(s.motion_mode, "coord")}

    def slow(self):
        out = {"pins": {p: self._pin(p) for p in self.pins}}
        out["drives"] = [{"n": d[0], "cur": self._pin(d[1]) if len(d) > 1 else None, "pwr": self._pin(d[2]) if len(d) > 2 else None,
                          "fault": self._pin(d[3]) if len(d) > 3 else None} for d in self.drives]
        for k, key in (("load", "SPINDLE_LOAD_PIN"), ("bus", "BUS_PIN"), ("yaw", "YAW_PIN"), ("contour", "CONTOUR_PIN"), ("remote", "REMOTE_ENABLE_PIN")):
            out[k] = self._pin(self.sig[key])
        if out["pins"] == {}:
            del out["pins"]
        if self._pin_exists("lathe.turret.station"):  # a lathe's turret (linuxcnc/lathe): where it is, its fault
            out["turret"] = {k: self._pin("lathe.turret." + k) for k in ("station", "stations", "homed", "busy", "fault", "error")}
        if self.arm:
            out["arm"] = self.arm_slow()
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

    def _wait(self, pred, timeout):
        """the worker's status until pred(it) holds (True), or timeout [s] (False)"""
        t = time.time() + timeout
        while True:
            self.ws.poll()
            if pred(self.ws):
                return True
            if time.time() > t:
                return False
            time.sleep(0.02)

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
        elif re.match(r"^[Jj]\d+$", str(axis)):  # a joint (an arm's)
            j = int(str(axis)[1:])
            if not 0 <= j < self.njoints:
                return "no joint %s" % axis
            self.c.home(j)
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

    def _jog_target(self, axis):
        """(joint jog, index): "J3" a joint (joint mode: an arm's, or any machine's before homing); "X".."W" an
        axis the machine has (world mode); "TX".."TC" an arm's tool frame ("tool", 0..5)."""
        a = str(axis).upper()
        if re.match(r"^T[XYZABC]$", a):
            if not self.arm:
                raise ValueError("no tool frame: not an arm")
            return "tool", "XYZABC".index(a[1])
        if re.match(r"^J\d+$", a):
            j = int(a[1:])
            if not 0 <= j < self.njoints:
                raise ValueError("no joint %s" % axis)
            return True, j
        if a not in self.axes or len(a) != 1:
            raise ValueError("no axis %s" % axis)
        return False, AXES.index(a)

    def _motion_mode(self, joint):
        """joint mode (free) or world (teleop), and wait for motion to take it (up to 1 s; teleop needs the joints homed)"""
        lc = self.lc
        want = lc.TRAJ_MODE_FREE if joint else lc.TRAJ_MODE_TELEOP
        self.ws.poll()
        if self.ws.motion_mode == want:
            return None
        self.c.teleop_enable(0 if joint else 1)
        self.c.wait_complete(1.0)
        if not self._wait(lambda s: s.motion_mode == want, 1.0):
            return "no %s mode%s" % ("joint" if joint else "world", "" if joint else " (home first)")
        return None

    def cmd_jog(self, axis, direction, inc, rate):
        """inc > 0: one increment; inc == 0: continuous (stopped by jog_stop
        or by millweb's deadman). axis: X..W (world), J0.. (a joint: joint mode)."""
        lc = self.lc
        try:
            joint, a = self._jog_target(axis)
        except ValueError as e:
            return str(e)
        if self.arm and self.enable_released():
            return "the enabling switch is released: hold it to jog"
        if joint == "tool":
            return self._jog_tool(a, direction, inc, rate)
        err = self._ready(need_homed=not joint)
        if err:
            return err
        self._mode(lc.MODE_MANUAL)
        err = self._motion_mode(joint) if (joint or self.arm) else None
        if err:
            return err
        if not joint and not self.arm:
            self.ws.poll()
            if self.ws.motion_mode != lc.TRAJ_MODE_TELEOP:
                self.c.teleop_enable(1)
                self.c.wait_complete(1.0)
        vel = abs(rate) / 60.0
        if inc > 0:
            self.c.jog(lc.JOG_INCREMENT, joint, a, vel * (1 if direction > 0 else -1), inc)
        else:
            self.c.jog(lc.JOG_CONTINUOUS, joint, a, vel * (1 if direction > 0 else -1))
            self.jog_active[(joint, a)] = True
        return None

    def cmd_jog_stop(self, axis=None):
        try:
            keys = [self._jog_target(axis)] if axis else list(self.jog_active)
        except ValueError as e:
            return str(e)
        for joint, a in keys:
            if joint == "tool":  # a tool-frame jog is an MDI move: abort it
                if self.jog_active.pop((joint, a), None):
                    self.cu.abort()
                continue
            self.cu.jog(self.lc.JOG_STOP, joint, a)
            self.jog_active.pop((joint, a), None)
        return None

    TOOL_JOG_TURN = 45.0  # a held tool-frame rotation: at most this far [deg] in one move

    def _jog_tool(self, k, direction, inc, rate):
        """An arm's jog in its tool frame (k: 0..2 along the tool's X Y Z, 3..5 about them), as one G53 G1 MDI move
        from the commanded pose: inc > 0 that far [mm or deg]; inc == 0 (held) to the axes' limits along the line
        (a rotation: TOOL_JOG_TURN), stopped by jog_stop (abort) or the deadman. LinuxCNC interpolates A B C
        linearly: a rotation keeps the tool point, its path between the ends is the A B C line's."""
        err = self._ready()
        if err:
            return err
        self.ws.poll()
        p = list(self.ws.position[:6])
        R = rpy_matrix(*p[3:6])
        s = 1.0 if direction > 0 else -1.0
        xyz, abc = p[:3], p[3:6]
        if k < 3:
            u = [R[r][k] * s for r in range(3)]
            d = float(inc) if inc > 0 else self._reach_along(xyz, u)
            if d <= 1e-6:
                return "the tool %s%s: at the axes' limits" % ("+" if s > 0 else "-", "XYZ"[k])
            xyz = [xyz[r] + u[r] * d for r in range(3)]
        else:
            ang = float(inc) if inc > 0 else self.TOOL_JOG_TURN
            R2 = mat_mul(R, axis_rot(k - 3, s * ang))
            abc = [unwrap_deg(x, ref) for x, ref in zip(rpy_of(R2), abc)]
        words = dict(zip("XYZABC", xyz + abc))
        line = "G53 G1 " + " ".join("%s%.4f" % (a, words[a]) for a in self.axes if a in words) + " F%.1f" % max(1.0, abs(rate))
        err = self.mdi(line)
        if not err and inc <= 0:
            self.jog_active[("tool", k)] = True
        return err

    def _reach_along(self, xyz, u, cap=2000.0):
        """how far [mm] from xyz along the unit vector u the [AXIS_*] limits allow (at most cap)"""
        d = cap
        for r, a in enumerate("XYZ"):
            if abs(u[r]) < 1e-9 or a not in self.axes:
                continue
            lo, hi = self.cfg.num("AXIS_" + a, "MIN_LIMIT", -1e9), self.cfg.num("AXIS_" + a, "MAX_LIMIT", 1e9)
            d = min(d, ((hi if u[r] > 0 else lo) - xyz[r]) / u[r])
        return max(0.0, d - 0.01)

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
            c = {"t": int(t["t"]), "p": int(t.get("p", t["t"])), "d": float(t.get("d", 0)), "z": float(t.get("z", 0)),
                 "c": str(t.get("c", ""))[:120].replace("{", "(").replace("}", ")")}
            if t.get("fl") not in (None, "", 0):
                if not 1 <= int(t["fl"]) <= 64:
                    return "a tool's flutes: 1 to 64"
                c["fl"] = int(t["fl"])
            if t.get("ins"):
                if str(t["ins"]) not in INSERTS:
                    return "a tool's insert: " + ", ".join(INSERTS)
                c["ins"] = str(t["ins"])
            if t.get("ty"):
                if str(t["ty"]) not in MILL_TYPES:
                    return "a tool's type: " + ", ".join(MILL_TYPES)
                c["ty"] = str(t["ty"])
            for k in ("w", "cl", "cr", "so", "hd"):
                if t.get(k) not in (None, "", 0):
                    if not 0 < float(t[k]) <= 500:
                        return "a tool's %s: above 0, at most 500 mm" % {"w": "width", "cl": "cutting length", "cr": "corner radius",
                                                                        "so": "stick-out", "hd": "holder diameter"}[k]
                    c[k] = float(t[k])
            if t.get("ang") not in (None, "", 0):
                if not 1 <= float(t["ang"]) <= 179:
                    return "a tool's point angle: 1 to 179°"
                c["ang"] = float(t["ang"])
            for k in ("x", "i", "j"):  # a lathe's
                if t.get(k) is not None:
                    c[k] = float(t[k])
            if t.get("q"):
                if not 0 <= int(t["q"]) <= 9:
                    return "a tool's orientation: 0 to 9"
                c["q"] = int(t["q"])
            clean.append(c)
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
    MODEL = re.compile(r"\.(stl|obj)$", re.I)  # 3D models of fixtures and workpieces
    MAX_DEPTH, MAX_ENTRIES = 8, 5000
    MAX_MODEL = 5 * 1024 * 1024

    def _safe(self, name, folder=False, kind="program"):
        """The absolute path of a program (kind "model": a 3D model; "file":
        either; folder=True: a folder, "" the program folder itself), from its
        path in the program folder."""
        rel = str(name).replace("\\", "/").strip("/")
        parts = [p for p in rel.split("/") if p] if rel else []
        if not folder and not parts:
            raise ValueError("no program name")
        if len(parts) > self.MAX_DEPTH or any(not self.PART.match(p) or len(p) > 120 for p in parts):
            raise ValueError("names: letters, digits, space . _ - (not starting with a dot), folders separated by /")
        if not folder:
            ok = {"program": (self.PROGRAM,), "model": (self.MODEL,), "file": (self.PROGRAM, self.MODEL)}[kind]
            if not any(r.search(parts[-1]) for r in ok):
                raise ValueError({"program": "programs end in .ngc, .nc, .tap or .gcode", "model": "3D models: .stl or .obj",
                                  "file": "programs (.ngc .nc .tap .gcode) or 3D models (.stl .obj)"}[kind])
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
        """Every program and 3D model in the program folder and its folders
        (hidden ones and links out of it left out): files [{name, size,
        mtime, kind: program | model}], dirs [name]."""
        files, dirs = [], []
        root = os.path.realpath(self.nc_dir)
        for top, dnames, fnames in os.walk(root):
            depth = 0 if top == root else os.path.relpath(top, root).count(os.sep) + 1
            dnames[:] = sorted(d for d in dnames if self.PART.match(d) and depth < self.MAX_DEPTH
                               and os.path.realpath(os.path.join(top, d)).startswith(root + os.sep))
            rel = "" if top == root else os.path.relpath(top, root).replace(os.sep, "/") + "/"
            dirs += [rel + d for d in dnames]
            for n in sorted(fnames):
                kind = "program" if self.PROGRAM.search(n) else "model" if self.MODEL.search(n) else None
                if self.PART.match(n) and kind:
                    try:
                        st = os.stat(os.path.join(top, n))
                    except OSError:
                        continue
                    files.append({"name": rel + n, "size": st.st_size, "mtime": int(st.st_mtime), "kind": kind})
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
        """A program or a 3D model."""
        p = self._safe(name, kind="file")
        if self._loaded() == os.path.realpath(p):
            return "that program is loaded"
        if not os.path.isfile(p):
            return "no such file"
        used = self._in_setup(p)
        if used:
            return "the 3D setup uses it (%s): remove it there first" % used
        os.remove(p)
        return None

    def cmd_rename(self, name, to):
        """A program or a folder to a new name or place (in the program folder);
        a program keeps a program's extension."""
        folder = os.path.isdir(self._safe(name, folder=True))
        src, dst = self._safe(name, folder, "file"), self._safe(to, folder, "file")
        if not folder and bool(self.MODEL.search(src)) != bool(self.MODEL.search(dst)):
            return "a program stays a program, a model a model (keep the extension)"
        used = self._in_setup(src)
        if used:
            return "the 3D setup uses %s: remove it there first" % used
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

    # 3D models and the setup (what stands on the table, for the 3D view)
    def cmd_model_save(self, name, data, overwrite=True):
        p = self._safe(name, kind="model")
        if len(data) > self.MAX_MODEL:
            return "too large: 3D models up to %d MB" % (self.MAX_MODEL >> 20)
        err = model_check(name, data)
        if err:
            return err
        if not os.path.isdir(os.path.dirname(p)):
            return "no such folder"
        if not overwrite and os.path.exists(p):
            return "a model of that name is there already"
        with open(p + ".tmp", "wb") as f:
            f.write(data)
        os.replace(p + ".tmp", p)
        return None

    def model_read(self, name):
        p = self._safe(name, kind="model")
        with open(p, "rb") as f:
            return f.read(), int(os.stat(p).st_mtime)

    def setup(self):
        """The objects in the 3D view (setup_check's form); the built-in vise
        and stock when there is no setup file yet."""
        try:
            with open(self.setup_file) as f:
                return json.load(f)["objects"]
        except (OSError, ValueError, KeyError, TypeError):
            return [dict(o) for o in DEFAULT_SETUP]

    def setup_lathe(self):
        """A lathe's stock (lathe_check's form), or None."""
        try:
            with open(self.setup_file) as f:
                return json.load(f).get("lathe")
        except (OSError, ValueError, AttributeError):
            return None

    def setup_tools(self):
        """The tools' 3D models (tools_check's form); none when there is no setup file."""
        try:
            with open(self.setup_file) as f:
                t = json.load(f).get("tools", {})
            return t if isinstance(t, dict) else {}
        except (OSError, ValueError, AttributeError):
            return {}

    def cmd_setup_save(self, objects, tools=None, lathe=None):
        """objects: the setup's; tools: the tools' models (None: as they are); lathe: a lathe's stock (None: as it is)."""
        objects = setup_check(objects, lambda n: self._safe(n, kind="model"))
        tools = self.setup_tools() if tools is None else tools_check(tools, lambda n: self._safe(n, kind="model"))
        lathe = self.setup_lathe() if lathe is None else lathe_check(lathe)
        with open(self.setup_file + ".tmp", "w") as f:
            json.dump({"objects": objects, "tools": tools, "lathe": lathe}, f, indent=1)
        os.replace(self.setup_file + ".tmp", self.setup_file)
        return None

    def _in_setup(self, path):
        """The setup's object that uses this model (or a model in this folder), or None."""
        real = os.path.realpath(path)
        for o in self.setup() + [dict(t, name="T" + k) for k, t in self.setup_tools().items()]:
            src = o.get("src", "")
            if src.startswith("model:"):
                try:
                    m = os.path.realpath(self._safe(src[6:], kind="model"))
                except ValueError:
                    continue
                if m == real or m.startswith(real + os.sep):
                    return o.get("name") or src[6:]
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

    # HAL, for the HAL page (halshow, halmeter)
    def hal_list(self):
        """Every pin, signal and parameter with its links (halcmd show), or
        without links from the hal module when halcmd isn't there."""
        try:
            run = lambda what: subprocess.run(["halcmd", "-s", "show", what], capture_output=True, text=True, timeout=10).stdout  # noqa: E731
            out = parse_halcmd(run("pin"), run("sig"), run("param"))
            if out["pins"]:
                return out
        except (OSError, subprocess.SubprocessError):
            pass
        if not self.hal or not hasattr(self.hal, "get_info_pins"):
            raise ValueError("HAL isn't available to millweb")
        v = lambda x: self._fmt(x)  # noqa: E731
        return {"pins": [[p["NAME"], HAL_TYPES.get(p["TYPE"], "?"), HAL_DIRS.get(p["DIRECTION"], "?"), "", v(p["VALUE"]), ""] for p in self.hal.get_info_pins()],
                "sigs": [[g["NAME"], HAL_TYPES.get(g["TYPE"], "?"), v(g["VALUE"]), g.get("DRIVER") or "", []] for g in self.hal.get_info_signals()],
                "params": [[q["NAME"], "bit" if isinstance(q["VALUE"], bool) else "float" if isinstance(q["VALUE"], float) else "s32",
                            HAL_DIRS.get(q["DIRECTION"], "?"), "", v(q["VALUE"])] for q in self.hal.get_info_params()]}

    @staticmethod
    def _fmt(v):
        return v if isinstance(v, bool) else round(v, 6) if isinstance(v, float) else v

    def hal_values(self, names):
        """{name: value} for pins, signals or parameters (None: no such item)."""
        out = {}
        for n in names:
            try:
                out[n] = self._fmt(self.hal.get_value(n))
            except Exception:
                out[n] = None
        return out

    def cmd_hal_set(self, name, value, kind):
        """setp / sets with [MILLWEB] HAL_WRITE = yes: an unconnected input
        pin, a writable parameter, or a signal without a driver."""
        if (self.cfg.get("MILLWEB", "HAL_WRITE", "no") or "no").lower() not in ("yes", "1", "true"):
            return "setting HAL values is off ([MILLWEB] HAL_WRITE = yes turns it on)"
        name, value = str(name), str(value).strip()
        if not re.match(r"^[\w.\-]+$", name) or not re.match(r"^[-+\w.]+$", value):
            return "a HAL name and a plain value"
        cmd = {"pin": "setp", "param": "setp", "sig": "sets"}.get(kind)
        if not cmd:
            return "pin, param or sig"
        r = subprocess.run(["halcmd", cmd, name, value], capture_output=True, text=True, timeout=10)
        return (r.stderr or r.stdout).strip() or "halcmd %s failed" % cmd if r.returncode else None

    # the Debug page's saved views (watch lists and scope set-ups), for every browser
    def views(self):
        try:
            with open(self.views_file) as f:
                return json.load(f)["views"]
        except (OSError, ValueError, KeyError, TypeError):
            return []

    def cmd_views_save(self, views):
        if not isinstance(views, list) or len(views) > 64:
            raise ValueError("views: a list of at most 64")
        clean = []
        for v in views:
            if not isinstance(v, dict) or not str(v.get("name", "")).strip():
                raise ValueError("a view has a name")
            watch = [{"kind": w.get("kind") if w.get("kind") in ("pin", "sig", "param") else "pin", "name": str(w.get("name", ""))[:120]}
                     for w in (v.get("watch") or [])[:64] if isinstance(w, dict) and w.get("name")]
            scope = v.get("scope") if isinstance(v.get("scope"), dict) else {}
            text = json.dumps(scope)
            if len(text) > 8000:
                raise ValueError("a view's scope set-up is too large")
            clean.append({"name": str(v["name"]).strip()[:60], "watch": watch, "scope": json.loads(text)})
        with open(self.views_file + ".tmp", "w") as f:
            json.dump({"views": clean}, f, indent=1)
        os.replace(self.views_file + ".tmp", self.views_file)
        return None

    def cmd_pcomp(self, k, v):
        if k not in ("shaper", "defl", "shrink", "servo", "jerk-ms"):
            return "unknown pcomp part"
        if not self.info_pcomp():
            return "pcomp is not loaded"
        self.hal.set_p("%s.%s" % (self.pcomp, k), str(v))
        return None
