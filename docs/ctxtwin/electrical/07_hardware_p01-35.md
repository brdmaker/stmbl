# Gildemeister CTX twin, electrical documentation: 7. CNC hardware modules - ELTROPILOT controls, PDF pages 1-35

Source: `d5951336-7_Hardware_20201029_0001.pdf` (chapter "7 Hardware", 70 scanned pages; this file covers PDF pages 1-35, the other half is transcribed separately). Footer title block on every page: "Electrical technical documentation CTX twin" / "Electrical technical instructions CTX twin", **Doc. No.: DAAAE0G0, November 1993**, page numbers "Page 7-1" ... "Page 7-35". Running header: "7. CNC hardware modules - ELTROPILOT controls".

Original language: **English** (this chapter is the manufacturer's English translation; a few German terms remain in the drawings, they are quoted as printed with a translation). It is a generic description of the ELTROPILOT EP90 control hardware (Eltromatic / Gildemeister), not a machine-specific wiring diagram: it does not name the Indramat drive types or cabinet device tags (-K.., -X..). Those are in the other chapters.

Notation: [uncertain] = reading not sure, [unreadable] = cannot be read.

## Page index

PDF page = printed page number 7-n with n = PDF page (the page numbering is continuous, PDF page 1 = Page 7-1).

| PDF page | Printed page | Content |
|---|---|---|
| 1 | 7-1 | Table of contents of chapter 7 |
| 2 | 7-2 | blank (header/footer only) |
| 3 | 7-3 | 7.1.1 ELTROPILOT components (block diagram of the control rack) |
| 4 | 7-4 | 7.1.2 Connectors (front connector layout of VAP2/4, VIC30, VK030, VD030, VSNT350) |
| 5 | 7-5 | 7.1.3 General plan of 2-axis controls ("General plan of 2-axis EP90") |
| 6 | 7-6 | 7.1.4 General plan of 4-axis controls ("General plan of 4-axis EP90") |
| 7 | 7-7 | 7.1.5 General plan of 2+2 axis controls ("General plan of EP90 duplex machine"), sheet 1 |
| 8 | 7-8 | General plan of EP90 duplex machine, sheet 2 (VAP4(2), VAP4(3), C-axis II, HA II, AWZ I/II) |
| 9 | 7-9 | 7.2 BGT-V chassis and VSNT350 power pack, 7.2.1 Construction and concept, slot plan |
| 10 | 7-10 | Module dimensions; current supply concept (block diagram) |
| 11 | 7-11 | 7.2.2 Construction of chassis, mechanical data, environment |
| 12 | 7-12 | Shock/vibration without absorbers; assembly and installation drawing |
| 13 | 7-13 | 7.2.3 Power pack construction, characteristics, output voltages |
| 14 | 7-14 | Power pack block diagram (X0/X4, X1, X2) |
| 15 | 7-15 | Power failure and undervoltage (ACFAIL timing) |
| 16 | 7-16 | VSNT350 front connectors X1, X2 (pinouts), buffering input, PON signal |
| 17 | 7-17 | PON ratings, buffer mode, POK relay contact, thermoswitch, displays |
| 18 | 7-18 | VSNT350 module front |
| 19 | 7-19 | 7.3 VHD/40 hard disk module, technical features |
| 20 | 7-20 | VHD/40 VME connection, connector P1 pinout |
| 21 | 7-21 | VHD/40 connector X1 (SCSI) pinout |
| 22 | 7-22 | VHD/40 connectors X2/X3 (supply), X4/X5 (LED) |
| 23 | 7-23 | VHD/40 displays, SCSI address jumpers, SCSI termination |
| 24 | 7-24 | VHD/40 module front |
| 25 | 7-25 | 7.4 VD030 processor card with VGI, 7.4.1 VD030 features |
| 26 | 7-26 | VD030 features (cont.), expansion modules (VGI, SIOADP) |
| 27 | 7-27 | Block diagram VD030 |
| 28 | 7-28 | VD030 connectors X1 (20 mA serial pinout), X2, X3, X4/X5 |
| 29 | 7-29 | VD030 RESET switch, STATUS display; 7.4.2 VGI graphics card |
| 30 | 7-30 | VGI block diagram |
| 31 | 7-31 | VGI technical data |
| 32 | 7-32 | VGI video interface electrical values, VGA timing |
| 33 | 7-33 | VGI 15-pole monitor connector pinout, VD030 technical data |
| 34 | 7-34 | VD030 front of module |
| 35 | 7-35 | 7.5 VK030 processor card with SI188 PLC card, 7.5.1 VK030 features |

---

## PDF page 1 (Page 7-1): Table of contents

**7. CNC hardware modules - ELTROPILOT controls**

| Section | Title | Page |
|---|---|---|
| 7.1 | General, general plan | 7-1 |
| 7.1.1 | ELTROPILOT components | 7-3 |
| 7.1.2 | Connectors | 7-4 |
| 7.1.3 | General plan of 2-axis controls | 7-5 |
| 7.1.4 | General plan of 4-axis controls | 7-6 |
| 7.1.5 | General plan of 2+2-axis controls | 7-7 |
| 7.2 | BGT-V chassis and VSNT350 power pack | 7-9 |
| 7.2.1 | Construction and concept | 7-9 |
| 7.2.2 | Construction of chassis | 7-11 |
| 7.2.3 | Construction of power pack | 7-13 |
| 7.3 | VHD/40 hard disk module | 7-19 |
| 7.4 | VD030 processor card with VGI graphics card | 7-25 |
| 7.4.1 | VD030 processor card | 7-25 |
| 7.4.2 | VGI graphics card | 7-29 |
| 7.5 | VK030 processor card with SI188 PLC card | 7-35 |
| 7.5.1 | VK030 processor card | 7-35 |
| 7.5.2 | SI188 PLC card | 7-38 |
| 7.6 | VIC30 interpolator card | 7-41 |
| 7.7 | VAP4 axis periphery card | 7-45 |
| 7.8 | IO48 input/output module | 7-52 |
| 7.9 | Control panel components | 7-58 |
| 7.9.1 | Monitor unit | 7-58 |
| 7.9.2 | 9" colour monitor | 7-60 |
| 7.9.3 | Membrane keyboard | 7-62 |
| 7.9.4 | Handwheel | 7-68 |

Note: the TOC places VAP4 (7.7, page 7-45) and IO48 (7.8, page 7-52) in the second half of the PDF (pages 36-70), not in this file.

## PDF page 2 (Page 7-2)

Blank page (header and footer only).

## PDF page 3 (Page 7-3): 7.1.1 ELTROPILOT components

Block diagram (drawn rotated 90°). A dashed frame labelled **Chassis** contains the **VAPB-10 bus circuit board** with two buses drawn as arrows: **VME-Bus** and **Axis periphery bus**. Modules plugged on the bus board (from bottom to top of the drawing):

| Module | Type | Front connectors and where they go |
|---|---|---|
| Centr. accum. (central accumulator) | - | connected to the bus board (battery back-up) |
| Power pack | (VSNT350) | X1 -> "ELTROMATIC supply" (to the IO48 digital I/O modules); X2 [no outgoing line drawn] |
| Hard disk module | VHD/40 | X1 -> SCSI -> VD030 X2 |
| D-process. (D-processor) | VD030, with sub-modules "Graphic IPIN" and "Interface adaptor" | X1 -> "2x serial interface"; X2 -> SCSI (hard disk); X3 (Graphic IPIN) -> "Video interface"; X4, X5 (Interface adaptor) [no outgoing line drawn] |
| K-process (K-processor) | VK030, with sub-module "SPS-IPIN" (SPS = PLC) | X1 [no line]; X2 (SPS-IPIN) -> chain of IO48 modules (and "(alternative)" dashed line) |
| SW-IPO (software interpolator) | VIC30 | X1 |
| Axis periphery module | VAP4 | X1 -> "anal. I/O's (1 & 2)"; X2 -> "digit. I/O's"; X3 -> "Generator (1 & 2)"; X4 -> "anal. I/O's (3 & 4)"; X5 -> "Generator (3 & 4)" |

The VD030, VK030, VIC30 and VAP4 connect to the VME-Bus; the VAP4 and VIC30 additionally to the axis periphery bus.

ELTROMATIC digital I/O chain: three boxes "digit. I/O's (IO48)", each with connectors X11 [only on the first], X12, X13. VK030 X2 (SPS-IPIN) -> first IO48 X12 [uncertain which of X11/X12; the dashed "(alternative)" line goes to X11]; IO48 X13 -> next IO48 X12 -> X13 -> next IO48 X12; last X13 is closed by a terminator plug symbol. Power pack X1 "ELTROMATIC supply" feeds the chain. Arrow "to the control panel" from the IO48 chain.

"Generator" = encoder (incremental measuring system) input.

## PDF page 4 (Page 7-4): 7.1.2 Connectors

Front view of the connectors per module (drawn rotated):

| Module | Connector | Function (as printed) |
|---|---|---|
| VAP2/4 | X1 | Anal. I/O's (1 & 2) |
| VAP2/4 | X2 | Digit. I/O's |
| VAP2/4 | X3 | Generator (1 & 2) |
| VAP2/4 | X4 | Anal. I/O's (3 & 4) (only in the upper part, separated by a dashed line = VAP4 only) |
| VAP2/4 | X5 | Generator (3 & 4) (VAP4 only) |
| VIC30 | X1 | SI01 & SI02 (serial interfaces) |
| VK030 | X1 | SI01 & SI02 |
| VK030 | X2 | ELTROMATIC-Bus |
| VD030 | X1 | SI01 & SI02 |
| VD030 | X2 | SCSI (to the hard disk module) |
| VD030 | X3 | Video |
| VD030 | X4 | SI03 |
| VD030 | X5 | SI04 |
| VSNT350 | X1 (pin 1 marked) | ELTROMATIC supply |
| VSNT350 | X2 (pin 1 marked) | Accum. unit, Relay contact, Therm. contact |

So VAP2 = 2-axis variant (X1-X3), VAP4 = 4-axis variant (X1-X5): each VAP handles 2 analog outputs + 2 encoder inputs per connector pair.

## PDF page 5 (Page 7-5): 7.1.3 General plan of 2-axis controls

Caption: "General plan of 2-axis EP90". Overview cabling plan (rotated):

- **Operating unit** (dashed frame): **BF?90** [uncertain, printed "BF190"/"BF90"; panel module with X1, X4, X5] and **Monitor**. Cable **W00** between BF90 and Monitor (and to VD030 video); cable **W0?** [uncertain, "W03"/"W09"] from BF90 to VD030. "Connection to ext. e.g. alpha keyboard" from BF90 X5.
- **Control** (dashed frame): Power pack (X1), VD030 (X1...X5), VK030 (X1, X2), VAP4 (X1...X5).
- External lines from the control: "230V; 50Hz", "Connection to periph. unit (reader/punch)", "Connection to diagnosis terminal".
- Power pack X1 -> cable **W05** -> IO48 chain (ELTROMATIC supply).
- VK030 X2 -> cable **W07** -> "Dig.I/O" X12 (or "alternative" dashed to X11); then **W08** to the next "Dig.I/O" X12 -> X13 -> next "Dig.I/O" X12; last X13 terminated. "Dig.I/O's (measuring circuits)" line from VAP4.
- VAP4 -> four drive groups, each "Contr." (drive controller) + motor M + encoder symbol:
  - **X-axis I**: Contr. with cables W11, W12, W13 [uncertain assignment]; motor M with encoder.
  - **Z-axis I**: Contr. with W12, W14; motor with encoder.
  - **HA** (Hauptantrieb = main spindle drive): Contr. W14; motor M with an encoder, plus a second encoder labelled **C-axis** (separate C-axis encoder), cable W11/W12.
  - **AWZ** (Angetriebene Werkzeuge = driven tools): Contr. W13; motor M, no encoder drawn.
- Cable meaning by context: W11 = encoder (generator) cable to VAP X3/X5, W12 = analog set-point cable from VAP X1/X4, W13/W14 = drive enable/ready (digital) [uncertain; the numbers are legible but their function is not printed on this page].

## PDF page 6 (Page 7-6): 7.1.4 General plan of 4-axis controls

Caption: "General plan of 4-axis EP90". Same as page 5 but with **two VAP4** cards:

- VAP4 (lower): X-axis I, Z-axis I, C-axis (encoder) / HA.
- VAP4 (upper): AWZ, X-axis II, Z-axis II.
- Each axis: Contr. + M + encoder; cables W11, W12, W13, W14 as on page 5.
- Rest identical (Operating unit BF90 + Monitor, W00, W0?, VD030, VK030, Power pack, W05, W07, W08, IO48 chain with measuring circuits).

## PDF page 7 (Page 7-7): 7.1.5 General plan of 2+2 axis controls

Caption: "General plan of EP90 duplex machine" (this is the CTX twin type: two turret slides). Three VAP4 cards: **VAP4(1)**, **VAP4(2)**, **VAP4(3)** ("See Pg. 4" for VAP4(2)/(3), continued on next sheet).

- VAP4(1): X-axis I, Z-axis I, C-axis 1 (second encoder on HA 1), HA 1.
- VAP4(2): X-axis II, Z-axis II (box for Z-axis II printed "Regler" (controller) instead of "Contr.").
- Cables W11, W12, W13, W14 per axis.
- Operating unit, Control, W00, W05, W07, W08, IO48, as on page 5.

## PDF page 8 (Page 7-8): EP90 duplex machine, sheet 2

Caption: "General plan of EP90 duplex machine".

- VAP4(2) -> **HA II** (main spindle drive II) with motor encoder and a second encoder **C-axis II**; cables W11, W12, W13. Also "Dig. I/O's measuring circuits" from VAP4(2).
- VAP4(3) -> **AWZ I / Rev. I** and **AWZ II / Rev. II** (driven tools I and II; "Rev." = revolver = turret); each Contr. + M + encoder; cables W11, W12, W13, W14.

So in the duplex (CTX twin) configuration the control handles: X I, Z I, X II, Z II (feed axes), HA I + C I, HA II + C II (two spindles with separate C-axis encoders), AWZ I, AWZ II (two driven-tool drives with encoder) = 8 drives and 10 encoder inputs on three VAP4 cards (3 × 4 = 12 analog channels / 12 encoder channels available).

## PDF page 9 (Page 7-9): 7.2 BGT-V chassis and VSNT350 power pack / 7.2.1 Construction and concept

Text (as printed): "The mechanical construction of the chassis system is in accordance with DIN 41494, part 5 for a 6HE printed circuit board platform (double European: 233.4* 160 mm²). The width dimension is produced by the number of components to be fitted which are designed in rack/slide technique. The BGT10-V comprises of a power pack, a hard disk module and 10 plug-in circuit boards. A ventilation unit is located beneath the slide section, on the front of which the power connection (230 V) and an accumulator for data back-up in the event of a loss of power supply are accessible. The BGT is designed for back panel attachment."

"BGT10-V showing one component variant." Front view drawing:
- Left: Hard disk module, Power pack.
- Slots labelled (left to right): D-processor module, K-processor module, Interpolation unit, 2-axis peripheral module, free.
- Bottom: "Accu" and "Mains" (front of ventilation unit). Label "BGT-Front".
- "10 printed circuit board slots (4 TE each) => : 1: 2: 3: 4: 5: 6: 7: 8: 9:10:"
- "Daisy chain jumper (see VME bus)"; "Position jumper on top bus connector as it is not assigned by the VD030 D-processor module" -> slot 2.
- "No jumper required as no module is located in printed circuit board slots 5 - 10 with a VME bus connection" [as printed].

"A bus printed circuit board is located at the rear of the BGT for performing the main system connections. The plug-in circuit boards are equipped with two 96 pole connectors for connection to this bus printed circuit board. Due to the high plug-in and pull-out forces resulting from the large number of contacts, the modules are provided with two lever handles to improve handling."

## PDF page 10 (Page 7-10): module dimensions, current supply concept

Drawing: side view of a plug-in board (Display, Switch, Periphery connector at front; Bus connector (P1) and Bus connector (B2) [as printed, = P2] at rear; dimensions 160 and 233,4) and front of module (4TE / 8TE widths).

"Most of the plug-in circuit boards have a width of 4 TE (20.32 mm) and thus occupy one printed circuit board slot in the chassis. However, there are also modules with a width of 8 TE which are in part required for plug fitting sub-modules or the arrangement of several peripheral connectors."

**Current supply concept**: "The central power supply of the control components (BGT and ELTROMATIC) encompass a total of seven voltage values, all of which are generated by a plug-in power pack and having a common 0V reference."

Block diagram: Power pack outputs A1 +5V, A2 +12V, A3 -12V, A4 +15V, A5 -15V, A6 +5VG -> bus circuit board -> plug-in circuit boards; A6 also "+5V" to "External measuring systems"; A7 +7,5V -> ELTROMATIC module; STDBY from Accu. to the power pack; Chassis.

"A high 5V power supply is available for feeding the digital logic system. The auxiliary +/- 12V voltages are primarily required for operating drives and digital interface components. The +/- 15V voltage is required for supplying analog switching elements."

## PDF page 11 (Page 7-11): 7.2.2 Construction of chassis

"The BGT 10 - V chassis is a carrying system designed according to EMV aspects. Observance of DIN 41494 ensures that plug-in circuit boards, consisting of double European card, connectors according to DIN 41612 and front panel are interchangeable. ... The power pack, the hard disk module, as well as the plug-in circuit boards are adequately cooled by two axial ventilators fitted underneath the printed circuit board slots. The rear panel of the chassis is designed for cabinet installation. ... If the limiting values for oscillations and vibrations indicated in the specification are exceeded, a damping set is offered."

**Mechanical construction**: galvanized steel plates, five sectional/profile rails for the module group plastic guides; slide-in ventilator beneath the module groups (can be pulled out to the front); accumulator holder and power filter at the front.

| Item | Value |
|---|---|
| Total width | approx. 421 mm |
| Total height | approx. 358 mm |
| Total depth | approx. 280 mm |
| Ambient temperature, out of operation | -10 °C ... 60 °C |
| Ambient temperature, in operation | 5 °C ... 45 °C |
| Relative humidity, out of operation | 5 % ... 95 % without condensation |
| Relative humidity, in operation | 8 % ... 85 % without condensation |
| Height above sea level, out of operation | -60 m ... 12 km |
| Height above sea level, in operation | -60 m ... 1 km |
| Protection type | IP20 acc. to DIN 40050 or IEC 529 (front side); design according to VDE 0160 |
| Shock (with vibration absorbers) | 1/2 sine wave 11 ms (max. 10 knocks); out of operation 40 g, in operation 4 g |
| Vibration (with absorbers) | 80...200 Hz sine (peak-peak), 1 oct./min; out of operation 1.6 g, in service 0.4 g |

## PDF page 12 (Page 7-12): installation

| Shock and vibrations without vibration absorbers | Value |
|---|---|
| Shock | 1/2 sine wave 11 ms (max. 10 knocks); out of operation 30 g, in operation 3 g |
| Vibration | 80...200 Hz sine (peak-peak), 1 oct./min; out of operation 1.0 g, in operation 0.2 g |

**Assembly and installation** drawing: front view width 420 / 400 mm, height 325 [uncertain] / 357 mm; accumulator and 230 V inlet at the bottom front; "Fastening holes for M6"; bottom view with two fans, depth 275 mm, width 375 mm; marks "EE" [uncertain] and "PE".

"The BGT 10 - V is designed for installation in a switch cabinet or a comparable casing with a minimum protection type of IP 54. Install using screws size M6. Ensure that there is a clearance of 100 mm each above and below the casing for ventilation purposes."

## PDF page 13 (Page 7-13): 7.2.3 Power pack construction

"The power supply of the EP90 control system is assured by a slide-in power pack in double European format which is inserted at the front of the chassis. The unit is designed for a 230V power supply and has 7 output voltages."

| Output | Voltage | Use |
|---|---|---|
| A1 | +5 V | for system logic |
| A2, A3 | ±12 V | for digital interfaces etc. |
| A4, A5 | ±15 V | for analog circuit components |
| A6 | +5 V | for external measuring systems |
| A7 | +7.5 V | for external ELTROMATIC components |

- "Two power connectors (Xo, Xu) located at the rear provide both the connection to the primary power pack supply, as well as the connection of outputs A1-A6. Output A7 is located on a separate connector X1 at the front of the power pack."
- "Output A6 for supplying the measuring systems can be buffered when the power pack is switched off - the external accumulator necessary for this can be connected via connector X2 at the front of the module."
- One LED per output; LED extinguishes on voltage failure.
- "The power pack also accommodates a POK relay output with attendant LED display, identifying the status of the SYSTEM - RESET line of the VME bus system. The connection of this relay contact is located at the front connector X2, thus providing an externally usable power fail signal."
- "Thermal monitoring is performed by a thermoswitch, the N/C contact of which can similarly be connected to front connector X2."
- "Mains voltage monitoring ensures early detection of a failure or undervoltage of the mains power supply and indication by way of the error signal ACFAIL on the rear power supply connector."

## PDF page 14 (Page 7-14): power pack block diagram

Rear connector **X0, X4** [as printed; text says Xo/Xu] signals: PE, 230V (L, N), ACFAIL, +5,15V, +12,0V, -12,0V, +15,0V, -15,0V [the last sign reads "-"], 0V, +5,2 a (A6, buffered measuring-system supply), 0V a, POK*.

Internals: "Control circuit" with inputs PE, L, N and outputs ACFAIL, A1...A6, A7, 0V; a diode from the accumulator input into the A6 line (buffering); "PON generation" block fed by A1 and ACFAIL; relay A1 [uncertain, relay coil label] with contacts; thermoswitch "Th".

Front connectors:
| Connector | Pin signals |
|---|---|
| LEDs | POK, A1, A2, A3, A4, A5, A6, A7 |
| X1 | +7,5V, 0V (ELTROMATIC supply) |
| X2 | E6 (accumulator input for A6), 0Va, PON, PT, POK', TH (relay contact PT/POK', thermoswitch contact TH) [pin numbers not printed] |

## PDF page 15 (Page 7-15): power failure and undervoltage

| Item | Description |
|---|---|
| Power failure signal (ACFAIL) | TTL level, open collector, active high. "The ACFAIL signal generated by the power pack acts inversely to the VME bus signal ACFAIL*. Thus, for adaptation an inverter is required on the bus circuit board." |
| Identification of undervoltage | at < (230 V - 18 %). "ACFAIL switches to high level at a primary supply of < 230V -18%. If the mains voltage drops temporarily below this limit value, a minimum signal duration of 1 ms is guaranteed." |
| Identification of power failure | timing diagram 230 V / 230 V -10 %, ACFAIL, AUSGG. (Ausgang = output) with times tü and tP |
| Bridging time | tü ≥ 10 ms at 230 V - 10 % |
| Buffer time | tP ≥ 4 ms following a power failure signal |

## PDF page 16 (Page 7-16): VSNT350 front connectors X1, X2

**Front connector X1**: "The voltage of output 7 is supplied at this connector for the supply of the ELTROMATIC components. A screw/plug-in design 2-pole socket strip is provided as a connection element. The voltage of output 7 can be set at one of the potentiometers located at the front. The potentiometer is not accessible when the front panel is mounted (intended exclusively for setting by the manufacturer)."

| X1 pin | Voltage |
|---|---|
| 1 | +7.5 V, output 7 |
| 2 | 0 V, output 7 |

"Ensure that the supply lines to the ELTROMATIC components are provided with sufficient cross section and screening!"

**Front connector X2**: multi-input connection for an external accumulator unit (buffering output 6), the POK relay contact and the thermoswitch. Screw/plug-in 6-pole socket strip.

| X2 pin | Voltage / Signal | Meaning |
|---|---|---|
| 1 | E6 | input to A6 |
| 2 | 0 VG | 0 V to E6/A6 |
| 3 | PON | control signal accumulator unit |
| 4 | PT | common POK/TH connection |
| 5 | POK | relay contact |
| 6 | TH | thermal contact |

**Input for buffering output 6**: input voltage ≈ 5.6 V, input current ≤ 5.0 A. "The power pack is equipped with a longitudinal diode to ensure correct polarity and for decoupling the external accumulator unit from the internal controller. The voltage drop produced there requires an increased input voltage to also ensure sufficient supply to the measuring system in buffer mode."

"The PON control signal is provided to disconnect the output of an accumulator unit from the power consumer during mains operation. It switches to low level (power off) approx. 100 ms after the mains voltage is switched on and switches to high level (power on) at the same time as the ACFAIL signal when switching off the mains voltage. The signal is generated by an open collector driver ..."

## PDF page 17 (Page 7-17): PON ratings, POK relay, thermoswitch, displays

PON output: low ≤ 0.4 V, high ≤ 24 V, output current ≤ 100 mA. "Ensure that the connection lines to the accumulator unit are provided with sufficient cross section and screening!"

Information on buffer mode, to assess the accumulator capacity observe: number of measuring systems to be buffered; current requirement of the measuring systems (manufacturer and type); current requirement of the axis modules in buffer mode; buffer time.

**POK relay contact**: "In EP90 systems the power pack input POK* is controlled at supply connector Xu by the system reset of the VME bus (SYSRES*). However, both signals display inverse behaviour, an inverter being required on the bus circuit board for adaption."

| Relay contact | LED | Power pack: POK* | VME bus: SYSRES* |
|---|---|---|---|
| open | off | high level | high level |
| closed | on | low level | low level |

[as printed; the table shows the same level in both condition columns despite the text "inverse behaviour"]. Switching data: 48 V / 200 mA.

**Thermoswitch**: switching threshold 65 °C, switching data 48 V / 200 mA. "The contact opens in the event of excess temperature."

**Displays**: one LED per output voltage, driven directly from the output, shows only "voltage present / no voltage". "Undervoltage values cannot be identified."

## PDF page 18 (Page 7-18): VSNT350 module front

Front panel drawing labelled **VSNT350**: LEDs POK, A1, A2, A3, A4, A5, A6, A7 (top to bottom); X1 (2-pole, pin 1 at top); X2 (6-pole, pin 1 at top).

## PDF page 19 (Page 7-19): 7.3 VHD/40 hard disk module

"The concept of the EP90.0/1/2 control system versions requires a fixed disc intended for integration in the attendant chassis as a slide module. A carrier plate in double European format, accommodating the hard disk, is required for the slide-in feature. The carrier plate is designed as a printed circuit board bearing a P1 connector on the reverse side (top row of connectors of the VME bus) assuring the necessary supply voltages to be directly admitted from the bus circuit board to the hard disk module. The SCSI bus is connected via a 50-pole ribbon cable which is attached to the hard disk and routed out at the front of the module. The entire unit is equipped with a front panel, width 8 TE ... The EP90.0 chassis accommodates the printed circuit board slot of this module immediately next to the attendant CPU (VD030), producing an extremely short and easy to connect SCSI connection line." (Some letters of the scan are blotted, text restored from context.)

Block diagram: "not assigned".

Technical features ("currently, selected type from QUANTUM"):
- Hard disk with SCSI bus connection
- Data transfer rate (SCSI): 2 MByte/s
- Memory capacity: 42 MByte
- Average access time: 19 ms
- Ambient temperature: 0-50 °C (printed "0-50 $C")
- Supply voltages: +5 V & +12 V

## PDF page 20 (Page 7-20): VHD/40 VME connection

"The supply voltages for the hard disk are supplied by the VME bus which is connected via the top connector of the VME bus back plane. The 'Daisy Chain' lines are bridged at the printed circuit board slot."

Voltage supply: 4-pole connector to the disk; +5 V and +12 V tapped from the VME bus via P1.

**Connector P1 (VME connection)**: function VHD/40 voltage supply from VME bus; type 96-pole VG blade connector; name P1.

| Pin | Signal | Designation |
|---|---|---|
| 4b | BG0IN | Bus grant in 0 |
| 5b | BG0OUT | Bus grant out 0 |
| 6b | BG1IN | Bus grant in 1 |
| 7b | BG1OUT | Bus grant out 1 |
| 8b | BG2IN | Bus grant in 2 |
| 9a | GND | Ground |
| 9b | BG2OUT | Bus grant out 2 |
| 9c | GND | Ground |
| 10b | BG3IN | Bus grant in 3 |
| 11a | GND | Ground |
| 11b | BG3OUT | Bus grant out 3 |
| 15a | GND | Ground |
| 17a | GND | Ground |
| 19a | GND | Ground |
| 20b | GND | Ground |
| 21a | IACKIN | Interrupt acknowledge in |
| 22a | IACKOUT | Interrupt acknowledge out |
| 23b | GND | Ground |
| 31c | +12V | Voltage supply +12 V |
| 32a | +5V | Voltage supply +5 V |
| 32b | +5V | Voltage supply +5 V |
| 32c | +5V | Voltage supply +5 V |

"The 'Daisy chain' lines of the VME bus are closed via the P1 connector. Thus these lines do not need to be closed via jumpers on the back plane at this point."

## PDF page 21 (Page 7-21): VHD/40 connector X1 (SCSI)

Function: connection to SCSI bus; type 50-pole socket strip; name X1.

| Pin | Signal | Designation |
|---|---|---|
| 2 | DB (0) | Data bus 0 |
| 4 | DB (1) | Data bus 1 |
| 6 | DB (2) | Data bus 2 |
| 8 | DB (3) | Data bus 3 |
| 10 | DB (4) | Data bus 4 |
| 12 | DB (5) | Data bus 5 |
| 14 | DB (6) | Data bus 6 |
| 16 | DB (7) | Data bus 7 |
| 18 | DB (P) | Data bus parity |
| 20 | GND | Ground |
| 22 | GND | Ground |
| 24 | GND | Ground |
| 26 | (empty) | (empty) |
| 28 | GND | Ground |
| 30 | GND | Ground |
| 32 | -ATN | Attention |
| 34 | GND | Ground |
| 36 | -BSY | Busy |
| 38 | -ACK | Acknowledge |
| 40 | -RST | Reset |
| 42 | -MSG | Message |
| 44 | -SEL | Select |
| 46 | -C/D | Control/data |
| 48 | -REQ | Request |
| 50 | I/O | Input/Output |

"All uneven pins except pin 25 are connected to GND."

## PDF page 22 (Page 7-22): VHD/40 connectors X2/X3 and X4/X5

**Connectors X2 and X3 (voltage supply of the hard disk)**: 4-pole connector to hard disk, solder joint to the PCB, type JST LC-04B.

| X2/X3 pin | Signal |
|---|---|
| 1 | +12V |
| 2 | +12V GND |
| 3 | +5V GND |
| 4 | +5V |

Drawing "Connector top view, Connector X2", pins 1 2 3 4. "The hard disk is supplied with voltage via a plug connection soldered to the printed circuit board on one side."

**Connectors X4 and X5 (LED connection)**: connection for external LED; 2-pole connector to hard disk, solder joint, type JST S2B-EH.

| X4/X5 pin | Signal | Designation |
|---|---|---|
| 1 | DELA | DEL anode [= LED anode] |
| 2 | DELB | DEL cathode |

## PDF page 23 (Page 7-23): VHD/40 displays and settings

- Displays: "A green LED is lit when the drive executes an SCSI command ... not extinguishing until the command is completed."
- Setting elements: none on the PCB; settings only on the hard disk.
- Setting the SCSI address via jumpers A0, A1, A2 (0 = jumper not inserted, 1 = jumper inserted):

| SCSI address | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| A0 | 0 | 1 | 0 | 1 | 0 | 1 | 0 | 1 |
| A1 | 0 | 0 | 1 | 1 | 0 | 0 | 1 | 1 |
| A2 | 0 | 0 | 0 | 0 | 1 | 1 | 1 | 1 |

"Attention: Only applies to Quantum hard disk type ELS42S and LPS52S."

SCSI bus connection: "the terminating resistors may only be at the leading edge and the spatial end of the SCSI bus. The terminating resistors must be removed from the devices lying in between."

## PDF page 24 (Page 7-24): VHD/40 module front

Front panel drawing: LED at top; "FB cable" (Flachbandkabel = ribbon cable) exits at the front, "to VD030".

## PDF page 25 (Page 7-25): 7.4 VD030 processor card with VGI graphics card / 7.4.1 VD030 processor card

"The VD030 processor card serves to implement those 68000 CPU operating systems which use a VME bus. ..." Characteristics:
- 68030 CPU with MMU or 68EC030 without MMU (25 MHz)
- 4, 8 or 16 MB RAM with triple port (VME bus, LEB and CPU)
- a 32-pin EPROM base for 128 KB and more byte-wide EPROMs
- optional floating point unit (FPU): MC 68882 co-processor (IEEE 32 bit and 64 bit)
- VME bus interface via the bus interface controller (VIC) as master/slave interface; programmable base addresses; data transfer modes 8, 16, 32 bit, READ MODIFY WRITE and block transfer in master and slave mode; block transfer approx. 20 MB/s via the VME bus
- VME bus system controller with SYSCLK generator and SYSRES generation with bus arbitration logics
- VME bus interrupt handlers and interrupter (software programmable)
- A 2 KB battery buffered RAM for system dependent parameters or other essential data.

## PDF page 26 (Page 7-26): VD030 features (continued), expansion modules

- Battery-buffered REAL TIME CLOCK in the RAM chip (lithium battery; second, minute, hour, month, week day and calendar week).
- Up to 6 programmable 16 bit counter/timers.
- Status display (hexadecimal, software addressable, used as error status display) and reset switch on the front.
- Four serial interfaces: two (20 mA) on front connector X1; two more as TTL signals on an internal connector (level conversion on a separate board).
- SCSI interface for mass memory, connector X2 on the front.
- Local extension bus (LEB) for child boards, "intelligent peripheral interfaces (IPIN's)" (ETHERNET, Cheapernet, serial I/O, parallel I/O and other tasks).
- The processor board occupies one slot (4 TE).

**Expansion modules**: graphics interface (VGI) with VGA compatible video interface, monitor port on front connector X3. "In addition, an SIOADP interface adaptor can be used which converts the two serial (TTL) channels into V28 and 20mA signals, offering a separate connector (X3, X4) [as printed; elsewhere X4, X5] for each interface. The processor board with expansion modules occupies two printed circuit board slots (8TE) in a VME system (however only one bus connector)."

## PDF page 27 (Page 7-27): Block diagram VD030

"Block diagram of the processor board" (small, partly blurred print). Blocks: FPU; CPU 68EC030; internal arbiter / main address decoding [uncertain]; DRAM cont.; 4 MByte DRAM block transfer; DRAM extens.; semaphore register; address decoding & control; timer status & control; VIC (gate array, system controller, 4-level arbiter, VMEbus local-irq-handler, interrupter, location monitors); slave address decoder; 32 bit address bus; 32 bit data bus; "1 socket 32-pin up to 8 Mbit PROM"; "battery backup 2K SRAM real time clock"; SCSI-interface with 64 KByte data buffer; status & control register; timer 8536; SCC1 8530, SCC2 8530 with "20mA" [uncertain] interface; front panel elements: SCSI-conn., display, reset, hex-switch, 9-pin SUB-D, 20-pin connect.; local ext. bus; VME connectors P1, P2. Exact labels partly [unreadable] due to the small font.

## PDF page 28 (Page 7-28): VD030 connectors

**Connector X1**: "two serial interfaces for active 20mA operation. Data for transmission and receipt is available only, no control signals. These interfaces are suitable, e.g. for connecting control panel components (e.g. BST BTF90)." Type: 9-pole SUB-D, jack.

| X1 pin | Signal |
|---|---|
| 1 | TD1+ |
| 2 | TD1- |
| 3 | RD1+ |
| 4 | RD1- |
| 5 | GND |
| 6 | TD2+ |
| 7 | TD2- |
| 8 | RD2+ |
| 9 | RD2- |

**Connector X2**: 50-pole SCSI connector, ribbon cable to the adjacent hard disk module (VHD).

**Connector X3**: on the graphics IPIN (VGI) for a VGA monitor: analog RGB (incl. SYNC) and TTL-SYNC signals HSYNC & VSYNC (see "Graphics IPN VGI").

**Connectors X4 and X5**: on the SIOADP interface adaptor (ribbon cable to the basic board) for serial periphery; "X4 & S5" [as printed, = X5] provide one serial interface each for V28 and 20 mA operation (see "SIOADP interface adaptor").

## PDF page 29 (Page 7-29): RESET switch, STATUS display / 7.4.2 VGI graphics card

- RESET switch: generates RESET of the CPU module and/or the system (if SYSTEM-CONTROLLER active), recessed jack contact. LED adjacent: on = RESET, off = not RESET.
- STATUS display: software controlled hexadecimal display element.

**7.4.2 VGI graphics card**: piggy-back IPIN on the LEB; in the EP90 programmed as VGA card, 640 × 480 pixels. Graphics controller HITACHI ACRTC 63484 (sync generation and refresh only). 512 kByte VRAM via the 16 bit LEB data bus, organised 1024 × 512 × 4 pixels (16 colours/grey levels). CLUT generates analog R, G, B; max. 16 colours out of a palette of 256k; all three colour signals contain the composite sync; signals conform to RS-343A.

## PDF page 30 (Page 7-30): VGI block diagram

Crystal controlled oscillator 25,175 MHz -> CLK; divider ":8" ("Teiler" = divider) -> graphics controller ACRTC 63484 -> buffer -> /HSYNC, /VSYNC. LEB lines D16-31, A1, A1-9, A10-18 -> address latches ROW/COL (MAD0-8, MAD9-17); four VRAM chips (video RAM) on D16-19, D20-23, D24-27, D28-31; serial data -> colour pallet CLUT (D0-7, A1-3, CLK) -> Red, Green, Blue outputs.

## PDF page 31 (Page 7-31): VGI technical data and module architecture

- HITACHI 63484 graphic controller for sync and refresh generation
- VGA timing programming
- 640 × 480 pixel resolution
- 16 colours simultaneously from a pallet of 256k with CLUT
- Matrix memory with 512 kByte VRAM organised in 1 MPixel × 4 matrix dots
- Matrix memory access only possible by CPU
- 32 bit LEB connection as IPIN
- Matrix dot clock (pixel clock) = 25.175 MHz
- HSYNC, VSYNC with TTL level and active low

## PDF page 32 (Page 7-32): Electrical characteristic values of the video interface

- R, G, B generated by the CLUT BT471/478 to RS-343A, each terminated on the card with 75 Ω; outputs can drive a load of 37.5 Ω.
- Output voltage: 1 Vss; 0.7 Vss video signal, 0.3 Vss sync on all three video signals.
- HSYNC and VSYNC also output separately as TTL signals: active low, TTL level; output current 12 mA high, 24 mA low.
- Connector type: "see 3.3.4.3.2 below" [cross-reference as printed].

**Programmable timing for VGA mode**:

| Item | Value |
|---|---|
| H frequency | 31,47 kHz |
| HSW (h-sync width) | 3,813 µs |
| HDS (h-display start) | 1,907 µs |
| HDW (h-display width) | 25,422 µs |
| HC (h-cycle) | 31,778 µs |
| V frequency | 59,94 Hz |
| VSW | 2 Z (lines) = 63,56 µs |
| VDS | 32 Z = 1,02 ms [uncertain digits] |
| VDW | 480 Z = 12,253 ms [uncertain] |
| VC | 525 Z = 16,683 ms |
| Pixel clock | 25.175 MHz |
| R, G, B levels | 0 V (sync tip), 0,3 V (black), 1,0 V (peak white) |

## PDF page 33 (Page 7-33): VGI connecting conditions and technical data

15-pole SUB-D jack for the monitor; "Use coaxial cable for connecting the monitor".

| Pin | Signal | Pin | Signal |
|---|---|---|---|
| 1 | video - red | 9 | NC |
| 2 | video - green | 10 | GND -VSYNC |
| 3 | video - blue | 11 | NC |
| 4 | NC | 12 | NC |
| 5 | GND - red | 13 | /HSYNC |
| 6 | GND - green | 14 | /VSYNC |
| 7 | GND - blue | 15 | NC |
| 8 | GND -HSYNC | | |

(This is the standard VGA DE-15 pinout; the operator-panel monitor is a standard 31.5 kHz VGA monitor.)

Technical data (VD030 processor module): supply +5 V ±5 % ≤ 8 A; +12 V ±5 % ≤ 200 mA; -12 V ±5 % ≤ 200 mA. Ambient temperature 0...55 °C* (with ventilator cooling). PCB double European format 233.5 mm × 160 mm; front panel 8 TE. "* Attention: This data refers exclusively to the individual components PROCESSOR MODULE VD030 described in these instructions! It does not provide operating values for entire control systems or subassemblies! ..."

## PDF page 34 (Page 7-34): VD030 front of module

| Front element | Meaning |
|---|---|
| Status display | hex status display |
| BCD RESET switch | [as printed: BCD (switch) and RESET switch] |
| X1 | 2 serial interfaces (20 mA, 9-pole SUB-D) |
| X2 | SCSI bus connection |
| X3 | Video interface of the graphics IPIN |
| X4 | 1 serial interface of the SIO adaptor |
| X5 | 1 serial interface of the SIO adaptor |

## PDF page 35 (Page 7-35): 7.5 VK030 processor card with SI188 PLC card / 7.5.1 VK030 processor card

"The VK030 processor card serves to implement 68000 CPU operating systems which use a VME bus. ... single board solution with EPROM memory." Characteristics:
- 68030 CPU with MMU or 68EC030 without MMU
- 1 or 4 MB DRAM with triple port (VME bus, LEB and CPU)
- four 32 pin EPROM bases for 128 KB and more byte-wide EPROMs; memory expansion by plug-in sub-print with four additional EPROM sockets
- optional FPU MC 68882
- VME bus interface via VIC (master/slave); external base address set by the BCD switch; 8/16/32 bit, READ MODIFY WRITE, block transfer approx. 20 MB/s
- VME bus system controller with SYSCLK generator and SYSRES generation with bus arbitration logics
- VME bus interrupt handlers and interrupter
- 2 KB battery buffered RAM; battery-buffered REAL TIME CLOCK (lithium battery)
- Up to 3 programmable 16 bit counters/timers
- Status display (hex) and reset switch on the front
- Two serial interfaces on a common front connector (X1).

(The SI188 PLC card, 7.5.2 on page 7-38, continues in PDF pages 36-70.)

---

## For the retrofit

What these 35 pages tell (and do not tell) about keeping the Indramat drives under LinuxCNC:

1. **No Indramat type codes in this part.** Pages 1-35 describe only the Eltromatic ELTROPILOT EP90 control rack (BGT10-V chassis, VSNT350 power pack, VHD/40 disk, VD030 + VGI, VK030). Drive types (DDS/TDM/KDV/DAE etc.), plug-in modules, terminal numbers, enable/ready/brake signals and the E-stop chain are not shown here; take them from the drive/cabinet chapters and the drive rating plates.
2. **Command interface is analog, not SERCOS.** The general plans (pages 5-8) show every drive ("Contr."/"Regler") fed from a **VAP4 axis periphery card** connector "Anal. I/O's" (X1 for channels 1&2, X4 for 3&4) and its encoder ("Generator") read back into the same VAP4 (X3 for 1&2, X5 for 3&4). That is the classic **±10 V speed set-point + incremental encoder to the CNC** topology. There is no fibre-optic/SERCOS ring in the drawings. The exact voltage range, encoder signal type (TTL/RS-422 or sine) and resolution are in section 7.7 VAP4 (PDF page ~45 onwards, other half). Check them there before choosing LinuxCNC hardware.
3. **Axis count on the CTX twin (EP90 duplex, pages 7-8):** three VAP4 cards. VAP4(1): X I, Z I, HA 1 (main spindle 1) + separate C-axis 1 encoder. VAP4(2): X II, Z II, HA II + separate C-axis II encoder. VAP4(3): AWZ I and AWZ II (driven tools of turret I and II, "Rev." = revolver), each with an encoder. So **8 analog set-point channels** and **10 encoder channels** (4 feed axes, 2 spindle motor encoders, 2 separate C-axis encoders on the spindles, 2 driven-tool encoders). The 2- and 4-axis plans also show an AWZ drive without an encoder; on the twin both AWZ drives have encoders.
4. **Spindles** (HA I/II) are driven the same way as the axes (analog set-point from VAP4, encoder back). C-axis position comes from a **separate C-axis encoder** on each spindle, not from the spindle motor encoder. For LinuxCNC that means two encoder inputs per spindle (motor speed feedback for the drive and a C-axis encoder for position), and a spindle/C-axis mode switch (in the drive or in the control).
5. **Per-drive cabling:** each drive has cables W11, W12, W13, W14 (W12/W13/W14 vary by axis). From the drawing, W11/W12 run between VAP4, drive and encoder (set-point and encoder) and W13/W14 to the drive controllers; their exact meaning (set-point, encoder, enable/ready) is not printed here [uncertain]. Look them up in the cable lists.
6. **I/O:** the PLC I/O is the Eltromatic **IO48** module chain on the ELTROMATIC bus from VK030 X2 (cables W07, W08; IO48 connectors X11/X12/X13, last module terminated), supplied with **+7.5 V** from VSNT350 X1 via W05. The IO48 is a proprietary bus and cannot be reused under LinuxCNC as it is: replace it with LinuxCNC-compatible I/O (e.g. Mesa 7I84/7I76 24 V I/O) wired to the same field terminals. Digital "measuring circuits" (probe/touch inputs) go to VAP4 X2 "Digit. I/O's".
7. **Power and watchdog signals that could be reused:** VSNT350 X2 provides a potential-free **POK relay contact** (pins 4 PT / 5 POK, 48 V/200 mA, closed = system OK, follows VME SYSRES*) and a **thermoswitch N/C contact** (pins 4 PT / 6 TH, opens at 65 °C). If the cabinet uses the POK contact in the drive-enable or E-stop chain, a LinuxCNC watchdog output (e.g. Mesa watchdog-driven relay) must take its place when the EP90 rack is removed. Check the cabinet diagrams for wiring to VSNT350 X2.
8. **Measuring-system supply:** VSNT350 output A6 (+5 V, battery-bufferable via X2 E6) supplies the "external measuring systems" (encoders) via the VAP cards. If the VAP4/EP90 rack is removed, the encoders need a new +5 V supply (from the LinuxCNC encoder card or a separate PSU). Check the encoders for battery-buffered absolute/reference data (accumulator "Accu" on the rack front buffers A6). If a measuring system relies on that buffer, it must stay powered.
9. **Operator panel:** the monitor is a standard VGA monitor (640×480, 31.47 kHz/59.94 Hz, DE-15 pinout page 33) and can be reused on a PC. The BF90/BTF90 keyboard panel talks to the VD030 over **20 mA current-loop serial** (VD030 X1, 9-pole SUB-D: TD1±, RD1±, TD2±, RD2±). Reusing it needs a 20 mA-loop to RS-232/USB converter and a protocol decoder (not documented here), so the more practical option is to rewire the panel keys/handwheel to new I/O.
10. **Replacing the drives instead:** the interface to the drives would be the same ±10 V + encoder set (8 channels/10 encoders, e.g. Mesa 7I77 ×2 or 7I97 ×2/7I77 + 7I85). New drives would also need their own enable/ready/brake and E-stop wiring designed from the cabinet chapters. Nothing in these 35 pages fixes motor or encoder data.

To check in PDF pages 36-70 (other half): VAP4 analog output range and pinout of X1/X3/X4/X5 (encoder type/resolution, reference mark, drive-enable/ready contacts if they are on the VAP4), IO48 pinout and the voltage level of the I/O.
