"""A millweb test client: a WebSocket browser stand-in for the suites."""
import asyncio
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), "server"))
import wsserver  # noqa: E402


class C:
    """Replies by id; `st` merged from the diffs; every message kept in order."""

    def __init__(self, ws):
        self.ws, self.n, self.st, self.msgs, self.replies = ws, 0, {}, [], {}
        self.closed = False

    @classmethod
    async def open(cls, port):
        c = cls(await wsserver.connect("127.0.0.1", port))
        c.hello = await c.ws.recv_json(10)
        c.task = asyncio.ensure_future(c.reader())
        return c

    async def reader(self):
        while True:
            m = await self.ws.recv_json(3600)
            if m is None:
                self.closed = True
                return
            if m["t"] == "st":
                self.st.update(m)
            if m["t"] == "reply":
                self.replies[m["id"]] = m
            self.msgs.append(m)

    async def send(self, op, **kw):
        """Send without waiting; the id."""
        self.n += 1
        await self.ws.send_json({"op": op, "id": self.n, **kw})
        return self.n

    async def call(self, op, timeout=130, **kw):
        n = await self.send(op, **kw)
        end = time.time() + timeout
        while n not in self.replies and time.time() < end and not self.closed:
            await asyncio.sleep(0.02)
        return self.replies.pop(n, {"ok": False, "err": "connection closed" if self.closed else "no reply"})

    async def until(self, pred, timeout=15.0):
        end = time.time() + timeout
        while time.time() < end:
            try:
                if pred(self):
                    return True
            except (KeyError, IndexError, TypeError):
                pass
            await asyncio.sleep(0.05)
        return False

    def mark(self):
        """For `since`: the messages from here on."""
        return len(self.msgs)

    def since(self, mark, pred):
        return next((m for m in self.msgs[mark:] if pred(m)), None)

    def seen(self, pred):
        return self.since(0, pred)

    def last(self, t):
        return next((m for m in reversed(self.msgs) if m["t"] == t), None)

    def ev(self, mark, text):
        return self.since(mark, lambda m: m["t"] == "ev" and text in m["text"])

    async def close(self):
        await self.ws.close()
        self.task.cancel()


def segs(pv):
    """A program message's path as [kind, x0, y0, z0, x, y, z, feed, line, tool] segments."""
    out, d = [], pv.get("path") or []
    x0, y0, z0 = pv.get("start") or (0, 0, 0)
    for i in range(0, len(d), 7):
        k, x, y, z, f, line, tool = d[i:i + 7]
        out.append([k, x0, y0, z0, x, y, z, f, line, tool])
        x0, y0, z0 = x, y, z
    return out


def near(a, b, tol=1e-3):
    return all(abs(x - y) < tol for x, y in zip(a, b)) if isinstance(a, (list, tuple)) else abs(a - b) < tol
