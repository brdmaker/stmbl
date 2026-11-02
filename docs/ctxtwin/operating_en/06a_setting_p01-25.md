# Gildemeister CTX twin operating instructions (English edition), 6. Setting, pages 6-1 to 6-25

Source: `CTX_twin_Operating_instructions_6.pdf` (50 scanned pages), PDF pages 1-25 = manual pages 6-1 to 6-25. "Operating instructions CTX twin, Dok. No.: daaab1g1, February 1995", chapter 6 "Setting".
Original language: English. Continued in `06b_setting_p26-50.md`.

Running page header on every page: "6. Setting". Footer: "Operating instructions CTX twin, Dok. No.: daaab1g1, February 1995, Page 6-xx".

Symbols used in the margin: a warning triangle (rendered here as **Warning:**), a pointing hand (rendered as **Note:**), and a square "collision" pictogram (exclamation mark + broken part; rendered as **Caution (collision):**).

---

## 6. Setting (contents, pages 6-1 and 6-2)

| Section | Title | Page |
|---|---|---|
| 6.1 | General instructions | 6-3 |
| 6.1.1 | Coordinate system | 6-3 |
| 6.1.2 | Modular groups | 6-4 |
| 6.1.3 | Operating mode splitting | 6-4 |
| 6.1.4 | Processing possibilities | 6-5 |
| 6.2 | Prerequisites | 6-7 |
| 6.3 | A review of work steps | 6-7 |
| 6.4 | Details of the chucking device | 6-9 |
| 6.5 | Fitting and removing the chucking means | 6-12 |
| 6.5.1 | Removing the chucking means | 6-12 |
| 6.5.2 | Fitting the chucking means | 6-13 |
| 6.5.3 | Setting the chucking stroke | 6-14 |
| 6.5.4 | Chucking means parameter input | 6-17 |
| 6.5.5 | Rotational speed limit setting | 6-18 |
| 6.6 | Chucking unmachined part | 6-19 |
| 6.6.1 | Checking the chucking pressure and setting, if necessary | 6-19 |
| 6.6.2 | Tool holder fitting | 6-20 |
| 6.7 | Offset zero point in Z axis with compensated tool | 6-21 |
| 6.8 | Compensation of the following tools (not yet compensated tools) | 6-23 |
| 6.9 | Deactivate or set protection zones | 6-25 |
| 6.10 | Additional machining possibilities in the MACHINE operating mode | 6-26 |
| 6.10.1 | Constant feed for long workpieces without program | 6-26 |
| 6.10.2 | Parallel machining | 6-27 |
| 6.11 | C axis option | 6-28 |
| 6.11.1 | General view of work steps | 6-28 |
| 6.11.2 | Enter rotational speed and start driven tools | 6-29 |
| 6.11.3 | Compensating tools | 6-30 |
| 6.11.4 | Offset C axis zero point | 6-31 |
| 6.12 | Pick-off device option | 6-32 |
| 6.12.1 | Pick-off device setting option | 6-33 |
| 6.12.2 | Workpiece ejector pick-off device | 6-34 |
| 6.13 | Two pressure clamping device option | 6-36 |
| 6.14 | Tool measuring unit option | 6-37 |
| 6.15 | Bar machining option | 6-39 |
| 6.15.1 | Bar machining only with the lathe | 6-39 |
| 6.15.2 | Automatic material feeding and removal | 6-41 |
| 6.15.2.1 | General information | 6-41 |
| 6.15.2.2 | Machining with bar feeding | 6-44 |
| 6.15.2.3 | Machining with bar loading magazine | 6-46 |
| 6.16 | Control light option | 6-48 |
| 6.17 | Overload coupling X and Z axis | 6-49 |

(Note: several section titles in the contents differ slightly from the headings in the text, e.g. 6.1 "General instructions" vs. "General information", 6.1.2 "Modular groups" vs. "Unit groups", 6.6 "Chucking unmachined part" vs. "Clamping unmachined part".)

---

## 6.1 General information

### 6.1.1 Coordinate system

(Page 6-3.)

For the user or from the point of view of the controls both working spaces are identical with coordinate systems, axes arrangement and tool systems, because by folding the working spaces over to the other side the same conditions result.

The Z axes of both sides oppose each other, relative tool movements towards the tool coordinate system are allotted the same prefix in both working spaces. The turning spindles run opposing each other.

Figure: the two working spaces side by side, separated by a vertical line. Left side: chuck/spindle on the left, tool above, +X pointing up, +Z pointing right (away from the spindle), zero point `NC` on the spindle axis, rotation arrow `CCW`, viewing eye symbol looking from the left with an arrow pointing right. Right side mirror image: spindle on the right, +X up, +Z pointing left, `NC`, `CCW`, eye looking from the right.

### 6.1.2 Unit groups

(Page 6-4.)

The controls receive 2 modular groups in accordance with the machine concept, termed **AG** in the following.

- each AG is autonomous as far as its operating and programming surface is concerned
- an AG can include several spindles and several slides
- each AG deals with its own working space
- only one operating mode exists at the same time in an AG
- differing operating modes can simultaneously exist in both AG, i.e. operating mode splitting
- with full utilisation of operating mode splitting it is recommended that the working spaces be divided by a separating wall for safety reasons.

### 6.1.3 Operating mode splitting

The operating mode splitting can be switched on and off via a parameter switch:

- in the operating mode splitting ON the effect of the cycle start, cycle stop, and feed stop keys is dependent upon the setting of the **AG** selector switch.
- In the operating mode splitting OFF mode the buttons cycle start, cycle stop, and feed stop keys simultaneously affect both **AG**.

### 6.1.4 Machining possibilities

(Pages 6-5 and 6-6.)

Amongst others, the machine permits the following represented machining possibilities:

**Parallel machining** of
- 2 identical parts
- 2 identical parts time offset with automatic dividing wall
- 2 similar parts time offset with automatic dividing wall

**Full machining without dividing wall**
- Loading the spindle (left-hand side)
- Machining the workpiece
- Angular synchronous transfer
- Machining the 2nd side (right-hand side)
- Removing the finished part (right-hand side)

**Full machining with dividing wall** (page 6-6)
- Loading the spindle (left-hand side)
- Machining the workpiece
- Open the dividing wall
- Angle synchronous transfer
- Close dividing wall
- Machining the 2nd side (right-hand side)
- Removing the finished part (right-hand side)

Figures: small schematic per step showing the left and right spindles (headstock rectangles with centre lines) facing each other, the chuck, a gripper/loader symbol or tool symbol where loading/machining takes place, and (with dividing wall) a vertical bar between the spindles that is absent while the wall is open. In the "angular synchronous transfer" step the two chucks meet in the middle and both hold the workpiece.

---

## 6.2 Prerequisites

(Page 6-7.)

- The machine is switched on, see 5.7.1, "switch on machine" on page 5-35
- The reference point was moved to, see 5.7.2 "Move to reference point" on page 5-36
- The screen shows the menu `MACHINE`
- Workpieces, drawing, tools, chucks are prepared.

## 6.3 A review of the work steps

In principle, the sequence when setting is always the same:

- Select chucking means

**Warning:** The maximum chuck rotational speed must not be exceeded. Set the parameters correspondingly.

- Select chuck jaws or collets.
- If necessary, set the proximity switch for chucking stroke monitoring
- Set the parameters for internal or external chucking

**Warning:** Set chucking force and rotational speed. Do not exceed the maximum jaw weight.

- Chuck unmachined workpiece
- Check the run out
- Select tools and insert
- Remove not required tools from the tool holder
- Offset zero point in Z axis with the compensated tool.
- Calculate tools not yet compensated (for preset tools enter the setting dimensions in the tool editor (refer to programming handbook).

**Caution (collision):** Check that collision is excluded

(Page 6-8.)

**Caution (collision):** If necessary, set the protection zones in order to exclude collision with the activated tool e.g. with the chucking jaws.

- Select or enter program
- Check the program sequence with the graphic simulation
- Machine the first workpiece in an individual record

**Caution (collision):** Work with a reduced high speed and feed rate (override).

- Measure the first workpiece
- Correspondingly compensate tools and/or program

On the following pages the working steps are described in detail.

---

## 6.4 Details of the chucking device

(Page 6-9.)

**Warning:** Prior to undertaking work on the clamping device, familiarize yourself fully with its construction and function. The danger of doing something incorrect leading to injury to people and damage to the machine is nowhere greater than here. For this reason certain important information has been included at this point for the chucking device.

The chucking device is intended to chuck workpieces and comprises of the following modules:

**Parts of the chuck flying off have a high energy and can result in serious injury.**

The chucking device comprises the following parts (left hand headstock, right hand headstock, mirror imaged):

- **How does the chucking device function?**

A chucking pressure set on the pressure relief valve (5) passes to the chucking cylinder via a valve (3, 4) and hose to the clamping cylinder (7). In this manner the chucking piston (8) is moved in the direction determined by the foot switches or the program. The force enters the chuck/collet (11,12) via the tension tube (10), where it is transferred to the chucking jaws/ collets (14).

The parameter, see 6.5.4, "Chucking means parameter input" on page 6-17, must specify beforehand which chucking means can be used. As the workpieces can be internally or externally chucked, also the chucking direction has to be specified in the parameters.

In order to ensure a secure chucking, the chucking device is automatically monitored (chucking pressure/chucking stroke).

(Page 6-10.)

Figure: section through a headstock with the chucking cylinder at the left end of the spindle and the chuck at the right; below it the hydraulic symbol of a 4/3-way valve with two solenoids (3, 4) fed from `P`/`T` via the pressure reduction valve (5). Callouts 1-14:

| No. | Device | Designation |
|---|---|---|
| 1 | S 18121 | Left hand headstock proximity switch |
| | S 24121 | Right hand headstock proximity switch |
| 2 | S 18111 | Left hand headstock proximity switch |
| | S 24111 | Right hand headstock proximity switch |
| 3 | Y 18241 | Solenoid coil, left hand headstock way valve |
| | Y 24241 | Solenoid coil, right hand headstock way valve |
| 4 | Y 18251 | Solenoid coil, left hand headstock way valve |
| | Y 24251 | Solenoid coil, right hand headstock way valve |
| 5 | | Pressure reduction valve with integrated pressure monitoring |
| | F 18041 | Left hand headstock |
| | F 24041 | Right hand headstock |
| 6 | | Switching ring |
| 7 | | Clamping cylinder |
| 8 | | Clamping piston |
| 9 | | Spindle motor |
| 10 | | Tension and connection piece |
| 11 | | Chuck body |
| 12 | | Chuck piston |
| 13 | | Workpiece |
| 14 | | Chucking jaws |

(Page 6-11.)

- **Function of the chucking monitoring system**

The pressure set on the pressure reduction valve is monitored by a pressure monitor. Pressure drops or too low pressure prevent the headstock spindle from starting or stop it.

- **Function of the chucking stroke monitoring**

The correct setting of the chucking stroke monitoring

- prevents the chucking pressure from being built up, when the chuck / the collet is in a mechanical end position.
- ensures that the workpiece is securely clamped taking into consideration the unmachined part tolerance.

**Warning:** After changing the chuck or collet, reset the switches (proximity switches) which monitor the chucking pressure and stroke, refer to 6.5.3 "Chucking stroke setting" on page 6-14.
Faulty setting of the switches prevents a secure clamping of the workpiece, the start up of the turning spindle or reduction of the chuck clamping stroke.

---

## 6.5 Fitting and removing the chucking means

(Page 6-12.)

Here the removal and fitting of the standard chucking means is described (chucking cylinder/3 jaw chuck).

Prior to starting work select the module 1 or 2 concerned via selector switch, see page 5-10.

Chucking cylinder and tensioning tube as well as connecting piece are bonded together thus forming an assembly unit.

### 6.5.1 Removing the chucking means

**Note:** This parameter can only be changed by authorised dealers as it is password protected.

- Select `PARAMETER` operating mode.
- Select `CURRENT PARAMETER` menu point.
- ✓ The screen shows the dialog box `CHUCK DATA`.
- Enter a 2 in the `CHUCKING MODE` field (= external) and confirm.
- Select `MACHINE` operating mode.
- Open protection hood
- Move the chucking jaws inward using the foot switch (clamp chuck)
- Remove interchangeable false jaws
- Unscrew chuck fixing screws
- Move the chucking jaws out using foot switch (open chuck)
- ✓ The chuck is released from the spindle flange.
- Unscrew chuck and connection piece from the chucking cylinder by turning counterclockwise (if necessary, using the special tool provided with the chuck).
- Pull the chuck and connection piece out of the spindle towards the tool space and lay on a suitable surface (if necessary, with a crane).
- Switch off the machine (main switch off).

### 6.5.2 Fitting the chucking means

(Page 6-13.)

- Complete the chuck and grease (using heavy duty grease, observe the chuck manufacturer's information)
- Grease the connection piece
- Switch on machine
- Select operating mode `MACHINE`
- Open protection hood
- Open chuck (extend the clamping cylinder using foot switch)
- If necessary, check chuck flange for concentricity (radial 0.005; axial 0.005)
- Carefully clean spindle and chuck flange
- Push the chuck and connection piece into the spindle and turn in clockwise until the chuck body is in contact with the chuck flange.
- Turn approximately 1.5 turns to the left (note screw holes)
- Loosely screw in the chuck fixing screws
- Clamp chuck (move the chucking cylinder inward with the foot switch)
- Lightly tighten the chuck fixing screws
- Check run out (0.01), if necessary, align chuck
- Tighten chuck fixing screws using the torque values stated by the individual manufacturers in their documentation, e.g. for standard chuck Berg KH 200 use 67 Nm.

### 6.5.3 Setting the chucking stroke

(Page 6-14.)

**Left hand headstock**

Figure: rear end of the left headstock with the chucking cylinder (hydraulic hose with bellows connection at upper left), the switching ring on the cylinder rod and two proximity switches on holders: (1) upper switch, (2) lower switch.

- Adjust the proximity switch 1 (= S 18121) fully to the left, proximity switch 2 (= S 18111) fully to the right.
- Open the chuck completely.
- Displace the proximity switch 2 (= S 18111) to the left until the yellow light diode extinguishes.
- Subsequently move to the left approx. another 0.5 to 1 mm.
- Retighten the proximity switch holder.
- ✓ Now the "Front" chucking movement limit is set.
- Close the chuck
- Displace the proximity switch 1 (= S 18121) to the right until the yellow light diode extinguishes.
- Subsequently displace to the right approx. another 0.5 to 1 mm. Retighten the proximity switch holder
- ✓ Now the "Rear" chucking movement limit is set. The chuck must be within the operative stroke.

(Page 6-15.)

**Right hand headstock**

Figure: mirror image of the left-hand view (cylinder and hose at the right), proximity switches (1) upper and (2) lower.

- Adjust the proximity switch 1 (= S 24121) fully to the right, proximity switch 2 (= S 24111) fully to the left.
- Open the chuck completely.
- Displace the proximity switch 2 (= S 24111) to the right until the yellow light diode extinguishes.
- Subsequently move to the left approx. another 0.5 to 1 mm. [as printed; by symmetry with the left headstock "to the right" would be expected]
- Retighten the proximity switch holder.
- ✓ Now the "Front" chucking movement limit is set.
- Close the chuck
- Displace the proximity switch 1 (= S 24121) to the left until the yellow light diode extinguishes.
- Subsequently displace to the left approx. another 0.5 to 1 mm. Retighten the proximity switch holder
- ✓ Now the "Rear" chucking movement limit is set. The chuck must be within the operative stroke.
- Check the stroke with a depth gauge
- If the stroke is incorrect, check the screw-in depth of the tensioning tube and the proximity switch setting.
- Screw on the interchangeable false jaws

(Page 6-16.)

**Warning:** Following a collision causing damage to chuck parts, contact the individual chuck manufacturer.
Do not exceed the limit values punched on the chuck (rotational speed, jaw weight).

A precise description of the chuck can be found in the individual manufacturer's instructions.

### 6.5.4 Chucking means parameter input

(Page 6-17.)

To inform the controls which chuck is fitted and which chuck method is selected, the relevant data must be entered or changed in the parameters.

- Enter / change parameter
- Select `PARAMETER` operating mode
- Select `CURRENT PARAMETER` menu point
- Select `PLC PARAMETER` menu point
- Select `CHUCKING MODE` menu point
- ✓ The screen shows the dialog box `MACHINE PARAMETER: EDITING`. In this parameter the combination clamping cylinder - chuck or collet is entered.

- Possible entries in `CHUCKING MODE` field:

| Entry | Value |
|---|---|
| Internal | 1 |
| External | 2 |
| Disengage | 3 |

- Possible entries in the `CHUCK TYPE` entry field:

| Entry | Value |
|---|---|
| Manually actuated chuck type | 1 |
| Power operated chuck type | 2 |

- Possible entries in the `CHUCKING CYLINDER` field:

| Entry | Value |
|---|---|
| No chucking cylinder | 0 |
| Jaws open | 1 |
| Jaws close | 2 |

- ✓ The parameters are now set for chucking means and chucking mode.
- Close the dialog box

**Caution (collision):** After changing the chucking means and chucking mode parameters switch the machine off and on again so that the new values are accepted.

### 6.5.5 Setting rotational speed limit

(Page 6-18.)

Each chuck manufacturer specifies rotational speed limits for the various chucking means. The value for the maximum rotational speed is normally embossed in the chucking means. It is very important to observe these values.
Of course, also the limits resulting from the workpiece dimensions must be observed.

**Note:** This parameter can only be changed by authorised personnel as it is password protected.

- Select operating mode `PARAMETER`
- Select `CURRENT PARAMETER` menu point
- Select the `MACHINE PARAMETER` menu point
- Select the `ROTATIONAL SPEED` menu point
- Select the `SPINDLE ROTATIONAL SPEED 1`
- ✓ The screen shows the dialog box `MACHINE PARAMETER: EDITING`.
- Enter the required values in the `ABSOLUTE MAX: ROTATIONAL SPEED` field.
- ✓ The maximum spindle rotational speed is set. This cannot be exceeded by program commands.

---

## 6.6 Clamping unmachined part

(Page 6-19.)

If you want to turn a new finished component (start a new batch size), set the chucking pressure and rotational speed limit and insert the tools in the tool carrier, see 6.5.5 "Setting rotational speed limit" on page 6-18.

### 6.6.1 Checking the chucking pressure and set, if necessary

Figure: pressure gauge in a housing at the machine edge with the pressure reduction valve hand wheel (1) at the right.

- Read off the current chucking pressure from pressure gauge on the left hand machine edge

**If the chucking pressure has to be changed:**

- The pressure reduction valve is actuated using the key switch
- Adjust the pressure on the pressure reduction valve (1) using the hand wheel (pressure reducing valve), until the correct value is achieved for the unmachined part:
  - reduce (= turn to the left)
  - increase (= turn to the right)

**Note:** The system pressure is between 10 and 60 bar.

The pressure monitor is automatically set.

- Select required operating mode
- If necessary, check concentricity.

### 6.6.2 Tool holder loading

(Page 6-20.)

- Remove the tools not required
- Clamp the tools required for the workpiece
- Close the empty tool holder mountings with a sealing cap and an O ring

**Caution (collision):**
- Driven tools must be engaged
- **Do not change the swung in driven tool!**

---

## 6.7 Offset zero point in the Z axis with compensated tool

(Page 6-21.)

**Note:** Select the corresponding module 1 or 2 prior to starting using selector switch, see 5-10.

If necessary, move to the reference point, see 5.7.2, "Move to reference point" on page 5-36.

- Select operating mode `MACHINE`
- Select menu point `S`
- Select `ROTATIONAL SPEED S`
- ✓ The screen shows the dialog box `ROTATIONAL SPEED VALUE`.
- Enter the required rotational speed and confirm
- Start spindle
- Select menu point `F`
- Select `FEED MM/REV` menu point
- ✓ The screen shows the dialog box `FEED PER REVOLUTIONS`.
- Enter and confirm the required feed rate
- Select `SETTING` menu point
- Select `ZERO POINT OFFSET` menu point
- ✓ The screen shows the dialog box `TOOL SELECTION`.
- Enter tool number and confirm
- ✓ The selected tool holder position is swivelled into the working position and the screen indicates the `ZERO POINT OFFSET` dialog box.

(Page 6-22.)

- Press `Continue` key
- ✓ The message `Move to zero point in Z! Offset zero point` appears on the screen.
- Scratch workpiece
- Select `TAKE OVER VALUE` menu point
- ✓ The screen shows the dialog box `ENTER MEASURED VALUE`.
- Confirm the zero point offset calculated by the controls.
- ✓ `ZERO POINT OFFSET` appears in the dialog box. The calculated zero point offset is in the entry field.
- Close the dialog box.

---

## 6.8 Compensation of the following tools (tools not yet compensated)

(Page 6-23.)

If the workpiece is to be machined with various tools, the required following tools must also be compensated.

**Prerequisites:**
The workpiece zero point is set to the X axis and parameter 7 is defined (measuring mode by scratching).
For measuring mode with measuring sensor, see page 6.14, "Tool measuring device option" on page 6-37.

The parameter 7 is defined in the following manner:

- Select `PARAMETER` operating mode
- Select `CURRENT PARAMETER` menu point
- Select `NC SWITCH` menu point
- Select `TOOL MEASURING MODE` menu point
- ✓ The screen shows the dialog box `MEASURE TOOL`.
- Enter a 0 (= scratch) in the field `TYPE` and confirm.

The measuring mode is set to scratching.

(Page 6-24.)

**Procedure:**
- Traverse the slide so that the tool holder can swivel freely, without the tool colliding when swivelling.
- Select `MACHINE` operating mode
- Select `SETTING` menu point
- Select `TOOL SETTING` menu point
- Select `MEASURE TOOL` menu point
- ✓ The screen shows the dialog box `TOOL SELECTION`.
- Enter the tool number and confirm
- ✓ The selected tool holder position is swivelled into the working position and the screen shows the dialog box `MEASURE: T...`
- Press the `CONTINUE` key
- ✓ The screen shows the message `MEASURE TOOL T... Scratch in X!`
- Scratch workpiece
- Select `TAKE OVER VALUE` menu point
- ✓ The screen shows the dialog box `ENTER MEASURED VALUE`.
- Enter and confirm the workpiece diameter
- ✓ The screen shows the `MEASURE TOOL` dialog box. The calculated spacing between the tool centre and machine fixed reference point appears in the input field X.
- Close dialog box
- ✓ The screen shows the `TOOL SELECTION` dialog box.
- Traverse the slides so that the tool holder can swivel freely, without the tools colliding when swivelled.

- If necessary compensate for further tools in the same manner.
  - Close the dialog box
  - ✓ The following tools are compensated.

---

## 6.9 Deactivate or activate protection zones

(Page 6-25.)

**Note:** The components (e.g. automatic working space division wall) to be activated for the protection zones must be described (refer to programming handbook).

With the aid of the protection zones you can prevent the tool from colliding with, for example, the chuck jaws. These are active in the operating modes `MACHINE` / `MANUAL CONTROLS` and `AUTOMATIC` / `SINGLE`.

- **Switch on protection zone monitoring**
  - Select operating mode `PARAMETER`.
  - Select menu point `CURRENT PARAMETER`.
  - Select `MACHINE PARAMETER` menu point.
  - Select `OPERATING SPACE` menu point
  - Select `PROTECTION ZONE PARAMETER` menu point.
  - ✓ The screen shows the dialog box `MACHINE PARAMETER: EDITING`.
  - Enter a 1 (= ON) in the `MONITORING` field.

- **Setting protection zones**
  - Select `MACHINE` operating mode
  - Select `SETTING` operating mode
  - Select `PROTECTION ZONE` menu point
  - ✓ The screen describes the dialog box `SETTING PROTECTION ZONES`.
  - Press the `CONTINUE` key.
  - ✓ The screen shows the messages `MOVE TO PROTECTION ZONE IN...!`
  - Move to the protection zones in +X, -X, +Z and -Z and each time select the menu point `TAKE OVER VALUE`.
  - ✓ In the input fields for +X, -X, +Z and -Z the corresponding values are taken over for the protection zone.

**Caution (collision):** The protection zones are only effective with compensated tools.

---

## For the retrofit

- Control type/model is not named on pages 6-1 to 6-25 (page 6-46 names it "ELTROPILOT", see 06b). The control is a menu-driven Gildemeister CNC with operating modes `MACHINE`, `PARAMETER`, `AUTOMATIC` / `SINGLE`, `MANUAL CONTROLS`, menus `S`, `F`, `M`, `SETTING`, dialog boxes (pp. 6-12, 6-17, 6-21, 6-25).
- Two independent "modular groups" `AG 1`/`AG 2` (one per working space), each with own operating mode; "operating mode splitting" is a parameter switch: ON = cycle start / cycle stop / feed stop act on the AG chosen by the AG selector switch, OFF = these keys act on both AG at once (p. 6-4). Separating wall recommended when splitting (p. 6-4). Fits a two-LinuxCNC-instance setup with shared/forwarded cycle start/stop/feed-hold.
- Coordinates: each side is a mirror of the other; Z axes oppose each other, +Z points away from each spindle toward the middle, +X up/outward; both spindles' `CCW` defined in their own frame, spindles run opposed (p. 6-3). Configure each side with identical sign convention.
- Part transfer ("full machining"): load left spindle, machine, [open automatic dividing wall], "angular synchronous transfer" (both chucks meet and hold the part, spindles angle-synchronised), [close dividing wall], machine 2nd side on right spindle, unload right (pp. 6-5, 6-6). No M-codes for the transfer on these pages. Automatic dividing wall is a protection-zone component (p. 6-25).
- Chucking hydraulics per headstock (p. 6-10): way valve solenoids `Y 18241`/`Y 18251` (left), `Y 24241`/`Y 24251` (right); pressure reduction valve with integrated pressure switch `F 18041` (left), `F 24041` (right); chucking-stroke proximity switches `S 18121`/`S 18111` (left), `S 24121`/`S 24111` (right). Prefix 18xxx = left headstock, 24xxx = right.
- Interlocks the control must implement: low/dropping chuck pressure prevents spindle start or stops the spindle (p. 6-11); stroke switches must show the chuck within the operative stroke ("Front" limit switch 2, "Rear" limit switch 1), otherwise no secure clamping / no spindle start (pp. 6-11, 6-14, 6-15). Chuck opened/closed by foot switches or program (p. 6-9).
- Chuck parameters: `CHUCKING MODE` 1 internal / 2 external / 3 disengage; `CHUCK TYPE` 1 manual / 2 power; `CHUCKING CYLINDER` 0 none / 1 jaws open / 2 jaws close (p. 6-17). Max spindle speed parameter `ABSOLUTE MAX: ROTATIONAL SPEED` (p. 6-18) must not be exceedable by program.
- Chucking pressure 10-60 bar system range, set by hand wheel on pressure reduction valve (key switch), gauge at left machine edge (p. 6-19). Standard chuck Berg KH 200, fixing screws 67 Nm; flange concentricity 0.005, chuck run-out 0.01 (p. 6-13).
- Turret ("tool holder") swivels the selected tool into working position on tool selection; driven tools must be engaged; do not change the swung-in driven tool (pp. 6-20, 6-21, 6-24).
- Tool setting by scratching (`TOOL MEASURING MODE` `TYPE` 0 = scratch, "parameter 7"); X offset measured as distance tool centre to machine-fixed reference point (pp. 6-23, 6-24). Software protection zones in ±X/±Z, active in manual and automatic modes, only with compensated tools (p. 6-25).
