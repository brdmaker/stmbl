"""stman's new sheets: dac, ain, drvio (docs/stman.md). Parts are drawn in
framed groups, each pin joined to its net by a short wire and a label:
global labels for nets shared with the MCU sheet and between the new
sheets, local labels inside a sheet, stmbl's power symbols for the rails.

NEW: {sheet: (title, [(group title, [part, ...]), ...])}; a part is
(ref, lib_id, value, footprint, {pin: net}, note). A pin left out gets a
no-connect flag. Refs from 200 up.
"""
import os
import sys
import uuid

HERE = os.path.dirname(os.path.abspath(__file__))
from sexp import Sym, parse, dump, find, findall  # noqa: E402
import legacy  # noqa: E402
from schedit import Sheet, S, uid  # noqa: E402

G = 2.54
FP_R = "Resistor_SMD:R_0603_1608Metric"
FP_C = "Capacitor_SMD:C_0603_1608Metric"
FP_C0805 = "Capacitor_SMD:C_0805_2012Metric"
FP_C1206 = "Capacitor_SMD:C_1206_3216Metric"
FP_TB = "Connector_Phoenix_MC:PhoenixContact_MC_1,5_%d-G-3.5_1x%02d_P3.50mm_Horizontal"

POWER = {"GND": "stmbl:GND", "+3V3": "stmbl:+3.3V", "+5V": "stmbl:+5V", "+12V": "stmbl:+12V", "+24V": "stmbl:+24V",
         "+15VA": "stman:+15VA", "-15VA": "stman:-15VA"}
# nets that cross sheets (to the MCU sheet, or between the new sheets): global labels
GLOBAL_PREFIX = ("SPI_", "DAC1_SYNC", "DAC2_SYNC", "DAC_", "ADC_", "WD_", "OUT1_MCU", "OUT2_MCU", "OUT3_MCU", "OUT4_MCU", "OUT5_MCU",
                 "OUT6_MCU", "OUT7_MCU", "OUT8_MCU", "IN1_MCU", "IN2_MCU", "IN3_MCU", "IN4_MCU", "IN5_MCU", "IN6_MCU", "IN7_MCU", "IN8_MCU",
                 "ST_", "AO1_P", "AO2_P")


def R(ref, value, a, b, note=""):
    return (ref, "Device:R", value, FP_R, {"1": a, "2": b}, note)


def C(ref, value, a, b, fp=FP_C, note=""):
    return (ref, "Device:C", value, fp, {"1": a, "2": b}, note)


def TB(ref, n, value, nets, note):
    return (ref, "Connector_Generic:Conn_01x%02d" % n, value, FP_TB % (n, n), {str(i + 1): x for i, x in enumerate(nets)}, note)


def dac_channel(n, ref, amp, r):
    pins_amp = {"A": ("3", "2", "1"), "B": ("5", "6", "7")}[amp]
    parts = [
        (ref, "stman:AD5761R", "AD5761RBRUZ", "Package_SO:TSSOP-16_4.4x5mm_P0.65mm",
         {"9": "+15VA", "10": "-15VA", "7": "+3V3", "8": "+3V3", "12": "GND", "14": "GND", "13": "DAC%d_REF" % n,
          "2": "SPI_SCK", "3": "SPI_MOSI", "15": "SPI_MISO", "1": "DAC%d_SYNC" % n, "4": "GND", "5": "WD_OK",
          "6": "DAC_RESET", "16": "DAC_ALERT", "11": "DAC%d_OUT" % n},
         "* pin numbers: from the pin names, check the datasheet"),
        C("C%d" % (r + 3), "100n", "DAC%d_REF" % n, "GND"),
        C("C%d" % (r + 4), "100n", "+15VA", "GND"), C("C%d" % (r + 5), "100n", "-15VA", "GND"), C("C%d" % (r + 6), "100n", "+3V3", "GND"),
        R("R%d" % r, "4.7k", "DAC%d_OUT" % n, "LP%d_A" % n), R("R%d" % (r + 1), "4.7k", "LP%d_A" % n, "LP%d_B" % n),
        C("C%d" % r, "22n C0G", "LP%d_A" % n, "AO%d_F" % n), C("C%d" % (r + 1), "10n C0G", "LP%d_B" % n, "GND"),
        R("R%d" % (r + 2), "100", "AO%d_F" % n, "AO%d_P" % n),
        ("D%d" % r, "Diode:SMAJ15CA", "SMAJ15CA", "Diode_SMD:D_SMA", {"1": "AO%d_P" % n, "2": "GND"}, "TVS"),
        R("R%d" % (r + 3), "10", "GND", "AO%d_N" % n, "AO- link to the drive's E2"),
    ]
    return parts, {pins_amp[0]: "LP%d_B" % n, pins_amp[1]: "AO%d_F" % n, pins_amp[2]: "AO%d_F" % n}


def new():
    d1, a1 = dac_channel(1, "U40", "A", 230)
    d2, a2 = dac_channel(2, "U41", "B", 240)
    amp = {"8": "+15VA", "4": "-15VA"}
    amp.update(a1)
    amp.update(a2)
    dac = ("Command outputs AO1, AO2: AD5761R, 2nd-order low-pass 2.3 kHz, ±15 V", [
        ("±15 V: NMA1215 from +12 V, LC filtered", [
            ("U56", "Regulator_Switching:NMA1215SC", "NMA1215SC", "Converter_DCDC:Converter_DCDC_Murata_NMAxxxxSC_THT",
             {"1": "+12V", "2": "GND", "4": "-15V_RAW", "5": "GND", "6": "+15V_RAW"}, "output common to GND"),
            C("C200", "4.7u 25V", "+12V", "GND", FP_C0805),
            ("L200", "Device:L", "BLM18 ferrite", "Inductor_SMD:L_0603_1608Metric", {"1": "+15V_RAW", "2": "+15VA"}, ""),
            ("L201", "Device:L", "BLM18 ferrite", "Inductor_SMD:L_0603_1608Metric", {"1": "-15V_RAW", "2": "-15VA"}, ""),
            C("C201", "4.7u 25V", "+15V_RAW", "GND", FP_C0805), C("C202", "4.7u 25V", "-15V_RAW", "GND", FP_C0805),
            C("C203", "10u 25V", "+15VA", "GND", FP_C1206), C("C204", "10u 25V", "-15VA", "GND", FP_C1206)]),
        ("AO1: DAC, Sallen-Key (R230 R231 C230 C231, U42A), 100 R, TVS", d1),
        ("AO2 (fitted for the spindle's C axis): DAC, Sallen-Key (U42B)", d2),
        ("Filter op-amp, /ALERT, connector J5", [
            ("U42", "stman:OPA2197", "OPA2197IDR", "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm", amp, "unit A AO1, unit B AO2"),
            C("C250", "100n", "+15VA", "GND"), C("C251", "100n", "-15VA", "GND"),
            R("R250", "10k", "+3V3", "DAC_ALERT", "/ALERT pull-up"),
            TB("J5", 6, "AO", ["AO1_P", "AO1_N", "SHLD_AO", "AO2_P", "AO2_N", "SHLD_AO"], "to the drive's E1/E2 (E3/E4)"),
            C("C252", "10n 100V", "SHLD_AO", "GND")]),
    ])
    gnd_pins = (16, 17, 18, 19, 20, 21, 22, 27, 28, 29, 30, 31, 32, 33, 50, 52, 54, 56, 58, 60, 62, 64)
    adc_nets = {"1": "+5V", "37": "+5V", "38": "+5V", "48": "+5V", "23": "+3V3",
                "2": "GND", "26": "GND", "35": "GND", "40": "GND", "41": "GND", "47": "GND", "43": "GND", "46": "GND",
                "3": "GND", "4": "GND", "5": "GND", "6": "+3V3", "7": "+3V3", "8": "+3V3", "34": "+3V3",
                "9": "ADC_CONVST", "10": "ADC_CONVST", "11": "ADC_RESET", "12": "SPI_SCK", "13": "ADC_CS", "14": "ADC_BUSY",
                "24": "SPI_MISO", "36": "ADC_REGCAP1", "39": "ADC_REGCAP2", "42": "ADC_REF", "44": "ADC_REFCAP", "45": "ADC_REFCAP",
                "49": "AI1_IN", "51": "AI2_IN", "53": "AI0VM_IN", "55": "AO1_P", "57": "AO2_P", "59": "MON_P15", "61": "MON_N15", "63": "MON_24"}
    adc_nets.update({str(p): "GND" for p in gnd_pins})
    ain = ("Monitor ADC: AD7606 (drive monitors and 0VM, AO readbacks, rails)", [
        ("AD7606: 8 ch simultaneous, ±10 V (RANGE high), serial (PAR/SER high), internal reference", [
            ("U45", "Analog_ADC:AD7606", "AD7606BSTZ", "Package_QFP:LQFP-64_10x10mm_P0.5mm", adc_nets,
             "DB pins to GND in serial mode; * DOUT_A shares MISO: check tri-state with /CS high"),
            C("C270", "100n", "+5V", "GND"), C("C271", "10u", "+5V", "GND", FP_C0805), C("C272", "100n", "+3V3", "GND"),
            C("C273", "1u", "ADC_REGCAP1", "GND"), C("C274", "1u", "ADC_REGCAP2", "GND"),
            C("C275", "100n", "ADC_REF", "GND"), C("C276", "10u", "ADC_REFCAP", "GND", FP_C0805)]),
        ("Inputs: 2.2k series (the ADC's 1 MΩ inputs, ±16.5 V clamps); rail dividers; J6", [
            R("R270", "2.2k", "AI1", "AI1_IN"), R("R271", "2.2k", "AI2", "AI2_IN"), R("R272", "2.2k", "AI0VM", "AI0VM_IN"),
            R("R273", "10k", "+15VA", "MON_P15"), R("R274", "10k", "MON_P15", "GND"),
            R("R275", "10k", "-15VA", "MON_N15"), R("R276", "10k", "MON_N15", "GND"),
            R("R277", "30k", "+24V", "MON_24"), R("R278", "10k", "MON_24", "GND"),
            TB("J6", 4, "AI", ["AI1", "AI2", "AI0VM", "SHLD_AI"], "AI1 AI2 0VM shield"),
            C("C277", "10n 100V", "SHLD_AI", "GND")]),
    ])
    dout = []
    for u, base, sta, stb in (("U50", 0, "ST_A12", "ST_A34"), ("U51", 4, "ST_B12", "ST_B34")):
        ins = ["OUT1_IN", "OUT2_IN", "OUT3_MCU", "OUT4_MCU"] if not base else ["OUT5_MCU", "OUT6_MCU", "OUT7_MCU", "OUT8_MCU"]
        nets = {"3": ins[0], "5": ins[1], "7": ins[2], "9": ins[3], "4": sta, "8": stb, "2": "GND", "6": "GND",
                "18": "OUT%d" % (base + 1), "17": "OUT%d" % (base + 2), "14": "OUT%d" % (base + 3), "13": "OUT%d" % (base + 4)}
        nets.update({str(p): "+24V" for p in (1, 10, 11, 12, 15, 16, 19, 20)})
        dout.append((u, "Driver_FET:ITS724G", "ITS724G", "Package_SO:SOIC-20W_7.5x12.8mm_P1.27mm", nets, "* status logic"))
        dout.append(C("C%d" % (280 + base // 4), "100n 50V", "+24V", "GND"))
    dout += [
        ("U55", "stman:74LVC2G08", "74LVC2G08", "Package_TO_SOT_SMD:SOT-23-8",
         {"1": "OUT1_MCU", "2": "WD_OK", "7": "OUT1_IN", "5": "OUT2_MCU", "6": "WD_OK", "3": "OUT2_IN", "4": "GND", "8": "+3V3"},
         "OUT1 (RF), OUT2 (AH/RUN) only while the watchdog is fed"),
        C("C282", "100n", "+3V3", "GND"),
        R("R280", "10k", "OUT1_MCU", "GND"), R("R281", "10k", "OUT2_MCU", "GND"),
        R("R282", "10k", "+3V3", "ST_A12"), R("R283", "10k", "+3V3", "ST_A34"), R("R284", "10k", "+3V3", "ST_B12"), R("R285", "10k", "+3V3", "ST_B34"),
        TB("J7", 10, "OUT", ["OUT%d" % i for i in range(1, 9)] + ["GND", "GND"], "OUT1-8, 0V, 0V"),
    ]
    wdog = [
        ("U57", "Power_Supervisor:MAX6369", "MAX6369KA+T", "Package_TO_SOT_SMD:SOT-23-8",
         {"1": "WD_WDI", "2": "GND", "4": "+3V3", "5": "GND", "6": "GND", "7": "WD_OK", "8": "+3V3"}, "* SET0..2: timeout table"),
        C("C285", "100n", "+3V3", "GND"), R("R286", "10k", "+3V3", "WD_OK"),
    ]
    din = []
    for k in range(1, 9):
        r = 288 + 2 * k
        din += [R("R%d" % r, "47k", "IN%d" % k, "IN%d_MCU" % k), R("R%d" % (r + 1), "10k", "IN%d_MCU" % k, "GND"),
                C("C%d" % r, "1n", "IN%d_MCU" % k, "GND"),
                ("D%d" % r, "Diode:BAT54S", "BAT54S", "Package_TO_SOT_SMD:SOT-23", {"1": "GND", "2": "+3V3", "3": "IN%d_MCU" % k}, "")]
    din.append(TB("J8", 10, "IN", ["IN%d" % i for i in range(1, 9)] + ["+24V", "+24V"], "IN1-8, +24V for contacts (Bb)"))
    drvio = ("Drive I/O: 8 x 24 V out (ITS724G), 8 x 24 V in, watchdog", [
        ("Outputs: ITS724G x2, OUT1/OUT2 gated by the watchdog", dout),
        ("Watchdog MAX6369: WD_OK low -> DACs to 0 V, OUT1/OUT2 off", wdog),
        ("Inputs: 47k / 10k, 1 nF, BAT54S clamp (high > ~13 V, low < ~6 V)", din),
    ])
    return {"dac": dac, "ain": ain, "drvio": drvio}


# ------------------------------------------------------------------ drawing
def label_kind(net):
    if net in POWER:
        return "power"
    if net.startswith(GLOBAL_PREFIX):
        return "global"
    return "local"


def out_dir(ang):
    """a pin at library angle `ang` points into the body: the stub goes the other way (sheet coords)"""
    return {0: (-1, 0), 180: (1, 0), 90: (0, 1), 270: (0, -1)}[ang]


def extent(lib, nets):
    pins = Sheet.pin_points_lib(lib)
    xs, ys = [-5.0, 5.0], [-5.0, 5.0]
    for n, (lx, ly, a) in pins.items():
        x, y = lx, -ly
        dx, dy = out_dir(a)
        net = nets.get(n)
        ln = 2 * G + (len(net) * 1.1 + 3 if net and label_kind(net) != "power" else 5)
        xs += [x, x + dx * ln]
        ys += [y, y + dy * ln]
        if dy:
            xs += [x - 2.5, x + 2.5]
    for r in findall(lib, "symbol"):
        for rect in findall(r, "rectangle"):
            a, b = find(rect, "start"), find(rect, "end")
            xs += [float(a[1]), float(b[1])]
            ys += [-float(a[2]), -float(b[2])]
    return min(xs), min(ys) - 3, max(xs), max(ys) + 3


def draw_part(sh, part, x, y, path, libs_for, pwr_ctr):
    ref, lib_id, value, fp, nets, note = part
    lib = libs_for(lib_id)
    sym = sh.place(lib_id, lib, ref, value, x, y, [path], footprint=fp, fields={"Note": note} if note else None)
    lsym = sh.libs[lib_id]
    for n, (lx, ly, a) in Sheet.pin_points_lib(lsym).items():
        px, py = round(x + lx, 4), round(y - ly, 4)
        net = nets.get(n)
        ptype = [find(q, "number")[1] for u in findall(lsym, "symbol") for q in findall(u, "pin") if find(q, "number")[1] == n]
        if net is None:
            sh.no_connect(px, py)
            continue
        dx, dy = out_dir(a)
        ex, ey = round(px + dx * 2 * G, 4), round(py + dy * 2 * G, 4)
        sh.wire(px, py, ex, ey)
        kind = label_kind(net)
        if kind == "power" and dx:
            # beside an IC's other pins: the rail as a global label (KiCad joins it to the power symbols' net)
            kind = "global"
        if kind == "power":
            pid = POWER[net]
            pwr_ctr[0] += 1
            down = net == "GND" or net.startswith("-")
            # rails above, grounds below: a short leg so the symbol stands clear of the stub
            if dx:
                ey2 = ey + (G if down else -G)
                sh.wire(ex, ey, ex, ey2)
                ey = ey2
            sh.place(pid, libs_for(pid), "#PWR%04d" % pwr_ctr[0], net, ex, ey, [path])
        else:
            ang = {(-1, 0): 180, (1, 0): 0, (0, 1): 270, (0, -1): 90}[(dx, dy)]
            sh.label(net, ex, ey, ang, kind="global_label" if kind == "global" else "label")


def build_sheet(out, project, name, title, groups, path, libs_for, paper=(420, 297), pwr_base=1000):
    fu = str(uuid.uuid5(uuid.UUID("5a0e2b3c-1d4e-4f5a-8b6c-7d8e9fa0b1c2"), name))
    t = [Sym("kicad_sch"), S("version", 20230121), S("generator", Sym("eeschema")), S("uuid", fu),
         S("paper", "A3"), S("title_block", S("title", "stman: " + title), S("comment", 1, "docs/stman.md"),
                              S("comment", 2, "* in a part's Note: check against the datasheet")), [Sym("lib_symbols")]]
    fn = os.path.join(out, name + ".kicad_sch")
    open(fn, "w").write(dump(t) + "\n")
    sh = Sheet(fn, project)
    pwr = [pwr_base]
    x0, y0, W = 20.32, 22.86, paper[0] - 40
    y = y0
    for gtitle, parts in groups:
        items = [(p, extent(libs_for(p[1]), p[4])) for p in parts]
        # rows
        gx, gy, row_h, pos = 0.0, 0.0, 0.0, []
        for p, (a, b, c, d) in items:
            w, h = c - a, d - b
            if gx > 0 and gx + w > W - 12:
                gx, gy, row_h = 0.0, gy + row_h + 5.08, 0.0
            pos.append((p, gx - a, gy - b))
            gx += w + 5.08
            row_h = max(row_h, h)
        gh = gy + row_h
        sh.text(gtitle, x0, y + 5.08, size=2.0)
        top = y + 7.62
        for p, dx, dy in pos:
            sx, sy = round((x0 + 6 + dx) / 1.27) * 1.27, round((top + 3 + dy) / 1.27) * 1.27
            draw_part(sh, p, sx, sy, path, libs_for, pwr)
        sh.add(S("rectangle", S("start", x0, y), S("end", round(x0 + W, 4), round(top + gh + 6, 4)),
                 S("stroke", S("width", 0.25), S("type", Sym("dash"))), S("fill", S("type", Sym("none"))),
                 S("uuid", uid(fn, "frame", gtitle))))
        y = top + gh + 12
    if y > paper[1] - 10:
        find(sh.t, "paper")[1] = "A2" if y < 410 else "A1"
    sh.save()
    return fu, y


def build(out, project, libs, root_paths, sheet):
    import build as B
    root = sheet(project)
    ru = find(root.t, "uuid")[1]
    own = {"stman:AD5761R": B.AD5761R, "stman:OPA2197": B.OPA2197, "stman:74LVC2G08": B.AND2,
           "stman:+15VA": B.power_sym("+15VA", libs["stmbl:+12V"]), "stman:-15VA": B.power_sym("-15VA", libs["stmbl:GND"])}
    cache = {}

    def libs_for(lib_id):
        if lib_id in own:
            return own[lib_id]
        if lib_id in libs:
            return libs[lib_id]
        if lib_id not in cache:
            lib, nm = lib_id.split(":")
            cache[lib_id] = B.kicad_sym(lib, nm)
        return cache[lib_id]

    x, y = 165.1, 105.41
    for i, (name, (title, groups)) in enumerate(new().items()):
        su = str(uuid.uuid5(uuid.UUID("5a0e2b3c-1d4e-4f5a-8b6c-7d8e9fa0b1c3"), name))
        path = "/" + ru + "/" + su
        build_sheet(out, project, name, title, groups, path, libs_for, pwr_base=1000 * (i + 1))
        sy = y + i * 15.24
        root.add(S("sheet", S("at", x, sy), S("size", 30.48, 10.16), S("fields_autoplaced"),
                   S("stroke", S("width", 0.1524), S("type", Sym("solid"))), S("fill", S("color", 0, 0, 0, 0.0)), S("uuid", su),
                   S("property", "Sheetname", name, S("at", x, sy - 0.7, 0), legacy.eff(60, justify=["left", "bottom"])),
                   S("property", "Sheetfile", name + ".kicad_sch", S("at", x, sy + 10.76, 0), legacy.eff(60, justify=["left", "top"])),
                   S("instances", [Sym("project"), project] + [S("path", "/" + ru, S("page", str(10 + i)))])))
    root.text("stman (docs/stman.md): stmbl v4.0's LV sheets, edited in place;\n"
              "dac, ain, drvio: stman's new circuits (global labels to the MCU sheet).", x, y + 50, size=1.8)
    root.save()
