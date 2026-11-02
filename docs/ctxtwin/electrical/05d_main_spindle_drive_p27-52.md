# Gildemeister CTX twin, electrical documentation: 5 Main spindle drive (part d), PDF pages 27-52

Source: `08cabf3f-5_Main_spindel_drive_d_20201028_0001.pdf` (scan dated 2020-10-28), pages 27-52 of 52.
Content: the last part of an Indramat manual for main spindle drives RAC / KDA / TDA (manual pages 141-161:
chapter 6 "Interfaces for the NC controller and the feedback" and chapter 7 "Abbreviations and agreements,
type codes"), the Indramat back cover, and one machine-specific sheet "CTX-TWIN HSP parameters dd. 24.03.1994 /
PARAMETER LIST KDA 3.2" (document no. 9.559.035.4-00/065-000.0).
The manual pages carry no Gildemeister title block / drawing number; the page number printed on the page is
used as the sheet number. Original language: **English** (Indramat English edition); only small figure tags
are German (given with translation). Pages 1-26 are transcribed in a separate file.

| PDF page | Manual page / sheet | Content |
|---|---|---|
| 27 | 141 | Chapter 6 "Interfaces for the NC controller and the feedback", list of sections 6.1-6.9 |
| 28 | 142 | 6.1 Control inputs, signal outputs, analogue outputs, Bb contact: Fig. 105 function overview of X2 (+ Bb contact X15/X11) |
| 29 | 143 | Fig. 106 X2 hardware (opto-isolated inputs/outputs, supply, analogue outputs, Bb contact) |
| 30 | 144 | 6.2 Analogue speed set-point (standard), Fig. 107, inputs E1/E2, E3/E4 on X4 |
| 31 | 145 | 6.3 Digital speed set-point input (optional), Fig. 108, X4 16-bit parallel |
| 32 | 146 | 6.4 SERCOS interface (optional), Fig. 109 |
| 33 | 147 | 6.5 Digital position input (optional), Fig. 110, X6 (KDA/TDA) / X13 (RAC) |
| 34 | 148 | 6.6 Incremental encoder output (optional), Fig. 111, X6/X13 + X17 reference contact |
| 35 | 149 | 6.7 Serial interface RS 232 C (optional), Fig. 112, X6/X13 |
| 36 | 150 | 6.8 Motor feedback connection, Fig. 113, X3 |
| 37 | 151 | 6.9 Spindle feedback connection (optional), Fig. 114, X5 |
| 38 | 152 | blank (header "Interfaces" only) |
| 39 | 153 | Chapter 7 "Abbreviations and agreements, type codes" (title page) |
| 40 | 154 | Abbreviations RAC ... A1, A2, A3 |
| 41 | 155 | Abbreviations / symbols 0, 1, RSK, CDR, ADW, PDS, PDA, < > |
| 42 | 156 | Fig. 115 KDA 3 type code |
| 43 | 157 | Fig. 116 TDA 1 type code |
| 44 | 158 | Fig. 117 RAC 2 type code |
| 45 | 159 | Fig. 118 RAC 3 type code |
| 46 | 160 | Fig. 119 RAC 4 type code |
| 47 | 161 | Fig. 120 AS (programming module) type code |
| 48 | - | blank |
| 49 | - | Indramat back cover ("Presented by:", address) |
| 50 | - | blank |
| 51 | - | **CTX-TWIN HSP parameters dd. 24.03.1994, PARAMETER LIST KDA 3.2** (machine-specific) |
| 52 | - | blank (faint show-through of page 51) |

---

## PDF page 27 (manual p. 141): chapter 6 contents

Header: "Interfaces".

**6. Interfaces for the NC controller and the feedback**

- 6.1 Control inputs, signal outputs and analogue outputs, Bb contact
- 6.2 Analogue speed set-point (standard)
- 6.3 Digital speed set-point input (optional)
- 6.4 SERCOS interface (optional)
- 6.5 Digital position input (optional)
- 6.6 Incremental encoder interface (optional)
- 6.7 Serial interface (optional)
- 6.8 Motor feedback connection
- 6.9 Spindle feedback connection (optional)

## PDF page 28 (manual p. 142): 6.1 Control inputs, signal outputs and analogue outputs, Bb contact

**Fig. 105: Overview of the control input, signal output and analogue output functions** (figure tag
"Funktionsübersicht" (function overview)). Right side: terminal strip of the "Control unit".

### X2 Control inputs (signal convention: 1 = +24 V, 0 = 0 V, see p. 155)

| X2 terminal | Name | Function (as printed) |
|---|---|---|
| 23 | E-STOP (RAC only) | 0 = EMERG STOP disconnect (RAC) |
| 24 | RF (Controller enabl. sign.) | 0 = Controller lockout |
| 25 | RUN | 0 = Set-point lockout f. positioning |
| 26 | POS 1 | Select a position (with 27) |
| 27 | POS 2 | Select a position (with 26) |
| 28 | MD-RED | 0 = No torque limiting [as printed] |
| 29 | OSCILLATE | 1 = Oscillate speed |
| 30 | PAR 1 | Activate a parameter record (with 31) |
| 31 | PAR 2 | Activate a parameter record (with 30) |
| 32 | LIMIT 1 | Select a limiting speed (with 33, 34) |
| 33 | LIMIT 2 | Select a limiting speed |
| 34 | LIMIT 4 | Select a limiting speed |
| 35 | SPEED 1 | Acknowledge speed/motor 1/2 (with 36) |
| 36 | SPEED 2 | Acknowledge speed/motor 1/2 |
| 37 | EXT POS | 1 = Change input weighting |
| 3 | AS (starting lockout - KDA/TDA only) | 0 = Starting lockout KDA/TDA |

Table "Control inputs POS 1 / POS 2 -> Selected spindle position -> Parameter":

| POS 1 | POS 2 | Selected spindle position | Parameter |
|---|---|---|---|
| 0 | 0 | No position | - |
| 1 | 0 | Position 1 | A08 |
| 0 | 1 | Position 2 | A09 |
| 1 | 1 | Position 3 | A10 |

Table "Control inputs PAR 1 / PAR 2 -> Active parameter record":

| PAR 1 | PAR 2 | Active parameter record |
|---|---|---|
| 0 | 0 | P |
| 1 | 0 | Q |
| 0 | 1 | R |
| 1 | 1 | S |

Table "Control inputs LIMIT 1 / LIMIT 2 / LIMIT 4 -> Max. spindle speed -> Stored in parameter":

| LIMIT 1 | LIMIT 2 | LIMIT 4 | Max. spindle speed | Stored in parameter |
|---|---|---|---|---|
| 0 | 0 | 0 | None | -------- |
| 1 | 0 | 0 | LIMIT 1 | A12 |
| 0 | 1 | 0 | LIMIT 2 | A13 |
| 1 | 1 | 0 | LIMIT 3 | A14 |
| 0 | 0 | 1 | LIMIT 4 | A15 |
| 1 | 0 | 1 | LIMIT 5 | A16 |
| 0 | 1 | 1 | LIMIT 6 | A17 |
| 1 | 1 | 1 | LIMIT 7 | A18 |

(Parameter numbers printed in small boxes; second digit partly blurred, read as A12-A18 [uncertain only in
the scan quality, the sequence is consistent with "Limit 1-7" = A12-A18 on p. 51].)

Table "Control inputs SPEED 1 / SPEED 2 -> Acknowledgem. from gear-box (motor)":

| SPEED 1 | SPEED 2 | Acknowledgement from gearbox (motor) |
|---|---|---|
| 0 | 1 | Speed 2 (motor 2) engaged |
| 1 | 0 | Speed 1 (motor 1) engaged |
| 0,1 | 0,1 | Fault message GEARBOX (display) |

### X2 Signal outputs

| X2 terminal | Name | Meaning (as printed) |
|---|---|---|
| 4 | READY | 1 = Drive ready |
| 5 | N = CMD | 1 = N_act = N_set-point |
| 6 | N < MIN | 1 = N < N_min |
| 7 | IN POS | 1 = In position |
| 8 | 90% LOAD | 0 = Drive loaded at 90% |
| 9 | TEMP WARN | 0 = Temperature prewarning |
| 10 | LIMIT | 0 = Speed limit exceeded |
| 11 | SPEED CHANGE 1 | Change speed / motor control (with 12) |
| 12 | SPEED CHANGE 2 | Change speed / motor control |
| 13 | LOAD LIMIT | 0 = Load threshold reached |
| 14 | RF-SLAVE | 1 = Controller enable - slave drive |
| 15 | SPINDEL CONTR | 0 = Spindle not turning |

Table "Signal outputs SPEED CHANGE 1 / SPEED CHANGE 2 -> Effect":

| SPEED CHANGE 1 | SPEED CHANGE 2 | Effect |
|---|---|---|
| 0 | 1 | Speed 2 is engaged or motor 2 is switched on |
| 1 | 0 | Speed 1 is engaged or motor 1 is switched on |

### X2 Analogue outputs

| X2 terminal | Name | Meaning |
|---|---|---|
| 22 | OVM | Controller int. 0 V |
| 21 | N | Analogue output N (±10 V) (speed) |
| 20 | M | Analogue output M (+10 V) (torque) |

### X2 Supply voltage

| X2 terminal | Name | Connection |
|---|---|---|
| 17 | Screen | - |
| 2 | 0V internal | - |
| 18 | +24V internal | - |
| 19 | +24 V external | +24V from controller |
| 1 | 0 V external | 0V from controller |

(dashed link drawn 18-19 and 1-2 as the alternative when no external 24 V is available, see p. 143.)

### Bb contact

"X15 in RAC / X11 in KDA/TDA": terminals 5/1 and 6/2 (RAC number / KDA-TDA number), "Bb-contact",
"Closed, if drive is ready".

## PDF page 29 (manual p. 143): Fig. 106 X2 hardware

**Fig. 106: Control input, signal output and analogue output (X2) hardware** (figure tag "Hardware X2").

Circuit description:
- **Control inputs** (X2:23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 3): each input -> 2 k
  series resistor -> 12 V Zener diode -> optocoupler LED (with antiparallel protection diode); 10 k resistor
  from input to the common "0V ext" line. Opto transistor with 4k7 pull-up -> "To data bus of control unit".
  "Applies to all control inputs". Input voltage level diagram: 0 (low) below +5 V; 1 (high) from +18 V
  to +24 V, maximum +36 V.
- **Signal outputs** (X2:4 ... 15): from data bus of control unit -> optocoupler (LED with 470 R) ->
  transistor stage with 6k81 and 2k21 resistors -> output amplifier -> PTC-type current limiter
  (symbol with "max. 50 mA") -> output terminal; 22k1 from output to 0V ext. Supplied from "+24 V ext".
  "Applies to all signal outputs". Output voltage level diagram: 0 (low) below +5 V, 1 (high) +18 ... +24 V,
  max +36 V. Each output max. 50 mA.
- **Supply**: 17 Screen; 2 0V internal; 18 +24V internal; 19 +24 V external -> 10 µH choke -> diode ->
  "+24 V ext" rail; 1 0 V external -> 10 µH choke -> "0V ext" rail. Diagram "Voltage level for +24 Volt":
  +18 V to +36 V [small print, uncertain]. Note beside the dashed links: "Link only if no +24V from
  controller" [small print, uncertain wording].
- **Analogue outputs**: 22 OVM (common), 21 N (max 5 mA) from a -10 ... +10 V source, 20 M (max 5 mA)
  from a 0 ... +10 V source.
- **Bb contact**: X11 (KDA/TDA) terminals 1, 2 / X15 (RAC) terminals 5, 6: potential-free relay contact,
  "Bb-contact", rating 24 V, 1 A.

Text: "The inputs and outputs are electrically isolated. If no +24 V/0 V are available from the controller
to supply the outputs, link terminals 18 and 19 and terminals 1 and 2."

## PDF page 30 (manual p. 144): 6.2 Analogue speed set-point (standard)

**Fig. 107: Interface for analogue speed set-point** (figure tag "Schnittstelle-analog" (interface analogue)).

Circuit: two differential amplifiers in the control unit.
- Input 1: X4 terminal 2 (E2) -> 20 K -> "+" input; terminal 1 (E1) -> 20 K -> "-" input; 20 K feedback,
  20 K to ground on the other input; potentiometer P1 (offset, wiper to +15V_M) on the amplifier.
- Input 2: X4 terminal 6 (E4) -> 20 K -> "+"; terminal 5 (E3) -> 20 K -> "-"; same network; potentiometer P2.
- A "Parameter-dependent" switch selects which amplifier output is used.
- X4:4 = ground (screen symbol); X4:9 = +15V_M (10 mA); X4:8 = -15V_M (10 mA); X4:7 = ground/0 V.
- Voltage level of analogue inputs: -15 V ... +15 V (linear).

| X4 terminal | Signal |
|---|---|
| 1 | E1 (input 1, "-") |
| 2 | E2 (input 1, "+") |
| 4 | screen / chassis [symbol] |
| 5 | E3 (input 2, "-") |
| 6 | E4 (input 2, "+") |
| 7 | 0 V [symbol, uncertain] |
| 8 | -15 V_M, 10 mA |
| 9 | +15 V_M, 10 mA |

Text: "Analogue inputs E1/E2 and E3/E4 are configured as differential inputs. Potentiometers P1 and P2
compensate drift when 0 V is applied to the respective analogue input. They are located to the right and
left of plug terminal strip X4."
"Identification in the 'speed set-point' type code field is: A"

## PDF page 31 (manual p. 145): 6.3 Digital speed set-point input (optional)

**Fig. 108: Interface for digital speed set-point (optional)** (tag "Schnittstelle-digital").

X4 (25-pin): opto-isolated inputs, same input circuit as X2 (2 k, 12 V Zener, 10 k, optocoupler, 4k7 to
data bus), "Applies to all 16 inputs". Voltage level 0 (low) < +5 V, 1 (high) +18 ... +24 V, max +36 V.

| X4 | Signal |
|---|---|
| 1-14 | Bit 1 ... Bit 14 |
| 15 | Direc. of rotation (1 = anticlockwise rotation, 0 = clockwise rotation) |
| 16 | DATA VALID (1 = accept current N_set-point) |
| 17 | - |
| 18 | -15V int (max. 150 mA) |
| 19 | +15V int (max. 150 mA) |
| 20 | - |
| 21 | 0V ext |
| 22 | 0V ext |
| 23, 24 | - |
| 25 | screen (symbol) |

Text: "The inputs are electrically isolated. If no +24 V/0 V are available from the controller to drive the
inputs, link terminals 18 and 21. Terminal 19 is then 1 (high) and terminal 18 is 0 (low). The controller
must then generate the bit pattern for the position set-point via potential-free contacts."
"Important: If the internal ±15 V is used, the total loading must not exceed 150 mA. A digital position input
above ±15 V is also not possible."

## PDF page 32 (manual p. 146): 6.4 SERCOS interface (optional)

**Fig. 109: SERCOS interface (optional)** (tag "Schnittstelle-SERCOS"). "Location of plug terminal strip X4":
receiver diode IC 1 ("Optical fibre from the controller" -> electrical output); transmitter diode IC 2 with
driver circuit ("Electrical input" -> "Optical fibre to controller or to further control units"); LEDs
"Fault (red)" and "Ready (green)".
"Identification in 'speed set-point' type code field: L"

## PDF page 33 (manual p. 147): 6.5 Digital position input (optional)

**Fig. 110: Interface for digital position set-point (optional)** (tag "Schnittstelle-optional"
[uncertain]). Connector "X6 for KDA/TDA; X13 for RAC"; same opto input circuit, "Applies to all 16 inputs".

| Pin | Weight |
|---|---|
| 1 | 1 x 0,1° |
| 2 | 2 x 0,1° |
| 3 | 4 x 0,1° |
| 4 | 8 x 0,1° |
| 5 | 1 x 1° |
| 6 | 2 x 1° |
| 7 | 4 x 1° |
| 8 | 8 x 1° |
| 9 | 1 x 10° |
| 10 | 2 x 10° |
| 11 | 4 x 10° |
| 12 | 8 x 10° |
| 13 | 1 x 100° |
| 14 | 2 x 100° |
| 15 | Dir. of positioning (0 = pos. along shortest path, 1 = dir. of posit. of current N_set-point) |
| 16 | DATA VALID (1 = acceptance of current set-point) |
| 18 | -15V int (max. 150 mA) |
| 19 | +15V int (max. 150 mA) |
| 21, 22 | 0V ext |
| 25 | screen |
| 17, 20, 23, 24 | - |

(BCD position in 0.1° steps, 0 ... 399.9°.)
Text: same isolation/link 18-21 note as 6.3; "Identification in 'additional interfaces' type code field: D";
"Important: If the internal ±15 V is used, the total loading must not exceed 150 mA. A digital position input
above ±15 V is also not possible."

## PDF page 34 (manual p. 148): 6.6 Incremental encoder output (optional)

**Fig. 111: Interface for incremental encoder output (optional)** (tag "Schnittstelle-Inkre").
Connector "X6 for KDA/TDA, X13 for RAC" (outputs "To controller"):

| Pin | Signal |
|---|---|
| 1 | B-bar (/B) |
| 2 | - |
| 3 | 0 (marker) |
| 4 | 0-bar (/0) |
| 5 | A (max 40 mA) |
| 6 | A-bar (/A) (max 40 mA) |
| 7 | - |
| 8 | B |
| 9 | screen |
| 10 | 0 V (supply from controller) |
| 11 | - |
| 12 | +5 V (input, max 200 mA) supply from controller |
| 13-15 | - |

Output driver: line driver fed via optocoupler "From data bus", "Also valid for B, B-bar and 0, 0-bar",
"To further outputs". Output levels: 0 (low) < +0.5 V, 1 (high) > +1.8 V, up to +5 V. Supply +5 V
tolerance +4.75 ... +5.25 V.

**X17**: pin 1 / pin 2 = "Contact for reference switching signal from the main spindle control unit",
24 V; 0,25 A; pin 3 = screen.

Text: "The control unit supplies incremental encoder-compatible signals to the controller for displacement
measurement. For this option the motor must be fitted with high-resolution feedback (feedback type 3 or 4).
The outputs are electrically isolated. The controller must provide the +5 V supply.
Identification in 'additional interfaces' type code field: I"

## PDF page 35 (manual p. 149): 6.7 Serial interface (optional)

**Fig. 112: RS 232 C serial interface (optional)** (tag "Schnittstelle-seriell").
Connector "X6 for KDA/TDA; X13 for RAC":

| Pin | Signal |
|---|---|
| 1 | GND |
| 2 | TXD (max. 6 mA), output |
| 3 | RXD (8 mA), input |
| 4 | RTS (max. 6 mA), output |
| 5 | CTS (8 mA), input |
| 7 | GND |
| 6, 8-25 | - |

Levels TXD/RTS: 0 (low) -3 ... -15 V, 1 (high) +3 ... +15 V (marks at ±9 V). RXD/CTS: 1 (high) > +2 V,
0 (low) < +1 V, range -9 ... +9 V.
Text: "The serial interface is not potential-free, but can be correctly run from any input device (PC or
similar) with RS 232 C interface. Identification in 'additional interfaces' type code field: S"

## PDF page 36 (manual p. 150): 6.8 Motor feedback connection

**Fig. 113: Interface for motor feedback** (tag "Schnittstelle-Motor"). Connector X3 (15-pin), supply
+5 V (+4.75 ... +5.25 V).

| X3 pin | High-resolution motor feedback (Type 3) | Incremental encoder motor feedback (Type 1) |
|---|---|---|
| 1 | - | B-bar |
| 2 | 0 | - |
| 3 | - | 0 |
| 4 | - | 0-bar |
| 5 | - | A |
| 6 | - | A-bar |
| 7 | A | - |
| 8 | - | B |
| 9 | screen | screen |
| 10 | 0 V | 0 V |
| 11 | 0 ref | - |
| 12 | +5 V | +5 V |
| 13 | B | - |
| 14 | B ref | - |
| 15 | A ref | - |

Text: "Plug terminal strip X3 has pins for high-resolution motor feedback (Type 3) as well as pins for
incremental encoder motor feedback (Type 1)."

## PDF page 37 (manual p. 151): 6.9 Spindle feedback connection (optional)

**Fig. 114: Interface for spindle feedback (optional)** (tag "Schnittstelle-Spindel"). Connector X5 (15-pin).

| X5 pin | INDRAMAT high-resolution spindle feedback | Incremental encoder spindle feedback (ROD 420/426-compatible) |
|---|---|---|
| 1 | - | B-bar |
| 2 | 0 | - |
| 3 | - | 0 |
| 4 | - | 0-bar |
| 5 | - | A |
| 6 | - | A-bar |
| 7 | A | - |
| 8 | - | B |
| 9 | screen | screen |
| 10 | 0 V (-> 400 mA [uncertain]) | 0 V (-> 180 mA) |
| 11 | 0 ref | - |
| 12 | +5 V | +5 V |
| 13 | B | - |
| 14 | B ref | - |
| 15 | A ref | - |

Levels: differential, between A and A-bar, B and B-bar, 0 and 0-bar: 1 (high) > +1 V, 0 (low) < -1 V;
max. input voltage w.r.t. 0 V ±15 V. Supply of spindle feedback +5 V (+4.75 ... +5.25 V).
Text: "The control unit supplies the associated spindle feedback with +5 V. The inputs AA, BB and OO are
differential inputs. Plug terminal strip X5 has pins for the INDRAMAT high-resolution spindle feedback as
well as pins for incremental encoders used as spindle feedback (compatible with ROD 420 or ROD 426).
Only one of the quoted feedback types can be connected!
Identification in '2nd encoder input' type code field: P"

## PDF page 38 (manual p. 152)

Blank page, header "Interfaces".

## PDF page 39 (manual p. 153)

**7. Abbreviations and agreements, type codes** (chapter title page only).

## PDF page 40 (manual p. 154): abbreviations

| Abbreviation | Meaning |
|---|---|
| RAC | Main spindle controller with regeneration facility, for 2AD and 1MB asynchronous motors (see RAC type code) |
| KDA | Modular main spindle controller using cold-running technology, for 2AD and 1MB asynchronous motors (see KDA type code) |
| TDA | Modular main spindle controller using hot-running technology, for 2AD and 1MB asynchronous motors (see TDA type code) |
| AS.. | Programming module for RAC, KDA and TDA controllers (see AS type code) |
| 2AD | Asynchronous motor, separately-ventilated |
| 1MB | Frameless, bolt-on, water-cooled spindle motor |
| Bb | "Ready" contact on the controllers |
| Bb1 | "Ready" contact on the power supply module |
| E1/E2 | First analogue input of main spindle controllers |
| E3/E4 | Second analogue input of main spindle controllers |
| L+ L- | Link circuit voltage terminals (d.c. power) |
| [symbol not printed / unreadable] | Temperature |
| RPM | Speed in rpm |
| IGS | Incremental encoder output interface |
| Master | Master drive (speed-controlled) |
| Slave | Slave drive (torque-controlled) |
| L1, L2, L3 | RAC controller mains input terminals |
| A1, A2, A3 | Motor connections for controllers |

## PDF page 41 (manual p. 155): abbreviations (cont.)

| Symbol | Meaning |
|---|---|
| 0 | low = 0 V |
| 1 | high = +24 V |
| (rising/falling edge symbol) | Step signal |
| (bracket symbol) | Any signal state |
| RSK | PCB in RAC, controls the regenerated power |
| CDR | PCB in the controller, carries the microcomputer controller |
| ADW | Interface for analogue speed set-point |
| PDS | Interface for digital speed set-point |
| PDA | Parameter duplication adapter |
| < > | Summand, part of the total value for function parameters |

## PDF page 42 (manual p. 156): Fig. 115 KDA 3 type code

"Available versions of KDA 3". Example: **KDA 3.2 - 150 - 3 - A00 - W1**

| Field | Options |
|---|---|
| 1. Device abbreviation | KDA |
| 2. Series | 3 |
| 3. Version | 2 |
| 4. Rated current in amps | 050, 100, 150 |
| 5. DC-supply voltage | 300V = 3; 500V = 5 |
| 6. Speed set-point | Analogue = A; Digital 16-bit parallel = D; SERCOS interface = L |
| 7. Second encoder input | No 2nd encoder input = 0; Spindle positioning or two motor change-over = P; 2. and 3. encoder input for spindle synchronisation, in conjunction with analogue set-point and AS 35 only = Y |
| 8. Additional interfaces | None = 0; Digital 16-bit parallel set-point input = D; Serial interface via RS 232 C = S; Incremental encoder output = I |
| 9. AC-supply voltage for internal fan | 220V = W; 115V = U |
| 10. Type of cooling for power section | Forced ventilation by external fan = 1 |

Footer reference: "ENA1A-LiefProg/KDA3/9.563.001.4-00" [small print, uncertain].

## PDF page 43 (manual p. 157): Fig. 116 TDA 1 type code

Example: **TDA 1.1 - 100 - 3 - A00**. Fields: 1. TDA; 2. Series 1; 3. Version 1; 4. Rated current 050, 100;
5. DC-supply voltage 300V = 3; 6. Speed set-point A / D / L (as KDA); 7. Second encoder input 0 / P / Y
("in conjunction with analogue set-point and AS 75 only"); 8. Additional interfaces 0 / D / S / I.
Footer: "ENA7A-LiefProg/TDA1/9.559.042.4-00" [uncertain].

## PDF page 44 (manual p. 158): Fig. 117 RAC 2 type code

Example: **RAC 2.2 - 200 - 380 - A00 - W1**. Fields: 1. RAC; 2. Series 2; 3. Version 2; 4. Rated current
150, 200, 250; 5. AC-supply voltage 3 x 380V/50-60Hz = 380; 3 x 380V/50Hz, 3 x 415V/50Hz, 3 x 460V/60Hz = 460;
6. Speed set-point A / D / L; 7. Second encoder input 0 / P ("Spindle positioning or axis change-over") /
Y ("... in conjunction with analogue set-point and AS 65"); 8. Additional interfaces 0 / D / S / I;
9. Additional bleeder: none = W, with additional bleeder for braking during mains failure = Z;
10. Type of cooling: forced ventilation by internal fan = 1, water cooling = F.
Footer: "ENA1A-LiefProg/RAC22/9.558.043.4-02" [uncertain].

## PDF page 45 (manual p. 159): Fig. 118 RAC 3 type code

Example: **RAC 3.1 - 150 - 460 - A00 - W1 - 220**. Fields: 1. RAC; 2. Series 3; 3. Version 1; 4. Rated
current 100, 150; 5. AC-supply voltage 3x380V - 460V; ±10%; 50 - 60Hz = 460; 6. A / D / L; 7. 0 / P / Y
("... AS 65"); 8. 0 / D / S / I; 9. Additional bleeder W / Z; 10. Type of cooling: forced ventilation by
internal fan = 1; 11. AC-control voltage in Volts = 220.
Footer: "ENA1A-LiefProg/RAC3/9.558.053.4-01" [uncertain].

## PDF page 46 (manual p. 160): Fig. 119 RAC 4 type code

Example: **RAC 4.1 - 400 - 460 - A00 - W1**. Fields: 1. RAC; 2. Series 4; 3. Version 1; 4. Rated current
300, 400; 5. AC-supply 3 x 380V/50Hz, 3 x 415V/50Hz, 3 x 460V/60Hz = 460; 6. A / D / L; 7. 0 / P / Y;
8. 0 / D / S / I; 9. Additional bleeder: none = W; 10. Cooling: internal fan = 1, water cooling = F.

## PDF page 47 (manual p. 161): Fig. 120 AS type code (programming module)

Example: **AS 53 / 204 - 001**

| Field | Options |
|---|---|
| 1. Designation | AS |
| 2. According to controller type | KDA 3.2 = 3; RAC 2.2 = 5; RAC 3.1 = 6; TDA 1.1 = 7; RAC 4.1 = 8 |
| 3. Software codes | Standard main spindle drive = 1; Servo drive (always with incremental encoder output) = 2; Main spindle drive with incremental encoder output = 3; Main spindle drive with SERCOS interface = 4; Main spindle drive with additional functions = 5 |
| 4. According to motor feedback | High resolution motor feedback (standard) = 0 and 1; Incremental encoder ROD 1424 = 2 and 3 |
| 5. Code for motor/controller combination as specified and registered by Indramat | z.B. (e.g.) 04 |
| 6. User specification | Standard = 000 |

Footer: "ENA1A-LiefProg/AS/9.552.255.4-00" [uncertain].

## PDF pages 48, 50, 52

Blank (page 52 shows faint show-through of the parameter table).

## PDF page 49: back cover

"Presented by:" (empty box). INDRAMAT logo. "Indramat GmbH · Bgm.-Dr.-Nebel-Str. 2 · D-8770 Lohr ·
Telefon 0 93 52/40-0 · Tx 6 89 421 · Fax 09352/40-4885". "As of 01.07.1993 our new postcode will be 97816".
"Reproduction forbidden! Subject to revisions! Printed in Federal Republic of Germany".

## PDF page 51: CTX-TWIN HSP parameters dd. 24.03.1994 - PARAMETER LIST KDA 3.2

(HSP = Hauptspindel (main spindle).) Machine-specific sheet, typed.

Header data:
- Comments: Standard with IGS options
- Generator: MH.512-5-...
- N: 0 / 850 / 3200 / 6300 (rpm)
- MD max: 240 Nm
- P ma[x]: 0 KW / 22KW / 22KW / 11KW (power at the speeds above)

Type plate information:
- AS33/065-000 (programming module; per Fig. 120: KDA 3.2, software code 3 = main spindle drive with
  incremental encoder output, motor feedback 0 = high resolution, motor/controller combination 65, standard 000)
- Motor: 1MB241H...-6C
- Contr.: KDA3.2-150-3-L.0-.1 (as printed; per Fig. 115: KDA 3.2, 150 A, 300 V DC bus, field 6 = "L" =
  SERCOS speed set-point, field 7 ".", field 8 "0", field 9 ".", field 10 "1". The "L" is clearly printed but
  conflicts with "Standard with IGS options" and AS software code 3 (not 4 = SERCOS) and with "Cmd Volt" being
  set; the dots look like wildcards. Check the real type plate.)
- Software: MSP1V0.8
- Parameter from: 24.03.1994

Parameter table (columns M = motor parameters, A = general/function parameters, P/Q/R/S = the four parameter
records selected by X2:30 PAR 1 / X2:31 PAR 2):

| No. | M name | M value | A name | A value | P/Q/R/S name | P | Q | R | S |
|---|---|---|---|---|---|---|---|---|---|
| 1 | Poles | 6 | Max Rpm | 6300 | Ramp 1 | 3 | 0 | 0 | 0 |
| 2 | Feedback | 4 | Cmd Volt | 10 | Rpm 1 | 0 | 1 | 1 | 1 |
| 3 | T-Filter | 1 | Min Rpm | 5 | Ramp 2 | 0 | 0 | 0 | 0 |
| 4 | Slip Lim | 7,9 | Max Dev | 50 | Rpm 2 | 0 | 1500 | 1500 | 1500 |
| 5 | Flux | 20 | Funct 1 | 128 | Ramp 3 | 0 | 0 | 0 | 0 |
| 6 | Current | 145 | Funct 2 | 32 | Md-Red | 99 | 99 | 10 | 10 |
| 7 | Signslip | 38 | Offset | 0 | P-Gain 1 | 11 | 1 | 0,5 | 0,5 |
| 8 | Sign-Rpm | 850 | Pos 1 | 0 | I-Gain 1 | 7,7 | 0,8 | 0,4 | 0,4 |
| 9 | I-U-Rpm | 750 | Pos 2 | 120 | Gain-Rpm | 1000 | 1000 | 1500 | 1500 |
| 10 | Signvolt | 160 | Pos 3 | 240 | P-Gain 2 | 0,4 | 0,4 | 0,25 | 0,25 |
| 11 | Beta | 0,4 | Encoder 2 | 4 | I-Gain 2 | 0,04 | 0,04 | 0,02 | 0,02 |
| 12 | Phi | 10 | Limit 1 | 9999 | Gear-In | 1 | 1 | 1 | 1 |
| 13 | Voltfact | 0,6 | Limit 2 | 9999 | Gear-Out | 1 | 1 | 1 | 1 |
| 14 | Slipfact | 0.2 | Limit 3 | 9999 | G-Change | 0 | 0 | 0 | 0 |
| 15 | Motfunct | 99 | Limit 4 | 9999 | P-Maxrpm | 105 | 10 [speck, uncertain] | 10 [speck, uncertain] | 10 |
| 16 | Phifact | 0.1 | Limit 5 | 9999 | PQ-Funct | 0 | 8 | 0 | 0 |
| 17 | Contrtyp | 3,15 | Limit 6 | 9999 | Pos-Gain | 16,6 | 16,6 | 16,6 | 16,6 |
| 18 | | | Limit 7 | 9999 | Pos-Rpm | 150 | 150 | 150 | 150 |
| 19 | | | Baudrate | 9600 | Load-Lim | 10 | 10 | 10 | 10 |
| 20 | Offset 2 | | Osci Rpm | 20 | Alpha | 1 | 1 | 1 | 1 |
| 21 | Poles | | Maxtemp | 155 | P-Window | 0,1 | 0,1 | 0,1 | 0,1 |
| 22 | Feedback | | Funct 3 | 0 | Igsref | 350 | 350 | 350 | 350 |
| 23 | T-Filter | | Encoder 3 | 1024 | Igsopen | 6 | 6 | 6 | 6 |
| 24 | Slip Lim | | | | Igslines | 16000 | 2048 | 1024 | 1024 |
| 25 | Flux | | | | Igsfunct | 16 | 16 | 16 | 16 |
| 26 | Current | | | | Syncmode | | | | [specks] |
| 27 | Signslip | | | | Kv-Sync | | | | |
| 28 | Sign-Rpm | | | | S-Window | | | | |
| 29 | I-U-Rpm | | | | Smooth | | | | |
| 30 | Signvolt | | | | Syncoff | | | | |
| 31 | Beta | | | | | | | | |
| 32 | Phi | | | | | | | | |
| 33 | Voltfact | | | | | | | | |
| 34 | Slipfact | | | | | | | | |
| 35 | Motfunct | | | | | | | | |
| 36 | Phifact | | | | | | | | |
| 37-40 | | | | | | | | | |

(M rows 20-36 are the empty second motor set, "Offset 2 ... Phifact", for a second motor / motor change-over;
all empty. Sync parameters Syncmode ... Syncoff are empty.)

Footer: "Material number for AS 33/065-000: 00253189"; "Document No.: 9.559.035.4-00/065-000.0".

---

## For the retrofit

What these pages establish (facts from the document, interpretation marked):

- **Drive type**: Indramat **KDA 3.2-150-3** modular main spindle controller (150 A, on a 300 V DC bus from
  an Indramat supply module, "Bb1" ready contact there), programming module **AS33/065-000**, software
  **MSP1V0.8**, motor **1MB241H...-6C** (frameless water-cooled built-in spindle motor, 6 poles), 240 Nm max,
  22 kW from 850 to 3200 rpm, 11 kW at 6300 rpm, max 6300 rpm. The sheet is a single "CTX-TWIN HSP" list;
  whether both spindles use identical parameters is not stated here [check the second drive's AS module].
- **Speed command**: standard is the **analogue differential input X4:1/2 (E1/E2)** (second input X4:5/6
  E3/E4), ±10 V for max speed ("Cmd Volt" = 10, "Max Rpm" = 6300), 20 k input network, offset pots P1/P2.
  The type plate line reads "-L" (SERCOS) which contradicts this; confirm on the real units. If it is
  analogue: a LinuxCNC ±10 V DAC (e.g. Mesa 7i77/7i83) to X4:1/2 works directly. If it is really SERCOS
  (optical fibre on X4): LinuxCNC has no mainstream SERCOS I master, so drives would need re-equipping or
  replacing.
- **Enable / ready / fault (X2, 24 V opto-isolated, 1 = +24 V, high ≥ 18 V)**: inputs X2:24 RF (controller
  enable), X2:25 RUN (0 = set-point lockout), X2:3 AS starting lockout (KDA/TDA), X2:28 MD-RED (torque
  reduction), X2:30/31 PAR 1/2 (select P/Q/R/S record), X2:32-34 LIMIT, X2:35/36 SPEED 1/2 (gear ack; with
  0/0 or 1/1 the drive shows a GEARBOX fault), X2:37 EXT POS. Outputs (max 50 mA, sourced from +24 V ext on
  X2:19 / 0 V ext X2:1): X2:4 READY, X2:5 N = CMD (at speed), X2:6 N < MIN (zero speed), X2:7 IN POS,
  X2:8 90% LOAD, X2:9 TEMP WARN, X2:10 LIMIT, X2:13 LOAD LIMIT, X2:15 SPINDEL CONTR. **Bb ready relay
  contact X11:1-2 (KDA)**, 24 V/1 A, closed when ready: put it in the safety/E-stop chain. Several outputs are
  active-low (0 = event), so invert in HAL.
- **Analogue monitor outputs**: X2:21 N (speed, ±10 V) and X2:20 M (torque/load, 0 ... +10 V), 0 V = X2:22;
  usable for spindle load display in LinuxCNC via an ADC.
- **Spindle encoder / C axis / threading**: the drive has the **incremental encoder output (IGS)** option
  (AS software code 3, "Standard with IGS options"): connector **X6** on KDA, A, /A, B, /B, 0, /0 line driver,
  the NC must supply +5 V (X6:12, max 200 mA) and 0 V (X6:10). Line count is **per parameter record**:
  "Igslines" P = 16000, Q = 2048, R = 1024, S = 1024 (Igsref 350, Igsopen 6, Igsfunct 16). Interpretation:
  record P (PAR1 = PAR2 = 0, high gains P-Gain 11 / I-Gain 7,7, Md-Red 99, P-Maxrpm 105) is likely the
  C-axis / position-control record with 16000 lines/rev; Q/R/S are turning records. LinuxCNC can read X6
  with a differential encoder input (Mesa 7i77/7i85 etc.) for threading (spindle sync), CSS and C axis; note
  the scale changes when the parameter record changes, so HAL must follow PAR1/PAR2.
  X17:1-2 is a 24 V / 0.25 A contact "reference switching signal" from the spindle drive (index/home
  for the NC).
- **Motor feedback** is on X3 (high-resolution sine type, "Feedback" = 4 in parameter M2, "Generator
  MH.512-5-..."); a separate spindle encoder input X5 (ROD 420/426-compatible) exists only with the "P"
  option; "Encoder 2" = 4 and "Encoder 3" = 1024 are set, meaning unclear [uncertain].
- **Orientation**: the drive can orient itself: X2:26/27 POS 1/2 select positions A08/A09/A10 = Pos 1/2/3 =
  0°, 120°, 240° (parameters Pos 1-3), Pos-Gain 16,6, Pos-Rpm 150, P-Window 0,1; X2:25 RUN = 0 locks the
  set-point for positioning, X2:7 IN POS reports done; X2:37 EXT POS changes input weighting. LinuxCNC can
  either use this (M19 via digital outputs) or close the position loop itself through the IGS output.
- **Twin part transfer sync**: the sync parameters (Syncmode, Kv-Sync, S-Window, Smooth, Syncoff) are
  **empty** and field 7 is not "Y" (2nd/3rd encoder for spindle synchronisation), so the drives do not
  synchronise each other; synchronous transfer was presumably done by the NC (ELTROPILOT) using both IGS
  encoder outputs. In LinuxCNC, read both spindle encoders and run one spindle (or both as C axes in
  position mode via record P) as follower.
- **Temperature monitoring**: done inside the drive: "Maxtemp" = 155 (motor temperature limit, unit
  presumably °C [uncertain]); X2:9 TEMP WARN (0 = prewarning) is the only signal to the NC. The motor is
  water-cooled (1MB): keep the coolant flow interlock.
- **Keeping the drives**: needs 24 V I/O for RF/RUN/PAR/SPEED and READY/N=CMD/N<MIN/IN POS/TEMP WARN,
  one ±10 V output per spindle, one differential encoder input per spindle (5 V supplied by the NC),
  Bb in the safety chain. Back up the AS33 programming module (PDA = parameter duplication adapter) and this
  parameter list; keep spares of AS modules and of the KDA control board (CDR).
- **Replacing the drives**: a modern vector/servo drive must run a 6-pole asynchronous 1MB241 built-in
  motor (22 kW S1 band 850-3200 rpm, 240 Nm, 6300 rpm max, 145 A current parameter) with closed-loop
  sine/cosine motor feedback (X3 pinout above: A, A ref, B, B ref, 0, 0 ref, +5 V, 0 V); the Indramat
  high-resolution feedback type must be identified (signal levels/periods) before choosing a drive encoder
  card. Position feedback for C axis then comes from the new drive's encoder emulation or from the motor
  encoder directly.
