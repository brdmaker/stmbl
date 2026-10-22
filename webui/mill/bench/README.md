# webui/mill/bench: the console on the bench, X on a real drive

The console's gantry mill (X Y Y Z) with **X on the real stmbl drive** (the
7C81's smart-serial, as `hwtest/burnin`) and Y, Y2, Z simulated (as
`test/sim`). For trying the console against a real servo.

```sh
cd webui/mill/bench && linuxcnc mill.ini     # then http://<host>:8080/
```

**The X axis moves.** It homes where it stands; its travel is -300..300 mm
from there (the burn-in axis), at up to 500 mm/s and 1000 mm/s². G54 puts a
program's X 0..120 at machine X -60..60 (`bench.var`); the toolsetter is at
machine X 250.

| File | |
|---|---|
| `mill.ini` | `test/sim/mill.ini` with X as the real joint, `[HOSTMOT2]` as `hwtest/burnin` (hm2_spix), the drive in the Dynamics page (`DRIVE = X ...current power fault`) |
| `mill.hal` | hostmot2 + the drive on joint 0, the watchdog E-stop latch; joints 1-3, the probe and the tool changer simulated |
| `phasing_ferror.hal` | a larger following error while the drive autophases (`hwtest/burnin`) |
| `bench.var`, `tool.tbl` | `test/sim`'s, G54 X moved to -60 |

Programs come from `../test/sim/nc_files`.
