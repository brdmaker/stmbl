# Gildemeister CTX twin, electrical documentation: 3. Electronic modules (Electronic subassemblies, cooling unit), PDF pages 1-12

Source: `3_Electronic-subassemblies_Cooling_unit_20201028_0001.pdf` (scan dated 2020-10-28), "Electrical technical instructions CTX twin", Dok.-No.: DDAAE0G1, February 1995, printed pages 3-1 to 3-11 plus one blank page.
Original language: **English** (Gildemeister's own translation; no German text on these pages, so no translations are needed). Third-party vendor text (DINA Elektronik) is also in English.

This chapter is a descriptive text chapter, not a set of circuit-diagram sheets. It has no drawing numbers or title blocks. The page numbers below are the printed page numbers.

| PDF page | Printed page | Content |
|---|---|---|
| 1 | 3-1 | Table of contents, chapter 3 "Electronic modules" |
| 2 | 3-2 | 3.1 General (module construction) |
| 3 | 3-3 | 3.2 HM4 manual inching module: function description, connection assignment |
| 4 | 3-4 | 3.2.3 HM4 circuit per channel, 3.2.4 HM4 board layout (terminal plan) |
| 5 | 3-5 | 3.3 Rotational speed monitoring unit (DINA DNDS I / DNIA / DNCO), connection picture, function description |
| 6 | 3-6 | DNIA / DNDS dimension drawings, technical data, DNIA SUB-D 15 encoder pinouts (Sinumerik 800, Boehringer) |
| 7 | 3-7 | 3.4 Safety relay modules SRBF: features, application text |
| 8 | 3-8 | SRBF circuitry and terminal level (SRBF-1122 block diagram) |
| 9 | 3-9 | 3.5 Service instructions for cooling unit SK150E1V, 3.5.1 General (setpoints, alarm contacts) |
| 10 | 3-10 | 3.5.2 Ventilator, 3.5.3 Compressor, 3.5.4 Series transformer, 3.5.5 Maintenance, condensed water |
| 11 | 3-11 | 3.5.6 Malfunctions (NC fault and warning numbers 1200-1205) |
| 12 | - | Blank page (only show-through from page 11) |

---

## PDF page 1 - printed page 3-1: table of contents

Header: "3. Electronic modules"

| No. | Title | Page |
|---|---|---|
| 3.1 | General | 2 |
| 3.2 | HM4 - Manual inching module with 4 safety relays | 3 |
| 3.2.1 | Function description | 3 |
| 3.2.2 | Connection assignment | 3 |
| 3.2.3 | HM4 functions per channel (in total 4 channels) | 4 |
| 3.2.4 | Manual inching module assignment plan HM4 (4 fold) | 4 |
| 3.3 | Speed monitoring (Ident. No. 1047215 + 1047217) | 5 |
| 3.4 | Safety relay module SRBF | 7 |
| 3.5 | Service instructions for cooling unit SK150E1V | 9 |
| 3.5.1 | General | 9 |
| 3.5.2 | Ventilator | 10 |
| 3.5.3 | Compressor | 10 |
| 3.5.4 | Series transformer | 10 |
| 3.5.5 | Maintenance, condensed water | 10 |
| 3.5.6 | Malfunctions | 11 |

Footer (all pages): "Electrical technical instructions CTX twin - Dok.-No.: DDAAE0G1 - February 1995 - page 3-n".

## PDF page 2 - printed page 3-2: 3.1 General

- All modules consist of a printed circuit board with photo-imageable solder mask and position print.
- The modules connect to the machine with screw terminal blocks. The connection to the NC uses subminiature D plugs. All module connections are marked as in the circuit diagram.
- Each module is marked with its module name and identification number.

## PDF page 3 - printed page 3-3: 3.2 HM4 - Manual inching module with 4 safety relays

### 3.2.1 Function description

- In manual mode with the hood (guard) open, machine functions (steady rest, chuck, tailstock etc.) can be selected through the HM4. They are selected on the operating field or with the foot switch and engaged with the manual inching module.
- Each relay is switched by **two switches, decoupled by diodes**.
- **The module input circuit only gets voltage when the hood is open.**
- The operating elements switch the relays, and an input is made at the machine NC at the same time. A function is only triggered when **the HM4 relay has switched AND the NC has generated a release signal**. So the machine functions are monitored in hardware and in software.

### 3.2.2 Connection assignment

Inputs:

| Relay | Terminals |
|---|---|
| K1 | X1 / 15, 16, 17, 18 |
| K2 | X1 / 25, 26, 27, 28 |
| K3 | X1 / 35, 36, 37, 38 |
| K4 | X1 / 45, 46, 47, 48 |

Outputs:

| Relay | N/C contact | N/O contact |
|---|---|---|
| K1 | X1 / 11, 12 (1 N/C) | X1 / 13, 14 (1 N/O) |
| K2 | X1 / 21, 22 (1 N/C) | X1 / 23, 24 (1 N/O) |
| K3 | X1 / 31, 32 (1 N/C) | X1 / 33, 34 (1 N/O) |
| K4 | X1 / 41, 42 (1 N/C) | X1 / 43, 44 (1 N/O) |

Earth connection: X2 / 1, 2.

Note: Dold relays with positively guided contacts are used. All relays are soldered directly to the board, without sockets.

## PDF page 4 - printed page 3-4

### 3.2.3 HM4 function per channel (in total 4 channels)

Circuit description (one channel; Kn = K1...K4):

- Two input switch paths feed the relay coil. Path A: terminals X1/n5 and X1/n6 (K1: X1/15, X1/16; K2: X1/25, X1/26; K3: X1/35 [printed "X1/37", probably a misprint for X1/36, uncertain]; K4: X1/45, X1/46). Path B: terminals X1/n7 and X1/n8 (K1: X1/17, X1/18; K2: X1/27, X1/28; K3: X1/37, X1/38; K4: X1/45 [printed, probably X1/47, uncertain], X1/48).
- In each path the two terminals are bridged together at the top. Each path then goes through two series diodes (decoupling) to a common node that drives the relay coil Kn. The coil's other end goes to X2/1.
- LEDs: H1, H3, H5, H7 (one per channel, with series resistor) indicate path A; H2, H4, H6, H8 (with series resistor) indicate path B. The LED returns go to X2/2.
- There is a further diode across the coil node toward the path-B side (freewheel / decoupling diode, drawn reversed) [exact topology uncertain].
- Relay contacts Kn: N/C contact K1: X1/11 - X1/12, K2: X1/21 - X1/22, K3: X1/31 - X1/32, K4: X1/41 - X1/42. N/O contact K1: X1/13 - X1/14, K2: X1/23 - X1/24, K3: X1/33 - X1/34, K4: X1/43 - X1/44.

Connections (as readable):

| From | To | Note |
|---|---|---|
| X1/n5 + X1/n6 (bridged) | 2 diodes in series -> coil Kn (+) | input path A, LED H1/H3/H5/H7 |
| X1/n7 + X1/n8 (bridged) | 2 diodes in series -> coil Kn (+) | input path B, LED H2/H4/H6/H8 |
| coil Kn (-) | X2/1 | common return |
| LEDs | X2/2 | LED return |

### 3.2.4 Manual inching module configuration plan HM4 (four fold)

Board layout: X2 (terminals 1, 2) at top left; relays K1, K2, K3, K4 side by side; LEDs H1, H2 under K1, H3, H4 under K2, H5, H6 under K3, H7, H8 under K4. Terminal strip X1 in two rows:

| Row | Terminals (left to right) |
|---|---|
| Upper | 13, 14, 11, 12, 23, 24, 21, 22, 33, 34, 31, 32, 43, 44, 41, 42 |
| Lower | 15, 16, 17, 18, 25, 26, 26 [printed twice, should be 27, uncertain], 27 [should be 28, uncertain], 35, 36, 37, 38, 45, 46, 47, 48 |

Note: "Existing printed circuit board connections" (bridges on the board): X1/1,2 - 3,4; X1/5,6 - 7,8; X1/9,10 - 11,12; X1/13,14 - 15,16. [These numbers do not match the terminal numbering above. They are printed as shown, probably meaning the positional pins of the strip, uncertain.]

## PDF page 5 - printed page 3-5: 3.3 Rotational speed monitoring unit (Ident. No. 1047215 + 1047217)

Vendor stamp: DINA Elektronik, Beethovenstr. 45, 72649 Wolfschlugen, Tel. 07022 55291, Fax 07022 52413.

Connection picture (description):

- **Shaft encoder** -> SUB-D 15 pole connection (cable type DNKA) -> upper SUB-D on the **DNIA** (encoder interface). The second SUB-D on the DNIA is "for further processing in the machine controls" (loops the encoder signals on to the NC).
- DNIA terminals: top row A1, ..., IA1, IA2; bottom row IA1, IA2. The DNIA outputs IA1 / IA2 are wired to the **DNDS I** inputs IN1 / IN2 (outputs offset by 180 deg). DNIA A1 is wired to DNDS I A1 (supply) [uncertain].
- **DNDS I** (type label "Typ: DNDS I"), 3 LEDs on the front (labels approx. "Stillstand" (standstill), "A1/A2", "Drehzahl" (speed) [partly unreadable]). Top terminal row: 11, 12, 13, 14, A1 (24V), IN1, F1, F2, F3, F4. Bottom terminal row: D4, D3, D2, D1, IN2, A2, 24, 23, 22, 21.
- **DNCO** (programming plug-in, "DNCO connection terminals" F1, F2, F3, A1, 24V) sets the speed codes on D1-D4 / F1-F4. "Programming via DNCO or hard wiring".
- (3) **Deletion key** (Löschtaste): N/C pushbutton in the +24 V line, deletes stored faults.
- (4) **Door limit switch** "Door open / closed": switches over to 50 rpm when the door is open (wired to the F/D programming inputs).
- Bottom terminals: 24V, (2) programming wiring, 0V; the contacts 21-24 go to the **EMERGENCY STOP** circuit.
- (1) Screws for disassembling the clamping blocks when changing the unit.

Certificates: Technical committee for iron and metal II/Mainz, SUVA/Switzerland.

Function text (as printed):

- **Application 4:** Connection scheme DNDS I, DNIA and DNCO with shaft encoder to monitor an axis or spindle on machine tools (automatic lathes, milling machines, machining centres etc.). DNDS I is a 2-channel monitoring unit. Each channel has one standstill and one speed monitoring unit. DNDS I has 16 programmable max. rotational speeds (speed programming). Each speed can be reduced to 25% in steps of 5% (chuck program). **If no chuck size is set, the max. speed is 50 rpm (open door).** Use shielded lines for long programming connections D1-D4, F1-F4.
- **Standstill function:** fixed at 2 Hz. The 1st pulse at IN1 or IN2 opens contact 13-14 and closes 11-12. This state remains while the frequency is > 1.2 Hz. Top LED off. IN1 and IN2 pulses are offset by 180 deg.
- **Rotational speed function:** When the speed set via D1-D4 and F1-F4 is exceeded, contacts 13-14 and 23-24 open, 21-22 close. Bottom LED off. Normal state returns when the speed falls approx. 10% below the set speed.
- **Input fault a):** Top LED flashes = defect at IN1; bottom LED flashes = defect at IN2. Contacts 13-14 and 23-24 open, 11-12 and 21-22 closed. Fault is stored in DNDS I. Delete by interrupting the operating voltage for > 2 s with a normally closed key in series with +24 V. Causes: wire break, defect in one DNIA channel or one encoder track.
- **Input fault b):** Top and bottom LEDs off. Contacts 13-14, 23-24 open, 11-12, 21-22 closed. Causes: no signals on IN1 and IN2; line break between shaft encoder and DNIA, machine control and DNIA, or DNIA and DNDS I; soiling signal on pin 15 from DNIA missing; no operating voltage at the shaft encoder and DNIA.
- **DNDS I fault:** Failure of one of the two channels. Top and bottom LEDs off. Contacts 13-14 and 23-24 open, 11-12 and 21-22 closed. Fault is stored; delete as above.
- **DNIA function:** Adapts to the shaft encoder pulses for DNDS I and makes one signal for each of the two encoder tracks, on IA1 and IA2, offset 180 deg. DNIA monitors the encoder's soiling signal (Uas) and the track outputs for high impedance. If these fail, IA1 and IA2 have no potential. DNIA is matched to the SUB-D assignment and is linked to DNDS I by optocoupler. All DNIA settings are internal. DNIA suppresses low speeds:
  - Spindle monitoring: encoder 1024 imp./rev. < 16 rpm; encoder 4096 imp./rev. < 4 rpm.
  - Axis monitoring: encoder 2500 imp./rev. and 10 mm/rev. pitch: < 100 mm/min.

DNDS I contact summary (my summary of the text above; the normal states "standstill 13-14 closed" and "below limit 23-24 closed / 21-22 open" are inferred as the opposite of the stated switched states [uncertain]):

| State | 13-14 | 23-24 | 11-12 | 21-22 |
|---|---|---|---|---|
| Standstill (< 1.2 Hz) | closed | - | open | - |
| Moving (1st pulse) | open | - | closed | - |
| Speed below limit | - | closed | - | open |
| Speed above limit | open | open | - | closed |
| Input fault a/b, unit fault | open | open | closed | closed |

## PDF page 6 - printed page 3-6: DNIA / DNDS dimensions and technical data

Dimension drawings: DNIA 45 x 73 mm front, depth 120 mm; DNDS I / DNDS II 100 x 73 mm front, depth 120 mm, rail DIN EN 50022-35 (height 70 incl. rail [uncertain], 16 mm offset). "The 4 external screws are for fastening the terminal block."

Attention: Units are for mounting in the switch cabinet only, no external installation. Wrong connection or faults in the encoder circuit can cause hazards. Tampering with these units can endanger life and limb.

Technical data DNDS I, DNDS II:

| Item | Value |
|---|---|
| Operating voltage UB | 24 V DC |
| Power consumption | 10 W |
| Voltage range | 0.85-1.1 UB |
| Duty cycle | 100 % |
| Ambient temperature | -25 C to +70 C |
| Temperature dependency of switching point | 0.02 %/C |
| Voltage influence on switching point | none |
| Repeating accuracy | +/- 0.1 % |
| Switching point setting | by applying 24 V to the binary inputs |
| Contact material of output relays | hard silver |
| Contact type | positively guided contacts |
| Switching voltage, max. | 250 V AC, 250 V DC |
| Constant current | 6 A AC, 1 A DC |
| Short-circuit fuse | 6 A delay action |
| Climate resistance | DIN 40040 class F |
| Vibration resistance | 3 g at all levels 32 Hz |
| Test voltage | 2500 V, 50 Hz |
| Specification | VDE 0345 |
| Operating position | as desired |
| Creepage / air gaps | VDE 110 C, 250 V |
| Protection terminals | IP 20 (DIN 40050) |
| Protection housing | IP 50 (DIN 40050) |
| Conductor connection | 2 x 2.5 mm2, solid |
| Fastening | DIN 46121 + DIN 46277, DIN-EN 50022 |
| Connection designation | DIN 46199 |
| Weight | approx. 500 g |

Technical data DNIA:

| Item | Value |
|---|---|
| Operating voltage U | 5 V +/- 0.2 V DC = UB of encoder |
| Power consumption | approx. 100 mA |
| Duty cycle | 100 % |
| Output current | 10 mA |
| Ambient temperature | -25 C to +70 C |
| Climatic resistance | DIN 40040 class F |
| Vibration resistance | 3 g at all 3 levels 32 Hz |
| Weight | approx. 200 g |
| Connection to DNDS I, II | terminal 2 x 2.5 mm |
| Connection for encoder | SUB-D 15 pole |

DNIA SUB-D 15 pinouts (all pins of plug and socket are linked 1 to 1; * = inverted; *Uas = soiling signal):

| Pin | Sinumerik 800 | Boehringer |
|---|---|---|
| 1 | Ua1 | Ua2 |
| 2 | - | *Ua1 |
| 3 | *Ua2 | 0V |
| 9 | *Ua1 | Ua1 |
| 10 | Ua2 | - |
| 11 | 0V | *Ua2 |
| 14 | 5V | 5V |
| 15 | *Uas | *Uas |

## PDF page 7 - printed page 3-7: 3.4 Safety relay modules SRBF

Features ("Special features in keywords"):

- 36 mm width in the switch cabinet
- Top-hat rail mounting, vertical or horizontal
- Connections via plug receptacle (plug-in terminals)
- 4 safety releases / 1 return signal path
- Drop-out delayed safety release 0...220 seconds, infinitely variable
- Contact multiplication plug-in / cascadable
- Programmable function selection
- Redundant switching to prEN 954-1:1992, categories 3 and 4 (* see appendix 1, pg. 24)

Application text (summary): Safety circuits process signals from EMERGENCY STOP devices, interlocks for movable guards with and without tumbler, and similar devices against dangerous movements. A single fault must not lead to loss of the protective function. Safety relay components replace discretely wired auxiliary contactors and reduce space, wiring effort and wiring errors. The SRBG [sic, printed "SRBG"] new generation adds many releases, drop-out delayed releases, short- and cross-circuit monitoring of supply lines and programming options.

## PDF page 8 - printed page 3-8: SRBF circuitry and connection level

Text: SRBF modules have 5 safety relays forming a self-monitoring circuit of safety category 4 (prEN 954-1:1992). 2 x 2 relays give the 4 safety releases redundantly; the 5th relay is a fail-safe comparator that tests the circuit for faults before the next switch-on. The relays use the rotary armature principle with fully positively guided internal contacts and restricted guidance of the external contact pairs, so that even if one release contact welds, the other releases keep working and a stop and restart barrier are still guaranteed.

Block diagram SRBF-1122 (terminals, top to bottom, with right-hand labels):

| Terminal | Function |
|---|---|
| PE (earth screw) | earth |
| C | +24 V DC |
| D | 0 V |
| C1 | +24 V |
| C1.1 | +24 (monitoring U<) |
| C1.1 | +24 (monitoring U<) |
| D1 | 0 V |
| S1.1 | + signal input |
| S1.2 | + signal input |
| C1.1 [printed "C1.1"/"C1.2", uncertain] | +24 (monitoring U<) |
| S1.3 | + signal input |
| S1.4 | + signal input |
| S2.1 | + signal input |
| S4.1 | optional +/- signal input, not assigned |
| 13-14 | release path 1 (N/O, series contacts K3, K2, K1 [uncertain]) |
| 23-24 | release path 2 |
| 33-34 | release path 3 |
| 43-44 | release path 4 (contacts K5, K4) |
| L13 | +24 V DC |
| L14 | output PNP switching (driven from K2/K3 via optocoupler) |
| J1 | 0 V |

Annotation next to release path 2/3: "Umax 230 V ... Imax 6 A ohmic*" [partly unreadable]; "* Ohmic load".

Internal: U< undervoltage monitor and a switch element between C/D and C1; start/monitoring chain S1.1-S1.2 through N/C contacts of K5, K4, K3, K2 (feedback chain); K1 is the comparator relay with coil on S1.3/S1.4; coils K2, K3, K4, K5 are driven from S2.1/S4.1 via K1 contacts.

## PDF page 9 - printed page 3-9: 3.5 Service instructions for cooling unit SK150E1V

### 3.5.1 General

- Switch cabinet cooling unit, refrigerant **R134a** (CFC-free).
- Closed refrigeration circuit, components: 1 refrigeration compressor (fully hermetic); 2 condenser (air or water cooled); 3 ventilator (air cooled units only); 4 refrigerant drier; 5 capillary tube injection; 6 evaporator; 7 temperature control electronics. Refrigerant flows in the order 1-6.
- Soldered and screwed circuit: no maintenance except keeping the air-cooled condenser and the air filter mat at the air inlet clean. Do not loosen screw fittings (pressurised; loss of refrigerant stops the unit).
- Control electronics provide:
  - Suppression elements for spark quenching of the relays.
  - **Compressor on at 38 C** (IMPORTANT: switch-on delay approx. 3 minutes).
  - **Compressor off at 34 C.**
  - **Potential-free contact "switch cabinet temperature > 46 C"** (see NC fault message No. 1204). Contact switches back on at < 43 C.
  - **Potential-free contact "air inlet temperature > 83 C"** (soiled filter mat, see NC fault message No. 1200). Contact switches back on at < 75 C.
- Always switch off the cooling unit supply for cleaning or maintenance before opening it.

## PDF page 10 - printed page 3-10

### 3.5.2 Ventilator
Runs permanently; can only be switched off by the feed-in (main switch) or by opening the switch cabinet doors (door limit switch). Winding head thermostats trigger at approx. 135 C and switch off the motor; it restarts after cooling.

### 3.5.3 Compressor
Protected against overload by one or more motor protectors (Klixon). Switched on by the control electronics at approx. 38 C, off at approx. 34 C, with a switch-on delay of approx. 3 minutes.

### 3.5.4 Series transformer
Compressors of this size are only made with single-phase AC motors. A series auto-transformer is fitted so the unit runs directly from the **380 V three-phase supply**.

### 3.5.5 Maintenance, condensed water
- Check the filter mat on the air intake side at least once a month, depending on dust; clean when soiled.
- Condensate on the evaporator (especially after the cabinet was open for a long time) is led into the compressor room to evaporate; excess goes outside through a drain nipple and hose through the cabinet bottom. Occasional seepage is normal.
- Cabinet internal temperature should not be more than 5 C below the outside temperature, otherwise condensation forms inside the cabinet.

## PDF page 11 - printed page 3-11: 3.5.6 Malfunctions

| NC message | Text | Cause |
|---|---|---|
| Fault 1202 | Switch cabinet cooling: over current | Motor protection switch **Q321** has tripped |
| Fault 1200 | Switch cabinet cooling: filter blocked | Soiled filter mat (air inlet > 83 C contact) |
| Fault 1204 | Switch cabinet cooling: over temperature | Ambient too hot / unit overloaded; supply voltage too low ("Power supply gone below"); switch cabinet door limit switch not actuated |
| Warning 1201 | filter blocked | shown in automatic mode |
| Warning 1203 | over current | shown in automatic mode |
| Warning 1205 | over temperature | shown in automatic mode |

In automatic mode the warnings 1201/1203/1205 appear first; the part program continues until the next stop, and only then is the machine stopped with the matching fault message.

Ventilator not running: supply interrupted; cabinet door limit switch not actuated; ventilator winding protection tripped; ventilator stiff; ventilator motor defective.
Compressor not running: 38 C not exceeded; 3-minute delay not elapsed; control electronics wrongly set or defective; compressor overloaded or defective.
Compressor noisy: drive unit or valve damaged; lack of oil.
Fix faults in the order listed. Maintenance is limited to cleaning the filter mat and the housing interior.

## PDF page 12
Blank (show-through only).

---

## For the retrofit

Cooling unit SK150E1V (cabinet cooler) - signals the LinuxCNC/PLC side must handle:

| Signal | Type | Meaning | Original reaction |
|---|---|---|---|
| Q321 auxiliary contact | DI | cooler motor protection tripped | Fault 1202 / warning 1203 |
| "Air inlet > 83 C" potential-free contact (resets < 75 C) | DI | filter mat blocked | Fault 1200 / warning 1201 |
| "Cabinet temperature > 46 C" potential-free contact (resets < 43 C) | DI | cabinet over temperature | Fault 1204 / warning 1205 |

- The cooler controls itself (compressor 38 C on / 34 C off, 3 min delay, fan always on). LinuxCNC does not need to drive it; it only needs a supply from 380 V 3-phase via Q321. Contact polarity (N/O or N/C) and the terminal/wire numbers of these three inputs are not in this chapter; take them from the circuit-diagram sheets (Q321 area). Wire them fail-safe (break = alarm) if possible.
- Reproduce the original behaviour: in AUTO, raise a warning and let the program run to the next stop (M0/M30/program end), then block cycle start; in manual, show the fault at once. In HAL: a latched `cabinet-alarm` that feeds `halui.program.pause`/a cycle-start inhibit at the next stop, plus a message on the GUI.
- Door limit switch of the cabinet stops the fan; keep the cabinet closed or the 1204 over-temperature will follow. Keep Indramat drives and the new PC below their ambient limits; a cabinet at > 46 C is already near the limit for the drives.

Other modules in this chapter that matter for the retrofit:

- **HM4 manual inching module** (hood open, manual functions chuck / tailstock / steady rest): hardware relays K1-K4 powered only when the hood is open, plus an NC release signal. LinuxCNC must read the operator/foot-switch inputs and give the release; keep the HM4 hardware AND-function in series (two-channel, Dold positively guided relays).
- **DINA DNDS I + DNIA speed monitor** (spindle safe speed, Ident. 1047215 + 1047217): independent of the NC. It reads the spindle encoder (SUB-D 15, 5 V, Ua1/Ua2 + inverted + soiling signal Uas pin 15) via DNIA and opens the contacts 13-14 / 23-24 above the set speed (50 rpm with door open, or the D1-D4/F1-F4 coded chuck speed, 16 steps, reducible to 25% in 5% steps). Contacts 21-24 are in the EMERGENCY STOP chain. If you keep it: the encoder signal must still be looped through the DNIA second SUB-D to the new control (do not break the 1:1 loop), and LinuxCNC should drive D1-D4/F1-F4 for the chuck speed code (24 V binary inputs) and read 11-12 (standstill) / 13-14 (speed OK) as inputs for door-unlock and status. Its fault memory is cleared only by > 2 s supply interruption (N/C key in +24 V).
- **SRBF safety relays** (SRBF-1122, cat. 3/4, up to 220 s drop-out delay): handle E-stop and guard interlocks; release paths 13-14 ... 43-44 and the PNP feedback output L14. With the Indramat drives kept, the SRBF release contacts still switch the drive enables / main contactor; LinuxCNC must read the L14 feedback and must not bypass the hardware chain. If a delayed release is used, it is probably there to let the Indramat drives brake under control before power is removed [assumption, not stated in this chapter; check the E-stop sheets]; keep that delay when changing the E-stop logic.
