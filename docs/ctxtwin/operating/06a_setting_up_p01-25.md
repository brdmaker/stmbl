# Gildemeister CTX twin operating manual, 6. Setting up (Einrichten), pages 6-1 to 6-25

Source: `7076ce74-6_Einrichten.pdf`, PDF pages 1-25 of 50 (scanned), manual pages 6-1 to 6-25, "Betriebsanleitung CTX twin, Dok.-Nr.: daaab1d1, Februar 1995", chapter 6 "Einrichten" (Setting up).
Original language: German (translated to English; screen texts, menu items and dialog names given in German in backticks with English).

Continued in `06b_setting_up_p26-50.md` (pages 6-26 to 6-50, sections 6.10 to 6.18).

Running page header on every page: "6. Einrichten". Footer: "Betriebsanleitung CTX twin, Dok.-Nr.: daaab1d1, Februar 1995, Seite 6-xx". Margin symbols: a warning triangle (rendered here as **Warning:**), a pointing hand (rendered as **Note:**) and a collision symbol (exclamation mark with a broken part, rendered as **Caution:**). A check mark "✓" marks a result/screen reaction.

---

## Contents

(Pages 6-1 and 6-2.)

| Section | German title | English | Page |
|---|---|---|---|
| 6.1 | Allgemeine Hinweise | General notes | 6-3 |
| 6.1.1 | Koordinatensysteme | Coordinate systems | 6-3 |
| 6.1.2 | Aggregategruppen | Aggregate groups | 6-4 |
| 6.1.3 | Betriebsartensplitting | Operating mode splitting | 6-4 |
| 6.1.4 | Bearbeitungsmöglichkeiten | Machining possibilities | 6-5 |
| 6.2 | Voraussetzungen | Prerequisites | 6-7 |
| 6.3 | Die Arbeitsschritte im Überblick | The work steps at a glance | 6-7 |
| 6.4 | Die Spanneinrichtung im einzelnen | The clamping device in detail | 6-9 |
| 6.5 | Spannmittel aus- und einbauen | Removing and installing clamping devices | 6-12 |
| 6.5.1 | Spannmittel ausbauen | Removing the clamping device | 6-12 |
| 6.5.2 | Spannmittel einbauen | Installing the clamping device | 6-13 |
| 6.5.3 | Spannhub einstellen | Setting the clamping stroke | 6-14 |
| 6.5.4 | Parametereingabe Spannmittel | Parameter input, clamping device | 6-17 |
| 6.5.5 | Drehzahlbegrenzung einstellen | Setting the speed limit | 6-18 |
| 6.6 | Rohteil einspannen | Clamping the blank | 6-19 |
| 6.6.1 | Spanndruck überprüfen und ggf. einstellen | Checking and if necessary setting the clamping pressure | 6-19 |
| 6.6.2 | Werkzeugträger bestücken | Loading the tool carrier | 6-20 |
| 6.7 | Nullpunkt in Z-Achse verschieben mit verrechnetem Werkzeug | Shifting the zero point in the Z axis with a measured tool | 6-21 |
| 6.8 | Folgewerkzeuge (noch nicht verrechnete Werkzeuge) verrechnen | Measuring subsequent tools (tools not yet measured) | 6-23 |
| 6.9 | Schutzzonen setzen oder deaktivieren | Setting or deactivating protection zones | 6-25 |
| 6.10 | Zusätzliche Bearbeitungsmöglichkeiten in der Betriebsart MASCHINE | Additional machining possibilities in operating mode MACHINE | 6-26 |
| 6.10.1 | Dauervorschubdrehen langer Werkstücke ohne Programm | Continuous-feed turning of long workpieces without a program | 6-26 |
| 6.10.2 | Parallelbearbeitung | Parallel machining | 6-27 |
| 6.11 | Option C-Achse | Option C axis | 6-28 |
| 6.11.1 | Die Arbeitsschritte im Überblick | The work steps at a glance | 6-28 |
| 6.11.2 | Drehzahl eingeben und angetriebene Werkzeuge starten | Entering the speed and starting driven tools | 6-29 |
| 6.11.3 | Werkzeuge verrechnen | Measuring tools | 6-30 |
| 6.11.4 | Nullpunkt der C-Achse verschieben | Shifting the C-axis zero point | 6-31 |
| 6.12 | Option Abholeinrichtung | Option part pick-up device | 6-32 |
| 6.12.1 | Abholeinrichtung einrichten | Setting up the pick-up device | 6-33 |
| 6.12.2 | Abholeinrichtung Werkstückausstoßer | Pick-up device, workpiece ejector | 6-34 |
| 6.13 | Option Zwei-Druck-Spanneinrichtung | Option two-pressure clamping device | 6-36 |
| 6.14 | Option Werkzeugmeßeinrichtung | Option tool measuring device | 6-37 |
| 6.15 | Option Stangenbearbeitung | Option bar machining | 6-39 |
| 6.15.1 | Stangenbearbeitung nur mit der Drehmaschine | Bar machining with the lathe only | 6-39 |
| 6.15.2 | Automatische Material Zu- und Abführung | Automatic material feed and removal | 6-41 |
| 6.15.2.1 | Allgemeine Hinweise | General notes | 6-41 |
| 6.15.2.2 | Bearbeitung mit Stangenvorschub | Machining with bar feed | 6-44 |
| 6.15.2.3 | Bearbeitung mit Stangenlademagazin | Machining with bar loading magazine | 6-46 |
| 6.16 | Option Kontrolleuchte | Option indicator lamp | 6-48 |
| 6.17 | Option Überlastkupplung X- und Z-Achse | Option overload clutch X and Z axis | 6-49 |
| 6.18 | Werkstückanlagekontrolle | Workpiece contact (seating) check | 6-50 |

---

## 6.1 General notes (Allgemeine Hinweise)

### 6.1.1 Coordinate systems (Koordinatensysteme)

(Page 6-3.)

For the user, i.e. from the point of view of the control, both working areas are identical in coordinate systems, axis arrangement and tool systems, because folding (mirroring) the working areas onto the opposite side results in the same relationships.

The Z axes of the two sides are directed against each other; relative tool movements with respect to the workpiece coordinate system have the same sign in both working areas. The work spindles rotate against each other.

Figure: the two working areas side by side, separated by a vertical line. Left side: chuck/spindle at left, tool above, +X pointing up, +Z pointing right (towards the centre), zero point `NC` on the spindle axis, rotation arrow `CCW`, viewing direction eye at the left with arrow pointing right. Right side: mirror image, chuck at right, +X up, +Z pointing left (towards the centre), `NC`, `CCW`, eye at the right with arrow pointing left.

### 6.1.2 Aggregate groups (Aggregategruppen)

(Page 6-4.)

In accordance with the machine concept, the control contains 2 aggregate groups, hereinafter called **AG**.

- each AG is autonomous on the operating and programming interface
- one AG can contain several spindles and also several slides
- each AG handles its own working area
- within one AG only one operating mode exists at a time
- in the two AGs different operating modes can exist at the same time, i.e. operating mode splitting
- when operating mode splitting is fully used, it is sensible for safety reasons to separate the working areas by a partition wall.

### 6.1.3 Operating mode splitting (Betriebsartensplitting)

(Page 6-4.)

Operating mode splitting can be switched on and off via a parameter switch:

- in the mode operating mode splitting ON, the effect of the keys cycle start / cycle stop and feed stop depends on the position of the **AG** selector switch.
- in the mode operating mode splitting OFF, the keys cycle start / cycle stop and feed stop act on both **AG** simultaneously.

### 6.1.4 Machining possibilities (Bearbeitungsmöglichkeiten)

(Pages 6-5 and 6-6.)

This machine permits, among others, the machining possibilities shown below. (Each item is illustrated by a small sketch of the left and right spindle with chuck, the turret/tool, and a bracket below showing whether the working areas are separate or connected; a vertical bar between the spindles marks the partition wall.)

**Parallel machining** (Parallelbearbeitung) of
- 2 identical parts (sketch: both spindles cutting at the same time, separate working areas)
- 2 identical parts time-offset with automatic partition wall
- 2 similar parts time-offset with automatic partition wall
(sketches: one side cutting while the other side has its tool retracted, and both sides cutting)

**Complete machining without partition wall** (Komplettbearbeitung ohne Trennwand)
- loading the spindle on the left
- machining the workpiece
- angle-synchronous transfer (`Winkelsynchrone Übergabe`) (sketch: both chucks closed together on the one workpiece in the centre)
- machining of the 2nd side on the right
- removal of the finished part on the right

**Complete machining with partition wall** (Komplettbearbeitung mit Trennwand) (page 6-6)
- loading the spindle on the left
- machining the workpiece
- opening the partition wall
- angle-synchronous transfer
- closing the partition wall
- machining of the 2nd side on the right
- removal of the finished part on the right

## 6.2 Prerequisites (Voraussetzungen)

(Page 6-7.)

- The machine is switched on, see 5.7.1 "Switching on the machine" (`Maschine einschalten`) on page 5-35
- The reference point has been approached, see 5.7.2 "Approaching the reference point" (`Referenzpunkt anfahren`) on page 5-36
- The screen shows the menu `MASCHINE` (MACHINE)
- Workpieces, drawing, tools and chuck jaws are prepared

## 6.3 The work steps at a glance (Die Arbeitsschritte im Überblick)

(Pages 6-7 and 6-8.)

The setting-up procedure is in principle always the same:

- Select the clamping device (chuck/collet chuck)

**Warning:** (warning triangle) The maximum speed of the clamping device must not be exceeded. Set the parameters.

- Select the chuck jaws or collets
- if necessary adjust the proximity switches for clamping stroke monitoring
- Set the parameters for internal or external clamping

**Warning:** (warning triangle) Set the clamping pressure and speed. The maximum jaw weight must not be exceeded.

- Clamp the blank
- Check the concentricity (run-out)
- Select and insert the tools
- Remove tools not needed from the tool carrier
- Shift the zero point in the Z axis with a measured tool
- Measure tools not yet measured (for preset tools enter the setting dimensions in the tool editor, see the programming manual).

**Caution:** (collision symbol) Consider possible collisions.

(Page 6-8.)

**Caution:** (collision symbol) Set protection zones to rule out collisions of the activated tool, e.g. with the chuck jaws.

- Call up or enter the program
- Check the program run with the graphical simulation
- Machine the first blank in single block

**Caution:** (collision symbol) Work with reduced rapid traverse and feed (override) while doing this.

- Measure the finished first workpiece
- Correct the tools and/or the program according to the result

The following pages describe the work steps in detail.

## 6.4 The clamping device in detail (Die Spanneinrichtung im einzelnen)

(Pages 6-9 to 6-11.)

**Warning:** (warning triangle) Before carrying out any work on the clamping device you must be thoroughly familiar with its design and function. On no other part of the machine is the danger so great of doing something wrong and causing harm to persons or machine. This is why some important information about the clamping device has been inserted here.

**Parts of the chuck flying off have high energy and can cause the most serious injuries.**

The clamping device consists of the following parts (left headstock shown, right headstock mirror-inverted):

- **How does the clamping device work?**

A clamping pressure set at the pressure limiting valve (5) passes via a valve (3,4) and hoses to the clamping cylinder (7). This moves the clamping piston (8) in the direction determined by the foot switch or by the program. Via the draw tube (10) the force reaches the chuck / collet chuck (11,12), where it is transmitted to the chuck jaws / collets (14).

In the parameter, see 6.5.4 "Parameter input, clamping device" on page 6-17, it must first be defined which clamping device is fitted. As workpieces can be clamped externally or internally, the clamping direction must also be defined in the parameter.

To guarantee safe clamping, the clamping device is monitored automatically (clamping pressure / clamping stroke).

(Page 6-10.) Figure: section through the headstock: rotary clamping cylinder (7) with clamping piston (8) at the rear (left) end of the spindle, switching ring (6) on the cylinder, two proximity switches (1), (2) on a bracket above it, directional valve with solenoids (3), (4) and pressure reducing valve (5) with ports `P` and `T` below, spindle motor (9) around the spindle, draw tube (10) through the spindle, chuck body (11) with chuck piston (12) at the front, workpiece (13), chuck jaws (14).

| Pos. | Item | Left headstock | Right headstock |
|---|---|---|---|
| 1 | Proximity switch (`Näherungsschalter`) | S 18121 | S 24121 |
| 2 | Proximity switch | S 18111 | S 24111 |
| 3 | Solenoid, directional valve (`Magnetspule, Wegeventil`) | Y 18241 | Y 24241 |
| 4 | Solenoid, directional valve | Y 18251 | Y 24251 |
| 5 | Pressure reducing valve with integrated pressure monitoring (`Druckreduzierventil mit integr. Drucküberwachung`) | F 18041 | F 24041 |
| 6 | Switching ring (`Schaltring`) | | |
| 7 | Clamping cylinder (`Spannzylinder`) | | |
| 8 | Clamping piston (`Spannkolben`) | | |
| 9 | Spindle motor (`Spindelmotor`) | | |
| 10 | Draw tube and connecting piece (`Zug- und Verbindungsstück`) | | |
| 11 | Chuck body (`Spannfutterkörper`) | | |
| 12 | Chuck piston (`Futterkolben`) | | |
| 13 | Workpiece (`Werkstück`) | | |
| 14 | Chuck jaws (`Spannbacken`) | | |

(Page 6-11.)

- **Function of the clamping pressure monitoring** (`Funktion der Spanndrucküberwachung`)

The pressure set at the pressure reducing valve is monitored by a pressure switch. Pressure drops or too low a pressure prevent the work spindle from starting or bring the work spindle to a standstill.

- **Function of the clamping stroke monitoring** (`Funktion der Spannhubüberwachung`)

The correct setting of the clamping stroke monitoring

- prevents clamping pressure from building up when the chuck / collet chuck is at a mechanical end position.
- ensures that the workpiece is clamped safely, taking the blank tolerances into account.

**Warning:** (warning triangle) After changing the chuck / collet chuck the switches (proximity switches) that monitor clamping pressure and clamping stroke must be readjusted, see 6.5.3 "Setting the clamping stroke" on page 6-14. Incorrect setting of the switches prevents safe clamping of the workpieces or starting of the work spindle, or restricts the clamping stroke of the chuck.

## 6.5 Removing and installing clamping devices (Spannmittel aus- und einbauen)

(Page 6-12.)

Removal and installation of the standard clamping device (clamping cylinder / 3-jaw chuck) are described here.

Before starting work, select the aggregate concerned, 1 or 2, with the selector switch, see page 5-10.

Clamping cylinder and draw tube, and chuck and connecting piece, are glued (bonded) together and thus form one assembly unit.

### 6.5.1 Removing the clamping device (Spannmittel ausbauen)

**Note:** (pointing-hand symbol) Only authorised personnel can change this parameter, because it is password-protected.

- Select operating mode `PARAMETER`
- Select menu item `AKT. PARAMETER` (current parameters)
- ✓ The screen shows the dialog box `SPANNFUTTERDATEN` (chuck data).
- Enter a 2 (= external, `Außen`) in the field `SPANNART` (clamping type) and confirm
- Select operating mode `MASCHINE` (MACHINE)
- Open the guard (`Schutzhaube`)
- Move the chuck jaws inwards with the foot switch (clamp the chuck)
- Remove the top jaws (`Aufsatzbacken`)
- Unscrew the chuck fixing screws
- Move the chuck jaws outwards with the foot switch (open the chuck)
- ✓ This releases the chuck from the spindle flange.
- Unscrew the chuck and connecting piece from the clamping cylinder by turning anticlockwise (if necessary with the special tool belonging to the clamping device)
- Pull the chuck and connecting piece out of the spindle towards the tool area and put them down on a suitable support (possibly with a crane)
- Switch off the machine (main switch off)

### 6.5.2 Installing the clamping device (Spannmittel einbauen)

(Page 6-13.)

- Complete the chuck and grease it (with high-performance grease paste, observe the chuck manufacturer's instructions)
- Grease the connecting piece
- Switch on the machine
- Select operating mode `MASCHINE`
- Open the guard
- Open the chuck (move the clamping cylinder forwards with the foot switch)
- if necessary check the chuck flange for run-out (radial 0.005; axial 0.005)
- Clean the spindle and chuck flange carefully
- Push the chuck and connecting piece into the spindle and screw in by turning clockwise until the chuck body bears against the chuck flange or spindle
- Turn back approx. 1.5 turns to the left (observe the fixing screw holes)
- Screw in the chuck fixing screws loosely
- Clamp the chuck (move the clamping cylinder inwards with the foot switch)
- Tighten the chuck fixing screws slightly
- Check the run-out (0.01), align the chuck if necessary
- The chuck fixing screws must be tightened to the torques specified by the respective manufacturer in their documentation, e.g. for the standard chuck KH 200 from Berg with 67 Nm.

### 6.5.3 Setting the clamping stroke (Spannhub einstellen)

(Pages 6-14 and 6-15.)

**Left headstock** (`linker Spindelkasten`)

Figure: rear of the left headstock: clamping cylinder with rotary feed-through and hose at the left, switching ring, and two proximity switches on holders sliding on rods: (1) upper switch, (2) lower switch.

- Move proximity switch 1 (= S 18121) fully to the left, move proximity switch 2 (= S 18111) fully to the right
- Open the chuck completely
- Move proximity switch 2 (= S 18111) to the left until the yellow LED goes out
- then move it approx. 0.5 to 1 mm further to the left
- Tighten the proximity switch holder.
- ✓ Now the clamping travel limit "front" (`"vorn"`) is set.
- Close the chuck
- Move proximity switch 1 (= S 18121) to the right until the yellow LED goes out
- then move it approx. 0.5 to 1 mm further to the right, tighten the proximity switch holder again
- ✓ Now the clamping travel limit "rear" (`"hinten"`) is set. The chuck must be within the usable stroke (`Nutzhub`).

**Right headstock** (`rechter Spindelkasten`) (page 6-15)

Figure: mirror image of the above, clamping cylinder at the right; proximity switches (1) upper and (2) lower.

- Move proximity switch 1 (= S 24121) fully to the right, move proximity switch 2 (= S 24111) fully to the left
- Open the chuck completely
- Move proximity switch 2 (= S 24111) to the right until the yellow LED goes out
- then move it approx. 0.5 to 1 mm further to the right
- Tighten the proximity switch holder.
- ✓ Now the clamping travel limit "front" is set.
- Close the chuck
- Move proximity switch 1 (= S 24121) to the left until the yellow LED goes out
- then move it approx. 0.5 to 1 mm further to the left, tighten the proximity switch holder again
- ✓ Now the clamping travel limit "rear" is set. The chuck must be within the usable stroke.

- Check each stroke with a depth gauge
- If the stroke is not correct, check the screw-in depth of the draw tube and the setting of the proximity switches
- Screw on the top jaws

(Page 6-16.)

**Warning:** (warning triangle) After collisions in which chuck parts were damaged, you must consult the respective chuck manufacturer. Never exceed the limit values stamped on the clamping device (speed, jaw weight).

A detailed description of the chuck can be found in the respective manufacturer's instructions.

### 6.5.4 Parameter input, clamping device (Parametereingabe Spannmittel)

(Page 6-17.)

So that the control "knows" which clamping device is fitted and which clamping type is selected, you must enter or change these parameters.

- Enter / change parameters
- Select operating mode `PARAMETER`
- Select menu item `AKT. PARAMETER`
- Select menu item `SPS-PARAMETER` (PLC parameters)
- Select menu item `SPANNART` (clamping type)
- ✓ The screen shows the dialog box `MASCHINEN PARAMETER: EDITIERUNG` (machine parameters: editing). In this parameter set the combination clamping cylinder - chuck or collet chuck.

Possible entries:

| Field | Meaning | Value |
|---|---|---|
| `SPANNART` (clamping type) | `Innen` (internal) | 1 |
| `SPANNART` | `Außen` (external) | 2 |
| `SPANNART` | `Ausklink` (release/disengage) [uncertain meaning] | 3 |
| `SPANNFUTTERTYP` (chuck type) | manually operated chuck type (`handbetätigter Spannfuttertyp`) | 1 |
| `SPANNFUTTERTYP` | power-operated chuck type (`kraftbetätigter Spannfuttertyp`) | 2 |
| `SPANNZYLINDER` (clamping cylinder) | no clamping cylinder (`kein Spannzylinder`) | 0 |
| `SPANNZYLINDER` | jaws closed (`Backen zu`) | 1 |
| `SPANNZYLINDER` | jaws open (`Backen auf`) | 2 |

- ✓ Now the parameters for clamping device and clamping type are set correctly.
- Close the dialog box

**Caution:** (collision symbol) After changing the parameters for clamping device and clamping type you must switch the machine off and on again so that the new values are taken over.

### 6.5.5 Setting the speed limit (Drehzahlbegrenzung einstellen)

(Page 6-18.)

The clamping device manufacturers specify speed limits for the various clamping devices. The value for the maximum speed is usually stamped on the clamping device. These values must be observed without fail. Of course the limits resulting from the workpiece dimensions must also be taken into account.

**Note:** (pointing-hand symbol) Only authorised personnel can change this parameter, because it is password-protected.

- Select operating mode `PARAMETER`
- Select menu item `AKT. PARAMETER`
- Select menu item `MASCHINE PARAMETER` (machine parameters)
- Select menu item `DREHZAHLEN` (speeds)
- Select menu item `DREHZAHL SPINDEL 1` (speed spindle 1)
- ✓ The screen shows the dialog box `MASCHINEN PARAMETER: EDITIERUNG`.
- Enter the required value in the field `ABSOLUTE MAX. DREHZAHL` (absolute max. speed).
- ✓ The maximum spindle speed is set. It cannot be exceeded even by program commands.

## 6.6 Clamping the blank (Rohteil einspannen)

(Page 6-19.)

When you want to turn a new finished part (start a new batch), you must set the clamping pressure and speed limit, see 6.5.5 "Setting the speed limit" on page 6-18, and load the tool carrier.

### 6.6.1 Checking and if necessary setting the clamping pressure (Spanndruck überprüfen und ggf. einstellen)

Figure: pressure gauge (manometer) mounted in a housing at the machine edge, with the handwheel of the pressure reducing valve (1) protruding to the right.

- Read the current clamping pressure on the pressure gauge at the left or right edge of the machine

**If the clamping pressure has to be changed:**

- The pressure reducing valve is activated by means of a key switch
- Adjust the pressure with the handwheel on the pressure reducing valve (1) until the correct value for the blank is reached:
  - reduce (= turn to the left)
  - increase (= turn to the right)

**Note:** (pointing-hand symbol) The clamping pressure should be between 10 and 60 bar.

The pressure switch adjusts itself automatically along with it.

- Select the required operating mode
- if necessary check the run-out

### 6.6.2 Loading the tool carrier (Werkzeugträger bestücken)

(Page 6-20.)

- remove the tools not needed,
- clamp the tools needed for the workpiece,
- close the empty tool holders with a blanking cap with O-ring.

**Caution:** (collision symbol)
- driven tools must be engaged (latched in)
- **do not change the driven tool that is swivelled in (in working position)!**

## 6.7 Shifting the zero point in the Z axis with a measured tool (Nullpunkt in Z-Achse verschieben mit verrechnetem Werkzeug)

(Pages 6-21 and 6-22.)

**Note:** (pointing-hand symbol) Before starting, select the aggregate concerned, 1 or 2, with the selector switch, see 5-10.

If necessary approach the reference point, see 5.7.2 "Approaching the reference point" on page 5-36.

- Then select operating mode `MASCHINE`
- Select menu item `S`
- Select menu item `DREHZAHL S` (speed S)
- ✓ The screen shows the dialog box `DREHZAHLWERT` (speed value).
- Enter the required speed and confirm
- Start the spindle
- Select menu item `F`
- Select menu item `VORSCHUB MM/U` (feed mm/rev)
- ✓ The screen shows the dialog box `UMDREHUNGSVORSCHUB` (feed per revolution).
- Enter the required feed and confirm
- Select menu item `EINRICHTEN` (set-up)
- Select menu item `VERSCH. NULLPUNKT` (shift zero point)
- ✓ The screen shows the dialog box `WERKZEUGANWAHL` (tool selection).
- Enter the tool number and confirm
- ✓ The selected tool carrier station is swivelled into the working position, and the screen shows the dialog box `NULLPUNKT VERSCHIEBEN` (shift zero point).
- Press the `«Weiter»` (continue/next) key (page 6-22)
- ✓ The screen shows the message `"Nullpunkt in Z anfahren! Nullpunkt verschieben"` (Approach zero point in Z! Shift zero point).
- Scratch (touch) the workpiece
- Select menu item `WERT ÜBERNEHMEN` (accept value)
- ✓ The screen shows the dialog box `MESSWERT EINGEBEN` (enter measured value).
- Confirm the zero offset calculated by the control.
- ✓ The dialog box `NULLPUNKT VERSCHIEBEN` appears again. The input field contains the calculated zero offset.
- Close the dialog box

## 6.8 Measuring subsequent tools (tools not yet measured) (Folgewerkzeuge (noch nicht verrechnete Werkzeuge) verrechnen)

(Pages 6-23 and 6-24.)

If the workpiece is to be machined with different tools, you must also measure the subsequent tools needed for this.

**Prerequisites** (`Voraussetzungen`):
The workpiece zero point on the X axis is set and parameter 7 is defined (measuring method by scratching). For the measuring method with probe see 6.14 "Option tool measuring device" on page 6-37.

You define parameter 7 as follows:

- Select operating mode `PARAMETER`
- Select menu item `AKT. PARAMETER`
- Select menu item `NC-SCHALTER` (NC switches)
- Select menu item `WKZ-MESSART` (tool measuring method)
- ✓ The screen shows the dialog box `WERKZEUGMESSEN` (tool measuring).
- Enter a 0 (= scratching, `Ankratzen`) in the field `ART` (type) and confirm

The measuring method is set to scratching.

**Procedure** (`Vorgehensweise`) (page 6-24):

- Move the slide so that the tool carrier can swivel freely without tools colliding during swivelling
- Select operating mode `MASCHINE`
- Select menu item `EINRICHTEN`
- Select menu item `WKZ-EINRICHTEN` (tool set-up)
- Select menu item `WKZ-MESSEN` (tool measuring)
- ✓ The screen shows the dialog box `WERKZEUGANWAHL`.
- Enter the tool number and confirm
- ✓ The selected tool carrier station is swivelled into the working position and the screen shows the dialog box `MESSEN: T...` (measuring: T...).
- Press the `WEITER` key
- ✓ The screen shows the message `WERKZEUG T... MESSEN. Ankratzen in X!` (Measure tool T... Scratch in X!).
- Scratch the workpiece
- Select menu item `WERT ÜBERNEHMEN`
- ✓ The screen shows the dialog box `MESSWERT EINGEBEN`.
- Enter the workpiece diameter and confirm
- ✓ The screen shows the dialog box `WKZ MESSEN`. The input field X contains the calculated distance between the tool tip and the machine-fixed reference point.
- Close the dialog box
- ✓ The screen shows the dialog box `WERKZEUGANWAHL`.
- Move the slide so that the tool carrier can swivel freely without tools colliding during swivelling
- if necessary measure further subsequent tools in the same way
- Close the dialog box
- ✓ Now the subsequent tools are measured.

## 6.9 Setting or deactivating protection zones (Schutzzonen setzen oder deaktivieren)

(Page 6-25.)

**Note:** (pointing-hand symbol) The components (e.g. automatic working-area partition wall) for which you want to set protection zones must be described (see programming manual).

With the protection zones you can prevent the tool from colliding with e.g. the chuck jaws. They are effective in the operating modes `MASCHINE / HANDSTEUERN` (machine / manual control) and `AUTOMATIK / EINZEL` (automatic / single block).

- Switching on the protection zone monitoring
  - Select operating mode `PARAMETER`.
  - Select menu item `AKT. PARAMETER`.
  - Select menu item `MASCHINE PARAMETER`.
  - Select menu item `ARBEITSRAUM` (working area).
  - Select menu item `SCHUTZZONENPARAMETER` (protection zone parameters).
  - ✓ The screen shows the dialog box `MASCHINEN PARAMETER: EDITIERUNG`.
  - Enter a 1 (= ON, `EIN`) in the field `ÜBERWACHUNG` (monitoring).
- Setting protection zones
  - Select operating mode `MASCHINE`
  - Select menu item `EINRICHTEN`
  - Select menu item `SCHUTZZONEN` (protection zones)
  - ✓ The screen shows the dialog box `SCHUTZZONE SETZEN` (set protection zone).
  - Press the `WEITER` key
  - ✓ The screen shows the message `SCHUTZZONE IN ... ANFAHREN!` (Approach protection zone in ...!).
  - Approach the protection zones in +X, -X, +Z and -Z and each time select menu item `WERT ÜBERNEHMEN`
  - ✓ The corresponding values for the protection zone are taken over into the input fields for +X, -X, +Z and -Z.

**Caution:** (collision symbol) The protection zones are only effective with measured tools.

## For the retrofit

- Control structure: the control holds 2 "aggregate groups" (AG 1 = left side, AG 2 = right side); each is autonomous on the operator/programming interface, has its own working area, may contain several spindles and slides, and runs its own operating mode (page 6-4). This maps naturally onto two separate LinuxCNC/stmbl controllers.
- Operating mode splitting is a parameter switch: ON = cycle start/stop and feed stop act on the AG chosen by the `AG` selector switch; OFF = they act on both AGs at once (page 6-4). A shared panel must replicate this.
- Coordinates: both sides use identical, mirrored systems; the two Z axes point towards each other (+Z towards the machine centre on each side), +X up; tool motions relative to the workpiece have the same sign on both sides; the work spindles rotate against each other (page 6-3).
- Part transfer: "angle-synchronous transfer" (`Winkelsynchrone Übergabe`) of the part from the left to the right spindle, both chucks closing on one part; with the optional automatic partition wall the sequence is: machine side 1, open wall, synchronous transfer, close wall, machine side 2, unload right (pages 6-5, 6-6). No M-codes for the transfer are given in these pages.
- Optional automatic working-area partition wall (`Arbeitsraumtrennwand`) between the sides (pages 6-4 to 6-6); protection zones can be defined for it (page 6-25).
- Chuck hydraulics per headstock (left / right): proximity switches S 18121 / S 24121 (stroke limit "rear", chuck closed) and S 18111 / S 24111 (stroke limit "front", chuck open), directional valve solenoids Y 18241 / Y 24241 and Y 18251 / Y 24251, pressure reducing valve with integrated pressure switch F 18041 / F 24041 (page 6-10, 6-14, 6-15). Switch LEDs go out at the set limit (pages 6-14, 6-15).
- Interlocks the control must implement: low or dropping clamping pressure prevents spindle start or stops the spindle; stroke monitoring prevents building clamping pressure with the chuck at a mechanical end position and verifies safe clamping (page 6-11). Chuck opening/closing by foot switch in MACHINE mode (pages 6-12, 6-13).
- Clamping pressure 10 to 60 bar, set by handwheel on the pressure reducing valve, enabled by a key switch, read on a manometer at the left/right machine edge (page 6-19).
- PLC parameters: `SPANNART` 1 internal / 2 external / 3 `Ausklink`; `SPANNFUTTERTYP` 1 manual / 2 power chuck; `SPANNZYLINDER` 0 none / 1 jaws closed / 2 jaws open; power cycle needed after change (page 6-17). Spindle limit `ABSOLUTE MAX. DREHZAHL` per spindle, cannot be exceeded by the program (page 6-18).
- Standard chuck: KH 200 by Berg, fixing screws 67 Nm; chuck flange run-out 0.005 radial/axial, chuck run-out 0.01 (page 6-13).
- Turret: swivelling tool carrier, tool stations swivel into working position on tool selection; driven tools must be latched, never change the swivelled-in driven tool (pages 6-20, 6-21).
- Tool measuring by scratching (`WKZ-MESSART` `ART` = 0, "parameter 7"); tool X offset is stored as distance from tool tip to the machine-fixed reference point (pages 6-23, 6-24).
- Protection zones (+X, -X, +Z, -Z) active in MACHINE/manual and AUTOMATIC/single block, only with measured tools (page 6-25).
