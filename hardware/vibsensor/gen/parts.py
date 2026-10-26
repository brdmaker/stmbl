"""vibsensor: every part and what each of its pins connects to, from
docs/vibsensor.md and the firmware's pin plan (vibsensor/inc/vs.h).
hardware/kigen makes the KiCad schematic and board of it (board.py: the
placement).

A part: (ref, lib_id, value, footprint, {pin: net}, notes). A pin left out
is not connected (a no-connect flag). ✱: check against its datasheet.
"""

PROJECT = "vibsensor"
TITLE = "vibration and temperature sensor, CAN FD"
DOC = "docs/vibsensor.md"
DATE = "2026-10-26"
REV = "0.1"
NOTE = ("vibsensor: STM32G473 + IIS3DWB + TMP117 + TCAN1044V on a round board, potted in the sensor's housing.\n"
        "Two M8 4-pin sockets (1 V+, 2 CAN_H, 3 GND, 4 CAN_L): IN (J1) and OUT (J3), the bus and supply passed through;\n"
        "the last sensor's OUT takes a terminator plug (120 ohm). Node number and name over the bus (no jumpers).\n"
        "Connections are by net name (net labels): docs/vibsensor.md and vibsensor/inc/vs.h are the reference.")
NETCLASS_POWER = ("VIN", "24V_IN", "5V", "SW")  # not GND: the IIS3DWB's pads are 0.15 mm apart
NOT_FITTED = {"H1", "H2", "J2"}  # holes; SWD pads

FP_R = "Resistor_SMD:R_0402_1005Metric"
FP_C = "Capacitor_SMD:C_0402_1005Metric"
FP_C0603 = "Capacitor_SMD:C_0603_1608Metric"
FP_C0805 = "Capacitor_SMD:C_0805_2012Metric"
FP_C1206 = "Capacitor_SMD:C_1206_3216Metric"


def R(ref, value, a, b, fp=FP_R, note=""):
    return (ref, "Device:R", value, fp, {"1": a, "2": b}, note)


def C(ref, value, a, b, fp=FP_C, note=""):
    return (ref, "Device:C", value, fp, {"1": a, "2": b}, note)


SHEETS = {
    # ------------------------------------------------------------ the bus's supply: 24 V -> 5 V -> 3.3 V
    "power": [
        ("J1", "Connector_Generic:Conn_01x04", "M8 IN", "Connector_PinHeader_2.00mm:PinHeader_1x04_P2.00mm_Vertical",
         {"1": "24V_IN", "2": "CANH", "3": "GND", "4": "CANL"}, "the IN socket's leads (M8 male), M8 order: 1 V+, 2 CAN_H, 3 GND, 4 CAN_L ✱"),
        ("J3", "Connector_Generic:Conn_01x04", "M8 OUT", "Connector_PinHeader_2.00mm:PinHeader_1x04_P2.00mm_Vertical",
         {"1": "24V_IN", "2": "CANH", "3": "GND", "4": "CANL"},
         "the OUT socket's leads (M8 female): the bus and the supply go on to the next sensor; the last one takes a terminator plug (120 ohm)"),
        ("D1", "Device:D_Schottky", "PMEG6010CEH", "Diode_SMD:D_SOD-123F", {"1": "VIN", "2": "24V_IN"}, "reverse polarity"),
        ("D2", "Diode:SMF33A", "SMF33A", "Diode_SMD:D_SMF", {"1": "VIN", "2": "GND"}, "TVS ✱ (pad 1 the cathode)"),
        C("C1", "4.7u 50V", "VIN", "GND", FP_C1206, "input"),
        C("C2", "100n 50V", "VIN", "GND", FP_C, "at the buck's VIN"),
        ("U2", "vibsensor:TPS560430", "TPS560430XDBVR", "Package_TO_SOT_SMD:SOT-23-6",
         {"1": "CB", "2": "GND", "3": "FB", "4": "VIN", "5": "VIN", "6": "SW"}, "buck to 5 V, as brdstepper's ✱"),
        C("C3", "100n", "CB", "SW", FP_C, "bootstrap"),
        ("L1", "Device:L", "10u", "Inductor_SMD:L_Taiyo-Yuden_NR-30xx", {"1": "SW", "2": "5V"}, "✱ NR3015, >= 0.3 A"),
        C("C4", "22u 10V", "5V", "GND", FP_C0805, "buck output"),
        R("R1", "88.7k 1%", "5V", "FB", note="5.02 V with 22.1k (VREF 1.00 V)"),
        R("R2", "22.1k 1%", "FB", "GND"),
        ("U3", "Regulator_Linear:TLV75533PDBV", "TLV75533PDBVR", "Package_TO_SOT_SMD:SOT-23-5",
         {"1": "5V", "2": "GND", "3": "5V", "5": "3V3"}, "3.3 V"),
        C("C5", "1u", "5V", "GND", note="LDO input"),
        C("C6", "1u", "3V3", "GND", note="LDO output"),
        ("FB1", "Device:FerriteBead_Small", "600R@100MHz", "Inductor_SMD:L_0402_1005Metric", {"1": "3V3", "2": "VDDA"}, "MCU analog"),
        C("C7", "1u", "VDDA", "GND"),
        C("C8", "10n", "VDDA", "GND"),
        ("FB2", "Device:FerriteBead_Small", "600R@100MHz", "Inductor_SMD:L_0402_1005Metric", {"1": "3V3", "2": "3V3_ACC"}, "the accelerometer's own supply"),
        R("R3", "100k 1%", "VIN", "VIN_SENSE", note="bus voltage: 2.7 V at 30 V"),
        R("R4", "10k 1%", "VIN_SENSE", "GND"),
        C("C9", "10n", "VIN_SENSE", "GND"),
    ],
    # ------------------------------------------------------------ the MCU (vibsensor/inc/vs.h)
    "mcu": [
        ("U1", "MCU_ST_STM32G4:STM32G473CEUx", "STM32G473CEU6", "Package_DFN_QFN:QFN-48-1EP_7x7mm_P0.5mm_EP5.6x5.6mm",
         {"1": "3V3", "5": "OSC_IN", "6": "OSC_OUT", "7": "NRST", "8": "VIN_SENSE",
          "12": "ACC_CS", "13": "ACC_SCK", "14": "ACC_MISO", "15": "ACC_MOSI", "17": "ACC_INT1", "18": "ACC_INT2",
          "20": "VDDA", "21": "VDDA", "23": "3V3", "25": "LED", "26": "CAN_STB", "30": "I2C_SDA", "31": "I2C_SCL",
          "32": "TEMP_ALERT", "35": "3V3", "36": "SWDIO", "37": "SWCLK", "46": "CAN_RX", "47": "CAN_TX", "48": "3V3", "49": "GND"},
         "exposed pad: the only VSS / VSSA; vias to the GND plane. PA0 VIN_SENSE, PA4-7 SPI1, PB0/PB1 INT1/2, PA8/9 I2C2, PA10 ALERT, PB8/9 FDCAN1, PB12 LED, PB13 STB"),
        ("Y1", "Device:Crystal_GND24", "8MHz", "Crystal:Crystal_SMD_2016-4Pin_2.0x1.6mm", {"1": "OSC_IN", "3": "OSC_OUT", "2": "GND", "4": "GND"}, "✱ CL"),
        C("C10", "10p C0G", "OSC_IN", "GND", note="✱ per the crystal's CL"),
        C("C11", "10p C0G", "OSC_OUT", "GND"),
        C("C12", "100n", "NRST", "GND"),
        C("C13", "100n", "3V3", "GND", note="VDD (23)"),
        C("C14", "100n", "3V3", "GND", note="VDD (35)"),
        C("C15", "100n", "3V3", "GND", note="VDD (48)"),
        C("C16", "4.7u", "3V3", "GND", FP_C0603),
        R("R5", "10k", "CAN_RX", "GND", note="PB8-BOOT0 low until the option bytes are set"),
        R("R6", "1k", "LED", "LED_A"),
        ("D3", "Device:LED", "green", "LED_SMD:LED_0603_1608Metric", {"1": "GND", "2": "LED_A"}, "seen through the potting? ✱"),
        ("J2", "Connector:Conn_ARM_SWD_TagConnect_TC2030", "SWD", "Connector:Tag-Connect_TC2030-IDC-NL_2x03_P1.27mm_Vertical",
         {"1": "3V3", "2": "SWDIO", "3": "NRST", "4": "SWCLK", "5": "GND"}, "TC2030 pads, programmed before potting"),
    ],
    # ------------------------------------------------------------ the accelerometer and the thermometer
    "sensors": [
        ("U4", "vibsensor:IIS3DWB", "IIS3DWBTR", "Package_LGA:LGA-14_3x2.5mm_P0.5mm_LayoutBorder3x4y",
         {"1": "ACC_MISO", "2": "GND", "3": "GND", "4": "ACC_INT1", "5": "3V3_ACC", "6": "GND", "7": "GND", "8": "3V3_ACC",
          "9": "ACC_INT2", "12": "ACC_CS", "13": "ACC_SCK", "14": "ACC_MOSI"},
         "SPI mode 3; RES 2, 3 to GND, 10, 11 open (DS12569) ✱; near the mounting stud"),
        C("C17", "100n", "3V3_ACC", "GND", note="VDD (8) ✱"),
        C("C18", "100n", "3V3_ACC", "GND", note="VDD_IO (5)"),
        C("C19", "1u", "3V3_ACC", "GND"),
        ("U5", "vibsensor:TMP117", "TMP117AIDRVR", "Package_SON:WSON-6-1EP_2x2mm_P0.65mm_EP1x1.6mm",
         {"1": "I2C_SCL", "2": "GND", "3": "TEMP_ALERT", "4": "GND", "5": "3V3", "6": "I2C_SDA", "7": "GND"},
         "address 0x48 (ADD0 to GND); on the mounting face, thermally bonded ✱"),
        C("C20", "100n", "3V3", "GND", note="TMP117"),
        R("R7", "4.7k", "I2C_SDA", "3V3"),
        R("R8", "4.7k", "I2C_SCL", "3V3"),
        R("R9", "10k", "TEMP_ALERT", "3V3", note="ALERT is open drain"),
    ],
    # ------------------------------------------------------------ CAN FD
    "can": [
        ("U6", "Interface_CAN_LIN:TJA1042T-3", "TCAN1044VDR", "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm",
         {"1": "CAN_TX", "2": "GND", "3": "5V", "4": "CAN_RX", "5": "3V3", "6": "CANL", "7": "CANH", "8": "CAN_STB"},
         "CAN FD to 5 Mbit/s; the TJA1042T-3's pinout, as the TCAN1044V's ✱"),
        C("C21", "100n", "5V", "GND", note="VCC"),
        C("C22", "100n", "3V3", "GND", note="VIO"),
        ("H1", "Mechanical:MountingHole", "M2", "MountingHole:MountingHole_2.2mm_M2", {}, "to the housing"),
        ("H2", "Mechanical:MountingHole", "M2", "MountingHole:MountingHole_2.2mm_M2", {}, ""),
    ],
}

TITLES = {"power": "Supply: M8 IN / OUT (passed through), reverse polarity, TVS, buck 5 V, LDO 3.3 V, filtered rails, bus voltage",
          "mcu": "MCU STM32G473CEU6: clock, reset, LED, SWD",
          "sensors": "IIS3DWB accelerometer (SPI1), TMP117 thermometer (I2C2)",
          "can": "CAN FD transceiver, mounting (termination: a plug in the last sensor's OUT)"}

# nets with no power output on them: ERC needs a PWR_FLAG
PWR_FLAGS = ["VIN", "GND", "5V", "VDDA", "3V3_ACC"]


def own_symbols(k):
    """The project's own symbols (k: kigen.sch, its helpers)."""
    buck = k.box_symbol(
        "TPS560430", "TPS560430XDBVR", "Package_TO_SOT_SMD:SOT-23-6",
        "https://www.ti.com/lit/ds/symlink/tps560430.pdf",
        "TPS560430 4-36 V 600 mA synchronous buck, SOT-23-6 (DBV). VREF 1.0 V.",
        [("5", "VIN", "power_in"), ("4", "EN", "input"), None, ("2", "GND", "power_in")],
        [("1", "CB", "passive"), ("6", "SW", "output"), None, ("3", "FB", "input")])
    acc = k.box_symbol(
        "IIS3DWB", "IIS3DWBTR", "Package_LGA:LGA-14_3x2.5mm_P0.5mm_LayoutBorder3x4y",
        "https://www.st.com/resource/en/datasheet/iis3dwb.pdf",
        "IIS3DWB 3-axis vibration sensor, 26.7 kHz, +-2..16 g, SPI, LGA-14 2.5x3 mm. Pinout per DS12569.",
        [("12", "CS", "input"), ("13", "SPC", "input"), ("14", "SDI", "input"), ("1", "SDO", "output"), None,
         ("4", "INT1", "output"), ("9", "INT2", "output"), None, ("2", "RES", "passive"), ("3", "RES", "passive"),
         ("10", "RES", "passive"), ("11", "RES", "passive")],
        [("8", "VDD", "power_in"), ("5", "VDD_IO", "power_in"), None, None, None, None, None, None, None, None,
         ("6", "GND", "power_in"), ("7", "GND", "power_in")])
    tmp = k.box_symbol(
        "TMP117", "TMP117AIDRVR", "Package_SON:WSON-6-1EP_2x2mm_P0.65mm_EP1x1.6mm",
        "https://www.ti.com/lit/ds/symlink/tmp117.pdf",
        "TMP117 +-0.1 degC digital temperature sensor, I2C, WSON-6 (DRV). Pinout per SNOSD82.",
        [("6", "SDA", "bidirectional"), ("1", "SCL", "input"), ("3", "ALERT", "open_collector"), ("4", "ADD0", "input")],
        [("5", "V+", "power_in"), None, ("2", "GND", "power_in"), ("7", "EP", "passive")])
    return [buck, acc, tmp]
