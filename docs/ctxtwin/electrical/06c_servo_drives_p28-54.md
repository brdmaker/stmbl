# Gildemeister CTX twin, electrical documentation: 6 Servo drives (part c), PDF pages 28-54

Source: `be517c0e-6_Servo_drives_c_20201029_0001.pdf`, PDF pages 28-54 of 54 (pages 1-27 are transcribed separately).
These pages are not Gildemeister wiring sheets but the end of a bound-in Indramat manual:
**"DDS 2.1 and MDD with ANALOG interface", Indramat document 209-0069-4315-03, December 23, 1993**
(back cover: "DOK DDS2.1 MIT ANALOGINTERFACE APPL EN", 209-0069-4315, barcode 257 182, Indramat GmbH, Bgm.-Dr.-Nebel-Straße 2, D-97816 Lohr).
Manual page numbers 104-129 plus the back cover. Original language: **English** (the only German text is "DOK DDS2.1 MIT ANALOGINTERFACE APPL EN" = "documentation DDS 2.1 with analog interface, application, English"). There is no Gildemeister title block, drawing number or machine-specific data on these pages.

## Page index

| PDF page | Manual page | Content |
|---|---|---|
| 28 | 104 | 9. Diagnostics and fault clearance: display code "92"; 9.4 input errors and faults while saving |
| 29 | 105 | 9. cont.: commutation adjustment messages; 9.5 selecting signals for the analog diagnostics outputs |
| 30 | 106 | Fig. 87: block diagram, analog output signals on the DDS 2.1 (control loop structure) |
| 31 | 107 | 9.6 Notes on component replacement |
| 32 | 108 | 10. Overview of parameters |
| 33 | 109 | 11. Appendix, 11.1 Repair Card (Fig. 88) |
| 34 | 110 | 11.2 Specific axis data list, to be completed by hand (Fig. 89) |
| 35 | 111 | Specific axis data list, parameter tables (Fig. 90) |
| 36 | 112 | 11.3 Example of a parameter file printout (Fig. 91) |
| 37 | 113 | blank page |
| 38 | 114 | 11.4 DDS 2.1 connection diagram for motors with resolver feedback (Fig. 92) |
| 39 | 115 | 11.5 DDS 2.1 connection diagram for motors with digital servo feedback (Fig. 93) |
| 40 | 116 | 11.6 Pin allocations: X1, X2, X3, X4 (digital feedback) (Figs. 94-97) |
| 41 | 117 | Pin allocations: X4 (resolver feedback), X5, X6 (Figs. 98-100) |
| 42 | 118 | X13/X14: command communication module DAE 1.1 (Fig. 101) |
| 43 | 119 | X15/X16: command communication module DAA 1.1 (Fig. 102) |
| 44-47 | 120-123 | 11.7 Overview of Indramat companies, subsidiaries and agencies |
| 48-51 | 124-127 | 12. Glossary, explanations |
| 52-53 | 128-129 | 13. List of keywords |
| 54 | — | Back cover |

---

## PDF page 28 (manual p. 104) — 9. Diagnostics and fault clearance

**Display "92": "Absolute encoder not calibrated"** (Reading error: reference position/counting direction).
The parameter "reference position" and/or "counting direction" in the menu ABSOLUTE ENCODER SETUP could not be read.
- Cause: 1. These parameters have not yet been entered. 2. DSF feedback defective.
- Remedy: 1. Enter or confirm parameters. 2. Replace motor.

**9.4. Input errors and faults due to errors while saving** — Faults of this type only occur in the DDS 2.1 when working with a PC or a VT 100 terminal; they are indicated by messages on the operator interface.

| Message | Cause | Remedy |
|---|---|---|
| "Parameters outside the input limits" | Parameters are outside the upper and lower input limits. | Enter the figures within the permissible range. |
| "Error at saving the parameter" | An error was detected while attempting to write the parameter to the software module. | Repeat the entry. If the message appears again, replace the software module. Procedure: 1. Save the parameters from the old software module. 2. Replace the software module. 3. Enter the parameters in the new software module (see Section 4). |
| "Parameter only to change when RF not applied" | An attempt was made to change a parameter in one of the menus "INCREMENTAL ENCODER EMULATOR", "OPERATING MODES, SCALING", "ABSOLUTE ENCODER EMULATOR" while the controller enable (RF) signal was applied. | Cancel the RF signal and enter the parameter once more. |
| "Commutation adjustment not possible: Drive is not disabled." | Commutation offset adjustment was started while RF was still applied. | Cancel the RF signal and try again. |

## PDF page 29 (manual p. 105)

| Message | Cause | Remedy |
|---|---|---|
| "Commutation adjustment not possible: Turn power on" | Commutation offset adjustment started while power was down. | Turn power on and try again. |
| "Commutation adjustment not possible: drive interlock open" | Commutation offset adjustment started while the safety lockout was active. | Switch off the input for the safety lockout, **X3 pin 8/9**, and start again (see Section 7.2). |

**9.5. Selecting signals for diagnostics outputs** — The DDS 2.1 has two analog diagnostics outputs for emitting internal drive variables for test purposes.

Analog signal output: the DDS 2.1 with ANALOG interface can output two selectable signals via diagnostics output **AK1 on X3.1 and X3.2** or **AK2 on X3.3 and X3.4** (signals see Fig. 87). Selected via the menus "OPERATING MODES, SCALING" or "GAIN PARAMETERS" on the operator interface.
- "velocity data scaling at the analog output [rpm/10V]" scales velocity data on channel 1 or 2; unit [rpm/10V].
- "position data scaling at the analog output [dgr/10V]" scales position data on channel 1 or 2; unit [dgr/10V]; total voltage range is 20 V (±10 V).
- Selection with the "→" and "←" arrow keys in the fields "Analog output: Channel 1 →:" / "Analog output: Channel 2 →:". Selectable signals: Velocity command; Velocity actual value; Position actual value; Current actual value; Motor encoder sine signal; Motor encoder cosine signal.

## PDF page 30 (manual p. 106) — Fig. 87: Analog output signals on the DDS 2.1

Block diagram (drawing ref. "ENA3A-Abb52/InbetrDDS"). Signal flow, left to right:
ω_com. → command smoothing filter T_SG (first-order lag) → summing point (minus velocity actual value) → velocity controller (K_P, T_N, K_DP, T_GL) → summing point (minus current feedback) → current controller K_pi → summing point (minus EMF c_ω·ω) → motor electrical block 1/R_A, L_A → current i → torque constant K_M → m_d → summing point (minus load torque m_L) → 1/J_M → ω → integrator → φ_M.
Motor position is fed back via the encoder "sin/cos" → "Position calculation" block → position actual value; a differentiating block derives the velocity actual value.
Tapping points labelled as selectable analog outputs: "Velocity command", "Velocity actual value", "Current command" [as printed; text lists "current actual value"], "Position actual value", "Motor encoder sine signal", "Motor encoder cosine signal". Legend item "3: Command signal" = code no. and designation of the signals for analog output.

Legend: φ_com = desired position; φ_M = motor position; ω = motor speed; K_V = position loop gain factor; K_P = velocity loop proportional gain; T_N = velocity loop integral reaction time; K_DP = velocity loop derivative/proportional gain; T_GL = velocity loop smoothing time constant; T_SG = command value smoothing filter time constant; K_pi = current loop proportional gain; R_A = motor winding resistance; L_A = motor winding inductance; K_M = torque constant; c_ω = EMF constant; J_M = motor mass moment of inertia; i = motor current; m_d = motor torque; m_L = load torque.

## PDF page 31 (manual p. 107) — 9.6 Notes on component replacement

- Diagnostics display and message signals on the DDS 2.1 enable methodical fault location; replace defective components rather than repair on the machine.
- **The software module DSM 2.1 allows replacing the drive controller without re-parametrizing**: the new controller is ready as soon as the existing software module is plugged in.
- The new unit, motor or cable must have exactly the same type designation as the removed part. The configuration rating plate on the transparent protective plate of the drive controller carries all information needed to order a replacement from INDRAMAT or build one from single parts.
- If a drive controller fails, remove the (usually intact) software module DSM and plug it into the new drive module.
- When returning a unit to INDRAMAT Service, send the completed "Repair Card" (Fig. 88, Section 11.1) with a short fault report.

## PDF page 32 (manual p. 108) — 10. Overview of parameters

**Application parameters** (Description — Unit):
- Operating mode, 1 = torque / 2 = velocity loop
- Bipolar velocity limit value [rpm]
- Overload factor [%]
- Maximum command value for maximum velocity [V]
- Velocity at maximum command value [rpm]
- Command value smoothing filter time constant [ms]
- Position data scaling at the analog output [dgr/10V]
- Velocity data scaling at the analog output [rpm/10V]
- Line count of the incremental encoder
- Marker pulse offset
- Torque/current limit 1 to / 2 to / 3 to [%] (each)
- Selected error reaction [current code]

**Drive parameters**: Current loop proportional gain-1 [V/A]; Velocity loop proportional gain [mAs/rad]; Velocity loop integral reaction time [ms]; Velocity loop derivative/integral gain [mAs/rad]; Velocity loop derivative/proportional gain [uAs²/rad]; Smoothing time constant [usec]; Reset to standard parameters [Enter].

**Amplifier and motor parameters**: Amplifier parameters are set automatically. Motor parameters are set automatically for Series MDD motors ≥ 065: Motor type; Nominal torque [Nm]; Maximum speed of motor [rpm]; Mass moment of inertia [Kgm²]; Motor torque constant Km [Nm/A]; Peak motor current [A]; Nominal motor current [A].

## PDF page 33 (manual p. 109) — 11. Appendix, 11.1 Repair Card (Fig. 88)

Blank form "Repair Card for INDRAMAT equipment and components". Fields: Completed by; Company/Town; Date; component designation when replacing single components; SN (×2); Supply job no.; Shipment date; Machine manufacturer/Company; Type; Machine no.; if necessary commissioning date; Fault in axis no.; □ horizontal □ vertical □; Operating hours; Date fault occurred.
- Fault status: Fault ___; □ always present □ occurs sporadically □ occurs after ___ hrs □ occurs on impact/vibration □ is temperature-dependent □ other.
- Additional information (e.g. LED diagnosis messages in display).
- Cause of fault: □ not known □ faulty connection □ ext. short-circuit □ mech. damage □ loose cable connections □ other.
- Supplementary information — General details: □ no function □ drive running irregularly □ uncontrolled drive motion □ fault in one direction only □ burnt-out fuse on supply □ other. Related incidents: □ mechanical problems □ failure of power supply □ failure of controller □ motor failure □ cable break □ other. Controller, supply unit, amplifier, mains power: □ faulty control voltage □ mains power fuse burnt out □ defective fan □ defective bleeder resistor □ faulty power supply voltage □ connection bolt sheared off □ other.
- Control system: □ no function □ faulty display □ no command value output □ diagnosis □ dimensional shift in direction □ E-STOP circuit interrupted □ pos. control loop does not close □ programme sequence fault □ faulty internal auxiliary function (outputs) □ acknowledgements not accepted (inputs) □ other. Motor: □ defective thermocouple □ defective brake □ defective fan □ defective feedback □ defective speed encoder signal □ defective BLC signal □ earthing short-circuit □ overheating □ other. Remarks.

## PDF page 34 (manual p. 110) — 11.2 Specific axis data list (Fig. 89)

Blank form "Specific Axis Data List — This list is intended as an additional safety record of the parameter contents. Include this list in the machine file." Fields: Machine manufacturer; Plant No.; Machine Type; Axis designation. Axis equipment: Drive controller: DDS 2.1……; Software module: DSM 2.1-……; MDD servo motor: MDD……; Supply module: ……. Date; Completed by; Approved by; Company. (Ref. ENA3A-Abb55/InbetrDDS.) **Not filled in** in this copy.

## PDF page 35 (manual p. 111) — Specific axis data list, page 2 (Fig. 90)

Blank tables (column "Recorded value" empty).
1. Application parameters: Operating mode (1=torque loop/2=velocity loop) [÷]; Bipolar velocity limit value [rpm]; Overload factor [%]; Maximum command value for maximum velocity [V]; Velocity at maximum command value [rpm]; Command value smoothing filter time constant [ms]; Line count of the incremental encoder [÷]; Marker pulse offset [dgr]; Torque/current limit 1, 2, 3 [%]; Selected error reaction [current code].
2. Drive parameters: Current loop proportional gain-1 [V/A]; Velocity loop proportional gain [mAs/rad]; Velocity loop integral reaction time [ms]; Velocity loop derivative/proportional gain [µAs²/rad]; Velocity loop derivative/integral gain [mAs/rad]; Smoothing time constant [µsec]; Nominal torque [Nm]; Maximum speed of motor (nmax) [rpm]; Mass moment of inertia [kgm²]; Motor torque constant [Nm/A]; Peak motor current [A]; Nominal motor current [A]. (Ref. ENA3A-Abb56/InbetrDDS.)

## PDF page 36 (manual p. 112) — 11.3 Example of a parameter file printout (Fig. 91)

**Generic Indramat specimen, not the CTX twin's data** (Machine manufacturer: XY-AG; Plant No. 1234; Machine type: Machining center; Axis: X axis). Shows the type-code format:
- Drive controller: **DDS 2.1-W025-D**
- MDD servo motor: **MDD065A-N-040**
- Software module: **DSM 2.21-E11-01.03 (16.10.92)**

| Ident | Parameter | Value | Unit |
|---|---|---|---|
| | *OPERATING MODES, SCALING* | | |
| S-0-0032 | Operating mode | 2 | --- |
| S-0-0091 | Bipolar velocity limit value | 2500.0000 | rpm |
| P-0-0006 | Overload factor | 100 | % |
| P-7-0000 | Maximum command value for maximum velocity | 10.0 | V |
| P-7-0001 | Velocity at maximum command value | 1500 | rpm |
| P-7-0004 | Command value smoothing filter time constant | 20.00 | ms |
| P-0-0042 | Position data scaling at analog output | 90.0 | dgr/10V |
| P-0-0040 | Velocity data scaling at analog output | 1500 | rpm/10V |
| | *ABSOLUTE ENCODER SETUP* | | |
| P-7-0015 | Reference position | 0.5000 | rpm [unit as printed] |
| | *ERROR REACTION* | | |
| P-0-0007 | Error reaction | 0 | --- |
| | *TORQUE/CURRENT LIMIT* | | |
| P-7-0005 | Torque/current limit 1 | 100 | % |
| P-7-0006 | Torque/current limit 2 | 50 | % |
| P-7-0007 | Torque/current limit 3 | 25 | % |
| | *GAIN PARAMETERS* | | |
| S-0-0106 | Current loop proportional gain-1 | 9.60 | V/a |
| S-0-0100 | Velocity loop proportional gain | 1500 | mAs/rad |
| S-0-0101 | Velocity loop integral reaction time | 50.0 | ms |
| P-0-0002 | Velocity loop derivative/integral gain | 0 | mAs/rad |
| P-0-0003 | Velocity loop derivative/proportional gain | 0 | µAs²/rad |
| P-0-0004 | Smoothing time constant | 1000 | µs |

"Date: completed by: Fr. 13.11.1992 Mr. A.N. Other". (Ref. ENA3A-Abb71/InbetrDDS.)

## PDF page 37 (manual p. 113)

Blank page (header "11. Appendix" only).

## PDF page 38 (manual p. 114) — 11.4 DDS 2.1 drive controller connection diagram for motors with resolver feedback (Fig. 92)

Caption: "Connection diagram for DDS 2.1-...-R— drive controller for motors with resolver feedback." Drawing ref. ENA3A-Abb90/InbetrDDS. Drawn rotated 90° on the page.

**Circuit description.** The dash-dot frame "Drive controller DDS 2.1 - .... - R..." contains plug-in slots U1 "Command communication module", U2/U3/U4 "Additional plug-in module", and U5 "Programming module DSM" ("Connection of optional interface modules according to the related connection diagrams"). The drive gets its control voltages from the supply module via the 12-way bus cable on X1 ("Bus connecting cable", "Signal conditioning (12-way bus cable)", forwarded to further modules: "Connection of other modules via electrical connection accessories"; "Drive component voltage supply through electrical connection accessories"; earth wire to "Supply module"; one bus-cable wire marked "black"). DC link 300 V DC enters on X5 L-/L+ from busbars ("300 VDC supply voltage (busbars)", "DC supply voltage (busbars)" looped on to the next module). Motor power leaves on X5 A1/A2/A3 + PE; brake and thermistor on X6; resolver on X4 via pre-assembled cable IKS 374. An external 24 V power supply ("~ / 24V", outputs 24V_Ext, 0V_Ext) feeds X6 UB/0VB for the holding brake.

Connections:

| From (drive) | Signal | To | Notes |
|---|---|---|---|
| X1/1 | UD | bus cable | error message from supply module |
| X1/2 | BB | bus cable | ready message to supply module |
| X1/3 | +15V | bus cable | |
| X1/4-7 | 0VM | bus cable | |
| X1/8,9 | -15V | bus cable | |
| X1/10 | 0VL | bus cable | |
| X1/11 | +24V | bus cable | |
| X1/12 | ⏚ | bus cable (wire "black") | |
| X2/1,2,3,4,5,10 | 0VM, TxD, RxD, RTS, CTS, ⏚ | RS 232 interface | |
| X3/1 | AK1 | analog diagnostics output (op-amp output) | |
| X3/2 | 0VM | | |
| X3/3 | AK2 | analog diagnostics output | |
| X3/4 | 0VM | | |
| X3/5 | ⏚ | | |
| X3/6, X3/7 | Bb, Bb | potential-free relay contact "ready" | |
| X3/8, X3/9 | AS+, AS- | "Safety lockout" input: relay coil with diode | |
| X3/10, X3/11 | ASQ, ASQ | potential-free contact, acknowledge of safety lockout | |
| X5 L-, L+ | DC link | busbars, 300 V DC | |
| X5 A1 | phase 1 | motor connector pin A | |
| X5 A2 | phase 2 | motor connector pin B | |
| X5 A3 | phase 3 | motor connector pin C | |
| X5 ⏚ | PE | motor connector pin D; power cable shield to PE | |
| X6/4 | BR (+24 V for brake) | motor pin F | holding brake coil with diode + varistor (U) |
| X6/5 | 0VB | motor pin G | brake 0 V |
| X6/1 | TM+ | motor pin E | PTC thermistor |
| X6/2 | TM- | motor pin H | PTC thermistor |
| X6/3 | UB | external 24 V supply 24V_Ext | |
| X6/6 | 0VB | external supply 0V_Ext | |

Resolver feedback cable (pre-assembled cable type **IKS 374**; labels IN 290 = drive-side connector, IN 209 = [uncertain, points to the inner shield/cable], IN 513 = motor-side connector):

| X4 pin | Signal | Wire colour | Motor feedback connector pin |
|---|---|---|---|
| 1 | ⏚ (shield) | — | 9 [uncertain, shield drawn to pin 9] |
| 8 | SDO | vio | 7 |
| 15 | SDI | bl | 4 |
| 7 | SCL | rt | 2 |
| 14 | FS | sw | 3 |
| 12 | R1 | bn 1? [uncertain, maybe a cross-section mark] | 12 |
| 10 | S2 | gr | 8 |
| 3 | S4 | rs | 1 |
| 9 | S1 | bn | 5 |
| 2 | S3 | gn | 6 |
| 4 | 0VM/R3 | ws 1? [uncertain] | 10 |

(Colours: vio violet, bl blue, rt red, sw black, bn brown, gr grey, rs pink, gn green, ws white.) Note: the diagram shows SDO/SDI wired, while the resolver pin table (Fig. 98) lists X4/8 and X4/15 as "not used".

## PDF page 39 (manual p. 115) — 11.5 DDS 2.1 connection diagram for motors with digital servo feedback (Fig. 93)

Caption: "Connection diagram for DDS 2.1-...-R— drive controller for motors with digital servo feedback" [as printed; the drawing itself is labelled "Drive controller DDS 2.1 - .... - D..."]. Drawing ref. ENA3A-Abb91/InbetrDDS. Identical to Fig. 92 (X1, X2, X3, X5, X6, U1-U5, 24 V brake supply) except the feedback block is "Digital servo feedback" and X4 signals are:

| X4 pin | Signal | Wire colour | Motor feedback connector pin |
|---|---|---|---|
| 1 | ⏚ | — | 9 [uncertain] |
| 8 | SDO | vio | 7 |
| 15 | SDI | bl | 4 |
| 7 | SCL | rt | 2 |
| 14 | FS | sw | 3 |
| 12 | U_G | bn 1? [uncertain] | 12 |
| 10 | C+ | gr | 8 |
| 3 | C- | rs | 1 |
| 9 | S+ | bn | 5 |
| 2 | S- | gn | 6 |
| 4 | 0VM | ws 1? [uncertain] | 10 |

Same cable IKS 374 (IN 290 / IN 209 / IN 513).

## PDF page 40 (manual p. 116) — 11.6 Pin allocations/abbreviations on drive controller

**Plug-in terminal strip X1 (Fig. 94: Control voltage bus on the DDS 2.1)**

| Pin | Signal | Meaning |
|---|---|---|
| X1/1 | UD | Error message from supply module |
| X1/2 | BB | Ready message from drive controller to supply module |
| X1/3 | +15V | 15 V supply voltage |
| X1/4 to X1/7 | 0VM | 0 Volt test voltage |
| X1/8 to X1/9 | -15V | -15V supply voltage |
| X1/10 | 0VL | 0V load voltage |
| X1/11 | +24V | +24V supply voltage |
| X1/12 | ⏚ | Earth |

**X2 (Fig. 95: RS 232 interface on the DDS 2.1)**

| Pin | Signal | Meaning |
|---|---|---|
| X2/1 | 0VM | 0V test voltage |
| X2/2 | TXD | Transmit data |
| X2/3 | RXD | Receive data |
| X2/4 | RTS | Request to send |
| X2/5 | CTS | Clear to send |
| X2/10 | ⏚ | Earth |

**X3 (Fig. 96: Analog inputs and outputs on the DDS 2.1)**

| Pin | Signal | Meaning |
|---|---|---|
| X3/1 | AK1 | Analog output channel 1 |
| X3/2 | 0VM | 0V test voltage |
| X3/3 | AK2 | Analog output channel 2 |
| X3/4 | 0VM | 0 V test voltage |
| X3/5 | ⏚ | Earth |
| X3/6, X3/7 | Bb | Potential-free signal contact "Ready" |
| X3/8 | AS+ | Safety lockout |
| X3/9 | AS- | Safety lockout |
| X3/10 | ASQ | Accept safety lockout |
| X3/11 | ASQ | Accept safety lockout |

**X4 Digital feedback (Fig. 97: Connections of the digital servo feedback on the DDS 2.1)**

| Pin | Signal | Meaning |
|---|---|---|
| X4/1 | ⏚ | Earth |
| X4/2 | S- | Signal conductor |
| X4/3 | C- | Signal conductor |
| X4/4 | 0VM | 0V test voltage |
| X4/7 | SCL | Signal conductor |
| X4/8 | SDO | Signal conductor |
| X4/9 | S+ | Signal conductor |
| X4/10 | C+ | Signal conductor |
| X4/12 | UG | Signal conductor |
| X4/14 | FS | Signal conductor |
| X4/15 | SDI | Signal conductor |

## PDF page 41 (manual p. 117)

**X4 Resolver feedback (Fig. 98)**

| Pin | Signal | Meaning |
|---|---|---|
| X4/1 | ⏚ | Earth |
| X4/2 | S3 | Signal conductor |
| X4/3 | S4 | Signal conductor |
| X4/4 | 0VM/R3 | 0V test voltage |
| X4/5 | not used | |
| X4/6 | not used | |
| X4/7 | SCL | |
| X4/8 | not used | |
| X4/9 | S1 | Signal conductor |
| X4/10 | S2 | Signal conductor |
| X4/11 | not used | |
| X4/12 | R1 | Signal conductor |
| X4/13 | not used | |
| X4/14 | FS | |
| X4/15 | not used | |

**X5 (Fig. 99: Mains power connections on the DDS 2.1)**

| Pin | Signal | Meaning |
|---|---|---|
| X5/1 | A1 | Phase 1 motor connection |
| X5/2 | A2 | Phase 2 motor connection |
| X5/3 | A3 | Phase 3 motor connection |
| X5/4 | L- | Negative DC link circuit voltage |
| X5/5 | L+ | Positive DC link circuit voltage |

**X6 (Fig. 100: Connections for motor temperature monitoring, holding brake on the DDS 2.1)**

| Pin | Signal | Meaning |
|---|---|---|
| X6/1 | TM+ | Positive temperature sensing conductor |
| X6/2 | TM- | Negative temperature sensing conductor |
| X6/3 | UB | External +24V operating voltage |
| X6/4 | BR | +24V connection for brake |
| X6/5 | 0VB | 0V connection for brake |
| X6/6 | 0VB | External 0V operating voltage |

## PDF page 42 (manual p. 118) — Plug-in terminal X13, X14 (Fig. 101: Command communication module DAE 1.1)

DAE 1.1 = ANALOG interface with incremental encoder emulator (see glossary).

| Pin | Signal | Meaning |
|---|---|---|
| X13/1 | E1 NEGIN | Analog input 1 (-10V) |
| X13/2 | E2 POSIN | Analog input 2 (+10V) |
| X13/3 | TVW | Overtemperature warning |
| X13/4 | RF | Controller enable |
| X13/5 | AH | Drive halt |
| X13/6 | Ired1 | Current reduction 1 (torque reduction 1) |
| X13/7 | Ired2 | Current reduction 2 (torque reduction 2) |
| X13/8 | +UI, ext | External +24V operating voltage |
| X13/9 | 0VI, ext | |
| X13/10 | shield | Shield |
| X14/1 | Ua2 [overbar probably lost in scan] | (Negated square-wave pulse) |
| X14/2 | not used | |
| X14/3 | Ua0 | (Square-wave pulse) Marker pulse |
| X14/4 | Ua0 [negated] | (Negated square-wave pulse) Marker pulse |
| X14/5 | Ua1 | (Square-wave pulse) |
| X14/6 | Ua1 [negated] | (Negated square-wave pulse) |
| X14/7 | not used | |
| X14/8 | Ua2 | [meaning blank; = square-wave pulse channel 2] |
| X14/9 | shield | Shield |
| X14/10 | 0Vext | Supply voltage +0V |
| X14/11 | not used | |
| X14/12 | +5Vext | Supply voltage +5V |
| X14/13 | not used | |
| X14/14 | not used | |
| X14/15 | not used | |

## PDF page 43 (manual p. 119) — Fig. 102: Command communication module DAA 1.1

Side heading printed "Plug-in terminal X13, X14" [misprint; the table is X15/X16]. DAA 1.1 = ANALOG interface with absolute encoder emulator (SSI).

| Pin | Signal | Meaning |
|---|---|---|
| X15/1 | E1 NEGIN | Analog input 1 (-10V) |
| X15/2 | E2 POSIN | Analog input 2 (+10V) |
| X15/3 | TVW | Overtemperature warning |
| X15/4 | RF | Controller enable |
| X15/5 | AH | Drive halt |
| X15/6 | Ired1 | Current reduction 1 (Torque reduction 1) |
| X15/7 | Ired2 | (Torque reduction 2) |
| X15/8 | +UI, ext | External +24V operating voltage |
| X15/9 | 0VI, ext | |
| X15/10 | shield | Shield |
| X16/1-4 | not used | |
| X16/5 | DATA(+) | |
| X16/6 | DATA(-) | |
| X16/7-9 | not used | |
| X16/10 | 0Vext | Supply voltage |
| X16/11 | +Uext | Supply voltage +24V |
| X16/12 | not used | |
| X16/13 | clock pulse (+) | Clock line |
| X16/14 | clock pulse (-) | Clock line |
| X16/15 | shield | Shield |

## PDF pages 44-47 (manual pp. 120-123) — 11.7 Overview of INDRAMAT companies, subsidiaries and agencies

Address list (1993), not relevant to the machine. Germany: Indramat GmbH Lohr (Bgm.-Dr.-Nebel-Str. 2, D-8770 Lohr am Main, tel. 0 93 52/40-0, telex 6 89 421, fax 0 93 52/40-4885); Chemnitz; Düsseldorf (Techn. Büro Ratingen); München; Stuttgart (Leonberg). Further entries: Austria (Wien, Pasching), Belgium, Croatia, Czechoslovakia, Denmark, England (G.L. Rexroth Ltd, Cirencester), **Finland (Rexroth OY, Riihimiehentie 3, SF-01720 Vantaa, tel. 0/84 85 11, fax 0/84 63 87)**, France (Gennevilliers, Venissieux, Toulouse), Italy (Cernusco, Torino, Bologna), Netherlands, Spain, Sweden (Zander & Ingeström, Stockholm), Switzerland, Slovenia, Turkey, Yugoslavia, CIS (Moscow), Canada, Mexico, USA (Wood Dale IL; Rochester Hills MI), Argentina, Brazil, China, Hongkong, India, Japan, Korea, Taiwan, Thailand, Australia, New Zealand. (Full addresses as printed; omitted here as historical and irrelevant.)

## PDF pages 48-51 (manual pp. 124-127) — 12. Glossary, explanations

Key entries (abridged, wording as printed):
- **Absolute encoder emulator**: integral part of ANALOG interfaces with absolute encoder emulator **DAA 1.1**; outputs the drive's internal absolute position via **SSI** (synchronous serial interface) as position actual value.
- **Incremental encoder emulator (DAE)**: integral part of "ANALOG interfaces with incremental encoder emulator **DAE 1.1**"; outputs the drive's internal position for use by the NC as position actual value signal.
- **Digital servo feedback (DSF)**: high-resolution rotor position encoder (resolution 1/2 000 000 rev), absolute within one revolution.
- **Resolver feedback (RSF)**: cyclically absolute rotor position measurement (resolver principle).
- **Pulse wire absolute encoder (IDG)**: option for resolver feedback, absolute over 4096 revolutions.
- **Multiturn encoder (MTG)**: absolute over 4092 [as printed] revolutions.
- **High-resolution position interface (DLF)**: sinusoidal encoder signals conditioned at multiplication factor 2048.
- **Holding brake**: electromagnetic, closed when de-energized; "It is not intended as a brake in the event of an E-STOP!"
- **Controller enable**: "Analog interface RF" — signal to activate the drive when power has been switched on.
- **Velocity control loop**: operating mode with SERCOS and ANALOG interfaces, drive sets speed as function of velocity command. **Torque control loop**: only recommended for master/slave.
- **Bleeder**: electronically controlled braking resistor. **Continuous power rating at energy recovery**: via bleeder or, with regenerative supply units, back into the mains.
- **Software module (DSM)**: contains operating software and drive parameters. **Peak current**: max. current for 300 ms overload. **Status indicator**: two-character display (H1).
- Further entries: Absolute encoder, Drive controller, Application/Drive-related parameters, Emulator, Base torque, Incremental encoder, Interface (plug-in card), Kv factor (velocity in m/min at 1 mm lag error), Position control loop, Surface cooling, Position interface, Radial surface cooling, Reference point run, Resolver, Lag error, Vibration severity grade, Servo feedback, Signal conditioner, Singleturn encoder, Gear encoder interface.

## PDF pages 52-53 (manual pp. 128-129) — 13. List of keywords

Index referring to manual pages 12-95 (which are not in this part). Notable entries: ANALOG interface 13, 24; Command communication module 17, 24; DC link circuit connection (X5) 22; Incremental encoder emulation 73, 24; Incremental encoder resolution 73; SERCOS interface 12; SSI interface 24; Status indicator (H1) 22; Type code … 19, 23, 25, 27, 30; Torque reduction 82; Weight compensation 93; Wiring 53.

## PDF page 54 — Back cover

"DOK DDS2.1 MIT ANALOGINTERFACE APPL EN" (documentation DDS 2.1 with analog interface, application manual, English), 209-0069-4315, barcode 257 182, INDRAMAT logo, "Indramat GmbH • Bgm.-Dr.-Nebel-Straße 2 • D-97816 Lohr • Telefon 09352/404-0 • Tx 689421 • Fax 09352/404-885".

---

## For the retrofit

These pages are generic Indramat manual content: they give no CTX-twin-specific axis assignment, module type codes, cable numbers or parameter values. Look for those in pages 1-27 and the Gildemeister wiring sheets. What these pages do establish:

**Keeping the Indramat DDS 2.1 drives under LinuxCNC**
- **Drive**: Indramat DDS 2.1 digital AC servo controller (type code e.g. `DDS 2.1-W025-D`, suffix -R = resolver motor, -D = digital servo feedback motor), with an MDD servo motor (e.g. `MDD065A-N-040`). Parameters are held in the plug-in software module **DSM 2.1** (slot U5), so a drive swap keeps the parameters if the DSM moves with it. **Read out and record all parameters before any work** (form Fig. 89/90; menu items listed on pp. 32/36). This needs a PC/VT100 on RS 232 X2 and the Indramat parametrization program.
- **Command interface: analog ±10 V velocity command** on the command communication module in slot U1: X13/1 E1 NEGIN / X13/2 E2 POSIN (differential, DAE 1.1) or X15/1-2 (DAA 1.1). Operating mode S-0-0032 = 2 (velocity loop). Scaling: P-7-0000 "max command value" (V) ↔ P-7-0001 "velocity at max command" (rpm). There is no SERCOS on these drives (the SERCOS interface is a different U1 module). This interface is directly compatible with a LinuxCNC ±10 V analog output card (Mesa 7i77/7i83, etc.) with position loop closed in LinuxCNC (PID, velocity mode).
- **Position feedback to the control**:
  - DAE 1.1 (incremental encoder emulation) on X14: differential square-wave TTL/RS-422 Ua1/Ua1̅ (X14/5,6), Ua2/Ua2̅ (X14/8,1), marker Ua0/Ua0̅ (X14/3,4). The output needs **+5 V external supply on X14/12 and 0 V on X14/10** from the control. The line count is a drive parameter ("Line count of the incremental encoder"), as is the marker pulse offset. Can be wired straight to Mesa encoder inputs.
  - DAA 1.1 (absolute encoder emulation, SSI) on X16: DATA± X16/5,6, clock± X16/13,14, +24 V X16/11. This needs an SSI-capable input (e.g. Mesa SSI firmware).
  - Which module (DAE or DAA) each CTX axis has must be checked on the drive type plate or in the wiring sheets.
- **Control signals (X13 / X15, 24 V logic with external supply +UI ext X13/8, 0VI ext X13/9)**: RF controller enable (X13/4) → LinuxCNC `amp-enable`; AH drive halt (X13/5); Ired1/Ired2 torque reduction (X13/6,7, limits P-7-0005..0007); TVW overtemperature warning output (X13/3).
- **Ready / fault**: X3/6-7 Bb = potential-free "ready" contact → LinuxCNC `amp-fault` input / E-stop chain. Bus X1/1 UD (supply-module error) and X1/2 BB (ready to supply module) run internally between the modules on the 12-way bus cable.
- **E-stop / safety**: X3/8-9 AS+/AS- "safety lockout" (drive interlock, relay input), X3/10-11 ASQ acknowledge contact. The original machine probably wires these into the E-stop/door chain; keep them in the LinuxCNC E-stop chain. Note: the holding brake is not an E-stop brake (glossary).
- **Brake**: X6/4 BR / X6/5 0VB is switched by the drive from an external 24 V supply on X6/3 UB / X6/6 0VB. Motor thermistor PTC on X6/1-2 (TM+/TM-), monitored by the drive.
- **Power**: DC link 300 V DC on X5 L+/L- from busbars of a separate Indramat supply module (TVD/KDV type, not named on these pages). Control voltages ±15 V/+24 V come over the X1 bus. The supply module, its bleeder and its ready/UD logic must stay in place.
- **Diagnostics**: AK1/AK2 analog outputs on X3/1-4 (velocity/position/current/encoder sin-cos, scalable) are useful for tuning with a scope or HAL. Two-digit status display H1 (e.g. "92" absolute encoder not calibrated).

**Replacing the drives**
- MDD motors are synchronous AC servos with **resolver (RSF)** or **digital servo feedback (DSF**, sin/cos + serial SCL/SDO/SDI, absolute within one revolution), connected via cable IKS 374 (motor connector pins per tables above). A replacement drive must support Indramat resolver or DSF (sin/cos 1 Vpp + Indramat serial protocol) and the commutation offset. In practice this means a Rexroth/Indramat successor (DKC/ECODRIVE, IndraDrive) or a drive with resolver input plus a commutation offset adjustment. The DSF serial data channel is proprietary.
- Motor power: A/B/C/PE pins; brake F/G (24 V, diode + varistor in the motor); PTC E/H.
- If LinuxCNC drives the new amplifiers, use velocity or torque mode with the encoder read by LinuxCNC (or the replacement drive's encoder emulation).
