"""A small HTTP + WebSocket server on asyncio, standard library only.

Static files from one directory, and WebSocket connections (RFC 6455) on
one path. Enough for millweb: text frames, ping/pong, close, fragmented
messages; no extensions. Runs on any Python 3.7+ (LinuxCNC's own, on a
Raspberry Pi as well).
"""
import asyncio
import base64
import hashlib
import json
import mimetypes
import os
import struct
import urllib.parse

GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


def _xor(data, mask):
    """data XOR the 4-byte mask repeated (as one big integer: fast)."""
    n = len(data)
    if not n:
        return data
    m = (mask * (n // 4 + 1))[:n]
    return (int.from_bytes(data, "big") ^ int.from_bytes(m, "big")).to_bytes(n, "big")
MAX_MESSAGE = 8 * 1024 * 1024  # a program upload fits


class WebSocket:
    """One connection: await recv() for a text message (None when closed),
    send(text) to queue one."""

    def __init__(self, reader, writer, peer):
        self.reader, self.writer, self.peer = reader, writer, peer
        self.closed = False
        self._lock = asyncio.Lock()

    async def _frame(self, opcode, payload):
        n = len(payload)
        head = bytes([0x80 | opcode])
        if n < 126:
            head += bytes([n])
        elif n < 65536:
            head += bytes([126]) + struct.pack(">H", n)
        else:
            head += bytes([127]) + struct.pack(">Q", n)
        async with self._lock:
            self.writer.write(head + payload)
            await self.writer.drain()

    async def send(self, text):
        if self.closed:
            return
        try:
            await self._frame(0x1, text.encode("utf-8"))
        except (ConnectionError, RuntimeError):
            self.closed = True

    async def send_json(self, obj):
        await self.send(json.dumps(obj, separators=(",", ":")))

    async def close(self, code=1000):
        if not self.closed:
            try:
                await self._frame(0x8, struct.pack(">H", code))
            except (ConnectionError, RuntimeError):
                pass
            self.closed = True
            try:
                self.writer.close()
            except Exception:
                pass

    async def recv(self):
        """The next text message, or None once the connection is gone."""
        parts, op0 = [], None
        while not self.closed:
            try:
                b0, b1 = await self.reader.readexactly(2)
                n = b1 & 0x7F
                if n == 126:
                    n = struct.unpack(">H", await self.reader.readexactly(2))[0]
                elif n == 127:
                    n = struct.unpack(">Q", await self.reader.readexactly(8))[0]
                if n > MAX_MESSAGE:
                    await self.close(1009)
                    return None
                mask = await self.reader.readexactly(4) if b1 & 0x80 else None
                data = await self.reader.readexactly(n)
            except (asyncio.IncompleteReadError, ConnectionError):
                self.closed = True
                return None
            if mask:
                data = _xor(data, mask)
            op, fin = b0 & 0x0F, b0 & 0x80
            if op == 0x8:  # close
                await self.close()
                return None
            if op == 0x9:  # ping
                await self._frame(0xA, data)
                continue
            if op == 0xA:  # pong
                continue
            if op in (0x1, 0x2):
                op0, parts = op, [data]
            elif op == 0x0:
                parts.append(data)
            if fin and op0 is not None:
                msg = b"".join(parts)
                if sum(len(p) for p in parts) > MAX_MESSAGE:
                    await self.close(1009)
                    return None
                return msg.decode("utf-8", "replace") if op0 == 0x1 else msg
        return None


class Server:
    """serve(host, port): static files from `root`, WebSockets on `ws_path`
    handed to `on_ws(ws, query)` (an async function that runs for the
    connection's life)."""

    def __init__(self, root, on_ws, ws_path="/ws", ssl=None, extra=None):
        self.root = os.path.realpath(root)
        self.on_ws = on_ws
        self.ws_path = ws_path
        self.ssl = ssl
        self.extra = extra or {}  # path -> async fn() -> (status, content_type, bytes)

    async def serve(self, host, port):
        return await asyncio.start_server(self._client, host, port, ssl=self.ssl)

    async def _client(self, reader, writer):
        peer = writer.get_extra_info("peername")
        try:
            line = await asyncio.wait_for(reader.readline(), 10)
            if not line:
                writer.close()
                return
            method, target, _ = line.decode("latin-1").split(" ", 2)
            headers = {}
            while True:
                h = await asyncio.wait_for(reader.readline(), 10)
                if h in (b"\r\n", b"\n", b""):
                    break
                k, _, v = h.decode("latin-1").partition(":")
                headers[k.strip().lower()] = v.strip()
        except (asyncio.TimeoutError, ValueError, ConnectionError):
            writer.close()
            return
        url = urllib.parse.urlsplit(target)
        query = dict(urllib.parse.parse_qsl(url.query))
        if url.path == self.ws_path and headers.get("upgrade", "").lower() == "websocket":
            key = headers.get("sec-websocket-key", "")
            accept = base64.b64encode(hashlib.sha1((key + GUID).encode()).digest()).decode()
            writer.write(("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                          "Sec-WebSocket-Accept: %s\r\n\r\n" % accept).encode())
            await writer.drain()
            ws = WebSocket(reader, writer, peer)
            try:
                await self.on_ws(ws, query)
            finally:
                await ws.close()
            return
        if method not in ("GET", "HEAD"):
            await self._send(writer, 405, "text/plain", b"method not allowed")
            return
        if url.path in self.extra:
            status, ctype, body = await self.extra[url.path]()
            await self._send(writer, status, ctype, body, method == "HEAD")
            return
        path = urllib.parse.unquote(url.path)
        if path.endswith("/"):
            path += "index.html"
        full = os.path.realpath(os.path.join(self.root, path.lstrip("/")))
        if not (full == self.root or full.startswith(self.root + os.sep)) or not os.path.isfile(full):
            await self._send(writer, 404, "text/plain", b"not found")
            return
        ctype = mimetypes.guess_type(full)[0] or "application/octet-stream"
        with open(full, "rb") as f:
            body = f.read()
        await self._send(writer, 200, ctype, body, method == "HEAD")

    async def _send(self, writer, status, ctype, body, head_only=False):
        reason = {200: "OK", 404: "Not Found", 405: "Method Not Allowed", 403: "Forbidden"}.get(status, "OK")
        writer.write(("HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nCache-Control: no-cache\r\n"
                      "X-Content-Type-Options: nosniff\r\nConnection: close\r\n\r\n" % (status, reason, ctype, len(body))).encode())
        if not head_only:
            writer.write(body)
        try:
            await writer.drain()
        except ConnectionError:
            pass
        writer.close()


async def connect(host, port, path="/ws"):
    """A client (for tests): returns a WebSocket that masks its frames."""
    reader, writer = await asyncio.open_connection(host, port)
    key = base64.b64encode(os.urandom(16)).decode()
    writer.write(("GET %s HTTP/1.1\r\nHost: %s:%d\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                  "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n" % (path, host, port, key)).encode())
    await writer.drain()
    status = await reader.readline()
    if b"101" not in status:
        raise ConnectionError("no upgrade: %r" % status)
    while (await reader.readline()) not in (b"\r\n", b""):
        pass
    return ClientSocket(reader, writer, (host, port))


class ClientSocket(WebSocket):
    async def _frame(self, opcode, payload):
        n, mask = len(payload), os.urandom(4)
        head = bytes([0x80 | opcode])
        if n < 126:
            head += bytes([0x80 | n])
        elif n < 65536:
            head += bytes([0x80 | 126]) + struct.pack(">H", n)
        else:
            head += bytes([0x80 | 127]) + struct.pack(">Q", n)
        data = _xor(payload, mask)
        async with self._lock:
            self.writer.write(head + mask + data)
            await self.writer.drain()

    async def recv_json(self, timeout=5.0):
        m = await asyncio.wait_for(self.recv(), timeout)
        return None if m is None else json.loads(m)
