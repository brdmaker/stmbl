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
| LinuxCNC 2.9, default `G64 P0.01` (Q = P) | 13,333 measured (0.01 mm lines) | 0.01 mm, the shortest tried |

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
  segment per servo cycle. 0.1 mm lines at 1 kHz reached only 564 lines/s,
  below the 980 of shorter lines. That run is an outlier and is not
  explained yet (see `DEBUGNOTES.md`).

## Accuracy at 8000 mm/min (circle R 20)

| | path max [um] | servo max [um] |
|---|---|---|
| LinuxCNC + stmbl, 1 kHz, `G64 P0.01` | 13.4 | 18.6 |
| LinuxCNC + stmbl, 1 kHz, `G64 P0.002` | 11.1 | 11.5 |
| LinuxCNC + stmbl, 2 kHz, `G64 P0.01` | 9.3-11.5 | 13.0 |
| LinuxCNC + stmbl, 2 kHz, `G64 P0.002` | 10.9 | 11.4 |
| Five-axis RTCP, line, circle (`results.md`, 2 kHz) | 5.5-6.6 | 5.5-10.5 |
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

## Measured (`hsm_blocks.py`, machine `hsm400u` = the motors' encoders)

1 kHz, `G64 P0.01 Q0` (no merging):

| segment [mm] | lines | feed [mm/min] | lines/s | path max [um] | servo max [um] |
|---|---|---|---|---|---|
| 1 | 126 | 8000 | 133 | 13.3 | 18.5 |
| 0.5 | 251 | 8000 | 267 | 13.2 | 18.4 |
| 0.2 | 628 | 8000 | 667 | 13.2 | 18.5 |
| 0.1 | 1257 | 3383 | 564 | 21.3 | 21.4 |
| 0.05 | 2513 | 2941 | 980 | 13.2 | 18.5 |
| 0.02 | 6283 | 1176 | 980 | 15.1 | 29.8 |
| 0.01 | 12566 | 588 | 979 | 11.0 | 18.4 |

2 kHz, `G64 P0.01 Q0`:

| segment [mm] | lines | feed [mm/min] | lines/s | path max [um] | servo max [um] |
|---|---|---|---|---|---|
| 1 | 126 | 8000 | 133 | 9.5 | 13.1 |
| 0.5 | 251 | 8000 | 267 | 9.3 | 13.0 |
| 0.2 | 628 | 8000 | 667 | 9.4 | 12.9 |
| 0.1 | 1257 | 7999 | 1333 | 9.4 | 13.0 |
| 0.05 | 2513 | 3000 | 1000 | 9.4 | 12.9 |
| 0.02 | 6283 | 1522 | 1268 | 35.3 | 35.4 |
| 0.01 | 12566 | 920 | 1533 | 25.8 | 25.8 |

Default `G64 P0.01` (Q = P), 1 kHz and 2 kHz: 8000 mm/min for every segment
length from 1 mm down to 0.01 mm (133-13,333 lines/s). Path max is
13.4-13.6 um at 1 kHz and 9.3-11.5 um at 2 kHz; servo max is 18.6 um at
1 kHz and 13 um at 2 kHz.

## Sources (the iTNC 530's block processing time)

- [iTNC 530 User's Manual, NC software 340 490-05](https://content.heidenhain.de/doku/tnc_guide/pdf_files/iTNC530/34049x-05/zyklen/670_388-20.pdf): "Software option 2: block processing time 0.5 ms instead of 3.6 ms"
- [iTNC 530 manuals, NC software 340 422](https://content.heidenhain.de/doku/tnc_guide/pdf_files/iTNC530/340422-xx/lotse/387_952-22.pdf), the older software generation: the same option
- [iTNC 530 ISO programming, M120 look-ahead](https://www.manualsdir.com/manuals/798684/heidenhain-itnc-530-340-422-iso-programming.html?page=205): look-ahead of up to 99 blocks for radius compensation
