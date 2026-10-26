"""sysupdate.py against a stand-in update helper (MILLWEB_UPDATE_CMD): the ops, the broadcasts, the checks."""
import asyncio
import json
import os
import sys
import tempfile
import textwrap
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import sysupdate  # noqa: E402

FAKE = textwrap.dedent('''\
    #!/usr/bin/env python3
    # a stand-in for ebaz-update: state in $FAKE_DIR/state.json
    import json, os, sys
    d = os.environ["FAKE_DIR"]; p = os.path.join(d, "state.json")
    s = json.load(open(p)) if os.path.exists(p) else {"version": "1.0", "install": None}
    a = sys.argv[1:]
    open(os.path.join(d, "calls"), "a").write(" ".join(a) + "\\n")
    rel = [{"version": "1.1", "date": "2026-10-26", "notes": "## New\\n- faster", "bundle": "b/1.1.raucb"},
           {"version": "1.0", "date": "2026-10-20", "notes": "first", "bundle": "b/1.0.raucb"},
           {"version": "0.9", "date": "2026-10-10", "notes": "old", "bundle": "b/0.9.raucb"}]
    if a == ["status"]:
        print(json.dumps({"version": s["version"], "slot": "A", "install": s["install"], "last": None}))
    elif a == ["check"]:
        for r in rel:
            r["current"] = r["version"] == s["version"]; r["newer"] = r["version"] > s["version"]
        print(json.dumps({"ok": True, "current": s["version"], "latest": "1.1", "update_available": s["version"] < "1.1", "releases": rel}))
    elif a[:1] == ["install"]:
        if a[1] not in [r["version"] for r in rel]:
            print(json.dumps({"ok": False, "error": "no release " + a[1]}))
        else:
            s["install"] = {"state": "running", "version": a[1], "percent": 10, "message": "Checking bundle"}
            print(json.dumps({"ok": True, "started": a[1]}))
    elif a == ["reboot"]:
        print(json.dumps({"ok": True}))
    json.dump(s, open(p, "w"))
''')


class SysUpdateOps(unittest.TestCase):
    def setUp(self):
        self.d = tempfile.mkdtemp()
        self.helper = os.path.join(self.d, "helper")
        open(self.helper, "w").write(FAKE)
        os.chmod(self.helper, 0o755)
        os.environ["FAKE_DIR"] = self.d
        os.environ["MILLWEB_UPDATE_CMD"] = self.helper
        self.sent, self.events = [], []
        self.su = sysupdate.SysUpdate(self.sent.append, lambda lvl, text: self.events.append((lvl, text)))

    def tearDown(self):
        del os.environ["MILLWEB_UPDATE_CMD"]

    def op(self, op, **m):
        return asyncio.run(self.su.op(op, dict(m, op=op), "tester"))

    def calls(self):
        return open(os.path.join(self.d, "calls")).read().split("\n")[:-1]

    def test_check_lists_releases_and_says_a_newer_one_is_there(self):
        r = self.op("sys_check")
        self.assertTrue(r["check"]["update_available"])
        self.assertEqual([x["version"] for x in r["check"]["releases"]], ["1.1", "1.0", "0.9"])
        self.assertEqual(r["status"]["version"], "1.0")
        self.assertEqual(self.sent[-1]["t"], "sys")          # every browser hears it
        self.assertIn("## New", r["check"]["releases"][0]["notes"])

    def test_install_older_and_newer_and_refuse_a_second(self):
        r = self.op("sys_install", version="0.9")             # a downgrade is an install like any other
        self.assertEqual(r, {"started": "0.9"})
        self.assertIn("install 0.9", self.calls())
        self.assertTrue(self.su.installing())
        self.assertIn("already running", self.op("sys_install", version="1.1"))
        self.assertIn("wait for it", self.op("sys_reboot"))  # no restart in the middle of an install
        self.assertTrue(any("installs system 0.9" in e[1] for e in self.events))

    def test_install_of_an_unknown_version_says_why(self):
        self.assertIn("no release 2.0", self.op("sys_install", version="2.0"))
        self.assertIn("which version", self.op("sys_install"))

    def test_reboot_asks_the_helper(self):
        self.assertEqual(self.op("sys_reboot"), {})
        self.assertIn("reboot", self.calls())

    def test_no_helper_no_feature(self):
        os.environ["MILLWEB_UPDATE_CMD"] = ""
        su = sysupdate.SysUpdate(lambda m: None, lambda lvl, text: None)
        self.assertFalse(su.enabled)
        self.assertIn("no system updates", asyncio.run(su.op("sys_check", {"op": "sys_check"}, "x")))

    def test_a_helper_that_prints_no_json_is_an_error_not_a_crash(self):
        open(self.helper, "w").write("#!/bin/sh\necho 'sudo: a password is required' >&2\nexit 1\n")
        r = self.op("sys_status")
        self.assertIsInstance(r, str)
        self.assertIn("password is required", r)


if __name__ == "__main__":
    unittest.main()
