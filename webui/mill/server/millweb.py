"""millweb: the browser console's backend, between LinuxCNC and any number
of browsers (README.md, "The backend").

    millweb -ini machine.ini          as LinuxCNC's display program ([DISPLAY] DISPLAY = millweb)
    millweb --ini machine.ini         beside a LinuxCNC that is already running

Serves webui/mill (the console) over HTTP(S) and talks JSON over a
WebSocket on /ws:

  server -> browser   {"t": "hello" | "st" | "slow" | "ev" | "program" | "tools" | "offsets" |
                        "files" | "control" | "reply" | "file", ...}
  browser -> server   {"op": ..., "id": n, ...}   answered by {"t": "reply", "id": n, "ok": bool, "err": text}

One browser has control (a lease: "control" {take}); the others watch. Any
browser may press E-STOP. Continuous jogs need a "jog" message at least
every 300 ms or they stop.
"""
import argparse
import asyncio
import concurrent.futures
import datetime
import json
import os
import signal
import ssl
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import machine as machine_mod  # noqa: E402
import preview  # noqa: E402
import wsserver  # noqa: E402

WEBROOT = os.path.dirname(HERE)
JOG_DEADMAN = 0.3
VERSION = "0.1"


def now():
    return datetime.datetime.now().strftime("%H:%M:%S")


class Client:
    n = 0

    def __init__(self, ws):
        Client.n += 1
        self.id = "c%d" % Client.n
        self.ws = ws
        self.last = {}
        self.name = "%s:%s" % (ws.peer[0], ws.peer[1]) if ws.peer else self.id


class Hub:
    def __init__(self, mach, cfg, quit_event=None):
        self.m = mach
        self.cfg = cfg
        self.clients = {}
        self.control = None          # client id
        self.events = []             # (time, lvl, text)
        self.pool = concurrent.futures.ThreadPoolExecutor(max_workers=1, thread_name_prefix="millweb-cmd")
        self.ppool = concurrent.futures.ThreadPoolExecutor(max_workers=1, thread_name_prefix="millweb-preview")
        self.state = {}
        self.slow_state = {}
        self.program = None          # the last preview sent
        self.program_file = None
        self.jog_seen = {}           # axis -> time of the last continuous-jog message
        self.token = cfg.get("MILLWEB", "TOKEN")
        self.on_loss = (cfg.get("MILLWEB", "ON_CONTROL_LOSS", "continue") or "continue").lower()
        self.poll_hz = cfg.num("MILLWEB", "POLL_HZ", 30.0)
        self.quit = quit_event
        self.files_sig = None
        self.tools_sig = None

    # ------------------------------------------------------------ events
    def event(self, lvl, text):
        e = (now(), lvl, text)
        self.events.append(e)
        del self.events[:-300]
        self.broadcast({"t": "ev", "time": e[0], "lvl": lvl, "text": text})

    def broadcast(self, msg, only=None):
        data = json.dumps(msg, separators=(",", ":"))
        for c in list(self.clients.values()):
            if only is None or c.id in only:
                asyncio.ensure_future(c.ws.send(data))

    # ------------------------------------------------------------ connection
    async def handle(self, ws, query):
        c = Client(ws)
        self.clients[c.id] = c
        info = self.m.info()
        info["version"] = VERSION
        info["token_required"] = bool(self.token)
        await ws.send_json({"t": "hello", "you": c.id, "machine": info, "control": self.control,
                            "events": [{"time": t, "lvl": l, "text": x} for t, l, x in self.events[-80:]]})
        await ws.send_json({"t": "st", **self.state})
        if self.slow_state:
            await ws.send_json({"t": "slow", **self.slow_state})
        await ws.send_json(self.tools_msg())
        await ws.send_json(self.offsets_msg())
        await ws.send_json({"t": "files", "files": self.m.programs()})
        if self.program:
            await ws.send(json.dumps(self.program, separators=(",", ":")))
        self.event("info", "Browser connected: %s" % c.name)
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
                    asyncio.ensure_future(self.on_message(c, msg))
        finally:
            del self.clients[c.id]
            if self.control == c.id:
                await self.lose_control(c)
            self.event("info", "Browser left: %s" % c.name)

    async def lose_control(self, c):
        self.control = None
        await self.run(self.m.cmd_jog_stop)
        self.jog_seen.clear()
        if self.on_loss == "hold" and self.state.get("run") == "run":
            await self.run(self.m.cmd_cycle, "pause")
            self.event("warn", "The controlling browser went away: feed hold")
        else:
            self.event("warn", "The controlling browser went away; the machine carries on")
        self.broadcast({"t": "control", "holder": None})

    async def run(self, fn, *a, **kw):
        loop = asyncio.get_event_loop()
        return await loop.run_in_executor(self.pool, lambda: fn(*a, **kw))

    # ------------------------------------------------------------ messages
    async def on_message(self, c, msg):
        op, mid = msg.get("op"), msg.get("id")

        async def reply(err=None, **extra):
            out = {"t": "reply", "id": mid, "op": op, "ok": err is None}
            if err is not None:
                out["err"] = err
            out.update(extra)
            await c.ws.send_json(out)

        try:
            if op == "control":
                return await reply(await self.op_control(c, msg))
            if op == "estop":                      # anyone may stop the machine
                err = await self.run(self.m.cmd_estop, bool(msg.get("on", True)))
                if msg.get("on", True):
                    self.event("bad", "E-STOP from %s" % c.name)
                return await reply(err)
            if op == "jog" and msg.get("inc", 0) == 0 and self.control == c.id:
                # the deadman: keep-alive of a running continuous jog
                a = str(msg.get("axis", "x")).lower()
                if a in self.jog_seen:
                    self.jog_seen[a] = time.time()
                    return await reply()
            if op in ("files", "program_get", "ping", "events"):
                pass  # reading needs no control
            elif self.control != c.id:
                return await reply("this browser doesn't have control (take it first)")
            elif self.slow_state.get("remote") is False:
                return await reply("remote control is switched off at the machine")
            handler = getattr(self, "op_" + str(op), None)
            if handler is None:
                return await reply("unknown op %r" % op)
            self.m.done_flag = False
            res = await handler(c, msg)
            if isinstance(res, dict):
                return await reply(None, **res)
            return await reply(res)
        except (ValueError, KeyError, TypeError, OSError) as e:
            return await reply(str(e))

    async def op_control(self, c, msg):
        if msg.get("take", True):
            if self.token and msg.get("token") != self.token:
                return "wrong or missing token"
            if self.slow_state.get("remote") is False:
                return "remote control is switched off at the machine"
            if self.control not in (None, c.id) and not msg.get("force"):
                return "another browser has control"
            if self.control and self.control != c.id:
                self.event("warn", "Control taken over by %s" % c.name)
            self.control = c.id
            self.event("info", "%s has control" % c.name)
        elif self.control == c.id:
            await self.lose_control(c)
            return None
        self.broadcast({"t": "control", "holder": self.control})
        return None

    async def op_ping(self, c, m):
        return {"time": time.time()}

    async def op_power(self, c, m):
        err = await self.run(self.m.cmd_power, bool(m.get("on", True)))
        if not err:
            self.event("ok", "Machine %s" % ("on" if m.get("on", True) else "off"))
        return err

    async def op_home(self, c, m):
        return await self.run(self.m.cmd_home, m.get("axis", "all"))

    async def op_unhome(self, c, m):
        return await self.run(self.m.cmd_unhome, m.get("axis", "all"))

    async def op_jog(self, c, m):
        a = str(m.get("axis", "x")).lower()
        inc = float(m.get("inc", 0))
        err = await self.run(self.m.cmd_jog, a, int(m.get("dir", 1)), inc, float(m.get("rate", 1000)))
        if not err and inc == 0:
            self.jog_seen[a] = time.time()
        return err

    async def op_jog_stop(self, c, m):
        a = m.get("axis")
        self.jog_seen.pop(str(a).lower(), None) if a else self.jog_seen.clear()
        return await self.run(self.m.cmd_jog_stop, a)

    async def op_mdi(self, c, m):
        text = str(m.get("cmd", "")).strip()
        if not text or len(text) > 256:
            return "an MDI line: 1 to 256 characters"
        self.event("info", "MDI: %s" % text)
        return await self.run(self.m.mdi, text)

    async def op_cycle(self, c, m):
        act = m.get("action", "start")
        err = await self.run(self.m.cmd_cycle, act, int(m.get("line", 0) or 0))
        if not err:
            self.event("ok" if act in ("start", "resume") else "info",
                       {"start": "Cycle start" + (" from line %s" % m.get("line") if m.get("line") else ""), "pause": "Feed hold",
                        "resume": "Resumed", "step": "Single block", "stop": "Stopped"}.get(act, act))
        return err

    async def op_override(self, c, m):
        return await self.run(self.m.cmd_override, m["k"], m["v"])

    async def op_spindle(self, c, m):
        return await self.run(self.m.cmd_spindle, int(m.get("dir", 0)), m.get("speed"))

    async def op_coolant(self, c, m):
        return await self.run(self.m.cmd_coolant, m["k"], bool(m.get("on")))

    async def op_opt(self, c, m):
        return await self.run(self.m.cmd_opt, m.get("m1"), m.get("bd"))

    async def op_zero(self, c, m):
        err = await self.run(self.m.cmd_zero, m["axis"], float(m.get("value", 0)))
        if not err:
            self.event("ok", "G10 L20: %s = %.3f in %s" % (m["axis"].upper(), float(m.get("value", 0)), self.m.WCS[self.state.get("wcs", 0)]))
            self.broadcast(self.offsets_msg())
        return err

    async def op_wcs(self, c, m):
        err = await self.run(self.m.cmd_wcs, int(m["index"]))
        self.broadcast(self.offsets_msg())
        return err

    async def op_offset(self, c, m):
        err = await self.run(self.m.cmd_offset, int(m["index"]), m.get("x"), m.get("y"), m.get("z"))
        self.broadcast(self.offsets_msg())
        return err

    async def op_g92_clear(self, c, m):
        err = await self.run(self.m.cmd_g92_clear)
        self.broadcast(self.offsets_msg())
        return err

    async def op_tool_load(self, c, m):
        err = await self.run(self.m.cmd_tool_load, int(m["t"]))
        if not err:
            self.event("ok", "T%d M6" % int(m["t"]))
        return err

    async def op_tools_save(self, c, m):
        err = await self.run(self.m.cmd_tools_save, m["tools"])
        if not err:
            self.event("ok", "Tool table saved")
            self.broadcast(self.tools_msg())
        return err

    async def op_tool_measure(self, c, m):
        res = await self.run(self.m.cmd_tool_measure)
        if isinstance(res, dict):
            self.event("ok", "T%d length %.4f mm (tool setter)" % (res["tool"], res["length"]))
            self.broadcast(self.tools_msg())
        return res

    async def op_probe(self, c, m):
        res = await self.run(self.m.cmd_probe, m.get("kind", "z"), float(m.get("travel", 20)), float(m.get("feed", 300)),
                             float(m.get("plate", 0)), float(m.get("clearance", 5)))
        if isinstance(res, dict):
            self.event("ok", "Probe: machine Z %.4f" % res["mach"][2])
            self.broadcast(self.offsets_msg())
        return res

    async def op_files(self, c, m):
        return {"files": self.m.programs()}

    async def op_program_get(self, c, m):
        path = self.m._safe(m["name"])
        loop = asyncio.get_event_loop()
        pv = await loop.run_in_executor(self.ppool, self.make_preview, path)
        return {"program": pv}

    async def op_program_save(self, c, m):
        text = str(m.get("text", ""))
        if len(text) > 6 * 1024 * 1024:
            return "too large"
        err = await self.run(self.m.cmd_program_save, m["name"], text)
        if not err:
            self.event("ok", "Saved %s" % os.path.basename(m["name"]))
            self.broadcast({"t": "files", "files": self.m.programs()})
        return err

    async def op_program_delete(self, c, m):
        err = await self.run(self.m.cmd_program_delete, m["name"])
        if not err:
            self.broadcast({"t": "files", "files": self.m.programs()})
        return err

    async def op_program_load(self, c, m):
        err = await self.run(self.m.cmd_program_load, m["name"])
        if not err:
            self.event("ok", "Loaded %s" % os.path.basename(m["name"]))
        return err

    async def op_pcomp(self, c, m):
        return await self.run(self.m.cmd_pcomp, m["k"], m["v"])

    # ------------------------------------------------------------ data
    def tools_msg(self):
        return {"t": "tools", "tools": self.m.tools()}

    def offsets_msg(self):
        return {"t": "offsets", "offsets": self.m.offsets, "g92": self.m.g92, "wcs": self.state.get("wcs", 0)}

    def make_preview(self, path):
        s = self.state
        active = [s.get("g5x", [0, 0, 0])[i] + s.get("g92", [0, 0, 0])[i] for i in range(3)]
        tools = {}
        for t in self.m.tools():
            tools[t["t"]] = (t["t"], 0.0, 0.0, t["z"] / 25.4, 0, 0, 0, 0, 0, 0, t["d"] / 25.4, 0, 0, 0)
        pv = preview.parse(path, tools, self.m.var_file, "G21", "", active, pockets=[t["t"] for t in self.m.tools()])
        try:
            with open(path, errors="replace") as f:
                lines = f.read().splitlines()[:50000]
        except OSError:
            lines = []
        return {"t": "program", "name": os.path.basename(path), "path": path, "lines": lines,
                "segs": pv["segs"], "error": pv["error"], "wcs": s.get("wcs", 0)}

    # ------------------------------------------------------------ loops
    async def poll_loop(self):
        loop = asyncio.get_event_loop()
        period = 1.0 / max(self.poll_hz, 1.0)
        prev = {}
        while not (self.quit and self.quit.is_set()):
            t0 = time.time()
            try:
                st = self.m.status()
            except Exception as e:  # LinuxCNC gone
                self.event("bad", "LinuxCNC status: %s" % e)
                await asyncio.sleep(1.0)
                continue
            self.state = st
            diff = {k: v for k, v in st.items() if prev.get(k) != v}
            if diff:
                self.broadcast({"t": "st", **diff})
            # transitions worth an event
            if prev:
                if st["estop"] != prev.get("estop"):
                    self.event("bad" if st["estop"] else "ok", "E-STOP" if st["estop"] else "E-STOP released")
                if st["run"] == "done" and prev.get("run") in ("run", "hold"):
                    self.event("ok", "Program finished: %s" % os.path.basename(st["file"] or ""))
                if st["homed"] != prev.get("homed") and all(st["homed"]):
                    self.event("ok", "All axes homed")
            prev = st
            # a new program: its preview, once
            if st.get("file") and st["file"] != self.program_file:
                self.program_file = st["file"]
                pv = await loop.run_in_executor(self.ppool, self.make_preview, st["file"])
                self.program = pv
                self.broadcast(pv)
                if pv["error"]:
                    self.event("warn", "Preview: %s" % pv["error"])
            # the jog deadman
            for a, seen in list(self.jog_seen.items()):
                if time.time() - seen > JOG_DEADMAN:
                    self.jog_seen.pop(a, None)
                    await self.run(self.m.cmd_jog_stop, a)
                    self.event("warn", "Jog %s stopped: no keep-alive from the browser" % a.upper())
            await asyncio.sleep(max(0.0, period - (time.time() - t0)))

    async def slow_loop(self):
        while not (self.quit and self.quit.is_set()):
            try:
                for lvl, text in self.m.events():
                    self.event(lvl, text)
                sl = self.m.slow()
                diff = {k: v for k, v in sl.items() if self.slow_state.get(k) != v}
                self.slow_state = sl
                if diff:
                    self.broadcast({"t": "slow", **diff})
                if sl.get("remote") is False and self.control:
                    c = self.clients.get(self.control)
                    self.control = None
                    self.broadcast({"t": "control", "holder": None})
                    self.event("warn", "Remote control switched off at the machine")
                fs = json.dumps(self.m.programs())
                if fs != self.files_sig:
                    self.files_sig = fs
                    self.broadcast({"t": "files", "files": json.loads(fs)})
                try:
                    ts = os.path.getmtime(self.m.tool_file)
                except OSError:
                    ts = None
                if ts != self.tools_sig:
                    self.tools_sig = ts
                    self.broadcast(self.tools_msg())
            except Exception as e:
                self.event("bad", "millweb: %s" % e)
            await asyncio.sleep(0.5)


def make_ssl(cfg):
    cert, key = cfg.file("MILLWEB", "CERT"), cfg.file("MILLWEB", "KEY")
    if not cert:
        return None
    ctx = ssl.create_default_context(ssl.Purpose.CLIENT_AUTH)
    ctx.load_cert_chain(cert, key)
    return ctx


async def amain(args):
    import linuxcnc
    try:
        import hal
        h = hal.component("millweb")
        h.ready()
    except Exception:
        hal = None
    cfg = machine_mod.Config(args.ini)
    m = machine_mod.Machine(cfg, linuxcnc, hal)
    quit_event = asyncio.Event()
    hub = Hub(m, cfg, quit_event)
    host = args.host or cfg.get("MILLWEB", "HOST", "0.0.0.0")
    port = args.port or int(cfg.num("MILLWEB", "PORT", 8080))
    srv = wsserver.Server(WEBROOT, hub.handle, ssl=make_ssl(cfg))
    server = await srv.serve(host, port)
    print("millweb: http%s://%s:%d/ (%s)" % ("s" if srv.ssl else "", host, port, cfg.path), flush=True)
    hub.event("info", "millweb %s up on port %d" % (VERSION, port))
    loop = asyncio.get_event_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        try:
            loop.add_signal_handler(sig, quit_event.set)
        except NotImplementedError:
            pass
    tasks = [asyncio.ensure_future(hub.poll_loop()), asyncio.ensure_future(hub.slow_loop())]
    await quit_event.wait()
    for t in tasks:
        t.cancel()
    server.close()


def main():
    ap = argparse.ArgumentParser(prog="millweb", description="Browser console backend for LinuxCNC")
    ap.add_argument("-ini", "--ini", dest="ini", default=os.environ.get("INI_FILE_NAME"))
    ap.add_argument("--host")
    ap.add_argument("--port", type=int)
    args, _ = ap.parse_known_args()
    if not args.ini:
        ap.error("the machine's INI file: -ini FILE (or INI_FILE_NAME)")
    asyncio.run(amain(args))


if __name__ == "__main__":
    main()
