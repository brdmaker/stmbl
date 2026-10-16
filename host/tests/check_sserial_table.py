#!/usr/bin/env python3
"""The smart-serial descriptor table in src/comps/sserial.c must be what
tools/gentable.c generates (it is the source: edit it, run it, paste), and
must be what hostmot2 reads: the process data records' bits per direction
fill the discovery sizes and the C structs; every field byte aligned in
the bit stream where the struct has a byte type. Prints TAP."""
import os
import re
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
results = []


def ok(cond, name, detail=""):
    results.append((bool(cond), name, detail))


def blob(text):
    m = re.search(r"uint8_t sserial_slave\[\] = \{(.*?)\};", text, re.S)
    return bytes(int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", m.group(1)))


def records(b):
    ptocp, gtocp = struct.unpack_from("<HH", b, 2)

    def ptrs(a):
        r = []
        while True:
            v = struct.unpack_from("<H", b, a)[0]
            a += 2
            if v == 0:
                return r
            r.append(v)
    out = []
    for a in ptrs(ptocp):
        if b[a] != 0xA0:
            continue
        e = b.index(0, a + 14)
        name = b[e + 1:b.index(0, e + 1)].decode()
        out.append((name, b[a + 1], b[a + 2], b[a + 3]))
    return out


src = open(os.path.join(ROOT, "src/comps/sserial.c")).read()
with tempfile.TemporaryDirectory() as d:
    exe = os.path.join(d, "gentable")
    r = subprocess.run(["gcc", "-w", "-o", exe, os.path.join(ROOT, "tools/gentable.c")], capture_output=True, text=True)
    ok(r.returncode == 0, "tools/gentable.c builds", r.stderr[-300:])
    gen = subprocess.run([exe], capture_output=True, text=True).stdout if r.returncode == 0 else ""
ok(gen and blob(gen) == blob(src), "src/comps/sserial.c has gentable's table")
for what, pat in (("discovery sizes / pointers", r"const discovery_rpc_t discovery = \{(.*?)\};"),
                  ("scale_address", r"#define scale_address (\d+)")):
    a, g = re.search(pat, src, re.S), re.search(pat, gen, re.S)
    norm = lambda m: re.sub(r"\s+", "", m.group(1)) if m else None
    ok(norm(a) == norm(g), "  ...and its %s" % what)

b = blob(src)
recs = records(b)
inp, outp = b[0], b[1]
bits_in = sum(r[1] for r in recs if r[3] in (0x00, 0x40))
bits_out = sum(r[1] for r in recs if r[3] in (0x80, 0x40))
ok(8 + bits_in == inp * 8 and bits_out == outp * 8, "process data bits fill the discovery sizes (in %d, out %d)"
   % (bits_in, bits_out))
ok(bits_in <= 96 and bits_out <= 96, "  ...within 96 bits per direction (older Mesa sserial firmware)")
pos, aligned = 0, True
for name, n, typ, d in recs:
    if d not in (0x00, 0x40):
        continue
    if typ in (0x02, 0x03, 0x10) and pos % 8:  # unsigned / signed / float: a byte type in the struct
        aligned = False
    pos += n
ok(aligned, "input fields of byte types are byte aligned in the bit stream")
power = [r for r in recs if r[0] == "power"]
ok(power and power[0][1:] == (16, 0x03, 0x00), "power: 16 bit signed, drive -> LinuxCNC")
size = re.search(r"_Static_assert\(sizeof\(sserial_in_process_data_t\) == (\d+)", src)
ok(size and int(size.group(1)) == inp - 1, "sserial_in_process_data_t is the input bytes less the fault byte")

for i, (c, name, detail) in enumerate(results, 1):
    print("%s %d - sserial: %s" % ("ok" if c else "not ok", i, name))
    if not c and detail:
        print("#   %s" % detail)
print("1..%d" % len(results))
sys.exit(0 if all(r[0] for r in results) else 1)
