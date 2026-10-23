#!/usr/bin/env python3
"""stmbl drive configs across firmware versions (docs/firmware_versions.md).

A drive's flash config is text: `link pid`, `conf0.r = 2.15 # a comment`,
`io0.out0 = fault0.mot_brake`. Since firmware version 1 it carries the
version it was written for, a line `#stmbl_conf_version N`. When a firmware
version changes what a config must say (a pin renamed, a default that
changed, a template split), conf/patches/NNNN.json says how a config for
version N-1 becomes one for N, in operations that each have an exact
inverse, so a config goes up to a newer firmware and back down to an older:

    {"op": "rename", "from": "comp0.old", "to": "comp0.new"}   the pin, wherever it appears
    {"op": "set", "pin": "comp0.x", "from": 1, "to": 2}         only if it is still `from`; from null: a new line;
                                                                 to null: the line goes
    {"op": "line", "from": "link old", "to": "link new"}       a whole line; from null: added, to null: removed

    stmblconf.py version CONF
    stmblconf.py upgrade CONF [--to N] [-o OUT]       (N: this checkout's STMBL_FW_VERSION)
    stmblconf.py downgrade CONF --to N [-o OUT]
    stmblconf.py plan CONF --to N                     what would change, nothing written

Exact undo is the copy taken before (the console keeps one per change);
the inverse patches are for a config edited after an upgrade, when the
firmware goes back.
"""
import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
PATCHES = os.path.join(ROOT, "conf", "patches")
MARKER = "#stmbl_conf_version"
OPS = ("rename", "set", "line")


def fw_version(root=ROOT):
    """This checkout's STMBL_FW_VERSION (shared/version_number.h)."""
    with open(os.path.join(root, "shared", "version_number.h")) as f:
        m = re.search(r"#define\s+STMBL_FW_VERSION\s+(\d+)", f.read())
    return int(m.group(1)) if m else 0


def version_of(text):
    """The version a config was written for; 0: before versioning (no marker)."""
    for line in text.splitlines():
        if line.startswith(MARKER):
            try:
                return int(line[len(MARKER):].split()[0])
            except (ValueError, IndexError):
                return 0
    return 0


def with_version(text, v):
    """The text with its marker for version v (first line), or none for 0."""
    lines = [l for l in text.splitlines() if not l.startswith(MARKER)]
    if v:
        lines.insert(0, "%s %d" % (MARKER, v))
    return "\n".join(lines) + ("\n" if lines else "")


class Conflict(ValueError):
    pass


# ------------------------------------------------------------------ patches
def load_patches(d=PATCHES):
    """{version: patch} from conf/patches (each checked)."""
    out = {}
    if not os.path.isdir(d):
        return out
    for name in sorted(os.listdir(d)):
        if not re.match(r"^\d{4}\.json$", name):
            continue
        with open(os.path.join(d, name)) as f:
            p = json.load(f)
        check_patch(p, name)
        if p["version"] != int(name[:4]):
            raise ValueError("%s: version %s, not its name's" % (name, p["version"]))
        out[p["version"]] = p
    return out


def check_patch(p, where="patch"):
    if not isinstance(p, dict) or not isinstance(p.get("version"), int) or p["version"] < 1:
        raise ValueError("%s: a version (an integer from 1)" % where)
    for i, op in enumerate(p.get("ops", [])):
        w = "%s op %d" % (where, i)
        if op.get("op") not in OPS:
            raise ValueError("%s: op is one of %s" % (w, ", ".join(OPS)))
        if op["op"] == "rename" and not (PIN.match(str(op.get("from", ""))) and PIN.match(str(op.get("to", "")))):
            raise ValueError("%s: rename from and to are pins (comp0.pin)" % w)
        if op["op"] == "set" and not PIN.match(str(op.get("pin", ""))):
            raise ValueError("%s: set needs a pin" % w)
        if op["op"] == "set" and op.get("from") is None and op.get("to") is None:
            raise ValueError("%s: set from or to" % w)
        if op["op"] == "line" and op.get("from") is None and op.get("to") is None:
            raise ValueError("%s: line from or to" % w)


def invert(op):
    """The operation that undoes op."""
    if op["op"] == "rename":
        return dict(op, **{"from": op["to"], "to": op["from"]})
    return dict(op, **{"from": op.get("to"), "to": op.get("from")})


# ------------------------------------------------------------------ applying
PIN = re.compile(r"^[A-Za-z_][A-Za-z_0-9]*[0-9]+\.[A-Za-z_0-9]+$")
ASSIGN = re.compile(r"^\s*([A-Za-z_][A-Za-z_0-9]*[0-9]+\.[A-Za-z_0-9]+)\s*=\s*([^#]*?)\s*(#.*)?$")


def _body(line):
    """A line without its comment and outer spaces."""
    return line.split("#", 1)[0].strip() if not line.startswith(MARKER) else line


def _same_value(a, b):
    try:
        return abs(float(a) - float(b)) <= 1e-9 * max(1.0, abs(float(a)))
    except (TypeError, ValueError):
        return str(a).strip() == str(b).strip()


def _fmt(v):
    if isinstance(v, float) and v.is_integer():
        return str(int(v))
    return str(v)


def apply_op(lines, op):
    """lines changed by op (in place); a list of what it did, for people."""
    did = []
    if op["op"] == "rename":
        pat = re.compile(r"(?<![A-Za-z0-9_.])%s(?![A-Za-z0-9_])" % re.escape(op["from"]))
        if any(re.search(r"(?<![A-Za-z0-9_.])%s(?![A-Za-z0-9_])" % re.escape(op["to"]), l) for l in lines if not l.startswith("#")):
            raise Conflict("%s is in the config already: renaming %s onto it couldn't be undone" % (op["to"], op["from"]))
        for i, l in enumerate(lines):
            if l.startswith("#"):
                continue
            body, sep, comment = l.partition("#")
            nb = pat.sub(op["to"], body)
            if nb != body:
                did.append("%s → %s" % (l.strip(), (nb + sep + comment).strip()))
                lines[i] = nb + sep + comment
    elif op["op"] == "set":
        pin, frm, to = op["pin"], op.get("from"), op.get("to")
        idx = next((i for i, l in enumerate(lines) if (ASSIGN.match(l) or [None, None])[1] == pin), None)
        if idx is None:
            if frm is None and to is not None:
                lines.append("%s = %s" % (pin, _fmt(to)))
                did.append("added %s = %s" % (pin, _fmt(to)))
        else:
            m = ASSIGN.match(lines[idx])
            if frm is not None and _same_value(m.group(2), frm):
                if to is None:
                    did.append("removed %s" % lines[idx].strip())
                    del lines[idx]
                else:
                    old = lines[idx]
                    lines[idx] = "%s = %s%s" % (pin, _fmt(to), (" " + m.group(3)) if m.group(3) else "")
                    did.append("%s → %s" % (old.strip(), lines[idx].strip()))
            elif frm is not None:
                did.append("kept %s (not %s: set by hand)" % (lines[idx].strip(), _fmt(frm)))
    else:  # line
        frm, to = op.get("from"), op.get("to")
        if frm is None:
            if not any(_body(l) == to.strip() for l in lines):
                lines.append(to)
                did.append("added %s" % to)
        else:
            idx = next((i for i, l in enumerate(lines) if _body(l) == frm.strip()), None)
            if idx is not None:
                if to is None:
                    did.append("removed %s" % lines[idx].strip())
                    del lines[idx]
                else:
                    did.append("%s → %s" % (lines[idx].strip(), to))
                    lines[idx] = to
    return did


def migrate(text, to, patches=None):
    """(new text, steps): the config made for firmware version `to`, up or down.
    steps: [{version, direction, description, changes}]."""
    patches = load_patches() if patches is None else patches
    cur = version_of(text)
    lines = [l for l in text.splitlines() if not l.startswith(MARKER)]
    steps = []
    if to > cur:
        for v in range(cur + 1, to + 1):
            p = patches.get(v, {"version": v, "description": "nothing to change", "ops": []})
            changes = []
            for op in p.get("ops", []):
                changes += apply_op(lines, op)
            steps.append({"version": v, "direction": "up", "description": p.get("description", ""), "changes": changes})
    elif to < cur:
        for v in range(cur, to, -1):
            p = patches.get(v, {"version": v, "description": "nothing to change", "ops": []})
            changes = []
            for op in reversed(p.get("ops", [])):
                changes += apply_op(lines, invert(op))
            steps.append({"version": v, "direction": "down", "description": p.get("description", ""), "changes": changes})
    return with_version("\n".join(lines), to), steps


# ------------------------------------------------------------------ the command line
def main(argv=None):
    ap = argparse.ArgumentParser(description="stmbl drive configs across firmware versions")
    sub = ap.add_subparsers(dest="cmd", required=True)
    for name in ("version", "upgrade", "downgrade", "plan"):
        p = sub.add_parser(name)
        p.add_argument("conf")
        if name != "version":
            p.add_argument("--to", type=int, default=None)
            p.add_argument("-o", "--out")
    a = ap.parse_args(argv)
    text = open(a.conf).read()
    if a.cmd == "version":
        print(version_of(text))
        return 0
    to = fw_version() if a.to is None else a.to
    if a.cmd == "downgrade" and a.to is None:
        ap.error("downgrade: --to N")
    if (a.cmd == "upgrade" and to < version_of(text)) or (a.cmd == "downgrade" and to > version_of(text)):
        ap.error("%s: the config is for v%d" % (a.cmd, version_of(text)))
    try:
        new, steps = migrate(text, to)
    except Conflict as e:
        sys.exit("stmblconf: %s" % e)
    for s in steps:
        print("v%d %s: %s" % (s["version"], s["direction"], s["description"]))
        for c in s["changes"]:
            print("    " + c)
    if a.cmd != "plan":
        out = a.out or a.conf
        if out == a.conf:
            os.replace(a.conf, a.conf + ".v%d.bak" % version_of(text)) if os.path.exists(a.conf) else None
        with open(out, "w") as f:
            f.write(new)
        print("%s: v%d" % (out, to))
    return 0


if __name__ == "__main__":
    sys.exit(main())
