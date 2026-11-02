# Gildemeister CTX twin operating instructions (English edition), front matter and list of contents, pages 1-10

Source: `24b8aece-CTX_twin_Operating_instructions.pdf`, 10 pages (scanned). Manual's own page numbers: cover, copyright page, release-note insert (2 sides, "REL43-44.DOC, April 95"), pages xi-xvi (list of contents). "Operating instructions, CTX twin, Dok. No.: daaab1g1, February 1995".
Original language: English.

---

## PDF page 1: Cover

- Logo: `GILDEMEISTER` / `Drehmaschinen`; product name `CTX twin`.
- **Operating instructions**
- Instructions for the operation and maintenance of the CNC lathe and its controls
- Photo: turning a large shouldered steel workpiece in a chuck with coolant spray; insert tool in a turret tool holder at the right.
- GILDEMEISTER Drehmaschinen GmbH, Gildemeisterstraße 60, D-33689 Bielefeld
- Telefon: (05205) 74-0; Telefax Service: (05205) 74-3964; Telex: 932222; Telex Service: 932224
- Footer: Operating instructions, Dok. No.: daaab1g1, Issue: February 1995

## PDF page 2: Copyright

(Small handwritten pencil note at upper right, [unreadable].)

**Copyright**

This documentation is intended for the operating, maintenance and installation personnel. Passing on as well as copying of this documentation in full or part is not permitted, nor its use and notification of its contents. Exceptions must be expressively agreed to. Gildemeister reserve all rights in the event of a patent granted or rights for utility model.

Infringement will be liable to damages and can have legal repercussions.

## PDF page 3: Function extensions and changes (release insert)

**Function extensions and changes**

**Release 4.4 as compared with documentation for release 4.3**

### 1. Tool data

#### 1.1 Simple tool definition with temporary tools

The tool definition in NC programs or parameter editor can be limited to a minimum unless these tools are used for cycles.

The ident. number can be left out in the case of a tool definition in the NC program.
Tools defined in this manner are written over with a different NC program or erased with the aid of the tool editor.
Temporary tools are taken over in the automatic operating mode or with the aid of the setting function.

**Important:**

These temporary tools will also be taken over by the next program when they are programmed there, including all data. Correspondingly check the tool data prior to each new use. (underlined in the original)

#### 1.2 Special correction

An additional correction value is available for parting and button tools apart from DX, DZ.
DX and DZ refer to the cutting edge reference point. The additional correction covers the third cutting edge dependent upon the tool position of a further X or Z correction.

### 2. Tool editor

The `TOOL diagnosis` menu point has been deleted.
Entries such as `New cutting edge` or correction/compensation value are performed in the operating modes machine/setting in menu point `Operating life`.

### 3. Extended geometry programming in the machining section

The functions G1, G2 and G3 are extended by the parameters A, B, Q, ? [uncertain: printed "?"] and E. A precise description of the new parameters can be found in the other cycles.

Footer: REL43-44.DOC, April 95

## PDF page 4

(blank page)

## PDF pages 5-9: List of contents (pages xi-xv)

| No. | Title | Page |
|---|---|---|
| **1.** | **Remarks concerning these instructions** | **1-1** |
| 1.1 | Who are these instructions intended for? | 1-3 |
| 1.2 | What do these instructions contain? | 1-3 |
| 1.3 | What is _not_ contained in these instructions? | 1-4 |
| 1.4 | Notes | 1-4 |
| **2.** | **Technical data** | **2-1** |
| 2.1 | Technical data | 2-3 |
| 2.2 | Performance diagram, main drive 202 Nm | 2-4 |
| 2.3 | Performance diagram driven tools | 2-5 |
| 2.4 | Working space page 1 | 2-6 |
| 2.5 | Working space page 2 | 2-7 |
| 2.6 | Feed drives | 2-8 |
| **3.** | **General description of the machine** | **3-1** |
| 3.1 | Concept of the CTX twin | 3-3 |
| 3.1.1 | Modular groups | 3-3 |
| 3.2 | Assembly groups of CTX twin | 3-4 |
| 3.3 | Optional modules | 3-6 |
| 3.4 | Intended use of the machine | 3-8 |
| 3.5 | Warning against misuse | 3-9 |
| 3.6 | Safety devices | 3-10 |
| 3.6.1 | Key switch | 3-10 |
| 3.6.2 | Emergency stop cut-out | 3-10 |
| 3.6.3 | Agreement key | 3-10 |
| 3.6.4 | Isolating protection devices around the machining space | 3-11 |
| 3.6.4.1 | Automatic working space separation wall (operating mode splitting) | 3-11 |
| 3.6.5 | Rotational speed control | 3-12 |
| 3.6.6 | Workpiece clamping | 3-12 |
| 3.6.7 | Hydraulic group | 3-13 |
| 3.6.8 | Pneumatic (when fitted) | 3-13 |
| 3.6.9 | Cut-out devices for setting and maintenance spaces | 3-13 |
| 3.6.10 | Noise protection | 3-13 |
| 3.6.11 | Emissions | 3-13 |
| 3.6.12 | Chip conveyor | 3-14 |
| 3.7 | Lubricants and coolants | 3-15 |
| 3.7.1 | Filling quantities | 3-15 |
| 3.7.2 | Lubricant selection table | 3-16 |
| 3.7.3 | Coolants | 3-17 |
| 3.7.4 | Coolants for integrated spindle motor | 3-17 |
| 3.7.5 | Handling lubricants and coolants | 3-18 |
| **4.** | **Brief instructions for feeders** | **4-1** |
| 4.1 | Brief instructions for feeders | 4-3 |
| 4.1.1 | Starting the machine | 4-3 |
| 4.1.2 | Stopping the machine | 4-4 |
| 4.1.3 | Changing the workpiece | 4-4 |
| **5.** | **Operation of the machine and controls** | **5-1** |
| 5.1 | General instructions | 5-4 |
| 5.1.1 | Modular groups | 5-4 |
| 5.1.2 | Operating mode splitting | 5-4 |
| 5.2 | Operating elements and keyboard | 5-5 |
| 5.2.1 | Operating elements on the right hand side | 5-5 |
| 5.2.2 | Operating panel | 5-6 |
| 5.2.3 | Further operating elements | 5-20 |
| 5.3 | Menu technique and softkeys | 5-21 |
| 5.3.1 | Screen | 5-22 |
| 5.3.2 | General | 5-22 |
| 5.3.3 | Window - screen divisions | 5-24 |
| 5.3.4 | Display range | 5-26 |
| 5.4 | Command input | 5-28 |
| 5.4.1 | Pull-down menus | 5-28 |
| 5.4.2 | Dialog boxes | 5-29 |
| 5.5 | INFO system | 5-32 |
| 5.5.1 | General | 5-32 |
| 5.5.2 | Function and keyboard assignment in INFO system | 5-33 |
| 5.5.3 | INFO system faults | 5-33 |
| 5.6 | Graphical operator support | 5-34 |
| 5.7 | Switching the machine on and off | 5-35 |
| 5.7.1 | Switching on the machine | 5-35 |
| 5.7.2 | Move to reference point | 5-36 |
| 5.7.3 | Acknowledge fault display and messages | 5-37 |
| 5.7.4 | Moving clear of working space positioning switch | 5-38 |
| 5.7.5 | Switching off the machine | 5-39 |
| 5.7.6 | Emergency stop cut-out | 5-39 |
| **6.** | **Setting** | **6-1** |
| 6.1 | General instructions | 6-3 |
| 6.1.1 | Coordinate systems | 6-3 |
| 6.1.2 | Unit groups | 6-4 |
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
| **7.** | **Entering, invoking and processing a program** | **7-1** |
| 7.1 | Entering, invoking and processing programs | 7-3 |
| 7.2 | Invoking a finished program | 7-3 |
| 7.3 | Machining workpiece in individual record | 7-6 |
| 7.4 | Enter tool compensations | 7-7 |
| 7.5 | Automatic workpiece machining | 7-8 |
| 7.6 | Change workpiece in automatic operation | 7-9 |
| 7.7 | Read in program from external data carrier | 7-10 |
| 7.8 | Transmit/back up program to an external data carrier | 7-11 |
| 7.9 | Abort or interrupt program sequence | 7-12 |
| 7.10 | Abort program and starting anew from beginning | 7-12 |
| 7.11 | Expert programs | 7-13 |
| **8.** | **Maintenance** | **8-1** |
| 8.1 | Maintenance | 8-3 |
| 8.2 | Oil levels, check | 8-3 |
| 8.3 | Working area, check and cleaning | 8-4 |
| 8.4 | Maintenance plan according to assembly groups and operating hours | 8-5 |
| **9.** | **Parameter setting, M commands, PLC error information** | **9-1** |
| 9.1 | Parameters | 9-3 |
| 9.1.1 | Parameter setting | 9-3 |
| 9.1.2 | Parameter list | 9-6 |
| 9.1.2.1 | Protection classes / protection criteria | 9-6 |
| 9.1.2.2 | Range | 9-6 |
| 9.2 | Description of the M commands | 9-21 |
| 9.3 | Malfunction messages | 9-32 |
| 9.4 | External events | 9-159 |
| 9.4.1 | Assignment of the event flags | 9-160 |
| **Index** | | **X-1** |

Notes: page xii carries the running header "Safety regulations and precautionary measures" instead of "List of contents" (as printed).

## PDF page 10 (page xvi)

(blank page; header "List of contents", footer only)

---

## For the retrofit

- The control is a Gildemeister menu/softkey control with "release 4.4" software (insert, PDF p. 3); the contents list mentions pull-down menus, dialog boxes, an INFO system and graphical operator support (5.3-5.6, p. xii). No control manufacturer/model is named on these pages.
- The two lathes run as "modular groups"/"unit groups" with "operating mode splitting" and an "automatic working space separation wall" between the two working spaces (3.6.4.1, 5.1.2, 6.1.2, 6.1.3; p. xi-xiii) - this is where the side-to-side interlocks are described.
- "Parallel machining" (6.10.2), C axis option with driven tools (6.11), pick-off device option with workpiece ejector (6.12), two-pressure clamping (6.13), tool measuring unit (6.14), bar feeding/bar loading magazine (6.15), overload coupling on X and Z (6.17) (p. xiii-xiv).
- Main drive 202 Nm (2.2); performance diagram for driven tools (2.3); feed drives data (2.6) (p. xi).
- Safety items the control must handle: key switch, emergency stop cut-out, agreement (enabling) key, rotational speed control, workpiece clamping monitoring, hydraulic group, pneumatics, chip conveyor (3.6.1-3.6.12, p. xi-xii).
- Chapter 9 holds the parameter list (9-6), M command descriptions (9-21), malfunction (PLC) messages (9-32 to 9-158) and external events/event flags (9-159/9-160) (p. xv) - key source for PLC I/O and part transfer M-codes, not in this PDF set.
- Tool data: temporary tools are carried into the next program with all data; special correction for parting/button tools in addition to DX, DZ (insert, PDF p. 3).
