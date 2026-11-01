# Gildemeister CTX twin operating instructions (English edition), 9. Parameter setting, M commands, PLC fault messages, pages 9-1 to 9-32

Source: `43b2e6c5-CTX_twin_Operating_instructions_9a.pdf`, 32 scanned pages (PDF pages 1-32 = manual pages 9-1 to 9-32), "Operating instructions CTX", chapter 9 "Parameter setting, M commands, PLC fault messages", part a of 4. The footer carries "Doc. No.:" with no number filled in.

These pages contain the chapter contents, 9.1.1 Parameter setting (operator parameters and their menu paths) and the parameter list 9.1.2 from Parameter 020 to Parameter 047 (K numbers 5000-5002 and 50010-50239). The parameter list continues in part b (from page 9-33); 9.2 Description of the M commands (page 9-42) and 9.3 Fault messages (page 9-54) are in the later parts. Blank page: 9-2.

Running page header on every page: "9. Parameter setting, M commands, PLC fault messages". Footer: "Operating instructions CTX / Doc. No.: / Page 9-xx".

---

## 9. Parameter setting, M commands, PLC fault messages

(Page 9-1, chapter contents.)

| Section | Page |
|---|---|
| 9.1 Parameter | 9-3 |
| 9.1.1 Parameter setting | 9-3 |
| 9.1.2 Parameter list | 9-6 |
| 9.1.2.1 Protection classes / protection criteria | 9-6 |
| 9.1.2.2 Area | 9-6 |
| 9.2 Description of the M commands | 9-42 |
| 9.3 Fault messages | 9-54 |

Page 9-2: (blank page)

---

## 9.1 Parameters

### 9.1.1 Parameter setting

(Page 9-3.)

This lathe has preset values (= parameters) for certain functions e.g. for the NC zero point or the graphic display.

Several of these parameters must be set during your daily work. Some must remain unchanged under any circumstances.

The parameters which are important for your daily work will be explained in the following in an ascending order.

A complete list of parameters is in the programming handbook.

Menu points are provided for each parameter, which must be selected one after the other in order to set the individual value.

**NC zero point**
This parameter is intended to offset the NC zero point (= workpiece zero point in relation to the machine zero point). The X value then always remains the same, and the Z value changes dependent upon the workpiece length.

Operating mode `PARAMETER` menu point `CURRENT PARAMETER`, menu point `SETTING (MENU)` menu point `NC ZERO POINT`.

**Manual feed**
Use this parameter to set the feed speed (mm/min) for manual operation.

Operating mode `PARAMETER` menu point `CURRENT PARAMETER`, menu point `MACHINE PARAMETER`, menu point `FEED`.

**High speed manual**
Use this parameter to set the high speed (mm/min) for manual operation.

Operating mode `PARAMETER` menu point `ACT. PARAMETER`, menu point `MACHINE PARAMETER`, menu point `FEED`.

**High speed automatic**
Use this parameter to set the high speed (mm/min) for automatic operation (= automatic processing of a program). The speed can be set individually for each axis.

(Page 9-4.)

**Chucking mode** The chucking mode is set in this parameter.

Operating mode `PARAMETER` menu point `CURRENT PARAMETER`, menu point `PLC`, menu point `CHUCKING MODE`.

**Display language**
Not yet implemented

**Operation**
Operating mode `PARAMETER` menu point `CURRENT PARAMETER`, menu point `PLC PARAMETER`, menu point `HYDRAULIC, LUBRICATION` (additional messages via hydraulic and lubrication)
or
`HANDLING` menu point (for fitting the loading flap).

**Operation**
Operating mode `PARAMETER` menu point `CURRENT PARAMETER`, menu point `PLC PARAMETER`, menu point `CHUCKING MODE`.
or
menu point `TAILSTOCK`
or
menu point `HANDLING`
or
menu point `AUTO. HOOD`
or
menu point `MEASURING SYSTEM`

**High speed C axis**
Set the maximum angle speed in deg./min. for the spindle positioning at high speed of C axis 1 in this parameter.

Operating mode `PARAMETER` menu point `CURRENT PARAMETER`, menu point `MACHINE PARAMETER`, menu point `FEED`.

**Zero point C axis**
Set the zero offset for the C axis in this parameter.

Operating mode `PARAMETER` menu point `CURRENT PARAMETER`, menu point `MACHINE PARAMETER`, menu point `SETTING (menu)`, menu point `ZERO OFFSET C AXIS`.

**Work spindle rotational speed limitation**
In this parameter you set the maximum work spindle rotational speed.

Operating mode `PARAMETER` menu point `CURRENT PARAMETER`, menu point `MACHINE PARAMETER`, menu point `ROTATIONAL SPEEDS`, menu point `SPEED SPINDLE 1`.

(Page 9-5.)

**Measurement method**
In this parameter enter whether the dimensions of a tool will be set manually, with an optical measuring device or with a measuring probe.

Operating mode `PARAMETER` menu point `CURRENT PARAMETER`, menu point `MACHINE PARAMETER`, menu point `NC-SWITCH`, menu point `TOOL MEASURING MODE`.

### 9.1.2 Parameter list

(Page 9-6.)

Those parameters are described in this parameter list which refer to the equipment of a special machine.

All other parameters are to be found in the ELTROPILOT programming instructions, as these correlate with other machine types and have the same meaning.

#### 9.1.2.1 Protection class / protection criteria

- A - The parameter is not protected. Each operator can change the parameter.
- B - Operators qualified for the user class NC programmer may change the parameters.
- C - The parameter is protected by a password and must not be changed when the automatic machine operating mode is active.
- D - The parameter is only accessible to the Gilldemeister service personnel. The user can look at the parameter, however, has no means to change it.

#### 9.1.2.2 Range

Validity range of the individual parameter settings.
UNS32 = 0-4294967295

How the parameter tables are given here: one row per parameter, columns as printed (protection class, parameter text, value range, K No., and an "x" in the columns CTX 200, CTX 400/500/600, CTX twin when the parameter applies to that machine type). The value meanings printed under a parameter are given in the parameter cell. "Dialog box - n -" lines are given as separate rows; on several pages the dialog box divider lines and numbers ("- 2 -", "- 3 -", "- 4 -") are handwritten additions in the scan.

##### Parameter 020: Control parameter

| Prot. class | Parameter | Value range | K No. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|
| | **Dialog box - 1 -** | | | | | |
| D | Machine type number | UNS32 | 5000 | | | |
| D | Series version | UNS32 | 5001 | | | |
| D | Control type number | UNS32 | 5002 | | | |

##### Parameter 022: Spindle H, chuck H, gearing H

(Pages 9-7 to 9-8.)

| Prot. class | Parameter | Value range | K No. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|
| | **Dialog box - 1 -** | | | | | |
| A/Act | Chucking mode. Value = 1 internal chucking; Value = 2 external chucking; Value = 3 disengagement stroke | 1-3 | 50010 | x | x | x |
| A/Act | Chuck type. Value = 1 Manually operated; Value = 2 Power operated | 1-2 | 50011 | x | x | x |
| A/Act | Clamping cylinder. Value = 0 no clamping cylinder; Value = 1 return = close jaws; Value = 2 return = open jaws | 0-2 | 50012 | x | x | x |
| A/Act | Chuck: after clamping time [20 msec] | 0-100 | 50013 | x | | x |
| | **Dialog box - 2 -** (handwritten) | | | | | |
| C | Clamper type. Value = 0 hydraulic, stroke limitation safety prox. switches, centre position valves (standard); Value = 1 hydraulic, without stroke limitation, engagement valves | 0-10 | 50014 | | x | |
| C | Swivel chuck type. Value = 0 no swivel chuck fitted; Value = 1 AXN swivel chuck type from Autoblock with 4 positions | 0-10 | 50015 | | x | |
| C | Maximum chuck swivel speed. Value = maximum speed permitted for swivelling the swivel chuck [r.p.m] | 0-65535 | 50016 | | x | |
| | **Dialog box - 3 -** (number handwritten) | | | | | |
| D | Spindle fitted | 0-1 | 50017 | | | x |
| D | Spindle clamping fitted | 0-1 | 50018 | x | x | x |
| D | Clamp release time [1/50 sec.] | 0-255 | 50019 | x | x | x |
| D | Clamp clamping time [1/50 sec.] | 0-255 | 50020 | x | x | x |
| D | Type C axis. Value = 0 no C axis fitted; Value = 1 C axis with contact pressure roller; Value = 2 C axis without contact pressure roller | 0-10 | 50021 | x | x | x |
| D | Spindle: delay time 1 [100 msec] (HW spindle release --> controller release chopper ON) | 0-255 | 50022 | | | x |
| | **Dialog box - 4 -** (number handwritten) | | | | | |
| D | Gearing type. Value = 0 no gearing fitted; Value = 1 ZF: 2 stage electrically actuated | 0-10 | 50023 | | x | |

##### Parameter 023: Spindle 1, chuck 1, gearing 1

(Pages 9-8 to 9-9.)

| Prot. class | Parameter | Value range | K No. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|
| | **Dialog box - 1 -** | | | | | |
| A/Act | Chucking mode. Value = 1 internal chucking; Value = 2 external chucking; Value = 3 disengagement stroke | 1-3 | 50024 | | | |
| A/Act | Chuck type. Value = 1 Manually operated; Value = 2 Power operated | 1-2 | 50025 | | | |
| A/Act | Clamping cylinder. Value = 0 no clamping cylinder; Value = 1 return = close jaws; Value = 2 return = open jaws | 0-2 | 50026 | | | |
| A/Act | Chuck: after clamping time [20 msec] | 0-100 | 50027 | | | |
| | **Dialog box - 2 -** (handwritten) | | | | | |
| C | Clamper type. Value = 0 hydraulic, stroke limitation safety prox. switches, centre position valves (standard); Value = 1 hydraulic, without stroke limitation, engagement valves | 0-10 | 50028 | | | |
| C | Swivel chuck type. Value = 0 no swivel chuck fitted; Value = 1 AXN swivel chuck type from Autoblock with 4 positions | 0-10 | 50029 | | | |
| C | Maximum chuck swivel speed. Value = maximum speed permitted for swivelling the swivel chuck [r.p.m] | 0-65535 | 50030 | | | |
| | **Dialog box - 3 -** (number handwritten) | | | | | |
| D | Spindle fitted | 0-1 | 50031 | x | x | x |
| D | Spindle clamping fitted | 0-1 | 50032 | | | |
| D | Clamp release time [1/50 sec.] | 0-255 | 50033 | | | |
| D | Clamp clamping time [1/50 sec.] | 0-255 | 50034 | | | |
| D | Type C axis. Value = 0 no C axis fitted; Value = 1 C axis with contact pressure roller; Value = 2 C axis without contact pressure roller | 0-10 | 50035 | | | |
| D | Spindle: delay time 1 [100 msec] (HW spindle release --> controller release chopper ON) | 0-255 | 50036 | | | x |
| | **Dialog box - 4 -** (number handwritten) | | | | | |
| D | Gearing type. Value = 0 no gearing fitted; Value = 1 ZF: 2 stage electrically actuated | 0-10 | 50037 | | | |

##### Parameter 024: Spindle 2, chuck 2, gearing 2

(Pages 9-10 to 9-11.)

| Prot. class | Parameter | Value range | K No. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|
| | **Dialog box - 1 -** | | | | | |
| A/Act | Chucking mode. Value = 1 internal chucking; Value = 2 external chucking; Value = 3 disengagement stroke | 1-3 | 50038 | | | |
| A/Act | Chuck type. Value = 1 Manually operated; Value = 2 Power operated | 1-2 | 50039 | | | |
| A/Act | Clamping cylinder. Value = 0 no clamping cylinder; Value = 1 return = close jaws; Value = 2 return = open jaws | 0-2 | 50040 | | | |
| A/Act | Chuck: after clamping time [20 msec] | 0-100 | 50041 | | | |
| | **Dialog box - 2 -** (handwritten) | | | | | |
| C | Clamper type. Value = 0 hydraulic, stroke limitation safety prox. switches, centre position valves (standard); Value = 1 hydraulic, without stroke limitation, engagement valves | 0-10 | 50042 | | | |
| C | Swivel chuck type. Value = 0 no swivel chuck fitted; Value = 1 AXNB swivel chuck type from Autoblock with 4 positions | 0-10 | 50043 | | | |
| C | Maximum chuck swivel speed. Value = maximum speed permitted for swivelling the swivel chuck [r.p.m] | 0-65535 | 50044 | | | |
| | **Dialog box - 3 -** (number handwritten) | | | | | |
| D | Spindle fitted | 0-1 | 50045 | | | x [uncertain column] |
| D | Spindle clamping fitted | 0-1 | 50046 | | | |
| D | Clamp release time [1/50 sec.] | 0-255 | 50047 | | | |
| D | Clamp clamping time [1/50 sec.] | 0-255 | 50048 | | | |

(Parameter 024 continued, page 9-11.)

| Prot. class | Parameter | Value range | K No. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|
| D | Type C axis. Value = 0 no C axis fitted; Value = 1 C axis with contact pressure roller; Value = 2 C axis without contact pressure roller | 0-10 | 50049 | | | |
| D | Spindle: delay time 1 [100 msec] (HW spindle release --> controller release chopper ON) | 0-255 | 50050 | | | x |
| | **Dialog box - 3 -** (printed so; in the other spindle tables this is "- 4 -") | | | | | |
| D | Gearing type. Value = 0 no gearing fitted; Value = 1 ZF: 2 stage electrically actuated | 0-10 | 50051 | | | |

##### Parameter 025: Spindle 3, chuck 3, gearing 3

(Pages 9-11 to 9-12.)

| Prot. class | Parameter | Value range | K No. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|
| | **Dialog box - 1 -** | | | | | |
| A/Act | Chucking mode. Value = 1 internal chucking; Value = 2 external chucking; Value = 3 disengagement stroke | 1-3 | 50052 | x | | x |
| A/Act | Chuck type. Value = 1 Manually operated; Value = 2 Power operated | 1-2 | 50053 | x | | x |
| A/Act | Clamping cylinder. Value = 0 no clamping cylinder; Value = 1 return = close jaws; Value = 2 return = open jaws | 0-2 | 50054 | x | | x |
| A/Act | Chuck: after clamping time [20 msec] | 0-100 | 50055 | | | x |
| C | Clamper type. Value = 0 hydraulic, stroke limitation safety prox. switches, centre positioning valve (standard); Value = 1 hydraulic, without stroke limitation, engagement valves | 0-10 | 50056 | | | x |
| C | Swivel chuck type. Value = 0 no swivel chuck fitted; Value = 1 AXN swivel chuck type from Autoblock with 4 positions | 0-10 | 50057 | | | |
| C | Maximum chuck swivel speed. Value = maximum speed permitted for swivelling the swivel chuck [r.p.m] | 0-65535 | 50058 | | | |
| | **Dialog box - 3 -** (number handwritten) | | | | | |
| D | Spindle fitted | 0-1 | 50059 | x | | |
| D | Spindle clamping fitted | 0-1 | 50060 | | | x |
| D | Clamp release time [1/50 sec.] | 0-255 | 50061 | | | x |
| D | Clamp clamping time [1/50 sec.] | 0-255 | 50062 | | | x |
| D | Type C axis. Value = 0 no C axis fitted; Value = 1 C axis with contact pressure roller; Value = 2 C axis without contact pressure roller | 0-10 | 50063 | x | | x |
| D | Spindle: delay time 1 [100 msec] (HW spindle release --> controller release chopper ON) | 0-255 | 50064 | | | x |
| | **Dialog box - 4 -** (number handwritten) | | | | | |
| D | Gearing type. Value = 0 no gearing fitted; Value = 1 ZF: 2 stage electrically actuated | 0-10 | 50065 | | | x |

Note: on the CTX twin, the spindle/chuck parameter set marked for the twin is mainly "Spindle 3, chuck 3, gearing 3" (Parameter 025); in Parameter 025 no "Dialog box - 2 -" heading is printed (Clamper type follows directly after the chuck after-clamping time).

##### Parameter 026: Cont. and tool holder para. slide 1

(Page 9-13. Handwritten at top right of the page: "74 267(" [uncertain]. In the turret type list the lines for Value = 1, 2, 5 and 6 carry handwritten underlines/strokes.)

| Prot. class | Parameter | Value range | K No. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|
| | **Dialog box - 1 -** | | | | | |
| D | Slide fitted | 0-1 | 50066 | | | |
| D | Feed: delay time 1 [20 msec] (Regulator release chopper ON --> release brake) | 0-255 | 50067 | | x | |
| D | Feed: delay time 2 [20 msec] (brake on --> regulator release chopper ON) | 0-255 | 50068 | | x | |
| D | Feed: delay time 3 [100 msec] (HW feed release --> regulator release chopper ON) | 0-255 | 50069 | | | x |
| C | Feed: detent clutch type. Value = 0 no detent clutch; Value = 1 type 1 (Meyer) | 0-10 | 50070 | | | x |
| | **Dialog box -2-** | | | | | |
| D | Turret type. Value = 1 Multifix; Value = 2 Sauter, cam roller contr. Series 200; Value = 3 Sauter, cam roller contr. Series 500; Value = 4 Duplomatic turret Series BSV-N; Value = 5 Sauter Servo turret; Value = 6 Sauter, cam roller contr. Series 500 with Dahlander motor | 1-20 | 50071 | x | x | x |
| D | Number of tool positions | 1-16 | 50072 | x | x | x |

##### Parameter 027: Cont. and tool holder para. slide 2

(Page 9-14.)

| Prot. class | Parameter | Value range | K No. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|
| | **Dialog box - 1 -** | | | | | |
| D | Slide fitted | 0-1 | 50073 | x | | |
| D | Feed: delay time 1 [20 msec] (Regulator release chopper ON --> release brake) | 0-255 | 50074 | | | x |
| D | Feed: delay time 2 [20 msec] (brake on --> regulator release chopper ON) | 0-255 | 50075 | | | x |
| D | Feed: delay time 3 [100 msec] (HW feed release --> regulator release chopper ON) | 0-255 | 50076 | | | x |
| C | Feed: detent clutch type. Value = 0 no detent clutch; Value = 1 type 1 (Meyer) | 0-10 | 50077 | | | x |
| | **Dialog box -2-** | | | | | |
| D | Turret type. Value = 1 Multifix; Value = 2 Sauter, cam roller contr. Series 200; Value = 3 Sauter, cam roller contr. Series 500; Value = 4 Duplomatic turret Series BSV-N; Value = 5 Sauter Servo turret; Value = 6 Sauter, cam roller contr. Series 500 with Dahlander motor | 1-20 | 50078 | | | x |
| D | Number of tool positions | 1-16 | 50079 | | | x |

##### Parameter 028: Cont. and tool holder para. slide 3

(Page 9-15. No "x" printed in any machine column on this page.)

| Prot. class | Parameter | Value range | K No. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|
| | **Dialog box - 1 -** | | | | | |
| D | Slide fitted | 0-1 | 50080 | | | |
| D | Feed: delay time 1 [20 msec] (Regulator release chopper ON --> release brake) | 0-255 | 50081 | | | |
| D | Feed: delay time 2 [20 msec] (brake on --> regulator release chopper ON) | 0-255 | 50082 | | | |
| D | Feed: delay time 3 [100 msec] (HW feed release --> regulator release chopper ON) | 0-255 | 50083 | | | |
| C | Feed: detent clutch type. Value = 0 no detent clutch; Value = 1 type 1 (Meyer) | 0-10 | 50084 | | | |
| | **Dialog box -2-** | | | | | |
| D | Turret type. Value = 1 Multifix; Value = 2 Sauter, cam roller contr. Series 200; Value = 3 Sauter, cam roller contr. Series 500; Value = 4 Duplomatic turret Series BSV-N; Value = 5 Sauter Servo turret; Value = 6 Sauter, cam roller contr. Series 500 with Dahlander motor | 1-20 | 50085 | | | |
| D | Number of tool positions | 1-16 | 50086 | | | |

##### Parameter 029: Cont. and tool holder para. slide 4

(Page 9-16. The column header on this page is misaligned in print: "Value / range" is shifted one column and an extra "CTX" column appears; no "x" is printed in any machine column.)

| Prot. class | Parameter | Value range | K No. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|
| | **Dialog box - 1 -** | | | | | |
| D | Slide fitted | 0-1 | 50087 | | | |
| D | Feed: delay time 1 [20 msec] (Regulator release chopper ON --> release brake) | 0-255 | 50088 | | | |
| D | Feed: delay time 2 [20 msec] (brake on --> regulator release chopper ON) | 0-255 | 50089 | | | |
| D | Feed: delay time 3 [100 msec] (HW feed release --> regulator release chopper ON) | 0-255 | 50090 | | | |
| C | Feed: detent clutch type. Value = 0 no detent clutch; Value = 1 type 1 (Meyer) | 0-10 | 50091 | | | |
| | **Dialog box -2-** | | | | | |
| D | Turret type. Value = 1 Multifix; Value = 2 Sauter, cam roller contr. Series 200; Value = 3 Sauter, cam roller contr. Series 500; Value = 4 Duplomatic turret Series BSV-N; Value = 5 Sauter Servo turret; Value = 6 Sauter, cam roller contr. Series 500 with Dahlander motor | 1-20 | 50092 | | | |
| D | Number of tool positions | 1-16 | 50093 | | | |

##### Parameter 030: Cont. and tool holder para. slide 5

(Page 9-17. No "x" printed in any machine column.)

| Prot. class | Parameter | Value range | K No. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|
| | **Dialog box - 1 -** | | | | | |
| D | Slide fitted | 0-1 | 50094 | | | |
| D | Feed: delay time 1 [20 msec] (Regulator release chopper ON --> release brake) | 0-255 | 50095 | | | |
| D | Feed: delay time 2 [20 msec] (brake on --> regulator release chopper ON) | 0-255 | 50096 | | | |
| D | Feed: delay time 3 [100 msec] (HW feed release --> regulator release chopper ON) | 0-255 | 50097 | | | |
| C | Feed: detent clutch type. Value = 0 no detent clutch; Value = 1 type 1 (Meyer) | 0-10 | 50098 | | | |
| | **Dialog box -2-** | | | | | |
| D | Turret type. Value = 1 Multifix; Value = 2 Sauter, cam roller contr. Series 200; Value = 3 Sauter, cam roller contr. Series 500; Value = 4 Duplomatic turret Series BSV-N; Value = 5 Sauter Servo turret; Value = 6 Sauter, cam roller contr. Series 500 with Dahlander motor | 1-20 | 50099 | | | |
| D | Number of tool positions | 1-16 | 50100 | | | |

(No Parameter 031 is printed; the list continues with Parameter 032 on page 9-18. K numbers 50101-50107 do not appear.)

##### Parameter 032: Eltromatic modules

(Page 9-18.)

| Prot. class | Parameter | Value range | K No. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|
| | **Dialog box - 1 -** | | | | | |
| D | I/O - module 01 installed | 0-1 | 50108 | x | x | x |
| D | I/O - module 02 installed | 0-1 | 50109 | x | x | x |
| D | I/O - module 03 installed | 0-1 | 50110 | x | x | x |
| D | I/O - module 04 installed | 0-1 | 50111 | x | x | x |
| D | I/O - module 05 installed | 0-1 | 50112 | x | x | x |
| D | I/O - module 06 installed | 0-1 | 50113 | x | x | x |
| | **Dialog box - 2 -** | | | | | |
| D | I/O - module 07 installed | 0-1 | 50114 | x | x | x |
| D | I/O - module 08 installed | 0-1 | 50115 | x | x | x |
| D | I/O - module 09 installed | 0-1 | 50116 | x | x | x |
| D | I/O - module 10 installed | 0-1 | 50117 | x | x | x |
| D | I/O - module 11 installed | 0-1 | 50118 | x | x | x |
| D | I/O - module 12 installed | 0-1 | 50119 | x | x | x |
| | **Dialog box - 3 -** | | | | | |
| D | I/O - module 13 installed | 0-1 | 50120 | x | x | x |
| D | I/O - module 14 installed | 0-1 | 50121 | x | x | x |
| D | I/O - module 15 installed | 0-1 | 50122 | x | x | x |
| D | I/O - module 16 installed | 0-1 | 50123 | x | x | x |

##### Parameter 033: Tool monitoring

(Page 9-19.)

| Prot. class | Parameter | Value range | K No. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|
| | **Dialog box - 1 -** | | | | | |
| D | Tool monitoring I: unit type | 0-5 | 50124 | x | x | x |
| D | Tool monitoring I: output format | 0-17 | 50125 | x | x | x |
| D | Tool monitoring I: strategy | 0-15 | 50126 | x | x | x |
| | **Dialog box -2-** | | | | | |
| D | Tool monitoring II: unit type | 0-5 | 50127 | | | x |
| D | Tool monitoring II: output format | 0-17 | 50128 | | | x |
| D | Tool monitoring II: strategy | 0-15 | 50129 | | | x |

##### Parameter 034: Bulkhead plate

(Page 9-19.)

| Prot. class | Parameter | Value range | K No. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|
| | **Dialog box - 1 -** | | | | | |
| D | Bulkhead plate fitted | 0-1 | 50130 | | | x |
| C | Bulkhead plate: maximum time [sec] (Max. time for function execution) | 0-255 | 50131 | | | x |

##### Parameter 035: Quantity impulse

(Page 9-19.)

| Prot. class | Parameter | Value range | K No. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|
| | **Dialog box -1-** | | | | | |
| C | Quantity impulse output. Value = 0 impulses from AG1 and AG2 directly; Value = 1 impulses only from AG1; Value = 2 impulses only from AG2; Value = 3 impulses from AG1 and AG2 time offset | 0-10 | 50132 | | | x |

##### Parameter 036: Steady rest

(Page 9-20. The column header is misprinted on this page ("CTX 200 / CTX 400 / CTX twin 500 600"); the three "x" columns are taken as CTX 200, CTX 400/500/600, CTX twin.)

| Prot. class | Parameter | Value range | K No. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|
| | **Dialog box - 1 -** | | | | | |
| C/Act | Type steady rest 1. Value = 0 not fitted; Value = 1 fixed to bed; Value = 2 carried by bed | 0-10 | 50133 | x | x | x |
| C/Act | Type steady rest 2. Value = 0 not fitted; Value = 1 fixed to bed; Value = 2 carried by bed | 0-10 | 50134 | x | x | x |
| C/Act | Type steady rest 3. Value = 0 not fitted; Value = 1 fixed to bed; Value = 2 carried by bed | 0-10 | 50135 | x | x | x |
| C/Act | Type steady rest 4. Value = 0 not fitted; Value = 1 fixed to bed; Value = 2 carried by bed | 0-10 | 50136 | x | x | x |

##### Parameter 037: ISM temperature control

(Pages 9-20 to 9-21.)

| Prot. class | Parameter | Value range | K No. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|
| | **Dialog box - 1 -** | | | | | |
| D | Temperature control fitted | 0-1 | 50137 | | | x |
| D | Number of values for mean value comput. | 0-255 | 50138 | | | x |
| D | Max. ambient temperature in °C | 0-255 | 50139 | | | x |
| D | Cooling water temperature set point value in °C | 0-255 | 50140 | | | (the "x" is misprinted under the "D" in the protection class column) |
| D | Cooling water temperature in °C hysteresis | 0-255 | 50141 | | | x |
| | **Dialog box - 2 -** | | | | | |
| D | Winding temperature set point value in °C | 0-255 | 50142 | | | x |
| D | Winding temperature hysteresis in °C | 0-255 | 50143 | | | x |
| D | Front bearing temperature set point value in °C | 0-255 | 50144 | | | x |
| D | Front bearing temperature hysteresis in °C | 0-255 | 50145 | | | x |
| D | Rear bearing temperature set point value in °C | 0-255 | 50146 | | | x |
| D | Rear bearing temperature hysteresis in °C | 0-255 | 50147 | | | x |
| | **Dialog box - 3 -** | | | | | |
| D | MAX winding temp. in °C | 0-255 | 50148 | | | x |
| D | MAXMAX winding temp. in °C | 0-255 | 50149 | | | x |
| D | MAX front bearing temp. in °C | 0-255 | 50150 | | | x |
| D | MAXMAX front bearing temp. in °C | 0-255 | 50151 | | | x |
| D | MAX rear bearing temp. in °C | 0-255 | 50152 | | | x |
| D | MAXMAX rear bearing temp. in °C | 0-255 | 50153 | | | x |

##### Parameter 038: tailstock, centre sleeve

(Page 9-21.)

| Prot. class | Parameter | Value range | K No. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|
| | **Dialog box - 1 -** | | | | | |
| D | Tailstock type. Value = 0 fixed tailstock; Value = 1 hydraulic tailstock type 1 (movable by cylinder); Value = 2 hydraulic tailstock type 2 (fixed stop); Value = 3 hydraulic tailstock type 3 (Teach-in) | 0-10 | 50154 | x | x | x |
| D | Centre sleeve type. Value = 0 no centre sleeve fitted; Value = 1 hydraulic centre sleeve | 0-10 | 50155 | x | x | x |

##### Parameter 039: material feeding/removal (pages 9-23 to 9-25: "material feeding / discharge")

(Pages 9-22 to 9-25.)

| Prot. class | Parameter | Value range | K No. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|
| | **Dialog box - 1 -** | | | | | |
| D | Pick-off device type I. Value = 0 not fitted; Value = 1 pick-off device type 1 (CTX xxx); Value = 2 pick-off device type 2 (CTX twin); Value = 3 pick-off device type 3 (CTX500 4A); Value = 4 pick-off device type 4 (CTX200) | 0-10 | 50156 | x | x | x |
| C | Pick-off device I: time 1 [sec] (Max. time for performing function) | 0-25111 (as printed; probably 0-255) | 50157 | x | x | x |
| A/Act | Pick-off device I: time 2 [100 msec] (Time for flap opened) | 0-255 | 50158 | x | x | x |
| A/Act | Pick-off device I: time 3 [sec] (Time for conveyor on). Value = 0 = => no time limitation | 0-255 | 50159 | x | | x |
| | **Dialog box - 2 -** | | | | | |
| D | Pick-off device type II. Value = 0 not fitted; Value = 1 pick-off device type 1 (CTX xxx); Value = 2 pick-off device type 2 (CTX twin); Value = 3 pick-off device type 3 (CTX500 4A); Value = 4 pick-off device type 4 (CTX200) | 0-10 | 50160 | | | x |
| C | Pick-off device II: time 1 [sec] (Max. time for performing function) | 0-255 | 50161 | | | x |
| A/Act | Pick-off device II: time 2 [100 msec] (Time for flap opened) | 0-255 | 50162 | | | x |
| A/Act | Pick-off device II: time 3 [sec] (Time for conveyor on). Value = 0 = => no time limitation | 0-255 | 50163 | | | x |
| | **Dialog box - 3 -** | | | | | |
| D | Handling type I. Value = 0 not fitted; Value = 1 HH = Promot gantry; Value = 2 HH = Single robot; Value = 3 HH = Felsomat gantry; Value = 4 HH = Single robot for TWIN | 0-10 | 50164 | x | x | x |
| D | Loading flap type I. Value = 0 not fitted; Value = 1 loading flap type 1 | 0-10 | 50165 | x | x | x |
| D | Handling 1: axis barrier at HH in working space (bit list below) | UNS32 | 50166 | | | x |
| D | Handling type II. Value = 0 not fitted; Value = 1 HH = Promot gantry; Value = 2 HH = Single robot; Value = 3 HH = Felsomat gantry; Value = 4 HH = Single robot for TWIN | 0-10 | 50167 | | | x |
| D | Loading flap type II. Value = 0 not fitted; Value = 1 loading flap type 1 | 0-10 | 50168 | | | x |
| D | Handling II: axis barrier at HH in working space (bit list below) | UNS32 | 50169 | | | x |
| | **Dialog box - 4 -** | | | | | |
| C | Material feeding type I. Value = 0 not fitted; Value = 1 bar loader; Value = 2 bar feed; Value = 3 bar loader with CE; Value = 4 bar feed with CE; handwritten addition: Value = 5 short workpiece bar feed (Sameca Multisam [uncertain]) | 0-10 | 50170 | x | x | x |
| C | Material feeding type II. Value = 0 not fitted; Value = 1 bar loader; Value = 2 bar feed; Value = 3 bar loader with CE; Value = 4 bar feed with CE; handwritten addition: Value = 5 short workpiece bar feed (Sameca Multisam [uncertain]) | 0-10 | 50171 | x | x | x |

Axis barrier bit assignment for K 50166 (Handling 1, page 9-23) and K 50169 (Handling II, page 9-24); identical in both lists except that page 9-23 prints "Bit 0 lock X1 axis?" with a question mark:

| Bit | Meaning | Bit | Meaning | Bit | Meaning |
|---|---|---|---|---|---|
| Bit 0 | lock X1 axis (9-23: "lock X1 axis?") | Bit 10 | lock B2 axis | Bit 20 | lock Y4 axis |
| Bit 1 | lock Z1 axis | Bit 11 | lock C2 axis | Bit 21 | lock A4 axis |
| Bit 2 | lock Y1 axis | Bit 12 | lock X3 axis | Bit 22 | lock B4 axis |
| Bit 3 | lock A1 axis | Bit 13 | lock Z3 axis | Bit 23 | lock C4 axis |
| Bit 4 | lock B1 axis | Bit 14 | lock Y3 axis | Bit 24 | lock X5 axis |
| Bit 5 | lock C1 axis | Bit 15 | lock A3 axis | Bit 25 | lock Z5 axis |
| Bit 6 | lock X2 axis | Bit 16 | lock B3 axis | Bit 26 | lock Y5 axis |
| Bit 7 | lock Z2 axis | Bit 17 | lock C3 axis | Bit 27 | lock A5 axis |
| Bit 8 | lock Y2 axis | Bit 18 | lock X4 axis | Bit 28 | lock B5 axis |
| Bit 9 | lock A2 axis | Bit 19 | lock Z4 axis | Bit 29 | lock C5 axis |

##### Parameter 040: Protection hoods

(Page 9-25 onwards. Values 5 and 6 of K 50172 and the end of the value 4 text are handwritten additions.)

| Prot. class | Parameter | Value range | K No. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|
| | **Dialog box - 1 -** | | | | | |
| D | Protection hood I. Value = 0 type 1 (manually oper.); Value = 1 type 2 (auto., electr., with safety rail); Value = 2 type 3 (auto., electr., without safety rail); Value = 3 type 4 (auto., pneu., with safety rail); Value = 4 type 5 (auto., pneu., [handwritten:] without safety rail); [handwritten:] Value = 5 type 6 (auto., pneu. typ 2 with safety rail); Value = 6 type 7 (auto., pneu. typ 2 without safety rail) | 0-255 | 50172 | | | x |
| A/Act | Automatically unlocked protection hood I | 0-1 | 50173 | x | x | x |
| A/Act. | Automatic cycle start active protection hood I (handwritten in the margin: "v.s -> 0 -> 1" [uncertain]) | 0-1 | 50174 | x | x | x |
| D | Time monitoring protection hood I. Value = 0 = => no time limitation; Value #0 = => time monitoring in sec. | 0-2000 | 50175 | x | x | x |
| D | Protection hood I: delay time 1 [100 msec] (regulator release chopper OFF -> unlock hood) | 0-255 | 50176 | x | x | x |

##### Parameter 040: Protection hoods (continued)

(Page 9-26. Column header misprinted as on page 9-16/9-20; the "x" marks stand in the CTX twin position. Values 5 and 6 of K 50177 and the end of the value 4 text are handwritten additions.)

| Prot. class | Parameter | Value range | K No. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|
| | **Dialog box - 2 -** | | | | | |
| D | Protection hood type II. Value = 0 type 1 (manually oper.); Value = 1 type 2 (auto., electr., without safety rail) (as printed; for hood I this reads "with safety rail"); Value = 2 type 3 (auto., electr., without safety rail); Value = 3 type 4 (auto., pneu., with safety rail); Value = 4 type 5 (auto., pneu., [handwritten:] without safety rail); [handwritten:] Value = 5 type 6 (auto., pneu. typ 2 with safety rail); Value = 6 type 7 (auto., pneu. typ 2 without safety rail) | 0-255 | 50177 | | | x |
| A/Act | Automatically unlocked protection hood II | 0-1 | 50178 | | | x |
| A/Act. | Automatic cycle start active protection hood II | 0-1 | 50179 | | | x |
| D | Time monitoring protection hood I (as printed; belongs to hood II). Value = 0 = => no time limitation; Value #0 = => time monitoring in sec. | 0-2000 | 50180 | | | x |
| D | Protection hood II: delay time II [100 msec] (regulator release chopper OFF -> unlock hood) | 0-255 | 50181 | | | x |

##### Parameter 041: Measuring systems

(Pages 9-26 to 9-27; page 9-27 header: "Measuring system".)

| Prot. class | Parameter | Value range | K No. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|
| | **Dialog box - 1 -** | | | | | |
| C | Post process measuring type I. Value = 0 not fitted; Value = 1 type 1 | 0-10 | 50182 | x | x | x |
| C | Tool measuring type I. Value = 0 not fitted; Value = 1 type 1 | 0-10 | 50183 | x | x | x |
| C | Tool measuring type I (printed twice). Value = 0 not fitted; Value = 1 type 1 (MARPOSS) | 0-10 | 50184 | | x | x |
| | **Dialog box - 2 -** | | | | | |
| C | Post process measuring type II. Value = 0 not fitted; Value = 1 type 1 | 0-10 | 50185 | | | x |
| C | Tool measuring type II. Value = 0 not fitted; Value = 1 type 1 | 0-10 (printed in the text column) | 50186 | | x | |
| C | Tool measuring type II. Value = 0 not fitted; Value = 1 type 1 (MARPOSS) | 1-10 (as printed, in the text column) | 50187 | | x | |

##### Parameter 042: Lubrication

(Page 9-27.)

| Prot. class | Parameter | Value range | K No. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|
| | **Dialog box - 1 -** | | | | | |
| D | Additional messages for lubrication I | 0-10 | 50188 | x | x | x |
| C | Interval time for lubr. impulse I [min] | 0-255 | 50189 | | | x |
| C | Max. lubr. impulse I suppression time | 0-255 | 50190 | | | x |
| | **Dialog box - 2 -** | | | | | |
| D | Additional messages for lubrication II | 0-10 | 50191 | | | x |
| C | Interval time for lubr. impulse II [min] | 0-255 | 50192 | | | x |
| C | Max. lubr. impulse II suppression time | 0-255 | 50193 | | | x |

##### Parameter 043: Hydraulics

(Page 9-28.)

| Prot. class | Parameter | Value range | K No. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|
| | **Dialog box - 1 -** | | | | | |
| D | Additional messages for hydraulic I | 0-10 | 50194 | x | x | x |
| D | Switch off hydraulic I including drives | 0-10 | 50195 | x | x | x |
| | **Dialog box - 2 -** | | | | | |
| D | Additional signal for hydraulic II | 0-10 | 50196 | | | x |
| D | Switch off hydraulic II including drives | 0-10 | 50197 | | | x |

##### Parameter 044: Chip conveyor

(Page 9-28.)

| Prot. class | Parameter | Value range | K No. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|
| | **Dialog box - 1 -** | | | | | |
| D | Chip conveyor I fitted | 0-1 | 50198 | x | x | x |
| A/Act | Chip conveyor I: interval time ON [sec]. Value = 0 = => no interval operation | 0-1200 | 50199 | | | x |
| A/Act | Chip conveyor I: interval time OFF [sec]. Value = 0 = => no interval operation | 0-1200 | 50200 | | | x |
| | **Dialog box - 2 -** | | | | | |
| D | Chip conveyor II fitted | 0-1 | 50201 | | | x |
| A/Act | Chip conveyor II: interval time ON [sec]. Value = 0 = => no interval operation | 0-1200 | 50202 | | | x |
| A/Act | Chip conveyor II: interval time OFF [sec]. Value = 0 = => no interval operation | 0-1200 | 50203 | | | x |

##### Parameter 045: Comfort user relays

(Pages 9-29 to 9-30. Every entry has the same value list: Value = 0 not fitted; Value = 1 fitted, no automatic cut out; Value = 2 fitted, cut-out with emergency stop; Value = 3 fitted, cut out with fast stop (major fault, operating mode change, emergency stop).)

| Prot. class | Parameter | Value range | K No. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|
| | **Dialog box - 1 -** | | | | | |
| C | Comfort user relays type 1 to 4 | 0-3 | 50204 | x | x | x |
| C | Comfort user relays type 5 to 8 | 0-3 | 50205 | x | x | x |
| C | Comfort user relays type 9 to 12 | 0-3 | 50206 | x | x | x |
| C | Comfort user relays type 13 to 16 | 0-3 | 50207 | x | x | x |
| | **Dialog box - 2 -** | | | | | |
| C | Comfort user relays type 17 to 20 | 0-3 | 50208 | | | |
| C | Comfort user relays type 21 to 24 | 0-3 | 50209 | | | |
| C | Comfort user relays type 29 to 32 (printed in this order) | 0-3 | 50210 | | | |
| C | Comfort user relays type 25 to 28 | 0-3 | 50211 | | | |

##### Parameter 046: Test functions

(Page 9-31.)

| Prot. class | Parameter | Value range | K No. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|
| | **Dialog box - 1 -** | | | | | |
| D | Emulator operation | 0-1 | 50212 | x | x | x |
| D | Test fault texts | 0-1 | 50213 | x | x | x |
| D | Test 3 (reserve) | 0-255 | 50214 | | | |
| D | Test 4 (reserve) | 0-255 | 50215 | | | |
| D | Test 5 (reserve) | 0-255 | 50216 | | | |
| D | Test 6 (reserve) | 0-255 | 50217 | | | |
| | **Dialog box - 2 -** | | | | | |
| D | Test 7 (reserve) | 0-255 | 50218 | | | |
| D | Test 8 (reserve) | 0-255 | 50219 | | | |
| D | Test 9 (reserve) | 0-255 | 50220 | | | |
| D | Test 10 (reserve) | 0-255 | 50221 | | | |
| D | Test 11 (reserve) | 0-255 | 50222 | | | |
| D | Test 12 (reserve) | 0-255 | 50223 | | | |

##### Parameter 047: Reserve parameters

(Page 9-32; handwritten mark at the top left of the page [unreadable]. The table continues in part b from page 9-33.)

| Prot. class | Parameter | Value range | K No. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|
| | **Dialog box - 1 -** | | | | | |
| C | Unsigned long number 50224. Reserved for: handling the D releases | UNS32 | 50224 | x | x | x |
| C | Unsigned long number 50225. Reserved for: earth break monitoring active | UNS32 | 50225 | x | x | x |
| C | Unsigned long number 50226. Coolant: flow monitoring fitted | UNS32 0-1 | 50226 | | | x |
| C | Unsigned long number 50227. Coolant: flow monitoring debounce time Monitoring start / end [100 msec] | UNS32 0-65535 | 50227 | | | x |
| C | Unsigned long number 50228. Coolant: flow monitoring debounce time during monitoring [100 msec] | UNS32 0-65535 | 50228 | | | x |
| C | Unsigned long number 50229 | UNS32 | 50229 | | | |
| | **Dialog box - 2 -** | | | | | |
| C | Unsigned long number 50230 | UNS32 | 50230 | | | |
| C | Unsigned long number 50231 | UNS32 | 50231 | | | |
| C | Unsigned long number 50232 | UNS32 | 50232 | | | |
| C | Unsigned long number 50233 | UNS32 | 50233 | | | |
| C | Unsigned long number 50234 | UNS32 | 50234 | | | |
| C | Unsigned long number 50235 | UNS32 | 50235 | | | |
| | **Dialog box - 3 -** | | | | | |
| C | Unsigned long number 50236 | UNS32 | 50236 | | | |
| C | Unsigned long number 50237 | UNS32 | 50237 | | | |
| C | Unsigned long number 50238 | UNS32 | 50238 | | | |
| C | Unsigned long number 50239 | UNS32 | 50239 | | | |

(Continued in part b, page 9-33.)

---

## For the retrofit

- Control: the parameter list refers to the "ELTROPILOT programming instructions" for all other parameters (p. 9-6) and to "Eltromatic modules" (I/O modules 01-16, K 50108-50123, all marked for CTX twin, p. 9-18), i.e. a Gildemeister ELTROPILOT / Eltromatic control, not Siemens or Fanuc. "AG1"/"AG2" (two PLCs, one per side) appear in the quantity impulse parameter K 50132 (p. 9-19). The model numbers themselves (K 5000-5002) are not printed (p. 9-6).
- Two-sided structure: nearly every twin-relevant function exists as "I" and "II" (tool monitoring, pick-off device, handling, loading flap, material feeding, protection hood, measuring, lubrication, hydraulic, chip conveyor; pp. 9-19 to 9-28), so each side has its own hydraulic unit, lubrication, chip conveyor, hood and pick-off device.
- Spindles: twin-marked spindle data are in Parameter 025 "Spindle 3, chuck 3, gearing 3" (chuck mode internal/external/disengagement stroke, power chuck, clamping cylinder, clamper with stroke-limitation prox. switches, spindle clamping release/clamp times in 1/50 s, C axis type with/without contact pressure roller, gearing type, p. 9-11/9-12); "Spindle: delay time 1 [100 msec] (HW spindle release --> controller release chopper ON)" marked twin for spindles H, 1, 2, 3 (pp. 9-8 to 9-12). C axis 1 high speed in deg./min and C zero offset are operator parameters (p. 9-4).
- Feed axes: slides 1 and 2 marked twin with "Feed: delay time 1/2 [20 msec]" (chopper ON -> release brake; brake on -> chopper), "delay time 3 [100 msec] (HW feed release -> regulator release chopper ON)" and detent clutch type (Meyer) (pp. 9-13, 9-14): the axes have brakes the control must sequence with drive enable.
- Turret: turret type options Multifix, Sauter cam roller series 200/500, Duplomatic BSV-N, Sauter Servo turret, Sauter series 500 with Dahlander motor; up to 16 tool positions (K 50071/50072, 50078/50079, pp. 9-13, 9-14). The value set for this machine is not printed.
- Axis naming: handling axis barrier bit masks list axes X/Z/Y/A/B/C 1 to 5 (K 50166, 50169, pp. 9-23, 9-24); handling type value 4 "Single robot for TWIN"; pick-off device type 2 "(CTX twin)" (p. 9-22).
- Tailstock types fixed / hydraulic cylinder / fixed stop / teach-in, hydraulic centre sleeve (p. 9-21).
- Hoods/safety: protection hood types manual, auto electric or pneumatic with/without safety rail, auto-unlock, cycle start by hood, time monitoring up to 2000 s, delay "regulator release chopper OFF -> unlock hood" (drives must be disabled before the hood unlocks) (pp. 9-25, 9-26).
- Spindle motor temperature monitoring (ISM): winding, front/rear bearing and cooling water set points, hysteresis, MAX/MAXMAX limits in °C (pp. 9-20, 9-21). Coolant flow monitoring with debounce times (K 50226-50228, p. 9-32). Earth break monitoring (K 50225).
- Lubrication: interval time for lubrication impulse [min] and suppression time per side (p. 9-27). Chip conveyor interval ON/OFF up to 1200 s (p. 9-28). Bulkhead plate with max. function time (p. 9-19).
- No part-transfer M-codes or PLC message numbers on these pages (they are in 9.2 from p. 9-42 and 9.3 from p. 9-54, other parts).
