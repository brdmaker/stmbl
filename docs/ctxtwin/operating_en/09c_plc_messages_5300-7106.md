# Gildemeister CTX twin operating instructions (English edition), 9. Parameter setting, M commands, PLC error information (part c: PLC error messages 5300-7106), pages 9-79 to 9-116

Source: `ef44926a-CTX_twin_Operating_instructions_9c.pdf`, 38 scanned pages, manual pages 9-79 to 9-116 (PDF page n = manual page 9-(78+n)). "Operating instructions CTX twin, Dok. No.: daaab1g1, February 1995". Running header: "9. Parameter setting, M commands, PLC error information", with the first message number and type of each page at the right (e.g. "5300 F:").

These pages contain only the PLC error message list, from 5300 to 7106. The list **starts before these pages** (earlier numbers and the list's heading and introduction are in part b) and **continues after page 9-116** (part d, from 7107). There is no section heading, running text or figure on these pages. No blank pages.

Each entry in the manual consists of the number, the message text in bold (prefix `F:` fault, `W:` warning, `M:` message, as printed), a "Cause:" with a bold category (e.g. "possible defect", "incorrect setting, parameter", "inadmissible operation in machine status", "maintenance is necessary") plus a line of detail, and an "Overcome:" with who is to act (operator; technologist, setter; electrical / mechanical maintenance personnel) plus the remedy. In the tables below the category and who-acts are in italics, followed by the detail text. "AG1" appears in every message text, apparently the PLC/machine group designator [uncertain whether it denotes side 1 of the twin]. Messages marked `==> not used` have no Cause/Overcome. Obvious misprints are kept and marked [sic].

## PLC error information (continued)

### Page 9-79

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 5300 | `F: Input AG1: M function not defined` | *inadmissible operation in machine status:* M function not defined for this machine equipment | *by operator:* acknowledge error |
| 5301 | `F: INPUT AG1: M function inadmissible due machine status` | *inadmissible operation in machine status:* M function inadmissible due to machine status | *by operator:* Acknowledge error |
| 5350 | `F: Setting operation AG1: inadmissible selection` | *inadmissible operation in machine status:* Protection hood is closed or manual operating mode is not selected | *by operator:* Open protection hood and select manual operating mode |
| 5351 | `M: Setting operation AG1: Inadmissible` | *inadmissible operation in machine status:* Protection hood is closed or manual operating mode is not selected | *by operator:* Open protection hood and select manual operating mode |
| 5400 | `F: Eltromatic AG1: Run up error phase 3` | *possible defect:* Fault when testing the communication | *by electrical maintenance personnel:* Check I/O modules and connection to controls |
| 5401 | `F: Eltromatic AG1: Run up error phase 4` | *possible defect:* General error when commencing cyclical communication | *by electrical maintenance personnel:* Check I/O modules and connection to controls |

### Page 9-80

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 5402 | `F: Eltromatic AG1: Run up error phase 0` | *possible defect requiring maintenance:* Error during self test of I/O module | *by electrical maintenance personnel:* Check modules and connection to controls |
| 5403 | `F: Eltromatic AG1: Run up error phase 1` | *possible defect:* General error during identification phase | *by electrical maintenance personnel:* Check I/O modules and connection to controls |
| 5404 | `F: ELTROMATIC AG1: Run up error service channel error` | *possible defect:* General error during I/O module parameterization | *by electrical maintenance personnel:* Check I/O modules and connection to controls |
| 5405 | `F: Eltromatic AG1: error in cyclical operation` | *possible defect:* General error during cyclical communication | *by electrical maintenance personnel:* Check I/O modules and connection to controls |
| 5406 | `F: Eltromatic AG1: no I/O module identified` | *possible defect:* No I/O module could be identified | *by electrical maintenance personnel:* Check I/O modules and connection to controls |
| 5407 | `F: Eltromatic AG1: non supported module found` | *possible defect:* An unknown I/O module was used | *by electrical maintenance personnel:* Check I/O modules and connection to controls |

### Page 9-81

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 5408 | `F: Eltromatic AG1: Equipment and parameter do not agree` | *possible defect or incorrect setting, parameter:* The Eltromatic equipment determined during machine run up does not correspond to the information in the attendant parameters. | *by electrical maintenance personnel:* Check I/O modules and parameters or match to each other. |
| 5409 | `F: Eltromatic AG1: Faulty earth connection to module <Module No.>` | *possible defect:* Faulty earth connection to stated Eltromatic module. | *by electrical maintenance personnel:* Check the earth connections to the stated Eltromatic module. |
| 5410 | `F: Eltromatic AG1: General fault on module <Module No.>` | *possible defect:* General fault on stated Eltromatic module. | *by electrical maintenance personnel:* Check the stated Eltromatic module and its connections. |
| 5450 | `F: Main drive gearing AG1: inadmissible command` | *operation inadmissible in machine status:* The command is inadmissible in this machine status. | *by operator or electrical maintenance personnel:* Change machine status and repeat command |
| 5451 | `F: Main drive gearing AG1: faulty coupling` | *possible defect:* Required position not reached | *by operator or electrical maintenance personnel:* Repeat command, check gearing access |
| 5452 | `M: Main drive gearing AG1: position undefined` | *inadmissible operation in machine status:* The gearing is not in any of the permissible positions | *by operator:* Trigger the command for an admissible position |

### Page 9-82

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 5453 | `F: Main drive gearing AG1: monitoring triggered` | *possible defect:* The required position was left without reason | *by operator or electrical maintenance personnel:* Repeat command, check gearing access |
| 5500 | `F: Handling AG1: inadmissible command` | *operation inadmissible in the machine status or incorrect setting, parameter:* Inadmissible as the handling condition is incorrect. | *by operator or by technologist, setter:* acknowledge fault, switch handling system to correct condition. |
| 5501 | `M: Handling AG1: machine switched off by handling system` | Machine switched off from gantry (no unmachined parts) | *by technologist, setter:* Fill with unmachined parts, switch on gantry and machine again |
| 5502 | `F: Handling system AG1: not operational` | *operation inadmissible in machine status:* Handling system not switched on or operational | *by operator or technologist, setter:* Check handling |
| 5503 | `W: Handling AG1: not operational` | *inadmissible operation in the machine status:* Handling not switched on or operational | *by operator or technologist, setter:* check handling system |
| 5504 | `M: Handling AG1: Handling is not in automatic operation` | *inadmissible operation in the machine status:* Handling not in automatic operation whilst processing the handling commands. | *by operator:* Set handling to automatic and, if necessary, actuate lathe cycle start. |

### Page 9-83

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 5550 | `M: main chuck AG1: disengagement stroke selected` | *inadmissible operation in the machine status:* Select "operation" disengagement stroke in parameter | *by operator:* Correct parameter, see 9.1.2, "Parameter list" on page 9-6. |
| 5551 | `F: Main chuck AG1: pressure failure` | *incorrect setting, parameter:* "Clamping pressure" pressure switches signalled pressure failure | *by technologist, setter:* Check the corresponding pressure switch dependent upon the clamping direction |
| 5552 | `F: Main chuck AG1: faulty input signals` | *incorrect setting, parameter:* Incorrect signals encountered when clamping or releasing | *by technologist, setter:* Check setting (useful stroke, chucking pressure) |
| 5553 | `F: Main chuck AG1: rear stroke limitation faulty` | *possible defect:* Faulty proximity switch test "rear proximity switch" | *by electrical maintenance personnel:* Change proximity switch or check signal path in accordance with circuit diagram |
| 5554 | `M: Main chuck AG1: chucking stroke limit reached` | *incorrect setting, parameter:* Main chuck has reached end position without pressure | *by technologist, setter:* Check setting (useful stroke, chucking pressure) |
| 5555 | `F: Main chuck AG1: front stroke limitation faulty` | *possible defect:* Faulty proximity switch test "Front proximity switch" | *by electrical maintenance personnel:* Change proximity switch or check signal path in accordance with circuit diagram |

### Page 9-84

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 5556 | `F: Main chuck AG1: spindle running time without chucking pressure exceeded` | *inadmissible operation in machine status:* Spindle ran with chuck open for more than 90 seconds | *by operator:* Acknowledge fault, if necessary correct part program |
| 5557 | `M: main chuck AG1: position not defined` | *inadmissible operation in machine status:* Main chuck neither clamped nor fully open | *by operator:* "Clamp" or "release" command (M command or foot switch) |
| 5558 | `F: Main chuck AG1: monitoring triggered` | *incorrect setting, parameter:* End position "clamped" or "opened" left without reason | *by technologist, setter:* Check setting (useful stroke, chucking pressure) |
| 5559 | `F: Main chuck AG1: Maximum performance time exceeded` | *possible defect:* "Clamping without stroke limitation" module could not conclude the clamping or release cycle within 20 sec. | *by technologist, setter:* Main chuck settings (pressure switch, pressure reduction), check the valves. |
| 5600 | `M: main chuck AG1: in change position` | *inadmissible operation in machine status:* Chuck key is not inserted in the operating panel | *by operator:* Insert chuck key in release switch on operating panel |
| 5601 | `F: Main chuck AG1: maximum rotational speed exceeded` | *incorrect setting, parameter:* Rotational speed was higher than the set chuck rotational speed. | *by technologist, setter:* Check set rotational speed, if necessary correct part program |

### Page 9-85

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 5602 | `F: Main chuck AG1: inadmissible command` | *inadmissible command in machine status:* Inadmissible chuck command with spindle turning | *by operator:* Stop spindle in advance |
| 5650 | `F: Main spindle drive AG1: motor over temperature` | *inadmissible operation in machine status:* Motor overloaded | *by electrical maintenance personnel:* Check the operator [sic; 5651 says "Check chopper"] and allow the motor to cool down |
| 5651 | `W: Main spindle drive AG1: motor over temperature` | *inadmissible operation in the machine status:* Motor overloaded | *by electrical maintenance personnel:* Check chopper and permit the motor to cool down |
| 5652 | `F: Main spindle drive AG1: not operational` | *possible defect:* Main spindle drive interrupted | *by electrical maintenance personnel:* Overcome fault, refer to electrical service handbook |
| 5653 | `F: Main spindle drive AG1: auxiliary ventilator malfunction` | *possible defect:* Motor protection switch triggered | *by electrical maintenance personnel:* Check electricity setting |
| 5654 | `W: Main spindle drive AG1: auxiliary ventilator malfunction` | *possible defect:* Motor protection switch triggered | *by electrical maintenance personnel:* Check current setting |

### Page 9-86

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 5655 | `F: Main spindle drive AG1: temperature pre-warning` | *incorrect setting, parameter possible defect or overload in cycle:* The main spindle drive signals over temperature | *by electrical maintenance personnel:* Allow motor to cool down, check motor, check actuator |
| 5656 | `W: Main spindle drive AG1: temperature pre-warning` | *incorrect setting, parameter, possible defect or overload in cycle:* The main spindle drive signals over temperature | *by electrical maintenance personnel:* Allow motor to cool down, check motor, check actuator |
| 5700 | `F: Main spindle clamping AG1: faulty design` | *incorrect setting, parameter:* Incorrect proximity switch signals of spindle clamping | *by technologist, setter:* Check proximity switches |
| 5701 | `F: Main spindle clamping AG1: inadmissible command` | *inadmissible operation in the machine status:* Main spindle is not in the position window | *by operator:* Perform spindle positioning in advance with the C axis |
| 5702 | `F: Main spindle clamping AG1: monitoring triggered` | *incorrect setting, parameter:* The selected status has been left without reason | *by technologist, setter:* Check proximity switches |
| 5703 | `M: Main spindle clamping AG1: main spindle is clamped` | *main spindle is clamped:* The main spindle is manually clamped | *by operator:* Release the main spindle clamping |

### Page 9-87

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 5750 | `F: Hydraulic AG1: filter blocked` | *maintenance is necessary:* Excessive differential pressure upstream/downstream from the hydraulic filter | *by mechanical maintenance personnel:* Change hydraulic filter |
| 5751 | `W: Hydraulic AG1: filter blocked` | *maintenance is necessary:* Excessive differential pressure upstream / downstream from the hydraulic filter | *by mechanical maintenance personnel* (printed "Overcoming:"): Change hydraulic filter |
| 5752 | `F: Hydraulic AG1: no system pressure` | *maintenance is necessary:* Pressure loss or pressure failure in hydraulics | *by mechanical maintenance personnel:* Check oil level, hydraulics for leaks |
| 5753 | `W: Hydraulic AG1: minimum level reached` | *maintenance is necessary:* Oil level in hydraulic tank too low | *by mechanical maintenance personnel:* Check hydraulic system for leaks, top up oil |
| 5754 | `F: Hydraulic AG1: minimum level reached` | *maintenance is necessary:* Oil level in hydraulic tank too low | *by mechanical maintenance personnel:* Check hydraulic system for leaks, top up oil |
| 5755 | `F: Hydraulic AG1: motor overcurrent` | *possible defect:* Motor protection switch triggered | *by electrical maintenance personnel:* Check electricity setting |

### Page 9-88

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 5756 | `W: Hydraulic AG1: oil over temperature` | *possible defect:* Excessive hydraulic unit temperature | *by mechanical maintenance personnel:* Check hydraulic unit (oil level, others) |
| 5757 | `F: Hydraulic AG1: oil over temperature` | *possible defect:* Excessive hydraulic unit temperature | *by mechanical maintenance personnel:* Check hydraulic unit (oil level, others) |
| 5800 | `F: Coolant AG1: motor overcurrent` | *possible defect:* Motor protection switch(es) has/have been triggered | *By electrical maintenance personnel:* Check electricity setting |
| 5801 | `F: Coolant AG1: flow monitoring triggered` | *possibly defective or maintenance necessary:* Coolant supply is damaged | *by electrical or mechanical maintenance personnel:* Check coolant supply |
| 5850 | `F: Loading flap AG1: inadmissible command` | *Inadmissible operation in machine status:* Protection hood is opened or gripper is in working space | *by operator:* Close protection hood and remove gripper from working space |
| 5851 | `M: loading flap AG1: position not defined` | *inadmissible operation in the machine status:* Loading flap is neither closed nor open | *by operator:* Trigger "open" or "close" command |

### Page 9-89

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 5852 | `F: Loading flap AG1: monitoring triggered` | *incorrect setting, parameter:* The selected status has been left without reason | *by technologist, setter:* Check loading flap limit switch |
| 5900 | `F: Steady rests AG1: inadmissible command` | *inadmissible operation in the machine status:* The spindle is switched on, inadmissible centre sleeve position | *by operator:* Stop spindle or clamp centre sleeve |
| 5901 | `M: steady rest 1 AG1: undefined position` | *inadmissible operation in the machine status:* Steady rest 1 is neither closed nor open | *by operator:* "Clamp" or "release" command (M command or key) |
| 5902 | `F: Steady rest 1 AG1: monitoring triggered` | *incorrect setting, parameter:* Clamped or open condition has been left | *by technologist, setter:* Pressure switch "opened" or closed" [sic] |
| 5903 | `M: Steady rest 2 AG1: position undefined` | *operation inadmissible in the machine status:* Steady rest 2 is neither clamped nor fully open | *by operator:* "Clamp" or "release" command (M command or key) |
| 5904 | `F: Steady rest 2 AG1: monitoring triggered` | *incorrect setting, parameter:* Closed or open condition left | *by technologist, setter:* Check "open" or "close" pressure switch |

### Page 9-90

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 5950 | `F: NC AG1: not operational` | *possible defect:* Slaving fault overflow compound slide rest / C axis | *by electrical maintenance personnel:* Feed - drives, check compound slide rest generator |
| 5951 | `F: NC AG1: maximum NC reaction time exceeded` | *possible defect:* The NC reaction time to the PLC signal is greater than 300 ms | *By operator, if necessary Gildemeister Service:* Acknowledge fault, if necessary switch machine on and off. Please notify Gildemeister Service |
| 6000 | `F: Parameter AG1: take over only during machine acceleration` | *incorrect setting, parameter:* Switch off machine when parameters have changed | *by technologist, setter:* Change parameter, switch off machine and switch on again |
| 6001 | `F: Pick-off device type parameter AG1: value inadmissible` | *incorrect setting, parameter:* Incorrect "handling system and material feeder" parameter | *by technologist, setter:* Correct parameter, see 9.1.2, "Parameter list" on page 9-6 |
| 6002 | `F: Main drive gearing type parameter AG1: inadmissible value` | *incorrect setting, parameter:* Incorrect value in parameter "Main drive gearing data" | *by technologist, setter:* Correct parameter, see 9.1.2, "Parameter list" on page 9-6 |
| 6003 | `F: Handling type parameter AG1: inadmissible type` | *incorrect setting, parameter:* Incorrect parameter "handling system and material feeder" | *by technologist, setter:* Correct parameter, see 9.1.2, "Parameter list" on page 9-6 |

### Page 9-91

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 6004 | `F: Main chuck parameter AG1: inadmissible working direction` | *incorrect setting, parameter:* Incorrect value in "main spindle" parameter | *by technologist, setter:* Correct parameter, see 9.1.2, "Parameter list" on page 9-6 |
| 6005 | `F: main chuck parameter AG1: inadmissible chucking mode` | *incorrect setting, parameter:* Incorrect value in "Main spindle" parameter | *by technologist, setter:* Correct parameter, see 9.1.2, "Parameter list" on page 9-6 |
| 6006 | `F: Main chuck parameter AG1: inadmissible type` | *incorrect setting, parameter:* incorrect main chuck type in "Main spindle" parameter | *by technologist, setter:* Correct parameter, see 9.1.2, "Parameter list" on page 9-6 |
| 6007 | `F: Number of steady rests parameter AG1: value too large` | *incorrect setting, parameter:* Excessive value in "Steady rest description" parameter | *by technologist, setter:* Correct parameter |
| 6008 | `F: Material feeding type parameter AG1: inadmissible value` | *incorrect setting, parameter:* Incorrect "Handling system and material feeder" parameter | *by technologist, setter:* Correct parameter, see 9.1.2, "Parameter list" on page 9-6 |
| 6009 | `F: Centre sleeve parameter AG1: inadmissible type` | *incorrect setting, parameter:* Incorrect centre sleeve selected in "Tailstock, centre sleeve" | *by technologist, setter:* Correct parameter |

### Page 9-92

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 6010 | `F: Tailstock type parameter AG1: inadmissible value` | *incorrect setting, parameter.* Value in "Tailstock, centre sleeve" parameter incorrectly set | *by technologist, setter:* Correct parameter, see 9.1.2, "Parameter list" on page 9-6 |
| 6011 | `F: Parameter swivel chuck type AG1: inadmissible value` | `==> not used` | |
| 6012 | `F: Tool holder parameter AG1: inadmissible type` | *incorrect setting, parameter:* Incorrect tool holder type selected in "Toolholder data" parameter | *by technologist, setter:* Correct parameter, see 9.1.2, "Parameter list" on page 9-6 |
| 6013 | `F: Tool holder parameter AG1: inadmissible number of tools` | *incorrect setting, parameter:* Parameter "holder number and spindle" incorrectly set | *by technologist, setter:* Correct parameter, see 9.1.2, "Parameter list" on page 9-6 |
| 6014 | `F: Parameter ISM temperature control AG1: value "number of values for mean value formation" inadmissible` | *incorrect setting, parameter:* Value in "Temperature control spindle" parameter incorrect | *by technologist, setter:* Correct parameter |
| 6015 | `F: Parameter ISM temperature control AG1: value "Hysteresis cooling water temperature" inadmissible` | *incorrect setting, parameter:* Value in "Temperature control spindles" parameter incorrect | *by technologist, setter:* Correct parameter |
| 6016 | `F: Parameter ISM temperature control AG1: value "Hysteresis rear bearing temperature" inadmissible` | *incorrect setting, parameter:* Value in "Temperature control spindles" parameter incorrect | *by technologist, setter:* Correct parameter |

### Page 9-93

All six entries on this page have the same cause and remedy: Cause: *incorrect setting, parameter:* Value in "Temperature control spindles" parameter incorrect. Overcome: *by technologist, setter:* Correct parameter.

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 6017 | `F: Parameter ISM temperature control AG1: value "Hysteresis front bearing temperature" inadmissible` | as above | as above |
| 6018 | `F: Parameter ISM temperature control AG1: value "Hysteresis winding temperature" inadmissible` | as above | as above |
| 6019 | `F: Parameter ISM temperature control AG1: value "MAX rear bearing temperature" inadmissible` | as above | as above |
| 6020 | `F: Parameter ISM temperature control AG1: value "MAX front bearing temperature" inadmissible` | as above | as above |
| 6021 | `F: Parameter ISM temperature control AG1: value "MAX ambient temperature" inadmissible` | as above | as above |
| 6022 | `F: Parameter ISM temperature control AG1: value "MAX winding temperature" inadmissible` | as above | as above |

### Page 9-94

All six entries on this page: Cause: *incorrect setting, parameter:* Value in "Temperature control spindles" parameter incorrect. Overcome: *by technologist, setter:* Correct parameter.

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 6023 | `F: Parameter ISM temperature control AG1: value "MAXMAX rear bearing temperature" inadmissible` | as above | as above |
| 6024 | `F: Parameter ISM temperature control AG1: value "MAXMAX front bearing temperature" inadmissible` | as above | as above |
| 6025 | `F: Parameter ISM temperature control AG1: value "MAXMAX winding temperature" inadmissible` | as above | as above |
| 6026 | `F: Parameter ISM temperature control AG1: value "Set point value cooling water temperature" inadmissible` | as above | as above |
| 6027 | `F: Parameter ISM temperature control AG1: value "Set point value rear bearing temperature" inadmissible` | as above | as above |
| 6028 | `F: Parameter ISM temperature control AG1: value "Set point value front bearing temperature" inadmissible` | as above | as above |

### Page 9-95

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 6029 | `F: Parameter ISM temperature control AG1: value "Set point value winding temperature" inadmissible` | *incorrect setting, parameter:* Value in "Temperature control spindles" parameter incorrect | *by technologist, setter:* Correct parameter |
| 6030 | `F: Parameter lubrication AG1: Value "lubrication interval time" inadmissible` | *incorrect setting, parameter:* Value in "lubrication time interval" parameter incorrect | *by technologist, setter:* Correct parameter |
| 6031 | `F: Parameter lubrication AG1: Value "lubrication suppression time" inadmissible` | *incorrect setting, parameter:* Value in "lubrication suppression time" parameter incorrect | *by technologist, setter:* Correct parameter |
| 6100 | `F: Centre sleeve AG1: pressure failure` | *incorrect setting, parameter:* Centre sleeve clamped and pressure failure | *by technologist, setter:* Check centre sleeve pressure switch setting |
| 6101 | `M: Centre sleeve AG1: front end position reached` | *incorrect setting, parameter:* End position reached when clamping without pressure | *by technologist, setter:* Insufficient centre sleeve stroke, traverse tailstock |
| 6102 | `F: Centre sleeve AG1: incorrect input signals` | *Incorrect setting, parameter:* Incorrect signals encountered when traversing centre sleeve | *by technologist, setter* (printed "Overcoming:"): Check centre sleeve setting (proximity switches, pressure switches) |

### Page 9-96

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 6103 | `F: Centre sleeve AG1: inadmissible command` | *inadmissible operation in the machine status:* The spindle is switched on, no steady rest clamped | *by operator:* Stop the spindle or clamp a steady rest |
| 6104 | `M: Centre sleeve AG1: undefined setting` | *inadmissible status in machine status:* Centre sleeve is neither clamped nor fully run in | *by operator:* "Forward" or "Return" command (M command or foot switch) |
| 6105 | `F: Centre sleeve AG1: monitoring triggered` | *incorrect setting, parameter:* Leave clamped or run in condition | *by technologist, setter:* Check centre sleeve setting (proximity switches, pressure switches) |
| 6150 | `F: Tailstock AG1: inadmissible command` | *inadmissible operation in machine status:* Inadmissible as centre sleeve extended or main spindle turning | *by operator:* Acknowledge fault, if necessary correct part program |
| 6151 | `F: Tailstock AG1: not operational` | *incorrect setting, parameter:* Teach-in module of the automatic tailstock | *by technologist, setter:* Check unit, if necessary reprogram |
| 6152 | `M: tailstock AG1: position not defined` | *inadmissible operation in the machine status:* The traversable tailstock is in neither of the 2 end positions | *by operator:* Move using M/command, or `FORWARD` or `REVERSE` key |

### Page 9-97

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 6153 | `F: Tailstock AG1: monitoring triggered` | *incorrect setting, parameter or possible defect:* Traversable tailstock limit switch offset or defective | *by electrical or mechanical maintenance personnel:* Check limit switch, replace if necessary |
| 6200 | `F: Switch cabinet cooling AG1: filter blocked` | *maintenance is necessary:* Excessive differential pressure upstream/downstream of filter mat | *by mechanical maintenance personnel:* Clean filter or change |
| 6201 | `W: Switch cabinet cooling AG1: filter blocked` | *maintenance is necessary:* Excessive differential pressure upstream/downstream of filter mat | *by mechanical maintenance personnel:* Clean filter or change |
| 6202 | `F: Switch cabinet cooling AG1: overcurrent` | *possible defect:* Motor protection switch triggered | *by technologist, setter:* Check current setting |
| 6203 | `W: Switch cabinet cooling AG1: overcurrent` | *possible defect:* Motor protection switch triggered | *by electrical maintenance personnel:* Check electricity setting |
| 6204 | `F: Switch cabinet cooling AG1: over temperature` | *possible defect:* Switch cabinet over temperature | *by mechanical maintenance personnel:* Check cooling unit, excessive ambient temperature |

### Page 9-98

(Running header on this page reads "9205 W:", a misprint for 6205.)

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 6205 | `W: Switch cabinet cooling AG1: over temperature` | *possible defect:* Switch cabinet over temperature | *by mechanical maintenance personnel:* Check cooling unit, excessive ambient temperature |
| 6250 | `F: Lubrication AG1: no pressure` | *maintenance necessary:* Lubricant pressure switch signals no pressure | *by mechanical maintenance personnel:* Top up lubricating oil container, check pressure switch |
| 6251 | `W: Lubrication AG1: no pressure` | *maintenance is required:* Lubricant pressure switch signals no pressure | *by mechanical maintenance personnel:* Top up lubricating oil container, check pressure switch |
| 6252 | `W: Lubrication AG1: minimum level reached` | *Maintenance is necessary:* Fresh oil lubrication level too low | *by mechanical maintenance personnel:* Top up fresh oil |
| 6253 | `F: Lubrication AG1: minimum level reached` | *maintenance is necessary:* Fresh oil lubrication level too low | *by mechanical maintenance personnel:* Top up fresh oil |
| 6254 | `F: Lubrication AG1: motor overcurrent` | *possible defect:* Motor protection switch triggered | *by electrical maintenance personnel:* Check electricity setting |

### Page 9-99

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 6255 | `W: Lubrication AG1: motor overcurrent` | *possible defect:* Motor protection switch triggered | *by electrical maintenance personnel:* Check electricity setting |
| 6256 | `M: Lubrication oil AG1: top up oil` | *maintenance is necessary:* Insufficient fresh oil level | *by mechanical maintenance personnel:* Top up fresh oil |
| 6257 | `F: Lubrication AG1: permanent pressure` | *maintenance is necessary:* Lubricant pressure switch signals permanent pressure | *by mechanical maintenance personnel:* Check lubricant pressure lines and/or pressure switch |
| 6258 | `W: Lubrication AG1: permanent pressure` | *maintenance is necessary:* Lubricant pressure switch signals permanent pressure | *by mechanical maintenance personnel:* Check lubricant pressure lines and/or pressure switch |
| 6259 | `F: Lubrication AG1: no pressure circuit 1` | *maintenance is required:* Lubrication pressure switch signals no pressure | *by mechanical maintenance personnel:* Top up lubricating oil container, check pressure switch |
| 6260 | `F: Lubrication AG1: no pressure circuit 2` | *maintenance is required:* Lubrication pressure switch signals no pressure | *by mechanical maintenance personnel:* Top up lubricating oil container, check pressure switch |

### Page 9-100

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 6261 | `F: Lubrication AG1: permanent pressure circuit 1` | *maintenance is required:* Lubrication pressure switch signals permanent pressure | *by mechanical maintenance personnel:* Check lubrication pressure lines and/or pressure switch |
| 6262 | `F: Lubrication AG1: permanent pressure circuit 2` | *maintenance is required:* Lubrication pressure switch signals permanent pressure | *by mechanical maintenance personnel:* Check lubrication pressure lines and/or pressure switch |
| 6263 | `F: Lubrication AG1: command inadmissible` | `==> not used` | |
| 6264 | `F: Lubrication AG1: maximum suppression time exceeded` | *incorrect setting, parameter or programming fault:* Maximum suppression time exceeded | *by technologist, setter:* Check parameter, withdraw lubrication pulse suppression using M49, M18, M30, M99 or cycle stop |
| 6265 | `W: Lubrication AG1: no pressure circuit 1` | *maintenance is required:* Lubrication pressure switch signals no pressure | *by mechanical maintenance personnel:* Top up lubricating oil container, check pressure switch |
| 6266 | `W: Lubrication AG1: no pressure circuit 2` | *maintenance is required:* Lubrication pressure switch signals no pressure | *by mechanical maintenance personnel:* Top up lubricating oil container, check pressure switch |
| 6267 | `W: Lubrication AG1: permanent pressure circuit 1` | *maintenance is required:* Lubrication pressure switch signals permanent pressure | *by mechanical maintenance personnel:* Check lubricating pressure lines, and/or pressure switch |

### Page 9-101

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 6268 | `W: Lubrication AG1: permanent pressure circuit 2` | *maintenance is required:* Lubrication pressure switch signals permanent pressure | *by mechanical maintenance personnel:* Check lubricating pressure lines, and/or pressure switch |
| 6269 | `W: Lubrication AG1: maximum suppression time exceeded` | *incorrect setting, parameter or programming fault:* Maximum time under pressure exceeded | *by technologist, setter:* Check parameter, withdraw lubrication pulse suppression using M49, M18, M30, M99 or cycle stop |
| 6300 | `F: Protection hood AG1: max. time to open/close exceeded` | *possible defect or incorrect setting, parameter:* Possible defect on the automatic protection hood | *by electrical or mechanical maintenance personnel:* Check function, if necessary correct parameter 031 |
| 6301 | `M: Protection hood AG1: motor overcurrent` | *incorrect setting, parameter or possible defect:* Motor protection switch triggered | *by electrical maintenance personnel:* Check electricity setting |
| 6302 | `M: Protection hood AG1: not opened and not closed` | *Inadmissible operation in machine status:* Protection hood not locked and not opened | *by operator:* Open protection hood and then close properly |
| 6303 | `M: Protection hood AG1: opening not permitted as button pressed` | *inadmissible operation in the machine status:* Button actuated when opening the protection hood | *by operator:* Do not actuate any button, then open protection hood |

### Page 9-102

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 6304 | `M: Protection hood AG1: safety rail triggered` | *inadmissible operation in machine status:* Closing of automatic protection hood interrupted | *by operator:* Acknowledge fault, if necessary check safety rail |
| 6305 | `F: Protection hood AG1: safety circuit malfunction` | *possible defect:* Protection hood safety circuit has incorrect succession of signals | *by electrical maintenance personnel:* Check safety circuit (refer to circuit diagram) |
| 6306 | `F: Protection hood AG1: inadmissibly opened` | *inadmissible operation in machine status:* Protection hood was opened with the emergency release | *by operator:* Open protection hood and then close |
| 6350 | `F: Chip conveyor AG1: motor overcurrent` | *inadmissible operation in the machine status:* Motor protection switch triggered | *by operator or by electrical maintenance personnel.* Run chip conveyor empty, check current setting |
| 6351 | `W: Chip conveyor AG1: motor overcurrent` | *inadmissible operation in the machine status:* Motor protection switch triggered | *by operator or by electrical maintenance personnel.* Run conveyor empty, check electricity setting |
| 6400 | `F: Bar operation AG1: inadmissible command` | *inadmissible operation in machine status:* Command M94 triggered without bar operation running | *by operator:* Select and switch on bar operation |

### Page 9-103

(Number 6452 is circled by hand in the scanned copy.)

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 6401 | `F: Bar operation AG1: not operational` | *inadmissible operation in machine status:* Bar operation selected, however, not operational | *by electrical maintenance personnel:* Check bar unit |
| 6402 | `M: Bar operation AG1: bar change` | *inadmissible operation in machine status:* Bar is machined | *by operator:* Insert new bar, no bar loading magazine |
| 6450 | `F: Feed drives AG1: motor overcurrent` | *possible defect:* Motor protection switch triggered | *by electrical maintenance personnel:* Check electricity setting |
| 6451 | `F: Feed drives AG1: motor over temperature` | *incorrect setting, parameter or possible defect:* One of the feed drives signals over temperature | *by electrical maintenance personnel:* Chuck [sic, probably "Check"] motor chopper, allow motor to cool down |
| 6452 | `F: Feed drives AG1: not operational` | *possible defect:* Servo chopper of the feed drive damaged | *by electrical maintenance personnel:* See electrical service instructions: chopper, circuit diagrams |
| 6453 | `F: Feed drives AG1: slide X not operational` | *possible defect:* Chopper for slide X malfunction | *by electrical maintenance personnel:* Overcome malfunction, refer to electric service handbook |

### Page 9-104

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 6454 | `F: Feed drives AG1: slide X temperature pre-warning` | *incorrect setting, parameter, possible defect or overload in cycle:* one of the feed drives signals over-temperature | *by electrical maintenance personnel:* Permit drive to cool down, check drive, check chopper? |
| 6455 | `W: Feed drives AG1: slide X temperature pre-warning` | *incorrect setting, parameter, possible defect or overload in cycle:* one of the feed drives signals over-temperature | *by electrical maintenance personnel:* Permit drive to cool down, check drive, check actuator |
| 6456 | `F: Feed drives AG1: slide Z not operational` | *possible defect:* Chopper for slide Z malfunction | *by electrical maintenance personnel:* Overcome malfunction, refer to electric service handbook |
| 6457 | `F: Feed drives AG1: ventilator X axis motor overcurrent` | *possible defect:* Motor protection switch triggered | *by electrical maintenance personnel:* Check current setting |
| 6458 | `W: Feed drives AG1: slide Z temperature pre-warning` | *incorrect setting, parameter, possible defect or overload in cycle:* one of the feed drives signals over-temperature | *by electrical maintenance personnel:* Permit drive to cool down, check drive, check actuator |
| 6459 | `F: Feed drives AG1: slide Z temperature pre-warning` | *incorrect setting, parameter, possible defect or overload in cycle:* one of the feed drives signals over-temperature | *by electrical maintenance personnel:* Permit drive to cool down, check drive, check actuator |

### Page 9-105

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 6460 | `W: Feed drives AG1: ventilator X axis motor overcurrent` | *possible defect:* Motor protection switch triggered | *by electrical maintenance personnel:* Check current setting |
| 6461 | `M: Feed drives AG1: X axis ventilator motor overcurrent` | *possible defect:* Motor protection switch triggered | *by electrical maintenance personnel:* Check current setting |
| 6500 | `F: Tool holder AG1: motor overcurrent` | *possible defect:* Motor protection switch triggered | *by electrical maintenance personnel:* Check electricity setting |
| 6501 | `W: Tool holder AG1: motor overcurrent` | *possible defect:* Motor protection switch triggered | *by electrical maintenance personnel:* Check electricity setting |
| 6502 | `F: Tool holder AG1: motor over temperature` | *inadmissible operation in the machine status:* Motor temperature monitor triggered | *by electrical maintenance personnel:* Allow motor to cool down, press reset button on unit |
| 6503 | `W: Tool holder AG1: motor over temperature` | *inadmissible operation in machine status:* Motor temperature monitor triggered | *by electrical maintenance personnel:* Allow motor to cool down, press reset button on unit |

### Page 9-106

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 6504 | `F: Tool holder AG1: not locked in position` | *inadmissible operation in machine status:* Tool holder locked in incorrect position | *by operator:* Swivel tool holder with T command |
| 6505 | `F: Tool holder AG1: faulty parity check` | *possible defect:* Tool holder position generator had a parity error | *by operator or electrical maintenance personnel:* Swivel tool holder (Tx) position generator, check cable |
| 6506 | `F: Tool holder AG1: position not found` | *incorrect setting, parameter or possible defect:* Faulty position generator or incorrect value in parameter | *by technologist, setter or mechanical maintenance personnel:* Check position generator or correct parameter |
| 6507 | `F: Tool holder AG1: T value for tool holder too large` | *inadmissible operation in machine status or incorrect setting, parameter:* Enter entered value T value > in parameter "Holder number" [sic] | *by operator or by technologist, setter:* Correct parameter or enter smaller T value |
| 6508 | `F: Tool holder AG1: T value could not be taken over` | *inadmissible operation in the machine status:* Execution blocked due to machine status | *by operator:* Correct machine status (e.g. close hood) |
| 6509 | `F: Tool holder AG1: preindexing faulty` | *inadmissible operation in the machine status:* Faulty preindexation in the tool holder test | *by operator:* Swivel tool holder with T command |

### Page 9-107

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 6510 | `F: Tool holder AG1: tool holder does not lock` | *Incorrect setting, parameter:* Initiator "Control interlocking" does not switch | *by technologist, setter:* Check initiator, if necessary, readjust and test |
| 6511 | `F: Tool holder AG1: Overload clutch actuated` | *possible defect or overload:* Overload coupling triggered | *by electrical/mechanical maintenance personnel:* Find cause of fault and reset the tool holder |
| 6512 | `F: Tool holder AG1: Tool holder does not release` | *incorrect setting, parameter or possible defect:* The tool holder does not release during the time allowed | *by electrical/mechanical maintenance personnel:* Find cause of fault and reset the tool holder, if necessary |
| 6513 | `F: Tool holder AG1: Incorrect tool holder reference traverse` | *incorrect setting, parameter or possible defect:* The tool holder does not perform the reference movement during the time allowed | *by electrical/mechanical maintenance personnel:* Find cause of fault and reset the tool holder, if necessary |
| 6514 | `F: Tool holder AG1: internal tool holder fault` | *possible defect:* Tool holder does not clearly signal "disc disengaged" or "spindle disengaged" | *by electrical/mechanical maintenance personnel:* Find cause of fault and reset the tool holder, if necessary |
| 6550 | `F: Output modules AG1: control voltage failure` | *possible defect:* Automatic safety cut-out triggered | *by electrical maintenance personnel:* Overcome the cause of the overload |

### Page 9-108

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 6551 | `F: Input modules AG1: control voltage failure` | *Possible defect:* Automatic safety cut-out triggered | *by electrical maintenance personnel:* Overcome cause of fault |
| 6552 | `F: Machine AG1: not switched on` | *inadmissible operation in the machine status:* Drive not switched on on operating panel | *by operator:* Actuate `Drive on` button, lamp in button must have lit |
| 6553 | `F: Machine AG1: Emergency stop` | *inadmissible operation in the machine status:* Press Emergency stop button | *by operator:* Release emergency stop button |
| 6554 | `F: Machine malfunction AG1: see fault list` | *possible defect:* Only with certain options, refer to circuit diagram I 04.1 | *by electrical maintenance personnel:* Overcome fault |
| 6555 | `F: Valves AG1: control voltage failure` | *Possible defect:* Automatic safety cut-out triggered | *by electrical maintenance personnel:* Overcome cause of the overload |
| 6556 | `F: Machine warning AG1: refer to fault list` | *possible defect:* Only for certain options, refer to circuit diagram I 04.2 | *by electrical maintenance personnel:* Overcome fault |

### Page 9-109

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 6557 | `W: Machine warning AG1: refer to fault list` | *possible defect:* Only for certain options, refer to circuit diagram I 04.2 | *by electrical maintenance personnel:* Overcome fault |
| 6600 | `F: Insert aid AG1: inadmissible command` | `==> not used` | |
| 6601 | `M: Feeding aid AG1: position not defined` | *inadmissible operation in machine status:* Feeding aid is not an in end position [sic] | *by operator:* Move feeding aid to required end position with M command |
| 6602 | `F: Feeding aid AG1: monitoring triggered` | *incorrect setting, parameter or possible defect:* Feeding aid has left end position without a command | *by electrical or mechanical maintenance personnel.* Feeding aid (check hydraulics and limit switch) |
| 6650 | `M: Swivel chuck AG1: inadmissible changes to the selection` | `==> not used` | |
| 6651 | `F: Swivel chuck AG1: inadmissible command` | *inadmissible operation in machine status:* Spindle r.p.m too high or hardware swivel permission not present | *by operator:* Repeat procedure at lower main spindle r.p.m, if necessary, check parameter for maximum rotational speed swivelling |
| 6652 | `M: Swivel chuck AG1: position not defined` | *swivel command aborted:* The swivel chuck is in an undefined intermediate position | *by operator:* Swivel swivel chuck to any position |

### Page 9-110

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 6653 | `F: Swivel chuck AG1: monitoring triggered` | *possible defect:* The swivel chuck has left the current condition without command | *by technologist, setter:* Check swivel chuck setting (proximity switch, pressure switch) |
| 6654 | `F: Swivel chuck AG1: target position not found` | *possible defect:* The swivel chuck has not found the programmed position | *by technologist, setter:* Check swivel chuck settings of proximity switches |
| 6655 | `F: Swivel chuck AG1: maximum swivel time exceeded` | *possible defect:* The swivel chuck could not conclude the swivel movement within 20 seconds | *by technologist, setter:* Swivel chuck settings (pressure switch, pressure reducer) valves |
| 6700 | `M: Pick-off device gripper AG1: position not defined` | *Operation inadmissible in the machine status:* Gripper is not in either of the end positions | *by operator:* "Grip" or "Release" command (M command or button) |
| 6701 | `F: Pick-off device gripping AG1: monitoring triggered` | *incorrect setting, parameter:* The selected status has been left without reason | *by technologist, setter:* Check "gripper" proximity switch |
| 6702 | `M: Pick-off device flap AG1: position not defined` | *inadmissible operation in the machine status:* Flap is not in either of the end positions | *by operator:* "Open" or "Close" command (M command or button) |

### Page 9-111

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 6703 | `F: Flap pick-off device AG1: monitoring triggered` | *incorrect setting, parameter:* The selected status has been left without reason | *by technologist, setter:* Check "Flap" proximity switch |
| 6704 | `M: Swivel pick-off device AG1: position not defined` | *inadmissible operation in the machine status:* Swivel arm is not in either of the end positions | *by operator:* "Forward" or "Return" command (M command or button) |
| 6705 | `F: Swivel pick-off device AG1: Monitoring triggered` | *incorrect setting, parameter:* The selected status has been left without reason | *by technologist, setter:* Check "Swivel" proximity switch |
| 6706 | `F: Pick-off device AG1: inadmissible command` | *inadmissible operation in the machine status:* The command is not admissible in this machine status | *by operator or electrical maintenance personnel:* Change machine status and repeat command |
| 6707 | `F: Pick-off device AG1: maximum execution time exceeded` | *incorrect setting, parameter:* Pick-off command not performed in the prescribed time | *by technologist, setter:* Check pick-off device |
| 6708 | `F: Pick-off device conveyor belt AG1: faulty execution` | *possible defect:* Workpiece did not enter the transport channel within the maximum time specified in the parameter | *by technologist, setter:* Check pick-off device / parameter |

### Page 9-112

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 6709 | `F: Pick-off device conveyor belt AG1: motor overcurrent` | *possible defect:* Motor protection switch triggered | *by electrical maintenance personnel:* Check current setting |
| 6710 | `W: Pick-off device conveyor belt AG1: motor overcurrent` | *possible defect:* Motor protection switch triggered | *by electrical maintenance personnel:* Check current setting |
| 6711 | `M: Pick-off device conveyor belt AG1: workpiece in transport channel` | *possible defect:* The workpiece is in the conveyor channel with the conveyor belt switched off | *by operator:* By switching on the conveyor belt the workpiece is moved out of the conveyor channel |
| 6750 | `F: Driven tools AG1: inadmissible command` | *inadmissible operation in machine status:* Protection hood opened. Driven tool spindle release missing or the tool holder was not in a driven tool position | *by operator, technologist, setter or electrical maintenance personnel:* Swing in to driven tool position or find cause of missing spindle release |
| 6751 | `F: Driven tools AG1: faulty coupling procedure` | *incorrect setting, parameter:* Faulty coupling procedure with M103 / M104 / M105 | *by technologist, setter:* Check proximity switch engaged/disengaged |
| 6752 | `F: Driven tools AG1: coupling monitoring triggered` | *incorrect setting, parameter:* The selected status was left without reason | *by technologist, setter:* Check proximity switch engaged/disengaged |

### Page 9-113

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 6753 | `F: Driven tool AG1: not operational` | *possible defect:* The driven tool actuator is damaged | *by electrical maintenance personnel:* Refer to electrical service handbook to overcome malfunction |
| 6754 | `F: Driven tool AG1: Temperature pre-warning` | *Incorrect setting, parameter or possible defect or overload in cycle:* The driven tool actuator signals over-temperature | *by electrical maintenance personnel:* Permit drive to cool, check drive, check actuator |
| 6755 | `F: Driven tools AG1: Excessive temperature` | *incorrect setting, parameter or possible defect or overload in cycle:* The driven tool actuator signals over-temperature | *by electrical maintenance personnel:* Permit drive to cool down, check drive, check actuator |
| 6755 [sic; number printed twice, the W entry is probably 6756] | `W: Driven tools AG1: Excessive temperature` | *incorrect setting, parameter or possible defect or overload in cycle:* The driven tool actuator signals over-temperature | *by electrical maintenance personnel:* Permit drive to cool down, check drive, check actuator |
| 6800 | `F: C axis main spindle AG1: monitoring triggered` | *incorrect setting, parameter or possible defect:* C axis pressure switch and limit switch offset or defective | *by electrical or mechanical maintenance personnel.* Check pressure switch and limit switch, replace if necessary |
| 6850 | `F: Rotational speed monitoring AG1: unit damaged or triggered` | *inadmissible condition in machine status, possible defect:* Spindle rotates at > 50 r.p.m in setting, fault in rotational speed monitoring unit | *by operator, by electrical maintenance personnel:* Spindle < 50 r.p.m , check speed monitoring unit |

### Page 9-114

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 6900 | `F: AG1: Refer to PLC error list` | `==> not used` | |
| 6901 | `F: AG1: Refer to PLC error list` | `==> not used` | |
| 6902 | `W: AG1: Refer to PLC error list` | `==> not used` | |
| 6950 | `F: Working space separation wall AG1: inadmissible command` | *inadmissible operation in machine status:* Protection hoods are open | *by operator:* Close protection hoods |
| 6951 | `M: Working space separation wall AG1: position not defined` | *inadmissible operation in machine status:* Working space separation wall not in any end position | *by operator:* "Close" or "open" command (M command [closing bracket missing in print] |
| 6952 | `F: Working space separation wall AG1: monitoring triggered` | *incorrect setting, limit switch defective:* Selected status left without reason | *by technologist, setter:* Check hydraulic system and limit switches |
| 6953 | `F: Working space separation wall AG1: maximum performance time exceeded` | *incorrect setting, limit switch defective:* Command not performed during the time allowed | *by technologist, setter:* Check hydraulic system and limit switches |
| 7000 | `F: ISM cooling AG1: ventilator heat exchanger motor overcurrent` | *possible defect:* Motor protection switch triggered | *by electrical maintenance personnel:* Check current setting |

### Page 9-115

(Running header reads "7001 F:" although entry 7001 is a W message.)

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 7001 | `W: ISM cooling AG1: ventilator heat exchanger motor overcurrent` | *possible defect:* Motor protection switch triggered | *by electrical maintenance personnel:* Check current setting |
| 7002 | `F: ISM cooling AG1: pump motor overcurrent` | *possible defect:* Motor protection switch triggered | *by electrical maintenance personnel:* Check current setting |
| 7003 | `W: ISM cooling AG1: pump motor overcurrent` | *possible defect:* Motor protection switch triggered | *by electrical maintenance personnel:* Check current setting |
| 7050 | `F: Switch cabinet AG1: over-temperature` | *possible defect:* Switch cabinet over-temperature | *by mechanical maintenance personnel:* Check cooling unit, excessive ambient temperature? |
| 7051 | `W: Switch cabinet AG1: over temperature` | *possible defect:* Switch cabinet over temperature | *by mechanical maintenance personnel:* Check cooling unit, excessive ambient temperature? |
| 7100 | `F: ISM temperature control AG1: AEM module not operational` | *possible defect:* The AEM module is not operational | *by electrical maintenance personnel:* Check AEM module |

### Page 9-116

| No. | Message | Cause | Overcome |
|---|---|---|---|
| 7101 | `W: ISM temperature control AG1: AEM module not operational` | *possible defect:* The AEM module is not operational | *by electrical maintenance personnel:* Check AEM module |
| 7102 | `F: ISM temperature control AG1: max. rear bearing temperature exceeded` | *possible defect or incorrect setting,* max. rear bearing temperature parameter exceeded | *by electrical/mechanical maintenance personnel:* Permit bearing to cool, check bearing, check parameter |
| 7103 | `W: ISM temperature control AG1: max. rear bearing temperature exceeded` | *possible defect or incorrect setting,* max. rear bearing temperature parameter exceeded | *by electrical/mechanical maintenance personnel:* Permit bearing to cool, check bearing, check parameter |
| 7104 | `F: ISM temperature control AG1: max. front bearing temperature exceeded` | *possible defect or incorrect setting,* max. front bearing temperature parameter exceeded | *by electrical/mechanical maintenance personnel:* Permit bearing to cool, check bearing, check parameter |
| 7105 | `W: ISM temperature control AG1: max. front bearing temperature exceeded` | *possible defect or incorrect setting,* max. front bearing temperature parameter exceeded | *by electrical/mechanical maintenance personnel:* Permit bearing to cool, check bearing, check parameter |
| 7106 | `F: ISM temperature control AG1: max. motor winding temperature exceeded` | *possible defect or incorrect setting, parameter or overload in cycle:* Max. motor winding temperature exceeded | *by electrical/mechanical maintenance personnel:* Permit motor to cool, check motor, check parameter |

(The list continues on page 9-117, in part d.)

## For the retrofit

- Control/PLC: the PLC I/O is "Eltromatic" modules with a run-up sequence (phases 0, 1, 3, 4, service channel, cyclic operation), module numbers, earth monitoring, and a parameter check against the fitted equipment (5400-5410, p. 9-79 to 9-81). The NC must answer PLC signals within 300 ms (5951, p. 9-90). The control make/model is not named on these pages. Every message says "AG1"; whether a second set exists for the other side is not shown here.
- Axes: slides X and Z each have a servo "chopper" with temperature pre-warning; the X axis motor has a ventilator with its own motor protection (6450-6461, p. 9-103 to 9-105). 5950 mentions a "slaving fault overflow compound slide rest / C axis" (p. 9-90).
- Main spindle: drive overtemperature/pre-warning, auxiliary ventilator (5650-5656, p. 9-85/86); main drive gearing with positions and monitoring (5450-5453, p. 9-81/82); spindle clamping only inside a position window reached by C-axis positioning (5700-5703, p. 9-86); C axis has a pressure switch and a limit switch (6800, p. 9-113). Speed monitoring: spindle must stay < 50 rpm in setting mode (6850, p. 9-113). ISM spindle temperature control: rear/front bearing, winding, ambient and cooling water, with hysteresis, MAX, MAXMAX and set point parameters, AEM module, heat exchanger fan and pump (6014-6029, 7000-7003, 7100-7106, p. 9-92 to 9-116).
- Main chuck: clamp/release by M command or foot switch, front/rear stroke proximity switches, pressure switches per clamping direction, 20 s max. clamp/release time, no chuck command while the spindle turns, spindle may not run more than 90 s with the chuck open, max. chuck speed parameter, chuck key switch on the panel for "change position" (5550-5602, p. 9-83 to 9-85). Swivel chuck: 20 s max. swivel time, speed limit for swivelling (6651-6655, p. 9-109/110).
- Tailstock/centre sleeve: sleeve pressure switch and proximity switches, forward/return by M command or foot switch; traversable tailstock with 2 end positions (limit switches), `FORWARD`/`REVERSE` keys, teach-in module (6100-6153, p. 9-95 to 9-97). Steady rests 1 and 2 with open/closed pressure switches (5900-5904, p. 9-89).
- Turret ("tool holder"): swivelled by T command; position generator with parity check, preindexing, lock initiator "Control interlocking", overload clutch, reference traverse, "disc disengaged"/"spindle disengaged" signals (6500-6514, p. 9-105 to 9-107). Driven tools need the hood closed, a spindle release and a driven-tool position; coupling with M103/M104/M105 and an engaged/disengaged proximity switch (6750-6755, p. 9-112/113).
- Hydraulics: filter differential pressure, system pressure, min. level, motor overcurrent, oil over temperature (5750-5757, p. 9-87/88). Lubrication: pressure switches for circuits 1 and 2 (no pressure / permanent pressure), fresh-oil level, interval and suppression time parameters; suppression is withdrawn by M49, M18, M30, M99 or cycle stop (6030/6031, 6250-6269, p. 9-95, 9-98 to 9-101). Coolant motor and flow monitoring (5800/5801, p. 9-88). Switch cabinet cooling: filter, overcurrent, over temperature (6200-6205, 7050/7051).
- Safety: automatic protection hood with safety rail, safety circuit signal sequence check, emergency release, max. open/close time (parameter 031) (6300-6306, p. 9-101/102); setting operation needs hood open and manual mode (5350/5351, p. 9-79); `Drive on` button with lamp, emergency stop (6552/6553, p. 9-108); control voltage monitoring of input, output and valve modules (6550, 6551, 6555).
- Part handling/transfer: handling system/gantry (5500-5504, p. 9-82), loading flap (5850-5852), bar operation with M94 (6400-6402, p. 9-102/103), feeding aid (6601/6602), pick-off device with gripper, flap, swivel arm and conveyor belt with transport channel (6700-6711, p. 9-110 to 9-112), and a hydraulic working space separation wall opened/closed by M command with end limit switches, inadmissible with hoods open (6950-6953, p. 9-114). These pages do not give the M-codes for gripper, flap, separation wall or transfer.
- Chip conveyor motor protection (6350/6351, p. 9-102).
