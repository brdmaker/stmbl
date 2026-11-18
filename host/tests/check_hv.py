#!/usr/bin/env python3
"""Source checks for the F4's hv component and the F3's ls (no simulator
runs them; the logic is shared/hv_link.h, host/tests/test_hv_link.c).
DEBUGNOTES 2026-11-11:
- the state words from index uid on went into hv_uid[3] without an upper
  bound: fw_version (word 12) wrote hv_uid[3], one past the array;
- the F3 reports its link timeouts (link_to) and the F4 counts the drops
  through hv_link_word / hv_link_fault (a real F3 fault code wins);
- hv_verify reads back only with the drive disabled."""
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")


def read(rel):
    s = open(os.path.join(ROOT, rel)).read()
    s = re.sub(r"/\*.*?\*/", "", s, flags=re.S)
    return re.sub(r"//[^\n]*", "", s)


n = fails = 0


def ok(c, name):
    global n, fails
    n += 1
    fails += not c
    print("%s %d - hv: %s" % ("ok" if c else "not ok", n, name))


hv = read("src/comps/hv.c")
ls = read("stm32f303/src/comps/ls.c")
m = re.search(r"if\(([^{]*)\)\s*\{\s*hv_uid\[", hv)
ok(m is not None and "+ 3" in m.group(1), "hv_uid written only for the 3 uid words")
ok(re.search(r"state\.pins\.link_to\s*=", ls) is not None, "F3 ls reports link_to")
ok("hv_link_word(" in hv and re.search(r"PIN\(fault\)\s*=\s*hv_link_fault\(", hv) is not None,
   "F4 hv: drops counted, the fault through hv_link_fault")
ok(re.search(r"send_verify\s*&&\s*e\s*>\s*0\.0", hv) is not None, "hv_verify refused while enabled")
ok('COMMAND("hv_verify"' in open(os.path.join(ROOT, "src/comps/hv.c")).read(), "hv_verify command")
print("1..%d" % n)
sys.exit(1 if fails else 0)
