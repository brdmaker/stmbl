# Gildemeister CTX twin operating manual, 6. Setting up (Einrichten), pages 6-26 to 6-50

Source: `7076ce74-6_Einrichten.pdf`, PDF pages 26-50 of 50 (scanned), manual pages 6-26 to 6-50, "Betriebsanleitung CTX twin, Dok.-Nr.: daaab1d1, Februar 1995" (page 6-50 revised: "REV: dab9501 (05.05.1995)"), chapter 6 "Einrichten" (Setting up).
Original language: German (translated to English; screen texts, menu items and dialog names given in German in backticks with English).

Continuation of `06a_setting_up_p01-25.md` (contents list, 6.1 to 6.9).

Running page header: "6. Einrichten". Footer: "Betriebsanleitung CTX twin, Dok.-Nr.: daaab1d1, Februar 1995, Seite 6-xx". Margin symbols: warning triangle = **Warning:**, pointing hand = **Note:**, collision symbol = **Caution:**. "✓" marks a result/screen reaction.

---

## 6.10 Additional machining possibilities in operating mode MACHINE (Zusätzliche Bearbeitungsmöglichkeiten in der Betriebsart MASCHINE)

(Page 6-26.)

You have the following additional machining possibilities in operating mode `MASCHINE`:

- continuous-feed turning (`Dauervorschubdrehen`),
- parallel machining (`Parallelbearbeitung`)

### 6.10.1 Continuous-feed turning of long workpieces without a program (Dauervorschubdrehen langer Werkstücke ohne Programm)

In operating mode `MASCHINE` you can select continuous feed. This lets you face or longitudinally turn long workpieces without a program, e.g. clean up a face or a diameter, without having to keep the feed key pressed.

**Caution:** (collision symbol) You must switch off the continuous feed again by hand, as the control has no end dimension at which it could stop automatically:

- with cycle stop (`Zyklus-Stop`) (key symbol: rectangle with circular arrows around a circle)

or

- with feed hold (`Vorschub Halt`) (key symbol: zigzag line over a circle)

**Caution:** (collision symbol) Always watch the turning process.

- Select operating mode `MASCHINE`
- Select menu item `MANUAL`
- Select menu item `DAUERVORSCHUB` (continuous feed)
- ✓ The screen shows the dialog box `DAUERVORSCHUB`.
- Select the required direction (X or Z) with the `WEITER` key
- Confirm your choice with the cycle start key (`Zyklus-Start`).
- ✓ The slide now moves in the selected direction until you stop it again.

**Caution:** (collision symbol) The slide only stops when you press the key `«ZYKLUS-STOP»` or `«VORSCHUB HALT»`.

(Page 6-27.)

- Stop the feed movement with cycle stop (key symbol)

or

- with feed hold (key symbol)

### 6.10.2 Parallel machining (Parallelbearbeitung)

An automatic working-area partition wall (`Arbeitsraumtrennwand`) is advantageous. It divides the machine into two separate working areas; loading and unloading can take place time-offset. The required aggregate **AG 1 or AG 2** is selected with the selector switch, see page 5-10.

The setting-up procedures run as already described.

## 6.11 Option C axis (Option C-Achse)

(Page 6-28.)

With the Gildemeister CNC lathes of the CTX series you can machine in C-axis mode. In combination with driven tools, C-axis mode enables contours to be produced on peripheral (cylindrical) and end faces, e.g. pockets, polygons and hole patterns.

### 6.11.1 The work steps at a glance (Die Arbeitsschritte im Überblick)

Here follows an overview of the complete work sequence of C-axis machining:

- select the aggregate concerned, AG 1 or AG 2
- enter the speed and start the driven tools
- set up the tools
- if necessary shift the zero point of the C axis (this is required for special applications, e.g. when machining profile material or workpieces that already have a bore, where the zero point must be set with reference to this bore).

**Caution:** (collision symbol) Fit sealing plugs on unused tool holders when you change driven tools.

### 6.11.2 Entering the speed and starting driven tools (Drehzahl eingeben und angetriebene Werkzeuge starten)

(Pages 6-29 and 6-30.)

- Make the machine ready for operation and swivel in the driven tool.

- **Setting by hand** (`Einstellen von Hand`)
  - Select the spindle concerned with the spindle key (key symbol: tool/spindle pictogram)
- Select operating mode `MASCHINE`
- Select menu item `S`
- Select menu item `DREHZAHL S`
- ✓ The screen shows the dialog box `DREHZAHLWERT`
- Enter the required speed.
- Press the spindle jog key (`Spindel-Tipptaste`) briefly once if the tool has just been swivelled in
- Start the driven tool by pressing the spindle jog key again
- ✓ The screen shows the direction of rotation
- Press the key `«Spindel Stop»` (spindle stop)
- ✓ The speed of the driven tool is set

- **Setting via M function** (`Einstellen über M-Funktion`) (page 6-30)
  - Swivel in the tool

**Caution:** (collision symbol) A speed must have been entered for the tool (see "Setting by hand").

- Select operating mode `MASCHINE`
- Select menu item `M`
- Select menu item `M-DIREKT` (M direct)
- ✓ The screen shows the dialog box `M-FUNKTION` (M function)
- Enter the number of the required M function and confirm:
  - M103 = direction of rotation clockwise, viewed from the drive side
  - M104 = direction of rotation counter-clockwise, viewed from the drive side

**Caution:** (collision symbol) Note that some tool holders have a reversal of the direction of rotation.

### 6.11.3 Measuring tools (Werkzeuge verrechnen)

**Prerequisite** (`Voraussetzung`):
A speed must have been entered for the driven tool, see 6.11.2 "Entering the speed and starting driven tools" on page 6-29. You must also activate the C axis.

- **Activating the C axis** (`C-Achse aktivieren`)
  - Select operating mode `MASCHINE`
  - Select menu item `M`
  - Select menu item `M-DIREKT`
  - ✓ The screen shows the dialog box `M-FUNKTION`.
  - Enter 14 (= `C-Achse 1 EIN`, C axis 1 ON) and confirm
  - ✓ The machine is in C-axis mode; the C-axis reference point is approached. In the display area the field C is enabled.

- **Deactivating the C axis** (`C-Achse deaktivieren`) (page 6-31)

  You must deactivate the C axis to return to normal operation.
  - Select operating mode `MASCHINE`
  - Select menu item `M`
  - Select menu item `M-DIREKT`
  - ✓ The screen shows the dialog box `M-FUNKTION`
  - Enter 15 (= `C-Achse 1 AUS`, C axis 1 OFF) and confirm
  - ✓ The C axis is deactivated

| M code | Function (as given on pages 6-30, 6-31) |
|---|---|
| M14 | `C-Achse 1 EIN` (C axis 1 ON); machine goes to C-axis mode and approaches the C-axis reference point |
| M15 | `C-Achse 1 AUS` (C axis 1 OFF), return to normal (spindle) operation |
| M103 | driven tool, rotation clockwise viewed from the drive side |
| M104 | driven tool, rotation counter-clockwise viewed from the drive side |

### 6.11.4 Shifting the C-axis zero point (Nullpunkt der C-Achse verschieben)

(Page 6-31.)

For certain machining operations the zero point of the C axis must be shifted (see above).

- Select operating mode `PARAMETER`
- Select menu item `AKTUELLE PARAMETER` (current parameters)
- Select menu item `EINRICHTEN`
- ✓ The screen shows the list box `AUSWAHL DER AKTUELLEN EINRICHTE-PARAMETER` (selection of the current set-up parameters)
- Select the list element `Nullpunktverschiebung C-Achse` (zero offset C axis)
- ✓ The screen shows the dialog box `AKTUELLE EINRICHTEPARAMETER EDITIERUNG` (current set-up parameters editing), in which you can enter the zero offset of the C axis in degrees.
- Enter the required value and confirm.
- ✓ The zero offset of the C axis is effective.

## 6.12 Option part pick-up device (Option Abholeinrichtung)

(Page 6-32.)

The pick-up device is used to pick finished turned workpieces off the right headstock in operating mode `AUTOMATIK`. The finished workpiece is gripped and placed on the workpiece conveyor belt.

- max. Ø 50 mm
- max. length 200 mm

1. Conveyor belt (`Transportband`)
2. Swivel arm (`Schwenkarm`)
3. Gripper (`Greifer`)

Figure: end view of the right working area: swivel arm (2) pivoting from the conveyor belt (1) at the bottom up to the spindle centre, with the gripper (3) at its end; dashed outlines show the arm in its swung positions and the chuck.

### 6.12.1 Setting up the pick-up device (Abholeinrichtung einrichten)

(Page 6-33.)

For this, spindle motor 2 must be moved to the correct position in the working area. This position depends on the clamping device and the workpiece length.

**Prerequisite:** The tools are measured, and a finished turned workpiece is clamped.

- **Setting up** (`Einrichten`)
  - Move spindle motor AG 2 to the transfer position (`Übernahmeposition`) suitable for the workpiece.
  - Press the key `«Werkstückgreifer auf»` (workpiece gripper open) until the gripper is completely open.
  - Press the key `«Schwenkarm Abholeinrichtung vor»` (swivel arm pick-up device forward) until the pick-up device is at turning centre.
  - Select the menu item `MASCHINE/HANDSTEUERN/EINRICHTEN/MASCHINENMASSE` (machine / manual control / set-up / machine dimensions)
  - Take over the position values, relative to the machine zero point, into the control as `MASCHINENMASS` (machine dimension) no. 7 or higher.
  - or take over the position value into the NC program: with `G 701 X... Z...`
  - ✓ The pick-up device is now set up
  - Check that the workpiece is picked up correctly

### 6.12.2 Pick-up device, workpiece ejector (Abholeinrichtung Werkstückausstoßer)

(Page 6-34.)

The ejector ensures that the workpiece is placed safely from the clamping device into the pick-up device. It is a spring-preloaded device. 3 springs with different spring constants are available. The spring size is to be chosen depending on the weight of the workpiece to be machined.

| Spring size | Use |
|---|---|
| Gr. 1 | up to approx. 1 kg workpiece weight, and for aluminium parts |
| Gr. 2 | from 1 to approx. 2 kg workpiece weight |
| Gr. 3 | generally for workpieces that tend to tilt/jam, with a length of 150 - 200 mm, or from 2 to approx. 4 kg workpiece weight |

Make sure that the maximum workpiece dimensions are not exceeded.

- max. Ø 50 mm
- max. length 200 mm
- max. insertion depth 55 mm (measured from the chuck front edge)

**Note:** (pointing-hand symbol) The device is designed so that jaw flushing through the spindle is possible.

(Page 6-35.)

Figure: longitudinal section through the right spindle with the ejector installed in the draw tube: disc (8) at the chuck face (workpiece shown dotted), (6) rod/pusher at the front, receiving tube (1) in two halves screwed into the draw tube, (10) [uncertain: retaining part near the front of the receiving tube], (7) spring inside the guide tube, guide tube (4) on the threaded rod, circlip (3) at the rear. Callouts: 8, 6, 1, 10, 7, 4, 3.

When changing over to a different workpiece size, please proceed as follows:

- Remove the disc (8)
- Unscrew the receiving tube (1) from the draw tube using a socket wrench size SW 32
- Unscrew the receiving tube (1) into its two halves
- Release the circlip (3) (`Seegerring`)
- Pull the guide tube (4) off the threaded rod
- Change the spring
- Reassemble the device in reverse order

## 6.13 Option two-pressure clamping device (Option Zwei-Druck-Spanneinrichtung)

(Page 6-36.)

This device is used to clamp the workpiece with two different clamping forces.

**Function** (`Funktion`)
2 pressure reducing valves are selectively actuated via a 2-way valve. This 2-way valve is switched by M command, see 9.2 "Description of the M commands" (`Beschreibung der M-Befehle`) on page 9-21.

For the hydraulic control see the technical documentation, mechanical, the corresponding hydraulic plan.

**Prerequisite:** The last workpiece was machined with low pressure.

- Start the program with the M command for normal pressure (M34)
- The machine stops with the message `Kein Spanndruck vorhanden` (no clamping pressure present).
- Open the hood and re-clamp with the foot switch
- Restart the program
- ✓ The program runs up to the M command for low clamping pressure (M35) and M0 (program stop). The machine stops.
- Open the hood and secure the workpiece in the chuck by holding it
- Open the chuck with the foot switch and close it again
- ✓ The workpiece is now clamped with low pressure.
- Close the hood, start the program and machine the workpiece

**Note:** (pointing-hand symbol) If the difference from high to low pressure is too large, the chuck may not open because of excessive wedge forces (caused by high clamping forces). In such cases it is advisable to split the machining into two programs.

Between these two programs, selection of the low pressure in operating mode `HANDSTEUERN` (manual control) is required. The chuck can thus still be opened with high pressure and re-clamped with low pressure.

| M code | Function (page 6-36) |
|---|---|
| M34 | normal (high) clamping pressure |
| M35 | low clamping pressure |
| M0 | program stop |

## 6.14 Option tool measuring device (Option Werkzeugmeßeinrichtung)

(Pages 6-37 and 6-38.)

You can measure tools in three different ways:

- by scratching (`Ankratzen`)
- by approaching a probe (`Meßtaster`)
- with optics (`Optik`)

Measuring with the probe is explained here (for measuring by scratching see 6.8 "Measuring subsequent tools (tools not yet measured)" on page 6-23).

**Setting the parameter** (`Parameter setzen`)

- Select operating mode `PARAMETER`
- Menu item `AKTUELLE PARAMETER`
- Menu item `NC-SCHALTER`
- Menu item `WKZ-MESSART`
- ✓ The screen shows the dialog box `MASCHINEN-PARAMETER: EDITIERUNG`.
- Enter the digit 1 (= probe, `Taster`) in the field `ART:` and confirm

**Measuring device** (`Meßeinrichtung`)

**Caution:** (collision symbol) When swivelling the carrier watch the swing circle of the tools. Note the tool type! Some types are only approached in the Z direction. Observe the screen messages!

The tool measuring device cannot be used with a workpiece clamped!

- Open the guard
- Select operating mode `MASCHINE`
- Select menu item `EINRICHTEN`
- Key switch to set-up mode (`Einrichtebetrieb`) AG 1 or AG 2 (key-switch symbol: black dot on a bar)
- Swivel in the measuring arm (`Meßarm`)
- Select menu item `WKZ EINRICHTEN`
- Select menu item `WKZ MESSEN`
- ✓ The screen shows the dialog box `WERKZEUGANWAHL`.
- Enter the corresponding tool number and confirm
- ✓ The screen shows the dialog box `WKZ MESSEN T..` (here enter the number of the registered tool) (page 6-38)
- Move the cursor into field X and press the `«Weiter»` key
- Approach the probe in X
- ✓ As soon as the probe is touched, the X dimension is taken over automatically.
- After probing, move the tool clear again by at least 2 mm against the approach direction.
- Move the cursor into field Z and press the `Weiter` key
- Approach the probe in Z.
- ✓ As soon as the probe is touched, the Z dimension is taken over automatically.
- After probing, move the tool clear again by at least 2 mm against the approach direction.
- if necessary edit further tool data or carry out further measurements
- Before ending the measurements, move the axes clear
- After ending the measurements close the dialog box
- Swivel out the measuring arm
- Select menu item `EINRICHTEN`

**Caution:** (collision symbol) Do not forget to fold the probe back in. But only after you have closed the dialog box `WKZ MESSEN T..`. Otherwise mechanical vibrations can cause wrong measured values to be taken over.

## 6.15 Option bar machining (Option Stangenbearbeitung)

(Page 6-39.)

You can machine bar material with various options:

- bar machining with the lathe only
- bar machining with bar feed (`Stangenvorschub`)
- bar machining with bar loading magazine (`Stangenlademagazin`)

### 6.15.1 Bar machining with the lathe only (Stangenbearbeitung nur mit der Drehmaschine)

Whenever you want or need to machine bars, you need spindle reducing tubes (`Spindelreduzierrohre`). These are guide tubes that you insert into the spindle.

The inside diameter of the spindle reducing tube must always be **1 mm larger** than the diameter of the material to be machined.

**Warning:** (warning triangle) During machining, the bar end must on no account protrude out of the back of the spindle reducing tube.

Insert the spindle reducing tube as follows:

- Stop the spindle and open the guard
- ✓ The spindle can no longer start
- Remove the rear cover of the hollow clamping cylinder (`Hohlspannzylinder`) by loosening the three screws (see figure)
- Remove the clamping ring for the spindle reducing tube by loosening the three screws (see figure).

Figure: rear view of the clamping cylinder: round cover with three screws on a bolt circle and the hose connection (rotary feed-through) at lower left.

- Insert the spindle reducing tube into the draw tube (see figure)

(Page 6-40.) Figure: spindle reducing tube: flange with stop (1) at the left (rear) end, long tube, guide bush at the front end near the chuck.

- Push the spindle reducing tube through up to stop 1.
- Fit the clamping ring and closing cover.
- Remove the closing cover on the chuck (see figure).

Figure: front view of a 3-jaw chuck with three jaws at 120° and the central bore with closing cover and its screws.

- Insert the bar material.

**Warning:** (warning triangle) The bar end must on no account protrude beyond the rear stop of the spindle reducing tube. Otherwise injury to persons and damage to the machine may result.

### 6.15.2 Automatic material feed and removal (Automatische Material Zu- und Abführung)

#### 6.15.2.1 General notes (Allgemeine Hinweise)

(Page 6-41.)

The bar feed or bar loading magazine has been installed and commissioned as described in the manufacturer's operating instructions. For all work from the bar the following applies:

**Warning:** (warning triangle) The bar end must on no account protrude beyond the rear stop of the spindle reducing tube. Otherwise injury to persons and damage to the machine may result.

If the manufacturer specifies guidance of the material bar in the spindle, the inside diameter of the spindle reducing tube must always be **1 mm larger** than the diameter of the material to be machined, see 6.15.1 "Bar machining with the lathe only" on page 6-39.

**Straightness of the bars** (`Geradheit der Stangen`)

The deviation from straightness should not exceed a **tolerance of 0.5 mm per running metre**. This figure refers to a uniform curvature, not to a short kink within one metre.

Curvature in mm/m (`Krümmung in mm/m`):

| Curvature | Rating |
|---|---|
| < 1.5 mm / 3 m | good (`gut`) |
| 2.0 mm / 3 m | satisfactory (`zufriedenstellend`) |
| > 2.5 mm / 3 m | problematic (`problematisch`) |

**Limit speeds** (`Grenzdrehzahlen`)

Information on limit speeds is contained as guide values in the manufacturers' operating instructions.

**Remaining workpiece length** (`Werkstück-Restlänge`)

(Page 6-42.)

The remaining workpiece length is determined by the length of the headstock with hollow clamping device and the mounted clamping device, but must be set optimally for the clamping device used, see the operating instructions. In general it should be noted that with collet chucks the pusher head (`Stößelkopf`) may move in up to max. 5 mm before the rear clamping face of the collet, and the remnant length thus results from the length of the collet clamping face and of the workpiece and from the distance from the chuck front edge to the parting face.

**Processing profile bars** (`Verarbeitung von Profilstangen`)

Reloading new profile bars into loading magazines always requires special attention. In any case, collet chucks must be used in the lathe. If push-type collets to DIN 6343 are used, they must if necessary be secured against rotation in the chuck.

The magazine-internal cycle control (FMB) can be set so that threading-in at the programmed speed is possible and the machine starts only after a predefined feed length.

**Bar preparation** (`Stangenvorbereitung`)

(Page 6-43.)

When the maximum holding diameter of the guide tube is used, the bar end must be chamfered concentrically at 30°. See figure.

Figures:
- Top: pusher head (`Stößelkopf`, shown in section at left) and the bar end, chamfered at 30° down to a diameter d = 0.8xD; tolerance frame: concentricity `◎ Ø 0,3 A` relative to datum A (bar diameter D).
- Middle: pusher head bore with diameter d, tolerance +0.3/+0.2 [uncertain reading of the tolerance digits]; plain bar end with diameter d, tolerance frames: perpendicularity `⊥ 0,3 A` and run-out `↗ 0,1 A`, datum A.
- Bottom (hollow bars): tube end chamfered 30° to d = 0.8xD, concentricity `◎ Ø 0,3 A`, with a closing plug sealed by an `O - Ring` inserted in the bore.

If the bar diameter is approx. 4 mm smaller than the pusher head, bar preparation can be dispensed with, provided however:

- that the bar end shows no deformation whatsoever after sawing

**Hollow bars:** (tubes) (`Hohle Stangen: (Rohre)`)

Make a closing plug to prevent oil losses and contamination of the lathe's coolant with oil.

#### 6.15.2.2 Machining with bar feed (Bearbeitung mit Stangenvorschub)

(Page 6-44.)

With a bar feed you can machine bar parts up to several metres in length.

- Insert the spindle reducing tube as described in 6.15.1 "Bar machining with the lathe only" on page 6-39 (see also technical documentation, mechanical).
- Carry out installation and commissioning of the bar feed according to the manufacturer's instructions.
- Select operating mode `PARAMETER`
- Select menu item `AKTUELLE PARAMETER`
- Select menu item `SPS-PARAMETER`
- Select selector switch AG 1
- Select menu item `HANDHABUNG` (handling)
- ✓ The screen shows the dialog box `MASCHINEN-PARAMETER: EDITIERUNG`
- In the field `ZUFUEHR:` (feed/supply) select the digit 2 (= bar feed, `Stangenvorschub`) and confirm

**Caution:** (collision symbol) If you have changed this parameter, then switch the main switch off and on again after approx. 15 sec., so that the control can re-detect the machine equipment.

- Set the key switch `«Automatische Werkstückzufuhr»` (automatic workpiece feed) to on (symbol: vertical bar "I")
- For safety, position the stop of the tool carrier in front of the chuck (stop or bar gripper).
- Put a new bar into the guide (procedure for bar loading according to the operating instructions of the bar feed).
- Press the key `«Stangenvorschub Tippbetrieb»` (bar feed jog mode).
- ✓ The bar is pushed into the working area.
- Part off the bar end so that the part program can then start at the required position.
- ✓ You can start the program.

(Page 6-45.)

**Note:** (pointing-hand symbol) When machining bars with a bar feed please note the following:
After each program run, the control queries the bar feed whether there is still material in the guide. If the bar is not at its end, the part program continues to be processed. When the bar is at its end, the screen shows the message:
`1402 M:STANGENFUEHRUNG: STANGE WECHSELN` (bar guide: change bar)

- Remove the remnant from the chuck, and insert a new bar according to the operating instructions of the bar feed
- When the bar feed is swung out, the error appears:
  `1401 F:STANGENBETRIEB: NICHT BETRIEBSBEREIT` (bar operation: not ready for operation)
- When the new bar has been inserted and the feed has been swung in again and locked, the fault must be acknowledged.

If the start of the bar first has to be machined, this can be done manually in operating mode `"HANDSTEUERN"` or in operating mode `"AUTOMATIK"` with a special part program. Then the corresponding part program is started again with block N1.

| Message no. | Text | English |
|---|---|---|
| 1401 | `F:STANGENBETRIEB: NICHT BETRIEBSBEREIT` | F (fault): bar operation: not ready for operation |
| 1402 | `M:STANGENFUEHRUNG: STANGE WECHSELN` | M (message): bar guide: change bar |

**Example program for bar feeds** (`Beispielprogramm für Stangenvorschübe`)

**Main program** (`Hauptprogramm`)

| Block | Content | Comment (German) | English |
|---|---|---|---|
| Nxxx | ..... | Startbedingungen | start conditions |
| : | | Spannfutter öffnen | open chuck |
| Ny00 | ..... | Stange vorschieben | advance bar |
| Ny01 | M65 | Abfrage auf Stangenende (*) | query for bar end (*) |
| Ny02 | M37 | Spannfutter schließen | close chuck |
| Ny03 | ..... | Teilekontur erstellen | produce part contour |
| : | | Teil abstechen | part off |
| Nz01 | M99 | Programmende mit Rücksprung zum Anfang und Wiederstart | program end with jump back to start and restart |

(*) Query to the bar feed for end of material (`Abfrage an den Stangenvorschub auf Materialende`).

#### 6.15.2.3 Machining with bar loading magazine (Bearbeitung mit Stangenlademagazin)

(Page 6-46.)

Set up the bar loading magazine and carry out commissioning according to the operating instructions of the bar loading magazine.

**Note:** (pointing-hand symbol) As a rule no spindle reducing tube is needed. Depending on the make, however, other conditions may apply.

- Select operating mode `PARAMETER`
- Select menu item `AKTUELLE PARAMETER`
- Select menu item `SPS-PARAMETER`
- Select selector switch AG 1
- Select menu item `HANDHABUNG`
- ✓ The screen shows the dialog box `MASCHINEN-PARAMETER: EDITIERUNG`.
- In the field `ZUFUEHR:` select the digit 1 (= bar loader, `Stangenlader`) and confirm.

**Caution:** (collision symbol) If you have changed this parameter, then switch the main switch off and on again after approx. 15 sec., so that the control can re-detect the machine equipment.

- Set the key switch `"Automatische Werkstückzufuhr auf"` to on (symbol "I")
- Put the bars into the bar loading magazine according to the operating instructions for the bar loader.
- Align the first bar according to the operating instructions for the bar loader.
- Move the tool carrier to the required position.
- ✓ You can start the program. Bar remnants are disposed of automatically, and new bars are reloaded automatically until there are none left in the magazine.

| `ZUFUEHR` value | Meaning |
|---|---|
| 1 | bar loader (`Stangenlader`) |
| 2 | bar feed (`Stangenvorschub`) |

**Subprograms for bar loading magazines** (`Unterprogramme für Stangenlademagazine`)

With the control `ELTROPILOT` program branching is possible, i.e. with loading magazines a special subprogram can be called to machine a face of the newly loaded bar.

**Example program for bar loading magazines** (`Beispielprogramm für Stangenlademagazine`)

(Page 6-47.)

**Main program**

| Block | Content | Comment (German) | English |
|---|---|---|---|
| N...... | | Stange vorschieben | advance bar |
| | | Kontur bearbeiten | machine contour |
| | | Teil abstechen | part off |
| | | Teil abholen | pick up part |
| N..... | | | |
| N200 | M65 | Abfrage auf Stangenende | query for bar end |
| N201 | G909 | Interpreterstop | interpreter stop |
| N202 | IF {E1 [1] = = 1} | Abfrage des Ereignisses "Stangenende" | query of the event "bar end" |
| N203 | THEN | Ausführung wenn Stangenende | execution if bar end |
| N204 | G0 X0 Z.. T1 M5 | Startbedingung, Spindel Stop | start condition, spindle stop |
| N205 | M36 | Futter öffnen für Stangenwechsel | open chuck for bar change |
| N206 | M38 | Futter schließen nach Stangenwechsel | close chuck after bar change |
| N207 | G14 | Fahren auf Werkzeugwechsel | move to tool change (position) |
| N208 | ..... | | |
| N220 | ELSE | Ausführung, wenn kein Stangenende | execution if no bar end |
| N221 | ENDIF | Ende der Programmverzweigung | end of program branching |
| N222 | M99 | Rücksprung zum Wiederstart | jump back for restart |
| ENDE | | | (end) |

If it is necessary for the spindle to rotate when a new bar is changed in (e.g. for polygon material), blocks N204/N205 must be changed:

| Block | Content | Comment (German) | English |
|---|---|---|---|
| N.... | | | |
| N204 | G97 G0 X0 Z.. S200 T1 M3 | Startbedingung, Spindel Ein | start condition, spindle on |
| N205 | M33 | Spannfutter auf für Stangenwechsel mit Freigabe für Spindellauf | chuck open for bar change with enable for spindle running |

Please note that the spindle running time with the chuck open is limited to 3 min.

## 6.16 Option indicator lamp (Option Kontrolleuchte)

(Page 6-48.)

The following lamp indicators show the following operating states.

- Lamp (`Leuchte`)
  - RED (`ROT`) signals that the operating state "emergency stop" (`"Not-Aus"`) is present on the lathe
  - YELLOW (`GELB`) signals that the guards (`Schutzhauben`) of the lathe are open
  - GREEN (`GRÜN`) signals that the lathe is running in automatic mode with at least 75% (feed setting) override of the handwheel [uncertain: "Überlagerung des Handrades", i.e. the feed override setting].

These lamps can be tested for function with the lamp test function of the CNC control (operating mode Service, see programming manual).

## 6.17 Option overload clutch X and Z axis (Option Überlastkupplung X- und Z-Achse)

(Page 6-49.)

As an option, the machine can be equipped with an overload clutch in the X and Z axes. Due to the arrangement of the clutch between drive motor and feed screw, the torque is transmitted and at the same time safeguarded.

When the set torque is exceeded, steel balls are pressed out of the recesses of the pressure flange against a spring force and the clutch disengages. As the disc springs are operated in the falling range of their characteristic, the torque drops immediately when the clutch disengages. At the same time the signal is sent via a contactless limit switch to the lamp `"Überlast"` (overload) (key/lamp symbol: a square with a broken shaft), and the error message `1553: NOT-AUS` (emergency stop) appears on the control monitor. At the same time the servo drives for the X and Z axes are braked as quickly as possible.

The fault is to be cleared as follows:

- Switch the main switch off and on again, to safely delete remaining travel paths
- Briefly press the button `"Überlast"` as acknowledgement. If the lamp in the button goes out, both clutches are engaged again and work can continue.
- If the lamp in the button does not go out, operating mode `"Handsteuern"` must be selected. Then press the button `"Überlast"` and at the same time move the corresponding axis (axes) in the opposite direction with the manual direction keys (maximum 10 mm). The lamp `"Überlast"` goes out and the fault is acknowledged.

After the overload has been removed, the synchronous clutch re-engages automatically after one revolution at the point where it disengaged. Drive and output sides always have the same angular position relative to each other in operation.

After an overload event it must be checked whether this has caused a dimensional offset on the workpiece or, in the case of a collision, whether the geometry of the machine has changed. For this a geometric check according to DIN 8605 must be carried out. If the clutch is damaged it must always be replaced completely.

## 6.18 Workpiece contact (seating) check (Werkstückanlagekontrolle)

(Page 6-50. A "!" change bar is printed in the margin next to the heading; the page footer carries the revision note "REV: dab9501 (05.05.1995) zu daaab1d1 Februar 1995".)

A prerequisite for accurate machining of certain workpieces is their correct position in the clamping devices. The contact check establishes this after loading (the workpiece is clamped). Loading is done either by an automatic loading system, e.g. a gantry robot, or by transfer from one machine chuck to the other. The workpiece contact is monitored automatically after every clamp command. With this M function the monitoring process is synchronised with the part program.

M 027 or M 327 triggers the following functions and sequences:

- With the clamping device open, a monitoring cycle is started
- The result of the last monitoring cycle is transferred into the event flag (`Ereignismerker`)
  - E 1.4 for spindle H
  - E 2.4 for spindle 3

  Here:
  - 1 signal: workpiece is in contact (`Werkstück liegt an`)
  - 0 signal: workpiece is not in contact (`Werkstück liegt nicht an`)

  If a monitoring process is running, the transfer takes place only when the result is available.
- Data enable is withdrawn until the result has been written into the event query.
- If the contact with the workpiece is not correct, the message appears
  `5603 M: Hauptspannfutter AG1: Werkstück liegt nicht an` (main chuck AG1: workpiece not in contact)
  or
  `10603 M: HAuptspannfutter AG2: Werkstück liegt nicht an` (main chuck AG2: workpiece not in contact) (sic, "HAuptspannfutter")

The event query is to be evaluated by the part program after M 027 or M 327.

| Item | AG 1 | AG 2 |
|---|---|---|
| M function [pairing with AG inferred, the page lists both without assigning them] | M 027 | M 327 |
| Event flag | E 1.4 (spindle H) | E 2.4 (spindle 3) |
| Message | 5603 | 10603 |

## For the retrofit

- Control: the CNC is the Gildemeister `ELTROPILOT` (page 6-46); its programs use IF/THEN/ELSE/ENDIF on event flags (`E1 [1]`), `G909` interpreter stop, `G14` move to tool-change position, `G701 X.. Z..` machine-dimension positions, `M99` jump to start (pages 6-33, 6-47).
- Two aggregates AG 1 / AG 2 selected by a selector switch; key switch for set-up mode per AG (pages 6-27, 6-37).
- Continuous feed in X or Z in MACHINE mode without end point, stopped only by `ZYKLUS-STOP` or `VORSCHUB HALT` (pages 6-26, 6-27).
- C axis (option, per spindle): `M14` = C axis 1 ON (switch to C-axis mode and approach the C reference point), `M15` = C axis 1 OFF; C zero offset in degrees in set-up parameters (pages 6-30, 6-31). For LinuxCNC: spindle to position-mode switch plus homing on M14.
- Driven tools: `M103` clockwise / `M104` counter-clockwise viewed from the drive side; speed entered as S; spindle jog key starts the tool; some holders reverse direction (pages 6-29, 6-30).
- Clamping: `M34` normal pressure, `M35` low pressure (two-pressure option, 2 pressure reducing valves via a 2-way valve); message `Kein Spanndruck vorhanden` stops the machine (page 6-36). Bar programs use `M33` chuck open with spindle-run enable (spindle running with chuck open limited to 3 min), `M36` open for bar change, `M37` close chuck, `M38` close after bar change, `M65` query bar end (pages 6-45, 6-47). M5/M3 spindle stop/on.
- Workpiece seating check (`Werkstückanlagekontrolle`): `M027` / `M327` start a monitoring cycle after clamping, result in event flags `E 1.4` (spindle H) / `E 2.4` (spindle 3), 1 = seated; data enable is withheld until the result is in; messages `5603` (AG1) / `10603` (AG2). Explicitly used after transfer from one machine chuck to the other, i.e. part of the part transfer sequence (page 6-50).
- Part pick-up device (option) on the right side (AG 2): swivel arm, gripper, conveyor belt; max. Ø 50 mm, length 200 mm, insertion depth 55 mm; keys `«Werkstückgreifer auf»`, `«Schwenkarm Abholeinrichtung vor»`; spindle motor AG 2 is moved to the transfer position, i.e. the right spindle moves (Z on the headstock), position stored as `MASCHINENMASS` no. 7 or higher (pages 6-32 to 6-34). Spring-loaded ejector in the draw tube, springs Gr. 1/2/3 (pages 6-34, 6-35).
- Tool probe (option): `WKZ-MESSART` `ART` = 1; swing-in measuring arm, X and Z dimension taken automatically on probe contact, retract at least 2 mm; not usable with a clamped workpiece (pages 6-37, 6-38).
- Bar feed / bar loading magazine: PLC parameter `HANDHABUNG` `ZUFUEHR` 1 = bar loader, 2 = bar feed, main switch off ~15 s after change; key switch `«Automatische Werkstückzufuhr»`; key `«Stangenvorschub Tippbetrieb»`; messages `1401 F:STANGENBETRIEB: NICHT BETRIEBSBEREIT`, `1402 M:STANGENFUEHRUNG: STANGE WECHSELN` (pages 6-44, 6-45, 6-46). Hollow clamping cylinder (pages 6-39).
- Indicator lamp (option): red = emergency stop, yellow = guards open, green = automatic with feed override at least 75% (page 6-48).
- Overload clutch (option) in X and Z between motor and ball screw: contactless limit switch to lamp/button `"Überlast"`, error `1553: NOT-AUS`, X and Z servo drives braked as fast as possible; acknowledge by power cycle and `Überlast` button, or in manual mode hold `Überlast` and jog back max. 10 mm; clutch re-engages after one revolution at the same angle (page 6-49). Implement as an e-stop input per side.
