"""Motor table shared by gen_motor_tests.py (autotune) and gen_soak_tests.py.

Real motors come from conf/*.txt (r, l, polecount, j, max_ac_cur). Where a
config has no conf0.psi, psi = max_force / (1.5 * polecount * max_ac_cur),
i.e. the torque constant at peak current. Synthetic motors fill the corners
the configs do not reach (big, direct drive, spindle, micro, slow l/r).
"""

# name, r [ohm], l [H], psi [Wb], polecount, j [kg m^2], max_ac_cur [A]
MOTORS = [
    # conf/*.txt
    ("ecma_c30604fs",  2.1517, 0.022957, 0.047433, 5, 0.000044,   10),
    ("e240",           1.4,    0.0013,   0.2,      1, 0.0000268,  13),
    ("rx60_456",       10,     0.002,    0.1961,   1, 0.00001,    17),
    ("fanuc_a06b",     0.6,    0.01,     0.1852,   4, 0.00035,    18),
    ("fanuc_b05_6000", 1,      0.005,    0.1852,   4, 0.000025,   18),
    ("festo",          9.6,    0.01,     0.05,     4, 0.000026,   10),
    ("manutec_r15_1",  0.4,    0.002,    0.08,     3, 0.00035,    5),
    ("mitsu",          1.2,    0.005,    0.1333,   2, 0.00005,    10),
    ("move",           2,      0.007,    0.0667,   5, 0.00003,    3),
    ("p50b",           0.4,    0.01,     0.38,     2, 0.0001,     5),
    ("r2aa04003f",     12,     0.00066,  0.0267,   5, 0.00000247, 2),
    ("r2aa06020f",     2.7,    0.0007,   0.0524,   5, 0.0000219,  5.6),
    ("rexroth",        3.75,   0.008,    0.1111,   3, 0.000013,   6),
    ("rx60_123",       0.4,    0.002,    0.098,    2, 0.0001,     17),
    ("sda_b",          5,      0.01,     0.1,      5, 0.00005,    4),
    ("sgmph_01a1",     5,      0.01,     0.1667,   3, 0.0000015,  4),
    ("sgmph_08aa",     5,      0.01,     0.1333,   3, 0.0001,     10),
    ("sm060",          1.6,    0.008,    0.0844,   5, 0.00007,    9),
    ("sr60_2",         25.5,   0.001,    0.0467,   4, 0.000015,   2.5),
    ("weiler_x",       0.4,    0.01,     0.3333,   2, 0.00007,    10),
    ("as60_smartabs",  1.6,    0.008,    0.1056,   4, 0.00007,    9),
    ("bene_sanyo",     1.4,    0.0037,   0.05,     5, 0.00008,    8),
    ("sgmph_04aa",     5,      0.01,     0.1333,   3, 0.00002,    10),
    ("sr60_1",         2.45,   0.001,    0.0621,   4, 0.000039,   11),
    ("sr60_3",         25.5,   0.001,    0.0467,   4, 0.000011,   2.5),
    ("sr60_4",         7.6,    0.001,    0.0455,   4, 0.00005,    5.5),
    ("sr8_0",          2,      0.003,    0.05,     4, 0.0004,     10),
    ("sr8_1",          2,      0.003,    0.05,     4, 0.00015,    10),
    ("weiler_z",       0.4,    0.01,     0.1667,   2, 0.000045,   20),
    ("wobl",           1.2,    0.005,    0.1333,   2, 0.00005,    10),
    # SEW-EURODRIVE CMP40M/BP/KTY/RH1M (3000 rpm, 400 V; brake, resolver), from the
    # plate and SEW's CMP catalog: R1 19.92 ohm, L1 45.5 mH line to line (halved
    # here: phase values), Up0 56 V / 1000 rpm line rms -> psi 45.7 V phase peak
    # / (2 pi 50 Hz); 6 poles (150 Hz at 3000 rpm); J 0.18 kg cm^2 with the
    # brake; Imax 6 A rms -> 8.5 A peak. M0 0.8 Nm at I0 0.95 A, Mmax 3.8 Nm.
    ("sew_cmp40m",     9.96,   0.02275,  0.1455,   3, 0.000018,   8.5),
    # synthetic
    ("syn_servo_2kw",  0.3,    0.004,    0.25,     4, 0.0012,     30),
    ("syn_torque_14p", 2.5,    0.015,    0.35,     14, 0.01,      8),
    ("syn_spindle",    0.15,   0.002,    0.12,     2, 0.02,       40),
    ("syn_micro",      15,     0.004,    0.015,    4, 0.0000004,  1),
    ("syn_slow_lr",    0.1,    0.02,     0.1,      3, 0.0003,     20),
]
