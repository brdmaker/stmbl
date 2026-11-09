# fanuc_tw motion soak, desk drive 205735A45631, 2026-11-09

FANUC βiS 4/4000 #2 + βiA128 (fanuc_tw), firmware master c42486a2 built and
flashed from ~/brdmot/stmbl (make update), conf/fanuc_bis4-4000-2.txt (with
encmon) + link jog_cmd, 201 V bench DC, motor uncoupled. No LinuxCNC:
soak_usb.py moves stp0.target over the USB console (±25, ±6, ±0.5 rad;
150 rad/s, 2000 rad/s²), samples ~5/s; the scope stream is dropped now.

**PASS**: 601 s, 838 moves, no fault, ended disabled.

| | |
|---|---|
| following error moving / at rest | 0.022 / 0.0022 rad |
| velocity error max | 3.5 rad/s |
| current max | 2.9 A |
| motor thermal model (iit0.temp) | 30 → 33 °C |
| HV board temperature | → 66 °C (current derating from 70, fault at 90) |
| encoder | 3.01 M good frames, 0 CRC, 0 sync, 0 no reply; battery alarm and not-referenced never seen |
| encmon | worst residual 0.41 (trips above 0.30 held for 20 ms): first over 0.30 ~9 s in, in the first fast moves; no trip |
| rt | 149 → 150 µs average, max 163 of 200 |

Open: encmon's margin under fast moves (0.41 here, 0.15 at 100 rad/s in the
encmon test) and the HV board's temperature on the bare desk drive (66 °C,
58 °C last week).
