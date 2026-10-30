#!/usr/bin/env python3
"""The QtVCP widget in plain PyQt5 (no HAL): it initialises GL through Qt's
context, draws the model, follows set_joints and the mouse. Run under X
(Xvfb will do): tests/qt_smoke.sh. Saves tests/tmp/qt_widget.png.
"""
import os
import sys
import time

here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(here, "qtvcp"))
from PyQt5.QtCore import QPoint, Qt  # noqa: E402
from PyQt5.QtGui import QMouseEvent  # noqa: E402
from PyQt5.QtWidgets import QApplication  # noqa: E402
from vis3d_widget import Vis3dView  # noqa: E402

n = failed = 0


def ok(cond, what):
    global n, failed
    n += 1
    failed += not cond
    print("%s %d - %s" % ("ok" if cond else "not ok", n, what))


app = QApplication(sys.argv)
w = Vis3dView(model=os.path.join(here, "models", "arm6.vis"))
w.resize(800, 450)
w.show()
for _ in range(20):
    app.processEvents()
    time.sleep(0.01)
ok(w.v is not None, "the widget loads the model and initialises GL (%s)" % (getattr(w, "info", "") or w.error))
if w.v:
    w.set_joints([30, 25, -10, 0, -70, 0])
    w.update()
    for _ in range(10):
        app.processEvents()
        time.sleep(0.01)
    img = w.grabFramebuffer()
    os.makedirs(os.path.join(here, "tests", "tmp"), exist_ok=True)
    img.save(os.path.join(here, "tests", "tmp", "qt_widget.png"))
    colours = {img.pixel(x, y) for x in range(0, img.width(), 8) for y in range(0, img.height(), 8)}
    ok(len(colours) > 200, "it draws the scene (%d distinct colours)" % len(colours))
    p, _ = w.v.frame(-1)
    yaw0 = w.v._lib  # keep a reference
    before = w.v.project(p)
    press = QMouseEvent(QMouseEvent.MouseButtonPress, QPoint(400, 200), Qt.LeftButton, Qt.LeftButton, Qt.NoModifier)
    move = QMouseEvent(QMouseEvent.MouseMove, QPoint(480, 200), Qt.LeftButton, Qt.LeftButton, Qt.NoModifier)
    w.mousePressEvent(press)
    w.mouseMoveEvent(move)
    after = w.v.project(p)
    ok(before and after and abs(after[0] - before[0]) > 5, "a left drag orbits the camera")
print("1..%d\n# %d/%d passed" % (n, n - failed, n))
sys.exit(1 if failed else 0)
