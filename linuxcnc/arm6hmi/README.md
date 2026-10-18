# arm6hmi: the ARM-6 operator screen for LinuxCNC (QtVCP)

The operator screen of `docs/hmi/operator.dc.html` as a working QtVCP
screen. The 3D view is `linuxcnc/vis3d`'s `Vis3dView` (C + shaders, light
Python wrapper), so it runs on a Raspberry Pi 4.

![ARM-6 screen in LinuxCNC (arcmate-sim, after the self-test)](arm6hmi.png)

| File | What |
|---|---|
| `arm6.ui` | The layout (Qt Designer). Widgets carry a `role` property the stylesheet matches |
| `arm6.qss` | The look: near-black ground, cyan accent, amber warnings, red stops |
| `arm6_handler.py` | Builds the 3D view, the joint and jog rows and the drive table. Wires the buttons to `qtvcp.core.Action` and refreshes everything at 10 Hz |
| `arm6_postgui.hal` | Nets the joints and armguard into the 3D view's pins |
| `arm6_selftest.py` | Drives the running screen through its own buttons (`ARM6HMI_TEST`) |

## What it shows

| Panel | Source |
|---|---|
| Header | Mode buttons (AUTO / MDI / JOG / TEACH), program name, cycle time, state pill, POWER, E-STOP / RESET |
| Joints | `stat.joint[N]`: position bar within the limits, deg, deg/s, following error (amber above half of `max_ferror`) |
| Drives | `<DRIVE_PINS>.current`, `.power` and `.fault` per joint plus the gripper drive |
| Live model | `Vis3dView` posed from `joint.N.pos-fb`; obstacles from the cell. Guard colours: green OK, amber NEAR, red STOP |
| Pose | `stat.actual_position` X Y Z A B C, TCP speed, frame, tool |
| Program | Open / Run / Step / Pause / Stop, the G-code with the current line, MDI |
| Jog | JOINT / WORLD frame, increments 0.1 / 1 / 10 / 45 or continuous, speed, Record point (TEACH: appends `G1 ...` to `teach.ngc`), Home all |
| Gripper | `armgrip.0.*`: state, gap, the drive's current, payload; Open / Close by MDI (`M65` / `M64 P0`) |
| Power quota | `powerquota.0.total-power`, `.feed-scale`, `.limiting` |
| Safety | armguard state and limit, `armcollide.0.collision`, drive faults |

Panels whose component isn't loaded hide themselves (gripper, quota).

**The guard** follows armguard's own semantics:
- `armguard.0.state` non-zero means stopped (`reason` 1: collision, 2: protective stop).
- Otherwise the screen compares `armguard.0.clearance` (mm) with the cell's `prevent` limit. It shows NEAR below twice the limit, else ARMED.
- The 3D view draws the clearance shell at the limit's radius.

## Use

```ini
[DISPLAY]
DISPLAY = qtvcp arm6            # the screen in <config>/qtvcp/screens/arm6/
[HAL]
POSTGUI_HALFILE = arm6_postgui.hal
[ARM6HMI]
VIS3D = /path/to/stmbl/linuxcnc/vis3d   # built: make -C linuxcnc/vis3d
```

The handler's docstring lists the other `[ARM6HMI]` keys:
- `MODEL`
- `DRIVE_PINS`
- `GRIP_OPEN` / `GRIP_CLOSE`
- `QUOTA_MAX`
- `TEACH_FILE`
- `PREVENT_MM`

The default model `models/arm6_lcnc.vis` says `joints theta`, because LinuxCNC's genserkins joint values include the DH theta0.

On a Pi:
- Qt under X11 uses desktop GL through GLX, and vis3d takes its GLSL 1.20 path.
- `VIS3D_GLES=1` asks Qt for GLES 3.0 instead.
- `VIS3D_SAMPLES=4` turns MSAA on (default off: the Pi resolves the FBO every frame).

## Test

```sh
make -C hwtest/arcmate-sim test-hmi    # as a non-root user, needs Xvfb
```

The test copies the arcmate-sim config with the gripper and power quotas, sets `DISPLAY = qtvcp arm6`, and runs LinuxCNC on Xvfb. The self-test then works the screen through its own buttons:
- E-STOP reset, power on and home all;
- jog J1 +10° in joint mode (the row, the 3D pin and the tool point follow);
- an MDI move in Z (the pose label follows);
- check the guard is armed with the cell's 10 mm;
- record a point;
- take a screenshot, then E-STOP.

It reports 16 checks as TAP (16/16 here, llvmpipe).

Needs PyQt5 (and QScintilla, PyOpenGL) for LinuxCNC's Python. In the cloud image that is Python 3.12; see `DEBUGNOTES.md`.

## Open points

- **TOOL frame jog:** the button is there but disabled; armkins' tool frame jog isn't wired to the screen yet.
- **Not on a Pi yet:** check the frame rate there with `vis3d_render --bench`, and check the screen at the Pi's display size.
- **The pendant** (`docs/hmi/pendant.dc.html`) is not built as a screen.
