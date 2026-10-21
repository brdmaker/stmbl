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

## Mill console (`mill.html`)

A concept operator console for a gantry milling machine on stmbl drives and
LinuxCNC (the portal of `host/portal`), in the spirit of QtDragon but its
own design. Published at https://claude.ai/artifact/UuyHMUutm91XVcraCHsXmr;
one HTML file with three.js r128 from cdnjs. Everything is simulated in the
page.

**MAIN**
- **3D view:** the machine itself, modelled after the portal:
  - the frame on its feet, the table with T-slots, the Y rails, magnet tracks and scales;
  - the bridge with its Y motor coils, the X carriage, the Z ram, spindle and tool;
  - the tool setter, and the stock in its fixture at the G54 offset.
- **What it draws:** the program's toolpath (coloured by the predicted path error, by feed or by tool), the trail of the cut, chips, coolant nozzles and the travel limits.
- **Deflection ×N:** shows the head's lag and the bridge's yaw exaggerated.
- **Cameras:** ISO, TOP, FRONT, SIDE, PART and FOLLOW, plus free orbit.
- **Below the view:** the G-code (click a line to run from it), MDI with history, and a jog pad with increments, CONT and the keyboard.

**The control column** stays on every page:
- the DRO in G5x, machine or distance to go, with homed lamps and ZERO / SET (touch off) / REF per axis;
- the modal G-codes;
- spindle (CW / CCW / stop, at speed, load) and coolant (flood, mist, air);
- feed, rapid, spindle and max-velocity overrides;
- cycle start, feed hold (lifting Z 5 mm), single block and stop.

**The other pages**

| Page | What |
|---|---|
| FILE | The programs, and a preflight per program: travel against the soft limits, tools against the table, time, predicted path error p99, the code |
| WIZARDS | Facing, bolt circle and circular pocket: they write G-code and load it |
| OFFSETS | G54–G59.3 and G92, editable; set from the position; touch-plate Z |
| TOOLS | The tool table, editable; load (M6), measure on the setter, add, delete |
| PROBE | Z surface, corner, edges, bore and boss centre, edge angle, tool setter; probe parameters; set the offset from the result |
| DYNAMICS | pcomp's parts, the head's mode along the bridge, the gantry link and yaw, the spindle spectrum, the drives, power and quota, the dynwiz model |
| STATUS | The event log with a filter, machine counters, a HAL pin watch |
| SETTINGS | Units, decimals, deflection exaggeration, keyboard jog, M1, block delete, lift on hold, confirm start, the keys |

The program runs with tool changes, M3 / M8, an M1 optional stop and a
spindle-at-speed interlock. E-STOP (F1) and machine on (F2) gate
everything.

| Panel | Source |
|---|---|
| DRO, following error bars, homed | `joint.N.pos-fb`, `joint.N.f-error`, `stat.homed`; G5x / G92 from `stat` |
| Spindle, load, overrides | `spindle.0.*`, the spindle drive's current; `halui` feed / rapid / spindle / max-velocity override |
| 3D machine | the joints' positions; the head's deflection and the yaw from pcomp's model and `gantry0.yaw` |
| Path colour | pcomp's model (`[PCOMP]`) on the program's preview |
| Command conditioning | `pcomp.0.shaper/defl/shrink/jerk-ms`, `.fx/.fy-*`, `.delay` |
| Gantry link | `gantry0.yaw`, `.active`, `.r_vel` on the Y drives; Y1/Y2 `f-error` |
| Power | `stmbl-*.power`, the bus voltage, `powerquota.0.*` |
| Drives | each drive's current, following error, HV temperature, power, fault |
| Model | `dynwiz.json`; the button runs `dynwiz run` |
| Vibration | an FFT of the spindle drive's current (a HAL sampler, or the drive's scope) |
| Probe, tool setter | `motion.probe-input`, G38.x cycles as in QtDragon's probe screens |

Built for real it would be a QtVCP screen like `linuxcnc/arm6hmi`, with the
machine drawn by `linuxcnc/vis3d`.
