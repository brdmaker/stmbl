"""A machine described in one file, machine.json, and the LinuxCNC
configuration made from it (the INI and the HAL), for millweb-supervisor and
the console's Configure page.

The machine directory:

    machine.json        the machine: board, bitstream, axes, joints and their drives, I/O, the console
    custom.hal          optional: more HAL, loaded after the generated machine.hal
    bitstreams/         FPGA bitstreams to load before LinuxCNC starts
    history/            every saved machine.json (and custom.hal), newest last
    run/                made at each start: mill.ini, machine.hal; the drives' consoles; status
    logs/               LinuxCNC's output, the last start's
    tool.tbl, linuxcnc.var, nc_files/, millweb-setup.json, debug-views.json   kept across starts

validate() checks a machine and fills in what it leaves out; generate()
writes the INI and HAL text. Nothing here talks to LinuxCNC.
"""
import copy
import datetime
import difflib
import json
import math
import os
import re
import shutil

VERSION = 1
AXES = "XYZABCUVW"
BOARDS = ("none", "hm2_eth", "hm2_spix", "hm2_rpspi", "hm2_soc_ol")
DRIVES = ("sim", "stmbl_sserial", "stmbl_can", "stmbl_sim", "stmbl_arm")
GRIPPERS = ("none", "stmbl_arm", "stmbl_sserial", "stmbl_can")
REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
SPINDLES = ("none", "sim", "hal", "stmbl_sserial", "stmbl_can")
TURRETS = ("none", "sim", "stepper")
LOADERS = ("none", "fpga_manager", "command")
HOMING = ("absolute", "immediate", "switch", "index")
# the machine's layout: a gantry (the bridge moves in Y over a fixed table, one or two Y joints) or a fixed
# bridge over a table that moves in Y (Scripta CN 720 style: one joint an axis); the console draws it
# or a lathe: X (the cross slide, its DRO a diameter) and Z (along the spindle), the tools on a post in front of
# or behind the spindle's axis; or an arm: joints that aren't axes (armkins), the tool point as X Y Z A B C
TYPES = ("gantry", "bridge", "lathe", "arm")
CONSOLE_KEYS = {  # [MILLWEB] keys the console section may set as extra (the rest have fields of their own)
    "POLL_HZ", "HAL_HZ", "PREVIEW_SEGS", "PREVIEW_TIMEOUT", "PREVIEW_NICE", "DRIVE_POLL_MS", "CERT", "KEY", "PCOMP",
    "SPINDLE_LOAD_PIN", "BUS_PIN", "YAW_PIN", "CONTOUR_PIN", "AT_SPEED_PIN", "REMOTE_ENABLE_PIN", "AIR_PIN"}
HALNAME = re.compile(r"^[A-Za-z0-9_.:\-]+$")      # a HAL pin or signal name
PLAIN = re.compile(r"^[^\"\n\r\\]*$")              # a parameter string: no quotes, newlines or backslashes

# a machine with nothing connected: what the Configure page starts from
DEFAULT = {
    "version": VERSION,
    "name": "My mill",
    "type": "gantry",
    "units": "mm",
    "servo_period_ns": 1000000,
    "board": {"type": "none", "name": "", "params": "", "config": "", "watchdog_ms": 50},
    "bitstream": {"loader": "none", "file": "", "fpga": "fpga0", "command": "", "reload": "changed"},
    "can": {"spi": [], "discover": True, "spi_hz": 10000000},
    "axes": {
        "X": {"min": 0, "max": 500, "max_vel": 200, "max_acc": 2000},
        "Y": {"min": 0, "max": 500, "max_vel": 200, "max_acc": 2000},
        "Z": {"min": -150, "max": 0, "max_vel": 100, "max_acc": 1000},
    },
    "joints": [
        {"axis": "X", "drive": {"type": "sim"}, "ferror": 1, "min_ferror": 0.25, "home": {"method": "absolute", "pos": 0, "sequence": 1}},
        {"axis": "Y", "drive": {"type": "sim"}, "ferror": 1, "min_ferror": 0.25, "home": {"method": "absolute", "pos": 0, "sequence": 1}},
        {"axis": "Z", "drive": {"type": "sim"}, "ferror": 1, "min_ferror": 0.25, "home": {"method": "absolute", "pos": 0, "sequence": 0}},
    ],
    "estop": {"input": "", "invert": False},
    "probe": {"type": "none", "input": "", "invert": False, "sim_z": -100},
    # sim: answers at once; hal: its pins (on, forward, speed out in rpm, at-speed in); none
    # revs, index, speed_in: its encoder (spindle.0.revs, index-enable, speed-in in rps), for threading and G95
    # stmbl_sserial (port, channel), stmbl_can (bus, node): an stmbl drive in velocity mode (linuxcnc/lathe's
    # lathe.spindle): ratio motor turns a spindle turn; index_sensor: a once-a-turn sensor's pin (blank: the motor's zero)
    "spindle": {"type": "sim", "max_rpm": 24000, "min_rpm": 0, "on": "", "forward": "", "reverse": "", "speed": "", "at_speed": "",
                "revs": "", "index": "", "speed_in": "", "port": 0, "channel": 0, "bus": 0, "node": 1, "ratio": 1,
                "index_sensor": "", "console": True},
    # type lathe: post front (the tools between the operator and the spindle's axis: +X toward the operator) or back;
    # diameter: X in diameter (G7) from the start
    # turret (linuxcnc/lathe's lathe.turret): none, sim, or stepper (a stepgen's HAL prefix, steps a turret turn; index:
    # the once-a-turn pulse's pin); it turns forward to a station and locks running backwards (turns, turns/s, s)
    "lathe": {"post": "front", "diameter": True,
              "turret": {"type": "none", "stations": 6, "stepgen": "", "steps": 3200, "index": "", "index_invert": False,
                         "offset": 0, "overtravel": 0.03, "lock": 0.01, "speed": 1.0, "lock_speed": 0.2, "acc": 10,
                         "dwell": 0.3, "timeout": 10, "slip": 0.02, "sim_index_at": 0.37}},
    # a rack tool changer, M6 remapped to run/toolchange.ngc: tool n sits in pocket n (1..pockets), the pockets in a
    # row from pocket 1 at (x, y) by (dx, dy); the holder enters its fork sideways along +Y by slide (0: from above).
    # unclamp drives the drawbar (true: released), clamped / released are its sensors (blank: a dwell instead), air
    # blows the cone while the spindle is empty
    "atc": {"type": "none", "pockets": 8, "x": 0, "y": 0, "dx": 50, "dy": 0, "z_clear": 0, "z_pocket": -100, "slide": 0,
            "feed": 1000, "unclamp": "", "clamped": "", "released": "", "air": "", "dwell": 0.5, "timeout": 3},
    "toolsetter": None,
    "display": {"max_feed_override": 2.0, "max_spindle_override": 1.5, "min_spindle_override": 0.5},
    "console": {"port": 8080, "host": "0.0.0.0", "token": "", "on_control_loss": "continue", "hal_write": False,
                "drive_write": False, "cpus": "", "pins": [], "extra": {}},
    # the machine in the console's 3D view: the built-in gantry, and/or parts (STL, OBJ in models/) that move with the axes
    "model": {"builtin": True, "parts": []},
    # the stmbl drives' firmware (docs/firmware_versions.md): fw null expects this checkout's STMBL_FW_VERSION;
    # block: a drive with another firmware (or an HV board, or a config needing a patch) keeps the machine off
    "stmbl": {"fw": None, "block": True},
    # type arm (linuxcnc/armkins and the arm's components). Files: in the machine folder, absolute, or {repo}/... (this
    # checkout). model: the arm's model (arm_sim's .arm: dh, limits, bodies, masses) for armkins, armtool and,
    # unless collide_model, armguard, armcollide and armgrip; vis: the console's 3D model (blank: the model's bodies);
    # cell: armguard's (blank: no guard); collide: armcollide (needs drives that report current); tools: the tool
    # changer's table (blank: none; armkins puts X Y Z on the mounted tool's TCP, armtool watches the changer, M6
    # changes tools); quota: powerquota's config (blank: none); gripper: armgrip on a drive (mm of finger gap);
    # enable: the enabling switch (held: true, or invert); released, no jog runs (hold_all: no motion at all);
    # sim: stmbl_arm's arm file and drive library (joints whose drive is stmbl_arm: the simulated arm); home: the
    # console's Go home pose, a value a joint [deg] (empty: none)
    "arm": {"model": "", "vis": "", "cell": "", "collide": False, "collide_model": "", "tools": "", "quota": "", "home": [],
            "gripper": {"type": "none", "port": 0, "channel": 0, "bus": 0, "node": 1},
            "changer": {"valve": "", "locked": "", "unlocked": "", "present": "", "id": "", "ready": ""},
            "enable": {"input": "", "invert": False, "hold_all": False},
            "sim": {"arm": "", "lib": "{repo}/host/arm/libstmbl_drive.so"}},
}
MOVES = re.compile(r"^(?:-?([XYZ]))?(?:-?([XYZ]))?(?:-?([XYZ]))?S?$")  # S: turns with the spindle (a lathe's chuck)


class Invalid(ValueError):
    """A machine that can't be made into a configuration: errors as [(where, what)]."""

    def __init__(self, errors):
        self.errors = errors
        super().__init__("; ".join("%s: %s" % e if e[0] else e[1] for e in errors))


# ------------------------------------------------------------------ checking
def _num(v, where, errs, lo=None, hi=None, integer=False):
    if isinstance(v, bool) or not isinstance(v, (int, float)):
        errs.append((where, "a number"))
        return None
    if integer and int(v) != v:
        errs.append((where, "a whole number"))
    if (lo is not None and v < lo) or (hi is not None and v > hi):
        errs.append((where, "between %s and %s" % (lo, hi)))
    return v


def _merge(base, over):
    """over on top of base, one level of dicts deep (lists replaced)."""
    out = copy.deepcopy(base)
    for k, v in (over or {}).items():
        if isinstance(v, dict) and isinstance(out.get(k), dict):
            out[k] = dict(out[k], **v)
        else:
            out[k] = copy.deepcopy(v)
    return out


def joint_names(m):
    """Each joint's name: its axis, numbered when an axis has more than one (X, Y1, Y2, Z); an arm's J1, J2 …"""
    if m.get("type") == "arm":
        return ["J%d" % (i + 1) for i in range(len(m["joints"]))]
    count = {}
    for j in m["joints"]:
        count[j["axis"]] = count.get(j["axis"], 0) + 1
    seen, out = {}, []
    for j in m["joints"]:
        a = j["axis"]
        seen[a] = seen.get(a, 0) + 1
        out.append(a if count[a] == 1 else "%s%d" % (a, seen[a]))
    return out


def validate(raw, mdir=None):
    """The machine with its gaps filled in, or Invalid with every problem found."""
    errs = []
    if not isinstance(raw, dict):
        raise Invalid([("", "machine.json holds an object")])
    m = _merge(DEFAULT, {k: v for k, v in raw.items() if k not in ("axes", "joints")})
    arm = m.get("type") == "arm"
    if arm:
        if "spindle" not in raw:  # an arm has none unless it says so
            m["spindle"]["type"] = "none"
        a = m["arm"] = dict(DEFAULT["arm"], **(raw.get("arm") or {}))
        for k in ("gripper", "changer", "enable", "sim"):
            a[k] = dict(DEFAULT["arm"][k], **(a.get(k) or {}))
    m["axes"] = copy.deepcopy(raw.get("axes", DEFAULT["axes"]))
    m["joints"] = copy.deepcopy(raw.get("joints", DEFAULT["joints"]))
    if m.get("version") != VERSION:
        errs.append(("version", "%d (this millweb's)" % VERSION))
    if not isinstance(m["name"], str) or not m["name"].strip() or len(m["name"]) > 60 or not PLAIN.match(m["name"]):
        errs.append(("name", "1 to 60 characters, no quotes"))
    if m["type"] not in TYPES:
        errs.append(("type", "gantry (the bridge moves over the table) or bridge (a fixed bridge, the table moves in Y)"))
    if m["units"] != "mm":
        errs.append(("units", "mm (the console is for mm machines)"))
    _num(m["servo_period_ns"], "servo_period_ns", errs, 100000, 10000000, True)

    b = m["board"]
    if b["type"] not in BOARDS:
        errs.append(("board.type", "one of " + ", ".join(BOARDS)))
    hm2 = b["type"].startswith("hm2_")
    if hm2 and not re.match(r"^[A-Za-z0-9_]+$", str(b["name"])):
        errs.append(("board.name", "the board's name in its pins, e.g. 7i96s (hm2_7i96s.0...)"))
    for k in ("params", "config"):
        if not isinstance(b[k], str) or not PLAIN.match(b[k]):
            errs.append(("board." + k, "text without quotes or line breaks"))
    _num(b["watchdog_ms"], "board.watchdog_ms", errs, 1, 1000)

    bs = m["bitstream"]
    if bs["loader"] not in LOADERS:
        errs.append(("bitstream.loader", "one of " + ", ".join(LOADERS)))
    elif bs["loader"] != "none":
        if not isinstance(bs["file"], str) or not bs["file"] or bs["file"].startswith("/") or ".." in bs["file"].split("/"):
            errs.append(("bitstream.file", "a file in the machine's bitstreams/ folder"))
        elif mdir and not os.path.isfile(os.path.join(mdir, "bitstreams", bs["file"])):
            errs.append(("bitstream.file", "bitstreams/%s isn't there (upload it)" % bs["file"]))
    if bs["loader"] == "fpga_manager" and not re.match(r"^fpga\d+$", str(bs["fpga"])):
        errs.append(("bitstream.fpga", "the FPGA manager's name, e.g. fpga0"))
    if bs["loader"] == "command" and (not isinstance(bs["command"], str) or "{file}" not in bs["command"] or "\n" in bs["command"]):
        errs.append(("bitstream.command", "one line with {file} where the bitstream's path goes"))
    if bs["reload"] not in ("changed", "always"):
        errs.append(("bitstream.reload", "changed or always"))

    if not isinstance(m["axes"], dict) or not m["axes"]:
        errs.append(("axes", "at least one axis"))
        m["axes"] = {}
    for a, ax in list(m["axes"].items()):
        if a not in AXES or not isinstance(ax, dict):
            errs.append(("axes." + str(a), "an axis letter of " + AXES))
            continue
        for k in ("min", "max", "max_vel", "max_acc"):
            _num(ax.get(k), "axes.%s.%s" % (a, k), errs, 0 if k in ("max_vel", "max_acc") else None)
        if isinstance(ax.get("min"), (int, float)) and isinstance(ax.get("max"), (int, float)) and ax["min"] >= ax["max"]:
            errs.append(("axes.%s" % a, "min below max"))
        if all(isinstance(ax.get(k), (int, float)) for k in ("max_vel", "max_acc")) and (ax["max_vel"] <= 0 or ax["max_acc"] <= 0):
            errs.append(("axes.%s" % a, "max_vel and max_acc above 0"))

    if not isinstance(m["joints"], list) or not 1 <= len(m["joints"]) <= 9:
        errs.append(("joints", "1 to 9 joints"))
        m["joints"] = []
    sims = 0
    for i, j in enumerate(m["joints"]):
        w = "joints.%d" % i
        if not isinstance(j, dict) or (not arm and j.get("axis") not in m["axes"]):
            errs.append((w + ".axis", "one of the axes (%s)" % "".join(m["axes"])) if isinstance(j, dict) else (w, "a joint"))
            continue
        ax = {} if arm else m["axes"][j["axis"]]  # an arm's joints have their own travel [deg]
        d = j.setdefault("drive", {"type": "sim"})
        j.setdefault("ferror", 1)
        j.setdefault("min_ferror", 0.25)
        h = j["home"] = dict({"method": "absolute", "pos": 0, "sequence": 0, "search_vel": 0, "latch_vel": 0, "offset": 0, "switch": ""}, **(j.get("home") or {}))
        for k, f in (("min", -1), ("max", 1)):  # a joint's travel: the axis's, a little more
            if k not in j and isinstance(ax.get(k), (int, float)):
                j[k] = ax[k] + f
        for k in ("max_vel", "max_acc"):
            if k not in j and isinstance(ax.get(k), (int, float)):
                j[k] = round(ax[k] * 1.1, 6)
        for k in ("min", "max", "max_vel", "max_acc", "ferror", "min_ferror"):
            _num(j.get(k), "%s.%s" % (w, k), errs)
        mech = j.get("mech")
        if isinstance(mech, dict) and mech.get("lead") in (None, ""):  # no lead: the drive works in mm itself
            j.pop("mech")
            mech = None
        if mech is not None:  # the screw and the gearing between it and the motor: the drive's command in motor radians
            if not isinstance(mech, dict):
                errs.append((w + ".mech", "lead and ratio"))
            else:
                _num(mech.get("lead"), w + ".mech.lead", errs, 1e-3, 10000)
                _num(mech.setdefault("ratio", 1), w + ".mech.ratio", errs, 1e-3, 1000)
                if not isinstance(mech.setdefault("reverse", False), bool):
                    errs.append((w + ".mech.reverse", "true or false"))
        if d.get("type") not in DRIVES:
            errs.append((w + ".drive.type", "one of " + ", ".join(DRIVES)))
        elif d["type"] == "stmbl_sserial":
            if not hm2:
                errs.append((w + ".drive", "a drive on smart-serial needs a Mesa board (board.type hm2_...)"))
            _num(d.setdefault("port", 0), w + ".drive.port", errs, 0, 7, True)
            _num(d.setdefault("channel", 0), w + ".drive.channel", errs, 0, 7, True)
        elif d["type"] == "stmbl_can":
            _num(d.setdefault("bus", 0), w + ".drive.bus", errs, 0, max(0, len(m["can"]["spi"]) - 1), True)
            _num(d.setdefault("node", 1), w + ".drive.node", errs, 1, 127, True)
            if not m["can"]["spi"]:
                errs.append(("can.spi", "the SPI device of each CAN bus (e.g. /dev/spidev0.0), or sim"))
        elif d["type"] == "stmbl_arm":
            if not arm:
                errs.append((w + ".drive.type", "stmbl_arm (the simulated arm) on an arm only"))
        elif d["type"] == "stmbl_sim":
            sims += 1
            conf = d.setdefault("conf", "drive.conf")
            if not isinstance(conf, str) or conf.startswith("/") or ".." in conf.split("/"):
                errs.append((w + ".drive.conf", "the simulated drive's flash config, a file in the machine folder"))
            elif mdir and not os.path.isfile(os.path.join(mdir, conf)):
                errs.append((w + ".drive.conf", "%s isn't in the machine folder" % conf))
        d.setdefault("console", d.get("type") not in ("sim", "stmbl_arm"))
        if h["method"] not in HOMING:
            errs.append((w + ".home.method", "one of " + ", ".join(HOMING)))
        _num(h["sequence"], w + ".home.sequence", errs, -9, 9, True)
        _num(h["pos"], w + ".home.pos", errs)
        if h["method"] == "switch":
            if not HALNAME.match(str(h["switch"])):
                errs.append((w + ".home.switch", "the home switch's HAL pin"))
            if not h["search_vel"]:
                errs.append((w + ".home.search_vel", "the search speed (its sign is the direction)"))
        if h["method"] in ("switch", "index") and not h["latch_vel"]:
            errs.append((w + ".home.latch_vel", "the latch speed"))
        if h["method"] == "index" and d.get("type") == "sim":
            errs.append((w + ".home", "a simulated joint has no index"))
        if arm:
            _num(j.setdefault("gear", 1), w + ".gear", errs, -100000, 100000)
            if j["gear"] == 0:
                errs.append((w + ".gear", "the motor's turns a joint turn (not 0; negative: reversed)"))
            if h["method"] not in ("absolute", "immediate"):
                errs.append((w + ".home.method", "absolute or immediate on an arm"))
    if sims > 1:
        errs.append(("joints", "one simulated stmbl drive at most (stmbl_sim is a single drive)"))
    for a in m["axes"]:
        if not arm and not any(j.get("axis") == a for j in m["joints"] if isinstance(j, dict)):
            errs.append(("axes." + a, "no joint moves it"))
    if arm:
        _validate_arm(m, mdir, errs)
    if m["type"] == "bridge":  # the table and the X carriage each on one screw: no gantry pair
        for a in "XY":
            n = sum(1 for j in m["joints"] if isinstance(j, dict) and j.get("axis") == a)
            if n > 1:
                errs.append(("axes." + a, "a fixed-bridge machine moves %s with one joint (%d here: that's a gantry)" % (a, n)))
    if any(d.get("type") == "stmbl_can" for d in (j.get("drive", {}) for j in m["joints"] if isinstance(j, dict))):
        for s in m["can"]["spi"]:
            if s != "sim" and not re.match(r"^/dev/spidev\d+\.\d+$", str(s)):
                errs.append(("can.spi", "/dev/spidevX.Y or sim (%r)" % s))

    for k in ("estop", "probe"):
        p = m[k].get("input", "")
        if p and not HALNAME.match(str(p)):
            errs.append((k + ".input", "a HAL pin name"))
    if m["estop"]["invert"] and not m["estop"]["input"]:
        errs.append(("estop.invert", "only with an input"))
    if m["probe"]["type"] not in ("none", "input", "sim"):
        errs.append(("probe.type", "none, input or sim"))
    if m["probe"]["type"] == "input" and not m["probe"]["input"]:
        errs.append(("probe.input", "the probe's HAL pin"))
    if m["probe"]["type"] == "sim" and "Z" not in m["axes"]:
        errs.append(("probe.type", "the simulated probe needs a Z axis"))
    sp = m["spindle"]
    if sp["type"] not in SPINDLES:
        errs.append(("spindle.type", ", ".join(SPINDLES)))
    elif sp["type"] == "stmbl_sserial":
        if not hm2:
            errs.append(("spindle.type", "a drive on smart-serial needs a Mesa board (board.type hm2_...)"))
        _num(sp["port"], "spindle.port", errs, 0, 7, True)
        _num(sp["channel"], "spindle.channel", errs, 0, 7, True)
    elif sp["type"] == "stmbl_can":
        if not m["can"]["spi"]:
            errs.append(("can.spi", "the SPI device of each CAN bus (e.g. /dev/spidev0.0), or sim"))
        _num(sp["bus"], "spindle.bus", errs, 0, max(0, len(m["can"]["spi"]) - 1), True)
        _num(sp["node"], "spindle.node", errs, 1, 127, True)
    if sp["type"] in ("stmbl_sserial", "stmbl_can"):
        _num(sp["ratio"], "spindle.ratio", errs, 0.001, 1000)
        if sp["index_sensor"] and not HALNAME.match(str(sp["index_sensor"])):
            errs.append(("spindle.index_sensor", "a HAL pin name"))
        if not isinstance(sp["console"], bool):
            errs.append(("spindle.console", "true or false"))
        k = (sp["port"], sp["channel"]) if sp["type"] == "stmbl_sserial" else (sp["bus"], sp["node"])
        for i, j in enumerate(m["joints"]):
            d = j.get("drive", {}) if isinstance(j, dict) else {}
            if d.get("type") == sp["type"] and k == ((d.get("port"), d.get("channel")) if sp["type"] == "stmbl_sserial" else (d.get("bus"), d.get("node"))):
                errs.append(("spindle.type", "the same drive as joint %d" % i))
    _num(sp["max_rpm"], "spindle.max_rpm", errs, 0, 200000)
    _num(sp["min_rpm"], "spindle.min_rpm", errs, 0, 200000)
    for k in ("on", "forward", "reverse", "speed", "at_speed", "revs", "index", "speed_in"):
        if sp[k] and not HALNAME.match(str(sp[k])):
            errs.append(("spindle." + k, "a HAL pin name"))
    if sp["index"] and not sp["revs"]:
        errs.append(("spindle.index", "only with its revs pin (the encoder's position)"))
    if m["type"] == "lathe":
        la = m["lathe"]
        if la.get("post") not in ("front", "back"):
            errs.append(("lathe.post", "front or back"))
        if not isinstance(la.get("diameter"), bool):
            errs.append(("lathe.diameter", "true or false"))
        for a in "XZ":
            if a not in m["axes"]:
                errs.append(("axes." + a, "a lathe has X and Z"))
        for a in m["axes"]:
            if a not in "XZ":
                errs.append(("axes." + a, "a lathe here has X and Z only (C, Y: not yet)"))
        if m["atc"]["type"] != "none":
            errs.append(("atc.type", "none on a lathe (its tools: lathe.turret)"))
        tu = la["turret"] = dict(DEFAULT["lathe"]["turret"], **(la.get("turret") or {}))
        if tu["type"] not in TURRETS:
            errs.append(("lathe.turret.type", ", ".join(TURRETS)))
        elif tu["type"] != "none":
            _num(tu["stations"], "lathe.turret.stations", errs, 1, 32, True)
            for k, lo, hi in (("offset", -1, 1), ("overtravel", 0, 0.5), ("lock", 0, 0.5), ("speed", 0.01, 10),
                              ("lock_speed", 0.001, 10), ("acc", 0.01, 1000), ("dwell", 0, 10), ("timeout", 0.1, 120),
                              ("slip", 0.001, 0.5), ("sim_index_at", 0, 1)):
                _num(tu[k], "lathe.turret." + k, errs, lo, hi)
            if not isinstance(tu["index_invert"], bool):
                errs.append(("lathe.turret.index_invert", "true or false"))
        if tu["type"] == "stepper":
            if not tu["stepgen"] or not HALNAME.match(str(tu["stepgen"])):
                errs.append(("lathe.turret.stepgen", "the stepgen's HAL prefix (e.g. hm2_7i76e.0.stepgen.03)"))
            if not tu["index"] or not HALNAME.match(str(tu["index"])):
                errs.append(("lathe.turret.index", "the index pulse's HAL pin (one a turret turn)"))
            _num(tu["steps"], "lathe.turret.steps", errs, 1, 10000000)
        if m["toolsetter"] is not None:
            errs.append(("toolsetter", "none on a lathe"))
        if m["probe"]["type"] == "sim":
            errs.append(("probe.type", "none or input on a lathe"))
    if sp["type"] == "hal" and not (sp["on"] or sp["speed"] or sp["forward"]):
        errs.append(("spindle.on", "at least one of its pins: on, forward or speed"))
    t = m["atc"]
    if t["type"] not in ("none", "rack"):
        errs.append(("atc.type", "none or rack"))
    elif t["type"] == "rack":
        _num(t["pockets"], "atc.pockets", errs, 1, 99, True)
        for k in ("x", "y", "dx", "dy", "z_clear", "z_pocket", "slide"):
            _num(t[k], "atc." + k, errs)
        _num(t["feed"], "atc.feed", errs, 1, 100000)
        _num(t["dwell"], "atc.dwell", errs, 0, 60)
        _num(t["timeout"], "atc.timeout", errs, 0.1, 60)
        for k in ("unclamp", "clamped", "released", "air"):
            if t[k] and not HALNAME.match(str(t[k])):
                errs.append(("atc." + k, "a HAL pin name"))
        if not t["unclamp"]:
            errs.append(("atc.unclamp", "the drawbar's output pin"))
        if "Z" not in m["axes"]:
            errs.append(("atc.type", "a tool changer needs a Z axis"))
        if all(isinstance(t[k], (int, float)) for k in ("z_clear", "z_pocket")) and t["z_pocket"] >= t["z_clear"]:
            errs.append(("atc.z_pocket", "below z_clear"))
    ts = m["toolsetter"]
    if ts is not None:
        if not isinstance(ts, dict):
            errs.append(("toolsetter", "x, y, z_above, z_ref, travel, or none"))
        else:
            for k in ("x", "y", "z_above", "z_ref", "travel"):
                _num(ts.get(k), "toolsetter." + k, errs)
    sb = m["stmbl"]
    if sb.get("fw") is not None:
        _num(sb["fw"], "stmbl.fw", errs, 1, 100000, True)
    if not isinstance(sb.get("block", True), bool):
        errs.append(("stmbl.block", "true or false"))
    md = m["model"]
    if not isinstance(md.get("builtin", True), bool):
        errs.append(("model.builtin", "true or false"))
    tip = md.get("tip", [0, 0, 0])  # where the tool's tip is in the parts' files (a lathe's: they move from there)
    if not isinstance(tip, list) or len(tip) != 3 or not all(isinstance(v, (int, float)) and not isinstance(v, bool) for v in tip):
        errs.append(("model.tip", "3 numbers"))
    if not isinstance(md.get("parts", []), list) or len(md.get("parts", [])) > 40:
        errs.append(("model.parts", "a list of at most 40 parts"))
        md["parts"] = []
    for i, pt in enumerate(md.get("parts", [])):
        w = "model.parts.%d" % i
        if not isinstance(pt, dict):
            errs.append((w, "a part"))
            continue
        f = pt.get("file", "")
        if not isinstance(f, str) or not re.match(r"^[A-Za-z0-9_.\- ]{1,80}\.(stl|obj)$", f, re.I):
            errs.append((w + ".file", "an STL or OBJ file in the machine's models/ folder"))
        elif mdir and not os.path.isfile(os.path.join(mdir, "models", f)):
            errs.append((w + ".file", "models/%s isn't there (upload it)" % f))
        mv = MOVES.match(str(pt.get("moves", "")))
        if not mv or len([a for a in mv.groups() if a]) != len(set(a for a in mv.groups() if a)):
            errs.append((w + ".moves", "the axes it moves with: '' (fixed), Y, XY, XYZ, -X (a moving table), S (with the spindle) …"))
        for k, n in (("pos", 3), ("rot", 3)):
            v = pt.get(k, [0, 0, 0])
            if not isinstance(v, list) or len(v) != n or not all(isinstance(x, (int, float)) and not isinstance(x, bool) for x in v):
                errs.append(("%s.%s" % (w, k), "%d numbers" % n))
        _num(pt.get("scale", 1), w + ".scale", errs, 1e-6, 1e6)
        _num(pt.get("opacity", 1), w + ".opacity", errs, 0, 1)
        if not re.match(r"^#[0-9a-fA-F]{6}$", str(pt.get("color", "#8a99ab"))):
            errs.append((w + ".color", "#rrggbb"))
    for k in ("max_feed_override", "max_spindle_override", "min_spindle_override"):
        _num(m["display"][k], "display." + k, errs, 0, 10)

    c = m["console"]
    _num(c["port"], "console.port", errs, 1, 65535, True)
    if not isinstance(c["token"], str) or not PLAIN.match(c["token"]) or " " in c["token"]:
        errs.append(("console.token", "one word"))
    if c["on_control_loss"] not in ("continue", "hold"):
        errs.append(("console.on_control_loss", "continue or hold"))
    if not re.match(r"^[0-9,\- ]*$", str(c["cpus"])):
        errs.append(("console.cpus", "cores like 0, 0,1 or 0-1"))
    if not isinstance(c["pins"], list) or not all(isinstance(p, str) and HALNAME.match(p) for p in c["pins"]):
        errs.append(("console.pins", "HAL pin names"))
    if not isinstance(c["extra"], dict):
        errs.append(("console.extra", "KEY: value"))
    else:
        for k, v in c["extra"].items():
            if k not in CONSOLE_KEYS:
                errs.append(("console.extra." + k, "not a setting here (%s)" % ", ".join(sorted(CONSOLE_KEYS))))
            elif not all(isinstance(x, (str, int, float)) and PLAIN.match(str(x)) for x in (v if isinstance(v, list) else [v])):
                errs.append(("console.extra." + k, "plain values"))
    if errs:
        raise Invalid(errs)
    return m


def arm_path(p, mdir):
    """an arm's file as LinuxCNC gets it: {repo}/... in this checkout, relative in the machine folder"""
    p = str(p).replace("{repo}", REPO)
    return p if os.path.isabs(p) else os.path.join(mdir or "", p)


def dh_count(path, depth=0):
    """the dh lines in an arm_sim model file and what it includes"""
    n = 0
    for line in open(path, errors="replace"):
        w = line.split("#", 1)[0].split()
        if w[:1] == ["dh"]:
            n += 1
        elif w[:1] == ["include"] and len(w) > 1 and depth < 8:
            inc = w[1] if os.path.isabs(w[1]) else os.path.join(os.path.dirname(path), w[1])
            if os.path.isfile(inc):
                n += dh_count(inc, depth + 1)
    return n


def _validate_arm(m, mdir, errs):
    a = m["arm"]
    if set(m["axes"]) != set("XYZABC"):
        errs.append(("axes", "an arm's: X Y Z A B C (the tool point and its orientation)"))
    for k, want in (("spindle", "none"), ("probe", "none"), ("atc", "none")):
        if m[k]["type"] != want:
            errs.append((k + ".type", "none on an arm"))
    if m["toolsetter"] is not None:
        errs.append(("toolsetter", "none on an arm"))
    files = {"model": True, "vis": False, "cell": False, "collide_model": False, "tools": False, "quota": False}
    for k, need in files.items():
        v = a.get(k, "")
        if not isinstance(v, str) or not PLAIN.match(v) or " " in v:
            errs.append(("arm." + k, "a file name (no spaces or quotes)"))
        elif need and not v:
            errs.append(("arm." + k, "the arm's model file (arm_sim's .arm: its dh lines, limits, bodies)"))
        elif v and mdir and not os.path.isfile(arm_path(v, mdir)):
            errs.append(("arm." + k, "%s isn't there" % arm_path(v, mdir)))
    if a["model"] and mdir and os.path.isfile(arm_path(a["model"], mdir)):
        n = dh_count(arm_path(a["model"], mdir))
        if n != len(m["joints"]):
            errs.append(("joints", "%d: the model has %d dh lines" % (len(m["joints"]), n)))
    drives = [j.get("drive", {}).get("type") for j in m["joints"] if isinstance(j, dict)]
    sim = "stmbl_arm" in drives
    if sim and set(drives) != {"stmbl_arm"}:
        errs.append(("joints", "all on stmbl_arm (the simulated arm) or none"))
    if sim:
        for k in ("arm", "lib"):
            v = a["sim"].get(k, "")
            if not v or not isinstance(v, str):
                errs.append(("arm.sim." + k, "stmbl_arm's %s file" % ("arm (host/arm/.../*.arm)" if k == "arm" else "drive library (host/arm/libstmbl_drive.so)")))
            elif mdir and not os.path.isfile(arm_path(v, mdir)):
                errs.append(("arm.sim." + k, "%s isn't there" % arm_path(v, mdir)))
    if not isinstance(a["collide"], bool):
        errs.append(("arm.collide", "true or false"))
    elif a["collide"]:
        if not a["cell"]:
            errs.append(("arm.collide", "with a cell (armguard stops the arm on a collision)"))
        if "sim" in drives:
            errs.append(("arm.collide", "drives that report their current (not sim)"))
    g = a["gripper"]
    if g["type"] not in GRIPPERS:
        errs.append(("arm.gripper.type", ", ".join(GRIPPERS)))
    elif g["type"] == "stmbl_arm" and not sim:
        errs.append(("arm.gripper.type", "stmbl_arm with the simulated arm only"))
    elif g["type"] == "stmbl_sserial":
        if not m["board"]["type"].startswith("hm2_"):
            errs.append(("arm.gripper.type", "a drive on smart-serial needs a Mesa board (board.type hm2_...)"))
        _num(g["port"], "arm.gripper.port", errs, 0, 7, True)
        _num(g["channel"], "arm.gripper.channel", errs, 0, 7, True)
    elif g["type"] == "stmbl_can":
        if not m["can"]["spi"] and not any(j.get("drive", {}).get("type") == "stmbl_can" for j in m["joints"] if isinstance(j, dict)):
            errs.append(("can.spi", "the SPI device of each CAN bus (e.g. /dev/spidev0.0), or sim"))
        _num(g["bus"], "arm.gripper.bus", errs, 0, max(0, len(m["can"]["spi"]) - 1), True)
        _num(g["node"], "arm.gripper.node", errs, 1, 127, True)
    if g["type"] != "none" and not a["collide"]:
        errs.append(("arm.gripper.type", "with collide (armgrip checks the payload on the collision detector's model)"))
    ch = a["changer"]
    for k in ("valve", "locked", "unlocked", "present", "id"):
        if ch[k] and not HALNAME.match(str(ch[k])):
            errs.append(("arm.changer." + k, "a HAL pin name"))
    if a["tools"] and not sim:
        for k in ("valve", "locked", "unlocked"):
            if not ch[k]:
                errs.append(("arm.changer." + k, "the changer's %s pin" % {"valve": "lock valve (output)", "locked": "locked sensor", "unlocked": "unlocked sensor"}[k]))
    if not isinstance(ch["ready"], str) or "\n" in ch["ready"]:
        errs.append(("arm.changer.ready", "one MDI line (where the arm goes before a tool change), or blank"))
    hj = a["home"]
    if not isinstance(hj, list) or (hj and len(hj) != len(m["joints"])) or not all(isinstance(x, (int, float)) and not isinstance(x, bool) for x in hj):
        errs.append(("arm.home", "the home pose: a value a joint (%d), or none" % len(m["joints"])))
    e = a["enable"]
    if e["input"] and not HALNAME.match(str(e["input"])):
        errs.append(("arm.enable.input", "the enabling switch's HAL pin"))
    for k in ("invert", "hold_all"):
        if not isinstance(e[k], bool):
            errs.append(("arm.enable." + k, "true or false"))


# ------------------------------------------------------------------ making the configuration
def _fmt(v):
    if isinstance(v, bool):
        return "YES" if v else "NO"
    if isinstance(v, float):
        return ("%.6f" % v).rstrip("0").rstrip(".")
    return str(v)


DRIVE_PINS = ("pos_cmd", "vel_cmd", "pos_fb", "vel_fb", "enable", "fault", "index_enable", "current", "power", "con_cmd", "con_fb")


def _drive(m, d):
    """An stmbl drive's HAL prefix and pin names {pos_cmd, ...}: (None, None) for none"""
    t = d["type"]
    if t == "stmbl_sserial":
        return "hm2_%s.0.stbl.%d.%d" % (m["board"]["name"], d["port"], d["channel"]), {k: k for k in DRIVE_PINS}
    if t == "stmbl_can":
        return "stmblcan.%d.%d" % (d["bus"], d["node"]), {k: k.replace("_", "-") for k in DRIVE_PINS}
    if t == "stmbl_sim":
        return "stmbl-sim.0", {k: k for k in DRIVE_PINS}
    return None, None


def drives_of(m):
    """[(joint index, joint name, drive, HAL prefix, pin names {pos_cmd, ...}, console tty name or None)]; an stmbl
    spindle last, named S, its index None; an arm's gripper drive likewise, named G."""
    names, out = joint_names(m), []
    for i, j in enumerate(m["joints"]):
        d = j["drive"]
        if d["type"] == "stmbl_arm":  # the simulated arm: a drive's smart-serial pins per joint, no console
            out.append((i, names[i], d, "stmbl-arm.0.%d" % i, {k: k.replace("_", "-") for k in DRIVE_PINS}, None))
            continue
        pre, pins = _drive(m, d)
        out.append((i, names[i], d, pre, pins, ("drive-%s" % names[i].lower()) if pre and d.get("console") else None))
    sp = m["spindle"]
    if sp["type"] in ("stmbl_sserial", "stmbl_can"):
        pre, pins = _drive(m, sp)
        out.append((None, "S", sp, pre, pins, "drive-s" if sp.get("console", True) else None))
    g = m.get("arm", {}).get("gripper", {}) if m["type"] == "arm" else {}
    if g.get("type") == "stmbl_arm":
        out.append((None, "G", g, "stmbl-arm.0.grip", {k: k.replace("_", "-") for k in DRIVE_PINS}, None))
    elif g.get("type") in ("stmbl_sserial", "stmbl_can"):
        pre, pins = _drive(m, g)
        out.append((None, "G", g, pre, pins, "drive-g" if g.get("console", True) else None))
    return out


def _millweb_keys(m, mdir, run, drv):
    """[MILLWEB]: the console's settings, the drives' pins and consoles"""
    c = m["console"]
    ts = m["toolsetter"]
    mw = [("PORT", c["port"]), ("HOST", c["host"]), ("TOKEN", c["token"] or None), ("ON_CONTROL_LOSS", c["on_control_loss"]),
          ("HAL_WRITE", "yes" if c["hal_write"] else "no"), ("DRIVE_WRITE", "yes" if c["drive_write"] else "no"),
          ("CPUS", c["cpus"] or None), ("NC_DIR", os.path.join(mdir, "nc_files")),
          ("SETUP", os.path.join(mdir, "millweb-setup.json")), ("DEBUG_VIEWS", os.path.join(mdir, "debug-views.json")),
          ("MACHINE_DIR", mdir), ("MACHINE_TYPE", m["type"]),
          ("DRIVE_FW", m["stmbl"].get("fw")), ("DRIVE_FW_BLOCK", "yes" if m["stmbl"].get("block", True) else "no"),
          ("TOOLSETTER", " ".join(_fmt(ts[k]) for k in ("x", "y", "z_above", "z_ref", "travel")) if ts else None),
          ("PIN", list(c["pins"]) or None)]
    if m["spindle"]["type"] == "sim" and "AT_SPEED_PIN" not in c["extra"]:
        mw.append(("AT_SPEED_PIN", "spindle.0.at-speed"))
    for i, n, dd, pre, pins, con in drv:
        if pre:
            mw.append(("DRIVE", "%s %s.%s %s.%s %s.%s" % (n, pre, pins["current"], pre, pins["power"], pre, pins["fault"])))
        if con:
            mw.append(("DRIVE_LINK", "%s %s %s" % (n, os.path.join(run, con), pre)))
    for k, v in sorted(c["extra"].items()):
        mw.append((k, v))
    return mw


def _load_drivers(m, mdir, run, drv, h, first, last):
    """loadrt the Mesa board, the CAN buses, a simulated stmbl drive, the drives' consoles; their read and write
    functions into first and last"""
    b = m["board"]
    hm2 = b["type"].startswith("hm2_")
    board = "hm2_%s.0" % b["name"] if hm2 else None
    cans = [x for x in drv if x[2]["type"] == "stmbl_can"]
    consoles = [x for x in drv if x[5]]
    if hm2:
        h("loadrt hostmot2")
        args = ['config="%s"' % b["config"]] if b["config"] else []
        if b["params"]:
            args.append(b["params"])
        h("loadrt %s %s" % (b["type"], " ".join(args)))
        h("setp %s.watchdog.timeout_ns %d" % (board, int(b["watchdog_ms"] * 1e6)))
        first.append(board + ".read")
        last.append(board + ".write")
    if cans:
        buses = sorted({x[2]["bus"] for x in cans})
        nodes = ":".join(",".join(str(x[2]["node"]) for x in cans if x[2]["bus"] == bb) for bb in range(max(buses) + 1))
        h("loadrt stmblcan spi=%s nodes=%s discover=%d spi_hz=%d" % (",".join(m["can"]["spi"][:max(buses) + 1]), nodes,
                                                                      1 if m["can"]["discover"] else 0, m["can"]["spi_hz"]))
        first.append("stmblcan.read")
        last.append("stmblcan.write")
    sim = [x for x in drv if x[2]["type"] == "stmbl_sim"]
    if sim:
        dd = sim[0][2]
        h("loadrt stmbl_sim conf=%s tty=%s" % (os.path.join(mdir, dd["conf"]), os.path.join(run, "drive-sim-usb")) +
          (" sim=%s" % os.path.join(mdir, dd["sim"]) if dd.get("sim") else ""))
        first.insert(0, "stmbl-sim.0")
    if consoles:
        h("loadrt sserialterm tty=%s" % ",".join(os.path.join(run, x[5]) for x in consoles))


def generate(m, mdir, mill, custom=None):
    """{"mill.ini": text, "machine.hal": text} for machine m (validated) in machine
    folder mdir; mill is webui/mill (millweb's launcher is there). custom: the
    custom.hal text to be (None: whether the folder has one)."""
    if m["type"] == "arm":
        return generate_arm(m, mdir, mill, custom)
    run = os.path.join(mdir, "run")
    names = joint_names(m)
    coords = "".join(j["axis"] for j in m["joints"])
    dup = len(set(coords)) < len(coords)
    drv = drives_of(m)
    b = m["board"]
    hm2 = b["type"].startswith("hm2_")
    board = "hm2_%s.0" % b["name"] if hm2 else None
    cans = [x for x in drv if x[2]["type"] == "stmbl_can"]
    lathe = m["type"] == "lathe"
    turret = lathe and m["lathe"]["turret"]["type"] != "none"
    sdrv = next(((x[3], x[4]) for x in drv if x[0] is None), None)  # an stmbl spindle drive: (prefix, pins)
    consoles = [x for x in drv if x[5]]
    maxv = max(ax["max_vel"] for ax in m["axes"].values())
    maxa = max(ax["max_acc"] for ax in m["axes"].values())

    # ---------------------------------------------------------- the INI
    L = []
    w = L.append

    def sec(name, *kv):
        w("\n[%s]" % name)
        for k, v in kv:
            if v is None:
                continue
            for x in (v if isinstance(v, list) else [v]):
                w("%s = %s" % (k, _fmt(x)))

    w("# made by millweb from %s at %s: edit machine.json (the console's Configure page), not this file" % (
        os.path.join(mdir, "machine.json"), datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")))
    sec("EMC", ("VERSION", "1.1"), ("MACHINE", m["name"]), ("DEBUG", 0))
    d = m["display"]
    sec("DISPLAY", ("DISPLAY", os.path.join(mill, "millweb")), ("CYCLE_TIME", 0.1), ("POSITION_OFFSET", "RELATIVE"),
        ("LATHE", 1 if lathe else None), ("BACK_TOOL_LATHE", 1 if lathe and m["lathe"]["post"] == "back" else None),
        ("POSITION_FEEDBACK", "ACTUAL"), ("MAX_LINEAR_VELOCITY", maxv), ("DEFAULT_LINEAR_VELOCITY", round(maxv / 10, 3)),
        ("PROGRAM_PREFIX", os.path.join(mdir, "nc_files")), ("MAX_FEED_OVERRIDE", d["max_feed_override"]),
        ("MAX_SPINDLE_OVERRIDE", d["max_spindle_override"]), ("MIN_SPINDLE_OVERRIDE", d["min_spindle_override"]))
    sec("MILLWEB", *_millweb_keys(m, mdir, run, drv))
    atc = m["atc"]["type"] == "rack"
    sec("RS274NGC", ("PARAMETER_FILE", os.path.join(mdir, "linuxcnc.var")),
        ("RS274NGC_STARTUP_CODE", "G18 %s G21 G40 G49 G80 G90 G94 G97" % ("G7" if m["lathe"]["diameter"] else "G8") if lathe else None),
        ("SUBROUTINE_PATH", run if atc else None), ("REMAP", "M6 modalgroup=6 ngc=toolchange" if atc else None))
    sec("EMCMOT", ("EMCMOT", "motmod"), ("COMM_TIMEOUT", 1.0), ("SERVO_PERIOD", m["servo_period_ns"]))
    sec("TASK", ("TASK", "milltask"), ("CYCLE_TIME", 0.01))
    halfiles = [os.path.join(run, "machine.hal")]
    if (custom.strip() if custom is not None else os.path.isfile(os.path.join(mdir, "custom.hal"))):
        halfiles.append(os.path.join(mdir, "custom.hal"))
    sec("HAL", ("HALFILE", halfiles))
    sec("TRAJ", ("COORDINATES", " ".join(coords)), ("LINEAR_UNITS", "mm"), ("ANGULAR_UNITS", "degree"),
        ("DEFAULT_LINEAR_VELOCITY", round(maxv / 10, 3)), ("MAX_LINEAR_VELOCITY", maxv), ("MAX_LINEAR_ACCELERATION", maxa))
    if m["spindle"]["type"] != "none":
        sec("SPINDLE_0", ("MAX_FORWARD_VELOCITY", m["spindle"]["max_rpm"]))
    sec("EMCIO", ("EMCIO", "io"), ("CYCLE_TIME", 0.1), ("TOOL_TABLE", os.path.join(mdir, "tool.tbl")))
    sec("KINS", ("KINEMATICS", "trivkins coordinates=%s%s" % (coords, " kinstype=BOTH" if dup else "")), ("JOINTS", len(m["joints"])))
    for a in sorted(m["axes"], key=AXES.index):
        ax = m["axes"][a]
        sec("AXIS_" + a, ("MIN_LIMIT", ax["min"]), ("MAX_LIMIT", ax["max"]), ("MAX_VELOCITY", ax["max_vel"]), ("MAX_ACCELERATION", ax["max_acc"]))
    per_axis = {a: sum(1 for j in m["joints"] if j["axis"] == a) for a in m["axes"]}
    for i, j in enumerate(m["joints"]):
        h = j["home"]
        seq = h["sequence"]
        if per_axis[j["axis"]] > 1 and seq > 0:
            seq = -seq  # a gantry's joints home together, the final moves in step
        kv = [("TYPE", "ANGULAR" if j["axis"] in "ABC" else "LINEAR"), ("MIN_LIMIT", j["min"]), ("MAX_LIMIT", j["max"]),
              ("MAX_VELOCITY", j["max_vel"]), ("MAX_ACCELERATION", j["max_acc"]), ("FERROR", j["ferror"]),
              ("MIN_FERROR", j["min_ferror"]), ("HOME", h["pos"]), ("HOME_SEQUENCE", seq)]
        if h["method"] == "absolute":
            kv.append(("HOME_ABSOLUTE_ENCODER", 2))
        elif h["method"] == "immediate":
            kv += [("HOME_SEARCH_VEL", 0), ("HOME_LATCH_VEL", 0)]
        elif h["method"] == "switch":
            kv += [("HOME_OFFSET", h["offset"]), ("HOME_SEARCH_VEL", h["search_vel"]), ("HOME_LATCH_VEL", h["latch_vel"])]
        else:
            kv += [("HOME_OFFSET", h["offset"]), ("HOME_SEARCH_VEL", 0), ("HOME_LATCH_VEL", h["latch_vel"]), ("HOME_USE_INDEX", "YES")]
        sec("JOINT_%d" % i, *kv)
    ini = "\n".join(L) + "\n"

    # ---------------------------------------------------------- the HAL
    H = ["# made by millweb from machine.json: edit that (the Configure page), or add to custom.hal",
         "loadrt [KINS]KINEMATICS",
         "loadrt [EMCMOT]EMCMOT servo_period_nsec=[EMCMOT]SERVO_PERIOD num_joints=[KINS]JOINTS" + (" num_dio=4" if m["atc"]["type"] == "rack" else "")]
    h = H.append
    first, last = [], []  # functions before motion (read) and after it (write)
    loads = {}  # the stock comps by names= (each module loads once): {module: [names]}, loaded before the first net

    def comp(mod, *names):
        loads.setdefault(mod, []).extend(names)
    _load_drivers(m, mdir, run, drv, h, first, last)
    # joints with a screw lead (mech): the drive works in motor radians, LinuxCNC in mm; scale comps between
    mech = {i: m["joints"][i]["mech"] for i, n, dd, pre, pins, con in drv if pre and i is not None and m["joints"][i].get("mech")}
    if mech:
        h("loadrt scale names=%s" % ",".join("j%d-%s" % (i, k) for i in sorted(mech) for k in ("cmd", "vel", "fb")))
        first += ["j%d-fb" % i for i in sorted(mech)]
    if turret or sdrv:
        h("loadrt lathe turret=%d spindle=%d" % (1 if turret else 0, 1 if sdrv else 0))
    if sdrv:
        first.append("lathe.spindle")  # the drive's feedback into spindle.0.revs before motion reads it
    loads_at = len(H)
    for f in first:
        h("addf %s servo-thread" % f)
    h("addf motion-command-handler servo-thread")
    h("addf motion-controller servo-thread")
    tail = []  # functions between motion and the write
    for k, x in enumerate(consoles):
        tail.append("sserialterm.%d" % k)
    h("")
    for i, n, dd, pre, pins, con in drv:
        if i is None:
            continue  # the spindle's drive: below
        h("# joint %d (%s): %s" % (i, n, {"sim": "simulated (its command is its feedback)", "stmbl_sserial": "stmbl drive over smart-serial",
                                          "stmbl_can": "stmbl drive over CAN", "stmbl_sim": "simulated stmbl drive"}[dd["type"]]))
        if not pre:
            h("net j%d joint.%d.motor-pos-cmd => joint.%d.motor-pos-fb" % (i, i, i))
            continue
        if i in mech:
            me = mech[i]
            k = 2 * math.pi * me["ratio"] / me["lead"] * (-1 if me.get("reverse") else 1)  # motor rad per mm
            h("# %s mm a screw turn, the motor %s turns a screw turn: %s mm a motor turn, %s rad/mm" % (
                _fmt(float(me["lead"])), _fmt(float(me["ratio"])), _fmt(me["lead"] / me["ratio"]), _fmt(k)))
            for part, gain in (("cmd", k), ("vel", k), ("fb", 1 / k)):
                h("setp j%d-%s.gain %s" % (i, part, repr(gain)))
            h("net j%d-pos-cmd joint.%d.motor-pos-cmd => j%d-cmd.in" % (i, i, i))
            h("net j%d-pos-cmd-rad j%d-cmd.out => %s.%s" % (i, i, pre, pins["pos_cmd"]))
            h("net j%d-vel-cmd joint.%d.vel-cmd => j%d-vel.in" % (i, i, i))
            h("net j%d-vel-cmd-rad j%d-vel.out => %s.%s" % (i, i, pre, pins["vel_cmd"]))
            h("net j%d-pos-fb-rad %s.%s => j%d-fb.in" % (i, pre, pins["pos_fb"], i))
            h("net j%d-pos-fb j%d-fb.out => joint.%d.motor-pos-fb" % (i, i, i))
            tail += ["j%d-cmd" % i, "j%d-vel" % i]
        else:
            h("net j%d-pos-cmd joint.%d.motor-pos-cmd => %s.%s" % (i, i, pre, pins["pos_cmd"]))
            h("net j%d-vel-cmd joint.%d.vel-cmd => %s.%s" % (i, i, pre, pins["vel_cmd"]))
            h("net j%d-pos-fb %s.%s => joint.%d.motor-pos-fb" % (i, pre, pins["pos_fb"], i))
        h("net j%d-enable joint.%d.amp-enable-out => %s.%s" % (i, i, pre, pins["enable"]))
        h("net j%d-fault %s.%s => joint.%d.amp-fault-in" % (i, pre, pins["fault"], i))
        if m["joints"][i]["home"]["method"] == "index":
            h("net j%d-index joint.%d.index-enable <=> %s.%s" % (i, i, pre, pins["index_enable"]))
        if con:
            k = consoles.index(next(x for x in consoles if x[0] == i))
            h("net j%d-con-fb %s.%s => sserialterm.%d.con-fb" % (i, pre, pins["con_fb"], k))
            h("net j%d-con-cmd sserialterm.%d.con-cmd => %s.%s" % (i, k, pre, pins["con_cmd"]))
    for i, j in enumerate(m["joints"]):
        if j["home"]["method"] == "switch":
            h("net j%d-home-sw %s => joint.%d.home-sw-in" % (i, j["home"]["switch"], i))
    h("")
    # E-STOP: the input (true when the chain is closed) and a board's watchdog; else a loop
    e = m["estop"]

    def chain(src, dst, name):
        """src (a pin, or None: true) to dst through a lathe's checks: the turret's chain (a fault opens
        it), the spindle drive's fault"""
        if turret:
            if src:
                h("net %s %s => lathe.turret.chain-in" % (name, src))
            else:
                h("setp lathe.turret.chain-in 1")
            src, name = "lathe.turret.chain-out", name + "-turret"
        if sdrv:
            comp("not", "spindle-fault-not")
            comp("and2", "spindle-ok-and")
            tail.insert(0, "spindle-fault-not")
            tail.append("spindle-ok-and")
            h("net spindle-fault %s.%s => spindle-fault-not.in" % (sdrv[0], sdrv[1]["fault"]))
            h("net spindle-ok spindle-fault-not.out => spindle-ok-and.in1")
            if src:
                h("net %s %s => spindle-ok-and.in0" % (name, src))
            else:
                h("setp spindle-ok-and.in0 1")
            src, name = "spindle-ok-and.out", name + "-spindle"
        if src:
            h("net %s %s => %s" % (name, src, dst))
        else:
            h("setp %s 1" % dst)

    if e["input"] or hm2:
        h("loadrt estop_latch")
        tail.append("estop-latch.0")
        h("net estop-reset iocontrol.0.user-request-enable => estop-latch.0.reset")
        h("net estop-ok estop-latch.0.ok-out => iocontrol.0.emc-enable-in")
        if e["input"]:
            if e["invert"]:
                comp("not", "estop-not")
                tail.insert(0, "estop-not")
                h("net estop-raw %s => estop-not.in" % e["input"])
                chain("estop-not.out", "estop-latch.0.ok-in", "estop-in")
            else:
                chain(e["input"], "estop-latch.0.ok-in", "estop-in")
        else:
            chain(None, "estop-latch.0.ok-in", "estop-in")
        if hm2:
            h("net watchdog %s.watchdog.has_bit => estop-latch.0.fault-in" % board)
    else:
        chain("iocontrol.0.user-enable-out", "iocontrol.0.emc-enable-in", "estop-loop")
    p = m["probe"]
    if p["type"] == "input":
        if p["invert"]:
            comp("not", "probe-not")
            tail.append("probe-not")
            h("net probe-raw %s => probe-not.in" % p["input"])
            h("net probe probe-not.out => motion.probe-input")
        else:
            h("net probe %s => motion.probe-input" % p["input"])
    elif p["type"] == "sim":
        zj = next(i for i, j in enumerate(m["joints"]) if j["axis"] == "Z")
        h("# a simulated probe: trips when the tool tip (joint Z less the tool length) is below machine Z %s, during probe moves" % _fmt(p["sim_z"]))
        comp("sum2", "probe-tipz")
        comp("wcomp", "probe-low", "probe-moving")
        comp("conv_s32_float", "probe-mtype")
        comp("and2", "probe-and")
        tail += ["probe-tipz", "probe-low", "probe-mtype", "probe-moving", "probe-and"]
        h("net %s => probe-tipz.in0" % ("j%d" % zj if m["joints"][zj]["drive"]["type"] == "sim" else "j%d-pos-fb" % zj))  # Z's feedback signal
        h("net probe-tlo motion.tooloffset.z => probe-tipz.in1")
        h("setp probe-tipz.gain1 -1")
        h("net probe-tip probe-tipz.out => probe-low.in")
        h("setp probe-low.min -100000")
        h("setp probe-low.max %s" % _fmt(p["sim_z"]))
        h("net probe-mtype-s motion.motion-type => probe-mtype.in")
        h("net probe-mtype-f probe-mtype.out => probe-moving.in")
        h("setp probe-moving.min 4.5")
        h("setp probe-moving.max 5.5")
        h("net probe-tip-low probe-low.out => probe-and.in0")
        h("net probe-moving probe-moving.out => probe-and.in1")
        h("net probe probe-and.out => motion.probe-input")
    if turret:  # a lathe's turret: M6 turns it (lathe.turret), T prepares nothing
        tu = m["lathe"]["turret"]
        h("# the turret: %d stations, homed on its index, locked running backwards (linuxcnc/lathe)" % tu["stations"])
        for k, v in (("stations", tu["stations"]), ("offset", tu["offset"]), ("overtravel", tu["overtravel"]), ("lock", tu["lock"]),
                     ("speed", tu["speed"]), ("lock-speed", tu["lock_speed"]), ("acc", tu["acc"]), ("dwell", tu["dwell"]),
                     ("timeout", tu["timeout"]), ("slip", tu["slip"])):
            h("setp lathe.turret.%s %s" % (k, _fmt(v)))
        h("net tool-change iocontrol.0.tool-change => lathe.turret.change")
        h("net tool-changed lathe.turret.changed => iocontrol.0.tool-changed")
        h("net tool-number iocontrol.0.tool-prep-number => lathe.turret.tool")
        h("net turret-on motion.motion-enabled => lathe.turret.enable")
        if tu["type"] == "sim":  # no stepper: the command is the feedback, the index simulated
            h("net turret-pos lathe.turret.pos-cmd => lathe.turret.pos-fb")
            h("setp lathe.turret.sim-index-at %s" % _fmt(tu["sim_index_at"]))
            h("net turret-index lathe.turret.sim-index => lathe.turret.index")
        else:
            sg = tu["stepgen"]
            h("setp %s.position-scale %s" % (sg, _fmt(tu["steps"])))
            h("setp %s.maxvel %s" % (sg, _fmt(tu["speed"] * 1.25)))
            h("setp %s.maxaccel %s" % (sg, _fmt(tu["acc"] * 1.25)))
            h("net turret-pos-cmd lathe.turret.pos-cmd => %s.position-cmd" % sg)
            h("net turret-pos-fb %s.position-fb => lathe.turret.pos-fb" % sg)
            h("net turret-enable lathe.turret.enable-out => %s.enable" % sg)
            if tu["index_invert"]:
                comp("not", "turret-index-not")
                tail.insert(0, "turret-index-not")
                h("net turret-index-raw %s => turret-index-not.in" % tu["index"])
                h("net turret-index turret-index-not.out => lathe.turret.index")
            else:
                h("net turret-index %s => lathe.turret.index" % tu["index"])
        h("net turret-reset iocontrol.0.user-request-enable => lathe.turret.reset")
        tail.append("lathe.turret")
    else:
        h("net tool-change iocontrol.0.tool-change => iocontrol.0.tool-changed")
    h("net tool-prep iocontrol.0.tool-prepare => iocontrol.0.tool-prepared")
    sp = m["spindle"]
    if sp["type"] == "sim":
        h("setp spindle.0.at-speed 1")
        if lathe:  # an encoder too: threading (G33, G76) and feed per turn (G95) need it
            comp("sim_spindle", "sim-spindle")
            h("setp sim-spindle.scale 1")
            tail.insert(0, "sim-spindle")
            h("net spindle-rps spindle.0.speed-out-rps => sim-spindle.velocity-cmd spindle.0.speed-in")
            h("net spindle-revs sim-spindle.position-fb => spindle.0.revs")
            h("net spindle-index spindle.0.index-enable <=> sim-spindle.index-enable")
    elif sp["type"] == "hal":
        for k, pin, nm in (("on", "spindle.0.on", "spindle-on"), ("forward", "spindle.0.forward", "spindle-cw"),
                           ("reverse", "spindle.0.reverse", "spindle-ccw"), ("speed", "spindle.0.speed-out-abs", "spindle-rpm")):
            if sp[k]:
                h("net %s %s => %s" % (nm, pin, sp[k]))
        if sp["at_speed"]:
            h("net spindle-at-speed %s => spindle.0.at-speed" % sp["at_speed"])
        else:
            h("setp spindle.0.at-speed 1")
        if sp["revs"]:
            h("net spindle-revs %s => spindle.0.revs" % sp["revs"])
        if sp["index"]:
            h("net spindle-index spindle.0.index-enable <=> %s" % sp["index"])
        if sp["speed_in"]:
            h("net spindle-rps-fb %s => spindle.0.speed-in" % sp["speed_in"])
    elif sdrv:  # an stmbl drive in velocity mode: lathe.spindle commands it and sums its feedback into turns
        pre, pins = sdrv
        h("# the spindle: an stmbl drive in velocity mode (its pos_fb summed into spindle.0.revs, linuxcnc/lathe)")
        h("setp lathe.spindle.ratio %s" % _fmt(sp["ratio"]))
        h("net spindle-rps spindle.0.speed-out-rps => lathe.spindle.speed-rps")
        h("net spindle-on spindle.0.on => lathe.spindle.on")
        h("net spindle-vel-cmd lathe.spindle.vel-cmd => %s.%s" % (pre, pins["vel_cmd"]))
        h("net spindle-pos-cmd lathe.spindle.pos-cmd => %s.%s" % (pre, pins["pos_cmd"]))
        h("net spindle-enable motion.motion-enabled => %s.%s" % (pre, pins["enable"]))
        h("net spindle-pos-fb %s.%s => lathe.spindle.pos-fb" % (pre, pins["pos_fb"]))
        h("net spindle-vel-fb %s.%s => lathe.spindle.vel-fb" % (pre, pins["vel_fb"]))
        h("net spindle-revs lathe.spindle.revs => spindle.0.revs")
        h("net spindle-speed-fb lathe.spindle.speed-fb => spindle.0.speed-in")
        h("net spindle-index spindle.0.index-enable <=> lathe.spindle.index-enable")
        h("net spindle-at-speed lathe.spindle.at-speed => spindle.0.at-speed")
        if sp["index_sensor"]:
            h("setp lathe.spindle.use-sensor 1")
            h("net spindle-index-sensor %s => lathe.spindle.index-sensor" % sp["index_sensor"])
        con = next((k for k, x in enumerate(consoles) if x[0] is None), None)
        if con is not None:
            h("net spindle-con-fb %s.%s => sserialterm.%d.con-fb" % (pre, pins["con_fb"], con))
            h("net spindle-con-cmd sserialterm.%d.con-cmd => %s.%s" % (con, pre, pins["con_cmd"]))
    t = m["atc"]
    if t["type"] == "rack":  # the M6 remap's I/O: motion's digital outputs 0 (drawbar), 1 (air); inputs 0 (clamped), 1 (released)
        h("net atc-unclamp motion.digital-out-00 => %s" % t["unclamp"])
        if t["air"]:
            h("net atc-air motion.digital-out-01 => %s" % t["air"])
        if t["clamped"]:
            h("net atc-clamped %s => motion.digital-in-00" % t["clamped"])
        if t["released"]:
            h("net atc-released %s => motion.digital-in-01" % t["released"])
    h("")
    for f in tail + last:
        h("addf %s servo-thread" % f)
    H[loads_at:loads_at] = ["loadrt %s names=%s" % (k, ",".join(v)) for k, v in loads.items()]
    hal = "\n".join(H) + "\n"
    out = {"mill.ini": ini, "machine.hal": hal}
    if t["type"] == "rack":
        out["toolchange.ngc"] = toolchange_ngc(t)
    return out


def generate_arm(m, mdir, mill, custom=None):
    """An arm's configuration (as hwtest/arcmate-sim's, from machine.json): armkins on the model (and the changer's
    tools), each joint's drive through armguard (the cell) with armcollide's residuals, the gripper (armgrip), the
    changer (armtool, M6 = run/toolchange.ngc), the power quota; the E-STOP chain through each; the enabling
    switch on jog-inhibit."""
    run = os.path.join(mdir, "run")
    a = m["arm"]
    n = len(m["joints"])
    drv = drives_of(m)
    jdrv = [x for x in drv if x[0] is not None]
    gdrv = next((x for x in drv if x[1] == "G"), None)
    b = m["board"]
    hm2 = b["type"].startswith("hm2_")
    board = "hm2_%s.0" % b["name"] if hm2 else None
    sim = any(x[2]["type"] == "stmbl_arm" for x in jdrv)
    guard, collide, tools, quota = bool(a["cell"]), bool(a["collide"]), bool(a["tools"]), bool(a["quota"])
    grip = gdrv is not None
    path = lambda k: arm_path(a[k], mdir)  # noqa: E731
    model = path("model")
    cmodel = arm_path(a["collide_model"], mdir) if a["collide_model"] else model
    lin = [m["axes"][x] for x in "XYZ"]
    ang = [m["axes"][x] for x in "ABC"]
    maxv, maxa, maxw = max(x["max_vel"] for x in lin), max(x["max_acc"] for x in lin), max(x["max_vel"] for x in ang)
    consoles = [x for x in drv if x[5]]

    # ---------------------------------------------------------- the INI
    L = []
    w = L.append

    def sec(name, *kv):
        w("\n[%s]" % name)
        for k, v in kv:
            if v is None:
                continue
            for x in (v if isinstance(v, list) else [v]):
                w("%s = %s" % (k, _fmt(x)))

    w("# made by millweb from %s at %s: edit machine.json (the console's Configure page), not this file" % (
        os.path.join(mdir, "machine.json"), datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")))
    sec("EMC", ("VERSION", "1.1"), ("MACHINE", m["name"]), ("DEBUG", 0))
    d = m["display"]
    sec("DISPLAY", ("DISPLAY", os.path.join(mill, "millweb")), ("CYCLE_TIME", 0.1), ("POSITION_OFFSET", "RELATIVE"),
        ("POSITION_FEEDBACK", "ACTUAL"), ("GEOMETRY", "XYZABC"),
        ("MAX_LINEAR_VELOCITY", maxv), ("DEFAULT_LINEAR_VELOCITY", round(maxv / 5, 3)),
        ("MAX_ANGULAR_VELOCITY", maxw), ("DEFAULT_ANGULAR_VELOCITY", round(maxw / 5, 3)),
        ("PROGRAM_PREFIX", os.path.join(mdir, "nc_files")), ("MAX_FEED_OVERRIDE", d["max_feed_override"]))
    mw = _millweb_keys(m, mdir, run, drv)
    mw.append(("ARM_MODEL", arm_path(a["vis"], mdir) if a["vis"] else os.path.join(run, "arm.vis")))
    if a["enable"]["input"]:
        mw.append(("ENABLE_PIN", "motion.jog-inhibit"))  # the console: released while it is set
    sec("MILLWEB", *mw)
    sec("RS274NGC", ("PARAMETER_FILE", os.path.join(mdir, "linuxcnc.var")),
        ("SUBROUTINE_PATH", run if tools else None), ("REMAP", "M6 modalgroup=6 ngc=toolchange" if tools else None))
    sec("EMCMOT", ("EMCMOT", "motmod"), ("COMM_TIMEOUT", 1.0), ("SERVO_PERIOD", m["servo_period_ns"]))
    sec("TASK", ("TASK", "milltask"), ("CYCLE_TIME", 0.01))
    halfiles = [os.path.join(run, "machine.hal")]
    if (custom.strip() if custom is not None else os.path.isfile(os.path.join(mdir, "custom.hal"))):
        halfiles.append(os.path.join(mdir, "custom.hal"))
    sec("HAL", ("HALFILE", halfiles))
    sec("TRAJ", ("COORDINATES", "X Y Z A B C"), ("LINEAR_UNITS", "mm"), ("ANGULAR_UNITS", "degree"),
        ("DEFAULT_LINEAR_VELOCITY", round(maxv / 5, 3)), ("MAX_LINEAR_VELOCITY", maxv),
        ("DEFAULT_LINEAR_ACCELERATION", round(maxa / 2, 3)), ("MAX_LINEAR_ACCELERATION", maxa))
    sec("EMCIO", ("EMCIO", "io"), ("CYCLE_TIME", 0.1), ("TOOL_TABLE", os.path.join(mdir, "tool.tbl")))
    sec("KINS", ("KINEMATICS", "armkins model=%s%s" % (model, " tools=%s" % path("tools") if tools else "")), ("JOINTS", n))
    sec("STMBL_ARM", ("ARM", arm_path(a["sim"]["arm"], mdir) if sim else None), ("LIB", arm_path(a["sim"]["lib"], mdir) if sim else None),
        ("TOOLS", path("tools") if tools else None))
    if guard:
        sec("ARMGUARD", ("CELL", path("cell")))
    if guard or collide or grip:
        sec("ARMCOLLIDE", ("MODEL", cmodel))
    if quota:
        sec("POWERQUOTA", ("CONFIG", path("quota")))
    ch = a["changer"]
    sec("ARM6HMI", ("TOOL_READY", ch["ready"] or None), ("HOME_JOINTS", " ".join(_fmt(x) for x in a["home"]) or None))
    for x in "XYZABC":
        ax = m["axes"][x]
        sec("AXIS_" + x, ("MIN_LIMIT", ax["min"]), ("MAX_LIMIT", ax["max"]), ("MAX_VELOCITY", ax["max_vel"]), ("MAX_ACCELERATION", ax["max_acc"]))
    for i, j in enumerate(m["joints"]):
        h = j["home"]
        kv = [("TYPE", "ANGULAR"), ("MIN_LIMIT", j["min"]), ("MAX_LIMIT", j["max"]), ("MAX_VELOCITY", j["max_vel"]),
              ("MAX_ACCELERATION", j["max_acc"]), ("FERROR", j["ferror"]), ("MIN_FERROR", j["min_ferror"]),
              ("HOME", h["pos"]), ("HOME_SEQUENCE", h["sequence"])]
        kv += [("HOME_ABSOLUTE_ENCODER", 2)] if h["method"] == "absolute" else [("HOME_SEARCH_VEL", 0), ("HOME_LATCH_VEL", 0)]
        sec("JOINT_%d" % i, *kv)
    ini = "\n".join(L) + "\n"

    # ---------------------------------------------------------- the HAL
    H = ["# made by millweb from machine.json: edit that (the Configure page), or add to custom.hal",
         "loadrt [KINS]KINEMATICS",
         "loadrt [EMCMOT]EMCMOT servo_period_nsec=[EMCMOT]SERVO_PERIOD num_joints=[KINS]JOINTS num_dio=8 num_aio=16"]
    h = H.append
    first, last = [], []
    loads = {}

    def comp(mod, *names):
        loads.setdefault(mod, []).extend(names)
    _load_drivers(m, mdir, run, drv, h, first, last)
    if sim:
        h("loadrt stmbl_arm arm=[STMBL_ARM]ARM lib=[STMBL_ARM]LIB" + (" cell=[ARMGUARD]CELL" if guard else ""))
    if collide:
        h("loadrt armcollide model=[ARMCOLLIDE]MODEL")
    if guard:
        h("loadrt armguard model=[ARMCOLLIDE]MODEL cell=[ARMGUARD]CELL")
    if grip:
        h("loadrt armgrip model=[ARMCOLLIDE]MODEL")
    if tools:
        h("loadrt armtool model=%s tools=[STMBL_ARM]TOOLS" % model)
    if quota:
        h("loadrt powerquota config=[POWERQUOTA]CONFIG")
    # real drives work in motor radians: scale comps (the joint's degrees times its gear)
    real = [x for x in jdrv if x[2]["type"] in ("stmbl_sserial", "stmbl_can")]
    if real:
        h("loadrt scale names=%s" % ",".join("j%d-%s" % (x[0], k) for x in real for k in ("cmd", "vel", "fb", "vfb")))
    loads_at = len(H)
    for f in first:
        h("addf %s servo-thread" % f)
    for f in ["j%d-fb" % x[0] for x in real] + ["j%d-vfb" % x[0] for x in real]:
        h("addf %s servo-thread" % f)
    h("addf motion-command-handler servo-thread")
    h("addf motion-controller servo-thread")
    for f, on in (("armguard.0", guard), ("stmbl-arm.0", sim), ("armcollide.0", collide), ("armgrip.0", grip), ("armtool.0", tools), ("powerquota.0", quota)):
        if on:
            h("addf %s servo-thread" % f)
    tail = ["j%d-%s" % (x[0], k) for x in real for k in ("cmd", "vel")] + ["sserialterm.%d" % k for k in range(len(consoles))]
    h("")
    for i, nm, dd, pre, pins, con in jdrv:
        j = m["joints"][i]
        h("# joint %d (%s): %s" % (i, nm, {"sim": "simulated (its command is its feedback)", "stmbl_sserial": "stmbl drive over smart-serial",
                                            "stmbl_can": "stmbl drive over CAN", "stmbl_sim": "simulated stmbl drive",
                                            "stmbl_arm": "the simulated arm's drive"}[dd["type"]]))
        if guard:  # motion -> armguard -> the drive; the drive's feedback -> armguard -> motion
            h("net j%d-pos-cmd joint.%d.motor-pos-cmd => armguard.0.%d.motion-cmd" % (i, i, i))
            h("net j%d-vel-cmd joint.%d.vel-cmd => armguard.0.%d.motion-vel" % (i, i, i))
            cmd, vel = "armguard.0.%d.cmd" % i, "armguard.0.%d.vel" % i
        else:
            cmd, vel = "joint.%d.motor-pos-cmd" % i, "joint.%d.vel-cmd" % i
        fb_to = (["armguard.0.%d.drive-fb" % i] if guard else ["joint.%d.motor-pos-fb" % i]) + (["armcollide.0.%d.pos" % i] if collide else []) + \
            (["armgrip.0.%d.pos" % i] if grip else []) + (["armtool.0.%d.pos" % i] if tools else [])
        vfb_to = (["armcollide.0.%d.vel" % i] if collide else []) + (["armgrip.0.%d.vel" % i] if grip else [])
        if dd["type"] == "sim":
            h("net j%d-drv-fb %s => %s" % (i, cmd, " ".join(fb_to)))
        elif dd["type"] in ("stmbl_sserial", "stmbl_can"):
            k = j["gear"] * math.pi / 180  # motor rad a joint degree
            h("# the motor turns %s times a joint turn: %s rad a degree" % (_fmt(float(j["gear"])), _fmt(k)))
            for part, gain in (("cmd", k), ("vel", k), ("fb", 1 / k), ("vfb", 1 / k)):
                h("setp j%d-%s.gain %s" % (i, part, repr(gain)))
            h("net j%d-drv-cmd %s => j%d-cmd.in" % (i, cmd, i))
            h("net j%d-drv-cmd-rad j%d-cmd.out => %s.%s" % (i, i, pre, pins["pos_cmd"]))
            h("net j%d-drv-vel %s => j%d-vel.in" % (i, vel, i))
            h("net j%d-drv-vel-rad j%d-vel.out => %s.%s" % (i, i, pre, pins["vel_cmd"]))
            h("net j%d-drv-fb-rad %s.%s => j%d-fb.in" % (i, pre, pins["pos_fb"], i))
            h("net j%d-drv-fb j%d-fb.out => %s" % (i, i, " ".join(fb_to)))
            h("net j%d-vel-fb-rad %s.%s => j%d-vfb.in" % (i, pre, pins["vel_fb"], i))
            if vfb_to:
                h("net j%d-vel-fb j%d-vfb.out => %s" % (i, i, " ".join(vfb_to)))
        else:  # the simulated arm: joint degrees
            h("net j%d-drv-cmd %s => %s.%s" % (i, cmd, pre, pins["pos_cmd"]))
            h("net j%d-drv-vel %s => %s.%s" % (i, vel, pre, pins["vel_cmd"]))
            h("net j%d-drv-fb %s.%s => %s" % (i, pre, pins["pos_fb"], " ".join(fb_to)))
            if vfb_to:
                h("net j%d-vel-fb %s.%s => %s" % (i, pre, pins["vel_fb"], " ".join(vfb_to)))
        if guard:
            h("net j%d-pos-fb joint.%d.motor-pos-fb <= armguard.0.%d.fb" % (i, i, i))
        if collide:
            h("net j%d-current %s.%s => armcollide.0.%d.current" % (i, pre, pins["current"], i))
            h("net j%d-residual armcollide.0.%d.residual => %s" % (i, i, " ".join(["armguard.0.%d.residual" % i] + (["armgrip.0.%d.residual" % i] if grip else []))))
        if quota and pre:
            h("net j%d-power %s.%s => powerquota.0.power-%d" % (i, pre, pins["power"], i))
        if pre:
            h("net j%d-enable joint.%d.amp-enable-out => %s.%s" % (i, i, pre, pins["enable"]))
            h("net j%d-fault %s.%s => joint.%d.amp-fault-in" % (i, pre, pins["fault"], i))
        if con:
            k = consoles.index(next(x for x in consoles if x[0] == i))
            h("net j%d-con-fb %s.%s => sserialterm.%d.con-fb" % (i, pre, pins["con_fb"], k))
            h("net j%d-con-cmd sserialterm.%d.con-cmd => %s.%s" % (i, k, pre, pins["con_cmd"]))
    h("")
    on = [x for x, y in (("armcollide.0.enable", collide), ("armguard.0.enable", guard), ("armgrip.0.enable", grip), ("armtool.0.enable", tools)) if y]
    if on:
        h("net machine-enabled motion.motion-enabled => %s" % " ".join(on))
    resets = [x for x, y in (("armcollide.0.reset", collide), ("armguard.0.reset", guard), ("armgrip.0.reset", grip),
                             ("armtool.0.reset", tools), ("powerquota.0.reset", quota)) if y]
    if collide:
        h("net collision armcollide.0.collision => armguard.0.collision")
    if grip:  # armgrip on the gripper's drive (mm of finger gap); M64/M65 P0 close/open, M68 E0..2 what it expects
        gpre, gp = gdrv[3], gdrv[4]
        h("# the gripper: armgrip on its drive (finger gap in mm)")
        h("net grip-gap-cmd armgrip.0.gap-cmd => %s.%s" % (gpre, gp["pos_cmd"]))
        h("net grip-gap-vel armgrip.0.gap-vel => %s.%s" % (gpre, gp["vel_cmd"]))
        h("net grip-drv-en armgrip.0.drive-enable => %s.%s" % (gpre, gp["enable"]))
        h("net grip-gap-fb %s.%s => armgrip.0.gap-fb" % (gpre, gp["pos_fb"]))
        for k in ("mass", "cx", "cy", "cz"):
            h("net grip-pay-%s armgrip.0.payload-%s => armcollide.0.payload-%s" % (k, k, k))
        h("net grip-mute armgrip.0.mute => armcollide.0.mute")
        h("net grip-close motion.digital-out-00 => armgrip.0.close")
        h("net grip-width motion.analog-out-00 => armgrip.0.width-expect")
        h("net grip-mass motion.analog-out-01 => armgrip.0.mass-expect")
        h("net grip-cz motion.analog-out-02 => armgrip.0.cz")
        h("net grip-gripped armgrip.0.gripped => motion.digital-in-00")
        h("net grip-pay-ok armgrip.0.pay-ok => motion.digital-in-01")
    if tools:  # the changer: armtool watches it; armkins on the mounted tool's TCP; M6 = run/toolchange.ngc
        pin = lambda k, simpin: ch[k] or ("stmbl-arm.0.changer." + simpin if sim else "")  # noqa: E731
        h("# the tool changer (armtool), M6: run/toolchange.ngc")
        h("net tool-valve armtool.0.valve => %s" % pin("valve", "valve"))
        h("net tool-locked %s => armtool.0.locked motion.digital-in-02" % pin("locked", "locked"))
        h("net tool-unlocked %s => armtool.0.unlocked motion.digital-in-03" % pin("unlocked", "unlocked"))
        if pin("present", "present"):
            h("net tool-present %s => armtool.0.present" % pin("present", "present"))
        if pin("id", "id"):
            h("net tool-id %s => armtool.0.id" % pin("id", "id"))
        h("net tool-lock motion.digital-out-01 => armtool.0.lock")
        h("net tool-kins motion.analog-out-03 => motion.switchkins-type")
        h("net tool-kins-now armkins.tool => armtool.0.kins-tool")
        h("net tool-expect motion.analog-out-04 => armtool.0.expect")
        h("net tool-dock-sel motion.analog-out-05 => armtool.0.dock-select")
        for k in range(12):
            h("net tool-dock-%d armtool.0.dock-%d => motion.analog-in-%02d" % (k, k, k))
        if collide:
            for k in ("mass", "cx", "cy", "cz"):
                h("net tool-%s armtool.0.tool-%s => armcollide.0.tool-%s" % (k, k, k))
        if guard:
            h("net tool-mounted armtool.0.mounted => armguard.0.tool")
    if quota:  # the drives' input power into the supply's fuses; the feed scale into adaptive feed
        h("net feed-scale powerquota.0.feed-scale => motion.adaptive-feed")
    h("net tool-prep-loop iocontrol.0.tool-prepare => iocontrol.0.tool-prepared")
    h("net tool-change-loop iocontrol.0.tool-change => iocontrol.0.tool-changed")
    # the enabling switch: released (or not wired: never) stops and inhibits jogs (hold_all: every move)
    e = a["enable"]
    if e["input"]:
        h("# the enabling switch (%s): released, no jog%s" % (e["input"], ", no motion" if e["hold_all"] else ""))
        stop = ["motion.jog-inhibit", "motion.jog-stop"] + (["motion.feed-inhibit"] if e["hold_all"] else [])
        if e["invert"]:  # the pin is true while released
            h("net arm-released %s => %s" % (e["input"], " ".join(stop)))
        else:
            comp("not", "arm-enable-not")
            tail.insert(0, "arm-enable-not")
            h("net arm-enable %s => arm-enable-not.in" % e["input"])
            h("net arm-released arm-enable-not.out => %s" % " ".join(stop))
    # the E-STOP chain: the input (or the GUI's loop) -> armguard (a collision, a protective stop) -> powerquota ->
    # armgrip -> armtool -> LinuxCNC (through estop_latch with an input or a board's watchdog)
    est = m["estop"]

    def chain(src, dst):
        name = "estop-chain"
        if guard:
            comp("not", "guard-not")
            comp("and2", "guard-and")
            tail.extend(["guard-not", "guard-and"])
            h("net guard-estop armguard.0.estop => guard-not.in")
            h("net guard-ok guard-not.out => guard-and.in1")
            h("net %s %s => guard-and.in0" % (name, src)) if src else h("setp guard-and.in0 1")
            src, name = "guard-and.out", name + "-guard"
        for stage, on in (("powerquota.0", quota), ("armgrip.0", grip), ("armtool.0", tools)):
            if on:
                h("net %s %s => %s.chain-in" % (name, src, stage)) if src else h("setp %s.chain-in 1" % stage)
                src, name = stage + ".chain-out", name + "-" + stage.split(".")[0]
        h("net %s %s => %s" % (name, src, dst)) if src else h("setp %s 1" % dst)

    if est["input"] or hm2:
        h("loadrt estop_latch")
        tail.append("estop-latch.0")
        resets.append("estop-latch.0.reset")
        h("net estop-ok estop-latch.0.ok-out => iocontrol.0.emc-enable-in")
        if est["input"] and est["invert"]:
            comp("not", "estop-not")
            tail.insert(0, "estop-not")
            h("net estop-raw %s => estop-not.in" % est["input"])
            chain("estop-not.out", "estop-latch.0.ok-in")
        else:
            chain(est["input"] or None, "estop-latch.0.ok-in")
        if hm2:
            h("net watchdog %s.watchdog.has_bit => estop-latch.0.fault-in" % board)
    else:
        chain("iocontrol.0.user-enable-out", "iocontrol.0.emc-enable-in")
    if resets:
        h("net estop-reset iocontrol.0.user-request-enable => %s" % " ".join(resets))
    h("")
    for f in tail + last:
        h("addf %s servo-thread" % f)
    H[loads_at:loads_at] = ["loadrt %s names=%s" % (k, ",".join(v)) for k, v in loads.items()]
    out = {"mill.ini": ini, "machine.hal": "\n".join(H) + "\n"}
    if not a["vis"]:  # the console's 3D model: the arm model's bodies, its joints in DH theta (as armkins')
        out["arm.vis"] = "# made by millweb from machine.json: the arm's model for the console\njoints theta\ninclude %s\n" % model
    if tools:  # M6 and the dock reader, as hwtest/arcmate-sim's; the changer's tools as the first tool.tbl
        for f in ("toolchange.ngc", "dockread.ngc"):
            out[f] = open(os.path.join(REPO, "hwtest", "arcmate-sim", f)).read()
        try:
            names = [line.split()[1:3] for line in open(path("tools")) if line.split()[:1] == ["tool"]]
            out[TOOL_TBL_INIT] = "".join("T%s P%s ; %s (tool changer tool %s)\n" % (t[0], t[0], t[1] if len(t) > 1 else "", t[0]) for t in names)
        except OSError:
            pass
    return out


def toolchange_ngc(t):
    """The rack changer's M6 (run/toolchange.ngc): the tool in the spindle back to its pocket, the new one out of
    its own; a tool beyond the pockets is changed by hand (a message and a pause)."""
    f = _fmt
    P = "#<_pk>"
    px, py = "[%s + [%s - 1] * %s]" % (f(t["x"]), P, f(t["dx"])), "[%s + [%s - 1] * %s]" % (f(t["y"]), P, f(t["dy"]))
    sl = float(t["slide"])
    out_y = "[%s - %s]" % (py, f(sl)) if sl else py  # where the holder is clear of its fork, at the pocket's height
    above = f(min(t["z_pocket"] + 5, t["z_clear"]))

    def wait(n, what):  # a sensor: wait for it (abort when it doesn't come); none: the dwell
        pin = t["clamped" if n == 0 else "released"]
        if not pin:
            return ["G4 P%s" % f(t["dwell"])]
        return ["M66 P%d L3 Q%s" % (n, f(t["timeout"])),
                "o<tc-w%d> if [#5399 LT 0]" % n, "  (abort, tool change: the drawbar didn't %s)" % what, "o<tc-w%d> endif" % n]
    L = ["; made by millweb from machine.json's atc: edit that (the Configure page's Tools tab), not this file",
         "; M6 remap: tool n in pocket n (1..%d); the others by hand" % t["pockets"],
         "o<toolchange> sub", "M70", "o<tc-same> if [#<_selected_tool> EQ #<_current_tool>]", "  M72", "  o<toolchange> return", "o<tc-same> endif",
         "M5 M9", "G90 G21 G40 G49", "G53 G0 Z%s" % f(t["z_clear"]),
         "; the tool in the spindle back into its pocket",
         "o<tc-put> if [[#<_current_tool> GT 0] AND [#<_current_tool> LE %d]]" % t["pockets"],
         "  %s = #<_current_tool>" % P,
         "  G53 G0 X%s Y%s" % (px, out_y), "  G53 G0 Z%s" % above, "  G53 G1 Z%s F%s" % (f(t["z_pocket"]), f(t["feed"]))]
    if sl:
        L.append("  G53 G1 Y%s" % py)
    L += ["  M64 P0"] + ["  " + x for x in wait(1, "release")] + ["  G53 G0 Z%s" % f(t["z_clear"])]
    L += ["o<tc-put> elseif [#<_current_tool> GT 0]", "  (MSG, take T#<_current_tool> out of the spindle by hand, then cycle start)", "  M0",
          "o<tc-put> endif",
          "; the new tool out of its pocket",
          "o<tc-get> if [[#<_selected_tool> GT 0] AND [#<_selected_tool> LE %d]]" % t["pockets"],
          "  %s = #<_selected_tool>" % P,
          "  G53 G0 X%s Y%s" % (px, py), "  M64 P0"]
    if t["air"]:
        L.append("  M64 P1")
    L += ["  G53 G0 Z%s" % above, "  G53 G1 Z%s F%s" % (f(t["z_pocket"]), f(t["feed"])), "  M65 P0"] + ["  " + x for x in wait(0, "clamp")]
    if t["air"]:
        L.append("  M65 P1")
    if sl:
        L.append("  G53 G1 Y%s" % out_y)
    L += ["  G53 G0 Z%s" % f(t["z_clear"]),
          "o<tc-get> elseif [#<_selected_tool> GT 0]", "  M65 P0", "  (MSG, put T#<_selected_tool> into the spindle by hand, then cycle start)", "  M0",
          "o<tc-get> else", "  M65 P0", "  M65 P1",
          "o<tc-get> endif",
          "M61 Q#<_selected_tool>", "M72", "o<toolchange> endsub", "M2", ""]
    return "\n".join(L)


# ------------------------------------------------------------------ the folder
def paths(mdir):
    return {k: os.path.join(mdir, v) for k, v in (("machine", "machine.json"), ("custom", "custom.hal"), ("history", "history"),
                                                  ("run", "run"), ("logs", "logs"), ("bitstreams", "bitstreams"))}


def read(mdir):
    """(machine.json's text, custom.hal's text or "")."""
    p = paths(mdir)
    try:
        text = open(p["machine"]).read()
    except FileNotFoundError:
        text = json.dumps(DEFAULT, indent=2) + "\n"
    try:
        custom = open(p["custom"]).read()
    except FileNotFoundError:
        custom = ""
    return text, custom


def parse(text):
    try:
        return json.loads(text)
    except ValueError as e:
        raise Invalid([("", "machine.json isn't JSON: %s" % e)])


def check(mdir, text, custom, mill):
    """Validate a machine.json text (and custom.hal) and make its configuration:
    {"machine": normalized, "files": {name: text}}; Invalid if it can't."""
    m = validate(parse(text), mdir)
    if custom and not isinstance(custom, str):
        raise Invalid([("custom.hal", "text")])
    files = generate(m, mdir, mill, custom or "")
    return {"machine": m, "files": files}


def _atomic(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path + ".tmp", "w") as f:
        f.write(text)
    os.replace(path + ".tmp", path)


def save(mdir, text, custom, mill, who="", note=""):
    """Check, then write machine.json and custom.hal; the old ones go into history/.
    Returns the history entry's id."""
    check(mdir, text, custom, mill)
    p = paths(mdir)
    old_text, old_custom = read(mdir)
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S-%f")
    os.makedirs(p["history"], exist_ok=True)
    _atomic(p["machine"], text if text.endswith("\n") else text + "\n")
    if custom.strip():
        _atomic(p["custom"], custom if custom.endswith("\n") else custom + "\n")
    elif os.path.exists(p["custom"]):
        os.remove(p["custom"])
    if not any(x.endswith(".json") for x in os.listdir(p["history"])) and os.path.exists(p["machine"]):
        with open(os.path.join(p["history"], "00000000-000000-000000.json"), "w") as f:  # the file as it was before the first save
            json.dump({"time": "", "who": "", "note": "as it was before the first save", "machine": old_text, "custom": old_custom}, f)
    hid = stamp
    with open(os.path.join(p["history"], hid + ".json"), "w") as f:
        json.dump({"time": datetime.datetime.now().isoformat(timespec="seconds"), "who": who, "note": note,
                   "machine": text, "custom": custom}, f)
    hist = sorted(x for x in os.listdir(p["history"]) if x.endswith(".json"))
    for x in hist[:-50]:  # the last 50
        os.remove(os.path.join(p["history"], x))
    return hid


def history(mdir):
    p = paths(mdir)
    out = []
    try:
        names = sorted((x for x in os.listdir(p["history"]) if x.endswith(".json")), reverse=True)
    except FileNotFoundError:
        return []
    for x in names:
        try:
            h = json.load(open(os.path.join(p["history"], x)))
        except (OSError, ValueError):
            continue
        out.append({"id": x[:-5], "time": h.get("time"), "who": h.get("who", ""), "note": h.get("note", "")})
    return out


def history_get(mdir, hid):
    if not re.match(r"^[0-9-]+$", str(hid)):
        raise ValueError("no such version")
    try:
        h = json.load(open(os.path.join(paths(mdir)["history"], hid + ".json")))
    except (OSError, ValueError):
        raise ValueError("no such version")
    return h["machine"], h.get("custom", "")


def diff(a, b, name="machine.json"):
    return "".join(difflib.unified_diff(a.splitlines(True), b.splitlines(True), "saved/" + name, "new/" + name, n=2))


TOOL_TBL_INIT = "tool.tbl.init"  # in generate's files: the machine's first tool.tbl (not a run/ file)


def write_run(mdir, files):
    """The configuration into run/, and the files LinuxCNC needs that aren't there yet."""
    p = paths(mdir)
    os.makedirs(p["run"], exist_ok=True)
    files = dict(files)
    tbl = files.pop(TOOL_TBL_INIT, None) or "T1 P1 D6.0 Z+0.000 ;6 mm endmill\n"
    for name, text in files.items():
        _atomic(os.path.join(p["run"], name), text)
    for d in ("nc_files", "logs", "bitstreams"):
        os.makedirs(os.path.join(mdir, d), exist_ok=True)
    for f, text in ((("tool.tbl", tbl),) if files else ()) + (("linuxcnc.var", ""),):  # tool.tbl: once a configuration is made (an arm's: its changer's tools)
        if not os.path.exists(os.path.join(mdir, f)):
            with open(os.path.join(mdir, f), "w") as fh:
                fh.write(text)


def init(mdir, text=None):
    """A new machine folder (machine.json from text, else the default)."""
    os.makedirs(mdir, exist_ok=True)
    if not os.path.exists(os.path.join(mdir, "machine.json")):
        _atomic(os.path.join(mdir, "machine.json"), text or (json.dumps(DEFAULT, indent=2) + "\n"))
    write_run(mdir, {})


def copy_tree(src, dst):
    """A machine folder's files from a template (tests)."""
    shutil.copytree(src, dst, dirs_exist_ok=True)
