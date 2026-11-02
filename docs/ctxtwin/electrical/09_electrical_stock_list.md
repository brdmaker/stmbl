# Gildemeister CTX twin, electrical documentation: 9. Electrical stock list (Electrical equipment list), PDF pages 1-20

Source: `1c9fec57-9_Electrical_stock_list_20201029_0001.pdf` (chapter "9 Electrical stock list", 20 scanned pages). Printed title on every page: **"ELECTRICAL EQUIPMENT LIST"**, Gildemeister "Drehmaschinen" logo. Header fields: TYPE **CTX-TWIN**, ORDER (empty), MACHINE NO. (empty), CUSTOMER **CE**, page "P n". Footer on every page: **"KAN OST"**, **"OSTW"**, date **03.04.95** (3 April 1995). No separate document number is printed. The list ends on printed page P 21 with "** END OF ELECTRICAL EQUIPMENT LIST **".

Original language: **English** (computer-printed list, Gildemeister's own English; a few German-isms are kept as printed, e.g. "SUBMINIATUR-HOUSING", "25-POLAR" = 25-pole, "OELFLEX" = Ölflex cable, "INTERFERENCING BLOCK" = Entstörglied / suppressor). Translations/explanations are given in the Notes column.

Notation: [uncertain] = reading not sure, [unreadable] = cannot be read. Notes marked "interpretation" are mine, not printed.

## Page index

The scan is **incomplete**: printed pages **P 5 and P 6 are missing** (PDF page 5 is already P 7). The list is sorted by device tag, and the gap lies between -K37281 (last row of P 4) and -S12651 (first row of P 7), so the missing pages would hold tags -K37xxx(>37281) ... -K4xxxx, -M.., -Q.., -R.. and low -S.. numbers (motors, circuit breakers, etc.) [interpretation]. PDF page 20 is a blank back side (only show-through of the previous page).

| PDF page | Printed page ("P") | Content (device tags) |
|---|---|---|
| 1 | P 1 | -A12641 ... -A28911 (safety relays, inching module, encoder-signal transducers, Indramat DDS drives, monitoring systems) |
| 2 | P 2 | -A36061 ... -K10141 (power optocouplers, output optimizers, TS 20 probe, motor protection relays, contactors, aux. relays) |
| 3 | P 3 | -K11041 ... -K28981 (contactors, aux. relays, time relay, aux. contactors) |
| 4 | P 4 | -K36221 ... -K37281 (contactors, aux. relays) |
| - | P 5, P 6 | **not in the scan** |
| 5 | P 7 | -S12651 ... -S15021 (pushbutton parts, limit switches, key switch) |
| 6 | P 8 | -S15021 ... -S15222 (pushbutton parts, proximity switches) |
| 7 | P 9 | -S15222 ... -S27911 (pushbutton parts, proximity switches) |
| 8 | P 10 | -S27921 ... -S28981 (proximity switches, illuminated pushbuttons) |
| 9 | P 11 | -S36131 ... -W12631 (switch elements, proximity switches, TS 20, diodes, LEDs, cables, valve plugs) |
| 10 | P 12 | -W12821 ... -W17421 (cables, valve plugs) |
| 11 | P 13 | -W19011 ... -W37011 (motor/feedback cables) |
| 12 | P 14 | -W42011 ... -X 5 +VAP4.2 (sensor cables, connectors) |
| 13 | P 15 | -X 6 +A17011 ... -X14011 (connectors) |
| 14 | P 16 | -X14011 ... -X21041 (connectors, Indramat motor connectors) |
| 15 | P 17 | -X3 +VAP4.3 ... -X42061 (connectors) |
| 16 | P 18 | -X42061 ... -X6 +A23011 (connectors) |
| 17 | P 19 | -Z10031 ... -Z15951 (suppressors) |
| 18 | P 20 | -Z36011 ... -Z37231 (suppressors) |
| 19 | P 21 | -Z37241 ... -Z37261, end of list |
| 20 | - | blank back side |

## Column meaning

Printed columns: **IDENT-NO** (Gildemeister part number, 7 digits), **DESIGNATION** (first line = name, following lines = type / order data), **QTY**, **MU** (measuring unit code; 1 = pieces; 3 for cables with decimal quantities, presumably metres; 4 appears on two cable rows [meaning not printed]), **EQUIPMENT IDENT.** (device tag "-..." plus location "+...").

Location codes (interpretation, not explained in this chapter): **+S** = control cabinet (Schaltschrank), **+M** = machine, **+SM** = between cabinet and machine (cables/plugs), **+B** = operator panel (Bedienfeld), **+A....** / **+M....** = mounted on the named device, **+VAP4.1/.2/.3** = on ELTROPILOT EP90 axis periphery card VAP4 no. 1/2/3 (see chapter 7). The digits in the tags very probably encode drawing sheet + current path (e.g. -A19011 = sheet 190, path 11) [interpretation].

Manufacturer is not a separate column; where printed it is inside the designation ("FA." = Firma). Type codes that are recognisable are attributed in the Notes column [interpretation].

## Parts list

### P 1 (PDF page 1)

| Ident-No | Designation | Type / order data | Qty | MU | Tag | Loc. | Notes |
|---|---|---|---|---|---|---|---|
| 0950599 | SAFETY RELAY | P2HZ/5 24V DC 2S/2OE. | 1 | 1 | -A12641 | +S | Pilz P2HZ two-hand/safety relay; "2S/2OE" as printed (2 NO / 2 NC?) [uncertain] |
| 0928373 | INCHING MODULE HM 4 | 4 RELAYS 24 V DC 1 | 1 | 1 | -A15011 | +S | inching = Tippbetrieb |
| 1047396 | EMERGENCY-STOP RELAY | SRBF-1102, NR.: 640 0002 | 1 | 1 | -A15911 | +S | E-stop safety relay |
| 1044569 | TRANSDUCER FOR ENCODER SIGNALS | TYP HGV 1.1 | 1 | 1 | -A17041 | +S | encoder signal converter/distributor (Geber-Signal-Umsetzer) |
| 0453922 | AC-MODUL-CONTROLLER | DDS 2.1-W-050-DA01-00 | 1 | 1 | -A19011 | +S | **Indramat DDS 2.1 digital AC servo drive**, 50 A class |
| 0954333 | PLUG | MINI-COMBICON 10 POLES, MC 1,5/10-ST-3,81-BS: 1-10 | 1 | 1 | -A19011 | +S | Phoenix Contact plug for the DDS |
| 1043654 | ACCESSORIES | FOR DDS MODULE E3-DDS | 1 | 1 | -A19011 | +S | Indramat accessory kit |
| 0453922 | AC-MODUL-CONTROLLER | DDS 2.1-W-050-DA01-00 | 1 | 1 | -A21011 | +S | **Indramat DDS 2.1**, second unit |
| 0954333 | PLUG | MINI-COMBICON 10 POLES, MC 1,5/10-ST-3,81-BS: 1-10 | 1 | 1 | -A21011 | +S | |
| 1043654 | ACCESSORIES | FOR DDS MODULE E3-DDS | 1 | 1 | -A21011 | +S | |
| 1044569 | TRANSDUCER FOR ENCODER.SIGNALS | TYP HGV 1.1 | 1 | 1 | -A23041 | +S | printed "ENCODER.SIGNALS" (dot instead of space) |
| 0680311 | MONITORING SYSTEM | - | 1 | 1 | -A27911 | +S | no type printed |
| 0680311 | MONITORING SYSTEM | - | 1 | 1 | -A28911 | +S | no type printed |

### P 2 (PDF page 2)

| Ident-No | Designation | Type / order data | Qty | MU | Tag | Loc. | Notes |
|---|---|---|---|---|---|---|---|
| 0950958 | POWER OPTOCOUPLER | AMS 10-43/5, /53VDC/4A | 1 | 1 | -A36061 | +S | Leistungs-Optokoppler |
| 0950958 | POWER OPTOCOUPLER | AMS 10-43/5, /53VDC/4A | 1 | 1 | -A37061 | +S | |
| 0928606 | OUTPUT OPTIMIZING | WMG17-OV-24DC/60DC/3 | 1 | 1 | -A42031 | +S | "OV" or "0V" [uncertain] |
| 0930274 | MEASURING CALIPER TS 20 | WITH 3 M CABLE AND INTERFACE | 1 | 1 | -A42071 | +M | "Messtaster" = touch probe; TS 20 (probably Renishaw tool-setting probe) [interpretation] |
| 0928606 | OUTPUT OPTIMIZING | WMG17-OV-24DC/60DC/3 | 1 | 1 | -A42231 | +S | |
| 0928846 | MOTOR PROTECTION RELAY | BA9039/100 DC24V | 1 | 1 | -F36041 | +S | thermistor motor protection relay (Dold BA9039) [interpretation] |
| 0928846 | MOTOR PROTECTION RELAY | BA9039/100 DC24V | 1 | 1 | -F37041 | +S | |
| 0928449 | AUXILIARY RELAY | IK 8802.12/009 | 1 | 1 | -K08771 | +S | Dold IK 8802 [interpretation] |
| 0928449 | AUXILIARY RELAY | IK 8802.12/009 | 1 | 1 | -K08781 | +S | |
| 0949522 | CONTACTOR | 3TF2222-0BB4 | 1 | 1 | -K10061 | +S | Siemens 3TF22, 24 V DC coil |
| 0928449 | AUXILIARY RELAY | IK 8802.12/009 | 1 | 1 | -K10071 | +S | |
| 0949522 | CONTACTOR | 3TF2222-0BB4 | 1 | 1 | -K10071 | +S | same tag -K10071 as the row above, as printed |
| 0928449 | AUXILIARY RELAY | IK 8802.12/009 | 1 | 1 | -K10121 | +S | |
| 0928449 | AUXILIARY RELAY | IK 8802.12/009 | 1 | 1 | -K10141 | +S | |

### P 3 (PDF page 3)

| Ident-No | Designation | Type / order data | Qty | MU | Tag | Loc. | Notes |
|---|---|---|---|---|---|---|---|
| 0949522 | CONTACTOR | 3TF2222-0BB4 | 1 | 1 | -K11041 | +S | tag digits faint, "-K11041" [uncertain] |
| 0949522 | CONTACTOR | 3TF2222-0BB4 | 1 | 1 | -K14041 | +S | "-K14041" [uncertain], faint |
| 0928449 | AUXILIARY RELAY | IK 8802.12/009 | 1 | 1 | -K14121 | +S | |
| 0928449 | AUXILIARY RELAY | IK 8802.12/009 | 1 | 1 | -K14122 | +S | |
| 0928449 | AUXILIARY RELAY | IK 8802.12/009 | 1 | 1 | -K14131 | +S | |
| 0928449 | AUXILIARY RELAY | IK 8802.12/009 | 1 | 1 | -K14141 | +S | |
| 0928449 | AUXILIARY RELAY | IK 8802.12/009 | 1 | 1 | -K14151 | +S | |
| 0949522 | CONTACTOR | 3TF2222-0BB4 | 1 | 1 | -K15431 | +S | |
| 0949522 | CONTACTOR | 3TF2222-0BB4 | 1 | 1 | -K15451 | +S | |
| 0928450 | TIME RELAY | IL 7824.21 0,1-1S | 1 | 1 | -K15461 | +S | Zeitrelais 0.1-1 s |
| 0949520 | AUXILIARY CONTACTOR | 3TH2262-0BB4 | 1 | 1 | -K15941 | +S | Siemens 3TH22 contactor relay, 24 V DC |
| 0949520 | AUXILIARY CONTACTOR | 3TH2262-0BB4 | 1 | 1 | -K15951 | +S | |
| 0928449 | AUXILIARY RELAY | IK 8802.12/009 | 1 | 1 | -K27981 | +S | |
| 0928449 | AUXILIARY RELAY | IK 8802.12/009 | 1 | 1 | -K28981 | +S | |

### P 4 (PDF page 4)

| Ident-No | Designation | Type / order data | Qty | MU | Tag | Loc. | Notes |
|---|---|---|---|---|---|---|---|
| 0949522 | CONTACTOR | 3TF2222-0BB4 | 1 | 1 | -K36221 | +S | |
| 0949522 | CONTACTOR | 3TF2222-0BB4 | 1 | 1 | -K36231 | +S | |
| 0949522 | CONTACTOR | 3TF2222-0BB4 | 1 | 1 | -K36241 | +S | |
| 0949522 | CONTACTOR | 3TF2222-0BB4 | 1 | 1 | -K36251 | +S | |
| 0949522 | CONTACTOR | 3TF2222-0BB4 | 1 | 1 | -K36261 | +S | |
| 0928449 | AUXILIARY RELAY | IK 8802.12/009 | 1 | 1 | -K36271 | +S | |
| 0928449 | AUXILIARY RELAY | IK 8802.12/009 | 1 | 1 | -K36281 | +S | |
| 0949522 | CONTACTOR | 3TF2222-0BB4 | 1 | 1 | -K37221 | +S | |
| 0949522 | CONTACTOR | 3TF2222-0BB4 | 1 | 1 | -K37231 | +S | |
| 0949522 | CONTACTOR | 3TF2222-0BB4 | 1 | 1 | -K37241 | +S | |
| 0949522 | CONTACTOR | 3TF2222-0BB4 | 1 | 1 | -K37251 | +S | |
| 0949522 | CONTACTOR | 3TF2222-0BB4 | 1 | 1 | -K37261 | +S | |
| 0928449 | AUXILIARY RELAY | IK 8802.12/009 | 1 | 1 | -K37271 | +S | |
| 0928449 | AUXILIARY RELAY | IK 8802.12/009 | 1 | 1 | -K37281 | +S | |

### P 5, P 6

Not contained in the PDF (see page index).

### P 7 (PDF page 5)

| Ident-No | Designation | Type / order data | Qty | MU | Tag | Loc. | Notes |
|---|---|---|---|---|---|---|---|
| 0928738 | TERMINAL COVER | TYP-NR.: 04.964.5 | 1 | 1 | -S12651 | +B | |
| 1047227 | MUSHROOM PUSHBUTTON BLACK | TYP-NR.04-070.0 40MM | 1 | 1 | -S12651 | +B | Pilztaster |
| 0938262 | LIMIT SWITCH WITH SWIVELING LEVER | 3SE3 120-1J | 1 | 1 | -S12681 | +M | Siemens 3SE3 |
| 0938262 | LIMIT SWITCH WITH SWIVELING LEVER | 3SE3 120-1J | 1 | 1 | -S12881 | +M | |
| 0954158 | SWITCH ELEMENT | TYP-NR.:61-8675.11 | 1 | 1 | -S14131 | +B | EAO series 61 [interpretation] |
| 0954161 | CAP COLOURLESS | TYP-NR.:61-9351.7 | 1 | 1 | -S14131 | +B | |
| 0954162 | PUSHBUTTON UPPER SECTION | TYP-NR.:61-1150.8 | 1 | 1 | -S14131 | +B | |
| 0954325 | PLATE | TYP-NR.:51-910.0 | 1 | 1 | -S14131 | +B | |
| 0278141 | LOWER SECTION FOR PUSHBUTTON AND SELECTOR SWITCH | ZB2-BZ105 | 1 | 1 | -S14281 | +B | Telemecanique ZB2 |
| 0278320 | FRONT ELEMENT FOR KEY-OPERATED SELECTOR SWITCH | ZB2-BG4 | 1 | 1 | -S14281 | +B | Schlüsselschalter |
| 0949634 | PLATE | - | 1 | 1 | -S14281 | +B | |
| 0954158 | SWITCH ELEMENT | TYP-NR.:61-8675.11 | 1 | 1 | -S15021 | +B | |
| 0954161 | CAP COLOURLESS | TYP-NR.:61-9351.7 | 1 | 1 | -S15021 | +B | |

### P 8 (PDF page 6)

| Ident-No | Designation | Type / order data | Qty | MU | Tag | Loc. | Notes |
|---|---|---|---|---|---|---|---|
| 0954162 | PUSHBUTTON UPPER SECTION | TYP-NR.:61-1150.8 | 1 | 1 | -S15021 | +B | |
| 0954325 | PLATE | TYP-NR.:51-910.0 | 1 | 1 | -S15021 | +B | |
| 0954158 | SWITCH ELEMENT | TYP-NR.:61-8675.11 | 1 | 1 | -S15022 | +B | |
| 0954161 | CAP COLOURLESS | TYP-NR.:61-9351.7 | 1 | 1 | -S15022 | +B | |
| 0954162 | PUSHBUTTON UPPER SECTION | TYP-NR.:61-1150.8 | 1 | 1 | -S15022 | +B | |
| 0954325 | PLATE | TYP-NR.:51-910.0 | 1 | 1 | -S15022 | +B | |
| 0251370 | PROXIMITY SWITCH | BES516-325-S4-K WITH PLUG | 1 | 1 | -S15041 | +M | Balluff inductive sensor |
| 0251370 | PROXIMITY SWITCH | BES516-325-S4-K WITH PLUG | 1 | 1 | -S15061 | +M | |
| 0954158 | SWITCH ELEMENT | TYP-NR.:61-8675.11 | 1 | 1 | -S15221 | +B | |
| 0954161 | CAP COLOURLESS | TYP-NR.:61-9351.7 | 1 | 1 | -S15221 | +B | |
| 0954162 | PUSHBUTTON UPPER SECTION | TYP-NR.:61-1150.8 | 1 | 1 | -S15221 | +B | |
| 0954325 | PLATE | TYP-NR.:51-910.0 | 1 | 1 | -S15221 | +B | |
| 0954158 | SWITCH ELEMENT | TYP-NR.:61-8675.11 | 1 | 1 | -S15222 | +B | |
| 0954161 | CAP COLOURLESS | TYP-NR.:61-9351.7 | 1 | 1 | -S15222 | +B | |

### P 9 (PDF page 7)

| Ident-No | Designation | Type / order data | Qty | MU | Tag | Loc. | Notes |
|---|---|---|---|---|---|---|---|
| 0954162 | PUSHBUTTON UPPER SECTION | TYP-NR.:61-1150.8 | 1 | 1 | -S15222 | +B | |
| 0954325 | PLATE | TYP-NR.:51-910.0 | 1 | 1 | -S15222 | +B | |
| 0944658 | PROXIMITY SWITCH | BES 516-347-M0-Y-PU05 | 1 | 1 | -S15241 | +M | "M0" or "MO" [uncertain] |
| 0944658 | PROXIMITY SWITCH | BES 516-347-M0-Y-PU05 | 1 | 1 | -S15261 | +M | |
| 0954158 | SWITCH ELEMENT | TYP-NR.:61-8675.11 | 1 | 1 | -S15421 | +B | |
| 0954161 | CAP COLOURLESS | TYP-NR.:61-9351.7 | 1 | 1 | -S15421 | +B | |
| 0954162 | PUSHBUTTON UPPER SECTION | TYP-NR.:61-1150.8 | 1 | 1 | -S15421 | +B | |
| 0954325 | PLATE | TYP-NR.:51-910.0 | 1 | 1 | -S15421 | +B | |
| 0954158 | SWITCH ELEMENT | TYP-NR.:61-8675.11 | 1 | 1 | -S15422 | +B | |
| 0954161 | CAP COLOURLESS | TYP-NR.:61-9351.7 | 1 | 1 | -S15422 | +B | |
| 0954162 | PUSHBUTTON UPPER SECTION | TYP-NR.:61-1150.8 | 1 | 1 | -S15422 | +B | |
| 0954325 | PLATE | TYP-NR.:51-910.0 | 1 | 1 | -S15422 | +B | |
| 0944658 | PROXIMITY SWITCH | BES 516-347-M0-Y-PU05 | 1 | 1 | -S15461 | +M | |
| 0919642 | PROXIMITY SWITCH | BES 516-324-S4-C M8X1 IP67 | 1 | 1 | -S27911 | +M | |

### P 10 (PDF page 8)

| Ident-No | Designation | Type / order data | Qty | MU | Tag | Loc. | Notes |
|---|---|---|---|---|---|---|---|
| 0919642 | PROXIMITY SWITCH | BES 516-324-S4-C M8X1 IP67 | 1 | 1 | -S27921 | +M | |
| 0949997 | CAP YELLOW | TYP-NR.:61-9351.4 | 1 | 1 | -S27981 | +B | |
| 0954156 | SWITCH ELEMENT | TYP-NR.:61-8610.11 | 1 | 1 | -S27981 | +B | |
| 0954162 | PUSHBUTTON UPPER SECTION | TYP-NR.:61-1150.8 | 1 | 1 | -S27981 | +B | |
| 0954164 | BULB | 28V 1,12W, NR.:31-963.28 | 1 | 1 | -S27981 | +B | this row is circled by hand in pencil, with a pencil mark at the left |
| 0954325 | PLATE | TYP-NR.:51-910.0 | 1 | 1 | -S27981 | +B | |
| 0919642 | PROXIMITY SWITCH | BES 516-324-S4-C M8X1 IP67 | 1 | 1 | -S28911 | +M | |
| 0919642 | PROXIMITY SWITCH | BES 516-324-S4-C M8X1 IP67 | 1 | 1 | -S28921 | +M | |
| 0949997 | CAP YELLOW | TYP-NR.:61-9351.4 | 1 | 1 | -S28981 | +B | |
| 0954156 | SWITCH ELEMENT | TYP-NR.:61-8610.11 | 1 | 1 | -S28981 | +B | |
| 0954162 | PUSHBUTTON UPPER SECTION | TYP-NR.:61-1150.8 | 1 | 1 | -S28981 | +B | |
| 0954164 | BULB | 28V 1,12W, NR.:31-963.28 | 1 | 1 | -S28981 | +B | Glühlampe |
| 0954325 | PLATE | TYP-NR.:51-910.0 | 1 | 1 | -S28981 | +B | |

### P 11 (PDF page 9)

| Ident-No | Designation | Type / order data | Qty | MU | Tag | Loc. | Notes |
|---|---|---|---|---|---|---|---|
| 1047596 | SWITCH ELEMENT | TYP-NR.:61-8655.11 | 1 | 1 | -S36131 | +B | "61-" faint [uncertain] |
| 0954158 | SWITCH ELEMENT | TYP-NR.:61-8675.11 | 1 | 1 | -S36231 | +B | |
| 0251370 | PROXIMITY SWITCH | BES516-325-S4-K WITH PLUG | 1 | 1 | -S42011 | +M | |
| 0251370 | PROXIMITY SWITCH | BES516-325-S4-K WITH PLUG | 1 | 1 | -S42211 | +M | |
| 0949657 | MEASURING UNIT | TS 20 | 1 | 1 | -S42271 | +M | probe |
| 0017971 | DIODE | 1N 4007, C 66047-Z1306-A7 | 1 | 1 | -V27971 | +S | |
| 0017971 | DIODE | 1N 4007, C 66047-Z1306-A7 | 1 | 1 | -V28971 | +S | |
| 0954899 | LIGHT EMITTING DIODE | 17.180.5A 5MM | 1 | 1 | -V42081 | +M | |
| 0954899 | LIGHT EMITTING DIODE | 17.180.5A 5MM | 1 | 1 | -V42281 | +M | |
| 0920083 | CABLE | OELFLEX 7X1 SY | 2,400 | 4 | -W10011 | +M | Ölflex SY 7x1 mm²; qty printed with comma, MU 4 |
| 0922821 | VALVE PLUG MSUD-24953 | - | 1 | 1 | -W10161 | +SM | Ventilstecker |
| 0922821 | VALVE PLUG MSUD-24953 | - | 1 | 1 | -W10181 | +SM | |
| 0954992 | CABLE | W12621 | 1 | 1 | -W12621 | +M | |
| 0954993 | CABLE | W12631 | 1 | 1 | -W12631 | +M | |

### P 12 (PDF page 10)

| Ident-No | Designation | Type / order data | Qty | MU | Tag | Loc. | Notes |
|---|---|---|---|---|---|---|---|
| 0954995 | CABLE | W12821 | 1 | 1 | -W12821 | +M | |
| 0954996 | CABLE | W12831 | 1 | 1 | -W12831 | +M | |
| 0954997 | CABLE | W12881 | 1 | 1 | -W12881 | +M | |
| 0922821 | VALVE PLUG MSUD-24953 | - | 1 | 1 | -W15141 | +SM | |
| 0922821 | VALVE PLUG MSUD-24953 | - | 1 | 1 | -W15151 | +SM | |
| 0922821 | VALVE PLUG MSUD-24953 | - | 1 | 1 | -W15341 | +SM | |
| 0922821 | VALVE PLUG MSUD-24953 | - | 1 | 1 | -W15351 | +SM | |
| 0138843 | OIL FLEX NUMBER CABLE | 4X1 N=0-3 | 3.500 | 3 | -W15411 | +SM | numbered-core Ölflex |
| 0138844 | OIL FLEX NUMBER CABLE | 5X1 N=0-4 | 3.100 | 3 | -W15921 | +M | |
| 0138844 | OIL FLEX NUMBER CABLE | 5X1 N=0-4 | 3.600 | 3 | -W15922 | +M | |
| 0922821 | VALVE PLUG MSUD-24953 | - | 1 | 1 | -W15971 | +S | |
| 0922821 | VALVE PLUG MSUD-24953 | - | 1 | 1 | -W15981 | (none) | no location printed |
| 0456416 | CABLE | 2X1,0+4X2X0,25 | 2.400 | 3 | -W17041 | +S | Indramat-type feedback cable cross-section [interpretation] |
| 0456416 | CABLE | 2X1,0+4X2X0,25 | 3.600 | 3 | -W17071 | + S | printed "+ S" with a space |
| 0456416 | CABLE | 2X1,0+4X2X0,25 | 2.400 | 3 | -W17071 | +S | same tag twice, as printed |
| 0456416 | CABLE | 2X1,0+4X2X0,25 | 3.600 | 3 | -W17421 | +S | |

### P 13 (PDF page 11)

| Ident-No | Designation | Type / order data | Qty | MU | Tag | Loc. | Notes |
|---|---|---|---|---|---|---|---|
| 0938506 | CABLE | 4X2,5+2(2X0,75), NR.0036250 | 5.100 | 3 | -W19011 | +SM | motor power cable with 2 pairs (brake / thermal contact) [interpretation] |
| 0456416 | CABLE | 2X1,0+4X2X0,25 | 7.000 | 3 | -W19041 | +SM | motor feedback cable [interpretation] |
| 0251643 | SHIELDED CABLE | 6X0,34 | 4.200 | 3 | -W19051 | +S | |
| 0456416 | CABLE | 2X1,0+4X2X0,25 | 4.200 | 3 | -W19061 | +S | |
| 0938506 | CABLE | 4X2,5+2(2X0,75), NR.0036250 | 3.200 | 3 | -W21011 | +SM | |
| 0456416 | CABLE | 2X1,0+4X2X0,25 | 7.900 | 3 | -W21041 | +SM | |
| 0251643 | SHIELDED CABLE | 6X0,34 | 4.800 | 3 | -W21051 | +S | |
| 0456416 | CABLE | 2X1,0+4X2X0,25 | 4.800 | 3 | -W21061 | +S | |
| 0456416 | CABLE | 2X1,0+4X2X0,25 | 4.400 | 3 | -W23071 | + S | printed "+ S" |
| 0456416 | CABLE | 2X1,0+4X2X0,25 | 2.500 | 3 | -W23071 | +S | same tag twice, as printed |
| 0456416 | CABLE | 2X1,0+4X2X0,25 | 4.200 | 3 | -W23421 | +S | |
| 0949454 | CABLE | OELFLEX-FD 850, 12X1, NR.0027823 | 7.500 | 3 | -W36011 | +S | |
| 0949454 | CABLE | OELFLEX-FD 850, 12X1, NR.0027823 | 7.800 | 3 | -W37011 | (none) | no location printed |

### P 14 (PDF page 12)

| Ident-No | Designation | Type / order data | Qty | MU | Tag | Loc. | Notes |
|---|---|---|---|---|---|---|---|
| 0949651 | PLUG | BKS-S19-3-PU-15 WITH CABLE 15M | 1 | 1 | -W42011 | +M | Balluff connector cable |
| 0901935 | CABLE | LIYCY 2X0,5 | 10 | 3 | -W42051 | +M | |
| 0901935 | CABLE | LIYCY 2X0,5 | 10 | 4 | -W42081 | +M | MU 4 as printed |
| 0949651 | PLUG | BKS-S19-3-PU-15 WITH CABLE 15M | 1 | 1 | -W42211 | +M | |
| 0901935 | CABLE | LIYCY 2X0,5 | 10 | 3 | -W42251 | +M | |
| 0901935 | CABLE | LIYCY 2X0,5 | 10 | 3 | -W42281 | +M | |
| 0954791 | PLUG | CA06COM-PG-22-19-S-B, 14POL., FA.ITT-CANNON | 1 | 1 | -X 3 | +A36011 | 14-pole |
| 0954791 | PLUG | CA06COM-PG-22-19-S-B, 14POL., FA.ITT-CANNON | 1 | 1 | -X 3 | +A37011 | |
| 0137340 | PLUG INSERT | 09 33 006 2602 HAN6E | 1 | 1 | -X 4811 | +M | Harting Han 6E; tag printed "-X 4811" with a gap |
| 0245918 | PLUG CONNECTOR | 25-POLE DB-25S SOCKET | 1 | 1 | -X 5 | +VAP4.1 | VAP4 X5 = encoder inputs 3 & 4 (chapter 7) |
| 0921499 | SUBMINIATUR-HOUSING | 17-1589-06, 25-POLAR | 1 | 1 | -X 5 | +VAP4.1 | |
| 0245918 | PLUG CONNECTOR | 25-POLE DB-25S SOCKET | 1 | 1 | -X 5 | +VAP4.2 | |
| 0921499 | SUBMINIATUR-HOUSING | 17-1589-06, 25-POLAR | 1 | 1 | -X 5 | +VAP4.2 | |

### P 15 (PDF page 13)

| Ident-No | Designation | Type / order data | Qty | MU | Tag | Loc. | Notes |
|---|---|---|---|---|---|---|---|
| 0954331 | HOUSING | DA115339-1 15 POLES | 1 | 1 | -X 6 | +A17011 | |
| 0954331 | HOUSING | DA115339-1 15 POLES | 1 | 1 | -X 6 | +A23011 | |
| 0245918 | PLUG CONNECTOR | 25-POLE DB-25S SOCKET | 1 | 1 | -X1 | +VAP4.3 | VAP4 X1 = analog I/O 1 & 2 (set-points) |
| 0921499 | SUBMINIATUR-HOUSING | 17-1589-06, 25-POLAR | 1 | 1 | -X1 | +VAP4.3 | |
| 0150842 | CRIMP CONTACT PIN | 09 33 000 6105 HAN E 0,75-1,0 QMM, FA.HARTING | 1 | 1 | -X10011 | +M | |
| 0181051 | BUSHING CASE | 09 30 006 1540 HAN6E | 1 | 1 | -X10011 | +M | |
| 0901849 | PLUG CONNECTOR | 15-POLE DA-15S | 1 | 1 | -X14 | +A19011 | on the DDS drive |
| 0921498 | HOUSING | 17-1588-06 15 POLES | 1 | 1 | -X14 | +A19011 | |
| 0901849 | PLUG CONNECTOR | 15-POLE DA-15S | 1 | 1 | -X14 | +A21011 | |
| 0921498 | HOUSING | 17-1588-06 15 POLES | 1 | 1 | -X14 | +A21011 | |
| 0012443 | BUSHING CASE | 09 30 024 1540 HAN24E | 1 | 1 | -X14011 | +S | |
| 0012730 | ATTACHMENT HOUSING | 09 30 024 0304 HAN24E | 1 | 1 | -X14011 | +S | |
| 0138857 | PLUG INSERT | 09 33 024 2602 HAN24E | 1 | 1 | -X14011 | +S | |

### P 16 (PDF page 14)

| Ident-No | Designation | Type / order data | Qty | MU | Tag | Loc. | Notes |
|---|---|---|---|---|---|---|---|
| 0150842 | CRIMP CONTACT PIN | 09 33 000 6105 HAN E 0,75-1,0 QMM, FA.HARTING | 24 | 1 | -X14011 | +S | |
| 0919379 | SOCKET INSERT | DSTV-K-B24, NR.:12904.0 | 1 | 1 | -X14011 | +S | |
| 0442262 | MOTOR CONNECTOR | IN108 | 1 | 1 | -X19011 | +M | Indramat motor power connector |
| 0949538 | PLUG | IN510/L 12 POLES | 1 | 1 | -X19041 | +M | Indramat feedback connector |
| 0949538 | PLUG | IN510/L 12 POLES | 1 | 1 | -X19041 | +M19011 | on motor -M19011 |
| 0921498 | HOUSING | 17-1588-06 15 POLES | 1 | 1 | -X2 | +A17041 | |
| 0901849 | PLUG CONNECTOR | 15-POLE DA-15S | 1 | 1 | -X2 | +A23041 | |
| 0921498 | HOUSING | 17-1588-06 15 POLES | 1 | 1 | -X2 | +A23041 | |
| 0901849 | PLUG CONNECTOR | 15-POLE DA-15S | 1 | 1 | -X2 | +A23042 | |
| 0921498 | HOUSING | 17-1588-06 15 POLES | 1 | 1 | -X2 | +A23042 | last digit partly hidden by a punch hole, "+A23042" [uncertain] |
| 0442262 | MOTOR CONNECTOR | IN108 | 1 | 1 | -X21011 | +M | |
| 0949538 | PLUG | IN510/L 12 POLES | 1 | 1 | -X21041 | +M | |
| 0949538 | PLUG | IN510/L 12 POLES | 1 | 1 | -X21041 | +M21011 | on motor -M21011 |

### P 17 (PDF page 15)

| Ident-No | Designation | Type / order data | Qty | MU | Tag | Loc. | Notes |
|---|---|---|---|---|---|---|---|
| 0245918 | PLUG CONNECTOR | 25-POLE DB-25S SOCKET | 1 | 1 | -X3 | +VAP4.3 | VAP4 X3 = encoder inputs 1 & 2 |
| 0921499 | SUBMINIATUR-HOUSING | 17-1589-06, 25-POLAR | 1 | 1 | -X3 | +VAP4.3 | |
| 0901849 | PLUG CONNECTOR | 15-POLE DA-15S | 1 | 1 | -X4 | +A17041 | |
| 0921498 | HOUSING | 17-1588-06 15 POLES | 1 | 1 | -X4 | +A17041 | |
| 0901848 | PLUG CONNECTOR 15-POLE | DA-15P | 1 | 1 | -X4 | +A19011 | male (P), on DDS |
| 0921498 | HOUSING | 17-1588-06 15 POLES | 1 | 1 | -X4 | +A19011 | |
| 0901848 | PLUG CONNECTOR 15-POLE | DA-15P | 1 | 1 | -X4 | +A21011 | |
| 0921498 | HOUSING | 17-1588-06 15 POLES | 1 | 1 | -X4 | +A21011 | |
| 0901849 | PLUG CONNECTOR | 15-POLE DA-15S | 1 | 1 | -X4 | +A23041 | |
| 0921498 | HOUSING | 17-1588-06 15 POLES | 1 | 1 | -X4 | +A23041 | |
| 0950182 | PLUG SOCKET | SERIE C91M IP65 2 POLES, NR.: C 091 31 E 002 2001 | 1 | 1 | -X42051 | +M | Binder/Amphenol C091 style [interpretation] |
| 0950183 | PLUG | SERIE C91M IP65 2 POLES, NR.: C091 31 M 002 2001 | 1 | 1 | -X42051 | +M | |
| 0950182 | PLUG SOCKET | SERIE C91M IP65 2 POLES, NR.: C 091 31 E 002 2001 | 1 | 1 | -X42061 | +M | |

### P 18 (PDF page 16)

| Ident-No | Designation | Type / order data | Qty | MU | Tag | Loc. | Notes |
|---|---|---|---|---|---|---|---|
| 0950183 | PLUG | SERIE C91M IP65 2 POLES, NR.: C091 31 M 002 2001 | 1 | 1 | -X42061 | +M | |
| 0950183 | PLUG | SERIE C91M IP65 2 POLES, NR.: C091 31 M 002 2001 | 2 | 1 | -X42241 | +M | qty 2 |
| 0950182 | PLUG SOCKET | SERIE C91M IP65 2 POLES, NR.: C 091 31 E 002 2001 | 1 | 1 | -X42251 | +M | |
| 0950182 | PLUG SOCKET | SERIE C91M IP65 2 POLES, NR.: C 091 31 E 002 2001 | 1 | 1 | -X42261 | +M | |
| 0245918 | PLUG CONNECTOR | 25-POLE DB-25S SOCKET | 1 | 1 | -X5 | +VAP4.1 | repeats the P 14 entry, as printed |
| 0921499 | SUBMINIATUR-HOUSING | 17-1589-06, 25-POLAR | 1 | 1 | -X5 | +VAP4.1 | |
| 0245918 | PLUG CONNECTOR | 25-POLE DB-25S SOCKET | 1 | 1 | -X5 | +VAP4.2 | |
| 0921499 | SUBMINIATUR-HOUSING | 17-1589-06, 25-POLAR | 1 | 1 | -X5 | +VAP4.2 | |
| 0442838 | LOCKING BOW FOR COVER | 115339 259-8760-000 | 2 | 1 | -X6 | +A17011 | last digit partly under a punch hole [uncertain] |
| 0901849 | PLUG CONNECTOR | 15-POLE DA-15S | 1 | 1 | -X6 | +A17011 | |
| 0954331 | HOUSING | DA115339-1 15 POLES | 1 | 1 | -X6 | +A17011 | |
| 0901849 | PLUG CONNECTOR | 15-POLE DA-15S | 1 | 1 | -X6 | +A23011 | |
| 0954331 | HOUSING | DA115339-1 15 POLES | 1 | 1 | -X6 | +A23011 | |

### P 19 (PDF page 17)

| Ident-No | Designation | Type / order data | Qty | MU | Tag | Loc. | Notes |
|---|---|---|---|---|---|---|---|
| 0907486 | MOTOR INTERFERENCE | HRC 3/047-500 | 1 | 1 | -Z10031 | +S | motor RC suppressor (Motor-Entstörglied) |
| 0921954 | INTERFERENCING BLOCK | LG-Z-3TF, UB=24V DC, NR.26034 | 1 | 1 | -Z10061 | +S | coil suppressor for 3TF contactor |
| 0921954 | INTERFERENCING BLOCK | LG-Z-3TF, UB=24V DC, NR.26034 | 1 | 1 | -Z10071 | +S | |
| 0907486 | MOTOR INTERFERENCE | HRC 3/047-500 | 1 | 1 | -Z11011 | +S | |
| 0921954 | INTERFERENCING BLOCK | LG-Z-3TF, UB=24V DC, NR.26034 | 1 | 1 | -Z11041 | +S | |
| 0921954 | INTERFERENCING BLOCK | LG-Z-3TF, UB=24V DC, NR.26034 | 1 | 1 | -Z14041 | +S | |
| 0907486 | MOTOR INTERFERENCE | HRC 3/047-500 | 1 | 1 | -Z15411 | +S | |
| 0921954 | INTERFERENCING BLOCK | LG-Z-3TF, UB=24V DC, NR.26034 | 1 | 1 | -Z15431 | +S | |
| 0921954 | INTERFERENCING BLOCK | LG-Z-3TF, UB=24V DC, NR.26034 | 1 | 1 | -Z15451 | +S | |
| 0921954 | INTERFERENCING BLOCK | LG-Z-3TF, UB=24V DC, NR.26034 | 1 | 1 | -Z15941 | +S | |
| 0921954 | INTERFERENCING BLOCK | LG-Z-3TF, UB=24V DC, NR.26034 | 1 | 1 | -Z15951 | +S | |

### P 20 (PDF page 18)

| Ident-No | Designation | Type / order data | Qty | MU | Tag | Loc. | Notes |
|---|---|---|---|---|---|---|---|
| 0907486 | MOTOR INTERFERENCE | HRC 3/047-500 | 1 | 1 | -Z36011 | +S | |
| 0907486 | MOTOR INTERFERENCE | HRC 3/047-500 | 1 | 1 | -Z36031 | +S | |
| 0921954 | INTERFERENCING BLOCK | LG-Z-3TF, UB=24V DC, NR.26034 | 1 | 1 | -Z36221 | +S | |
| 0921954 | INTERFERENCING BLOCK | LG-Z-3TF, UB=24V DC, NR.26034 | 1 | 1 | -Z36231 | +S | |
| 0921954 | INTERFERENCING BLOCK | LG-Z-3TF, UB=24V DC, NR.26034 | 1 | 1 | -Z36241 | +S | |
| 0921954 | INTERFERENCING BLOCK | LG-Z-3TF, UB=24V DC, NR.26034 | 1 | 1 | -Z36251 | +S | |
| 0921954 | INTERFERENCING BLOCK | LG-Z-3TF, UB=24V DC, NR.26034 | 1 | 1 | -Z36261 | +S | |
| 0907486 | MOTOR INTERFERENCE | HRC 3/047-500 | 1 | 1 | -Z37011 | +S | |
| 0907486 | MOTOR INTERFERENCE | HRC 3/047-500 | 1 | 1 | -Z37031 | +S | |
| 0921954 | INTERFERENCING BLOCK | LG-Z-3TF, UB=24V DC, NR.26034 | 1 | 1 | -Z37221 | +S | |
| 0921954 | INTERFERENCING BLOCK | LG-Z-3TF, UB=24V DC, NR.26034 | 1 | 1 | -Z37231 | +S | |

### P 21 (PDF page 19)

| Ident-No | Designation | Type / order data | Qty | MU | Tag | Loc. | Notes |
|---|---|---|---|---|---|---|---|
| 0921954 | INTERFERENCING BLOCK | LG-Z-3TF, UB=24V DC, NR.26034 | 1 | 1 | -Z37241 | +S | |
| 0921954 | INTERFERENCING BLOCK | LG-Z-3TF, UB=24V DC, NR.26034 | 1 | 1 | -Z37251 | +S | |
| 0921954 | INTERFERENCING BLOCK | LG-Z-3TF, UB=24V DC, NR.26034 | 1 | 1 | -Z37261 | +S | |

"** END OF ELECTRICAL EQUIPMENT LIST **"

## For the retrofit

### What this list is (and is not)

It is a short list (about 250 rows) and clearly **not the complete bill of materials of the machine**: it has no main transformer (-T), no power supplies (-G), no Indramat supply module (KDV/TVD), no feed-axis drives, no ELTROPILOT modules, and no NC operator panel. Even allowing for the two missing pages (P 5/P 6, which by tag order can only hold -K37.../-K4..., -M, -P, -Q, -R and low -S tags), there is no room in the sort order for -G or -T items, and the -A group (P 1 -> P 2) is continuous. Together with "CUSTOMER CE" and the tag ranges (sheets 100-160, 170-230, 270-290, 360-370, 420-422), it looks like a **list of the order-specific / option and CE-conformity equipment**, while the base machine (drives, NC) is documented in the other chapters [interpretation]. Check chapters 7 and the drive/wiring chapters before concluding anything is absent from the machine.

### Drives and drive accessories

| Tag | Type code | Notes |
|---|---|---|
| -A19011 | **Indramat DDS 2.1-W-050-DA01-00** "AC-MODUL-CONTROLLER" (Ident 0453922) | + Mini-Combicon MC 1,5/10-ST-3,81 10-pole plug, + accessory kit **E3-DDS** |
| -A21011 | **Indramat DDS 2.1-W-050-DA01-00** (same) | same accessories |
| on DDS | -X14 (DA-15S, 15-pole female), -X4 (DA-15P, 15-pole male) | D-sub connectors on the DDS front: by DDS 2.1 convention one is the command/interface card connector, the other the incremental-encoder emulation output [interpretation, not printed] |

No DDS plug-in modules (DSS/ANA/DAE/DLF/DSF/...), no TDM/KDS/KDV/TVD units and no RAC spindle drive appear by name in the transcribed pages. The DDS type code "DDS 2.1-W-050-D..." means: DDS series 2.1, "W" cooling/mounting variant, **050 = 50 A peak current**, "D" = digital servo feedback (motor-integrated high-resolution feedback), A01-00 = configuration/version [interpretation from Indramat DIAX02 naming]. The DDS 2.1 is a DIAX02 digital drive that needs configuration plug-in cards; which command interface is fitted (analog ANA, SERCOS DSS, ...) is **not stated here** and must be read off the units or the drive chapter.

### Motors and feedback (only what the connectors/cables show)

| Item | Evidence | Notes |
|---|---|---|
| -M19011 | -X19011 "MOTOR CONNECTOR IN108" (+M), -X19041 "IN510/L 12 POLES" (+M and +M19011); cables -W19011 4X2,5+2(2X0,75) 5.1 m, -W19041 2X1,0+4X2X0,25 7.0 m | Indramat servo motor (MDD/MKD family by connector type) powered by -A19011 [interpretation]; motor type not printed (would be on missing P 5/P 6 if listed) |
| -M21011 | -X21011 IN108, -X21041 IN510/L; cables -W21011 3.2 m, -W21041 7.9 m | same, powered by -A21011 |
| -A17041, -A23041 | "TRANSDUCER FOR ENCODER SIGNALS TYP HGV 1.1", connectors -X2/-X4 (DA-15) and -X2 +A23042 | encoder signal converters/splitters; cables 2X1,0+4X2X0,25 (-W17041/-W17071/-W17421, -W23071/-W23421) |
| -A17011, -A23011 | only connector -X6 (DA115339-1 15-pole housing) listed | device itself not in the list; likely encoders of sheet 170/230 [uncertain] |
| -W19051, -W21051 | SHIELDED CABLE 6X0,34, 4.2 m / 4.8 m, +S | cabinet-internal shielded cable, probably DDS <-> control set-point/enable [interpretation] |
| -W19061, -W21061 | 2X1,0+4X2X0,25, +S | cabinet-internal, probably DDS encoder-emulation -> HGV/VAP4 [interpretation] |

### Control (ELTROPILOT / Eltromatic)

No ELTROPILOT module is listed as a part, but connectors on three **ELTROPILOT EP90 VAP4** axis-periphery cards are: -X5 +VAP4.1, -X5 +VAP4.2 (encoder inputs 3 & 4), -X1 +VAP4.3 (analog I/O 1 & 2 = set-point outputs), -X3 +VAP4.3 (encoder inputs 1 & 2) - all 25-pole DB-25S with 17-1589-06 housings. Chapter 7 shows that VAP4 cards output **analog set-points** and read **incremental encoders** ("Generator"). So the drives in this list are tied to the NC through VAP4 analog/encoder channels, not a fieldbus.

### Safety, contactors, other cabinet parts

- Safety: -A12641 Pilz **P2HZ/5** 24 V DC; -A15911 E-stop relay **SRBF-1102** (Nr. 640 0002); -A15011 inching module **HM 4**; -A27911/-A28911 "MONITORING SYSTEM" (0680311, type not printed); -S12651 mushroom pushbutton 40 mm; -S14281 key-operated selector (Telemecanique ZB2-BG4/ZB2-BZ105).
- Contactors: Siemens **3TF2222-0BB4** (24 V DC) -K10061, -K10071, -K11041, -K14041, -K15431, -K15451, -K36221...-K36261, -K37221...-K37261; aux. contactors **3TH2262-0BB4** -K15941, -K15951; aux. relays **IK 8802.12/009** (-K08771, -K08781, -K10071, -K10121, -K10141, -K14121...-K14151, -K27981, -K28981, -K36271/81, -K37271/81); time relay **IL 7824.21 0,1-1S** -K15461. Suppressors LG-Z-3TF 24 V DC and motor RC suppressors HRC 3/047-500 (-Z10031, -Z11011, -Z15411, -Z36011/31, -Z37011/31).
- Motor protection: **BA9039/100 DC24V** thermistor relays -F36041, -F37041; power optocouplers **AMS 10-43/5** -A36061, -A37061 (53 V DC / 4 A); 14-pole ITT-Cannon CA06COM connectors on -A36011/-A37011 and Ölflex FD 850 12x1 cables -W36011/-W37011 (sheets 360/370: two small motor groups, e.g. turret or conveyor drives [interpretation]).
- Probing: TS 20 probe -A42071 / -S42271 with output optimizers WMG17 -A42031/-A42231 and proximity switches -S42011/-S42211.
- Transformers / power supplies: **none in this list**.

### Consequences for keeping the original drives under LinuxCNC

- The control side is ELTROPILOT EP90 with VAP4 cards, i.e. **analog ±10 V set-point + incremental encoder** feedback to the NC (chapter 7, confirmed here by the -X1/-X3/-X5 +VAP4.x connector entries). If the DDS 2.1 units carry an analog command card (ANA type), LinuxCNC can drive them the same way: ±10 V DAC output + enable per axis, and read position from the DDS incremental-encoder emulation (or the HGV 1.1 transducer outputs) into encoder counter inputs (e.g. Mesa 7i77 class hardware). Velocity loop stays in the DDS, position loop moves to LinuxCNC.
- Confirm on the hardware: the plug-in cards in -A19011/-A21011 (command interface: analog vs SERCOS DSS; feedback card), the DDS firmware/parameter module, and what the HGV 1.1 converts (resolution, signal levels; 5 V differential vs 11 µA/1 Vpp). Back up the DDS parameters before any change.
- The 50 A DDS units with IN108/IN510 motors are the larger servo axes or spindles of this order; the other drives of the machine (feed axes, main spindles, driven tools, Indramat supply module) are not in this list and must be inventoried from the cabinet and the other chapters.
- Contactor/safety logic (Pilz P2HZ, SRBF E-stop relay, HM 4 inching module, 24 V DC contactors) is conventional relay logic and can be retained and interfaced to LinuxCNC I/O (enable chain, E-stop status).
