"""Planes for kigen route (BrdRoute knows plane nets but not plane layers): a board with a
GND plane on In1.Cu is routed as a three-layer board (F, In2, B), and the
plane's net is connected beforehand:

  fanout()      every SMD pad of the net gets a short stub and a via down to
                the plane, and a big pad (the MCU's exposed pad) a grid of vias
  prepare_dsn() the router's view of the board: the plane layer hidden (the
                (plane NET ...) statement stays: BrdRoute then has nothing to
                connect on that net)

The vias and stubs go into the board first, so the router sees them as copper
(KiCad's DSN export writes them as wiring) and routes around them.
"""
import math

import pcbnew

VIA, DRILL = 0.6, 0.3         # mm
STUB = 0.25                   # stub width
CLEARANCE = 0.15
EDGE = 0.2                    # copper to board edge
HOLE_PITCH = 0.75             # via to via, centres
RINGS = [0.35 + 0.1 * i for i in range(40)]


def mm(v):
    return pcbnew.FromMM(v)


def pt(x, y):
    return pcbnew.VECTOR2I(int(mm(x)), int(mm(y)))


def _add_via(board, net, x, y):
    v = pcbnew.PCB_VIA(board)
    v.SetViaType(pcbnew.VIATYPE_THROUGH)
    v.SetPosition(pt(x, y))
    v.SetWidth(mm(VIA))
    v.SetDrill(mm(DRILL))
    v.SetNet(net)
    board.Add(v)


def _add_stub(board, net, layer, a, b):
    t = pcbnew.PCB_TRACK(board)
    t.SetStart(pt(*a))
    t.SetEnd(pt(*b))
    t.SetLayer(layer)
    t.SetWidth(mm(STUB))
    t.SetNet(net)
    board.Add(t)


def fanout(board, net_name, half, centre=(100.0, 100.0)):
    """Add the stubs and vias of net_name: (placed, [pads without room])."""
    net = board.FindNet(net_name)
    pads = list(board.GetPads())
    cx, cy = centre
    lim = half - EDGE
    vias = []                  # (x, y) of vias so far
    placed, failed = 0, []
    info = []
    for p in pads:
        pos = p.GetPosition()
        info.append((p, pos.x / 1e6, pos.y / 1e6))

    def near(x, y, r):
        return [q for q, qx, qy in info if abs(qx - x) < r + 4 and abs(qy - y) < r + 4]

    def via_ok(x, y, own):
        if abs(x - cx) > lim - VIA / 2 or abs(y - cy) > lim - VIA / 2:
            return False
        for vx, vy in vias:
            if math.hypot(vx - x, vy - y) < HOLE_PITCH:
                return False
        for q in near(x, y, VIA):
            if q.HitTest(pt(x, y), int(mm(VIA / 2 + CLEARANCE))):
                return False
        return True

    def stub_ok(layer, a, b, own):
        n = max(2, int(math.hypot(b[0] - a[0], b[1] - a[1]) / 0.05))
        for i in range(n + 1):
            x = a[0] + (b[0] - a[0]) * i / n
            y = a[1] + (b[1] - a[1]) * i / n
            if abs(x - cx) > lim - STUB / 2 or abs(y - cy) > lim - STUB / 2:
                return False
            for q in near(x, y, 0.5):
                if q is own or not q.IsOnLayer(layer) or (q.GetNet() is not None and q.GetNetname() == net_name):
                    continue
                if q.HitTest(pt(x, y), int(mm(STUB / 2 + CLEARANCE))):
                    return False
        return True

    targets = [(p, x, y) for p, x, y in info if p.GetNetname() == net_name and p.GetAttribute() == pcbnew.PAD_ATTRIB_SMD]
    through = [(p, x, y) for p, x, y in info if p.GetNetname() == net_name and p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH]
    for p, x, y in sorted(targets, key=lambda t: (t[0].GetParent().GetReference(), t[0].GetNumber())):
        w, h = p.GetSizeX() / 1e6, p.GetSizeY() / 1e6
        # connected already: a plated hole of the net sits in the pad (the thermal-via footprints)
        if any(q.GetParent() is p.GetParent() and p.HitTest(q.GetPosition()) for q, _, _ in through):
            continue
        layer = pcbnew.B_Cu if p.IsOnLayer(pcbnew.B_Cu) else pcbnew.F_Cu
        if min(w, h) >= 2.5:   # exposed pad: a grid of vias inside it
            nx, ny = int((w - 1.0) / 1.6) + 1, int((h - 1.0) / 1.6) + 1
            for i in range(nx):
                for j in range(ny):
                    vx = x + (i - (nx - 1) / 2) * 1.6
                    vy = y + (j - (ny - 1) / 2) * 1.6
                    _add_via(board, net, vx, vy)
                    vias.append((vx, vy))
                    placed += 1
            continue
        done = False
        for r in RINGS:
            cands = []
            for k in range(24):
                a = 2 * math.pi * k / 24
                vx, vy = round((x + r * math.cos(a)) * 20) / 20, round((y + r * math.sin(a)) * 20) / 20
                cands.append((vx, vy))
            for vx, vy in cands:
                if via_ok(vx, vy, p) and stub_ok(layer, (x, y), (vx, vy), p):
                    _add_stub(board, net, layer, (x, y), (vx, vy))
                    _add_via(board, net, vx, vy)
                    vias.append((vx, vy))
                    placed += 1
                    done = True
                    break
            if done:
                break
        if not done:
            failed.append("%s-%s" % (p.GetParent().GetReference(), p.GetNumber()))
    return placed, failed


def _balanced(text, start):
    """Index just past the parenthesised expression that opens at text[start]."""
    depth, i = 0, start
    while True:
        c = text[i]
        if c == '"':
            i = text.index('"', i + 1)
        elif c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1


def prepare_dsn(dsn, planes):
    """The router's view of the board: the plane layers hidden (their shapes in
    the padstacks too), so it routes the layers between. The (plane NET ...)
    statements stay: BrdRoute then leaves those nets' pads to the plane."""
    text = open(dsn).read()
    for net, layer in planes:
        heads = ["(layer %s\n" % layer, '(keepout "" (circle %s' % layer]
        heads += ["(shape (%s %s" % (k, layer) for k in ("circle", "rect", "path", "polygon")]
        for head in heads:
            while True:
                i = text.find(head)
                if i < 0:
                    break
                text = text[:i] + text[_balanced(text, i):].lstrip(" ")
    open(dsn, "w").write(text)
