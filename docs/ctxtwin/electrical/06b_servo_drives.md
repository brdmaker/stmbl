# Gildemeister CTX twin, electrical documentation: 6. Servo drives (part b) - Indramat DDS 2.1 with ANALOG interface, commissioning and diagnostics, PDF pages 1-36

Source: `76fa33e2-6_Servo_drives_b_20201029_0001.pdf` (chapter "6 Servo drives", part b, 36 scanned pages). These pages are **not Gildemeister drawings**: they are pages 41-76 of the Indramat manual bound into the machine documentation. Footer on every page: "INDRAMAT - **DDS 2.1 and MDD with ANALOG interface - 209-0069-4315-03 - December 23, 1993**" (some pages print the revision as **209-0069-4315-02**, same date: printed pages 53, 55, 57, 59, 61, 63, 65, 67, 69, 71, 73, 75). There is no Gildemeister title block, no machine sheet number and no cabinet device tag (-K.., -X.. of the machine) on these pages.

Original language: **English** (Indramat's own English edition). No German text appears except the bilingual language-selection screen (quoted with translation).

Running headers: "5. Operating the drive controller during commissioning and diagnostics" (printed pages 41-51), "6. Commissioning procedure up to initial start-up of the AC servo drive" (52-57), "7. Commissioning the functions of the digital AC servo drive" (58-76).

Indramat drawing references (small print in figure corners) are quoted as printed, e.g. "ENA3A-Abb22/InbetrDDS".

Notation: [uncertain] = reading not sure, [unreadable] = cannot be read. The screen photos (DOS terminal screenshots) are low-contrast; numeric values read from them are marked where doubtful. Note: the values in the screenshots are Indramat examples, not the settings of this machine.

## Page index

PDF page n = printed manual page 40 + n.

| PDF page | Printed page / figure | Content |
|---|---|---|
| 1 | 41, Fig. 32 | Function levels of the user interface (menu tree of the parametrization and diagnostic program) |
| 2 | 42, Fig. 33-35 | Screens: Absolute encoder error, Program start, Main menu |
| 3 | 43, Fig. 36-38 | Screens: DRIVE STATUS, LANGUAGE SELECTION, safety note "Drive Parametrization" |
| 4 | 44, Fig. 39-41 | Screens: PARAMETER menu, OPERATING MODES, SCALING (+ help text) |
| 5 | 45, Fig. 42-44 | OPERATING MODES, SCALING for absolute encoder motors (+ help); INCREMENTAL ENCODER SETUP |
| 6 | 46, Fig. 45-47 | Incremental encoder setup help; ABSOLUTE ENCODER SETUP (+ help) |
| 7 | 47, Fig. 48-50 | MODULO PARAMETERS (+ help); ERROR REACTION |
| 8 | 48, Fig. 51-53 | TORQUE/CURRENT LIMITS (+ help); GAIN PARAMETERS |
| 9 | 49, Fig. 54-56 | Gain parameters help; AMPLIFIER/MOTOR PARAMETERS; SAVE PARAMETERS |
| 10 | 50, Fig. 57 | LOAD PARAMETERS |
| 11 | 51 | 5.2 Saving data / loading parameters (procedures) |
| 12 | 52, Fig. 58 | 6. Commissioning, 6.1 checks with power disconnected; configuration rating plate |
| 13 | 53 | Wiring, earth connections, power cables, terminals |
| 14 | 54, Fig. 59 | Bus connecting cable and end connector; screening; transformer; E-STOP |
| 15 | 55 | 6.2 Checks with control voltage; 6.3 Entering parameters for initial start-up |
| 16 | 56 | Command value parameters, error reaction, gains, overload; 6.4 Powering up; 6.5 Brake check |
| 17 | 57 | 6.6 Initial start-up with a battery power source |
| 18 | 58 | 7. Commissioning the functions, 7.1 Error reaction (package-wide errors) |
| 19 | 59, Fig. 60 | Error reaction table 0/1/2 |
| 20 | 60, Fig. 61 | Holding brake reaction under different error reactions (timing) |
| 21 | 61 | 7.2 Safety lockout (drive interlock open) |
| 22 | 62, Fig. 62 | Connector X3 pinout, safety lockout inputs AS+/AS-, feedback ASQ |
| 23 | 63, Fig. 63 | Time sequence for safety lockout activation |
| 24 | 64, Fig. 64 | 7.3 Holding brake function; terminal block X6 |
| 25 | 65, Fig. 65-66 | Brake timing vs controller enable and vs error reaction |
| 26 | 66 | Manual brake release, brake check, error 68 |
| 27 | 67, Fig. 67 | 7.4 Operational status "Bb" (Ready) contact X3-6/7 |
| 28 | 68 | Status displays bb / Ab / AF; 7.5 Temperature monitoring, warnings 50/51 |
| 29 | 69, Fig. 68 | TVW warning output, X13/15 analog interface and X6; errors 18/19 |
| 30 | 70 | 7.6 Velocity command matching (scaling) |
| 31 | 71, Fig. 69-70 | Direction of rotation; input signal limits; ANALOG interface wiring to the NC |
| 32 | 72 | 7.7 Command value smoothing; 7.8 indirect relative position sensing |
| 33 | 73, Fig. 71 | Incremental encoder emulator DAE 1.1 output X14 pinout; resolution settings |
| 34 | 74, Fig. 72 | Max. motor speed vs line count (504 kHz limit); marker pulse offset setting |
| 35 | 75, Fig. 73 | Checking marker pulse offset; keyway position on MDD motors |
| 36 | 76 | 7.9 Indirect absolute position sensing (DAA 1.1, SSI), absolute encoder monitoring, error 76 |

---

## PDF page 1 (printed page 41): Fig. 32 "Function levels of the user interface"

Menu tree of the Indramat PC parametrization and diagnostic program (runs on a PC terminal emulator connected to the DDS 2.1 serial port). Boxes with keys (white fields = keys on the keyboard):

- PROGRAM START [Enter] -> MAIN MENU. MAIN MENU [ESC] -> PROGRAM END.
- MAIN MENU key 1 -> DRIVE STATUS [ESC back]; key 3 -> LANGUAGE SELECTION [ESC back]; key 2 -> "Safety note 'Drive Parametrization'" [ESC back / W] -> PARAMETERS.
- PARAMETERS [ESC -> main menu]:
  - 1 -> OPERATION MODES SCALING ([?] -> HELP TEXT)
  - 2 -> POSITION EVALUATION 1) 2) ([?] -> HELP TEXT)
  - 3 -> ERROR REACTION
  - 4 -> TORQUE REDUCTION ([?] -> HELP TEXT)
  - 5 -> GAIN PARAMETERS ([?] -> HELP TEXT)
  - 6 -> AMPLIFIER AND MOTOR PARAMETERS
  - 7 -> SAVE PARAMETERS
  - 8 -> LOAD PARAMETERS
- Footnotes:
  1) If the drive controller is equipped with an incremental encoder emulator Type **DAE 1.1**, the "INKREMENTAL ENCODER SETUP" [sic] menu will be displayed.
  2) If the drive controller is equipped with an absolute encoder emulator Type **DAA 1.1**, the "ABSOLUTE ENCODER SETUP" menu will be displayed.
- Note: The fields with a white background correspond to the keys on the keyboard. Ref. ENA3A-Abb22/InbetrDDS.

## PDF page 2 (printed page 42)

**Absolute encoder error (Fig. 33)** - screen text:
"ABSOLUTE ENCODER ERROR. Initialized internal position: 2939.4812 / Stored internal position: 2602.1660 / Difference internal position: 337.3152 [values uncertain]. The initialized position is outside the 'Position Monitoring Window'. The positions above are only valid when the error occurs the first time. The axis has been driven without power or the input for the parameter 'Position Monitoring Window' is near by 0. If you are sure that both didn't happen, then the encoder has a mechanical defect and you must not drive this axis until you got information from INDRAMAT-Service. CAUTION: after resetting the error the axis can be started, but the position is not the same since the last power down. You can reset the error by pressing S1 at DDS."
Terminal status line: "ALT-F10 HELP | VT-100 | FDX | 9600 N81 | LOG CLOSED | PRT OFF | CR | CR" (i.e. serial link 9600 baud, 8N1, VT-100 emulation).

**Program start (Fig. 34)**: "To start Parametrization and Diagnostic Program, press ENTER".

**Main menu (Fig. 35)**: "INDRAMAT - Control System and Servo Drive - MAIN MENU. Drive Status: 100 Ready for main power (Bb closed). This parametrization and diagnostic program is designed to adapt the DDS 2.1 Servo Drive with an analog interface module to both the control unit and the mechanics of the machine. In addition, diagnostic messages are provided. 1 -> Drive Status, 2 -> Parameters, 3 -> Language Selection / Sprachauswahl, ESC -> Exit Program. Select a number from the menu above or ESC."

## PDF page 3 (printed page 43)

**Drive status (Fig. 36 "DRIVE STATUS" menu)**:

| Item | Value shown |
|---|---|
| Drive Status | 100 Ready for main power (Bb closed) |
| Motor Type | mdd071b-n-030-n2g [uncertain] |
| Amplifier Type | DDS2.1-W050-R [uncertain, last letter] |
| Software Version | DSM 2.1-E11-01.06 (16.11.93) [uncertain] |
| Active Operation Mode | Velocity Regulation |
| Velocity Command Value (RPM) | 0.0000 |
| Velocity Feedback Value (RPM) | 0.0000 |
| Current Command Value (A) | 0.00 |
| Motor Torque (NM) | 0.00 |
| Relative Motor Torque (%) | 0.00 |
| Rotor Position Revolutions / Angular (degr) | 0.1302 / 49.7571 |
| I/O - Status DAE / DAA: TVW, RF, Ah, Ired1, Ired2, Marker Pulse | 0, 0, 1, 0, 0, 0 |

"ESC -> Return to main menu".

**Language selection (Fig. 37)**: "In diesem Menue koennen Sie zwei Sprachen auswaehlen. Fuer DEUTSCH druecken Sie '1'!" (In this menu you can select two languages. For GERMAN press '1') / "In this menu you can select two languages. For ENGLISH press '2'!" / "Wahl / Selection:" / "Press ESC to return to main menu".

**Safety note "Drive Parametrization" (Fig. 38)**: "WARNING! Since changing the gains on the drive can cause severe oscillations and instability, improper gain adjustments may cause severe damage to the drive and/or the machine. Steps to operate the drive in a system: Use application parameters from control manual and system documentation. Use gain parameters from system parameter list. You read the warning! -> continue with 'c' or 'ESC' to return to main menu."

## PDF page 4 (printed page 44)

**Parameter menu (Fig. 39)**: Application Parameters: 1 -> Operation Mode, Scaling; 2 -> Encoder Emulation Setup; 3 -> Error Reaction; 4 -> Torque (Current) Limits. Drive Parameters: 5 -> Gain Parameters; 6 -> Motor and Drive Data. Parameter Management: 7 -> Save Parameters to Disk; 8 -> Load Parameters from Disk.

**Operating modes, scaling - feedback versions with DSF or RSF (Fig. 40)** (DSF / RSF = motor feedback types):

| Parameter | Example value |
|---|---|
| Analog Output Channel 1 | Current Command Value |
| Analog Output Channel 2 | Velocity Feedback Value |
| Operation Mode (1 = Torque / 2 = Velocity Regulation) | 2 |
| Bipolar Velocity Limit Value (RPM) | 4000.0000 |
| Overload Factor (%) | 400 |
| Maximum Command Value for Maximum Velocity (V) | 10.0 |
| Velocity at Maximum Command Value (RPM) | 4000 |
| Command Value Smoothing Filter Time Constant (msec) | 0.25 |
| Position Data Scaling at the Analog Outp. (dgr/10V) | 360.0 |
| Velocity Data Scaling at the Analog Outp. (RPM/10V) | 4000 |
| Velocity Loop Monitoring (0 = OFF / 1 = ON) | 1 |

**Help text (Fig. 41)**, input limits:

| Parameter | Limits |
|---|---|
| Operation Mode | 1 or 2 |
| Bipolar Velocity Limit Value | 0 ... 50000 |
| Overload Factor | 0 ... 400 |
| Maximum Command Value for Desired Velocity | 0 ... 10 |
| Velocity at Maximum Command Value | 0 ... 60000 |
| Command Value Smoothing Filter Time Constant | 0.25 ... 1000 |
| Position Data Scaling at the Analog Output | 0.1 ... 1474560 |
| Velocity Data Scaling at the Analog Output | 1 ... 65000 |
| Position Monitoring Window (only with DAA) | 0 ... 4095.9 |
| Position Output (only with DAA) | 0 or 1 |

"The command scaling is determined by the value input at Max. Command Value and the corresponding velocity. For example, 2000 RPM at 8V corresponds to the inputs: 'Maximum Command Value for Desired Velocity' = 8 and 'Velocity at Maximum Command Value' = 2000. The input may not be greater than 60000RPM at 10V or 3000RPM at 0.5V !!"

## PDF page 5 (printed page 45)

**Operating modes, scaling - feedback versions with absolute encoder options (DSF + MTG or RSF + IDG) (Fig. 42)**: same screen and same example values as Fig. 40.
**Help text (Fig. 43)**: identical to Fig. 41.

**Incremental encoder setup (Fig. 44)**: "NOTE: These parameters can only be changed when the drive is not enabled (RF is not applied). The line count that has been entered must agree with the control unit. The maximum line count may also be limited by the maximum motor speed. See DDS 2.1 Users Manual."
- Incremental Encoder Line Count (1 .. 262144): 16383 [uncertain, possibly 16384]
- Marker Pulse Offset (0 .. 359.9 degrees): 0.0
- Rotor Position: Revolutions 0.1302, Angular (degrees) 49.7571

## PDF page 6 (printed page 46)

**Incremental encoder setup help (Fig. 45)**: "Linear motion is evaluated indirectly by the motor incremental encoder emulator which is representative of the motor rotor position. It is possible to shift the angle of the marker pulse clockwise looking into the motor shaft so that it is opposite of the physical home switch. This angle can be adjusted from 0 to 359.9 degrees in 0.1 degree increments. The maximum speed of the motor is derived from the input decoder line count by the following calculation: Maximum Speed of the Motors = 60 * 500000 [uncertain; page 74 gives 504000] / Encoder Line Count (RPM)."

**Absolute encoder setup (Fig. 46)**: screen photo (its title reads "INCREMENTAL ENCODER SETUP" [as printed]); same notes; line count 16383 [uncertain], marker pulse offset 0.0; Rotor position revolutions 787.0720 [uncertain], angular 26.2251 [uncertain].
**Help (Fig. 47)**: identical to Fig. 45.

## PDF page 7 (printed page 47)

**Modulo parameters (Fig. 48)**: "NOTE: These parameters can only be changed when the drive is not enabled (RF not applied)."
- Gear Ratio: Input Revolutions: 5
- Gear Ratio: Output Revolutions: 1
- Modulo-Count Direction (0 = positive / 1 = negative): 0
- Reference-Position (0 .. 359.9999 degrees): 100.0000 [uncertain]
- Motor Position: Revolutions 0.5000, Angular (degrees) 179.9996
- Message: "Parameter is outside the input limits"

**Help (Fig. 49)**: "The emulated position is a value between 0 .. 359.9 degrees in relation to an external axis with the gear ratio: gr = Output Revolutions / Input Revolutions. For the turns the input limit is 1 .. 9 999 999. In addition to this the maximum values for the ratio are: gr_max = 15.99 and (1/gr)_max = 4096. Changing the count direction: Be careful using this parameter because of the danger of positive feedback. Make sure the lag monitoring of your control unit is active. positive count direction: increasing absolute position values correspond to clockwise rotation at motor shaft. negative count direction: increasing absolute position values correspond to counter-clockwise rotation at motor shaft."

**Error reaction (Fig. 50)**:
- 0 -> The drive automatically switches to velocity loop regulation and provides a velocity command value of 0. After 500 msec. the motor torque is disabled and an error signal is sent to the power supply on the X1 (X1.2 [uncertain]) connector.
- 1 -> The drive reacts the same as in error reaction 0, with the exception that the drive does not signal the power supply that an error has occurred.
- 2 -> The drive immediately switches its torque to 0. No signal is provided to the power supply.
- Current selection: 0 -> change to: ___. "Press ENTER to confirm change or 'ESC' to return to main menu".

## PDF page 8 (printed page 48)

**Torque/current limits (Fig. 51)**: "The motor torque is adjusted in this menu. There are 2 Ired control inputs which will allow 4 different current limit values. These values can be in a range from 0% to 400%. 100% = Continuous Motor Current (Torque)."

| Ired2 | Ired1 | Adjustment | Example |
|---|---|---|---|
| 0 | 0 | Full Torque | - |
| 0 | 1 | Current Limit 1 | 24 % [uncertain] |
| 1 | 0 | Current Limit 2 | 50 % [uncertain] |
| 1 | 1 | Current Limit 3 | 25 % [uncertain] |

**Help (Fig. 52)**: "The Ired inputs allow four peak current adjustments to be defined. The torque values are adjusted for continuous torque at stand still. Valid inputs are in the range 0 to 400. The activation of the torque limit takes place by selecting the inputs Ired1 and Ired2 in a binary fashion. **0 volts on these inputs represents a logical '0' and +12..32 volts represents a logical '1'.**"

**Gain parameters (Fig. 53)**: "WARNING: See DDS 2.1 Users Manual Before Adjusting Gains!" Analog Output Channel 1 -> Current Command Value, Channel 2 -> Velocity Feedback Value.

| Parameter | Example value |
|---|---|
| Current Loop Proportional Gain-1 (V/A) | 30.00 [uncertain] |
| Velocity Loop Proportional Gain Kp (mAs/rad) | 1800 [uncertain] |
| Velocity Loop Integral Reaction Time Tn (ms) | 0.0 [uncertain] |
| Velocity Loop Derivative/Integral Gain (mAs2/rad) | 0 |
| Velocity Loop Derivative/Proport. Gain (uAs2/rad) | 0 |
| Smoothing Time Constant (usec) | 400 [uncertain] |
| Reset to Standard Parameters | -> ENTER |

## PDF page 9 (printed page 49)

**Gain parameters help (Fig. 54)**, input limits: Current Loop Proportional Gain-1: (1..15.93)*Curr.Loop Base Gain; Velocity Loop Proportional Gain Kp: 0 ... 65000; Velocity Loop Integral Reaction Time Tn: 0.1 ... 6500; Velocity Loop Derivative/Integral Gain: 0 ... 65000; Velocity Loop Derivative/Proport. Gain: 0 ... 35000; Smoothing Time Constant: 250 ... 65000. "Reset to Standard Parameters: Standard gain values are selected from the motor feedback. The original gain values from the motor are restored with this command."

**Amplifier/motor parameters (Fig. 55)** (example):

| Amplifier Parameters | | Motor Parameters | |
|---|---|---|---|
| Amplifier Type | DDS2.1-W050-R [uncertain] | Motor Type | mdd071b-n-030-n2g [uncertain] |
| Amplifier Peak Current (A) | 50.0 | Nominal Torque (Nm) | 4.4 |
| Amplifier Continuous Current (A) | 40.0 [uncertain] | Maximum Velocity of Motor (RPM) | 3100.0000 [uncertain] |
| Effective Peak Current (A) | 32.8 | Mass Moment of Inertia (kg m2) | 0.00089 [uncertain] |
| Effective Continuous Current (A) | 29.2 [uncertain] | Motor Torque Constant (Nm/A) | 0.600 [uncertain] |
| | | Peak Motor Current (A) | 32.8 |
| | | Nominal Motor Current (A) | 7.3 [uncertain] |

**Save parameters (Fig. 56)**: fields Machine-Manufacturer, Machine-No., Machine-Type, Axis, Date, inspected by. "to save parameters: - if you are using INDRAMAT's 'DDS2PC Terminal Emulation Software' as emulator hit ALT-S otherwise follow instructions - put terminal emulator in receive-mode (Download) - enter filename (to save parameters to) - use text or ASCII download format - then press switch S1 on the DDS2 to start downloading - after the last character is received ('$'), exit receive-mode - hit one more key to return to main menu. NOTE: to exit this menu without saving, press S1 on the DDS and hit any key [partly unreadable]."

## PDF page 10 (printed page 50)

**Load parameters (Fig. 57)**: "If you are using INDRAMAT's 'DDS2PC Terminal Emulation Software' for your terminal emulator, first hit 'y' to continue loading (or any key to escape) then press ALT-L, otherwise follow the instructions below. To load parameters: - Press 'y' to continue with loading (H1 on DDS must show '--') - Put terminal emulator in send-mode (Upload) - Enter filename (to load parameters from) - Use text or ASCII format - Start uploading (from terminal emulator). Continue with loading parameters? yes -> 'y'". Rest of page blank.

## PDF page 11 (printed page 51): 5.2 Saving data

- Saving data using the personal computer. Note box: "The file format is: text (ASCII)."
- Procedure: 1. In "PARAMETERS" call submenu "SAVE PARAMETERS". 2. Enter data to identify the machine (Fig. 56); up to 55 alphanumeric characters per field; complete with <ENTER>. 3. Set the emulation program to receive mode (download). 4. Enter a file name. 5. Press button **S1 on the DDS 2.1** - starts data transfer. 6. Complete the transfer after the last "$" character. 7. Hit any key to return.
- Note box: edit the parameter file only with a clear-text editor (DOS Editor, Norton Editor). Changes allowed before the ":" control character; parameter values after ":" may be changed; the "$" is the end-of-file character and must neither be removed nor changed. Example printout in Appendix, Section 11.5.
- Loading parameters to the drive (for axes with identical parameters): 1. "PARAMETERS" -> "LOAD PARAMETERS". 2. Emulation program to transmit mode (upload). 3. Select a parameter file. 4. Start transmission. 5. Message "hit any key to continue" when done.

## PDF page 12 (printed page 52): 6. Commissioning procedure up to initial start-up of the AC servo drive

6.1 Checks to be run with power disconnected.
- Mains power requirements: check that the mains supply suits the installed power supply module (see power supply module documentation).
- Drive components: must suit the connection voltages (Project Planning Doc. No. **209-0069-4317-00**).
- Note box: check the configuration of the drive controller (Fig. 58): the data on the configuration rating plate must correspond to the components installed; otherwise drive and machine mechanics may be destroyed.

**Fig. 58 Configuration rating plate** (example, "SYSTEMCONFIGURATION"):

| Plate line | Meaning |
|---|---|
| DDS 2.1-W050-DA01-00 | complete drive controller type code (configuration) |
| DDS 2.1-W050-D | basic unit (power section, 50 A) |
| DSM 2.1-E11-01.RS | software module (plug-in, firmware) |
| DAE 1.1 | ANALOG interface with incremental encoder emulator (slot U1 [uncertain which slot]) |
| COVER | blank slot |
| COVER | blank slot |
| COVER | blank slot |

Ref. ENA3A-Abb.41InbetrDDS.

## PDF page 13 (printed page 53)

- Wiring: check for short circuits, interruptions, reverse polarity and conductor cross-sections.
- Earth connections: earth components as in INDRAMAT connection diagrams; earth each motor to its drive module; earth each drive module separately to the power supply module. Note: "The earthable point of the power supply module is the common reference point for all drive components." Connect it to the source earth. Danger box: these earth connections are operational earths with a protective function, do not disconnect.
- Conductive connection between drive controller frames and cabinet rear wall (EMC).
- Power cables: twist the cables from the drive controllers to the motor or use a four-core cable (3x phase, 1x earth). Cross-sections per "VDO 0113" [as printed; VDE 0113].
- Power connections to additional modules: twisted and short.
- Power connections between servo drive modules: normally two conductor rails (DC bus); otherwise two 16 mm2 twisted conductors, max. 1 m.
- Terminals and plug connectors: proper contact, firmly connected, sub-miniature connectors screwed down.

## PDF page 14 (printed page 54)

**Fig. 59 Arrangement of bus connecting cable and end connector**: power supply module on the left, digital AC servo drives to the right, two DC-bus conductor rails across the top; a flat ribbon bus connecting cable runs across the modules (black core at the bottom); an end connector is plugged into the last drive. Ref. ENA3A-Abb42/InbetrDDS.
- Bus connecting cable: used for control voltage supply and coordination of monitoring devices. Connect with the black core at the bottom. For control voltage monitoring plug the end connector into the drive controller furthest from the power supply module. Note: the end connector is part of the accessories supplied with the power supply module.
- Screening: shields of motor feedback cable and thermistor conductors to the frame earth terminal of the drive controller.
- Connection of a power transformer: transformer output voltage must match the power supply module connection voltage.
- E-STOP: on E-STOP a malfunction of the servo drive cannot be totally excluded, therefore the circuit must be broken (see power supply module documentation). Until the drive has stopped, uncontrolled motions must be reckoned with; precautions at the higher plant level. Axes needing immobilization after stop need a holding brake; MDD motors can be supplied with an optional holding brake.

## PDF page 15 (printed page 55)

6.2 Checks with control voltage applied: switch on the signal conditioning voltage supply at the power supply module (it provides the signal voltage for the drive controller). Note box: "DO NOT switch on the main contactor K1 for powering up." (K1 = main contactor in the Indramat power supply module wiring, not necessarily the machine tag.)
- "H1" status indicator: drive in working order when H1 displays "Bb" (Ready for powering up); otherwise see Section 9.
- Power circuit ON/OFF switching sequence: guaranteed when the "Ready" contact in the power supply module is installed according to INDRAMAT diagrams. Do not power up until the power supply module's "Ready" contact is closed.

6.3 Entering parameters for initial start-up. Pre-conditions: auxiliary equipment connected (Section 4); familiar with the program (Section 5).
- Activate the "velocity loop" mode in "OPERATING MODES, SCALING" (Fig. 40). Danger: torque loop mode by accident -> drive accelerates uncontrollably at the smallest command.
- Limit maximum velocity: "bipolar velocity limit value" to approx. 10% of maximum NC working motor speed.
- Limit maximum torque to 100% (Section 7.11).

## PDF page 16 (printed page 56)

- Set command value parameters: 1. "maximum command value for maximum velocity" = max. output voltage of the battery power source (e.g. 10 V). 2. "velocity at maximum command value" = "bipolar velocity limit value". 3. "command value smoothing filter time constant" = 0.250 (makes the parameter ineffective).
- Select error reactions: "ERROR REACTION" menu, activate "0".
- Set gain parameters: "GAIN PARAMETERS", "reset to standard parameters" (standard values stored in the motor feedback are loaded into the software module).
- Set short-time operation torque: overload factor from selection list Doc. No. **209-0069-4302-00 IDE**, enter under "overload factor".

6.4 Powering up. Danger box: be prepared for uncontrolled drive motion; E-STOP button within reach; safety limit switches far enough from fixed stops; check the E-STOP signal chain.
- Switch on power: drive shows "Bb" (ready); switch on power via the power supply module.
- Diagnostics display changes to "Ab": control and power circuits ready; otherwise Section 9.

6.5 Checks after switching on power - holding brake: the drive module applies the brake-release signal at the same time as the controller enable signal; check that axis locking is cancelled.

## PDF page 17 (printed page 57): 6.6 Initial start-up with a battery power source

The axis can be run without an NC using a battery power source (Section 4.2): control not installed, control failed, optimization work. Warning box (running outside a secondary control loop is beyond prescribed use): 1. nobody in range of axis motions; 2. max. velocity such that the operator can stop by cancelling "RF" (controller enable); 3. safety distance between E-STOP limit switch and mechanical stop.
Procedure: connect battery source per Section 4.2, Fig. 30; apply controller enable; de-activate "drive halt" (Section 7.12); apply a small command. **Positive voltage at differential input E1 against E2 = motor clockwise (looking at the motor shaft); negative = counter-clockwise.** Check in "DRIVE STATUS" (Velocity Command Value, Velocity Feedback Value). Note: do NOT change direction by swapping two motor power cores (may destroy the motor). Cancel torque and velocity limitation afterwards.

## PDF page 18 (printed page 58): 7. Commissioning the functions of the AC servo drive with ANALOG interface, 7.1 Error reaction

Error reactions are preselectable per drive; reactions 1 and 2 allow completing machining when errors occur in other axes (e.g. loading axes). Errors causing a reaction in the entire drive package (all drives on one power supply module):

| Code | Error |
|---|---|
| 22 | Motor encoder error |
| 24 | Overcurrent |
| 60 | Bridge fuse |
| 61 | Earth-fault fuse |
| 67 | Faulty hardware synchronization |
| 69 | ±15 Volt error |
| 70 | ±24 Volt error |
| 71 | ±10 Volt error |
| 72 | +8 Volt error |
| 73 | Voltage supply driver stage |

Danger: these errors prevent a controlled stop. Package reaction: 1. affected drive sends an error signal to the power supply module; 2. supply module cuts the power; 3. "undervoltage" signal from the supply is ignored by unaffected drives, which can be stopped via the NC (depending on residual DC-link energy); 4. deceleration can be shortened with the supply module's "plug braking" function.

## PDF page 19 (printed page 59): Fig. 60 Setting error reactions on the DDS 2.1

Note: "plug braking" affects all drives in one package irrespective of their error reaction. Other errors: any reaction selectable per axis.

| Error reaction | Drive reaction |
|---|---|
| 0 | Automatic switch to velocity loop mode; braking at maximum acceleration; torque disable after 500 ms; opening of the "bb" Ready contact **X3-6/7**; error signal to the power supply module, which cuts the power |
| 1 | Same as 0; no error signal to the power supply module; other drives on the same supply remain operative |
| 2 | Undelayed torque disable (axis coasts to a halt); undelayed braking through motor holding brake (if fitted); no error signal to supply; unaffected drives remain operative; "Ready" contact Bb X3-6/7 open. Note: not recommended for motors with holding brake (>20,000 revolutions against a closed brake wears it out) |

Note box: irrespective of the reaction the error can be evaluated by the NC lag (following-error) detection and by the **Bb contact (X3 pin 6/7)**, allowing rapid shutdown by the NC.

## PDF page 20 (printed page 60): Fig. 61 Reaction of the holding brake under different error reactions

Timing diagram, signals Error, Speed command value 0, Motor torque, Holding brake closes, Controller enable (RF), time axis 0...500 ms then 0...100 ms. Footnotes:
1) Error reaction 2: brake closes immediately, torque disabled at once.
2) Error reaction 0 or 1: brake closes when speed < 10 rpm; after 400 ms at the latest even when speed > 10 rpm (torque off at 500 ms).
3) After the error is removed and RF reset, the brake releases automatically; the drive provides a command value 0 for 100 ms for safe brake opening. Ref. ENA3A-Abb43/InbetrDDS.

## PDF page 21 (printed page 61): 7.2 Safety lockout (drive interlock open)

DDS 2.1 controllers have a safety lockout to stop an axis starting up accidentally: when activated, the control electronics of the power output stage are disconnected by a relay contact. Warning: it is NOT intended to stop a moving axis; when active the drive cannot be operated via NC or drive, torque is disabled immediately; vertical axes must be braked mechanically first; with holding brakes cancel controller enable first, then switch on the lockout.

## PDF page 22 (printed page 62): Fig. 62 Control inputs and signal outputs of the safety lockout

Activate by applying +24 V to AS+ / AS- on connector X3. Relay switching is reported to the NC by potential-free contacts ASQ-ASQ closing.

Connector **X3** (11-pin terminal strip on the front of the DDS 2.1, right side; front view also shows X2 (above X3) and X4 (15-pin D-sub, below)):

| X3 pin | Signal | Remark |
|---|---|---|
| 1 | AK1 | analog output channel 1 (diagnostic) |
| 2 | 0VM | measurement 0 V |
| 3 | AK2 | analog output channel 2 |
| 4 | 0VM | measurement 0 V |
| 5 | (earth symbol) | shield / PE |
| 6 | Bb | Ready contact (potential-free) |
| 7 | Bb | Ready contact |
| 8 | AS+ | 1) safety lockout relay coil + |
| 9 | AS- | 1) relay coil - |
| 10 | ASQ | 2) feedback contact |
| 11 | ASQ | 2) feedback contact |

1) AS+, AS- = control input for relay coil, voltage 20-30 V DC, power 1.5 W. 2) ASQ, ASQ = potential-free feedback contact, 24 V, 1 A. 3) Control contact for power electronics (relay contact shown inside the drive). Ref. ENA3A-Abb17.6/InbetrDDS.
Status: H1 displays "AS" when the lockout is active.

## PDF page 23 (printed page 63)

Additional information: "Safety lockout for DDS 2 drive controllers", Doc. No. **209-0069-4313**.
**Fig. 63 Time sequence for safety lockout activation**: speed command n(com.) on input E1-E2 ramps to 0 ("NC-stop: operation completed", "Drive stopped n(com.) = 0"); then Controller enable on pin RF goes 0; motor torque goes 0; after 0.5 s the safety lockout input signal on AS+, AS- goes 1 ("AS safety lockout active"); then the safety device (door) may open ("Safety device cancelled"). Ref. ENA3A-Abb65.2/InbetrDDS [uncertain].

## PDF page 24 (printed page 64): 7.3 Holding brake function

Note: MDD holding brake is not a service brake; wears out after approx. 20,000 revolutions if applied while running. De-energized brake = holding. The drive controls the brake according to controller enable (Fig. 65) and error reaction (Fig. 66). Brake power: DC from an external source to terminal block X6 (Fig. 64).

**Fig. 64 Connecting up the holding brake to terminal block X6**: motor power cable to A1, A2, A3 (power terminals at the bottom front), shield clamped; brake and thermistor cores to X6 (top front, next to S1 and the 7-segment display H1; "U5" label below).

| X6 pin | Signal | Motor cable core 1) / external |
|---|---|---|
| 1 | TM+ | core 5 or white (motor thermistor) |
| 2 | TM- | core 6 or brown (motor thermistor) |
| 3 | UB | 2) +24 V ext. ±10 % |
| 4 | BR | core 7 or red (brake) |
| 5 | 0VB | core 8 or black (brake) |
| 6 | 0VB | 2) 0 V ext. |

1) The cores in the motor power cable are identified by figures; exception: cable **IN 253** has colour-coded cores. 2) External brake supply 24 V DC ±10 %, recommended cross-section 0.75 mm2. Ref. ENA3A-Abb65.1/InbetrDDS.

## PDF page 25 (printed page 65)

**Fig. 65 Holding brake vs controller enable**: signals External controller enable via NC control, Speed command value = 0, Motor torque, Holding brake closes. 1) brake closes when speed < 10 rpm; 2) after 400 ms if speed > 10 rpm; 3) brake opens immediately when controller enable is applied; 4) Caution: motor torque is disabled 100 ms after the brake has closed. Ref. ENA3A-Abb16/InbetrDDS.
**Fig. 66**: same as Fig. 61 (brake vs error reaction, same footnotes 1-3).

## PDF page 26 (printed page 66)

Manual brake release (when RF cannot be set): Danger, secure vertical axes. 1. Disconnect brake conductors (X6/4 and X6/5). 2. +24 V DC to core "7"/red. 3. 0 V to core "8"/black. 4. Brake opens. 5. Move axis. 6. Remove supply. 7. Reconnect per Fig. 64.
Verifying: audible knock when the brake opens. Dimensioning: brake holding moment must exceed the axis requirement (Section 8.3), 30 % oversizing for vibration.
Error diagnosis: H1 "**68**" = error in the holding brake and its control signals (Section 9.3).

## PDF page 27 (printed page 67): 7.4 Operational status (Ready)

Potential-free "Bb" (Ready) contact on **X3 pin 6/7**. Opens on: interruption of control voltage; error in the drive. Closed while: control voltage present and drive free of errors.
**Fig. 67 "Ready" signal contact for external evaluation**: X3 pin 6 Bb fed from +24 V, pin 7 Bb to a PLC input of the NC control. Ref. ENA3A-Abb17.7/InbetrDDS.

## PDF page 28 (printed page 68)

Status diagnoses on H1:

| H1 | Meaning |
|---|---|
| bb | "Ready" - control voltage applied, drive free of errors, ready for powering up |
| Ab | "Drive ready" for power output - mains power switched on |
| AF | "Drive enable" - controller enable applied, drive follows the command value |

7.5 Temperature monitoring of drive controller and motor (causes: dirty heat transfer, overload, cooling failure). When the limit is exceeded a warning is emitted for 30 s, as flashing messages: "**50**" Warning: amplifier overtemperature; "**51**" Warning: motor overtemperature.

## PDF page 29 (printed page 69)

Warning signal output "TVW" on the ANALOG interface: 24 V signal for evaluation by the NC.

**Fig. 68 Drive controller inputs and outputs for temperature monitoring**: motor power cable to A1/A2/A3; conductors for motor temperature monitoring and brake to X6 (TM+ 1 core 5/white, TM- 2 core 6/brown, UB 3, BR 4 core 7/red, 0VB 5 core 8/black, 0VB 6); front shows A1, A2, A3, L-, L+ [uncertain] (DC bus), X5, S1, H1, U5, X1, slots U1 U2 U3 U4, X2, X3, X4. ANALOG interface connector **X13/15** 1):

| Terminal | Signal |
|---|---|
| E1 | velocity command + |
| E2 | velocity command - |
| TVW | warning output (24 V) - "TVW signal output" |
| RF | controller enable input |
| AH | drive halt input |
| Ired1 | current reduction 1 |
| Ired2 | current reduction 2 |
| +UI | +24 V ±10 % from external power source |
| 0VI | 0 V of external source |
| (earth) | shield |

1) Connector designation may vary according to the installed ANALOG interface (see Fig. 18). 2) Motor cable cores identified by figures; exception cable IN 253 colour-coded. Ref. ENA3A-Abb31.1/InbetrDDS.
Internal reaction: within 30 s the NC may stop the drive in a controlled way; afterwards the drive reacts per preset error reaction. H1 errors: "**18**" Amplifier overtemperature - shutdown; "**19**" Motor overtemperature - shutdown (Section 9.3).

## PDF page 30 (printed page 70): 7.6 Velocity command matching

Two parameters in "OPERATING MODES, SCALING": "Maximum command value for desired velocity" (max. voltage at analog inputs E1, E2) and "Velocity at maximum command value". Example 2000 rpm at 8 V: entries 8 [V] and 2000 [rpm]. Limits: 0 to 10 [V]; 0 to 65000 [rpm]. Note: if the ratio exceeds 6500 rpm per 1 V, error "parameters outside range".

## PDF page 31 (printed page 71)

Direction: positive E1 vs E2 = clockwise (looking at shaft), negative = counter-clockwise; do not swap motor cores.

**Fig. 69 Input signals at the ANALOG interface**:

| Designation | | Unit | Min. | Max. |
|---|---|---|---|---|
| Input voltage | E1 | V | -10 | +10 |
| Input voltage | E2 | V | -10 | +10 |
| Input voltage | ΔU | V | | 10 |
| Input current | E1POSIN | mA | -0.25 | +0.25 |
| Input current | E2NEGIN | mA | -0.5 | +0.5 |

Ref. ENA3A-Abb65.8/InbetrDDS.

**Fig. 70 ANALOG interface on the DDS 2.1 drive controller** - connecting the velocity command interface to an NC: "Velocity command ±10V analog" from the NC control (D/A converter symbol "#/n") to X13/15 E1, E2 -> differential amplifier in the drive; PLC connected to TVW (output), RF, AH, Ired1, Ired2 (inputs); external power source +24 V ±10 % to +UI, 0 V to 0VI; earth terminal. Module front shows X5, X1, U1-U4, X2, X3, X4. 1) Interface designation depends on the installed ANALOG interface (Fig. 18). Ref. ENA3A-Abb65.9/InbetrDDS.

Connections: NC analog out + -> X13/15 E1; NC analog out - -> E2; drive TVW -> PLC input; PLC output -> RF; PLC output -> AH; PLC outputs -> Ired1, Ired2; +24 V ext -> +UI; 0 V ext -> 0VI.

## PDF page 32 (printed page 72)

7.7 Command value smoothing: stepped NC command values (cycle time) cause torque fluctuation, noise, vibration, stress, losses. Parameter "Command value smoothing filter time constant"; recommended 1/3 of the control cycle time. Too high -> delays and contouring errors; to cancel enter 0.25 msec.

7.8 Actual-position sensing by indirect relative position measuring system: position measured on the machine (direct) or on the motor (indirect). INDRAMAT supplies an interface as standard on digital drives with ANALOG interface, in two versions: indirect relative (incremental encoder compatible signals) and indirect absolute (7.9). The motor feedback senses rotor position (cyclically absolute), high-resolution conditioning in the drive. The output connector of the ANALOG interface with incremental encoder emulator (**DAE 1.1**) provides incremental-encoder-compatible square-wave signals.

## PDF page 33 (printed page 73): Fig. 71 Output signals of the incremental encoder version of the ANALOG interface, Type DAE 1.1

Waveforms: Ua1, Ua2 (90° quadrature), Ua0 marker pulse, "360° electrical = one cycle"; "TTL square-wave pulses when looking on the motor shaft, clockwise rotation (see Project Planning Manual, Doc. No. 209-0069-4317)".

Connector **X14** (15-pin, DAE 1.1 front) to the NC position interface ("Incremental encoder interface"), wire colours of the cable as printed (gr grey, rs pink, rt red, sw black, bn brown, gn green):

| NC wire | Signal | X14 pin |
|---|---|---|
| gr | Ua2 | 8 |
| rs | /Ua2 | 1 |
| rt | Ua0 | 3 |
| sw | /Ua0 | 4 |
| bn | Ua1 | 5 |
| gn | /Ua1 | 6 |
| - | - | 7, 2, 9 (not used) |
| white | 0V ext. | 10 (0V ext., earth symbol) |
| - | - | 11 (not used) |
| brown | +5V ext. | 12 (+5V ext.) |
| - | - | 13, 14, 15 (not used) |

Pairs are twisted (Ua2//Ua2, Ua0//Ua0, Ua1//Ua1). The +5 V for the output stage is supplied externally by the NC (pin 12, 0 V pin 10). Ref. ENA3A-Abb65.3/InbetrDDS.

Settable incremental encoder resolution ("INCREMENTAL ENCODER SETUP"): line count 1 to 252 144 lines/revolution [as printed; screen shows 262144]; marker pulse offset 0 to 359.9 [deg.], increment 0.1 [deg.], clockwise looking on the motor shaft; max output frequency 504 [kHz] (Fig. 72).

## PDF page 34 (printed page 74)

**Fig. 72 Maximum motor speed in relation to the settable encoder line count**: n_max = 60 [s/min] * 504000 [lines/sec] / line count [lines/rev.]; curve "max. output frequency: 504 kHz", "permissible range" below; axes lines/motor revolution 0-20000, motor speed 0-10000 rpm. (e.g. 2500 lines -> 12096 rpm; 10000 lines -> 3024 rpm.)

Setting the "market" [sic] pulse offset: homing fails if the home switch edge coincides with the marker pulse. Procedure: 1. traverse to the home switch edge; 2. note rotor position (degrees) in "Drive Status"; 3. continue until "marker pulse" display goes 0 -> 1; 4. stop; 5. the difference should be 90°; otherwise enter the amount needed to make up 90° as "marker pulse offset".

## PDF page 35 (printed page 75)

Checking: homing at approx. 10 % rapid velocity, then increase step by step; on error repeat. Motors with keyways: normalized keyway position relative to the marker pulse. **Fig. 73**: view of the output side of an MDD motor; reference point at "marker pulse offset" = 0 is 10° from the keyway position. Ref. ENA3A-Abb45/InbetrDDS.

## PDF page 36 (printed page 76): 7.9 Actual-position sensing by indirect absolute position measuring system

Benefits: no homing after NC shutdown, no risk in homing, no reference cams. Motors with an additional absolute encoder option: absolute resolution up to 4096 motor revolutions; requires system configuration RA 02 or DA 02 (Section 2.3). The ANALOG interface with absolute encoder emulator (**DAA 1.1**) outputs absolute position in Gray code via an **SSI** (synchronous serial interface).
Absolute encoder monitoring: position stored at shutdown and compared at start-up; deviation beyond the "absolute encoder monitoring window" -> error "**76**" "absolute encoder error". Danger: the axis was moved while shut down. Reset with key S1 (Fig. 15); if 76 cannot be reset, fault in the feedback circuit -> replace the motor.

---

## For the retrofit

What these pages establish (generic Indramat manual, applies to every DDS 2.1 axis drive with ANALOG interface; the machine-specific type codes, slot equipment and wiring tags are in the other parts of chapter 6):

**Keeping the Indramat drives under LinuxCNC**
- Drive: Indramat **DDS 2.1** digital AC servo controller ("W050" = 50 A peak example) with plug-in software module **DSM 2.1-E11-xx** and an **ANALOG interface** plug-in card: **DAE 1.1** (incremental encoder emulator) or **DAA 1.1** (absolute, SSI Gray code). Motors MDD (on this machine MDD 093) with DSF/RSF resolver-type motor feedback, optional MTG/IDG multiturn and optional holding brake. Check the configuration rating plate of each drive (Fig. 58 style, e.g. "DDS 2.1-W050-DA01-00") to see which interface is fitted.
- Command interface: **analog ±10 V velocity command**, differential input E1/E2 on X13/15 (input current max 0.25/0.5 mA). Positive E1-E2 = clockwise. Scaling set in drive ("max command value" V and "velocity at max command value" rpm, max 6500 rpm/V). Fits LinuxCNC directly with a differential ±10 V DAC card (Mesa 7i77/7i97 style, Pico, etc.). Drive must be in operation mode 2 (velocity); mode 1 (torque) is dangerous by accident. Set command smoothing to 0.25 ms (off) or ~1/3 of the servo period.
- Position feedback to the control: DAE 1.1 on **X14** (15-pin): differential **TTL (RS-422) quadrature** Ua1,/Ua1 (pins 5/6), Ua2,/Ua2 (8/1), marker Ua0,/Ua0 (3/4), output stage powered by **+5 V from the control** on pin 12, 0 V pin 10. Line count programmable 1...262144 lines/rev (drive menu), max output 504 kHz (n_max = 60*504000/lines). Read the existing line count from each drive before connecting (must match LinuxCNC encoder scale; x4 counts). With DAA 1.1 instead: SSI Gray code, absolute up to 4096 revs - needs an SSI reader (Mesa SSI firmware).
- Discrete I/O on X13/15, all 24 V logic (0 V = 0, +12...32 V = 1), supplied from external +24 V ±10 % to +UI/0VI: inputs **RF** (controller enable), **AH** (drive halt), **Ired1/Ired2** (4-step current limit, binary); output **TVW** (24 V, overtemperature pre-warning, 30 s before shutdown).
- Ready/fault: potential-free **Bb** contact **X3-6/7** (closed = control voltage OK and no error). Use as amp-fault input to LinuxCNC (and in the E-stop chain). Also analog test outputs AK1/AK2 on X3-1/3 (0VM 2/4).
- Brake: the drive itself controls the MDD holding brake on **X6** (BR = X6-4, 0VB = X6-5) from a 24 V ±10 % supply on X6-3/6, released together with RF; brake closes <10 rpm or after 400 ms, torque off 100 ms after closing. Nothing to do in LinuxCNC besides RF; keep 24 V brake supply. Motor thermistor TM+/TM- on X6-1/2 stays with the drive.
- Safety lockout: **AS+/AS- X3-8/9** (20-30 V DC, 1.5 W relay coil) disconnects the power stage drive; feedback contact **ASQ X3-10/11** (24 V, 1 A). Sequence: stop, drop RF, after ~0.5 s energize AS. Check whether the machine uses it (door interlock).
- E-stop / power: drives sit on a common DC bus (two conductor rails) fed by an Indramat power supply module (TVD/KDV type, see other parts) with main contactor K1, "Ready" contact, bus connecting ribbon cable with end connector, and optional "plug braking" (DC-bus short braking). Power may only be switched on when the supply's Ready contact is closed; error reaction 0 drives the supply's power-off via X1. The LinuxCNC E-stop must keep this sequence: control voltage -> Bb on all drives -> K1 on (H1 shows Ab) -> RF (H1 AF).
- Commissioning tools: parameters are set with a PC terminal program over the drive's serial port (VT-100, 9600 8N1, DDS2PC software), save/load as ASCII files via button S1. **Back up every drive's parameter file before the retrofit.** Errors on H1: 18/19 overtemperature, 22 encoder, 24 overcurrent, 50/51 warnings, 60/61 fuses, 68 brake, 69-73 supply voltages, 76 absolute encoder.

**Replacing the drives**
- A replacement drive must drive MDD synchronous motors with Indramat resolver feedback (DSF/RSF), or the motors must be replaced too (new motors with standard encoders are often easier, since the MDD feedback is proprietary).
- Required functions to replace: ±10 V velocity input or step/dir, encoder output (or LinuxCNC reads the new motor encoder directly), enable, ready/fault output, 24 V holding-brake control (release with enable, close <10 rpm), motor thermistor monitoring, safe torque off equivalent to AS+/AS-, DC-bus braking (the Indramat supply module also provides regenerative/plug braking).
- MDD holding brakes are holding brakes only (approx. 20,000 revolutions life when applied while running): any new drive must close the brake only at standstill.
