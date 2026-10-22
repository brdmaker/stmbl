#!/usr/bin/env python3
"""LinuxCNC with millweb for the test suites: a copy of test/sim in a
temporary directory, started headless, stopped by stopping millweb.

    python3 harness.py [--ini-set SECTION.KEY=VALUE ...] [--keep] -- CMD ARGS...

runs CMD in LinuxCNC's environment with MILLWEB_PORT (and MILLWEB_DIR, the
config's directory) and exits with its status. As a module: `with Machine() as m:`
gives m.port and m.dir. Runs as a non-root user (rtapi_app refuses root).
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
LINUXCNC = os.environ.get("LINUXCNC", "/home/user/linuxcnc-2.9")
# a run-in-place tree's environment; nothing for an installed LinuxCNC
RIP = ("export PATH=%s/pybin:$PATH; . %s/scripts/rip-environment >/dev/null; " % (LINUXCNC, LINUXCNC)
       if os.path.exists(os.path.join(LINUXCNC, "scripts", "rip-environment")) else "")


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
    def __init__(self, ini_set=(), keep=False, hal_extra=""):
        self.ini_set, self.keep, self.hal_extra = list(ini_set), keep, hal_extra
        self.port = free_port()
        self.dir = None
        self.proc = None

    def __enter__(self):
        if os.geteuid() == 0:
            raise SystemExit("run as a non-root user (LinuxCNC's rtapi_app refuses root)")
        if millweb_pids():
            raise SystemExit("a millweb is already running: stop that LinuxCNC first")
        self.dir = tempfile.mkdtemp(prefix="millweb-test.")
        shutil.copytree(os.path.join(HERE, "sim"), self.dir, dirs_exist_ok=True)
        # [DISPLAY] PROGRAM_PREFIX = nc_files: git keeps no empty folder, a
        # fresh checkout has none
        os.makedirs(os.path.join(self.dir, "nc_files"), exist_ok=True)
        p = os.path.join(self.dir, "mill.ini")
        ini = open(p).read()
        # millweb through a wrapper that keeps its output (LinuxCNC's own log
        # file is deleted when it exits)
        wrap = os.path.join(self.dir, "millweb-display")
        with open(wrap, "w") as f:
            f.write('#!/bin/sh\nexec "%s" "$@" >> "%s" 2>&1\n' % (os.path.join(MILL, "millweb"), os.path.join(self.dir, "millweb.log")))
        os.chmod(wrap, 0o755)
        ini = set_ini(ini, "DISPLAY", "DISPLAY", wrap)
        ini = set_ini(ini, "MILLWEB", "PORT", str(self.port))
        ini = set_ini(ini, "MILLWEB", "HOST", "127.0.0.1")
        for item in self.ini_set:
            k, v = item.split("=", 1)
            sec, key = k.split(".", 1)
            ini = set_ini(ini, sec, key, v)
        open(p, "w").write(ini)
        if self.hal_extra:
            with open(os.path.join(self.dir, "mill.hal"), "a") as f:
                f.write("\n" + self.hal_extra + "\n")
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

    @staticmethod
    def lcnc_env():
        """This environment plus LinuxCNC's (PATH, PYTHONPATH, ... from rip-environment)."""
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
        self.stop_millweb()
        if self.proc:
            try:
                self.proc.wait(40)
            except subprocess.TimeoutExpired:
                os.killpg(self.proc.pid, signal.SIGKILL)
                ok = False
        if self.keep or exc[0] is not None:
            print("(kept %s)" % self.dir, flush=True)
        elif self.dir:
            shutil.rmtree(self.dir, ignore_errors=True)
        self.clean_exit = ok
        return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ini-set", action="append", default=[], help="SECTION.KEY=VALUE in the test INI")
    ap.add_argument("--keep", action="store_true")
    ap.add_argument("cmd", nargs=argparse.REMAINDER)
    a = ap.parse_args()
    cmd = a.cmd[1:] if a.cmd[:1] == ["--"] else a.cmd
    if not cmd:
        ap.error("a command to run: -- CMD ARGS")
    with Machine(a.ini_set, a.keep) as m:
        print("millweb on port %d (%s)" % (m.port, m.dir), flush=True)
        env = dict(m.lcnc_env(), MILLWEB_PORT=str(m.port), MILLWEB_DIR=m.dir)
        rc = subprocess.run(cmd, env=env).returncode
    if not m.clean_exit:
        print("FAIL LinuxCNC didn't stop when millweb did")
        rc = rc or 1
    sys.exit(rc)


if __name__ == "__main__":
    main()
