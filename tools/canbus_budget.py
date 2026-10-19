#!/usr/bin/env python3
"""CAN bus time budget for cyclic drive traffic (docs/can_support.md).

Frame lengths from the ISO 11898-1 bit layout, with worst-case stuff bits:
  classic CAN (11-bit ID): SOF, ID 11, RTR, IDE, r0, DLC 4, data, CRC 15,
    CRC delimiter, ACK 2, EOF 7, IFS 3; stuffing over SOF..CRC: at most one
    bit per 4 after the first (worst case: (n - 1) // 4).
  CAN FD (11-bit ID, BRS): nominal rate for SOF, ID 11, RRS, IDE, FDF, res,
    BRS, and after the CRC: CRC delimiter, ACK 2, EOF 7, IFS 3; data rate
    for ESI, DLC 4, data, stuff count 4, CRC 17 (<= 16 bytes) or 21, with
    fixed stuff bits in the CRC field (one per 4 bits) and dynamic stuffing
    before it.

usage: canbus_budget.py            the tables of docs/can_support.md
"""


def classic_us(n, rate=1e6):
    stuffed = 34 + 8 * n  # SOF .. CRC
    bits = 47 + 8 * n + (stuffed - 1) // 4
    return bits / rate * 1e6


FD_DLC = [0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64]


def fd_len(n):
    return next(d for d in FD_DLC if d >= n)


def fd_us(n, nominal=1e6, data=5e6):
    n = fd_len(n)
    arb = 17 + (17 - 1) // 4                       # SOF .. BRS, stuffed
    crc = 17 if n <= 16 else 21
    dyn = 5 + 8 * n                                # ESI, DLC, data (dynamic stuffing)
    data_bits = dyn + dyn // 4 + 4 + 1 + crc + (crc + 4) // 4  # + stuff count, parity, CRC, fixed stuff
    tail = 1 + 2 + 7 + 3                           # CRC delimiter, ACK, EOF, IFS (nominal)
    return (arb + tail) / nominal * 1e6 + data_bits / data * 1e6


def drives(cycle_us, frames_per_cycle, per_drive, load=0.7):
    """How many drives fit: fixed frames once per cycle, per_drive each."""
    budget = cycle_us * load - frames_per_cycle
    return max(0, int(budget // per_drive))


def main():
    print("frame times, worst-case stuffing [us]")
    print("  classic 1 Mbit/s:  8 B %.0f, 0 B (SYNC) %.0f" % (classic_us(8), classic_us(0)))
    for d in (2e6, 5e6, 8e6):
        print("  FD 1 / %.0f Mbit/s: 16 B %.0f, 32 B %.0f, 64 B %.0f" %
              (d / 1e6, fd_us(16, data=d), fd_us(32, data=d), fd_us(64, data=d)))
    print()
    print("drives per bus at 70 % load (command and feedback each cycle)")
    print("  %-46s %8s %8s %8s" % ("scheme", "2 kHz", "1 kHz", "500 Hz"))
    rows = [
        ("classic 1M: SYNC + 8 B cmd + 8 B fb per drive", classic_us(0), 2 * classic_us(8)),
        ("FD 1/2M: SYNC + 16 B cmd + 16 B fb per drive", fd_us(0, data=2e6), 2 * fd_us(16, data=2e6)),
        ("FD 1/5M: SYNC + 16 B cmd + 16 B fb per drive", fd_us(0, data=5e6), 2 * fd_us(16, data=5e6)),
        ("FD 1/5M: one 64 B cmd for 4 + 16 B fb each", None, fd_us(16, data=5e6)),
        ("FD 1/8M: SYNC + 16 B cmd + 16 B fb per drive", fd_us(0, data=8e6), 2 * fd_us(16, data=8e6)),
    ]
    for name, fixed, per in rows:
        out = []
        for cyc in (500.0, 1000.0, 2000.0):  # cycle times [us]: 2 kHz, 1 kHz, 500 Hz
            if fixed is None:  # 64-byte command frames carry 4 drives each
                n = 0
                while True:
                    m = n + 1
                    t = -(-m // 4) * fd_us(64, data=5e6) + m * per
                    if t > cyc * 0.7:
                        break
                    n = m
                out.append(n)
            else:
                out.append(drives(cyc, fixed, per))
        print("  %-46s %8d %8d %8d" % (name, out[0], out[1], out[2]))


if __name__ == "__main__":
    main()
