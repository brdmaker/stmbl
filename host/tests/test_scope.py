#!/usr/bin/env python3
"""The capture scope (shared/comps/scope.c), every setting, checked on the
decoded samples (TAP).

Each case runs stmbl_host with the drive's templates (the scope loaded),
captures sim0's sine and square (known signals), downloads with scopedump,
decodes the lines as servoterm-cli --capture does (base64, 16 bit, CRC-16)
and checks the samples themselves: the trigger crossing at the trigger
index, the sample interval, the signal's frequency, the quantisation.

usage: test_scope.py   (from host/, after make stmbl_host)
"""
import base64
import math
import os
import re
import subprocess
import sys
import tempfile

HOST = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
CONF = "../hwtest/burnin/ECMA-C30604FS/stmbl.conf"
FREQ = 50.0  # sim0's sine, Hz
n_tests = fails = 0


def ok(c, name, detail=""):
    global n_tests, fails
    n_tests += 1
    fails += not c
    print("%s %d - scope: %s" % ("ok" if c else "not ok", n_tests, name))
    if not c and detail:
        print("#   %s" % detail)


def run(script, timeout=120):
    pre = "lcnc 0 0\nsim0.freq = %g\nsim0.amp = 1\nrun 2\n" % FREQ  # amp / freq settle (0.2 s filter)
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as f:
        f.write(pre + script)
        path = f.name
    try:
        r = subprocess.run(["./stmbl_host", "--drive", CONF, path], cwd=HOST, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, text=True, timeout=timeout)
    finally:
        os.unlink(path)
    return r.stdout.splitlines()


def crc16(d):
    c = 0xffff
    for b in d:
        c ^= b << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) & 0xffff if c & 0x8000 else (c << 1) & 0xffff
    return c


def captures(lines):
    """Every scopedump in the output: dict(nch, n, dt, pre, names, ch=[[...]], crc_bad)."""
    out, cur = [], None
    for l in lines:
        m = re.match(r"^scope begin nch=(\d+) n=(\d+) dt=(\S+) pre=(\d+)$", l)
        if m:
            cur = dict(nch=int(m.group(1)), n=int(m.group(2)), dt=float(m.group(3)), pre=int(m.group(4)),
                       names=[], rng=[], raw=b"", crc_bad=0, lines=0)
            continue
        if cur is None:
            continue
        m = re.match(r"^scope ch(\d+) (\S+) min=(\S+) max=(\S+)$", l)
        if m:
            cur["names"].append(m.group(2))
            cur["rng"].append((float(m.group(3)), float(m.group(4))))
            continue
        m = re.match(r"^scope data (\S+) ([0-9a-f]{4})$", l)
        if m:
            d = base64.b64decode(m.group(1))
            cur["crc_bad"] += crc16(d) != int(m.group(2), 16)
            cur["raw"] += d
            cur["lines"] += 1
            continue
        m = re.match(r"^scope end lines=(\d+)$", l)
        if m:
            nch, n = cur["nch"], cur["n"]
            q = [cur["raw"][i] | cur["raw"][i + 1] << 8 for i in range(0, len(cur["raw"]), 2)]
            cur["ch"] = [[cur["rng"][c][0] + q[i * nch + c] / 65535.0 * (cur["rng"][c][1] - cur["rng"][c][0])
                          for i in range(n)] for c in range(nch)] if len(q) == n * nch else None
            cur["end_lines"] = int(m.group(1))
            out.append(cur)
            cur = None
    return out


def pin_values(lines, pin):
    v = []
    for l in lines:
        m = re.match(r"^%s = (\S+)" % re.escape(pin), l)
        if m:
            v.append(float(m.group(1)))
    return v


def freq_of(x, dt):
    """Frequency from rising zero crossings (interpolated)."""
    t = []
    for i in range(1, len(x)):
        if x[i - 1] < 0 <= x[i]:
            t.append((i - 1 + -x[i - 1] / (x[i] - x[i - 1])) * dt)
    return (len(t) - 1) / (t[-1] - t[0]) if len(t) > 1 else 0.0


SETUP = """scope0.nch = 2
scope0.ch0 = sim0.sin
scope0.ch1 = sim0.square
scope0.trig = sim0.sin
"""


def well_formed(c):
    return c and c["ch"] is not None and c["crc_bad"] == 0 and c["end_lines"] == c["lines"] and \
        c["lines"] == math.ceil(c["n"] * c["nch"] / 24)


# 1. rising edge, rt, 2 channels, 25 % before the trigger
L = run(SETUP + "scope0.level = 0.5\nscope0.pre = 0.25\nscopearm\nrun 0.5\nscope0.state\nscopedump\n")
c = (captures(L) or [None])[0]
ok(well_formed(c) and c["n"] == 2048 and c["pre"] == 512 and c["dt"] == 0.0002 and
   c["names"] == ["sim0.sin", "sim0.square"],
   "rt: 2 x 2048 samples at 0.2 ms, 512 before the trigger, sources named, every CRC right",
   c and {k: c[k] for k in ("n", "pre", "dt", "names", "crc_bad")})
s = c["ch"][0] if well_formed(c) else [0] * 2048
p = c["pre"] if c else 0
ok(s[p - 1] < 0.5 <= s[p] + 1e-4, "rising edge: the trigger sample is the first at or above the level",
   "%g %g" % (s[p - 1], s[p]))
ok(abs(freq_of(s, 0.0002) - FREQ) < 0.5, "the samples' time base: a %g Hz sine measures %g Hz" % (FREQ, freq_of(s, 0.0002)))
ok(abs(max(s) - 1) < 0.01 and abs(min(s) + 1) < 0.01, "the decoded amplitude (sim0.amp = 1)", "%g %g" % (min(s), max(s)))
sq = c["ch"][1] if well_formed(c) else [0]
ok(set(round(v, 3) for v in sq) <= {-1.0, 1.0} and all((a > 0) == (b > 0) for a, b in zip(sq, s) if abs(b) > 0.05),
   "channel 2 sampled with channel 1: the square in phase with the sine")
# 16 bit: a decoded sample within half a step of the true sine at its time
t0 = math.asin(max(-1, min(1, s[p])))
f = freq_of(s, 0.0002)
err = 0
step = (max(s) - min(s)) / 65535
for i in range(len(s)):
    err = max(err, abs(s[i] - math.sin(t0 + 2 * math.pi * f * (i - p) * 0.0002)))
ok(err < 0.001, "16 bit quantisation: the decoded sine matches the ideal one (max error %.1e, step %.1e)" % (err, step))

# 2. falling edge, 3. either edge
for edge, name in ((1, "falling"), (2, "either")):
    L = run(SETUP + "scope0.level = 0.3\nscope0.edge = %d\nscope0.pre = 0.5\nscopearm\nrun 0.5\nscopedump\n" % edge)
    c = (captures(L) or [None])[0]
    if well_formed(c):
        s, p = c["ch"][0], c["pre"]
        up = s[p - 1] < 0.3 <= s[p] + 1e-4
        down = s[p - 1] > 0.3 >= s[p] - 1e-4
        ok((down and not up) if edge == 1 else (up or down), "%s edge: the crossing at the trigger sample" % name,
           "%g %g" % (s[p - 1], s[p]))
    else:
        ok(False, "%s edge: a capture" % name)

# 4. div: a sample every 4 periods
L = run(SETUP + "scope0.level = 0\nscope0.div = 4\nscope0.pre = 0.1\nscopearm\nrun 2\nscopedump\n")
c = (captures(L) or [None])[0]
ok(well_formed(c) and c["dt"] == 0.0008 and abs(freq_of(c["ch"][0], 0.0008) - FREQ) < 0.5,
   "div = 4: dt 0.8 ms, the sine still measures %g Hz" % FREQ, c and (c["dt"], freq_of(c["ch"][0], c["dt"])))

# 5. pre = 0 (all after) and 6. pre = 1 (all before, the trigger sample last)
for pre, want in ((0, 0), (1, 2047)):
    L = run(SETUP + "scope0.level = 0.5\nscope0.pre = %g\nscopearm\nrun 0.6\nscopedump\n" % pre)
    c = (captures(L) or [None])[0]
    if well_formed(c):
        s, p = c["ch"][0], c["pre"]
        crossing = s[p] >= 0.5 - 1e-4 and (p == 0 or s[p - 1] < 0.5) and s[p] < 0.5 + 2 * math.pi * FREQ * 0.0002
        ok(p == want and crossing, "pre = %g: the trigger sample at index %d, the crossing there" % (pre, want),
           "pre %d s[p] %g" % (p, s[p]))
    else:
        ok(False, "pre = %g: a capture" % pre)

# 7. auto: no trigger (level out of reach): forced after a capture length
L = run(SETUP + "scope0.level = 5\nscope0.mode = 2\nscope0.pre = 0.25\nscopearm\nrun 0.45\nscope0.state\n"
        "run 0.45\nscope0.state\nscopedump\nrun 0.01\nscope0.state\n")
st = pin_values(L, "scope0.state")
c = (captures(L) or [None])[0]
# forced after pre (512) + depth (2048) samples (0.51 s), done after post (1536) more: 0.82 s
ok(st[:3] == [1, 3, 1] and well_formed(c), "auto: no trigger, forced after a capture length (still armed at 0.45 s, "
   "done by 0.9 s); re-armed after the dump", st)

# 8. normal: triggered again after each dump
L = run(SETUP + "scope0.level = 0.5\nscope0.mode = 1\nscopearm\nrun 0.5\nscopedump\nrun 0.5\nscope0.state\n"
        "scopedump\n")
cs = captures(L)
ok(len(cs) == 2 and all(well_formed(x) for x in cs) and pin_values(L, "scope0.state")[-1] == 3,
   "normal: re-armed by the dump, the second capture done without scopearm", len(cs))

# 9. single: not re-armed
L = run(SETUP + "scope0.level = 0.5\nscope0.mode = 0\nscopearm\nrun 0.5\nscopedump\nrun 0.5\nscope0.state\n"
        "scopedump\n")
ok(pin_values(L, "scope0.state")[-1] == 3 and len(captures(L)) == 2 and
   captures(L)[0]["raw"] == captures(L)[1]["raw"],
   "single: stays done; a second dump gives the same capture again")

# 10. armed by the pin's rising edge, not by its level
L = run(SETUP + "scope0.level = 0.5\nscope0.arm = 1\nrun 0.5\nscope0.state\nrun 0.5\nscope0.state\n"
        "scope0.arm = 0\nrun 0.01\nscope0.arm = 1\nrun 0.001\nscope0.state\n")
st = pin_values(L, "scope0.state")
ok(st == [3, 3, 1], "arm pin: a rising edge arms (done stays done while the pin stays 1; 0 -> 1 arms again)", st)

# 11. 8 channels; unlinked channels named scope0.chN; nch clamped
L = run("scope0.nch = 8\nscope0.ch0 = sim0.sin\nscope0.trig = sim0.sin\nscope0.ch7 = sim0.square\n"
        "scope0.level = 0\nscopearm\nrun 0.3\nscopedump\nscope0.nch = 12\nscopearm\nrun 0.3\nscopestat\n"
        "scope0.nch = 0\nscopearm\nrun 0.6\nscopestat\n")
c = (captures(L) or [None])[0]
ok(well_formed(c) and c["n"] == 512 and c["names"][0] == "sim0.sin" and c["names"][7] == "sim0.square" and
   c["names"][3] == "scope0.ch3" and set(c["ch"][3]) == {c["rng"][3][0]},
   "8 channels x 512 samples; an unlinked channel is named scope0.chN and constant", c and c["names"])
ok(any("8 channels x 512 samples" in l for l in L) and any("1 channels x 4096 samples" in l for l in L),
   "nch is clamped to 1..8 (scopestat)", [l for l in L if l.startswith("scope:")])

# 12. fast: the frt loop, 20 kHz, with div 2
L = run(SETUP + "scope0.fast = 1\nscope0.div = 2\nscope0.level = 0\nscopearm\nrun 0.3\nscopedump\n")
c = (captures(L) or [None])[0]
ok(well_formed(c) and c["dt"] == 0.0001 and abs(freq_of(c["ch"][0], 0.0001) - FREQ) < 0.5,
   "fast (frt, 20 kHz) with div 2: dt 0.1 ms, the sine measures %g Hz" % FREQ, c and c["dt"])

# 13. dump before done; forced trigger; scopestat
L = run(SETUP + "scope0.level = 5\nscopearm\nrun 0.1\nscopedump\nscopestat\nscopetrig\nrun 0.5\nscopestat\nscopedump\n")
ok(any(l == "scope: no capture (armed)" for l in L), "scopedump before the capture: refused, says why")
ok(any(l.startswith("scope: armed, 2 channels x 2048 samples, dt 0.0002 s") for l in L) and
   any(l.startswith("scope: done") for l in L) and len(captures(L)) == 1,
   "scopetrig forces the trigger; scopestat follows the state", [l for l in L if l.startswith("scope:")])

print("1..%d\n# %d/%d passed" % (n_tests, n_tests - fails, n_tests))
sys.exit(1 if fails else 0)
