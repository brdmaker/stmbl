"""vibsensor's board for hardware/kigen (place.py): round, 26 mm, four
layers, potted in the sensor's aluminium housing (docs/vibsensor.md §1).

The bottom faces the housing's base, which is screwed to the machine: the
IIS3DWB goes there at the centre, over the mounting stud, and the TMP117
beside it, against the base. The M8 socket's leads come in from the top
(J1's pads); two M2 holes hold the board to the housing before potting.
"""
OUTLINE = ("circle", 13.0)     # 26 mm across
LAYERS = 4
TITLE = "vibration and temperature sensor, CAN FD"
RULES = {"m_MinClearance": 0.127, "m_TrackMinWidth": 0.127, "m_ViasMinSize": 0.45, "m_MinThroughDrill": 0.2,
         "m_CopperEdgeClearance": 0.25, "m_HoleToHoleMin": 0.25, "m_HoleClearance": 0.2}
R = OUTLINE[1]

FIXED = {
    "U4": ("B", 0, 0.0, 0.0),           # the accelerometer on the stud's axis
    "H1": ("F", 0, -(R - 2.6), 0.0),
    "H2": ("F", 0, R - 2.6, 0.0),
    "J1": ("F", 90, -3.0, -(R - 2.8)),  # the M8 leads' pads, a row along the top edge
}
OVERHANG = {"H1": 0.4, "H2": 0.4}
TOP_ONLY = {"L1", "C1", "D2", "J2"}     # the tall ones away from the base; SWD reachable from the top
BOTTOM_FIRST = {"U5"}                   # the thermometer against the base
EARLY = {"U1", "U6"}                    # the large ones before the passives
ANCHOR = {"U5": (0.0, 4.5), "U1": (0.0, 2.0), "U6": (0.0, -6.0), "U2": (-5.0, -4.0), "L1": (-6.0, 3.0), "J2": (5.0, 5.0)}
BIG_NETS = {"GND", "3V3", "5V", "VIN"}
PLANES = [("GND", "In1.Cu")]
PAD_NOPIN = {}


def draw(board, h):
    """The housing's M6 stud under the board's centre."""
    h.circle(board, 3.0, h.pcbnew.B_Fab)
