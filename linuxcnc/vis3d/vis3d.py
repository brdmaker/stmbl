"""vis3d for Python: a thin ctypes binding of libvis3d.so (vis3d.h).

The library does the work (the model, the kinematics, all GL); Python only
passes joint values in and calls draw, a handful of calls per frame. Used
by the QtVCP widget (qtvcp/vis3d_widget.py); any other GL host works too:
make its context current, then gl_init / resize / draw.

    v = Vis3d("models/arm6.vis")
    v.gl_init()                 # in the host's GL context
    v.resize(w, h)
    v.set_joints([0, 10, 20, 0, -90, 0])
    v.draw(t)
"""

import ctypes
import os

_HERE = os.path.dirname(os.path.abspath(__file__))
GETPROC = ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_char_p)

GUARD_OFF, GUARD_OK, GUARD_SLOW, GUARD_STOP = range(4)


def _lib(path=None):
    for p in ([path] if path else []) + [os.path.join(_HERE, "libvis3d.so"), "libvis3d.so"]:
        try:
            lib = ctypes.CDLL(p)
            break
        except OSError:
            continue
    else:
        raise OSError("libvis3d.so not found: make -C %s" % _HERE)
    P, D, I, F = ctypes.c_void_p, ctypes.c_double, ctypes.c_int, ctypes.c_float
    DP = ctypes.POINTER(ctypes.c_double)
    sig = {
        "vis3d_load": (P, [ctypes.c_char_p, ctypes.c_char_p, I]),
        "vis3d_free": (None, [P]),
        "vis3d_joints": (I, [P]),
        "vis3d_joint_pin": (ctypes.c_char_p, [P, I]),
        "vis3d_joint_limits": (None, [P, I, DP, DP]),
        "vis3d_obstacles": (I, [P]),
        "vis3d_obstacle_name": (ctypes.c_char_p, [P, I]),
        "vis3d_set_joints": (None, [P, DP, I]),
        "vis3d_frame": (None, [P, I, DP, DP]),
        "vis3d_set_guard": (None, [P, I, D, I]),
        "vis3d_set_tint": (None, [P, I, F, F, F, F]),
        "vis3d_trail": (None, [P, I, I]),
        "vis3d_set_accent": (None, [P, F, F, F]),
        "vis3d_show": (None, [P, I, I]),
        "vis3d_camera": (None, [P, D, D, D, DP]),
        "vis3d_orbit": (None, [P, D, D]),
        "vis3d_pan": (None, [P, D, D]),
        "vis3d_zoom": (None, [P, D]),
        "vis3d_project": (I, [P, DP, DP, DP]),
        "vis3d_gl_init": (I, [P, GETPROC, ctypes.c_char_p, I]),
        "vis3d_resize": (None, [P, I, I]),
        "vis3d_draw": (None, [P, D]),
        "vis3d_gl_release": (None, [P]),
        "vis3d_gl_info": (ctypes.c_char_p, [P]),
        "vis3d_stats": (None, [P, ctypes.POINTER(I), ctypes.POINTER(I)]),
    }
    for name, (res, args) in sig.items():
        f = getattr(lib, name)
        f.restype, f.argtypes = res, args
    return lib


class Vis3d:
    def __init__(self, model, lib=None):
        self._lib = _lib(lib)
        err = ctypes.create_string_buffer(512)
        self._v = self._lib.vis3d_load(model.encode(), err, len(err))
        if not self._v:
            raise ValueError(err.value.decode())
        self.n = self._lib.vis3d_joints(self._v)
        self._q = (ctypes.c_double * self.n)()
        self._getproc = None  # keeps the ctypes callback alive

    # -- the model
    def pins(self):
        return [self._lib.vis3d_joint_pin(self._v, j).decode() for j in range(self.n)]

    def limits(self, j):
        a, b = ctypes.c_double(), ctypes.c_double()
        self._lib.vis3d_joint_limits(self._v, j, ctypes.byref(a), ctypes.byref(b))
        return a.value, b.value

    def obstacles(self):
        return [self._lib.vis3d_obstacle_name(self._v, i).decode() for i in range(self._lib.vis3d_obstacles(self._v))]

    def set_joints(self, q):
        for j in range(min(self.n, len(q))):
            self._q[j] = q[j]
        self._lib.vis3d_set_joints(self._v, self._q, self.n)

    def frame(self, k=-1):
        """position, rotation (3x3 rows) of link k (0 base .. n), -1 the tool point"""
        p, R = (ctypes.c_double * 3)(), (ctypes.c_double * 9)()
        self._lib.vis3d_frame(self._v, k, p, R)
        return list(p), [list(R[0:3]), list(R[3:6]), list(R[6:9])]

    # -- overlays
    def set_guard(self, state, radius, nearest=-1):
        self._lib.vis3d_set_guard(self._v, state, radius, nearest)

    def set_tint(self, link, r, g, b, a):
        self._lib.vis3d_set_tint(self._v, link, r, g, b, a)

    def trail(self, on=True, clear=False):
        self._lib.vis3d_trail(self._v, int(on), int(clear))

    def set_accent(self, r, g, b):
        self._lib.vis3d_set_accent(self._v, r, g, b)

    def show(self, obstacles=True, grid=True):
        self._lib.vis3d_show(self._v, int(obstacles), int(grid))

    # -- camera
    def camera(self, yaw, pitch, dist, target=None):
        t = (ctypes.c_double * 3)(*target) if target else None
        self._lib.vis3d_camera(self._v, yaw, pitch, dist, t)

    def orbit(self, dx, dy):
        self._lib.vis3d_orbit(self._v, dx, dy)

    def pan(self, dx, dy):
        self._lib.vis3d_pan(self._v, dx, dy)

    def zoom(self, f):
        self._lib.vis3d_zoom(self._v, f)

    def project(self, p):
        """a point (model units) to window pixels, or None behind the camera"""
        x, y = ctypes.c_double(), ctypes.c_double()
        q = (ctypes.c_double * 3)(*p)
        return (x.value, y.value) if self._lib.vis3d_project(self._v, q, ctypes.byref(x), ctypes.byref(y)) else None

    # -- GL (the host's context current)
    def gl_init(self, getproc=None):
        """getproc: name (bytes) -> address (int), e.g. Qt's; None: EGL / GLX"""
        if getproc:
            self._getproc = GETPROC(lambda name: getproc(name) or 0)
        else:
            self._getproc = ctypes.cast(None, GETPROC)
        err = ctypes.create_string_buffer(1024)
        if self._lib.vis3d_gl_init(self._v, self._getproc, err, len(err)) < 0:
            raise RuntimeError(err.value.decode())
        return self._lib.vis3d_gl_info(self._v).decode()

    def resize(self, w, h):
        self._lib.vis3d_resize(self._v, int(w), int(h))

    def draw(self, t=0.0):
        self._lib.vis3d_draw(self._v, t)

    def gl_release(self):
        self._lib.vis3d_gl_release(self._v)

    def stats(self):
        t, d = ctypes.c_int(), ctypes.c_int()
        self._lib.vis3d_stats(self._v, ctypes.byref(t), ctypes.byref(d))
        return t.value, d.value

    def close(self):
        if self._v:
            self._lib.vis3d_free(self._v)
            self._v = None
