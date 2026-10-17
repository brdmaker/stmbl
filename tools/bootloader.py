#!/usr/bin/env python
"""Put the STMBL drive into ST's USB DFU (ROM) bootloader.

Sends `bootloader` on the drive's console and waits until the DFU device
(0483:df11) shows up. ST's ROM sometimes goes straight back to the app
(about 1 entry in 20 on the bench, most likely on a USB reset while the
host enumerates it), so it tries again until the DFU device stays.

usage: bootloader.py [DFU-UTIL]   (default: dfu-util)
"""
import subprocess
import sys
import time

try:
    import serial.tools.list_ports
except ImportError:
    print("missing pyserial, run sudo apt install python3-serial")
    sys.exit(-1)

DFU_UTIL = sys.argv[1] if len(sys.argv) > 1 else "dfu-util"
ATTEMPTS = 10


def dfu_present():
    try:
        out = subprocess.check_output([DFU_UTIL, "-l"], stderr=subprocess.STDOUT)
    except (OSError, subprocess.CalledProcessError):
        return False
    return b"0483:df11" in out


def stmbl_ports():
    return [p for p in serial.tools.list_ports.comports() if "483:5740" in p[2]]


def wait(cond, timeout):
    end = time.time() + timeout
    while time.time() < end:
        if cond():
            return True
        time.sleep(0.2)
    return False


for attempt in range(1, ATTEMPTS + 1):
    if dfu_present():
        print("STMBL in the DFU bootloader" + (" (attempt %d)" % attempt if attempt > 1 else ""))
        sys.exit(0)
    ports = stmbl_ports()
    if not ports:
        # rebooting, or already on its way into DFU
        if wait(lambda: dfu_present() or stmbl_ports(), 5.0):
            continue
        print("No boards found, or board is already in bootloader")
        sys.exit(0)
    port = ports[0]
    print("Reseting stmbl at " + port[2])
    try:
        stmbl = serial.Serial(port[0])
        stmbl.write("bootloader\n".encode())
        stmbl.close()  # don't hold the port across the reset
    except (OSError, serial.SerialException):
        print("Cannot open port, check permissions, or disconnect Servoterm")
        sys.exit(1)
    wait(dfu_present, 4.0)

print("STMBL did not stay in the DFU bootloader after %d attempts" % ATTEMPTS)
sys.exit(1)
