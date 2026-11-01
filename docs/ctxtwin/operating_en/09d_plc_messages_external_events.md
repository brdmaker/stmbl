# Gildemeister CTX twin operating instructions (English edition), 9. Parameter setting, M commands, PLC error information (part d: PLC messages 7107-12250, 9.4 External events), pages 9-117 to 9-160

Source: `CTX_twin_Operating_instructions_9d.pdf`, 46 scanned pages, "Operating instructions CTX twin, Dok. No.: daaab1g1, February 1995", manual pages 9-117 to 9-160 plus two unnumbered blank pages at the end.
Original language: English.

Running page header: "9. Parameter setting, M commands, PLC error information", with the first message number on the page and its class letter at top right (e.g. "7107 W:"). Footer: "Operating instructions CTX twin / Dok. No.: daaab1g1, February 1995 / Page 9-xxx".

These pages continue the PLC error list that starts in part c (the section heading and introduction are there; this file starts mid-list at message 7107 of the slide 1 / `AG1` group). They contain the end of the `AG1` list (7107-7250), the complete `AG2` (slide 2) list 10300-12250, and section 9.4 "External events" with 9.4.1 "Assignment of the event flag". The chapter ends on page 9-160; the last two PDF pages are blank.

Scan notes: PDF page 26 (manual page 9-142, between 9-141 and 9-143) has its footer misprinted as "Page 9-140". Some numbers carry handwritten circles/marks (7107, 7109, 7110, 7150, 11452 with an arrow, 11753, 12150); these are owner annotations, not part of the print. Message 11755 is printed twice (F and W); the W entry is presumably 11756.

Message classes (prefix of every text): `F:` fault, `W:` warning, `M:` message (as used throughout the list). `AG1` = slide 1 (left lathe), `AG2` = slide 2 (right lathe) (see 9.4.1 headings "slide 1 AG1" / "slide 2 AG2"). The AG2 numbers mirror the AG1 numbers with an offset of 5000 (e.g. 7107 / 12107, 7250 / 12250).

Each message in the manual has the layout: number, bold message text, "Cause:" (category, then specific cause on the next line), "Overcome:" (who, then the action). In the table below the category and the specific cause are separated by a full stop.

## 9.3 PLC error information (continued)

### Slide 1 (AG1), end of list: 7107-7250 (pages 9-117, 9-118)

| No. | Message text | Cause | Overcome (by whom / action) |
|---|---|---|---|
| 7107 | `W: ISM temperature control AG1: max. motor winding temperature exceeded` | possible defect or incorrect setting, parameter or overload in cycle. Max. motor winding temperature exceeded | by electrical/mechanical maintenance personnel. Permit motor to cool, check motor, check parameter |
| 7108 | `M: ISM temperature control AG1: max. ambient temperature exceeded` | possible defect or incorrect setting, parameter. Max. ambient temperature exceeded | by electrical/mechanical maintenance personnel. Check temperature sensor, check parameter |
| 7109 | `F: ISM temperature control AG1: AEM module does not react` | possible defect or incorrect setting, parameter. AEM module does not correctly react to measurement requirement | by electrical maintenance personnel. Check AEM module, check temperature sensor, check parameter |
| 7110 | `M: ISM temperature control AG1: AEM module does not react` | possible defect or incorrect setting, parameter. AEM module does not correctly react to measurement requirement | by electrical maintenance personnel. Check AEM module, check temperature sensor, check parameter |
| 7111 | `W: ISM temperature control AG1: AEM module does not react` | possible defect or incorrect setting, parameter. AEM module does not correctly react to measurement requirement | by electrical maintenance personnel. Check AEM module, check temperature sensor, check parameter |
| 7112 | `F: ISM temperature control AG1: maxmax rear bearing temperature exceeded` | possible defect or incorrect setting, parameter. Maxmax rear bearing temperature exceeded | by electrical/mechanical maintenance personnel. Permit bearing to cool, check bearing, check parameter |
| 7113 | `F: ISM temperature control AG1: maxmax front bearing temperature exceeded` | possible defect or incorrect setting, parameter. Maxmax front bearing temperature exceeded | by electrical/mechanical maintenance personnel. Permit bearing to cool, check bearing, check parameter |
| 7114 | `F: ISM temperature control AG1: maxmax motor winding temperature exceeded` | possible defect or incorrect setting, parameter, or overload in cycle. Maxmax motor winding temperature exceeded | by electrical/mechanical maintenance personnel. Permit motor to cool, check motor, check parameter |
| 7150 | `F: Supply AG1: not operational` | possible defect. Supply is disturbed | by electrical maintenance personnel. Overcome fault refer to electrical service handbook |
| 7151 | `F: Supply AG1: temperature prewarning` | possible defect or incorrect setting, parameter, or overload in cycle. Supply signals over temperature | by electrical maintenance personnel. Check supply |
| 7152 | `W: Supply AG1: temperature prewarning` | possible defect or incorrect setting, parameter, or overload in cycle. Supply signals over temperature | by electrical maintenance personnel. Check supply |
| 7250 | `F: PLC AG1: Internal software error` | internal PLC fault | by setter, if necessary Gildemeister service. Check PLC setting, parameter and switch machine off and on again, please notify Gildemeister service |

### Slide 2 (AG2): 10300-12250 (pages 9-119 to 9-158)

| No. | Message text | Cause | Overcome (by whom / action) |
|---|---|---|---|
| 10300 | `F: Input AG2: M function not defined` | inadmissible operation in machine status: M function not defined for this machine equipment | by operator. acknowledge error |
| 10301 | `F: INPUT AG2: M function inadmissible due machine status` | inadmissible operation in machine status: M function inadmissible due to machine status | by operator. Acknowledge error |
| 10350 | `F: Setting operation AG2: inadmissible selection` | inadmissible operation in machine status: Protection hood is closed or manual operating mode is not selected | by operator. Open protection hood and select manual operating mode |
| 10351 | `M: Setting operation AG2: Inadmissible` | inadmissible operation in machine status. Protection hood is closed or manual operating mode is not selected | by operator. Open protection hood and select manual operating mode |
| 10400 | `F: Eltromatic AG2: Run up error phase 3` | possible defect. Fault when testing the communication | by electrical maintenance personnel. Check I/O modules and connection to controls |
| 10401 | `F: Eltromatic AG2: Run up error phase 4` | possible defect. General error when commencing cyclical communication | by electrical maintenance personnel. Check I/O modules and connection to controls |
| 10402 | `F: Eltromatic AG2: Run up error phase 0` | possible defect requiring maintenance. Error during self test of I/O module | by electrical maintenance personnel. Check modules and connection to controls |
| 10403 | `F: Eltromatic AG2: Run up error phase 1` | possible defect. General error during identification phase | by electrical maintenance personnel. Check I/O modules and connection to controls |
| 10404 | `F: ELTROMATIC AG2: Run up error service channel error` | possible defect. General error during I/O module parameterization | by electrical maintenance personnel. Check I/O modules and connection to controls |
| 10405 | `F: Eltromatic AG2: error in cyclical operation` | possible defect. General error during cyclical communication | by electrical maintenance personnel. Check I/O modules and connection to controls |
| 10406 | `F: Eltromatic AG2: no I/O module identified` | possible defect. No I/O module could be identified | by electrical maintenance personnel. Check I/O modules and connection to controls |
| 10407 | `F: Eltromatic AG2: non supported module found` | possible defect. An unknown I/O module was used | by electrical maintenance personnel. Check I/O modules and connection to controls |
| 10408 | `F: Eltromatic AG2: Equipment and parameter do not agree` | possible defect or incorrect setting, parameter. The Eltromatic equipment determined during machine run up does not correspond to the information in the attendant parameters. | by electrical maintenance personnel. Check I/O modules and parameters or match to each other. |
| 10409 | `F: Eltromatic AG2: Faulty earth connection to module <Module No.>` | possible defect. Faulty earth connection to stated Eltromatic module. | by electrical maintenance personnel. Check the earth connections to the stated Eltromatic module. |
| 10410 | `F: Eltromatic AG2: General fault on module <Module No.>` | possible defect. General fault on stated Eltromatic module. | by electrical maintenance personnel. Check the stated Eltromatic module and its connections. |
| 10450 | `F: Main drive gearing AG2: inadmissible command` | operation inadmissible in machine status. The command is inadmissible in this machine status. | by operator or electrical maintenance personnel. Change machine status and repeat command |
| 10451 | `F: Main drive gearing AG2: faulty coupling` | possible defect. Required position not reached | by operator or electrical maintenance personnel. Repeat command, check gearing access |
| 10452 | `M: Main drive gearing AG2: position undefined` | inadmissible operation in machine status. The gearing is not in any of the permissible positions | by operator. Trigger the command for an admissible position |
| 10453 | `F: Main drive gearing AG2: monitoring triggered` | possible defect. The required position was left without reason | by operator or electrical maintenance personnel. Repeat command, check gearing access |
| 10500 | `F: Handling AG2: inadmissible command` | operation inadmissible in the machine status or incorrect setting, parameter. Inadmissible as the handling condition is incorrect. | by operator or by technologist, setter. acknowledge fault, switch handling system to correct condition. |
| 10501 | `M: Handling AG2: machine switched off by handling system` | Machine switched off from gantry (no unmachined parts) | by technologist, setter. Fill with unmachined parts, switch on gantry and machine again |
| 10502 | `F: Handling system AG2: not operational` | operation inadmissible in machine status. Handling system not switched on or operational | by operator or technologist, setter. Check handling |
| 10503 | `W: Handling AG2: not operational` | inadmissible operation in the machine status. Handling not switched on or operational | by operator or technologist, setter. check handling system |
| 10504 | `M: Handling AG2: Handling is not in automatic operation` | inadmissible operation in the machine status. Handling not in automatic operation whilst processing the handling commands. | by operator. Set handling to automatic and, if necessary, actuate lathe cycle start. |
| 10550 | `M: main chuck AG2: disengagement stroke selected` | inadmissible operation in the machine status. Select "operation" disengagement stroke in parameter | by operator. Correct parameter, see 9.1.2, "Parameter list" on page 9-6. |
| 10551 | `F: Main chuck AG2: pressure failure` | incorrect setting, parameter. "Clamping pressure" pressure switches signalled pressure failure | by technologist, setter. Check the corresponding pressure switch dependent upon the clamping direction |
| 10552 | `F: Main chuck AG2: faulty input signals` | incorrect setting, parameter. Incorrect signals encountered when clamping or releasing | by technologist, setter. Check setting (useful stroke, chucking pressure) |
| 10553 | `F: Main chuck AG2: rear stroke limitation faulty` | possible defect. Faulty proximity switch test "rear proximity switch" | by electrical maintenance personnel. Change proximity switch or check signal path in accordance with circuit diagram |
| 10554 | `M: Main chuck AG2: chucking stroke limit reached` | incorrect setting, parameter. Main chuck has reached end position without pressure | by technologist, setter. Check setting (useful stroke, chucking pressure) |
| 10555 | `F: Main chuck AG2: front stroke limitation faulty` | possible defect. Faulty proximity switch test "Front proximity switch" | by electrical maintenance personnel. Change proximity switch or check signal path in accordance with circuit diagram |
| 10556 | `F: Main chuck AG2: spindle running time without chucking pressure exceeded` | inadmissible operation in machine status. Spindle ran with chuck open for more than 90 seconds | by operator. Acknowledge fault, if necessary correct part program |
| 10557 | `M: main chuck AG2: position not defined` | inadmissible operation in machine status. Main chuck neither clamped nor fully open | by operator. "Clamp" or "release" command (M command or foot switch) |
| 10558 | `F: Main chuck AG2: monitoring triggered` | incorrect setting, parameter. End position "clamped" or "opened" left without reason | by technologist, setter. Check setting (useful stroke, chucking pressure) |
| 10559 | `F: Main chuck AG2: Maximum performance time exceeded` | possible defect. "Clamping without stroke limitation" module could not conclude the clamping or release cycle within 20 sec. | by technologist, setter. Main chuck settings (pressure switch, pressure reduction), check the valves |
| 10600 | `M: main chuck AG2: in change position` | inadmissible operation in machine status. Chuck key is not inserted in the operating panel | by operator. Insert chuck key in release switch on operating panel |
| 10601 | `F: Main chuck AG2: maximum rotational speed exceeded` | incorrect setting, parameter. Rotational speed was higher than the set chuck rotational speed. | by technologist, setter. Check set rotational speed, if necessary correct part program |
| 10602 | `F: Main chuck AG2: inadmissible command` | inadmissible command in machine status. Inadmissible chuck command with spindle turning | by operator. Stop spindle in advance |
| 10650 | `F: Main spindle drive AG2: motor over temperature` | inadmissible operation in machine status. Motor overloaded | by electrical maintenance personnel. Check the operator [sic] and allow the motor to cool down |
| 10651 | `W: Main spindle drive AG2: motor over temperature` | inadmissible operation in the machine status. Motor overloaded | by electrical maintenance personnel. Check chopper and permit the motor to cool down |
| 10652 | `F: Main spindle drive AG2: not operational` | possible defect. Main spindle drive interrupted | by electrical maintenance personnel. Overcome fault, refer to electrical service handbook |
| 10653 | `F: Main spindle drive AG2: auxiliary ventilator malfunction` | possible defect. Motor protection switch triggered | by electrical maintenance personnel. Check electricity setting |
| 10654 | `W: Main spindle drive AG2: auxiliary ventilator malfunction` | possible defect. Motor protection switch triggered | by electrical maintenance personnel. Check current setting |
| 10655 | `F: Main spindle drive AG2: temperature pre-warning` | incorrect setting, parameter possible defect or overload in cycle. The main spindle drive signals over temperature | by electrical maintenance personnel. Allow motor to cool down, check motor, check actuator |
| 10656 | `W: Main spindle drive AG2: temperature pre-warning` | incorrect setting, parameter, possible defect or overload in cycle. The main spindle drive signals over temperature | by electrical maintenance personnel. Allow motor to cool down, check motor, check actuator |
| 10700 | `F: Main spindle clamping AG2: faulty design` | incorrect setting, parameter. Incorrect proximity switch signals of spindle clamping | by technologist, setter. Check proximity switches |
| 10701 | `F: Main spindle clamping AG2: inadmissible command` | inadmissible operation in the machine status. Main spindle is not in the position window | by operator. Perform spindle positioning in advance with the C axis |
| 10702 | `F: Main spindle clamping AG2: monitoring triggered` | incorrect setting, parameter. The selected status has been left without reason | by technologist, setter. Check proximity switches |
| 10703 | `M: Main spindle clamping AG2: main spindle is clamped` | main spindle is clamped. The main spindle is manually clamped | by operator. Release the main spindle clamping |
| 10750 | `F: Hydraulic AG2: filter blocked` | maintenance is necessary. Excessive differential pressure upstream/downstream from the hydraulic filter | by mechanical maintenance personnel. Change hydraulic filter |
| 10751 | `W: Hydraulic AG2: filter blocked` | maintenance is necessary. Excessive differential pressure upstream / downstream from the hydraulic filter | ("Overcoming:") by mechanical maintenance personnel. Change hydraulic filter |
| 10752 | `F: Hydraulic AG2: no system pressure` | maintenance is necessary. Pressure loss or pressure failure in hydraulics | by mechanical maintenance personnel. Check oil level, hydraulics for leaks |
| 10753 | `W: Hydraulic AG2: minimum level reached` | maintenance is necessary. Oil level in hydraulic tank too low | by mechanical maintenance personnel. Check hydraulic system for leaks, top up oil |
| 10754 | `F: Hydraulic AG2: minimum level reached` | maintenance is necessary. Oil level in hydraulic tank too low | by mechanical maintenance personnel. Check hydraulic system for leaks, top up oil |
| 10755 | `F: Hydraulic AG2: motor overcurrent` | possible defect. Motor protection switch triggered | by electrical maintenance personnel. Check electricity setting |
| 10756 | `W: Hydraulic AG2: oil over temperature` | possible defect. Excessive hydraulic unit temperature | by mechanical maintenance personnel. Check hydraulic unit (oil level, others) |
| 10757 | `F: Hydraulic AG2: oil over temperature` | possible defect. Excessive hydraulic unit temperature | by mechanical maintenance personnel. Check hydraulic unit (oil level, others) |
| 10800 | `F: Coolant AG2: motor overcurrent` | possible defect. Motor protection switch(es) has/have been triggered | By electrical maintenance personnel. Check electricity setting |
| 10801 | `F: Coolant AG2: flow monitoring triggered` | possible defect or maintenance necessary. Coolant supply is damaged | by electrical or mechanical maintenance personnel. Check coolant supply |
| 10850 | `F: Loading flap AG2: inadmissible command` | Inadmissible operation in machine status. Protection hood is opened or gripper is in working space | by operator. Close protection hood and remove gripper from working space |
| 10851 | `M: loading flap AG2: position not defined` | inadmissible operation in the machine status. Loading flap is neither closed nor open | by operator. Trigger "open" or "close" command |
| 10852 | `F: Loading flap AG2: monitoring triggered` | incorrect setting, parameter. The selected status has been left without reason | by technologist, setter. Check loading flap limit switch |
| 10900 | `F: Steady rests AG2: inadmissible command` | inadmissible operation in the machine status. The spindle is switched on, inadmissible centre sleeve position | by operator. Stop spindle or clamp centre sleeve |
| 10901 | `M: steady rest 1 AG2: undefined position` | inadmissible operation in the machine status. Steady rest 1 is neither closed nor open | by operator. "Clamp" or "release" command (M command or key) |
| 10902 | `F: Steady rest 1 AG2: monitoring triggered` | incorrect setting, parameter. Clamped or open condition has been left | by technologist, setter. Pressure switch "opened" or closed" |
| 10903 | `M: Steady rest 2 AG2: position undefined` | operation inadmissible in the machine status. Steady rest 2 is neither clamped nor fully open | by operator. "Clamp" or "release" command (M command or key) |
| 10904 | `F: Steady rest 2 AG2: monitoring triggered` | incorrect setting, parameter. Closed or open condition left | by technologist, setter. Check "open" or "close" pressure switch |
| 10950 | `F: NC AG2: not operational` | possible defect. Slaving fault overflow compound slide rest / C axis | by electrical maintenance personnel. Feed - drives, check compound slide rest generator |
| 10951 | `F: NC AG2: maximum NC reaction time exceeded` | possible defect. The NC reaction time to the PLC signal is greater than 300 ms | By operator, if necessary Gildemeister Service. Acknowledge fault, if necessary switch machine on and off. Please notify Gildemeister Service |
| 11000 | `F: Parameter AG2: take over only during machine acceleration` | incorrect setting, parameter. Switch off machine when parameters have changed | by technologist, setter. Change parameter, switch off machine and switch on again |
| 11001 | `F: Pick-off device type parameter AG2: value inadmissible` | incorrect setting, parameter. Incorrect "handling system and material feeder" parameter | by technologist, setter. Correct parameter, see 9.1.2, "Parameter list" on page 9-6 |
| 11002 | `F: Main drive gearing type parameter AG2: inadmissible value` | incorrect setting, parameter. Incorrect value in parameter "Main drive gearing data" | by technologist, setter. Correct parameter, see 9.1.2, "Parameter list" on page 9-6 |
| 11003 | `F: Handling type parameter AG2: inadmissible type` | incorrect setting, parameter. Incorrect parameter "handling system and material feeder" | by technologist, setter. Correct parameter, see 9.1.2, "Parameter list" on page 9-6 |
| 11004 | `F: Main chuck parameter AG2: inadmissible working direction` | incorrect setting, parameter. Incorrect value in "main spindle" parameter | by technologist, setter. Correct parameter, see 9.1.2, "Parameter list" on page 9-6 |
| 11005 | `F: main chuck parameter AG2: inadmissible chucking mode` | incorrect setting, parameter. Incorrect value in "Main spindle" parameter | by technologist, setter. Correct parameter, see 9.1.2, "Parameter list" on page 9-6 |
| 11006 | `F: Main chuck parameter AG2: inadmissible type` | incorrect setting, parameter. incorrect main chuck type in "Main spindle" parameter | by technologist, setter. Correct parameter, see 9.1.2, "Parameter list" on page 9-6 |
| 11007 | `F: Number of steady rests parameter AG2: value too large` | incorrect setting, parameter. Excessive value in "Steady rest description" parameter | by technologist, setter. Correct parameter |
| 11008 | `F: Material feeding type parameter AG2: inadmissible value` | incorrect setting, parameter. Incorrect "Handling system and material feeder" parameter | by technologist, setter. Correct parameter, see 9.1.2, "Parameter list" on page 9-6 |
| 11009 | `F: Centre sleeve parameter AG2: inadmissible type` | incorrect setting, parameter. Incorrect centre sleeve selected in "Tailstock, centre sleeve" | by technologist, setter. Correct parameter |
| 11010 | `F: Tailstock type parameter AG2: inadmissible value` | incorrect setting, parameter. Value in "Tailstock, centre sleeve" parameter incorrectly set | by technologist, setter. Correct parameter, see 9.1.2, "Parameter list" on page 9-6 |
| 11011 | `F: Parameter swivel chuck type AG2: inadmissible value` ==> not used | (not used) | (not used) |
| 11012 | `F: Tool holder parameter AG2: inadmissible type` | incorrect setting, parameter. Incorrect tool holder type selected in "Toolholder data" parameter | by technologist, setter. Correct parameter, see 9.1.2, "Parameter list" on page 9-6 |
| 11013 | `F: Tool holder parameter AG2: inadmissible number of tools` | incorrect setting, parameter. Parameter "holder number and spindle" incorrectly set | by technologist, setter. Correct parameter, see 9.1.2, "Parameter list" on page 9-6 |
| 11014 | `F: Parameter ISM temperature control AG2: value "number of values for mean value formation" inadmissible` | incorrect setting, parameter. Value in "Temperature control spindles" parameter incorrect | by technologist, setter. Correct parameter |
| 11015 | `F: Parameter ISM temperature control AG2: value "Hysteresis cooling water temperature" inadmissible` | incorrect setting, parameter. Value in "Temperature control spindles" parameter incorrect | by technologist, setter. Correct parameter |
| 11016 | `F: Parameter ISM temperature control AG2: value "Hysteresis rear bearing temperature" inadmissible` | incorrect setting, parameter. Value in "Temperature control spindles" parameter incorrect | by technologist, setter. Correct parameter |
| 11017 | `F: Parameter ISM temperature control AG2: value "Hysteresis front bearing temperature" inadmissible` | incorrect setting, parameter. Value in "Temperature control spindles" parameter incorrect | by technologist, setter. Correct parameter |
| 11018 | `F: Parameter ISM temperature control AG2: value "Hysteresis winding temperature" inadmissible` | incorrect setting, parameter. Value in "Temperature control spindles" parameter incorrect | by technologist, setter. Correct parameter |
| 11019 | `F: Parameter ISM temperature control AG2: value "MAX rear bearing temperature" inadmissible` | incorrect setting, parameter. Value in "Temperature control spindles" parameter incorrect | by technologist, setter. Correct parameter |
| 11020 | `F: Parameter ISM temperature control AG2: value "MAX front bearing temperature" inadmissible` | incorrect setting, parameter. Value in "Temperature control spindles" parameter incorrect | by technologist, setter. Correct parameter |
| 11021 | `F: Parameter ISM temperature control AG2: value "MAX ambient temperature" inadmissible` | incorrect setting, parameter. Value in "Temperature control spindles" parameter incorrect | by technologist, setter. Correct parameter |
| 11022 | `F: Parameter ISM temperature control AG2: value "MAX winding temperature" inadmissible` | incorrect setting, parameter. Value in "Temperature control spindles" parameter incorrect | by technologist, setter. Correct parameter |
| 11023 | `F: Parameter ISM temperature control AG2: value "MAXMAX rear bearing temperature" inadmissible` | incorrect setting, parameter. Value in "Temperature control spindles" parameter incorrect | by technologist, setter. Correct parameter |
| 11024 | `F: Parameter ISM temperature control AG2: value "MAXMAX front bearing temperature" inadmissible` | incorrect setting, parameter. Value in "Temperature control spindles" parameter incorrect | by technologist, setter. Correct parameter |
| 11025 | `F: Parameter ISM temperature control AG2: value "MAXMAX winding temperature" inadmissible` | incorrect setting, parameter. Value in "Temperature control spindles" parameter incorrect | by technologist, setter. Correct parameter |
| 11026 | `F: Parameter ISM temperature control AG2: value "Set point value cooling water temperature" inadmissible` | incorrect setting, parameter. Value in "Temperature control spindles" parameter incorrect | by technologist, setter. Correct parameter |
| 11027 | `F: Parameter ISM temperature control AG2: value "Set point value rear bearing temperature" inadmissible` | incorrect setting, parameter. Value in "Temperature control spindles" parameter incorrect | by technologist, setter. Correct parameter |
| 11028 | `F: Parameter ISM temperature control AG2: value "Set point value front bearing temperature" inadmissible` | incorrect setting, parameter. Value in "Temperature control spindles" parameter incorrect | by technologist, setter. Correct parameter |
| 11029 | `F: Parameter ISM temperature control AG2: value "Set point value winding temperature" inadmissible` | incorrect setting, parameter. Value in "Temperature control spindles" parameter incorrect | by technologist, setter. Correct parameter |
| 11030 | `F: Parameter lubrication AG2: Value "lubrication interval time" inadmissible` | incorrect setting, parameter. Value in "lubrication time interval" parameter incorrect | by technologist, setter. Correct parameter |
| 11031 | `F: Parameter lubrication AG2: Value "lubrication suppression time" inadmissible` | incorrect setting, parameter. Value in "lubrication suppression time" parameter incorrect | by technologist, setter. Correct parameter |
| 11100 | `F: Centre sleeve AG2: pressure failure` | incorrect setting, parameter. Centre sleeve clamped and pressure failure | by technologist, setter. Check centre sleeve pressure switch setting |
| 11101 | `M: Centre sleeve AG2: front end position reached` | incorrect setting, parameter. End position reached when clamping without pressure | by technologist, setter. Insufficient centre sleeve stroke, traverse tailstock |
| 11102 | `F: Centre sleeve AG2: incorrect input signals` | Incorrect setting, parameter. Incorrect signals encountered when traversing centre sleeve | ("Overcoming:") by technologist, setter. Check centre sleeve setting (proximity switches, pressure switches) |
| 11103 | `F: Centre sleeve AG2: inadmissible command` | inadmissible operation in the machine status. The spindle is switched on, no steady rest clamped | by operator. Stop the spindle or clamp a steady rest |
| 11104 | `M: Centre sleeve AG2: undefined setting` | inadmissible status in machine status. Centre sleeve is neither clamped nor fully run in | by operator. "Forward" or "Return" command (M command or foot switch) |
| 11105 | `F: Centre sleeve AG2: monitoring triggered` | incorrect setting, parameter. Leave clamped or run in condition | by technologist, setter. Check centre sleeve setting (proximity switches, pressure switches) |
| 11150 | `F: Tailstock AG2: inadmissible command` | inadmissible operation in machine status. Inadmissible as centre sleeve extended or main spindle turning | by operator. Acknowledge fault, if necessary correct part program |
| 11151 | `F: Tailstock AG2: not operational` | incorrect setting, parameter. Teach-in module of the automatic tailstock | by technologist, setter. Check unit, if necessary reprogram |
| 11152 | `M: tailstock AG2: position not defined` | inadmissible operation in the machine status. The traversable tailstock is in neither of the 2 end positions | by operator. Move using M/command, or `FORWARD` or `REVERSE` key |
| 11153 | `F: Tailstock AG2: monitoring triggered` | incorrect setting, parameter or possible defect. Traversable tailstock limit switch offset or defective | by electrical or mechanical maintenance personnel. Check limit switch, replace if necessary |
| 11200 | `F: Switch cabinet cooling AG2: filter blocked` | maintenance is necessary. Excessive differential pressure upstream/downstream of filter mat | by mechanical maintenance personnel. Clean filter or change |
| 11201 | `W: Switch cabinet cooling AG2: filter blocked` | maintenance is necessary. Excessive differential pressure upstream/downstream of filter mat | by mechanical maintenance personnel. Clean filter or change |
| 11202 | `F: Switch cabinet cooling AG2: overcurrent` | possible defect. Motor protection switch triggered | by technologist, setter. Check current setting |
| 11203 | `W: Switch cabinet cooling AG2: overcurrent` | possible defect. Motor protection switch triggered | by electrical maintenance personnel. Check electricity setting |
| 11204 | `F: Switch cabinet cooling AG2: over temperature` | possible defect. Switch cabinet over temperature | by mechanical maintenance personnel. Check cooling unit, excessive ambient temperature |
| 11205 | `W: Switch cabinet cooling AG2: over temperature` | possible defect. Switch cabinet over temperature | by mechanical maintenance personnel. Check cooling unit, excessive ambient temperature |
| 11250 | `F: Lubrication AG2: no pressure` | maintenance necessary. Lubricant pressure switch signals no pressure | by mechanical maintenance personnel. Top up lubricating oil container, check pressure switch |
| 11251 | `W: Lubrication AG2: no pressure` | maintenance is required. Lubricant pressure switch signals no pressure | by mechanical maintenance personnel. Top up lubricating oil container, check pressure switch |
| 11252 | `W: Lubrication AG2: minimum level reached` | Maintenance is necessary. Fresh oil lubrication level too low | by mechanical maintenance personnel. Top up fresh oil |
| 11253 | `F: Lubrication AG2: minimum level reached` | maintenance is necessary. Fresh oil lubrication level too low | by mechanical maintenance personnel. Top up fresh oil |
| 11254 | `F: Lubrication AG2: motor overcurrent` | possible defect. Motor protection switch triggered | by electrical maintenance personnel. Check electricity setting |
| 11255 | `W: Lubrication AG2: motor overcurrent` | possible defect. Motor protection switch triggered | by electrical maintenance personnel. Check electricity setting |
| 11256 | `M: Lubrication oil AG2: top up oil` | maintenance is necessary. Insufficient fresh oil level | by mechanical maintenance personnel. Top up fresh oil |
| 11257 | `F: Lubrication AG2: permanent pressure` | maintenance is necessary. Lubricant pressure switch signals permanent pressure | by mechanical maintenance personnel. Check lubricant pressure lines and/or pressure switch |
| 11258 | `W: Lubrication AG2: permanent pressure` | maintenance is necessary. Lubricant pressure switch signals permanent pressure | by mechanical maintenance personnel. Check lubricant pressure lines and/or pressure switch |
| 11259 | `F: Lubrication AG2: no pressure circuit 1` | Maintenance is required. Lubrication pressure switch signals no pressure | by mechanical maintenance personnel. Top up lubricating oil container, check pressure switch |
| 11260 | `F: Lubrication AG2: no pressure circuit 2` | maintenance is required. Lubrication pressure switch signals no pressure | by mechanical maintenance personnel. Top up lubricating oil container, check pressure switch |
| 11261 | `F: Lubrication AG2: permanent pressure circuit 1` | maintenance is required. Lubrication pressure switch signals permanent pressure | by mechanical maintenance personnel. Check lubrication pressure lines and/or pressure switch |
| 11262 | `F: Lubrication AG2: permanent pressure circuit 2` | maintenance is required. Lubrication pressure switch signals permanent pressure | by mechanical maintenance personnel. Check lubrication pressure line and/or pressure switch |
| 11263 | `F: Lubrication AG2: command inadmissible` ==> not used | (not used) | (not used) |
| 11264 | `F: Lubrication AG2: maximum suppression time exceeded` | incorrect setting, parameter or programming fault. Maximum suppression time exceeded | by technologist, setter. Check parameter, withdraw lubrication pulse suppression using M49, M18, M30, M99 or cycle stop |
| 11265 | `W: Lubrication AG2: no pressure circuit 1` | maintenance is required. Lubrication pressure switch signals no pressure | by mechanical maintenance personnel. Top up lubricating oil container, check pressure switch |
| 11266 | `W: Lubrication AG2: no pressure circuit 2` | maintenance is required. Lubrication pressure switch signals no pressure | by mechanical maintenance personnel. Top up lubricating oil container, check pressure switch |
| 11267 | `W: Lubrication AG2: permanent pressure circuit 1` | maintenance is required. Lubrication pressure switch signals permanent pressure | by mechanical maintenance personnel. Check lubricating pressure lines, and/or pressure switch |
| 11268 | `W: Lubrication AG2: permanent pressure circuit 2` | maintenance is required. Lubrication pressure switch signals permanent pressure | by mechanical maintenance personnel. Check lubricating pressure lines, and/or pressure switch |
| 11269 | `W: Lubrication AG2: maximum suppression time exceeded` | incorrect setting, parameter or programming fault. Maximum time under pressure exceeded | by technologist, setter. Check parameter, withdraw lubrication pulse suppression using M49, M18, M30, M99 or cycle stop |
| 11300 | `F: Protection hood AG2: max. time to open/close exceeded` | possible defect or incorrect setting, parameter. Possible defect on the automatic protection hood | by electrical or mechanical maintenance personnel. Check function, if necessary correct parameter 031 |
| 11301 | `M: Protection hood AG2: motor overcurrent` | incorrect setting, parameter or possible defect. Motor protection switch triggered | by electrical maintenance personnel. Check electricity setting |
| 11302 | `M: Protection hood AG2: not opened and not closed` | Inadmissible operation in machine status. Protection hood not locked and not opened | by operator. Open protection hood and then close properly |
| 11303 | `M: Protection hood AG2: opening not permitted as button pressed` | inadmissible operation in the machine status. Button actuated when opening the protection hood | by operator. Do not actuate any button, then open protection hood |
| 11304 | `M: Protection hood AG2: safety rail triggered` | inadmissible operation in machine status. Closing of automatic protection hood interrupted | by operator. Acknowledge fault, if necessary check safety rail |
| 11305 | `F: Protection hood AG2: safety circuit malfunction` | possible defect. Protection hood safety circuit has incorrect succession of signals | by electrical maintenance personnel. Check safety circuit (refer to circuit diagram) |
| 11306 | `F: Protection hood AG2: inadmissibly opened` | inadmissible operation in machine status. Protection hood was opened with the emergency release | by operator. Open protection hood and then close |
| 11350 | `F: Chip conveyor AG2: motor overcurrent` | inadmissible operation in the machine status. Motor protection switch triggered | by operator or by electrical maintenance personnel. Run chip conveyor empty, check current setting |
| 11351 | `W: Chip conveyor AG2: motor overcurrent` | inadmissible operation in the machine status. Motor protection switch triggered | by operator or by electrical maintenance personnel. Run conveyor empty, check electricity setting |
| 11400 | `F: Bar operation AG2: inadmissible command` | inadmissible operation in machine status. Command M94 triggered without bar operation running | by operator. Select and switch on bar operation |
| 11401 | `F: Bar operation AG2: not operational` | inadmissible operation in machine status. Bar operation selected, however, not operational | by electrical maintenance personnel. Check bar unit |
| 11402 | `M: Bar operation AG2: bar change` | inadmissible operation in machine status. Bar is machined | by operator. Insert new bar, no bar loading magazine |
| 11450 | `F: Feed drives AG2: motor overcurrent` | possible defect. Motor protection switch triggered | by electrical maintenance personnel. Check electricity setting |
| 11451 | `F: Feed drives AG2: motor over temperature` | incorrect setting, parameter or possible defect. One of the feed drives signals over temperature | by electrical maintenance personnel. Chuck [sic, probably "Check"] motor chopper, allow motor to cool down |
| 11452 | `F: Feed drives AG2: not operational` | possible defect. Servo chopper of the feed drive damaged | by electrical maintenance personnel. See electrical service instructions: chopper, circuit diagrams |
| 11453 | `F: Feed drives AG2: slide X not operational` | possible defect. Chopper for slide X malfunction | by electrical maintenance personnel. Overcome malfunction, refer to electric service handbook |
| 11454 | `F: Feed drives AG2: slide X temperature pre-warning` | incorrect setting, parameter, possible defect or overload in cycle. one of the feed drives signals over-temperature | by electrical maintenance personnel. Permit drive to cool down, check drive, check chopper? |
| 11455 | `W: Feed drives AG2: slide X temperature pre-warning` | incorrect setting, parameter, possible defect or overload in cycle. one of the feed drives signals over-temperature | by electrical maintenance personnel. Permit drive to cool down, check drive, check actuator |
| 11456 | `F: Feed drives AG2: slide Z not operational` | possible defect. Chopper for slide Z malfunction | by electrical maintenance personnel. Overcome malfunction, refer to electric service handbook |
| 11457 | `F: Feed drives AG2: ventilator X axis motor overcurrent` | possible defect. Motor protection switch triggered | by electrical maintenance personnel. Check current setting |
| 11458 | `W: Feed drives AG2: slide Z temperature pre-warning` | incorrect setting, parameter, possible defect or overload in cycle. one of the feed drives signals over-temperature | by electrical maintenance personnel. Permit drive to cool down, check drive, check actuator |
| 11459 | `F: Feed drives AG2: slide Z temperature pre-warning` | incorrect setting, parameter, possible defect or overload in cycle. one of the feed drives signals over-temperature | by electrical maintenance personnel. Permit drive to cool down, check drive, check actuator |
| 11460 | `W: Feed drives AG2: ventilator X axis motor overcurrent` | possible defect. Motor protection switch triggered | by electrical maintenance personnel. Check current setting |
| 11461 | `M: Feed drives AG2: X axis ventilator motor overcurrent` | possible defect. Motor protection switch triggered | by electrical maintenance personnel. Check current setting |
| 11500 | `F: Tool holder AG2: motor overcurrent` | possible defect. Motor protection switch triggered | by electrical maintenance personnel. Check electricity setting |
| 11501 | `W: Tool holder AG2: motor overcurrent` | possible defect. Motor protection switch triggered | by electrical maintenance personnel. Check electricity setting |
| 11502 | `F: Tool holder AG2: motor over temperature` | inadmissible operation in the machine status. Motor temperature monitor triggered | by electrical maintenance personnel. Allow motor to cool down, press reset button on unit |
| 11503 | `W: Tool holder AG2: motor over temperature` | inadmissible operation in machine status. Motor temperature monitor triggered | by electrical maintenance personnel. Allow motor to cool down, press reset button on unit |
| 11504 | `F: Tool holder AG2: not locked in position` | inadmissible operation in machine status. Tool holder locked in incorrect position | by operator. Swivel tool holder with T command |
| 11505 | `F: Tool holder AG2: faulty parity check` | possible defect. Tool holder position generator had a parity error | by operator or electrical maintenance personnel. Swivel tool holder (Tx) position generator, check cable |
| 11506 | `F: Tool holder AG2: position not found` | incorrect setting, parameter or possible defect. Faulty position generator or incorrect value in parameter | by technologist, setter or mechanical maintenance personnel. Check position generator or correct parameter |
| 11507 | `F: Tool holder AG2: T value for tool holder too large` | inadmissible operation in machine status or incorrect setting, parameter. Enter entered value T value > in parameter "Holder number" [sic] | by operator or by technologist, setter. Correct parameter or enter smaller T value |
| 11508 | `F: Tool holder AG2: T value could not be taken over` | inadmissible operation in the machine status. Execution blocked due to machine status | by operator. Correct machine status (e.g. close hood) |
| 11509 | `F: Tool holder AG2: preindexing faulty` | inadmissible operation in the machine status. Faulty preindexation in the tool holder test | by operator. Swivel tool holder with T command |
| 11510 | `F: Tool holder AG2: tool holder does not lock` | Incorrect setting, parameter. Initiator "Control interlocking" does not switch | by technologist, setter. Check initiator, if necessary, readjust and test |
| 11511 | `F: Tool holder AG2: Overload clutch actuated` | possible defect or overload. Overload coupling triggered | by electrical/mechanical maintenance personnel. Find cause of fault and reset the tool holder |
| 11512 | `F: Tool holder AG2: Tool holder does not release` | incorrect setting, parameter or possible defect. The tool holder does not release during the time allowed | by electrical/mechanical maintenance personnel. Find cause of fault and reset the tool holder, if necessary |
| 11513 | `F: Tool holder AG2: Incorrect tool holder reference traverse` | incorrect setting, parameter or possible defect. The tool holder does not perform the reference movement during the time allowed | by electrical/mechanical maintenance personnel. Find cause of fault and reset the tool holder, if necessary |
| 11514 | `F: Tool holder AG2: Internal tool holder fault` | possible defect. Tool holder does not clearly signal "disc disengaged" or "spindle disengaged" | by electrical/mechanical maintenance personnel. Find cause of fault and reset the tool holder, if necessary |
| 11550 | `F: Output modules AG2: control voltage failure` | possible defect. Automatic safety cut-out triggered | by electrical maintenance personnel. Overcome the cause of the overload |
| 11551 | `F: Input modules AG2: control voltage failure` | Possible defect. Automatic safety cut-out triggered | by electrical maintenance personnel. Overcome cause of fault |
| 11552 | `F: Machine AG2: not switched on` | inadmissible operation in the machine status. Drive not switched on on operating panel | by operator. Actuate `Drive on` button, lamp in button must have lit |
| 11553 | `F: Machine AG2: Emergency stop` | inadmissible operation in the machine status. Press Emergency stop button | by operator. Release emergency stop button |
| 11554 | `F: Machine malfunction AG2: see fault list` | possible defect. Only with certain options, refer to circuit diagram I 04.1 | by electrical maintenance personnel. Overcome fault |
| 11555 | `F: Valves AG2: control voltage failure` | Possible defect. Automatic safety cut-out triggered | by electrical maintenance personnel. Overcome cause of the overload |
| 11556 | `F: Machine warning AG2: refer to fault list` | possible defect. Only for certain options, refer to circuit diagram I 04.2 | by electrical maintenance personnel. Overcome fault |
| 11557 | `W: Machine warning AG2: refer to fault list` | possible defect. Only for certain options, refer to circuit diagram I 04.2 | by electrical maintenance personnel. Overcome fault |
| 11600 | `F: Insert aid AG2: inadmissible command` ==> not used | (not used) | (not used) |
| 11601 | `M: Feeding aid AG2: position not defined` | inadmissible operation in machine status. Feeding aid is not an [sic, "in an"] in end position | by operator. Move feeding aid to required end position with M command |
| 11602 | `F: Feeding aid AG2: monitoring triggered` | incorrect setting, parameter or possible defect. Feeding aid has left end position without a command | by electrical or mechanical maintenance personnel. Feeding aid (check hydraulics and limit switch) |
| 11650 | `M: Swivel chuck AG2: inadmissible changes to the selection` ==> not used | (not used) | (not used) |
| 11651 | `F: Swivel chuck AG2: inadmissible command` | inadmissible operation in machine status. Spindle r.p.m too high or hardware swivel permission not present | by operator. Repeat procedure at lower main spindle r.p.m, if necessary, check parameter for maximum rotational speed swivelling |
| 11652 | `M: Swivel chuck AG2: position not defined` | swivel command aborted. The swivel chuck is in an undefined intermediate position | by operator. Swivel swivel chuck to any position |
| 11653 | `F: Swivel chuck AG2: monitoring triggered` | possible defect. The swivel chuck has left the current condition without command | by technologist, setter. Check swivel chuck setting (proximity switch, pressure switch) |
| 11654 | `F: Swivel chuck AG2: target position not found` | possible defect. The swivel chuck has not found the programmed position | by technologist, setter. Check swivel chuck settings of proximity switches |
| 11655 | `F: Swivel chuck AG2: maximum swivel time exceeded` | possible defect. The swivel chuck could not conclude the swivel movement within 20 seconds | by technologist, setter. Swivel chuck settings (pressure switch, pressure reducer), valves |
| 11700 | `M: Pick-off device gripping AG2: position not defined` | Operation inadmissible in the machine status. Gripper is not in either of the end positions | by operator. "Grip" or "Release" command (M command or button) |
| 11701 | `F: Pick-off device gripping AG2: monitoring triggered` | incorrect setting, parameter. The selected status has been left without reason | by technologist, setter. Check "gripper" proximity switch |
| 11702 | `M: Pick-off device flap AG2: position not defined` | inadmissible operation in the machine status. Flap is not in either of the end positions | by operator. "Open" or "Close" command (M command or button) |
| 11703 | `F: Flap pick-off device AG2: monitoring triggered` | incorrect setting, parameter. The selected status has been left without reason | by technologist, setter. Check "Flap" proximity switch |
| 11704 | `M: Swivel pick-off device AG2: position not defined` | inadmissible operation in the machine status. Swivel arm is not in either of the end positions | by operator. "Forward" or "Return" command (M command or button) |
| 11705 | `F: Swivel pick-off device AG2: Monitoring triggered` | incorrect setting, parameter. The selected status has been left without reason | by technologist, setter. Check "Swivel" proximity switch |
| 11706 | `F: Pick-off device AG2: inadmissible command` | inadmissible operation in the machine status. The command is not admissible in this machine status | by operator or electrical maintenance personnel. Change machine status and repeat command |
| 11707 | `F: Pick-off device AG2: maximum execution time exceeded` | incorrect setting, parameter. Pick-off command not performed in the prescribed time | by technologist, setter. Check pick-off device |
| 11708 | `F: Pick-off device conveyor belt AG2: faulty execution` | possible defect. Workpiece did not enter the transport channel within the maximum time specified in the parameter | by technologist, setter. Check pick-off device / parameter |
| 11709 | `F: Pick-off device conveyor belt AG2: motor overcurrent` | possible defect. Motor protection switch triggered | by electrical maintenance personnel. Check current setting |
| 11710 | `W: Pick-off device conveyor belt AG2: motor overcurrent` | possible defect. Motor protection switch triggered | by electrical maintenance personnel. Check current setting |
| 11711 | `M: Pick-off device conveyor belt AG2: workpiece in transport channel` | possible defect. The workpiece is in conveyor channel with the conveyor belt switched off | by operator. By switching on the conveyor belt the workpiece is moved out of the conveyor channel |
| 11750 | `F: Driven tools AG2: inadmissible command` | inadmissible operation in machine status. Protection hood opened. Driven tool spindle release missing or the tool holder was not in a driven tool position | by operator, technologist, setter or electrical maintenance personnel. Swing in to driven tool position or find cause of missing spindle release |
| 11751 | `F: Driven tools AG2: faulty coupling procedure` | incorrect setting, parameter. Faulty coupling procedure with M203 / M204 / M205 | by technologist, setter. Check proximity switch engaged/disengaged |
| 11752 | `F: Driven tools AG2: coupling monitoring triggered` | incorrect setting, parameter. The selected status was left without reason | by technologist, setter. Check proximity switch engaged/disengaged |
| 11753 | `F: Driven tools AG2: not operational` | possible defect. The driven tool actuator is damaged | by electrical maintenance personnel. Refer to electrical service handbook to overcome malfunction |
| 11754 | `F: Driven tools AG2: Temperature pre-warning` | Incorrect setting, parameter or possible defect or overload in cycle. The driven tool actuator signals over-temperature | by electrical maintenance personnel. Permit drive to cool, check drive, check actuator |
| 11755 | `F: Driven tools AG2: Over temperature` | Incorrect setting, parameter or possible defect or overload in cycle. The driven tool actuator signals over-temperature | by electrical maintenance personnel. Permit drive to cool, check drive, check actuator |
| 11755 [sic, number printed twice; probably 11756] | `W: Driven tools AG2: Excess temperature` | incorrect setting, parameter or possible defect or overload in cycle. The driven tool actuator signals over-temperature | by electrical maintenance personnel. Permit drive to cool down, check drive, check actuator |
| 11800 | `F: C axis main spindle AG2: monitoring triggered` | incorrect setting, parameter or possible defect. C axis pressure switch and limit switch offset or defective | by electrical or mechanical maintenance personnel. Check pressure switch and limit switch, replace if necessary |
| 11850 | `F: Rotational speed monitoring AG2: unit damaged or triggered` | inadmissible operation in machine status or possible defect. Spindle turns > 50 r.p.m in setting operation, fault speed control unit | by operator or electrical maintenance personnel. Check spindle < 50 r.p.m, check speed control unit. |
| 11900 | `F: AG2: Refer to PLC error list` ==> not used | (not used) | (not used) |
| 11901 | `M: AG2: Refer to PLC error list` ==> not used | (not used) | (not used) |
| 11902 | `W: AG2: Refer to PLC error list` ==> not used | (not used) | (not used) |
| 11950 | `F: Working space dividing wall AG2: inadmissible command` | inadmissible operation in machine status. Protection hoods are open | by operator. Close protection hoods |
| 11951 | `M: Working space dividing wall AG2: position not defined` | inadmissible operation in machine status. Working space dividing wall not in any end position | by operator. "Close" or "open" command (M command [closing bracket missing in print] |
| 11952 | `F: Working space dividing wall AG2: monitoring triggered` | incorrect setting, limit switch defective. Selected status left without reason | by technologist, setter. Check hydraulic system and limit switches |
| 11953 | `F: Working space dividing wall AG2: maximum performance time exceeded` | incorrect setting, limit switch defective. Command not performed during the time allowed | by technologist, setter. Check hydraulic system and limit switches |
| 12000 | `F: ISM cooling AG2: ventilator heat exchanger motor overcurrent` | possible defect. Motor protection switch triggered | by electrical maintenance personnel. Check current setting |
| 12001 | `W: ISM cooling AG2: ventilator heat exchanger motor overcurrent` | possible defect. Motor protection switch triggered | by electrical maintenance personnel. Check current setting |
| 12002 | `F: ISM cooling AG2: pump motor overcurrent` | possible defect. Motor protection switch triggered | by electrical maintenance personnel. Check current setting |
| 12003 | `W: ISM cooling AG2: pump motor overcurrent` | possible defect. Motor protection switch triggered | by electrical maintenance personnel. Check current setting |
| 12050 | `F: Switch cabinet AG2: over-temperature` | possible defect. Switch cabinet over-temperature | by mechanical maintenance personnel. Check cooling unit, excessive ambient temperature? |
| 12051 | `W: Switch cabinet AG2: over temperature` | possible defect. Switch cabinet over temperature | by mechanical maintenance personnel. Check cooling unit, excessive ambient temperature? |
| 12100 | `F: ISM temperature control AG2: AEM module not operational` | possible defect. The AEM module is not operational | by electrical maintenance personnel. Check AEM module |
| 12101 | `W: ISM temperature control AG2: AEM module not operational` | possible defect. The AEM module is not operational | by electrical maintenance personnel. Check AEM module |
| 12102 | `F: ISM temperature control AG2: max. rear bearing temperature exceeded` | possible defect or incorrect setting, max. rear bearing temperature parameter exceeded | by electrical/mechanical maintenance personnel. Permit bearing to cool, check bearing, check parameter |
| 12103 | `W: ISM temperature control AG2: max. rear bearing temperature exceeded` | possible defect or incorrect setting, max. rear bearing temperature parameter exceeded | by electrical/mechanical maintenance personnel. Permit bearing to cool, check bearing, check parameter |
| 12104 | `F: ISM temperature control AG2: max. front bearing temperature exceeded` | possible defect or incorrect setting, max. front bearing temperature parameter exceeded | by electrical/mechanical maintenance personnel. Permit bearing to cool, check bearing, check parameter |
| 12105 | `W: ISM temperature control AG2: max. front bearing temperature exceeded` | possible defect or incorrect setting, max. front bearing temperature parameter exceeded | by electrical/mechanical maintenance personnel. Permit bearing to cool, check bearing, check parameter |
| 12106 | `F: ISM temperature control AG2: max. motor winding temperature exceeded` | possible defect or incorrect setting, parameter or overload in cycle. Max. motor winding temperature exceeded | by electrical/mechanical maintenance personnel. Permit motor to cool, check motor, check parameter |
| 12107 | `W: ISM temperature control AG2: max. motor winding temperature exceeded` | possible defect or incorrect setting, parameter or overload in cycle. Max. motor winding temperature exceeded | by electrical/mechanical maintenance personnel. Permit motor to cool, check motor, check parameter |
| 12108 | `M: ISM temperature control AG2: max. ambient temperature exceeded` | possible defect or incorrect setting, parameter. Max. ambient temperature exceeded | by electrical/mechanical maintenance personnel. Check temperature sensor, check parameter |
| 12109 | `F: ISM temperature control AG2: AEM module does not react` | possible defect or incorrect setting, parameter. AEM module does not correctly react to measurement requirement | by electrical maintenance personnel. Check AEM module, check temperature sensor, check parameter |
| 12110 | `M: ISM temperature control AG2: AEM module does not react` | possible defect or incorrect setting, parameter. AEM module does not correctly react to measurement requirement | by electrical maintenance personnel. Check AEM module, check temperature sensor, check parameter |
| 12111 | `W: ISM temperature control AG2: AEM module does not react` | possible defect or incorrect setting, parameter. AEM module does not correctly react to measurement requirement | by electrical maintenance personnel. Check AEM module, check temperature sensor, check parameter |
| 12112 | `F: ISM temperature control AG2: maxmax rear bearing temperature exceeded` | possible defect or incorrect setting, parameter Maxmax rear bearing temperature exceeded | by electrical/mechanical maintenance personnel. Permit bearing to cool, check bearing, check parameter |
| 12113 | `F: ISM temperature control AG2: maxmax front bearing temperature exceeded` | possible defect or incorrect setting, parameter Maxmax front bearing temperature exceeded | by electrical/mechanical maintenance personnel. Permit bearing to cool, check bearing, check parameter |
| 12114 | `F: ISM temperature control AG2: maxmax motor winding temperature exceeded` | possible defect or incorrect setting, parameter, or overload in cycle. Maxmax motor winding temperature exceeded | by electrical/mechanical maintenance personnel. Permit motor to cool, check motor, check parameter |
| 12150 | `F: Supply AG2: not operational` | possible defect. Supply is disturbed | by electrical maintenance personnel. Overcome fault refer to electrical service handbook |
| 12151 | `F: Supply AG2: temperature prewarning` | possible defect or incorrect setting, parameter, or overload in cycle. Supply signals over temperature | by electrical maintenance personnel. Check supply |
| 12152 | `W: Supply AG2: temperature prewarning` | possible defect or incorrect setting, parameter, or overload in cycle. Supply signals over temperature | by electrical maintenance personnel. Check supply |
| 12250 | `F: PLC AG2: Internal software error` | internal PLC fault | by setter, if necessary Gildemeister service. Check PLC parameter setting and switch machine off and on again, please notify Gildemeister service |

Page placement: 9-119: 10300-10401; 9-120: 10402-10407; 9-121: 10408-10452; 9-122: 10453-10504; 9-123: 10550-10555; 9-124: 10556-10601; 9-125: 10602-10654; 9-126: 10655-10703; 9-127: 10750-10755; 9-128: 10756-10851; 9-129: 10852-10904; 9-130: 10950-11003; 9-131: 11004-11009; 9-132: 11010-11016; 9-133: 11017-11022; 9-134: 11023-11028; 9-135: 11029-11102; 9-136: 11103-11152; 9-137: 11153-11204; 9-138: 11205-11254; 9-139: 11255-11260; 9-140: 11261-11267; 9-141: 11268-11303; 9-142 (printed "9-140"): 11304-11400; 9-143: 11401-11453; 9-144: 11454-11459; 9-145: 11460-11503; 9-146: 11504-11509; 9-147: 11510-11550; 9-148: 11551-11556; 9-149: 11557-11653; 9-150: 11654-11703; 9-151: 11704-11709; 9-152: 11710-11753; 9-153: 11754-11902; 9-154: 11950-12002; 9-155: 12003-12102; 9-156: 12103-12108; 9-157: 12109-12114; 9-158: 12150-12250.

## 9.4 External events

(Page 9-159.)

It is possible to react to external events with the event interrogation in the part program (signals from the machine, periphery etc.). In this manner an individual part program sequence is possible. The interrogation is made with IF, WHILE interrogations (refer to programming handbook).

Example:

```
IF        IF {E1 [1] = = 1}
THEN
:
:         if the event is "1"
:
ELSE
:
:
:         if the event is "0"
:
ENDIF
```

- `E1` = Event list from slide 1
- `E2` = Event list from slide 2
- `[1]` = Event 1 from the list
- :
- `[16]` = event 16 from the list
- `= = 1` = interrogation from "1"

### 9.4.1 Assignment of the event flag

(Page 9-160.) X = event available on that machine type.

**Slide 1 AG1**

| Branch | Assignment slide 1 AG1 | CTX 200 | CTX 400 | CTX 500 | CTX 600 | CTX twin |
|---|---|---|---|---|---|---|
| 1 | Material feeding = bar end reached | X | X | X | X | X |
| 7 | Tool monitoring | | X | X | X | |
| 9 | Handling input X on high signal | | X | X | X | X |
| 10 | Handling input Y on high signal | | X | X | X | X |
| 12 | Chuck condition of spindle H = "Clamped" | | | | | X |

**Slide 2 AG2**

| Branch | Assignment slide 2 AG2 | CTX 200 | CTX 400 | CTX 500 | CTX 600 | CTX twin |
|---|---|---|---|---|---|---|
| 1 | Material feeding = bar end reached | | | | | X |
| 7 | Tool monitoring | | | | | |
| 9 | Handling input X on high signal | | | | | X |
| 10 | Handling input Y on high signal | | | | | X |
| 12 | Chuck condition of spindle H = "Clamped" | | | | | X |

PDF page 45: (blank page, chapter header only). PDF page 46: (blank page).

## For the retrofit

- Two independent PLC/NC channels: `AG1` = slide 1, `AG2` = slide 2 (9-160); AG2 message numbers = AG1 + 5000 (e.g. 7250/12250, 7107/12107; 9-117, 9-156, 9-158). A LinuxCNC message scheme per side could mirror this.
- Remote I/O is "Eltromatic" modules with run-up phases 0, 1, 3, 4, service channel, cyclic operation, earth fault/general fault per module (10400-10410, 9-119 to 9-121).
- Feed drives use servo "choppers" per slide X and Z, with over-temperature pre-warnings and an X axis ventilator motor (11450-11461, 9-143 to 9-145). NC must answer PLC signals within 300 ms (10951, 9-130); "slaving fault overflow compound slide rest / C axis" (10950).
- Main spindle: drive with auxiliary ventilator and over-temperature (10650-10656); "ISM" temperature control via an "AEM module" monitoring front/rear bearing, motor winding, ambient and cooling water temperatures with MAX / MAXMAX / set point / hysteresis parameters (7107-7114, 11014-11029, 12100-12114); ISM cooling with heat exchanger ventilator and pump (12000-12003). Main drive gearing with positions (10450-10453). Spindle clamping only within a position window reached via the C axis (10701); C axis has a pressure switch and limit switch (11800, 9-153).
- Main chuck: hydraulic with pressure switches per clamping direction, front/rear stroke proximity switches, chucking stroke limit, max 20 s clamp/release cycle (10559), alarm if spindle runs > 90 s with chuck open (10556), max chuck speed parameter (10601), no chuck command with spindle turning (10602), chuck change position needs chuck key switch on operating panel (10600); clamp/release via M command or foot switch (10557) (9-123, 9-124).
- Tailstock/centre sleeve: traversable tailstock with 2 end positions and teach-in module, centre sleeve with proximity and pressure switches, forward/return by M command or foot switch (11100-11153, 9-135 to 9-137). Steady rests 1 and 2 with open/closed pressure switches (10900-10904).
- Turret ("tool holder"): position generator with parity, locking initiator "Control interlocking", preindexing, overload clutch, reference traverse, release time, "disc disengaged"/"spindle disengaged" signals, motor over-temperature with reset button on unit (11500-11514, 9-145 to 9-147). Driven tools coupled with M203 / M204 / M205, engaged/disengaged proximity switch, need tool holder in a driven tool position and spindle release (11750-11755, 9-152, 9-153).
- Hydraulics: filter differential pressure, system pressure, min level, motor overcurrent, oil over temperature (10750-10757). Lubrication: pressure switch circuits 1 and 2 (no pressure / permanent pressure), level, interval and suppression-time parameters; suppression cancelled by M49, M18, M30, M99 or cycle stop (11250-11269, 9-138 to 9-141). Coolant motor overcurrent and flow monitor (10800, 10801). Switch cabinet cooling filter/overcurrent/temperature (11200-11205, 12050-12051). Chip conveyor (11350).
- Safety: automatic protection hood with open/close time parameter 031, safety rail, safety circuit signal sequence, emergency release detection; no button pressed while opening (11300-11306, 9-141, 9-142). Setting operation only with hood open and manual mode (10350); spindle > 50 rpm in setting operation trips rotational speed monitoring unit (11850). `Drive on` button with lamp, Emergency stop (11552, 11553).
- Part transfer/automation: working space dividing wall between the sides, hydraulic with limit switches, only with hoods closed, open/close by M command (11950-11953, 9-154). Loading flap, gantry/handling system, pick-off device (gripper, flap, swivel arm, conveyor belt with transport channel), feeding aid, swivel chuck (11651-11655: max swivel speed parameter, 20 s swivel time), bar operation with M94 (11400) (9-128, 9-142, 9-149 to 9-152).
- Part program event inputs: `E1[n]`/`E2[n]` (16 per slide) readable with IF/WHILE; on CTX twin: 1 bar end reached, 9 handling input X, 10 handling input Y, 12 chuck of spindle H clamped, for both slides (9-159, 9-160). Useful as LinuxCNC `motion.digital-in` equivalents for handshaking between the sides.
- Referenced documents: circuit diagram sheets I 04.1 / I 04.2 (machine fault list, 11554, 11556), electrical service handbook, programming handbook, parameter list 9.1.2 on page 9-6.
