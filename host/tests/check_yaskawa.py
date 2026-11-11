#!/usr/bin/env python3
"""Source check for src/comps/yaskawa.c (no simulator runs the F4's encoder
timers): the reply is decoded by the bounded shared/yaskawa_frame.h and its
position used only through yask_track (a bad reply holds the position),
the error pin comes from the tracker, and the waits in rt are bounded
(DEBUGNOTES 2026-11-11; the decoder itself: host/tests/test_yaskawa_frame.c)."""
import os
import re
import sys

src = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "src", "comps", "yaskawa.c")).read()
src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
src = re.sub(r"//[^\n]*", "", src)
rt = re.search(r"static void rt_func\([^)]*\)\s*\{(.*?)\n\}", src, re.S).group(1)
n = fails = 0


def ok(c, name):
    global n, fails
    n += 1
    fails += not c
    print("%s %d - yaskawa: %s" % ("ok" if c else "not ok", n, name))


ok("yask_decode(" in rt, "decodes with shared/yaskawa_frame.h")
ok("yask_track(" in rt and re.search(r"PIN\(pos\)\s*=.*\bp\b", rt) is not None,
   "the position only through yask_track")
ok(re.search(r"PIN\(error\)\s*=\s*error\s*;", rt) is not None and rt.count("PIN(error) =") == 1,
   "error set once, from the tracker (not overwritten)")
ok(re.search(r"\bm_data\[", src) is None and "yaskawa_reply" not in src, "no unbounded bit buffers left")
ok(re.search(r"while\s*\(\s*!\s*\(\s*DMA2->LISR", rt) is None, "the request wait is bounded")
ok("ic_filter" in rt and "CCMR2" in rt, "the capture input filter is set (ic_filter)")
print("1..%d" % n)
sys.exit(1 if fails else 0)
