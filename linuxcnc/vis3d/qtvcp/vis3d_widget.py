"""Vis3dView: the arm in 3D, a QtVCP widget (a QOpenGLWidget around libvis3d).

In a QtVCP screen: put a Vis3dView in Designer (or create it in the handler),
set its `model` property to a .vis file, and net its HAL pins:

    net j0 joint.0.pos-fb => qtvcp.vis3d.joint-0      (one per joint)
    net clear      armguard.clearance      => qtvcp.vis3d.clearance
    net clear-stop armguard.clearance-stop => qtvcp.vis3d.clearance-stop

From the clearance and its limit the widget shows the guard's state as
armguard reacts: ok, slowing below twice the limit, stop at it; the shell
around the tool point has the limit's radius. guard-nearest (an obstacle's
index in the model, -1 none) is optional. Outside QtVCP (plain PyQt5, no
HAL) it runs too: set_joints() feeds it.

Mouse: left drag orbits, right / middle drag pans, wheel zooms; double click
clears the trail.
"""

import os
import time

from PyQt5.QtCore import QTimer, Qt, pyqtProperty
from PyQt5.QtGui import QSurfaceFormat
from PyQt5.QtWidgets import QOpenGLWidget

import sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from vis3d import Vis3d  # noqa: E402

try:  # inside QtVCP: HAL pins
    import hal
    from qtvcp.widgets.widget_baseclass import _HalWidgetBase
except ImportError:  # plain PyQt5: no HAL
    hal = None

    class _HalWidgetBase(object):
        pass


def guard_state(clearance, limit):
    """the guard's state from the clearance and its stop limit, as armguard
    reacts: off (no limit), ok, slowing below 2x the limit, stop at it"""
    if limit <= 0:
        return 0
    return 3 if clearance <= limit else 2 if clearance < 2 * limit else 1


class Vis3dView(QOpenGLWidget, _HalWidgetBase):
    def __init__(self, parent=None, model=None, fps=30):
        super(Vis3dView, self).__init__(parent)
        self._model_path = model or ""
        self._fps = fps
        self.v = None
        self.error = ""
        self._joints = []
        self._pins = {}
        self._drag = None
        self._t0 = time.monotonic()
        fmt = QSurfaceFormat()
        fmt.setDepthBufferSize(24)
        fmt.setSamples(4)  # MSAA where the board has it (the Pi 4 does)
        self.setFormat(fmt)
        self.setMinimumSize(200, 150)
        self._timer = QTimer(self)
        self._timer.timeout.connect(self.update)
        self._timer.start(int(1000 / max(1, fps)))

    # -- Designer properties
    def _get_model(self):
        return self._model_path

    def _set_model(self, path):
        self._model_path = path
        if self.v:  # reload in the GL context
            self.makeCurrent()
            self.v.close()
            self.v = None
            self._load()
            self.doneCurrent()

    model = pyqtProperty(str, _get_model, _set_model)

    def _get_fps(self):
        return self._fps

    def _set_fps(self, fps):
        self._fps = fps
        self._timer.setInterval(int(1000 / max(1, fps)))

    fps = pyqtProperty(int, _get_fps, _set_fps)

    # -- QtVCP: pins
    def _hal_init(self):
        name = self.HAL_NAME_
        n = 9  # VIS3D_MAXJ: the model may be loaded later (GL init)
        for j in range(n):
            self._pins["joint-%d" % j] = self.HAL_GCOMP_.newpin("%s.joint-%d" % (name, j), hal.HAL_FLOAT, hal.HAL_IN)
        self._pins["clearance"] = self.HAL_GCOMP_.newpin(name + ".clearance", hal.HAL_FLOAT, hal.HAL_IN)
        self._pins["clearance-stop"] = self.HAL_GCOMP_.newpin(name + ".clearance-stop", hal.HAL_FLOAT, hal.HAL_IN)
        self._pins["guard-nearest"] = self.HAL_GCOMP_.newpin(name + ".guard-nearest", hal.HAL_S32, hal.HAL_IN)
        self._pins["trail-clear"] = self.HAL_GCOMP_.newpin(name + ".trail-clear", hal.HAL_BIT, hal.HAL_IN)

    # -- without HAL
    def set_joints(self, q):
        self._joints = list(q)

    # -- GL
    def _load(self):
        if not self._model_path:
            self.error = "no model"
            return
        try:
            self.v = Vis3d(self._model_path)
            ctx = self.context()

            def getproc(name):
                p = ctx.getProcAddress(name)
                return int(p) if p else 0

            self.info = self.v.gl_init(getproc)
            self.v.resize(self.width() * self.devicePixelRatioF(), self.height() * self.devicePixelRatioF())
            self.error = ""
        except (OSError, ValueError, RuntimeError) as e:
            self.error = str(e)
            self.v = None

    def initializeGL(self):
        self._load()

    def resizeGL(self, w, h):
        if self.v:
            r = self.devicePixelRatioF()
            self.v.resize(w * r, h * r)

    def paintGL(self):
        if not self.v:
            return
        if self._pins:
            q = [self._pins["joint-%d" % j].get() for j in range(self.v.n)]
            c, lim = self._pins["clearance"].get(), self._pins["clearance-stop"].get()
            self.v.set_guard(guard_state(c, lim), lim, self._pins["guard-nearest"].get())
            if self._pins["trail-clear"].get():
                self.v.trail(True, True)
        else:
            q = self._joints
        if q:
            self.v.set_joints(q)
        self.v.draw(time.monotonic() - self._t0)

    # -- mouse
    def mousePressEvent(self, e):
        self._drag = (e.x(), e.y(), e.button())

    def mouseMoveEvent(self, e):
        if self._drag and self.v:
            x0, y0, b = self._drag
            if b == Qt.LeftButton:
                self.v.orbit(e.x() - x0, e.y() - y0)
            else:
                self.v.pan(e.x() - x0, e.y() - y0)
            self._drag = (e.x(), e.y(), b)
            self.update()

    def mouseReleaseEvent(self, e):
        self._drag = None

    def wheelEvent(self, e):
        if self.v:
            self.v.zoom(0.9 if e.angleDelta().y() > 0 else 1.0 / 0.9)
            self.update()

    def mouseDoubleClickEvent(self, e):
        if self.v:
            self.v.trail(True, True)


if __name__ == "__main__":  # a quick look without LinuxCNC: the arm waves
    import math
    from PyQt5.QtWidgets import QApplication

    app = QApplication(sys.argv)
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    w = Vis3dView(model=sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "models", "arm6.vis"), fps=60)
    w.resize(1280, 720)
    w.setWindowTitle("vis3d")
    t0 = time.monotonic()

    def wave():
        t = time.monotonic() - t0
        w.set_joints([40 * math.sin(t * 0.5), 20 + 15 * math.sin(t * 0.7), 10 * math.sin(t * 0.9), 30 * math.sin(t),
                      -70 + 20 * math.sin(t * 1.3), 60 * math.sin(t * 0.8)])

    timer = QTimer()
    timer.timeout.connect(wave)
    timer.start(16)
    w.show()
    sys.exit(app.exec_())
