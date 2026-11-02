# Gildemeister CTX twin, electrical documentation: 6. Servo drives (part a) - Indramat DDS 2.1 and MDD with ANALOG interface, Applications Manual, PDF pages 1-40

Source: `28bb1bf4-6_Servo_drives_a_20201029_0001.pdf` (chapter "6 Servo drives", part a, 40 scanned pages). This part is **not a Gildemeister drawing**: it is the first 40 pages of the Indramat manual bound into the machine documentation:

- Title: "DDS 2.1 and MDD - Digital intelligent AC servo drives with ANALOG interface - Applications Manual"
- **Doc. No. 209-0069-4315-03 EN/12.93**, date "December 1993" (footer: "DDS 2.1 and MDD with ANALOG interface · 209-0069-4315-03 · December 23, 1993"), internal storage index "File 11b", replaces Doc. No. 209-0069-4315-02/06.93.
- Printed page numbers = PDF page numbers (PDF page 1 = cover, PDF page 40 = manual page 40). The scan stops in the middle of section 5.1; the rest of the manual (sections 5.1 cont. to 13, including the pin allocations in 11.6 and the connection diagrams 11.4/11.5) is presumably in part b.

Original language: **English** (Indramat's English edition). Only a few German words appear ("Achtung", "binär Cod.", "SPRACHAUSWAHL"); they are given with a translation.

Because this is a generic manufacturer manual, there are **no Gildemeister device tags (-K.., -X.. cabinet terminals, wire numbers)** in these pages. The connector designations below (X1 ... X16, U1 ... U5, H1, S1) are the drive's own front-panel designations.

Notation: [uncertain] = reading not sure, [unreadable] = cannot be read.

## Page index

| PDF page | Printed page / section | Content |
|---|---|---|
| 1 | cover | Title page: Indramat, "DDS 2.1 and MDD Digital intelligent AC servo drives with ANALOG interface, Applications Manual", 209-0069-4315-03 EN/12.93 |
| 2 | - | blank (back of cover) |
| 3 | 3 | Section register (tab overview, sections 1-13) |
| 4 | 4 | blank |
| 5 | 5 | About this documentation: title, document type, doc. no., date, use, copyright, validity |
| 6 | 6 | Fig. 1 Documentation overview (part 1) |
| 7 | 7 | Documentation overview (part 2): selection data, MDD motor project planning manuals, SYSDA, glossary |
| 8 | 8 | blank |
| 9 | 9 | Contents (sections 1-7.7) |
| 10 | 10 | Contents (7.8-13), headed "Inhaltsverzeichnis" (table of contents) |
| 11 | 11 | 1. The digital intelligent AC servo drive (applications, SERCOS / ANALOG interface) |
| 12 | 12 | SERCOS interface, Fig. 2 |
| 13 | 13 | ANALOG interface, Fig. 3 (velocity command ±10 V, position actual value output) |
| 14 | 14 | 2. ... with ANALOG interface: modular drive package, Fig. 4 |
| 15 | 15 | 2.1 Components of the digital AC servo drive, Fig. 5 |
| 16 | 16 | 2.2 Function schematic, Fig. 6 |
| 17 | 17 | 2.3 System configuration, Fig. 7 components of a configured drive controller (slots U1-U5) |
| 18 | 18 | Distinguishing criteria, Fig. 8 (RA01 / DA01 / RA02 / DA02) |
| 19 | 19 | Configuration rating plate Fig. 9, type code Fig. 10 |
| 20 | 20 | System configurations RA 01 and DA 01 |
| 21 | 21 | System configurations RA 02 and DA 02 |
| 22 | 22 | 2.4 System configuration components: DDS 2.1 basic unit front, Fig. 15 (X1-X6, U1-U5, H1, S1) |
| 23 | 23 | Rating plate and type code of the basic unit, Figs. 16, 17 |
| 24 | 24 | Command communication module DAE 1.1 / DAA 1.1, Fig. 18 (terminal blocks X13 / X15, X14 / X16) |
| 25 | 25 | Type codes: command communication module (DAA/DAE/DSS), additional plug-in module (DEF/DLF/DZF) |
| 26 | 26 | Software module DSM, Fig. 21 |
| 27 | 27 | Software module rating plate and type code, Figs. 22, 23 |
| 28 | 28 | Digital AC servo motors: feedback versions DSF / RSF / MTG, Fig. 24 |
| 29 | 29 | Type code MDD motors, Fig. 25 |
| 30 | 30 | MDD rating plate, Fig. 26 (example MDD090C-N-020-N2L-110GB0/S016) |
| 31 | 31 | 3. Fundamental safety notes: 3.1 symbols, 3.2 use as prescribed |
| 32 | 32 | 3.3 commissioning, 3.4 protection of personnel (E-STOP, dangerous terminals X5, L1/L2/L3) |
| 33 | 33 | Fig. 28 protective plates; lethal voltage warning; no earth-leakage breakers |
| 34 | 34 | 3.5 protection of equipment, 3.6 protection of machinery |
| 35 | 35 | 4. Commissioning equipment, 4.1 measuring instruments, Fig. 29 X3 pinout (analog diagnostics outputs) |
| 36 | 36 | 4.2 Battery power source, Fig. 30 (circuit to DAA X15) |
| 37 | 37 | 4.3 Service cable IN 391 (RS-232, X2 pinout), 4.4 PC |
| 38 | 38 | 4.5 VT 100 terminal settings (9600 baud) |
| 39 | 39 | 5. Operating the drive controller, 5.1 main menu |
| 40 | 40 | Drive status menu (TVW, RF, AH, Ired1, Ired2, Nullimp.), parameters menu, language selection |

---

## PDF page 1: Cover

- Logo: INDRAMAT
- "DDS 2.1 and MDD / Digital intelligent AC servo drives / with ANALOG interface"
- "Applications Manual"
- Empty frame (label field)
- "209-0069-4315-03 EN/12.93"

## PDF page 2: blank

Back of the cover, no content.

## PDF page 3: Section register

| Section | Title |
|---|---|
| - | About this documentation |
| - | Contents |
| 1 | The digital intelligent AC servo drive |
| 2 | The digital intelligent AC servo drive with ANALOG interface |
| 3 | Fundamental safety notes |
| 4 | Commissioning equipment |
| 5 | Operating the drive controller during commissioning and diagnostics |
| 6 | Commissioning procedure up to initial start-up of the AC servo drive |
| 7 | Commissioning the functions of the digital AC servo drive with ANALOG interface |
| 8 | Final commissioning work |
| 9 | Diagnostics and fault clearance |
| 10 | Overview of parameters |
| 11 | Appendix |
| 12 | Glossary |
| 13 | List of keywords |

## PDF page 4: blank

## PDF page 5: About this documentation

| Field | Value |
|---|---|
| Title | DDS 2.1 and MDD, Digital intelligent AC servo drives with ANALOG interface |
| Type of documentation | Applications manual |
| Doc. No. | 209-0069-4315-03 |
| Date | December 1993 (year partly blurred, "1993" from footer) |
| Internal storage index | File 11b |
| Replacement for | Doc. No. 209-0069-4315-02/06.93 |

"This document is used:"
- as a practical instruction manual for commissioning the digital AC servo drive with ANALOG interface and is intended for trained personnel.
- to explain the safety considerations when handling the digital AC servo drive.
- to explain the drive controller functions and their application to the machine.
- to parametrize the drive controller during commissioning.
- to ensure security of the parameters entered on completion of commissioning.
- for fault diagnostics and fault clearance on the digital AC servo drive.

Copyright: copying, giving to others, use or communication forbidden without express authority; offenders liable for damages; all rights reserved for patent / utility model.
Validity: all rights reserved with respect to the content of this documentation and the availability of the products.

## PDF page 6: Documentation overview "Digital intelligent AC servo drives" (Fig. 1, part 1)

Rows (phase: documents):

| Phase | Document | Doc. No. |
|---|---|---|
| Information / Pre-selection | Digital intelligent AC main spindle and AC servo drives with SERCOS interface | 71800 |
| Information / Pre-selection | Digital intelligent AC servo drives | 71801 |
| Selection / Dimensioning (Dimensioning) | DDS 2 and MDD, Digital intelligent AC servo drives, Dimensioning manual (dimensioning of AC servo axes, calculation examples) | 209-0069-4320 |
| Selection / Dimensioning (Selecting functions) | DDS 2.1 and MDD, System configuration manual (list of proven and available system configurations, how to order) | 209-0069-4312 |
| Project planning (design, assembly, installation) | DDS 2.1 Digital AC servo drive controller, Project planning manual (technical, design data; assembly, installation; interfaces; available equipment/options/accessories; connection requirements) | 209-0069-4317 |
| Project planning | MDD Digital AC servo motors, Application manual (installation notes, assembly, transportation/storage) | 9.578.066.4 |
| Project planning | MDD Digital AC servo motors, Excerpts from standards | 9.578.067.4 |
| Project planning | Fans for mounting on MDD servo motors, Project planning manual (technical data, design data, assembly, fan types for MDD motors) | 9.578.003.4 |
| Project planning | DDS 2 Digital AC servo drive controller, Reference list, linear scales | 209-0069-xxxx (as printed) |
| Application (commissioning, operation, diagnostics) | **"this is the documentation in front of you"**: DDS 2.1 and MDD with ANALOG interface, Application manual (commissioning, diagnostics, fault clearance, overview of parameters) | 209-0069-4315 |
| Application | DDS 2.1 and MDD with SERCOS interface, Application manual | 209-0069-4321 |
| Application | Starting lockout on DDS 2 drive controllers, Application manual (protecting persons against unintentional start-up of servo motors, plant reliability) | 209-0069-4313 |

## PDF page 7: Documentation overview (part 2)

| Phase | Document | Doc. No. |
|---|---|---|
| Information | Digital intelligent AC servo drives, System presentation | 209-0069-4314 |
| Information | Digital AC servo motors, Overview (power steps, dimensions, application notes) | 9.578.XXX.4 (as printed) |
| Selection ("Selecting motor/controller combinations according to power steps") | DDS 2 and MDD running on power supply modules with direct mains connection, Selection data (stabilized DC link circuit voltage) | 209-0069-4302 |
| Selection | DDS 2 and MDD running on power supply modules for 220 V mains connection, Selection data (non-stabilized DC link circuit voltage) | 209-0069-4318 |
| Project planning | Digital AC servo motors, Project planning manual, one per frame size (see below) | |
| Application | SYSDA 1.1 commissioning equipment for digital intelligent AC drives with SERCOS interface, Application manual | 209-0069-4322 |
| Application | Digital intelligent AC servo drives, Glossary | 209-0069-4316 |
| Application | Digital intelligent AC servo drives, Documentation overview | 209-0069-4319 |

MDD motor project planning manuals (stacked booklets; assignment of numbers to frame sizes read along the stack, [uncertain] for the exact pairing):

| Motor | Doc. No. |
|---|---|
| MDD021 | 9.578.060.4 |
| MDD025 | 9.578.061.4 |
| MDD041 | 9.578.062.4 |
| MDD065 | 9.578.056.4 |
| MDD071 | 9.578.058.4 |
| MDD090 | 9.578.057.4 |
| MDD093 | 9.578.054.4 |
| MDD095 | 9.578.063.4 |
| MDD112 | 9.578.059.4 |
| MDD115 | 9.578.064.4 |
| MDD117 | 9.578.065.4 |

(The MDD 093 used on the CTX twin feed axes would therefore have project planning manual 9.578.054.4 [uncertain pairing].)

## PDF page 8: blank

## PDF pages 9-10: Contents

| Section | Title | Page |
|---|---|---|
| 1. | The digital intelligent AC servo drive | 11 |
| 2. | The digital intelligent AC servo drive with ANALOG interface | 14 |
| 2.1 | Components of the digital AC servo drive | 15 |
| 2.2 | Function schematic of the digital AC servo drive | 16 |
| 2.3 | System configuration of the digital AC servo drive | 17 |
| 2.4 | System configuration components | 22 |
| 3. | Fundamental safety notes | 31 |
| 3.1 | Warning notes and symbols | 31 |
| 3.2 | Use as prescribed | 31 |
| 3.3 | Safety notes for commissioning | 32 |
| 3.4 | Notes on protection of personnel | 32 |
| 3.5 | Notes on protection of equipment | 34 |
| 3.6 | Notes on protection of machinery | 34 |
| 4. | Commissioning equipment | 35 |
| 4.1 | Measuring instruments | 35 |
| 4.2 | Battery power source | 36 |
| 4.3 | Service cable IN 391 | 37 |
| 4.4 | Personal computer (PC) | 37 |
| 4.5 | VT 100 Terminal | 38 |
| 5. | Operating the drive controller during commissioning and diagnostics | 39 |
| 5.1 | How to handle the parametrizing and diagnostics program | 39 |
| 5.2 | Saving data | 51 |
| 6. | Commissioning procedure up to initial start-up of the AC servo drive | 52 |
| 6.1 | Checks to be run with power disconnected | 52 |
| 6.2 | Checks to be run with control voltage applied | 55 |
| 6.3 | Entering parameters for initial start-up | 55 |
| 6.4 | Powering up | 56 |
| 6.5 | Checks to be run after switching on power | 56 |
| 6.6 | Initial start-up of the drive with a battery power source | 57 |
| 7. | Commissioning the functions of the AC servo drive with ANALOG interface | 58 |
| 7.1 | Error reaction | 58 |
| 7.2 | Safety lockout (drive interlock open) | 61 |
| 7.3 | Holding brake function | 64 |
| 7.4 | Operational status (Ready) | 67 |
| 7.5 | Temperature monitoring | 68 |
| 7.6 | Velocity command matching | 70 |
| 7.7 | Command value smoothing | 72 |
| 7.8 | Actual-position sensing by indirect relative position measuring system | 72 |
| 7.9 | Actual-position sensing by indirect absolute position measuring system | 76 |
| 7.10 | Master/slave mode | 80 |
| 7.11 | Torque reduction | 82 |
| 7.12 | Drift-free positioning of a servo axis | 85 |
| 7.13 | Velocity Loop | 86 |
| 7.14 | What to do in the event of a power failure or emergency shutdown | 88 |
| 7.15 | Setting the short-time operation torque "M_KB" | 89 |
| 8. | Final commissioning work | 90 |
| 8.1 | Preparations for running the axis with an NC control system | 90 |
| 8.2 | Compensating the servo axis drift | 91 |
| 8.3 | Checking the servo drive | 92 |
| 8.4 | Saving data | 94 |
| 9. | Diagnostics and fault clearance | 95 |
| 9.1 | Diagnosing the operating status of the controller using its status display | 95 |
| 9.2 | Status indications during drive initialization | 96 |
| 9.3 | Fault diagnostics and clearance using the drive controller status display | 97 |
| 9.4 | Input errors and faults due to errors while saving | 104 |
| 9.5 | Selecting signals for diagnostics outputs | 105 |
| 9.6 | Notes on component replacement | 107 |
| 10. | Overview of parameters | 108 |
| 11. | Appendix | 109 |
| 11.1 | Repair Card | 109 |
| 11.2 | Specific axis data list, to be completed by hand | 110 |
| 11.3 | Example of a parameter file printout | 112 |
| 11.4 | DDS 2.1 drive controller connection diagram for motors with resolver feedback | 114 |
| 11.5 | DDS 2.1 drive controller connection diagram for motors with digital servo feedback | 115 |
| 11.6 | Pin allocations/abbreviations on drive controller | 116 |
| 11.7 | Overview of INDRAMAT companies, subsidiaries and agencies | 120 |
| 12. | Glossary | 124 |
| 13. | List of keywords | 128 |

Page 10 header reads "Inhaltsverzeichnis" (German: table of contents). Item 7.8 has a hand/print correction (part of "sensing by indirect relative" overprinted in bold [uncertain]).

## PDF page 11: 1. The digital intelligent AC servo drive

Digital intelligent AC servo drives are microprocessor-controlled brushless three-phase AC drives with outstanding servo control characteristics in terms of dynamic response and precision. Applications listed: CNC machine tool axes; electronic transmissions, e.g. for gear milling machines; grinding machines; robots; handling systems; assembly equipment; woodworking machines; packaging machines; textile machines; printing machines.

All drive control, monitoring, parametrizing and diagnostics functions are digital, employing a signal processor, with an extremely high-resolution measurement of the rotor position over the entire speed range.

INDRAMAT digital intelligent AC servo drives can be supplied with either of two interfaces: **SERCOS interface** or **ANALOG interface**.

## PDF page 12: SERCOS interface

SERCOS = serial real-time communication system between NC controls and drives, proposed for standardization in draft standard DIN IEC/TC 44 by VDW and ZVEI.

Fig. 2 "CNC axis with digital three-phase AC servo drive and SERCOS interface" (drawing ref. ENA3A-Abb0.25/InbetrDDS): NC control (terminal with "Parameters, Diagnoses, Operating data") -> SERCOS interface <-> SERCOS interface of the digital intelligent drive controller -> drive microprocessor (fine interpolation; position, velocity, torque control; current vector controlling; parametrization, diagnostics) -> PWM current regulation -> motor M 3~; feedback "Rotor position" from motor feedback; "Current actual value" loop; optional linear scale to the drive controller.

Benefits only via SERCOS: display/entry of all drive data via the SERCOS terminal of the NC; speed resolution 0.0001 rpm up to maximum speed; internal fine interpolation and position loop (cycle time 0.250 ms, resolution 0.00001 mm to 180 m/min with linear scales of 20 µm graticule constant, free of following error); normalization of position, velocity, acceleration and load data, gearbox, spindle pitch etc.; reduced complexity of NC controls, wiring and encoders.

## PDF page 13: ANALOG interface

"DDS 2.1 Series drive controllers with ANALOG interface allow digital intelligent AC servo drives to be operated by conventional NC controls with a **±10V analog interface**."

Possible operating modes: **velocity control loop**; **torque control loop** (for master/slave mode, see Section 7.10).

Fig. 3 "CNC axis with digital three-phase AC servo drive and ±10V ANALOG interface" (ENA3A-Abb0.5/InbetrDDS):
- NC control (position loop): command W, actual X, difference X_d = W - X -> gain K_v -> D/A ("#/n") -> **Velocity command analog ±10V** -> drive controller.
- Drive controller with analog velocity command interface: A/D ("n/#") -> drive microprocessor (velocity loop and commutation) -> PWM current regulation -> motor; RS 232 to terminal (Parameters, Diagnoses, Operating data); rotor position from motor feedback; "Position interface" output -> **Position actual value (square pulses)** -> "Position interface" of the NC control.
- Legend: K_v = loop gain factor, W = command variable, X = control variable, X_d = W - X.

Differences from analog drives:
- Output of the rotor position either as an **incremental encoder signal** or an **absolute position encoder signal** for use as position actual value in the NC (no additional measuring system and wiring; shorter motor).
- Resolution of the position encoder signal matched by parametrization.
- Drift-free positioning after stopping via a switching input; position held by the drive's internal velocity loop while the controller is active.
- Control and monitoring of the holding brake through the drive controller.

## PDF page 14: 2. The digital intelligent AC servo drive with ANALOG interface

Modular construction: AC servo drives and AC main spindle drives of different power ratings combined into drive packages with **one common power supply module**. The power supply modules may be connected directly to three-phase mains **3 x AC (380 to 460) V, (50-60) Hz**. A digital intelligent AC servo drive = matched combination of AC servo motor **MDD** and **DDS 2.1** drive controller.

Fig. 4 "Digital AC servo drives combined to form a modular drive package" (ENA3A-Abb1/InbetrDDS): left to right: Mains power connection -> Power supply module -> AC main spindle drive (with its motor) -> Digital AC servo drive, drive controller DDS 2.1 (with AC servo motor MDD) -> Other digital AC servo drives (one with a motor, one with a small motor/feedback) [uncertain detail]. Busbars (DC link) run across the top of all modules, a ribbon bus cable (control voltage bus) along the middle.

## PDF page 15: 2.1 Components of the digital AC servo drive

Fig. 5 "Individual components of a digital AC servo drive with their designations" (ENA3A-Abb2/InbetrDDS): electrical connection accessories; configuration rating plate; transparent protective cover; plug-in modules; software module; configured DDS 2.1 drive controller; motor power cable; motor feedback cable; AC servo motor MDD.

## PDF page 16: 2.2 Function schematic

Fig. 6 "Interaction of NC control, digital drive controller and digital servo motor" (ENA3A-Abb3/InbetrDDS): same content as Fig. 3: NC position loop -> velocity command analog ±10 V -> drive (velocity loop and commutation, PWM current regulation) -> motor; feedback rotor position; "Position interface" -> position actual value (pulses) back to the NC "Position Interface"; RS 232 to terminal.

## PDF page 17: 2.3 System configuration of the digital AC servo drive

Drive controller, basic unit and components are combined into a configured drive controller; with an MDD motor they form "proven and readily available system configurations" (Fig. 11 to 14).

Fig. 7 "Components of a configured drive controller" (ENA3A-Abb3.1/InbetrDDS): basic unit "DDS 2.1" with slots **U5** (software module, top), **U1** (command communication module), **U2, U3, U4** (additional plug-in modules); transparent (protective) cover with configuration rating plate "SYSTEMCONFIGURATION" (small print: "DDS 2.1-W050-DS01-00" [uncertain], "DDS 2.1-W050-D", further lines [unreadable]); label "ATTENTION / Achtung" (German: attention).

Constituent parts of a configured drive controller: drive controller (basic unit); software module; command communication module; additional plug-in module; transparent (protective) cover; configuration rating plate.

## PDF page 18: Distinguishing criteria

The digital AC servo drive with ANALOG interface is distinguished by: 1. type of position actual-value evaluation (incremental / absolute); 2. motor feedback version. Combinations are identified by an alphanumeric code (e.g. RA01).

Fig. 8 "Distinguishing criteria for selection of the system configuration" (ENA3A-Abb4/InbetrDDS):

| Command communication | Position actual-value output | Motor feedback version | System configuration |
|---|---|---|---|
| ANALOG interface with position actual-value output | Incremental | MDD with resolver feedback | **RA 01** |
| " | Incremental | MDD with digital servo feedback | **DA 01** |
| " | Absolute | MDD with resolver feedback | **RA 02** |
| " | Absolute | MDD with digital servo feedback | **DA 02** |

System configurations shown in Figs. 11-14; the configuration and plug-in module types are given on the configuration rating plate (Figs. 9, 10).

## PDF page 19: Configuration rating plate and type code

Fig. 9 "Rating place [sic] of the configured drive controller" (Abb 8.1/InbetrDDS), example plate "SYSTEM CONFIGURATION":

| Line | Text |
|---|---|
| Configuration | DDS 2.1-W050-DA01-00 |
| Basic unit | DDS 2.1-W050-D |
| Software module | DSM 2.1-S11-01.RS (as printed; S11 is the SERCOS software code, for DA01 one would expect E11, see p. 20) |
| Slot U1 | DAE 1.1 |
| U2 | COVER |
| U3 | COVER |
| U4 | COVER |

Fig. 10 "Type code explaining a configured drive controller", example **DDS 2.1-W050-DA01-00**:

| Pos. | Meaning | Codes |
|---|---|---|
| 1 | Designation | DDS = Digital drive controller |
| 2 | Series | 2 |
| 3 | Version | 1 |
| 4 | Cooling method | F = liquid cooled; K = refrigeration technique (outside cabinet); W = heat dissipation technique (inside cabinet) |
| 5 | Current ratings | 015 = 15 A; 025 = 25 A; 050 = 50 A; 100 = 100 A; 150 = 150 A |
| 6 | Feedback ("MDD" motor) | D = digital servo feedback; R = resolver feedback |
| 7 | Command signal communication | A = ANALOG interface; S = SERCOS interface |
| 8 | Function code (determined and documented by INDRAMAT) | e.g. 01 |
| 9 | Version number of the function code | e.g. 00 |

## PDF page 20: System configurations RA 01 and DA 01

**RA 01** (Fig. 11 "ANALOG interface with incremental position actual-value output for motor series with resolver feedback", ENA3A-Abb5/InbetrDDS). NC control <- "Analog velocity command" / "Incremental position actual value" -> configured drive controller -> cable -> servo motor.

| No. | Designation | Associated type code |
|---|---|---|
| 1 | Drive controller / basic unit | DDS 2.1-. . . . -R |
| 2 | Configuration rating plate (sticker) | TYS-DDS 2.1 - . . . . - RA01 - 00 |
| 3 | Software module | DSM 2.1-E11-01.RS |
| 4 | ANALOG interface with incremental encoder emulator | DAE 1.1 |
| 5 | Digital AC servo motor with resolver feedback | MDD .... - . - ... - .. G - ...../.... |

**DA 01** (Fig. 11 [sic, numbering repeated] "ANALOG interface with incremental position actual-value output for motor series with digital servo feedback", ENA3A-Abb6/InbetrDDS):

| No. | Designation | Associated type code |
|---|---|---|
| 1 | Drive controller / basic unit | DDS 2.1-. . . . -D |
| 2 | Configuration rating plate (sticker) | TYS-DDS 2.1 - . . . . - DA01 - 00 |
| 3 | Software module | DSM 2.1-E11-01.RS |
| 4 | ANALOG interface with incremental encoder emulator | DAE 1.1 |
| 5 | Digital AC servo motor MDD 065, 071, 090, 093, 095, 112, 115, 117 | MDD .... - . - ... - .. L - ...../.... |

## PDF page 21: System configurations RA 02 and DA 02

**RA 02** (caption printed as "Fig. 11: ANALOG interface with incremental position actual-value output for motor series with resolver feedback", evidently a misprint for "absolute"; ENA3A-Abb7/InbetrDDS). Arrows: "Analog velocity command" / "Absolute position actual value".

| No. | Designation | Associated type code |
|---|---|---|
| 1 | Drive controller / basic unit | DDS 2.1-. . . . -R |
| 2 | Configuration rating plate (sticker) | TYS-DDS 2.1 - . . . . - RA02 - 00 |
| 3 | Software module | DSM 2.1-A11-01.RS |
| 4 | ANALOG interface with absolute encoder emulator | DAA 1.1 |
| 5 | Digital AC servo motor with resolver feedback | MDD .... - . - ... - .. K - ...../.... |

**DA 02** (Fig. 11 "ANALOG interface with absolute position actual-value output for motor series with digital servo feedback", ENA3A-Abb8/InbetrDDS):

| No. | Designation | Associated type code |
|---|---|---|
| 1 | Drive controller / basic unit | DDS 2.1-. . . . -D |
| 2 | Configuration rating plate (sticker) | TYS-DDS 2.1 - . . . . - DA02 - 00 |
| 3 | Software module | DSM 2.1-A11-01.RS |
| 4 | ANALOG interface with absolute encoder emulator | DAA 1.1 |
| 5 | Digital AC servo motor MDD 065, 071, 090, 093, 095, 112, 115, 117 | MDD .... - . - ... - .. M - ...../.... |

## PDF page 22: 2.4 System configuration components - drive controller basic unit

A system configuration = "configured drive controller" + "MDD servo motor". Fig. 15 "Designations of DDS2.1 basic unit features" (ENA3A-Abb9/InbetrDDS):

| Designation | Function |
|---|---|
| Earthing screw (top) | Earthing connection to power supply module |
| Earthing screw | Earthing connection to motor |
| **X5** A1, A2, A3 | Motor power connection |
| **X5** L+, L- | DC link circuit connection |
| **X6** | Connection for: motor temperature monitor; motor holding brake; 24V external |
| **S1** | Fault reset button |
| **U5** | Slot for software module |
| **H1** | Status display, alarm and fault messages (2-digit 7-segment display) |
| **X1** | Control voltage bus connection |
| **X2** | RS-232 interface for VT 100 terminal or PC |
| **X3** | Analog I/Os (11-pin terminal, see p. 35) |
| **X4** | Motor feedback connection (D-sub) |
| **U1** | Slot for command communication module |
| **U2, U3, U4** | Slots for additional plug-in modules |
| Rating plate | on the front, "DDS 2.1-W..-D" |

## PDF page 23: Rating plate and type code, basic unit

Fig. 16 "Rating plate of a drive controller, basic unit" (example):

| Field | Example |
|---|---|
| Type designation | DDS 2.1-W025-D |
| Article No. | 247203 |
| Shipment date | K42/92 |
| Bar code | (bar code) |
| Serial number | SN247203-01073 |
| Status as-delivered | A04 |

The rating plate is located on the front panel of the basic unit (Fig. 15).

Fig. 17 "Type code explaining the drive controller basic unit", example **DDS 2.1-W050-D**: positions 1-6 as in Fig. 10 (DDS / series 2 / version 1 / cooling F, K, W / current 015, 025, 050, 100, 150 A / feedback D = digital servo feedback, R = resolver feedback).

## PDF page 24: Command communication module with position actual-value output

Fig. 18 "Designations of ANALOG interface module features" (ENA3A-Abb10/InbetrDDS):

**DAE 1.1** - ANALOG interface with incremental encoder emulator:
- Type code label at top; offset balancing potentiometer ("OFFSET NOM." [uncertain]).
- **Terminal block X13**: analog input (E1, E2); overtemperature warning (TVW); controller enable (RF); drive halt (AH); current reduction (Ired1, Ired2); external +24V operating voltage (+UL, 0VL); shield.
- Terminal order on the front, top to bottom: E1, E2, TVW, RF, AH, Ired1, Ired2, +UL, 0VL, shield (10 positions).
- **X14** (15-pin D-sub): incremental position actual-value output.

**DAA 1.1** - ANALOG interface with absolute encoder emulator:
- Same terminal block, here **X15**: E1, E2, TVW, RF, AH, Ired1, Ired2, +UL, 0VL, shield.
- **X16** (15-pin D-sub): absolute position actual-value output.

Text: "The ANALOG interface of the DDS 2.1 drive controller serves to communicate with an NC control equipped with a ±10V analog interface." Two versions: incremental encoder emulator / absolute encoder emulator.

- Incremental encoder emulation: outputs internal drive information on position or angle to the NC as actual-value input. **Output signals: incremental encoder compatible square-wave signals (voltage signals).**
- Absolute encoder emulation: outputs position via the **SSI (synchronous serial interface)** usual in absolute encoders.

## PDF page 25: Type codes of modules

Fig. 19 "Type code explaining the command communication module", example DSS 1.1:

| Pos. | Meaning | Code |
|---|---|---|
| 1 | Digital ANALOG interface with absolute encoder emulator | DAA |
| 1 | Digital ANALOG interface with incremental encoder emulator | DAE |
| 1 | Digital SERCOS interface | DSS |
| 2 | Series | 1 |
| 3 | Version | 1 |

Fig. 20 "Type code explaining the additional plug-in modules" ("as provided at present for the SERCOS interface"), example DLF 1.1:

| Code | Meaning |
|---|---|
| DEF | Digital incremental position interface (square-wave signals) |
| DLF | Digital high-resolution position interface (sinusoidal signals) |
| DZF | Digital gear encoder interface |
| Series / Version | 1 / 1 |

## PDF page 26: Software module

Fig. 21 "Designations of software module features" (ENA3A-Abb12/InbetrDDS): software version, bar code, serial number on top label; side label "DSM 2.1-E11-01RS" (type code of the software module); abbreviated type code "DSM 2.1".

- Matching of controller to motor and of the drive to the machine mechanics is done by parameters stored in the software module.
- Unit replacement: the software module stores operating software and parameters; plugging the old module into the new controller transfers the matching automatically.
- Duplication via the serial interface for identical machines or backup.
- Standard software module: drive-related parameter values calculated by INDRAMAT are **stored in the motor feedback** and activated on request during commissioning; application-related parameters are set on site.
- Note: "The documentation and management of application-related parameters is the responsibility of the customer."
- Compatibility: updates supplied without change of type designation; compatible with modules in the field.

## PDF page 27: Software module rating plate and type code

Fig. 22 rating plate: **DSM 2.1-E11-01.RS**

Fig. 23 type code, example DSM 2.1-S11-01.RS:

| Pos. | Meaning | Code |
|---|---|---|
| 1 | Designation | DSM |
| 2 | Series | 2 |
| 3 | Version | 1 |
| 4 | Software type | A11 = Analog absolute encoder (DAA1.1); **E11 = Analog incremental encoder (DAE1.1)**; S11 = SERCOS (DSS1.1); SZU = SERCOS (twin axis alternation) |
| 5 | Software version | 01 |
| 6 | Software release (update; latest update supplied automatically) | RS |

## PDF page 28: Digital AC servo motors - feedback versions

MDD motors are supplied with: digital servo feedback (DSF); DSF + multi-turn encoder (DSF + MTG); resolver feedback (RSF); RSF + multi-turn encoder (RSF + MTG).

Fig. 24 "Properties of available feedback versions":

| Property | Digital servo feedback | Resolver feedback |
|---|---|---|
| Sensor principle | Optical scanning of a code disk | Three-phase transformer with angle-dependent coupling ratio |
| Maximum position resolution | 256 x 2^13 = 2 097 152 increments/revolution | 3 x 2^13 = 24 576 increments/revolution |
| System accuracy | ± 0.5 angular minutes | ± 7 angular minutes |
| Multi-turn version, sensing range | 4096 rotor revolutions | 4096 rotor revolutions |
| Available on motor series | MDD 065 to MDD 117 | MDD 021 to MDD 117 |
| Suitable for applications with | High demands on dynamic control response, synchronism, absolute accuracy | Low demands on synchronism, absolute accuracy; extreme impact and vibration stresses |
| Range of applications | Servo applications in machine tool axes, robots, handling, assembly, wood-working, packaging, textile, printing machines | Handling applications, feed axes, low-cost applications etc. |

## PDF page 29: Type code MDD servo motors

Fig. 25, example **MDD 112B-N-015-N2L-130GB0/S000**:

| Pos. | Meaning | Codes |
|---|---|---|
| 1 | Designation | MDD |
| 2 | Motor frame size | 112 |
| 3 | Motor overall length | B |
| 4 | Frame design | N |
| 5 | Rated speed | 015 = 1500 rpm |
| 6 | Vibration severity | N |
| 7 | Motor version | 2 |
| 8 | Feedback | G = RSF; K = RSF + IDG [as printed; MTG meant, uncertain]; L = DSF; M = DSF + MTG |
| 9 | Spigot diameter | 130 |
| 10 | Drive shaft | G |
| 11 | Installation position of power plug | B |
| 12 | Holding brake | 0 = without; 1 = 14.0 Nm; 2 = 40.0 Nm; 3 = 60.0 Nm |
| 13 | Special version | S000 |

(Assignment of positions 9-11 to the characters follows the leader lines [uncertain].) "For further details please refer to the relevant motor documentation."

## PDF page 30: MDD rating plate

Fig. 26 legend: (1) INDRAMAT logo; (2) Build date (week and year); (3) motor type; (4) S.No. serial number; (5) Com.No. job number; (6) Part No.; (7) bar code; (8) natural convection cooling; (9) MdN continuous zero-speed torque natural convection [Nm]; (10) IdN continuous zero-speed current natural convection [A]; (11) surface cooled or liquid cooled; (12) continuous zero-speed torque surface/liquid cooled [Nm]; (13) continuous zero-speed current surface/liquid cooled [A]; (14) I.Cl. insulation class; (15) protection class; (16) n rated speed [rpm]; (17) Km torque constant [Nm/A]; (18) m mass [kg]; (19) Brake: holding torque [Nm], rated voltage (24 V ±10 %), rated current [A]; (20)/(21) internal index numbers (side text "INN 09.06").

Example completed plate:

| Field | Value |
|---|---|
| Type | MDD090C-N-020-N2L-110GB0/S016 |
| Build Date | 44/92 |
| S.No. / Com.No. / Part No. | 123456 / 234567 / 345678 |
| Tcont natural convection / surface cooled | 10.4 Nm / 16.0 Nm |
| IdN natural / surface cooled | 9.5 A / 14.6 A |
| I.Cl. / IP | F / IP 65 |
| n | 2000 min^-1 |
| Km | 1.10 Nm/A |
| m | 50 kg |
| Brake | 14.0 Nm, 24 V ±10 %, 0.75 A |

## PDF page 31: 3. Fundamental safety notes

3.1 Fig. 27 symbols: warning triangle = "Danger: Instructions or DOs and DON'Ts for the prevention of accidents and damage to equipment"; pointing hand = "Note: special notes or DOs and DON'Ts relating to accident prevention".

3.2 Use as prescribed: machine in flawless condition; work only by trained personnel / trained electrician; instructions known; "**the drives are operated exclusively in secondary position control loops**".

## PDF page 32: 3.3 / 3.4

3.3 Risks during commissioning: wiring errors (motor, drive controller, feedback), errors in the NC control, monitoring equipment out of action. "To reduce the accident risk commissioning must always be carried out by trained and qualified personnel only."

3.4 Protection of personnel: drives must be part of the global safety concept. Unintentional start-up prevented by: **disconnecting the power contactor (E-STOP)**; disconnecting and locking the master switch; for "starting lockout (drive interlock open)" see Section 7.2.

Dangerous voltages:
- Drive controller **X5**: **L+, L-** DC link voltage (after switch-off wait approx. 5 min until below 50 V, if in doubt short-circuit); **A1, A2, A3** motor voltage (dangerous also when the motor runs out after power-off).
- Power supply module: **L1, L2, L3** mains connection.

## PDF page 33

Fig. 28 "Power supply module and DDS 2.1 drive controller with screwed-down transparent protective plates" (ENA3A-Abb15/InbetrDDS).

Warning: "Accident risk through lethal voltage levels! Never operate drive controllers without the transparent protective plate in place. All connections on terminal block X5 carry lethal voltage levels when power is switched on. The same applies to the power and DC link circuit terminals of the power supply module."

Note: "Earth linkage circuit-breaker systems [earth-leakage / RCD] cannot be used on INDRAMAT equipment." Protection against indirect contact by other means, e.g. overcurrent protective devices with multiple earthing.

## PDF page 34: 3.5 / 3.6

3.5 Equipment: inputs only with prescribed voltages; do not connect outputs to external sources; do not connect mains, DC link or motor conductors to the low-voltage **±15 V and +24 V** supplies; components 100 % h.v. tested to VDE 0160; disconnect all components before an h.v. test of the machine (VDE 0113); ESD precautions; software modules stored/shipped in conductive packaging.

3.6 Machinery: if the NC position loop is opened during commissioning and the drives run in velocity loop, the machine is at risk (limited stroke). Enabling drives and injecting a velocity command only by qualified personnel; make sure the emergency stop is triggered by limit switch or E-STOP button.

## PDF page 35: 4. Commissioning equipment / 4.1 Measuring instruments

Needed: measuring instruments; battery power source; service cable IN 391; PC with VT 100 emulation or a VT 100 terminal; Selection Data Doc. No. 209-0069-4302. Instruments: multimeter; oscilloscope or plotter (for prototype test records and fault locating).

Fig. 29 "Connection of voltage measuring instruments to the analog diagnostics outputs on the drive controller" (ENA3A-Abb17/InbetrDDS). **X3 pinout** (basic unit, 11-pin screw terminal):

| X3 pin | Signal | Note |
|---|---|---|
| 1 | AK1 | analog diagnostics output 1 (voltmeter V between 1 and 2) |
| 2 | 0V_M | measurement 0 V |
| 3 | AK2 | analog diagnostics output 2 (voltmeter V between 3 and 4) |
| 4 | 0V_M | measurement 0 V |
| 5 | earth / shield symbol | |
| 6 | Bb | ready-to-operate contact ("Betriebsbereit") [meaning from Indramat convention, not printed here] |
| 7 | Bb | ready contact, second terminal |
| 8 | AS+ | starting lockout input + [meaning not printed on this page; see section 7.2, uncertain] |
| 9 | AS- | starting lockout input - [uncertain] |
| 10 | ASQ | starting lockout acknowledge contact [uncertain] |
| 11 | ASQ | acknowledge contact, second terminal [uncertain] |

## PDF page 36: 4.2 Battery power source

Text: drives are for secondary control loops; with ANALOG interface the command is an analog voltage from an NC control. Warning: operating the drive in anything other than a secondary control loop risks uncontrolled motions; special precautions in Section 6.6. Without NC the drive can be run in velocity loop mode with a battery power source as in Fig. 30.

Fig. 30 "Proposed circuitry for connecting up a battery power source to the DDS 2.1 with ANALOG interface" (ENA3A-Abb18/InbetrDDS). Drive shown with a DAA module (terminal block **X15**, D-sub **X16**) in U1 and basic unit **X4**.

Legend:
- G: battery power source 10 ... 15 V
- V: integral DC voltmeter for velocity command (V_com), labelled "V set-point"
- R1: potentiometer for settable velocity command, 10k
- R2: protective resistor, 1k0
- S1: selector switch for velocity command (V_com) (two-pole changeover, reverses battery polarity)
- S2: controller enable switch (RF)
- S3: starting lockout (drive interlock open) switch (AH)
- S4: Ired switch ("binär Cod." = binary coded)
- H1: starting lockout (drive interlock open) indicator (AH)
- H2: controller enable indicator (RF)
- H3: overtemperature warning indicator (TVW)
- External power supply: +24V ±10 %, 0V

Connections (box terminals -> X15):

| From | To | Signal |
|---|---|---|
| G via S1 (polarity selector) -> R2 1k0 -> R1 10k (wiper) | X15 E1 | velocity command + |
| G via S1 -> lower end of R1 | X15 E2 | velocity command reference |
| Voltmeter V | between E1 and E2 | V set-point |
| X15 TVW | H3 -> 0VI | overtemperature warning lamp |
| +UI (external +24 V) via S3 | X15 AH | drive halt / start enable; H1 from AH line to 0 V |
| +UI via S2 | X15 RF | controller enable; H2 on RF line to 0 V [uncertain routing] |
| +UI via S4 (binary coded) | X15 Ired1, Ired2 | current/torque reduction |
| External supply +24 V ±10 % | X15 +UI | (printed "+UI" here, "+UL" on p. 24) |
| External supply 0 V | X15 0VI | (printed "0VI", "0VL" on p. 24) |

So the control inputs RF, AH, Ired1/2 are **+24 V active inputs** referenced to the module's +UL/0VL supply, and TVW is a +24 V output able to drive a lamp.

## PDF page 37: 4.3 Service cable IN 391 / 4.4 PC

Switch off power before connecting. INDRAMAT pre-assembled cables type **IN 391/2** (lengths 2, 5, 10, 15 m; the suffix is the length).

Fig. 31 "Connecting up a personal computer via RS 232 interface to a DDS 2.1 with ANALOG interface" (ENA3A-Abb19/InbetrDDS):

| X2 (drive) pin | Signal | -> | PC 9-way D-sub socket pin | Signal |
|---|---|---|---|---|
| 2 | TxD | crossed | 2 | RxD |
| 3 | RxD | crossed | 3 | TxD |
| 4 | RTS | bridged to 5 on drive side | - | - |
| 5 | CTS | bridged to 4 | - | - |
| 7 | GND | | 5 | GND |
| Frame | shield | | | |
| - | | PC side bridges | 7-8 | RTS-CTS |
| - | | PC side bridges | 4-6 | DTR-DSR |

PC requirements: IBM compatible, MS-DOS, RS 232, floppy or hard disk, VT 100 terminal emulation software (available from INDRAMAT).

## PDF page 38: 4.5 VT 100 Terminal

VT 100 is the minimum hardware for the user interface stored in the DDS 2.1; it cannot store data.

| Parameter | Setting |
|---|---|
| Terminal type | VT 100 |
| Columns | 80 |
| Lines | 24 |
| Line feed | off (only CR after ENTER) |
| Automatic line wrap | off |
| Backspace key | active |
| Control characters | invisible |
| Parity | data bits 8 (as printed, i.e. 8 data bits, no parity) |
| Stop bit | 1 |
| Transmission rate | **9600 Baud** (no other rate possible) |

## PDF page 39: 5. Operating the drive controller / 5.1

Pre-conditions: DDS 2.1 installed and wired (Section 6); motor power and feedback cables connected; PC or VT 100 connected via IN 391; emulation program running.

Switch on power at the power supply module; the drive responds: **"To start Parametrization and Diagnostic Program, press ENTER"**.

MAIN MENU: 1 "DRIVE STATUS"; 2 "PARAMETERS"; 3 "LANGUAGE SELECTION". ESC returns one level / exits.

## PDF page 40: Drive status / parameters / language

DRIVE STATUS shows signal states of the ANALOG interface module:

| Signal | Meaning |
|---|---|
| TVW | Overtemperature warning |
| RF | Controller enable |
| AH | Drive enable (here; called "drive halt" on p. 24) |
| Ired1 | Torque reduction |
| Ired2 | Torque reduction |
| Nullimp. | Zero pulse (marker pulse) |

plus: type designations of motor/controller, current operating mode, velocity, current and motor torque values, rotor position.

PARAMETERS: 8 submenus (keys 1 to 8). LANGUAGE SELECTION / "SPRACHAUSWAHL" (German: language selection): English or German. (Page ends; following illustrations are in part b.)

---

## For the retrofit

What this part tells us (generic Indramat manual; the machine-specific type plates, X3/X13 wiring and Gildemeister cabinet tags must be taken from the other chapters / part b and from the real drives):

**Drive family.** Indramat **DDS 2.1** digital drive controllers ("DDS 2.1-W0xx-D/R", heat dissipation inside cabinet = W, 15/25/50/100/150 A), on a common Indramat **power supply module** (3 x 380-460 V, 50-60 Hz, DC link L+/L- via busbars, control voltage bus X1). The feed motors **MDD 093** are available with digital servo feedback (DSF, type letter L) or resolver (RSF, G); the actual feedback type is in the motor type code (8th position) and the "-D"/"-R" suffix of the DDS. Check the configuration plate on each drive's cover (e.g. "DDS 2.1-W050-DA01-00").

**Command interface.** With the ELTROPILOT (VAP2/VAP4 axis modules: analog outputs + "Generator" encoder inputs, see 07_hardware) the drives are almost certainly the **ANALOG** version: plug-in module **DAE 1.1** in slot U1 (system configuration **DA01**/RA01, software module **DSM 2.1-E11-01.RS**). Command = **±10 V analog velocity command** on DAE **X13 E1/E2** (differential input, offset pot on the module). Torque mode only for master/slave. This makes keeping the drives under LinuxCNC straightforward: a standard ±10 V velocity-mode servo axis (e.g. Mesa 7i77, 7i33/7i83 or similar), position loop in LinuxCNC (PID with feed-forward), exactly as the EP90 did ("drives are operated exclusively in secondary position control loops").

**Position feedback to the control.** DAE 1.1 **X14** (15-pin D-sub) outputs an **emulated incremental encoder**: square-wave, voltage signals (A, B, marker "Nullimp."), resolution set by drive parameter (section 7.8, part b). Line count and signal level (likely RS-422 differential 5 V, to be confirmed with the pin table in section 11.6) must be read from the drive parameters / X14 pinout; then wire to an encoder input of the new controller. If a DAA 1.1 is fitted instead, X16 outputs **SSI** absolute position (needs an SSI-capable input, e.g. Mesa SSI firmware).

**I/O signals (DAE X13 / DAA X15, +24 V logic, supplied via +UL/0VL):**
- **RF** controller enable (input) - drive from a LinuxCNC enable output (via relay/24 V output), part of the E-stop chain.
- **AH** drive halt / drive enable (input; "Antrieb Halt") - when removed the drive brakes to standstill with velocity command ignored [details in 7.x, part b].
- **Ired1/Ired2** torque reduction (binary coded inputs), probably unused or used for fixed-stop functions.
- **TVW** overtemperature warning (output) - feed to LinuxCNC input / PLC.
- **Bb** (X3 pins 6-7) ready contact (meaning per Indramat convention, confirm in part b) - must be in series with the power supply module ready / E-stop chain and read by LinuxCNC as "drive OK".
- **AS+/AS-, ASQ** (X3 8-11) starting lockout (safe-stop relay) and its acknowledge contact, if used by Gildemeister.
- **X6**: motor temperature, **holding brake** (24 V, e.g. 0.75 A on MDD090) and external 24 V; brake is controlled by the drive itself (section 7.3), so no LinuxCNC brake output is needed unless Gildemeister wired it differently.
- **AK1/AK2** (X3 1-4) analog diagnostics outputs - useful for tuning with a scope.
- **S1** fault reset button and **H1** 2-digit status display on the front; RS-232 **X2** service port at 9600 8N1 VT100 (cable IN 391: X2 pins 2/3 crossed, GND 7) - back up every drive's parameters (software module DSM) before any work.

**E-stop chain.** The manual relies on the **power contactor (E-STOP)** in the supply to the power supply module, plus RF/AH removal; earth-leakage breakers must not be used. Keep the Gildemeister contactor / power supply ready logic and let LinuxCNC drop RF/AH and the contactor on E-stop.

**Replacing the drives.** If the DDS 2.1 are replaced: the MDD motors are permanent-magnet synchronous motors; new drives must handle their feedback (Indramat DSF high-resolution optical encoder with 2^21 counts/rev and parameter memory in the feedback, or 3-phase resolver for RSF) - this is the main obstacle; usually the motor feedback has to be swapped for a standard encoder/resolver or Indramat-compatible drives (e.g. later DKC/ECODRIVE with DSF support) used. Also needed: brake 24 V control, motor thermal sensor, DC-link/regen equivalent of the Indramat supply module, and 3 x 400 V mains. The LinuxCNC side stays the same in either case if the new drives take ±10 V velocity command and give incremental encoder feedback.
