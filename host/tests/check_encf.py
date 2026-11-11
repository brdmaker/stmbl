#!/usr/bin/env python3
"""Source check for src/comps/encf.c (no simulator runs the F4's encoder
timers): the request for the next frame goes out at the start of rt, before
the decode, so the encoder latches its position at a fixed time
(stmbl_freak 2963ea4; DEBUGNOTES 2026-11-11), and the frame is decoded by
the bounded shared/encf_frame.h (host/tests/test_encf_frame.c)."""
import os
import re
import sys

src = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "src", "comps", "encf.c")).read()
src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
src = re.sub(r"//[^\n]*", "", src)
rt = re.search(r"static void rt_func\([^)]*\)\s*\{(.*?)\n\}", src, re.S).group(1)

n = fails = 0


def ok(c, name):
    global n, fails
    n += 1
    fails += not c
    print("%s %d - encf: %s" % ("ok" if c else "not ok", n, name))


req = rt.find("SPI3->DR")
dec = rt.find("encf_decode(")
ok(req >= 0 and rt.count("SPI3->DR") == 1, "one request a run")
ok(dec >= 0, "decodes with shared/encf_frame.h")
ok(0 <= req < dec, "the request goes out before the decode")
cpy = rt.find("memcpy(cap")
ok(0 <= cpy < rt.find("DMA_Cmd(DMA1_Stream0, ENABLE)"), "captures copied before the DMA restarts over them")
ok(re.search(r"while\s*\(\s*DMA1_Stream0", rt) is None, "no unbounded DMA wait in rt")
print("1..%d" % n)
sys.exit(1 if fails else 0)
