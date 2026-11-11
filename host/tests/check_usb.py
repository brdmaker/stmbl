#!/usr/bin/env python3
"""Source check: the drive's two USB IN writers (src/syscalls.c _write,
src/usb_cdc.c cdc_tx) go through shared/cdc_in.h's cdc_in_put, which never
writes over unsent bytes (host/tests/test_cdc_in.c), and nothing else
writes APP_Rx_Buffer (DEBUGNOTES 2026-10-14)."""
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
n = fails = 0


def ok(c, name):
    global n, fails
    n += 1
    fails += not c
    print("%s %d - usb: %s" % ("ok" if c else "not ok", n, name))


for f in ("src/syscalls.c", "src/usb_cdc.c"):
    s = re.sub(r"//[^\n]*", "", open(os.path.join(ROOT, f)).read())
    ok("cdc_in_put(" in s, f + " writes through cdc_in_put")
    ok(re.search(r"APP_Rx_Buffer\s*\[", s) is None, f + ": no direct APP_Rx_Buffer[...] writes")
print("1..%d" % n)
sys.exit(1 if fails else 0)
