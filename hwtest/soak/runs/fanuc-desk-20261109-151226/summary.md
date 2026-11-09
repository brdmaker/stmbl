# fanuc_tw verification soak, 2026-11-09 (the third that day)

conf/fanuc_bis4-4000-2.txt as committed in 94320b7f: encmon0.delay 0.32 ms
(fanuc_tw's lag), encmon0.min_bemf 10 V, + link jog_cmd. Same motion as the
runs before (±25, ±6, ±0.5 rad; 150 rad/s, 2000 rad/s²). Started at 28 °C.

**PASS**: 601 s, 843 moves, no fault, ended disabled.

| | |
|---|---|
| encmon worst residual | **0.18** of the 0.30 trip (0.41 in the morning's run, 0.38 with the delay alone) |
| following error moving / at rest | 0.022 / 0.0026 rad |
| current max | 2.8 A |
| encoder | 3.01 M frames, 0 errors; battery alarm, not-referenced never |
| HV board | 28 -> 67.6 °C |
| pid0.vel_error | max 3.4 rad/s, but one sample of 3350 read `-inf` (121.6 s, after a stop at -6 rad, dc 208 V): a garbled reply or a real one-cycle value? Open (DEBUGNOTES) |
