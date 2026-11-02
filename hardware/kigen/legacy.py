"""kigen.legacy: KiCad 5 (legacy) schematics to KiCad 7, the drawing kept.

KiCad 7's kicad-cli can't read the legacy .sch format, and the stmbl v4.0
schematic (hw/kicad/v4.0) is legacy. This converts a sheet hierarchy and its
symbol cache library to .kicad_sch files with every symbol, field, wire,
junction, no-connect, label, note and sheet in its place, so kicad-cli can
export, check and plot it, and KiCad 7 opens it as a KiCad 7 project.

  convert(src_dir, root_sch, cache_lib, out_dir, project, rename=None)

root_sch becomes <project>.kicad_sch; every sheet it uses, <name>.kicad_sch.
A sheet used twice (stmbl's FB0 and FB1 are one fb.sch) stays one file with
two instances, each with its own references (the legacy "AR Path" lines).
Coordinates: legacy mils -> mm (x 0.0254), on the same 50-mil grid.

What it doesn't carry: bitmaps (logos), De Morgan alternates, bus entries
(stmbl has none). Fields keep their text, position and visibility; their
justification is approximate (it doesn't change connectivity).
"""
import os
import re
import uuid

from sexp import Sym, dump

MIL = 0.0254
NS = uuid.UUID("2b8f0f6e-6c63-4b7e-9a51-7374616e6c67")


def mm(v):
    return round(float(v) * MIL, 4)


def uid(*key):
    return str(uuid.uuid5(NS, "/".join(str(k) for k in key)))


def S(name, *a):
    return [Sym(name)] + list(a)


def eff(size_mil, hide=False, justify=None, italic=False, bold=False):
    f = [Sym("font"), [Sym("size"), mm(size_mil), mm(size_mil)]]
    if italic:
        f.append(Sym("italic"))
    if bold:
        f.append(Sym("bold"))
    e = [Sym("effects"), f]
    if justify:
        e.append([Sym("justify")] + [Sym(j) for j in justify])
    if hide:
        e.append(Sym("hide"))
    return e


def stroke(kind="default"):
    return S("stroke", S("width", 0), S("type", Sym(kind)))


def unquote(s):
    s = s.strip()
    return s[1:-1].replace('\\"', '"') if s.startswith('"') and s.endswith('"') else s


def text(s):
    """legacy text escapes: ~ is a space in labels and pin names; overbar markup kept"""
    return s


# ------------------------------------------------------------------ the symbol library
PIN_TYPE = {"I": "input", "O": "output", "B": "bidirectional", "T": "tri_state", "P": "passive", "U": "unspecified",
            "W": "power_in", "w": "power_out", "C": "open_collector", "E": "open_emitter", "N": "no_connect"}
PIN_ANGLE = {"R": 0, "U": 90, "L": 180, "D": 270}
FILL = {"N": "none", "F": "outline", "f": "background"}


def parse_lib(path):
    """{name: legacy DEF block lines}"""
    out, cur, name = {}, None, None
    for line in open(path, encoding="utf-8", errors="replace"):
        line = line.rstrip("\n")
        if line.startswith("DEF "):
            name = line.split()[1].lstrip("~")
            cur = [line]
        elif cur is not None:
            cur.append(line)
            if line.startswith("ENDDEF"):
                out[name] = cur
                cur = None
    return out


def split_fields(line):
    return re.findall(r'"(?:[^"\\]|\\.)*"|\S+', line)


def lib_symbol(name, lines, lib_id=None):
    """A legacy DEF as a KiCad 7 symbol (lib_symbols entry named lib_id)."""
    d = split_fields(lines[0])
    ref, text_off, show_num, show_name, units, locked, power = d[2], int(d[4]), d[5] == "Y", d[6] == "Y", int(d[7]), d[8], d[9]
    lib_id = lib_id or name
    short = lib_id.split(":")[-1]
    props, graphics = [], {}
    field_names = {0: "Reference", 1: "Value", 2: "Footprint", 3: "Datasheet"}
    in_draw = False
    for line in lines[1:]:
        f = split_fields(line)
        if not f:
            continue
        if f[0].startswith("F") and f[0][1:].isdigit() and not in_draw:
            n = int(f[0][1:])
            val = unquote(f[1])
            x, y, size, orient, vis = mm(f[2]), mm(f[3]), int(f[4]), f[5], f[6]
            pname = field_names.get(n, unquote(f[9]) if len(f) > 9 else "Field%d" % n)
            if n == 0 and val == "":
                val = ref
            props.append(S("property", pname, val, S("at", x, y, 90 if orient == "V" else 0), eff(size, hide=vis == "I")))
        elif f[0] == "DRAW":
            in_draw = True
        elif f[0] == "ENDDRAW":
            in_draw = False
        elif in_draw:
            item, unit, conv = draw_item(f)
            if item is None or conv == 2:
                continue
            graphics.setdefault(unit, []).append(item)
    sym = [Sym("symbol"), lib_id]
    if power == "P":
        sym.append(S("power"))
    if not show_num:
        sym.append(S("pin_numbers", Sym("hide")))
    pn = S("pin_names", S("offset", mm(text_off)))
    if not show_name:
        pn.append(Sym("hide"))
    sym.append(pn)
    sym += [S("in_bom", Sym("yes")), S("on_board", Sym("yes"))]
    have = {p[1] for p in props}
    for k in ("Reference", "Value", "Footprint", "Datasheet"):
        if k not in have:
            props.append(S("property", k, "", S("at", 0, 0, 0), eff(50, hide=True)))
    sym += props
    if units > 1:  # a pin in every unit (an op-amp's supply) is one shared pin: unit 0
        def nums(items):
            return {[q for q in e if isinstance(q, list) and q and q[0] == "number"][0][1]
                    for e in items if isinstance(e, list) and e and e[0] == "pin"}
        shared = set.intersection(*[nums(graphics.get(u, [])) for u in range(1, units + 1)])
        for n in sorted(shared):
            first = True
            for u in range(1, units + 1):
                keep = []
                for e in graphics.get(u, []):
                    if isinstance(e, list) and e and e[0] == "pin" and n in nums([e]):
                        if first:
                            graphics.setdefault(0, []).append(e)
                            first = False
                        continue
                    keep.append(e)
                graphics[u] = keep
    if units > 1 and lib_id in COMMON_UNIT and graphics.get(0):
        # KiCad 7 lists a shared pin once per placed unit: put them on the unit the drawing wires them on
        u = COMMON_UNIT[lib_id]
        pins0 = [e for e in graphics[0] if isinstance(e, list) and e and e[0] == "pin"]
        graphics[0] = [e for e in graphics[0] if e not in pins0]
        graphics.setdefault(u, []).extend(pins0)
    for unit in sorted(graphics):
        sym.append([Sym("symbol"), "%s_%d_1" % (short, unit)] + graphics[unit])
    return sym


def draw_item(f):
    k = f[0]
    if k == "X":  # X name num x y length orient sizenum sizename unit convert etype [shape]
        name, num, x, y, ln, orient = f[1], f[2], mm(f[3]), mm(f[4]), mm(f[5]), f[6]
        unit, conv, etype = int(f[9]), int(f[10]), f[11]
        shape = f[12] if len(f) > 12 else ""
        p = [Sym("pin"), Sym(PIN_TYPE.get(etype, "unspecified")),
             Sym({"I": "inverted", "C": "clock", "CI": "inverted_clock", "L": "input_low", "CL": "clock_low",
                  "V": "output_low", "F": "edge_clock_high", "X": "non_logic"}.get(shape.replace("N", ""), "line")),
             S("at", x, y, PIN_ANGLE.get(orient, 0)), S("length", ln)]
        if "N" in shape:
            p.append(Sym("hide"))
        p += [S("name", "~" if name == "~" else name.replace("~", " ") if False else name, eff(int(f[8]))),
              S("number", num, eff(int(f[7])))]
        return p, unit, conv
    if k == "S":  # S x1 y1 x2 y2 unit convert width fill
        return [Sym("rectangle"), S("start", mm(f[1]), mm(f[2])), S("end", mm(f[3]), mm(f[4])),
                S("stroke", S("width", mm(f[7])), S("type", Sym("default"))), S("fill", S("type", Sym(FILL.get(f[8], "none"))))], \
            int(f[5]), int(f[6])
    if k == "C":  # C x y r unit convert width fill
        return [Sym("circle"), S("center", mm(f[1]), mm(f[2])), S("radius", mm(f[3])),
                S("stroke", S("width", mm(f[6])), S("type", Sym("default"))), S("fill", S("type", Sym(FILL.get(f[7], "none"))))], \
            int(f[4]), int(f[5])
    if k == "P":  # P n unit convert width x y ... fill
        n = int(f[1])
        pts = [S("xy", mm(f[5 + 2 * i]), mm(f[6 + 2 * i])) for i in range(n)]
        fill = f[5 + 2 * n] if len(f) > 5 + 2 * n else "N"
        return [Sym("polyline"), [Sym("pts")] + pts, S("stroke", S("width", mm(f[4])), S("type", Sym("default"))),
                S("fill", S("type", Sym(FILL.get(fill, "none"))))], int(f[2]), int(f[3])
    if k == "A":  # A x y r start end unit convert width fill sx sy ex ey
        cx, cy, r = float(f[1]), float(f[2]), float(f[3])
        sx, sy, ex, ey = float(f[10]), float(f[11]), float(f[12]), float(f[13])
        import math
        a0, a1 = math.atan2(sy - cy, sx - cx), math.atan2(ey - cy, ex - cx)
        if int(f[4]) > int(f[5]):
            a0, a1 = a1, a0
            sx, sy, ex, ey = ex, ey, sx, sy
        if a1 < a0:
            a1 += 2 * math.pi
        am = (a0 + a1) / 2
        mx, my = cx + r * math.cos(am), cy + r * math.sin(am)
        return [Sym("arc"), S("start", mm(sx), mm(sy)), S("mid", mm(mx), mm(my)), S("end", mm(ex), mm(ey)),
                S("stroke", S("width", mm(f[8])), S("type", Sym("default"))), S("fill", S("type", Sym(FILL.get(f[9], "none"))))], \
            int(f[6]), int(f[7])
    if k == "T":  # T angle x y size hidden unit convert text italic bold hjust vjust
        txt = unquote(f[8]).replace("~", " ")
        return [Sym("text"), txt, S("at", mm(f[2]), mm(f[3]), int(f[1]) / 10.0),
                eff(int(f[4]), hide=f[5] == "1")], int(f[6]), int(f[7])
    return None, 0, 0


# ------------------------------------------------------------------ orientation
def orient(m):
    """legacy transform (x1 y1 x2 y2) -> (angle, mirror) of a KiCad 7 symbol.
    The transform maps library (x, y up) to sheet (x, y down); KiCad 7's (at .. angle)
    rotates counter-clockwise on the sheet, then (mirror x|y) flips."""
    want = tuple(int(v) for v in m)
    for angle in (0, 90, 180, 270):
        for mirror in (None, "x", "y"):
            if transform(angle, mirror) == want:
                return angle, mirror
    raise ValueError("transform %r" % (m,))


def transform(angle, mirror):
    # lib (lx, ly) -> sheet (sx, sy); the identity is (lx, -ly)
    a, b, c, d = 1, 0, 0, -1
    for _ in range(angle // 90):  # counter-clockwise on a y-down sheet: (sx, sy) -> (sy, -sx)
        a, b, c, d = c, d, -a, -b
    if mirror == "x":   # about the x axis: sy -> -sy
        c, d = -c, -d
    elif mirror == "y":  # about the y axis: sx -> -sx
        a, b = -a, -b
    return (a, b, c, d)


# ------------------------------------------------------------------ a sheet
def parse_sch(path):
    lines = open(path, encoding="utf-8", errors="replace").read().split("\n")
    out = {"comps": [], "wires": [], "junctions": [], "noconn": [], "texts": [], "sheets": [], "paper": "A4"}
    i = 0
    while i < len(lines):
        ln = lines[i]
        if ln.startswith("$Descr"):
            out["paper"] = ln.split()[1]
        elif ln.startswith("$Comp"):
            c = {"fields": [], "ar": []}
            i += 1
            while not lines[i].startswith("$EndComp"):
                f = split_fields(lines[i])
                if f and f[0] == "L":
                    c["lib"], c["ref"] = f[1], f[2]
                elif f and f[0] == "U":
                    c["unit"], c["ts"] = int(f[1]), f[3]
                elif f and f[0] == "P":
                    c["x"], c["y"] = int(f[1]), int(f[2])
                elif f and f[0] == "AR":
                    kv = dict(re.findall(r'(\w+)="([^"]*)"', lines[i]))
                    c["ar"].append((kv["Path"], kv["Ref"], int(kv.get("Part", "1") or 1)))
                elif f and f[0] == "F":
                    c["fields"].append(f)
                elif f and lines[i].startswith("\t") and len(f) == 4 and "m" not in c and c.get("pos3"):
                    c["m"] = f
                elif f and lines[i].startswith("\t") and len(f) == 3:
                    c["pos3"] = f
                i += 1
            out["comps"].append(c)
        elif ln.startswith("Wire ") or ln.startswith("Entry "):
            kind = ln.split()[1]
            x1, y1, x2, y2 = (int(v) for v in lines[i + 1].split())
            out["wires"].append((ln.split()[0], kind, x1, y1, x2, y2))
            i += 1
        elif ln.startswith("Connection"):
            f = ln.split()
            out["junctions"].append((int(f[2]), int(f[3])))
        elif ln.startswith("NoConn"):
            f = ln.split()
            out["noconn"].append((int(f[2]), int(f[3])))
        elif ln.startswith("Text "):
            f = ln.split()
            kind, x, y, spin, size = f[1], int(f[2]), int(f[3]), int(f[4]), int(f[5])
            shape = f[6] if kind in ("HLabel", "GLabel") else None
            txt = lines[i + 1]
            out["texts"].append((kind, x, y, spin, size, shape, txt))
            i += 1
        elif ln.startswith("$Sheet"):
            s = {"pins": []}
            i += 1
            while not lines[i].startswith("$EndSheet"):
                f = split_fields(lines[i])
                if f[0] == "S":
                    s["x"], s["y"], s["w"], s["h"] = (int(v) for v in f[1:5])
                elif f[0] == "U":
                    s["ts"] = f[1]
                elif f[0] == "F0":
                    s["name"], s["nsize"] = unquote(f[1]), int(f[2])
                elif f[0] == "F1":
                    s["file"], s["fsize"] = unquote(f[1]), int(f[2])
                elif f[0].startswith("F"):
                    s["pins"].append((unquote(f[1]), f[2], f[3], int(f[4]), int(f[5]), int(f[6])))
                i += 1
            out["sheets"].append(s)
        i += 1
    return out


LABEL_SHAPE = {"I": "input", "O": "output", "B": "bidirectional", "T": "tri_state", "U": "passive",
               "Input": "input", "Output": "output", "BiDi": "bidirectional", "3State": "tri_state", "UnSpc": "passive"}


def label_angle(kind, spin):
    """legacy spin -> KiCad 7 label angle and justification. Local labels and notes: 0 text
    to the right of the point, 1 up, 2 to the left, 3 down. Hierarchical and global labels:
    the shape's point is at (x, y); 0 pointing left (text to the right) ... as KiCad 5 drew them."""
    if kind in ("Label", "Notes"):
        return {0: (0, ["left", "bottom"]), 1: (90, ["left", "bottom"]), 2: (180, ["right", "bottom"]), 3: (270, ["right", "bottom"])}[spin]
    return {0: (180, ["right"]), 1: (90, ["left"]), 2: (0, ["left"]), 3: (270, ["right"])}[spin]


def sheet_sexp(sch, lib_syms, project, sheet_paths, file_uuid, page_of):
    """One sheet. sheet_paths: the instance paths this file is used under, {legacy path prefix: KiCad 7 path}."""
    used = {}
    body = []
    for c in sch["comps"]:
        lib = c["lib"]
        if lib not in lib_syms:
            raise SystemExit("no symbol %s in the cache library" % lib)
        used[lib] = lib_syms[lib]
        angle, mirror = orient(c["m"])
        props = []
        names = {0: "Reference", 1: "Value", 2: "Footprint", 3: "Datasheet"}
        for f in c["fields"]:
            n = int(f[1])
            val = unquote(f[2])
            o, x, y, size, vis = f[3], int(f[4]), int(f[5]), int(f[6]), f[7]
            hj = f[8] if len(f) > 8 else "C"
            vj = f[9] if len(f) > 9 else "CNN"
            pname = names.get(n, unquote(f[10]) if len(f) > 10 else "Field%d" % n)
            # legacy stores a field's offset from the symbol before the symbol's transform: on the sheet it is
            # T x offset (measured on stmbl v4.0's plots); KiCad 7 stores it on the sheet, and turns and
            # justifies the text itself from the symbol's orientation
            t = [int(v) for v in c["m"]]
            dx, dy = x - c["x"], y - c["y"]
            x, y = c["x"] + t[0] * dx + t[1] * dy, c["y"] + t[2] * dx + t[3] * dy
            vert = o == "V"
            just = []
            if hj in ("L", "R"):
                just.append({"L": "left", "R": "right"}[hj])
            if vj[0] in ("T", "B"):
                just.append({"T": "top", "B": "bottom"}[vj[0]])
            props.append(S("property", pname, val, S("at", mm(x), mm(y), 90 if vert else 0),
                           eff(size, hide=vis[-1] == "1", justify=just or None, italic=len(vj) > 1 and vj[1] == "I",
                               bold=len(vj) > 2 and vj[2] == "B")))
        unit = c.get("unit", 1)
        sym = [Sym("symbol"), S("lib_id", lib), S("at", mm(c["x"]), mm(c["y"]), angle)]
        if mirror:
            sym.append(S("mirror", Sym(mirror)))
        sym += [S("unit", unit), S("in_bom", Sym("yes")), S("on_board", Sym("yes")), S("dnp", Sym("no")),
                S("uuid", uid(file_uuid, "comp", c["ts"]))] + props
        # pins (uuids)
        for p in pins_of(lib_syms[lib], unit):
            sym.append(S("pin", p, S("uuid", uid(file_uuid, "comp", c["ts"], "pin", p))))
        # instances: one per use of this file
        inst = []
        for lpath, kpath in sheet_paths.items():
            ref, part = c["ref"], unit
            for p, r, u in c["ar"]:
                if p == lpath + "/" + c["ts"] or (lpath == "" and p == "/" + c["ts"]):
                    ref, part = r, u
            inst.append(S("path", kpath, S("reference", ref), S("unit", part)))
        sym.append(S("instances", [Sym("project"), project] + inst))
        body.append(sym)
    for kind0, kind, x1, y1, x2, y2 in sch["wires"]:
        if kind0 == "Wire" and kind == "Wire":
            body.append(S("wire", S("pts", S("xy", mm(x1), mm(y1)), S("xy", mm(x2), mm(y2))), stroke(),
                          S("uuid", uid(file_uuid, "w", x1, y1, x2, y2))))
        elif kind0 == "Wire" and kind == "Bus":
            body.append(S("bus", S("pts", S("xy", mm(x1), mm(y1)), S("xy", mm(x2), mm(y2))), stroke(),
                          S("uuid", uid(file_uuid, "b", x1, y1, x2, y2))))
        elif kind0 == "Wire" and kind == "Notes":
            body.append(S("polyline", S("pts", S("xy", mm(x1), mm(y1)), S("xy", mm(x2), mm(y2))), stroke("dash"),
                          S("uuid", uid(file_uuid, "n", x1, y1, x2, y2))))
    for x, y in sch["junctions"]:
        body.append(S("junction", S("at", mm(x), mm(y)), S("diameter", 0), S("color", 0, 0, 0, 0), S("uuid", uid(file_uuid, "j", x, y))))
    for x, y in sch["noconn"]:
        body.append(S("no_connect", S("at", mm(x), mm(y)), S("uuid", uid(file_uuid, "nc", x, y))))
    for kind, x, y, spin, size, shape, txt in sch["texts"]:
        ang, just = label_angle(kind, spin)
        u = S("uuid", uid(file_uuid, "t", kind, x, y, txt))
        if kind == "Notes":
            body.append(S("text", txt.replace("\\n", "\n"), S("at", mm(x), mm(y), ang), eff(size, justify=just), u))
        elif kind == "Label":
            body.append(S("label", txt, S("at", mm(x), mm(y), ang), eff(size, justify=just), u))
        elif kind == "GLabel":
            body.append(S("global_label", txt, S("shape", Sym(LABEL_SHAPE.get(shape, "passive"))), S("at", mm(x), mm(y), ang),
                          S("fields_autoplaced"), eff(size, justify=just), u))
        elif kind == "HLabel":
            body.append(S("hierarchical_label", txt, S("shape", Sym(LABEL_SHAPE.get(shape, "passive"))), S("at", mm(x), mm(y), ang),
                          eff(size, justify=just), u))
    for s in sch["sheets"]:
        sid = uid("sheet", s["ts"])
        sh = [Sym("sheet"), S("at", mm(s["x"]), mm(s["y"])), S("size", mm(s["w"]), mm(s["h"])), S("fields_autoplaced"),
              S("stroke", S("width", 0.1524), S("type", Sym("solid"))), S("fill", S("color", 0, 0, 0, 0.0)), S("uuid", sid),
              S("property", "Sheetname", s["name"], S("at", mm(s["x"]), mm(s["y"]) - 0.7, 0), eff(s["nsize"], justify=["left", "bottom"])),
              S("property", "Sheetfile", s["file"].replace(".sch", ".kicad_sch"), S("at", mm(s["x"]), mm(s["y"] + s["h"]) + 0.6, 0),
                eff(s["fsize"], justify=["left", "top"]))]
        for name, shape, side, x, y, size in s["pins"]:
            ang = {"L": 180, "R": 0, "T": 90, "B": 270}[side]
            sh.append(S("pin", name, Sym(LABEL_SHAPE.get(shape, "passive")), S("at", mm(x), mm(y), ang),
                        eff(size, justify=["left" if side == "R" else "right"]), S("uuid", uid(sid, "pin", name))))
        inst = [S("path", kpath, S("page", page_of[(kpath, s["ts"])])) for kpath in sheet_paths.values()]
        sh.append(S("instances", [Sym("project"), project] + inst))
        body.append(sh)
    return used, body


def pins_of(sym, unit):
    out = []
    for e in sym:
        if isinstance(e, list) and e and e[0] == "symbol":
            u = int(e[1].rsplit("_", 2)[-2])
            if u not in (0, unit):
                continue
            for p in e:
                if isinstance(p, list) and p and p[0] == "pin":
                    for q in p:
                        if isinstance(q, list) and q and q[0] == "number":
                            out.append(q[1])
    return out


def file_sexp(paper, title, used, body, file_uuid, root=False):
    f = [Sym("kicad_sch"), S("version", 20230121), S("generator", Sym("eeschema")), S("uuid", file_uuid), S("paper", paper)]
    if title:
        f.append(S("title_block", S("title", title)))
    f.append([Sym("lib_symbols")] + [used[k] for k in sorted(used)])
    f += body
    if root:
        f.append(S("sheet_instances", S("path", "/", S("page", "1"))))
    return f


def convert(src_dir, root_sch, cache_lib, out_dir, project, title=""):
    """Convert root_sch and the sheets under it. Returns {file: parsed legacy sheet} for edits."""
    libdefs = parse_lib(os.path.join(src_dir, cache_lib))
    lib_syms = {name: lib_symbol(name, lines) for name, lines in libdefs.items()}
    root_uuid = uid("root", project)
    # walk the hierarchy: (legacy path, KiCad path, file)
    parsed = {}
    uses = {}  # file -> {legacy path: kicad path}
    order = []
    page_of = {}

    def walk(fname, lpath, kpath):
        if fname not in parsed:
            parsed[fname] = parse_sch(os.path.join(src_dir, fname))
            order.append(fname)
        uses.setdefault(fname, {})[lpath] = kpath
        for s in parsed[fname]["sheets"]:
            page_of[(kpath, s["ts"])] = str(len(page_of) + 2)
            walk(s["file"], lpath + "/" + s["ts"], kpath + "/" + uid("sheet", s["ts"]))

    walk(root_sch, ROOT_LPATH.get(root_sch, ""), "/" + root_uuid)
    out = {}
    for fname in order:
        file_uuid = root_uuid if fname == root_sch else uid("file", fname)
        used, body = sheet_sexp(parsed[fname], lib_syms, project, uses[fname], file_uuid, page_of)
        name = project if fname == root_sch else os.path.splitext(fname)[0]
        out[name] = (parsed[fname]["paper"], used, body, file_uuid, fname == root_sch)
    return out, lib_syms


ROOT_LPATH = {}
COMMON_UNIT = {}  # multi-unit symbol -> the unit its shared (power) pins go on  # legacy path of a root that was a sub-sheet in the original (its AR Path prefix)


def write(out, out_dir, title):
    os.makedirs(out_dir, exist_ok=True)
    for name, (paper, used, body, fu, root) in out.items():
        with open(os.path.join(out_dir, name + ".kicad_sch"), "w") as f:
            f.write(dump(file_sexp(paper, title if root else "", used, body, fu, root)) + "\n")
