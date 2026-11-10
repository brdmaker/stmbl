"""servoterm-cli --update against a fake drive on a pty that answers every
fwdata line "fw busy": a drive whose realtime loop stopped (RT_TOO_LONG)
never programs the staged lines. servoterm-cli looped for ever (bench,
2026-11-10, DEBUGNOTES "servoterm-cli --update hangs"); it must give up.
python3 host/tests/test_servoterm_update.py"""
import os
import pty
import select
import shutil
import subprocess
import tempfile
import threading
import time
import unittest

HOST = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.dirname(HOST)
CLI = os.path.join(ROOT, "servoterm-cli", "servoterm-cli")


def fake_drive(master, stop):
    buf = b""
    while not stop.is_set():
        r, _, _ = select.select([master], [], [], 0.05)
        if not r:
            continue
        try:
            buf += os.read(master, 4096)
        except OSError:
            return
        while b"\n" in buf or b"\r" in buf:
            i = min(x for x in (buf.find(b"\n"), buf.find(b"\r")) if x >= 0)
            line, buf = buf[:i].decode(errors="replace").strip(), buf[i + 1:]
            if not line:
                continue
            if line == "ident":
                out = 'ident name="" motor=""\n'
            elif line.startswith("fwbegin"):
                out = "fw ok 0\n"
            elif line.startswith("fwdata"):
                # as shared/fwstage.c with its queue full: the next line
                # busy, the ones after it a gap (-3) at what it has
                off = int(line.split()[1])
                out = "fw busy\n" if off == 0 else "fw err -3 at 0\n"
            else:
                out = "\n"
            os.write(master, out.encode())


class StuckUpdate(unittest.TestCase):
    def test_gives_up(self):
        d = tempfile.mkdtemp()
        try:
            img = os.path.join(d, "img.bin")
            with open(img, "wb") as f:
                f.write(os.urandom(300))
            master, slave = pty.openpty()
            stop = threading.Event()
            t = threading.Thread(target=fake_drive, args=(master, stop), daemon=True)
            t.start()
            t0 = time.time()
            try:
                p = subprocess.run([CLI, "-d", os.ttyname(slave), "--update", img, "--no-apply"],
                                   capture_output=True, text=True, timeout=60)
            except subprocess.TimeoutExpired:
                self.fail("servoterm-cli --update still running after 60 s on a drive that stays busy")
            finally:
                stop.set()
            out = p.stdout + p.stderr
            self.assertNotEqual(p.returncode, 0, out)
            self.assertIn("stays busy", out)
            self.assertLess(time.time() - t0, 40)
        finally:
            shutil.rmtree(d)


if __name__ == "__main__":
    unittest.main()
