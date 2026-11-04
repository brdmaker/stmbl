# stmbl pack power supply (PSU): design state

A supply card for a pack of stmbl v4 drives on single-phase 230 V, sized
for the CN 720 (three axes with CMP40Ms, the spindle VFD on its own feed).
This collects what was decided in discussion. Nothing is drawn in KiCad
yet. The old `hw/kicad/rectifier2/` is the starting point for the blocks.
The braking side is in [emergency_stop.md](emergency_stop.md).

## Decided

**Power path**

```
230 V ─ 2-pole isolator ─ slow fuse ─ EMI filter + varistor ─ K1 (L) / K2 (N)
      ─ PTC ‖ KBY ─ bridge ─┬─ C bus ~2 mF / 450 V ─┬─ DC fuse ─ drive 1..n
                            ├─ chopper (hardware)    │
                            ├─ bleed KBL (NC) 1 kΩ   │
                            └─ bus-live LED, isolated bus sense
```

| block | choice | why |
|---|---|---|
| bus | ~325 V (360 V at +10 % mains), about 2 mF, e.g. 4 × 470 µF 450 V | 400 V three-phase would be above stmbl's 380–410 V limits |
| main relays K1, K2 | force-guided 1 NO + 1 NC, 8 A PCB (Finder 50.12 class), **K1 in L, K2 in N** | two independent breaks, each with a mirror contact; 2-pole relays with a mirror are too big. The inlet isolator is the service disconnect |
| inrush | **PTC** about 50–100 Ω cold, rated ≥ 105 J per charge, bypassed by **KBY** (force-guided) | a PTC left in circuit by a failed bypass limits itself, an NTC burns. KBY's mirror blocks a start if it has welded |
| cut timing | K1/K2 drop **immediately**, no RC hold-up | the drives brake from the bus, which stays charged after the mains is cut (100 J vs 5–10 J per axis). Never switch between the caps and the drives |
| chopper | hardware comparator, on ~375 V / off ~365 V, 600 V IGBT/MOSFET, 100–150 Ω, thermal switch, fuse; drives' `high_dc_volt` ~385 V | must work with no firmware and no 24 V: powered from the bus |
| bleed | NC relay KBL + 1 kΩ, held open while running, released 1–2 s after the cut | fails to "discharged". The delay must not starve a stop in progress |
| drive fuses | fast, 450 V DC, 6–10 A per drive | a shorted stage doesn't take the pack down |

**Relay drive (hardware only, never over software)**

- **+24 V side:** `DRV_ENABLE` from the controller board's chain:
  pwmdc K7 → E-stop pole 1 → E-stop-tested → OFF-tested → OFF → run relay.
  On the PSU it passes through **KS**, a normally closed relay the PSU's
  microcontroller can only *open*.
- **0 V side:** the coils' return, `COIL_RET`, goes back to the controller
  and through the **E-stop's second pole** to GND. So the E-stop opens the
  coils directly, whatever the relay logic does (two channels, no safety
  module).
- **Suppression:** a diode plus a 24 V zener per coil, so the contacts open fast.
- **Feedback loop:** the NC mirrors of K1, K2 and KBY in series with the
  ON button. A welded relay means "won't start".

**pwmdc** (on the controller board, done and simulated): LinuxCNC's
per-servo-period toggle → BC817 → BC817/BC807 push-pull → 68 Ω → pump
(C27 4.7 µF 50 V, BAT54 ×3, C28 4.7 µF 35 V) → 390 Ω + G6K-2G-Y-TR DC12
between the pumped rail and +24 V. No DC fault can raise the rail above
+24 V, so it fails to off. Pull-in takes about 3 ms, release 20–40 ms. The
coil has +16 % margin at 22.8 V supply with the coil at 50 °C. `PWM_FAIL`
is on the same pole as OUT.

**Controller and link**

- **The microcontroller is on the 24 V side**, sharing the controller's
  ground: an STM32G4 (G431/G473, the `vibsensor`/`brdstepper` framework).
  The F042 in `rectifier2` is too small.
- **Smart-serial** on one port, plain RS-422 with no isolation needed. It gives
  process data, the console, `servoterm-cli --update` and the version check,
  like a drive.
- **Isolated reads from the hot side:**
  - bus voltage, through an AMC1311/AMC1306;
  - mains present after K1/K2, through an optocoupler (this is the direct weld check);
  - chopper active, through an optocoupler;
  - heatsink and PTC temperature.
- **What the microcontroller does:**
  - times the bypass: closes KBY at 90 % of peak once the bus is steady, and
    confirms it from KBY's contact;
  - raises `BUS_READY`;
  - cuts power (opens KS) if the bus hasn't charged within 2 s, sags below 80 %
    under load, a mirror disagrees with its coil, mains is present while it
    should be off, or it overheats;
  - discharges the bus after the cut;
  - drives the ON lamp: steady = ready, slow flash = charging, fast flash = fault.

| PSU → LinuxCNC | LinuxCNC → PSU |
|---|---|
| bus voltage, bus power (for `powerquota`) | software off (opens KS) |
| `BUS_READY`, fault code | discharge request |
| K1/K2/KBY/KBL coil and mirror states, mains after relays | chopper test (optional) |
| chopper on-time, energy, resistor temperature estimate; heatsink, PTC temperatures | |

## Open

1. **Which signals stay wires once smart-serial is there.** The 8-wire
   cable drawn earlier: 1 `DRV_ENABLE`, 2–3 feedback loop, 4 `COIL_RET`,
   5 `BUS_READY`, 6 `PSU_FAULT`, 7 `SW_OFF`, 8 GND. Proposal:
   - **keep 1–4** (the safety path);
   - **keep `BUS_READY`** as a dry contact into LinuxCNC's estop input, so
     motion can't enable on a link error;
   - **drop `PSU_FAULT` and `SW_OFF`** to smart-serial. Link loss for more
     than about 50 ms opens KS, so it still fails off.
2. **Hot-side supply for the chopper and the bus sense.**
   - Proposal: a small off-line converter from the bus (LNK304 as in
     `rectifier2`, or a VIPer). The chopper then works with the 24 V gone,
     which was the goal ("braking must not depend on the LV side").
   - The isolated amplifiers' hot side then runs from it too.
   - The alternative, an isolated DC/DC from 24 V, is simpler but loses the
     chopper together with the 24 V.
3. **Power rating.**
   - 8 A relays and a 230 V single-phase feed give about 1.5 kW continuous,
     enough for the CN 720.
   - The 2 kW stage limit discussed earlier would need DIN contactors.
     That's possible through the planned external-contactor connector,
     with the same monitoring.
4. **Bus current or power measurement.** A shunt plus an AMC1302 in the bus
   return would give `powerquota` a measured figure instead of the drives'
   sum. It's optional, and costs one more isolated channel.
5. **Board split.**
   - One board with ≥ 6 mm creepage between the hot area and the 24 V area,
     or a hot power board plus a small 24 V logic board stacked on headers.
   - Splitting keeps the 24 V board reusable and its rework safe.
6. **Bus capacitance.** Each drive's own 540 µF adds to the 2 mF. Size it
   against ripple and braking energy in the host simulator's shared-bus
   model (`host/hw/hv.c`, `mains_*` / `regen_*` tests) before buying caps.

## Next steps

1. Settle the open points above.
2. A one-page KiCad schematic with `hardware/kigen` (like `vibsensor`),
   with part values.
3. The `psu/` firmware target: smart-serial process data, the console,
   update, the charge, bypass and bleed sequence, weld checks; host
   simulator tests of the sequence and its faults.
4. millweb: a PSU card on the Drives page.
