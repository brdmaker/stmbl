# hwtest/phasing: autophasing on the real drive

Without commutation feedback (no U/V/W, no absolute encoder) the drive
finds the rotor's electrical angle at every enable until it has seen the
encoder index (`shared/comps/fb_switch.c`, DEBUGNOTES "Autophasing"). How
well that goes depends on where the rotor sits at boot. This suite boots
the drive with the rotor at many positions and checks each phasing.

```sh
cd hwtest/phasing
./phasing.py                      # 12 positions, free and brake closed
./phasing.py --align              # also the old align method, to compare
./phasing.py --positions 24 --step 0.3 --no-brake
```

The motor moves (the check move, and `--step` mm on to the next
position). LinuxCNC and halrun must not be running; the drive is on
`/dev/ttyACM0` (`--usb`) and the Mesa card's smart-serial (`--prefix`).

## Each case

1. The drive is reset over USB: a fresh boot, so the incremental encoder
   counts from 0 again and phasing starts from a new angle error.
2. A fresh HAL session (`phasing.hal`; hostmot2 does not take a drive
   back after it reset), the drive's `fault0.max_pos_error` lowered
   (`--max-pos-error`), the case's `fb_switch0.phase_mode` set.
3. Enable at the current position: the rotor's swing (hm2 `pos_fb`) and
   the time until the drive is enabled, or its fault. Then
   `fb_switch0.phase_fail`, `phase_amp`, `phase_len`.
4. The angle check: the holding torque at rest (`pid0.torque_cmd`), then a
   `--check-move` mm move out and back at `--check-vel` with velocity
   feedforward: the error must stay under `--max-error` and the drive
   enabled. A wrong angle shows here (weak, or a position error fault).
5. A move of `--step` mm to the next position (0.55 mm: not a fraction of
   the 2 mm electrical period, so the positions spread over it), disable.

| case | phasing with | pass |
|---|---|---|
| `probe` | `phase_mode 1` (default), free rotor | swing under `--max-swing`, the check move fine |
| `brake` | the same, the brake held closed (`io0.out0 = 0`), released for the check move | the check move fine, or `PHASING_ERROR` (17): refusing is right when nothing can move |
| `align` (`--align`) | `phase_mode 0` | the check move fine (the swing is only recorded) |

A reset at the end puts the RAM settings back.

## Results

`runs/<date>-<time>/` (git-ignored): `summary.md` (PASS / FAIL, ranges per
case, a table of every case), `cases.csv`, `phasing.log` (every console
command and answer).
