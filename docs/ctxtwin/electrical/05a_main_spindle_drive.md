# Gildemeister CTX twin, electrical documentation: 5 Main spindle drive (part a), PDF pages 1-28

Source: `57c1082c-5_Main_spinde_drive_a.pdf` (28 scanned pages). This part is **not a Gildemeister drawing set**: it is the beginning of the Indramat manual
"A.C. Main spindle drives with controlled asynchronous motors - Applications", **Doc. No. 209-0041-4109-00, IEN/12.91** (date of issue 12.91),
filed by Gildemeister in chapter 5 of the machine documentation. Original language: **English** (the "IEN" in the document number means the
English edition). The only German texts are the figure file names in the corners of the figures ("Hauptspindel-KDA/TDA" etc.).
The PDF covers the cover page, the documentation overview, the table of contents and chapter 1 "Commissioning" (manual pages 1-26).
The manual covers the Indramat KDA 3.2, TDA 1.1, RAC 2.2 and RAC 3.1 main spindle controllers. It does **not** say which of these the CTX twin
uses. That has to come from the rating plates, from part b or from the Gildemeister wiring sheets.

## Page table

There are no drawing or sheet numbers. The manual page number is printed at the bottom of each page (PDF page = manual page + 2).

| PDF page | Manual page | Content |
|---|---|---|
| 1 | cover | INDRAMAT logo, "A.C. Main spindle drives with controlled asynchronous motors, Applications", 209-0041-4109-00 IEN/12.91 |
| 2 | (back of cover) | blank (the cover shows through) |
| 3 | 1 | Short contents: "Intelligent, maintenance-free AC main drives - applications" |
| 4 | 2 | About this documentation: title, doc no., documentation overview |
| 5 | 3 | Current documents (KDA 3.2 ... 1MB rotor) |
| 6 | 4 | Current documents, continued (IGS option, HGV 1, high-res. encoder, selection lists) |
| 7 | 5 | Contents: chapters 1-3.4 |
| 8 | 6 | Contents: 3.5-3.21 |
| 9 | 7 | Contents: chapters 4-7 |
| 10 | 8 | blank (the contents page shows through) |
| 11 | 9 | Chapter 1 title page, Commissioning |
| 12 | 10 | 1.1 Overview, Fig. 1: KDA/TDA modular system |
| 13 | 11 | Fig. 2: RAC 2.2 complete controller |
| 14 | 12 | Fig. 3: RAC 3.1 complete controller |
| 15 | 13 | 1.2 Safety notes, 1.2.1 personnel |
| 16 | 14 | Note on earth-leakage circuit breakers (e.l.c.b.); 1.2.2 equipment protection |
| 17 | 15 | 1.2.3 Machine protection |
| 18 | 16 | 1.3 Equipment required, Fig. 4: set-point box wiring to X4/X2 |
| 19 | 17 | 1.4 KDA/TDA: 1.4.1 mains supply, 1.4.2 checks with the power off a)-b) |
| 20 | 18 | 1.4.2 c)-h), 1.4.3 checks with the signal conditioning powered up (intro) |
| 21 | 19 | 1.4.3 a)-e): LEDs, fan, power on/off sequence, DC link, motor feedback |
| 22 | 20 | 1.4.3 f)-h): set-point, spindle encoder, motor temperature; 1.4.4 a) |
| 23 | 21 | 1.4.4 b) DC link voltage 255-345 V; 1.4.5 initial start-up |
| 24 | 22 | 1.5 RAC 2.2 / RAC 3.1: 1.5.1 mains supply requirements |
| 25 | 23 | 1.5.2 checks with the power off a)-d) |
| 26 | 24 | 1.5.2 e)-g); 1.5.3 checks with the mains connected but the power circuits disconnected, a)-c) |
| 27 | 25 | 1.5.3 d)-f); 1.5.4 Connecting the power infeed (X15) |
| 28 | 26 | Note: E-STOP X2/23; 1.5.4 a)-b) (DC link 480-550 V); 1.5.5 initial start-up |

---

## PDF page 1 - cover

- Logo: INDRAMAT
- "A.C. Main spindle drives with controlled asynchronous motors"
- "Applications"
- Empty frame (for a label)
- "209-0041-4109-00 IEN/12.91"

## PDF page 2 - blank

Back of the cover. Only the cover printing shows through. No content.

## PDF page 3 (manual p. 1) - Contents

Heading: "Intelligent, maintenance-free AC main drives - applications"

| Chapter | Page |
|---|---|
| Commissioning | 9 |
| Operating the controllers | 27 |
| Controller functions | 43 |
| Diagnostics and fault-finding | 117 |
| Summary of parameters | 129 |
| Overview of interface and associated functions | 141 |
| Abbreviations and agreements, type codes | 153 |

## PDF page 4 (manual p. 2) - About this documentation

- Title: "AC main spindle drives with controlled asynchronous motors, Applications"
- Doc. No.: 209-0041-4109-00
- Date of issue: 12.91
- "Documentation overview - a.c. main spindle drives:"

| Document | Content |
|---|---|
| Selection data, a.c. main spindle drives, No. 9.567.013.4-.. | (Brief description of the HA system); physical power steps; (functional power levels); ordering documents. "File 7, register CD" |
| Boxes **KDA, TDA, RAC 2, RAC 3**: project planning documents (design, installation, assembly) | Mechanical data; thermal data; electrical data for RG; technical data; available versions |
| Boxes **2 AD, 1 MB**: project planning documents (design, installation, assembly) | Technical data: motor; technical data: holding brake; technical data: fans; dimensional data sheets; graphs ("Graps" as printed): radial loading; electrical connections; available versions |
| Applications information, a.c. main spindle drives, No. 209-0041-4109-00 | Commissioning; operating the controllers; functions of the main spindle drives; diagnostics and fault-finding; appendix with parameter and interface overview, list of keywords, electrical connections, current documents |

(HA = Hauptantrieb, main drive; RG = Regelgerät, controller. These expansions are an interpretation and are not printed.)

## PDF page 5 (manual p. 3) - Current documents

| Item | Document | No. |
|---|---|---|
| KDA 3.2 | "AC main spindle drives with KDA 3.2 - applications" | ID 73 311 |
| TDA 1.1 | "TDA modular main spindle controller - applications" | ID 9.559/041.4-00 |
| RAC 2.2 | "AC main spindle drives with RAC 2.2 compact controller - project planning notes" | 9.558.084.4-01 |
| RAC 3.1 | "AC main spindle drives with RAC 3.1 compact controller - project planning notes" | 9.558.085.4-02 |
| 2AD 100 | "2AD 100 asynchronous motor - project planning" | 9.567.009.4-01 |
| 2AD 101 | "2AD 101 asynchronous motor - project planning" | 9.567.018.4-01 |
| 2AD 132 | "2AD 132 asynchronous motor - project planning" | 9.567.010.4-01 |
| 2AD 160 | "2AD 160 asynchronous motor - project planning" | 9.567.011.4-01 |
| 2AD 180 | "2AD 180 asynchronous motor - project planning" | 9.567.012.4-01 |
| 2AD + gearbox | "2AD main spindle drives with selectable planet gears - project planning" | 9.567.022.4-00 |
| 1MB stator | "1MB frameless motors, stator installation and cooling guidelines - installation instructions" | 9.600.063.4-00 |
| 1MB rotor | "1MB frameless motors, rotor installation and dismantling guidelines - installation instructions" | 9.600.062.4-00 |

## PDF page 6 (manual p. 4) - Current documents (continued)

| Item | Document | No. |
|---|---|---|
| IGS option | "Incremental encoder output on main spindle controllers KDA 3.2, RAC 2.2, RAC 3.1 - applications" | 9.568.015.4-02 |
| HGV 1 | "Coupler for high-resolution encoders - applications" | 9.568.010.4-00 |
| Indramat high-res. encoder | "High-resolution main spindle position encoder for KDA and RAC main spindle controllers - applications" | 9.552.247.4-01 |
| Selection lists, operating characteristics | "Main spindle drives with 2AD motors - project planning" | 9.567.013.4-01 |

(1MB = Indramat frameless built-in motor, i.e. the motor type that fits an "integrated spindle motor". Interpretation, not printed.)

## PDF page 7 (manual p. 5) - Contents

1. Commissioning Indramat a.c. main spindle drives ... 9
   - 1.1 Overview of main spindle drive systems ... 10
   - 1.2 Safety notes ... 13
   - 1.3 Equipment required ... 16
   - 1.4 Main spindle drive with KDA or TDA ... 17
   - 1.5 Main spindle drives with complete controllers RAC 2.2 and RAC 3.1 ... 22
2. Operating the controllers ... 27
   - 2.1 The control panel ... 29
   - 2.2 Operating mode ... 30
   - 2.2.1 Displaying the drive's internal state variables ... 31
   - 2.2.2 Outputting the drive's internal state variables via the analogue output N ... 32
   - 2.3 Parameter mode ... 34
   - 2.3.1 Quick test of parameter values ... 35
   - 2.3.2 Reading the parameters ... 36
   - 2.3.3 Changing the parameters ... 37
   - 2.3.4 Displaying the software version ... 39
   - 2.3.5 Duplicating the parameters ... 40
   - 2.3.6 Loading the operating parameters via the serial interface ... 41
3. Controller functions ... 43
   - 3.1 Drive "ready" state ... 45
   - 3.2 Speed set-point ... 46
   - 3.2.1 Analogue speed set-point (standard) ... 46
   - 3.2.2 Digital speed set-point (optional) ... 49
   - 3.3 Main spindle speed output signals ... 53
   - 3.4 Speed set-point ramps ... 55

## PDF page 8 (manual p. 6) - Contents (continued)

- 3.5 Speed set-point smoothing ... 59
- 3.6 Spindle positioning ... 61
- 3.6.1 Basic data for spindle positioning ... 63
- 3.6.2 Spindle positioning via motor feedback ... 66
- 3.6.3 Spindle positioning via motor feedback with spindle reference switch ... 68
- 3.6.4 Spindle positioning via spindle feedback ... 71
- 3.7 Speed controller functions ... 73
- 3.8 C-axis function ... 76
- 3.9 Power and torque limiting ... 79
- 3.10 Drive utilization output ... 82
- 3.10.1 Analogue output for drive utilization (analogue output M) ... 82
- 3.10.2 Signal output with adjustable threshold for drive utilization (LOAD LIMIT) ... 86
- 3.11 Temperature pre-warning ... 87
- 3.12 Maximum spindle speeds ... 89
- 3.13 Spindle monitoring ... 91
- 3.14 Switchable parameter records ... 92
- 3.15 Gear change ... 95
- 3.15.1 Automatic gear change via the controller ... 95
- 3.15.2 Gear change via the external controller ... 99
- 3.16 Two-motor changeover ... 101
- 3.17 Master-slave operation ... 105
- 3.18 EMERGENCY-STOP circuit (RAC) ... 110
- 3.19 Performance during mains failure (RAC) ... 112
- 3.20 Performance during mains failure and EMERGENCY-STOP (KDA, TDA) ... 115
- 3.21 Starting lockout in KDA/TDA ... 115

## PDF page 9 (manual p. 7) - Contents (continued)

4. Diagnostics and fault clearance ... 117
   - 4.1 Operating status display ... 118
   - 4.2 Fault diagnostics ... 121
5. Overview of parameters ... 129
   - 5.1 General parameters (A parameters) ... 130
   - 5.2 Switchable parameter records (P, Q, R, S parameters) ... 133
   - 5.3 Motor parameters (M parameters) ... 135
   - 5.4 Parameter protocol and list of additions ... 138
6. Interface overview ... 141
   - 6.1 Control inputs, signal outputs and analogue outputs, Bb contact ... 142
   - 6.2 Analogue speed set-point (standard) ... 144
   - 6.3 Digital speed set-point input (optional) ... 145
   - 6.4 SERCOS interface (optional) ... 146
   - 6.5 Digital position input (optional) ... 147
   - 6.6 Incremental encoder output (optional) ... 148
   - 6.7 Serial interface (optional) ... 149
   - 6.8 Motor feedback connection ... 150
   - 6.9 Spindle feedback connection (optional) ... 151
7. Abbreviations and agreements, type codes ... 153

Note: the PDF ends at manual page 26. Chapters 2-7, which hold the pinouts (6.x), the parameter lists (5.x), the fault codes (4.x), the C-axis function (3.8) and the positioning functions (3.6), are not in part a. They are presumably in part b or later parts [uncertain].

## PDF page 10 (manual p. 8) - blank

The contents page shows through. No content.

## PDF page 11 (manual p. 9) - Chapter 1 title page

"1. Commissioning INDRAMAT a.c. mains spindle drives" (sic, "mains")
- 1.1 Overview of main spindle drive systems
- 1.2 Safety notes
- 1.3 Equipment required
- 1.4 Main spindle drive with KDA and TDA
- 1.5 Main spindle drive with RAC

## PDF page 12 (manual p. 10) - 1.1 Overview, Fig. 1

**Fig. 1: Main spindle drives with KDA or TDA (modular system)**. File name in the figure corner: "Hauptspindel-KDA/TDA" (main spindle KDA/TDA).

Block diagram:
- 3-ph mains -> **supply module** (Indramat supply module, e.g. TVD/KDV type, not named here) -> DC link busbars -> **KDA or TDA main spindle drive module** -> further "additional axis modules" (drawn dashed) on the same DC link.
- "Bus connection cable" between the supply module and the drive modules (control voltage and monitoring).
- **AS programming module**: plug-in parameter module on the front of the KDA/TDA.
- **Control panel**: detachable, with display and four arrow keys and a mode switch.
- "Heatsink fan (KDA only)" below the module.
- **Motor connection** (power) from the drive module to the motor; **motor feedback** from the motor back to the drive module.
- Motor fan: "3-ph mains" to the "Motor fan" (fed directly from the mains).
- "BB contact (X11)": ready-to-operate relay contact (Bb = Betriebsbereit, ready to operate).

Interfaces on the front:

| Connector | Function |
|---|---|
| X2 | Control inputs, signal outputs, analogue outputs |
| X3 | Motor feedback |
| X4 | Speed set-point, analogue, E1/E2 and E3/E4 (16-bit parallel and SERCOS are optional) |
| X5 | 2 encoder inputs for spindle feedback (optional) |
| X11 | Bb contact |

Interfaces underneath the controller:

| Connector | Function |
|---|---|
| X6 | Position set-point, parallel (optional); incremental encoder output (optional); RS 232 (optional) |

## PDF page 13 (manual p. 11) - Fig. 2, RAC 2.2

**Fig. 2: Main spindle drive with complete controller RAC 2.2**. File name in the figure corner: "Hauptspindel-RAC 2.2".

- Master switch (0 / I) on the front of the RAC 2.2.
- 3-ph mains enters the RAC directly. The RAC 2.2 is a complete unit with its own supply, so no separate supply module is needed.
- Control panel and **AS 5./ programming module** [the digit after "AS 5." is not printed; it reads "AS 5./"].
- Plug-in terminals: **X15: ON, OFF, CS, Bb**; **X16: motor temp. sensor**. X16 is drawn as 3-pole, X15 as 10-pole (dots).
- From the RAC to the motor: "Motor connection", "Motor fan connection" (the fan is fed from the RAC), "Motor feedback".

| Connector | Function |
|---|---|
| X2 | Control inputs, signal outputs, analogue outputs |
| X3 | Motor feedback |
| X4 | Speed set-point, analogue, E1/E2 and E3/E4 (16-bit parallel and SERCOS are optional) |
| X5 | 2 encoder inputs for spindle feedback (optional) |
| X13 | Position set-point, parallel (optional); incremental encoder output (optional); RS 232 (optional) |
| X15 | ON, OFF, CS, Bb |
| X16 | Motor temperature sensor |

## PDF page 14 (manual p. 12) - Fig. 3, RAC 3.1

**Fig. 3: Main spindle drive with complete controller RAC 3.1**. File name in the figure corner: "Hauptspindel-RAC3.1".

- Inputs at the top terminals: "3-ph mains", "Control voltage 220 V" (separate control supply, terminal X14 according to p. 24), "Motor connection", "Motor fan connection".
- Plug-in terminals X15 (ON, OFF, CS, Bb) and X16 (motor temp. sensor), control panel, **AS 6./ programming module**.
- Motor feedback goes back to the controller.
- Interfaces X2, X3, X4, X5 and X13 as for the RAC 2.2 (same text).

## PDF page 15 (manual p. 13) - 1.2 Safety notes

The following problems can occur when a drive or drive package is operated for the first time: wiring fault in the motor, controller or feedback; fault in the NC program; monitors not working "for operational reasons". These can cause personal injury and damage to the drive and the machine.

**1.2.1 Notes on safety of personnel**
- a) Danger from moving axes, due to unintentional starting after malfunctions or faults, and operation in the speed or position control loop.
  Measures: personnel must stay out of the area where movements take place; drives must be stopped and secured against unintentional starting when people work in the hazardous area.
  Protection against unintentional starting: disconnect the power contactor (EMERGENCY-STOP); switch the master switch off during long breaks.
- b) Danger from contact with electrical parts. Dangerous voltages: mains **L1, L2, L3**; link circuit DC voltage **L+, L-**; motor voltage **A1, A2, A3**.
  Measures: open the master switches and secure them against reconnection. The drives must be locked, because a rotating motor generates voltage on the motor cables. **Allow approx. 5 minutes for the link circuit to discharge** and check that it is below 50 V before starting work; if in doubt, use a short-circuit. In operation the transparent cover or front panel must be screwed on.

## PDF page 16 (manual p. 14)

**Note!** Current-operated e.l.c.b. systems (earth-leakage circuit breakers, RCDs) cannot be used with INDRAMAT equipment. In the case of indirect contact, the mains contactor should be replaced by other means, e.g. an overcurrent protective device.

**1.2.2 Notes on equipment protection**
- a) Incorrect connections: apply only voltages that meet the specified data; do not connect outputs to separate voltage sources; mains, link circuit and motor cables must not be connected to or touch the low-voltage **±15 V and +24 V** rails, and they must be well insulated from each other.
- b) Separate-source voltages or high voltage: the units are high-voltage tested to VDE 0160. Before a high-voltage or separate-source withstand test on the machine, disconnect or withdraw all unit connections (permitted by VDE 0113).
- c) Electrostatic charge: earth your body, the soldering iron, the parts and the tools. Store and ship programming modules only in conductive packaging.

## PDF page 17 (manual p. 15) - 1.2.3 Notes on machine protection

If, during commissioning, the NC position control loop is run separately and the drive is run in the speed control loop, there is a risk of machine damage because of the limited travel of linear axes. Only skilled personnel should apply the drive enable signal and the speed set-point. The emergency stop should be provided by limit switches or EMERGENCY-STOP buttons.

## PDF page 18 (manual p. 16) - 1.3 Equipment required, Fig. 4

Equipment needed for commissioning:
- DC and AC multimeter
- Set-point box (analogue set-point)
- 37-way sub-miniature connector (for X2)
- 9-way sub-miniature connector (for X4)
- 10-way plug terminal block (RAC only, for X15)

**Fig. 4: Circuit for operating the drive with the above-mentioned equipment (set-point box and sub-miniature connectors).** File name in the figure corner: "Sollwertbox" (set-point box).

Legend of the set-point box:
- R1: 10-turn potentiometer for U_set-point (10k)
- R2: protective resistor (1k)
- S1: 3-pole changeover switch
- S2: switch for the controller enabling signal
- V: DC voltmeter for indication (+/-10 V)

**Connector X4** (9-way Cannon/D-sub; "Solder side" view, pins 1-5 top row, 6-9 bottom row; "Solder cup contacts"), on the controller side:

| X4 pin | Signal (as printed) | Meaning |
|---|---|---|
| 1 | (E1) | Speed set-point input 1, differential input with E2 |
| 2 | (E2) | Speed set-point input 1, reference |
| 4 | earth symbol | Screen / housing earth |
| 5 | (E3) | Second differential set-point input (C-axis / EXT POS, see p. 20) |
| 6 | (E4) | Reference for E3 |
| 7 | 0V | Signal zero |
| 8 | -15V | Auxiliary supply out |
| 9 | +15V | Auxiliary supply out |
| 3 | not shown | - |

E1/E2 and E3/E4 each feed a differential amplifier (triangle symbol) in the controller.

Set-point box wiring to X4, as far as it can be traced in the scan:
- X4/8 (-15V) and X4/9 (+15V) go to the two throws of S1. S1 selects the polarity of the supply for the potentiometer R1 (10k). R1 is in series with R2 (1k) to X4/7 (0V) [the exact pole assignment of S1 is uncertain: three poles, and the lines cross].
- The S1 common / R1 top node and the R1 wiper are taken through the voltmeter V (+/-10 V) to the set-point inputs: one node to **X4/1 (E1)** and **X4/5 (E3)**, the other node to **X4/2 (E2)** and **X4/6 (E4)**. E1/E2 and E3/E4 are therefore driven in parallel [uncertain which node is the wiper].
- The cable screen (dashed ellipse) goes to X4/4 (earth).

**Connector X2** (37-way Cannon/D-sub; "Solder side" view, pins 1-19 top row, 20-37 bottom row; "Solder cup contacts"), "Control unit":

| X2 pin | Signal (as printed) | Wiring in Fig. 4 |
|---|---|---|
| 1 | 0V ext | bridged to X2/2 |
| 2 | 0V int | bridged to X2/1 |
| (no number printed) | AS (KDA/TDA only) | connected to the +24 V bus (X2/18-X2/19); the pin number is not printed [unreadable] |
| 18 | +24V int | bridged to X2/19 (internal 24 V used as external 24 V) |
| 19 | -24V ext (printed with a minus sign; logically +24 V ext [uncertain]) | bridged to X2/18; feeds S2 and the E-STOP button |
| 23 | E-STOP (RAC only) | from the +24 V bus through an EMERGENCY-STOP pushbutton (NC contact) |
| 24 | RF | Regler-Freigabe (controller enable), from +24 V through S2 |
| 25 | RUN | bridged to X2/24, so it is switched together with RF through S2 |

Connections in Fig. 4, summarised: X2/1-X2/2; X2/AS-X2/18-X2/19 (+24 V bus); X2/19 -> S2 -> X2/24 (RF) and X2/25 (RUN); +24 V bus -> NC E-STOP button -> X2/23. Page 26 confirms the NC contact between X2/18 and X2/23.

## PDF page 19 (manual p. 17) - 1.4 Main spindle drive with KDA or TDA

**1.4.1 Mains supply requirements**: before each switch-on, check that the mains meets the requirements of the supply module (see the supply module documentation).

**1.4.2 Checks with the equipment switched off**
- a) Installed drive components: they must be designed for the existing input voltages. Check the rating plate against the type code. The **AS programming module** data must match the ratings of the installed drive components, otherwise there is a risk of damage.
- b) Condition of wiring: check for short-circuits, breaks, wrong connections ("corrections" as printed), cross-sections and identity against the INDRAMAT connection diagram.
  - Earthing: exactly as in the INDRAMAT diagrams. Each motor is earthed at its own drive module. Drive modules are earthed separately at the power supply module. The supply module earthing point is the central reference earth for all drive components and is connected to the mains earth. The module housings must make good electrical contact with the cubicle.
  - Twisting the load conductors: twist the motor leads from the main drive module, or use a 4-core cable (3 x phase, 1 x earth).
  - Connections to auxiliary modules (additional capacitance module, extra storage capacitors): twisted and as short as possible.
  - Power connections to main drive modules: normally two busbars between modules that sit side by side. Otherwise two twisted **16 mm²** conductors, not more than **1 m** long.

## PDF page 20 (manual p. 18)

- c) Cross-sections of power cables: to VDE 0100 / VDE 0113, at the maximum ambient temperature and the continuous motor and transformer currents.
- d) Terminals and connectors: conductors secure; sub-miniature connectors screwed up.
- e) Bus connecting cable: control voltage and monitor connections run through a bus cable whose **black core must be underneath**.
- f) End connector for the bus cable: fit the line monitor end connector (supplied with the power supply module) to the drive module farthest from the supply module.
- g) Screening: connect the screens of the set-point cable, motor feedback cable, spindle transmitter (encoder) cable and temperature sensor leads at the main drive module.
- h) Power transformer: do not interchange primary and secondary. Excessive supply voltage can damage the drive package.

**1.4.3 Checks with the signal conditioning powered up**: with the drive package switched off, disconnect the NC speed set-point (X4) and the control inputs/signal outputs (X2). Connect the set-point box to X4 and wire the control inputs X2 as in Fig. 4. The control voltage must be present at the power supply module, which also powers the signal electronics in the drive modules. **Main contactor K1** must be off.

## PDF page 21 (manual p. 19)

a) Check the LED and LCD status signals:

| Location | Signal | Colour | Status |
|---|---|---|---|
| Supply module | Bleeder overload | red | off |
| Supply module | Power ON | green | off |
| Main drive module KDA | FAULT | red | off |
| Main drive module KDA | READY | green | off |

"NO POWER" must appear in the LCD.

- b) Fan operation: check the connections and operation of the fans on the main drive module and the motor.
- c) Power stage on/off sequence: the switching sequence is correct when the "ready" contacts of the supply and main drive modules are wired as in the INDRAMAT diagrams. Power can then only be applied when the **Bb1 contact** in the supply module is closed. Series resistors are short-circuited only if the **Bb contact of the main drive module closes** or **+24 V is present at the READY output**.
- d) DC link short circuit: so that the main drive can be braked when the mains is disconnected, **no intermediate circuit short-circuit resistor** may be fitted.
- e) Motor feedback: set the mode switch to the right and press "up" until motor speed **N** appears. Turning the motor shaft clockwise by hand (seen from the front) must show a positive speed.

## PDF page 22 (manual p. 20)

f) Checking the speed set-point (analogue):
- Press "up" until speed set-point **C** appears.
- A positive voltage at **X4/1 relative to X4/2** shows a positive set-point.
- **C-axis operation**: apply a positive set-point to **X4/5 relative to X4/6**. The set-point input is activated via the **EXT POS** input and parameter **PQ-FUNCT (P/Q/R/S 16)**. A positive set-point again appears in the display (see section 3.8 "C-axis operation").
- **Note:** the set-point input null is set at the factory with the inputs short-circuited. It can be adjusted for X4/1-X4/2 with **potentiometer P1 above X4**, and for X4/5-X4/6 with **potentiometer P2 below X4** (KDA).

g) Checking the spindle speed detector: this requires a separate spindle feedback on the optional interface for the 2nd encoder input (**X5**). Press "up" until spindle speed **S** appears. Turning the incremental encoder shaft clockwise by hand must show a positive speed.

h) Checking the motor temperature measurement: press "up" until **T** appears.
- With a cold motor: "T < 40°C".
- A flashing display means the sensors are wrongly connected or the motor is below 0°C.
- If the motor temperature sensors are faulty, the drive switches off after **10 minutes** and "NO TEMP." appears. **"TEMP WARN"** goes to zero **30 s** before this.

**1.4.4 Checks after the power infeed has been connected**: set the enable switch on the set-point box to OFF and the set-point to 0 V, then connect power.
a) Status: green "Power on" LED on the supply module lit; **"NO RF"** (no controller enabling signal) in the KDA display; green "READY" LED on the KDA lit.

## PDF page 23 (manual p. 21)

- b) DC link voltage: press "up" until **UD** appears. It must read **255-345 V** (KDA/TDA on its supply module).

**1.4.5 Initial start-up**
- a) Switch on the enable on the set-point box and apply a small set-point. The speed must follow.
  **Important:** if there is a fault the drive can "chatter" uncontrollably. Remove the enable at once. If the motor turns in an uncoordinated way, check the motor phase sequence. If it only turns slowly without following the set-point, check the feedback connections.
- b) Speed / set-point ratio: the drive must reach the speed in parameter **MAXRPM (A01)** when the voltage in parameter **CMDVOLT (A02)** is applied.

## PDF page 24 (manual p. 22) - 1.5 RAC 2.2 and RAC 3.1

**1.5.1 Mains supply requirements**

RAC 2.2:
- 380 V type (type code box "AC supply voltage = 380"): 3 x 380 V +15%/-10%, 50/60 Hz
- 460 V type (type code box "AC supply voltage = 460"): 3 x 380 V +15%/-10%, 50 Hz, or 3 x 415 V +10%/-15%, 50 Hz, or 3 x 460 V +15%/-10%, 60 Hz
- mains-related earth

RAC 3.1:
- 3 x 380 V to 460 V ±10%, 50/60 Hz
- mains-related earth

No transformer is needed if these conditions are met.

Other mains supplies:
- Mains-related earth but other phase-to-phase voltages: use an auto-transformer.
- No mains-related earth conductor but suitable phase-to-phase voltages: no isolating transformer needed if the mains meets special conditions and the RAC has overvoltage protection devices (see "Electrical connections").
- No mains-related earth and different voltages: three-phase isolating transformer, secondary as above, short-circuit voltage ≤ 4%, secondary neutral connected to system earth.

## PDF page 25 (manual p. 23) - 1.5.2 Checks with the equipment switched off

- a) Compare the rating plates with the supply voltage. The AS programming module data must match the installed components. **If a KDA 3.2 is operated with an RAC 2.2, the KDA must be designed for 500 V supply (type code box "DC supply voltage" = 5).**
- b) Wiring: as in 1.4.2 b). Earthing per the INDRAMAT diagrams; RAC housing in good contact with the cubicle; twisted motor leads or a 4-core cable; connections to further KDA modules (RAC 2.2 only) twisted and short.
- c) Cross-sections: VDE 0100 / VDE 0113.
- d) Terminals and connectors secure; sub-miniature connectors screwed up.

## PDF page 26 (manual p. 24)

- e) Bus cable to additional KDA drive modules (RAC 2.2 only): black core underneath. No end connector required.
- f) Screening: set-point cable, motor feedback cable, spindle transmitter cable and temperature sensor leads are screened and connected at the RAC.
- g) Power transformer: do not interchange primary and secondary.

**1.5.3 Checks with mains supply connected but power circuits disconnected**: with the RAC off, disconnect the NC set-point (X4) and X2. Connect the set-point box to X4 and wire X2 as in Fig. 4. **Withdraw plug connector X15** so that the power contactor cannot pull in.
- a) Control voltage ON: RAC 2.2: master switch to ON. RAC 3.1: apply **220 V control voltage to terminal X14**. Panel: READY and FAULT LEDs off; "NO POWER" in the display.
- b) Fan: check the fans on the RAC and the motor. A transformer for the motor fan supply is only needed if an RAC 3.1 is on mains above **3 x 420 V, 50 Hz**.
- c) Motor feedback: "up" to N; clockwise by hand gives a positive speed.

## PDF page 27 (manual p. 25)

- d) Speed set-point: as on page 20. X4/1 positive relative to X4/2 gives a positive C. For C-axis: X4/5 relative to X4/6, enabled via EXT POS and **PQ-FUNCT (PQRS 16)**. **Note:** on the RAC, null adjustment P1 is **left** of X4 (X4/1-X4/2) and P2 is **right** of X4 (X4/5-X4/6).
- e) Spindle speed detector: separate spindle feedback on X5 (2nd encoder input); "S" display positive for clockwise rotation.
- f) Motor temperature: same text as on page 20 (T < 40°C cold; flashing = wrongly connected or < 0°C; NO TEMP. shutdown after 10 min, TEMP WARN drops 30 s before).

**1.5.4 Connecting the power infeed**: enable switch OFF, set-point 0 V. Power is applied by switching the **ON and OFF contacts on terminal strip X15**. Use a 10-way terminal block with 2 switches: an **NO contact between X15/1 and X15/2** (ON) and an **NC contact between X15/3 and X15/4** (OFF), and plug it onto X15. Power is switched on with the NO contact and off with the NC contact.

## PDF page 28 (manual p. 26)

**Note:** do not disconnect the power with the NC contact while the motor is rotating, because the motor can then coast uncontrollably. Instead switch control input **E-STOP (X2/23)** to 0 (an NC contact between **X2/18 and X2/23**, see Fig. 4). The motor is then braked to a standstill before the main contactor drops out.

- a) Panel messages: green READY LED on; "NO RF" (no controller enabling signal) in the display.
- b) DC link voltage: "up" until UD appears. It must read **480-550 V** (RAC).

**1.5.5 Initial start-up**
- a) Enable on the set-point box, small set-point, the speed must follow. **Important:** the same chatter / phasing / feedback warning as on page 23.
- b) The drive must reach **MAXRPM (A01)** at the set-point voltage **CMDVOLT (A02)**.

---

## For the retrofit

What part a tells us (generic Indramat information; the machine-specific type is not in these pages):

- **Drive family**: Indramat analogue-era AC main spindle controllers for asynchronous motors: modular **KDA 3.2 / TDA 1.1** on a supply module (TVD/KDV) with a common DC link of approx. 300 V (UD 255-345 V), or the compact **RAC 2.2 / RAC 3.1** (UD 480-550 V). Motors are 2AD or **1MB frameless** (built-in spindle motor). For an 18 kW integrated spindle motor, 1MB with a KDA or RAC is likely, but **check the rating plates** and the type code (and the AS programming module) of both spindle drives. The CTX twin has two spindles, so two drives.
- **Speed command**: standard **analogue differential ±10 V set-point at X4/1 (E1) - X4/2 (E2)**, scaled by **CMDVOLT (A02) -> MAXRPM (A01)**. A second differential input **X4/5 (E3) - X4/6 (E4)** is used for **C-axis operation**, selected by the **EXT POS** input and **PQ-FUNCT (P/Q/R/S 16)**. The drive then runs in position/C-axis mode with the NC closing the position loop through E3/E4. X4/7 is 0 V and X4/8/9 are ±15 V. Options are a 16-bit parallel set-point or SERCOS. A LinuxCNC analogue output card (e.g. Mesa 7i77) can drive E1/E2 directly. A second DAC channel or a relay can handle the C-axis input E3/E4 with EXT POS.
- **Enable / ready / fault** (X2, 37-pin D-sub): **RF (X2/24) = controller enable** and **RUN (X2/25)** at +24 V. **E-STOP (X2/23, RAC only)** must be held at +24 V; taking it to 0 brakes the motor to a stop. **AS** (KDA/TDA only, the starting lockout, section 3.21) is on the 24 V bus. X2/1-2 are 0 V ext/int and X2/18-19 are +24 V int/ext. Status outputs include **READY** (+24 V output), the **Bb contact** (X11 on KDA, X15 on RAC) for the main contactor chain (Bb1 in the supply module), and TEMP WARN. RAC power on/off is through **X15/1-2 (ON, NO)** and **X15/3-4 (OFF, NC)**. The X2 pin numbers of the signal outputs (READY, n=0, n=nset, LOAD LIMIT, analogue outputs N and M) are in section 6.1, which is not in part a.
- **Spindle position feedback / C axis / threading**: motor feedback is on X3. A separate spindle encoder can go to **X5 (2 encoder inputs, optional)**. The drive can output an **incremental encoder signal (IGS option)** on **X6 (KDA) / X13 (RAC)**. That output is the natural source for LinuxCNC spindle position (threading, G33/G76, rigid tapping, C axis, and synchronising both spindles for the twin part transfer). Check whether the CTX twin has the IGS option or a separate spindle encoder (HGV 1 coupler / Indramat high-resolution encoder) by looking at the X5/X6/X13 cables. Chapter 6 (pinouts) is in a later part.
- **Orientation**: spindle positioning is built into the drive (section 3.6, via motor feedback, motor feedback plus reference switch, or spindle feedback) and is triggered by control inputs. The details are in part b. Either keep drive-internal orientation (driven by HAL outputs) or orient in LinuxCNC through the C-axis mode.
- **Temperature monitoring**: motor temperature sensor (X16 on RAC). TEMP WARN pre-warning goes to 0 30 s before the shutdown. Sensor fault gives NO TEMP. and shutdown after 10 min. Wire TEMP WARN to a LinuxCNC input for a controlled stop.
- **Safety and power**: no RCDs (e.l.c.b.); 5 min DC link discharge; do not open the main contactor while the spindle is turning; use E-STOP X2/23 first. Do not fit a DC link short-circuit resistor (needed for braking at mains loss).
- **Replacing the drives**: a new drive must handle an asynchronous built-in motor (1MB-type) with the existing motor encoder (Indramat motor feedback on X3; type and signal levels to be checked), the motor KTY/PTC temperature sensor, and the separate motor fan. It must provide an encoder output or a spindle encoder for threading/C axis, positioning, and electronic gearing or sync for the twin part transfer, which a modern drive with a fieldbus/position mode or LinuxCNC position-loop control (velocity command with an encoder) can do. The motor data (M parameters, chapter 5.3) from the existing AS module should be read out before decommissioning. Use the panel parameter mode or RS 232 to record the A/P/Q/R/S/M parameters.
