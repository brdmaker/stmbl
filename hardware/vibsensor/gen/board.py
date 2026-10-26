"""vibsensor's board for hardware/kigen (place.py): 32 x 18 mm, chamfered
to the enclosure's "shard" shape (hardware/vibsensor/enclosure), four
layers, potted.

The bottom faces the enclosure's solid floor, which is bolted to the
machine: the IIS3DWB goes there at the centre, over the M6 stud hole, and the
TMP117 beside it. The board sits on two bosses of the floor (M2 holes H1,
H2 on the long axis). The M8 socket is in the tail (-x): its leads come to
J1's pads at that end. +x, the nose, is the sensor's X axis.
"""
L, W = 32.0, 18.0                      # length (x), width (y)
NOSE, TAIL = 4.0, 2.5                  # chamfers
OUTLINE = ("poly", [(-L / 2 + TAIL, -W / 2), (L / 2 - NOSE, -W / 2), (L / 2, -W / 2 + NOSE), (L / 2, W / 2 - NOSE),
                    (L / 2 - NOSE, W / 2), (-L / 2 + TAIL, W / 2), (-L / 2, W / 2 - TAIL), (-L / 2, -W / 2 + TAIL)])
LAYERS = 4
TITLE = "vibration and temperature sensor, CAN FD"
RULES = {"m_MinClearance": 0.127, "m_TrackMinWidth": 0.127, "m_ViasMinSize": 0.45, "m_MinThroughDrill": 0.2,
         "m_CopperEdgeClearance": 0.25, "m_HoleToHoleMin": 0.25, "m_HoleClearance": 0.2}
HOLE_X = 9.0                           # the bosses' M2 holes at +-9 mm on the long axis

FIXED = {
    "U4": ("B", 0, 0.0, 0.0),           # the accelerometer on the stud's axis
    "H1": ("F", 0, -HOLE_X, 0.0),
    "H2": ("F", 0, HOLE_X, 0.0),
    "J1": ("F", 0, -L / 2 + 1.8, -3.0),  # the M8 leads' pads, a column at the tail
    "U1": ("F", 0, 0.0, 0.0),           # the MCU over the accelerometer, between the bosses
}
OVERHANG = {}
TOP_ONLY = {"L1", "C1", "D2", "J2"}     # the tall ones away from the base; SWD reachable from the top
BOTTOM_FIRST = {"U5"}                   # the thermometer against the base
EARLY = {"U6"}                          # the large ones before the passives
ANCHOR = {"U5": (4.0, 0.0), "U6": (-7.0, 0.0), "U2": (-8.0, -4.0), "L1": (-7.0, 4.0), "J2": (4.0, 6.5)}
BIG_NETS = {"GND", "3V3", "5V", "VIN"}
PLANES = [("GND", "In1.Cu")]
PAD_NOPIN = {}


def draw(board, h):
    """The floor's M6 stud hole under the board's centre."""
    h.circle(board, 3.0, h.pcbnew.B_Fab)
