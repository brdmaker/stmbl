# fanuc_tw motion soak, desk drive 205735A45631, 2026-11-05

FANUC βiS 4/4000 + βiA128 (fanuc_tw), conf/fanuc_bis4-4000.txt + link jog_cmd,
201 V bench DC, motor uncoupled. No LinuxCNC: soak_usb.py moves stp0.target over
the USB console (±25, ±6, ±0.5 rad; 150 rad/s, 2000 rad/s²), samples ~4/s.

**PASS**: 601 s, 704 moves, no fault, ended disabled.

| | |
|---|---|
| following error moving / at rest | 0.028 / 0.0027 rad |
| velocity error max | 4.4 rad/s |
| current max | 2.0 A |
| motor thermal model (iit0.temp) | 30 → 33 °C |
| HV board temperature | 29 → 58 °C, levelling off (current derating from 70, fault at 90) |
| encoder frames | 3.01 M good (≈ 5000/s), 0 CRC, 0 sync, 0 no reply |
| rt | 154 µs average, 171 max of 200 (before the run) |

The console carried the HV board's scope stream (DEBUGNOTES 2026-11-05); the
summary's "enc_ok 0" was the parser tripping over it: start 185460, end 3195816.
