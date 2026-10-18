#!/usr/bin/env python3
"""vis3d drawing, offscreen (vis3d_render, EGL): the ARM-6 model in every GL
flavour the library supports (GLES 3 as on a Pi 4, GLES 2, GL 3.3 core,
GL 3.1 and 2.1 as a Pi 4's desktop GL gives QtVCP on X11), checked on the image:
the scene is there, the flavours agree, the overlays show. TAP output.
Also prints the frame rate (here: Mesa's software renderer, not a GPU).
"""
import os
import struct
import subprocess
import sys
import zlib

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(HERE, "tests", "tmp")
MODEL = os.path.join(HERE, "models", "arm6.vis")
RENDER = os.path.join(HERE, "vis3d_render")
W, H = 480, 270

n = failed = 0


def ok(cond, what):
    global n, failed
    n += 1
    failed += not cond
    print("%s %d - %s" % ("ok" if cond else "not ok", n, what))


def read_png(path):  # our own writer: 8-bit RGB, filter 0 on every row
    data = open(path, "rb").read()
    pos, idat, w, h = 8, b"", 0, 0
    while pos < len(data):
        ln, typ = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + ln]
        if typ == b"IHDR":
            w, h = struct.unpack(">II", body[:8])
        elif typ == b"IDAT":
            idat += body
        pos += 12 + ln
    raw = zlib.decompress(idat)
    rows = [raw[y * (1 + 3 * w) + 1:(y + 1) * (1 + 3 * w)] for y in range(h)]
    return w, h, rows


def px(rows, x, y):
    r = rows[y]
    return r[3 * x], r[3 * x + 1], r[3 * x + 2]


def render(name, *args):
    out = os.path.join(OUT, name + ".png")
    p = subprocess.run([RENDER, MODEL, "-o", out, "-s", "%dx%d" % (W, H)] + list(args),
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    return p.returncode, p.stdout, out


def count(rows, pred):
    return sum(1 for y in range(H) for x in range(W) if pred(*px(rows, x, y)))


def cyan(r, g, b):  # the accent: the rings, the reach circle
    return g > 150 and b > 150 and r < 140


def red(r, g, b):
    return r > 150 and g < 120 and b < 120


os.makedirs(OUT, exist_ok=True)
pose = ["-q", "-20,25,-5,10,-70,15"]
imgs = {}
for api in ("gles3", "gles2", "gl33", "gl31", "gl21"):
    rc, out, path = render("arm6_" + api, "--api", api, *pose)
    info = [l for l in out.splitlines() if "GLSL" in l]
    ok(rc == 0 and os.path.exists(path), "%s: draws without GL errors (%s)" % (api, info[0].split(": ", 1)[1] if info else out.strip()))
    if rc == 0:
        imgs[api] = read_png(path)[2]

base = imgs.get("gles3")
if base:
    distinct = len(set(px(base, x, y) for y in range(0, H, 3) for x in range(0, W, 3)))
    ok(distinct > 500, "the scene isn't empty (%d distinct colours)" % distinct)
    ok(count(base, cyan) > 80, "the accent shows (rings, reach circle): %d px" % count(base, cyan))
    for api, rows in imgs.items():
        if api == "gles3":
            continue
        diff = sum(abs(a - b) for y in range(0, H, 2) for a, b in zip(base[y], rows[y])) / (W * H / 2 * 3)
        ok(diff < 3.0, "%s looks like gles3 (mean difference %.2f / 255)" % (api, diff))

    rc, _, path = render("arm6_stop", *pose, "--guard", "3,0.2,0", "--tint", "3,1,0.36,0.33,0.8")
    rows = read_png(path)[2] if rc == 0 else None
    ok(rows is not None and count(rows, red) > count(base, red) + 300,
       "a protective stop: the shell, the nearest obstacle and the tinted link turn red (%d px more)" %
       ((count(rows, red) - count(base, red)) if rows else 0))

    # the same end pose with and without the move before it: the trail's
    # warm pixels are the difference
    rc1, _, p1 = render("arm6_trail", "-q", "-40,20,0,0,-70,0", "--path", "40,20,0,0,-70,0")
    rc2, _, p2 = render("arm6_notrail", "-q", "40,20,0,0,-70,0")
    trail = 0
    if rc1 == 0 and rc2 == 0:
        a, b = read_png(p1)[2], read_png(p2)[2]
        trail = sum(1 for y in range(H) for x in range(W)
                    if px(a, x, y) != px(b, x, y) and px(a, x, y)[0] > px(a, x, y)[2] + 40)
    ok(trail > 60, "the trail of a move shows (%d warm pixels)" % trail)

rc, out, _ = render("bench", "--api", "gles3", "-s", "1280x720", "--bench", "60")
fps = [l for l in out.splitlines() if "fps" in l]
ok(rc == 0 and fps, "bench runs")
if fps:
    print("# " + fps[0].split(": ", 1)[1] + " (software rendering)")

print("1..%d\n# %d/%d passed" % (n, n - failed, n))
sys.exit(1 if failed else 0)
