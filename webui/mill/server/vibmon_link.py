"""millweb's link to vibmon (tools/vibmon, the vibration sensors' PC side):
its JSON socket ([MILLWEB] VIBMON = /run/vibmon.sock), read in the event
loop. What comes in goes to on_msg; send() queues a command. When vibmon
isn't there (yet, or any more) the link tries again every 2 s and says so.
"""
import asyncio
import json


class VibLink:
    def __init__(self, path, on_msg, on_state=None, retry=2.0):
        self.path = path
        self.on_msg = on_msg
        self.on_state = on_state or (lambda up: None)
        self.retry = retry
        self.writer = None
        self.up = False
        self.status = None  # vibmon's last status

    async def run(self, quit_event=None):
        while not (quit_event and quit_event.is_set()):
            try:
                reader, self.writer = await asyncio.open_unix_connection(self.path)
            except OSError:
                await asyncio.sleep(self.retry)
                continue
            self._set(True)
            try:
                while True:
                    line = await reader.readline()
                    if not line:
                        break
                    try:
                        m = json.loads(line)
                    except ValueError:
                        continue
                    if m.get("type") == "status":
                        self.status = m
                    self.on_msg(m)
            except (OSError, asyncio.IncompleteReadError):
                pass
            finally:
                self.writer = None
                self.status = None
                self._set(False)
            await asyncio.sleep(self.retry)

    def _set(self, up):
        if up != self.up:
            self.up = up
            self.on_state(up)

    def send(self, cmd):
        """Queue a command for vibmon; an error text when it isn't there."""
        if not self.writer:
            return "vibmon isn't running ([MILLWEB] VIBMON = %s)" % self.path
        self.writer.write((json.dumps(cmd) + "\n").encode())
        return None
