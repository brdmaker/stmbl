#!/usr/bin/env python3
"""LinuxCNC with millweb for the test suites: a copy of test/sim in a
temporary directory, started headless, stopped by stopping millweb.

    python3 harness.py [--ini-set SECTION.KEY=VALUE ...] [--keep] [--drive] -- CMD ARGS...
    python3 harness.py --serve [--port 8080] [--stress-files]     (for suites run from another computer)

runs CMD in LinuxCNC's environment with MILLWEB_PORT (and MILLWEB_DIR, the
config's directory) and exits with its status. As a module: `with Machine() as m:`
gives m.port and m.dir. Runs as a non-root user (rtapi_app refuses root).

--supervised (Machine(supervised=True)): the machine of test/machine
(machine.json) run by millweb-supervisor, which makes its INI and HAL and
restarts LinuxCNC; m.dir is the machine folder, m.sup the supervisor.

--drive (Machine(drive=True)): a simulated stmbl drive on X (test/sim-drive:
stmbl_sim closing joint 0's loop, its console through sserialterm), with
[MILLWEB] DRIVE_LINK set; needs stmbl_sim.so and sserialterm.so (make -C
host/lcnc all install, make -C linuxcnc/sserialterm all install).
"""
import argparse
import os
import re
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
MILL = os.path.dirname(HERE)
ROOT = os.path.dirname(os.path.dirname(MILL))  # the repo
def _find_linuxcnc():
    """A run-in-place tree (LINUXCNC=path, or ~/linuxcnc-2.9), else "": an installed LinuxCNC on PATH."""
    if "LINUXCNC" in os.environ:
        return os.environ["LINUXCNC"]
    for d in (os.path.expanduser("~/linuxcnc-2.9"), "/home/user/linuxcnc-2.9"):
        if os.path.exists(os.path.join(d, "scripts", "rip-environment")):
            return d
    return ""


LINUXCNC = _find_linuxcnc()
# the shell lines that give LinuxCNC's environment: a run-in-place tree's, or nothing (installed)
RIP = ("export PATH=%s/pybin:$PATH; . %s/scripts/rip-environment >/dev/null; " % (LINUXCNC, LINUXCNC)
       if LINUXCNC and os.path.exists(os.path.join(LINUXCNC, "scripts", "rip-environment")) else "")


def sample_dirs():
    """Where LinuxCNC's own sample programs are: the tree's, the package's, the user's."""
    return [d for d in (os.path.join(LINUXCNC, "nc_files") if LINUXCNC else "", "/usr/share/linuxcnc/ncfiles",
                        "/usr/share/doc/linuxcnc/examples/nc_files", os.path.expanduser("~/linuxcnc/nc_files")) if d and os.path.isdir(d)]


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def set_ini(text, section, key, value):
    """INI text with SECTION's KEY set to value (replaced, or added)."""
    lines = text.splitlines(True)
    sec = None
    for i, l in enumerate(lines):
        s = l.strip()
        if s.startswith("["):
            if sec == section:  # leaving the section: add it here
                lines.insert(i, "%s = %s\n" % (key, value))
                return "".join(lines)
            sec = s.strip("[]")
            continue
        if sec == section and re.match(r"^#?\s*%s\s*=" % re.escape(key), s):
            lines[i] = "%s = %s\n" % (key, value)
            return "".join(lines)
    if sec == section:
        return "".join(lines) + "%s = %s\n" % (key, value)
    return "".join(lines) + "\n[%s]\n%s = %s\n" % (section, key, value)


def millweb_pids():
    out = []
    for p in os.listdir("/proc"):
        if p.isdigit() and int(p) != os.getpid():
            try:
                cmd = open("/proc/%s/cmdline" % p, "rb").read().split(b"\0")
            except OSError:
                continue
            if any(c.endswith(b"webui/mill/millweb") for c in cmd):
                out.append(int(p))
    return out


class Machine:
    # the simulated drive's X: slower than the test machine's (the drive's
    # motor tops out at ~500 mm/s), and room for autophasing at enable
    DRIVE_INI = ["JOINT_0.MAX_VELOCITY=200", "JOINT_0.MAX_ACCELERATION=1500", "JOINT_0.FERROR=5", "JOINT_0.MIN_FERROR=2.5",
                 "AXIS_X.MAX_VELOCITY=200", "AXIS_X.MAX_ACCELERATION=1500"]

    def __init__(self, ini_set=(), keep=False, hal_extra="", port=None, host="127.0.0.1", drive=False, supervised=False, machine=None,
                 vib=0):
        self.ini_set, self.keep, self.hal_extra, self.drive = list(ini_set), keep, hal_extra, drive
        self.vib, self.vibmon = vib, None  # vib: this many simulated vibration sensors, through vibmon --sim
        self.supervised, self.machine_json = supervised, machine  # machine: a machine.json (dict) in place of test/machine's
        self.port = port or free_port()
        self.host = host
        self.dir = None
        self.proc = None

    def __enter__(self):
        if os.geteuid() == 0:
            raise SystemExit("run as a non-root user (LinuxCNC's rtapi_app refuses root)")
        if millweb_pids():
            raise SystemExit("a millweb is already running: stop that LinuxCNC first")
        self.dir = tempfile.mkdtemp(prefix="millweb-test.")
        if self.supervised:
            return self.start_supervised()
        shutil.copytree(os.path.join(HERE, "sim"), self.dir, dirs_exist_ok=True)
        p = os.path.join(self.dir, "mill.ini")
        ini = open(p).read()
        ini_set, hal_extra = list(self.ini_set), self.hal_extra
        if self.drive:
            shutil.copytree(os.path.join(HERE, "sim-drive"), self.dir, dirs_exist_ok=True)
            hal_extra += "\n" + open(os.path.join(self.dir, "drive.hal")).read().replace("@DIR@", self.dir)
            ini_set = self.DRIVE_INI + ["MILLWEB.DRIVE=X stmbl-sim.0.current stmbl-sim.0.power stmbl-sim.0.fault",
                                        "MILLWEB.DRIVE_LINK=X %s/drive-x stmbl-sim.0" % self.dir] + ini_set
        if self.vib:
            sock = os.path.join(self.dir, "vibmon.sock")
            self.vibmon = subprocess.Popen([sys.executable, os.path.join(ROOT, "tools", "vibmon", "vibmon.py"), "--sim", str(self.vib),
                                            "--config", os.path.join(self.dir, "sensors.json"), "--sock", sock],
                                           stdout=open(os.path.join(self.dir, "vibmon.log"), "w"), stderr=subprocess.STDOUT)
            ini_set = ["MILLWEB.VIBMON=" + sock] + ini_set
        # millweb through a wrapper that keeps its output (LinuxCNC's own log
        # file is deleted when it exits)
        wrap = os.path.join(self.dir, "millweb-display")
        with open(wrap, "w") as f:
            f.write('#!/bin/sh\nexec "%s" "$@" >> "%s" 2>&1\n' % (os.path.join(MILL, "millweb"), os.path.join(self.dir, "millweb.log")))
        os.chmod(wrap, 0o755)
        ini = set_ini(ini, "DISPLAY", "DISPLAY", wrap)
        ini = set_ini(ini, "MILLWEB", "PORT", str(self.port))
        ini = set_ini(ini, "MILLWEB", "HOST", self.host)
        for item in ini_set:
            k, v = item.split("=", 1)
            sec, key = k.split(".", 1)
            ini = set_ini(ini, sec, key, v)
        open(p, "w").write(ini)
        if hal_extra:
            with open(os.path.join(self.dir, "mill.hal"), "a") as f:
                f.write("\n" + hal_extra + "\n")
        sh = RIP + "cd %s && exec linuxcnc mill.ini > linuxcnc.log 2>&1" % self.dir
        self.proc = subprocess.Popen(["bash", "-c", sh], start_new_session=True)
        t = time.time() + 90
        while time.time() < t:
            if self.proc.poll() is not None:
                break
            try:
                socket.create_connection(("127.0.0.1", self.port), 1).close()
                return self
            except OSError:
                time.sleep(0.3)
        log = "".join(open(os.path.join(self.dir, f)).read()[-3000:] for f in ("linuxcnc.log", "millweb.log")
                      if os.path.exists(os.path.join(self.dir, f)))
        self.__exit__(None, None, None)
        raise RuntimeError("millweb didn't come up:\n" + log)

    def start_supervised(self):
        import json
        shutil.copytree(os.path.join(HERE, "machine"), self.dir, dirs_exist_ok=True)
        shutil.copytree(os.path.join(HERE, "sim", "nc_files"), os.path.join(self.dir, "nc_files"), dirs_exist_ok=True)
        if self.machine_json is not None:
            with open(os.path.join(self.dir, "machine.json"), "w") as f:
                json.dump(self.machine_json, f, indent=2)
        sup = os.path.join(MILL, "millweb-supervisor")
        sh = RIP + "exec python3 %s %s --port %d --host %s > %s/supervisor.log 2>&1" % (sup, self.dir, self.port, self.host, self.dir)
        self.proc = subprocess.Popen(["bash", "-c", sh], start_new_session=True)
        if not self.wait_state("running", 90):
            log = "".join(open(os.path.join(self.dir, f)).read()[-3000:] for f in ("supervisor.log", "logs/linuxcnc.log")
                          if os.path.exists(os.path.join(self.dir, f)))
            self.__exit__(None, None, None)
            raise RuntimeError("the supervisor didn't get LinuxCNC running:\n" + log)
        return self

    def status(self):
        import json
        try:
            return json.load(open(os.path.join(self.dir, "run", "status.json")))
        except (OSError, ValueError):
            return {}

    def wait_state(self, state, timeout=60):
        """Until the supervisor says state (and, for running, millweb answers)."""
        t = time.time() + timeout
        while time.time() < t:
            if self.proc.poll() is not None:
                return False
            if self.status().get("state") == state:
                return True
            time.sleep(0.2)
        return False

    @staticmethod
    def lcnc_env():
        """This environment plus LinuxCNC's (a run-in-place tree's rip-environment; installed: as it is)."""
        sh = RIP + "env -0"
        out = subprocess.run(["bash", "-c", sh], capture_output=True, timeout=20).stdout
        return dict(kv.split("=", 1) for kv in out.decode().split("\0") if "=" in kv)

    def halcmd(self, *args):
        """halcmd in this LinuxCNC's environment; its output."""
        sh = RIP + "exec halcmd \"$@\""
        return subprocess.run(["bash", "-c", sh, "halcmd"] + [str(a) for a in args], capture_output=True, text=True,
                              timeout=20).stdout

    def stop_millweb(self):
        for p in millweb_pids():
            try:
                os.kill(p, signal.SIGTERM)
            except OSError:
                pass

    def __exit__(self, *exc):
        ok = True
        if self.supervised:
            if self.proc and self.proc.poll() is None:
                os.killpg(self.proc.pid, signal.SIGTERM)  # the supervisor stops LinuxCNC, then itself
        else:
            self.stop_millweb()
        if self.proc:
            try:
                self.proc.wait(40)
            except subprocess.TimeoutExpired:
                os.killpg(self.proc.pid, signal.SIGKILL)
                ok = False
        if self.vibmon:
            self.vibmon.terminate()
            self.vibmon.wait(10)
        if self.keep or exc[0] is not None:
            print("(kept %s)" % self.dir, flush=True)
        elif self.dir:
            shutil.rmtree(self.dir, ignore_errors=True)
        self.clean_exit = ok
        return False


def serve(a):
    """The test machine for suites run from another computer: on the network,
    until Ctrl-C (or SIGTERM)."""
    stop = []
    for sig in (signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, lambda *x: stop.append(1))
    with Machine(a.ini_set, a.keep, port=a.port, host="0.0.0.0", drive=a.drive) as m:
        if a.stress_files:  # written here, so nothing big goes over the network
            import stress_suite
            print("writing the stress suite's programs ...", flush=True)
            stress_suite.write_programs(m.dir)
        ip = socket.gethostbyname(socket.gethostname())
        print("the test machine (test/sim) on http://%s:%d/ (%s)\n"
              "from another computer:  make test-gui-live REMOTE=<this host>:%d   make test-stress REMOTE=<this host>:%d\n"
              "Ctrl-C stops it" % (ip, m.port, m.dir, m.port, m.port), flush=True)
        while not stop and m.proc.poll() is None:
            time.sleep(0.5)
        if m.proc.poll() is not None:
            print("LinuxCNC stopped", flush=True)
    sys.exit(0 if m.clean_exit else 1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ini-set", action="append", default=[], help="SECTION.KEY=VALUE in the test INI")
    ap.add_argument("--keep", action="store_true")
    ap.add_argument("--drive", action="store_true", help="a simulated stmbl drive on X, its console through sserialterm")
    ap.add_argument("--supervised", action="store_true", help="test/machine's machine.json, run by millweb-supervisor")
    ap.add_argument("--serve", action="store_true", help="serve the test machine on the network for suites run elsewhere")
    ap.add_argument("--port", type=int, default=8080, help="--serve's port")
    ap.add_argument("--stress-files", action="store_true", help="--serve: write the stress suite's programs too")
    ap.add_argument("cmd", nargs=argparse.REMAINDER)
    a = ap.parse_args()
    if a.serve:
        return serve(a)
    cmd = a.cmd[1:] if a.cmd[:1] == ["--"] else a.cmd
    if not cmd:
        ap.error("a command to run: -- CMD ARGS (or --serve)")
    with Machine(a.ini_set, a.keep, drive=a.drive, supervised=a.supervised) as m:
        print("millweb on port %d (%s)" % (m.port, m.dir), flush=True)
        env = dict(m.lcnc_env(), MILLWEB_PORT=str(m.port), MILLWEB_DIR=m.dir)
        rc = subprocess.run(cmd, env=env).returncode
    if not m.clean_exit:
        print("FAIL LinuxCNC didn't stop when millweb did")
        rc = rc or 1
    sys.exit(rc)


if __name__ == "__main__":
    main()
