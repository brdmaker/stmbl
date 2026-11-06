# encmon with fanuc_tw, FANUC betaiS 4/4000 #2, desk drive 205735A45631, 2026-11-06

conf/fanuc_bis4-4000-2.txt + link encmon + link jog_cmd, 201 V bus, motor
uncoupled. encmon defaults: thres 0.30 (residual / back-EMF), trip_time
20 ms, min_bemf 5 V (here above ~60 rad/s), lp 50 Hz. encmon_test.py moves
stp0 at 100 rad/s; conf0.mot_fb_offset is mechanical (x 4 pole pairs).

| offset error (electrical) | current limit | encmon max ratio | result |
|---|---|---|---|
| 0 (commissioned), 3 enables, 8 moves each | 12.7 A | 0.150-0.152 | quiet |
| 10 deg | 12.7 A | 0.33 (under 20 ms) | quiet |
| 30 deg | 12.7 A | - | **fault 18 at the enable** |
| 115 deg (0.5 rad mech) | 3 A | - | **fault 18 at the enable** |
| 180 deg (pi/4 mech) | 2 A | - | **fault 18 at the enable** |
| 0 (motor #1's offset: pi mech = 4 pi el) | 2 A | 0.137 | quiet: the same electrical angle |

The two betaiS 4/4000 offsets (+1.560 / -1.582) differ by pi mechanical:
FANUC mounts these encoders at the same electrical angle, so either config
runs either motor. A wrong offset that matters (>= 30 deg el.) trips encmon
at the first enable, before any commanded move.
