# ARM-6 HMI previews

Design previews for a LinuxCNC operator interface for the six-axis arm
(`host/arm`): the cell as the robot branch builds it, six stmbl joints and
a gripper on CAN, the collision guard, power quotas, the stop modes and
holding brakes. Concepts only: the numbers are placeholders in the right
ranges, nothing is wired to HAL yet.

| File | What | Open it |
|---|---|---|
| `arm3d.html` | Real-time WebGL view: the arm as a kinematic chain posed from joint angles, a pick-and-place cycle run in 1 ms servo steps and rendered at the display rate, joint sliders, TCP readout and trail, collider boxes and a clearance guard that slows from 2× the limit and stops at it | In a browser, from disk (three.js r128 from cdnjs) |
| `operator.dc.html` | Operator screen, 1440 wide and down to a phone: mode bar, E-STOP, joint DROs with following error, the seven drives (bus, current, power, HV temperature, state), a live side view, Cartesian pose, program, jog in JOINT / WORLD / TOOL, gripper, power quota, safety panel, event strip | A Design-canvas artboard: https://claude.ai/artifact/9RudHWYvcZ9PHFBBEccGvs (needs its runtime; the file is the source) |
| `pendant.dc.html` | Teach pendant, 390×844: big jog keys, frame and increment, pose strip, record point / grip / home, a deadman that gates every move | The same canvas |

The operator screen is built as a QtVCP screen: `linuxcnc/arm6hmi`, with
`linuxcnc/vis3d` as its 3D view (`make -C hwtest/arcmate-sim hmi`).

The live 3D demo is also published at https://claude.ai/artifact/Fc7dxVy4o2ACBKtQ646cDH.

## Design

Dark industrial look for a shop floor: near-black ground, one cyan accent
for the machine's own state and controls, amber for warnings and the
gripper, red only for E-STOP and protective stops. Numbers in a monospace
face so columns line up; the screen is scanned, not read. The 3D view uses
Chakra Petch and IBM Plex; the operator screen Space Grotesk and JetBrains
Mono (both from Google Fonts).

## Mapping to LinuxCNC

Every panel maps to pins that exist on this branch:

| Panel | Source |
|---|---|
| Joint DROs, following error | `joint.N.pos-fb`, `joint.N.f-error` |
| Drives | the stmblcan process data: bus voltage, current, power (`pwr`), HV temperature, fault / state |
| Cartesian pose, TCP speed | `armkins` (world and tool frames), the tool table's TCP |
| Collision guard, clearance, protective stop | the `armguard` component: clearance, the predictive stop, the backoff |
| Power quota | `powerquota`: quota, used power, feed scale, fuse profile |
| Safety | `fault0.*_stop` (quick stop / dynamic brake), `brake_on_loss`, `fault0.brake_time` |
| Gripper | the 7th drive: width, force, part detection, payload |

Rendering the arm in real time (the native option is built:
`linuxcnc/vis3d`, a C library with shaders and a QtVCP widget, for a
Raspberry Pi 4 GUI):

1. **Web HMI** (as `arm3d.html`): a small Python bridge reads the joint
   pins with the `hal` module and pushes them over a WebSocket at
   100–250 Hz; the browser does forward kinematics and draws at 60 fps.
   Nothing touches the RT side. Easiest to style; runs on a tablet pendant.
2. **QtVCP**: a custom OpenGL / VTK widget drawing from the same pins,
   inside a QtVCP screen with the program, overrides and MDI.
3. **Vismach**: LinuxCNC's own simple 3D preview from link lengths; quick,
   but a fixed look.

Link meshes would come from the robot's STEP files, the way
`tools/step2cell.py` already derives the collider boxes.
