#!/usr/bin/env python3
"""Per-section maxima of a soak test: run_tests.py says what failed, this
says where. usage: soak_report.py gen/soak/soak_<motor>.test..."""

import os
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import run_tests  # noqa: E402

PINS = ("pid0.pos_error", "fault0.pos_error", "hv0.sim_vel", "hv0.iq_fb", "fault0.fault")


def report(path):
    sc = run_tests.build_script(path)
    script, args, timeout = sc.text, sc.args, sc.timeout
    names = re.findall(r"^# section (\S+)$", script, re.M)
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as f:
        f.write(script)
    try:
        out = subprocess.run(["./stmbl_host"] + args + [f.name], cwd=run_tests.HOST_DIR,
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                             universal_newlines=True, timeout=timeout).stdout
    finally:
        os.unlink(f.name)
    chunks = re.split(r"^sim time:.*$", out, flags=re.M)
    print("# %s" % os.path.basename(path))
    print("  %-10s %9s %9s %8s %7s %5s" % ("section", "|ferror|", "|rest|", "|vel|", "|iq|", "fault"))
    for name, chunk in zip(["setup"] + names, chunks):
        v = run_tests.parse_values(chunk)
        m = [max((abs(x) for x in v.get(p, [])), default=float("nan")) for p in PINS]
        print("  %-10s %9.4f %9.4f %8.1f %7.2f %5.0f" % ((name,) + tuple(m)))


for p in sys.argv[1:]:
    report(p)
