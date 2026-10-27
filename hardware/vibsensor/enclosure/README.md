# Enclosure concept "Shard"

The vibration sensor's housing (docs/vibsensor.md, board
[hardware/vibsensor](../README.md)): a parametric OpenSCAD model
(`shard.scad`), its STLs for prototype prints, and renders.

![front, rear, top, with the terminator](render/sheet.png)

![exploded: cover, board, base](render/exploded.png)

## The idea

A faceted, low "shard" instead of a round can.
- **Easy to mount flat:**
  - two M4 bolts through the lugs on the X axis, at the nose and the tail;
  - or an M6 stud in the tapped hole under the accelerometer.
- **Orientation:** the pointed nose is the sensor's +X, also marked by
  an arrow on the ridge. Mounted along a machine axis, the readings need no
  mapping.
- **LED:** a light-pipe slit along the ridge, seen from any side.
- **Cables:** two M8 4-pin sockets on the tail's two 45° faces, pointing
  back and out and tilted up 10°, so the cables clear the surface the sensor
  sits on and each other.
  - **IN** (−y side) is male, **OUT** (+y side) female. Sensors chain on
    standard M8 4-pin cables (female to male), IN from the bus's start, OUT
    to the next sensor. Supply and CAN pass straight through.
  - **Termination:** the last sensor's OUT takes a terminator plug, an M8
    male plug with the 120 Ω inside (orange ring in the renders). No jumper:
    the sensor stays closed.

## Build-up

| part | what | how |
|---|---|---|
| base | the floor (5 mm, solid) and the walls, the lugs, the two M8 bosses | milled from aluminium 6082, black anodised; walls drafted outward toward the foot |
| board | 32 × 18 mm, chamfered to the same shape (gen/board.py) | on two M2 bosses of the floor, 2.2 mm above it (room for the bottom-side parts); the IIS3DWB over the M6 hole |
| potting | fills the cavity to 2.6 mm over the board's top | a hard epoxy: it couples the board to the floor, as the bosses do |
| cover | faceted cap, ridge 5.5 mm over the cavity | aluminium (milled) or glass-filled PA12 (SLS), light grey; glued on the potting's rim |
| light pipe | clear / orange insert in the ridge's slit | over the LED (placed under it in the layout) |
| M8 sockets | M8 × 1, 4-pin, rear-mounted in the tail's bosses: IN male, OUT female | their leads to the board's J1 (IN) and J3 (OUT) pads, wired 1:1 |
| terminator | M8 male plug, 120 Ω between pins 2 and 4 | in the last sensor's OUT |

**Size** (from the model):
- 66 mm long including the lugs, 39 mm across the sockets, 17 mm high;
- the body without lugs and sockets: 45 × 24 mm (27 mm at the foot's
  draft).

**Why the floor carries the sensor:** the accelerometer sits right over
the mounting point. From it the path to the machine is the board, two
short bosses and the potting into solid aluminium, so the housing doesn't
resonate in the 6.3 kHz band (docs/vibsensor.md §1).

## Files

| file | what |
|---|---|
| `shard.scad` | the model. `part` = assembly, terminated, exploded, base, cover, pcb. The parameters at the top follow the board. |
| `stl/shard-base.stl`, `stl/shard-cover.stl` | for prototype prints |
| `render/` | front, rear, top, bottom, terminated, exploded; `sheet.png` holds front, rear, top and terminated |
| `Makefile` | `make`: STLs and renders (OpenSCAD; xvfb-run without a display) |

## Open before drawings

- **M8 sockets:** the exact rear-mount sockets (male and female), their
  thread depth and how they are sealed.
- **Terminator:** a stock M8 terminating plug if one with this pinout is
  found, otherwise a field-wireable M8 male plug with a 120 Ω resistor
  inside, potted.
- **Wall and floor:**
  - wall thickness for milling (2.4 mm now);
  - the floor's thickness against the M6 thread depth (4 mm now).
- **Resonance:** check the housing's first resonance, which should sit well
  above 6.3 kHz. FEA on the model, or a tap test on a print.
- **Potting:** compound and depth; how air escapes during potting.
- **IP67:** the cover's seal to the base, if it isn't fully potted
  under it.
- **LED:** placed under the slit in the board's layout.
