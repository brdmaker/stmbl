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
import base64
import binascii
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
NO_REPLY = object()  # a handler that sent its reply itself
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
        self.upool = concurrent.futures.ThreadPoolExecutor(max_workers=1, thread_name_prefix="millweb-urgent")
        self.state = {}
        self.slow_state = {}
        self.program = None          # the last preview sent (its JSON text)
        self.pv_cache = {}           # key -> (text, head): the last few previews
        self.pv_running = {}         # key -> future: one child process per preview
        self.pv_sem = asyncio.Semaphore(2)
        self.pv_segs = int(cfg.num("MILLWEB", "PREVIEW_SEGS", 100000))
        self.pv_timeout = cfg.num("MILLWEB", "PREVIEW_TIMEOUT", 120.0)
        self.resyncing = False
        self.line_index = {}         # path -> (mtime, size, byte offset of every 1000th line, lines)
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
        await ws.send_json(self.files_msg())
        await ws.send_json(self.setup_msg())
        if self.program:
            await ws.send(self.program)
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

    async def lose_control(self, c, released=False):
        self.control = None
        await self.urgent(self.m.cmd_jog_stop)
        self.jog_seen.clear()
        if released:
            self.event("info", "%s released control" % c.name)
        elif self.on_loss == "hold" and self.state.get("run") == "run":
            await self.run(self.m.cmd_cycle, "pause")
            self.event("warn", "The controlling browser went away: feed hold")
        else:
            self.event("warn", "The controlling browser went away; the machine carries on")
        self.broadcast({"t": "control", "holder": None})

    async def run(self, fn, *a, **kw):
        loop = asyncio.get_event_loop()
        return await loop.run_in_executor(self.pool, lambda: fn(*a, **kw))

    async def urgent(self, fn, *a, **kw):
        """E-STOP, stop, jog stop: not behind a command that is waiting."""
        loop = asyncio.get_event_loop()
        return await loop.run_in_executor(self.upool, lambda: fn(*a, **kw))

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
                err = await self.urgent(self.m.cmd_estop, bool(msg.get("on", True)))
                if msg.get("on", True):
                    self.event("bad", "E-STOP from %s" % c.name)
                return await reply(err)
            if op == "jog" and msg.get("inc", 0) == 0 and self.control == c.id:
                # the deadman: keep-alive of a running continuous jog
                a = str(msg.get("axis", "x")).lower()
                if a in self.jog_seen:
                    self.jog_seen[a] = time.time()
                    return await reply()
            if op in ("files", "program_get", "program_lines", "model_get", "ping", "events"):
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
            if res is NO_REPLY:
                return
            if isinstance(res, dict):
                return await reply(None, **res)
            return await reply(res)
        except (ValueError, KeyError, TypeError, IndexError, OSError) as e:
            return await reply(str(e) or type(e).__name__)
        except Exception as e:  # a bug: say so, keep serving
            self.event("bad", "millweb: %s failed: %r" % (op, e))
            return await reply("internal error: %r" % e)

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
            await self.lose_control(c, released=True)
            return None
        self.broadcast({"t": "control", "holder": self.control})
        return None

    async def op_ping(self, c, m):
        return {"time": time.time()}

    async def op_events(self, c, m):
        return {"events": [{"time": t, "lvl": l, "text": x} for t, l, x in self.events[-max(1, int(m.get("n", 300))):]]}

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
        return await self.urgent(self.m.cmd_jog_stop, a)

    async def op_mdi(self, c, m):
        text = str(m.get("cmd", "")).strip()
        if not text or len(text) > 256:
            return "an MDI line: 1 to 256 characters"
        self.event("info", "MDI: %s" % text)
        return await self.run(self.m.mdi, text, bool(m.get("wait")))  # wait: reply when done

    async def op_cycle(self, c, m):
        act = m.get("action", "start")
        if act == "stop":
            err = await self.urgent(self.m.cmd_stop)
        else:
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
        files, dirs = self.m.programs()
        return {"files": files, "dirs": dirs}

    async def op_program_get(self, c, m):
        path = self.m._safe(m["name"])
        text, head = await self.preview(path)
        # the preview's text goes in as it is: no second encoding of a big message
        await c.ws.send('{"t":"reply","id":%s,"op":"program_get","ok":true,"program":%s}' % (json.dumps(m.get("id")), text))
        return NO_REPLY

    async def op_program_lines(self, c, m):
        """Lines start ... start+count-1 (from 1) of a program: the listing of a
        long one comes in windows."""
        path = self.m._safe(m["name"])
        start, count = max(1, int(m.get("start", 1))), max(1, min(5000, int(m.get("count", 2000))))
        loop = asyncio.get_event_loop()
        lines, n = await loop.run_in_executor(self.ppool, self.read_lines, path, start, count)
        return {"start": start, "lines": lines, "nlines": n}

    async def op_program_save(self, c, m):
        text = str(m.get("text", ""))
        if len(text) > 6 * 1024 * 1024:
            return "too large"
        err = await self.run(self.m.cmd_program_save, m["name"], text, bool(m.get("overwrite", True)))
        if not err:
            self.event("ok", "Saved %s" % m["name"])
            self.broadcast(self.files_msg())
        return err

    async def op_program_delete(self, c, m):
        err = await self.run(self.m.cmd_program_delete, m["name"])
        if not err:
            self.event("info", "Deleted %s" % m["name"])
            self.broadcast(self.files_msg())
        return err

    async def op_program_rename(self, c, m):
        """A program or a folder renamed or moved: {name, to}."""
        err = await self.run(self.m.cmd_rename, m["name"], m["to"])
        if not err:
            self.event("info", "Renamed %s to %s" % (m["name"], m["to"]))
            self.broadcast(self.files_msg())
        return err

    async def op_model_save(self, c, m):
        """A 3D model (STL or OBJ) uploaded: {name, data (base64), overwrite}."""
        try:
            data = base64.b64decode(str(m.get("data", "")), validate=True)
        except (ValueError, binascii.Error):
            return "the model's data isn't base64"
        err = await self.run(self.m.cmd_model_save, m["name"], data, bool(m.get("overwrite", True)))
        if not err:
            self.event("ok", "Saved %s (%.1f MB)" % (m["name"], len(data) / 1e6))
            self.broadcast(self.files_msg())
        return err

    async def op_model_get(self, c, m):
        loop = asyncio.get_event_loop()
        data, mtime = await loop.run_in_executor(self.ppool, self.m.model_read, m["name"])
        return {"name": m["name"], "mtime": mtime, "data": base64.b64encode(data).decode()}

    async def op_setup_save(self, c, m):
        """The objects on the table for the 3D view: every browser gets them."""
        err = await self.run(self.m.cmd_setup_save, m["objects"])
        if not err:
            self.broadcast(self.setup_msg())
        return err

    async def op_folder_create(self, c, m):
        err = await self.run(self.m.cmd_folder_create, m["name"])
        if not err:
            self.event("info", "New folder %s" % m["name"])
            self.broadcast(self.files_msg())
        return err

    async def op_folder_delete(self, c, m):
        err = await self.run(self.m.cmd_folder_delete, m["name"])
        if not err:
            self.event("info", "Deleted folder %s" % m["name"])
            self.broadcast(self.files_msg())
        return err

    async def op_program_load(self, c, m):
        err = await self.run(self.m.cmd_program_load, m["name"])
        if not err:
            self.event("ok", "Loaded %s" % m["name"])
            # its preview, also when it is the same file again (edited on disk since?)
            self.program_file = self.m._safe(m["name"])
            asyncio.ensure_future(self.send_preview(self.program_file))
        return err

    async def op_pcomp(self, c, m):
        return await self.run(self.m.cmd_pcomp, m["k"], m["v"])

    # ------------------------------------------------------------ data
    def setup_msg(self):
        return {"t": "setup", "objects": self.m.setup()}

    def files_msg(self):
        files, dirs = self.m.programs()
        return {"t": "files", "files": files, "dirs": dirs}

    def tools_msg(self):
        return {"t": "tools", "tools": self.m.tools()}

    def offsets_msg(self):
        return {"t": "offsets", "offsets": self.m.offsets, "g92": self.m.g92, "wcs": self.state.get("wcs", 0)}

    def read_lines(self, path, start, count):
        """(lines start ... start+count-1, how many lines), through an index of
        every 1000th line's offset, so line 900 000 of a 30 MB file is quick."""
        st = os.stat(path)
        ix = self.line_index.get(path)
        if not ix or ix[0] != st.st_mtime or ix[1] != st.st_size:
            offs, n, pos = [0], 0, 0
            with open(path, "rb") as f:
                for line in f:
                    n += 1
                    pos += len(line)
                    if n % 1000 == 0:
                        offs.append(pos)
            ix = (st.st_mtime, st.st_size, offs, n)
            self.line_index = {path: ix}  # the last file only
        offs, n = ix[2], ix[3]
        k = min((start - 1) // 1000, len(offs) - 1)
        out = []
        with open(path, "rb") as f:
            f.seek(offs[k])
            ln = k * 1000
            for line in f:
                ln += 1
                if ln >= start:
                    out.append(line.decode("utf-8", "replace").rstrip("\r\n"))
                    if len(out) >= count:
                        break
        return out, n

    async def preview(self, path):
        """(the program message as JSON text, its header): from LinuxCNC's
        interpreter in a child process (preview.py); cached; one at a time per file."""
        s = self.state
        active = [round(s.get("g5x", [0, 0, 0])[i] + s.get("g92", [0, 0, 0])[i], 4) for i in range(3)]
        tl = self.m.tools()
        try:
            st = os.stat(path)
            key = (path, st.st_mtime, st.st_size, tuple(active), json.dumps(tl), s.get("wcs", 0))
        except OSError:
            key = (path, None)
        if key in self.pv_cache:
            return self.pv_cache[key]
        if key in self.pv_running:
            return await asyncio.shield(self.pv_running[key])
        fut = asyncio.get_event_loop().create_future()
        self.pv_running[key] = fut
        try:
            async with self.pv_sem:
                res = await self._preview_child(path, active, tl)
            self.pv_cache[key] = res
            while len(self.pv_cache) > 3:
                self.pv_cache.pop(next(iter(self.pv_cache)))
            fut.set_result(res)
            return res
        except BaseException as e:
            fut.set_exception(e)
            raise
        finally:
            del self.pv_running[key]
            if fut.done() and not fut.cancelled():
                fut.exception()  # retrieved: no "never retrieved" warning

    async def _preview_child(self, path, active, tl):
        req = {"path": path, "var": self.m.var_file, "unitcode": "G21", "active": active,
               "tools": {t["t"]: (t["t"], 0.0, 0.0, t["z"] / 25.4, 0, 0, 0, 0, 0, 0, t["d"] / 25.4, 0, 0, 0) for t in tl},
               "pockets": [t["t"] for t in tl], "max_segs": self.pv_segs, "timeout": self.pv_timeout,
               "rapid": self.m.maxv * 60, "acc": self.cfg.num("TRAJ", "MAX_LINEAR_ACCELERATION", 1000.0)}
        head, ptext = {"error": None, "truncated": None, "stats": None, "lines": 0, "start": [0, 0, 0]}, "[]"
        proc = await asyncio.create_subprocess_exec(sys.executable, preview.__file__, stdin=asyncio.subprocess.PIPE,
                                                    stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)
        try:
            out, err = await asyncio.wait_for(proc.communicate(json.dumps(req).encode()), self.pv_timeout + 30)
        except asyncio.TimeoutError:
            proc.kill()
            await proc.wait()
            out, err = b"", b"no answer in time"
        first, _, rest = out.partition(b"\n")
        try:
            head.update(json.loads(first))
            ptext = rest.decode().strip() or "[]"
        except ValueError:
            tail = err.decode(errors="replace").strip().splitlines()[-1:] or ["exit %s" % proc.returncode]
            head["error"] = "the preview failed: %s" % tail[0]
        loop = asyncio.get_event_loop()
        try:
            lines, n = await loop.run_in_executor(self.ppool, self.read_lines, path, 1, 2000)
        except OSError:
            lines, n = [], 0
        msg = {"t": "program", "name": self.m.relpath(path), "path": path, "lines": lines, "line0": 1, "nlines": n,
               "error": head["error"], "truncated": head["truncated"], "stats": head["stats"], "start": head["start"],
               "wcs": self.state.get("wcs", 0), "seconds": head.get("seconds")}
        return json.dumps(msg, separators=(",", ":"))[:-1] + ',"path":' + ptext + "}", msg

    # ------------------------------------------------------------ loops
    async def resync(self):
        self.resyncing = True
        try:
            note = await self.run(self.m.resync_offsets)
            if note:
                self.event("warn", note)
                self.broadcast(self.offsets_msg())
        finally:
            await asyncio.sleep(2.0)  # not again at once if it didn't help
            self.resyncing = False

    async def send_preview(self, path):
        """The loaded program's preview to every browser (in the background:
        a long program doesn't hold up the status)."""
        try:
            text, head = await self.preview(path)
        except Exception as e:
            self.event("bad", "Preview: %s" % e)
            return
        if path != self.program_file:
            return  # another program was loaded meanwhile
        self.program = text
        self.broadcast_raw(text)
        if head["error"]:
            self.event("warn", "Preview: %s" % head["error"])
        if head["truncated"]:
            self.event("warn", "Preview: %s" % head["truncated"])

    def broadcast_raw(self, text):
        for c in list(self.clients.values()):
            asyncio.ensure_future(c.ws.send(text))

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
                asyncio.ensure_future(self.send_preview(st["file"]))
            # the status without a work offset (DEBUGNOTES): fix it when the machine allows
            if self.m.wcs_unknown and st["run"] == "idle" and all(st["homed"]) and not self.resyncing:
                asyncio.ensure_future(self.resync())
            # the jog deadman
            for a, seen in list(self.jog_seen.items()):
                if time.time() - seen > JOG_DEADMAN:
                    self.jog_seen.pop(a, None)
                    await self.urgent(self.m.cmd_jog_stop, a)
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
                fm = self.files_msg()
                fs = json.dumps(fm)
                if fs != self.files_sig:
                    self.files_sig = fs
                    self.broadcast(fm)
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
