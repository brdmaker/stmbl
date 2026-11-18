"""host/arm/lcnc/Makefile (stmbl_arm, the LinuxCNC module): every object is
rebuilt when a header it includes changes. Its rules listed headers by hand
and missed some: after geom.h and tool.h grew (2026-11-14), armcore.o and
stmbl_arm_rt.o were kept with the old struct layouts and the module ran
with them mixed (the drive library's path read empty). Needs LinuxCNC's
Makefile.modinc (skipped without it, as in CI).
python3 host/tests/test_arm_makedeps.py"""
import glob
import os
import subprocess
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
LCNC = os.path.join(HERE, "..", "arm", "lcnc")
ROBOT = os.path.abspath(os.path.join(HERE, "..", "..", "shared", "robot"))
MODINC = [p for p in (os.path.expanduser("~/linuxcnc-2.9/src/Makefile.modinc"), "/home/user/linuxcnc-2.9/src/Makefile.modinc",
                      "/usr/share/linuxcnc/Makefile.modinc") if os.path.exists(p)]


def includes(src):
    """the robot headers a source includes, directly or through the others"""
    seen, todo = set(), [src]
    while todo:
        f = todo.pop()
        try:
            text = open(f).read()
        except OSError:
            continue
        for line in text.splitlines():
            line = line.strip()
            if line.startswith("#include \""):
                h = line.split('"')[1]
                for d in (os.path.dirname(f), ROBOT, os.path.join(HERE, "..", "arm"), os.path.join(HERE, "..", "lcnc")):
                    p = os.path.abspath(os.path.join(d, h))
                    if os.path.exists(p) and p not in seen:
                        seen.add(p)
                        todo.append(p)
                        break
    return {p for p in seen if p.startswith(ROBOT)}


@unittest.skipUnless(MODINC, "no LinuxCNC Makefile.modinc")
class ArmModuleDeps(unittest.TestCase):
    def test_a_header_change_rebuilds_every_object_that_includes_it(self):
        r = subprocess.run(["make", "-s", "-C", LCNC, "stmbl_arm.so"], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr[-500:])
        srcs = {"armcore.o": "../armcore.c", "plant.o": "../plant.c", "changersim.o": "../changersim.c",
                "gripsim.o": "../gripsim.c", "stmbl_arm_rt.o": "stmbl_arm.c"}
        for h in ("geom.h", "tool.h", "robot.h", "kin.h"):
            # as make names it (the rules and gcc's .d files: relative to host/arm/lcnc)
            rel = os.path.relpath(os.path.join(ROBOT, h), LCNC)
            out = subprocess.run(["make", "-n", "-C", LCNC, "-W", rel, "stmbl_arm.so"],
                                 capture_output=True, text=True).stdout
            for o, src in srcs.items():
                if os.path.join(ROBOT, h) in includes(os.path.join(LCNC, src)):
                    self.assertIn("-o %s" % o if o != "stmbl_arm_rt.o" else "stmbl_arm_rt.o", out,
                                  "%s includes %s but make wouldn't rebuild it" % (o, h))


if __name__ == "__main__":
    unittest.main()
