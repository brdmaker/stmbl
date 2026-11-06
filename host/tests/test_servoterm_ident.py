"""servoterm-cli against a simulated drive on a pty (stmbl_host --pty): the
drive's name and motor type (shared/confident.h) in the prompt, --expect-name,
and --load refusing a config for another drive.
python3 host/tests/test_servoterm_ident.py"""
import os
import pty
import select
import shutil
import subprocess
import sys
import tempfile
import time
import unittest

HOST = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.dirname(HOST)
CLI = os.path.join(ROOT, "servoterm-cli", "servoterm-cli")
CONF = """#stmbl_name x-axis
#stmbl_motor SEW CMP40M
link pid
link pmsm
conf0.polecount = 3
"""


class Ident(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.conf = os.path.join(self.dir, "drive.conf")
        with open(self.conf, "w") as f:
            f.write(CONF)
        self.tty = os.path.join(self.dir, "tty")
        self.drive = subprocess.Popen([os.path.join(HOST, "stmbl_host"), "--drive", self.conf, "--pty", self.tty],
                                      cwd=HOST, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        end = time.time() + 10
        while not os.path.exists(self.tty) and time.time() < end:
            time.sleep(0.05)
        time.sleep(0.3)

    def tearDown(self):
        self.drive.terminate()
        self.drive.wait()
        shutil.rmtree(self.dir)

    def cli(self, *args):
        p = subprocess.run([CLI, "-d", self.tty] + list(args), capture_output=True, text=True, timeout=30)
        return p.returncode, p.stdout + p.stderr

    def test_expect_name(self):
        rc, out = self.cli("--expect-name", "y-axis", "-c", "name z")
        self.assertEqual(rc, 2)
        self.assertIn("is drive 'x-axis', not 'y-axis': nothing done", out)
        rc, out = self.cli("--expect-name", "x-axis", "-c", "ident")
        self.assertEqual(rc, 0)
        self.assertIn('ident name="x-axis" motor="SEW CMP40M"', out)

    def test_load_other_drive(self):
        other = os.path.join(self.dir, "y.conf")
        with open(other, "w") as f:
            f.write(CONF.replace("x-axis", "y-axis"))
        rc, out = self.cli("--load", other, "--no-backup")
        self.assertNotEqual(rc, 0)
        self.assertIn("is for drive 'y-axis', this is drive 'x-axis': drive not touched", out)
        rc, out = self.cli("--load", other, "--no-backup", "--force-name")
        self.assertEqual(rc, 0, out)
        with open(self.conf) as f:  # --pty writes flash back
            self.assertIn("#stmbl_name y-axis", f.read())

    def test_load_into_erased_flash(self):
        """A new drive's config flash was never written: erased, all 0xFF (the
        bench, 2026-10-29). showconf printed the 0xFF bytes, servoterm-cli took
        0xFF for a scope frame's marker and swallowed the next 8 bytes
        ("adding #"), and --load aborted at its first line."""
        with open(self.conf, "wb") as f:
            f.write(b"\xff" * 15000)
        self.tearDown()
        self.setUp_erased()
        rc, out = self.cli("-c", "version")
        self.assertIn("conf=0", out)                       # an empty config, not garbage
        new = os.path.join(self.dir, "new.conf")
        with open(new, "w") as f:
            f.write(CONF)
        rc, out = self.cli("--load", new, "--no-backup")
        self.assertEqual(rc, 0, out)
        self.assertIn("written to flash and verified", out)

    def setUp_erased(self):
        self.dir = tempfile.mkdtemp()
        self.conf = os.path.join(self.dir, "drive.conf")
        with open(self.conf, "wb") as f:
            f.write(b"\xff" * 15000)
        self.tty = os.path.join(self.dir, "tty")
        self.drive = subprocess.Popen([os.path.join(HOST, "stmbl_host"), "--drive", self.conf, "--pty", self.tty],
                                      cwd=HOST, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        end = time.time() + 10
        while not os.path.exists(self.tty) and time.time() < end:
            time.sleep(0.05)
        time.sleep(0.3)

    def test_prompt(self):
        pid, fd = pty.fork()
        if pid == 0:
            os.execv(CLI, [CLI, "-d", self.tty])
        out = b""

        def read(sec):
            nonlocal out
            end = time.time() + sec
            while time.time() < end:
                if select.select([fd], [], [], 0.1)[0]:
                    try:
                        out += os.read(fd, 4096)
                    except OSError:
                        return
        read(2)
        os.write(fd, b"name y-axis\n")
        read(2)
        os.kill(pid, 15)
        os.waitpid(pid, 0)
        text = out.decode(errors="replace")
        self.assertIn("x-axis SEW CMP40M> ", text)
        self.assertIn("y-axis SEW CMP40M> ", text)


if __name__ == "__main__":
    if not os.path.exists(CLI):
        subprocess.run(["make", "-s", "-C", os.path.dirname(CLI)], check=True)
    sys.exit(unittest.main(verbosity=0, exit=False).result.wasSuccessful() is False)
