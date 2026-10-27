#!/usr/bin/env python3
"""Node numbers on the measurement bus: vibaddr.py (the PC) against the
sensors' own C code (shared/vibaddr.c, built here into a library) on a
simulated CAN bus. Every sensor sees every frame; answers that are the same
frame overlap as they would on CAN.

  python3 tools/vibmon/test_vibaddr.py
"""
import ctypes
import os
import random
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import vibaddr  # noqa: E402


class Frame(ctypes.Structure):
    _fields_ = [("id", ctypes.c_uint16), ("len", ctypes.c_uint8), ("d", ctypes.c_uint8 * 64)]


class Addr(ctypes.Structure):
    _fields_ = [("uid", ctypes.c_uint8 * 12), ("node", ctypes.c_uint8), ("name", ctypes.c_char * 16),
                ("fw", ctypes.c_uint16), ("muted", ctypes.c_uint8), ("identify", ctypes.c_uint8),
                ("store_req", ctypes.c_uint8)]


def build():
    d = tempfile.mkdtemp()
    so = os.path.join(d, "libvibaddr.so")
    subprocess.check_call(["cc", "-O1", "-shared", "-fPIC", "-Wall", "-Wextra", "-Werror", "-I", os.path.join(ROOT, "shared"),
                           "-o", so, os.path.join(ROOT, "shared", "vibaddr.c")])
    lib = ctypes.CDLL(so)
    lib.vibaddr_rx.argtypes = [ctypes.POINTER(Addr), ctypes.POINTER(Frame), ctypes.POINTER(Frame)]
    lib.vibaddr_tap.argtypes = [ctypes.POINTER(Addr), ctypes.POINTER(Frame)]
    return lib


class Sensor:
    """A sensor: its addressing state, and its flash (node, name)."""
    def __init__(self, lib, uid, flash=(0, b"")):
        self.lib, self.uid, self.flash = lib, uid, flash
        self.boot()

    def boot(self):  # power cycle: what the flash says
        self.a = Addr()
        self.a.uid[:] = list(self.uid)
        self.a.node, self.a.name, self.a.fw = self.flash[0], self.flash[1], 1

    def rx(self, can_id, data):
        f, out = Frame(), Frame()
        f.id, f.len = can_id, len(data)
        f.d[:len(data)] = list(data)
        r = self.lib.vibaddr_rx(ctypes.byref(self.a), ctypes.byref(f), ctypes.byref(out))
        if self.a.store_req:  # the firmware writes its flash config
            self.a.store_req = 0
            self.flash = (self.a.node, self.a.name)
        return (out.id, bytes(out.d[:out.len])) if r else None

    def tap(self):
        out = Frame()
        return (out.id, bytes(out.d[:out.len])) if self.lib.vibaddr_tap(ctypes.byref(self.a), ctypes.byref(out)) else None


class SimBus:
    def __init__(self, sensors):
        self.sensors, self.q, self.sent = sensors, [], 0

    def send(self, can_id, data):
        self.sent += 1
        for s in self.sensors:
            r = s.rx(can_id, data)
            if r and r not in self.q:  # the same frame from several sensors: one on the wire
                self.q.append(r)

    def recv(self, timeout):
        q, self.q = self.q, []
        return q


fails = checks = 0


def check(ok, what):
    global fails, checks
    checks += 1
    print("%s %d - %s" % ("ok" if ok else "not ok", checks, what))
    if not ok:
        fails += 1


def main():
    lib = build()
    rnd = random.Random(7)
    uids = [bytes(rnd.getrandbits(8) for _ in range(12)) for _ in range(4)]
    uids[1] = uids[0][:11] + bytes([uids[0][11] ^ 1])  # two IDs apart in the last bit only
    sensors = [Sensor(lib, u) for u in uids]
    bus = SimBus(sensors)
    a = vibaddr.Addresser(bus)

    found = a.scan()
    check(sorted(found) == sorted(uids), "scan finds all four new sensors, two of them one bit apart")
    check(bus.sent < 4 * 100 + 10, "scan: about 97 questions a sensor (%d frames)" % bus.sent)
    check(a.scan(all_sensors=False) and len(a.scan(all_sensors=False)) == 4, "all four have no number yet")

    cfg = {"sensors": []}
    rep = a.auto(cfg)
    nodes = sorted(s["node"] for s in cfg["sensors"])
    check(nodes == [1, 2, 3, 4], "auto: numbers 1..4 (%s)" % nodes)
    check(all(s.flash[0] in (1, 2, 3, 4) for s in sensors), "each sensor stored its number in flash")
    check(a.scan(all_sensors=False) == [], "none left without a number")
    check(len(rep) == 4 and all("new" in r for _, r in rep), "report says four new")

    # name the positions: tap each one
    for s, pos in zip(sensors, ("base", "X axis", "Y axis", "spindle")):
        bus.identify_tap = s
        a.bus.send(vibaddr.CMD_ID, vibaddr.fd_pad(bytes([vibaddr.IDENTIFY, 1])))
        bus.q.append(s.tap())  # the operator taps this one
        got = None
        for i, d in bus.recv(0):
            if d[0] == vibaddr.TAP:
                got = bytes(d[1:13])
        a.bus.send(vibaddr.CMD_ID, vibaddr.fd_pad(bytes([vibaddr.IDENTIFY, 0])))
        check(got == s.uid, "tap identifies the %s sensor" % pos)
        e = next(x for x in cfg["sensors"] if x["uid"] == got.hex())
        e["name"], e["position"] = pos, pos
    check(sensors[0].tap() is None, "no tap answers with identify off")
    rep = a.auto(cfg)
    check(all(s.flash[1] == p.encode() for s, p in zip(sensors, ("base", "X axis", "Y axis", "spindle"))),
          "names written to the sensors")

    # power cycle: everything as it was, nothing to do
    for s in sensors:
        s.boot()
    before = [dict(x) for x in cfg["sensors"]]
    rep = a.auto(cfg)
    check(rep == [] and cfg["sensors"] == before, "after a power cycle: nothing to do (%s)" % rep)
    q = a.query(sensors[3].uid)
    check(q["node"] == cfg["sensors"][[x["uid"] for x in cfg["sensors"]].index(sensors[3].uid.hex())]["node"]
          and q["name"] == "spindle", "query: node and name")

    # a known sensor whose flash was lost (reflashed): its number comes back
    sp = sensors[3]
    sp.flash = (0, b"")
    sp.boot()
    rep = a.auto(cfg)
    want = next(x for x in cfg["sensors"] if x["uid"] == sp.uid.hex())["node"]
    check(sp.flash[0] == want and sp.flash[1] == b"spindle", "a reflashed sensor gets its number and name back")

    # a sensor away: reported, its number reserved; a new one gets 5
    bus.sensors = sensors[:3]
    newu = bytes(rnd.getrandbits(8) for _ in range(12))
    newbie = Sensor(lib, newu)
    bus.sensors.append(newbie)
    rep = a.auto(cfg)
    check(any("missing" in r for _, r in rep), "the spindle sensor away: missing")
    check(newbie.flash[0] == 5, "a new sensor doesn't take the missing one's number: 5 (%d)" % newbie.flash[0])

    # the new one replaces the spindle sensor
    a.replace(cfg, sp.uid.hex(), newu.hex())
    check(newbie.flash == (want, b"spindle"), "replace: the new sensor takes over node %d and the name" % want)
    check(len(cfg["sensors"]) == 4 and sp.uid.hex() not in [x["uid"] for x in cfg["sensors"]], "the old one left the list")
    check(a.auto(cfg) == [], "then nothing to do")

    # a sensor from another machine, stored as node 2: parted from ours
    alien = Sensor(lib, bytes(rnd.getrandbits(8) for _ in range(12)), (2, b"other"))
    bus.sensors.append(alien)
    rep = a.auto(cfg)
    ours2 = next(x for x in cfg["sensors"] if x["node"] == 2)
    check(alien.flash[0] not in [x["node"] for x in cfg["sensors"] if x["uid"] != alien.uid.hex()] and alien.flash[0] != 2,
          "a sensor from elsewhere on node 2 moves to a free number (%d)" % alien.flash[0])
    check(next(s for s in bus.sensors if s.uid.hex() == ours2["uid"]).flash[0] == 2, "ours keeps node 2")
    nodes = [x["node"] for x in cfg["sensors"]]
    check(len(nodes) == len(set(nodes)), "no number twice (%s)" % nodes)

    # config file round trip
    d = tempfile.mkdtemp()
    p = os.path.join(d, "sensors.json")
    vibaddr.save_config(p, cfg)
    check(vibaddr.load_config(p) == cfg, "sensors.json round trip")
    check(vibaddr.load_config(os.path.join(d, "none.json")) == {"sensors": []}, "no file: empty list")

    print("1..%d" % checks)
    print("# %d/%d passed" % (checks - fails, checks))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
