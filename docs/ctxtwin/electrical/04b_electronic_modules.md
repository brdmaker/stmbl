# Gildemeister CTX twin, electrical documentation: 4 Electronic modules (part b), Indramat KDV 4.1 supply module manual, PDF pages 1-18

Source: `4_Electronic_modules_b_20201028_0001.pdf` (18 pages, scanned 2020-10-28 on a Canon SC1011). The pages are manual pages 25-42 of an Indramat manual for the **KDV 4.1 a.c. mains supply module** (type KDV 4.1-30-3). This is the second half of that manual; sections 1-4.2 are presumably in part a. The pages carry no Gildemeister title block and no document number or date. The only identifiers are the Indramat drawing codes printed in the figure corners (`ENA8A-...`, `ENA1A-...`, `109-xxxx-xxxx-xx`), listed per page below. Original language: **English**, a translated Indramat manual. The scan contains no German text apart from the drawing code `ENA8A-ÜbwachKDV41` ("Überwachung", monitoring) and `ENA8A-CZ02/Maßblatt` ("Maßblatt", dimension sheet).

Note: these pages contain no machine-specific wiring, no Gildemeister device tags (-K.., -X.. of the cabinet) and no axis drive modules. They describe only the KDV 4 supply module, its terminals and its accessories. The CTX-specific connections are on other sheets.

## Page index

| PDF page | Manual page / drawing code | Content |
|---|---|---|
| 1 | p. 25 | 4.3 Circuit-breaker ratings (Table 3); 4.4 Mains contactor ratings (Table 4) |
| 2 | p. 26, `ENA8A-KDV3Skizze-30` | 4.5 Mains connection for electronics and fan supply; 4.6 Link circuit choke; 4.7 Additional capacitance; 4.8 RCD compatibility; 4.9 Safe clearances (Fig. 12) |
| 3 | p. 27, `ENA8A-Bus12/16` | 4.10 D.C. link circuit (cross sections); 4.11 Bus connection; 4.12 Control cubicle test; Fig. 13: X1 bus cable pinout 12-way/16-way |
| 4 | p. 28, `ENA8A-KDV4/HSM` | 5 Description of interface: 5.1 Signal voltages (X10/1-5); 5.2 "Ready" Bb1 (X10/6-7); 5.3 Supply to electronics OK, HSM (X10/8, X10/5), Fig. 14; 5.4 Temperature pre-warning TVW (X10/9-10) |
| 5 | p. 29 | 5.5 Power feed OK, UD contact (X10/11-12); 5.6 NCB link (X10/13-14); 5.7 Fan output (X13, X14a, X14b) |
| 6 | p. 30, `ENA8A-ÜbwachKDV41` | 6 Trouble-shooting notes; 6.1 Fault-finding; Fig. 15: KDV 4 monitoring and diagnostic system (logic diagram) |
| 7 | p. 31, `ENA8A-KDV41/Anzeigen` | 6.2 Diagnostic signals, LED list |
| 8 | p. 32 | 6.3 List of faults and their remedies [1]-[8] |
| 9 | p. 33 | 6.4 Fuses F2, F3, F4, F6, F7, F8 |
| 10 | p. 34 | 7.1 Dimensional data for KDV 4 (front view with terminal designations, weights, tightening torques) |
| 11 | p. 35, `ENA8A-KDV/Teilung aus 109-0747-3002-00` | 7.2 Control cubicle layout with drilling pattern (module arrangements KDV/KDA/KD/TDM) |
| 12 | p. 36, `ENA8A-KDV3/Montage/109-0747-4003-00` | 7.3 Control cubicle installation data (exploded view, part numbers) |
| 13 | p. 37, `ENA8A-CZ02/Maßblatt aus 109-0662-3001-00`, `ENA8A-TCM/Maßblatt aus 109-0546-3001-01` | 7.4 Additional capacitor CZ 02; 7.5 Additional capacitor TCM 1.1 |
| 14 | p. 38, `ENA8A-GLD/GLD14-GLD15/109-0252-4013-01` | 7.6 Link circuit smoothing choke GLD 14 / GLD 15 |
| 15 | p. 39, `ENA8A-KD20/9.565.015.4-00` | 7.7 Commutating choke KD 19 / KD 20 |
| 16 | p. 40, `ENA8A-KDV31/ZwKrschütz`, `ENA8A-KDV31/WidZSB` | 7.8 Link circuit short-circuit contactor; 7.9 Link circuit short-circuit resistor |
| 17 | p. 41 | 8 Ordering information: 8.1 Type code; 8.2 Available versions and accessories |
| 18 | p. 42, `ENA1A-KDV3/4/Zubehör` | 8.3 Electrical connection accessories E7/E8/E9-KDV (Fig. 16); 8.4 List of components |

---

## PDF page 1 (manual p. 25)

### 4.3 Circuit-breaker ratings

"The following recommendations apply when connecting the KDV 4.1 to 3 x 380-460 V a.c. mains supplies in accordance with the connection diagram."

**Table 3: Recommended circuit-breakers for 380 V 3-ph. mains supplies (Q1 as per connection diagram in section 4.1)**

| Link circuit cont. power in kW | Rated current in A | Circuit breaker Siemens 3VE.. | Adjustment range in A | Set value in A |
|---|---|---|---|---|
| 10,0 | 26 | 4200-. CS.. | 36...50 | 39 |
| 12,5 | 32 | 4200-. CT.. | 45...63 | 48 |
| 15,0 | 38 | 4200-. CT... | 45...63 | 57 |
| 17,5 | 45 | 5201-. CS... | 63...80 | 68 |
| 20,0 | 51 | 5201-. CS.. | 63...80 | 77 |
| 22,5 | 57 | 5201-. CS.. | 63...80 | 80 |
| 25,0 | 64 | 5201-. CS.. | 63...80 | 80 |
| 27,5 | 70 | 5201-. CS.. | 63...80 | 80 |
| 30,0 | 77 | 5201-. CS.. | 63...80 | 80 |

(So the breakers are Siemens 3VE4200-... and 3VE5201-...; the dots are printed as such.)

"If the power supply is protected by fuse links, no semiconductor fuses are necessary. Utilization category gL fuses are suitable. Fuses can be selected according to mains current."

### 4.4 Mains contactor ratings

"The following contactors should be selected when connecting the KDV 4 directly to 3 x 380-460 V mains supplies as per the connection diagram. They should be chosen according to the mains current only. Because of the charging current limiting of the KDV 4.1 at link circuit power ratings equal to or greater than 10 kW, inrush current peaks can be ignored."

**Table 4: Recommended mains contactor ratings (K1 as per connection diagram in section 4.1)**

| Link circuit continuous power in kW | Rated current in A | Mains contactor Siemens type |
|---|---|---|
| 10,0 | 26 | 3TB 44 |
| 12,5 | 32 | 3TB 44 |
| 15,0 | 38 | 3TF 46 |
| 17,5 | 45 | 3TF 46 |
| 20,0 | 51 | 3TF 47 |
| 22,5 | 57 | 3TF 47 |
| 25,0 | 64 | 3TF 48 |
| 27,5 | 70 | 3TF 48 |
| 30,0 | 77 | 3TF50 |

The manual's own tags are Q1 (mains circuit breaker) and K1 (mains contactor). The connection diagram they refer to (section 4.1) is not in this part.

---

## PDF page 2 (manual p. 26)

### 4.5 Mains connection for electronics and fan supply

- Voltage: 220 V a.c.
- Consumption: 6.8 A (at max. utilization of electronic and fan supplies)
- Terminal: X10a/1 - X10a/3

"The electronics and fans of the connected drive modules are supplied via the same terminals. In the KDV 4.1 the supply to the electronics and the fans have separate fuse protection. Short-circuit protection for the connecting lead only need be provided in the control cubicle (e.g. 10 A miniature circuit-breaker, tripping characteristic L)."

### 4.6 Link circuit choke

"The KDV 4 must always be run with an inductor in the "L+" and the "L-" lines. Both the GLD 14 and GLD 15 smoothing chokes contain an inductor for "L+" and "L-"."

Order code for smoothing chokes:

| Link circuit power | Two inductors 2 x 1 mH |
|---|---|
| up to 15 kW | GLD 14 |
| above 15 kW | GLD 15 |

### 4.7 Additional capacitance

"To increase the performance of the KDV 4.1, additional capacitors CZ 02 or additional capacitance modules type TCM may be connected to the d.c. link circuit. Additional capacitance of 4 mF is required to utilize the maximum power of the unit (see section 2.2). In a few applications the drives have to run free following a mains failure. The energy stored in the link circuit can be used for this. The stored link circuit energy can be increased by further additional capacitors. A maximum of 8 mF additional capacitance can be connected."

### 4.8 Compatibility with a residual-current device

"A residual-current-operated circuit-breaker must not be fitted to the mains supply cable of the KDV 4."

### 4.9 Safe clearances from combustible materials

"The bleeder resistor is placed in a separate housing within the KDV 4. Combustible materials such as conductors and cable ducts must have a minimum clearance of 100 mm above and 40 mm to the side of the housing."

**Fig. 12: Safe clearances from combustible materials** (`ENA8A-KDV3Skizze-30`): isometric sketch of the KDV with the heatsink at the back and the "Bleeder housing" on top. Dimensions: 100 (clearance above), 165, 40 (side clearance), 45, 40 [uncertain which edges the 45/40 belong to].

---

## PDF page 3 (manual p. 27)

### 4.10 D.C. link circuit

"When making the connections, use the busbars contained in the drive modules' connection accessory kits. Use twisted single cores for longer connections (max. length 2 metres)."

| Link circuit power in kW | Min. cross section in mm² |
|---|---|
| 14 | 10 |
| 22 | 25 |
| 30 | 35 |

### 4.11 Bus connection for supply to electronics and signal exchange

"For checking correct plug connection, the bus connection is terminated with an end connector, which is part of the supply module's electrical connection accessories."

### 4.12 Control cubicle test

"No voltages other than those specified in the data sheet or the interface instructions may be connected. Disconnect all KDV 4 connections before carrying out a high-voltage test on the control cubicle."

### Fig. 13: Bus connecting cable for signal exchange at interface between TVM/TDM and KDV/KDS devices (`ENA8A-Bus12/16`)

Left: Connector X1 (1) = X1 for 12-way types. Right: Connector X1 (2) = X1 for 16-way types. "Bus connection" in the middle. This ribbon/bus cable carries the electronics supply and the status signals from the supply module to the drive modules (TDM, KDA, KDS etc.).

Pin assignment:

| 12-way X1 pin | Signal | -> 16-way X1 pin | Signal on 16-way side |
|---|---|---|---|
| 1 | UD | 1 | UD |
| 2 | BB | 2 | BB |
| 3 | +15V | 3 | +15V |
| - | - | 4 | +15V (not connected to the 12-way side) |
| 4 | 0VM | 5 | 0VM |
| 5 | 0VM | 6 | 0VM |
| 6 | 0VM | 7 | 0VM |
| 7 | 0VM | 8 | 0VM |
| 8 | -15V | 9 | -15V |
| 9 | -15V | 10 | -15V |
| 10 | 0VL | 13 | 0VL |
| 11 | +24V | 11 | +24V |
| - | - | 12 | +24V (not connected to the 12-way side) |
| - | - | 14 | 0VL (not connected to the 12-way side) |
| - | - | 15 | free |
| 12 | PE (earth symbol) | 16 | PE (earth symbol) |

Signal meanings (from sections 5/6): UD = power-feed-OK / set-point-zeroing signal (Fig. 15 calls X1.1 "Fault condition sets drive set-point to zero"); BB = "Betriebsbereit" (ready), Fig. 15: "X1.2 Drive ready for power connection"; 0VM = measuring 0 V for ±15 V; 0VL = load 0 V for +24 V.

---

## PDF page 4 (manual p. 28)

### 5. Description of interface

#### 5.1 Signal voltages

"The signal voltages can be tapped off at terminal strip X10. These terminals are provided for test and measurement purposes. If these voltages are used outside the KDV 4.1, ensure that no interference voltages are injected (short, screened cables). The maximum permissible loading must not be exceeded. The ±15 V_M has short-circuit protection. The +24 V_L is protected by fuse F2."

| Terminal | Voltage | Meaning |
|---|---|---|
| X10/1 | +15 V_M | measuring voltage, 100 mA max. |
| X10/2 | 0 V_M | ref. potential of measuring voltage |
| X10/3 | -15 V_M | measuring voltage, 100 mA max. |
| X10/4 | +24 V_L | load voltage, 1 A max. |
| X10/5 | 0 V_L | ref. potential of load voltage |

#### 5.2 "Ready" state

Bb1 contact (X10/6 - X10/7)

| Condition | Relay de-energized | Fault | "Ready" |
|---|---|---|---|
| Contact 24V/1A d.c. | open | open | closed |

"The Bb1 contact of the KDV 4.1 is very important as it incorporates the drive system into the emergency-stop circuit. The power supply can only be connected when it is closed. The Bb1 contact closes when the supply to the electronics appears at terminal X10a and no fault is present."

"The Bb1 contact opens during the following faults:
- Tacho fault
- Overtemperature in drive modules
- Drive module bridge fuse
- No ±15 V_M / +24 V_L signal voltage
- Open-circuit bus connection or no end connector
- KDV 4.1 heatsink temperature too high
- Overcurrent in KDV 4.1 power circuits
- Earth fault in drive system
- Bleeder overloaded"

#### 5.3 Supply to electronics OK

HSM - aux. voltage messages

| Condition | No volts | Fault | Electronic message supply OK |
|---|---|---|---|
| Transistor output 100 mA max. | 0 V | 0 V | +24 V |

"If a fault occurs in the electronic supply (auxiliary voltage), the aux. voltage message is set to 0V_L and the aux. voltage LED goes out."

**Fig. 14: Aux. voltage alarm output circuit** (`ENA8A-KDV4/HSM`): inside the KDV 4, a PNP transistor fed from +24V_L drives the output through a PTC/fuse element [uncertain, drawn as a resistor with a temperature/arrow symbol, labelled "9,4 Ω" [uncertain value]] and a series diode to terminal **X10/8**. A protective element (zener/suppressor [uncertain]) is drawn to 0V. Reference **X10/5 = 0V_L**. The "External circuit" shows a relay coil with a free-wheeling diode between X10/8 and X10/5. So HSM is a +24 V high-side transistor output, 100 mA max.

"The drives remain operational. After the power supply is disconnected reconnection is interlocked via the Bb1 contact."

"Note: The electronics and fans of the connected drives are supplied via the same terminals. If the aux. voltage message stays off, this indicates that the KDV's or drive modules' temperature monitors have been activated."

#### 5.4 Temperature pre-warning

TVW contact (X10/9 - X10/10)

| Condition | Relay de-energized | Temp. to high | Temp. OK |
|---|---|---|---|
| Contact 24V/1A d.c. | open | open | closed |

"The temperature pre-warning contact opens when the heatsink temperature is unduly high and the red "TÜ" LED comes on. The Bb1 contact interrupts the emergency-stop circuit after 30 seconds. The way the drive system reacts to this fault depends on the position of the "NCB" link (see section 5.6)."

"If NC-controlled stopping is required, the drives should be stopped within 30 seconds of the KDV's or drive modules' temperature pre-warning being activated."

---

## PDF page 5 (manual p. 29)

### 5.5 Power feed OK

UD contact (X10/11 - X10/12)

| Condition | Relay de-energized | Fault | Power OK |
|---|---|---|---|
| Contact 24V/1A | open | open | closed |

"The UD contact acknowledges that the power supply is OK. It opens for the following faults:
- no clockwise rotating field
- mains failure/phase failure
- link circuit voltage below 200 V

The way the drive system reacts to one of these faults depends on the position of the "NCB" link (see section 5.6). If NC-controlled stopping is required, the drives must be stopped by the master controller when the UD contact is activated."

"This allows the drives to be stopped under NC control in the event of mains failure or phase failure. The power regenerated during braking must be greater than the power requirement. In the case of a drive fault the power supply is always disconnected via the Bb1 contact."

"Note: With the NCB link closed, the master controller must always ensure that the drives are stopped if there is no set-point zeroing, i.e. the master controller must always evaluate the UD contact and stop the drives when the contact opens. Otherwise, the drives can be expected to slow down uncontrolled if the power feed is removed."

### 5.6 Stopping the drive if power supply is faulty

NCB link (X10/13 - X10/14)

"With the NCB link open, faulty power feed and a fault within the drive system, the speed set-point of all connected drives is set to zero. The drives are braked at maximum torque. In the case of a drive malfunction the power is also disconnected via the Bb1 contact."

"With the NCB link closed, the set-point zeroing action is suppressed during the following faults:
- faulty power supply
  - mains failure/phase failure
  - link circuit voltage below 200 V
- drive faults
  - open-circuit bus connection or no end connector
  - overcurrent in power circuits of KDV 4.1
  - earth fault in drive module
  - bleeder overloaded"

### 5.7 Fan output

Fan output (X13, X14a, X14b). "This plug is only used to supply power to the fans."

"Note: No external voltage must be applied to this connector, otherwise there is a risk of damaging the KDV 4.1"

### Summary: X10 terminal strip of the KDV 4.1 (compiled from pages 4-5)

| X10 | Function |
|---|---|
| 1 | +15 V_M (100 mA) |
| 2 | 0 V_M |
| 3 | -15 V_M (100 mA) |
| 4 | +24 V_L (1 A, fuse F2) |
| 5 | 0 V_L |
| 6-7 | Bb1 "ready" contact, 24 V/1 A d.c., closed = ready (E-stop chain) |
| 8 | HSM aux-voltage-OK, transistor output +24 V / 100 mA |
| 9-10 | TVW temperature pre-warning contact, closed = temp OK |
| 11-12 | UD power-feed-OK contact, closed = power OK |
| 13-14 | NCB link (open = drives brake to zero on supply fault; closed = NC must stop them) |

X10a/1-3: 220 V a.c. supply for electronics and fans. Exact pin assignment of X10a/1-3 is not given on these pages (presumably L1/L3 per fuse names F3 "L1" and F4 "L3", plus PE [uncertain]).

---

## PDF page 6 (manual p. 30)

### 6. Trouble-shooting notes

"Because of the resulting production down-time, lengthy fault-finding and repairs to drive components on the machine are unacceptable. Thanks to their construction, Indramat a.c. drives enable individual functional units to be easily and completely replaced without adjustments. This means that in the event of a fault, servicing is limited to fault-location either on the motor, the supply module or the drive module, or its complete replacement."

### 6.1 Fault-finding

"Because of the interaction between NC controller, supply and drive modules, motor, mechanical system and position measurement, poor performance of axis movements can be caused either by a fault in the above devices or incorrect matching of individual components. The KDV 4 is fitted with a comprehensive diagnostic system for rapid fault location."

### Fig. 15: KDV 4 monitoring and diagnostic system (`ENA8A-ÜbwachKDV41`)

Logic diagram, described:

- **"Link circuit voltage < 50V"** -> R input of an RS flip-flop (RQ / S Q-bar), whose output goes towards the **NCB-link** (drawn as a wire link) and an OR gate (≥1).
- **"Mains monitor"** (with LED) and **"Link circuit voltage ≥ 200V"** (with LED) -> AND gate (&). The AND output drives the **UD** contact ("UD opens during fault") and also sets the flip-flop S input and feeds a second AND gate.
- OR gate (≥1) output -> **X1.1** ("Fault condition sets drive set-point to zero"). Inputs: the NCB-link path and the output of the second AND gate (whose inputs are the mains/link-OK signal and the "ready" chain).
- **"+24VL, ±15VM"** (with LED) -> large AND gate.
- **"Overcurrent KDV 4"** -> RS flip-flop (stored, LED) -> Q-bar -> large AND.
- **"Earth fault in drive system"** -> RS flip-flop (stored, LED) -> Q-bar -> large AND.
- **"Bleeder monitor"** -> RS flip-flop (stored, LED) -> Q-bar -> large AND.
- **"Temperature monitor KDV 4"** -> 30 sec. delay -> RS flip-flop (stored, LED) -> Q-bar -> large AND. The temperature monitor also drives the **TVW** contact directly ("TVW opens during fault").
- Large AND output and **X1.2** ("Drive ready for power connection", the BB line from the drive modules) -> AND -> **Bb1** contact ("Bb1 opens during fault"). This output also feeds back to the set-point zeroing logic.
- **"Link circuit voltage ≤ 440V"** -> **X1.15** "main drive lock-out" [note: Fig. 13 shows 16-way X1 pin 15 as "free", so X1.15 is used only on some module types [uncertain]].
- **"Monitor for supply to electronics"** (with LED) -> **HSM** output "0V during fault".

---

## PDF page 7 (manual p. 31)

### 6.2 Diagnostic signals, fault list (`ENA8A-KDV41/Anzeigen`)

"Note: Messages are only valid if the "+24V, ±15V" indicators show green continuously"

| LED colour | Light emitting diode | OFF | ON |
|---|---|---|---|
| green | AUX.-VOLT. | No supply to electronics [1] | Supply to electronics OK |
| green | +24V/±15V | Signal voltages faulty [2] | Signal voltages OK |
| green | MAINS FAULT | Power not connected or faulty [3] | Power OK |
| green | POWER OK | Power not connected or faulty, link circuit voltage faulty [4] | Power OK |
| red (1) | BLEEDER OVERL. | Drives' braking mode within limits | Disconnection due to high braking energy or fault in KDV [5] |
| red (1) | TEMP. FAULT | Temperature within limits | Disconnection due to excessively high temperature [6] |
| red (1) | OVERCURRENT | Current in power circuits within limits | Disconnection due to overcurrent, short-circuit in KDV, in drive module, in cable or in motor [7] |
| red (1) | EARTH CON. | No earth fault in drive system | Disconnection due to earth fault in KDV 4, in drive module, in cable or in motor [8] |

"(1) Message and interlock is stored. Reset by pressing reset button or switching on the power to the electronics again."
"[ ] Fault number (see "List of faults and their remedies")"

---

## PDF page 8 (manual p. 32)

### 6.3 List of faults and their remedies

**[1] No supply to electronics.** Possible causes: fuse F3 or F4 blown; broken supply lead. Remedies: check voltage at X10a (220 V a.c.); check fuses F3 and F4.

**[2] No control voltage.** Possible causes: fuses F3 or F4 blown; signal voltages exceed maximum loading; link circuit voltage less than 180 V a.c. after mains failure. Remedies: disconnect bus connection to drive modules and measure signal voltages; check fuses F3 and F4.

**[3] Power not connected or faulty.** Possible causes: no clockwise rotating field at X9; one phase missing at X9. Remedies: check mains feed at X9, 3 x 380-460 V a.c.; check clockwise rotating field at X9.

**[4] Power not connected or faulty. Fault in link circuit.** Possible causes: no clockwise rotating field at X9; one phase missing at X9; link circuit voltage less than 200 V d.c.; link circuit smoothing choke not connected. Remedies: check 3 x 380-460 V a.c. mains feed at X9; disconnect busbars to drive modules and measure link circuit voltage at X9 (L-, L+); check link circuit choke is correctly connected.

**[5] Disconnection due to excessive braking energy.** Possible causes: energy in drives too high when braking during emergency-stop or mains failure; bleeder resistor faulty; fault in KDV 4. Remedies: check energy in drives; arrange delayed power disconnection for emergency-stop.

**[6] Disconnection due to excessively high temperature.** Possible causes: fan not connected or faulty; fuses F6, F7 or F8 blown; heatsink dirty; KDV 4 overloaded; ambient temperature too high. Remedies: check fan, including F6, F7 and F8; clean heatsink; check loading; check ambient temperature; test TVW contact.

**[7] Disconnection due to overcurrent.** Possible causes: faulty drive module; short-circuit in KDV 4; mains inductance too high; motor power cable damaged; short-circuited motor winding. Remedies: check drive modules with associated motors and cables; check mains infeed (mains transformer).

**[8] Disconnection due to earth fault.** Possible causes: faulty drive module; motor power cable damaged; motor frame short. Remedies: check drive modules with associated motors and cables.

---

## PDF page 9 (manual p. 33)

### 6.4 Fuses

| Description | Symbol | 5 x 20 mm fuse link |
|---|---|---|
| +24 V_Lext (output) | F2 | 2 A / 250 E medium-blow |
| Aux. voltage fuse L1 | F3 | 10 A / 250 E slow-blow |
| Aux. voltage fuse L3 | F4 | 10 A / 250 E slow-blow |
| Ext. heatsink fan | F6 | 0,63 A /250 E medium-blow |
| Fan fuse (L1) | F7 | 10 A /250 E slow-blow |
| Fan fuse (L3) | F8 | 10 A/ 250 E slow-blow |

"With the exception of F6, the fuses are located on the front of the unit (F6 is on the fan)."

---

## PDF page 10 (manual p. 34)

### 7. Dimensional data / 7.1 Dimensional data for KDV 4

Drawing printed rotated 90°. Three views: front view, side view and top view with heatsink.

**Front view terminal designations** (read from enlarged scan):
- Left power terminal row: **L3, L2, L1 = X9** (mains 3 x 380-460 V), **1L+, 2L+ = X28** [uncertain: printed "X 28" or "X 2B"], **1L-, 2L- = X28a** [uncertain]. Pitch dimension 52,5. The legend at the bottom spells the connections as "L1; L2; L3 / 2L-; 1L- / 2L+; 1L+" (choke connections 1L±/2L±, see GLD on page 14).
- **X12**: PE (earth symbol), at the left edge.
- **X9a**: two screw terminals **L-** and **L+** (d.c. link busbar output to the drive modules).
- "LED's" row at the top. **X1**: bus connector (to the drive modules).
- **F2** and **RESET S2** (push button) next to X1.
- **X10**: terminal strip (14 terminals; note "max. 2,5 mm²").
- **F8, F7, F4, F3**: fuse holders. **X10a**: 3-pin plug (220 V a.c. electronics/fan supply).
- **X13** (fan plug, bottom) and **X14a / X14b** (fan plugs, top right). **SW 5**: earthing/fixing screw (Allen head, width across flats 5).
- Overall dimensions: 390 high, 373 (fixing pitch), 355 deep, 325, 345 (heatsink), 150 wide; "160 without fan cowl (185 with fan cowl)"; heatsink cut-out offset 23 [uncertain], M8 studs, "Gasket seal", "Shock protection".

Tightening torques:
- L1; L2; L3 / 2L-; 1L- / 2L+; 1L+ = M6; M_A = 5 Nm
- L+; L- ; earth = M5; M_A = 3 Nm
- M_A = tightening torque [Nm]

Weight:
| Item | Weight |
|---|---|
| KDV 4.1 supply module | approx. 21 kg |
| Mech. mounting kit M1-KD | approx. 1.7 kg |
| Fan unit LE 4 | approx. 4.2 kg |
| Max. total weight | approx. 26.9 kg |

---

## PDF page 11 (manual p. 35)

### 7.2 Control cubicle layout with drilling pattern (`ENA8A-KDV/Teilung aus 109-0747-3002-00`)

Drawing printed rotated 90°.

**"Inside view of control cubicle"**, "Cubicle dimensions": cut-out "for KDS, KDV and KDA modules" 86 +1 wide x 351 +1 high. Hole pattern 4x ø5, 2x ø11, 96 ±0,2, 373 ±0,2, 403 ±0,2, 15, 50. "Cut-out for external fan only required for units with forced cooling": 115 x 92, 78 ±0,2, 18, 6x ø5, 133 ±0,2, 9.

**"Arrangement of KD modules in control cubicle"**: four example module rows (each module with its fan unit below), with module pitches:
- KDV 3. | KDA 2./KDA 3. | KD./TDM. ; pitches 110 ±0,5, 155 ±0,5
- KD. | KDA 2./KDA 3. | KDV 3. ; pitches 110 ±0,5, 200 ±0,5
- KD. | KDV 3. | KDA 2./KDA 3. ; pitches 155 ±0,5, 110 ±0,5; overall 555
- KDV 3. | KDA 2./KDA 3. | TDM 3./4. ; pitches 110 ±0,5, 137 ±0,5

(KDA = Indramat a.c. main spindle drive module, KD. = generic KD drive module, TDM = servo drive module, KDV = supply module. The sketch refers to KDV 3; the same applies to KDV 4.)

---

## PDF page 12 (manual p. 36)

### 7.3 Control cubicle installation data (`ENA8A-KDV3/Montage/109-0747-4003-00`)

Exploded view of the "KDV three-phase supply module" mounted through the rear panel of the cubicle, with the heatsink outside. Parts and labels:

- Air duct 109-0575-4834-XX (1), "required for forced cooling with fan" (dimensions 69, 185)
- KD module mounting frame 109-0575-4831-XX (2)
- Protective grille (shock protection) 109-0575-3236-XX (1)
- Fan mounting frame 109-0575-4833-XX (1)
- M8 thread; M4 thread
- Cut-out for additional fan
- Rear panel of enclosure: "Refer to drawg. no. 109-0574-3004-XX for details of enclosure and cubicle construction"
- Cut-out for additional KD module
- Cheese-head screw / 2x M4x14Z4 DIN912 (1)
- Cheese-head screw/ 4x M4x16 DIN912 (2); (2) Contact washer M4/4x
- "Ensure electrical connection between rear panel of cubicle and mounting frame"
- Allen head with SW5
- Fan for forced cooling 109-0575-4832-XX (1), "For use with units with cooling type W1- see KDV manual"
- Cheese head screw / 4x M4x18Z4-1 DIN912 (1)
- "Parts marked (1) are included in kit M1 - 109-0574-4819-XX"
- "Parts marked (2) are included in kit M1 - 109-0575-4851-XX"

---

## PDF page 13 (manual p. 37)

### 7.4 Dimensional data for additional capacitor CZ 02 (`ENA8A-CZ02/Maßblatt aus: 109-0662-3001-00`)

Round capacitor on a square retainer 102 x 102, slot 4,4 x 7, length min. 165. "Drilling pattern for CZ-02 retainer": 3 x M4 on a circle, 120° spacing, radius 44,5.

### 7.5 Dimensional data for additional capacitor TCM 1.1 (`ENA8A-TCM/Maßblatt aus: 109-0546-3001-01`)

Module 208 wide (front), 105 wide (side), height 355/373/390, offsets 18, 7, 60, 9. Min. 80 mm clearance above and below ("Cooling air outlet" / "Cooling air inlet"). "Shock protection". Terminal **X15**: L-, L+, L-, L+ and earth ("Shown without shock protection").
- "Tightening torque MA (Nm) for L-; L+; earth from Ms 58 : M5 = 2,5 Nm"
- "X15: L-; L+ = power connections max. 6mm2"
- Weight: TCM 1.1-04-W0 ca. 6kg; TCM 1.1-08-W0 ca. 7kg

---

## PDF page 14 (manual p. 38)

### 7.6 Dimensional data for link circuit smoothing choke GLD 14 / 15 (`ENA8A-GLD/GLD14-GLD15/109-0252-4013-01`)

(Heading partly illegible on the scan; reconstructed from the drawing code and section 4.6.)

Two-coil choke. Electrical connections at the top: 1.1, 2.1, 1.2, 2.2; earth stud M6x20.

**Internal circuit diagram:** coil 1: 1.1 -> 2.1 (dot at 1.1 end); coil 2: 1.2 -> 2.2 (dot at 2.2 end [uncertain]); core connected to earth.

**Connection to KDV supply module ("GLD 14 or GLD 15 choke" -> "Choke connections KDV supply module"):**

| Choke terminal | KDV terminal |
|---|---|
| 1.1 | 2L+ |
| 2.1 | 1L+ |
| 1.2 | 1L- |
| 2.2 | 2L- |

So the L+ inductor sits between KDV terminals 1L+ and 2L+, and the L- inductor between 1L- and 2L-.

| Type | mH/A per coil | A | B | C | E | F | Hø | I | J | Electr. connect. | Weight kg |
|---|---|---|---|---|---|---|---|---|---|---|---|
| GLD 15 | 1,0/100 | 200 | 150 | 340 | 80 | 120 | 11x18 | 115 | 145 | 35mm² | 25 |
| GLD 14 | 1,0/50 | 140 | 102 | 245 | -- | 75 | 7x15 | 95 | 117 | 16mm² | 9,5 |

"Slot in direction "J"".

---

## PDF page 15 (manual p. 39)

### 7.7 Dimensional data for commutating choke KD 19 / 20 (`ENA8A-KD20/9.565.015.4-00`)

Three-phase choke. Circuit diagram: U1 -> U2, V1 -> V2, W1 -> W2, separate PE terminal. It is placed in the mains supply to the KDV (X9). Eye bolt G; "Slot in direction "B"".

| Type | Current/ A | A | B | C | D | D1 | E | F | G | H | earth | Terminals /mm² | Weight kg |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| KD 19 | 45 | 150 | 90 | 190 | 100 | 70 | 70 | 6x10 | (blank) | 20 | M5 | 16 | 6,5 |
| KD 20 | 90 | 240 | 120 | 260 | 170 | 110 | 90 | 11x18 | M8 | 25 | M8 | 35 | 18 |

---

## PDF page 16 (manual p. 40)

### 7.8 Link circuit short-circuit contactor (`ENA8A-KDV31/ZwKrschütz`)

**A.C. voltage operation**
- Contactor 220V a.c. 50/60Hz LC1-D258 MA 65
- Contactor 110V a.c. 50/60Hz LC1-D258 FA 65
- Aux. block LA1-D22 A 65
- Overvoltage limiter LA9-D 0 9980

Dimensions: 81 high, 94 deep (124 with aux. switch block), 2 x ø4,5 holes, 48, 40, 56, 50/60.

**D.C. voltage operation**
- Contactor 24V d.c. LP1-D258 BW 65
- Aux. block LA1-D22 A 65
- Overvoltage limiter LA9-D221 [uncertain last digits]

Dimensions: 82 high, 129 deep (156 with aux. switch block), 2 x ø4,5, 40/50, 50/60.

(Telemecanique LC1/LP1 contactors. They short-circuit the d.c. link through the resistor below to brake the motors in an emergency.)

### 7.9 Link circuit short-circuit resistor (`ENA8A-KDV31/WidZSB`)

1. Link circuit short-circuit resistor, Type: ZSB 30 x 215 - 2R2 ± 10 % (2,2 Ω)
2. Fixing bracket, Type: Z angle

Dimensions: ø34 ±1,0, length 215 ±5 / 265 ±5 (with brackets), 185 ±5 / 249 ±5 (fixing), 49, 2, 5,5, 5,2, 12, 34.

---

## PDF page 17 (manual p. 41)

### 8. Ordering information / 8.1 Type code of KDV 4

Example: **KDV 4 . 1 - 30 - 3**
- KDV = Device abbreviation
- 4 = Series
- 1 = Version
- 30 = Continuous output in kW
- 3 = D.C. power voltage [presumably code 3 = 300 V class link [uncertain]; not explained on this page]

### 8.2 Available versions of supply module KDV 4 and accessories

| Description | Available versions |
|---|---|
| Power supply module | KDV 4.1-30-3 |
| Electrical connection accessories | E7-KDV, E8-KDV, E9-KDV (see section 8.3) |
| Mechanical assembly kit | M1-KD |
| Fan unit | LE 4-220 |
| Link circuit smoothing choke | GLD 14, GLD 15 |
| Additional capacitors | CZ 02, TCM 1.1-04-WO |
| Commutating choke | KD 19 ...see data sheet, section 7; KD 20 ...see data sheet, section 7 |

---

## PDF page 18 (manual p. 42)

### 8.3 Summary of electrical connection accessories (`ENA1A-KDV3/4/Zubehör`)

**Fig. 16: Electrical connection accessories according to connected drive modules**
- **E7-KDV**: KDV 3./4. on the right, three K.. modules to the left; 16-way bus, end connector at the left module.
- **E8-KDV**: KDV 3./4. on the left, three K.. modules to the right; 16-way bus, end connector at the right.
- **E9-KDV** (two variants): KDV on the right with T.., K.., K.. to the left, or KDV on the left with K.., K.., T.. to the right. Here the bus is **12-way** at the T.. (TDM) module and the end connector is at the T.. module.

So E7/E8 cover 16-way (K-type: KDA/KDS) modules and E9 covers a mix with 12-way TDM modules, using the adapter cable of Fig. 13.

### 8.4 List of components in the mains power supply unit

| No. | Part |
|---|---|
| 1.0 | Supply module KDV 4.1-30-3 |
| 1.1 | Mechanical assembly kit M1-KD |
| 1.2 | Electrical connection accessories E-KDV |
| 1.3 | Fan unit LE 4-220 |
| 2.0 | Link circuit choke GLD; Selection as per sections 2.2 and 4.6 |
| 3.0 | Commutation choke KD..; Selection as per section 2.2 |

---

## For the retrofit

**What these pages tell us.** They cover only the **Indramat KDV 4.1-30-3** d.c. link supply module: 30 kW continuous, 3 x 380-460 V a.c. in, regenerative/bleeder braking. These pages do not mention a command interface, encoders, SERCOS or analog set-points. Those belong to the drive modules (TDM servo axes, KDA/KDS spindles) and their plug-in cards, which are documented elsewhere. Facts that matter for keeping the original drives under LinuxCNC:

1. **The drive family is the Indramat analog generation, not SERCOS.** The bus (X1) carries only ±15 V_M, +24 V_L and the status lines UD and BB, plus X1.15 lock-out. Fig. 13 names TVM/TDM (servo) and KDV/KDS modules, and page 11 names KDA/KD/TDM. TDM and KDA/KDS of this era are analog ±10 V speed-command drives with resolver/tacho feedback inside the drive. Position feedback to the CNC comes from separate encoders or scales. The command interface and the feedback resolution still have to be confirmed from the drive-module sheets: type plates (TDM x.x-xxx-300-W..), plug-in modules and control-side wiring.
2. **E-stop chain: Bb1 = X10/6-X10/7** (24 V/1 A d.c., closed = ready). Bb1 must be in series in the E-stop / main-contactor (K1) chain: the power contactor may only pull in when Bb1 is closed. Bb1 opens on any drive fault (tacho fault, drive overtemp, bridge fuse, missing ±15/24 V, open bus or missing end connector, KDV overtemp after 30 s, overcurrent, earth fault, bleeder overload). Keep this wiring and also wire Bb1 (or an aux contact) to a LinuxCNC input, so that `estop` / `amp-fault` see it.
3. **UD = X10/11-X10/12** (closed = power OK: phase sequence, mains present, link ≥ 200 V). If the NCB link (**X10/13-14**) is closed on this machine, the control must watch UD and stop the axes itself, because the drives do no set-point zeroing. Check the NCB link position in the cabinet. With NCB open, the KDV zeros all set-points via bus X1.1 and the drives brake at max torque. In LinuxCNC, wire UD to an input and trigger a controlled stop or E-stop when it opens.
4. **TVW = X10/9-X10/10** temperature pre-warning: it opens 30 s before Bb1 drops. Wire it to a LinuxCNC input and feed-hold or stop within 30 s.
5. **HSM = X10/8 (vs X10/5 0V_L)**: +24 V transistor output, 100 mA, = electronics supply OK. Usable directly as a 24 V PLC/LinuxCNC input.
6. **Drive enable sequence.** The KDV needs 220 V a.c. on **X10a** (electronics and fans of all modules) before power. Then the 3-phase mains goes through Q1 (Siemens 3VE) and K1 (Siemens 3TB/3TF) to **X9** with a correct clockwise rotating field. The mains must not be fed through an RCD. The link (L+/L- at **X9a**) goes to the modules by busbar, and the GLD choke is between 1L±/2L±. The per-axis enables (RF / regler-freigabe) and the analog set-point lines are on the drive modules, not on the KDV. The bus BB line (X1.2, "drive ready for power connection") collects the drive-module readiness.
7. **Braking / E-stop energy.** There is a bleeder in the KDV and an optional link short-circuit contactor (Telemecanique LC1/LP1-D258) with a ZSB 2,2 Ω resistor as dynamic brake. Fault [5] recommends "delayed power disconnection for emergency-stop": the LinuxCNC E-stop should first ramp the axes down (or zero the set-points) and then drop K1 after a delay. It should not cut power instantly while the drives are still loaded.
8. **Signal supplies.** X10/1-5 give ±15 V_M (100 mA) and +24 V_L (1 A, F2 2 A). These are for test only. Do not use them to power LinuxCNC I/O beyond these limits.
9. **Diagnostics.** The front LEDs (AUX.-VOLT., +24V/±15V, MAINS FAULT, POWER OK, BLEEDER OVERL., TEMP. FAULT, OVERCURRENT, EARTH CON.) latch the red faults. Reset with S2 RESET or by cycling the 220 V electronics supply. Fuse list: F2 2 A M, F3/F4 10 A T (aux L1/L3), F6 0,63 A M (fan), F7/F8 10 A T (fan L1/L3).

**If the Indramat drives are replaced.** The KDV 4.1, its chokes (GLD, KD 19/20), capacitors (CZ 02/TCM) and short-circuit contactor go with them. Modern drives with their own rectifier or a common d.c. bus take 3 x 400 V directly. The things to carry over from this design:
- the mains breaker/contactor sizing (77 A max at 30 kW)
- an E-stop chain that includes the new drives' ready/STO contacts in place of Bb1
- a power-OK/phase-loss input in place of UD
- an over-temperature pre-warning input in place of TVW
- an aux-supply-OK input in place of HSM
- sufficient braking-resistor capacity for E-stop from full spindle speed (the KDV bleeder plus the ZSB 2,2 Ω short-circuit resistor did that job)
