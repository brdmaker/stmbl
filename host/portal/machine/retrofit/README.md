# Retrofit machines

Imaginary commercial machines, each retrofitted with stmbl drives and
LinuxCNC. They are portal files for `host/portal/portal_sim`, the LinuxCNC
module `stmbl_portal` and `linuxcnc/dynwiz -b sim`. Motors are Baldor's
catalog figures (`docs/baldor_linear.md`). Masses, modes, friction and
frames are made up, of a plausible size. Each file's `#@dynwiz` line holds
the acceleration and speed limits its LinuxCNC INI would have.

| file | machine | travel X × Y | motors | what makes it hard |
|---|---|---|---|---|
| `laser3015.portal` | 3 × 1.5 m fiber laser flatbed | 1500 × 3000 mm | Y1/Y2 LMBL18B, X LMBL14B (iron core) | 146 kg gantry, a 1.9 m bridge (head y 19 Hz mid-span), cogging 18–28 N every 22.86 mm |
| `dispenser.portal` | benchtop fluid dispenser / light pick-and-place | 600 × 400 mm | Y LMCF06E on one side, idler rail; X LMCF04C | one Y motor (the bridge twists), a light frame on rubber feet, a light, stiff head (85–140 Hz) |
| `cmm.portal` | bridge CMM on a granite table, air bearings | 800 × 1000 mm | Y1/Y2 LMCF08F, X LMCF08E | almost no friction, a 10 kg probe ram on 17–26 Hz with 1.2 % damping, heavier than its carriage |
| `pcbdrill.portal` | PCB driller, cast-iron base | 700 × 600 mm | Y1/Y2 LMCF12F, X LMCF12E | 2 g point-to-point, a heavy spindle, the frame on its feet at 17–20 Hz |

## What their drive configs needed

All start from `../y.conf`'s settings (`vel_int0.mode 1`, `vel1.bc 1`,
pos_bw 100, vel_bw 500, `conf0.j` the whole moving mass).

| machine | change | why |
|---|---|---|
| laser3015 Y | `conf0.cur_bw = 2000` | Y1 and Y2 fought at 200 Hz, ±4 A (bridge yaw, below) |
| pcbdrill Y | `conf0.cur_bw = 2000` | the same at 234 Hz, after reversing at slow speed |
| cmm Y | `conf0.vel_bw = 300` | the same at 208 Hz: on air bearings nothing damps it |
| dispenser Y | `conf0.vel_bw = 200` | one motor against the bridge's twist on its guide blocks, 225 Hz |
| laser3015, pcbdrill, dispenser | a brake chopper (`bus ... 360 R`) | at their accelerations, braking pumped the bus over the drives' 380 V (HV_VOLT_ERROR); the chopper must switch on below `conf0.max_dc_volt` |

**The bridge's yaw.** Each Y drive is tuned for its share of the moving
mass. In the yaw, where Y1 moves against Y2, it moves only `2 J / L²` of
the bridge: 18 of 73 kg on the laser, 4.7 of 21.5 kg on the driller. Its
velocity loop is then four or five times faster than tuned, which meets
the default current loop (1000 rad/s). Guide friction damps it on some
machines and not on others. A faster current loop or a slower velocity
loop fixes it.

`dynwiz` finds every one of these:
- `mass` and `cogging` flag the oscillation;
- `write` proposes a lower `vel_bw`;
- a fault is reported by name.

`tests/portal_retrofit_fight.test` and dynwiz's `test_untuned_detected`
put the old settings back to show it.

## Running them

```sh
cd host/portal
./portal_sim machine/retrofit/cmm.portal script.txt
../../linuxcnc/dynwiz/dynwiz -b sim --portal machine/retrofit/laser3015.portal -y run
```

Under LinuxCNC, `hwtest/portal-sim`'s config takes the file in `[PORTAL]
FILE`. Set the axis limits to its travel and its `#@dynwiz` limits.
