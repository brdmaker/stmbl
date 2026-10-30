"""brdstepper's board for hardware/kigen (place.py): 28 x 28 mm, four layers,
M2.5 holes on the NEMA11's 23 mm square, U4 (the encoder) on the bottom,
centred on the shaft. The edge connectors and the encoder go where
docs/brdstepper_hw.md says; the rest is placed around them.

variant("A") (smart-serial) and variant("B") (CAN) give the attributes
kigen reads from each board's gen/board.py.
"""
OUTLINE = ("rect", 14.0)       # 28 x 28 mm
HOLE = 11.5                    # M2.5 at 23 mm
LAYERS = 4
RULES = {"m_MinClearance": 0.127, "m_TrackMinWidth": 0.127, "m_ViasMinSize": 0.45, "m_MinThroughDrill": 0.2,
         "m_CopperEdgeClearance": 0.2, "m_HoleToHoleMin": 0.25,
         "m_HoleClearance": 0.15}  # the USB4105 footprint has 0.194 mm between its own hole and pad
HALF = OUTLINE[1]

# where the edge connectors and the encoder go: (side, rotation, x, y) of the
# footprint origin. Rotation 0 has the mating face of J1 and the GH connectors
# toward +y; their front is put on the board edge.
FIXED = {
    "H1": ("F", 0, -HOLE, -HOLE), "H2": ("F", 0, HOLE, -HOLE),
    "H3": ("F", 0, -HOLE, HOLE), "H4": ("F", 0, HOLE, HOLE),
    "U4": ("B", 90, 0.0, 0.0),                                 # on the shaft axis (turned: J2's pins pass beside it)
    "J2": ("F", 90, -4.3, 3.0),                                # supply, beside the shaft: DC only (see README)
    "J1": ("F", 180, 0.0, -HALF + 3.1),                        # USB-C, north edge ("PCB Edge" at +3.1)
}
FIXED_A = {"J4": ("F", 90, HALF - 3.2, 0.0)}                   # smart-serial, east edge
FIXED_B = {"J5": ("F", 270, -HALF + 3.2, 0.0),                 # CAN, west edge
           "J6": ("F", 0, 0.0, HALF - 3.2)}                    # CAN, south edge
OVERHANG = {"J1": 1.2, "J4": 0.05, "J5": 0.05, "J6": 0.05, "H1": 0.5, "H2": 0.5, "H3": 0.5, "H4": 0.5}  # mm a courtyard may stand past the edge

# parts that stay on top: the bottom faces the motor, a few mm away
TOP_ONLY = {"L1", "C1", "C2", "D2", "J2", "J3"}
BOTTOM_FIRST = {"U1", "U8", "U9"}  # thin, leaving the top to what must be there
EARLY = {"U2", "U3"}             # the bridges: kept off the centre, and their thermal pads reach both sides
# parts carrying the switched coil current stay out of a square around the
# shaft (§8: their field reaches the sensor); the supply (DC) parts may come
# closer, but not over the sensor itself
KEEPOUT = {r: 5.5 for r in ("U2", "U3", "C23", "C24", "C27", "C28")}
KEEPOUT["J3"] = 4.0            # the motor-lead pads
KEEPOUT.update({r: 3.0 for r in ("L1", "U6", "C1", "C2", "C3", "C4", "C5", "D2")})

# a few parts start from a chosen spot instead of the centre of their nets
ANCHOR = {"U1": (0.0, -7.0), "J3": (6.0, 2.0), "U2": (6.0, -4.0), "U3": (6.0, 4.0),
          "U8": (7.0, -6.0), "U9": (-7.0, 0.0), "J7": (-4.0, -6.0), "L1": (-8.0, 4.0), "U6": (-8.0, 8.0)}
BIG_NETS = {"GND", "3V3", "VM", "5V", "VDDA"}
PLANES = [("GND", "In1.Cu")]
PAD_NOPIN = {"U1": ("49",), "U2": ("17",), "U3": ("17",)}  # thermal-via pads carry the exposed pad's number


# the loosest placement: place.py tries gaps between courtyards from GAP_MAX down
# and keeps the first that leaves no part without room
GAP_MAX = 2.0
SPREAD = True                 # then push the parts apart into whatever room is left


def draw(board, h):
    """The shaft and the magnet under U4."""
    h.circle(board, 3.0, h.pcbnew.B_Fab)
    h.circle(board, 0.6, h.pcbnew.Dwgs_User)


def variant(v):
    """The attributes kigen reads from gen/board.py, for variant v."""
    assert v in ("A", "B"), v
    return {
        "OUTLINE": OUTLINE, "HOLE": HOLE, "LAYERS": LAYERS, "RULES": RULES,
        "TITLE": "closed-loop NEMA11 stepper drive, %s" % ("smart-serial (Mesa)" if v == "A" else "CAN"),
        "FIXED": dict(FIXED, **(FIXED_A if v == "A" else FIXED_B)),
        "OVERHANG": OVERHANG, "TOP_ONLY": TOP_ONLY, "BOTTOM_FIRST": BOTTOM_FIRST, "EARLY": EARLY,
        "KEEPOUT": KEEPOUT, "ANCHOR": ANCHOR, "BIG_NETS": BIG_NETS, "PLANES": PLANES, "PAD_NOPIN": PAD_NOPIN,
        "GAP_MAX": GAP_MAX, "SPREAD": SPREAD, "draw": draw,
    }
