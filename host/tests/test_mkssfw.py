#!/usr/bin/env python3
"""tools/mkssfw.py: the setsserial image (TAP)."""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))
import mkssfw  # noqa: E402

n = fails = 0


def ok(c, name):
    global n, fails
    n += 1
    fails += not c
    print("%s %d - mkssfw: %s" % ("ok" if c else "not ok", n, name))


app = bytes((i * 7 + 1) & 0xff or 1 for i in range(40000))
img = mkssfw.make(app)
ok(len(img) == 16384 * 4 and img[:16384] == bytes(16384), "block 0 zeros, the application in whole 16 kB blocks")
ok(img[16384:16384 + len(app)] == app and set(img[16384 + len(app):]) == {0xff}, "the application, padded with 0xff")
bad = app[:16384] + bytes(16384) + app[:100]
try:
    mkssfw.make(bad)
    ok(False, "an all-zero block refused")
except ValueError as e:
    ok("0x4000" in str(e), "an all-zero erase block is refused (setsserial would skip it)")
try:
    mkssfw.make(b"\x01" * (449 * 1024))
    ok(False, "too large refused")
except ValueError:
    ok(True, "an application larger than the area is refused")
img2 = mkssfw.make(app, erase_log2=11, max_size=56 * 1024)
ok(len(img2) % 2048 == 0 and img2[:2048] == bytes(2048), "brdstepper (G431): 2 kB pages")
print("1..%d\n# %d/%d passed" % (n, n - fails, n))
sys.exit(1 if fails else 0)
