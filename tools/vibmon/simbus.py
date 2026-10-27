"""A measurement bus of simulated sensors: each sensor is a host simulator
(host/stmbl_host) running the sensor's own components (vib, vnode, and the
board's template vibsensor/conf/vnode.txt) with vibsim in place of the
IIS3DWB. The bus steps them in lockstep: recv(t) runs every sensor t
seconds and collects what they sent. Same interface as SocketCanBus.

    bus = SimBus([SimSensor(uid_hex), ...])
    bus.sensors[0].cmd("vibsim0.unb = 1")
"""
import os
import re
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
HOST = os.path.join(ROOT, "host", "stmbl_host")
TEMPLATE = os.path.join(ROOT, "vibsensor", "conf", "vnode.txt")
FRAME_RE = re.compile(r"^vnout 0x([0-9a-f]+) (\d+) ([0-9a-f]*)$")
END_RE = re.compile(r"^vnout \d+ frames$")


class SimSensor:
    def __init__(self, uid_hex, host=HOST):
        self.uid = bytes.fromhex(uid_hex)
        self.p = subprocess.Popen([host], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True, bufsize=1)
        self.log = []
        self.alive = True
        lines = ["vnuid " + uid_hex, "load vibsim", "vibsim0.rt_prio = 1"]
        with open(TEMPLATE) as f:
            lines += [l.strip() for l in f if l.strip() and not l.startswith("#")]
        lines += ["vib0.temp = vibsim0.temp", "autonrt 1", "start"]
        for l in lines:
            self.cmd(l)
        self.step(0)

    def cmd(self, line):
        self.p.stdin.write(line + "\n")

    def deliver(self, can_id, data):
        self.cmd("vnin 0x%x %s" % (can_id, bytes(data).hex()))

    def step(self, dt):
        """Run dt seconds; the frames sent meanwhile [(id, data)]."""
        if dt > 0:
            self.cmd("run %g" % dt)
        self.cmd("vnout")
        self.p.stdin.flush()
        frames = []
        while True:
            line = self.p.stdout.readline()
            if not line:
                raise IOError("stmbl_host ended: " + "\n".join(self.log[-5:]))
            line = line.rstrip("\n")
            m = FRAME_RE.match(line)
            if m:
                frames.append((int(m.group(1), 16), bytes.fromhex(m.group(3))))
            elif END_RE.match(line):
                return frames
            elif not line.startswith("OK "):
                self.log.append(line)
                del self.log[:-200]

    def query(self, pin):
        """A pin's value (the console's answer)."""
        self.cmd(pin)
        self.step(0)
        for l in reversed(self.log):
            m = re.match(r"^%s(?: <= \S+)? = (\S+)$" % re.escape(pin), l)
            if m:
                return float(m.group(1))
        return None

    def close(self):
        if self.p.poll() is None:
            self.p.stdin.close()
            self.p.wait()


class SimBus:
    def __init__(self, sensors):
        self.sensors = list(sensors)
        self.t = 0.0

    def now(self):
        return self.t

    def send(self, can_id, data):
        for s in self.sensors:
            if s.alive:
                s.deliver(can_id, data)

    def recv(self, timeout):
        out = []
        for s in self.sensors:
            if s.alive:
                for f in s.step(timeout):
                    if f not in out:  # the same frame from several: one on the wire
                        out.append(f)
        self.t += timeout
        return out

    def close(self):
        for s in self.sensors:
            s.close()
