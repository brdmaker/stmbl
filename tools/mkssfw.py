#!/usr/bin/env python3
"""Make the image for LinuxCNC's `setsserial flash` from a drive application
.bin (docs/sserial_bootloader.md):

  block 0 (one erase block of zeros: setsserial never writes block 0, it is
  the bootloader's) + the application, padded with 0xff to whole blocks.

setsserial skips erase blocks that are all zero bytes; the bootloader erased
them, so they would read 0xff and the application's CRC would fail. Such a
block is refused here (rebuild with a change, e.g. a different version
string, or pad that region in the linker script).

usage: mkssfw.py APP.bin OUT.bin [--erase-log2 14] [--max KB]
       stmbl F4: erase blocks of 16 kB (2^14), application area 448 kB
"""
import argparse
import sys


def make(app, erase_log2=14, max_size=448 * 1024):
    blk = 1 << erase_log2
    if len(app) == 0:
        raise ValueError("empty application")
    if len(app) > max_size:
        raise ValueError("application %d bytes > %d (the application area)" % (len(app), max_size))
    pad = (-len(app)) % blk
    body = app + b"\xff" * pad
    zero = [i for i in range(0, len(body), blk) if body[i:i + blk] == bytes(blk)]
    if zero:
        raise ValueError("all-zero erase block(s) at application offset %s: setsserial would skip them"
                         % ", ".join("0x%x" % z for z in zero))
    return bytes(blk) + body


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("app")
    ap.add_argument("out")
    ap.add_argument("--erase-log2", type=int, default=14)
    ap.add_argument("--max", type=int, default=448, help="application area, kB")
    a = ap.parse_args()
    app = open(a.app, "rb").read()
    try:
        img = make(app, a.erase_log2, a.max * 1024)
    except ValueError as e:
        print("mkssfw: %s" % e, file=sys.stderr)
        return 1
    open(a.out, "wb").write(img)
    print("mkssfw: %s: %d bytes application, %d blocks of %d bytes (block 0 placeholder) -> %s"
          % (a.app, len(app), len(img) >> a.erase_log2, 1 << a.erase_log2, a.out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
