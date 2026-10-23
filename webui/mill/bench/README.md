# webui/mill/bench: the console on the bench, X on a real drive

The console's gantry mill (X Y Y Z) with **X on the real stmbl drive** (the
7C81's smart-serial, as `hwtest/burnin`) and Y, Y2, Z simulated (as
`test/sim`). For trying the console against a real servo.

With the Configure page (millweb-supervisor and a machine folder, the
usual way; `machine.json` here makes the same configuration as `mill.ini`,
`test_machineconf.test_the_bench_is_bench`):

```sh
mkdir -p ~/machine-bench && cp machine.json tool.tbl ~/machine-bench/
cp bench.var ~/machine-bench/linuxcnc.var
webui/mill/millweb-supervisor ~/machine-bench   # then http://<host>:8080/
```

Programs go into `~/machine-bench/nc_files/`. Or by hand, without the
Configure page:

```sh
cd webui/mill/bench && linuxcnc mill.ini     # then http://<host>:8080/
```

**The X axis moves.** Homing moves every axis:

| Axis | Homing |
|---|---|
| Z (simulated) | first: up onto a switch at Z 0, then to Z -5 |
| Y, Y2 (simulated) | together (a gantry): onto switches at Y 1000, then to Y 990 |
| X (real) | last: creeps at 3 mm/s to the encoder's index (at most one revolution, 10 mm), which is X 0; repeatable to ~2 counts |

The simulated switches are `sim_home_switch` on the joints' raw positions.
X's travel is -300..300 mm (the burn-in axis), at up to 500 mm/s and 1000
mm/s². Jogs stay inside the workspace (`WORKSPACE_X/Y/Z` in `mill.ini`: X
-100..100, Y 400..600, Z -195..0); programs may use the whole travel. G54
puts a program's X 0..120 at machine X -60..60 (`bench.var`); the
toolsetter is at machine X 250.

| File | |
|---|---|
| `mill.ini` | `test/sim/mill.ini` with X as the real joint, `[HOSTMOT2]` as `hwtest/burnin` (hm2_spix), the drive in the Dynamics page (`DRIVE = X ...current power fault`) |
| `mill.hal` | hostmot2 + the drive on joint 0 (index homing: `joint.0.index-enable` <=> `stbl.0.0.index_enable`), the watchdog E-stop latch; joints 1-3 with home switches, the probe and the tool changer simulated |
| `machine.json` | the same machine for millweb-supervisor: `phasing_ferror` on X, `switch: "sim"` home switches on Y, Y2, Z, the workspace in `console.extra` |
| `phasing_ferror.hal` | a larger following error while the drive autophases (`hwtest/burnin`); a POSTGUI_HALFILE, which millweb loads as AXIS does |
| `bench.var`, `tool.tbl` | `test/sim`'s, G54 X moved to -60 |

Programs come from `../test/sim/nc_files`.
