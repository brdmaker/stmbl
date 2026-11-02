# Gildemeister CTX twin, electrical documentation: 5 Main spindle drive (part b), Indramat main spindle drive manual, chapters 2-3, PDF pages 1-44

**Source:** `4223b561-5_Main_spindel_drive_b_20201028_0001.pdf` (scan dated 2020-10-28 in the file name). No Gildemeister title block, drawing number or date is printed on any page. The content is an excerpt from the **INDRAMAT main spindle drive manual (Rexroth Indramat, for RAC / KDA / TDA controllers with 2AD main spindle motors)**: manual pages 27-70, chapter 2 "Operating the controllers" and chapter 3 "Controller functions" up to the start of section 3.6.3. The manual's own document number and edition are not on these pages [not in this part].

**Original language:** English (Indramat's English edition). The only German text is the small figure file tags in the bottom-right corner of each frame (e.g. "Bedienfeld1"); they are given with a translation.

**Not a wiring diagram:** these pages are a generic manufacturer's manual. They contain no Gildemeister device tags (-K.., -A..), wire numbers or machine-specific parameter values. The connector pinouts (X2, X4, X6/X13, X15/X11) are the drive's own terminals. The machine-specific wiring is in the other parts of chapter 5 and in the circuit diagrams.

Page relation: **PDF page n = manual page n + 26** (PDF 1 = manual p. 27, PDF 44 = manual p. 70). The scan has no gaps.

## Page table

| PDF page | Manual page / figure (file tag) | Content |
|---|---|---|
| 1 | p. 27 | Chapter 2 "Operating the controllers": table of contents 2.1-2.3.6 |
| 2 | p. 28 | 2. Operating the controllers: introduction |
| 3 | p. 29, Fig. 5 ("Bedienfeld1") | 2.1 The control panel: LEDs, mode switch, display, keys; operating mode vs. parameter mode |
| 4 | p. 30 | 2.2 Operating mode: power-up messages NO POWER / NO RF |
| 5 | p. 31, Fig. 6 ("Zustandsgr.") | 2.2.1 Displaying the internal state variables (N, C, S, M, POS, UD, ID, PD, T, P) |
| 6 | p. 32, Fig. 7 ("Signalzustand") | Display of control input states (digit assignment, X2 terminals); 2.2.2 Analogue output N (X2/21) |
| 7 | p. 33, Fig. 8 ("Analog N") | Switching analogue output N to the state variables, with scaling |
| 8 | p. 34 | 2.3 Parameter mode |
| 9 | p. 35, Fig. 9 ("Param.-Prüfsumme") | 2.3.1 Quick test of parameter values (checksum SUM) |
| 10 | p. 36, Fig. 10 ("6 Parametersätze"), Fig. 11 ("Parametersatz") | 2.3.2 Reading the parameters; the six parameter records A, P, Q, R, S, M |
| 11 | p. 37, Fig. 12 ("Wechsel-Parameter") | Changing to another parameter record; recording values; 2.3.3 Changing the parameters |
| 12 | p. 38, Fig. 13 ("Ändern Parameter"), Fig. 14 ("Ändern-Parameter M") | Changing parameter values (code entry) in A, P, Q, R, S and in M |
| 13 | p. 39, Fig. 15 ("Softwareversion") | RF-AGAIN, faults UNACCEPT and FEEDBACK; 2.3.4 Software version (e.g. "RAC 4 V 1.3") |
| 14 | p. 40 | 2.3.5 Duplicating parameters with the PDA 1 adapter; fault EEPROM |
| 15 | p. 41 | Fault PARALOST / BASISPAR; 2.3.6 Loading parameters via the serial interface |
| 16 | p. 42 | Blank (only the header) |
| 17 | p. 43 | Chapter 3 "Controller functions": table of contents 3.1-3.18 |
| 18 | p. 44 | Table of contents, continued: 3.19-3.24 |
| 19 | p. 45, Fig. 16 ("Steuereingänge") | 3.1 Drive "ready" state: conditions; X2 control inputs, READY output, "Bb" contact |
| 20 | p. 46 | 3.2 Speed set-point; 3.2.1 Analogue speed set-point (standard) |
| 21 | p. 47, Fig. 17 ("Drehzahlsollwert") | X4 analogue inputs (2 differential inputs), X2 inputs, parameter-record table PAR1/PAR2 |
| 22 | p. 48, Fig. 18 ("Para-Sollwert-Analog") | Parameters for the analogue speed set-point (A01, A02, PQRS15, PQRS16, A05, A03, A06) |
| 23 | p. 49, Fig. 19 ("Para-Sollwert-Digital") | 3.2.2 Digital speed set-point (optional); parameters |
| 24 | p. 50, Fig. 20 ("Bewertung-binär") | X4 digital set-point pinout, binary coding |
| 25 | p. 51, Fig. 21 ("Bewertung-Fließkomma") | X4 digital set-point pinout, floating-point coding |
| 26 | p. 52 | "For technical reasons this page remains blank" |
| 27 | p. 53, Fig. 23 ("Hauptspindeldrehzahl") | 3.3 Main spindle speed signals: X2/5 N = CMD, X2/6 N < MIN |
| 28 | p. 54, Fig. 24 ("Parameter Hauptspindel") | Parameters A03, A04, A05 for the speed signals; safety note |
| 29 | p. 55, Fig. 25 ("Drehzahlsollwertrampen") | 3.4 Speed set-point ramps: control inputs |
| 30 | p. 56, Fig. 26 ("Parameter Drehzahlsollwertrampe") | Ramp parameters P01-P05 and A06 |
| 31 | p. 57, Fig. 27 ("Umrechnung von Drehzahl") | Conversion graph ΔN, Δt to RAMP in rad/s²; formula |
| 32 | p. 58, Fig. 28 ("Drehzahlrampen Sprung") | Speed ramps for a step change of the set-point |
| 33 | p. 59, Fig. 29 ("Drehzahlsollwertglättung") | 3.5 Speed set-point smoothing: control inputs |
| 34 | p. 60, Fig. 30 ("Para-Drehz-Glätt") | Smoothing parameters; Tg = 0.5 × 2^RAMP1 ms |
| 35 | p. 61, Fig. 31 ("Mechanische Kopplung") | 3.6 Spindle positioning: accuracy; coupling types vs. positioning method |
| 36 | p. 62, Figs. 32-34 ("Starres-Getriebe", "Schalt-Getriebe", "Schlupf-Getriebe") | The three positioning methods (block diagrams) |
| 37 | p. 63, Fig. 35 ("Spindelpos Standard") | 3.6.1 Basic data: 3 positions via POS1/POS2, INPOS output X2/7 |
| 38 | p. 64, Fig. 36 ("Para-Spindel") | Parameters for spindle positioning (A06-A10, P12, P13, P16-P18, P21) |
| 39 | p. 65, Fig. 37 ("Eingangsbelegung") | Option: 3600 spindle positions, BCD position input X6 (KDA 3) / X13 (RAC 2, RAC 3) |
| 40 | p. 66, Figs. 38, 39 ("Para-Spindel-3600", "Para-Spindel-zusatz") | Parameters for 3600 positions; 3.6.2 Positioning via motor feedback (P16) |
| 41 | p. 67, Fig. 40 ("Fkt-Motorfeedback") | Timing diagram, positioning via motor feedback |
| 42 | p. 68, Fig. 41 ("Para-Spindel-refer") | 3.6.3 Positioning via motor feedback with spindle reference switch: parameters |
| 43 | p. 69, Fig. 42 ("Referenzschalter") | Reference switch wiring to X2/37 and the dwell angle α_min |
| 44 | p. 70, Fig. 43 ("Fkt-Referenzschalter") | Timing diagram with reference switch (HOMING); text breaks off at "Possible fault message: HOMING remains in the display" |

Translations of the German figure tags: Bedienfeld = control panel; Zustandsgr(ößen) = state variables; Signalzustand = signal state; Param.-Prüfsumme = parameter checksum; 6 Parametersätze = 6 parameter records; Parametersatz = parameter record; Wechsel-Parameter = change of parameter (record); Ändern Parameter = change parameters; Softwareversion = software version; Steuereingänge = control inputs; Drehzahlsollwert = speed set-point; Para-Sollwert-Analog/-Digital = parameters, analogue/digital set-point; Bewertung-binär / -Fließkomma = weighting, binary / floating-point; Hauptspindeldrehzahl = main spindle speed; Parameter Hauptspindel = parameters, main spindle; Drehzahlsollwertrampen = speed set-point ramps; Umrechnung von Drehzahl = conversion of speed; Drehzahlrampen Sprung = speed ramps, step; Drehzahlsollwertglättung / Para-Drehz-Glätt = speed set-point smoothing; Mechanische Kopplung = mechanical coupling; Starres-Getriebe = rigid gearing; Schalt-Getriebe = change-speed gearbox; Schlupf-Getriebe = gearing with slip; Spindelpos(itionierung) Standard = spindle positioning, standard; Eingangsbelegung = input assignment; Para-Spindel-zusatz = additional spindle parameters; Fkt-Motorfeedback = function, motor feedback; Referenzschalter = reference switch.

---

## PDF page 1 (manual p. 27): Chapter 2 contents

Header: "Operating the controllers".

- 2. Operating the controllers
- 2.1 The control panel
- 2.2 Operating mode
- 2.2.1 Displaying the drive's internal state variables
- 2.2.2 Outputting the drive's internal state variables via the analogue output N
- 2.3 Parameter mode
- 2.3.1 Quick test of parameter values
- 2.3.2 Reading the parameters
- 2.3.3 Changing the parameters
- 2.3.4 Displaying the software version
- 2.3.5 Duplicating the parameters
- 2.3.6 Loading the operating parameters via the serial interface

## PDF page 2 (p. 28): 2. Operating the controllers

"INDRAMAT main spindle drives make a wide range of functions available for numerous applications. The drives can be easily matched to specific applications.
The controllers are operated via the control panel, provided that the controllers are supplied with control voltage, i.e. commissioning must have already been carried out to a large extent."

## PDF page 3 (p. 29): 2.1 The control panel, Fig. 5

The control panel is on the front of the controller. It is used for drive and fault diagnosis and for checking and setting parameters. Green LED = drive OK; red LED = fault.

Fig. 5 "Control panel with legends":
- Top row: green LED "Ready", operating mode switch labelled "Parameter ←", red LED "Fault".
- Display: one line of text.
- Key panel with arrow keys up, right, left, down, and an "accept" key in the centre (red).

Mode switch:
- **Operating mode:** the drive is ready for power input or power output. Switch to the **right**.
- **Parameter mode:** power output is inhibited; parameters can be checked and changed. Switch to the **left**.

## PDF page 4 (p. 30): 2.2 The main spindle drive in the operating mode

- Operating mode: ready for power input/output.
- Choice: the mode switch must be set to the right. Control voltage must be present.
- Messages at power-up:
  - control voltage only: **NO POWER**, both LEDs off;
  - ready for power output: **NO RF** (no controller enabling signal), green LED on.

## PDF page 5 (p. 31): 2.2.1 Displaying the drive's internal state variables, Fig. 6

In operating mode the internal state variables for diagnosis are called up with the "up" and "down" keys ("down" runs the sequence in reverse; both keys repeat when held). Note: pressing the red centre key (accept) returns the display to the initial state.

Fig. 6 "Drive's internal state variables" (operating mode switch to the right):

| Display | Internal state variable |
|---|---|
| NO POWER or NO RF | Initial state |
| N= | Motor speed |
| C= | Speed set-point |
| S= | Spindle feedback speed |
| M= | Torque utilization (torque set-point) |
| POS= | Spindle or motor position |
| UD= | Link circuit voltage |
| ID= | Link circuit current |
| PD= | Link circuit power |
| T= | Temperature of motor windings |
| P....... | Signal states of control inputs |

## PDF page 6 (p. 32): signal states of the control inputs, Fig. 7; 2.2.2 analogue output N

"The signal states of the control inputs are also displayed."

Fig. 7 "Display of signal states of control inputs, and assignment of digits", example readout **`R 5 3 1 2 1`**. The letter and digits mean, from left to right:

1. **Letter: PAR1 (X2/30), PAR2 (X2/31)** (active parameter record)

   | Letter | PAR1 | PAR2 |
   |---|---|---|
   | P | 0 V | 0 V |
   | Q | +24 V | 0 V |
   | R (example) | 0 V | +24 V |
   | S | +24 V | +24 V |

2. **Digit 2: LIMIT 1 (X2/32), LIMIT 2 (X2/33), LIMIT 4 (X2/34)** (binary 0-7)

   | Digit | LIMIT 1 | LIMIT 2 | LIMIT 4 |
   |---|---|---|---|
   | 0 | 0 V | 0 V | 0 V |
   | 1 | +24 V | 0 V | 0 V |
   | 2 | 0 V | +24 V | 0 V |
   | 3 | +24 V | +24 V | 0 V |
   | 4 | 0 V | 0 V | +24 V |
   | 5 (example) | +24 V | 0 V | +24 V |
   | 6 | 0 V | +24 V | +24 V |
   | 7 | +24 V | +24 V | +24 V |

3. **Digit 3: POS 1, POS 2.** The figure prints the terminals as "(X2/35) (X2/36)". Every other figure puts POS 1 on X2/26 and POS 2 on X2/27, so "X2/35, X2/36" here is a misprint in the original [uncertain: the terminal numbers printed in this figure].

   | Digit | POS 1 | POS 2 |
   |---|---|---|
   | 0 | 0 V | 0 V |
   | 1 | +24 V | 0 V |
   | 2 | 0 V | +24 V |
   | 3 (example) | +24 V | +24 V |

4. **Digit 4: MD-RED (X2/28)**: 0 = 0 V, 1 = +24 V (example 1).
5. **Digit 5: SPEED 1 (X2/35), SPEED 2 (X2/36)**: 1 = +24 V / 0 V; 2 = 0 V / +24 V (example); 0 = 0 V / 0 V.
6. **Digit 6: EXT POS (X2/37)**: 0 = 0 V, 1 = +24 V (example 1).

**2.2.2 Outputting the drive's internal state variables via the analogue output N:** "The analogue output N (X2/21) is designed as a universal output. It outputs voltage between -10 and +10 volts." Any displayed state variable can be routed to output N with the LEFT arrow key while control voltage is present, for example to record it on an oscilloscope while the drive runs.

Note to Fig. 8: all state variables can still be called up into the display while the output N assignment stays unchanged. DOWN scrolls back. ACCEPT resets the display to the initial state, and the N output assignment remains unchanged.

## PDF page 7 (p. 33): Fig. 8 "Switching the analogue output N to the drive's internal state variables"

Analogue output **N (X2/21)**. LEFT arrow selects the signal; at some entries a RIGHT arrow selects a high-resolution variant. On entering each new display line, "The last selected N output weighting is retained" until LEFT/RIGHT is pressed.

| Display | Key | Signal on N (X2/21) | Weighting |
|---|---|---|---|
| NO POWER / NO RF (initial state), shows "N=NCMD" | none | Motor speed | ±10 V ≙ ±MAX RPM |
| N | → (right) | High-resolution motor speed | ±10 V ≙ ±50 1/rpm (sic, rpm) over the entire speed range |
| N | ← (left) | Motor speed | ±10 V ≙ MAX RPM (value in parameter A01) |
| C | ← | Speed set-point | ±10 V ≙ MAX RPM (A01) |
| C | → | Internal speed set-point (ramp) | ±10 V ≙ MAX RPM (A01) |
| S | ← | Spindle encoder speed | ±10 V ≙ ±10 000 1/rpm (rpm) |
| M | ← | Torque set-point, not delayed | ±10 V ≙ drive-dependent torque set-point |
| POS | ← | Spindle (motor) position ∠° | ±10 V ≙ ±180° |
| POS | → | Spindle (motor) position in ∠° (high resolution) | -10 V ... +10 V ≙ 0.044° |
| UD | ← | Link circuit voltage | +10 V ≙ +1000 V in link circuit |
| ID | ← | Link circuit current | ±10 V ≙ rated current of controller |
| PD | ← | Link circuit power | ±10 V ≙ ±100 kW in RAC 2.2; ±50 kW in KDA 3 and RAC 3 |
| T | ← | Temperature of motor windings | 0 ... +10 V ≙ 0 ... +150 °C |
| P....... | none | The last selected N output weighting is retained | |

## PDF page 8 (p. 34): 2.3 The main spindle drive in the parameter mode

- The drive is not ready for power input or output.
- Parameters can be tested, read and changed. They are stored in an **EEPROM on the "AS" programming module** (plug-in module).
- Choice: the printed text says "The mode selection switch must be set to the right. Control voltage must be present." Page 29 says parameter mode = switch to the **left**, so "right" here is a misprint [uncertain].
- Note: if parameter mode is selected while the motor rotates, the drive brakes to a standstill and stays without torque.
- Display: "(P01...)" appears when parameter mode is first selected after control voltage is applied. Otherwise the display shows the parameter that was current when operating mode was reselected.

## PDF page 9 (p. 35): 2.3.1 Quick test of parameter values, Fig. 9

- The parameter values of a programming module are stored with a **parameter checksum**. Comparing it with the checksum in the machine file shows whether the module holds the correct values.
- The checksum does **not** include **OFFSET (A07)**, because A07 can differ even between identical machines.
- Fig. 9 "Display of parameter checksum" (switch left): from e.g. "P 01 ....." (parameter displayed), first press and hold "accept", then press the right arrow. The display shows "SUM ....." (checksum).

## PDF page 10 (p. 36): 2.3.2 Reading the parameters, Figs. 10, 11

Fig. 10 "The six parameter records of INDRAMAT main spindle drives":
- **Parameter record A** (general parameters)
- **Parameter records P, Q, R, S** (selectable, chosen with PAR1/PAR2)
- **Parameter record M** (motor parameters)
- "☞ Parameter list!"

Records A and M are always active, plus one of P, Q, R, S.

Fig. 11 "Reading a parameter record, e.g. parameter record P" (switch left):

| Display | Key | Meaning |
|---|---|---|
| P01 .... (number flashing) | initial state | Parameter No. P01 with value |
| RAMP 1 | ← | Parameter name is displayed |
| P01 .... | (automatic) | Automatic return to parameter number |
| P02 .... | ↑ | Parameter No. P02 with value |
| RPM 1 | ← | Parameter name is displayed |
| P02 .... | (automatic) | Automatic return to parameter number |
| P03 .... | ↑ | Parameter No. P03 with value |
| : | | etc., etc. |

## PDF page 11 (p. 37): changing record, Fig. 12; 2.3.3 Changing the parameters

Fig. 12 "Changing the parameter records in the display":

| Display | Key | Meaning |
|---|---|---|
| P06 .... | initial state (any) | A P parameter is displayed |
| (P)06 .... (letter flashing) | 2× ← | Ready to quit P parameter |
| (Q)06 .... | ↑ | Change to Q parameter |
| Q(06) .... | → | Activate numbers for scrolling |
| (R)06 .... | ↑ | Change to R parameter |
| : | | etc., etc. |

Recording: a parameter form for listing the parameters, with the permissible ranges, is in the annex [the annex is not in this part].

2.3.3: the user parameters (A, P, Q, R, S) have factory default values and may need to be changed for an application. The motor parameters (M) are optimized in the factory and may only be changed by skilled personnel, otherwise the drive can be damaged. Exceptions: **M03 T-FILTER** and **M15 MOTFUNCT** (see section 5, parameter overview or parameter list). Parameter values are protected against unintended change by a code.

## PDF page 12 (p. 38): Figs. 13, 14, changing values

A new value must be transferred to the programming module's memory with the red "accept" key.

Fig. 13 "Changing parameter values in parameter records A, P, Q, R, S":
1. e.g. "A07 ...." (initial state);
2. → shows **LOCKED** (access to the value locked);
3. code for changing the value: accept, 2× ←, 2× accept;
4. "A07 ....": ↑/↓ change the numbers (key repeats), ←/→ change the decimal place;
5. accept: "A07 ...." (number flashing), new value accepted into the programming module.

Note: the controller accepts the new value into memory if the LEFT key is pressed after the value has been changed. The parameter number flashes and the old value is displayed. [As printed; this appears to describe discarding a change, and the text is ambiguous: uncertain.]

Fig. 14 "Changing parameters in the M parameter record": from "M07 ....", → shows LOCKED. The code is a sequence of ↑, ←, accept, →, ↓ keys drawn as a circle [exact order uncertain]. Then ↑/↓ change the numbers, ←/→ the decimal place, and accept stores the new value.

## PDF page 13 (p. 39): RF-AGAIN, fault messages, 2.3.4 software version

**IMPORTANT: RISK OF DAMAGE!** To start the drive again with modified parameters, move the mode switch to the right. The display shows "**RF-AGAIN**", and the RF input (controller enabling signal) must be switched from 0 to +24 V.

- Fault **UNACCEPT**: cause, invalid parameter combination. Remedy: switch left, press red key, the incorrect parameter is displayed; enter a valid value.
- Fault **FEEDBACK**: cause, the type of motor feedback and the value in **M02** do not match. Remedy: switch left, enter the correct value (see section 5.3).

2.3.4: the AS programming module also holds the operating software. Note its version on the parameter form. Fig. 15 "Displaying the software version": from e.g. "Q 11 ....", accept goes back to "Q 01 ...." (first parameter); accept again shows e.g. **"RAC 4 V 1.3"** briefly, then returns automatically.

## PDF page 14 (p. 40): 2.3.5 Duplicating the parameters

- Parameters of a master programming module can be copied to a slave module for the same controller (security copy, series machines).
- Prerequisite: **parameter duplicating adapter PDA 1**.
- Procedure: switch off the main drive (no control voltage). Unplug AS and plug in the PDA 1. Master module goes in "MASTER", slave in "SLAVE". Mode switch left. Switch on the control voltage: the display shows the master checksum and a rotating pointer; the transfer takes up to 30 s. If the switch is not left, "SWITCH" appears and the transfer starts when it is moved left. At the end the display shows "SWITCH ->". Set the switch right so that the slave EEPROM cannot be corrupted at power-down; the slave checksum appears with a request to switch off, e.g. "**4B13 OFF**". Switch off and remove the slave. When done, refit the master module in place of the PDA 1.
- Important: do not remove programming modules with power on. The slave is erased if no master is plugged in. Only the EEPROM contents are copied; the system software is ignored; checksums are not compared.
- Fault **EEPROM**: the slave EEPROM cannot be programmed (defective or missing). Remedy: switch off and use a new slave module.

## PDF page 15 (p. 41): PARALOST, 2.3.6 serial interface

- Fault **PARALOST**: the programming module has no parameter values (not loaded), or the master and slave software versions have different numbers of parameters. Remedy: switch left and press red key. This loads general (basic) parameters; "**BASISPAR**" shows when loading is complete; red key again gives parameter mode. The basic parameters are not optimized and must be replaced by the optimized values (AS/...).
- 2.3.6 Loading parameters via the serial interface. Prerequisite: serial interface fitted (type code field "additional interfaces": **S**), and a data input device (PC, magnetic tape, punched tape or other control). Purpose: fast input for series machines; parameters can be printed. Process: see section 3.22 "Serial interface" [not in this part].

## PDF page 16 (p. 42)

Blank except the header "Operating the controllers".

## PDF pages 17-18 (pp. 43-44): Chapter 3 "Controller functions", contents

3.1 Drive "ready" state · 3.2 Speed set-point · 3.2.1 Analogue speed set-point (standard) · 3.2.2 Digital speed set-point (optional) · 3.3 Main spindle speed output signals · 3.4 Speed set-point ramps · 3.5 Speed set-point smoothing · 3.6 Spindle positioning · 3.6.1 Basic data for spindle positioning · 3.6.2 Spindle positioning via motor feedback · 3.6.3 … via motor feedback with spindle reference switch · 3.6.4 … via spindle feedback · 3.7 Speed controller functions · **3.8 C-axis function** · 3.9 Power and torque limiting · 3.10 Drive utilization output · 3.10.1 Analogue output for drive utilization (analogue output M) · 3.10.2 Signal output with adjustable threshold for drive utilization (LOAD LIMIT) · 3.11 Temperature pre-warning · 3.12 Maximum spindle speeds · 3.13 Spindle monitoring · 3.14 Switchable parameter records · 3.15 Gear change · 3.15.1 Automatic gear change via the controller · 3.15.2 Gear change via the external controller · 3.16 Two-motor changeover · 3.17 Master-slave operation · 3.18 EMERGENCY-STOP circuit (RAC) · 3.19 Performance during mains failure (RAC) · 3.20 Performance during mains failure and EMERGENCY-STOP (KDA, TDA) · 3.21 Starting lockout in KDA/TDA · 3.22 Serial interface (optional) · 3.23 SERCOS interface (optional) · 3.24 Incremental encoder output (optional).

This part covers only 3.1 to the start of 3.6.3. Sections 3.6.4 and later (including C-axis, LOAD LIMIT, temperature pre-warning, incremental encoder output) are not in this PDF.

## PDF page 19 (p. 45): 3.1 Drive "ready" state, Fig. 16

The drive is "ready" when:
- all internal controller signal voltages are present;
- the control inputs and signal outputs (**terminal strip X2**) are supplied with +24 V and 0 V. The supply should come from the external controller, but the internal +24 V can also be used;
- the motor and controller temperatures are within range: **motor winding 0 < t < 155 °C**, controller heatsink 0 < t < t_max permissible;
- the mode switch is to the right;
- there is no fault message in the display;
- link circuit voltage is present;
- for **RAC 2 and RAC 3** the "E-stop" control input is at 1;
- for **KDA 3** the "AS" (starting lockout) control input is at 1.

The ready state can be evaluated via the **READY** signal output and the potential-free **"Bb"** contact.

Fig. 16 "Control inputs and signal output, 'Bb' potential-free contact". Terminal strip **X2** of the control unit:

| X2 terminal | Signal | Direction / note |
|---|---|---|
| X2/23 | E-STOP (RAC only) | input, must be 1 for ready |
| X2/24 | RF (controller enabling signal) | input |
| X2/25 | RUN | input |
| X2/26 | POS 1 | input |
| X2/27 | POS 2 | input |
| X2/28 | MD-RED | input (torque reduction) |
| X2/29 | OSCILLATE | input |
| X2/30 | PAR 1 | input |
| X2/31 | PAR 2 | input |
| X2/32 | LIMIT 1 | input |
| X2/33 | LIMIT 2 | input |
| X2/34 | LIMIT 4 | input |
| X2/35 | SPEED 1 | input |
| X2/36 | SPEED 2 | input |
| X2/37 | EXT POS | input |
| X2/3 | AS (starting lockout, KDA only) | input, must be 1 for ready |
| X2/2 | 0 V int | internal supply |
| X2/18 | +24 V int | internal supply |
| X2/1 | Input 0 V ext | reference for inputs, fed from the controller (0 V) |
| X2/19 | Input +24 V ext | fed from the controller (+24 V) |
| X2/4 | READY | signal output, 1 ≙ "Ready" message to the controller |
| X15 (RAC) / X11 (KDA) | "Bb" relay contact | potential-free, closed = ready |

Connections drawn: "From the controller 0 V → X2/1, +24 V → X2/19". "Link if 0 V / +24 V not available from the controller": jumper **X2/2 to X2/1** and **X2/18 to X2/19**. For ready, X2/23 and X2/3 are drawn at 1. Legend: 1 ≙ +24 V, 0 ≙ 0 V; signal step; any output state.

When ready, the green "READY" LED is on. On a fault, the green LED goes off, the red "FAULT" LED comes on and the fault message flashes in the display.

## PDF page 20 (p. 46): 3.2 Speed set-point, 3.2.1 Analogue speed set-point (standard)

- The set-point comes from the controller as an analogue voltage or digitally as a bit pattern. Speed range from minimum **0.0005 rpm** up to maximum speed. Optional: SERCOS interface (see 3.2.1 [sic, 3.23]).
- **The speed set-point is read every 1.2 ms.**
- 3.2.1: the analogue interface is for use as a main spindle drive, a positioning drive with various gear ratios, or a **C-axis drive for lathes**.
- "The difference input provides a large degree of decoupling between NC controller and control unit." Set-point voltage matching is easy. An excessive N set-point gives only the specified maximum speed.
- One of the two set-point inputs is always active.
- Positioning needs suitable set-point weighting, depending on the gear ratio. **C-axis operation requires high resolution of the speed set-point.**
- Weighting for different speeds and for C-axis operation is selected via parameter records (PQRS).

## PDF page 21 (p. 47): Fig. 17 "Inputs for analogue speed set-point, control inputs"

Function conversion: the control unit must have the analogue set-point interface (type code field "Speed set-point = **A**").

**X4 Analogue inputs** (control unit):

| X4 pin | Signal |
|---|---|
| X4/1 (E1), X4/2 (E2) | Differential input 1: analogue voltage ±10 V (screened pair from the NC) |
| X4/5 (E3), X4/6 (E4) | Differential input 2: analogue voltage ±10 V |
| X4/4 | Screen / ground connection (cable screens of both pairs drawn to pin 4) |

Changeover between input 1 and input 2 is drawn as a switch controlled by parameter **P/Q/R/S16** (PQ-FUNCT) and by input **EXT POS (X2/37)**.

X2 inputs in this figure: X2/23 E-STOP (RAC only) = 1; X2/24 RF = 1; X2/25 RUN: 1 ≙ enable speed set-point; X2/30 PAR 1 and X2/31 PAR 2: change parameter records PQRS (table); X2/29 OSCILLATE drawn at 0; X2/37 EXT POS: 1 ≙ change input weighting to P-MAXRPM of the active parameter record (PQR or S); X2/3 AS (starting lockout, KDA only) = 1.

Table, parameter record selection:

| PAR 1 | PAR 2 | Active parameter record |
|---|---|---|
| 0 | 0 | P |
| 1 | 0 | Q |
| 0 | 1 | R |
| 1 | 1 | S |

Note: for noise immunity (earth loops), connect the screen of the set-point cable **at the controller (drive) end only**. Do not run the set-point line parallel to power cables.

## PDF page 22 (p. 48): Fig. 18 "Parameters for analogue speed set-point"

| Parameter | Description | Function | Value range or value |
|---|---|---|---|
| A01 | MAX RPM | Maximum speed of motor | 1 ... 24000 (1/rpm) |
| A02 | CMD VOLT | Analogue voltage value | 6.0 ... 10.0 (V) |
| P/Q/R/S15 | P-MAXRPM | Motor speed for further input weighting | 1 ... 24000 (1/rpm) |
| P/Q/R/S16 | PQ-FUNCT | Change analogue inputs when changing input weighting | <1> |
| A05 | FUNCT 1 | Analogue speed set-point below MIN RPM is invalid | <2> |
| A03 | MIN RPM | Speed limit | 1 ... 999 (1/rpm) |
| A06 | FUNCT 2 | With EXTPOS = 1, at analogue output N: 10 V = P-MAXRPM | <128> |

("<n>" are bit values added into the function word.)

Notes:
- Set-point weighting = **CMDVOLT / MAXRPM** with EXTPOS = 0.
- With EXTPOS = 1 the weighting = **CMDVOLT / P-MAXRPM**. P-MAXRPM belongs to the selected record (P, Q, R or S). With FUNCT2 the analogue output N can be referred to P-MAXRPM.
- Changing to the 2nd differential input is done with EXT POS. Then PQ-FUNCT must be increased by <1> and EXT POS must be 1.
- If CMDVOLT < 10 V, up to 105 % of MAXRPM can be reached by a suitably higher voltage.
- To prevent drift from noise at zero set-point, a set-point below MIN RPM can be made invalid.
- Possible error messages: **ADW2**, **NO INPUT**.

## PDF page 23 (p. 49): 3.2.2 Digital speed set-point (optional), Fig. 19

- The set-point comes directly from the PLC, binary or floating-point coded, read in parallel every 1.2 ms. Recommended for severe interference and long lines.
- Resolution: binary 1 rpm; floating point 0.0005 rpm.
- Function conversion: digital set-point interface needed (type code field "Speed set-point = **D**").

| Parameter | Description | Function | Value |
|---|---|---|---|
| A06 | FUNCT 2 | Digital speed set-point, binary-coded | <0> |
| | | Digital speed set-point, floating-point coded | <2> |
| A01 | MAXRPM | Maximum speed of motor | 1 ... 24000 (1/rpm) |

Note: MAX RPM is not exceeded even with an excessive set-point.

## PDF page 24 (p. 50): binary coding, Fig. 20

Fig. 20 "Inputs for digital speed set-point, weighting for binary coding and control inputs". **X4 Digital set-point inputs:**

| X4 pin | Signal |
|---|---|
| 1 | Bit 1, 1 ≙ 1 rpm |
| 2 | Bit 2, 1 ≙ 2 rpm |
| 3 | Bit 3, 4 |
| 4 | Bit 4, 8 |
| 5 | Bit 5, 16 |
| 6 | Bit 6, 32 |
| 7 | Bit 7, 64 |
| 8 | Bit 8, 128 |
| 9 | Bit 9, 256 |
| 10 | Bit 10, 512 |
| 11 | Bit 11, 1024 |
| 12 | Bit 12, 2048 |
| 13 | Bit 13, 4096 |
| 14 | Bit 14, 8192 |
| 15 | Bit 15, direction of rotation *) (0 = clockwise, 1 = anticlockwise) |
| 16 | DATA VALID (1 ≙ accept current bit pattern) |
| 17 | (no label) |
| 18 | -15 V int (output) |
| 19 | +15 V int (output) |
| 20 | (no label) |
| 21 | 0 V ext (input) |
| 22 | 0 V ext (input) |
| 23, 24, 25 | (no label) |

Bits 1-14: binary-coded set-point, "0" ≙ 0 1/min. X2 inputs: 23 E-STOP (RAC only) = 1, 24 RF = 1, 25 RUN 1 ≙ enable speed set-point, 29 OSCILLATE = 0, 37 EXT POS: 1 ≙ set-point reduced by a factor of 100, 3 AS (KDA/TDA only) = 1.

| EXTPOS | N set-point min | N set-point max |
|---|---|---|
| 0 | 1 | 16383 |
| 1 | 0.01 | 163.8 |

Note: DATA VALID = 1, pattern read continuously; DATA VALID = 0, the last pattern read stays effective.

## PDF page 25 (p. 51): floating-point coding, Fig. 21

X4 pinout as Fig. 20, but bits 1-4 = **exponent** (1, 2, 4, 8) and bits 5-14 = **mantissa** (bit 5 ≙ 1 rpm, then 2, 4, 8, 16, 32, 64, 128, 256, 512), "0" ≙ 0 1/min. Bit 15 = direction (0 clockwise, 1 anticlockwise); 16 DATA VALID; 18 -15 V int; 19 +15 V int; 21, 22 0 V ext.

- N set-point = mantissa × 2^(exponent − 11)
- min N set-point = 0.00049 1/rpm; max N set-point = 16368 1/rpm
- In this case the EXTPOS input has no significance.

Note on DATA VALID as on page 24. Possible error message: **NO INPUT**.

## PDF page 26 (p. 52)

"For technical reasons this page remains blank" (so the manual's Fig. 22 is missing or was never printed).

## PDF page 27 (p. 53): 3.3 Main spindle speed signals, Fig. 23

The main spindle speed decides when feed is enabled, gear changes and the holding brake is clamped. Signal outputs "N_actual = N_set-point" and "N < N_min", plus a selectable torque cut-off below N_min. Thresholds are adjustable.

Fig. 23 "Control inputs, signal outputs and displays for main spindle speed":
- Inputs: X2/24 RF (1 ≙ controller enabling signal), X2/25 RUN (1 ≙ speed set-point enable), X2/23 = 1, X2/29 = 0, X2/3 AS = 1.
- **X2 Signal outputs:** **X2/5 N = CMD** (1 ≙ N_actual = N_set-point); **X2/6 N < MIN** (1 ≙ N_actual < N_min).

| Display message | N = CMD | N < MIN |
|---|---|---|
| N = NCMD | 1 | 0 or 1 |
| N * NCMD (N ≠ NCMD) | 0 | 0 or 1 |
| N < NMIN | 0 or 1 | 1 |

Note: "N * NCMD" means N_actual ≠ N_set-point; it shows above minimum speed before the set-point speed is reached.

## PDF page 28 (p. 54): Fig. 24 "Parameters for main spindle speed signals"

| Parameter | Designation | Function | Value |
|---|---|---|---|
| A03 | MIN RPM | Speed limit below which output "N < MIN" goes to 1 | 1 ... 999 (1/rpm) |
| A04 | MAX DEV | Max. deviation of speed from set-point within which "N = CMD" goes to 1 | 1 ... 999 (1/rpm) |
| A05 | FUNCT 1 | "N = CMD" also activated without controller enable (RF = 0) | <256> |
| | | MAX DEV entered as %: 1 = 0.1 % of the current N set-point | <512> |
| | | Torque disconnection when speed is below MINRPM | <4> |

Notes: with MAX DEV as a percentage, the smallest deviation is limited to 10 rpm. With RF = 0 the controller can check standstill via "N = CMD" (set-point must be 0). Torque disconnection below MINRPM is useful if the motor must be locked in position after braking without removing RF.
**Safety note:** "N < MIN" does **not** meet personnel-protection requirements for the working area of the main spindle.

## PDF page 29 (p. 55): 3.4 Speed set-point ramps, Fig. 25

The control unit can turn sudden set-point steps into ramps. There are three successive ramp slopes, e.g. for breakaway damping and acceleration limiting with belt couplings; in the constant-power range the third ramp keeps regulation active. Ramp slopes can be set for up to 4 speed reduction stages (gear stages, via records).

Fig. 25 inputs: X2/23 E-STOP: 0 ≙ Emergency Stop (NOT AUS); X2/24 RF: 1 ≙ controller enabling signal; X2/25 RUN: 1 ≙ set-point enable; X2/37 EXT POS: 1 ≙ change of weighting; X2/29 = 0; X2/3 AS (KDA/TDA only) = 1.

Note: ramps are active when RF and RUN switch on and off. If ramps must also act on a change of weighting (EXT POS), during Emergency Stop and Mains Off, set this in the function parameters.

## PDF page 30 (p. 56): Fig. 26 "Parameters for speed set-point ramps"

| Parameter | Designation | Function | Value |
|---|---|---|---|
| S/R/Q P01 | RAMP 1 | Ramp 1 off / slope of ramp 1 | 0 / 1 ... 999 (rad/s²) |
| S/R/Q P02 | RPM 1 | End speed of ramp 1 | 1 ... 24000 (1/rpm) |
| S/R/Q P03 | RAMP 2 | Ramp 2 off / slope of ramp 2 | 0 / 1 ... 999 (rad/s²) |
| S/R/Q P04 | RPM 2 | End speed of ramp 2 | 1 ... 24000 (1/rpm) |
| S/R/Q P05 | RAMP 3 | Ramp 3 off / slope of ramp 3 | 0 / 1 ... 999 (rad/s²) |
| A06 | FUNCT 2 | Ramp also active when switching EXT POS and Mains Off (all controllers) and when switching E-STOP (RAC only) | <32> |

Notes: all three ramps must be set when the ramp function is used. Slopes are in rad/s² (Fig. 27).

## PDF page 31 (p. 57): Fig. 27 "Converting speed ΔN and ramp-up time Δt to RAMP (rad/s²)"

Graph: Δt/s (0-3) against ΔN/min⁻¹ (0-10000), with a RAMP axis at Δt = 1 s (marks 100, 500, 1000). Example 1: ΔN = 5000 1/rpm in Δt = 2 s ⇒ RAMP = 250 rad/s². Example 2: ΔN = 9000 1/rpm in Δt = 1 s ⇒ RAMP = 940 rad/s². Method: mark (ΔN, Δt), draw a line through the origin, and read RAMP where it crosses the RAMP axis.

Formula: **RAMP = π/30 × ΔN/Δt** × min/s (ΔN in rpm, Δt in s).

## PDF page 32 (p. 58): Fig. 28 "Speed ramps for step change in speed set-point"

Function sequence: a set-point step U_analogue ≙ A01 (CMD VOLT) makes the motor accelerate along RAMP1 (S/R/Q P01) up to RPM1 (P02), then RAMP2 (P03) up to RPM2 (P04), then RAMP3 (P05) up to A01 (MAX RPM). When the set-point drops back, the ramps run in reverse order.

## PDF page 33 (p. 59): 3.5 Speed set-point smoothing, Fig. 29

When the NC ramps the spindle, or in positioning mode, staircase set-point changes from the NC cause torque jumps. Smoothing damps them, with adjustable time constants per speed reduction stage. Fig. 29 inputs: X2/23 = 1, X2/24 RF = 1, X2/25 RUN = 1, X2/29 = 0, X2/37 EXT POS: 1 ≙ change of weighting, X2/3 = 1.
Note: smoothing is inoperative when the weighting is changed with EXT POS = 1, but can be made operative via parameters.

## PDF page 34 (p. 60): Fig. 30 "Parameters for smoothing speed set-point"

| Parameter | Designation | Function | Value |
|---|---|---|---|
| S/R/Q P01 | RAMP 1 | Value for smoothing time constant | 0 ... 11 |
| S/R/Q P02 | RPM 1 | No function, but value 0 must be defined | 0 |
| S/R/Q P03 | RAMP 2 | Ditto | 0 |
| S/R/Q P04 | RPM 2 | Ditto | 0 |
| S/R/Q P05 | RAMP 3 | Ditto | 0 |
| A06 | FUNCT 2 | Smoothing also active for EXT POS = 1 | <32> |

**Tg = 0.5 × 2^RAMP1 ms** (digital filter; so 0.5 ms ... 1024 ms). Within one parameter record, smoothing excludes the ramp function. Works for analogue and digital set-points.

## PDF page 35 (p. 61): 3.6 Spindle positioning, Fig. 31

- Three standard spindle positions (for tool change on milling centres). Option: **3600 selectable positions** (e.g. workpiece indexing on lathes).
- Accuracy: position set-points to 0.1°, corrected to max. 1/1000°.

Fig. 31 "Mechanical coupling and positioning methods":

| Mechanical coupling | Method | Sections |
|---|---|---|
| Solid gearing, i = 1:1 | via motor feedback | 3.6.1, 3.6.2 |
| Change-speed gear, i = optional | via motor feedback + spindle reference switch | 3.6.1, 3.6.3 |
| Gearing with slip, i = optional | via spindle feedback | 3.6.1, 3.6.4 |

## PDF page 36 (p. 62): Figs. 32-34, the three methods

- Fig. 32, motor feedback, solid gearing i = 1: RAC or KDA/TDA, power input, power output to the **2AD main spindle motor**. X2 = spindle positioning (from the controller); X3 = spindle & rotor position (motor encoder). The motor is coupled directly to the spindle.
- Fig. 33, motor feedback + spindle reference switch with change-speed gear: X3 = rotor position from the 2AD motor; the reference switch signal from a switch at the spindle goes to X2; a gearbox sits between motor and spindle.
- Fig. 34, spindle feedback with slipping gearing (belt): X3 = rotor position; **X5 = spindle position from an incremental encoder** belt-driven by the spindle; X2 = spindle positioning.

## PDF page 37 (p. 63): 3.6.1 Basic data, Fig. 35

a) Standard version, three spindle positions: the PLC inhibits the speed set-point by removing RUN, then selects one of 3 positions in binary with POS1/POS2.

| POS 1 (X2/26) | POS 2 (X2/27) | Selected spindle position | Parameter |
|---|---|---|---|
| 0 | 0 | No position | none |
| 1 | 0 | Position 1 | A08 |
| 0 | 1 | Position 2 | A09 |
| 1 | 1 | Position 3 | A10 |

All positions refer to A07 (OFFSET). X2/25 RUN: 0 ≙ speed set-point inhibited. Output **X2/7 INPOS**: 1 ≙ "in position" (tolerance window P-WINDOW, P/Q/R/S21). Other inputs: X2/23 = 1, X2/24 = 1, X2/29 = 0, X2/3 = 1.

Note: with "no position" selected, the drive brakes to standstill and stays in speed control without drift; display NO RUN. It returns to speed control when RUN = 1.

## PDF page 38 (p. 64): Fig. 36 "Parameters for spindle positioning (standard)"

| Parameter | Designation | Function | Value |
|---|---|---|---|
| A07 | OFFSET | Angular offset of zero pulse and spindle reference point | 0 ... 360.0 (degr.) |
| A08 | POS 1 | Spindle position 1 | 0 ... 360.0 (degr.) |
| A09 | POS 2 | Spindle position 2 | 0 ... 360.0 (degr.) |
| A10 | POS 3 | Spindle position 3 | 0 ... 360.0 (degr.) |
| A06 | FUNCT 2 | Direction of positioning (valid in all records PQRS): shortest path / as per sign of N set-point | <0> / <16> |
| S/R/Q P16 | PQ-FUNCT | Direction of positioning, per active record (A06 must not be <16>): shortest path / as per sign of N set-point | <0> (A06 ≠ <16>!) / <128> (A06 ≠ <16>!) |
| S/R/Q P17 | POS GAIN | Gain factor in position control loop; 16.6 ≙ Kv = 1 | 0 ... 99.9 (1/s) |
| S/R/Q P18 | POS RPM | Positioning speed (limited to 30 × POSGAIN) | 0 ... 9999 (1/rpm) |
| S/R/Q P21 | P-WINDOW | Tolerance window inside which "INPOS" appears | 0.01 ... 99.99 (degr.) |
| S/R/Q P12 | GEAR IN | Reduction ratio i = N1/N2, N1 | 1 ... 999 |
| S/R/Q P13 | GEAR OUT | N2 | 1 ... 999 |

Notes: values refer to the spindle. Positions refer to OFFSET. Direction is set in A06 for all records, or per record via PQ-FUNCT (PQRS16), in which case A06 must not contain <16>.

## PDF page 39 (p. 65): option 3600 positions, Fig. 37

Precondition: digital position input interface (type code field "additional interface" = **D**).

Fig. 37 "Assignment of inputs for digital position set-value, control inputs and signal outputs". Inputs for digital positioning: **X6 for KDA 3; X13 for RAC 2 and RAC 3**:

| Pin | Signal (BCD-coded position input) |
|---|---|
| 1 | 1 × 0.1° |
| 2 | 2 × 0.1° |
| 3 | 4 × 0.1° |
| 4 | 8 × 0.1° |
| 5 | 1 × 1° |
| 6 | 2 × 1° |
| 7 | 4 × 1° |
| 8 | 8 × 1° |
| 9 | 1 × 10° |
| 10 | 2 × 10° |
| 11 | 4 × 10° |
| 12 | 8 × 10° |
| 13 | 1 × 100° |
| 14 | 2 × 100° |
| 15 | Positioning direction *) (0 = shortest path, 1 = as per speed set-point) |
| 16 | DATA VALID (1 ≙ enable bit pattern) |
| 17 | (no label) |
| 18 | -15 V int |
| 19 | +15 V int |
| 20 | (no label) |
| 21 | 0 V ext |
| 22 | 0 V ext |
| 23-25 | (no label); screen/earth symbol at pin 25 [uncertain] |

X2: 23 E-STOP (RAC) = 1, 24 RF = 1, 25 RUN: 0 ≙ inhibit N set-point, 26 POS 1 = 0, 27 POS 2 = 0, 29 = 0, 3 AS (KDA/TDA) = 1, 18 drawn [no function given]. Output X2/7 INPOS (1 ≙ inside tolerance window P-WINDOW, P/Q/R/S21).

Notes: RUN must be 0 when giving digital position set-points. The bit pattern is taken over with DATA VALID; if it changes while DATA VALID = 1 the drive moves at once to the new position. The three parameter positions have priority, so POS 1 and POS 2 must be 0.

## PDF page 40 (p. 66): Fig. 38, 3.6.2, Fig. 39

Fig. 38 "Parameters for spindle positioning (3600 positions)": A07 OFFSET 0 ... 360.0°; S/R/Q P17 POS GAIN 0 ... 99.9 1/s (16.6 ≙ Kv = 1); P18 POS RPM 0 ... 9999 rpm (≤ 30 × POSGAIN); P21 P-WINDOW 0.01 ... 99.99°; P12 GEAR IN N1 1 ... 999; P13 GEAR OUT N2 1 ... 999. Values refer to the spindle; digital set-points are relative to OFFSET.

3.6.2 Spindle positioning via motor feedback, Fig. 39 "Additional parameters to those in 3.6.1":

| Parameter | Designation | Function | Value |
|---|---|---|---|
| S/R/Q P16 | PQ-FUNCT | Direction of rotation of motor and spindle: same / opposite | <0> / <2> |

## PDF page 41 (p. 67): Fig. 40 "Operating sequence for spindle positioning via motor feedback (e.g. direction of rotation as per sign of applied speed set-point)"

Timing traces: RUN X2/25, POS 1 X2/26, POS 2 X2/27, motor speed N (also on X2/21), rotor position 0-360°, INPOS X2/7, display.
Sequence: with RUN = 1 the display shows N = NCMD. RUN drops to 0 with POS2 = 1: the motor brakes, searches the zero, moves at POS RPM (P18) into P-WINDOW (P21) around A09 (POS 2); INPOS = 1. Display goes "POS ORDER" → "INPOS". Then POS1 + POS2 selects A10 (POS 3), then POS1 alone selects A08 (POS 1), each time "POS ORDER" → "INPOS". RUN = 1 again: "Motor accelerates to set-point speed", display N * CMD → N = CMD. Positions are measured from A07 (OFFSET). Direction per A06 (FUNCT 2) = <16>, as per sign of N set-point.
Note: positioning accuracy is **0.1° with motor feedback type 1** and **0.001° with motor feedback type 3** (type code field "Motor feedback" of the motor).

## PDF page 42 (p. 68): 3.6.3 motor feedback with spindle reference switch, Fig. 41

| Parameter | Designation | Function | Value |
|---|---|---|---|
| S/R/Q P16 | PQ-FUNCT | Direction of rotation of motor and spindle: same / opposite | <0> / <2> |
| | | Spindle reference signal at **X2/37** | <64> |
| | | Attention: value not permitted | <65> crossed out [reading of the crossed-out value uncertain] |
| S/R/Q P15 | P-MAXRPM | In this case: spindle speed for locating the reference switch signal | 1 ... 24000 1/rpm |

Notes: the sign of the search speed is the sign of the set-point when positioning starts (RUN 1 → 0). If the controller must set the set-point to zero, a positive or negative search speed can be set.
Reference switch: its signal goes to the **EXT POS** input. α_min = minimum dwell angle of the switch; max. reference error = α_min + switch hysteresis. Hysteresis is cancelled if the switch is always searched in the same direction (set-point always has the same sign at start). α_min depends on the search speed P-MAXRPM.

## PDF page 43 (p. 69): Fig. 42 "Reference switch connections and dwell angle"

A spindle reference switch (3-wire, PNP) senses a cam on the spindle (angle α drawn). Connections:
- switch supply + → **+24 V ext** (also X2/19 +24 V ext);
- switch supply − → **0 V** (X2/1 0 V ext);
- switch output → **X2/37 EXT POS**;
- other X2 terminals shown: X2/2 0 V int, X2/18 +24 V int, **X2/17 (screen)**.

α_min = P-MAXRPM × 550·10⁻⁶ s × 6° × min/s.
Notes: e.g. an inductive proximity switch, **Balluff BES 516 series**. Use a screened cable and connect the screen to **X2/17**; do not run it parallel to power cables. INPOS comes only from comparing set-point and actual position indirectly, so monitor the spindle position with a second proximity switch; position errors can damage the machine.

## PDF page 44 (p. 70): Fig. 43 "Operating sequence for spindle positioning via motor feedback with reference switch (e.g. direction of rotation as per sign of applied speed set-point)"

As Fig. 40, with an extra trace **EXT POS X2/37** (reference switch pulse, width α). After RUN → 0 the spindle runs at P15 (P-MAXRPM) until the reference edge, with display "HOMING". It then positions at P18 (POSRPM) into P21 (P-WINDOW): "POS ORDER" → "INPOS". Positions A09 (POS 2), A10 (POS 3), A08 (POS 1) refer to A07 (OFFSET). Direction per A06 (FUNCT 2) = <16>.
Notes: OFFSET refers to the rising edge of the reference pulse for clockwise and the trailing edge for anticlockwise rotation, so the same physical edge is used both ways. OFFSET = angle between the positioning reference point and that edge. Accuracy 0.1°/i (motor feedback type 1) and 0.001°/i (type 3).
Last line (text continues in the next part): "Possible fault message: HOMING remains in the display".

---

## For the retrofit

This part is the Indramat manual, not the CTX wiring. Read it together with the machine circuit diagrams (other parts of chapter 5, chapter 7 hardware) to find which X2/X4 terminals Gildemeister actually wired and to which ELTROPILOT/PLC points. The software version string on these pages is the example "RAC 4 V 1.3". Read the real controller type (RAC 2.x / RAC 3 / KDA 3 / TDA), its type code (speed set-point A or D, additional interface S/D, SERCOS, incremental encoder output) and the parameter list from each drive and from the parameter form in the annex.

### Keeping the original spindle drives (LinuxCNC)

- **Drive type:** Indramat RAC (or KDA/TDA) main spindle controller with 2AD induction motor (the CTX twin "integrated spindle motors" are probably 2AD/1MB-type built-in motors [verify on the nameplate]). Parameters live in the plug-in **AS programming module** (EEPROM). Before any work, read every A, P, Q, R, S, M parameter and the SUM checksum (Fig. 9) and note the software version. A spare copy can be made with the PDA 1 adapter.
- **Speed command:** standard is a **±10 V differential analogue input on X4/1-2 (E1/E2)**, with a second input X4/5-6 (E3/E4) and the screen on X4/4 at the drive end only. Scaling: A02 CMD VOLT (6-10 V) ≙ A01 MAX RPM, or ≙ P-MAXRPM (PQRS15) of the active record when **EXT POS (X2/37) = 1**. The set-point is sampled every 1.2 ms. A Mesa analogue output (e.g. 7i77, 7i83 or 7i48-type, differential ±10 V) can drive it. If the drive has the digital set-point option (type code D), X4 is instead a 16-bit parallel interface (14 bits + direction + DATA VALID, binary or floating point), which would need 16 isolated 24 V outputs [check type code].
- **Enable and handshake:** inputs X2/24 **RF** (controller enable), X2/25 **RUN** (set-point enable), X2/23 **E-STOP** (RAC, must be 1), X2/3 **AS** starting lockout (KDA/TDA). They are 24 V logic referred to X2/1 (0 V ext) and X2/19 (+24 V ext), or jumpered to the internal supply X2/2, X2/18. After a parameter change the drive needs RF switched 0 → 1 ("RF-AGAIN").
- **Status outputs (24 V):** X2/4 **READY**; potential-free **"Bb"** contact on X15 (RAC) / X11 (KDA), to be put in the E-stop/ready chain; X2/5 **N = CMD** (at speed, window A04); X2/6 **N < MIN** (zero speed, A03; not a safety signal); X2/7 **INPOS**. Fault status shows on the LED and display only. These pages describe no separate fault output, so READY/Bb must serve as the fault signal into LinuxCNC.
- **Gear / records:** PAR1/PAR2 (X2/30/31) select record P/Q/R/S (different MAX RPM, ramps, gains, gear ratio GEAR IN/OUT P12/P13). LIMIT1/2/4 (X2/32-34), MD-RED (X2/28, torque reduction), OSCILLATE (X2/29), SPEED1/2 (X2/35/36) are further 24 V inputs; their function is in sections 3.7-3.15 (not in this part). LinuxCNC (via the HAL or classicladder ladder logic) must drive the same lines as the original PLC.
- **Ramps:** the drive can ramp internally (PQRS P01-P05, RAMP = π/30·ΔN/Δt) or smooth the set-point (Tg = 0.5·2^RAMP1 ms). For C-axis, rigid tapping and synchronous spindles, ramps and smoothing must be off in the record used, so that LinuxCNC's position loop is not delayed.
- **Orientation:** the drive orients itself. RUN = 0, then POS1/POS2 select A08/A09/A10 relative to A07 OFFSET, with INPOS on X2/7, speed P18 and window P21. The 3600-position BCD option uses X6 (KDA 3) or X13 (RAC 2/3) with DATA VALID. With a reference switch the switch goes to X2/37 EXT POS (screen X2/17, e.g. Balluff BES 516). For spindle-orient (M19) under LinuxCNC this can be kept as is, with `spindle.0.orient` mapped to RUN = 0 plus POS bits and `orient-is-oriented` to INPOS. Alternatively do the orientation in LinuxCNC's own position loop through the analogue command.
- **Spindle encoder / C axis / threading / twin transfer sync:** the drive reads motor feedback on **X3** (rotor position; type 1 = 0.1°, type 3 = 0.001° positioning resolution) and an optional **spindle incremental encoder on X5**. LinuxCNC needs its own spindle position for threading (`spindle.N.revs`, index), for C axis and for synchronous transfer between the two spindles. Check whether the machine has a separate spindle encoder (ROD/ERN) wired to the NC or only to X5. If only the drive sees it, use the optional **incremental encoder output** (section 3.24, type code) or tap the encoder with a splitter. C-axis via analogue speed command is supported by the drive ("C-axis drive for lathes", high-resolution set-point weighting by record, EXT POS weighting switch). The C-axis function itself (3.8) is in the next part. Both spindles need a closed position loop in LinuxCNC (PID on analogue command, encoder feedback) to synchronise for part transfer.
- **Diagnostics:** analogue output **N (X2/21, ±10 V)** can show motor speed, set-point, spindle encoder speed (±10 V ≙ ±10 000 rpm), torque, position, link voltage (10 V ≙ 1000 V), current, power, motor winding temperature (0-10 V ≙ 0-150 °C). It is useful as a spare analogue input to LinuxCNC (load meter, temperature). It is selectable only from the keypad, with no remote selection; the setting is kept.
- **Temperature monitoring:** the drive monitors the motor winding itself (ready only for 0 < t < 155 °C, value shown as T=). The temperature pre-warning output (3.11) and LOAD LIMIT output (3.10.2) are in the next part; wire them to LinuxCNC inputs if present.

### Replacing the drives

- The replacement must handle the motor's feedback (Indramat 2AD/1MB motor encoder on X3: check the "motor feedback" type code, type 1 or 3, probably a sin/cos or incremental encoder) and its KTY winding temperature sensor (limit 155 °C). If the new drive cannot read the motor encoder, a separate spindle encoder is needed.
- Copy the motor data (record M: M02 feedback type, M03 T-FILTER, M15 MOTFUNCT, etc.) and MAX RPM, gear ratios and gains before removal. These pages give only parameter names and ranges, not the CTX values.
- Features LinuxCNC must take over if the new drive lacks them: ramps, at-speed (N = CMD), zero-speed (N < MIN), orientation (3 positions plus OFFSET), the parameter-record/gear switching (PAR1/PAR2), torque reduction (MD-RED), and E-STOP/Bb contact behaviour (braking on E-stop and on mains failure, sections 3.18-3.20, next part).
- Mains, link-circuit and braking arrangement: RAC units have their own mains infeed, while KDA/TDA run on a shared DC link (TVD/KDV supply). Check which one is fitted (PD scaling: RAC 2.2 ±100 kW, RAC 3/KDA 3 ±50 kW), because it decides whether the replacement needs its own infeed and braking resistor.
