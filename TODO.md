# TODO

Open work, newest first. Bugs and findings go in `DEBUGNOTES.md`.

- **Motor temperature sensor on the LV board:** firmware, templates
  (`mtemp_fb0`, `mtemp_fb1`), wizard and the solder-pad mod are ready
  (docs/motor_temp_lv.md). Next: open JP2 on the bench drive, KTY to FB0
  pins 7 / 8, check the reading's resolution and that the resolver stays
  clean enabled and under motion.
- **USB console under motor current** (DEBUGNOTES 2026-10-29): the drive
  and the wizard now recover (re-enumeration, reconnect), but it breaks
  every few seconds during id_mot. Check the bench's USB cable, grounding,
  a USB isolator. Find why the tuning over USB came out lower.
- **Resolver on FB1:** needs an excitation on FB1's Z driver (TIM1 CH3,
  PE13) synced to FB0's, polarity demodulation for sin1/cos1 in `adc.c`, a
  `res_fb1` template; FB1 gets 1 of 10 ADC samples, so it is noisier.
- **SEW CMP40M:** commissioned (wizard, all steps, `conf/sew_cmp40m.txt`)
  without the KTY. Put the KTY back once it's read on the LV board (above),
  and run at 314 V for the full 3000 rpm.
- **Wizard simulator:** `syn_torque_14p` fails at the torque constant check
  (identified 1.31 vs nameplate 10.4 Nm/A), already before 2026-10-29.
- Boot-time fault 11 (HV volt error at 57 ms, before the bus is measured)
  is logged on every boot.
