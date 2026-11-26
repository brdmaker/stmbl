#!/usr/bin/env python3
"""Unit tests for soak.py's feedback frame counters (2026-11-26: a soak had passed
with 6-9 % of the Yaskawa encoder's frames lost, unseen).

    python3 hwtest/soak/test_soak.py
"""
import argparse
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import soak  # noqa: E402

GOOD, ERRS = soak.FB_COUNTERS["yaskawa_abs0"]


def reading(ok, crc=0, frame=0, noreply=0):
    return {"ok": ok, "crc_er": crc, "frame_er": frame, "noreply": noreply}


class FbCounters(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()
        args = argparse.Namespace(out=self.dir, max_fb_err_pct=0.01, target="hw", mode="motion", port="x")
        self.run = soak.Run(args)

    def tearDown(self):
        for f in (self.run.console_log, self.run.events_f, self.run.health_f, self.run.cycles_f):
            f.close()

    def test_delta_first_and_reset(self):
        self.assertIsNone(soak.fb_delta(None, reading(100)))
        self.assertEqual(soak.fb_delta(reading(100, frame=2), reading(250, frame=5))["frame_er"], 3)
        # the drive restarted: its counters went back
        self.assertIsNone(soak.fb_delta(reading(1000, frame=9), reading(50)))

    def test_clean_interval(self):
        self.assertTrue(soak.fb_check(self.run, soak.fb_delta(reading(0), reading(50000)), "yaskawa_abs0",
                                      GOOD, ERRS))
        self.assertEqual(self.run.events, [])

    def test_few_errors_info(self):
        # 1 in 50001: 0.002 %, under 0.01 %: noted, not a failure
        d = soak.fb_delta(reading(0), reading(50000, frame=1))
        self.assertTrue(soak.fb_check(self.run, d, "yaskawa_abs0", GOOD, ERRS))
        self.assertEqual([(e[1], e[2]) for e in self.run.events], [("fb_errors", False)])

    def test_many_errors_fail(self):
        # the bad cable of 2026-11-25: 13 % frame errors
        d = soak.fb_delta(reading(0), reading(23169, frame=3418))
        self.assertFalse(soak.fb_check(self.run, d, "yaskawa_abs0", GOOD, ERRS))
        self.assertTrue(self.run.events[-1][2])
        self.assertIn("frame_er 3418", self.run.events[-1][3])

    def test_summary_totals(self):
        soak.fb_check(self.run, soak.fb_delta(reading(0), reading(1000, frame=1)), "yaskawa_abs0", GOOD, ERRS)
        soak.fb_check(self.run, soak.fb_delta(reading(1000, frame=1), reading(3000, frame=1, crc=2)),
                      "yaskawa_abs0", GOOD, ERRS)
        self.run.summary(final=True)
        with open(os.path.join(self.dir, "summary.md")) as f:
            text = f.read()
        self.assertIn("| Feedback yaskawa_abs0 | 3003 frames, 3 bad", text)
        self.assertIn("crc_er 2, frame_er 1, noreply 0", text)


if __name__ == "__main__":
    unittest.main()
