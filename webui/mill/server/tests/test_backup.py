"""backup.py and the console's ops (supervisor.ConfigOps): download the machine folder as a zip, restore it."""
import base64
import io
import json
import os
import sys
import tempfile
import unittest
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import backup  # noqa: E402
import machineconf  # noqa: E402
import supervisor  # noqa: E402


def write(p, text):
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "w") as f:
        f.write(text)


def read(p):
    with open(p) as f:
        return f.read()


class Backup(unittest.TestCase):
    def setUp(self):
        self.mdir = tempfile.mkdtemp()
        m = dict(machineconf.DEFAULT, name="Test mill")
        write(os.path.join(self.mdir, "machine.json"), json.dumps(m, indent=2) + "\n")
        write(os.path.join(self.mdir, "custom.hal"), "# mine\n")
        write(os.path.join(self.mdir, "tool.tbl"), "T1 P1 D6\n")
        write(os.path.join(self.mdir, "comp", "blink.comp"), "component blink;\n")
        write(os.path.join(self.mdir, "nc_files", "part.ngc"), "G0 X0\nM2\n")
        write(os.path.join(self.mdir, "run", "status.json"), "{}")      # runtime: never in a backup
        write(os.path.join(self.mdir, "logs", "linuxcnc.log"), "x")
        write(os.path.join(self.mdir, "tool.tbl.tmp"), "half written")
        self.ops = supervisor.ConfigOps(self.mdir)

    def names(self, data):
        return sorted(zipfile.ZipFile(io.BytesIO(data)).namelist())

    def upload(self, data, piece=1000):
        r = None
        for off in range(0, len(data), piece):
            r = self.ops.restore_upload(off, base64.b64encode(data[off:off + piece]).decode(), len(data))
        return r

    def test_the_zip_holds_the_machine_not_the_runtime(self):
        name, data = backup.make(self.mdir)
        self.assertRegex(name, r"^test-mill-\d{8}-\d{4}\.zip$")
        self.assertEqual(self.names(data), ["comp/blink.comp", "custom.hal", "machine.json", "millweb-backup.json",
                                            "nc_files/part.ngc", "tool.tbl"])
        self.assertNotIn("nc_files/part.ngc", self.names(backup.make(self.mdir, programs=False)[1]))
        man = json.loads(zipfile.ZipFile(io.BytesIO(data)).read("millweb-backup.json"))
        self.assertEqual((man["machine"], man["programs"]), ("Test mill", True))

    def test_backup_get_op(self):
        r = self.ops.backup_get(programs=False)
        self.assertEqual(r["size"], len(base64.b64decode(r["data"])))

    def test_restore_brings_the_folder_back_and_keeps_a_copy_of_before(self):
        _, data = backup.make(self.mdir)
        write(os.path.join(self.mdir, "custom.hal"), "# changed since\n")
        write(os.path.join(self.mdir, "comp", "new.comp"), "component new;\n")
        os.remove(os.path.join(self.mdir, "tool.tbl"))
        r = self.upload(data)                                # in pieces, as the page sends it
        s = r["restore"]
        self.assertEqual((s["machine"], s["changed"], s["new"], s["removed"]), ("Test mill", ["custom.hal"], ["tool.tbl"], ["comp/new.comp"]))
        self.assertEqual(s["problems"], [])
        self.ops.restore_queue()
        note = backup.apply(self.mdir, backup.pending_path(self.mdir))
        self.assertIn("Restored Test mill", note)
        self.assertEqual(read(os.path.join(self.mdir, "custom.hal")), "# mine\n")
        self.assertTrue(os.path.exists(os.path.join(self.mdir, "tool.tbl")))
        self.assertFalse(os.path.exists(os.path.join(self.mdir, "comp", "new.comp")))
        self.assertTrue(os.path.exists(os.path.join(self.mdir, "run", "status.json")))   # runtime untouched
        copies = os.listdir(os.path.join(self.mdir, "backups"))
        self.assertEqual(len(copies), 1)
        before = zipfile.ZipFile(os.path.join(self.mdir, "backups", copies[0]))
        self.assertEqual(before.read("custom.hal"), b"# changed since\n")             # undo: the copy of before

    def test_a_backup_without_programs_leaves_the_programs_alone(self):
        _, data = backup.make(self.mdir, programs=False)
        write(os.path.join(self.mdir, "nc_files", "newer.ngc"), "M2\n")
        self.assertEqual(self.upload(data)["restore"]["removed"], [])
        self.ops.restore_queue()
        backup.apply(self.mdir, backup.pending_path(self.mdir))
        self.assertTrue(os.path.exists(os.path.join(self.mdir, "nc_files", "newer.ngc")))

    def test_a_zip_of_the_folder_itself_works_too(self):
        buf = io.BytesIO()
        with zipfile.ZipFile(buf, "w") as z:                # Windows: Send to, compressed folder
            z.writestr("machine/machine.json", read(os.path.join(self.mdir, "machine.json")))
            z.writestr("machine/custom.hal", "# from windows\n")
        s = self.upload(buf.getvalue())["restore"]
        self.assertEqual(s["changed"], ["custom.hal"])

    def test_problems_in_the_backups_machine_json_are_shown(self):
        buf = io.BytesIO()
        with zipfile.ZipFile(buf, "w") as z:
            z.writestr("machine.json", '{"name": "x", "axes": "nonsense"}')
        self.assertTrue(self.upload(buf.getvalue())["restore"]["problems"])

    def test_what_isnt_a_backup_is_refused(self):
        for files, why in (({"readme.txt": "x"}, "no machine.json"), ({"machine.json": "{}", "../evil": "x"}, "won't write")):
            buf = io.BytesIO()
            with zipfile.ZipFile(buf, "w") as z:
                for n, t in files.items():
                    z.writestr(n, t)
            with self.assertRaises(ValueError) as e:
                self.upload(buf.getvalue())
            self.assertIn(why, str(e.exception))
        with self.assertRaises(ValueError) as e:
            self.upload(b"not a zip at all")
        self.assertIn("not a zip", str(e.exception))

    def test_pieces_out_of_step_are_refused(self):
        self.ops.restore_upload(0, base64.b64encode(b"abc").decode(), 10)
        with self.assertRaises(ValueError) as e:
            self.ops.restore_upload(5, base64.b64encode(b"abc").decode(), 10)
        self.assertIn("out of step", str(e.exception))

    def test_only_the_last_copies_of_before_are_kept(self):
        d = os.path.join(self.mdir, "backups")
        os.makedirs(d)
        for i in range(7):
            write(os.path.join(d, "before-restore-2026010%d-000000.zip" % i), "x")
        backup.safety_copy(self.mdir)
        self.assertEqual(len(os.listdir(d)), backup.KEEP)


if __name__ == "__main__":
    unittest.main()
