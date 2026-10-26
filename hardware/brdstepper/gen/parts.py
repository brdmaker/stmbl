"""brdstepper: every part and what each of its pins connects to, from
docs/brdstepper_hw.md (section numbers in the comments). hardware/kigen makes
the KiCad schematic and board of it (board.py: the placement).

A part: (ref, lib_id, value, footprint, {pin: net}, notes). A pin left out
is not connected (a no-connect flag). Nets are named as in the document.
"""

PROJECT = "brdstepper"
TITLE = "closed-loop NEMA11 stepper drive"
DOC = "docs/brdstepper_hw.md"
DATE = "2026-10-23"
REV = "0.1"
NOTE = ("brdstepper: STM32G473 + AS5047P + 2x DRV8876, 28 x 28 mm on the back of a NEMA11 motor.\n"
        "Connections are by net name (net labels): docs/brdstepper_hw.md is the reference.\n"
        "Variant A fits U8/J4 (smart-serial to a Mesa card), variant B U9/J5/J6/JP1 (CAN).")
NETCLASS_POWER = ("VM", "GND", "COIL_*", "5V*", "BUCK_SW")
VARIANTS = {r: "A" for r in ("U8", "J4", "R20", "C38")}               # smart-serial
VARIANTS.update({r: "B" for r in ("U9", "J5", "J6", "JP1", "R21", "C39", "C40")})  # CAN
NOT_FITTED = {"H1", "H2", "H3", "H4", "TP1", "TP2", "TP3", "JP1"}

FP_R = "Resistor_SMD:R_0402_1005Metric"
FP_C = "Capacitor_SMD:C_0402_1005Metric"
FP_C0603 = "Capacitor_SMD:C_0603_1608Metric"
FP_C0805 = "Capacitor_SMD:C_0805_2012Metric"
FP_C1206 = "Capacitor_SMD:C_1206_3216Metric"
FP_C1210 = "Capacitor_SMD:C_1210_3225Metric"


def R(ref, value, a, b, fp=FP_R, note=""):
    return (ref, "Device:R", value, fp, {"1": a, "2": b}, note)


def C(ref, value, a, b, fp=FP_C, note=""):
    return (ref, "Device:C", value, fp, {"1": a, "2": b}, note)


def DRV(ref, coil, in1, in2, ipropi, vcp, cph, cpl):
    """§4.2: a DRV8876 per coil, wired the same."""
    return (ref, "brdstepper:DRV8876RGT", "DRV8876RGTR", "Package_DFN_QFN:VQFN-16-1EP_3x3mm_P0.5mm_EP1.68x1.68mm_ThermalVias",
            {"1": "nSLEEP", "2": "nFAULT", "3": "VREF_DRV", "4": ipropi, "6": "COIL_%s+" % coil, "7": "GND", "8": "COIL_%s-" % coil,
             "9": "VM", "10": vcp, "11": cph, "12": cpl, "13": "GND", "14": "3V3", "15": in1, "16": in2, "17": "GND"},
            "IMODE (5) open: fixed off-time, overcurrent latched; PMODE high: IN1/IN2 PWM")


SHEETS = {
    # ------------------------------------------------------------ §3 power
    "power": [
        ("J2", "Connector_Generic:Conn_01x03", "VM / GND / EN", "Connector_JST:JST_XH_B3B-XH-A_1x03_P2.50mm_Vertical",
         {"1": "VM", "2": "GND", "3": "EN_RAW"}, "motor supply 12-36 V; EN: enable / estop input ✱ current rating"),
        ("D2", "Diode:SMAJ24A", "SMAJ24A", "Diode_SMD:D_SMA", {"1": "VM", "2": "GND"}, "TVS ✱ (for 24 V; lower for less)"),
        C("C1", "22u 50V", "VM", "GND", FP_C1210, "bulk, near the drivers"),
        C("C2", "22u 50V", "VM", "GND", FP_C1210, "bulk"),
        ("U6", "brdstepper:TPS560430", "TPS560430XDBVR", "Package_TO_SOT_SMD:SOT-23-6",
         {"1": "BUCK_CB", "2": "GND", "3": "BUCK_FB", "4": "VM", "5": "VM", "6": "BUCK_SW"}, "buck to 5 V ✱ (values per its datasheet)"),
        C("C3", "2.2u 50V", "VM", "GND", FP_C1206, "buck input"),
        C("C4", "100n 50V", "VM", "GND", FP_C, "buck input, at the pin"),
        C("C5", "100n", "BUCK_CB", "BUCK_SW", FP_C, "bootstrap"),
        ("L1", "Device:L", "10u", "Inductor_SMD:L_Taiyo-Yuden_NR-40xx", {"1": "BUCK_SW", "2": "5V_BUCK"}, "✱ NR4018, ≥ 0.6 A"),
        C("C6", "22u 10V", "5V_BUCK", "GND", FP_C0805, "buck output"),
        C("C7", "22u 10V", "5V_BUCK", "GND", FP_C0805, "buck output"),
        R("R1", "88.7k 1%", "5V_BUCK", "BUCK_FB", note="5.02 V with 22.1k (VREF 1.00 V)"),
        R("R2", "22.1k 1%", "BUCK_FB", "GND"),
        ("D1", "Diode:BAT54C", "BAT54C", "Package_TO_SOT_SMD:SOT-23", {"1": "5V_BUCK", "2": "5V_USB", "3": "5V"}, "5 V OR: buck, USB"),
        ("D3", "Device:D_Schottky", "BAT54WS", "Diode_SMD:D_SOD-323", {"1": "5V", "2": "5V_MESA"}, "5 V OR: the Mesa port (variant A)"),
        C("C8", "1u", "5V", "GND", FP_C, "LDO input"),
        ("U5", "Regulator_Linear:TLV75533PDBV", "TLV75533PDBVR", "Package_TO_SOT_SMD:SOT-23-5",
         {"1": "5V", "2": "GND", "3": "5V", "5": "3V3"}, "3.3 V, ≥ 150 mA ✱"),
        C("C9", "1u", "3V3", "GND", FP_C, "LDO output"),
        ("FB1", "Device:FerriteBead_Small", "600R@100MHz", "Inductor_SMD:L_0603_1608Metric", {"1": "3V3", "2": "VDDA"}, "VDDA / VREF+"),
        C("C10", "1u", "VDDA", "GND", FP_C, "VDDA (21)"),
        C("C11", "10n", "VDDA", "GND", FP_C, "VDDA (21)"),
        C("C12", "1u", "VDDA", "GND", FP_C, "VREF+ (20)"),
        C("C13", "10n", "VDDA", "GND", FP_C, "VREF+ (20)"),
        R("R3", "100k 1%", "VM", "VM_SENSE", note="§4.8: 3.0 V at 33 V"),
        R("R4", "10k 1%", "VM_SENSE", "GND"),
        C("C14", "10n", "VM_SENSE", "GND"),
    ],
    # ------------------------------------------------------------ §2, §4.1 the MCU
    "mcu": [
        ("U1", "MCU_ST_STM32G4:STM32G473CEUx", "STM32G473CEU6", "Package_DFN_QFN:QFN-48-1EP_7x7mm_P0.5mm_EP5.6x5.6mm",
         {"1": "3V3", "5": "OSC_IN", "6": "OSC_OUT", "7": "NRST", "8": "IPROPI_A", "9": "IPROPI_B", "10": "VM_SENSE", "11": "NTC",
          "12": "VREF_DRV", "13": "nSLEEP", "14": "nFAULT", "15": "EN_IN", "17": "AIN", "18": "HWID", "20": "VDDA", "21": "VDDA",
          "22": "TP_PB10", "23": "3V3", "24": "TP_PB11", "25": "LED", "26": "CAN_STB", "29": "B_IN2", "30": "A_IN1", "31": "A_IN2",
          "32": "B_IN1", "33": "USB_DM", "34": "USB_DP", "35": "3V3", "36": "SWDIO", "37": "SWCLK", "38": "ENC_CS", "41": "ENC_CLK",
          "42": "ENC_MISO", "43": "ENC_MOSI", "44": "SS_TX", "45": "SS_RX", "46": "CAN_RX", "47": "CAN_TX", "48": "3V3", "49": "GND"},
         "exposed pad: the only VSS / VSSA; solder it, with vias to the In1 GND plane (add them in layout). Spare: PB2, PB14, PB15, PC4, PC10, PC11, PC13"),
        ("Y1", "Device:Crystal_GND24", "8MHz", "Crystal:Crystal_SMD_2016-4Pin_2.0x1.6mm", {"1": "OSC_IN", "3": "OSC_OUT", "2": "GND", "4": "GND"}, "✱ CL"),
        C("C15", "10p C0G", "OSC_IN", "GND", note="✱ per the crystal's CL"),
        C("C16", "10p C0G", "OSC_OUT", "GND"),
        C("C17", "100n", "NRST", "GND"),
        C("C18", "100n", "3V3", "GND", note="VDD (23)"),
        C("C19", "100n", "3V3", "GND", note="VDD (35)"),
        C("C20", "100n", "3V3", "GND", note="VDD (48)"),
        C("C21", "4.7u", "3V3", "GND", FP_C0603),
        R("R5", "10k", "CAN_RX", "GND", note="PB8-BOOT0 low until the option bytes are set"),
        R("R6", "1k", "LED", "LED_A"),
        ("D4", "Device:LED", "green", "LED_SMD:LED_0603_1608Metric", {"1": "GND", "2": "LED_A"}, ""),
        R("R7", "10k", "3V3", "HWID", note="§6 HWID: A not fitted, B 10k"),
        R("R8", "10k", "HWID", "GND", note="§6 HWID: A 0 Ω, B 10k"),
        R("R9", "1k", "AIN_IN", "AIN", note="spare analog input"),
        C("C22", "10n", "AIN", "GND"),
        ("TP1", "Connector:TestPoint", "AIN", "TestPoint:TestPoint_Pad_D1.0mm", {"1": "AIN_IN"}, ""),
        ("TP2", "Connector:TestPoint", "PB10", "TestPoint:TestPoint_Pad_D1.0mm", {"1": "TP_PB10"}, "spare I/O"),
        ("TP3", "Connector:TestPoint", "PB11", "TestPoint:TestPoint_Pad_D1.0mm", {"1": "TP_PB11"}, "spare I/O"),
        ("J7", "Connector_Generic:Conn_01x05", "SWD", "Connector_PinHeader_1.27mm:PinHeader_1x05_P1.27mm_Vertical_SMD_Pin1Left",
         {"1": "3V3", "2": "SWDIO", "3": "NRST", "4": "SWCLK", "5": "GND"},
         "§4.7's 1.27 mm 5-pin (VCC SWDIO NRST SWCLK GND): pads for pogo pins or a header; a TC2030's guide holes find no room"),
        R("R10", "10k 1%", "3V3", "NTC", note="NTC pull-up"),
        ("TH1", "Device:Thermistor_NTC", "10k B3380", "Resistor_SMD:R_0603_1608Metric", {"1": "NTC", "2": "GND"}, "close to the drivers"),
    ],
    # ------------------------------------------------------------ §4.2, §5 the H-bridges
    "bridges": [
        DRV("U2", "A", "A_IN1", "A_IN2", "IPROPI_A", "U2_VCP", "U2_CPH", "U2_CPL"),
        DRV("U3", "B", "B_IN1", "B_IN2", "IPROPI_B", "U3_VCP", "U3_CPH", "U3_CPL"),
        C("C23", "100n 50V", "VM", "GND", FP_C, "U2 VM, at the pin"),
        C("C24", "10u 50V", "VM", "GND", FP_C1206, "U2 VM"),
        C("C25", "100n 16V", "U2_VCP", "VM", FP_C, "U2 VCP"),
        C("C26", "22n 50V", "U2_CPH", "U2_CPL", FP_C, "U2 flying"),
        C("C27", "100n 50V", "VM", "GND", FP_C, "U3 VM, at the pin"),
        C("C28", "10u 50V", "VM", "GND", FP_C1206, "U3 VM"),
        C("C29", "100n 16V", "U3_VCP", "VM", FP_C, "U3 VCP"),
        C("C30", "22n 50V", "U3_CPH", "U3_CPL", FP_C, "U3 flying"),
        R("R11", "2.0k 1%", "IPROPI_A", "GND", note="§5: 3.3 V = 1.65 A"),
        C("C31", "100p C0G", "IPROPI_A", "GND", note="≤ 100 pF (or not fitted)"),
        R("R12", "2.0k 1%", "IPROPI_B", "GND"),
        C("C32", "100p C0G", "IPROPI_B", "GND"),
        R("R13", "100k", "nSLEEP", "GND"),
        R("R14", "10k", "nFAULT", "3V3"),
        C("C33", "100n", "VREF_DRV", "GND"),
        ("J3", "Connector_Generic:Conn_01x04", "MOTOR", "Connector_PinHeader_2.00mm:PinHeader_1x04_P2.00mm_Vertical",
         {"1": "COIL_A+", "2": "COIL_A-", "3": "COIL_B+", "4": "COIL_B-"},
         "A+ A- B+ B-: solder pads for the motor leads (§4.7 allows pads; a JST-PH 4 doesn't fit the 28 mm board)"),
    ],
    # ------------------------------------------------------------ §4.3 encoder, §4.7 EN_IN
    "encoder": [
        ("U4", "brdstepper:AS5047P", "AS5047P-ATSM", "Package_SO:TSSOP-14_4.4x5mm_P0.65mm",
         {"1": "ENC_CS", "2": "ENC_CLK", "3": "ENC_MISO", "4": "ENC_MOSI", "5": "GND", "11": "3V3", "12": "3V3", "13": "GND"},
         "on the bottom, centred on the shaft ✱; 3.3 V mode: VDD and VDD3V3 together"),
        C("C34", "100n", "3V3", "GND", note="✱ per the 3.3 V application circuit"),
        C("C35", "10u", "3V3", "GND", FP_C0603),
        R("R15", "10k", "EN_RAW", "EN_IN", note="EN_IN 3.3-24 V ✱"),
        R("R16", "100k", "EN_IN", "GND"),
        C("C36", "10n", "EN_IN", "GND"),
        ("D5", "Device:D_Schottky", "BAT54WS", "Diode_SMD:D_SOD-323", {"1": "3V3", "2": "EN_IN"}, "clamp to 3V3 ✱"),
    ],
    # ------------------------------------------------------------ §4.4-4.6 USB, smart-serial, CAN
    "interfaces": [
        ("J1", "Connector:USB_C_Receptacle_USB2.0_16P", "USB-C", "Connector_USB:USB_C_Receptacle_GCT_USB4105-xx-A_16P_TopMnt_Horizontal",
         {"A4": "5V_USB", "A9": "5V_USB", "B4": "5V_USB", "B9": "5V_USB", "A6": "USB_DP", "B6": "USB_DP", "A7": "USB_DM", "B7": "USB_DM",
          "A5": "USB_CC1", "B5": "USB_CC2", "A1": "GND", "B1": "GND", "A12": "GND", "B12": "GND", "S1": "USB_SHIELD"}, "USB4105 ✱"),
        R("R17", "5.1k", "USB_CC1", "GND"),
        R("R18", "5.1k", "USB_CC2", "GND"),
        R("R19", "1M", "USB_SHIELD", "GND"),
        C("C37", "4.7n", "USB_SHIELD", "GND"),
        ("U7", "Power_Protection:USBLC6-2SC6", "USBLC6-2SC6", "Package_TO_SOT_SMD:SOT-23-6",
         {"1": "USB_DM", "6": "USB_DM", "3": "USB_DP", "4": "USB_DP", "5": "5V_USB", "2": "GND"}, "USB ESD; D+ has the MCU's own pull-up"),
        ("U8", "Interface_UART:MAX3490xSA", "SN65HVD30DR", "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm",
         {"1": "3V3", "2": "SS_RX", "3": "SS_TX", "4": "GND", "5": "SS_TX+", "6": "SS_TX-", "7": "SS_RX-", "8": "SS_RX+"},
         "variant A ✱ (the MAX3490's pinout, as the SN65HVD30's)"),
        C("C38", "100n", "3V3", "GND", note="U8"),
        R("R20", "120", "SS_RX+", "SS_RX-", FP_R, "RX termination (variant A)"),
        ("J4", "Connector_Generic_MountingPin:Conn_01x06_MountingPin", "SMART-SERIAL", "Connector_JST:JST_GH_SM06B-GHS-TB_1x06-1MP_P1.25mm_Horizontal",
         {"1": "SS_TX+", "2": "SS_TX-", "3": "SS_RX+", "4": "SS_RX-", "5": "5V_MESA", "6": "GND", "MP": "GND"}, "variant A; to the Mesa card's RJ45 ✱"),
        ("U9", "Interface_CAN_LIN:TJA1042T-3", "TCAN1044VDR", "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm",
         {"1": "CAN_TX", "2": "GND", "3": "5V", "4": "CAN_RX", "5": "3V3", "6": "CANL", "7": "CANH", "8": "CAN_STB"},
         "variant B ✱ (the TJA1042T-3's pinout, as the TCAN1044V's)"),
        C("C39", "100n", "5V", "GND", note="U9 VCC"),
        C("C40", "100n", "3V3", "GND", note="U9 VIO"),
        ("JP1", "Jumper:SolderJumper_2_Open", "TERM", "Jumper:SolderJumper-2_P1.3mm_Open_RoundedPad1.0x1.5mm", {"1": "CANH", "2": "CAN_TERM"}, "fit at the bus ends"),
        R("R21", "120", "CAN_TERM", "CANL"),
        ("J5", "Connector_Generic_MountingPin:Conn_01x04_MountingPin", "CAN", "Connector_JST:JST_GH_SM04B-GHS-TB_1x04-1MP_P1.25mm_Horizontal",
         {"1": "CANH", "2": "CANL", "3": "VM", "4": "GND", "MP": "GND"}, "variant B"),
        ("J6", "Connector_Generic_MountingPin:Conn_01x04_MountingPin", "CAN", "Connector_JST:JST_GH_SM04B-GHS-TB_1x04-1MP_P1.25mm_Horizontal",
         {"1": "CANH", "2": "CANL", "3": "VM", "4": "GND", "MP": "GND"}, "variant B, daisy chain"),
        ("H1", "Mechanical:MountingHole", "M2.5", "MountingHole:MountingHole_2.7mm_M2.5", {}, "NEMA11: 23 mm square"),
        ("H2", "Mechanical:MountingHole", "M2.5", "MountingHole:MountingHole_2.7mm_M2.5", {}, ""),
        ("H3", "Mechanical:MountingHole", "M2.5", "MountingHole:MountingHole_2.7mm_M2.5", {}, ""),
        ("H4", "Mechanical:MountingHole", "M2.5", "MountingHole:MountingHole_2.7mm_M2.5", {}, ""),
    ],
}

TITLES = {"power": "Power: motor supply, buck 5 V, 5 V OR, LDO 3.3 V, VDDA, VM sense",
          "mcu": "MCU STM32G473CEU6: clock, reset, SWD, LED, HWID, NTC, spare",
          "bridges": "H-bridges: DRV8876 per coil, current sense, motor connector",
          "encoder": "Encoder AS5047P, enable input",
          "interfaces": "USB-C, smart-serial (variant A), CAN (variant B), mounting"}

# nets with no power output on them: ERC needs a PWR_FLAG (the supply comes from a connector or a passive part)
PWR_FLAGS = ["VM", "GND", "5V", "VDDA", "5V_USB"]


def own_symbols(k):
    """The project's own symbols (k: kigen.sch, its helpers)."""
    import os
    from sexp import Sym, parse, find, findall
    drv = k.box_symbol(
        "DRV8876RGT", "DRV8876RGTR", "Package_DFN_QFN:VQFN-16-1EP_3x3mm_P0.5mm_EP1.68x1.68mm_ThermalVias",
        "https://www.ti.com/lit/ds/symlink/drv8876.pdf",
        "DRV8876 brushed DC / H-bridge, 37 V 3.5 A, current mirror (IPROPI), VQFN-16 (RGT). Pinout per SLVSDS7B.",
        [("15", "EN/IN1", "input"), ("16", "PH/IN2", "input"), ("1", "nSLEEP", "input"), ("14", "PMODE", "input"),
         ("5", "IMODE", "input"), ("3", "VREF", "input"), ("4", "IPROPI", "output"), ("2", "nFAULT", "open_collector"),
         None, ("13", "GND", "power_in"), ("17", "PAD", "power_in")],
        [("9", "VM", "power_in"), ("10", "VCP", "passive"), ("11", "CPH", "passive"), ("12", "CPL", "passive"), None,
         ("6", "OUT1", "output"), ("8", "OUT2", "output"), None, None, None, ("7", "PGND", "power_in")], w=17.78)
    buck = k.box_symbol(
        "TPS560430", "TPS560430XDBVR", "Package_TO_SOT_SMD:SOT-23-6",
        "https://www.ti.com/lit/ds/symlink/tps560430.pdf",
        "TPS560430 4-36 V 600 mA synchronous buck, SOT-23-6 (DBV). VREF 1.0 V.",
        [("5", "VIN", "power_in"), ("4", "EN", "input"), None, ("2", "GND", "power_in")],
        [("1", "CB", "passive"), ("6", "SW", "output"), None, ("3", "FB", "input")])
    # the AS5047P is pin-compatible with KiCad's AS5047D; its VDD3V3 is an
    # output only in 5 V mode, here it is tied to the 3V3 rail (an input)
    lib = parse(open(os.path.join(k.KICAD_SYMBOLS, "Sensor_Magnetic.kicad_sym")).read())
    enc = k.rename(next(s for s in findall(lib, "symbol") if s[1] == "AS5047D"), "AS5047D", "AS5047P")
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
