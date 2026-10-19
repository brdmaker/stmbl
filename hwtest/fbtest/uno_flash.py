#!/usr/bin/env python3
"""Flash an Intel HEX file into an Arduino Uno through its optiboot loader.

avrdude 7.1 fails on the fbtest Uno: it syncs before the loader has
started after the reset, the loader gets a partial command, optiboot's
watchdog resets it, and with no application in flash it never answers
again. This waits for the loader before the first byte, writes only the
pages that hold data, then reads them back in a second session.

    uno_flash.py fbtest.hex [/dev/ttyUSB0]
"""
import sys
import time

import serial

PAGE = 128
OK = b'\x14\x10'


def read_hex(path):
    mem = {}
    base = 0
    for line in open(path):
        line = line.strip()
        if not line.startswith(':'):
            continue
        b = bytes.fromhex(line[1:])
        n, addr, typ, data = b[0], (b[1] << 8) | b[2], b[3], b[4:4 + b[0]]
        if (sum(b) & 0xff) != 0:
            sys.exit('bad checksum: ' + line)
        if typ == 0:
            for i, v in enumerate(data):
                mem[base + addr + i] = v
        elif typ == 2:
            base = ((data[0] << 8) | data[1]) << 4
        elif typ == 4:
            base = ((data[0] << 8) | data[1]) << 16
        elif typ == 1:
            break
    pages = {}
    for a in sorted(mem):
        p = a - a % PAGE
        pages.setdefault(p, bytearray(b'\xff' * PAGE))[a - p] = mem[a]
    return pages


class Loader:
    def __init__(self, port):
        self.s = serial.Serial(port, 115200, timeout=0.2)

    def cmd(self, b, n=0):
        self.s.write(b + b' ')
        r = self.s.read(n + 2)
        if len(r) != n + 2 or r[0] != 0x14 or r[-1] != 0x10:
            raise IOError('loader: %s -> %s' % (b[:1].hex(), r.hex()))
        return r[1:-1]

    def start(self):
        for _ in range(5):
            self.s.dtr = False
            time.sleep(0.1)
            self.s.dtr = True  # the falling edge resets through the capacitor
            # nothing before the loader runs (start-up delay plus its
            # UART set-up): a partial command makes it reset itself
            time.sleep(0.25)
            self.s.reset_input_buffer()
            for _ in range(3):
                self.s.write(b'\x30 ')
                if self.s.read(2) == OK:
                    sig = self.cmd(b'\x75', 3)
                    if sig != b'\x1e\x95\x0f':
                        raise IOError('not an ATmega328P: ' + sig.hex())
                    return
        raise IOError('no answer from the loader')

    def addr(self, a):
        w = a // 2
        self.cmd(bytes([0x55, w & 0xff, w >> 8]))

    def leave(self):
        try:
            self.cmd(b'\x51')
        except IOError:
            pass


def main():
    path = sys.argv[1]
    port = sys.argv[2] if len(sys.argv) > 2 else '/dev/ttyUSB0'
    pages = read_hex(path)
    if not pages or max(pages) >= 0x7e00:
        sys.exit('image empty or overlaps the loader')
    ld = Loader(port)
    for attempt in range(3):
        try:
            ld.start()
            for p in sorted(pages):
                ld.addr(p)
                ld.cmd(bytes([0x64, 0, PAGE, 0x46]) + bytes(pages[p]))
            ld.start()
            for p in sorted(pages):
                ld.addr(p)
                got = ld.cmd(bytes([0x74, 0, PAGE, 0x46]), PAGE)
                if got != pages[p]:
                    raise IOError('verify failed at 0x%04x' % p)
            ld.leave()
            print('uno_flash: %d bytes in %d pages written and verified'
                  % (sum(len(v) for v in pages.values()), len(pages)))
            return
        except IOError as e:
            print('uno_flash: attempt %d: %s' % (attempt + 1, e))
    sys.exit(1)


if __name__ == '__main__':
    main()
