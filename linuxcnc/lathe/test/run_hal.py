#!/usr/bin/env python3
"""The lathe module in HAL, in a real thread: the turret (an ideal closed-loop stepper, the module's simulated
index) through changes, and the spindle's turns. Run in LinuxCNC's environment (rip-environment), as a user
(not root):  python3 run_hal.py"""
import hal, time, subprocess, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
p = subprocess.Popen(["halrun", "-I", "-f", os.path.join(HERE, "turret.hal")], stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
time.sleep(2)
c = hal.component("turtest"); c.ready()
g = hal.get_value
def change(tool, timeout=8):
    hal.set_p("lathe.turret.tool", str(tool)); hal.set_p("lathe.turret.change", "1")
    t0 = time.time()
    while time.time() - t0 < timeout and not g("lathe.turret.changed") and not g("lathe.turret.fault"): time.sleep(0.01)
    dt = time.time() - t0
    hal.set_p("lathe.turret.change", "0"); time.sleep(0.05)
    return dt
ok = True
for tool in (3, 3, 2, 7):
    dt = change(tool)
    st, pos, homed, fault = g("lathe.turret.station"), g("lathe.turret.pos-cmd"), g("lathe.turret.homed"), g("lathe.turret.fault")
    want = ((tool - 1) % 6) + 1
    frac = (pos + 0.01 - 0.37 - 0.05) % 1.0 * 6
    print("T%d: %.2f s station %d homed %d fault %d pos %.4f (station angle %.3f)" % (tool, dt, st, homed, fault, pos, frac))
    ok &= st == want and not fault and abs(frac - round(frac)) < 0.02 and round(frac) % 6 == want - 1
# the spindle: 10 turns/s for 1 s
hal.set_p("lathe.spindle.speed-rps", "10"); hal.set_p("lathe.spindle.on", "1")
r0 = g("lathe.spindle.revs"); time.sleep(1.0); r1 = g("lathe.spindle.revs")
print("spindle: %.2f turns in 1 s, at-speed %s, speed-fb %.2f" % (r1 - r0, g("lathe.spindle.at-speed"), g("lathe.spindle.speed-fb")))
ok &= 9.0 < r1 - r0 < 11.0 and g("lathe.spindle.at-speed")
print("OK" if ok else "FAILED")
c.exit()
p.stdin.write(b"exit\n"); p.stdin.flush(); p.wait(10)
sys.exit(0 if ok else 1)
