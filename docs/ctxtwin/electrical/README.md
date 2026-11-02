# CTX twin: electrical documentation as text

The scanned electrical folder (chapters 3-9), transcribed page by page. Each
file keeps every pinout, parameter and message and ends with a "For the
retrofit" section. Unclear readings are marked [uncertain] or [unreadable].

Most of the folder is **Indramat's own manuals**, not Gildemeister drawings:
the generic DDS 2.1 servo, KDA/RAC spindle and KDV 4 supply manuals. Only a
few pages are specific to this machine:
- the spindle drive parameter sheet, `05d_..._p27-52.md`, PDF page 51;
- the electrical equipment list, `09_electrical_stock_list.md`;
- the ELTROPILOT cabling plans, `07_hardware_*`.

**Gaps:**
- **No cabinet circuit diagrams** in these chapters: no -K/-X wiring for the
  E-stop chain, the drive enables or the I/O.
- **Stock list:** printed pages P 5 and P 6 are missing from the scan, and
  they would hold the motor (-M) and some contactor entries.
- **Drive parameter values:** only the spindle drive has them (the
  parameter sheet). The feed-drive parameters must be read from each drive.

| file | source | content |
|---|---|---|
| [03_electronic_subassemblies_cooling_unit.md](03_electronic_subassemblies_cooling_unit.md) | Gildemeister DDAAE0G1, Feb 1995 | HM4 hood-open inching relays, DINA spindle speed monitor (E-stop chain), SRBF safety relays, SK150 cabinet cooler and its alarm contacts |
| [04a_electronic_modules.md](04a_electronic_modules.md) | Indramat KDV 4 manual, 06.91 | supply module KDV 4.1-30-3: DC link, X1 bus, X10 status contacts, Indramat's E-stop wiring schemes |
| [04b_electronic_modules.md](04b_electronic_modules.md) | same, second part | X10/X1 pinouts, Bb1 / UD / TVW / HSM, NCB link, faults, ordering codes |
| [05a_main_spindle_drive.md](05a_main_spindle_drive.md) | Indramat AC main spindle drives (KDA/TDA/RAC), 12.91 | overview, commissioning, set-point wiring |
| [05b_main_spindle_drive.md](05b_main_spindle_drive.md) | same | keypad, parameter records P/Q/R/S, X2 terminal, X4 command, orientation, feedback |
| [05c_main_spindle_drive.md](05c_main_spindle_drive.md) | same | controller functions: positioning, C axis, limits, load/temperature outputs, E-stop, mains failure |
| [05d_main_spindle_drive_p01-26.md](05d_main_spindle_drive_p01-26.md) | same | E-stop / AS, status and fault messages, parameter overview |
| [05d_main_spindle_drive_p27-52.md](05d_main_spindle_drive_p27-52.md) | same + **CTX-TWIN HSP parameter sheet 24.03.1994** | X2/X4/X6/X3/X5 pinouts, type codes; **this machine's spindle drive and its parameters** |
| [06a_servo_drives.md](06a_servo_drives.md) | Indramat DDS 2.1 + MDD, ANALOG interface, 209-0069-4315-03, 12.93 | system configurations, plug-in modules, type codes, safety, service port |
| [06b_servo_drives.md](06b_servo_drives.md) | same | setup program menus, first start-up, error reactions, brake, Ready, scaling, **DAE 1.1 encoder output** |
| [06c_servo_drives_p01-27.md](06c_servo_drives_p01-27.md) | same | absolute output, master/slave, torque reduction, velocity loop, fault codes |
| [06c_servo_drives_p28-54.md](06c_servo_drives_p28-54.md) | same | diagnostics, parameter forms, connection diagrams, **X1-X6, X13-X16 pinouts** |
| [07_hardware_p01-35.md](07_hardware_p01-35.md) | Gildemeister DAAAE0G0, Nov 1993 | ELTROPILOT EP90 rack, cabling plans (duplex = twin), power supply, CPU and graphics cards |
| [07_hardware_p36-70.md](07_hardware_p36-70.md) | same | VK030, SI188 PLC, VIC30 interpolator, **VAP4 axis card**, IO48 I/O, monitor, keyboard, handwheel |
| [09_electrical_stock_list.md](09_electrical_stock_list.md) | Gildemeister equipment list, 03.04.95 | order-specific parts by tag (P 5-6 missing) |

## The drive system in short

**Command interface: analog ±10 V velocity plus incremental encoder back, no
SERCOS.** The original control's VAP4 axis cards each had four 16-bit ±10 V
outputs and four encoder counters. The twin used three VAP4 cards
(07_hardware):
- **VAP4.1:** X1, Z1, main spindle 1, plus a separate C-axis encoder.
- **VAP4.2:** the same for side 2.
- **VAP4.3:** the driven tools of both turrets.

A Mesa analog card (7i77 or similar) takes their place one for one.

**Feed axes: Indramat DDS 2.1 + MDD 093**
- **Interface module:** DAE 1.1 (configuration DA01: analog interface with
  incremental encoder emulation). Two DDS 2.1-W050-DA01-00 are in the stock
  list; that the feed axes use the same module is inferred.
- **Command:** differential ±10 V, X13/1 E1, X13/2 E2. Velocity mode;
  the volts-to-rpm scaling and a command-smoothing time are drive parameters.
- **Encoder out, X14 (15-pin):** RS-422. A Ua1 on 5, /Ua1 on 6; B Ua2 on 8,
  /Ua2 on 1; marker Ua0 on 3, /Ua0 on 4. **The control supplies +5 V on 12**
  (0 V on 10).
- **Line count:** set in the drive, 1-262144 lines per turn, output limited
  to 504 kHz. Read it before setting the LinuxCNC encoder scale. A low count
  makes the position loop step audibly.
- **Logic inputs, 24 V (X13):** RF (enable) on 4, AH (drive halt, must be at
  24 V to move) on 5, Ired1/Ired2 (torque limits) on 6-7. TVW (temperature
  warning) is an output on 3. The external 24 V goes to 8/9.
- **Ready:** Bb potential-free contact on X3/6-7, closed = ready.
- **Safety lockout:** AS+/AS- on X3/8-9, acknowledge contact ASQ on X3/10-11.
- **Holding brake:** the drive switches it itself, on X6 (BR on 4, 0VB on 5,
  24 V supply on 3/6). It releases with RF and closes below 10 rpm or after
  400 ms. The motor thermistor is on X6/1-2.
- **Service port, X2:** RS-232, 9600 baud, 8N1, VT100. Back up every drive's
  parameters; they live on the plug-in DSM module, so moving the module to a
  spare drive carries the settings over.

**Main spindles: Indramat KDA 3.2-150-3 + 1MB241H built-in motor**
(parameter sheet 24.03.1994)
- **Motor:** 6-pole, water cooled, max 240 Nm, 22 kW from 850 to 3200 rpm,
  max 6300 rpm.
- **Command:** ±10 V = 6300 rpm on X4/1-2 (E1/E2). The type-plate line has
  an "L" that would mean SERCOS, but the rest of the sheet says analog:
  check the plate.
- **X2 (24 V):** inputs are RF on 24, RUN on 25, POS1/POS2 on 26/27,
  PAR1/PAR2 on 30/31 (select parameter record), EXT POS on 37, and AS on 3.
  Outputs are READY on 4, N=CMD on 5, N<MIN on 6, INPOS on 7 and TEMP WARN
  on 9. Analog speed is on 21 and load on 20. The Bb ready relay is on
  X11/1-2.
- **Encoder out on X6, NC supplies 5 V:** the line count depends on the
  active parameter record (P 16000, Q 2048, R and S 1024). LinuxCNC must
  switch its encoder scale with PAR1/PAR2. Record P is probably the C axis.
- **Orientation:** inside the drive, three positions (0, 120, 240°) with
  INPOS.
- **Spindle sync for the part transfer:** not in the drive (its sync
  parameters are empty). The control did it, so LinuxCNC must too.
- **Ramps and set-point filter:** must be off in any record used for the C
  axis or sync.
- **After RF:** the command is ignored for 200 ms while the motor magnetises.

**Supply: Indramat KDV 4.1-30-3**
- 3 x 380-460 V in, 320 V DC link, 30 kW, feeds braking energy back into the
  mains.
- **X10 contacts:** Bb1 ready on 6-7, TVW temperature pre-warning on 9-10
  (opens 30 s before Bb1), UD power OK on 11-12, HSM electronics-OK output
  on 8.
- **NCB link (X10/13-14):** keep it open, so the supply module brakes the
  drives itself on mains loss.

**Safety parts that stay** (03_electronic_subassemblies):
- the **DINA** spindle speed monitor, which works from the encoder without
  the NC (50 rpm with the hood open) and sits in the E-stop chain;
- the **SRBF** safety relays, with an adjustable off-delay;
- the **HM4** relays for inching with the hood open;
- the **Pilz P2HZ/5** (stock list).

## For the LinuxCNC retrofit with the original drives

**Per side** (X, Z, spindle), plus the driven tool:

| what | count | wiring |
|---|---|---|
| ±10 V analog outputs | 3 (+1 driven tool) | differential to E1/E2; shield at the drive end |
| encoder inputs | 4 (X, Z, spindle via drive X6, separate C-axis encoder) (+1) | RS-422; the card supplies 5 V to the drive's encoder output |
| 24 V outputs | RF + AH per servo, RF/RUN/POS1/POS2/PAR1/PAR2 per spindle | — |
| 24 V inputs | Bb/ready per drive, TVW, spindle READY/INPOS/TEMP WARN, supply Bb1/UD/TVW/HSM | — |

**E-stop chain:**
- Bb1 of the supply and each drive's Bb go in series with the Mesa
  watchdog contact. The watchdog replaces the VAP4's.
- E-stop drops RF and the drives brake at full torque. The main contactor
  drops after a delay (the SRBF off-delay), not at once.

**Order:**
1. control voltage;
2. the drives report ready (bb);
3. main contactor;
4. RF;
5. the speed command, 100 ms later (200 ms for the spindle).

**Position loop:**
- Velocity-mode drives need only a gentle loop. Scale the output in mm/s
  (`OUTPUT_SCALE` = speed at 10 V), FF1 = 1, P about 20-30, I = D = 0, and
  a deadband of 1 count.
- A 1 kHz servo thread is plenty.
- Analog wiring next to the power cables is the usual source of hum: keep the
  DAC card in the drive cabinet and use differential, shielded pairs.
