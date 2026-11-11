#!/usr/bin/env python3
"""The fault codes (fault_t, shared/common.h) and every list that names them
stay in step: the drive's strings (shared/comps/fault.c), the soak test's
names (hwtest/soak/soak.py). A code added without its name printed "?" or a
wrong neighbour's name (soak.py stopped at FB_MON_ERROR, 18, until
2026-11-11). brdmot's console (webui/mill/server/drives.py) keeps its own
list: checked too when the brdmot checkout is next to this one."""
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
n = fails = 0


def ok(c, name, detail=""):
    global n, fails
    n += 1
    fails += not c
    print("%s %d - faults: %s" % ("ok" if c else "not ok", n, name))
    if not c and detail:
        print("#   " + detail)


common = open(os.path.join(ROOT, "shared/common.h")).read()
body = re.search(r"typedef enum \{\s*NO_ERROR = 0,(.*?)\} fault_t;", common, re.S).group(1)
body = re.sub(r"//[^\n]*", "", body)
codes = ["NO_ERROR"] + [c.strip() for c in body.split(",") if c.strip()]

fc = open(os.path.join(ROOT, "shared/comps/fault.c")).read()
strings = re.search(r"fault_string\[\]\s*=\s*\{(.*?)\};", fc, re.S).group(1)
strings = re.findall(r'"((?:[^"\\]|\\.)*)"', re.sub(r"//[^\n]*", "", strings))
ok(len(strings) == len(codes), "fault.c: a string for every fault_t code (%d)" % len(codes),
   "%d strings, %d codes" % (len(strings), len(codes)))

soak = open(os.path.join(ROOT, "hwtest/soak/soak.py")).read()
names = re.findall(r'"([A-Z_]+)"', re.search(r"FAULTS = \[(.*?)\]", soak, re.S).group(1))
ok(names == codes, "soak.py: FAULTS is fault_t, in order", "missing from %d: %s" % (len(names), codes[len(names):]))

drives = os.path.join(ROOT, "..", "webui", "mill", "server", "drives.py")  # stmbl as brdmot's submodule
if os.path.exists(drives):
    d = open(drives).read()
    lst = re.findall(r'"((?:[^"\\]|\\.)*)"', re.search(r"FAULTS = \[(.*?)\]", d, re.S).group(1))
    ok(len(lst) == len(codes), "brdmot drives.py: a name for every code", "%d names" % len(lst))
print("1..%d" % n)
sys.exit(1 if fails else 0)
