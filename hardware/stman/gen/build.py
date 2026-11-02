#!/usr/bin/env python3
"""stman's schematic: the stmbl v4.0 LV sheets, converted with their drawing
kept, edited for stman, and three new sheets (docs/stman.md).

  python3 gen/build.py          writes hardware/stman/*.kicad_sch, .kicad_pro, sym-lib-table

1. **Convert:** hw/kicad/v4.0's "ctrl" hierarchy (LV board: f4, supply, CMD,
   FB0/FB1, IO) with hardware/kigen/legacy.py. ctrl.sch becomes the root sheet.
2. **Edit in place (EDITS):** remove the parts stman doesn't have, with the
   wires they leave dangling; give the MCU pins stman uses their new nets
   (global labels where stmbl's labels were); isolate the CMD port.
3. **Add sheets:** dac (AD5761R outputs, ±15 V), ain (AD7606 monitor ADC),
   drvio (24 V outputs and inputs, watchdog, drive connectors).

Refs from 200 up are stman's; the others are stmbl v4.0's.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.dirname(HERE)
REPO = os.path.dirname(os.path.dirname(OUT))
sys.path.insert(0, os.path.join(REPO, "hardware", "kigen"))
import legacy  # noqa: E402
from schedit import Sheet, S  # noqa: E402
from sexp import Sym, parse, dump, find, findall  # noqa: E402

PROJECT = "stman"
SRC = os.path.join(REPO, "hw", "kicad", "v4.0")
KICAD_SYMBOLS = "/usr/share/kicad/symbols"
G = 2.54

# ------------------------------------------------------------------ what goes
REMOVE = {
    "stman": ["P14"],                                   # the link to the HV board
    "cmd": ["U10", "U11", "R68", "R69", "R70", "R71", "R72", "R73", "C55", "C56", "R103", "R104", "P15", "R17", "R18"],
    "io": ["Q1", "Q2", "Q3", "Q4", "R9", "R10", "R11", "R12", "R13", "R14", "R40", "R45", "D2", "D3", "D5", "D7", "P5", "P9", "P10",
           "D4", "D14", "D15", "R20", "R22", "R74", "R119", "R106", "R108", "R105", "R98", "R99", "C52", "C59", "P4", "C98", "JP3",
           "D19", "D20", "R120", "R134", "C100", "C130"],
    "f4": ["U22"],                                      # ESD on the CMD lines: behind the isolator now
}

# STM32F405VG (U26) pins and their stman nets (docs/stman.md, MCU pins); global labels
MCU = {
    "23": "SS_TX", "69": "SS_RX", "93": "SS_TXE",
    "89": "SPI_SCK", "90": "SPI_MISO", "91": "SPI_MOSI",
    "92": "DAC1_SYNC", "96": "DAC2_SYNC", "88": "DAC_RESET", "77": "DAC_ALERT",
    "37": "ADC_CS", "95": "ADC_CONVST", "67": "ADC_BUSY", "68": "ADC_RESET",
    "87": "WD_WDI", "1": "WD_OK",
    "2": "OUT1_MCU", "3": "OUT2_MCU", "4": "OUT3_MCU", "5": "OUT4_MCU", "98": "OUT5_MCU", "97": "OUT6_MCU",
    "25": "OUT7_MCU", "26": "OUT8_MCU",
    "81": "ST_A12", "82": "ST_A34", "8": "ST_B12", "9": "ST_B34",
    "33": "IN1_MCU", "34": "IN2_MCU", "35": "IN3_MCU", "46": "IN4_MCU", "66": "IN5_MCU", "83": "IN6_MCU",
    "15": "IN7_MCU", "16": "IN8_MCU",
}


# ------------------------------------------------------------------ symbols
def kicad_sym(lib, name):
    """a KiCad library symbol, its 'extends' resolved, named lib:name"""
    import importlib.util
    spec = importlib.util.spec_from_file_location("ksch", os.path.join(REPO, "hardware", "kigen", "sch.py"))
    ksch = importlib.util.module_from_spec(spec)
    sys.path.insert(0, os.path.join(REPO, "hardware", "kigen"))
    spec.loader.exec_module(ksch)
    s = ksch.flat_symbol(lib + ":" + name)
    s[1] = lib + ":" + name
    return s


def power_sym(name, base):
    """a power symbol named name: stmbl's base symbol (GND or +12V) with its pin and value renamed"""
    s = parse(dump(base))
    s[1] = "stman:" + name
    for p in findall(s, "property"):
        if p[1] == "Value":
            p[2] = name
    for u in findall(s, "symbol"):
        u[1] = u[1].replace(base[1].split(":")[-1], name)
        for p in findall(u, "pin"):
            find(p, "name")[1] = name
    return s


def box(name, value, pins_left, pins_right, w=15.24, fp=""):
    """a rectangle with pins (num, name, type) down its left and right (None: a gap)"""
    n = max(len(pins_left), len(pins_right))
    h = (n + 1) * G
    top = round(h / 2 / 1.27) * 1.27
    x0 = round(w / 2 / 1.27) * 1.27
    body = [Sym("symbol"), name + "_0_1",
            S("rectangle", S("start", -x0, top), S("end", x0, round(top - h, 4)),
              S("stroke", S("width", 0.254), S("type", Sym("default"))), S("fill", S("type", Sym("background"))))]
    pins = [Sym("symbol"), name + "_1_1"]
    for side, lst in ((-1, pins_left), (1, pins_right)):
        for i, p in enumerate(lst):
            if p is None:
                continue
            num, pname, kind = p
            y = round(top - G * (i + 1), 4)
            pins.append(S("pin", Sym(kind), Sym("line"), S("at", side * (x0 + G), y, 0 if side < 0 else 180), S("length", G),
                          S("name", pname, legacy.eff(50)), S("number", str(num), legacy.eff(50))))
    return [Sym("symbol"), "stman:" + name, S("in_bom", Sym("yes")), S("on_board", Sym("yes")),
            S("property", "Reference", "U", S("at", 0, top + 1.27, 0), legacy.eff(50)),
            S("property", "Value", value, S("at", 0, round(top - h - 1.27, 4), 0), legacy.eff(50)),
            S("property", "Footprint", fp, S("at", 0, 0, 0), legacy.eff(50, hide=True)),
            S("property", "Datasheet", "", S("at", 0, 0, 0), legacy.eff(50, hide=True)), body, pins]


AD5761R = box("AD5761R", "AD5761RBRUZ",
              [("7", "DVCC", "power_in"), ("8", "VLOGIC", "power_in"), ("1", "~{SYNC}", "input"), ("2", "SCLK", "input"),
               ("3", "SDI", "input"), ("4", "~{LDAC}", "input"), ("5", "~{CLEAR}", "input"), ("6", "~{RESET}", "input"),
               ("14", "DGND", "power_in")],
              [("9", "AVDD", "power_in"), ("10", "AVSS", "power_in"), ("11", "VOUT", "output"), ("13", "VREF", "passive"),
               ("15", "SDO", "output"), ("16", "~{ALERT}", "open_collector"), None, None, ("12", "AGND", "power_in")],
              w=17.78, fp="Package_SO:TSSOP-16_4.4x5mm_P0.65mm")
OPA2197 = box("OPA2197", "OPA2197IDR",
              [("3", "+INA", "input"), ("2", "-INA", "input"), ("5", "+INB", "input"), ("6", "-INB", "input"), ("8", "V+", "power_in"),
               ("4", "V-", "power_in")],
              [("1", "OUTA", "output"), ("7", "OUTB", "output")], fp="Package_SO:SOIC-8_3.9x4.9mm_P1.27mm")
AND2 = box("74LVC2G08", "74LVC2G08",
           [("1", "1A", "input"), ("2", "1B", "input"), ("5", "2A", "input"), ("6", "2B", "input"), ("8", "VCC", "power_in"),
            ("4", "GND", "power_in")],
           [("7", "1Y", "output"), ("3", "2Y", "output")], fp="Package_TO_SOT_SMD:SOT-23-8")


# ------------------------------------------------------------------ 1. convert
def convert():
    legacy.ROOT_LPATH["ctrl.sch"] = "/56590966"
    legacy.COMMON_UNIT["stmbl:LM358"] = 2
    out, libs = legacy.convert(SRC, "ctrl.sch", "stmbl_4.0-cache.lib", OUT, PROJECT)
    legacy.write(out, OUT, "stman: stmbl with an analog output")
    return libs


def sheet(name):
    return Sheet(os.path.join(OUT, name + ".kicad_sch"), PROJECT)


def root_paths():
    """KiCad instance paths: the root's and each sheet's, from the converted root"""
    root = sheet(PROJECT)
    ru = find(root.t, "uuid")[1]
    paths = {"stman": "/" + ru}
    for sh in root.items("sheet"):
        nm = [p[2] for p in findall(sh, "property") if p[1] == "Sheetname"][0]
        paths[nm] = "/" + ru + "/" + find(sh, "uuid")[1]
    return paths


# ------------------------------------------------------------------ 2. edits
def relabel_pin(sh, ref, pin, net):
    """The pin's net becomes `net` (a global label): stmbl's label(s) on the pin's stub are renamed (the first)
    or removed; a pin wired to anything else (a power symbol, other pins) is cut loose and labelled at its end."""
    labels, wires, start = sh.labels_at_pin(ref, pin)
    pts = sh.points()
    # the stub: the wires from the pin up to a junction or another pin
    others = [t for w in wires for p in sh.wire_ends(w) for t in pts.get((round(p[0], 2), round(p[1], 2)), [])
              if t[0] == "pin" and not (sh.refs(t[1]) and ref in sh.refs(t[1]) and t[2] == pin)]
    if labels and not others and len(wires) <= 3:
        sh.set_label(labels[0], net, kind="global_label")
        for l in labels[1:]:
            sh.remove(l)
        return
    # cut: the wire touching the pin goes; label the pin's end
    for w in wires:
        if any(abs(p[0] - start[0]) < 1e-3 and abs(p[1] - start[1]) < 1e-3 for p in sh.wire_ends(w)):
            sh.remove(w)
    sym = sh.by_ref(ref)[0]
    lib = sh.libs[find(sym, "lib_id")[1]]
    ang = [a for n, (x, y, a) in Sheet.pin_points_lib(lib).items() if n == pin][0]
    # the pin points into the body at `ang`: the label faces away from it
    lab_ang = {0: 180, 180: 0, 90: 270, 270: 90}[ang]
    sh.label(net, start[0], start[1], {180: 180, 0: 0, 90: 90, 270: 270}[lab_ang], kind="global_label")


def edit_f4():
    sh = sheet("f4")
    sh.remove_ref(*REMOVE["f4"])
    sh.cleanup()
    for pin, net in MCU.items():
        relabel_pin(sh, "U26", pin, net)
    # the ESD part on stmbl's analog inputs: those are IN7/IN8 now; CAN is gone from U20
    for ref, pin, net in (("U23", "3", "IN8_MCU"), ("U23", "4", "IN7_MCU")):
        relabel_pin(sh, ref, pin, net)
    # stmbl tied the CMD port's lines to the IO header's SPI here (label pairs off the pins): both gone
    for l in [l for l in sh.items("hierarchical_label") if l[1] in ("CMD_12", "CMD_36", "CMD_45", "IO_SPI_CK", "IO_SPI_MISO", "IO_SPI_MOSI")]:
        sh.remove(l)
    for pin in ("3", "6"):
        labels, wires, start = sh.labels_at_pin("U20", pin)
        for l in labels:
            sh.remove(l)
        for w in wires:
            sh.remove(w)
        sh.no_connect(*start)
    sh.cleanup()
    sh.save()


def edit_cmd(libs):
    sh = sheet("cmd")
    sh.remove_ref(*REMOVE["cmd"])
    sh.cleanup()
    # the LED pins of the RJ45: unused
    j3 = sh.pin_points(sh.by_ref("J3")[0])
    for p in ("9", "10", "11", "12", "4", "5", "7", "8"):
        if not [t for t in sh.points().get((round(j3[p][0], 2), round(j3[p][1], 2)), []) if t[0] != "pin"]:
            if p in ("9", "10", "11", "12"):
                sh.no_connect(*j3[p])
    # the transceivers stay on the cable's side: their hierarchical labels become local nets to the isolator
    for old, new in (("D12", "SS_RXD"), ("E12", "SS_RXDE"), ("D36", "SS_TXD"), ("E36", "SS_TXDE")):
        for l in sh.items("hierarchical_label"):
            if l[1] == old:
                sh.set_label(l, new, kind="label")
    for l in [l for l in sh.items("hierarchical_label") if l[1] in ("D45", "E45", "D78", "E78", "L1", "L2")]:
        sh.remove(l)
    # U9 drives the cable: its receiver output off the TX data net
    ro = sh.pin_points(sh.by_ref("U9")[0])["1"]
    for w in list(sh.items("wire")):
        if any(abs(p[0] - ro[0]) < 1e-3 and abs(p[1] - ro[1]) < 1e-3 for p in sh.wire_ends(w)):
            sh.remove(w)
    sh.cleanup()
    sh.no_connect(*ro)
    # cable side supplies: GND -> SS_GND, the shield -> SS_SHIELD, +5F -> SS_5V
    for nm, base in (("SS_GND", "stmbl:GND"), ("SS_SHIELD", "stmbl:GNDD"), ("SS_5V", "stmbl:+5F")):
        sh.ensure_lib("stman:" + nm, power_sym(nm, libs[base]))
    sh.swap_power("GND", "stman:SS_GND")
    sh.swap_power("GNDD", "stman:SS_SHIELD")
    sh.swap_power("+5F", "stman:SS_5V")
    # the isolator, the cable's supply pairs, the hierarchical labels to the MCU
    p = root_paths()["CMD"]
    ctr = [0]
    x0, y0 = 233.68, 78.74
    iso = kicad_sym("Isolator", "ISO7331FC")
    u = sh.place("Isolator:ISO7331FC", iso, "U200", "ISO7331FC", x0, y0, [p], footprint="Package_SO:SOIC-16W_7.5x10.3mm_P1.27mm")
    pins = sh.pin_points(u)
    lib = sh.libs["Isolator:ISO7331FC"]
    names = {find(q, "number")[1]: find(q, "name")[1] for uu in findall(lib, "symbol") for q in findall(uu, "pin")}
    side1 = {"VCC1": ("+3.3V", None), "GND1": ("GND", None), "INA": ("SS_TX", "g"), "INB": ("SS_TXE", "g"), "OUTC": ("SS_RX", "g"), "EN1": ("+3.3V", None)}
    side2 = {"VCC2": ("SS_5V", None), "GND2": ("SS_GND", None), "OUTA": ("SS_TXD", "l"), "OUTB": ("SS_TXDE", "l"), "INC": ("SS_RXD", "l"),
             "EN2": ("SS_5V", None)}
    for num, (x, y) in pins.items():
        nm = names[num]
        want = side1.get(nm) or side2.get(nm)
        if not want:
            sh.no_connect(x, y)
            continue
        net, kind = want
        left = x < x0
        x2 = x - 2 * G if left else x + 2 * G
        sh.wire(x, y, x2, y)
        if kind == "g":
            sh.label(net, x2, y, 180 if left else 0, kind="global_label", shape="output" if nm.startswith("OUT") else "input")
        elif kind == "l":
            sh.label(net, x2, y, 180 if left else 0)
        else:
            pw = {"+3.3V": libs["stmbl:+3.3V"], "GND": libs["stmbl:GND"]}.get(net)
            lid = "stmbl:" + net if pw is not None else "stman:" + net
            down = "GND" in net
            sh.wire(x2, y, x2, y + (G if down else -G))
            ctr[0] += 1
            sh.place(lid, pw if pw is not None else sh.libs[lid], "#PWR9%02d" % ctr[0], net, x2, y + (G if down else -G), [p])
    sh.text("stman: the transceivers U8 (receive, pair 1/2) and U9 (send, pair 3/6) are on the cable's side,\n"
            "supplied from the cable (SS_5V, SS_GND); ISO7331FC U200 (outputs low while unpowered) is the only path to the MCU.\n"
            "U10, U11 (pairs 4/5, 7/8) removed: those pairs carry the cable's +5 V and GND (JP200, JP201: check against the Mesa card *)",
            25.4, 185.42, size=1.5)
    # J3 4/5 and 7/8: the cable's supply, by solder jumpers
    jp = kicad_sym("Jumper", "SolderJumper_3_Bridged12")
    for ref, x, y, a, b in (("JP200", 63.5, 147.32, "SS_P45", "SS_P78"), ("JP201", 101.6, 147.32, "SS_P78", "SS_P45")):
        j = sh.place("Jumper:SolderJumper_3_Bridged12", jp, ref, "%s | %s" % (a, b), x, y, [p],
                     footprint="Jumper:SolderJumper-3_P1.3mm_Bridged12_RoundedPad1.0x1.5mm")
        jpins = sh.pin_points(j)
        sh.wire(jpins["1"][0], jpins["1"][1], jpins["1"][0] - G, jpins["1"][1])
        sh.label(a, jpins["1"][0] - G, jpins["1"][1], 180)
        sh.wire(jpins["3"][0], jpins["3"][1], jpins["3"][0] + G, jpins["3"][1])
        sh.label(b, jpins["3"][0] + G, jpins["3"][1], 0)
        net = "SS_5V" if ref == "JP200" else "SS_GND"
        x2, y2 = jpins["2"]
        sh.wire(x2, y2, x2, y2 + G if net == "SS_GND" else y2 - G)
        ctr[0] += 1
        sh.place("stman:" + net, sh.libs["stman:" + net], "#PWR9%02d" % ctr[0], net, x2, y2 + G if net == "SS_GND" else y2 - G,
                 [p], angle=0)
    for pinno, net in (("4", "SS_P45"), ("5", "SS_P45"), ("7", "SS_P78"), ("8", "SS_P78")):
        x, y = j3[pinno]
        sh.label(net, x, y, 0)
    sh.cleanup()
    sh.save()


def edit_supply():
    """Reverse polarity: D200 in series from P3's +UI to the +24V rail, in place of the wire up from P3
    (the TVS D22, the LED and the bulk capacitor sit after it, on +24V)."""
    sh = sheet("supply")
    sh.remove_wire((27.94, 67.31), (27.94, 74.93))
    d = kicad_sym("Device", "D_Schottky")
    sh.place("Device:D_Schottky", d, "D200", "SS34A", 27.94, 71.12, [root_paths()["supply"]], angle=270,
             footprint="stmbl:SMA_Standard", fields={"Note": "stman: reverse polarity, +UI (P3) -> +24V"})
    sh.text("stman: D200 reverse polarity", 30.48, 64.77, size=1.27)
    regulators(sh)
    sh.save()


# the ACT4088 (end of life, 30 V) -> TI LMR51420YF (4.5-36 V, 2 A, 1.1 MHz, SOT-23-6, VFB 0.6 V; datasheet SLUSEF6C):
# stmbl's ACT4088 symbol with the LMR514x0's pin numbers, so stmbl's wiring stays as drawn
LMR_PINS = {"SW": ("2", "SW"), "IN": ("3", "VIN"), "EN": ("5", "EN"), "FB": ("4", "FB"), "GND": ("1", "GND"), "BST": ("6", "CB")}
LMR_VALUE = "LMR51420YFDDCR"   # YF: 1.1 MHz forced PWM (fixed frequency at light load, no PFM bursts near the analog band)
# synchronous: stmbl's catch diodes are not needed (left on the board, not fitted)
NOT_FITTED = ("D9", "D12", "D13")
# the datasheet asks for >= 4.7 uF ceramic at VIN, rated about twice the input; and 25 V output caps on 12 V
# (0603 kept: an 0805 does not fit stmbl's regulator corner; e.g. TDK C1608X5R1H475K, C143 22 uF is the bulk)
VALUES = {"C102": ("4.7µ 50V", None), "C133": ("4.7µ 50V", None), "C137": ("4.7µ 50V", None),
          "C110": ("10µ 25V", None), "C132": ("10µ 25V", None)}
# the feedback dividers for VFB 0.6 V: the top resistors stay, the bottom ones change
DIVIDERS = {"R35": "6.98k", "R59": "2.87k", "R132": "7.15k"}
NOTES = {"(51k + 3.9k + 10k) / 10k": "0.6V * (51k + 3.9k + 7.15k) / 7.15k = 5.21V   (stman: LMR51420, VFB 0.6 V *)",
         "(51k + 3.9k + 3.9k) / 3.9k": "0.6V * (51k + 3.9k + 2.87k) / 2.87k = 12.08V",
         "(51k + 1k + 10k) / 10k": "0.6V * (51k + 1k + 6.98k) / 6.98k = 5.07V"}


def lmr_symbol(act):
    s = parse(dump(act))
    s[1] = "stman:LMR514x0"
    for pr in findall(s, "property"):
        if pr[1] == "Value":
            pr[2] = LMR_VALUE
    for u in findall(s, "symbol"):
        u[1] = u[1].replace("ACT4088", "LMR514x0")
        for p in findall(u, "pin"):
            num, name = LMR_PINS[find(p, "name")[1]]
            find(p, "name")[1] = name
            find(p, "number")[1] = num
    return s


def regulators(sh):
    sh.ensure_lib("stman:LMR514x0", lmr_symbol(sh.libs["stmbl:ACT4088"]))
    for ref in ("U13", "U27", "U28"):
        sym = sh.by_ref(ref)[0]
        find(sym, "lib_id")[1] = "stman:LMR514x0"
        for pr in findall(sym, "property"):
            if pr[1] == "Value":
                pr[2] = LMR_VALUE
    for ref, v in DIVIDERS.items():
        for pr in findall(sh.by_ref(ref)[0], "property"):
            if pr[1] == "Value":
                pr[2] = v
    for ref, (v, fp) in VALUES.items():
        for pr in findall(sh.by_ref(ref)[0], "property"):
            if pr[1] == "Value":
                pr[2] = v
            if pr[1] == "Footprint" and fp:
                pr[2] = fp
    for ref in NOT_FITTED:
        find(sh.by_ref(ref)[0], "dnp")[1] = Sym("yes")
    for t in sh.items("text"):
        for k, v in NOTES.items():
            if k in t[1]:
                t[1] = v
    # EN: the ACT4088's floated (enabled); the LMR514x0's is pulled up to the input here (U28's is the MCU's EN_5V)
    path = root_paths()["supply"]
    rsym = kicad_sym("Device", "R")
    for i, ref in enumerate(("U13", "U27")):
        en = sh.pin_points(sh.by_ref(ref)[0])["5"]
        for nc in list(sh.items("no_connect")):
            at = find(nc, "at")
            if abs(float(at[1]) - en[0]) < 1e-3 and abs(float(at[2]) - en[1]) < 1e-3:
                sh.remove(nc)
        sh.wire(en[0], en[1], en[0] - 2 * G, en[1])
        sh.label("EN_" + ref, en[0] - 2 * G, en[1], 180)
        x, y = 30.48 + 12.7 * i, 175.26
        r = sh.place("Device:R", rsym, "R%d" % (212 + i), "100k", x, y, [path], footprint="stmbl:R_0603",
                     fields={"Note": "stman: %s enable pull-up (LMR514x0 EN)" % ref})
        p1, p2 = sh.pin_points(r)["1"], sh.pin_points(r)["2"]
        sh.wire(p1[0], p1[1], p1[0], p1[1] - G)
        sh.place("stmbl:+24V", sh.libs["stmbl:+24V"], "#PWR95%d" % i, "+24V", p1[0], p1[1] - G, [path])
        sh.wire(p2[0], p2[1], p2[0], p2[1] + G)
        sh.label("EN_" + ref, p2[0], p2[1] + G, 270)
    # U28's EN from the MCU (EN_5V, driven high by io4 unless fbsd): pulled down so it is defined (off) while the MCU resets
    r = sh.place("Device:R", rsym, "R214", "100k", 30.48 + 12.7 * 2, 175.26, [path], footprint="stmbl:R_0603",
                 fields={"Note": "stman: U28 EN pull-down (LMR514x0 EN must not float)"})
    p1, p2 = sh.pin_points(r)["1"], sh.pin_points(r)["2"]
    sh.wire(p1[0], p1[1], p1[0], p1[1] - G)
    sh.label("EN_5V", p1[0], p1[1] - G, 90)
    sh.wire(p2[0], p2[1], p2[0], p2[1] + 2 * G)
    sh.place("stmbl:GND", sh.libs["stmbl:GND"], "#PWR952", "GND", p2[0], p2[1] + 2 * G, [path])
    for i, line in enumerate(("stman: U13, U27, U28 are LMR51420YF (ACT4088 end of life)",
                              "R35, R59, R132 for VFB 0.6 V; EN: R212, R213 pull-ups, R214 pull-down",
                              "D9, D12, D13 not fitted (synchronous); C102, C133, C137 4.7u 50V (0603 X5R)")):
        sh.text(line, 25.4, 149.86 + 2.54 * i, size=1.27)


def edit_fb():
    """A polyfuse in each port's supply: F200 (FB0) / F201 (FB1) between the connector's pin 7 and the
    port supply (JP2 / JP5, C114 / C117), in place of the wire from the connector."""
    sh = sheet("fb")
    sh.remove_wire((55.88, 92.71), (63.5, 92.71))
    f = kicad_sym("Device", "Polyfuse")
    rp = root_paths()
    sh.place("Device:Polyfuse", f, "F200", "0.5A hold", 59.69, 92.71, [(rp["FB0"], "F200"), (rp["FB1"], "F201")], angle=90,
             footprint="Fuse:Fuse_1206_3216Metric", fields={"Note": "stman: port supply polyfuse (5 V or 12 V by JP2/JP5)"})
    sh.save()


def edit_io():
    sh = sheet("io")
    sh.remove_ref(*REMOVE["io"])
    sh.cleanup()
    sh.save()


def edit_root():
    sh = sheet(PROJECT)
    sh.remove_ref(*REMOVE["stman"])
    for l in list(sh.items("hierarchical_label")):
        sh.remove(l)
    # sheet pins with no hierarchical label left in their sheet go, with their wires
    for s in sh.items("sheet"):
        fn = [p[2] for p in findall(s, "property") if p[1] == "Sheetfile"][0]
        child = Sheet(os.path.join(OUT, fn), PROJECT)
        names = {l[1] for l in child.items("hierarchical_label")}
        s[:] = [x for x in s if not (isinstance(x, list) and x and x[0] == "pin" and x[1] not in names)]
    sh.cleanup()
    sh.save()


# ------------------------------------------------------------------ 3. new sheets
# (built by gen/newsheets.py)

def project_file():
    with open(os.path.join(OUT, PROJECT + ".kicad_pro"), "w") as f:
        json.dump({"meta": {"filename": PROJECT + ".kicad_pro", "version": 1},
                   "schematic": {"legacy_lib_dir": "", "legacy_lib_list": []},
                   "sheets": [], "text_variables": {}, "boards": [],
                   "libraries": {"pinned_footprint_libs": [], "pinned_symbol_libs": []}}, f, indent=2)
    with open(os.path.join(OUT, "fp-lib-table"), "w") as f:
        f.write('(fp_lib_table\n  (lib (name "stmbl")(type "KiCad")(uri "${KIPRJMOD}/../../hw/kicad/lib/stmbl.pretty")(options "")(descr "stmbl footprints"))\n)\n')


def main():
    libs = convert()
    edit_f4()
    edit_cmd(libs)
    edit_io()
    edit_supply()
    edit_fb()
    import newsheets
    newsheets.build(OUT, PROJECT, libs, root_paths, sheet)
    edit_root()
    project_file()
    print("wrote", OUT)


if __name__ == "__main__":
    main()
