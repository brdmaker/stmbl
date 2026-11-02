# Gildemeister CTX twin operating manual, 7. Entering, calling and running programs (Programme eingeben, aufrufen und abarbeiten), pages 7-1 to 7-30

Source: `7_Programme_eingeben_aufrufen_und_abarbeiten.pdf` (uploaded as `d7392655-7_Programme_eingeben_aufrufen_und_abarbeiten.pdf`), 30 scanned pages = manual pages 7-1 to 7-30. "Betriebsanleitung CTX twin" (operating manual), Dok.-Nr.: daaab1d1, Februar 1995. Pages 7-13 to 7-30 carry the revision note "REV: dab9501 (05.05.1995) zu daaab1d1 Februar 1995".
Original language: German.

Running page header on every page: "7. Programme eingeben, aufrufen und abarbeiten". Footer: "Betriebsanleitung CTX twin / Dok.-Nr.: daaab1d1 Februar 1995 / Seite 7-xx".

Symbols used in the manual: a pointing-hand symbol marks a note (given here as "Note:"); a square symbol with "!" and a hand marks a warning/caution (given here as "Caution:"); a check mark (√) marks the result of a step (given here as "->").

---

## Contents (page 7-1)

| Section | Title (German) | English | Page |
|---|---|---|---|
| 7.1 | Programme eingeben, aufrufen und abarbeiten | Entering, calling and running programs | 7-3 |
| 7.2 | Fertiges Programm aufrufen | Calling a finished program | 7-3 |
| 7.3 | Werkstück im Einzelsatz abarbeiten | Machining a workpiece in single block | 7-6 |
| 7.4 | Werkzeug-Korrekturen eingeben | Entering tool offsets | 7-7 |
| 7.5 | Werkstücke automatisch abarbeiten | Machining workpieces automatically | 7-8 |
| 7.6 | Werkstück wechseln im Automatik-Betrieb | Changing the workpiece in automatic mode | 7-9 |
| 7.7 | Programm von externem Datenträger einlesen | Reading a program from an external data medium | 7-10 |
| 7.8 | Programm auf externen Datenträger senden / sichern | Sending / saving a program to an external data medium | 7-11 |
| 7.9 | Programmablauf ab- oder unterbrechen | Aborting or interrupting program execution | 7-12 |
| 7.10 | Programm abbrechen und von Anfang an neu starten | Aborting a program and restarting from the beginning | 7-12 |
| 7.11 | Expertenprogramme "%XTWI..NCS" | Expert programs "%XTWI..NCS" | 7-13 |
| 7.11.1 | Allgemeines | General | 7-13 |
| 7.11.2 | Expertenprogramme der Version %XTWI... | Expert programs of version %XTWI... | 7-14 |
| 7.11.2.1 | Vorhandene Programme | Available programs | 7-15 |
| 7.11.2.2 | Beschreibung der Programme | Description of the programs | 7-16 |
| | Programm %XTWIFAHR.NCS | Program %XTWIFAHR.NCS | 7-16 |
| | Programm %XTWIFEST.NCS | Program %XTWIFEST.NCS | 7-18 |
| | Programm %XTWI1U2.NCS | Program %XTWI1U2.NCS | 7-20 |
| | Programm %XTWI1U2A.NCS | Program %XTWI1U2A.NCS | 7-23 |
| 7.11.2.3 | Übersicht der Programme mit ihren Variablen | Overview of the programs with their variables | 7-27 |
| 7.11.3 | Expertenprogramme der Version %X.... | Expert programs of version %X.... | 7-28 |
| 7.11.3.1 | Vorhandene Programme | Available programs | 7-29 |
| | Programm %X27.NCS | Program %X27.NCS | 7-30 |

Page 7-2: (blank page)

---

## 7.1 Entering, calling and running programs (page 7-3)

You can enter programs yourself (= programming).

How to program with the Eltropilot control is described in the programming handbook (`Programmier-Handbuch`). For events, see chapter 9.4, "Externe Ereignisse" (external events), on page 9-159.

You can call finished programs from the machine's data memory or from external data media and run them.

The procedure is described below.

Where you have to press a soft key, this operating manual uses the expression **select** (`wählen`), e.g. select operating mode `HANDSTEUERN` (manual control), or select menu item `M-DIREKT` (direct M function).

## 7.2 Calling a finished program (pages 7-3 to 7-5)

You can call finished, stored programs from the internal data memory or, via the interface, also from external data media.

- Press the `BETRIEBSARTEN` (operating modes) key (key symbol: a screen with a row of small boxes at the top).
- Select operating mode `DIN-PLUS`.
- Select menu item `PROG`.
- Select menu item `ORGANISATION`.
- -> The screen shows the program list.
- Save an old program if necessary.
- Select a program with the cursor.
- Press menu item `FKT.` (functions).
- Select menu item `EDITIEREN` (edit).
- -> The screen shows the selected program block by block, one below the other.

Note: Now you should display the workpiece contour graphic to check the program.

Proceed as follows (page 7-4):

- Press the `BETRIEBSARTEN` key.
- Select operating mode `DINPLUS`.
- Select menu item `GRAFIK` (graphics).
- Select menu item `GRAFIK EIN` (graphics on).
- -> The screen shows the current workpiece contour.
- If the workpiece contour graphic cannot be recognised, the screen display may be set too large or too small for the workpiece dimensions. In this case proceed as follows:
- Press the `BETRIEBSARTEN` key.
- Select operating mode `SIMULATION`.
- Select menu item `PROG`.
- -> The screen shows the dialog box `NC-PROGRAMM` (NC program).
- Select the desired program.
- -> The screen shows the workpiece contour. You can display any section enlarged.
- Enter a 0.
- Select menu item `FENSTER` (window).
- -> The screen shows a red rectangle over the workpiece contour.
- Move the red rectangle with the cursor keys over the desired section of the workpiece contour and confirm.

Note (page 7-5): You can enlarge or reduce the window with the `Seite AUF` (page up) or `Seite AB` (page down) key.

The selected section is shown enlarged. You can return to the "normal" view:

- Enter a 0.
- Select menu item `STANDARDGRÖSSE` (standard size).
- -> The screen shows the workpiece contour again in its original size.
- -> If the program is correct, machine the first workpiece in single block as follows.

## 7.3 Machining a workpiece in single block (pages 7-6, 7-7)

You can test a program by running it block by block. After each block you must press the `ZYKLUS-START` (cycle start) key again to run the next block. Otherwise single block corresponds to operating mode `AUTOMATIK` (automatic).

- Press the `BETRIEBSARTEN` key.
- Select operating mode `AUTOMATIK`.
- Select menu item `EINZEL` (single).
- -> The control changes to single-block mode. The screen's status line shows `EINZELSATZBETRIEB` (single-block mode).

Note: If you select menu item `EINZEL` again, the control switches back to operating mode `AUTOMATIK`.

- Select menu item `PROG`.
- Select menu item `PROGRAMMANWAHL` (program selection).
- -> The screen shows the dialog box `AUTOMATIKPROGRAMM` (automatic program).
- Enter the desired program number or select it from the dialog box and confirm.
- -> The program is translated into DIN-PLUS. **Feed override is automatically active (100 %).**
- Turn the feed down to zero with the handwheel.
- Start the program with the `ZYKLUS-START` key.
- -> The program is now run block by block. The tool is swivelled into working position. The spindle turns at the programmed speed. The screen shows the programmed set position (`Soll-Position`) and the current actual position (`Ist-Position`) of the tool as well as the pending tool offset.
- Increase the feed slightly with the handwheel.
- -> The slide moves slowly to the set position.

Caution (page 7-7): Make sure that the tool does not collide with the workpiece.

If necessary you must correct:

- Feed to zero
- Check the NC zero point
- Check the tool setting dimensions in the parameters
- -> The screen shows matching set and actual values for the tool position.
- Increase the feed with the handwheel to approx. 100 %.
- Start the next block.
- -> The next block is executed. Then the slides stop.
- Run the program block by block in the same way.
- -> After the program has been run, the screen shows the first block of the program again.

## 7.4 Entering tool offsets (page 7-7)

You can measure a workpiece and correct tools if necessary as follows:

- Select operating mode `MASCHINE` (machine).
- Select menu item `EINZELSATZBETRIEB` (single-block mode).
- Select menu item `KORR` (correction).
- Select menu item `WKZ-KORREKTUREN` (tool offsets).

Note: In special cases, e.g. with different fits on one workpiece, select additive offsets.

- -> The screen shows the dialog box `WKZ-KORREKTUREN`. The current tool number is displayed.
- Enter the offset values for X and Z and confirm.

## 7.5 Machining workpieces automatically (page 7-8)

When you have made all corrections and the first workpiece has been machined to your satisfaction, you can go back from single-block mode to operating mode `AUTOMATIK`.

- If necessary select operating mode `AUTOMATIK`.
- Select menu item `EINZEL`.
- -> The control is in operating mode `AUTOMATIK` again. The screen's status line shows `AUTOMATIKBETRIEB` (automatic mode).
- Clamp the blank.
- Close the guard (`Schutzhaube`) (watch for the enables on the screen).
- Start the cycle.
- -> The workpiece is now machined automatically.

## 7.6 Changing the workpiece in automatic mode (page 7-9)

- Preconditions:

The previous workpiece must be finished, i.e. the program must have run to the program end.

- Work sequence:
  - Open the guard
  - Grip the finished part and open the chuck
  - Remove the finished part
  - If necessary, clean chips from the chuck jaws
  - Insert the blank carefully into the chuck

Caution: Insert workpieces correctly! You must insert the blank very carefully, otherwise imbalance and in the worst case a "crash" can result. Make sure that the blank lies cleanly against the clamping jaws.

  - Clamp the chuck (press the pedal until the error message in the screen display goes out)
  - Close the guard and wait for the enables
- -> The slide and spindle symbols on the screen are black.
  - Start the cycle
- -> The new workpiece is machined.

## 7.7 Reading a program from an external data medium (page 7-10)

When a ready external data medium is connected, you can read in programs as follows:

- Select operating mode `DIN-PLUS`.

Note: In operating mode DIN-Plus the program always jumps back to the menu last selected. Leave it to the top level with the `Esc` key.

- Select menu item `PROG`.
- Select menu item `ORGANISATION`.
- Select menu item `FKT.`.
- Select menu item `EXTERNE EINGABE` (external input).
- -> The screen shows the message `Transfer ist bereit zum Empfang` (transfer is ready to receive), `Abbruch mit der >-Taste` (cancel with the > key).
- Start the external device.
- -> The program is now transferred automatically into the program memory of the NC control. The correct program number is stored on the external data medium and is transferred and displayed automatically. If a program number already exists in memory, an error message is given. The existing program is not overwritten.

- Calling the program: see 7.1, "Programme eingeben, aufrufen und abarbeiten" on page 7-3.

## 7.8 Sending / saving a program to an external data medium (page 7-11)

You can not only read programs from external data media but also store or save them there. When the external data medium is connected and ready, proceed as follows:

- Select operating mode `DIN-PLUS`.
- Select menu item `PROG`.
- Select menu item `ORGANISATION`.
- -> The screen shows the list of all existing programs.
- Mark the desired programs with the `WEITER` (continue/next) key.

Note: You can mark individually or all together (with menu item `MARK`).

- Select menu item `FKT.`.
- Select menu item `EXTERNE AUSGABE` (external output).
- -> The screen shows the message `Transfer beginnt mit dem Senden` (transfer starts sending), `Abbruch mit der >-Taste` (cancel with the > key).
- -> The program is automatically copied to the external data medium and stored. It also remains in the NC control.

**Wrong program caught?!** (`Falsches Programm erwischt ?!`)
If you have selected a wrong program by mistake, you can cancel the transfer by pressing the `WEITER` key. The program is then not transferred any further.

## 7.9 Aborting or interrupting program execution (page 7-12)

You can abort or interrupt program execution, e.g. if you want to change a tool.

## 7.10 Aborting a program and restarting from the beginning (page 7-12)

You can interrupt the running of any program at any time.

- Press the `VORSCHUB STOP` (feed stop) key (key symbol: wavy line over a circle).
- Press the `ZYKLUS STOP` (cycle stop) key (key symbol: circle in a frame with arrows).
- Move the slides clear in operating mode `MASCHINE / HANDSTEUERN` (machine / manual control).
- Remove the cause of the abort.

If you want to call the program again, start as usual, see 7.5, "Werkstücke automatisch abarbeiten" on page 7-8.

---

## 7.11 Expert programs "%XTWI..NCS" (page 7-13)

### 7.11.1 General

**Expert programs** (`Expertenprogramme`)

For certain sequences (e.g. travel to a fixed stop) there are expert programs. These contain all necessary program steps for the respective task. They can, however, be influenced at the points provided (e.g. speed, direction of rotation). If the possibilities for influencing them are not sufficient, the write-protected programs (master originals, `Uroriginale`) must be copied. The master originals are part of the machine delivery.

**There are 2 kinds of expert programs**

| Generation | Notation | Remarks |
|---|---|---|
| 1st generation | `%XTWI..` | no support from TURN PLUS, specially for the CTX twin |
| 2nd generation | `%X......` | function-dependent / no support from TURN PLUS, specially for the CTX twin |

### 7.11.2 Expert programs of version %XTWI... (page 7-14)

Below, the expert programs %XTWI... are described under the following aspects:

1. Program number and task of the expert program
2. Sequence steps within the expert program
3. State after the expert program has run
4. Precondition for starting the expert program
5. Influencing the expert program

**Notes** (`Hinweise`):

The expert programs are built so that they must be called by both unit groups (`Aggregatgruppen`) simultaneously.
While the expert programs are running, the machine keys act on both unit groups simultaneously.
The global D variables #46 to #50 are reserved for the expert programs. Any other use is not permitted.
The 14 local transfer variables exist separately for each unit group. They have a different meaning depending on the expert program. They are written with the subprogram call. Variables not written pass the value "0".

The variables are:

| No. | Variable | No. | Variable | No. | Variable | No. | Variable |
|---|---|---|---|---|---|---|---|
| 1 | LA | 5 | LE | 9 | J | 13 | R |
| 2 | LB | 6 | LF | 10 | K | 14 | S |
| 3 | LC | 7 | LH | 11 | O | | |
| 4 | LD | 8 | I | 12 | P | | |

Notation in the program call:

```
N... L"XTWI..." V1 LA... LB... ... ... R... S...
```

Notation in the expert program:

```
#__la, #__lb, ...., ...., #__r, #__s
```

The addresses of the %XTWI.. expert programs are not supported by the operating mode TURN PLUS.

With this revision the switch-over to inch programming is taken into account in the expert programs. The fixed values are converted accordingly via variable #778.

#### 7.11.2.1 Available programs (page 7-15)

The following expert programs are available:

| Program | Function |
|---|---|
| `%XTWIFAHR.NCS` | With this program slide 2 moves in -Z direction without fixed stop. |
| `%XTWIFEST.NCS` | With this program slide 2 moves in Z direction to a fixed stop. |
| `%XTWI1U2.NCS` | With this program the workpiece is transferred, rotating, from spindle H to spindle 3 without parting off. |
| `%XTWI1U2A.NCS` | With this program the workpiece is transferred, rotating, from spindle H to spindle 3 with parting off. |

#### 7.11.2.2 Description of the programs (page 7-16)

##### Program %XTWIFAHR.NCS (pages 7-16, 7-17)

With this program slide 2 moves in Z direction without a fixed stop.

**Sequence** (`Ablauf`):

- Check of the variable values
- Move at rapid traverse speed in -Z direction up to just in front of the part
- Move at feed speed onto the workpiece (1000 mm/min).

**State** (`Zustand`):

- Spindle H and spindle 3 face each other
- The clamping device of spindle 3 is open

**Precondition** (`Voraussetzung`):

- The expert program must be called by slide 1 and slide 2.
- XTWIFAHR is called by other expert programs, e.g. XTWI1U2.
- The workpiece must be clamped by spindle H.
- The clamping device of spindle 3 is empty.

**Influencing** (`Beeinflussung`, page 7-17):

| Variable | Meaning | Values |
|---|---|---|
| LA | Z coordinate for prepositioning in rapid traverse | 0 = machine dimension (`Maschinenmaß`) 1; 1-6 = machine dimension 1 to 6; zzz = direct entry of the Z coordinate |
| LB | Incremental feed path for moving onto the workpiece | 0 = machine dimension = pick-up position (`Abholposition`) |

**Example:**
Slide 2 is to advance with spindle 3 to pick up the workpiece in spindle H. The feed path onto the workpiece for clamping is 30 mm. The pick-up position (2 mm in front of the workpiece) was entered in machine dimension 2.

**Program call:**

```
N200 L"XTWIFAHR" V1 LA2 LB30
```

(LA2: preposition in rapid to machine dimension 2; LB30: then 30 mm incremental feed onto the workpiece.)

##### Program %XTWIFEST.NCS (pages 7-18, 7-19)

With this program the slide moves in Z direction to a fixed stop.

**Sequence:**

- Check of the variable values
- Move at rapid traverse speed in -Z direction up to just in front of the part
- Switch on torque reduction for slide 2
- Move at feed speed to the fixed stop (500 mm/min).
- Switch off torque reduction for slide 2

**State:**

- Spindle H and spindle 3 face each other on block
- The clamping device of spindle 3 is open

**Precondition:**

- The expert program must be called by slide 1 and slide 2.
- XTWIFEST is called by other expert programs.
- The workpiece must be clamped by spindle H.
- The clamping device of spindle 3 is empty.
- Parameter values for "travel to fixed stop" (`Fahren auf Festanschlag`) are present.
- Notes:
  - Travel to fixed stop must not be run in single block.
  - The fixed-stop coordinate refers to the spindle nose of spindle 3.

**Influencing** (page 7-19):

| Variable | Meaning | Values |
|---|---|---|
| LA | Z coordinate for prepositioning in rapid traverse | 0 = machine dimension 1; 1-6 = machine dimension 1 to 6; zzz = direct entry of the Z coordinate |
| LB | Maximum feed path for travel to fixed stop | Entry mandatory. Value must be greater than the difference between machine dimension and stop edge (`Anschlagkante`). |
| LC | Minimum feed path for travel to fixed stop | Entry mandatory. The value is the difference between machine dimension and stop edge, minus 1 mm. |

**Example:**
Slide 2 is to travel with spindle 3 to a fixed stop to pick up the workpiece in spindle H. The pick-up position (2 mm in front of the workpiece) was taught into machine dimension 2. The maximum feed path for travel to fixed stop is 2.5 mm and the minimum feed path is 1.5 mm.

**Program call:**

```
N200 L"XTWIFEST" V1 LA2 LB2.5 LC1.5
```

(LA2: preposition to machine dimension 2; LB2.5: maximum fixed-stop feed path 2.5 mm; LC1.5: minimum fixed-stop feed path 1.5 mm.)

##### Program %XTWI1U2.NCS (pages 7-20 to 7-22)

With this program the workpiece is transferred, rotating, from the main spindle to spindle 3 without parting off.

**Sequence:**

- Check of the variable values
- Move slide 1 clear in X direction
- Open chuck of spindle 3
- Activate speed limit for the main spindle
- Activate direction of rotation and speed for the main spindle
- Switch on speed or angle synchronous run (`Drehzahl- oder Winkelsynchronlauf`)
- With or without travel to fixed stop
- Close chuck of spindle 3
- Measure angle error, only with synchronous run
- Open chuck of spindle H, switch off synchronous run
- Slide 2 moves to the machining position
- Stop spindle H

**State:**

- Spindle 3 rotates and spindle H is stopped.
- The speed limit selected in the expert program is in effect.
- Slide 2 is in Z direction at its machining position (machine dimension no. 9).
- Slides 1 and 2 are in X direction at the working-area limit.
- The chuck of spindle H is open.
- The chuck of spindle 3 is closed (with workpiece).

**Precondition:**

- Machine dimensions no. 1 ... no. 6 (approach position in front of the workpiece) and no. 9 (retract position after pick-up) taught.
- The expert program must be called by slide 1 and slide 2.
- The workpiece must be clamped by spindle H.
- The clamping device of spindle 3 is empty.
- The expert programs "XTWIFEST" and "XTWIFAHR" must be present.
- Parameter values for "travel to fixed stop" are present.

**Influencing** (page 7-21; `$1` = unit group/slide 1, `$2` = unit group/slide 2):

| Variable | Meaning | Values |
|---|---|---|
| $1 LA | Value for the speed | 0 = 1000 1/min; 300 - 2000 = possible other entries |
| $1 LB | Direction of rotation of spindle H | 0 = CCW; 3 = CW; 4 = CCW |
| $1 LC | Speed or angle synchronous run | 0 = angle synchronous run; 1 = speed synchronous run |
| $1 LD | Value for the angular offset in angle synchronous run (positive only) | |
| $2 LE | With or without travel to fixed stop | 0 = with travel to fixed stop; 1 = without travel to fixed stop |
| $2 LF | Z coordinate for prepositioning in rapid traverse | 0 = machine dimension 1; 1-6 = machine dimension 1 to 6; zzz = direct entry of the Z coordinate |
| $2 LH | Maximum feed path for travel to fixed stop [see note] | Entry mandatory. The value is the difference between machine dimension and stop edge, minus 1 mm. |
| $2 I | Minimum feed path for travel to fixed stop [see note] | Entry mandatory. Value must be greater than the difference between machine dimension and stop edge. |
| $2 J | Feed path for pick-up without travel to fixed stop | 0 = machine dimension = pick-up position |
| $1$2 K | Selection of coolant for jaw flushing (`Backenspülen`) | 0 = none; 1 = M50, jaw flushing spindle H; 2 = M350, jaw flushing spindle 3; 3 = M50 and M350, jaw flushing spindle H and 3 |

[uncertain] Note: on page 7-21 the labels "Maximaler"/"Minimaler" of LH and I appear swapped against their descriptions. Pages 7-25 and 7-27 (and the example on 7-22, LH1.5 I2.5) give LH = minimum feed path, I = maximum feed path; this is most likely correct.

**Example** (page 7-22):
Spindle 3 is to rotate angle-synchronously with the main spindle with an angular offset of 30 degrees. The speed of the main spindle is 500 rpm clockwise. Slide 2 is to travel with spindle 3 to a fixed stop to pick up the workpiece in spindle H. The pick-up position (2 mm in front of the workpiece) was taught into machine dimension 2. The maximum feed path for travel to fixed stop is 2.5 mm and the minimum feed path 1.5 mm.

**Program call:**

AG1 (unit group 1):

```
N200 L"XTWI1U2" V1 LA500
     LB3 LD30
```

(LA500: main spindle 500 rpm; LB3: clockwise; LD30: 30 degree angular offset; LC not written = 0 = angle synchronous run.)

AG2 (unit group 2):

```
N200 L"XTWI1U2" V1 LF2
     LH1.5 I2.5
```

(LF2: preposition to machine dimension 2; LH1.5 / I2.5: fixed-stop feed paths 1.5 mm and 2.5 mm; LE not written = 0 = with travel to fixed stop.)

##### Program %XTWI1U2A.NCS (pages 7-23 to 7-26)

With this program the workpiece is transferred, rotating, from the main spindle to spindle 3 with parting off.

**Sequence:**

- Check of the variable values
- Move slide 1 clear in X direction
- Open chuck of spindle 3
- Activate speed limit for the main spindle
- Switch on speed or angle synchronous run
- Activate the right cutting edge of the parting tool on slide 1
- Move slide 1 to the start position for parting off
- With or without travel to fixed stop
- Close chuck of spindle 3
- Measure angle error, only with angle synchronous run
- Part off the workpiece with or without reduced feed
- Carry out parting-off check (`Abstechkontrolle`) of slide 2
- Slide 2 moves to the machining position
- Move slide 1 to the start position for parting off
- Activate the left cutting edge of the parting tool on slide 1
- Switch off synchronous run
- Stop spindle H

**State:**

- Spindle H is stopped
- Spindle 3 rotates
- The speed limit selected in the expert program is in effect.
- Slide 2 is in Z direction at its machining position
- Slide 1 is in X direction at the start position for parting off. (The tool change point must be approached in the main program after the transfer.)
- The chuck of spindle H is closed
- The chuck of spindle 3 is closed (with workpiece)

**Precondition** (page 7-24):

- The expert program must be called by slide 1 and slide 2.
- The machine dimension for gripping must be taught (no. 1 ... no. 6).
- Machine dimension no. 9 (machining position slide 2) must be taught.
- Speed or cutting speed and direction of rotation as well as feed type and feed value for parting off are selected on AG1.
- The workpiece must be clamped by spindle H.
- The parting tool of slide 1 must be swivelled in.
- The correct zero offset for parting off must be in effect.
- The clamping device of spindle 3 is empty.
- The expert program "XTWIFEST" must be present.
- The expert program "XTWIFAHR" must be present.
- Parameter values for "travel to fixed stop" are present.
- Parameter values for "parting-off check" (`Abstechkontrolle`) are present.

**Influencing** (page 7-25):

| Variable | Meaning | Values |
|---|---|---|
| $1 LA | Speed limit of spindle H | 0 = 2000 1/min; xx = 300 to 3500 1/min |
| $1 LB | Reduced feed value for the parting-off operation | |
| $1 LC | Selection of spindle synchronisation | 0 = angle synchronous run; 1 = speed synchronous run |
| $1 LD | Value for the angular offset in angle synchronous run (positive only) | |
| $2 LE | With or without travel to fixed stop | 0 = with travel to fixed stop; 1 = without travel to fixed stop |
| $2 LF | Z coordinate for prepositioning in rapid traverse | 0 = machine dimension 1; 1-6 = machine dimension 1 to 6; zzz = direct entry of the Z coordinate |
| $2 LH | Minimum feed path for travel to fixed stop | Entry mandatory. The value is the difference between machine dimension and stop edge, minus 1 mm. |
| $2 I | Maximum feed path for travel to fixed stop | Entry mandatory. Value must be greater than the difference between machine dimension and stop edge. |
| $2 J | Feed path for pick-up without travel to fixed stop | 0 = machine dimension = pick-up position |
| $1$2 K | Selection of coolant for jaw flushing | 0 = none; 1 = M50, jaw flushing spindle H; 2 = M350, jaw flushing spindle 3; 3 = M50 and M350, jaw flushing spindle H and 3 |
| $1 O | Start coordinate in X direction for the parting-off operation | |
| $1 P | Coordinate in X direction (reduced feed) for the parting-off operation | |
| $1 R | End coordinate in X direction for the parting-off operation | |
| $1 S | Start coordinate in Z direction for the parting-off operation | |

**Example** (page 7-26):
Spindle 3 is to rotate angle-synchronously with the main spindle with an angular offset of 30 degrees. The speed of the main spindle is 2 000 rpm clockwise. The speed limit is 2000 1/min. Slide 2 is to travel with spindle 3 to a fixed stop to pick up the workpiece in spindle H. The pick-up position (2 mm in front of the workpiece) was taught into machine dimension 2. The feed path for travel to fixed stop is 2.5 mm and the minimum travel path 1.5 mm. A ring (outside diameter 30 mm, inside diameter 12 mm, length 36 mm) is to be parted off with feed per revolution (0.1 mm/rev). From diameter 18 the reduced feed of 0.05 mm/rev is to be used.

**Program call:**

```
N190 G97 G95 S2000 M03 F0.1 T1
```

(Speed, direction, feed per revolution and parting tool selected on AG1 before the call, as required by the preconditions.)

AG1:

```
N200 L"XTWI1U2A" V1 LB0.05
     LD30 O32 P18 R10 S-36
```

(LB0.05: reduced parting feed 0.05 mm/rev; LD30: 30 degree angular offset; O32: X start; P18: X from which the reduced feed applies; R10: X end; S-36: Z start of parting. LA not written = 0 = speed limit 2000 1/min; LC = 0 = angle synchronous run.)

AG2:

```
N200 L"XTWI1U2A" V1 LF2
     LH1.5 I2.5
```

(LF2: preposition to machine dimension 2; LH1.5: minimum, I2.5: maximum fixed-stop feed path.)

#### 7.11.2.3 Overview of the programs with their variables (page 7-27)

Figure: four framed summary boxes, one per expert program, repeating the "Beeinflussung" lists above. Content as printed:

**Box "Expertenprogramm: %XTWIFAHR.NCS"** - "Mit diesem Programm fährt der Schlitten 2 in Z-Richtung auf Festanschlag" (with this program slide 2 moves in Z direction to a fixed stop) [the box text says "to a fixed stop", unlike page 7-16 "without fixed stop"; probably a copy error].

| Variable | Meaning | Values |
|---|---|---|
| $2 LA | Z coordinate for prepositioning in rapid traverse | 0 = machine dimension 1; 1-6 = machine dimension 1 to 6; zzz = direct entry of the Z coordinate |
| $2 LB | Feed path for moving onto the workpiece | 0 = machine dimension = pick-up position |

**Box "Expertenprogramm: %XTWIFEST.NCS"** - slide 2 moves in Z direction to a fixed stop.

| Variable | Meaning | Values |
|---|---|---|
| $2 LA | Z coordinate for prepositioning in rapid traverse | 0 = 1000 1/min [as printed, apparently a misprint for "machine dimension 1"]; 1-6 = machine dimension 1 to 6; zzz = direct entry of the Z coordinate |
| $2 LB | Maximum feed path for travel to fixed stop | |
| $2 LC | Minimum feed path for travel to fixed stop | |

**Box "Expertenprogramm: %XTWI1U2.NCS"** - workpiece transferred, rotating, from the main spindle to spindle 3 without parting off.

| Variable | Meaning | Values |
|---|---|---|
| $1 LA | Value for the speed | 0 = 1000 1/min; 300 - 2000 = possible other properties (`mögliche andere Eigenschaften`) |
| $1 LB | Direction of rotation of spindle H | 0 = CCW; 3 = CW; 4 = CCW |
| $1 LC | Speed or angle synchronous run | 0 = angle synchronous run; 1 = speed synchronous run |
| $1 LD | Angular offset in angle synchronous run (positive only) | |
| $2 LE | With or without travel to fixed stop | 0 = with; 1 = without travel to fixed stop |
| $2 LF | Z coordinate for prepositioning in rapid traverse | 0 = machine dimension 1; 1-6 = machine dimension 1 to 6; zzz = direct entry of the Z coordinate |
| $2 LH | Minimum feed path for travel to fixed stop | |
| $2 I | Maximum feed path for travel to fixed stop | |
| $2 J | Feed path for pick-up without travel to fixed stop | 0 = machine dimension = pick-up position |
| $1$2 K | Selection of coolant for jaw flushing | 0 = none; 1 = M 50, jaw flushing spindle H; 2 = M 350, jaw flushing spindle 3; 3 = M 50 and M350, spindle H and 3 |

**Box "Expertenprogramm: %XTWI1U2A.NCS"** - workpiece transferred, rotating, from the main spindle to spindle 3 with parting off.

| Variable | Meaning | Values |
|---|---|---|
| $1 LA | Speed limit of spindle H | 0 = 2000 1/min; xx = 300 to 3500 1/min |
| $1 LB | Reduced feed value for parting off | |
| $1 LC | Selection of spindle synchronisation | 0 = angle synchronisation; 1 = speed synchronisation |
| $1 LD | Angular offset in angle synchronous run (positive only) | |
| $2 LE | With or without travel to fixed stop | 0 = with; 1 = without travel to fixed stop |
| $2 LF | Z coordinate for prepositioning in rapid traverse | 0 = machine dimension 1; 1-6 = machine dimension 1 to 6; zzz = direct entry of the Z coordinate |
| $2 LH | Minimum feed path for travel to fixed stop | |
| $2 I | Maximum feed path for travel to fixed stop | |
| $2 J | Feed path for pick-up without travel to fixed stop | 0 = machine dimension = pick-up position |
| $1$2 K | Selection of coolant for jaw flushing | 0 = none; 1 = M 50, jaw flushing spindle H; 2 = M 350, jaw flushing spindle 3; 3 = M 50 M350, spindle H and 3 |
| $1 O | Start coordinate in X direction for parting off | |
| $1 P | Coordinate in X direction (reduced feed) | |
| $1 R | End coordinate in X direction for parting off | |
| $1 S | Start coordinate in Z direction for parting off | |

### 7.11.3 Expert programs of version %X.... (page 7-28)

Below, the expert programs %.... are described under the following aspects:

1. Program number and task of the expert program
2. Sequence steps within the expert program
3. State after the expert program has run
4. Precondition for starting the expert program
5. Influencing the expert program

**Notes:**

The expert programs are built so that they can be called by each unit group individually.
The global D variables #46 to #50 are reserved for the expert programs. Any other use is not permitted.
The 14 local transfer variables exist separately for each unit group. They have a different meaning depending on the expert program. They are written with the subprogram call. Variables not written pass the value "0".

The variables are:

| No. | Variable | No. | Variable | No. | Variable | No. | Variable |
|---|---|---|---|---|---|---|---|
| 1 | LA | 5 | LE | 9 | J | 13 | R |
| 2 | LB | 6 | LF | 10 | K | 14 | S |
| 3 | LC | 7 | LH | 11 | O | | |
| 4 | LD | 8 | I | 12 | P | | |

Notation in the program call:

```
N... L"X......" V1 LA... LB... ... ... R... S...
```

Notation in the expert program:

```
#__la, #__lb, ...., ...., #__r, #__s
```

The addresses of the %X..... expert programs are not supported by the operating mode TURN PLUS.

#### 7.11.3.1 Available programs (page 7-29)

The following expert programs are available:

| Program | Function |
|---|---|
| `%X27.NCS` | With this program the workpiece contact check (`Werkstückanlagekontrolle`) of spindle 0 or 3 is evaluated. |

##### Program %X27.NCS (page 7-30)

With this program the workpiece contact check of spindle 0 or spindle 3 is evaluated. In case of a fault the program stops with M00 and a text message. In special cases a subprogram with the automatic fault reaction can be used instead of the M00.

**Sequence:**

- Check of the variable values
- Query of the event flags (`Ereignismerker`) of the contact check

**State:**

- The clamping device of spindle 0 or spindle 3 is clamped or unclamped

**Precondition:**

- The expert program must be called by slide 1 for spindle 0.
- The expert program must be called by slide 2 for spindle 3.
- The workpiece lies against the contact face, in clamped or unclamped state.
- The contact check is active.

**Influencing:**

| Variable | Meaning | Values |
|---|---|---|
| S | Selection of the spindle at which the contact check is to be evaluated | 0 = spindle 0; 3 = spindle 3 |

**Example:**
Slide 2 has taken over the workpiece from spindle 0 with spindle 3. After reaching the machining position it is checked whether the contact check has responded.

**Program call:**

AG2:

```
N200 L"X27" V1 S3
```

(S3: evaluate the contact check of spindle 3.)

---

## For the retrofit

- Control: "Eltropilot" control (p. 7-3) with operating modes `DIN-PLUS`/`DINPLUS`, `TURN PLUS`, `SIMULATION`, `AUTOMATIK`, `MASCHINE`, `HANDSTEUERN` (pp. 7-3, 7-4, 7-6, 7-7, 7-12, 7-13); program files named `%....NCS`, subprogram call `L"name" V1` with 14 transfer variables LA, LB, LC, LD, LE, LF, LH, I, J, K, O, P, R, S, read as `#__la` ... `#__s` (pp. 7-14, 7-28). Global D variables #46 to #50 reserved for expert programs; #778 = inch/metric conversion (p. 7-14). Programs are addressed per unit group with `$1`/`$2` (pp. 7-21, 7-25).
- Structure of the "twin" as seen by these pages: two unit groups / channels AG1 and AG2 = slide 1 and slide 2 (pp. 7-22, 7-26). Slide 1 works on spindle H (main spindle, also called spindle 0); slide 2 carries spindle 3 and moves in Z to take the part from spindle H (pp. 7-15 to 7-23, 7-30). Each slide has X and Z (pp. 7-20, 7-23, 7-25).
- Part transfer H -> spindle 3 (expert programs, pp. 7-15 to 7-27): must be called in both channels at the same time (a channel sync point); during them the machine keys act on both channels (p. 7-14). Sequence for `%XTWI1U2`: slide 1 X clear, open chuck 3, speed limit + speed/direction on spindle H, speed- or angle-synchronous coupling of spindle 3 to H (angular offset LD, positive only), optional travel to fixed stop, close chuck 3, measure angle error (sync only), open chuck H, sync off, slide 2 to machining position (machine dimension no. 9), stop spindle H (p. 7-20). `%XTWI1U2A` adds parting off by slide 1 (right/left cutting edge selection, reduced feed below diameter P, parting-off check by slide 2) and leaves chuck H closed (p. 7-23).
- Spindle limits from the variables: transfer speed default 1000 1/min, 300-2000 allowed (U2); speed limit default 2000 1/min, 300-3500 allowed (U2A) (pp. 7-21, 7-25). Direction codes 3 = CW, 0/4 = CCW, i.e. M03/M04 (p. 7-21).
- Travel to fixed stop (slide 2 / spindle 3 Z axis): rapid to prepositioning point, switch on torque reduction for slide 2, feed 500 mm/min to the stop with min/max feed path window, torque reduction off; not allowed in single block; stop coordinate refers to spindle 3 nose (p. 7-18). Approach without fixed stop at 1000 mm/min (p. 7-16). The drive needs a torque-limit / fixed-stop-detect mode for this.
- Taught "machine dimensions" (`Maschinenmaß`) 1-6 = approach/pick-up positions, 9 = slide 2 machining/retract position (pp. 7-17, 7-20, 7-24).
- M-codes: M50 = jaw flushing coolant spindle H, M350 = jaw flushing spindle 3 (pp. 7-21, 7-25) (suggests the second channel's M-codes are offset by 300); M03 spindle CW, M00 stop on contact-check fault (pp. 7-26, 7-30). G97/G95 used (p. 7-26).
- Workpiece contact check (`Werkstückanlagekontrolle`) on spindle 0 and spindle 3, evaluated via event flags (`Ereignismerker`) by `%X27` (p. 7-30): an input per spindle for part seated against the face.
- Interlocks visible to the operator: guard (`Schutzhaube`) must be closed and enables shown before cycle start; slide and spindle symbols turn black when enabled (pp. 7-8, 7-9). Chuck clamped by foot pedal; an error message stays until clamping is complete (p. 7-9). Feed override is forced to 100 % at program selection; handwheel used as feed override in single block (p. 7-6). Keys `ZYKLUS-START`, `ZYKLUS STOP`, `VORSCHUB STOP` (pp. 7-6, 7-12).
- Turret: tool is "swivelled into working position" (p. 7-6), parting tool "swivelled in" (p. 7-24) - a swivelling tool holder/turret on slide 1.
- External program I/O over an interface (`EXTERNE EINGABE`/`EXTERNE AUSGABE`) (pp. 7-10, 7-11).
