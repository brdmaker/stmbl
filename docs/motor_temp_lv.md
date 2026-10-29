# Motor temperature sensor on the LV board (feedback port pin 7)

By default the motor's temperature sensor (KTY84-130, PT1000, PTC, NTC or a
thermal switch) goes to the HV board's 2-pin motor temperature connector.
The F3 measures it there (`hv0.mot_temp`), with the HV side as its
ground. On a motor whose sensor wires run in the feedback cable, as on SEW's CMP
motors, those wires then carry the inverter's switching right next to the
feedback pairs. On the SEW bench that spoiled the resolver signal: its amplitude fell
from 0.259 to 0.16–0.25 with the drive enabled, and the drive faulted under motion
(DEBUGNOTES 2026-10-29).

Instead, the sensor can go to **pin 7 of a feedback port** (FB0 or FB1),
against pin 8 (GND), and be measured on the LV board. The board has the
circuit for it already: pin 7 has a 10 kΩ pull-up to +5 V and a 10 k / 1 k
divider into the F4's ADC (`io0.fb0`, `io0.fb1`). A 3-pad solder jumper
normally ties pin 7 to the port's +5 V supply for an encoder. That
jumper has to be opened.

| Port | Jumper | Pull-up | ADC divider | Reading |
|---|---|---|---|---|
| FB0 (with the motor feedback) | JP2 | R137 10 k | R135 10 k / R136 1 k, PC2 | `io0.fb0` |
| FB1 (second feedback port) | JP5 | R140 10 k | R138 10 k / R139 1 k, PC3 | `io0.fb1` |

**FB0** works only with a **resolver**, which needs no supply. Encoders
take their +5 V from pin 7. **FB1** works with any feedback on FB0, as
long as nothing else uses FB1.

## The solder-pad mod

Do this with the drive unpowered. The positions are from the v4.0 layout
(`hw/kicad/v4.0`); check them against your board (v4.3) before you cut.

1. **Find the jumper.** The three RJ45 jacks sit side by side at the board
   edge: CMD, FB0, FB1. JP2 (FB0) and JP5 (FB1) are on the component side,
   about 4 mm in from their jack, next to a 0603 resistor (R137 for JP2,
   R140 for JP5). Each is three pads in a row along the board edge, a
   thin trace joining the centre pad to one end pad.
   - centre pad: the RJ45's pin 7
   - end pad with the trace (pad 1, the one towards the CMD jack): +5 V
   - the other end pad (pad 3): +12 V, open
2. **Check before cutting** (multimeter, ohms): RJ45 pin 7 to the end pad
   with the trace reads 0 Ω.
3. **Cut the trace** between the centre pad and the +5 V pad with a scalpel,
   two cuts and lift the piece out. Don't bridge to the +12 V pad.
4. **Check after cutting:** pin 7 to the +5 V pad now reads about 10 kΩ (the
   pull-up), and pin 7 to pin 8 (GND) about 11 kΩ with nothing plugged in
   (the ADC divider). Pin 7 to the +12 V pad is open.
5. **Powered, no sensor:** `io0.fb0` (or `fb1`) reads about 2.6 V (the
   pull-up against the divider). 5.0 V means the jumper is still closed.

To undo it, bridge the centre pad to the +5 V pad with solder.

## Wiring

| RJ45 pin (T568B) | |
|---|---|
| 7 (brown stripe) | sensor |
| 8 (brown) | sensor, GND |

A KTY84 goes with its band (the cathode) to pin 8. The others either way round.
On a SEW CMP motor the KTY84 is the brown / white pair of the feedback
cable: it then goes into the same RJ45 as the resolver (pins 1–6), on 7 and 8.

**Insulation.** The sensor is then on the LV side, with the feedback, the
USB and the PC. Its insulation from the winding, inside the motor, must be
rated for that, as the feedback's is. On motors that bring the sensor out in
the feedback cable (SEW CMP) it is. Otherwise check the motor's
documentation.

## Config

The template links the input to `mtemp0` in place of the HV board's:

```
link mtemp_fb0      # or mtemp_fb1
conf0.mot_temp_type = 1   # 1 KTY84-130, 2 PT1000, 3 PTC, 4 NTC, 5 switch
```

`rpull0` (`shared/comps/rpull.c`, `shared/rpull.h`) turns the pin's voltage
into ohms. It takes the pull-up's current minus the divider's: `r = v / ((vsup − v) / rpull −
v / rload)`, with `vsup` 5 V, `rpull` 10 k and `rload` 11 k. With nothing
connected (or the jumper still closed) no current flows into a sensor:
`r = -1`, which `mtemp` reports as an open wire. `mtemp` filters the
reading, detects open and shorted wires, and converts it to °C as before.

**Resolution.** A KTY84 near 600 Ω gives about 0.26 V on pin 7, about 2.6 mV
per K, or 0.24 mV per K at the ADC: about a third of an ADC step. `mtemp`
filters 5000 readings a second, so the noise averages it out. Trim `rpull0.vsup` if
the +5 V is off. A 2.2 kΩ pull-up in place of the 10 kΩ (R137 / R140) gives
about 4× the signal, and `rpull0.rpull` must then follow it.

## Setup wizard

In the wiring step the wizard asks where the sensor is connected
(`temp.input`): `hv` (the HV board, the default), `fb1`, or `fb0` (only with
a resolver). For `fb0` / `fb1` it shows the jumper and the pins, and the
config gets `link mtemp_fb0` / `mtemp_fb1`. The base step's cold check then reads
`rpull0.r`. An open reading there adds "is its jumper open?".

## Tests

- `host/tests/mot_temp_fb.test`: a KTY84 and a PT1000 on FB0 pin 7 at known
  temperatures, the open wire, the jumper left closed.
- `host/tests/run_wizard_sim.py`, `ecma_c30604fs-fb1`: the whole wizard with
  the sensor on FB1, its cold reading; `-fb1jumper`: the jumper left closed
  stops the base step with the hint.
- `servoterm-cli` `make check`: the config builder (`mtemp_fb0`,
  `mtemp_fb1`) and the wiring text.
