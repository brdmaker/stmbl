"""Generate the brdstepper KiCad 7 project from parts.py:

  brdstepper.kicad_sym   the project's own symbols (DRV8876RGT, TPS560430, AS5047P)
  sym-lib-table          that library, as "brdstepper"
  brdstepper.kicad_pro   the project
  brdstepper.kicad_sch   the root sheet, with a sheet per part of the design
  <sheet>.kicad_sch      power, mcu, bridges, encoder, interfaces

Every pin is joined to its net by a short wire and a global label of the net's
name; a pin with no net gets a no-connect flag. Parts are laid out in rows, so
the sheets read as a netlist, not as a drawn schematic. UUIDs are derived from
names, so running it again changes only what changed.

  python3 gen.py [OUTDIR]     (default: the directory above this one)
"""
import os
import sys
import uuid

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from sexp import Sym, parse, dump, find, findall  # noqa: E402
import parts  # noqa: E402

KICAD_SYMBOLS = os.environ.get("KICAD7_SYMBOL_DIR", "/usr/share/kicad/symbols")
PROJECT = "brdstepper"
NS = uuid.UUID("6f1c2b0e-55a1-4f0e-9d0b-6272647374ff")
GRID = 1.27
STUB = 2.54


def uid(*key):
    return str(uuid.uuid5(NS, "/".join(key)))


def snap(v):
    return round(round(v / GRID) * GRID, 4)


def S(*a):
    return [Sym(a[0])] + list(a[1:])


def font(hide=False, justify=None):
    e = [Sym("effects"), [Sym("font"), [Sym("size"), 1.27, 1.27]]]
    if justify:
        e.append([Sym("justify")] + [Sym(j) for j in justify.split()])
    if hide:
        e.append(Sym("hide"))
    return e


# ------------------------------------------------------------------ the project's own symbols
def pin(kind, x, y, ang, name, num, length=2.54):
    return [Sym("pin"), Sym(kind), Sym("line"), [Sym("at"), x, y, ang], [Sym("length"), length],
            [Sym("name"), name, font()], [Sym("number"), str(num), font()]]


def box_symbol(name, value, fp, ds, desc, left, right, w=15.24):
    """A rectangle with pins down its left and right: [(num, name, type)], None for a gap."""
    n = max(len(left), len(right))
    h = (n + 1) * 2.54
    top = snap(h / 2)
    x0 = snap(w / 2)
    body = [Sym("symbol"), name + "_0_1",
            [Sym("rectangle"), [Sym("start"), -x0, top], [Sym("end"), x0, top - h],
             [Sym("stroke"), [Sym("width"), 0.254], [Sym("type"), Sym("default")]], [Sym("fill"), [Sym("type"), Sym("background")]]]]
    pins = [Sym("symbol"), name + "_1_1"]
    for side, lst in ((-1, left), (1, right)):
        for i, p in enumerate(lst):
            if p is None:
                continue
            num, pname, kind = p
            y = round(top - 2.54 * (i + 1), 4)
            pins.append(pin(kind, side * (x0 + 2.54), y, 0 if side < 0 else 180, pname, num))
    return [Sym("symbol"), name, [Sym("in_bom"), True], [Sym("on_board"), True],
            [Sym("property"), "Reference", "U", [Sym("at"), -x0, top + 1.27, 0], font(justify="left")],
            [Sym("property"), "Value", value, [Sym("at"), 0, top - h - 1.27, 0], font()],
            [Sym("property"), "Footprint", fp, [Sym("at"), 0, top - h - 3.81, 0], font(hide=True)],
            [Sym("property"), "Datasheet", ds, [Sym("at"), 0, top - h - 6.35, 0], font(hide=True)],
            [Sym("property"), "ki_description", desc, [Sym("at"), 0, 0, 0], font(hide=True)],
            body, pins]


def own_symbols():
    drv = box_symbol(
        "DRV8876RGT", "DRV8876RGTR", "Package_DFN_QFN:VQFN-16-1EP_3x3mm_P0.5mm_EP1.68x1.68mm_ThermalVias",
        "https://www.ti.com/lit/ds/symlink/drv8876.pdf",
        "DRV8876 brushed DC / H-bridge, 37 V 3.5 A, current mirror (IPROPI), VQFN-16 (RGT). Pinout per SLVSDS7B.",
        [("15", "EN/IN1", "input"), ("16", "PH/IN2", "input"), ("1", "nSLEEP", "input"), ("14", "PMODE", "input"),
         ("5", "IMODE", "input"), ("3", "VREF", "input"), ("4", "IPROPI", "output"), ("2", "nFAULT", "open_collector"),
         None, ("13", "GND", "power_in"), ("17", "PAD", "power_in")],
        [("9", "VM", "power_in"), ("10", "VCP", "passive"), ("11", "CPH", "passive"), ("12", "CPL", "passive"), None,
         ("6", "OUT1", "output"), ("8", "OUT2", "output"), None, None, None, ("7", "PGND", "power_in")], w=17.78)
    buck = box_symbol(
        "TPS560430", "TPS560430XDBVR", "Package_TO_SOT_SMD:SOT-23-6",
        "https://www.ti.com/lit/ds/symlink/tps560430.pdf",
        "TPS560430 4-36 V 600 mA synchronous buck, SOT-23-6 (DBV). VREF 1.0 V.",
        [("5", "VIN", "power_in"), ("4", "EN", "input"), None, ("2", "GND", "power_in")],
        [("1", "CB", "passive"), ("6", "SW", "output"), None, ("3", "FB", "input")])
    # the AS5047P is pin-compatible with KiCad's AS5047D; its VDD3V3 is an
    # output only in 5 V mode, here it is tied to the 3V3 rail (an input)
    lib = parse(open(os.path.join(KICAD_SYMBOLS, "Sensor_Magnetic.kicad_sym")).read())
    enc = rename(next(s for s in findall(lib, "symbol") if s[1] == "AS5047D"), "AS5047D", "AS5047P")
    for prop in findall(enc, "property"):
        if prop[1] == "Value":
            prop[2] = "AS5047P"
        elif prop[1] == "Datasheet":
            prop[2] = "https://ams-osram.com/products/sensor-solutions/position-sensors/ams-as5047p-high-speed-rotary-position-sensor"
        elif prop[1] == "ki_description":
            prop[2] = "AS5047P 14-bit magnetic rotary position sensor, SPI + ABI/UVW, TSSOP-14 (pinout as the AS5047D ✱)"
    for u in findall(enc, "symbol"):
        for p in findall(u, "pin"):
            if find(p, "number")[1] == "12":
                p[1] = Sym("power_in")
    return [drv, buck, enc]


def rename(sym, old, new):
    sym = parse(dump(sym))
    sym[1] = new
    for u in findall(sym, "symbol"):
        if u[1].startswith(old + "_"):
            u[1] = new + u[1][len(old):]
    return sym


# ------------------------------------------------------------------ library symbols, flattened
_libs = {}


def lib_file(name):
    if name not in _libs:
        path = os.path.join(OUT, PROJECT + ".kicad_sym") if name == PROJECT else os.path.join(KICAD_SYMBOLS, name + ".kicad_sym")
        _libs[name] = {s[1]: s for s in findall(parse(open(path).read()), "symbol")}
    return _libs[name]


def flat_symbol(lib_id):
    """The symbol as a schematic's lib_symbols carries it: extends resolved, named Lib:Name."""
    libname, name = lib_id.split(":")
    syms = lib_file(libname)
    s = syms[name]
    ext = find(s, "extends")
    if ext:
        base = flat_symbol(libname + ":" + ext[1])
        base = rename(base, ext[1], name)
        props = {p[1]: p for p in findall(s, "property")}
        out = []
        for e in base:
            if isinstance(e, list) and e and e[0] == "property" and e[1] in props:
                out.append(props.pop(e[1]))
            else:
                out.append(e)
        # child-only properties go after the parent's
        i = max(i for i, e in enumerate(out) if isinstance(e, list) and e and e[0] == "property")
        out[i + 1:i + 1] = list(props.values())
        for k in ("in_bom", "on_board"):
            if find(s, k) and find(out, k):
                out[out.index(find(out, k))] = find(s, k)
        s = out
    else:
        s = parse(dump(s))
    s[1] = name
    return s


def lib_symbol(lib_id):
    s = flat_symbol(lib_id)
    s[1] = lib_id
    return s


def sym_pins(s):
    """[(number, name, type, x, y, angle, hidden)] of unit 1 (and the common unit), style 1."""
    out = []
    for u in findall(s, "symbol"):
        unit, style = u[1].rsplit("_", 2)[-2:]
        if unit not in ("0", "1") or style not in ("0", "1"):
            continue
        for p in findall(u, "pin"):
            at = find(p, "at")
            out.append((find(p, "number")[1], find(p, "name")[1], str(p[1]), float(at[1]), float(at[2]),
                        float(at[3]) if len(at) > 3 else 0.0, Sym("hide") in p))
    return out


# ------------------------------------------------------------------ a sheet
LABEL_ANGLE = {0: 180, 180: 0, 90: 270, 270: 90}   # pin angle -> label angle, the label facing away


def stub_dir(a):
    return {0: (-1, 0), 180: (1, 0), 90: (0, 1), 270: (0, -1)}[int(a) % 360]


def label_len(net):
    return 1.0 * len(net) + 3.0


def part_extent(pins, nets):
    """Bounding box relative to the symbol's origin, labels included."""
    xs, ys = [-3.0, 3.0], [-3.0, 3.0]
    for num, _, _, px, py, a, _ in pins:
        x, y = px, -py
        dx, dy = stub_dir(a)
        ln = STUB + (label_len(nets[num]) if num in nets else 1.5)
        xs += [x, x + dx * ln]
        ys += [y, y + dy * ln]
        if dy:  # a vertical label is as wide as its text height
            xs += [x - 2, x + 2]
    return min(xs), min(ys), max(xs), max(ys)


def layout(items, width):
    """Rows of boxes: [(key, (x0, y0, x1, y1))] -> {key: (X, Y)} origin positions, and the height used."""
    pos, x, y, row_h = {}, 0.0, 0.0, 0.0
    for key, (x0, y0, x1, y1) in items:
        w, h = x1 - x0, y1 - y0
        if x > 0 and x + w > width:
            x, y, row_h = 0.0, y + row_h + 7.62, 0.0
        pos[key] = (snap(x - x0), snap(y - y0))
        x += w + 7.62
        row_h = max(row_h, h)
    return pos, y + row_h


PAPER = [("A4", 297, 210), ("A3", 420, 297), ("A2", 594, 420), ("A1", 841, 594)]


def sheet(name, plist, root_uuid, sheet_uuid, page, extra=()):
    used = {}
    items = []
    for p in list(plist) + list(extra):
        ref, lib_id, value, fp, nets, note = p
        if lib_id not in used:
            used[lib_id] = lib_symbol(lib_id)
        pins = sym_pins(used[lib_id])
        nums = {q[0] for q in pins}
        bad = set(nets) - nums
        if bad:
            raise SystemExit("%s (%s): no pin %s" % (ref, lib_id, ", ".join(sorted(bad))))
        items.append((ref, part_extent(pins, nets), p, pins))
    for paper, w, h in PAPER:
        pos, used_h = layout([(i[0], i[1]) for i in items], w - 50)
        if used_h < h - 60:
            break
    path = "/%s/%s" % (root_uuid, sheet_uuid)
    body = []
    for ref, _, p, pins in items:
        _, lib_id, value, fp, nets, note = p
        X, Y = pos[ref]
        X, Y = X + 25.4, Y + 25.4
        lsym = used[lib_id]
        s = [Sym("symbol"), [Sym("lib_id"), lib_id], [Sym("at"), X, Y, 0], [Sym("unit"), 1],
             [Sym("in_bom"), not lib_id.startswith("power:")], [Sym("on_board"), not lib_id.startswith("power:")],
             [Sym("dnp"), False], [Sym("uuid"), uid(name, ref)]]
        vals = {"Reference": ref, "Value": value, "Footprint": fp}
        for lp in findall(lsym, "property"):
            if lp[1].startswith("ki_"):
                continue
            at = find(lp, "at")
            q = [Sym("property"), lp[1], vals.get(lp[1], lp[2]),
                 [Sym("at"), round(X + float(at[1]), 4), round(Y - float(at[2]), 4), float(at[3]) if len(at) > 3 else 0]]
            eff = find(lp, "effects")
            q.append(eff if eff else font())
            s.append(q)
        if note:
            s.append([Sym("property"), "Note", note.replace("✱", "*"), [Sym("at"), X, Y, 0], font(hide=True)])
        for num, *_ in pins:
            s.append([Sym("pin"), num, [Sym("uuid"), uid(name, ref, "pin", num)]])
        s.append([Sym("instances"), [Sym("project"), PROJECT,
                                     [Sym("path"), path if not ref.startswith("#") else path, [Sym("reference"), ref], [Sym("unit"), 1]]]])
        body.append(s)
        for num, pname, kind, px, py, a, hidden in pins:
            x, y = round(X + px, 4), round(Y - py, 4)
            if num not in nets:
                if kind != "no_connect":
                    body.append([Sym("no_connect"), [Sym("at"), x, y], [Sym("uuid"), uid(name, ref, "nc", num)]])
                continue
            dx, dy = stub_dir(a)
            ex, ey = round(x + dx * STUB, 4), round(y + dy * STUB, 4)
            body.append([Sym("wire"), [Sym("pts"), [Sym("xy"), x, y], [Sym("xy"), ex, ey]],
                         [Sym("stroke"), [Sym("width"), 0], [Sym("type"), Sym("default")]], [Sym("uuid"), uid(name, ref, "w", num)]])
            la = LABEL_ANGLE[int(a) % 360]
            body.append([Sym("global_label"), nets[num], [Sym("shape"), Sym("passive")], [Sym("at"), ex, ey, la],
                         [Sym("fields_autoplaced")], font(justify="right" if la in (180, 270) else "left"),
                         [Sym("uuid"), uid(name, ref, "l", num)],
                         [Sym("property"), "Intersheetrefs", "${INTERSHEET_REFS}", [Sym("at"), ex, ey, 0], font(hide=True)]])
    doc = [Sym("kicad_sch"), [Sym("version"), 20230121], [Sym("generator"), Sym("eeschema")],
           [Sym("uuid"), sheet_uuid if name != "root" else root_uuid], [Sym("paper"), paper],
           title_block(parts.TITLES.get(name, name), page),
           [Sym("lib_symbols")] + list(used.values())] + body
    return doc


def title_block(title, page):
    return [Sym("title_block"), [Sym("title"), "brdstepper: " + title], [Sym("date"), DATE], [Sym("rev"), REV],
            [Sym("company"), "stmbl"],
            [Sym("comment"), 1, "Generated by gen/gen.py from docs/brdstepper_hw.md: edit gen/parts.py"],
            [Sym("comment"), 2, "Parts with * in their Note: check against the datasheet before layout"]]


def root(root_uuid, sheet_ids):
    body = []
    x, y = 25.4, 38.1
    for i, (name, su) in enumerate(sheet_ids):
        w, h = 60.96, 20.32
        body.append([Sym("sheet"), [Sym("at"), x, y], [Sym("size"), w, h], [Sym("fields_autoplaced")],
                     [Sym("stroke"), [Sym("width"), 0.1524], [Sym("type"), Sym("solid")]],
                     [Sym("fill"), [Sym("color"), 0, 0, 0, 0.0]], [Sym("uuid"), su],
                     [Sym("property"), "Sheetname", name, [Sym("at"), x, round(y - 0.7116, 4), 0], font(justify="left bottom")],
                     [Sym("property"), "Sheetfile", name + ".kicad_sch", [Sym("at"), x, round(y + h + 0.5846, 4), 0], font(justify="left top")],
                     [Sym("instances"), [Sym("project"), PROJECT, [Sym("path"), "/" + root_uuid, [Sym("page"), str(i + 2)]]]]])
        body.append([Sym("text"), parts.TITLES[name], [Sym("at"), x + w + 5.08, y + h / 2, 0],
                     font(justify="left"), [Sym("uuid"), uid("root", "text", name)]])
        y += h + 12.7
    body.append([Sym("text"), NOTE, [Sym("at"), 25.4, y + 5.08, 0], font(justify="left top"), [Sym("uuid"), uid("root", "note")]])
    return [Sym("kicad_sch"), [Sym("version"), 20230121], [Sym("generator"), Sym("eeschema")], [Sym("uuid"), root_uuid],
            [Sym("paper"), "A4"], title_block("closed-loop NEMA11 stepper drive", 1), [Sym("lib_symbols")]] + body + \
           [[Sym("sheet_instances"), [Sym("path"), "/", [Sym("page"), "1"]]]]


NOTE = ("brdstepper: STM32G473 + AS5047P + 2x DRV8876, 28 x 28 mm on the back of a NEMA11 motor.\n"
        "Connections are by net name (global labels): docs/brdstepper_hw.md is the reference.\n"
        "Variant A fits U8/J4 (smart-serial to a Mesa card), variant B U9/J5/J6/JP1 (CAN).")


def pwr_flags():
    return [("#FLG%02d" % (i + 1), "power:PWR_FLAG", "PWR_FLAG", "", {"1": n}, "") for i, n in enumerate(parts.PWR_FLAGS)]


def project_json(sheet_ids, root_uuid):
    import json
    return json.dumps({
        "meta": {"filename": PROJECT + ".kicad_pro", "version": 1},
        "board": {"design_settings": {"defaults": {}, "rules": {}}},
        "schematic": {"legacy_lib_dir": "", "legacy_lib_list": []},
        "sheets": [[root_uuid, ""]] + [[su, n] for n, su in sheet_ids],
        "text_variables": {},
        "boards": [], "libraries": {"pinned_footprint_libs": [], "pinned_symbol_libs": []},
        "net_settings": {"classes": [
            {"name": "Default", "clearance": 0.15, "track_width": 0.2, "via_diameter": 0.6, "via_drill": 0.3,
             "diff_pair_width": 0.2, "diff_pair_gap": 0.15, "wire_width": 6, "bus_width": 12, "line_style": 0,
             "microvia_diameter": 0.3, "microvia_drill": 0.1, "pcb_color": "rgba(0, 0, 0, 0.000)",
             "schematic_color": "rgba(0, 0, 0, 0.000)"},
            {"name": "Power", "clearance": 0.2, "track_width": 0.5, "via_diameter": 0.8, "via_drill": 0.4,
             "diff_pair_width": 0.2, "diff_pair_gap": 0.15, "wire_width": 6, "bus_width": 12, "line_style": 0,
             "microvia_diameter": 0.3, "microvia_drill": 0.1, "pcb_color": "rgba(0, 0, 0, 0.000)",
             "schematic_color": "rgba(0, 0, 0, 0.000)"}],
            "meta": {"version": 3},
            "netclass_patterns": [{"netclass": "Power", "pattern": p} for p in
                                  ("VM", "GND", "COIL_*", "5V*", "BUCK_SW")]},
    }, indent=2) + "\n"


DATE = "2026-10-23"
REV = "0.1"
OUT = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.dirname(HERE)


def write(name, text):
    with open(os.path.join(OUT, name), "w") as f:
        f.write(text)


def main():
    os.makedirs(OUT, exist_ok=True)
    write(PROJECT + ".kicad_sym", dump([Sym("kicad_symbol_lib"), [Sym("version"), 20220914],
                                        [Sym("generator"), Sym("brdstepper_gen")]] + own_symbols()) + "\n")
    write("sym-lib-table", '(sym_lib_table\n  (lib (name "%s")(type "KiCad")(uri "${KIPRJMOD}/%s.kicad_sym")(options "")(descr "brdstepper parts"))\n)\n'
          % (PROJECT, PROJECT))
    root_uuid = uid("root")
    sheet_ids = [(n, uid("sheet", n)) for n in parts.SHEETS]
    for i, (n, su) in enumerate(sheet_ids):
        extra = pwr_flags() if n == "power" else ()
        write(n + ".kicad_sch", dump(sheet(n, parts.SHEETS[n], root_uuid, su, i + 2, extra)) + "\n")
    write(PROJECT + ".kicad_sch", dump(root(root_uuid, sheet_ids)) + "\n")
    write(PROJECT + ".kicad_pro", project_json(sheet_ids, root_uuid))
    print("wrote", OUT)


if __name__ == "__main__":
    main()
