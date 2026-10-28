#!/usr/bin/env python3
"""Each STL's size as printed (x, y on the bed, z its height): within the bed? The Bambu Lab H2C:
330 x 320 x 320, 300 x 320 with both nozzles; with a margin, 295 x 315 x 310 (BED="x y z" to change)."""
import glob, os, struct, sys
BED = [float(v) for v in os.environ.get("BED", "295 315 310").split()]
bad = 0
for f in sorted(glob.glob(os.path.join(os.path.dirname(os.path.abspath(__file__)), "stl", "*.stl"))):
    d = open(f, "rb").read()
    n = struct.unpack("<I", d[80:84])[0]
    lo, hi = [1e9] * 3, [-1e9] * 3
    for i in range(n):
        v = struct.unpack("<12f", d[84 + 50 * i: 84 + 50 * i + 48])
        for k in range(3):
            for p in (v[3 + k], v[6 + k], v[9 + k]):
                lo[k] = min(lo[k], p); hi[k] = max(hi[k], p)
    size = [hi[k] - lo[k] for k in range(3)]
    fits = lambda a, b: a <= BED[0] and b <= BED[1]  # noqa: E731
    ok = (fits(size[0], size[1]) or fits(size[1], size[0])) and size[2] <= BED[2] and abs(lo[2]) < 0.01
    bad += not ok
    print("%-18s %6.1f x %6.1f x %6.1f  %s" % (os.path.basename(f), *size, "ok" if ok else "TOO BIG / NOT ON THE BED (z from %.2f)" % lo[2]))
sys.exit(1 if bad else 0)
