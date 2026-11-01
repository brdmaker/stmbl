# HSM 400U: the retrofit (simulated) against the original iTNC 530

`hsm_blocks.py` cuts a circle R 20 mm made of straight lines of one length,
as CAM output is, at the owner's 8000 mm/min. It measures the feed LinuxCNC
actually reaches and how far the tool tip strays from the program. Run it
with `make blocks`.

**Caveat:** the retrofit numbers come from the simulation. Its machine data
are guesses (`host/mill/HSM400U.md`), so treat them as a rough estimate. The
iTNC 530 numbers are Heidenhain's published figures; nothing on the original
control was measured.

## Block rate (lines/s)

| | lines/s | shortest line at full 8000 mm/min |
|---|---|---|
| iTNC 530, standard (3.6 ms per block) | ~280 | 0.48 mm |
| iTNC 530, software option 2 (0.5 ms per block) | ~2000 | 0.067 mm |
| LinuxCNC 2.9, 1 kHz servo, no merging (`G64 P0.01 Q0`) | ~980 measured | 0.2 mm |
| LinuxCNC 2.9, 2 kHz servo, no merging | 1000-1500 measured | 0.1 mm |
| LinuxCNC 2.9, default `G64 P0.01` (Q = P) | 6,666 measured (0.02 mm lines; 13,333 with 0.01 mm) | 0.02 mm, the shortest tried |

- **Which iTNC 530 rate applies:** it depends on whether option 2 is
  enabled on this machine. The option list is under MOD on the control. A
  high-speed machine from 2004 most likely has it.
- **Why LinuxCNC's default is so fast:** when Q is not given, LinuxCNC sets
  it equal to P. It then joins consecutive lines that lie within Q of one
  straight line, before planning. Dense, smooth CAM output (as here) becomes
  a few longer moves, so the program's line count does not limit the feed.
  Noisy or finely detailed surfaces cannot be joined; for those, the
  no-merging rows apply.
- **LinuxCNC's own planner limit:** without merging, it runs about one
  segment per servo cycle. 0.1 mm lines at 1 kHz reached only 569 lines/s,
  below the 980 of shorter lines. That run is an outlier and is not
  explained yet (see `DEBUGNOTES.md`).

## Accuracy at 8000 mm/min (circle R 20)

| | path max [um] | servo max [um] |
|---|---|---|
| LinuxCNC + stmbl, 1 kHz, `G64 P0.01` | 14.5 | 19.5 |
| LinuxCNC + stmbl, 2 kHz, `G64 P0.01` | 10.7-11.5 | 13.6 |
| LinuxCNC + stmbl, 1 kHz, `G64 P0.002` (A/C layout, before B/C) | 11.1 | 11.5 |
| LinuxCNC + stmbl, 2 kHz, `G64 P0.002` (A/C layout, before B/C) | 10.9 | 11.4 |
| Five-axis RTCP, line, circle (`results.md`, 2 kHz) | 7.7-8.3 | 7.9-11.2 |
| iTNC 530 + Simodrive 611 D | not published for this machine | — |

- **Path:** the tip's largest distance from the programmed lines. It
  includes the blending tolerance that G64 P allows.
- **Servo:** the tip's largest distance from LinuxCNC's own commanded path.
  This is the drives' and the mechanics' part of the error.
- **What the original's accuracy depends on:** its contour accuracy is set
  by the cycle 32 tolerance (TOLERANCE), the LAG MODE user parameter, and
  how the 611 D loops are tuned. The manual gives only the thermal precision
  of the machine: about ±8 um per axis. The two controls can only be
  compared properly on the machine: the same test part, or a ballbar /
  KGM circle test before and after the retrofit.
- **The mechanics count for both:** once the glass scales close the loop,
  ballscrew heat and pitch error mostly drop out. What remains is how stiff
  the loop is (servo rate, feedforward) and how the axes are tuned.

## Measured (`hsm_blocks.py`, machine `hsm400u` = the motors' encoders, B/C layout)

Circle R 20 mm 30 mm off the C axis at F8000. Without merging (`G64 P0.01 Q0`):

**servo 1 ms (1000 Hz), G64 P0.01 Q0**

| segment [mm] | lines | status | feed [mm/min] | lines/s | path max [um] | servo max [um] | time [s] |
|---|---|---|---|---|---|---|---|
| 1 | 126 | done | 8000 | 133 | 14.5 | 19.7 | 1.00 |
| 0.5 | 251 | done | 8000 | 267 | 14.5 | 19.4 | 1.00 |
| 0.2 | 628 | done | 8000 | 667 | 14.5 | 19.5 | 1.00 |
| 0.1 | 1257 | done | 3415 | 569 | 32.4 | 32.5 | 2.01 |
| 0.067 | 1876 | done | 3938 | 980 | 14.5 | 19.4 | 2.02 |
| 0.05 | 2513 | done | 2941 | 980 | 14.5 | 19.4 | 2.58 |
| 0.02 | 6283 | done | 1176 | 980 | 16.5 | 29.2 | 6.42 |

**servo 0.5 ms (2000 Hz), G64 P0.01 Q0**

| segment [mm] | lines | status | feed [mm/min] | lines/s | path max [um] | servo max [um] | time [s] |
|---|---|---|---|---|---|---|---|
| 1 | 126 | done | 8000 | 133 | 10.7 | 13.7 | 0.99 |
| 0.5 | 251 | done | 8000 | 267 | 11.0 | 13.7 | 1.00 |
| 0.2 | 628 | done | 8000 | 667 | 10.8 | 13.6 | 1.00 |
| 0.1 | 1257 | done | 8000 | 1333 | 10.8 | 13.6 | 1.00 |
| 0.067 | 1876 | done | 4278 | 1064 | 15.7 | 15.6 | 1.75 |
| 0.05 | 2513 | done | 3003 | 1001 | 11.0 | 13.7 | 2.52 |
| 0.02 | 6283 | done | 1523 | 1269 | 43.8 | 43.8 | 4.87 |

Default (`G64 P0.01`, Q = P):

**servo 1 ms (1000 Hz), G64 P0.01**

| segment [mm] | lines | status | feed [mm/min] | lines/s | path max [um] | servo max [um] | time [s] |
|---|---|---|---|---|---|---|---|
| 1 | 126 | done | 8000 | 133 | 14.5 | 19.6 | 1.00 |
| 0.5 | 251 | done | 8000 | 267 | 14.5 | 19.5 | 1.00 |
| 0.2 | 628 | done | 8000 | 667 | 14.5 | 19.5 | 0.99 |
| 0.1 | 1257 | done | 8000 | 1333 | 14.5 | 19.4 | 0.99 |
| 0.067 | 1876 | done | 8000 | 1990 | 14.5 | 19.4 | 0.99 |
| 0.05 | 2513 | done | 8000 | 2667 | 14.5 | 19.4 | 0.99 |
| 0.02 | 6283 | done | 8000 | 6666 | 14.3 | 19.4 | 0.99 |

**servo 0.5 ms (2000 Hz), G64 P0.01**

| segment [mm] | lines | status | feed [mm/min] | lines/s | path max [um] | servo max [um] | time [s] |
|---|---|---|---|---|---|---|---|
| 1 | 126 | done | 8000 | 133 | 10.7 | 13.5 | 0.99 |
| 0.5 | 251 | done | 8000 | 267 | 10.9 | 13.6 | 0.99 |
| 0.2 | 628 | done | 8000 | 667 | 10.9 | 13.6 | 0.99 |
| 0.1 | 1257 | done | 8000 | 1333 | 10.9 | 13.6 | 0.99 |
| 0.067 | 1876 | done | 8000 | 1990 | 10.8 | 13.5 | 0.99 |
| 0.05 | 2513 | done | 8000 | 2667 | 11.4 | 13.5 | 0.99 |
| 0.02 | 6283 | done | 8000 | 6666 | 11.5 | 13.7 | 0.99 |

## Sources (the iTNC 530's block processing time)

- [iTNC 530 User's Manual, NC software 340 490-05](https://content.heidenhain.de/doku/tnc_guide/pdf_files/iTNC530/34049x-05/zyklen/670_388-20.pdf): "Software option 2: block processing time 0.5 ms instead of 3.6 ms"
- [iTNC 530 manuals, NC software 340 422](https://content.heidenhain.de/doku/tnc_guide/pdf_files/iTNC530/340422-xx/lotse/387_952-22.pdf), the older software generation: the same option
- [iTNC 530 ISO programming, M120 look-ahead](https://www.manualsdir.com/manuals/798684/heidenhain-itnc-530-340-422-iso-programming.html?page=205): look-ahead of up to 99 blocks for radius compensation
