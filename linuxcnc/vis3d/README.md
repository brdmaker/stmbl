# vis3d: a native 3D view of the arm for LinuxCNC screens

A C library that draws the six-axis arm with shaders, and a light Python
wrapper that puts it into a QtVCP screen. Built for a Raspberry Pi 4
running the GUI next to LinuxCNC: the geometry is uploaded once, and a
frame is one matrix per link and one draw call per part. Python only
passes joint values in and calls `draw`, a handful of calls a frame,
unlike Vismach, which makes thousands of PyOpenGL calls.

![ARM-6 picking, guard OK](shots/arm6_pick.png)
![a protective stop: the conveyor too close, joint 3 in fault](shots/arm6_stop.png)

## Contents

- [Layout](#layout)
- [The look](#the-look)
- [Model files](#model-files)
- [Using it](#using-it)
- [GL support](#gl-support)
- [Tests](#tests)
- [Open points](#open-points)

## Layout

| File | What |
|---|---|
| `vis3d.h` | the API: load a model, set joints, overlays, camera, `gl_init` / `resize` / `draw` |
| `model.c` | the model file parser, primitive tessellation, STL (binary and ASCII), the DH chain's frames, camera |
| `render.c` | GL: entry points through the host's `getproc`, shader dialect per context, buffers, the frame |
| `shaders.h` | the shaders, written once and mapped to GLSL ES 3.00 / 1.00 and GLSL 3.30 / 1.20 |
| `vmath.h` | 4×4 matrices (column-major), Craig DH |
| `vis3d.py` | ctypes binding (`Vis3d`) |
| `qtvcp/vis3d_widget.py` | `Vis3dView`, a `QOpenGLWidget` for QtVCP: HAL pins in, mouse orbit / pan / zoom |
| `vis3d_render.c` | offscreen renderer (EGL, no X): PNG screenshots and `--bench` frame rates |
| `models/arm6.vis` | the ARM-6 cell: includes arm_sim's `arcmate_base.arm`, adds the look and the obstacles |

The library links no GL library and never opens a window. The host makes
its context current and passes its `getproc`; Qt's
`QOpenGLContext.getProcAddress` is what the widget passes. Without one it
falls back to EGL's or GLX's.

## The look

- **Ground:** a dark studio. A gradient background with a vignette, and
  fog into it.
- **Floor:** a procedural grid with a fine and a coarse step. The arm's
  reach is drawn as a circle in the accent colour, the work area glows
  faintly, and there's a soft contact shadow at the base.
- **Lighting:** a cool key light, a warm fill from behind, and a sky/ground
  ambient. Blinn specular is set by each material's roughness, with a
  fresnel rim light in the accent colour.
- **Materials:** per part (`color`, `metal`, `rough`, `emissive`, `alpha`,
  `accent`). Emissive parts, such as the joint rings, pulse slightly.
- **Overlays:**
  - Obstacles are drawn translucent with edges. The guard colours the
    nearest one: green ok, amber slowing, red stop.
  - A clearance shell around the tool point shows the guard's radius,
    with a fresnel rim and slow scan lines.
  - The tool point's trail is a ribbon, fading with age.
  - Per-link tints mark a joint in fault or a selection.
- **No per-pixel full-screen passes.** No bloom, blur or shadow maps, so
  the Pi's GPU only pays for the pixels the arm covers.

## Model files

Text, `#` comments, angles in degrees, lengths in the file's unit
(`units m`, the default, or `units mm`):

```
dh J a alpha d theta0          Craig DH as genserkins / arm_sim (J from 1)
prismatic J                    joint J moves d instead of theta
pin J NAME                     the HAL pin with joint J's position
limits J MIN MAX
tcp X Y Z                      tool point in the last link's frame
material NAME [color R G B] [metal M] [rough R] [emissive E] [alpha A] [accent]
part L box SX SY SZ [at X Y Z] [rpy A B C] [material M]
part L cylinder R H [axis x|y|z] [at ..] [rpy ..] [material M]
part L ring R T H [axis x|y|z] [at ..] [rpy ..] [material M]
part L sphere R [at ..] [material M]
part L capsule X1 Y1 Z1 X2 Y2 Z2 R [material M]
part L mesh FILE.stl [scale S] [at ..] [rpy ..] [material M]
body L capsule|sphere ...      arm_sim's collision bodies (geom.h), drawn as "body"
obstacle NAME box|capsule|sphere ...   world obstacles, as geom.h (planes skipped)
camera YAW PITCH DIST [target X Y Z]
include FILE
```

`L` is the link a part is fixed to: 0 for the base, J for the link after
joint J. Lines the loader doesn't know are skipped, so an arm_sim model can
be included as is. `arm6.vis` does exactly that: the chain, the joint
limits and the collision capsules are the same ones armkins, the guard and
the simulator use. Real link meshes go in as STL, exported from the
robot's CAD or the STEP files that `tools/step2cell.py` already reads.
Default materials: `body`, `dark`, `metal`, `accent`, `warn`, `obstacle`.

## Using it

**In a QtVCP screen:** add a `Vis3dView`, set `model`, and net its pins:

```
net j0 joint.0.pos-fb => qtvcp.vis3d.joint-0                # ... joint-5
net clear      armguard.clearance      => qtvcp.vis3d.clearance
net clear-stop armguard.clearance-stop => qtvcp.vis3d.clearance-stop
```

From the clearance and its limit the widget shows the guard's state as
armguard reacts (ok, slowing below twice the limit, stop at it), with the
clearance shell at the limit's radius. `guard-nearest` (an obstacle's
index in the model) is optional: armguard has no pin for it yet.

Mouse: left drag orbits, right or middle drag pans, the wheel zooms, and a
double click clears the trail. `python3 qtvcp/vis3d_widget.py` opens it on
its own with the arm waving (PyQt5, no LinuxCNC).

**From Python** (any GL host):

```python
from vis3d import Vis3d
v = Vis3d("models/arm6.vis")
v.gl_init()                       # in the host's GL context
v.resize(1280, 720)
v.set_joints([0, 20, -10, 0, -70, 0])
v.set_guard(2, 0.15, 0)           # slowing, 0.15 m, the conveyor nearest
v.draw(t)
p, R = v.frame(-1)                # the tool point, for a DRO
x, y = v.project(p)               # where to put a Qt label next to it
```

**Measuring a board:**

```
./vis3d_render models/arm6.vis --api gles3 -s 1280x720 --bench 300
```

prints the frame rate without any window system. Run it on the Pi before
building a screen around it.

## GL support

| Context | Shaders | Where |
|---|---|---|
| OpenGL ES 3.x | GLSL ES 3.00 | Raspberry Pi 4 (ES 3.1, V3D), EGL / Wayland |
| OpenGL ES 2.0 | GLSL ES 1.00 | older boards |
| OpenGL 3.3+ (core or compatibility) | GLSL 3.30 | PCs; Qt on X11 got a 4.5 compatibility context here |
| OpenGL 2.1 | GLSL 1.20 | the Pi's desktop GL through Mesa (GLX) |

Core profiles get one vertex array object, created and kept bound. The
shaders use no derivatives and no textures, so ES 2.0 needs no extensions.

## Tests

```
make check       # 28 loader / kinematics checks, 12 drawing checks
make qt-check    # the widget in PyQt5 on Xvfb (PYTHON=python3.13 here)
make shots       # the screenshots above
```

- **The kinematics:** `tests/test_vis3d.c` compares every link frame with
  `shared/robot`'s `rb_fk` over 200 random poses, within 0.2 µm.
- **The loader:** units, every shape, STL (including a binary file whose
  header starts with `solid`), and error messages with file and line.
- **Drawing:** `tests/render_tests.py` draws offscreen in GLES 3, GLES 2,
  GL 3.3 core and GL 2.1. It checks the four images agree, the accent
  shows, a protective stop turns things red, and a move leaves a trail.
- **Where it ran:** on Mesa's software renderer (llvmpipe), about 60–70 fps
  at 720p on this machine's CPU. That says nothing about a Pi's GPU; use
  `--bench` there.

## Open points

- **Not run on a Raspberry Pi yet:** the GLES 3.1 path is the one the Pi
  takes, but it has only been exercised on Mesa's llvmpipe here.
- **Not loaded into QtVCP yet:** the widget follows QtVCP's
  `_HalWidgetBase` pattern (pins in `_hal_init`) and was tested in plain
  PyQt5. It still needs a Designer plugin entry
  (`qtvcp/plugins/`) to show in the widget box.
- **Placeholder geometry:** the parts in `arm6.vis` are primitives around
  arm_sim's guessed capsules. The real arm wants STL meshes.
- **No text in GL:** labels belong to Qt widgets placed with `project()`.
- **The nearest obstacle:** armguard doesn't export which obstacle is
  nearest, so the widget can't colour it yet. A pin for it would be a
  small addition to armguard.
