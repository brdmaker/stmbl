"""Vis3dView: the arm in 3D, a QtVCP widget (a QOpenGLWidget around libvis3d).

In a QtVCP screen: put a Vis3dView in Designer (or create it in the handler),
set its `model` property to a .vis file, and net its HAL pins:

    net j0 joint.0.pos-fb => qtvcp.vis3d.joint-0      (one per joint)
    net guard-state armguard.0.state     => qtvcp.vis3d.guard-state
    net clearance   armguard.0.clearance => qtvcp.vis3d.clearance

and set guard_limit_mm (the cell's `prevent MM`) from the handler. The
widget then shows the guard as armguard acts: stop while armguard holds the
arm (state != 0), near below twice the limit, armed otherwise; the shell
around the tool point has the limit's radius. guard-nearest (an obstacle's
index in the model, -1 none) is optional.

The `cell` property adds armguard's cell file: its obstacles, and with a
tool changer the tool table it includes (the tools drawn in their docks).
The `tool` pin (armtool.0.mounted) says which tool is on the flange.
Outside QtVCP (plain PyQt5, no HAL) it runs too: set_joints() and
set_tool() feed it.

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


def guard_state(armguard_state, clearance_mm, limit_mm):
    """vis3d's guard state (0 off, 1 ok, 2 near, 3 stop) from armguard's
    state (0: passing the commands through, else stopping / backing off /
    holding), its clearance and the cell's prevent limit [mm]"""
    if armguard_state:
        return 3
    if limit_mm <= 0:
        return 0
    return 2 if clearance_mm < 2 * limit_mm else 1


class Vis3dView(QOpenGLWidget, _HalWidgetBase):
    def __init__(self, parent=None, model=None, fps=30):
        super(Vis3dView, self).__init__(parent)
        self._model_path = model or ""
        self._cell_path = ""
        self._tool = 0
        self._fps = fps
        self.v = None
        self.error = ""
        self._joints = []
        self._pins = {}
        self._drag = None
        self._t0 = time.monotonic()
        self.guard_limit_mm = 0.0  # the cell's prevent limit; 0: no guard shown
        self._unit_m = 1.0         # the model's length unit in metres
        # MSAA off by default: on the Pi 4's tile-based GPU QOpenGLWidget
        # renders into a multisample FBO and resolves it every frame, not
        # free (VIS3D_SAMPLES=4 to try it). VIS3D_GLES=1 asks Qt for OpenGL
        # ES (the Pi's conformant ES 3.1) instead of desktop GL 2.1 / 3.1.
        fmt = QSurfaceFormat()
        fmt.setDepthBufferSize(24)
        fmt.setSamples(int(os.environ.get("VIS3D_SAMPLES", "0")))
        if os.environ.get("VIS3D_GLES") == "1":
            fmt.setRenderableType(QSurfaceFormat.OpenGLES)
            fmt.setVersion(3, 0)
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

    def _get_cell(self):
        return self._cell_path

    def _set_cell(self, path):  # armguard's cell file: obstacles, tool table
        self._cell_path = path
        if self.v:
            self._set_model(self._model_path)

    cell = pyqtProperty(str, _get_cell, _set_cell)

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
        self._pins["guard-state"] = self.HAL_GCOMP_.newpin(name + ".guard-state", hal.HAL_S32, hal.HAL_IN)
        self._pins["clearance"] = self.HAL_GCOMP_.newpin(name + ".clearance", hal.HAL_FLOAT, hal.HAL_IN)
        self._pins["guard-nearest"] = self.HAL_GCOMP_.newpin(name + ".guard-nearest", hal.HAL_S32, hal.HAL_IN)
        self._pins["trail-clear"] = self.HAL_GCOMP_.newpin(name + ".trail-clear", hal.HAL_BIT, hal.HAL_IN)
        self._pins["tool"] = self.HAL_GCOMP_.newpin(name + ".tool", hal.HAL_S32, hal.HAL_IN)

    # -- without HAL
    def set_joints(self, q):
        self._joints = list(q)

    def set_tool(self, tool_id):
        self._tool = tool_id

    # -- GL
    def _load(self):
        if not self._model_path:
            self.error = "no model"
            return
        try:
            self.v = Vis3d(self._model_path)
            if self._cell_path:
                self.v.load_cell(self._cell_path)
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
            lim = self.guard_limit_mm
            st = guard_state(self._pins["guard-state"].get(), self._pins["clearance"].get(), lim)
            self.v.set_guard(st, lim / 1000.0 / self._unit_m, self._pins["guard-nearest"].get())
            if self._pins["trail-clear"].get():
                self.v.trail(True, True)
            tool = self._pins["tool"].get()
        else:
            q, tool = self._joints, self._tool
        self.v.set_tool(tool)
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
