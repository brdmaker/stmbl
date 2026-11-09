# fanuc_tw soak with encmon0.delay = 0.32 ms, 2026-11-09 (the second that day)

As fanuc-desk-20261109-140105, with encmon0.delay 0.00032 (the bench sweep
in that run: fast moves 0.22 -> 0.17) and firmware with fanuc_tw ref_move.

Stopped by the script at 552 s on a garbled reply ("fault0.fault = 0.1": the
drive had no fault, state 1, ended at fault 0): 770 moves, no fault.

| | |
|---|---|
| following error moving / at rest | 0.022 / 0.0025 rad |
| current max | 2.7 A |
| encoder | 2.77 M frames, 0 errors |
| encmon worst | 0.38: at the ±0.5 rad moves (6.8 s, 7.5 s), not the fast ones |
| HV board | 50 °C at the start (warm from the run before) -> **70.4 °C**, the derating point |

encmon's peaks come at the small moves: ~20 rad/s, 0.1-0.2 A, just above
min_bemf (5 V = ~15 rad/s here), where a fixed ~2 V voltage error is a large
fraction of the back-EMF. Next: min_bemf for this motor, then the 30 deg el.
offset test again to see it still trips.
