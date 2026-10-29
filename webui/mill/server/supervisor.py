"""millweb-supervisor: runs LinuxCNC for a machine folder, again and again.

    millweb-supervisor MACHINE_DIR [--port N] [--linuxcnc CMD]

Each start:
  1. machine.json is checked and made into run/mill.ini and run/machine.hal
     (machineconf.py);
  2. the FPGA's bitstream is loaded (bitstream.loader: fpga_manager or a
     command; with reload "changed" only when it isn't loaded already since
     this boot);
  3. the machine's own HAL components, comp/*.comp, are compiled (cached) and
     installed (comps.py);
  4. `linuxcnc run/mill.ini` runs, with millweb as its display, until it ends.

Then: run/request says why it ended. "restart" (the console's Restart, after a
save) starts again at once; "stop" waits for a Start; nothing (a crash, a
config LinuxCNC refused) starts again after a pause, and after three quick
failures in a row waits for a Start.

While LinuxCNC isn't running the supervisor serves the console on millweb's
port itself: the same page, which shows the Configure page, the state and
the last start's log, and can start LinuxCNC again. run/status.json says
what it is doing; logs/linuxcnc.log holds LinuxCNC's output (and millweb's),
the start before in linuxcnc.1.log.
"""
import argparse
import asyncio
import base64
import binascii
import hashlib
import json
import os
import re
import shlex
import signal
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
MILL = os.path.dirname(HERE)
WEBROOT = MILL
sys.path.insert(0, HERE)
import backup  # noqa: E402
import comps  # noqa: E402
import machineconf
import sysupdate  # noqa: E402
import wsserver  # noqa: E402

VERSION = "0.1"


def now():
    return time.strftime("%H:%M:%S")


def boot_id():
    try:
        return open("/proc/sys/kernel/random/boot_id").read().strip()
    except OSError:
        return ""


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


# ------------------------------------------------------------------ the bitstream
def load_bitstream(m, mdir, log):
    """Load machine m's bitstream. Returns a note (str) or raises RuntimeError."""
    bs = m["bitstream"]
    if bs["loader"] == "none":
        return None
    path = os.path.join(mdir, "bitstreams", bs["file"])
    if not os.path.isfile(path):
        raise RuntimeError("bitstream %s isn't there" % path)
    digest = sha256(path)
    state_file = os.path.join(mdir, "run", "bitstream.json")
    key = {"loader": bs["loader"], "fpga": bs["fpga"], "command": bs["command"], "sha256": digest, "boot": boot_id()}
    try:
        old = json.load(open(state_file))
    except (OSError, ValueError):
        old = {}
    if bs["reload"] == "changed" and {k: old.get(k) for k in key} == key:
        return "bitstream %s already loaded (sha256 %s…)" % (bs["file"], digest[:12])
    if bs["loader"] == "fpga_manager":
        sudo = os.environ.get("MILLWEB_SUDO", "sudo -n" if os.geteuid() != 0 else "")
        cmd = shlex.split(sudo) + [sys.executable, os.path.join(HERE, "fpga_load.py"), "--fpga", bs["fpga"]]
        if os.environ.get("MILLWEB_FPGA_SYSFS"):
            cmd += ["--sysfs", os.environ["MILLWEB_FPGA_SYSFS"]]
        if os.environ.get("MILLWEB_FIRMWARE_DIR"):
            cmd += ["--firmware-dir", os.environ["MILLWEB_FIRMWARE_DIR"]]
        cmd.append(path)
    else:
        cmd = shlex.split(bs["command"].replace("{file}", shlex.quote(path)))
    log("$ %s\n" % " ".join(shlex.quote(c) for c in cmd))
    t0 = time.time()
    try:
        r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=180, cwd=mdir)
    except (OSError, subprocess.TimeoutExpired) as e:
        raise RuntimeError("loading the bitstream: %s" % e)
    out = r.stdout.decode(errors="replace")
    log(out)
    if r.returncode != 0:
        tail = out.strip().splitlines()[-1:] or ["exit %d" % r.returncode]
        raise RuntimeError("loading the bitstream failed: %s" % tail[0])
    json.dump(dict(key, file=bs["file"], time=time.time()), open(state_file, "w"))
    return "bitstream %s loaded in %.1f s" % (bs["file"], time.time() - t0)


# ------------------------------------------------------------------ the Configure page's operations
class ConfigOps:
    """machine.json, custom.hal, history and bitstreams for the console: the
    same in the supervisor (LinuxCNC down) and in millweb (LinuxCNC up)."""

    def __init__(self, mdir):
        self.mdir = mdir

    def get(self):
        text, custom = machineconf.read(self.mdir)
        return {"text": text, "custom": custom, "default": json.dumps(machineconf.DEFAULT, indent=2) + "\n",
                "dir": self.mdir, "bitstreams": self.bitstreams(), "models": self.models(), "history": machineconf.history(self.mdir)[:30],
                "running": self.running_hash(), "saved": hashlib.sha256((text + "\0" + custom).encode()).hexdigest()}

    def running_hash(self):
        try:
            return json.load(open(os.path.join(self.mdir, "run", "status.json"))).get("config")
        except (OSError, ValueError):
            return None

    def check(self, text, custom):
        """Problems as [[where, what]], the generated files, and the change against the saved file."""
        saved, saved_custom = machineconf.read(self.mdir)
        out = {"diff": machineconf.diff(saved, text) + machineconf.diff(saved_custom, custom or "", "custom.hal")}
        try:
            r = machineconf.check(self.mdir, text, custom or "", MILL)
            out.update(ok=True, errors=[], files=r["files"], machine=r["machine"])
        except machineconf.Invalid as e:
            out.update(ok=False, errors=[list(x) for x in e.errors], files={})
        return out

    def save(self, text, custom, who, note=""):
        try:
            hid = machineconf.save(self.mdir, text, custom or "", MILL, who, note)
        except machineconf.Invalid as e:
            return "the machine has problems: %s" % e, None
        return None, hid

    def version(self, hid):
        text, custom = machineconf.history_get(self.mdir, hid)
        return {"text": text, "custom": custom}

    # the machine's 3D model files (models/), for the console's 3D view
    MODEL_MAX = 6 * 1024 * 1024

    def models(self):
        d = os.path.join(self.mdir, "models")
        try:
            return [{"name": n, "size": os.path.getsize(os.path.join(d, n))} for n in sorted(os.listdir(d))
                    if os.path.isfile(os.path.join(d, n)) and re.search(r"\.(stl|obj)$", n, re.I)]
        except FileNotFoundError:
            return []

    def model_name(self, name):
        if not re.match(r"^[A-Za-z0-9_.\- ]{1,80}\.(stl|obj)$", str(name), re.I) or str(name).startswith("."):
            raise ValueError("an .stl or .obj file name (letters, digits, space . _ -)")
        return os.path.join(self.mdir, "models", name)

    def model_save(self, name, data):
        try:
            path = self.model_name(name)
            raw = base64.b64decode(str(data), validate=True)
        except (ValueError, binascii.Error) as e:
            return str(e) if isinstance(e, ValueError) and "file name" in str(e) else "the file's data isn't base64"
        if not raw or len(raw) > self.MODEL_MAX:
            return "1 byte to 6 MB (fewer triangles, or split it into parts)"
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path + ".tmp", "wb") as f:
            f.write(raw)
        os.replace(path + ".tmp", path)
        return None

    def model_get(self, name):
        path = self.model_name(name)
        with open(path, "rb") as f:
            return {"name": name, "data": base64.b64encode(f.read()).decode()}

    def model_delete(self, name):
        path = self.model_name(name)
        text, _ = machineconf.read(self.mdir)
        try:
            if any(p.get("file") == name for p in json.loads(text).get("model", {}).get("parts", [])):
                return "machine.json's 3D model uses it: take it out first"
        except (ValueError, AttributeError):
            pass
        try:
            os.remove(path)
        except FileNotFoundError:
            return "no such file"
        return None

    def bitstreams(self):
        d = os.path.join(self.mdir, "bitstreams")
        try:
            return [{"name": n, "size": os.path.getsize(os.path.join(d, n))} for n in sorted(os.listdir(d))
                    if os.path.isfile(os.path.join(d, n)) and not n.startswith(".")]
        except FileNotFoundError:
            return []

    def bitstream_save(self, name, data):
        if not re.match(r"^[A-Za-z0-9_.\-]{1,80}$", str(name)) or name.startswith("."):
            return "a plain file name (letters, digits, . _ -)"
        try:
            raw = base64.b64decode(str(data), validate=True)
        except (ValueError, binascii.Error):
            return "the file's data isn't base64"
        if not raw:
            return "an empty file"
        d = os.path.join(self.mdir, "bitstreams")
        os.makedirs(d, exist_ok=True)
        with open(os.path.join(d, name + ".tmp"), "wb") as f:
            f.write(raw)
        os.replace(os.path.join(d, name + ".tmp"), os.path.join(d, name))
        return None

    def bitstream_delete(self, name):
        if not re.match(r"^[A-Za-z0-9_.\-]{1,80}$", str(name)):
            return "no such file"
        text, _ = machineconf.read(self.mdir)
        try:
            if json.loads(text).get("bitstream", {}).get("file") == name:
                return "machine.json loads it: choose another first"
        except ValueError:
            pass
        try:
            os.remove(os.path.join(self.mdir, "bitstreams", name))
        except FileNotFoundError:
            return "no such file"
        return None

    # the whole folder as a .zip, and a restore from one (backup.py)
    def backup_get(self, programs=True):
        name, data = backup.make(self.mdir, programs)
        return {"name": name, "size": len(data), "data": base64.b64encode(data).decode()}

    def restore_upload(self, offset, data, total):
        """a piece of a backup; with the last: what restoring it would change"""
        try:
            raw = base64.b64decode(str(data), validate=True)
        except (ValueError, binascii.Error):
            raise ValueError("the file's data isn't base64")
        try:
            path = backup.upload(self.mdir, int(offset), raw, int(total))
            if path is None:
                return {"received": int(offset) + len(raw)}
            return {"received": int(total), "restore": backup.inspect(self.mdir, path, self.restore_problems)}
        except backup.Error as e:
            raise ValueError(str(e))

    def restore_problems(self, text, custom):
        r = self.check(text, custom)
        return r["errors"]

    def restore_queue(self):
        """the uploaded backup becomes the one the next start restores (LinuxCNC down: it writes linuxcnc.var)"""
        part = backup.part_path(self.mdir)
        if not os.path.exists(part):
            raise ValueError("no backup uploaded")
        try:
            backup.inspect(self.mdir, part)
        except backup.Error as e:
            raise ValueError(str(e))
        os.replace(part, backup.pending_path(self.mdir))

    def log(self, which=0, n=400):
        p = os.path.join(self.mdir, "logs", "linuxcnc.log" if not which else "linuxcnc.%d.log" % which)
        try:
            lines = open(p, errors="replace").read().splitlines()
        except FileNotFoundError:
            lines = []
        return {"lines": lines[-n:], "total": len(lines)}

    def status(self):
        try:
            return json.load(open(os.path.join(self.mdir, "run", "status.json")))
        except (OSError, ValueError):
            return {"state": "unknown"}

    def request(self, what):
        """millweb's way to tell the supervisor why LinuxCNC is about to end."""
        p = os.path.join(self.mdir, "run", "request")
        with open(p + ".tmp", "w") as f:
            f.write(what)
        os.replace(p + ".tmp", p)


# ------------------------------------------------------------------ the supervisor
class Client:
    n = 0

    def __init__(self, ws):
        Client.n += 1
        self.id = "s%d" % Client.n
        self.ws = ws
        self.name = "%s:%s" % ws.peer[:2] if ws.peer else self.id


class Supervisor:
    def __init__(self, mdir, port=None, linuxcnc="linuxcnc", host=None):
        self.mdir = os.path.abspath(mdir)
        self.ops = ConfigOps(self.mdir)
        self.port_override, self.host_override = port, host
        self.linuxcnc = linuxcnc
        self.want = "run"            # run | stop
        self.state = "starting"      # starting | loading | running | stopping | restarting | stopped | failed
        self.since = time.time()
        self.error = None
        self.note = None
        self.starts = 0
        self.fails = 0
        self.config = None           # the hash of what LinuxCNC runs
        self.good = None             # the hash of the last configuration LinuxCNC ran with (its console answered)
        self.proc = None
        self.server = None
        self.clients = {}
        self.control = None
        self.events = []
        self.wake = asyncio.Event()
        self.quit = asyncio.Event()
        self.logf = None
        machineconf.init(self.mdir)
        self.sys = sysupdate.SysUpdate(self.broadcast, self.event)  # while this serves the console

    # -------------------------------------------------- state
    def status(self):
        return {"state": self.state, "since": self.since, "error": self.error, "note": self.note, "starts": self.starts,
                "want": self.want, "config": self.config, "pid": self.proc.pid if self.proc and self.proc.returncode is None else None,
                "version": VERSION, "dir": self.mdir}

    def set_state(self, state, error=None, note=None):
        self.state, self.since, self.error = state, time.time(), error
        if note is not None:
            self.note = note
        st = self.status()
        p = os.path.join(self.mdir, "run", "status.json")
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p + ".tmp", "w") as f:
            json.dump(st, f)
        os.replace(p + ".tmp", p)
        self.broadcast({"t": "sup", "status": st})
        self.event("bad" if state == "failed" else "info", "LinuxCNC: %s%s" % (state, " (%s)" % error if error else ""))

    def event(self, lvl, text):
        e = {"time": now(), "lvl": lvl, "text": text}
        self.events.append(e)
        del self.events[:-200]
        self.broadcast(dict(e, t="ev"))
        print("millweb-supervisor: %s %s" % (lvl, text), flush=True)

    def broadcast(self, msg):
        data = json.dumps(msg, separators=(",", ":"))
        for c in list(self.clients.values()):
            asyncio.ensure_future(c.ws.send(data))

    def log(self, text):
        if self.logf:
            self.logf.write(text)
            self.logf.flush()

    def open_log(self):
        d = os.path.join(self.mdir, "logs")
        os.makedirs(d, exist_ok=True)
        if self.logf:
            self.logf.close()
        for i in (2, 1):  # linuxcnc.log -> .1 -> .2
            src = os.path.join(d, "linuxcnc.log" if i == 1 else "linuxcnc.%d.log" % (i - 1))
            if os.path.exists(src):
                os.replace(src, os.path.join(d, "linuxcnc.%d.log" % i))
        self.logf = open(os.path.join(d, "linuxcnc.log"), "w")
        self.log("== %s millweb-supervisor: start %d of %s\n" % (time.strftime("%Y-%m-%d %H:%M:%S"), self.starts, self.mdir))

    def machine(self):
        """machine.json, checked, with the supervisor's port; and its hash."""
        text, custom = machineconf.read(self.mdir)
        m = machineconf.validate(machineconf.parse(text), self.mdir)
        if self.port_override:
            m["console"]["port"] = self.port_override
        if self.host_override:
            m["console"]["host"] = self.host_override
        return m, hashlib.sha256((text + "\0" + custom).encode()).hexdigest()

    def port(self):
        try:
            return self.machine()[0]["console"]["port"]
        except (machineconf.Invalid, OSError):
            return self.port_override or 8080

    # -------------------------------------------------- the loop
    async def run(self):
        while not self.quit.is_set():
            if self.want == "stop":
                if self.state not in ("stopped", "failed"):
                    self.set_state("stopped")
                await self.serve()
                self.wake.clear()
                await self.wait(self.wake.wait())
                continue
            await self.serve()
            self.starts += 1
            self.open_log()
            self.set_state("loading", note="")
            pending = backup.pending_path(self.mdir)
            if os.path.exists(pending):
                try:
                    note = await asyncio.get_event_loop().run_in_executor(None, backup.apply, self.mdir, pending)
                    self.log("== %s\n" % note)
                    self.event("ok", note)
                except (backup.Error, OSError, ValueError) as e:
                    self.log("== restore failed: %s\n" % e)
                    self.event("bad", "Restore failed: %s (the folder before it is in backups/)" % e)
                finally:
                    try:
                        os.remove(pending)
                    except OSError:
                        pass
            try:
                m, digest = self.machine()
                machineconf.write_run(self.mdir, machineconf.generate(m, self.mdir, MILL))
            except machineconf.Invalid as e:
                self.log("machine.json: %s\n" % e)
                self.want = "stop"
                self.set_state("failed", "machine.json: %s" % e)
                continue
            try:
                req = os.path.join(self.mdir, "run", "request")
                if os.path.exists(req):
                    os.remove(req)
                note = await asyncio.get_event_loop().run_in_executor(None, load_bitstream, m, self.mdir, self.log)
                if note:
                    self.event("info", note)
            except RuntimeError as e:
                self.want = "stop"
                self.set_state("failed", str(e))
                continue
            try:
                note = await asyncio.get_event_loop().run_in_executor(None, comps.build, self.mdir, self.log)
                if note:
                    self.event("info", note)
            except comps.Error as e:
                self.log("%s\n" % e)
                self.want = "stop"
                self.set_state("failed", str(e))
                continue
            await self.unserve()
            ran = await self.run_linuxcnc(digest)
            if self.quit.is_set():
                break
            why = ""
            try:
                why = open(os.path.join(self.mdir, "run", "request")).read().strip()
                os.remove(os.path.join(self.mdir, "run", "request"))
            except OSError:
                pass
            self.config = None
            if why == "restart":
                self.fails = 0
                self.set_state("restarting")
                continue
            if why == "stop" or self.want == "stop":
                self.want = "stop"
                self.fails = 0
                continue
            # it ended by itself
            self.fails = self.fails + 1 if ran < 60 else 1
            err = self.why_failed()
            if digest != self.good:  # a configuration that never ran: trying again won't help
                self.want = "stop"
                self.set_state("failed", "LinuxCNC didn't start with this configuration: %s" % err)
                continue
            if self.fails >= 3:
                self.want = "stop"
                self.set_state("failed", "LinuxCNC ended %d times in a row: %s" % (self.fails, err.strip()[:200]))
                continue
            self.set_state("restarting", "LinuxCNC ended after %d s: %s" % (ran, err.strip()[:200]))
            await self.serve()
            await self.wait(asyncio.sleep(2 * self.fails))

    def why_failed(self):
        """The likeliest line of the log: the first error LinuxCNC printed."""
        lines = self.ops.log(0, 400)["lines"]
        pat = re.compile(r"error|failed|can't|cannot|not found|no such|invalid|refus|does not exist", re.I)
        hits = [l.strip() for l in lines if pat.search(l) and not l.startswith("==") and "terminated with an error" not in l]
        hal = [l for l in hits if re.search(r"\.hal:\d+:", l)]  # a HAL file's line: the most telling
        hits = hal + hits
        return (hits[0] if hits else (lines[-2].strip() if len(lines) > 1 else "see the log"))[:240]

    async def wait(self, aw):
        """aw, or until quit."""
        t = asyncio.ensure_future(aw)
        q = asyncio.ensure_future(self.quit.wait())
        await asyncio.wait([t, q], return_when=asyncio.FIRST_COMPLETED)
        for x in (t, q):
            if not x.done():
                x.cancel()

    async def run_linuxcnc(self, digest):
        """LinuxCNC until it ends; the seconds it ran."""
        import pty
        ini = os.path.join(self.mdir, "run", "mill.ini")
        env = dict(os.environ, MILLWEB_SUPERVISED="1")
        env["PATH"] = comps.bin_dir(self.mdir) + os.pathsep + env.get("PATH", "")  # userspace comps: loadusr NAME
        env.pop("DISPLAY", None)  # no X error windows
        t0 = time.time()
        # on a terminal: the linuxcnc script sends its output to temporary
        # files it deletes, unless it runs on one; this keeps it, in the log
        master, slave = pty.openpty()
        loop = asyncio.get_event_loop()

        def readable():
            try:
                data = os.read(master, 65536)
            except OSError:
                data = b""
            if not data:
                loop.remove_reader(master)
                return
            self.log(data.decode(errors="replace").replace("\r\n", "\n"))
        loop.add_reader(master, readable)
        try:
            self.proc = await asyncio.create_subprocess_exec(*shlex.split(self.linuxcnc), ini, cwd=os.path.join(self.mdir, "run"),
                                                             stdin=slave, stdout=slave, stderr=slave, env=env, start_new_session=True)
        finally:
            os.close(slave)
        self.config = digest
        self.set_state("starting")
        port = self.port()
        up = False
        while self.proc.returncode is None and not self.quit.is_set():
            if not up:
                try:
                    _, w = await asyncio.open_connection("127.0.0.1", port)
                    w.close()
                    up = True
                    self.good = digest
                    self.set_state("running")
                except OSError:
                    pass
            try:
                await asyncio.wait_for(self.proc.wait(), 0.3 if not up else 1.0)
            except asyncio.TimeoutError:
                pass
        if self.quit.is_set() and self.proc.returncode is None:
            await self.stop_linuxcnc("stop")
        await self.proc.wait()
        await asyncio.sleep(0.2)  # the last of its output
        try:
            loop.remove_reader(master)
        except (ValueError, OSError):
            pass
        os.close(master)
        self.log("== LinuxCNC ended: %s\n" % self.proc.returncode)
        return int(time.time() - t0)

    def millweb_pids(self):
        """millweb (LinuxCNC's display) in LinuxCNC's session."""
        out = []
        if not self.proc:
            return out
        for p in os.listdir("/proc"):
            if not p.isdigit():
                continue
            try:
                if os.getsid(int(p)) != self.proc.pid:
                    continue
                cmd = open("/proc/%s/cmdline" % p, "rb").read().split(b"\0")
            except OSError:
                continue
            if any(c.endswith(b"/millweb") for c in cmd):
                out.append(int(p))
        return out

    async def stop_linuxcnc(self, why):
        """LinuxCNC ends: its display (millweb) quits, LinuxCNC follows."""
        self.ops.request(why)
        pids = self.millweb_pids()
        for p in pids:
            try:
                os.kill(p, signal.SIGTERM)
            except OSError:
                pass
        try:
            await asyncio.wait_for(self.proc.wait(), 40)
        except asyncio.TimeoutError:
            try:
                os.killpg(self.proc.pid, signal.SIGTERM)
            except OSError:
                pass

    # -------------------------------------------------- the console while LinuxCNC is down
    async def serve(self):
        if self.server:
            return
        port = self.port()
        try:
            host = self.machine()[0]["console"]["host"]
        except (machineconf.Invalid, OSError):
            host = self.host_override or "0.0.0.0"
        for _ in range(100):  # millweb may still hold it a moment
            try:
                self.server = await wsserver.Server(WEBROOT, self.handle).serve(host, port)
                self.sys.start()
                return
            except OSError:
                await asyncio.sleep(0.2)
        self.event("bad", "can't serve the console on port %d" % port)

    async def unserve(self):
        if not self.server:
            return
        self.sys.stop()
        self.server.close()
        for c in list(self.clients.values()):
            try:
                await c.ws.close(1012)  # restarting: the page connects again, to millweb
            except Exception:
                pass
        await self.server.wait_closed()
        self.server = None

    async def handle(self, ws, query):
        c = Client(ws)
        self.clients[c.id] = c
        try:
            token = self.machine()[0]["console"]["token"]
        except (machineconf.Invalid, OSError):
            token = ""
        await ws.send_json({"t": "hello", "you": c.id, "machine": None, "supervisor": self.status(), "control": self.control,
                            "token_required": bool(token), "events": self.events[-60:],
                            "sysupdate": self.sys.enabled})
        if self.sys.status is not None:
            await ws.send_json(self.sys.snapshot())
        try:
            while True:
                text = await ws.recv()
                if text is None:
                    break
                try:
                    msg = json.loads(text)
                except ValueError:
                    continue
                if isinstance(msg, dict):
                    asyncio.ensure_future(self.on_message(c, msg, token))
        finally:
            self.clients.pop(c.id, None)
            if self.control == c.id:
                self.control = None
                self.broadcast({"t": "control", "holder": None})

    async def on_message(self, c, m, token):
        op, mid = m.get("op"), m.get("id")

        async def reply(err=None, **extra):
            await c.ws.send_json(dict({"t": "reply", "id": mid, "op": op, "ok": err is None}, **({"err": err} if err else {}), **extra))
        try:
            loop = asyncio.get_event_loop()
            if op == "control":
                if m.get("take", True):
                    if token and m.get("token") != token:
                        return await reply("wrong or missing token")
                    if self.control not in (None, c.id) and not m.get("force"):
                        return await reply("another browser has control")
                    self.control = c.id
                else:
                    self.control = None if self.control == c.id else self.control
                self.broadcast({"t": "control", "holder": self.control})
                return await reply()
            if op == "ping":
                return await reply()
            if op == "sup_status":
                return await reply(status=self.status())
            if op == "log_get":
                return await reply(**self.ops.log(int(m.get("which", 0)), int(m.get("n", 400))))
            if op == "config_get":
                return await reply(**await loop.run_in_executor(None, self.ops.get))
            if op == "config_check":
                return await reply(**await loop.run_in_executor(None, self.ops.check, str(m.get("text", "")), str(m.get("custom", ""))))
            if op == "config_version":
                return await reply(**self.ops.version(m.get("version")))
            if op == "mmodel_get":
                return await reply(**await loop.run_in_executor(None, self.ops.model_get, m.get("name")))
            if op == "backup_get":
                return await reply(**await loop.run_in_executor(None, self.ops.backup_get, m.get("programs", True) is not False))
            if op in sysupdate.READ_OPS:
                r = await self.sys.op(op, m, c.name)
                return await reply(r) if isinstance(r, str) else await reply(**r)
            if op not in ("config_save", "bitstream_save", "bitstream_delete", "mmodel_save", "mmodel_delete", "lcnc_start", "lcnc_restart", "lcnc_stop",
                          "restore_upload", "restore_apply") + sysupdate.OPS:
                return await reply("LinuxCNC isn't running (%s): only the Configure page works" % self.state)
            if self.control != c.id:
                return await reply("this browser doesn't have control (take it first)")
            if op == "config_save":
                err, hid = await loop.run_in_executor(None, self.ops.save, str(m.get("text", "")), str(m.get("custom", "")), c.name, str(m.get("note", ""))[:200])
                if err:
                    return await reply(err)
                self.event("ok", "machine.json saved by %s" % c.name)
                if m.get("start"):
                    self.start()
                return await reply(version=hid)
            if op == "bitstream_save":
                err = await loop.run_in_executor(None, self.ops.bitstream_save, m.get("name"), m.get("data", ""))
                return await reply(err, bitstreams=self.ops.bitstreams())
            if op == "mmodel_save":
                err = await loop.run_in_executor(None, self.ops.model_save, m.get("name"), m.get("data", ""))
                return await reply(err, models=self.ops.models())
            if op == "mmodel_delete":
                return await reply(self.ops.model_delete(m.get("name")), models=self.ops.models())
            if op == "bitstream_delete":
                return await reply(self.ops.bitstream_delete(m.get("name")), bitstreams=self.ops.bitstreams())
            if op in sysupdate.OPS:
                r = await self.sys.op(op, m, c.name)
                return await reply(r) if isinstance(r, str) else await reply(**r)
            if op == "restore_upload":
                return await reply(**await loop.run_in_executor(None, self.ops.restore_upload, m.get("offset", 0), m.get("data", ""), m.get("total", 0)))
            if op == "restore_apply":
                await loop.run_in_executor(None, self.ops.restore_queue)
                self.event("warn", "%s restores the machine folder from a backup" % c.name)
                self.start()
                return await reply()
            if op in ("lcnc_start", "lcnc_restart"):
                self.start()
                return await reply()
            if op == "lcnc_stop":
                self.want = "stop"
                self.wake.set()
                return await reply()
        except (ValueError, KeyError, TypeError, OSError) as e:
            return await reply(str(e) or type(e).__name__)

    def start(self):
        self.want = "run"
        self.fails = 0
        self.event("info", "LinuxCNC: start asked for")
        self.wake.set()

    async def shutdown(self):
        self.quit.set()


def main(argv=None):
    ap = argparse.ArgumentParser(description="LinuxCNC for a machine folder, again and again (webui/mill/README.md)")
    ap.add_argument("dir", help="the machine folder (machine.json)")
    ap.add_argument("--port", type=int, help="the console's port (else machine.json's)")
    ap.add_argument("--host", help="the address to serve on (else machine.json's)")
    ap.add_argument("--linuxcnc", default=os.environ.get("MILLWEB_LINUXCNC", "linuxcnc"), help="how to run LinuxCNC")
    ap.add_argument("--init", action="store_true", help="make the folder with a default machine.json, then exit")
    a = ap.parse_args(argv)
    if a.init:
        machineconf.init(os.path.abspath(a.dir))
        print("made %s" % os.path.abspath(a.dir))
        return 0
    if os.geteuid() == 0:
        sys.exit("run as the machine's user, not root (LinuxCNC's rtapi_app refuses root); the bitstream loader uses sudo")

    async def go():
        s = Supervisor(a.dir, a.port, a.linuxcnc, a.host)
        loop = asyncio.get_event_loop()
        for sig in (signal.SIGINT, signal.SIGTERM):
            loop.add_signal_handler(sig, s.quit.set)
        await s.run()
        s.set_state("stopped")
        await s.unserve()
    asyncio.run(go())
    return 0


if __name__ == "__main__":
    sys.exit(main())
