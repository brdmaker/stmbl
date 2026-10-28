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
import random
import re
import shutil

VERSION = 1
AXES = "XYZABCUVW"
BOARDS = ("none", "hm2_eth", "hm2_spix", "hm2_rpspi", "hm2_soc_ol")
DRIVES = ("sim", "stmbl_sserial", "stmbl_can", "stmbl_sim")
LOADERS = ("none", "fpga_manager", "command")
HOMING = ("absolute", "immediate", "switch", "index")
# the machine's layout: a gantry (the bridge moves in Y over a fixed table, one or two Y joints) or a fixed
# bridge over a table that moves in Y (Scripta CN 720 style: one joint an axis); the console draws it
TYPES = ("gantry", "bridge")
CONSOLE_KEYS = {  # [MILLWEB] keys the console section may set as extra (the rest have fields of their own)
    "POLL_HZ", "HAL_HZ", "PREVIEW_SEGS", "PREVIEW_TIMEOUT", "PREVIEW_NICE", "DRIVE_POLL_MS", "CERT", "KEY", "PCOMP",
    "SPINDLE_LOAD_PIN", "BUS_PIN", "YAW_PIN", "CONTOUR_PIN", "AT_SPEED_PIN", "REMOTE_ENABLE_PIN", "AIR_PIN",
    "WORKSPACE_X", "WORKSPACE_Y", "WORKSPACE_Z"}
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
    "spindle": {"type": "sim", "max_rpm": 24000, "min_rpm": 0, "on": "", "forward": "", "reverse": "", "speed": "", "at_speed": ""},
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
}
MOVES = re.compile(r"^(?:-?([XYZ]))?(?:-?([XYZ]))?(?:-?([XYZ]))?$")


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
    """Each joint's name: its axis, numbered when an axis has more than one (X, Y1, Y2, Z)."""
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
        if not isinstance(j, dict) or j.get("axis") not in m["axes"]:
            errs.append((w + ".axis", "one of the axes (%s)" % "".join(m["axes"])))
            continue
        ax = m["axes"][j["axis"]]
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
        elif d["type"] == "stmbl_sim":
            sims += 1
            conf = d.setdefault("conf", "drive.conf")
            if not isinstance(conf, str) or conf.startswith("/") or ".." in conf.split("/"):
                errs.append((w + ".drive.conf", "the simulated drive's flash config, a file in the machine folder"))
            elif mdir and not os.path.isfile(os.path.join(mdir, conf)):
                errs.append((w + ".drive.conf", "%s isn't in the machine folder" % conf))
        d.setdefault("console", d.get("type") != "sim")
        if h["method"] not in HOMING:
            errs.append((w + ".home.method", "one of " + ", ".join(HOMING)))
        _num(h["sequence"], w + ".home.sequence", errs, -9, 9, True)
        _num(h["pos"], w + ".home.pos", errs)
        if h["method"] == "switch":
            if h["switch"] == "sim":  # a simulated joint's own switch (sim_home_switch), sim_at from where it starts
                if d.get("type") != "sim":
                    errs.append((w + ".home.switch", "sim: only for a simulated joint (a drive's switch is a HAL pin)"))
                if h.setdefault("sim_at", 10) != "random":  # "random": a new start position at every start
                    _num(h["sim_at"], w + ".home.sim_at", errs)
            elif not HALNAME.match(str(h["switch"])):
                errs.append((w + ".home.switch", "the home switch's HAL pin (sim: a simulated one)"))
            if not h["search_vel"]:
                errs.append((w + ".home.search_vel", "the search speed (its sign is the direction)"))
        if h["method"] in ("switch", "index") and not h["latch_vel"]:
            errs.append((w + ".home.latch_vel", "the latch speed"))
        if h["method"] == "index" and d.get("type") == "sim":
            errs.append((w + ".home", "a simulated joint has no index"))
        # an stmbl drive that autophases at enable (no commutation signals) moves
        # its rotor up to half an electrical period: a larger following error
        # until the joint is homed and has been on phasing_time s
        if j.get("phasing_ferror") not in (None, 0):
            if d.get("type") not in ("stmbl_sserial", "stmbl_can", "stmbl_sim"):
                errs.append((w + ".phasing_ferror", "only for an stmbl drive"))
            _num(j["phasing_ferror"], w + ".phasing_ferror", errs, 0)
            _num(j.setdefault("phasing_time", 1.5), w + ".phasing_time", errs, 0, 60)
    if sims > 1:
        errs.append(("joints", "one simulated stmbl drive at most (stmbl_sim is a single drive)"))
    for a in m["axes"]:
        if not any(j.get("axis") == a for j in m["joints"] if isinstance(j, dict)):
            errs.append(("axes." + a, "no joint moves it"))
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
    if sp["type"] not in ("none", "sim", "hal"):
        errs.append(("spindle.type", "none, sim or hal"))
    _num(sp["max_rpm"], "spindle.max_rpm", errs, 0, 200000)
    _num(sp["min_rpm"], "spindle.min_rpm", errs, 0, 200000)
    for k in ("on", "forward", "reverse", "speed", "at_speed"):
        if sp[k] and not HALNAME.match(str(sp[k])):
            errs.append(("spindle." + k, "a HAL pin name"))
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
            errs.append((w + ".moves", "the axes it moves with: '' (fixed), Y, XY, XYZ, -X (a moving table) …"))
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


# ------------------------------------------------------------------ making the configuration
def _fmt(v):
    if isinstance(v, bool):
        return "YES" if v else "NO"
    if isinstance(v, float):
        return ("%.6f" % v).rstrip("0").rstrip(".")
    return str(v)


def drives_of(m):
    """[(joint index, joint name, drive, HAL prefix, pin names {pos_cmd, ...}, console tty name or None)]."""
    names, out = joint_names(m), []
    b = m["board"]
    for i, j in enumerate(m["joints"]):
        d = j["drive"]
        t = d["type"]
        if t == "stmbl_sserial":
            pre = "hm2_%s.0.stbl.%d.%d" % (b["name"], d["port"], d["channel"])
            pins = {k: k for k in ("pos_cmd", "vel_cmd", "pos_fb", "vel_fb", "enable", "fault", "index_enable", "current", "power", "con_cmd", "con_fb")}
        elif t == "stmbl_can":
            pre = "stmblcan.%d.%d" % (d["bus"], d["node"])
            pins = {k: k.replace("_", "-") for k in ("pos_cmd", "vel_cmd", "pos_fb", "vel_fb", "enable", "fault", "index_enable", "current", "power", "con_cmd", "con_fb")}
        elif t == "stmbl_sim":
            pre = "stmbl-sim.0"
            pins = {k: k for k in ("pos_cmd", "vel_cmd", "pos_fb", "vel_fb", "enable", "fault", "index_enable", "current", "power", "con_cmd", "con_fb")}
        else:
            pre, pins = None, None
        out.append((i, names[i], d, pre, pins, ("drive-%s" % names[i].lower()) if pre and d.get("console") else None))
    return out


def sim_starts(m, rng=None):
    """sim_at "random": where each such axis starts this time, in machine
    coordinates: anywhere in its joints' travel short of their switches
    (at HOME_OFFSET, in the search direction), one start an axis (a gantry's
    two sides together), so each start homes from somewhere new."""
    rng = rng or random.Random()
    lo, hi = {}, {}
    for j in m["joints"]:
        h = j["home"]
        if h["method"] != "switch" or h.get("switch") != "sim" or h.get("sim_at") != "random":
            continue
        a = j["axis"]
        sw = float(h["offset"])
        l, u = (float(j["min"]), sw - 1.0) if h["search_vel"] >= 0 else (sw + 1.0, float(j["max"]))
        lo[a], hi[a] = max(lo.get(a, l), l), min(hi.get(a, u), u)
    return {a: round(rng.uniform(lo[a], hi[a]), 3) for a in lo if hi[a] > lo[a]}


def sim_switch_distance(j, starts):
    """How far a simulated joint starts from its simulated home switch, in its
    search direction."""
    h = j["home"]
    if h["sim_at"] != "random":
        return abs(float(h["sim_at"]))
    if j["axis"] not in starts:
        return 10.0
    return round(abs(float(h["offset"]) - starts[j["axis"]]), 3)


def generate(m, mdir, mill, custom=None, rng=None):
    """{"mill.ini": text, "machine.hal": text} for machine m (validated) in machine
    folder mdir; mill is webui/mill (millweb's launcher is there). custom: the
    custom.hal text to be (None: whether the folder has one)."""
    run = os.path.join(mdir, "run")
    names = joint_names(m)
    coords = "".join(j["axis"] for j in m["joints"])
    dup = len(set(coords)) < len(coords)
    drv = drives_of(m)
    b = m["board"]
    hm2 = b["type"].startswith("hm2_")
    board = "hm2_%s.0" % b["name"] if hm2 else None
    cans = [x for x in drv if x[2]["type"] == "stmbl_can"]
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
        ("POSITION_FEEDBACK", "ACTUAL"), ("MAX_LINEAR_VELOCITY", maxv), ("DEFAULT_LINEAR_VELOCITY", round(maxv / 10, 3)),
        ("PROGRAM_PREFIX", os.path.join(mdir, "nc_files")), ("MAX_FEED_OVERRIDE", d["max_feed_override"]),
        ("MAX_SPINDLE_OVERRIDE", d["max_spindle_override"]), ("MIN_SPINDLE_OVERRIDE", d["min_spindle_override"]))
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
    sec("MILLWEB", *mw)
    atc = m["atc"]["type"] == "rack"
    sec("RS274NGC", ("PARAMETER_FILE", os.path.join(mdir, "linuxcnc.var")),
        ("SUBROUTINE_PATH", run if atc else None), ("REMAP", "M6 modalgroup=6 ngc=toolchange" if atc else None))
    sec("EMCMOT", ("EMCMOT", "motmod"), ("COMM_TIMEOUT", 1.0), ("SERVO_PERIOD", m["servo_period_ns"]))
    sec("TASK", ("TASK", "milltask"), ("CYCLE_TIME", 0.01))
    halfiles = [os.path.join(run, "machine.hal")]
    if (custom.strip() if custom is not None else os.path.isfile(os.path.join(mdir, "custom.hal"))):
        halfiles.append(os.path.join(mdir, "custom.hal"))
    phasing = [i for i, j in enumerate(m["joints"]) if j.get("phasing_ferror")]
    sec("HAL", ("HALFILE", halfiles), ("POSTGUI_HALFILE", os.path.join(run, "postgui.hal") if phasing else None))
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
        if j.get("phasing_ferror"):
            kv += [("PHASING_FERROR", j["phasing_ferror"]), ("PHASING_TIME", j["phasing_time"])]
        sec("JOINT_%d" % i, *kv)
    ini = "\n".join(L) + "\n"

    # ---------------------------------------------------------- the HAL
    H = ["# made by millweb from machine.json: edit that (the Configure page), or add to custom.hal",
         "loadrt [KINS]KINEMATICS",
         "loadrt [EMCMOT]EMCMOT servo_period_nsec=[EMCMOT]SERVO_PERIOD num_joints=[KINS]JOINTS" + (" num_dio=4" if m["atc"]["type"] == "rack" else "")]
    h = H.append
    first, last = [], []  # functions before motion (read) and after it (write)
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
    # joints with a screw lead (mech): the drive works in motor radians, LinuxCNC in mm; scale comps between
    mech = {i: m["joints"][i]["mech"] for i, n, dd, pre, pins, con in drv if pre and m["joints"][i].get("mech")}
    if mech:
        h("loadrt scale names=%s" % ",".join("j%d-%s" % (i, k) for i in sorted(mech) for k in ("cmd", "vel", "fb")))
        first += ["j%d-fb" % i for i in sorted(mech)]
    for f in first:
        h("addf %s servo-thread" % f)
    h("addf motion-command-handler servo-thread")
    h("addf motion-controller servo-thread")
    tail = []  # functions between motion and the write
    for k, x in enumerate(consoles):
        tail.append("sserialterm.%d" % k)
    h("")
    for i, n, dd, pre, pins, con in drv:
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
    simsw = [i for i, j in enumerate(m["joints"]) if j["home"]["method"] == "switch" and j["home"]["switch"] == "sim"]
    starts = sim_starts(m, rng)
    if simsw:
        h("# simulated home switches: sim_at from where each joint starts, in the search direction")
        h("loadrt sim_home_switch names=%s" % ",".join("j%d-homesw" % i for i in simsw))
    for i, j in enumerate(m["joints"]):
        if i in simsw:
            hs = j["home"]
            h("net j%d => j%d-homesw.cur-pos" % (i, i))
            d = sim_switch_distance(j, starts)
            if hs["sim_at"] == "random":
                h("# joint %d starts at %s %s, %s from its switch, this time (sim_at random)" % (i, j["axis"], _fmt(starts.get(j["axis"], 0.0)), _fmt(d)))
            h("setp j%d-homesw.home-pos %s" % (i, _fmt(d * (1 if hs["search_vel"] >= 0 else -1))))
            h("net j%d-home-sw j%d-homesw.home-sw => joint.%d.home-sw-in" % (i, i, i))
            tail.append("j%d-homesw" % i)
        elif j["home"]["method"] == "switch":
            h("net j%d-home-sw %s => joint.%d.home-sw-in" % (i, j["home"]["switch"], i))
    h("")
    # E-STOP: the input (true when the chain is closed) and a board's watchdog; else a loop
    e = m["estop"]
    if e["input"] or hm2:
        h("loadrt estop_latch")
        tail.append("estop-latch.0")
        h("net estop-reset iocontrol.0.user-request-enable => estop-latch.0.reset")
        h("net estop-ok estop-latch.0.ok-out => iocontrol.0.emc-enable-in")
        if e["input"]:
            if e["invert"]:
                h("loadrt not names=estop-not")
                tail.insert(0, "estop-not")
                h("net estop-raw %s => estop-not.in" % e["input"])
                h("net estop-in estop-not.out => estop-latch.0.ok-in")
            else:
                h("net estop-in %s => estop-latch.0.ok-in" % e["input"])
        else:
            h("setp estop-latch.0.ok-in 1")
        if hm2:
            h("net watchdog %s.watchdog.has_bit => estop-latch.0.fault-in" % board)
    else:
        h("net estop-loop iocontrol.0.user-enable-out => iocontrol.0.emc-enable-in")
    p = m["probe"]
    if p["type"] == "input":
        if p["invert"]:
            h("loadrt not names=probe-not")
            tail.append("probe-not")
            h("net probe-raw %s => probe-not.in" % p["input"])
            h("net probe probe-not.out => motion.probe-input")
        else:
            h("net probe %s => motion.probe-input" % p["input"])
    elif p["type"] == "sim":
        zj = next(i for i, j in enumerate(m["joints"]) if j["axis"] == "Z")
        h("# a simulated probe: trips when the tool tip (joint Z less the tool length) is below machine Z %s, during probe moves" % _fmt(p["sim_z"]))
        h("loadrt sum2 names=probe-tipz")
        h("loadrt wcomp names=probe-low,probe-moving")
        h("loadrt conv_s32_float names=probe-mtype")
        h("loadrt and2 names=probe-and")
        tail += ["probe-tipz", "probe-low", "probe-mtype", "probe-moving", "probe-and"]
        # Z in machine coordinates (the joint's position), not the motor's: an
        # incremental axis counts from where it started until it is homed
        h("net probe-z joint.%d.pos-fb => probe-tipz.in0" % zj)
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
    h("net tool-change iocontrol.0.tool-change => iocontrol.0.tool-changed")
    h("net tool-prep iocontrol.0.tool-prepare => iocontrol.0.tool-prepared")
    sp = m["spindle"]
    if sp["type"] == "sim":
        h("setp spindle.0.at-speed 1")
    elif sp["type"] == "hal":
        for k, pin, nm in (("on", "spindle.0.on", "spindle-on"), ("forward", "spindle.0.forward", "spindle-cw"),
                           ("reverse", "spindle.0.reverse", "spindle-ccw"), ("speed", "spindle.0.speed-out-abs", "spindle-rpm")):
            if sp[k]:
                h("net %s %s => %s" % (nm, pin, sp[k]))
        if sp["at_speed"]:
            h("net spindle-at-speed %s => spindle.0.at-speed" % sp["at_speed"])
        else:
            h("setp spindle.0.at-speed 1")
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
    hal = "\n".join(H) + "\n"
    out = {"mill.ini": ini, "machine.hal": hal}
    if phasing:
        out["postgui.hal"] = _phasing_hal(m, phasing, drv)
    if t["type"] == "rack":
        out["toolchange.ngc"] = toolchange_ngc(t)
    return out


def _phasing_hal(m, joints, drv):
    """The following error while a drive may autophase: [JOINT_n]PHASING_FERROR
    until the joint is homed and has been enabled PHASING_TIME s, then FERROR /
    MIN_FERROR. Loaded after the display starts (POSTGUI_HALFILE): it sets
    ini.N.ferror, pins LinuxCNC makes after the HALFILEs. mux4 and timedelay
    only: a uspace module loads once, and machine.hal may load and2, not, mux2."""
    names = ",".join
    H = ["# made by millweb from machine.json: a larger following error while a drive autophases",
         "loadrt timedelay names=%s" % names("j%d-phasing-on" % i for i in joints),
         "loadrt mux4 names=%s" % names("j%d-phasing-%s" % (i, k) for i in joints for k in ("ferror", "min-ferror"))]
    for i in joints:
        H.append("addf j%d-phasing-on servo-thread" % i)
        H.append("addf j%d-phasing-ferror servo-thread" % i)
        H.append("addf j%d-phasing-min-ferror servo-thread" % i)
    for i in joints:
        H += ["# joint %d: sel 3 (homed and on PHASING_TIME s): the normal limits" % i,
              "setp j%d-phasing-on.on-delay [JOINT_%d]PHASING_TIME" % (i, i),
              "setp j%d-phasing-on.off-delay 0" % i,
              "net j%d-enable => j%d-phasing-on.in" % (i, i),
              "net j%d-phasing-homed joint.%d.homed => j%d-phasing-ferror.sel0 j%d-phasing-min-ferror.sel0" % (i, i, i, i),
              "net j%d-phasing-settled j%d-phasing-on.out => j%d-phasing-ferror.sel1 j%d-phasing-min-ferror.sel1" % (i, i, i, i)]
        for k, ini in (("ferror", "FERROR"), ("min-ferror", "MIN_FERROR")):
            for n in range(3):
                H.append("setp j%d-phasing-%s.in%d [JOINT_%d]PHASING_FERROR" % (i, k, n, i))
            H.append("setp j%d-phasing-%s.in3 [JOINT_%d]%s" % (i, k, i, ini))
            H.append("net j%d-phasing-%s j%d-phasing-%s.out => ini.%d.%s" % (i, k, i, k, i, k.replace("-", "_")))
    return "\n".join(H) + "\n"


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


def write_run(mdir, files):
    """The configuration into run/, and the files LinuxCNC needs that aren't there yet."""
    p = paths(mdir)
    os.makedirs(p["run"], exist_ok=True)
    for name, text in files.items():
        _atomic(os.path.join(p["run"], name), text)
    for d in ("nc_files", "logs", "bitstreams"):
        os.makedirs(os.path.join(mdir, d), exist_ok=True)
    for f, text in (("tool.tbl", "T1 P1 D6.0 Z+0.000 ;6 mm endmill\n"), ("linuxcnc.var", "")):
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
