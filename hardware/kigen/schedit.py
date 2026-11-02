"""kigen.schedit: edit KiCad 7 schematic sheets in place (the drawing kept).

Used to turn the converted stmbl v4.0 sheets into stman's (hardware/stman):
remove symbols and the wires left dangling, relabel a pin, swap power
symbols, add symbols with labelled pins, wires, labels, sheets.

Coordinates are mm on the sheet, y down. A symbol's pin position is its
library position through the symbol's transform (legacy.transform).
"""
import uuid

from sexp import Sym, parse, dump, find, findall
import legacy

NS = uuid.UUID("0c4f8a8e-6d55-4e5e-8b7d-736368656469")
G = 2.54


def uid(*k):
    return str(uuid.uuid5(NS, "/".join(str(x) for x in k)))


def S(name, *a):
    return [Sym(name)] + list(a)


def r4(v):
    return round(v, 4)


def key(x, y):
    return (round(x, 2), round(y, 2))


class Sheet:
    def __init__(self, path, project):
        self.path, self.project = path, project
        self.t = parse(open(path).read())
        self.libs = {s[1]: s for s in find(self.t, "lib_symbols")[1:]}

    def save(self, path=None):
        with open(path or self.path, "w") as f:
            f.write(dump(self.t) + "\n")

    # ------------------------------------------------------------ queries
    def items(self, kind):
        return [e for e in self.t if isinstance(e, list) and e and e[0] == kind]

    def symbols(self):
        return self.items("symbol")

    @staticmethod
    def refs(sym):
        inst = find(sym, "instances")
        out = []
        if inst:
            for proj in findall(inst, "project"):
                for p in findall(proj, "path"):
                    out.append(find(p, "reference")[1])
        return out

    def by_ref(self, ref):
        return [s for s in self.symbols() if ref in self.refs(s)]

    def pin_points(self, sym):
        """{pin number: (x, y)} of a placed symbol"""
        lib = self.libs[find(sym, "lib_id")[1]]
        at = find(sym, "at")
        X, Y, ang = float(at[1]), float(at[2]), int(float(at[3])) if len(at) > 3 else 0
        mir = find(sym, "mirror")
        a, b, c, d = legacy.transform(ang, str(mir[1]) if mir else None)
        unit = int(find(sym, "unit")[1]) if find(sym, "unit") else 1
        out = {}
        for u in findall(lib, "symbol"):
            un = int(u[1].rsplit("_", 2)[-2])
            if un not in (0, unit):
                continue
            for p in findall(u, "pin"):
                pa = find(p, "at")
                lx, ly = float(pa[1]), float(pa[2])
                out[find(p, "number")[1]] = (r4(X + a * lx + b * ly), r4(Y + c * lx + d * ly))
        return out

    def points(self):
        """connection points: {(x, y): [what]} -- pins, labels, sheet pins, no-connects, wire ends"""
        pts = {}

        def add(x, y, what):
            pts.setdefault(key(x, y), []).append(what)
        for s in self.symbols():
            for n, (x, y) in self.pin_points(s).items():
                add(x, y, ("pin", s, n))
        for k in ("label", "global_label", "hierarchical_label"):
            for l in self.items(k):
                at = find(l, "at")
                add(float(at[1]), float(at[2]), (k, l))
        for nc in self.items("no_connect"):
            at = find(nc, "at")
            add(float(at[1]), float(at[2]), ("nc", nc))
        for sh in self.items("sheet"):
            for p in findall(sh, "pin"):
                at = find(p, "at")
                add(float(at[1]), float(at[2]), ("sheetpin", sh, p))
        for w in self.items("wire"):
            for xy in findall(find(w, "pts"), "xy"):
                add(float(xy[1]), float(xy[2]), ("wire", w))
        return pts

    @staticmethod
    def wire_ends(w):
        a, b = findall(find(w, "pts"), "xy")
        return (float(a[1]), float(a[2])), (float(b[1]), float(b[2]))

    def on_wire(self, x, y, exclude=None):
        """wires passing through (x, y) (not just ending there)"""
        out = []
        for w in self.items("wire"):
            if w is exclude:
                continue
            (x1, y1), (x2, y2) = self.wire_ends(w)
            if abs(x1 - x2) < 1e-6 and abs(x - x1) < 1e-3 and min(y1, y2) - 1e-3 <= y <= max(y1, y2) + 1e-3:
                out.append(w)
            elif abs(y1 - y2) < 1e-6 and abs(y - y1) < 1e-3 and min(x1, x2) - 1e-3 <= x <= max(x1, x2) + 1e-3:
                out.append(w)
        return out

    # ------------------------------------------------------------ edits
    def remove(self, e):
        self.t[:] = [x for x in self.t if x is not e]

    def remove_ref(self, *refs):
        for ref in refs:
            syms = self.by_ref(ref)
            if not syms:
                raise SystemExit("%s: no %s" % (self.path, ref))
            for s in syms:
                self.remove(s)

    def cleanup(self):
        """Remove what the removals left dangling: wires with a free end, power symbols and labels on
        nothing, junctions joining fewer than three wires."""
        for _ in range(200):
            changed = False
            pts = self.points()
            for w in self.items("wire"):
                for (x, y) in self.wire_ends(w):
                    others = [t for t in pts.get(key(x, y), []) if not (t[0] == "wire" and t[1] is w)]
                    if not others and not self.on_wire(x, y, exclude=w):
                        self.remove(w)
                        changed = True
                        break
            if changed:
                continue
            pts = self.points()
            for s in self.symbols():
                if not str(find(s, "lib_id")[1]).split(":")[-1] or not self.is_power(s):
                    continue
                (x, y), = self.pin_points(s).values()
                here = [t for t in pts.get(key(x, y), []) if not (t[0] == "pin" and t[1] is s)]
                if not here and not self.on_wire(x, y):
                    self.remove(s)
                    changed = True
            for k in ("label", "global_label", "hierarchical_label"):
                for l in self.items(k):
                    at = find(l, "at")
                    x, y = float(at[1]), float(at[2])
                    here = [t for t in pts.get(key(x, y), []) if t[0] in ("wire", "pin")]
                    if not here and not self.on_wire(x, y):
                        self.remove(l)
                        changed = True
            for j in self.items("junction"):
                at = find(j, "at")
                x, y = float(at[1]), float(at[2])
                n = len([t for t in pts.get(key(x, y), []) if t[0] in ("wire", "pin")]) + 2 * len(
                    [w for w in self.on_wire(x, y) if key(x, y) not in map(lambda p: key(*p), self.wire_ends(w))])
                if n < 3:
                    self.remove(j)
                    changed = True
            if not changed:
                return

    def is_power(self, s):
        lib = self.libs.get(find(s, "lib_id")[1])
        return lib is not None and find(lib, "power") is not None

    def labels_at_pin(self, ref, pin):
        """the labels on the net segment from a pin: follows wires (not through junctions to other pins)"""
        sym = self.by_ref(ref)[0]
        start = self.pin_points(sym)[pin]
        seen, todo, found, wires = set(), [start], [], []
        pts = self.points()
        while todo:
            p = todo.pop()
            if key(*p) in seen:
                continue
            seen.add(key(*p))
            for t in pts.get(key(*p), []):
                if t[0] in ("label", "global_label", "hierarchical_label"):
                    found.append(t[1])
                if t[0] == "wire":
                    wires.append(t[1])
                    for q in self.wire_ends(t[1]):
                        todo.append(q)
        return found, wires, start

    def set_label(self, label, text, kind=None, shape="bidirectional"):
        """rename a label; kind turns it into another kind of label at the same place"""
        label[1] = text
        if kind and label[0] != kind:
            label[0] = Sym(kind)
            if kind == "label":
                label[:] = [x for x in label if not (isinstance(x, list) and x and x[0] == "shape")]
            elif not find(label, "shape"):
                label.insert(2, S("shape", Sym(shape)))
            if kind == "global_label" and not find(label, "fields_autoplaced"):
                label.append(S("fields_autoplaced"))

    def swap_power(self, old_value, new_lib_id):
        for s in self.symbols():
            if self.is_power(s) and self.value(s) == old_value:
                find(s, "lib_id")[1] = new_lib_id
                for p in findall(s, "property"):
                    if p[1] == "Value":
                        p[2] = new_lib_id.split(":")[-1]

    @staticmethod
    def value(s):
        for p in findall(s, "property"):
            if p[1] == "Value":
                return p[2]

    def ensure_lib(self, lib_id, sym):
        if lib_id not in self.libs:
            sym = parse(dump(sym))
            sym[1] = lib_id
            find(self.t, "lib_symbols").append(sym)
            self.libs[lib_id] = sym

    def add(self, e):
        # before sheet_instances, if any
        si = find(self.t, "sheet_instances")
        if si:
            self.t.insert(self.t.index(si), e)
        else:
            self.t.append(e)

    def wire(self, x1, y1, x2, y2):
        self.add(S("wire", S("pts", S("xy", r4(x1), r4(y1)), S("xy", r4(x2), r4(y2))), legacy.stroke(), S("uuid", uid(self.path, "w", x1, y1, x2, y2))))

    def junction(self, x, y):
        self.add(S("junction", S("at", r4(x), r4(y)), S("diameter", 0), S("color", 0, 0, 0, 0), S("uuid", uid(self.path, "j", x, y))))

    def no_connect(self, x, y):
        self.add(S("no_connect", S("at", r4(x), r4(y)), S("uuid", uid(self.path, "nc", x, y))))

    def label(self, text, x, y, angle=0, kind="label", shape="bidirectional", size=1.27):
        just = {0: ["left"], 180: ["right"], 90: ["left"], 270: ["right"]}[angle]
        e = [Sym(kind), text]
        if kind != "label":
            e.append(S("shape", Sym(shape)))
        e += [S("at", r4(x), r4(y), angle)]
        if kind == "global_label":
            e.append(S("fields_autoplaced"))
        e += [legacy.eff(size / legacy.MIL, justify=just + (["bottom"] if kind == "label" else []))]
        e.append(S("uuid", uid(self.path, kind, text, x, y)))
        self.add(e)

    def text(self, txt, x, y, size=1.5):
        self.add(S("text", txt, S("at", r4(x), r4(y), 0), legacy.eff(size / legacy.MIL, justify=["left", "bottom"]), S("uuid", uid(self.path, "text", txt))))

    def place(self, lib_id, lib_sym, ref, value, x, y, inst_paths, angle=0, mirror=None, footprint="", fields=None, unit=1, hide_ref=False):
        """A symbol at (x, y); its Reference above and Value below it."""
        self.ensure_lib(lib_id, lib_sym)
        u = uid(self.path, "sym", ref, x, y)
        lib = self.libs[lib_id]
        is_pwr = find(lib, "power") is not None
        e = [Sym("symbol"), S("lib_id", lib_id), S("at", r4(x), r4(y), angle)]
        if mirror:
            e.append(S("mirror", Sym(mirror)))
        e += [S("unit", unit), S("in_bom", Sym("no" if is_pwr else "yes")), S("on_board", Sym("no" if is_pwr else "yes")),
              S("dnp", Sym("no")), S("uuid", u)]
        pins = self.pin_points_lib(lib, unit)
        ys = [p[1] for p in pins.values()] or [0]
        top, bot = y - max(ys) - 1.8, y - min(ys) + 1.8
        e.append(S("property", "Reference", ref, S("at", r4(x), r4(top), 0), legacy.eff(50, hide=hide_ref or is_pwr)))
        e.append(S("property", "Value", value, S("at", r4(x), r4(bot if not is_pwr else y - 3.2), 0), legacy.eff(50)))
        e.append(S("property", "Footprint", footprint, S("at", r4(x), r4(y), 0), legacy.eff(50, hide=True)))
        e.append(S("property", "Datasheet", "", S("at", r4(x), r4(y), 0), legacy.eff(50, hide=True)))
        for k, v in (fields or {}).items():
            e.append(S("property", k, v, S("at", r4(x), r4(y), 0), legacy.eff(50, hide=True)))
        for n in pins:
            e.append(S("pin", n, S("uuid", uid(u, "pin", n))))
        e.append(S("instances", [Sym("project"), self.project] + [S("path", p, S("reference", ref), S("unit", unit)) for p in inst_paths]))
        self.add(e)
        return e

    @staticmethod
    def pin_points_lib(lib, unit=1):
        out = {}
        for u in findall(lib, "symbol"):
            un = int(u[1].rsplit("_", 2)[-2])
            if un not in (0, unit):
                continue
            for p in findall(u, "pin"):
                pa = find(p, "at")
                out[find(p, "number")[1]] = (float(pa[1]), float(pa[2]), int(float(pa[3])))
        return out
