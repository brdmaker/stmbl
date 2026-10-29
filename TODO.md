# TODO

Open work, newest first. Bugs and findings go in `DEBUGNOTES.md`.

- **Motor temperature sensor on the LV board.** The KTY/PT1000/PTC input is
  on the HV board (`mtemp`, hv0.mot_temp), whose ground is the HV side's; on
  the SEW CMP40M its wires run with the resolver's and couple the inverter's
  switching into it (DEBUGNOTES 2026-10-29). Read the sensor on an LV input
  instead (an analog input of io0, or FB1's spare sin/cos), selectable per
  config.
- **USB console stalls while the motor runs** (DEBUGNOTES 2026-10-29, Open):
  the IN endpoint keeps a packet armed that the core never sends; abort and
  resend don't revive it. Next: re-initialise the USB core when healing
  fails, and check the bench's USB cable and grounding.
- **Resolver on FB1:** needs an excitation on FB1's Z driver (TIM1 CH3,
  PE13) synced to FB0's, polarity demodulation for sin1/cos1 in `adc.c`, a
  `res_fb1` template; FB1 gets 1 of 10 ADC samples, so it is noisier.
- **SEW CMP40M commissioning:** mechanical id (wizard step 12), tuning, then
  brake, limits, link, verify, save; `conf/sew_cmp40m.txt` with the measured
  values (r 22.54 ohm, l 27.9 mH, 3 pole pairs, psi 0.1535 Wb).
- Boot-time fault 11 (HV volt error at 57 ms, before the bus is measured)
  is logged on every boot.
