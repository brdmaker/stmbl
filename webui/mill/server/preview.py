"""The toolpath of a G-code file, from LinuxCNC's own interpreter (the
`gcode` module, as AXIS and QtVCP preview), without OpenGL.

parse(path, ...) -> {"segs": [[kind, x0, y0, z0, x, y, z, feed, line, tool], ...],
                     "lines": n, "error": None or text}

kind: 0 rapid, 1 feed (arcs come as short feed segments), 2 probe, 3 tool
change (where it happens, the new tool). Positions [mm] in the coordinates
of the work offset active when the program starts (`active`: the machine's
G5x + G92 then, taken out again), feed [mm/min].
"""
import os
import shutil
import tempfile

try:
    import gcode
    from rs274.interpret import Translated, ArcsToSegmentsMixin
except ImportError:  # outside LinuxCNC: parse() says so
    gcode = None

    class Translated:  # stand-ins: parse() reports that the preview needs LinuxCNC
        pass

    class ArcsToSegmentsMixin:
        pass

MM = 25.4  # the canon works in inches


class Canon(Translated, ArcsToSegmentsMixin):
    def __init__(self, tools=None, parameter_file="", max_segs=400000, pockets=None):
        self.parameter_file = parameter_file
        self.segs = []
        self.lineno = 0
        self.feedrate = 0.0
        self.tool = 0
        self.lo = (0.0,) * 9
        self.first = True
        self.max_segs = max_segs
        self.tools = tools or {}
        self.messages = []
        self.state = None
        self.arcdivision = 32
        self.pockets = pockets or []  # the tool table's rows in order: pocket i -> tool number
        self.pending_tc = []          # tool changes before the first move: placed there

    # state the interpreter reports
    def next_line(self, st):
        self.state = st
        self.lineno = st.sequence_number

    def set_feed_rate(self, rate):
        self.feedrate = rate * MM

    def set_spindle_rate(self, *a): pass
    def set_traverse_rate(self, *a): pass
    def select_plane(self, *a): pass
    def comment(self, text): pass
    def tool_offset(self, *a): pass
    def user_defined_function(self, *a): pass
    def dwell(self, *a): pass

    def message(self, text):
        self.messages.append(text)

    def check_abort(self):
        return len(self.segs) > self.max_segs

    def change_tool(self, pocket):
        # the interpreter names the tool table's row (its "pocket"), from 1
        t = self.pockets[pocket - 1] if 0 < pocket <= len(self.pockets) else pocket
        self.tool = t
        x, y, z = (v * MM for v in self.lo[:3])
        self.segs.append([3, x, y, z, x, y, z, 0.0, self.lineno, t])
        if self.first:
            self.pending_tc.append(self.segs[-1])

    def get_tool(self, n):
        return self.tools.get(n, (n,) + (0.0,) * 12 + (0,))

    def get_external_angular_units(self): return 1.0
    def get_external_length_units(self): return 1.0  # an mm machine (stat.linear_units)
    def get_axis_mask(self): return 7
    def get_block_delete(self): return False

    # motion: positions arrive translated (machine coordinates, inches)
    def _seg(self, kind, l):
        if not self.first and l[:3] != self.lo[:3]:
            self.segs.append([kind, self.lo[0] * MM, self.lo[1] * MM, self.lo[2] * MM,
                              l[0] * MM, l[1] * MM, l[2] * MM, self.feedrate if kind else 0.0, self.lineno, self.tool])
        if self.first:
            for tc in self.pending_tc:  # where the tool is when the program starts moving
                tc[1:7] = [l[0] * MM, l[1] * MM, l[2] * MM] * 2
            self.pending_tc = []
        self.first = False
        self.lo = tuple(l)

    def straight_traverse_translated(self, *l): self._seg(0, l)
    def straight_feed_translated(self, *l): self._seg(1, l)

    def straight_traverse(self, *a): self._seg(0, self.rotate_and_translate(*a))
    def straight_feed(self, *a): self._seg(1, self.rotate_and_translate(*a))
    def straight_probe(self, *a): self._seg(2, self.rotate_and_translate(*a))

    def rigid_tap(self, x, y, z):
        lo = self.lo
        self._seg(1, self.rotate_and_translate(x, y, z, 0, 0, 0, 0, 0, 0)[:3] + lo[3:])
        self._seg(1, lo)

    def straight_arcsegments(self, segs):
        for l in segs:
            self._seg(1, l)


def parse(path, tools=None, parameter_file=None, unitcode="G21", initcode="", active=(0.0, 0.0, 0.0), pockets=None):
    out = {"segs": [], "lines": 0, "error": None}
    try:
        with open(path, errors="replace") as f:
            out["lines"] = sum(1 for _ in f)
    except OSError as e:
        out["error"] = str(e)
        return out
    if gcode is None:
        out["error"] = "the preview needs LinuxCNC's python (the gcode module)"
        return out
    td = tempfile.mkdtemp(prefix="millweb-preview.")
    try:
        par = os.path.join(td, os.path.basename(parameter_file or "linuxcnc.var"))
        if parameter_file and os.path.exists(parameter_file):
            shutil.copy(parameter_file, par)
        else:
            open(par, "w").close()  # the interpreter writes it back at the end
        canon = Canon(tools, par, pockets=pockets)
        result, seq = gcode.parse(path, canon, unitcode, initcode)
        if result > gcode.MIN_ERROR:
            out["error"] = "line %d: %s" % (seq, gcode.strerror(result))
    finally:
        shutil.rmtree(td, ignore_errors=True)
    ax, ay, az = active
    out["segs"] = [[s[0], round(s[1] - ax, 4), round(s[2] - ay, 4), round(s[3] - az, 4),
                    round(s[4] - ax, 4), round(s[5] - ay, 4), round(s[6] - az, 4), round(s[7], 1), s[8], s[9]]
                   for s in canon.segs]
    return out
