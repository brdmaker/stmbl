#!/usr/bin/env python3
"""Load a bitstream into an FPGA through Linux's FPGA manager (Zynq, other
SoC FPGAs): copy it into the firmware folder, ask the manager to load it,
wait until it says "operating".

    fpga_load.py [--fpga fpga0] [--sysfs /sys/class/fpga_manager] [--firmware-dir /lib/firmware] FILE

Needs root (both folders are root's). millweb-supervisor runs it with
`sudo -n`; a sudoers line lets the machine's user do only this:

    lcnc ALL=(root) NOPASSWD: /usr/bin/python3 /path/to/webui/mill/server/fpga_load.py *

For a Xilinx Zynq the file is the .bit.bin (bootgen -process_bitstream bin),
loaded as a full bitstream (flags 0).
"""
import argparse
import hashlib
import os
import shutil
import sys
import time


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--fpga", default="fpga0")
    ap.add_argument("--sysfs", default="/sys/class/fpga_manager")
    ap.add_argument("--firmware-dir", default="/lib/firmware")
    ap.add_argument("--timeout", type=float, default=20.0)
    ap.add_argument("file")
    a = ap.parse_args(argv)
    if not a.fpga.startswith("fpga") or not a.fpga[4:].isdigit():
        sys.exit("fpga_load: --fpga fpgaN")
    mgr = os.path.join(a.sysfs, a.fpga)
    if not os.path.isdir(mgr):
        sys.exit("fpga_load: no FPGA manager at %s (is the kernel's fpga_manager driver loaded?)" % mgr)
    data = open(a.file, "rb").read()
    name = "millweb-%s.bin" % hashlib.sha256(data).hexdigest()[:12]
    os.makedirs(a.firmware_dir, exist_ok=True)
    dst = os.path.join(a.firmware_dir, name)
    if not os.path.exists(dst):
        shutil.copyfile(a.file, dst + ".tmp")
        os.replace(dst + ".tmp", dst)
    try:
        with open(os.path.join(mgr, "flags"), "w") as f:
            f.write("0")
    except OSError:
        pass  # older kernels: no flags, full bitstreams
    with open(os.path.join(mgr, "firmware"), "w") as f:
        f.write(name)
    end = time.time() + a.timeout
    state = ""
    while time.time() < end:
        try:
            state = open(os.path.join(mgr, "state")).read().strip()
        except OSError:
            state = "?"
        if state == "operating":
            print("fpga_load: %s loaded into %s (%d bytes)" % (os.path.basename(a.file), a.fpga, len(data)))
            return 0
        if "error" in state:
            break
        time.sleep(0.05)
    sys.exit("fpga_load: %s says %r, not operating" % (a.fpga, state))


if __name__ == "__main__":
    sys.exit(main())
