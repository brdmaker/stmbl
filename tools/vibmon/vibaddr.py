"""The PC's side of the measurement bus's node numbers (shared/vibaddr.h,
docs/vibsensor.md §3.1): find every sensor by its unique ID, keep the
machine's sensor list (sensors.json), and give each sensor its number.

A bus is anything with send(can_id, data) and recv(timeout) -> [(can_id,
data), ...]: SocketCAN on the machine (SocketCanBus), a simulated bus in
the tests.

    a = Addresser(bus)
    a.scan()                      every sensor's uid (bytes)
    report = a.auto(config)       known sensors keep their numbers, new
                                  ones get free numbers; config updated
    uid = a.identify(10)          tap a sensor: its uid
"""
import json
import os
import struct

CMD_ID, RSP_ID = 0x7E5, 0x7E4
SCAN, YES, QUERY, INFO, SET, SET_OK, STORE, STORED, MUTE, UNMUTE, IDENTIFY, TAP = (
    0x10, 0x11, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29)
SET_NODE, SET_NAME = 1, 2
UID_BITS, NODE_MAX = 96, 15
FD_LENS = (8, 12, 16, 20, 24, 32, 48, 64)


def fd_pad(data):
    """Data padded to a CAN FD length."""
    n = len(data)
    if n <= 8:
        return bytes(data)
    return bytes(data) + bytes(next(l for l in FD_LENS if l >= n) - n)


def uid_hex(uid):
    return uid.hex()


class Addresser:
    def __init__(self, bus, timeout=0.003):
        self.bus = bus
        self.timeout = timeout  # s to wait for answers: sensors answer from their CAN interrupt

    def _ask(self, data, want):
        self.bus.send(CMD_ID, fd_pad(data))
        return [d for i, d in self.bus.recv(self.timeout) if i == RSP_ID and d and d[0] in want]

    def _scan_q(self, scope, n, prefix):
        return bool(self._ask(bytes([SCAN, scope, n]) + prefix.to_bytes(12, "big"), (YES,)))

    def _find_one(self, scope):
        """Binary search for one unmuted sensor's uid: 97 questions."""
        if not self._scan_q(scope, 0, 0):
            return None
        uid = 0
        for i in range(UID_BITS):
            if not self._scan_q(scope, i + 1, uid):  # bit i = 0? nobody: it is 1
                uid |= 1 << (UID_BITS - 1 - i)
        return uid.to_bytes(12, "big")

    def scan(self, all_sensors=True):
        """Every sensor's uid (all_sensors=False: those without a number)."""
        found = []
        self.bus.send(CMD_ID, fd_pad(bytes([UNMUTE])))
        while len(found) < 64:
            uid = self._find_one(1 if all_sensors else 0)
            if uid is None:
                break
            found.append(uid)
            self.bus.send(CMD_ID, fd_pad(bytes([MUTE]) + uid + b"\x01"))
        self.bus.send(CMD_ID, fd_pad(bytes([UNMUTE])))
        return found

    @staticmethod
    def _info(d):
        uid, node, fw = d[1:13], d[13], struct.unpack_from("<H", d, 14)[0]
        name = d[16:32].split(b"\0")[0].decode("utf-8", "replace")
        return {"uid": uid_hex(uid), "node": node, "fw": fw, "name": name}

    def query(self, uid):
        r = self._ask(bytes([QUERY]) + uid, (INFO,))
        r = [d for d in r if d[1:13] == uid]
        return self._info(r[0]) if r else None

    def set(self, uid, node=None, name=None):
        what = (SET_NODE if node is not None else 0) | (SET_NAME if name is not None else 0)
        nm = (name or "").encode()[:16].ljust(16, b"\0")
        r = self._ask(bytes([SET]) + uid + bytes([node or 0]) + nm + bytes([what]), (SET_OK,))
        r = [d for d in r if d[1:13] == uid]
        if not r:
            raise IOError("sensor %s: no answer to SET" % uid_hex(uid))
        return self._info(r[0])

    def store(self, uid):
        r = [d for d in self._ask(bytes([STORE]) + uid, (STORED,)) if d[1:13] == uid]
        if not r:
            raise IOError("sensor %s: no answer to STORE" % uid_hex(uid))
        return r[0][13]

    def identify(self, wait, poll=0.1):
        """Turn identify on, wait up to `wait` s for a tap; the tapped uid (or None)."""
        self.bus.send(CMD_ID, fd_pad(bytes([IDENTIFY, 1])))
        try:
            t = 0.0
            while t < wait:
                for i, d in self.bus.recv(poll):
                    if i == RSP_ID and d and d[0] == TAP:
                        return bytes(d[1:13])
                t += poll
            return None
        finally:
            self.bus.send(CMD_ID, fd_pad(bytes([IDENTIFY, 0])))

    def auto(self, config):
        """Bring the bus in line with config ({"sensors": [{uid, node, name}]}),
        updating config: a known sensor gets its number back, a new one keeps
        a free number it has or gets the lowest free one (the numbers of
        known sensors that are away are not free), and two on one number are
        parted. Returns [(uid, what)] of what was done or found."""
        sensors = config.setdefault("sensors", [])
        by_uid = {s["uid"]: s for s in sensors}
        found = {uid_hex(u): (u, self.query(u)) for u in self.scan(True)}
        report = []
        for s in sensors:
            if s["uid"] not in found:
                report.append((s["uid"], "missing (node %d, %s)" % (s["node"], s.get("name", ""))))
        # a known sensor's number stays its own while it is away (unplugged,
        # being serviced); replace() hands it to a sensor that takes its place
        used = {s["node"] for s in sensors}
        # known sensors first: they keep the numbers the machine's config gives them
        for h, (u, info) in sorted(found.items(), key=lambda kv: kv[0] not in by_uid):
            if h in by_uid:
                s = by_uid[h]
                if info["node"] != s["node"] or info["name"] != s.get("name", "")[:16]:
                    self.set(u, s["node"], s.get("name", ""))
                    self.store(u)
                    report.append((h, "set to node %d (was %d)" % (s["node"], info["node"])))
                continue
            node = info["node"]
            if not (1 <= node <= NODE_MAX) or node in used:
                free = [n for n in range(1, NODE_MAX + 1) if n not in used]
                if not free:
                    report.append((h, "no free node"))
                    continue
                was, node = node, free[0]
                name = info["name"] or "new %d" % node
                self.set(u, node, name)
                self.store(u)
                report.append((h, "new: node %d (was %d)" % (node, was)))
            else:
                name = info["name"] or "new %d" % node
                report.append((h, "new: kept node %d" % node))
            used.add(node)
            s = {"uid": h, "node": node, "name": name, "position": None}
            sensors.append(s)
            by_uid[h] = s
        return report

    def replace(self, config, old_uid, new_uid):
        """A new sensor takes a known one's place: its number, name and
        position. The old one leaves the list."""
        sensors = config["sensors"]
        old = next(s for s in sensors if s["uid"] == old_uid)
        new = next((s for s in sensors if s["uid"] == new_uid), None)
        u = bytes.fromhex(new_uid)
        self.set(u, old["node"], old.get("name", ""))
        self.store(u)
        if new:
            sensors.remove(new)
        sensors[sensors.index(old)] = dict(old, uid=new_uid)


def load_config(path):
    try:
        with open(path) as f:
            return json.load(f)
    except FileNotFoundError:
        return {"sensors": []}


def save_config(path, config):
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        json.dump(config, f, indent=2)
        f.write("\n")
    os.replace(tmp, path)


class SocketCanBus:
    """CAN FD over Linux SocketCAN (e.g. can1: the measurement bus)."""
    def __init__(self, ifname):
        import socket
        self.s = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
        self.s.setsockopt(socket.SOL_CAN_RAW, socket.CAN_RAW_FD_FRAMES, 1)
        self.s.bind((ifname,))

    def send(self, can_id, data):
        flags = 0x01 if len(data) > 8 else 0  # CANFD_BRS
        self.s.send(struct.pack("=IBBxx64s", can_id, len(data), flags, bytes(data)))

    def recv(self, timeout):
        import select
        import time
        out, end = [], time.monotonic() + timeout
        while True:
            left = end - time.monotonic()
            if left <= 0 or not select.select([self.s], [], [], left)[0]:
                return out
            fr = self.s.recv(72)
            can_id, n = struct.unpack_from("=IB", fr)
            out.append((can_id & 0x7FF, fr[8:8 + n]))
