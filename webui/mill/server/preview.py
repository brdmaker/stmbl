"""The toolpath of a G-code file, from LinuxCNC's own interpreter (the
`gcode` module, as AXIS and QtVCP preview), without OpenGL.

parse(path, ...) -> {"segs": [[kind, x0, y0, z0, x, y, z, feed, line, tool], ...],
                     "stats": {...}, "lines": n, "error": None or text, "truncated": None or text}

kind: 0 rapid, 1 feed (arcs come as short feed segments), 2 probe, 3 tool
change (where it happens, the new tool). Positions [mm] in the coordinates
of the work offset active when the program starts (`active`: the machine's
G5x + G92 then, taken out again), feed [mm/min].

The whole program is interpreted; `stats` (travel, lengths, time, tools)
cover every move. The path to draw has at most `max_segs` segments: past
that, neighbouring moves of the same kind are merged (every 2nd, 4th, ...
point kept), so a program of millions of moves still draws as its shape.
`timeout` [s] bounds the interpreting; then `truncated` says so and the
stats cover what was read.

millweb runs it in a child process (a crash or a long program doesn't stop
the server): `python3 preview.py` with a JSON request on stdin writes one
line of JSON (everything but the path), then the path as a JSON array of
[kind, x, y, z, feed, line, tool] per segment, each starting where the one
before it ended; "start" in the first line is where the first one starts.
"""
import json
import math
import os
import shutil
import sys
import tempfile
import time

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
    def __init__(self, tools=None, parameter_file="", max_segs=100000, pockets=None, deadline=None,
                 rapid=6000.0, acc=1000.0):
        self.parameter_file = parameter_file
        self.segs = []                # the path to draw (merged past max_segs)
        self.lineno = 0
        self.feedrate = 0.0
        self.tool = 0
        self.lo = (0.0,) * 9
        self.first = True
        self.max_segs = max(1000, int(max_segs))
        self.stride = 1               # moves per drawn segment
        self.acc_n = 0                # moves merged into the last segment so far
        self.deadline = deadline
        self.timed_out = False
        self.tools = tools or {}
        self.messages = []
        self.state = None
        self.arcdivision = 32
        self.pockets = pockets or []  # the tool table's rows in order: pocket i -> tool number
        self.pending_tc = []          # tool changes before the first move: placed there
        # over every move
        self.n = 0
        self.lo_ext = [math.inf] * 3
        self.hi_ext = [-math.inf] * 3
        self.cut = self.rapid_len = self.time = 0.0
        self.used = []
        self.rapid, self.acc = float(rapid), float(acc)

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
        if self.deadline is not None and time.time() > self.deadline:
            self.timed_out = True
            return True
        return False

    def change_tool(self, pocket):
        # the interpreter names the tool table's row (its "pocket"), from 1
        t = self.pockets[pocket - 1] if 0 < pocket <= len(self.pockets) else pocket
        self.tool = t
        if t not in self.used:
            self.used.append(t)
        x, y, z = (v * MM for v in self.lo[:3])
        self.segs.append([3, x, y, z, x, y, z, 0.0, self.lineno, t])
        self.acc_n = 0
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
            x0, y0, z0 = self.lo[0] * MM, self.lo[1] * MM, self.lo[2] * MM
            x, y, z = l[0] * MM, l[1] * MM, l[2] * MM
            self._account(kind, x0, y0, z0, x, y, z)
            f = self.feedrate if kind else 0.0
            last = self.segs[-1] if self.segs else None
            if last is not None and self.acc_n and self.acc_n < self.stride and last[0] in (0, 1) and kind in (0, 1) \
                    and last[9] == self.tool:
                last[4:9] = [x, y, z, max(f, last[7]), self.lineno]  # merged: the same start, this end
                last[0] = last[0] or kind  # a cut if any of it cut
                self.acc_n += 1
            else:
                self.segs.append([kind, x0, y0, z0, x, y, z, f, self.lineno, self.tool])
                self.acc_n = 1
                if len(self.segs) > self.max_segs:
                    self._halve()
        if self.first:
            for tc in self.pending_tc:  # where the tool is when the program starts moving
                tc[1:7] = [l[0] * MM, l[1] * MM, l[2] * MM] * 2
            self.pending_tc = []
        self.first = False
        self.lo = tuple(l)

    def _account(self, kind, x0, y0, z0, x, y, z):
        self.n += 1
        for i, v in enumerate((x, y, z)):
            if v < self.lo_ext[i]:
                self.lo_ext[i] = v
            if v > self.hi_ext[i]:
                self.hi_ext[i] = v
        if self.n == 1:
            for i, v in enumerate((x0, y0, z0)):
                self.lo_ext[i] = min(self.lo_ext[i], v)
                self.hi_ext[i] = max(self.hi_ext[i], v)
        d = math.sqrt((x - x0) ** 2 + (y - y0) ** 2 + (z - z0) ** 2)
        f = (self.rapid if kind == 0 else max(self.feedrate, 1e-6)) / 60.0
        self.time += d / f + min(f / self.acc, math.sqrt(d / self.acc))  # as the console's planner
        if kind == 0:
            self.rapid_len += d
        else:
            self.cut += d

    def _halve(self):
        """Merge neighbouring moves (rapid or feed, the same tool; drawn as a
        cut if either cut): about half as many. Tool changes and probe moves
        stay as they are."""
        out, s = [], self.segs
        i = 0
        while i < len(s):
            a = s[i]
            b = s[i + 1] if i + 1 < len(s) else None
            if b is not None and a[0] in (0, 1) and b[0] in (0, 1) and a[9] == b[9]:
                out.append([a[0] or b[0], a[1], a[2], a[3], b[4], b[5], b[6], max(a[7], b[7]), b[8], a[9]])
                i += 2
            else:
                out.append(a)
                i += 1
        if len(out) > 0.8 * len(s):  # hardly anything to merge (tool changes, probing): stop trying so often
            self.max_segs = int(len(out) * 1.5)
        self.segs = out
        self.stride *= 2
        self.acc_n = self.stride  # the next move starts a new segment

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


def parse(path, tools=None, parameter_file=None, unitcode="G21", initcode="", active=(0.0, 0.0, 0.0), pockets=None,
          max_segs=100000, timeout=120.0, rapid=6000.0, acc=1000.0):
    out = {"segs": [], "lines": 0, "error": None, "truncated": None, "stats": None}
    try:
        with open(path, "rb") as f:
            out["lines"] = sum(1 for _ in f)
    except OSError as e:
        out["error"] = str(e)
        return out
    if gcode is None:
        out["error"] = "the preview needs LinuxCNC's python (the gcode module)"
        return out
    td = tempfile.mkdtemp(prefix="millweb-preview.")
    canon = Canon(tools, "", max_segs=max_segs, pockets=pockets, deadline=time.time() + timeout if timeout else None,
                  rapid=rapid, acc=acc)
    try:
        par = os.path.join(td, os.path.basename(parameter_file or "linuxcnc.var"))
        if parameter_file and os.path.exists(parameter_file):
            shutil.copy(parameter_file, par)
        else:
            open(par, "w").close()  # the interpreter writes it back at the end
        canon.parameter_file = par
        try:
            result, seq = gcode.parse(path, canon, unitcode, initcode)
            if result > gcode.MIN_ERROR:
                out["error"] = "line %d: %s" % (seq, gcode.strerror(result))
        except KeyboardInterrupt:  # how gcode.parse stops when check_abort says so
            if not canon.timed_out:
                raise
            out["truncated"] = "interpreting stopped after %.0f s at line %d" % (timeout, canon.lineno)
    finally:
        shutil.rmtree(td, ignore_errors=True)
    ax, ay, az = active
    r = lambda v: round(v, 3)  # noqa: E731
    out["segs"] = [[s[0], r(s[1] - ax), r(s[2] - ay), r(s[3] - az), r(s[4] - ax), r(s[5] - ay), r(s[6] - az),
                    round(s[7], 1), s[8], s[9]] for s in canon.segs]
    have = canon.n > 0
    out["stats"] = {
        "moves": canon.n, "shown": len([s for s in canon.segs if s[0] != 3]), "stride": canon.stride,
        "ext": {a: ([r(canon.lo_ext[i] - active[i]), r(canon.hi_ext[i] - active[i])] if have else [0, 0])
                for i, a in enumerate("xyz")},
        "cut": round(canon.cut, 1), "rapid": round(canon.rapid_len, 1), "time": round(canon.time, 1), "tools": canon.used,
    }
    return out


def main():
    """The child process: a JSON request on stdin; the header line, then the path."""
    req = json.load(sys.stdin)
    try:  # the interpreter reads tool data from LinuxCNC's shared memory: stat().poll() maps it (DEBUGNOTES)
        import linuxcnc
        keep = linuxcnc.stat()
        keep.poll()
    except Exception:
        pass
    tools = {int(k): tuple(v) for k, v in (req.get("tools") or {}).items()}
    t0 = time.time()
    pv = parse(req["path"], tools, req.get("var"), req.get("unitcode", "G21"), req.get("initcode", ""),
               tuple(req.get("active", (0, 0, 0))), req.get("pockets"), req.get("max_segs", 100000),
               req.get("timeout", 120.0), req.get("rapid", 6000.0), req.get("acc", 1000.0))
    segs = pv.pop("segs")
    pv["start"] = segs[0][1:4] if segs else [0, 0, 0]
    pv["seconds"] = round(time.time() - t0, 2)
    w = sys.stdout.write
    w(json.dumps(pv, separators=(",", ":")) + "\n")
    w("[")
    for i in range(0, len(segs), 5000):  # in pieces: one string of the whole path would double the memory
        part = [v for s in segs[i:i + 5000] for v in (s[0], s[4], s[5], s[6], s[7], s[8], s[9])]
        w(("," if i else "") + json.dumps(part, separators=(",", ":"))[1:-1])
    w("]\n")
    sys.stdout.flush()


if __name__ == "__main__":
    main()
