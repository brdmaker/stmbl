# Gildemeister CTX twin operating instructions (English edition), 7. Entering, invoking and processing a program, pages 7-1, 7-2 and 7-13 to 7-22

Source: `32333259-CTX_twin_Operating_instructions_7.pdf`, 12 PDF pages (scanned), manual pages 7-1, 7-2 and 7-13 to 7-22, "Operating instructions CTX twin, Dok. No.: daaab1g1", chapter 7 "Entering, invoking and processing a program". Pages 7-1 and 7-2 are dated "February 1995"; pages 7-13 to 7-22 are dated "May 1995" (footer "Doc. No.: daaab1g1, May 1995"), i.e. a later replacement insert.
Original language: English.

Running page header on every page: "7. Entering, invoking and processing a program". Footer: "Operating instructions CTX twin, Dok. No.: daaab1g1, February/May 1995, Page 7-xx".

**Pages 7-3 to 7-12 (sections 7.1 to 7.10) are not in this PDF.** Blank pages: 7-2, 7-22.

Handwritten annotations by a previous user (Finnish, blue/black ink) appear on pages 7-14 and 7-18; they are reproduced in square brackets where they occur.

---

## 7. Entering, invoking and processing a program (contents, page 7-1)

| Section | Title | Page |
|---|---|---|
| 7.1 | Entering, Invoking and processing programs | 7-3 |
| 7.2 | Invoking a finished program | 7-3 |
| 7.3 | Machining workpiece in individual record | 7-6 |
| 7.4 | Entering tool compensations | 7-7 |
| 7.5 | Automatic workpiece machining | 7-8 |
| 7.6 | Change workpiece in automatic operation | 7-9 |
| 7.7 | Read in program from external data carrier | 7-10 |
| 7.8 | Transmit/back up program to an external data carrier | 7-11 |
| 7.9 | Abort or interrupt program sequence | 7-12 |
| 7.10 | Abort program and start anew from beginning | 7-12 |
| 7.11 | Expert program | 7-13 |

(Page 7-2: blank page.)

---

## 7.11 Expert programs

(Page 7-13)

(valid for Turn Plus, from software issue 4.3)
Expert programs are available for certain sequences (e.g. moving to a fixed stop). They include all necessary program steps for the individual task, but can also be influenced at the intended positions (e.g. rotational speed, direction of rotation). If the influencing possibilities are insufficient the write protected programs (original) must be copied. The original forms a component part of the machine delivery.

Written `%XTWI...` No support by TURN PLUS
Especially for CTX twin

The following is a description of the expert program `%XTWI...` in accordance with the following points of view:

1. Program number and task of the expert program
2. Sequence steps within the expert program
3. Condition after the elapse of the expert program
4. Prerequisite for starting the expert program
5. Influencing the expert program
6. Programming example

**Note:**
The expert program architecture requires them to be selected by both unit groups simultaneously.
With the elapse of the expert programs the machine keys act on both unit groups simultaneously.
The global D variables #46 to #50 are reserved for expert programs. Any other use is not permitted.
The 14 local transfer variables are available separately for each unit group. They have different meanings in accordance with the individual expert program. They are written for sub-program selection. Non written variables transmit the value "0". The following variables are available:

| No. | Variable | No. | Variable | No. | Variable |
|---|---|---|---|---|---|
| 1 | `LA` | 6 | `LF` | 11 | `O` |
| 2 | `LB` | 7 | `LH` | 12 | `P` |
| 3 | `LC` | 8 | `I` | 13 | `R` |
| 4 | `LD` | 9 | `J` | 14 | `S` |
| 5 | `LE` | 10 | `K` | | |

Method of writing when selecting a program:

```
N... L"XTWI..." V1 LA... LB... ... ... R... S...
```

Method of writing in the expert program:

```
#__la, #__lb, ..., ..., #__r, #__s
```

The addresses of the `%XTWI` expert programs are not supported by the operating mode TURN PLUS.

Switching over to inch programming in the expert programs has been included. The fixed values are correspondingly converted using the variable #778.

### The following expert programs are available:

(Page 7-14)

| Program | Function |
|---|---|
| `%XTWIFAHR.NCS` | The slide 2 moves in the -Z direction **without** a fixed stop using this program |
| `%XTWIFEST.NCS` | The slide 2 moves in the -Z direction **to** the fixed stop using this program. |
| `%XTWI1U2.NCS` | The workpiece is transferred rotating from the spindle H to the spindle 3 **without** parting off using this program. |
| `%XTWI1U2A.NCS` | The workpiece is transferred rotating from the spindle H to the spindle 3 **with** parting off using this program. |

[Handwritten: the `%XTWI1U2.NCS` entry is circled and both `%XTWI1U2.NCS` and `%XTWI1U2A.NCS` are ticked with an "X". Under `%XTWI1U2.NCS`: "LEUKAKAPPALE" (Finnish, approx. "chuck part"); under `%XTWI1U2A.NCS`: "TANKOTYÖ" (Finnish, "bar work").]

---

### Expert program: %XTWIFAHR.NCS

(Page 7-15)

With this program the slide 2 moves in the Z direction **without** fixed stop

**Sequence:**
Checking the variable values
Move at high speed in the Z direction to just before the workpiece
Move at feed speed to the workpiece (1000 mm/min.)

**Condition:**
Spindle H and spindle 3 are opposing each other
The clamping means spindle 3 is opened

**Prerequisite:**
The expert program must be selected from slide 1 and slide 2
XTWIFAHR is selected from other expert programs, e.g. XTWI1U2
The workpiece must be clamped by spindle H
The clamping means spindle 3 is empty

**Influencing**

| Variable | Meaning | Values |
|---|---|---|
| `LA` | Z coordinate for prepositioning at high speed | 0 = machine dimension 1; 1 - 6 = machine dimension 1 to 6; zzz = direct entry of Z coordinates |
| `LB` | Incremental feed path for moving to the workpiece | 0 = Machine dimension = pick-off position |

**Example:**
Slide 2 should advance with spindle 3 to pick-off the workpiece in the spindle H. The feed path to the workpiece for clamping amounts to 30 mm. The pick-off position (2 mm in front of the workpiece) was taught in the machine dimension 2.

**Program selection:**

```
N200 L"XTWIFAHR" V1 LA2 LB30
```

---

### Expert program: %XTWIFEST.NCS

(Page 7-16)

With this program the slide 2 moves in the Z direction to the fixed stop

**Sequence:**
Control of variable values
Move in rapid traverse in -Z direction to just in front of part
Switch on torque reduction for slide 2
Move at feed speed to the fixed stop (500 mm/min).
Switch off torque reduction for slide 2

**Condition:**
Spindle H and spindle 3 stand opposing each other
The clamping means 3 is opened

**Prerequisite:**
The expert program must be selected from slide 1 and 2.
XTWIFEST is selected from another expert program
The workpiece must be clamped by spindle H
The clamping means spindle 3 is empty
Parameter values exist for "Move to fixed stop"

**Note:** Do not move to fixed stop in single block.
The fixed stop coordinates refer to the spindle nose of spindle 3

**Influencing**

| Variable | Meaning | Values |
|---|---|---|
| `LA` | Z coordinates for prepositioning at rapid traverse | 0 = machine dimension 1; 1-6 = machine dimension 1 to 6; zzz = direct input of Z coordinates |
| `LB` | Maximum feed path for moving to fixed stop | Entry is compulsory. Value must be greater than the difference between the machine dimension and stop edge |
| `LC` | Maximum feed path for moving to fixed stop (as printed; from the text and the example this is the minimum feed path) | Entry is compulsory. The value is calculated from the difference between the machine dimension and stop edge, less 1 mm. |

**Example:**
Slide 2 should move to the fixed stop with the spindle 3 to pick-off the workpiece in spindle H. The pick-off position (2 mm in front of the workpiece) is taught in the machine dimension 2. The maximum feed path for moving to the fixed stop is 2.5 mm and the minimum feed path amounts to 1.5 mm.

**Program selection:**

```
N200 L"XTWIFEST" V1 LA2 LB2.5 LC1.5
```

---

### Expert program %XTWI1U2.NCS

(Page 7-17)

With this program the workpiece is transferred rotating from the main spindle to spindle 3 without parting off.

**Sequence:**
Control of the variable values
Move slide 1 free in X direction
Open chuck spindle 3
Activate the rotational speed limit for the main spindle
Activate the rotational direction and rotational speed for the main spindle
Switch rotational speed or angle synchronous run
With or without move to fixed stop
Close chuck spindle 3
Only measure angle fault during angle synchronous run
Open chuck spindle H
Switch off synchronous run
Move slide 2 to machining position
Stop spindle H

**Condition:**
Spindle 3 rotating and spindle H stationary
The rotational speed limitation selected in the expert program is active
Slide 2 is at its machining position in the Z direction
  Machining position (machine dimension No. 9)
Slide 1 and 2 are at the working space limit in the X direction
Spindle H chuck is open
Spindle 3 chuck is closed (with the workpiece)

**Prerequisite:**
Machine dimensions No. 1 ... No. 6 (move forward position in front of the workpiece) and No. 9 (Return postion after pick-off) taught-in
The expert program must be selected by slide 1 and 2
The workpiece must be clamped by spindle H
The clamping means spindle 3 is empty
The expert programs "XTWIFEST" and "XTWIFAHR" must be present
Parameter values for "Move to fixed stop" are present

#### Expert program: %XTWI1U2.NCS (continued)

(Page 7-18)

**Influencing:**

| Variable | Meaning | Values |
|---|---|---|
| `$1 LA` | Value for rotational speed | 0 = 1000 r.p.m; 300 - 2000 = possible other entries |
| `$1 LB` | Rotational direction for spindle H | 0 = CCW; 3 = CW; 4 = CCW |
| `$1 LC` | Rotational speed or angle synchronous run | 0 = Angle synchronous run; 1 = Rotational speed synchronous run |
| `$1 LD` | Value for angular offset with angle synchronous run (only positive) | |
| `$2 LE` | With or without moving to a fixed stop | 0 = With moving to fixed stop; 1 = Without moving to fixed stop |
| `$2 LF` | Z coordinate for prepositioning at rapid traverse | 0 = Machine dimension 1; 1-6 = Machine dimension 1 to 6; zzz = Direct input of Z coordinates |
| `$2 LH` | Minimum feed path for moving to fixed stop | Input compulsory. The value is calculated from the difference between the machine dimension and stop edge, less 1 mm. |
| `$2 I` | Maximum feed path for moving to fixed stop | Input compulsory. Value must be larger than the difference between the machine dimension and the stop edge. |
| `$2 J` | Feed path to pick-up without moving to fixed stop | 0 = Machine dimension = pick-off position |
| `$1$2 K` | Selection of the coolant for jaw flushing | 0 = without; 1 = M50, jaw flushing spindle H; 2 = M350, jaw flushing spindle 3; 3 = M50 and M350, jaw flushing spindle H and 3 |

[Handwritten in the margin: a wavy line pointing at `$1 LD`; next to `$2 LH` "min 4,5 x"; next to `$2 I` "max esim 5,5" (Finnish "esim." = "e.g."), i.e. a previous user's values: minimum 4.5, maximum e.g. 5.5 [uncertain reading of the digits].]

**Example:**
The spindle 3 must turn with the main spindle at a synchronised angle with an angular offset of 30 deg. The rotational speed of the main spindle is 500 r.p.m clockwise. Slide 2 should move with spindle 3 to collect the workpiece and move it in spindle H to the fixed stop. The pick-off position (2 mm in front of the workpiece) was taught in the machine dimension 2. The maximum feed path for moving to a fixed stop is 2.5 mm and the minimum feed path 1.5 mm.

**Program selection:**

```
AG1  N200 L"XTWI1U2" V1 LA500 LB3 LD30
AG2  N200 L"XTWI1U2" V1 LF2 LH1.5 I2.5
```

---

### Expert program: %XTWI1U2A.NCS

(Page 7-19)

Using this program the workpiece is transferred rotating from the main spindle to spindle 3 with parting off.

**Sequence:**
Control the variable values
Move slide 1 free in the X direction
Open chuck spindle 3
Activate the rotational speed limitation for the main spindle
Switch on rotational speed or angle synchronous run
Activate the right hand cutting tool for parting off on slide 1
Move slide 1 to the start position for parting off
With or without moving to fixed stop
Close chuck spindle 3
Measure angle error only in angle synchronous run
Part off workpiece with or without reduced feed
Undertake parting control from slide 2
Slide 2 moves to the machining position
Move slide 1 to start position for parting
Activate left hand blade of parting tool on slide 1
Switch off synchronous running
Stop spindle H

**Condition:**
Spindle H stationary
Spindle 3 rotating
The rotational speed limit selected in expert program is effective
The slide 2 is in its machining position in the Z direction
Slide 1 is at the parting start point in the X direction
  (The tool change point must be moved to after transfer in the main program)
The chuck for spindle H is closed
The chuck for spindle 3 is closed (with workpiece)

**Prerequisite:**
The expert program must be selected from slide 1 and 2
The machine dimension for pick-off must be taught-in (No.1 .. No.6)
The machine dimension No.9 (machining position slide 2) must be taught-in
The rotational speed and cutting speed together with the turning direction as well as the feed value must have been selected for parting in AG1
The workpiece must be clamped by spindle H
The parting tool must be swung in from slide 1
The correct zero offset for parting must be effective
The clamping means spindle 3 is empty
The expert program "XTWIFEST" must be present
The expert program "XTWIFAHR" must be present
Parameter values are present for "Move to fixed stop"
Parameter values for "Part-off control" are present

#### Expert program: %XTWI1U2A.NCS (continued)

(Page 7-20)

**Influencing:**

| Variable | Meaning | Values |
|---|---|---|
| `$1 LA` | Rotational speed limitation for spindle H | 0 = 2000 r.p.m; xx = 300 - 3500 r.p.m |
| `$1 LB` | Reduced feed value for parting cycle | |
| `$1 LC` | Spindle synchronisation selection | 0 = Angle synchronous run; 1 = Rotational speed synchronous run |
| `$1 LD` | Value for angular offset with angle synchronous run (only positive) | |
| `$2 LE` | With or without moving to a fixed stop | 0 = With moving to fixed stop; 1 = Without moving to fixed stop |
| `$2 LF` | Z coordinate for prepositioning at rapid traverse | 0 = Machine dimension 1; 1-6 = Machine dimension 1 to 6; zzz = Direct input of Z coordinates |
| `$2 LH` | Minimum fed path for moving to fixed stop | Input compulsory. Value is calculated from the difference between the machine dimension and stop edge, less 1 mm. |
| `$2 I` | Maximum feed path for moving to fixed stop | Input compulsory. Value must be larger than the difference between the machine dimension and the stop edge. |
| `$2 J` | Feed path to pick-up without moving to fixed stop | 0 = Machine dimension = pick-off position |
| `$1$2 K` | Selection of the coolant for jaw flushing | 0 = without; 1 = M50, jaw flushing spindle H; 2 = M350, jaw flushing spindle 3; 3 = M50 and M350, jaw flushing spindle H and 3 |
| `$1 O` | Start coordinates in X direction for parting cycle | |
| `$1 P` | Coordinate in X direction (reduced feed) for parting cycle | |
| `$1 R` | End coordinate in X direction for parting cycle | |
| `$1 S` | Start coordinate in Z direction for parting cycle | |

**Example:**
The spindle 3 should turn angularly synchronised with the main spindle at an angular offset of 30 deg. The main spindle rotational speed is clockwise 2000 r.p.m. The rotational speed limit is 2000 r.p.m. The slide 2 should run to the fixed stop with spindle 3 to pick-off the workpiece in spindle H. The pick-off position (2 mm in front of the workpiece) is taught-in in the machine dimension 2. The Feed path for moving to the fixed stop is 2.5 mm and the minimum traverse path 1.5 mm. A ring (external diameter 30 mm, internal diameter 12 mm, length 36 mm) is to be parted off at a rotational feed (0.1 mm/rev.). A reduced feed rate of 0.05 mm/rev. is used as from a diameter of 18 mm.

**Program selection:**

```
N190 G97 G95 S2000 M03 F0.1 T1
AG1 N200 L"XTWI1U2A" V1 LB0.05 LD30 O32 P18 R10 S-36
AG2 N200 L"XTWI1U2A" V1 LF2 LH1.5 I2.5
```

---

### Summary cards (page 7-21)

Page 7-21 repeats the influencing variables of the four expert programs as four boxed quick-reference cards. Differences from the full descriptions are noted.

**Expert program: %XTWIFAHR.NCS**
With this program slide 2 moves in the Z direction **without** fixed stop

| Variable | Meaning | Values |
|---|---|---|
| `$2 LA` | Z coordinate for prepositioning at high speed | 0 = machine dimension 1; 1 - 6 = machine dimension 1 to 6; zzz = Direct entry of Z coordinate |
| `$2 LB` | Incremental feed path for moving to the workpiece | 0 = Machine dimension = pick-off position |

**Expert program: %XTWIFEST.NCS**
With this program slide 2 moves in the Z direction to the fixed stop

| Variable | Meaning | Values |
|---|---|---|
| `$2 LA` | Z coordinates for prepositioning at rapid traverse | 0 = machine dimension 1; 1-6 = machine dimension 1 to 6; zzz = direct entry of Z coordinates |
| `$2 LB` | Maximum feed path for moving to fixed stop | |
| `$2 LC` | Maximum feed path for moving to fixed stop (as printed) | |

**Expert program: %XTWI1U2.NCS**
With this program the spindle is transferred rotating from spindle H to spindle 3 without parting

| Variable | Meaning | Values |
|---|---|---|
| `$1 LA` | Value for rotational speed | 0 = 1000 r.p.m; 300 - 2000 = possible other entries |
| `$1 LB` | Rotational direction of spindle H | 0 = CCW; 3 = CW; 4 = CCW |
| `$1 LC` | Rotational speed or angle synchronous run | 0 = Angle synchronous run; 1 = Rotational speed synchronous run |
| `$1 LD` | Value for angular offset with angle synchronous run (only positive) | |
| `$2 LE` | With or without moving to a fixed stop | 0 = With moving to fixed stop; 1 = Without moving to fixed stop |
| `$2 LF` | Z coordinate for prepositioning at rapid traverse | 0 = Machine dimension 1; 1-6 = Machine dimension 1 to 6; zzz = Direct input of Z coordinates |
| `$2 LH` | Minimum fed path for moving to fixed stop. | |
| `$2 I` | Maximum feed path for moving to fixed stop | |
| `$2 J` | Feed path to pick-up without moving to fixed stop | 0 = Machine dimension = pick-off position |
| `$1$2 K` | Selection of the coolant for jaw flushing | 0 = without; 1 = M50, jaw flushing spindle H; 2 = M350, jaw flushing spindle 3; 3 = M50 and M350, jaw flushing spindle H and 3 |

**Expert program: %XTWI1U2A.NCS**
Using this program the workpiece is transferred rotating from the main spindle to spindle 3 with parting off.

| Variable | Meaning | Values |
|---|---|---|
| `$1 LA` | Rotational speed limitation for spindle H | 0 = 2000 r.p.m; xx = 300 - 3500 r.p.m |
| `$1 LB` | Reduced feed value for parting cycle | |
| `$1 LC` | Spindle synchronisation selection | 0 = Angle synchronous run; 1 = Rotational speed synchronous run |
| `$1 LD` | Value for angular offset with angle synchronous run (only positive) | |
| `$2 LE` | With or without moving to a fixed stop | 0 = With moving to fixed stop; 1 = Without moving to fixed stop |
| `$2 LF` | Z coordinate for prepositioning at rapid traverse | 0 = Machine dimension 1; 1-6 = Machine dimension 1 to 6; zzz = Direct input of Z coordinates |
| `$2 LH` | Minimum fed path for moving to fixed stop | |
| `$2 I` | Maximum feed path for moving to fixed stop | |
| `$2 J` | Feed path to pick-up without moving to fixed stop | 0 = Machine dimension = pick-off position |
| `$1$2 K` | Selection of the coolant for jaw flushing | 0 = without; 1 = M50, jaw flushing spindle H; 2 = M350, jaw flushing spindle 3; 3 = M50 and M350, jaw flushing spindle H and 3 |
| `$1 O` | Start coordinates in X direction for parting cycle | |
| `$1 P` | Coordinate in X direction (reduced feed) | |
| `$1 R` | End coordinate in X direction for parting cycle | |
| `$1 S` | Start coordinate in Z direction for parting cycle | |

(Page 7-22: blank page.)

---

## For the retrofit

- Control: "Turn Plus, from software issue 4.3", programs named `%...NCS`, subprogram call syntax `L"name" V1`, `#` variables, channel prefixes `$1`/`$2` and unit groups AG1/AG2. The control model is not named on these pages; TURN PLUS is an operating mode of the control (p. 7-13). (Inference, not stated here: this syntax resembles the Gildemeister EPL / Heidenhain CNC PILOT family; confirm from chapter 2 or the control nameplate.)
- Naming: slide 1 (left, AG1, `$1`) and slide 2 (right, AG2, `$2`); spindle H = main (left) spindle on AG1, spindle 3 = counter (right) spindle carried by slide 2 which moves in -Z towards spindle H (pp. 7-14 to 7-17).
- Part transfer is done by expert programs that must be called in both channels simultaneously; while running, machine keys act on both unit groups (p. 7-13). Global D variables #46 to #50 reserved; 14 local transfer variables LA..S per channel (p. 7-13).
- Transfer sequence without parting (`%XTWI1U2`): slide 1 X clear, open chuck 3, main spindle speed limit, start spindle H (default 1000 rpm, 300-2000), speed- or angle-synchronous run with angular offset, approach with or without fixed stop, close chuck 3, measure angle fault (angle sync only), open chuck H, sync off, slide 2 to machining position (machine dimension No. 9), stop spindle H (p. 7-17). With parting (`%XTWI1U2A`): same plus parting with slide 1 tool (right then left blade), "parting control" from slide 2, speed limit default 2000 rpm (300-3500) (pp. 7-19, 7-20).
- Approach of slide 2: rapid to taught machine dimension 1-6 (pick-off position, 2 mm before the part), then feed 1000 mm/min without stop (`XTWIFAHR`) or 500 mm/min with torque reduction on slide 2 to a fixed stop (`XTWIFEST`), min/max feed path window; never in single block; fixed stop coordinates refer to spindle 3 nose (pp. 7-15, 7-16).
- Needs in LinuxCNC/stmbl: spindle-to-spindle speed and angle (position) synchronisation with offset, angle-error check, Z2 torque limit + stall/fixed-stop detection, chuck open/close for both spindles with confirm.
- M-codes: M50 = jaw flushing spindle H, M350 = jaw flushing spindle 3 (pp. 7-18, 7-20); M03 used in example (p. 7-20).
- Previous owner's handwritten notes mark `XTWI1U2`/`XTWI1U2A` as used, with min 4.5 / max approx. 5.5 fixed-stop feed path [uncertain] (pp. 7-14, 7-18).
