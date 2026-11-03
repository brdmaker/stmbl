# FANUC motors and drives on hand

Five FANUC servo motors to run with stmbl, plus a FANUC control and
amplifiers. The FANUC control and amplifiers are kept as a reference
system, not for running the motors (see [fanuc-system.md](fanuc-system.md)).

| doc | what |
|---|---|
| [motors.md](motors.md) | each motor: nameplate, derived values, what stmbl needs |
| [encoders.md](encoders.md) | αA64 (supported by `encf`) and βiA128 (to be tested): frame, wiring, bench test |
| [fanuc-system.md](fanuc-system.md) | the CNC and the βiSV / β amplifiers; using them to record the βiA128's traffic |

Values from the labels are as read from the photos (`img/`). Everything
else is marked: **✱** means unverified, to be checked against the FANUC
manual or on the bench.

## Inventory

| qty | motor | spec | encoder | stmbl today |
|---|---|---|---|---|
| 2 | βiS 12/2000 | A06B-0077-B103 | βiA128, A860-2020-T301 | encoder to be tested |
| 2 | βiS 4/4000 | A06B-0063-B103 | βiA128, A860-2020-T301 | encoder to be tested |
| 1 | α2/3000 | A06B-0373-B175 | αA64, A860-0360-T001 | supported (`encf`, template `fanuc_fb0`) |

| qty | unit | type |
|---|---|---|
| 1 | GE Fanuc CNC control unit, with I/O board | A02B-0289-B802 |
| 4 | servo amplifier βiSV 20 (FSSB) | A06B-6130-H00x ✱ (label partly unreadable in the photo) |
| 1 | servo amplifier β series | A06B-6093-H111 |

All five motors are 200 V class with 4 pole pairs and a Y winding, which is
the normal stmbl case (230 VAC in, 325 V DC bus). None has a separate
brake connector in the photos, so they are taken to be brakeless ✱.

## Plan

1. **α2/3000 on stmbl.** Its αA64 is the encoder `encf` was written for.
   Wire it to FB0, encoder only, and check `encf0.crc_ok` counts and
   `encf0.pos` follows the shaft. This proves the cable, the 5 V supply and
   the request/receive path. Then run it in closed loop and autotune.
2. **βiA128 on the same cable.** If `crc_ok` counts, the i-series encoder
   speaks the same frame and the βiS motors work like the α2. If not, dump
   raw frames (`encf0.send_step = 100`) and compare them with the αA64's.
3. **If needed, record the FANUC system's own traffic** (CNC + βiSV 20 +
   βiS motor) with a logic analyzer, decode it, and add a βiA128 mode to
   `encf`, tested in the host simulator on the recorded frames first.
4. **Once the encoders read:** `conf/` files for the three models, and the
   motors added to the simulator's motor set (soak and e-brake suites).

Before changing a drive's config, back it up (`servoterm-cli --dump`). For
flashing, use only the repo's make targets (see `CLAUDE.md`).
