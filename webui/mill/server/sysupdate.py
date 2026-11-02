"""System updates of the controller, through its update helper.

The helper is the controller image's (ebaz-update on the EBAZ4205 image: RAUC A/B slots, an update
server's index.json, hostmot2 repo ebaz4205/image), run as root with `sudo -n`. It prints JSON:

    status            this system's version and slot, both slots, a running install, how the last one ended
    check             the server's releases, newest first: version, date, notes, newer/current/in_other_slot
    install VERSION   starts installing a release (newer or older) into the other slot; progress in status
    reboot            restarts into it

When an install has finished, the controller restarts into it by itself (after a running program has ended:
`busy` says why not yet).

millweb and millweb-supervisor both serve the ops (sys_status, sys_check, sys_install, sys_reboot), so updates
also work while LinuxCNC is down. Browsers get {"t": "sys", "status", "check"} whenever either changes.
No helper (MILLWEB_UPDATE_CMD unset and no /usr/local/sbin/ebaz-update): the feature is off.
"""
import asyncio
import json
import os
import shlex

DEFAULT_CMD = "/usr/local/sbin/ebaz-update"


def helper():
    """the helper's command line, or None"""
    cmd = os.environ.get("MILLWEB_UPDATE_CMD")
    if cmd is not None:
        return shlex.split(cmd) or None
    return ["sudo", "-n", DEFAULT_CMD] if os.access(DEFAULT_CMD, os.X_OK) else None


class SysUpdate:
    CHECK_EVERY = 3600.0   # the server, for "a new version is available"

    def __init__(self, broadcast, event, busy=None):
        self.cmd = helper()
        self.broadcast, self.event = broadcast, event
        self.busy = busy or (lambda: None)   # why the controller can't restart now, or None
        self.status, self.check = None, None
        self.task = None
        self.restarting = None               # the version being restarted into
        self._waiting = None

    @property
    def enabled(self):
        return self.cmd is not None

    async def run(self, *args, timeout=60):
        p = await asyncio.create_subprocess_exec(*self.cmd, *args, stdout=asyncio.subprocess.PIPE,
                                                 stderr=asyncio.subprocess.PIPE)
        try:
            out, err = await asyncio.wait_for(p.communicate(), timeout)
        except asyncio.TimeoutError:
            p.kill()
            raise ValueError("the update helper didn't answer in %d s" % timeout)
        why = "the update helper: %s" % (err.decode().strip() or out.decode().strip() or "exit %d" % p.returncode)
        if not out.strip():  # e.g. sudo refused: nothing on stdout
            raise ValueError(why)
        try:
            return json.loads(out.decode())
        except ValueError:
            raise ValueError(why)

    def snapshot(self):
        return {"t": "sys", "status": self.status, "check": self.check, "restarting": self.restarting}

    async def refresh(self, check=False):
        self.status = await self.run("status")
        if check:
            self.check = await self.run("check", timeout=30)
        self.broadcast(self.snapshot())
        return {"status": self.status, "check": self.check, "restarting": self.restarting}  # a reply's fields: no "t" (it would replace "reply")

    def installing(self):
        i = (self.status or {}).get("install") or {}
        return i.get("state") == "running" or self.restarting is not None or self.to_restart() is not None

    def to_restart(self):
        """an install of this boot that has finished and waits for the restart: its version"""
        s = self.status or {}
        i, last = s.get("install") or {}, s.get("last") or {}
        if i.get("state") == "installed" and last.get("outcome") == "pending" and last.get("to") == i.get("version"):
            return i["version"]
        return None

    def start(self):
        if self.enabled and self.task is None:
            self.task = asyncio.ensure_future(self.loop())

    def stop(self):
        if self.task:
            self.task.cancel()
            self.task = None

    async def loop(self):
        """the server every hour; a running install every second"""
        last_check, was = -self.CHECK_EVERY, None
        loop = asyncio.get_event_loop()
        while True:
            try:
                due = loop.time() - last_check >= self.CHECK_EVERY
                await self.refresh(check=due)
                if due:
                    last_check = loop.time()
                    c = self.check or {}
                    if c.get("update_available") and was != c.get("latest"):
                        self.event("ok", "A new system version is available: %s (Settings, System)" % c["latest"])
                        was = c.get("latest")
                await self.announce_install()
            except (ValueError, OSError) as e:
                self.status = {"error": str(e)}
                self.broadcast(self.snapshot())
            await asyncio.sleep(1.0 if self.installing() else 30.0)

    _said = None

    async def announce_install(self):
        i = (self.status or {}).get("install") or {}
        key = (i.get("version"), i.get("state"))
        if i and key != self._said and i.get("state") in ("installed", "failed"):
            self._said = key
            if i["state"] == "installed":
                self.event("ok", "System %s installed" % i["version"])
            else:
                self.event("bad", "Installing system %s failed: %s" % (i["version"], (i.get("error") or "").splitlines()[-1:] or "?"))
        await self.auto_restart()

    async def auto_restart(self):
        """an install has finished: restart into it, once nothing runs"""
        v = self.to_restart()
        if v is None or self.restarting is not None:
            return
        why = self.busy()
        if why:
            if self._waiting != v:
                self._waiting = v
                self.event("warn", "System %s: the controller restarts when the program has ended" % v)
                self.broadcast(self.snapshot())
            return
        self.restarting = v
        self.event("warn", "The controller restarts into system %s" % v)
        self.broadcast(self.snapshot())
        try:
            await self.run("reboot")
        except (ValueError, OSError) as e:
            self.restarting = None
            self.event("bad", "Restart into system %s: %s" % (v, e))
            self.broadcast(self.snapshot())

    # ------------------------------------------------------------ ops (millweb and millweb-supervisor)
    async def op(self, op, m, who):
        """the reply's fields, or an error string"""
        if not self.enabled:
            return "no system updates on this controller"
        try:
            return await self._op(op, m, who)
        except (ValueError, OSError) as e:
            return str(e)

    async def _op(self, op, m, who):
        if op == "sys_status":
            return await self.refresh()
        if op == "sys_check":
            return await self.refresh(check=True)
        if op == "sys_install":
            v = str(m.get("version", ""))
            if not v:
                return "which version?"
            if self.installing():
                return "an installation is already running"
            r = await self.run("install", v)
            if not (r or {}).get("ok"):
                return (r or {}).get("error") or "the update helper refused"
            self.event("warn", "%s installs system %s" % (who, v))
            await self.refresh()
            return {"started": v}
        if op == "sys_reboot":
            if ((self.status or {}).get("install") or {}).get("state") == "running":
                return "an installation is running: wait for it"
            self.event("warn", "%s restarts the controller" % who)
            await self.run("reboot")
            return {}
        return "unknown op %r" % op


OPS = ("sys_status", "sys_check", "sys_install", "sys_reboot")
READ_OPS = ("sys_status", "sys_check")
