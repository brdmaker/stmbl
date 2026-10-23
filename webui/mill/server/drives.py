"""stmbl drives for the Debug page: each drive's console, owned by millweb.

A drive's console is a tty: the pty sserialterm makes of its smart-serial
console (docs/sserial_console.md), or its USB port. millweb keeps it open,
so it hears the drive's own messages (faults, enables) and can ask it for
values nobody wires to LinuxCNC (fault state and code, bus voltage,
temperatures). Browsers see the console as a terminal and may type into it.

    [MILLWEB]
    DRIVE_LINK = X /tmp/drive-x hm2_7i96s.0.stbl.0.0     (repeat per drive)

name, its console's tty, and optionally the HAL prefix of its smart-serial
process data (pos_fb, vel_fb, current, power, fault, enable).

The link is slow (a byte a servo period each way over smart-serial, ~1 kB/s),
so millweb asks one value at a time and only while the console is quiet, a
value every DRIVE_POLL_MS (250). Answers to its own questions don't show in
the browsers' console; everything else the drive prints does.

Commands from browsers: reading (about, show, showconf, a pin's name, ...)
for everyone; anything else (a pin set, start/stop, flashsaveconf, jogs,
resets, firmware) only with control and [MILLWEB] DRIVE_WRITE = yes, and the
ones that move the motor, restart the drive or write its flash only when the
browser says it is sure (the page asks).
"""
import os
import re
import time

# the drive's values on the card, asked in turn
POLL = ["fault0.state", "fault0.fault", "fault0.last_fault", "fault0.dc_volt", "fault0.hv_temp", "fault0.mot_temp",
        "fault0.ac_cur", "fault0.pos_error", "fault0.sat"]
# the drive's process data over smart-serial, read from HAL (prefix.<field>)
PD = ["enable", "fault", "pos_fb", "vel_fb", "current", "power"]

STATES = ["DISABLED", "ENABLED", "PHASING", "SOFT FAULT", "HARD FAULT", "LED TEST"]  # shared/common.h state_t
FAULTS = ["no error", "command error (link lost)", "motor feedback error", "commutation feedback error", "joint feedback error",
          "position error", "saturation", "motor over temperature", "HV CRC error", "HV timeout", "HV over temperature",
          "HV bus voltage", "HV fault", "HV current offset", "HV overcurrent (RMS)", "HV overcurrent (peak)",
          "HV overcurrent (hardware)", "phasing failed", "encoder monitor", "HV firmware version"]  # fault_t
HV_VERSION_FAULT = 19

# the drive's commands (COMMAND(...) in shared/ and src/) that only read
READ_CMDS = {"about", "help", "list", "show", "show_hal", "show_config", "showconf", "linked", "hal", "uptime", "uid",
             "confcrc", "scopestat", "fwstat", "crash"}
# writes that move the motor, stop it, restart the drive or rewrite its flash or firmware: the page asks first
DANGER_CMDS = {"flashsaveconf", "flashloadconf", "deleteconf", "loadconf", "bootloader", "sserialboot", "hardboot", "reset", "hv_update",
               "fwbegin", "fwdata", "fwend", "fwapply", "hv_update", "jogl", "jogr", "start", "stop", "enable", "disable",
               "load", "link", "relink", "fault"}
PIN_RE = re.compile(r"^[A-Za-z_][A-Za-z_0-9]*[0-9]+\.[A-Za-z_0-9]+$")          # a pin, comp0.pin (or a prefix of one)
VALUE_RE = re.compile(r"^([A-Za-z_][A-Za-z_0-9]*[0-9]+\.[A-Za-z_0-9]+)(?: <= \S+)? = (\S+)$")
TAG_RE = re.compile(r"<[^>]*>")
VERSION_RE = re.compile(r"^version fw=(-?\d+) hv=(-?\d+) hv_image=(-?\d+) conf=(-?\d+)")  # the firmware's `version` (docs/firmware_versions.md)
MAX_LINE = 63  # the drive's console takes 63 characters a line (a longer one is dropped)


def classify(cmd):
    """("read" | "write" | "danger", None) for a console line, or (None, why not)."""
    if not cmd or len(cmd) > MAX_LINE:
        return None, "a line of 1 to %d characters (the drive drops longer ones)" % MAX_LINE
    if any(ord(ch) < 32 or ord(ch) > 126 for ch in cmd):
        return None, "plain printable characters only"
    word = cmd.split()[0]
    if "=" in cmd:
        return "write", None
    if PIN_RE.match(cmd.strip()):
        return "read", None
    if word in READ_CMDS and not (word == "crash" and cmd.split()[1:]):  # "crash clear" clears
        return "read", None
    if word in DANGER_CMDS:
        return "danger", None
    return "write", None


class DriveLink:
    """One drive's console in millweb's event loop: open, read, poll, write."""

    SCROLLBACK = 32000  # characters kept for a browser that opens the console

    def __init__(self, name, path, pd=None, loop=None, out=None, event=None, poll_ms=250):
        self.name, self.path, self.pd = name, path, pd
        self.loop, self.out, self.event = loop, out or (lambda d, text: None), event or (lambda lvl, text: None)
        self.poll_s = max(0.05, poll_ms / 1000.0)
        self.fd = None
        self.buf = b""
        self.scroll = ""
        self.values, self.when = {}, {}
        self.fw = None
        self.state = "closed"            # closed | open | detached | missing
        self.detached_by, self.detached_until = None, 0.0
        self.rx = self.tx = 0
        self.last_rx = self.last_tx = self.last_user = 0.0
        self.pending = {}                # pin asked -> when
        self.poll_i = 0
        self.want_about = True
        self.about_next = False
        self.fault_seen = None           # (code, time): the drive's last "ERROR: Fault n" (a fault often clears before a poll)
        self.ver = None                  # {fw, hv, hv_image, conf}: the drive's `version`; fw 0: from before versioning
        self.ver_due = 0.0               # when to ask again (the HV board's needs a moment after a start)
        self.ver_asked = False
        self.busy = False                # a config job runs (driveconf.py): no polling meanwhile
        self.capture = None              # a job's list: every line the drive says
        self.err = None

    # ------------------------------------------------------------ the tty
    def open(self):
        import termios
        import tty
        if self.fd is not None:
            return True
        try:
            fd = os.open(self.path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        except OSError as e:
            if self.state != "missing":
                self.err = str(e)
            self.state = "missing"
            return False
        try:
            if os.isatty(fd):
                tty.setraw(fd)
                attr = termios.tcgetattr(fd)
                attr[3] &= ~termios.ECHO
                termios.tcsetattr(fd, termios.TCSANOW, attr)
        except termios.error:
            pass
        self.fd, self.state, self.err, self.buf = fd, "open", None, b""
        self.want_about = True
        if self.loop:
            self.loop.add_reader(fd, self.readable)
        return True

    def close(self):
        if self.fd is None:
            return
        if self.loop:
            try:
                self.loop.remove_reader(self.fd)
            except (ValueError, OSError):
                pass
        try:
            os.close(self.fd)
        except OSError:
            pass
        self.fd = None
        self.pending.clear()

    def detach(self, who, seconds=600):
        """Let another program have the tty (servoterm-cli --dump, an update): closed until attach, at most `seconds`."""
        self.close()
        self.state, self.detached_by, self.detached_until = "detached", who, time.time() + seconds

    def attach(self):
        self.state, self.detached_by = "closed", None
        return self.open()

    def readable(self):
        try:
            data = os.read(self.fd, 4096)
        except BlockingIOError:
            return
        except OSError as e:  # the pty went (LinuxCNC stopped)
            self.close()
            self.state, self.err = "missing", str(e)
            return
        if not data:
            return
        self.feed(data)

    def write(self, line):
        if self.fd is None:
            return "the drive's console is %s" % ("detached" if self.state == "detached" else "not open (%s)" % (self.err or self.path))
        data = (line + "\n").encode()
        try:
            os.write(self.fd, data)
        except OSError as e:
            return "the drive's console: %s" % e
        self.tx += len(data)
        self.last_tx = time.time()
        return None

    # ------------------------------------------------------------ what the drive says
    def feed(self, data, now=None):
        now = now or time.time()
        self.rx += len(data)
        self.last_rx = now
        self.buf += data
        *lines, self.buf = self.buf.split(b"\n")
        shown = []
        for raw in lines:
            line = raw.decode("utf-8", "replace").rstrip("\r")
            if self.capture is not None:
                self.capture.append(line)
            if self.line(line, now):
                shown.append(line)
        if self.buf and len(self.buf) > 512:  # no newline for a long time: show it as it is
            shown.append(self.buf.decode("utf-8", "replace"))
            self.buf = b""
        if shown:
            text = "\n".join(shown) + "\n"
            self.scroll = (self.scroll + text)[-self.SCROLLBACK:]
            self.out(self, text)

    def line(self, line, now):
        """Take in one line; True if the console should show it."""
        m = VALUE_RE.match(line.strip())
        if m:
            try:
                v = float(m.group(2))
            except ValueError:
                v = None
            if v is not None:
                self.values[m.group(1)] = v
                self.when[m.group(1)] = now
                if m.group(1) == "fault0.fault" and int(v) == HV_VERSION_FAULT and (self.ver or {}).get("hv") == (self.ver or {}).get("hv_image"):
                    self.ver_due = 0  # the HV board's version changed: ask
            if m.group(1) in self.pending:
                del self.pending[m.group(1)]
                return False  # our own question's answer
            return True
        vm = VERSION_RE.match(line.strip())
        if vm or line.strip() == "not found: version":  # this firmware's versions, or one from before them
            self.ver = dict(zip(("fw", "hv", "hv_image", "conf"), (int(x) for x in vm.groups()))) if vm else {"fw": 0, "hv": None, "hv_image": None, "conf": None}
            self.ver_due = now + (5 if vm and self.ver["hv"] < 0 else 10)
            mine, self.ver_asked = self.ver_asked, False
            return not mine
        if self.about_next:
            self.about_next = False
            self.fw = line.strip()[:120]
        if line.startswith("######## software info"):
            self.about_next = True
        text = TAG_RE.sub("", line).strip()
        mm = re.match(r"^(INFO|WARNING|ERROR):\s*(.*)$", text)
        if mm and mm.group(2):
            f = re.match(r"^Fault (\d+)", mm.group(2))
            if mm.group(1) == "ERROR" and f:
                self.fault_seen = (int(f.group(1)), now)
                if int(f.group(1)) == HV_VERSION_FAULT:
                    self.ver_due = 0  # its versions now
            self.event({"INFO": "info", "WARNING": "warn", "ERROR": "bad"}[mm.group(1)], "Drive %s: %s" % (self.name, mm.group(2)))
        return True

    # ------------------------------------------------------------ asking
    def tick(self, now=None):
        """Called often (every 50 ms): reopen, poll a value when the console is quiet."""
        now = now or time.time()
        if self.state == "detached":
            if now < self.detached_until:
                return
            self.attach()
        if self.fd is None:
            if now - getattr(self, "_tried", 0) > 2.0:
                self._tried = now
                self.open()
            return
        for k, t in list(self.pending.items()):
            if now - t > 2.0:  # no answer (link down, drive busy): ask again later
                del self.pending[k]
        quiet = now - self.last_rx > 0.15 and now - self.last_user > 0.5 and now - self.last_tx >= self.poll_s
        if not quiet or self.pending:
            return
        if self.busy:
            return
        if self.want_about:
            self.want_about = False
            self.write("about")
            return
        if now >= self.ver_due and not self.ver_asked:
            self.ver_due = now + 10  # asked again if no answer
            if not self.write("version"):
                self.ver_asked = True
            return
        pin = POLL[self.poll_i % len(POLL)]
        self.poll_i += 1
        if not self.write(pin):
            self.pending[pin] = now

    def user(self, line):
        """A browser's line: written now (the polling waits)."""
        self.last_user = time.time()
        err = self.write(line)
        if not err:
            echo = "> " + line + "\n"
            self.scroll = (self.scroll + echo)[-self.SCROLLBACK:]
            self.out(self, echo)
        return err

    # ------------------------------------------------------------ the card
    def status(self, pin=None, now=None):
        now = now or time.time()
        v = self.values
        age = (now - max(self.when.values())) if self.when else None
        state = int(v["fault0.state"]) if "fault0.state" in v else None
        fault = int(v["fault0.fault"]) if "fault0.fault" in v else None
        last = int(v["fault0.last_fault"]) if "fault0.last_fault" in v else None
        if self.fault_seen:
            last = self.fault_seen[0]
        r = lambda k, n=1: round(v[k], n) if k in v else None  # noqa: E731
        out = {
            "n": self.name, "tty": self.path, "link": self.state if self.state != "open" else ("ok" if age is not None and age < 5 else "silent"),
            "by": self.detached_by, "err": self.err if self.state == "missing" else None,
            "age": round(age, 1) if age is not None else None, "fw": self.fw, "rx": self.rx, "tx": self.tx,
            "state": STATES[state] if state is not None and 0 <= state < len(STATES) else None,
            "fault": FAULTS[fault] if fault and 0 <= fault < len(FAULTS) else None, "fault_n": fault,
            "last": FAULTS[last] if last and 0 <= last < len(FAULTS) else None,
            "last_at": time.strftime("%H:%M:%S", time.localtime(self.fault_seen[1])) if self.fault_seen else None,
            "dc": r("fault0.dc_volt"), "hv_t": r("fault0.hv_temp"), "mot_t": r("fault0.mot_temp"), "ac": r("fault0.ac_cur", 2),
            "perr": r("fault0.pos_error", 4), "sat": r("fault0.sat", 3),
            "ver": self.ver, "busy": self.busy,
        }
        if self.pd and pin:
            out["pd"] = {k: pin("%s.%s" % (self.pd, k)) for k in PD}
        return out


def links_from(cfg_values):
    """[MILLWEB] DRIVE_LINK lines -> [(name, tty, pd prefix or None)]."""
    out = []
    for line in cfg_values:
        w = line.split()
        if len(w) >= 2:
            out.append((w[0], w[1], w[2] if len(w) > 2 else None))
    return out
