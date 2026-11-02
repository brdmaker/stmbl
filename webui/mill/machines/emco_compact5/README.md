# Emco Compact 5 CNC

A small bench lathe: X (the cross slide) about 50 mm, Z about 260 mm, a 3000 rpm spindle, and a
6-station turret in front of the spindle's axis. `machine.json` is the machine for millweb, with
simulated joints and a simulated spindle encoder, so it runs without the lathe. For the real one:
- **Board:** set the `board` (a Mesa card).
- **Joints:** set them to the stmbl drives, with their `mech` lead.
- **Spindle:** set it to `hal`, with its pins and encoder.

## Its 3D model

Make the parts from the machine's STEP assembly (the one from Inventor, with every part named):

```
pip install cadquery-ocp
python3 ../../tools/step2parts.py "Emco Compact 5 CNC.stp" step2parts.json models/
```

`step2parts.json` sorts the parts into four files, in the lathe view's frame. The frame has the
spindle's axis along X and the chuck's face at X0. The fasteners and the parts hidden inside the
covers are left out.

| file | what | moves | size |
|---|---|---|---|
| `emco_c5_fixed.stl` | bed, headstock, tailstock, motor, covers, chip tray | (fixed) | 3.8 MB |
| `emco_c5_z.stl` | the saddle with the X motor | `Z` | 0.6 MB |
| `emco_c5_xz.stl` | the cross slide and the turret | `XZ` | 0.7 MB |
| `emco_c5_spindle.stl` | the spindle and the 4-jaw chuck | `S` (turns with the spindle) | 0.6 MB |

Upload the files on the Configure page's 3D model tab and add one part each:
- **Moves:** as in the table.
- **Built-in machine:** off.
- **Tool tip:** about X 136, Y 0, Z 0, where the turret's tool tip is in the files. Move it if the
  drawn tool doesn't sit in the turret's station.

The model's STEP is the user's. Its derived STLs are not in the repository.
