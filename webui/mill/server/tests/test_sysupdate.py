"""sysupdate.py against a stand-in update helper (MILLWEB_UPDATE_CMD): the ops, the broadcasts, the checks."""
import asyncio
import json
import os
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import sysupdate  # noqa: E402

FAKE = os.path.join(os.path.dirname(os.path.dirname(HERE)), "test", "fake-update-helper")


class SysUpdateOps(unittest.TestCase):
    def setUp(self):
        self.d = tempfile.mkdtemp()
        self.helper = os.path.join(self.d, "helper")
        open(self.helper, "w").write(open(FAKE).read())  # a copy: one test replaces it
        os.chmod(self.helper, 0o755)
        os.environ["FAKE_DIR"] = self.d
        os.environ["MILLWEB_UPDATE_CMD"] = self.helper
        self.sent, self.events = [], []
        self.su = sysupdate.SysUpdate(self.sent.append, lambda lvl, text: self.events.append((lvl, text)))

    def tearDown(self):
        del os.environ["MILLWEB_UPDATE_CMD"]
        os.environ.pop("FAKE_INSTALL_DONE", None)

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

    def test_replies_dont_carry_a_message_type(self):
        # millweb and the supervisor merge these into {"t": "reply", ...}: a "t" here made the page see no reply
        for op in ("sys_status", "sys_check"):
            self.assertNotIn("t", self.op(op))

    def test_install_older_and_newer_and_refuse_a_second(self):
        r = self.op("sys_install", version="0.9")             # a downgrade is an install like any other
        self.assertEqual(r, {"started": "0.9"})
        self.assertIn("install 0.9", self.calls())
        self.assertTrue(self.su.installing())
        self.assertIn("already running", self.op("sys_install", version="1.1"))
        self.assertIn("wait for it", self.op("sys_reboot"))  # no restart in the middle of an install
        self.assertTrue(any("installs system 0.9" in e[1] for e in self.events))

    def test_a_finished_install_restarts_by_itself(self):
        os.environ["FAKE_INSTALL_DONE"] = "1"
        self.op("sys_install", version="1.1")
        self.assertTrue(self.su.installing())                 # installed, not restarted yet: still busy
        asyncio.run(self.su.announce_install())
        self.assertIn("reboot", self.calls())
        self.assertEqual(self.su.restarting, "1.1")
        self.assertEqual(self.sent[-1]["restarting"], "1.1")  # the page says so
        asyncio.run(self.su.announce_install())
        self.assertEqual(self.calls().count("reboot"), 1)      # once

    def test_no_restart_while_a_program_runs(self):
        os.environ["FAKE_INSTALL_DONE"] = "1"
        why = ["a program is running"]
        self.su.busy = lambda: why[0]
        self.op("sys_install", version="1.1")
        asyncio.run(self.su.announce_install())
        asyncio.run(self.su.announce_install())
        self.assertNotIn("reboot", self.calls())
        self.assertEqual(sum("when the program has ended" in e[1] for e in self.events), 1)
        why[0] = None                                         # the program has ended
        asyncio.run(self.su.announce_install())
        self.assertIn("reboot", self.calls())

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
