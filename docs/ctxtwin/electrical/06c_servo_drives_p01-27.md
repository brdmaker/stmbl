# Gildemeister CTX twin, electrical documentation: 6 Servo drives (part c), PDF pages 1-27

**Source:** `6_Servo_drives_c_20201029_0001.pdf` (scan dated 2020-10-29), PDF pages 1-27 of 54.
These pages are not Gildemeister drawings. They are an excerpt from the INDRAMAT manual
**"DDS 2.1 and MDD with ANALOG interface"**, doc. no. **209-0069-4315-02** (odd pages) /
**209-0069-4315-03** (even pages; the footer number alternates as printed), dated **December 23, 1993**,
manual pages **77-103**: the end of chapter 7 "Commissioning the functions of the digital AC servo drive",
chapter 8 "Final commissioning work" and the start of chapter 9 "Diagnostics and fault clearance".
**Original language: English** (no German text on these pages). No title block, no device tags of the
machine (-K, -X numbers) and no machine-specific wiring on these pages.

Figure source codes printed in the figures (Indramat drawing IDs): `ENA3A-Abb65.35/InbetrDDS`,
`ENA3A-Abb46InbetrDDS`, `ENA3A-Abb47/InbetrDDS` [uncertain, small print], `ENA3A-Abb65.10/InbetrDDS`,
`ENA3A-Abb48/InbetrDDS` [uncertain], `ENA3A-Abb65.7/InbetrDDS`, `ENA3A-Abb65.5/InbetrDDS`,
`ENA3A-Abb65.6/InbetrDDS`, `ENA3A-Abb53/InbetrDDS`, `ENA3A-Abb53.1/InbetrDDS`, `ENA3A-Abb50/InbetrDDS`.

## Page index

| PDF page | Manual page | Section | Content |
|---|---|---|---|
| 1 | 77 | 7.9 (cont.) | Absolute position output via DAA 1.1, X16 SSI pinout (Fig. 74) |
| 2 | 78 | 7.9 | Setting the reference position (Fig. 75), absolute position signal (Fig. 76) |
| 3 | 79 | 7.9 | Counting direction of position actual value output (Fig. 77), diagnosis warning |
| 4 | 80 | 7.10 | Master/slave mode: procedure, wiring, direction reversal |
| 5 | 81 | 7.10 | Checking master/slave; connection diagram (Fig. 78) |
| 6 | 82 | 7.11 | Torque reduction, Ired1/Ired2 table (Fig. 79), formula |
| 7 | 83 | 7.11 | Example calculation MDD112B-N-030 / DDS 2.1-050, characteristic (Fig. 80) |
| 8 | 84 | 7.11 | Activating torque reduction, wiring (Fig. 81) |
| 9 | 85 | 7.12 | Drift-free positioning, drive halt "AH" (Fig. 82) |
| 10 | 86 | 7.13 | Velocity loop, block diagram (Fig. 83) |
| 11 | 87 | 7.13 | Velocity loop parameters, PI transition function (Fig. 84) |
| 12 | 88 | 7.13 / 7.14 | Current loop gain; power failure / emergency shutdown |
| 13 | 89 | 7.15 | Short-time operation torque M_KB, overload factor, duty cycle |
| 14 | 90 | 8 / 8.1 | Final commissioning; NC position loop + DDS velocity loop (Fig. 85); checking position loop |
| 15 | 91 | 8.1 / 8.2 | Closing position loop; drift (offset) compensation on DAE 1.1 (Fig. 86) |
| 16 | 92 | 8.3 | Checking servo drive: safety limit switch, load torque measurement |
| 17 | 93 | 8.3 | Base torque, rapid traverse torque, weight compensation, recovered energy |
| 18 | 94 | 8.4 | Saving data |
| 19 | 95 | 9 / 9.1 | Diagnostics: status codes bb, Ab, AF, AS |
| 20 | 96 | 9.1 / 9.2 | AH; initialization codes -1 ... -9 |
| 21 | 97 | 9.3 | Fault codes "..", 18, 19 |
| 22 | 98 | 9.3 | Fault codes 22, 24, 25, 26 |
| 23 | 99 | 9.3 | 26 remedy; 33, 50, 51 |
| 24 | 100 | 9.3 | 60, 61, 67, 68 |
| 25 | 101 | 9.3 | 69, 70, 71, 72, 73 |
| 26 | 102 | 9.3 | 76, 81, 82, 83 |
| 27 | 103 | 9.3 | 84, 87, 88, 89, 91 |

---

## PDF page 1 (manual p. 77) - 7. Commissioning the functions of the digital AC servo drive

**Absolute position output**

> The absolute position is output through the ANALOG interface with absolute encoder emulator **DAA 1.1**
> via connector **X16** and in **SSI data format** (refer to Digital intelligent AC servo drives, Project
> Planning Manual, Doc. No. 209-0069-4317-0019 [as printed; probably 209-0069-4317-00, see p. 23]):

**Fig. 74: Output connector pin allocation for drive controller with absolute encoder interface.**
Left box "NC control" with sub-box "Position interface", inner dashed box "Absolute encoder interface
(SSI data format)". Right: drive controller front view with plug-in slots U1, U2, U3, U4, connectors X1,
X2, X3, X4; display label on top reads "DDS 2.1-W-0.." [unreadable remainder].

X16 pin allocation (cable wire colours on NC side as printed):

| X16 pin | Signal (drive side) | Wire colour / NC-side label |
|---|---|---|
| 8 | - | - |
| 1 | - | - |
| 3 | - | - |
| 4 | - | - |
| 5 | DATA (+) | brown, DATA (+) |
| 6 | DATA (-) | green, DATA (-) |
| 7 | - | - |
| 2 | - | - |
| 9 | - | - |
| 10 | 0V ext. | white, 0V ext. |
| 11 | +5V ext. | brown, +5V ext. |
| 12 | - | - |
| 13 | clock pulse (+) | red, clock pulse (+) |
| 14 | clock pulse (-) | black, clock pulse (-) |
| 15 | Shield (earth symbol) | cable shield |

Pins are drawn in the order 8, 1, 3, 4, 5, 6, 7, 2, 9, 10, 11, 12, 13, 14, 15. DATA and clock pairs are
drawn as twisted pairs. The +5 V / 0 V for the SSI output stage are supplied externally by the NC ("ext.").

## PDF page 2 (manual p. 78)

**Setting the reference position**

> The reference position is set using the parameter "absolute encoder parameter" in the "POSITION
> EVALUATION" menu.
> Example (see Fig. 75): - The desired reference position is in the middle of the operating stroke.
> Procedure: 1. Manoeuvre the axis to the manually measured reference position. 2. Enter the desired
> absolute position actual value in the parameter "reference position" (according to the example, this
> will be 2048).

**Fig. 75: Example: setting the reference position using the "absolute encoder parameter".** Motor with
ball screw and "Slide", arrow "Reference position" in the middle. Range 0 ... 4096 motor revolutions.
1) Maximum stroke with absolute indirect position actual-value sensing on the motor (4096 motor revolutions);
2) Useful stroke (>0 ... <4096); 3) Max. deceleration path.

Warning (triangle): **"Traversing commands outside the permissible stroke will result in a high accident
risk for both operators and machines."** If the maximum stroke is exceeded, the absolute position actual
value will be lost.

**Fig. 76: Absolute position actual value signal of the DAA 1.1.** Sawtooth P from 0 to 4095, repeating.
P = absolute position actual value output on DAA 1.1; A = traversing range (>0 ... <4096) limited by the
absolute position actual value output signal on DAA 1.1.

## PDF page 3 (manual p. 79)

**Counting direction of the position actual value output**: To match the servo axis traversing direction
to the counting direction in the machine's coordinates system, the counting direction of the position
actual value output can be fixed by means of parameters.

**Fig. 77: Counting direction of the position actual value output referred to the motor's direction of rotation.**

| Counting direction of position actual value output | Motor rotation (looking at drive side) | "Counting direction" parameter |
|---|---|---|
| Positive (rising) | clockwise arrow | 0 |
| Negative (falling) | clockwise arrow | 1 |
| Positive (rising) | counter-clockwise arrow | 1 |
| Negative (falling) | counter-clockwise arrow | 0 |

Warning: **"Risk of personal injury or machine damage! Be careful using this parameter because of the danger
of positive feedback in the position control loop. This will lead to uncontrolled axis motion at maximum speed."**

**Diagnosis**: when the parameter is selected the warning "Caution: Changing this parameter will change the
counting direction of the absolute encoder. Danger of positive feedback!! Make sure the lag monitoring of
your control unit is active." is generated.

## PDF page 4 (manual p. 80) - 7.10 Master/slave mode

Axes that cannot achieve the required torque with one drive use two drives (master/slave). The master
controls velocity for both positively locked drives; the slave receives its torque command as an analog
voltage from the master ("torque arm").

Danger: **"NEVER operate the slave drive on its own. Even the smallest torque command signal will cause the
slave drive to rev up uncontrollably. To avoid this, make sure the two drives are positively locked."**

Procedure for setting up a master/slave drive:
1. Set operating mode "velocity loop" on the master drive controller.
2. Call up the "OPERATING MODES, SCALING" menu on the master controller.
3. Analog output: channel 2 -> call up the current command value with the arrow keys.
4. Connect the analog output of the master **AK2, connector X3.3**, with analog input **E1** of the slave.
5. Connect the master's **0V_M (test) (connector X3.4)** to analog input **E2** of the slave.
6. Make sure master and slave are force locked and positively locked.
7. Set "torque loop" mode on the slave controller.
8. Continue parametrizing the slave in the same way as the master.

Wiring: Fig. 78 (same direction of rotation).
Reversing the direction of rotation: same direction: master AK2 -> slave E1, master 0V_M -> slave E2.
Opposite direction: master AK2 -> slave E2, master 0V_M -> slave E1.

## PDF page 5 (manual p. 81)

**Checking the master slave drive**: 1. Call up "OPERATING MODES, SCALING" (Section 5). 2. Set current
command value output to channel 2 (Section 9.5). 3. Record the current command value of master and slave at
terminals AK2 on connector X3.3 and X3.4 respectively. 4. Both values must be the same, maximum permissible
deviation < 10 %.

**Fig. 78: Connection diagram for a master/slave application (same direction of rotation for master and slave).**
Top: "NC control" box with "Position interface", "PLC", "Command value". Two DDS units "Master" and "Slave".

Terminal strip X13/15 (footnote 1: "Connector designation depend on the installed ANALOG interface"), on
both units, top to bottom: **E1, E2, TVW, RF, AH, Ired1, Ired2, +UI, 0VI, shield (earth)**.
Master connector **X3**, top to bottom: **AK1, 0VM, AK2, 0VM, Bb, Bb, AS+, AS-, ASQ, ASQ**.
External power supply "+24V / 0V" (AC/DC converter symbol).

Connections readable:
- NC "Command value" -> master X13/15 E1/E2.
- NC "PLC" -> RF and AH of master and slave [uncertain which lines exactly]; TVW back to PLC [uncertain].
- NC "Position interface" <- from master (arrow up into position interface) [uncertain origin: motor feedback/encoder emulation output].
- Master X3 AK2 -> slave E1; master X3 0VM -> slave E2.
- +24V -> +UI of both, 0V -> 0VI of both [uncertain, lines drawn to both strips].
- Master X3 Bb/Bb, AS+/AS-, ASQ/ASQ: lines drawn out of figure [destination unreadable].
Bottom: mechanical sketch: master and slave motors driving a common gear to "Load".

## PDF page 6 (manual p. 82) - 7.11 Torque reduction

The DDS 2.1 allows three torque reductions, selected by binary coded signals at control inputs **Ired1**
and **Ired2**. Enter the reduction in %; 100 = continuous zero-speed torque M_dN of the uncooled motor
(see "nominal torque" in "AMPLIFIER/MOTOR PARAMETERS" menu). Limits for entries: 0 to 400.
Procedure: 1. Call up "TORQUE/CURRENT LIMITS" menu. 2. Select the torque reduction. 3. Enter the torque.

**Fig. 79: Overview of setting options for external torque reduction**

| Ired2 | Ired1 | Setting | Torque value in % of nominal torque M_dN |
|---|---|---|---|
| 0 | 0 | Full torque | - |
| 0 | 1 | Current limit 1 to | e.g. 80 |
| 1 | 0 | Current limit 2 to | e.g. 150 |
| 1 | 1 | Current limit 3 to | e.g. 50 |

1 = input activated (+12 ... 32 V); 0 = input deactivated (0 ... 3 V).

Calculation: Reduced torque [%] = 100 x M_max.desired / M_dN (M_max.desired = desired torque [Nm],
M_dN = nominal (zero-speed) torque [Nm]).

## PDF page 7 (manual p. 83)

Example: desired M_max desired = 20 Nm. Motor/controller: **MDD112B-N-030, DDS 2.1-050**, mains power supply
with stabilized DC link circuit. From selection data list: M_dN = 17.5 Nm (uncooled motor), full torque
M_max = 35.2 Nm. 100 x 20 / 17.5 = 114 %. Enter e.g. "current limit 1": 114.

**Fig. 80: Setting the torque reduction/current limit to 20 Nm.** Torque/speed characteristic:
M_max = 35.2 Nm = 201 %, M_red = 20 Nm = 114 %, M_dN = 17.5 Nm = 100 %, speed axis up to n_max.

Note: full torque 35.2 Nm corresponds to 201 % of M_dN; entering more than 201 limits the drive to M_max of the motor.

## PDF page 8 (manual p. 84)

**Activating the torque reduction**: current limit 1 is selected by applying +24 V DC from an external
source to inputs Ired1 and Ired2 (Fig. 81) [as printed; per the table, limit 1 = Ired1 only].

**Fig. 81: Activating torque/current limit 1.** NC control -> X13/15 (E1, E2, TVW, RF, AH, Ired1, Ired2,
+UI, 0VI, shield). NC "+24V" -> Ired1; NC "0V" -> [uncertain, drawn to Ired2/0V line]. External power source
"+24V ±10 %" -> +UI, "0V" -> 0VI. Drive front: X5, X1, X2, X3, X4, slots U1-U4. Footnote 1: connector
designation depends on installed ANALOG interface (see Fig. 18).

Note: **"Before recording the step response, deactivate the torque limit."**

## PDF page 9 (manual p. 85) - 7.12 Drift-free positioning of a servo axis

With conventional analog drives the axis may drift at 0 V command due to temperature drift / zero balance.
The DDS 2.1 with ANALOG interface allows drift-free positioning in "velocity loop" mode after the axis has stopped.

**Drive halt "AH"**: activated by applying **0 V DC or leaving the input open** at "AH". The drive stops
under consideration of any set torque limit (7.11). Status indicator H1 shows "AH".
Danger: if "drive halt" is deactivated (24 V applied) while controller enable is active, the drive
immediately follows the applied command value. While AH is active, velocity command values are not evaluated.

**Fig. 82: Circuitry and diagnosis display for drift-free positioning.** Status indicator "H1" shows "AH".
NC control/PLC "+24V" -> AH via contact (footnote 2: "Drive halt" function activated by applying 0V to pin "AH");
"0V". External power source +24V ±10 % -> +UI, 0V -> 0VI. X13/15 strip as above. Drive front shows S1, H1,
U5, X5, X1, X3, A1, A2, A3 [uncertain].

## PDF page 10 (manual p. 86) - 7.13 Velocity loop

Matching to the mechanics is done by activating the velocity loop parameters stored in the motor feedback
(Section 6.3). Instability causes: backlash, insufficient rigidity, unfavourable inertia ratio (ideal 1:1).
Effects: poor surface quality, increased wear. Note: changing standard settings only by personnel with
sound control engineering knowledge; otherwise consult INDRAMAT Service.

**Fig. 83: Block diagram of the velocity loop.** omega command -> T_GL (smoothing) -> Kp_omega -> sum ->
3) current limitation -> K_PI -> I command; parallel branches K_DP, K_i (with 2) unit integrator), K_DI;
omega actual and I actual feedback. Legend: Kp_omega velocity loop proportional gain; T_GL smoothing time
constant; K_DP velocity loop derivative/proportional gain; T_N velocity loop integral reaction time;
K_DI velocity loop derivative/integral gain; K_PI current loop proportional gain; K_i integral gain
(1) K_i = Kp_omega / T_N); 2) unit integrator; 3) current limitation.

## PDF page 11 (manual p. 87)

- **Kp_omega**: proportional gain of the velocity loop.
- **K_DP**: derivative/differential gain of the velocity loop.
- **Integral reaction time**: Fig. 84, PI step response. T_N = K_P / K_I; T_N [ms], K_P [mA/rad/s],
  K_I [A/rad], Ua output [V], Ue input [V], h(t) transition function.
- **K_DI**: acceleration-dependent component acting on the integral gain.
- **T_GL smoothing time constant**: filter in the proportional component against quantization; the smallest
  value **250 µs** makes the filter inactive.

## PDF page 12 (manual p. 88)

**Current loop proportional gain**: "This value is motor-specific, set by INDRAMAT and may not be changed."

**7.14 What to do in the event of a power failure or emergency shutdown**: INDRAMAT power supply units are
equipped with monitoring and signalling devices permitting safe return movements in the presence of faults
(the exceptions are **TVM and KDV**). See the application manual for the installed power supply module.

## PDF page 13 (manual p. 89) - 7.15 Setting the short-time operation torque "M_KB"

M_KB entered in parameter "overload factor"; values in selection data list Doc. No. 209-0069-4302-00 IDE.
Overload factor = M_KB / M_dN x 100 [%], settable range 0 to 400 %.
Duty cycle ED = (M_dN)^2 / (M_KB)^2 x 100 [%] (load condition S6 to DIN 57530/VDE0530).
Continuous operation at M_dN: up to 25 % of n_max (max. NC useful velocity), up to 45 °C ambient;
overtemperature of 60 K on the motor frame.

## PDF page 14 (manual p. 90) - 8. Final commissioning work, 8.1 Preparations

**Fig. 85: Velocity loop and position loop with direct measuring system.** NC control: position loop,
command W, deviation Xd = W - X, Vcom = Kv x Xd -> DDS 2.1 controller (velocity loop, +Vcom / -Vact,
microprocessor, rotor position from motor feedback) -> AC motor (M 3~) -> machine slide; position X
measured by direct (linear) measuring system back to the NC.

**Checking the position loop**: 1. Open the loop by disconnecting command conductor E1/E2. 2. Inject a
positive velocity command with a battery at E1/E2 of the ANALOG interface. 3. The NC should emit a
negative command voltage; if not, reverse the polarity of the position actual value.

## PDF page 15 (manual p. 91)

Close loop again: 1. Power off. 2. Disconnect battery. 3. Reconnect command conductor and controller enable
from NC. 4. Set velocity command evaluation on DDS 2.1 (Section 7.6).

**8.2 Compensating the servo axis drift**: potentiometer (offset voltage compensation) on the ANALOG
interface. 1. Apply 24 V DC to "AH" and "RF". 2. Activate axis via NC. 3. Lag display. 4. Turn
potentiometer until the lag oscillates around zero.

**Fig. 86: Drift compensation on the ANALOG interface (detail of Fig. 18).** Module labelled **DAE 1.1**
in slot 1, pot marked "OFFSET" [uncertain small print], terminal X13 with E1, E2, TVW, RF, AH; X2, X3 at right.

## PDF page 16 (manual p. 92) - 8.3 Checking the servo drive

Safety limit switch: cams long enough; traverse at max velocity past switch; measure braking path (both
directions for vertical axes); set braking path as min distance between fixed stop and switch.

Load torque: option 1 "DRIVE STATUS" menu (Nm and %). Option 2: measure DC voltage at **X3.3 and X3.4 -> 0VM**
(Fig. 29); **10 V = rated current of the controller**. Analog output channel 2 = current command value.
M_load = (U x I_controller / 10) x K_m (U at X3.3/X3.4 [V], I_controller rated current [A], K_m torque
constant [Nm/A] at 20 °C, from "AMPLIFIER/MOTOR PARAMETERS" or motor plate "T Const.").

## PDF page 17 (manual p. 93)

Base torque in feed range: <= 60 % of nominal (causes: axis lock not released, lubrication, friction,
weight compensation, jamming, unreleased holding brake, dimensioning). Rapid traverse: <= 75 % (hydraulic
weight compensation, oil-flooded gear, belt tension, lubrication film, ball screw, dimensioning).
Weight compensation: equal minimum current up and down.
Recovered energy: brake all axes from rapid with E-STOP without supply module tripping "bleeder overload"
or DDS 2.1 error 25 "overvoltage"; continuous test >= 15 min. Otherwise change supply module or add
extension module **TBM or TCM**.

## PDF page 18 (manual p. 94) - 8.4 Saving data

"Specific axis data list" (blank form in appendix) to be filled at start-up and kept in machine file;
PC saving in Section 5.2.

## PDF page 19 (manual p. 95) - 9. Diagnostics and fault clearance

Two-digit seven-segment "Status indicator H1" (Fig. 15). Initialization errors: 83, 84, 87, 88, 89, 91.
Warnings and command errors flash. Text also in "Drive Status" menu (Section 5.1, Fig. 36).

9.1 Operating status codes:

| Display | Name | Meaning / diagnosis text |
|---|---|---|
| bb | Ready | ready for powering up; "Ready for main power (Bb closed)" |
| Ab | Drive ready | control and power section ready |
| AF | Drive enable | enable given, drive follows command |
| AS | Safety lockout | power stage locked (safe torque disabling); "Drive interlock open"; after release RF must be applied again |

## PDF page 20 (manual p. 96)

| AH | Drive halt | stopped at torque from "TORQUE/CURRENT LIMIT", remains in velocity loop; danger note as p. 9 |

If the controller stalls in an init display, replace it.
9.2 Initialization: -1 Data RAM error; -2 Waiting for PLL to stabilize; -3 Initializing hardware;
-4 Reading user parameters from software module; -5 Reading parameters from amplifier; -6 Reading
parameters from motor feedback; -7 Reading configuration, selecting language; -8 Calculating runtime
parameters; -9 Initializing position.

## PDF pages 21-27 (manual pp. 97-103) - 9.3 Fault codes

Fault reset: button **S1** (Fig. 15); mains must be on; controller ready only after reset.

| Code | Name | Cause / remedy (abridged, faithful) |
|---|---|---|
| . . | Watchdog | software module missing/defective or processor defective; install module / replace controller |
| 18 | Amplifier overtemperature shutdown | after 30 s warning 50; cooling, cabinet air conditioning |
| 19 | Motor overtemperature shutdown | after 30 s warning 51, **24 V DC output via "TVW"**; overload, short in temperature conductor |
| 22 | Motor encoder error | encoder cable / motor feedback; danger: re-set absolute reference after motor change (7.9) |
| 24 | Overcurrent | phase current > 1.5 x rated; current loop parameters, motor cable |
| 25 | Overvoltage | DC link Ud > 475 V; bleeder; see 8.3 |
| 26 | Undervoltage error | DC link < +200 V signalled from supply module via control voltage bus; mains off without first removing RF, supply fault |
| 33 | External power supply voltage error | external supply of DC-decoupled plug-in module I/O out of range; see 209-0069-4317-00 |
| 50 | Amplifier overtemperature warning | follows command 30 s, then "error reaction" (P-0-0007), 7.1 |
| 51 | Motor overtemperature warning | follows command 30 s, then error reaction |
| 60 | Bridge fuse | bridge current > 2 x rated; motor cable short / power section |
| 61 | Earth connection | sum of phase currents > 0.5 x I_N |
| 67 | Faulty hardware synchronization | PWM PLL; replace unit |
| 68 | Brake fault | drive pilots MDD holding brake, brake current monitored; supply **24 V ±10 %**, motor cable polarity, brake, controller |
| 69 | ±15 V fault | control voltage bus cable / supply module |
| 70 | +24 V fault | control voltage bus cable / supply module |
| 71 | +10 V fault | current sensor supply; replace controller |
| 72 | +8 V fault | encoder supply; short in motor encoder or external encoder cable |
| 73 | Driver voltage fault | replace controller |
| 76 | Absolute encoder error | stored position vs. absolute encoder at power-up outside window ("Operating Modes, Scaling"); danger: axis moved while off; reset S1, else feedback/motor defective |
| 81 | Program RAM error | replace controller |
| 82 | Data RAM error | replace controller |
| 83 | Error reading drive data | EEPROM in controller; replace |
| 84 | Drive data invalid | EEPROM not written/invalid; INDRAMAT service |
| 87 | Parameter data invalid | software module not initialized / EEPROMs changed / defective; invalid params shown "***" in PARAMETERS menu |
| 88 | Error reading motor data | data memory in motor feedback; cable or motor |
| 89 | Motor data invalid | motor EEPROM; INDRAMAT service |
| 91 | Configuration error | software/hardware mismatch, plug-in module defective/missing |

---

## For the retrofit

What these pages establish (generic Indramat manual, not machine wiring; check the CTX twin drawings for
actual tags and which plug-in modules are fitted):

- **Drives**: INDRAMAT **DDS 2.1** digital AC servo controllers with **MDD** motors (example in manual:
  MDD112B-N-030 / DDS 2.1-050; CTX feed motors are MDD 093). Motor data and velocity-loop default gains
  live in the motor feedback EEPROM; firmware/parameters on a plug-in **software module** (errors 87/91 if missing).
- **Command interface**: **analog ±10 V velocity command** on ANALOG interface module **DAE 1.1**,
  differential inputs **E1/E2** on X13 (X15 depending on module), with offset pot for drift trim.
  So LinuxCNC can keep the drives with an analog-out card (e.g. Mesa 7i77 / 7i83) closing the position loop.
- **Control I/O on X13/15** (24 V logic, external supply **+UI / 0VI**, 24 V ±10 %):
  **RF** = controller enable (input), **AH** = drive halt (input, active at 0 V/open: must be 24 V to move),
  **Ired1/Ired2** = torque-limit select, **TVW** = motor overtemperature warning output (24 V).
- **Ready/fault**: X3 contacts **Bb/Bb** (ready contact, "Bb closed" = ready for main power; belongs in
  the E-stop/main contactor chain), **AS+/AS-, ASQ/ASQ** = safety lockout (start lockout) input and its
  acknowledge contact [interpretation from general Indramat knowledge; these pages only name the terminals]. Analog test outputs **AK1, AK2 vs 0VM** on X3 (10 V = rated current) are handy for
  torque monitoring.
- **Position feedback to the control**: either incremental encoder emulation (other DAx module, in
  pages outside this part) or, as shown here, absolute encoder emulator **DAA 1.1** on **X16, SSI**
  (DATA± pins 5/6, CLOCK± 13/14, +5V ext 11, 0V ext 10, shield 15), 4096-revolution multiturn range
  (position lost if exceeded; reference via "absolute encoder parameter"). LinuxCNC can read SSI with a
  Mesa card (SSI module in hostmot2) - verify which emulator the CTX uses before choosing hardware.
- **Brake**: MDD holding brake driven by the DDS itself (24 V ±10 %, fault 68); no separate PLC brake
  output needed if retained.
- **Power supply / E-stop**: DC link from INDRAMAT supply module (TVM/KDV have no safe-return feature;
  TBM/TCM extension modules for regenerative energy); E-stop braking test at 8.3; fault 26 if mains is
  dropped before removing RF -> LinuxCNC/PLC sequence must drop RF (and AH) before the main contactor.
- **Replacing the drives**: new drives need a motor feedback compatible with the MDD resolver/feedback
  with EEPROM (proprietary Indramat), so replacement usually means replacing motors too, or using a
  drive that supports Indramat MDD feedback. Master/slave torque coupling (7.10) is not expected on the CTX feed axes.
