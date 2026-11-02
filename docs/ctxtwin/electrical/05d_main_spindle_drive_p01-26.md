# Gildemeister CTX twin, electrical documentation: 5 Main spindle drive (part d), PDF pages 1-26

Source: `08cabf3f-5_Main_spindel_drive_d_20201028_0001.pdf` (scan dated 2020-10-28, file name "5 Main spindel drive d"), PDF pages 1-26 of 52.
Document: these pages are **not** Gildemeister circuit diagrams. They are printed pages 115-140 of an INDRAMAT main spindle drive manual (English edition) covering the **KDA / TDA / RAC** main spindle drive family: end of chapter 3 "Controller functions", chapter 4 "Diagnostics and fault clearance", chapter 5 "Overview of parameters". No title block, document number, edition date or Gildemeister drawing number appears on any of these pages. The running page headers are "Controller functions", "Diagnostics and fault clearance" and "Overview of parameters". Figure numbers 96-104.
Original language: English (Indramat translation). No German text on these pages, so no translations are needed.
Note: the parameter list forms (PDF pages 24-25) are **blank**. No machine-specific parameter values are recorded in this part.

## Page index

| PDF page | Printed page | Content |
|---|---|---|
| 1 | 115 | 3.20 Performance during mains failure and emergency stop (KDA, TDA); 3.21 Starting lockout in KDA/TDA |
| 2 | 116 | Fig. 96: control inputs for starting lockout in KDA, internal circuit; notes, safety note |
| 3 | 117 | Chapter 4 divider: Diagnostics and fault clearance (4.1, 4.2) |
| 4 | 118 | 4 / 4.1 Operating status diagnostics: DISABLED, E-STOP, HOMING, IN POS |
| 5 | 119 | Status: NO POWER, NO RF, NO RUN, N = NCMD, N * NCMD, N < NMIN |
| 6 | 120 | Status: OSCILLATE, POS ORDER, SLAVE |
| 7 | 121 | 4.2 Fault diagnostics: ADW2, BLEEDER, BS, DACFAULT |
| 8 | 122 | Faults: DC FAULT, EARTH CON, EEPROM, FEEDBACK, GEAR BOX |
| 9 | 123 | Faults: HOMING, KK TEMP, KK WARN, MOT TEMP |
| 10 | 124 | Faults: MOT WARN, MOTOR 1/2, NO INPUT, NO MAINS, NO RAC 2.2, NO TEMP |
| 11 | 125 | Faults: NO 8 MHz / NO 12 MHz, NO 8 V, NO 15 V, NO 24 V, NO 24 Vex |
| 12 | 126 | Faults: OVER VOLT, PARA LOST, PHASE, POS ERROR, RAC TYPE / KDA TYPE |
| 13 | 127 | Faults: RAM FAULT, RF AGAIN, RSK FAULT, RSKCURNT, SPINDLE, X12 ADW |
| 14 | 128 | Fault: 2X SIM |
| 15 | 129 | Chapter 5 divider: Overview of parameters (5.1, 5.2, 5.3) |
| 16 | 130 | 5.1 General parameters A01-A20 (Fig. 97) |
| 17 | 131 | Function table FUNCT 1 (A05) (Fig. 98) |
| 18 | 132 | Function table FUNCT 2 (A06) (Fig. 99) |
| 19 | 133 | 5.2 Switchable parameter records P/Q/R/S 01-21 (Fig. 100) |
| 20 | 134 | Function table PQ FUNCT (PQRS 16) (Fig. 101) |
| 21 | 135 | 5.3 Motor parameters M01-M16 (Fig. 102) |
| 22 | 136 | Function table MOTFUNCT (M15) (Fig. 103) |
| 23 | 137 | FEEDBACK fault and values of the FEEDBACK parameter M02 (Fig. 104) |
| 24 | 138 | PARAMETER LIST form (A, P, Q, R, S, M), blank |
| 25 | 139 | FUNCTION PARAMETER ADDITION LIST form, blank |
| 26 | 140 | Empty page (header "Overview of parameters" only) |

---

## PDF page 1 (p. 115): Controller functions

### 3.20 Performance during mains failure and emergency-stop (KDA, TDA)

Besides the main drive with KDA or TDA, servo drives of the modular drive system can also be connected to the same power supply module.

Modular drive system requirements:
- Stopping the servo drives has priority over stopping the main drive.
- The total power the drives regenerate while braking must not exceed the regenerated power rating of the supply unit. Otherwise the link circuit voltage rises to inadmissible levels.
- If the link circuit voltage reaches the permissible limit because of the power regenerated while the drives brake, the KDA/TDA interrupts braking of the main spindle drives. When the voltage drops, the main spindle drives return to braking mode.

### 3.21 Starting lockout in KDA/TDA

For safety reasons the main spindle should be stopped while machine tools are being set up, and sudden unintentional starting must be prevented. INDRAMAT main spindle drives therefore have a starting lockout facility:
- The modular system's main drive remains selectively and reliably shut down.
- Disconnection is safe even if the electronics malfunction, because the opto-couplers are switched off and the power bridge is inhibited.
- Even with defective power transistors, no rotating field can be generated while the starting lockout is active.

## PDF page 2 (p. 116): Fig. 96

**Fig. 96: Control inputs for starting lockout in KDA; internal circuit of controller** (drawing ID at bottom right: "Anlaufsperre-KDA" (starting lockout KDA)).

Legend (top left, small print): "1 ≙ +24 V", "0 ≙ 0 V", a step symbol "Signal step" [uncertain], a dashed-box symbol "Any output state" [uncertain].

Terminal strip **X2** of the control unit (KDA), as drawn:

| X2 terminal | Signal | Direction |
|---|---|---|
| 23 | E-STOP (RAC only) | control input |
| 24 | RF (controller enabling signal) | control input |
| 25 | RUN | control input |
| 26 | POS 1 | control input |
| 27 | POS 2 | control input |
| 28 | MD-RED | control input |
| 29 | OSCILLATE | control input |
| 30 | PAR 1 | control input |
| 31 | PAR 2 | control input |
| 32 | LIMIT 1 | control input |
| 33 | LIMIT 2 | control input |
| 34 | LIMIT 4 | control input |
| 35 | SPEED 1 | control input |
| 36 | SPEED 2 | control input |
| 37 | EXT POS | control input |
| 3 | AS (KDA only), starting lockout | control input. Left margin: "0 ≙ Starting lockout active" |
| 4 | READY | signal output. Left margin: "0 ≙ Drive not ready" |

Internal circuit, as described in the drawing: input X2/3 (AS) drives a relay coil inside the control unit, referenced to "0V ext or 0V int". The relay has two contacts:
1. One contact (from +5 V) gives the "DISABLED" signal to the processor.
2. The other contact (from +15 V) switches the supply of the opto-coupler drivers. The drawing shows the chain opto-coupler, diode, transistor labelled "Drive for power circuits", with "6x" (one per power transistor of the bridge).
With AS = 0, the relay drops out, the opto-couplers lose supply and no gate signals can reach the power bridge.

Notes:
- If the AS control input is at 0, the display shows DISABLED and the starting lockout is activated.
- If AS goes from 0 to 1, the controller enabling signal must be applied again (RF goes from 0 to 1) to start the drive.
- If the starting lockout is active, the READY signal output goes to 0. The Bb potential-free contact stays closed, so the other control units of the drive package are not shut down.

Safety note: remove the connector plug from terminal strip X2 to make sure the AS input is 0. **The starting lockout (AS) does not meet personnel safety requirements in the vicinity of the main spindle.**

## PDF page 3 (p. 117): chapter divider

4. Diagnostics and fault clearance. 4.1 Operating status display. 4.2 Fault diagnostics.

## PDF pages 4-6 (pp. 118-120): 4 / 4.1 Operating status diagnostics

The control unit shows main spindle drive operating states and faults in its display. Requirement: drive in operating mode (mode switch to the right). In 4.1 the green "READY" LED comes on and the operating states appear in the display.

| Status message | Meaning (as printed) | See section |
|---|---|---|
| DISABLED (KDA only) | Drive free of torque, power output stage inhibited. "The AS (X2/3) control input is at 1." [as printed; contradicts p. 116, where AS = 0 means DISABLED] | 3.20 [as printed; the starting lockout is 3.21] |
| E-STOP (RAC only) | Drive free of torque at standstill. E-STOP control input (X2/23) is not at 1. | 3.18 |
| HOMING | Drive searches for the spindle reference signal at the speed in parameter P-MAXRPM, or for the spindle feedback or motor feedback zero pulse at the speed in parameter POS RPM (PQRS 18). | 3.6 |
| IN POS | Spindle or rotor position has reached the position set-point within tolerance. Tolerance = parameter P-WINDOW (PQRS 21). | 3.6 |
| NO POWER | Link circuit voltage in the control unit is below 400 V. | 3.1 |
| NO RF | Controller enabling signal not present. RF control input (X2/24) is not at 1. | 3.7 |
| NO RUN | Applied speed set-point is switched off internally, drive is controlled to zero speed without drift. RUN control input (X2/25) is at 0. | 3.6.1 |
| N = NCMD | Motor speed has reached the speed set-point within tolerance (MAX DEV (A04)). | 3.3 |
| N * NCMD (N ≠ NCMD) | Motor speed deviates from set-point by more than the tolerance MAX DEV (A04). Note 1: appears while accelerating and braking if the set-point changes abruptly, and also when the drive has pure P control under load. | 3.3 |
| N < NMIN | Motor speed is below parameter MIN RPM (A03). | 3.3 |
| OSCILLATE | Motor speed oscillates at the speed in parameter OSCI RPM (A20) to support gear change. OSCILLATE control input (X2/29) is at 1. | 3.15.2 |
| POS ORDER | Drive has received a position command via POS 1 (X2/26) and POS 2 (X2/27) or the digital position input (optional) and approaches the set-point position. | 3.6 |
| SLAVE | Drive is parameterized as slave drive and now operates in torque control mode. | 3.17 |

## PDF pages 7-14 (pp. 121-128): 4.2 Fault diagnostics

The red "FAULT" LED comes on and the fault messages appear in the display. After the fault has been remedied, cancel the message with the red accept key (reset).

| Fault message | Cause | Remedy | See |
|---|---|---|---|
| ADW2 | Plug connector X7 on the CDR board of the control unit has an ADW2 analogue speed set-point interface fitted. | Switch off. Replace ADW2 by ADW3. Contact INDRAMAT service. | |
| BLEEDER | Bleeder monitor tripped due to overheating. Bleeder in control unit faulty or not fitted. | Check bleeder resistor in the upper part of the control unit (visual inspection). If OK, allow a cooling-down period (approx. 15 min). If the unit has no "additional bleeder" option, <256> is wrongly included in FUNCT2 (A06). | |
| BS (KDA only) | Limiting current set on the electronic cut-out has been exceeded. | Check motor cable for short circuit or earth fault. Check motor for shorted windings or earth fault. | |
| DACFAULT | Fault on the CDR board. | Switch off. Replace CDR board. Contact INDRAMAT service. | |
| DC FAULT | DC link circuit does not charge properly when the power contactor is energized. | Check supply cable is secure at the mains terminals. Remove a possible short circuit in the DC link (L+, L-) (KDA/TDA and RAC2 only). Otherwise contact INDRAMAT service. | |
| EARTH CON | Earth fault in motor cable or DC link circuit. | Switch off. Check motor cable (cubicle cable entry gland?), replace if necessary. Check motor. For KDA check the DC link wiring. | |
| EEPROM | No EEPROM (IC3) on the programming module, or the EEPROM is faulty. | Switch off. Plug in or replace the EEPROM. Contact INDRAMAT service. | |
| FEEDBACK | Wrong value in FEEDBACK parameter "(M03)" [as printed here; p. 137 and the M table give FEEDBACK = M02]. Faulty or wrong feedback cable. | Set the value that suits the motor feedback type. Repair or replace the feedback cable. | 5.3 |
| GEAR BOX | Gear box cannot be engaged, or the reduction stage acknowledgement signal is faulty. | Switch off. Check contactor wiring and acknowledgement contacts. Check the change gear works. | 3.15.1 |
| HOMING (does not go off after spindle rotates) | No spindle reference signal at EXT POS control input (X2/37). No spindle or motor feedback zero pulse. | Check connections and closing angle of the spindle reference switch. Check feedback leads. | 3.6 |
| KK TEMP | Heatsink of the control unit overheated. TEMPWARN signal (X2/9) was ignored. Control unit has shut down. | Check unit fan and airways, ambient temperature, and the link between TEMPWARN output (X2/9) and the controller. Reset required. | 3.11 |
| KK WARN | Heatsink has reached the cut-off temperature. TEMP WARN output (X2/9) has switched from 1 to 0. | The drive must be clearly unloaded: if the heatsink temperature rises further, the drive switches off **after 30 s**. Check fan, airways and load cycle. | 3.11 |
| MOT TEMP | Motor winding temperature has reached **155 °C**. TEMPWARN was ignored, control unit switched off. | Check motor fan, airways, load cycle and the TEMPWARN-to-controller wiring. Reset required. | 3.11 |
| MOT WARN | Motor temperature has reached **145 °C**. TEMPWARN output (X2/9) switched from 1 to 0. | Check motor fan, airways, load cycle. Cancel with red accept key. | 3.11 |
| MOTOR 1/2 | No motor contactor changeover acknowledgement. | Check wiring against the connection diagram. | 3.16 |
| NO INPUT | No set-point interface card plugged in, or the card is not recognized. | Switch off. Check set-point interface, replace if necessary. Contact INDRAMAT service. | |
| NO MAINS (RAC 3 only) | No voltage at mains terminals L1, L2, L3, or voltage too low. | Check mains supply. | |
| NO RAC 2.2 (RAC 2.1 only) | Software and equipment do not match. | Use correct programming module (AS4./...). | |
| NO TEMP | Broken connection between motor temperature sensor and control unit, or faulty sensor (message appears approx. 10 min after the fault occurs). | Check sensor connections. If the sensor is faulty contact INDRAMAT service. | 3.11 |
| NO 8 MHz or NO 12 MHz | Software in programming module does not match the control unit. | Use the matching programming module. Contact INDRAMAT service. | |
| NO 8 V | Control unit 8 V supply faulty. | Contact INDRAMAT service. | |
| NO 15 V | Control unit ±15 V supply faulty. | Check bus connecting cable (in KDA). Contact INDRAMAT service. | |
| NO 24 V | Control unit ±24 V supply faulty. | Check bus connecting cable (in KDA). Contact INDRAMAT service. | |
| NO 24 Vex | No external voltage for the control inputs and signal outputs (X2/1 and 19), wrong polarity, or out of tolerance. | Check external +24 V: must be **18 V to 36 V**. Message clears automatically when OK. If no external +24 V is available, the internal +24 V / 0 V supply can be used (link +24 Vint/0 Vint to +24 Vext/0 Vext on X2). | 3.1 |
| OVER VOLT (RAC 2 and RAC 3 only) | Mains voltage exceeds rated device voltage by more than 15 %. Power contactor cannot be engaged. | Check mains. Use a matching transformer if necessary. | |
| PARA LOST | Programming module has no parameter values (not loaded or erased). | Slide the mode switch to the left and press the red button. This loads general base parameters with which the drive can run. Loading is complete when "BASISPAR" appears. Pressing the red button again puts the drive into parameter mode. Base parameters are not optimum and should be replaced by the optimized values (AS../...); refer to the parameter list supplied with the programming module. | |
| PHASE (RAC only) | No clockwise mains field, or one of three phases missing (phase failure in RAC 3 only). | Check mains. | |
| POS ERROR | Contouring error monitor tripped during spindle positioning with spindle reference switch. Drive cannot follow the position input. | Check whether the spindle is jammed or sluggish, or the coupling is faulty. Check whether torque reduction is active; switch it off (MD-RED (X2/28) = 0). Reduce the positioning rate (POSRPM). | |
| RAC TYPE or KDA TYPE | Value of CURRENT parameter (M06) exceeds the rated current of the control unit. | Set the value from the parameter list supplied with the programming module. | |
| RAM FAULT (RAC only) | Fault in RAM check. | Switch off. Replace CDR board. Contact INDRAMAT service. | |
| RF AGAIN | Drive waits for a new controller enable to run again. | RF (X2/24) must be switched 0 -> 1 again. | |
| RSK FAULT (RAC 3 only) | RSK board inoperative. | Switch off. If the fault persists after switching on, contact INDRAMAT service. | |
| RSKCURNT (RAC 3 only) | Overcurrent during regenerative feedback. | Check whether infeed voltage is below minimum. | |
| SPINDLE | No speed information from the spindle position encoder. | Check coupling between spindle and drive. Check wiring between spindle feedback and control unit. | 3.13 |
| X12 ADW | Connector strip X12 on the CDR board has an ADW2 analogue interface fitted. | Switch off. Replace ADW2 with the correct interface board. Contact INDRAMAT service. | |
| 2X SIM | Control unit has 2 serial interfaces fitted; only one is allowed. | Compare the interface configuration in the type code with the fitted interface; replace the wrong one. Contact INDRAMAT service. | |

Terminals referenced on X2 in this chapter: X2/1 and X2/19 (+24 Vext supply / reference for inputs and outputs), X2/3 AS, X2/4 READY, X2/9 TEMPWARN (output, 1 = OK, 1 -> 0 on KK WARN / MOT WARN), X2/23 E-STOP, X2/24 RF, X2/25 RUN, X2/26 POS1, X2/27 POS2, X2/28 MD-RED, X2/29 OSCILLATE, X2/37 EXT POS (spindle reference switch input).
Other connectors named: X4 (analogue inputs E1/E2 and E3/E4, see parameter tables), X7 and X12 on the CDR board (set-point interface slots), DC link L+/L-, mains L1/L2/L3.

## PDF page 15 (p. 129): chapter divider

5. Overview of parameters. 5.1 General parameters (A parameters). 5.2 Switchable parameters (P, Q, R, S parameters). 5.3 Motor parameters (M parameters).

## PDF page 16 (p. 130): 5.1 General parameters (A parameters), Fig. 97

The general parameters are always active while the drive operates. They can only be changed in parameter mode. When parameter mode is selected the drive brakes along the set ramps and is then free of torque.

| Parameter | Designation | Function | See chap. | Value range |
|---|---|---|---|---|
| A01 | MAX RPM | Maximum speed of motor | 3.2 | 1...24000 (1/rpm) |
| A02 | CMD VOLT | Analogue voltage for MAX RPM | 3.2 | 6...10 (V) |
| A03 | MIN RPM | Speed limit below which the signal output "N < MIN" goes to 1 | 3.3 | 1...999 (1/rpm) |
| A04 | MAX DEV | Max. deviation of speed from set-point within which the signal output "N = CMD" goes to 1 | 3.3 | 1...999 (1/rpm) |
| A05 | FUNCT 1 | See A05 function table | | Sum 0...9999 (Σ<>) |
| A06 | FUNCT 2 | See A06 function table | | Sum 0...9999 (Σ<>) |
| A07 | OFFSET | Angular offset of zero pulse | 3.6.1 | 0...360.0° (∢ degr.) |
| A08 | POS 1 | Spindle position 1 | 3.6.1 | 0...360.0° |
| A09 | POS 2 | Spindle position 2 | 3.6.1 | 0...360.0° |
| A10 | POS 3 | Spindle position 3 | 3.6.1 | 0...360.0° |
| A11 | ENCODER | Number of spindle encoder lines or Indramat encoder ID | 3.6.4 | 128, 256, 512, 1024, 2048, 4096, 8192, 3 "oder" (or) 4 |
| A12 | LIMIT 1 | Max. spindle speed 1 | 3.12 | 1...24000 (1/rpm) |
| A13 | LIMIT 2 | Max. spindle speed 2 | 3.12 | 1...24000 (1/rpm) |
| A14 | LIMIT 3 | Max. spindle speed 3 | 3.12 | 1...24000 (1/rpm) |
| A15 | LIMIT 4 | Max. spindle speed 4 | 3.12 | 1...24000 (1/rpm) |
| A16 | LIMIT 5 | Max. spindle speed 5 | 3.12 | 1...24000 (1/rpm) |
| A17 | LIMIT 6 | Max. spindle speed 6 | 3.12 | 1...24000 (1/rpm) |
| A18 | LIMIT 7 | Max. spindle speed 7 | 3.12 | 1...24000 (1/rpm) |
| A19 | BAUDRATE | Serial interface transmission speed | 3.23 | 50, 75, 110, 150, 300, 600, 1200, 2400, 4800, 9600, 19200 |
| A20 | OSCI RPM | Oscillation speed for gear change | 3.15 | 1...200 (1/rpm) |

Footer: "Parameter list!" (drawing ID "Para-allgemein" (general parameters)).
Note: enter the total of the summands (<>) assigned to the functions as the value of FUNCT 1 (A05) and FUNCT 2 (A06). An addition list is added to the parameter list for recording the summands.

## PDF page 17 (p. 131): Fig. 98, function table FUNCT 1 (A05)

Each row: function selected by the summand, or the alternative function with summand <0>.

| Summand | Function if summand set | Function if <0> | See chap. |
|---|---|---|---|
| <1> | Torque limiting if control input MD-RED is at 1 | Torque and power limiting if MD-RED is at 1 | 3.9 |
| <2> | Analogue speed set-point below MIN RPM (A03) is invalid (drift inhibit) | Analogue speed set-point always valid | 3.3 |
| <4> | Torque is disconnected if speed is below MIN RPM (A03) | No speed-related torque disconnection | 3.3 |
| <8> | An existing 2nd encoder input is ignored (type code field "2nd encoder input" = P) | Spindle positioning with 2nd encoder input, but without 2nd encoder input on motor shaft | 3.6.4 |
| <16> | Motor braking without overshoot (I-gain of N controller switched off relative to speed set-point) | Drive overshoots during braking because the I-gain of the N controller is permanently active | 3.7 |
| <32> | At switch-on, power P = UD * ID is switched to analogue output N | At switch-on, speed N is switched to analogue output N (basic state) | 2.2.2 |
| <64> | Master drive: analogue output N delivers the torque set-point continuously | Analogue output N selectable via the control panel (see 2.2.2) | 3.17 |
| <128> | At switch-on, motor temperature T is switched to analogue output N | At switch-on, speed N on analogue output N (basic state) | 2.2.2 |
| <256> | Signal output N = CMD remains active even without controller enable (RF = 0) | Signal output N = CMD active only while drive is running | 3.3 |
| <512> | Changed meaning of MAX DEV (A04): speed deviation is MAX DEV x 0.1 % of the current speed set-point | MAX DEV value means 1/rpm | 3.3 |

Footer: "Addition list!" (ID "Fkt-TabelleA05").

## PDF page 18 (p. 132): Fig. 99, function table FUNCT 2 (A06)

| Summand | Function if summand set | Function if <0> | See chap. |
|---|---|---|---|
| <1> | Value in LOAD LIM (PQRS 19) means % of peak torque | Value in LOAD LIM (PQRS 19) means power in kW | 3.10 |
| <2> | Type code field "speed set-point" = D: floating-point coding of speed set-point. Type code field "speed set-point" = A and "additional interface" = D: analogue speed set-point in parameter records P, Q, binary speed set-point in records R, S | Type code field "speed set-point" = D: binary-coded speed set-point | 3.2.2 |
| <4> | Type code field "2nd encoder input" = P: second encoder input used as motor feedback input for motor 2 | Type code field "2nd encoder input" = P: second encoder input used as spindle feedback | 3.16 |
| <8> | Slave mode: voltage at analogue input E1/E2 on terminal strip X4 is interpreted as torque set-point | Analogue input E1/E2 on X4 used as speed set-point input | 3.17 |
| <16> | Spindle positioning at speed POS RPM (PQRS 19 [as printed; POS RPM is PQRS 18 in Fig. 100]); direction of rotation corresponds to the sign of the speed set-point just applied | Spindle positioning at speed POS RPM (PQRS 19 [as printed]), shortest path | 3.6.1 |
| <32> | With EXT POS = 1, control input E-STOP 1 -> 0 and mains failure: set-point ramps or smoothing are active | With EXT POS = 1, E-STOP 1 -> 0 and mains failure: set-point ramps and smoothing are inoperative | 3.5 / 3.4 |
| <64> | For RAC 3 only: mains undervoltage correction active | For RAC 3 only: mains undervoltage correction inactive | |
| <128> | With EXT POS = 1: at analogue output N, 10 V corresponds to speed P MAX RPM (PQRS 15) | 10 V = speed MAX RPM (A01) | 3.2 |
| <256> | For RAC 2.2 only: bleeder monitor active | For RAC 2.2 only: bleeder monitor inoperative | 3.19 |
| <512> | Analogue output M changed: output of approximately calculated drive torque; analogue output N changed: torque set-point is output | Analogue outputs M and N not changed via function parameter (A06) | 3.10.1 |

Footer: "Addition list!" (ID "Fkt-TabelleA06").

## PDF page 19 (p. 133): 5.2 Switchable parameter records (P, Q, R, S), Fig. 100

The records P, Q, R and S can be selected while the drive operates, via control inputs PAR1 and PAR2 (X2/30, X2/31). Values may only be changed in parameter mode. When parameter mode is selected the drive brakes along the set ramp and is then free of torque. See section 3.14.

Each parameter exists four times: Pxx, Qxx, Rxx, Sxx.

| No. | Designation | Function | See chap. | Value range |
|---|---|---|---|---|
| 01 | RAMP 1 | Slope of speed set-point ramp 1 | 3.4; 3.5 | 0...999 (rad/s²) |
| 02 | RPM 1 | Final speed of ramp 1 | 3.4; 3.5 | 0...24000 (1/rpm) |
| 03 | RAMP 2 | Slope of speed set-point ramp 2 | 3.4; 3.5 | 0...999 (rad/s²) |
| 04 | RPM 2 | Final speed of ramp 2 | 3.4; 3.5 | 0...24000 (1/rpm) |
| 05 | RAMP 3 | Slope of speed set-point ramp 3 | 3.4; 3.5 | 0...999 (rad/s²) |
| 06 | MD-RED | Torque limiting | 3.9 | 1...99 (printed unit "(1/rpm)" [as printed; probably %]) |
| 07 | P-GAIN 1 | P gain of speed controller | 3.7 | 0...20.0 |
| 08 | I-GAIN 1 | I gain 1 of speed controller | 3.7 | 0...20.0 |
| 09 | GAIN RPM | Changeover speed from P-I gain 1 to P-I gain 2 | 3.7 | 0...9999 (1/rpm) |
| 10 | P-GAIN 2 | P gain 2 of speed controller | 3.7 | 0...9.99 |
| 11 | I-GAIN 2 | I gain 2 of speed controller | 3.7 | 1...9999 |
| 12 | GEAR IN | Reduction ratio i = N1/N2, N1 = motor speed (GEAR IN = N1) | 3.6; 3.12 | 1...9999 |
| 13 | GEAR OUT | (GEAR OUT = N2) | | 1...9999 |
| 14 | G-CHANGE | Assignment of gear stage or motor | 3.15; 3.16 | 0, 1, 2 |
| 15 | P-MAX RPM | Motor speed for further evaluation of the analogue speed set-point voltage | 3.2 | 1...24000 (1/rpm) |
| 16 | PQ-FUNCT | See function table for PQRS 16 | | Σ<> |
| 17 | POS GAIN | Gain factor in position control loop | 3.6 | 0...99.9 (1/s) |
| 18 | POS RPM | Positioning speed (limited to 30 x POS GAIN) | 3.6 | 1...9999 (1/rpm) |
| 19 | LOAD LIM | Signal limit for drive load | 3.10 | 1...99 (% "bzw" (or) kW) |
| 20 | ALPHA | Phase angle correction factor | | 0...9.99 |
| 21 | P-WINDOW | Tolerance window inside which the message "IN POS" appears | 3.6 | 0.01...99.99 (∢ degr.) |

Footer: "Parameter list!" (ID "Para-umschaltbar" (switchable parameters)).
Note: enter the total of the summands as value of PQ FUNCT (PQRS 16). ALPHA (PQRS 20) corrects the phase angle between motor current and motor voltage in the constant torque range. In the RAC the value is permanently 1.0.

## PDF page 20 (p. 134): Fig. 101, function table PQ FUNCT (PQRS 16)

| Summand | Function if summand set | Function if <0> | See chap. |
|---|---|---|---|
| <1> | If EXT POS = 1: analogue N set-point via E3/E4 (X4) | If EXT POS = 1: analogue N set-point via E1/E2 (X4) | 3.2 |
| <2> | Direction of rotation of motor shaft and spindle or spindle feedback is opposing | Direction of motor shaft and spindle or spindle feedback is identical | 3.6 |
| <4> | Drive stopped without overshoot by disconnecting the I gain of the speed controller with Nset < MINRPM (A03) | I gain of speed controller always active | 3.7 |
| <8> | Slave mode: voltage at analogue input E1/E2 (X4) is torque set-point | Speed-controlled mode: E1/E2 (X4) used for speed set-point | 3.17 |
| <16> | Change analogue output M: 10 V ≙ value of LOAD LIM (% or kW) | Analogue output M not changed over | 3.10.1 |
| <32> | Torque limiting via voltage at analogue input E3/E4 (X4): 10 V ≙ 100 % Mset | No torque limiting via voltage value | 3.9 |
| <64> | Spindle positioning with reference switch on spindle | Spindle positioning without reference switch on spindle | 3.6.3 |
| <128> | Spindle positioning contrary to the convention in A06: if A06 = <16> -> effect of A06 = <0>; if A06 = <0> -> effect of A06 = <16> | Spindle positioning as specified in A06 (A06 = <16> or <0>) | 3.6.1 |
| <256> | For RAC 2.2 and RAC 3.1 only, with additional bleeder option: on mains failure braking must be done by the NC controller | For RAC 2.2 and RAC 3.1 only with additional bleeder option: braking during mains failure | 3.19 |

Footer: "Addition list!" (ID "Fkt-TabellePQ").

## PDF page 21 (p. 135): 5.3 Motor parameters (M parameters), Fig. 102

The motor parameters are set at the factory and guarantee the specified characteristic of the drive (motor/controller programming module). Only skilled personnel may change them, otherwise the drive can be damaged. Exception: the MOTFUNCT parameter in drives with RAC.

| Parameter | Designation | Function | See chap. | Value range |
|---|---|---|---|---|
| M01 | POLES | Number of motor poles | | 2, 4, 6, 8 |
| M02 | FEEDBACK | Type of feedback | | 1...4 |
| M03 | T-FILTER | Tacho filter | 3.7 | 0...3 |
| M04 | SLIP-LIM | Slip limiting as multiple of SIGN SLIP | | 1.0...7.9 |
| M05 | FLUX | Magnetisation current (peak value in A) | | 1...I_typ (A) |
| M06 | CURRENT | Maximum motor current (peak value in A) | | 1...I_typ (A) |
| M07 | SIGNSLIP | Core slip of drive | | 0...300 (1/rpm) |
| M08 | SIGN RPM | Speed for boosting slip | | 100...9999 (1/rpm) |
| M09 | I-U RPM | Speed for changing over to voltage control | | 100...9999 (1/rpm) |
| M10 | SIGN VOLT | No-load voltage of drive at SIGN RPM | | 0...999 (V) |
| M11 | BETA | Correction factor for matching the phase angle in the field-weakening range | | 0...9.99 |
| M12 | PHI | Correction angle for changing to field-weakening range | | 0...45 (∢ degr.) |
| M13 | VOLT FACT | Factor for load-dependent voltage increase | | 0...1.0 |
| M14 | SLIP FACT | Factor for boosting slip in the field-weakening range | | 0...1.0 |
| M15 | MOT FUNCT | See function table for M15 | | Σ<> |
| M16 | PHI FACT | Factor for changing from base speed range to field-weakening range | | 0...9.99 |

Footer: "Parameter list!" (ID "Para-Motor").

## PDF page 22 (p. 136): Fig. 103, function table MOTFUNCT (M15)

| Summand | Function if summand set | Function if <0> | See chap. |
|---|---|---|---|
| (<1>) | VOLT FACT parameter (M13) is operative | VOLT FACT (M13) inoperative (standard voltage increase operative) | |
| (<2>) | SLIP FACT parameter (M14) is operative | SLIP FACT (M14) inoperative (standard slip boost operative) | |
| (<4>) | Voltage amplitude at D/A converter limited to 7 V | Voltage amplitude at D/A converter not limited | |
| (<8>) | Voltage amplitude at D/A converter limited: no load 7 V; accelerating 7 V + Δ; braking 7 V + ΔU/2 | Not limited | |
| (<12>) | Like <8> but braking 7 V + ΔU | Not limited | |
| <16> | For RAC 2.2 and RAC 3.1 only: DC braking during mains failure | No DC braking during mains failure | 3.19 |
| <32> | For RAC 2.2-250: clock time 565 µs. For KDA/TDA: double voltage output | For RAC 2.2-250: clock time 600 µs | |
| (<64>) | For water-cooled motors: higher slip boost | Normal slip boost | |

Footer: "Addition list!" (ID "Fkt-TabelleM15").
Important: the user should only change the existing MOTFUNCT value by the summand <16>. Consult INDRAMAT before changing the sum (Σ<>).

## PDF page 23 (p. 137): FEEDBACK parameter, Fig. 104

Possible fault message FEEDBACK. Cause: wrong value in FEEDBACK parameter (M02); the motor feedback type requires a different value. Faulty or wrong feedback cable. Remedy: enter the M02 value matching the motor feedback type. Repair or replace the feedback cable.

Values for the FEEDBACK parameter (Fig. 104, correlation between feedback type and parameter value M02, ID "Para-M03" [as printed]):

| M02 value | Feedback type |
|---|---|
| 1 | Type 1: incremental encoder with 1024 lines |
| 3 | Type 3: INDRAMAT high-resolution encoder with 256 teeth, Ø 125 mm |
| 4 | Type 4: INDRAMAT high-resolution encoder with 512 teeth, Ø 256 mm |
| 2 | Type 2: incremental encoder with 4096 lines |

Note: for 2AD motors the feedback type can be read from the motor type designation (type code field "motor feedback"). For **1MB motors (spindle motors)** the feedback type cannot be read from the type designation because the motor feedback is mounted separately on the spindle. Only values 3 and 4 are possible.
Important: **interchanging feedback types 3 and 4 can damage the drive!**

## PDF page 24 (p. 138): PARAMETER LIST form

Header fields: Motor:, Controller:, Prog.-module:, Software:, Check sum:, Date:. **All empty.**
Blocks: A-Parameter (A01-A20 with columns Description, Parameter number, Current value, Value when delivered, Value range), P-, Q-, R-, S-Parameter (01-21 each, descriptions RAMP 1 ... P-WINDOW), M-Parameter (M01-M16). Value ranges as in Figs. 97, 100, 102. **No values are filled in.** (Page footer ID "Parameter" [uncertain].)

## PDF page 25 (p. 139): FUNCTION PARAMETER ADDITION LIST form

"FUNCTION PARAMETER ADDITION LIST - for the parameter list". Header fields Motor, Controller, Prog.-module, Software, Checksum, Date: empty.
Boxes with summand lines `< >`, remarks and `Σ< > =`: A05 FUNCT 1, A06 FUNCT 2, P16 / Q16 / R16 / S16 PQ-FUNCT, M15 MOT FUNCT ("(Enter value <16> only!)", "(Enter value <32> only!)", Note: "Value specified by manufacturer"). **All empty.** Footer ID "Parameter-Bl..." [unreadable].

## PDF page 26 (p. 140)

Empty page, header "Overview of parameters" only.

---

## For the retrofit

These pages are the generic Indramat KDA/TDA/RAC main spindle drive manual (back part). They contain no machine-specific wiring and no parameter values. Important points:

**Drive type.** The manual covers the INDRAMAT KDA (modular, on a common DC-link supply module, e.g. TVM/KDV) and TDA, plus the stand-alone RAC 2/RAC 3. On the CTX twin, with spindles on the modular servo package, KDA/TDA on the shared supply is most likely. **Confirm from the drive nameplates and the other parts of chapter 5.** Motors are probably 1MB (integrated spindle motors), which the manual names as the spindle motor type. The drive is an asynchronous-motor vector drive with a programming module (EEPROM, `AS..`) that holds all parameters. Before any work, **read out and photograph the parameter set from each drive's display** (mode switch left = parameter mode). The lists in this PDF are blank, and a lost module gives only "BASISPAR" defaults.

**Keeping the drives under LinuxCNC.**
- Speed command: analogue ±10 V on X4 E1/E2 (differential). Scaling: CMD VOLT (A02, 6-10 V) = MAX RPM (A01), or P-MAX RPM (PQRS 15) with FUNCT2 <128>. A binary/digital set-point option (type code "D") also exists. Check the type code; the ADW3 card (X7/X12 on the CDR board) indicates analogue. LinuxCNC: a ±10 V DAC per spindle (e.g. Mesa 7i77 analogue out). Use RUN (X2/25) as the "spindle on" gate.
- Enable and status, all 24 V digital on X2 (external +24 V on X2/1 and X2/19, 18-36 V): inputs RF X2/24 (enable; after AS or some faults RF must go 0 -> 1 again, "RF AGAIN"), RUN X2/25, AS X2/3 (starting lockout, **not** a personnel safety function), E-STOP X2/23 (RAC only), MD-RED X2/28 (torque reduction), PAR1/PAR2 X2/30/31 (select parameter record P/Q/R/S, e.g. per gear or per mode), LIMIT 1/2/4 X2/32-34 (select max speed A12-A18), SPEED 1/2 X2/35/36, OSCILLATE X2/29 (gear change), POS1/POS2 X2/26/27 (select orient position A08-A10), EXT POS X2/37 (spindle reference switch). Outputs: READY X2/4, TEMPWARN X2/9 (1 = OK), plus N = NCMD, N < NMIN, IN POS (terminals are not on these pages), and the Bb ready contact of the package. HAL: RF/RUN from `spindle.N.on` and machine-on, READY/Bb into the e-stop chain, N = NCMD to `spindle.N.at-speed`.
- Orientation: the drive can orient on its own (POS1/POS2 select A08-A10 positions, OFFSET A07, gains POS GAIN/POS RPM/P-WINDOW, feedback "IN POS"). This is the simplest way to keep tool-change or part-transfer orientation without LinuxCNC closing the loop.
- C axis, threading, twin spindle sync: the drive's own encoder input is the motor/spindle feedback (Indramat high-resolution type 3 = 256 teeth Ø125 or type 4 = 512 teeth Ø256, or 1024/4096-line incremental; ENCODER A11). These pages do not show whether the drive outputs encoder signals for the CNC. LinuxCNC needs its own spindle encoder signal (quadrature into a Mesa encoder input) for threading (`spindle.N.revs`) and for C-axis position control. In C-axis mode, run the drive in speed mode with LinuxCNC closing the position loop through ±10 V (high P-GAIN, I-GAIN, records selected via PAR1/PAR2), or check part c/e of the chapter for an Indramat C-axis option. Synchronous part transfer between the two spindles is then done in LinuxCNC (both spindles position-controlled, or one as master and one following the encoder). The SLAVE/torque-follower mode (FUNCT2 <8>, PQ-FUNCT <8>, master outputs torque on analogue output N with FUNCT1 <64>) is meant for mechanically coupled motors, not for twin-spindle sync.
- Temperature: the drive monitors motor winding temperature itself (warning at 145 °C, MOT WARN; trip at 155 °C, MOT TEMP; NO TEMP for a broken sensor) and the heatsink (KK WARN, trip 30 s later, KK TEMP). The CNC must act on TEMPWARN X2/9, for example by reducing load or stopping, otherwise the drive trips.
- Analogue outputs M and N (load/torque, speed/power/temperature selectable via FUNCT1/2) can be read on Mesa analogue inputs for a load meter.
- DC link and braking: on mains failure or E-stop the servo axes stop first. The spindles brake only within the regeneration limit of the supply. Expect long spindle stop times and keep the Indramat supply module's sequence (Bb, power contactor) unchanged.

**Replacing the drives.** New VFD/servo spindle drives need: motor data from the M parameters (poles M01, magnetisation current M05, max. current M06, slip M07, field weakening M08-M12; read them out first). A spindle encoder interface compatible with the Indramat high-resolution gear-tooth sensor (types 3/4 are sine-type sensors on a toothed wheel, not standard TTL; check the signal before reusing it) or a new encoder. Motor PTC/KTY temperature sensor handling. A braking resistor or regenerative supply, since the shared Indramat DC-link supply would be lost. With C-axis and spindle synchronisation, a servo-capable spindle drive with position feedback (or EtherCAT/Mesa encoder feedback to LinuxCNC) is required, not a plain V/f inverter.

**Inconsistencies in the original document** (marked [as printed] above): DISABLED page says AS = 1, but Fig. 96 says AS = 0 means lockout. FEEDBACK is called M03 on p. 122 and M02 on p. 137 (the M table gives M02 = FEEDBACK, M03 = T-FILTER). FUNCT2 <16> cites POS RPM as PQRS 19, while Fig. 100 gives POS RPM = PQRS 18 and LOAD LIM = PQRS 19.
