# Gildemeister CTX twin operating instructions (English edition), 6. Setting, pages 6-26 to 6-50

Source: `CTX_twin_Operating_instructions_6.pdf` (50 scanned pages), PDF pages 26-50 = manual pages 6-26 to 6-50. "Operating instructions CTX twin, Dok. No.: daaab1g1, February 1995", chapter 6 "Setting".
Original language: English. Contents list and sections 6.1-6.9 are in `06a_setting_p01-25.md`.

Running page header on every page: "6. Setting". Footer: "Operating instructions CTX twin, Dok. No.: daaab1g1, February 1995, Page 6-xx".

Margin symbols: warning triangle = **Warning:**, pointing hand = **Note:**, square collision pictogram = **Caution (collision):**.

---

## 6.10 Additional machining possibilities in MACHINE operating mode

(Page 6-26.)

The following machining possibilities are available in the `MACHINE` operating mode:

- Constant feed turning
- Parallel machining

### 6.10.1 Constant feed - turning long workpieces without a program

You can select constant feed in the operating mode `MACHINE`. This allows you to turn long workpieces across the face or longitudinally, for example "cleanly" turning a face surface or a diameter, without having to keep the feed key depressed.

**Caution (collision):** Switch off the constant feed again by hand, as the controls were not given an end dimension at which it could automatically stop:

- with cycle stop (key symbol: square with circle and circular arrows)
- or
- with feed stop (key symbol: zig-zag line over a circle)

**Caution (collision):** Pay attention during the turning cycle.

- Select `MACHINE` operating mode
- Select `MANUAL` menu point
- Select `CONSTANT FEED` menu point
- ✓ The screen shows the dialog box `CONSTANT FEED`.
- Select the required direction with the `CONTINUE` key (X or Z)
- Confirm your selection with the cycle start key.
- ✓ The slide now moves in the selected direction, until it is stopped again.

**Caution (collision):** The slide only stops when the `CYCLE STOP` or `FEED STOP` key is pressed.

(Page 6-27.)

- stop the feed movement with cycle stop (key symbol)
- or
- with feed stop (key symbol)

### 6.10.2 Parallel machining

An automatic work dividing wall is advantageous as it splits the machine into two separate working spaces, loading and unloading can be performed at different times. The required module **AG 1 or AG 2** can be selected via the selector switch, see page 5-10.

---

## 6.11 C axis option

(Page 6-28.)

With the Gildemeister CNC lathes of the CTX series it is possible to machine workpieces in the C axis. The C axis operation permits the machining of contours on nape and end faces, e.g. pockets, polygons and drilling patterns. ["nape" as printed; means the outer cylindrical surface]

### 6.11.1 General view of worksteps

The following is a general view of the complete work sequence of a C axis machining sequence:

- select the module AG 1 or AG 2 concerned
- Enter the rotational speed and start the driven tools.
- Set up the tools.
- If necessary, displace the C axis zero point. (This is necessary for special applications, e.g. for machining profile material or workpieces, which are already predrilled and the zero point must be set with reference to this hole.)

**Caution (collision):** Fit closing plugs to tool holders not in use when changing driven tools.

### 6.11.2 Enter rotational speed and start driven tools

(Page 6-29.)

- Make the machine operational and swivel the driven tool in.

- **Manual setting**
  - Select spindle concerned by pressing the spindle key (key symbol: spindle/tool pictogram)
  - Select the `MACHINE` operating mode
  - Select the menu point `S`
  - Select the menu point `ROTATIONAL SPEED S`
  - ✓ The screen shows the dialog box `ROTATIONAL SPEED VALUE`
  - Enter the required rotational speed
  - Press the spindle inching button once when the tool is newly swung in
  - Start the driven tool by pressing the spindle inching button again
  - ✓ The screen shows the turning direction
  - Press the `spindle stop` key.
  - ✓ The driven tool rotational speed is set.

(Page 6-30.)

- **Setting via the M function**
  - Swivel in the tool

**Caution (collision):** A rotational speed for the tool must be entered (see "manual setting").

  - Select `MACHINE` operating mode
  - Select menu point `M`
  - Select menu point `M directly`
  - ✓ The screen shows the dialog box `M FUNCTION`
  - Enter the required M function number, and confirm:

| M function | Meaning |
|---|---|
| M103 | Clockwise rotational direction seen from the drive side. |
| M104 | Counterclockwise rotational direction seen from the drive side. |

**Caution (collision):** Please note that some tool holders are equipped with a rotational direction reverse system.

### 6.11.3 Compensating tools

**Prerequisite:**
A rotational speed must be entered for the driven tool, see page 6.11.2 "Enter rotational speed and start driven tools" on page 6-29.
In addition, the C axis must be activated.

- **Activate the C axis**
  - Select the `MACHINE` operating mode.
  - Select the `M` menu point.
  - Select `M DIRECT` menu point
  - ✓ The screen shows the dialog box `M FUNCTION`
  - Enter 14 (= C axis 1 ON) and confirm
  - ✓ The machine is in C axis operation, the reference point C axis is moved to. The field C is released in the display range.

(Page 6-31.)

- **Deactivate the C axis**

You have to deactivate the C axis in order to return to normal operation.

  - Select the `MACHINE` operating mode.
  - Select the `M` menu point.
  - Select `M DIRECT` menu point
  - ✓ The screen shows the dialog box `M FUNCTION`
  - Enter 15 (= C axis 1 OFF) and confirm
  - ✓ The C axis is deactivated

### 6.11.4 Offset the C axis zero point

For certain machining processes it is necessary to offset the C axis zero point (see above).

- Select `PARAMETER` operating mode.
- Select `CURRENT PARAMETER` menu point
- Select `SETTING` menu point
- ✓ The screen shows the list box `SELECT THE CURRENT SETTING PARAMETERS`
- Select list element `C axis zero point offset`.
- ✓ The screen shows the dialog box `EDIT CURRENT SETTING PARAMETER` in which you can enter the zero point offset of the C axis in degrees.
- Enter the required value and confirm.
- ✓ The C axis zero point offset is active.

---

## 6.12 Pick-off device option

(Page 6-32.)

The pick-off device is intended for picking up finished turned workpieces on the right hand headstock in the `AUTOMATIC` operating mode. It picks off the finished turned workpiece and loads it onto the workpiece transport belt.

- Max. 50 mm dia.
- max. 200 mm length

1. Transport belt
2. Swivel arm
3. Gripper

Figure: end view of the pick-off device: swivel arm (2) pivoting from a point beside the spindle, gripper (3) at its tip shown at the turning centre (chuck outline dashed), and the swivel arm swung down (dash-dot) depositing the part on the transport belt (1) in a chute below.

### 6.12.1 Pick-off device setting option

(Page 6-33.)

For this the spindle motor 2 has to be moved to the correct position in the working space. This position is dependent upon chucking means and workpiece length.

**Prerequisites:**

The tools are compensated and a finished turned workpiece is clamped.

- **Setting**
  - Move the spindle motor AG 2 in the take over position suitable for the workpiece.
  - Press the `open workpiece gripper` key, until the gripper is fully open.
  - Press the `swivel arm pick-off device forward` key, until the pick-off device is at the turning centre.
  - Select the menu point `MACHINE/MANUAL CONTROL/SETTING/MACHINE DIMENSIONS`
  - Related to the machine datum, take over the position values into the controls, as the `MACHINE DIMENSION` No. 7 or higher.
  - Or take over the position value into the NC program: with `G 701 X... Z...`
  - ✓ The pick-off device is now set up.
  - Check that the workpiece is correctly picked off.

### 6.12.2 Pick-off device workpiece ejector

(Page 6-34.)

The ejector guarantees the secure insertion of the workpiece from the clamping means into the pick-off device. This is a pretensioned spring loaded device. There are three springs available with differing spring constant values. Select the spring size dependent upon the weight of the workpiece to be machined.

- Size 1 up to approx. 1 kg workpiece weight and for aluminium parts
- Size 2 from 1 to approx. 2 kg workpiece weight
- Size 3 in all cases for workpieces which tend to tilt, having a length of 150 - 200 mm or a workpiece weight from 2 to approx. 4 kg.

Ensure that the workpiece dimensions are not exceeded.

- Max. 50 mm dia.
- Max. 200 mm length
- Max. 55 mm cut-in depth (measured from front edge of chuck

**Note:** The device is designed to ensure that jaw flushing through the spindle is possible.

(Page 6-35.)

Figure: longitudinal section through the right-hand spindle/chuck showing the ejector inside the spindle bore. Callouts: (8) disc at the rear end, (6) threaded rod, (1) take-up tube, (10), (7) spring [labels 10 and 7 not named in the text; 7 appears to point at the spring, uncertain], (4) guide tube, (3) circlip.

Proceed as follows when changing over to a different workpiece size:

- Dismantle the disc (8)
- Unscrew the take-up tube (1) from the tension tube using a socket spanner size SW 32
- Screw apart the two halves of the take-up tube (1)
- Release the circlip (3)
- Pull the guide tube (4) from the threaded rod
- Change the spring
- Fit the unit in the reverse order.

---

## 6.13 Two pressure clamping device option

(Page 6-36.)

This device serves for clamping the workpiece with two different clamping forces.

**Function:**
2 Pressure reduction valves are optionally selected via a 2 way valve. This 2 way valve is switched via the M commands, see 9.2, "Description of the M commands" on page 9-21.

For hydraulic control, refer to the corresponding hydraulic plan in the Mechanical Technical documentation.

**Prerequisite:**
Last workpiece is machined at low pressure.

- Start the program with the M command for normal pressure (`M34`)
- The machine stops with the message `no clamping pressure present`
- Open the hood and retighten using the foot switch
- Restart program
- ✓ Program runs up to the M command for low pressure (`M35`) and `M0` (program stop). Machine stops.
- Open the hood and secure the workpiece in the chuck by counterholding
- Open the chuck using the foot switch and close again.
- ✓ The workpiece is now clamped at low pressure.
- Close the hood, start the program and machine the workpiece.

**Note:** The chuck may not open due to excessive forces (created due to high clamping forces) in the event of a high difference between the high and low pressure.
In such cases it is recommended that the machining be divided into 2 programs.

Between these two programs a pressure selection of the lower pressure is necessary in the operating mode `MANUAL CONTROL`. Then the chuck can still be opened at high pressure and reclamped at low pressure.

---

## 6.14 Tool measuring unit

(Page 6-37.)

You can measure tool in three different modes:

- By scratching
- By moving in a measuring probe
- With an optical system

Here measuring with the measuring probe is explained (for measuring by scratching), see 6.8, "Compensation of the following tools ( not yet compensated tools) on page 6-23.

**Setting parameters**

- Select `PARAMETER` operating mode
- `CURRENT PARAMETER` menu point
- `NC SWITCH` menu point
- `TOOL MEASURING MODE` measuring point
- ✓ Screen shows the dialog box `MACHINE PARAMETER: EDITING`
- Enter the number 1 (= key) in the field `TYPE:` and confirm

**Measuring device**

**Caution (collision):** When swivelling the holder take note of the tool movement circle! Please note the tool type! Certain types are only moved to in the Z direction. Note screen messages!

It is not possible to use tool measuring units with the workpiece clamped.

- Open the protection hood
- Select `MACHINE` operating mode
- Select `SETTING` menu point
- Set the key switch to setting operation AG 1 or AG 2 (key switch symbol: filled dot)
- Swivel measuring arm
- Select `TOOL SETTING` menu point
- Select `TOOL MEASURING` menu point
- ✓ The screen shows the dialog box `TOOL SELECTION`.
- Enter the tool number menu and confirm.

(Page 6-38.)

- ✓ The screen shows the dialog box `MEASURE TOOL T..` (enter the number of the announced tool here).
- Move the cursor in the X field and press the `Continue` key.
- Move to the measuring probe in X.
- ✓ As soon as the measuring probe is moved to, the X dimension is automatically taken over.
- After probing move the tool free by at least 2 mm in the direction opposing the approach.
- Move the cursor to the Z field and press the continue key.
- Move to the measuring probe in Z.
- ✓ As soon as the measuring probe is moved to the Z dimension is automatically taken over.
- After probing the tool move free again by at least 2 mm opposite to the approach direction.
- If necessary process further tool data or make further measurements.
- Move the axes free prior to concluding the measurements.
- Close the dialog box after concluding the measurements.
- Swing out the measuring arm.
- Select `SETTING` menu point.

**Caution (collision):** Do not forget to hinge the probe back in, however not before the dialog box `TOOL MEASURE T..` has been closed. Otherwise incorrect measured values could be taken over due to the mechanical vibration.

---

## 6.15 Bar machining option

(Page 6-39.)

You can machine bar material with various options:

- Bar machining only with the lathe
- Bar machining with bar feeding
- Bar machining with bar loading magazine

### 6.15.1 Bar machining only with the lathe

Whenever bars are to be machined, spindle reduction tubes are required, which are inserted into the spindle.

The internal diameter of the spindle reduction tube must always be one millimetre larger than the diameter of the material to be machined.

**Warning:** When machining under now circumstances may the bar protrude from the rear of the spindle reduction tube. [as printed; "under no circumstances"]

Insert the spindle reduction tube as follows:
- Stop the spindle and open the protection hood.
- ✓ The spindle can no longer start
- Dismantle the rear cover of the hollow clamping cylinder by loosening the three screws (see illustration).
- Remove the clamping ring for the spindle reduction tube, by loosening the three screws (see illustration).

Figure: rear end of the hollow clamping cylinder (with hydraulic hose connection at lower left) seen end-on: round cover/clamping ring with screws on a bolt circle.

- Insert the spindle reduction tube in the tensioning tube (see illustration).

(Page 6-40.)

Figure: spindle reduction tube (flanged tube) pushed into the spindle from the rear up to stop (1); chuck at the right.

- Push the spindle reduction tube up to the stop 1.
- Fit the clamping ring and closing cover.
- Dismantle the closing cover on the chuck (see illustration). ["or the chuck" as printed]

Figure: front view of a 3-jaw chuck with the closing cover over the centre bore.

- Insert the bar material.

**Warning:** Under no circumstances must the bar end protrude beyond the rear stop of the spindle reducing tube. This could result in injury to persons and damage to the machine.

### 6.15.2 Automatic material infeed and outfeed

(Page 6-41. The contents list calls this "Automatic material feeding and removal".)

#### 6.15.2.1 General instructions

Bar feeder or bar loading magazine are installed and commissioned as described in the manufacturer's operating instructions. The following is applicable for all bar work.

**Warning:** Under no circumstances permit the bar end to extend beyond the rear stop of the spindle reduction tube. This can result in injury to persons and damage to the machine.

If the manufacturer specifies a guidance of the material bar in the spindle, ensure that the internal diameter of the spindle reduction tube is always 1 mm larger than the diameter of the material to be machined, see 6.15.1, "bar machining only with the lathe" on page 6-39.

**Bar straightness**

The straightness of the bar should be within a tolerance of 0.5 mm per running metre.
This information refers to a uniform bending, and not to a short bend within one metre.

Bending in mm/m:

| Bending | Rating |
|---|---|
| < 1.5 mm/3m | good |
| 2.0 mm/3m | satisfactory |
| > 2.5 mm/3m | critical |

**Rotational speed limits**

The manufacturer's operating instructions contain guide values concerning rotational speed limits.

(Page 6-42.)

**Workpiece residual length**

The workpiece residual length is determined by the length of the headstock including hollow chucking device and the clamping means fitted, however, it must be optimally matched to the clamping means employed, see operating instructions. In general it must be noted that collets allow the ram head to run in up to 5 mm in front of the rear clamping surface of the collet. Thus the offcut length is determined by the length of the collet clamping surface and the workpiece as well as the spacing from the collet front edge and the parting surface.

**Processing of profile bars**

The loading of new profile bars in loading magazines always requires particular care. In all cases the lathe must be operated with collet chucks. If pressure collets are used in accordance with DIN 6343 these must be secured in the chuck against turning.

The cycle control (FMB) built-in the magazine can be set so as to allow loading at a programmed rotational speed and the machine start only occurs after a predefined feed length.

(Page 6-43.)

**Bar preparation**

When utilizing the maximum take-up diameter of the guide tube, the bar end must be concentrically chamfered to 30°, see illustration.

Figures (three): (1) solid bar of diameter D with the end chamfered at 30° down to d = 0,8xD, concentricity tolerance `◎ Ø 0,3 A` to datum A (bar axis), shown next to the ram head (pusher) section; (2) bar end faced square, perpendicularity `⊥ 0,3 A` and run-out `↗ 0,1 A`, ram head bore marked `d +0,3/+0,2`; (3) hollow bar (tube) of outer diameter D with a plug and `O - Ring` in the bore, end chamfered 30° to d = 0,8xD, `◎ Ø 0,3 A`.

If the bar diameter is approx. 4 mm smaller than the ram head it is possible to work without bar preparation, however, on the condition:

- that the bar end does not show any deformation after sawing off.

**Hollow bars:** (tubes)

Produce a plug in order to avoid oil loss and oil smearing with the coolant of the lathe.

#### 6.15.2.2 Machining with bar feeding

(Page 6-44.)

A bar feed allows bar sections of up to several meter length to be machined.

- Insert the spindle reduction tube as described in 6.15.1 "Bar machining only with lathe" on page 6-39 (also refer to Mechanical Technical Documentation).
- Install and commission the bar feeder in accordance with the manufacturer's instructions.
- Select `PARAMETER` operating mode.
- Select `CURRENT PARAMETER` menu point.
- Select `PLC PARAMETER` menu point.
- Select selector switch `AG 1`.
- Select `HANDLING` menu point.
- ✓ The screen shows the dialog box `MACHINE PARAMETER: EDITING`
- Select the number 2 (= bar feeder) in the field `FEEDING` and confirm

**Caution (collision):** If you made changes to this parameters, switch the main switch off and on again after approx. 15 seconds, permitting the controls to reidentify the machine equipment.

- Set the "automatic workpiece feeding" key switch to [symbol missing on this page; on page 6-46 the symbol is a vertical bar "I"]
- For safety reasons position the tool holder stop in front of the chuck (stop or bar gripper).
- Insert a new bar in the guide (proceed with the bar loading in accordance with the manufacturer's instructions).
- Push the `bar feed inching operation` button.
- ✓ The bar is pushed into the working space.
- Part off the bar end so that subsequently the part program can start at the required position.
- ✓ The program can be started.

(Page 6-45.)

**Note:** For bar machining with the aid of a bar feeder ensure that the following points are observed:
Each time the program is run through, the controls check with the bar feeder, whether there is still material in the guide. When the bar is finished, the message M: "bar guide: change bar" appears on the screen. `1402 M: BAR GUIDANCE: BAR CHANGE`

- Remove the remaining material from the chuck and insert a new bar in accordance with the operating instructions for the bar feeder.
- The fault `1401 F: BAR OPERATION: NOT OPERATIONAL` appears when swinging out the feed
- Acknowledge the malfunction once the new bar is inserted and the feed swung into position again and locked.

If the start of the bar must first be machined this can be performed in the `MANUAL CONTROL` operating mode or in the `AUTOMATIC` operating mode using a special part program. Subsequently restart the corresponding part program with the N1 block.

**Bar feeding example program**

Main program

| Block | Code | Comment |
|---|---|---|
| Nxxx | ..... | Start conditions |
| : | | Open chuck |
| Ny00 | ..... | Push bar forward |
| Ny01 | M65 | Interrogate bar end (*) |
| Ny02 | M37 | Close chuck |
| Ny03 | ..... | Create part contour |
| : | | Part off |
| Nz01 | M99 | Program end with return to start and restart |

(*) Interrogation of material end from the bar feeding:

[The sentence ends here on page 6-45; nothing follows.]

#### 6.15.2.3 Machining with bar loading magazine

(Page 6-46.)

Install the bar loading magazine and undertake the commissioning in accordance with the bar loading machine manufacturer's instructions.

**Note:** A spindle reduction tube is generally not required here. However, other conditions may exists dependent upon the make.

- Select `PARAMETER` operating mode
- Select `CURRENT PARAMETER` menu point
- Select `PLC PARAMETER` menu point
- Select selector switch `AG 1`
- Select `HANDLING` menu point
- ✓ The screen shows the dialog box `MACHINE PARAMETER: EDITING`
- Select the number 1 (= bar feeder) in the field `FEEDING` and confirm [as printed; 6.15.2.2 uses 2 = bar feeder, so 1 presumably = bar loading magazine]

**Caution (collision):** If you have changed parameters, switch the main switch off and on again after approx. 15 seconds, permitting the controls to reidentify the machine equipment.

- Set the "automatic workpiece feeding" key switch to `I`
- Insert the bars in the bar loading magazine in accordance with the bar feeder operating instructions.
- Align the first bar in accordance with the bar feeder operating instructions.
- Move the tool holder to the required position.
- ✓ The program can now be started. The remaining bar material is automatically removed and new bars are automatically loaded until there are no more bars in the magazine.

**Sub programs for bar loading magazine**

A program branching is possible using the ELTROPILOT controls, i.e. a special sub-program can be selected for loading magazines in order to face the newly loaded bar.

(Page 6-47.)

**Example for bar loading magazines**

Main program

| Block | Code | Comment |
|---|---|---|
| N...... | | Push bar forward |
| | | Machine contour |
| | | Part off part |
| | | Pick-off part |
| N...... | | |
| N200 | M65 | Bar end interrogation |
| N201 | G909 | Interpreter stop |
| N202 | IF {E1 [1] = = 1} | Interrogation of the "Bar end" event |
| N 203 | THEN | Execution with bar end |
| N204 | G0 X0 Z.. T1 M5 | Start conditions, spindle stop |
| N205 | M36 | Open chuck for bar change |
| N206 | M37 | Close chuck after bar change |
| N207 | G14 | Move to tool change |
| N208 | .... | |
| N220 | ELSE | Execution when no bar end |
| N221 | ENDIF | End of program branching |
| N222 | M99 | Return to restart |
| END | | |

Change the blocks N204/N205 if it is necessary to turn the spindle when changing in a new bar (e.g. for polygon material).

| Block | Code | Comment |
|---|---|---|
| N.... | | |
| N204 | G97 G0 X0 Z.. S200 T1 M3 | Start condition, spindle ON |
| N205 | M33 | Chuck to bar change with release for spindle run |

Please note that the spindle running time is limited to 3 min. with the chuck open.

---

## 6.16 Control light option

(Page 6-48.)

The following operating modes are indicated by the following light indicators.

- **Light**
  - RED signals the presence of "Emergency stop" operating condition on the lathe
  - YELLOW signals that the lathe protection hoods are open
  - GREEN signals that the machine is running in automatic operation with at least 75% (feed setting) superposition of the handwheel running. [as printed; meaning: feed override at least 75%]

Use the lamp test function of the CNC controls to test these lights. (Operating mode service, refer to programming handbook).

---

## 6.17 Overload clutch X and Z axis option

(Page 6-49.)

As an option the machine can be equipped with an overload clutch in the X and Z axis. The torque is transmitted and simultaneously secured by arranging the overload clutch between drive motor and feed spindle.

Steel balls are pressed out of the recesses in the pressure flange by spring force and the clutch disengages once the set torque is exceeded. As the plate springs are operated in the dropping area of their characteristic curve the torque drops off immediately as the clutch disengages.
At the same time the signal is given via a proximity limit switch to the "overload" lamp (key symbol: box with a broken vertical line), the fault message `1553: EMERGENCY STOP` appears on the control monitor. Simultaneously the servo drives for the X and Z axis are quickly braked.

The fault is overcome as follows:

- Switch the main switch off and on again in order to safely delete residual traverse paths.
- Acknowledge by briefly pressing the `Overload` key. If the lamp in the button is cancelled both couplings reengage and machining can be continued.
- If the lamp is not cancelled select the `Manual control` operating mode. Following this press the `Overload` key and simultaneously move the corresponding axis(es) in the counter direction (maximum 10 mm) using the manual direction key. The `Overload` lamp is cancelled and the fault acknowledged.

After reinstating a correct load situation the synchronous clutch engages automatically after one revolution at the point where it disengaged. Drive and power take off side always have the same angular setting in operation.

Following an overload check whether this has caused a dimensional displacement on the workpiece or if the machine geometry has been changed by the collision. For this undertake a geometrical inspection in accordance with DIN 8605.
Always replace the complete clutch in the event of damage.

Page 6-50: (blank page; only show-through from page 6-49)

---

## For the retrofit

- Control: Gildemeister "ELTROPILOT" CNC (named p. 6-46), with `IF {E1 [1] = = 1} THEN / ELSE / ENDIF` program branching on PLC events and `G909` interpreter stop (p. 6-47). Operating modes `MACHINE`, `MANUAL CONTROL`, `AUTOMATIC`, `PARAMETER`; selector switch `AG 1`/`AG 2` per working space (pp. 6-27, 6-44).
- M-codes on these pages: `M3` spindle on, `M5` spindle stop, `M0` program stop, `M99` end with return to start (pp. 6-36, 6-45, 6-47); `M14` / `M15` = C axis 1 ON / OFF; on M14 the C axis moves to its reference point (pp. 6-30, 6-31); `M103` / `M104` = driven tool CW / CCW seen from drive side (p. 6-30); `M33` chuck to bar change with release for spindle run, `M36` open chuck for bar change, `M37` close chuck (p. 6-45, 6-47); `M34` normal clamping pressure / `M35` low clamping pressure (two-pressure option, switches a 2-way valve between two pressure reduction valves) (p. 6-36); `M65` interrogate bar end (pp. 6-45, 6-47). G-codes: `G0`, `G97`, `G14` move to tool change, `G701 X.. Z..` take over pick-off position, `G909` interpreter stop (pp. 6-33, 6-47).
- C axis: activated per spindle by M14, references on activation; C zero offset in degrees as a setting parameter (pp. 6-30, 6-31). Driven tools started via spindle key + spindle inching button or M103/M104; some holders reverse direction mechanically (pp. 6-29, 6-30).
- Spindle interlocks: spindle cannot start with hood open (p. 6-39); chuck-open spindle run (M33) limited to 3 min (p. 6-47); missing chuck pressure stops the program with `no clamping pressure present` (p. 6-36).
- Part handling on the right side (AG 2): pick-off device (swivel arm + gripper + transport belt, parts max. 50 mm dia., 200 mm long, 55 mm cut-in depth) works in `AUTOMATIC`, keys `open workpiece gripper`, `swivel arm pick-off device forward`; positioning by moving "spindle motor AG 2" (the right headstock is an axis that travels) to a take-over position stored as `MACHINE DIMENSION` No. 7 or higher (pp. 6-32 to 6-34). Spring ejector in the spindle (p. 6-34).
- Bar feeder / bar loading magazine on AG 1 (left): PLC parameter `HANDLING` -> `FEEDING` (2 = bar feeder; 1 printed for magazine), key switch "automatic workpiece feeding", button `bar feed inching operation`, messages `1401 F: BAR OPERATION: NOT OPERATIONAL`, `1402 M: BAR GUIDANCE: BAR CHANGE`; magazine has its own cycle control (FMB) (pp. 6-42, 6-44 to 6-46). Power cycle ~15 s after equipment parameter changes so the control re-identifies equipment (p. 6-44).
- Tool measuring probe (measuring arm swung in manually, key switch "setting operation AG 1 / AG 2"); probe contact latches the X/Z value; back off >= 2 mm; parameter `TOOL MEASURING MODE` `TYPE` 1 = key/probe, 0 = scratch (pp. 6-37, 6-38).
- Signal lamp column: RED = emergency stop, YELLOW = protection hoods open, GREEN = automatic with feed override >= 75% (p. 6-48).
- Overload clutch option on X and Z (ball-detent between motor and ball screw, re-engages after one revolution at the same angle): proximity switch -> `overload` lamp, message `1553: EMERGENCY STOP`, X/Z servos fast-braked; reset by power cycle + `Overload` key, or in `Manual control` hold `Overload` and jog back max. 10 mm (p. 6-49). Needs an input per side and a quick-stop path in the stmbl/LinuxCNC config.
