# Gildemeister CTX twin, electrical documentation: 5. Main spindle drive (part c) - Indramat main spindle drive "Controller functions", PDF pages 1-44

Source: `9e8f1459-5_Main_spindel_drive_c_20201028_0001.pdf` (chapter "5 Main spindle drive", part c, 44 scanned pages, scanned 2020-10-28). These pages are **not a Gildemeister drawing set**: they are pages 71-114 of the INDRAMAT application manual for the AC main spindle drives (control units **RAC** 2.2 / 3.1 and **KDA/TDA**), chapter 3 "Controller functions" (running header "Controller functions" on every page, sections 3.6.4 to 3.19, Fig. 44 to Fig. 95). The pages have no title block, no Gildemeister document number and no date; the only document number printed is the cross reference to Indramat document **No. 9.552.247.4** (high-resolution main spindle position encoders). Each figure carries a small German file tag in its corner (e.g. "Para-Spindellage"), quoted below with translation where useful.

Original language: **English** (Indramat's English edition). No machine-specific device tags (-K.., -X.. of the Gildemeister cabinet) appear; the connector names X2, X3, X4, X5, X8, X14, X15, X16 are the drive's own connectors, and K1...K18 are example relays in Indramat's sample circuits.

Notation: [uncertain] = reading not sure, [unreadable] = cannot be read. Parameter boxes printed as "S R Q P16" mean the parameter exists in all four switchable records P, Q, R, S (P16, Q16, R16, S16). Bit values of function parameters are printed in angle brackets, e.g. <16>. "1/rpm" is printed for min^-1.

## Page index

PDF page n = manual page 70 + n.

| PDF page | Manual page | Content |
|---|---|---|
| 1 | 71 | 3.6.4 Spindle positioning via spindle feedback, Fig. 44 parameters |
| 2 | 72 | Fig. 45 timing diagram: spindle positioning via spindle feedback |
| 3 | 73 | 3.7 Speed controller functions, Fig. 46 X2 control inputs / PQRS gain table |
| 4 | 74 | Fig. 47 parameters for speed controller functions |
| 5 | 75 | Fig. 48 speed controller and tacho filter data |
| 6 | 76 | 3.8 C-axis function, Fig. 49, Fig. 50 (1MB / 2AD motor arrangements) |
| 7 | 77 | C-axis requirements, Fig. 51 control inputs for C-axis |
| 8 | 78 | Fig. 52 parameters for C-axis function |
| 9 | 79 | 3.9 Power and torque limiting, Fig. 53 |
| 10 | 80 | Fig. 54, MD-RED / 90 % LOAD text |
| 11 | 81 | Fig. 55 peak value limiting I/O (X2, X4 E3/E4), Fig. 56 parameters |
| 12 | 82 | 3.10 Drive utilization output, 3.10.1 analogue output M, Fig. 57 |
| 13 | 83 | Fig. 58, Fig. 59 (torque / power utilization display) |
| 14 | 84 | Fig. 60 torque display, Fig. 61 X2 inputs and analogue output M |
| 15 | 85 | Fig. 62 parameters for analogue output M |
| 16 | 86 | 3.10.2 LOAD LIMIT output, Fig. 63, Fig. 64 |
| 17 | 87 | 3.11 Temperature pre-warning, Fig. 65 |
| 18 | 88 | List of temperature fault messages |
| 19 | 89 | 3.12 Maximum spindle speeds, Fig. 66 |
| 20 | 90 | Fig. 67 parameters for max. spindle speeds / gear ratio |
| 21 | 91 | 3.13 Spindle monitor, Fig. 68 |
| 22 | 92 | 3.14 Switchable parameter records, Fig. 69 |
| 23 | 93 | Fig. 70 inputs for parameter record changeover |
| 24 | 94 | Fig. 71 overview of switchable parameters PQRS (incl. IGS incremental encoder output) |
| 25 | 95 | 3.15 Gear change, 3.15.1 via the controller, Fig. 72 |
| 26 | 96 | Fig. 73 gear change parameters |
| 27 | 97 | Fig. 74 gear change sequence |
| 28 | 98 | Fig. 75 connection diagram for gear change (K1/K2, indexing drive) |
| 29 | 99 | 3.15.2 Gear change via external controller, Fig. 76, Fig. 77 |
| 30 | 100 | Fig. 78 sequence, gear change via external controller |
| 31 | 101 | 3.16 Two-motor changeover, Fig. 79 |
| 32 | 102 | Fig. 80, Fig. 81 two-motor changeover I/O and parameters |
| 33 | 103 | Fig. 82 two-motor changeover sequence |
| 34 | 104 | Fig. 83 connection diagram for two-motor changeover (X8, X3, X5, X16, X2, X14) |
| 35 | 105 | 3.17 Master-slave operation, Fig. 84 |
| 36 | 106 | Fig. 85, Fig. 86 master drive |
| 37 | 107 | Fig. 87, Fig. 88 slave drive |
| 38 | 108 | Fig. 89 matching the slave (X2:21/22 -> X4:1/2) |
| 39 | 109 | Fig. 90 master-slave with RAC (X15), Fig. 91 with KDA/TDA |
| 40 | 110 | 3.18 EMERGENCY-STOP circuit (RAC), Fig. 92 |
| 41 | 111 | Fig. 93 E-stop ramp parameter |
| 42 | 112 | 3.19 Performance during mains failure (RAC), d.c. braking, bleeder energies |
| 43 | 113 | Fig. 94, Fig. 95 braking parameters, bleeder monitor |
| 44 | 114 | Bleeder monitor features |

## Summary: drive connector pinout collected from these pages

Compiled from all figures in this part (the full connector chapter of the manual is not in these 44 pages). Logic levels: 1 = +24 V, 0 = 0 V.

**X2 control inputs (24 V)**

| X2 pin | Signal | Meaning (from these pages) |
|---|---|---|
| 3 | AS | Starting lockout (Anlaufsperre), KDA/TDA only; must be 1 |
| 23 | E-STOP | RAC only; 0 = emergency stop, set-point forced to 0, display "E-STOP" |
| 24 | RF | Controller enable (Reglerfreigabe); full torque 200 ms after RF = 1 |
| 25 | RUN | Set-point enable / spindle run; RUN = 0 with POS1/POS2 -> spindle positioning |
| 26 | POS 1 | Spindle position select bit 1 (A08 [uncertain]/A09/A10) |
| 27 | POS 2 | Spindle position select bit 2 |
| 28 | MD-RED | 1 = activate torque/power reduction |
| 29 | OSCILLATE | 1 = oscillate motor shaft for gear change |
| 30 | PAR 1 | Parameter record select bit 1 (P/Q/R/S) |
| 31 | PAR 2 | Parameter record select bit 2 |
| 32, 33, 34 | LIMIT 1, LIMIT 2, LIMIT 4 | Select one of 7 max. spindle speeds A12...A18 |
| 35, 36 | SPEED 1, SPEED 2 | Gear stage (or motor contactor) acknowledgement |
| 37 | EXT POS | 1 = changeover of input weighting (C-axis fine set-point P-MAXRPM / 2nd differential input) |

**X2 outputs**

| X2 pin | Signal | Meaning |
|---|---|---|
| 4 | READY | Drive ready |
| 6 | N < MIN | Speed below MIN RPM (gear change permitted) (read "X2/6" from zoom) |
| 7 | INPOS | Spindle in position |
| 8 | 90% LOAD | 0 = 90 % of current possible maximum utilization reached |
| 9 | TEMP WARN | 0 = motor or controller temperature pre-warning |
| 10 | LIMIT | 0 = selected limit speed reached |
| 11 | SPEED CHANGE 1 | Gear-change motor / motor contactor 1 (max. 50 mA) |
| 12 | SPEED CHANGE 2 | Gear-change motor / motor contactor 2 (max. 50 mA) |
| 13 | LOAD LIMIT | 0 = LOAD LIM threshold exceeded |
| 14 | RF-SLAVE | Controller enable passed on to slave drive |
| 15 | SPINDLE CONTR | 0 = spindle monitor fault |
| 20 | M | Analogue output M, 0...+10 V drive utilization |
| 21 | N | Analogue output N (speed; torque set-point in master mode) |
| 22 | 0VM | Reference 0 V for analogue outputs |

**Other connectors**

| Connector | Function (from these pages) |
|---|---|
| X3 | Motor feedback ("Feedback 1") |
| X4:1 E1 / X4:2 E2 | Analogue differential speed set-point input (+/-10 V; torque set-point in slave mode) |
| X4:5 E3 / X4:6 E4 (X4:4 shield [uncertain]) | Second analogue differential input (torque limiting by voltage; C-axis 2nd set-point input) |
| X5 | Second encoder input (spindle encoder; or feedback of motor 2) - option "2nd encoder input" = P |
| X8: A1, A2, A3, PE | Motor power output |
| X14 | (RAC 2.2 only) mains/aux connection for motor contactor coils (2U, 2W) |
| X15/1-2 ON, X15/3-4 OFF, X15/5-6 Bb | RAC power ON/OFF contacts and ready-for-operation (Betriebsbereit) contact |
| X16: T1, T2 | Motor temperature sensor |

---

## PDF page 1 (manual page 71): 3.6.4 Spindle positioning via spindle feedback

Header: "Controller functions".

**3.6.4 Spindle positioning via spindle feedback**

- A precondition is that the control unit must be fitted with the interface for the 2nd encoder input (type code field "2nd encoder input" = P).
- The incremental encoder fitted to the spindle must be compatible with ROD 420/426 (5 V supply voltage).
- The INDRAMAT high-resolution encoder used for spindle feedback can only be evaluated at high resolution in conjunction with the "incremental encoder output" or "Sercos" interface (type code "Additional interface" = I or L).

Fig. 44: Additional parameters to those in 3.6.1 for spindle positioning via spindle feedback (figure tag "Para-Spindel-lage")

| Parameter (records) | Designation | Function | Value range or value |
|---|---|---|---|
| S R Q P16 | PQ-FUNCT | Direction of rotation of motor and spindle - same | <0> |
| | | - opposite | <2> |
| A05 | FUNCT 1 | Positioning via spindle feedback, not motor feedback | <8> |
| A11 | ENCODER 2 | Number of lines if an incremental encoder is used as spindle feedback | 128 ... 8192 |
| | | Spindle feedback is INDRAMAT high-resolution encoder with dia. 128 mm (256 teeth) | <3> |
| | | ... with dia. 256 mm (512 teeth) | <4> |

(The P16 box shows the record letters "S R Q" printed in front; the P record is implied - this is how all PQRS parameters are drawn in this manual.) Footnote: "Parameter list!"

**Note:** When an incremental encoder is used the spindle positioning accuracy depends on the number of lines used. If the INDRAMAT high-resolution encoder is used and the control unit is modified as above, the spindle positioning accuracy is 1/1000th of an angular degree. (For further information on INDRAMAT high-resolution main spindle position encoders, refer to document No. 9.552.247.4).

## PDF page 2 (manual page 72): Fig. 45 timing diagram

Fig. 45: Operating sequence for spindle positioning via spindle feedback (e.g. direction of rotation as per sign of applied speed set-point). Figure tag "Fkt-Spindellagegeber".

Signal traces (top to bottom), all on control connector X2 of the drive:
- RUN, input X2:25
- POS 1, input X2:26
- POS 2, input X2:27
- "S Spindle speed" (speed curve) and a second trace "S X2:21" [the label under "S" reads X2:21; on Fig. 74 and Fig. 78 X2:21 is "Analogue output N", i.e. the actual-speed analogue output, so this trace is the speed signal at analogue output N, not a command input]
- Spindle position 0 deg / 90 / 180 / 270 / 360 deg ramp
- INPOS, output X2:7
- Display line ("z.B." = e.g.): "N = CMD" -> "POS ORDER" -> "INPOS" ... -> "N = CMD", "N = CMD" [first text reads "N ≠ CMD" uncertain]

Sequence shown: RUN high, spindle runs at S = S set-point. POS 2 set, RUN removed -> spindle decelerates to positioning speed P18 (POSRPM), then moves to the position given by A09 (POS 2), stops within the window P21 (P-WINDOW), INPOS output goes high, display "INPOS". Next POS 1 alone -> position "(POS 1)", which is printed with parameter box "A09" (the same number as POS 2; by the sequence A07 OFFSET, A08, A09, A10 it is presumably A08 - misprint, uncertain); POS 1 + POS 2 -> A10 (POS 3). Position references measured from A07 (OFFSET) above 0 deg. "Positioning direction A06 (FUNCT 2) = <16> direction as per sign of N set-point". When RUN returns, "Spindle accelerates to set-point speed", display returns to N = CMD.

**Note:** The value of the OFFSET parameter is the angle between the spindle positioning reference point and the spindle feedback zero pulse.

## PDF page 3 (manual page 73): 3.7 Speed controller functions

Text: When changing the speed or changing the operating mode of the drive from the main spindle to the C-axis servo function, different mechanical systems make different demands on the speed controller. Four different parameter records, which can be called up via control inputs, are available. Specific speed control parameters are included in each of these parameter records for the appropriate mechanical coupling or operating mode. Because of the wide speed range, the main spindle drive operates in the constant torque or constant power range, irrespective of speed. This also requires the speed controller to be changed over according to the speed.

Fig. 46: Control inputs for selecting the switchable parameter records (tag "Auswahl Parameter"). Control unit connector X2, "Control inputs":

| X2 pin | Signal | State shown |
|---|---|---|
| 23 | E-STOP (RAC only) | 1 |
| 24 | RF | 1 = Controller enabling signal |
| 25 | RUN | any |
| 26 | POS 1 | any |
| 27 | POS 2 | any |
| 28 | MD-RED | any |
| 29 | OSCILLATE | 0 |
| 30 | PAR 1 | select one of the switchable parameter records |
| 31 | PAR 2 | select one of the switchable parameter records |
| 32 | LIMIT 1 | any |
| 33 | LIMIT 2 | any |
| 34 | LIMIT 4 | any |
| 35 | SPEED 1 | any |
| 36 | SPEED 2 | any |
| 37 | EXT POS | any |
| 3 | AS (KDA/TDA) | 1 |

Legends: 1 = +24 V, 0 = 0 V; step symbol = signal step; dashed box = any output state.

Table: selected speed controller parameters

| PAR 1 | PAR 2 | P-GAIN 1 | I-GAIN 1 | Change-over speed GAIN RPM | P-GAIN 2 | I-GAIN 2 |
|---|---|---|---|---|---|---|
| 0 | 0 | P07 | P08 | P09 | P10 | P11 |
| 1 | 0 | Q07 | Q08 | Q09 | Q10 | Q11 |
| 0 | 1 | R07 | R08 | R09 | R10 | R11 |
| 1 | 1 | S07 | S08 | S09 | S10 | S11 |

**Note:** The motor is sufficiently magnetized and can provide the full torque only 200 ms after the controller enabling signal (RF = 1). The set-point is inhibited during this interval.

## PDF page 4 (manual page 74): Fig. 47 Parameters for speed controller functions (tag "Para-Drehz-Fkt")

| Parameter | Designation | Function | Value range or value |
|---|---|---|---|
| S R Q P07 | P-GAIN 1 | P-gain of speed controller is below changeover speed | 0 ... 20.00 |
| S R Q P08 | I-GAIN 1 | I-gain of speed controller is below changeover speed | 0 ... 20.00 |
| S R Q P09 | GAIN RPM | Changeover speed from P-I gain 1 to P-I gain 2 | 0 ... 9999 (1/rpm) |
| S R Q P10 | P-GAIN 2 | P-gain of speed controller is above changeover speed | 0 ... 9.99 |
| S R Q P11 | I-GAIN 2 | I-gain of speed controller is above changeover speed | 0 ... 9.999 |
| A03 | MIN RPM | Speed limit | 1 ... 999 (1/rpm) |
| A05 | FUNCT 1 | I-gain of the N-controller is switched off at Nref below the value of MINRPM, general | <16> |
| S R Q P16 | PQ FUNCT | I-gain of the N-controller is switched off at Nref below the value of MINRPM, depends on parameter | <4> |
| M03 | T-FILTER | Value of time constant of tacho filter | 0 ... 3 |

Notes: Changeover from P-I GAIN 1 to P-I GAIN 2 is automatic if the speed of the motor is below the value of GAIN RPM [as printed]. The drive switches automatically to P-GAIN 2 and I-GAIN 2 at the speed in parameter I-U RPM (M09), even if GAIN RPM was set higher. For braking without overshoot, for a speed set-point of less than MIN RPM, the drive can be switched to pure P-control. This is always achieved with the FUNCT 1 parameter (A parameter), but only with the PQ FUNCT parameter (PQRS parameter) in the respective, active parameter record. The drive can also be run with true P- or I-control (value 0 is set for the other respective parameter).

## PDF page 5 (manual page 75): Fig. 48 Speed controller and tacho filter data (tag "Tachofilter")

- Settling time T_N of speed controller: T_N = 7 ms x P-GAIN / I-GAIN
- Data for P-gain: (approx. 80 1/rpm) / P-GAIN = max. torque set-point
- Time constant T_GN of tacho filter:

| T-FILTER | T_GN / ms |
|---|---|
| 0 | = 0 |
| 1 | = 1.1 |
| 2 | = 1.7 |
| 3 | = 2.7 |

Note: The values given are guide values only. They vary by about 10 % depending on the respective drives.

## PDF page 6 (manual page 76): 3.8 C-axis function

At low lathe speeds the main spindle is used as a servo axis. Contours can be cut on the workpiece through interpolation with the X axis.

Fig. 49: C-axis on turning machine (sketch: chuck with Z-axis arrow; chuck front view with C-axis rotation arrow and tool moving in X-axis). Tag "C-Achse".

Fig. 50: C-axis with Indramat main spindle drives (tag "C-Achse-Indramat"): top sketch "1MB frameless spindle motor" (motor built into spindle) with lines "Power" in and "Spindle and rotor position" out (one encoder serves both). Bottom sketch "2AD main spindle motor" coupled by belt/gear to spindle: "Power" in, "Rotor position" out from motor, separate "Spindle position" out from the spindle encoder.

Text: The performance of the C-axis very much depends on the stiffness of the coupling between the motor and the spindle. Maximum stiffness is obtained with the 1MB frameless spindle motor.

## PDF page 7 (manual page 77): C-axis requirements

Compared to pure main spindle speed control, for C-axis applications the drive requires: higher rotational speed stiffness; finer resolution of set-point input voltage; a second set-point input, depending on the type of controller.
Functions available: changeover to a parameter record that is specially optimized to high rotational speed stiffness; changeover to fine resolution set-point voltage; changeover to second set-point input with function parameters.

Requirements:
- high-resolution motor feedback (on 2AD motor - type code field "motor feedback" = 3, 1MB motor with INDRAMAT high-resolution encoder)
- direct measuring system on spindle (with incremental encoder or INDRAMAT high-resolution encoder).

**Note:** When the INDRAMAT high-resolution encoder is used, the additional "incremental encoder output" interface is required on the control unit. This provides the NC controller with incremental encoder-compatible signals with adjustable number of encoder lines.

Fig. 51: Control inputs for C-axis function (tag "C-Achs-Fkt"). X2 control inputs as Fig. 46, with: 23 E-STOP (RAC only) = 1; 24 RF (controller enabling signal) = 1 (step); 29 OSCILLATE = 0; 30 PAR 1, 31 PAR 2 = "Changeover PQRS parameter records (see table)"; 37 EXT POS = "1 = Changeover input weighting"; 3 AS (starting lockout - KDA/TDA only) = 1.

| PAR 1 | PAR 2 | Active parameter record |
|---|---|---|
| 0 | 0 | P |
| 1 | 0 | Q |
| 0 | 1 | R |
| 1 | 1 | S |

## PDF page 8 (manual page 78): Fig. 52 Parameters for C-axis function (tag "Para-C-Achse")

| Parameter | Designation | Function | Value range or value |
|---|---|---|---|
| A01 | MAXRPM | Maximum speed of motor | 1 ... 24000 (1/rpm) |
| A02 | CMD VOLT | Analogue voltage value | 6.0 ... 10.0 (V) |
| S R Q P15 | P-MAXRPM | Motor speed for further input weighting (low speed) | 1 ... 24000 (1/rpm) |
| S R Q P16 | PQ-FUNCT | Change of analogue inputs when changing the input weighting | <1> |
| A11 | ENCODER 2 | For "incremental encoder output" option only - INDRAMAT high-res. encoder: 128 mm dia. | <3> |
| | | - INDRAMAT high-res. encoder: 256 mm dia. | <4> |

Note: If the EXT POS control input is set to 0, the weighting CMDVOLT = MAXRPM applies (for high speed). If EXT POS = 1, the weighting CMDVOLT = P MAXRPM applies (for low speed). Changeover to the 2nd differential input (parameter PQ FUNCT) can be obtained with the weighting changeover (EXT POS = 1).

## PDF page 9 (manual page 79): 3.9 Power and torque limiting

In exceptional cases, torque and power limiting protects the workpiece, tool and machine form [sic, = from] damage. INDRAMAT main spindle drives provide a choice between: limiting the peak torque without limiting the peak power of the drive; limiting the peak torque and the peak power of the drive simultaneously.

Fig. 53: Example showing the torque limited to 80 % of the peak value without limiting the peak power (tag "Dreh-80%"). Graph torque M (100 %, 80 %) and power P (100 %, 80 %) vs speed N; curves "Peak torque", "Peak power", "Torque, unlimited", "Power, limited".

## PDF page 10 (manual page 80): Fig. 54

Fig. 54: Example showing the torque and the power limited to 80 % of the peak value (tag "Dreh-Leistung-80%"); curves Peak torque, Peak power, Torque unlimited, Torque limited, Power limited.

Text: In the simplest case the amount of limiting is stored as a % in the switchable parameter records. One value is possible per parameter record. If several limiting values are required within the parameter records, then the limiting value can also be input as a voltage at the analogue input. In both cases limiting is activated via the MD-RED control input. To avoid speed dips, the drive monitors the utilization. If the utilization has reached 90 % of the current possible maximum value, then the "90 % LOAD" output switches from 1 to 0.

## PDF page 11 (manual page 81): Fig. 55 / Fig. 56 peak value limiting

Fig. 55: Control inputs, signal output and analogue input for peak value limiting (tag "Spitzenwertbegrenzung" = peak value limiting). Control unit:

- X2 Control inputs: 23 E-STOP (RAC only) = 1; 24 RF (controller enabling signal) = 1; 25 RUN = 1; 26 POS 1, 27 POS 2 = any; 28 MD-RED = step, "1 = Activate limiting"; 29 OSCILLATE = 0; 30 PAR 1 ... 37 EXT POS = any; 3 AS (starting lockout - KDA/TDA only) = 1.
- X2 Signal output: 8 "90% LOAD" -> "0 = 90 % of current possible maximum utilization".
- X4 Analogue input: pins 5 (E3) and 6 (E4) = differential input to an amplifier; pin 4 = shield/ground [uncertain, drawn as shield connection]. "Analogue voltage 0 ... +/-10 V": +/-10 V = 100 % (no limit), +/-8 V = 80 % (80 % limiting), etc.

Fig. 56: Parameters for peak value limiting (tag "Para-Spitzenwert")

| Parameter | Designation | Function | Value range or value |
|---|---|---|---|
| A05 | FUNCT 1 | Torque and power limiting when MD-RED control input is set to 1 | <0> |
| | | Torque limiting only when MD-RED control input is set to 1 | <1> |
| S R Q P6 | MD-RED | Percentage value of peak torque | 1 ... 99 (%) |
| S R Q P16 | PQ-FUNCT | Torque limiting via voltage at analogue input X4 (E3/E4): +/-10 V = 100 % = peak torque | <32> |

Note: Since the "MD-RED" and "PQ-FUNCT" parameters occur a number of times, both types of limiting can be used by means of relevant parameter values and by switching the parameter records, without interrupting the operation of the drive.

## PDF page 12 (manual page 82): 3.10 Drive utilization output

There is a risk of damage to tools, workpieces or the machine if the possible cutting power is fully utilized. To prevent damage, the main spindle drive controller has three simple facilities for displaying and signalling the utilization:
- analogue output for visual display on a measuring instrument (analogue output M, see 3.10.1)
- "90% LOAD" signal output (see 3.9)
- "LOAD LIMIT" signal output with adjustable signal threshold (see 3.10.2)

**3.10.1 Analogue output for drive utilization (analogue output M).** A simple visual indication of the drive utilization can be set up at the analogue output M with a voltage value of between 0 and +10 V. The voltage at M (0 ... 10 V) corresponds to: a) either the percentage torque utilization of the drive, b) a torque utilization related to the machining, c) a power level related to the machining, d) a torque value.

**Ref. a), percentage torque utilization** - basic state of analogue output M (for default parameter values). The voltage value refers to the speed-dependent, available maximum torque (100 % value).

Fig. 57: Display of percentage torque utilization (tag "Dreh-%%"): torque characteristic of drive with 100/80/60/40/20 % curves; Weighting: 100 % = 10 V; meter scale 0 V, 2 V (20 %), 4 V (40 %), 6 V (60 %), 8 V (80 %), 10 V (100 %).

## PDF page 13 (manual page 83): Ref. b) and c)

**Ref. b), torque utilization related to the machining**: voltage relates to parameter LOAD LIM (PQRS 19); LOAD LIM (PQRS 19) must be arranged via function parameter FUNCT2 (A06) = <1> as a % value; PQ FUNCT (PQRS 16) must contain <16>. Output M indicates how near the drive approaches a set percentage torque utilization threshold. Four machining-dependent thresholds can be set by the fourfold LOAD LIM parameters.
Fig. 58: Display of torque utilization in relation to a set percentage utilization threshold (tag "Dreh-%-Auslast"). Weighting X % = 10 V; X % = value in LOAD LIM * %; scale 0.2*X% ... X%.

**Ref. c), power level related to the machining**: voltage relates to LOAD LIM (PQRS 19); LOAD LIM arranged as the power value in the basic state (default FUNCT2 (A06) = <0>); PQ FUNCT (PQRS 16) must contain <16>. Four machining-dependent power thresholds.
Fig. 59: Display of power in relation to a set percentage power threshold (tag "Anzeige-Leistung"). Weighting X kW = 10 V; X kW = value in LOAD LIM * kW.

## PDF page 14 (manual page 84): Ref. d) torque value, Fig. 60, Fig. 61

**Ref. d), torque value**: voltage relates to LOAD LIM (PQRS 19); FUNCT 2 (A06) must contain <512>; PQ FUNCT (PQRS 16) has no significance. Output M indicates how near the load approaches a machining-dependent torque limit. The torque value indicated is calculated from the link circuit power and the present motor speed.
Fig. 60: Display of torque values (tag "Anzeige-Drehmoment"): weighting X Nm = 10 V; X = 10 * (number in LOAD LIM) * Nm.

Fig. 61: Control inputs and analogue output M (tag "Analogausgang M"): X2 inputs as before (23 E-STOP (RAC only) = 1, 24 RF = 1, 29 OSCILLATE = 0, 30 PAR 1 / 31 PAR 2 "Select one of 4 switchable parameter records (see table)", 3 AS (starting lockout - KDA/TDA only) = 1). **X2 Analogue output: pin 20 = M** ("Drive utilization as analogue voltage").

| PAR 1 | PAR 2 | Parameter LOAD LIM |
|---|---|---|
| 0 | 0 | P19 |
| 1 | 0 | Q19 |
| 0 | 1 | R19 |
| 1 | 1 | S19 |

## PDF page 15 (manual page 85): Fig. 62 Parameters for analogue output M (tag "Para-Analog M")

| Parameter | Designation | Function | Value range or value |
|---|---|---|---|
| S R Q P16 | PQ-FUNCT | M output indicates percentage utilization, +10 V = 100 % | <0> |
| | | M output gives voltage +10 V in relation to value in LOAD LIM (kW or %) as arranged in A06 | <16> |
| S R Q P19 | LOAD LIM | Torque or power utilization threshold, depending on A06 | 0 ... 99 (% or kW) |
| A06 | FUNCT2 | LOAD LIM parameter is power-related, value indicates kW | <0> |
| | | LOAD LIM parameter is torque-related, value indicates % | <1> |
| | | M output gives torque, +10 V = 10 * (number in LOAD LIM) * Nm | <512> |

Note: Analogue output M is designed to monitor the static drive utilization. The voltage is smoothed to suppress high-frequency variations. When the torque value is output at analogue output M, the percentage drive utilization that is otherwise output at this point appears at analogue output N. If LOAD LIM (PQRS 19) is used as a torque threshold, power threshold or weighting factor for torque output, the LOAD LIMIT signal output is also active for this value.

## PDF page 16 (manual page 86): 3.10.2 Signal output with adjustable threshold for drive utilization (LOAD LIMIT)

The signal threshold for drive utilization is fully adjustable. It is included in each of the switchable parameter records PQRS and can be arranged as torque utilization or power signal threshold. If the drive utilization reaches the signal threshold of the active parameter record, the LOAD LIMIT signal output is active.

Fig. 63: Control inputs and signal output for adjustable signal threshold (tag "Meldeschwelle" = signalling threshold). X2 inputs as before (23 E-STOP (RAC only) = 1, 24 RF (controller enabl. sign.) = 1, 25 RUN = 1, 29 OSCILLATE = 0, 30 PAR 1 / 31 PAR 2 = select one of 4 switchable parameter records, 3 AS (starting lockout - KDA/TDA only) = 1). **X2 Signal output: pin 13 = LOAD LIMIT**, "0 = Signal threshold is exceeded". Table PAR1/PAR2 00 -> P19, 10 -> Q19, 01 -> R19, 11 -> S19.

Fig. 64: Parameters for adjustable signal threshold (tag "Para-Meldeschwelle")

| Parameter | Designation | Function | Value range or value |
|---|---|---|---|
| S R Q P19 | LOAD LIM | Signal limit for drive utilization | 1 ... 99 (% or kW) |
| A06 | FUNCT 2 | Signal limit is power-related, the value in LOAD LIM indicates kW | <0> |
| | | Signal limit is torque-related, the value in LOAD LIM indicates % of peak torque | <1> |

## PDF page 17 (manual page 87): 3.11 Temperature pre-warning

Due to the overload capacity of the main drives, load cycles can be implemented which for short periods demand considerably more power than the continuous power from the drive. Choice of correct drive size ensures that the temperature of the motor and the controller is always within the permissible range. However, if for example the cooling system of the motor or controller is not operating correctly (contamination, etc.) there is a risk of damage to the drive due to overheating.

INDRAMAT main spindle drives are protected against damage due to overheating:
- If the temperature of the motor or the controller approaches the limit of the permissible temperature range, this is signalled to the controller via the TEMPWARN alarm output. If caused by the motor temperature, the message MOTWARN appears in the display; KKWARN appears if caused by the controller.

**Important:** In the case of KKWARN, the drive torque is immediately limited to 50 %.
- The drive is switched off automatically if the load is not removed and the temperature rises further.

Fig. 65: Control inputs and signal output for temperature pre-warning (tag "Temperaturvorwarnung"). X2 inputs: 23 E-STOP (RAC only) = 1, 24 RF = 1, 29 OSCILLATE = 0, 3 AS (starting lockout - KDA/TDA only) = 1. **X2 Signal output: pin 9 = TEMP WARN**, "0 = Temperature pre-warning has tripped".

| | Permissible temperature ranges | Signal thresholds |
|---|---|---|
| Motor | 0 deg < theta < 155 degC | 145 degC |
| Controller | 0 deg < theta < theta_max | theta_max [subscript "max" partly unreadable] |

Note: If KKWARN appears in the display, the drive switches off after 30 seconds and KKTEMP appears in the display. If MOTWARN (windings temperature 145 degC) and the temperature continues to rise, the drive switches off at a windings temperature of 155 degC and MOTTEMP then appears in the display. With no temperature sensor connected, the TEMPWARN output is also active after 10 minutes' operation with at least 10 % loading. NO TEMP appears in the display and the drive switches off.

## PDF page 18 (manual page 88): Possible fault messages in the display

NO TEMP; MOT WARN; KK WARN; MOT TEMP; KK TEMP. (Rest of page empty; bleed-through from the reverse side.)

## PDF page 19 (manual page 89): 3.12 Maximum spindle speeds

In machine tools, various tools or chucking devices with different permissible maximum speeds can be used on the main spindle. INDRAMAT main spindle drives enable the spindle speed to be monitored and limited to 7 different, freely-selectable maximum speeds.

Fig. 66: Control inputs and signal output for max. spindle speeds (tag "Spindelmaximaldreh"). X2 inputs: 23 E-STOP (RAC only) = 1, 24 RF = 1, 25 RUN = 1, 29 OSCILLATE = 0, 32 LIMIT 1 / 33 LIMIT 2 / 34 LIMIT 4 = "Select one of 7 max. spindle speeds (see table)", 3 (starting lockout - KDA/TDA only) = 1. **X2 Signal output: pin 10 = LIMIT**, "0 = Limit speed is reached".

| LIMIT 1 | LIMIT 2 | LIMIT 4 | Max. spindle speed | Stored in parameter |
|---|---|---|---|---|
| 0 | 0 | 0 | none | ------- |
| 1 | 0 | 0 | LIMIT 1 | A12 |
| 0 | 1 | 0 | LIMIT 2 | A13 |
| 1 | 1 | 0 | LIMIT 3 | A14 |
| 0 | 0 | 1 | LIMIT 4 | A15 |
| 1 | 0 | 1 | LIMIT 5 | A16 |
| 0 | 1 | 1 | LIMIT 6 | A17 |
| 1 | 1 | 1 | LIMIT 7 | A18 |

## PDF page 20 (manual page 90): Fig. 67 Parameters for max. spindle speeds (tag "Para-Spindel-max")

| Parameter | Designation | Function | Value range or value |
|---|---|---|---|
| A12 ... A18 | LIMIT 1 ... LIMIT 7 | Max. spindle speed 1 ... 7 | 1 ... 24000 (1/rpm) each |
| S R Q P12 | GEAR IN | Reduction ratio i = N1/N2 (N1) | 1 ... 999 |
| S R Q P13 | GEAR OUT | Reduction ratio i = N1/N2 (N2) | 1 ... 999 |

Note: The spindle speed is limited to the amount (+/-) of the selected maximum speed and on reaching the limit speed is also acknowledged via the LIMIT signal output. The controller calculates the limit speed of the motor associated with the limit speed of the spindle with the reduction ratio (GEAR IN/GEAR OUT). If the speed set-point is excessively high, the message N_actual = N_set-point (signal output N=CMD) does not appear.

## PDF page 21 (manual page 91): 3.13 Spindle monitor

The spindle monitor detects any defective or dangerous condition when the machine is first started if this is due to a malfunction in the mechanical transmission components. The controller monitors breaks in the mechanical connection between the spindle and the spindle feedback. The controller also detects the absence of signals from the spindle feedback but when the motor is running. Spindle monitoring requires a spindle feedback unit to be connected to the controller.

Fig. 68: Control inputs and signal output for spindle monitor (tag "Spindelüberwachung" = spindle monitoring). X2 inputs: 23 E-STOP (RAC only) = 1, 24 RF = 1, 25 RUN = 1, 29 OSCILLATE = 0, 3 AS (starting lockout - KDA/TDA only) = 1. **X2 Signal output: pin 15 = SPINDLE CONTR**, "0 = Fault".

Note: For very slow spindle speeds (spindle coasting) the spindle monitor still signals the OK status. Faults on the spindle encoder line can be detected as increments and the OK status is signalled despite the fact that the spindle is stationary (check function precisely during commissioning).

## PDF page 22 (manual page 92): 3.14 Switchable parameter records

The switchable parameter records enable the drive to be automatically and rapidly matched to the various tasks of a flexible machine tool whilst the drive is in the operating mode: matching to different speeds; providing specific functions for various operating processes; operation with various position control loop parameters for spindle positioning; automatic speed change in a two-speed gearbox.

INDRAMAT main spindle drives have six parameter records. Of these, two (A and M parameters) are always active. In addition, one of the 4 switchable parameter records (P, Q, R, S) is always active.

Fig. 69: The six parameter records for INDRAMAT spindle drives (tag "6 Parametersätze"): Parameter record A (general parameters); Parameter record P / Q / R / S; Parameter record M (Motor parameters). "Parameter list!"

## PDF page 23 (manual page 93): Fig. 70 Control inputs for changing over the parameter records

Tag "Umschaltung-Para". X2 inputs 23 E-STOP (RAC only), 24 RF (controller enabling signal), 25 RUN, 26 POS 1, 27 POS 2, 28 MD-RED, 29 OSCILLATE, 30 PAR 1, 31 PAR 2 ("Changeover the 4 parameter records PQRS (see table)"), 32 LIMIT 1, 33 LIMIT 2, 34 LIMIT 4, 35 SPEED 1, 36 SPEED 2, 37 EXT POS, 3 AS (starting lockout - KDA only) [printed "KDA only" here]. (No fixed states drawn on this figure.)

| PAR 1 | PAR 2 | Active parameter record |
|---|---|---|
| 0 | 0 | P |
| 1 | 0 | Q |
| 0 | 1 | R |
| 1 | 1 | S |

## PDF page 24 (manual page 94): Fig. 71 Overview of switchable parameters PQRS (tag "Para-Übersicht")

| Parameter name | Record P | Record Q | Record R | Record S | Function | Value range |
|---|---|---|---|---|---|---|
| RAMP 1 | P01 | Q01 | R01 | S01 | 3 speed set-point ramps or set-point smoothing | 0 ... 999 (rad/s²) |
| RPM 1 | P02 | Q02 | R02 | S02 | (ramps) | 0 ... 24000 (1/rpm) |
| RAMP 2 | P03 | Q03 | R03 | S03 | (ramps) | 0 ... 999 (rad/s²) |
| RPM 2 | P04 | Q04 | R04 | S04 | (ramps) | 0 ... 24000 (1/rpm) |
| RAMP 3 | P05 | .. | .. | .. | (ramps) | 0 ... 999 (rad/s²) |
| MD-RED | P06 | .. | .. | .. | Torque lim. in % | 1 ... 99 |
| P-GAIN 1 | P07 | .. | .. | .. | Speed controller config. 1 | 0 ... 19.99 |
| I-GAIN 1 | P08 | .. | .. | .. | Speed controller config. 1 | 0 ... 9.99 |
| GAIN-RPM | P09 | .. | .. | .. | C/O speed from P-I gain 1 to 2 | 0 ... 9999 (1/rpm) |
| P-GAIN 2 | P10 | .. | .. | .. | Speed controller config. 2 | 0 ... 9.99 |
| I-GAIN 2 | P11 | .. | .. | .. | Speed controller config. 2 | 0 ... 9.999 |
| GEAR IN | P12 | .. | .. | .. | Gear ratio i = N1/N2 | 1 ... 999 |
| GEAR OUT | P13 | .. | .. | .. | Gear ratio | 1 ... 999 |
| G-CHANGE | P14 | .. | .. | .. | Change-gear allocation | 0 ... 2 |
| P-MAX RPM | P15 | .. | .. | .. | Further evaluation of set-point voltage | 1 ... 24000 (1/rpm) |
| PQ-FUNCT | P16 | .. | .. | .. | Parameter record functions | 0 ... 9999 |
| POS GAIN | P17 | .. | .. | .. | Gain factor of position control loop | 0 ... 99.9 (1/s) |
| POS RPM | P18 | .. | .. | .. | Positioning speed | 1 ... 9999 (1/rpm) |
| LOAD LIM | P19 | .. | .. | .. | Load signal threshold | 1 ... 99 (%, kW) |
| ALPHA | P20 | .. | .. | .. | [no function text printed] | 0 ... 9.999 |
| P-WINDOW | P21 | Q21 | R21 | S21 | Positioning tolerance window | 0 ... 9.99 (angle degr.) |

Note: the P-GAIN 1 range here reads 0 ... 19.99, while Fig. 47 says 0 ... 20.00; I-GAIN 1 0 ... 9.99 here vs 0 ... 20.00 in Fig. 47 (as printed).

For incremental encoder output option (IGS) only, type code field "Additional interface" = I:

| Parameter name | P | Q | R | S | Function | Value range |
|---|---|---|---|---|---|---|
| IGS REF | P22 | Q22 | R22 | S22 | Reference switch offset | 0 ... 359.9 (angle degr.) |
| IGS OPEN | P23 | Q23 | R23 | S23 | Opening angle of reference switch | 0 ... 359.9 (angle degr.) |
| IGS LINES | P24 | Q24 | R24 | S24 | Number of lines of simul. encoder | 1 ... 16383 |
| IGS FUNCT | P25 | Q25 | R25 | S25 | Functions with IGS | 0 ... 999 |

See "Incremental encoder output".

Note: When the drive is operating the parameter record functions PQ-FUNCT make the following possible: changeover of analogue differential inputs (X4); indication of possible reversed direction of rotation of motor and spindle feedback or motor and spindle after a gear change; changeover to stop drive without overshoot; changeover of speed- and torque-controlled operation; changeover of voltage at analogue output M from torque loading to LOAD LIM related voltage output; changeover from torque limiting by parameter value to limiting by voltage at analogue differential input (X4); changeover to spindle positioning via spindle reference switch.

## PDF page 25 (manual page 95): 3.15 Gear change

Various gear reduction stages are used according to the spindle speed to obtain a wide speed range with constant power at the spindle. In modern machine tools this requires automatic gear changes. For two reduction stages the controller handles the automatic changeover, for more than two stages the changeover is supported by the controller by speed oscillation (hunting).

**3.15.1 Automatic gear change via the controller.** Requirements: two auxiliary d.c. contactors (see Fig. 75); two-stage gearbox with gear-change motor; acknowledgement feedback for gear stages. Gear change is initiated by a change to a parameter record that is assigned to a stage that is not engaged.

Fig. 72: Control inputs and signal outputs for gear-change via the controller (tag "Get-Umschalt-Regel" [uncertain]).

Function table 1 (header printed "POS 1 | POS 2", meaning the parameter record select inputs PAR 1/PAR 2 [as printed]):

| PAR1 | PAR2 | Para-record | Example of assignment | Assignm. in parameter G-CHANGE (PQRS 14) |
|---|---|---|---|---|
| 0 | 0 | P | Gear stage 1 | P14 = 1 |
| 1 | 0 | Q | Gear stage 2 | Q14 = 2 |
| 0 | 1 | R | No assignment | R14 = 0 |
| 1 | 1 | S | No assignment | S14 = 0 |

Control inputs (acknowledgement from gearbox):

| SPEED 1 | SPEED 2 | Meaning |
|---|---|---|
| 0 | 1 | Gear stage 2 is engaged |
| 1 | 0 | Gear stage 1 is engaged |
| 0,1 | 0,1 | Fault message: GEARBOX (display) |

Signal outputs (actuate gear-change motor):

| SPEED 1 CHANGE | SPEED 2 CHANGE | Meaning |
|---|---|---|
| 0 | 1 | Gear stage 2 is engaged |
| 1 | 0 | Gear stage 1 is engaged |
| 0 | 0 | Gear-change motor is stopped |

X2 connections: 23 E-STOP (RAC only) = 1; 24 RF = 1; 25 RUN = 1; 29 OSCILLATE = 0; 30 PAR 1 / 31 PAR 2 = "Gear change initiated by change in parameter record"; 35 SPEED 1 / 36 SPEED 2 = "Changeover acknowledged by gearbox"; 3 AS (starting lockout - KDA/TDA only) = 1. **X2 signal outputs: 11 SPEED CHANGE 1, 12 SPEED CHANGE 2** = "Gear-change motor actuated via aux. d.c. contactor".

Note: The assignment of gear stages to parameter records P and Q is given as an example only. Any combination of parameter records P, Q, R and S is possible.

## PDF page 26 (manual page 96): Fig. 73 Parameters for gear change via the controller (tag "Para-Getriebeumschalt")

| Parameter | Designation | Function | Value range or value |
|---|---|---|---|
| S R Q P14 | G-CHANGE | Assignment of parameter record to a gear stage - None | <0> |
| | | - Stage 1 | <1> |
| | | - Stage 2 | <2> |
| A20 | OSZI RPM | Oscillating speed for gear change | 1 ... 200 (1/rpm) |

Note: Gear stages 1 and 2 must each be assigned at least once to a parameter record by parameter values 1 and 2 in the G-CHANGE parameters. When changing from a parameter record with assigned gear stage to a parameter record that is not assigned to any gear stage, the previous gear stage remains coupled. The switchable parameter records enable the drive to be optimized according to the gear stages (see Section 3.14).

## PDF page 27 (manual page 97): Fig. 74 Sequence of operations - gear change via the controller (tag "Fkt-Regelgerät")

Traces: "Parameter record P: P14 = 1 (gear stage 1)" -> "Parameter record Q: Q14 = 2 (gear stage 2)"; PAR 1 X2:30 (goes 1); PAR 2 X2:31 (0); Motor speed / **Analogue output N X2:21**: N = N_set, drops to N, oscillates at A20 (OSZI RPM), then "Motor accelerates to set-point speed", N = N_set; SPEED 1 X2:35: goes 0 = "Acknowledge 'gear stage 1 OFF'"; SPEED 2 X2:36: goes 1 = "Acknowledge 'gear stage 2 ON'", "Gear-change interval (10 s max.)", 200 ms; SPEED-CHANGE 2 X2:12: "Movement in direction of engagement" with a short 200 ms interruption; SPEED-CHANGE 1 X2:11: 200 ms pulse after 2 s (reverse jog); "Gear change completed".

Note: The gear change must be completed within 10 seconds. If the gear stage to be engaged is not acknowledged via the corresponding control input SPEED 1 or SPEED 2, the controller display shows GEARBOX and switches the drive and actuating motor off. The indexing mechanism is displaced against the direction of engagement by the gear-change motor every 2 seconds for 200 ms so that the tooth-to-tooth points on the gear wheels can be released again. This can happen up to a maximum of five times during the 10-second interval. During the gear-change process the motor shaft hunts at the OSCI RPM speed and at a frequency of approximately 2.5 Hz. The message OSCILLATE appears in the display. Possible fault message in the display: GEARBOX.

## PDF page 28 (manual page 98): Fig. 75 Connection diagram for gear change (tag "Anschlußplan-Getriebe")

Circuit: Control unit connector X2 drives two external auxiliary d.c. relays K1, K2 (each with free-wheeling diode), which reverse a 24 V DC indexing (gear-change) motor; the gearbox limit switches S1/S2 report back to X2.

Connections read:
- X2:12 SPEED CHANGE 2 -> NO contact (drawn as switch "K1" next to it [label placement uncertain]) -> coil K2 (with diode) -> 0 V_ext. 
- X2:11 SPEED CHANGE 1 -> coil K1 (with diode) -> 0 V_ext. Note on outputs: "Max. loading 50 mA".
- (In the drawing each output line passes an NC interlock contact of the other relay - the contact next to X2:12 is labelled K1, the one next to X2:11 is labelled K2 - so that K1 and K2 cannot pull in together [interpretation].)
- X2:36 SPEED 2 <- indexing drive terminal 6 <- S2 (S2 acknowledges gear stage II).
- X2:35 SPEED 1 <- indexing drive terminal 4 <- S1 (S1 acknowledges gear stage I).
- Supply rails: +24 V_ext / 24 V DC, 0 V. "Do not take 24 V d.c. for gear-change motor from RAC/KDA!"
- Motor current: "Loading: I_max = 7 A (pull-in torque), I_continuous = 2 A".
- K2 and K1 changeover contacts (2 poles each) switch +24 V DC / 0 V with reversed polarity to indexing drive terminals 2 and 3 -> motor M (terminals 31 and 53).
- Indexing drive terminal 5 = common of S1/S2 (fed from +24 V) [uncertain].
- Indexing drive terminals 7, 1 -> "Optional holding brake 24 V d.c. 2 A".
- Box label: "ZF indexing drive" / "Indexing drive"; "S1 acknowledges gear stage I, S2 acknowledges gear stage II".

## PDF page 29 (manual page 99): 3.15.2 Gear change via an external controller

The controller supports the gear change by the NC by: signalling the gear change "ready" state (N < Nmin); oscillate the speed of the motor shaft. The four parameter records enable the drive to be optimized according to the reduction stage.

Fig. 76: Control inputs for gear change via external controller (tag "Umschaltung-Para-ext"). X2: 23 E-STOP (RAC only) = 1, 24 RF = 1, 25 RUN = 1, 29 OSCILLATE = step "1 = Oscillate speed of motor shaft", 30 PAR 1 / 31 PAR 2 = "Changeover the 4 parameter records (see table)", 3 AS (starting lockout - KDA/TDA only) = 1. Table PAR1/PAR2: 00 P, 10 Q, 01 R, 11 S.

Fig. 77: Parameters for gear change via external controller (tag "Para-ext-Getriebeumschalt")

| Parameter | Designation | Function | Value range or value |
|---|---|---|---|
| S R Q P14 | G-CHANGE | Assignment of parameter record to a gear stage - None | 0 |
| A20 | OSZI RPM | Oscillating speed for gear change | 1 ... 200 (1/rpm) |
| A03 | MIN RPM | Speed for initiating gear change via external controller | 1 ... 999 (1/rpm) |

## PDF page 30 (manual page 100): Fig. 78 Sequence - gear change via external controller with constant speed set-point (tag "Fkt-ext-Steuer")

Traces: **N < MIN, output X2:6** (zoomed: "X2/6"); **OSCILLATE, input X2:29**; Motor speed / **Analogue output N X2:21**: from N = N_set brakes down to A04 (MIN RPM) [printed "A04", whereas Fig. 77 lists MIN RPM as A03 - uncertain], then oscillates at A20 (OSCI RPM) with 180 ms / 220 ms half periods; "Time taken by external controller to change gear"; then "Motor accelerates to set-point speed", N = N_set.

Note: The controller initiates the gear change by taking the OSCILLATE control input to 1. The drive then brakes at the set ramp and changes to oscillating speed at 2.5 Hz. The OSCILLATE message appears in the display. The oscillating speed is the value in parameter OSCI RPM. The control unit tells the controller via the N < MIN signal output at what time point the speed of the motor permits a gear change. The signal output N < MIN gives a steady 1 if the value of parameter MIN RPM is greater than that input by OSCI RPM. Oscillation is asymmetrical, i.e. the motor shaft continues to turn during oscillation and can overcome tooth-tooth positions during engagement. If the OSCILLATE control input is switched back to 0 after the gear change, the drive returns to the set-point speed.

## PDF page 31 (manual page 101): 3.16 Two-motor changeover

It is possible in special installations that two drive tasks have to be performed at two different sites. If the two tasks never occur simultaneously then they can be performed with two motors connected to only one control unit. The motors are selected by the controller.

Fig. 79: Schematic of two-motor changeover arrangement (tag "2-Motoren-Umschaltung"): NC controller -> "Speed set-point and motor selection" -> control unit inputs **X4** (speed set-point) and **X2** (motor selection); control unit power output switched by a changeover contact to Motor 1 or Motor 2; Motor 1 feedback -> **X3**; Motor 2 feedback -> **X5**.

Requirement: two motors of identical type; a control unit with two encoder inputs (type code field "2nd encoder input" = P); two auxiliary d.c. contactors and two motor contactors (see Fig. 83).

## PDF page 32 (manual page 102): Fig. 80 / Fig. 81 two-motor changeover

Motor changeover is initiated by changing one of the parameter records that is assigned to the other motor.

Fig. 80: Control inputs and signal outputs for two-motor changeover (tag "2-Mot-Umschalt-Regel"). Function table (header printed "POS 1 | POS 2" = PAR 1/PAR 2):

| PAR1 | PAR2 | Para-record | Example of assignment | Assignment in G-CHANGE (PQRS 14) |
|---|---|---|---|---|
| 0 | 0 | P | Motor 1 | P14 = 1 |
| 1 | 0 | Q | Motor 2 | Q14 = 2 |
| 0 | 1 | R | Motor 1 | R14 = 1 |
| 1 | 1 | S | Motor 2 | S14 = 2 |

| SPEED 1 | SPEED 2 | Acknowledgem. from motor contactor |
|---|---|---|
| 0 | 1 | Motor 2 is engaged |
| 1 | 0 | Motor 1 is engaged |
| 0/1 | 0/1 | Fault message: MOTOR 1/2 (display) |

| SPEED 1 CHANGE | SPEED 2 CHANGE | Actuate motor contactors |
|---|---|---|
| 0 | 1 | Motor contactor 2 is energized |
| 1 | 0 | Motor contactor 1 is energized |
| 0/1 | 0/1 | Fault |

X2: 23 E-STOP (RAC only) = 1, 24 RF = 1, 25 RUN = 1, 29 OSCILLATE = 0, 30/31 PAR 1/PAR 2 "Motor changeover initiated by change in parameter record", 35/36 SPEED 1/SPEED 2 "Changeover acknowledged by motor contactors", 3 AS (starting lockout - KDA/TDA only) = 1; outputs 11 SPEED CHANGE 1, 12 SPEED CHANGE 2 "Motor contactors actuated via the two aux. d.c. contactors".

Fig. 81: Parameters for two-motor changeover (tag "Para-2-Motorenumschalt")

| Parameter | Designation | Function | Value |
|---|---|---|---|
| S R Q P14 | G-CHANGE | Assignment of parameter record to a motor - Motor 1 | <1> |
| | | - Motor 2 | <2> |
| A06 | FUNCT 2 | Second encoder input is used as motor feedback input for motor 2 | <4> |

Note: The values 1 or 2 must always be assigned to the G-CHANGE parameters in the four switchable parameter records; 0 is not allowed! No spindle positioning can be performed via the spindle position encoder during two-motor changeover as the second encoder input is used for motor feedback from the second motor.

## PDF page 33 (manual page 103): Fig. 82 Sequence of operations - two-motor changeover (tag "Fkt-2Motor")

Traces: P14 = 1 (Motor 1) -> Q14 = 2 (Motor 2); PAR 1 X2:30 goes 1; PAR 2 X2:31 = 0; Motor speed / Analogue output N X2:21: Motor 1 at N = N_set brakes to 0, then Motor 2 accelerates to N = N_set; SPEED 1 X2:35 drops: "'Motor 1 off' acknowledgement from motor contactor K1"; SPEED 2 X2:36 rises: "'Motor 2 on' acknowledgement from motor contactor 2", "Motor changeover lasts 2 secs max."; SPEED-CHANGE 2 X2:12 drops: "Motor contactor K1 de-energized by aux. contactor K3"; SPEED-CHANGE 1 X2:11 rises: "Motor contactor K2 energized by aux. contactor K4". [Note: the signal naming here crosses (SPEED-CHANGE 2 drives K1/K3) - as printed.]

Note: Motor changeover must be completed within 2 seconds, otherwise the fault message "Motor 1/2" appears in the display and the drive is switched off. The auxiliary contactors must also change over the temperature sensors of the motors, otherwise the respective active motor is not monitored. Possible fault messages in the display: Motor 1/2; NO TEMP; FEEDBACK.

## PDF page 34 (manual page 104): Fig. 83 Connection diagram for two-motor changeover (tag "Anschlußplan-2-Mot-U")

Circuit: the drive (control unit) power output X8 feeds motor M2 via 3-pole contactor K2 and motor M1 via contactor K1; aux. relays K3/K4 (2 x 3TJ50) driven from X2 outputs switch the contactor coils and the motor temperature sensors.

Connections read:
- **X8: A1, A2, A3, PE** (motor power output) -> K2 main contacts -> M2; and -> K1 main contacts -> M1. PE to both motors.
- **X5** <- "Feedback 2" (cable marked 8 [= 8 cores, uncertain]) <- feedback encoder of M2.
- **X3** <- "Feedback 1" (cable marked 8) <- feedback encoder of M1.
- **X16: T2, T1** (motor temperature sensor input), shield to PE: T2 -> common of both motor thermistors (theta symbol) [uncertain]; T1 -> K4 contact -> thermistor of M2 ... K3 contact -> thermistor of M1 [which contact feeds which motor is uncertain; K4 is next to M2's sensor line, K3 to M1's].
- **X2:35 Speed 1** <- K1 auxiliary NO contact <- 24 V ext; **X2:36 Speed 2** <- K2 auxiliary NO contact <- 24 V ext.
- **X2:11 Speed Change 1** -> K4 coil A1, A2 -> 0 V ext (free-wheeling diode across coil); **X2:12 Speed Change 2** -> K3 coil A1, A2 -> 0 V ext (diode). Relay type "2 x 3TJ50".
- **2U** -> K2 NC aux contact and K1 NC aux contact (mutual interlock) -> K3 contact / K4 contact -> coils K2 (A1-A2) and K1 (A1-A2) with RC/varistor suppressors -> **2W**. [Reading: 2U -> K2 contact -> K3 contact -> K1 coil? Exact series order uncertain.]
- "X14 (RAC 2.2 only)" / "KDA/TDA mains connection for K1 and K2" -> 2U / 2W (contactor coil supply).
- Supply rails: 24V ext, 0V ext.

## PDF page 35 (manual page 105): 3.17 Master-slave operation

Two drives can be combined as master/slave to increase the drive power. In this case the master receives the speed [printed "sped"] set-point from the NC, the slave the torque set-point from the master. Master-slave operation can be used in a stiff mechanical connection for transmitting power to a common drive shaft.

**Important:** If the mechanical link between the slave and the master drive is broken, the speed of the slave drive increases to the maximum speed.

Fig. 84: Schematic of master-slave drive for increasing the drive power (tag "Master-Slave"): NC controller "Speed set-point" -> X4 of control unit (master); master X2 -> "Torque set-point" -> X4 of control unit (slave); each control unit: "Power" in, "Motor cable" to its motor, "Feedback" from motor to X3; master and slave motors drive a "Tandem gearbox" with output M_ab, P_ab.

With regard to hardware and software, the control units for the master-slave drive are identical and are defined as master and slave via parameters.

## PDF page 36 (manual page 106): Details of master drive

Fig. 85: Control inputs and signal outputs for master drive (tag "Master-Antrieb").
- X2 Signal outputs: **4 READY** ("&" with READY from slave); **9 TEMP WARN** ("&" with TEMP WARN from slave); **14 RF-SLAVE** ("1 = Controller enabling signal from controller", i.e. passes the controller enable to the slave); **22 0VM** and **21 N** = "Torque set-point for slave drive" (analogue).
- X2 Control inputs: 23 E-STOP (RAC only) = 1; 24 RF (controller enabling signal) = 1 ("1 = Controller enabling signal for slave drive" arrow); 25 RUN = 1; 29 OSCILLATE = 0; 3 AS (KDA/TDA only) = 1.
- Legend: "&" = AND logic operation.

Fig. 86: Parameters for master drive (tag "Para-Master")

| Parameter | Designation | Function | Value |
|---|---|---|---|
| A05 | FUNCT 1 | Analogue output N continuously outputs the current torque set-value. Not switchable! | <64> |

## PDF page 37 (manual page 107): Details of slave drive

Fig. 87: Control inputs and signal outputs for slave drive (tag "Slave-Antrieb"). X2 signal outputs: 4 READY ("&" with READY from master), 9 TEMP WARN ("&" with TEMP WARN from master). X2 inputs: 23 E-STOP (RAC only) = 1; 24 RF = "1 = Controller enabling signal for slave drive"; 25 RUN = 1; 29 OSCILLATE = 0; 30/31 PAR 1/PAR 2 "Change the parameter records"; 3 AS (KDA only) = 1.

Fig. 88: Parameters for slave drive (tag "Para-Slave")

| Parameter | Designation | Function | Value |
|---|---|---|---|
| A06 | FUNCT 2 | The voltage at analogue input E1/E2 (X4) is interpreted as a torque set-point. - Not switchable! | <8> |
| S R Q P16 | PQ-FUNCT | The voltage at analogue input E1/E2 (X4) is interpreted as a torque set-point. - Can be changed according to parameter record | <8> |

Note: The slave drive must be fitted with an analogue set-point interface (type code field "Speed set-point" = A). If a main spindle drive is programmed for torque input, the message SLAVE appears in the display. The drive can be switched over from torque input to speed input by means of the switchable parameter PQ FUNCT whilst the machine tool is operating.

## PDF page 38 (manual page 108): Matching the slave to the master drive

Fig. 89: Data for matching the slave, parameters for slave (tag "Slave-Anpassung"):
- Master control unit **X2:21 N** and **X2:22 0V_M** -> "Torque set-point" (crossed lines, "Polarity!") -> Slave control unit **X4:1 E1** and **X4:2 E2** (differential input amplifier).
- Formula: Value for A02 = sqrt(M06² - M05²) / (Rated current of control unit) * 10 V.

| Parameter | Designation | Function | Value |
|---|---|---|---|
| A02 | CMD VOLT | Match the input weighting of the slave to the torque set-point of the master | Enter value from above formula (V) |
| M05 | FLUX | Magnetizing current of master motor | Read value (A) and use in formula |
| M06 | CURRENT | Maximum value of motor current of master motor | Read value (A) and use in formula |

Note: The correct polarity (direction of rotation) must be observed when applying the torque set-point to the slave. Connecting 0 VM of the master and E2 of the slave gives the same direction of rotation. If the master and slave act in opposition (M indication shows 100 % for both) then the torque set-point must be reversed. The slave drive is matched to the analogue output N of the master via slave parameter A02 so that both drives produce the same torque. To do this, read the values of parameter M05 and M06 and calculate the necessary value for A02 from the formula in Fig. 89.

**Important:** The motor parameter values (M parameters) for the master and slave must be identical.

## PDF page 39 (manual page 109): Fig. 90 / Fig. 91 master-slave connection diagrams

Fig. 90: Connection diagram for master-slave drive with RAC (tag "Anschlußplan-RAC"). (Small print, read from a 300 dpi zoom.)
- Supply: +24V ext / 0V ext; relay **K10** coil switched by "ON" button "(mind. 0,6s)" (= at least 0.6 s).
- **RAC (MASTER), Parameter A05 = <64>**:
  - X4/2 E2, X4/1 E1 <- "Speed set-point" (differential input).
  - X2/25 RUN <- "Set-point input" [as printed; RUN = set-point enable]; X2/24 RF <- "Controller enabling signal"; X2/23 E-STOP <- "Emergency stop".
  - X15/1 ON and X15/2 ON bridged by contact K10 [printed "K10" next to the contact, uncertain]; X15/3 OFF and X15/4 OFF via a pushbutton NC contact (OFF button) in series with the "Emergency stop from controller" loop.
  - Outputs: X2/22 0V_M and X2/21 "Analogue output N" -> crossed ("Polarity!") -> slave X4/2 E2, X4/1 E1.
  - X2/14 RF SLAVE -> slave X2/24 RF.
  - X15/5 Bb and X15/6 Bb (ready contact; "Bb" = Betriebsbereit, ready for operation) -> wired to slave X15/3, X15/4 (OFF) so the slave's power is dropped if the master is not ready.
  - X2/9 TEMP WARN [pin number blurred in this figure, could read "X2/8"; Fig. 85 gives 9] -> relay K11 coil -> 0 V ext; X2/4 READY -> relay K12 coil -> 0 V ext (both with free-wheeling diodes).
- **RAC (SLAVE), Parameter A06 = <8>**: X4/2 E2, X4/1 E1 from master; +24 V ext -> X2/25 RUN and X2/23 E-STOP (bridged to +24 V); X2/24 RF from master X2/14; X15/1-X15/2 ON bridged; X15/3, X15/4 OFF from master Bb contact; X15/5, X15/6 Bb -> contact to "Emergency stop from controller" loop together with K18 [label "K18", uncertain]; X2/9 TEMP WARN -> K11 contact, X2/4 READY -> K12 contact -> "To controller".

Fig. 91: Connection diagram for master-slave drive with KDA/TDA (tag "Anschlußplan-KDA"): same as Fig. 90 without the X15 power circuit: KDA/TDA (Master) Parameter A05 = <64>: X4/2 E2, X4/1 E1 <- Speed set-point; X2/25 RUN <- Set-point input; X2/24 RF <- Controller enabling signal; outputs X2/22 0V_M, X2/21 analogue output N -> (Polarity!) slave X4/2, X4/1; X2/14 RF SLAVE -> slave X2/24 RF; slave X2/25 RUN <- +24 V ext; X2/9 TEMP WARN and X2/4 READY of the master -> K11, K12 coils -> 0 V ext; KDA/TDA (Slave) Parameter A06 = <8>; slave X2/9 TEMP WARN and X2/4 READY -> K11, K12 contacts -> "To controller".

## PDF page 40 (manual page 110): 3.18 EMERGENCY-STOP circuit (RAC)

The EMERGENCY-STOP circuit is built into the RAC control unit. It requires no additional circuitry for the power-down logic. In addition, energy released during braking is fed back into the mains. For the EMERGENCY-STOP circuit there are two distinct methods for stopping the drive and switching off the power:

*Internal set-point disconnection:* The speed set-point is set to zero internally after the E-STOP control input goes from 1 to 0. This can be effected immediately or via the speed set-point ramps (selected via function parameters). Power-down occurs when the motor shaft is stationary but no later than 5 secs. after the internal N set-point is set to 0. If the set-point is set to zero immediately, the drive brakes at maximum torque, just like when the drive follows steeply-set ramps. The motor coasts if the power is disconnected before the motor stops.

*Set-point input via the NC controller:* The OFF contact must be opened after the controller has stopped the drive. This disconnects the power.

Note: The EMERGENCY-STOP system must be selected and tested according to the installation in use.

Fig. 92: Control inputs for EMERGENCY-STOP circuit (tag "Not-Aus"): X2: 23 E-STOP (RAC only) = step "0 = EMERGENCY STOP circuit"; 24 RF = 1; 25 RUN = 1; 29 OSCILLATE = 0. NC controller contact -> **X15:3 / X15:4 OFF** (control unit power-off loop).

Note: The message "E-STOP" appears in the display when the E-STOP control input is at 0.

## PDF page 41 (manual page 111): Fig. 93 Parameters for activating the speed set-point ramps during an EMERGENCY-STOP (tag "Para-NOT-AUS")

| Parameter | Designation | Function | Value |
|---|---|---|---|
| A06 | FUNCT 2 | Ramp also active during E-Stop | <32> |

## PDF page 42 (manual page 112): 3.19 Performance during mains failure (RAC)

To prevent the main spindle from coasting during a mains failure, the RAC control unit can brake the asynchronous drive in this case also. This is effected either at maximum braking torque or at approximately 1/5 of the maximum braking torque by means of so-called direct-current braking.

*D.C. braking:* The energy regenerated during braking is converted into heat in the rotor of the motor. The braking time is approximately five times longer than that of controlled braking at maximum braking torque, but is adequate for many main spindle applications. The permissible amount of braking energy depends on the size of the motor. Braking the rotor's load moment of inertia from Nmax increases the rotor temperature by about 2 K (degC). Additional moments of inertia increase the temperature-rise linearly. When the motor is at operating temperature, the temperature-rise should not be more than 20 K, as otherwise this affects the service life of the bearings. During idling the thermal recovery time in this case is about 30 minutes. D.C. braking is included in the standard control unit and is activated in the standard parameter record.

*Braking at maximum torque:* In the RAC the energy regenerated during braking is converted into heat via a load resistor (additional bleeder). Due to short deceleration distances and times this type of braking is advantageous in servo applications. The deceleration time is the same as in regenerative braking. The permissible amount of energy depends on the controller:

| Controller | Permissible braking energy |
|---|---|
| RAC 3.1 | 120 kWs |
| RAC 2.2 | 240 kWs |

Maximum-torque braking is only possible in the RAC 2.2 and RAC 3.1 with the "Additional bleeder" option (type code field = Z1).

## PDF page 43 (manual page 113): Fig. 94 / Fig. 95

Fig. 94: Parameters for d.c. braking (tag "Para-Gleich-Brems")

| Parameter | Designation | Function | Value |
|---|---|---|---|
| M15 | MOT-FUNCT | No braking during mains failure | <0> |
| | | "D.C. braking" during mains failure | <16> |

Note: Braking is not possible with the set ramp or via the NC controller during d.c. braking.

Fig. 95: Parameters for braking at maximum torque (tag "Para-Brems-Max")

| Parameter | Designation | Function | Value |
|---|---|---|---|
| A06 | FUNCT 2 | Additional bleeder is monitored (RAC 2.2 only) - inoperative | <0> |
| | | - aktive [sic] | <256> |
| | | Ramp active if mains off | <32> |
| S R Q P16 | PQ-FUNCT | Motor brakes during mains failure | <0> |
| | | Motor has to be braked via the NC controller during a mains failure | <256> |

Note on bleeder monitor: So that the bleeder has time to cool down, braking operations with the additional bleeder must not be repeated in rapid succession. A cooling time of 10 to 15 minutes is necessary after braking at the maximum permissible energy. In addition, no large amounts of braking energy must be allowed to occur. If bleeder overheating is detected during braking, the RAC automatically changes over to d.c. braking if this is activated.

## PDF page 44 (manual page 114): Bleeder monitor features

- RAC 2.2: Activated bleeder monitoring prevents a device with overheated bleeder from being switched on again. The display then shows "BLEEDER". Activated bleeder monitoring also signals "BLEEDER" if the RAC 2.2 has been inadvertently installed without a bleeder.
- RAC 3.1: Bleeder monitoring is always operative when a bleeder is fitted. A missing bleeder is therefore not diagnosed!

During a mains failure, with both control units RAC 2.2 and RAC 3.1 (with additional bleeder option), the decision as to whether the drive is to be braked immediately or whether the internal ramp or the NC controller specifies the speed set-point for deceleration, can be made with the PQ FUNCT value in the switchable parameter records. NC braking enables the tool and workpiece to be separated without damage.

Possible fault message in the display: BLEEDER

---

## For the retrofit

What this part tells us (generic Indramat manual, so the CTX twin's actual parameter values and wiring must be confirmed from the drive's parameter printout and the Gildemeister wiring sheets in parts a/b of chapter 5):

**Keeping the original Indramat spindle drives under LinuxCNC**

- **Drive type**: Indramat AC main spindle control units RAC 2.2 / RAC 3.1 (or KDA/TDA on a common DC bus); this machine has integrated spindle motors (1MB frameless type per Fig. 50; "18 kW" per the job description, not on these pages). Check the type plate for the option fields: "Speed set-point" = A (analogue), "2nd encoder input" = P, "Additional interface" = I (incremental encoder output, IGS) or L (SERCOS).
- **Speed command**: analogue +/-10 V differential into X4:1 E1 / X4:2 E2. Weighting A02 CMD VOLT (6.0...10.0 V) = A01 MAXRPM. For the C axis / low speed, EXT POS (X2:37) = 1 switches the weighting to P15 P-MAXRPM (finer resolution) and optionally to the second differential input X4:5/6 (PQ-FUNCT <1>). LinuxCNC: a differential +/-10 V DAC (e.g. Mesa 7I77 analog out or similar) drives E1/E2; make sure the spindle "RUN" (X2:25) and "RF" (X2:24) are both 1. The set-point is blocked for 200 ms after RF goes 1 (motor magnetizing), so delay spindle-at-speed logic accordingly.
- **Spindle encoder / C-axis feedback**: the drive evaluates the spindle encoder on X5 (ROD 420/426 compatible, 5 V TTL, 128...8192 lines, or Indramat high-resolution encoder 128 mm/256 teeth or 256 mm/512 teeth). With the high-resolution encoder, the "incremental encoder output" (IGS) option generates simulated A/B/Z for the NC with P24 IGS LINES (1...16383 lines) and reference offset P22. That IGS output (or a passive tap of a ROD-type encoder) is what LinuxCNC should count for spindle position, threading (spindle-synchronized motion needs index + quadrature) and C-axis position loop. For C-axis, LinuxCNC closes the position loop and sends a velocity command through E1/E2 (or E3/E4 with EXT POS = 1) to the drive in speed mode; select a stiff P/Q/R/S parameter record via PAR 1/PAR 2 (X2:30/31).
- **Twin part transfer / synchronous spindles**: nothing machine-specific here. The drive offers master-slave torque sharing (3.17) for a tandem gearbox only, not spindle-to-spindle angle sync. For a synchronized hand-over between the two spindles LinuxCNC must run both spindles as position-controlled axes (each with its own encoder feedback and analogue command) and gear them in software.
- **Orientation**: the drive has built-in positioning (3.6.4): RUN = 0 and POS 1 / POS 2 (X2:26/27) select positions A08 [uncertain]/A09/A10 relative to A07 OFFSET from the encoder zero pulse, speed P18 POS RPM, window P21 P-WINDOW, gain P17 POS GAIN; INPOS reported on X2:7. Simplest retrofit: keep this (LinuxCNC drives POS1/POS2/RUN as outputs and reads INPOS); alternative: orient from LinuxCNC via the C-axis position loop.
- **Enable / ready / fault signals**: inputs E-STOP X2:23 (RAC), RF X2:24, RUN X2:25, AS X2:3 (KDA/TDA). Outputs READY X2:4, TEMP WARN X2:9, SPINDLE CONTR X2:15 (0 = spindle encoder/mechanical fault), LIMIT X2:10, N<MIN X2:6, INPOS X2:7, 90% LOAD X2:8, LOAD LIMIT X2:13. On RAC the power section is switched by X15 ON/OFF contacts and reports Bb (ready) on X15/5-6; the E-stop chain must open X15:3/4 after the drive has braked (power-down at the latest 5 s after N set-point 0). A06 <32> keeps the ramp active during E-stop. Analogue outputs: M (X2:20, load 0...10 V) and N (X2:21, speed) vs 0VM X2:22 can feed a load meter / speed readback in LinuxCNC.
- **Temperature monitoring**: motor thermistor on X16 T1/T2 is evaluated by the drive: TEMP WARN (X2:9) at 145 degC winding (MOTWARN), switch-off at 155 degC (MOTTEMP); controller heatsink KKWARN limits torque to 50 % and trips after 30 s (KKTEMP); no sensor -> NO TEMP trip after 10 min at >= 10 % load. LinuxCNC only needs to read TEMP WARN and READY.
- Gear change (3.15) and two-motor changeover (3.16) use SPEED 1/2 and SPEED CHANGE 1/2; on an integrated-motor-spindle lathe they are presumably unused [assumption, check parameter list: P14 G-CHANGE].
- Mains failure: D.C. braking (M15 <16>) or bleeder braking (RAC 2.2/3.1 option Z1, 240/120 kWs); P16 <256> lets the NC brake the spindle during mains failure.

**Replacing the drives**

- Record all A, M, P, Q, R, S parameters of both drives first (motor data M05 FLUX = magnetizing current, M06 CURRENT = max. motor current, A01 MAXRPM, encoder settings A11, gear P12/P13, limits A12...A18) - these are the data a new vector drive needs for the existing integrated spindle motors.
- The new drive must support an induction (asynchronous) motor with closed-loop vector control from the existing motor encoder (Indramat high-resolution sine/cosine type on the 1MB motor - check signal format and whether a suitable encoder interface exists) and a separate spindle position encoder, or the NC closes the C-axis loop with the spindle encoder.
- It must provide: +/-10 V or fieldbus speed command, enable/ready/fault, motor thermistor input (155 degC class winding, warn at 145 degC), and a braking resistor or regenerative supply (the RAC regenerates to the mains; KDA/TDA via the supply module).
- The functions that LinuxCNC can take over from the drive: orientation (POS1/POS2), max-speed limits (LIMIT 1/2/4), load signalling, spindle-monitor plausibility (compare motor speed vs spindle encoder), and the spindle-to-spindle sync for part transfer.
