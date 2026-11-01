# Gildemeister CTX twin operating manual, 9. Parameter setting, M-commands, PLC error messages (Parametereinstellung, M-Befehle, SPS-Fehlermeldungen), pages 9-85 to 9-136

Source: `7fc10043-9_Parametereinstellung_M-Befehle_SPS-Fehlermeldungen_c.pdf`, 52 scanned pages (part c of 4 of chapter 9), manual pages 9-85 to 9-136. "Betriebsanleitung CTX twin, Dok.-Nr.: daaab1d1, Februar 1995".
Original language: German.

Running page header on every page: "9. Parametereinstellung, M-Befehle, SPS-Fehlermeldungen" plus, at the top right, the number of the first message on the page (e.g. "1508 F:"). Footer: "Betriebsanleitung CTX twin", "Dok.-Nr.: daaab1d1, Februar 1995", "Seite 9-xx".

**This part starts in the middle of the PLC message list (`SPS-Fehlermeldungen`)**: the first entry on page 9-85 is message 1508 F; messages before 1508 and the introduction to the list (meaning of the letters F/M/W, the cause and remedy categories) are in part b. The list does not end here: the last entry on page 9-136 is 6800 F and the list continues in part d. These 52 pages contain only PLC messages (no parameter tables or M-code list; those are in the earlier parts, e.g. "9.1.2 Parameterliste" on page 9-6, cited by several messages).

Each entry in the manual has the form: number + type letter, message text (bold), then `Ursache:` (cause) with a cause category and a description, and `Behebung:` (remedy) with who must act (`durch ...`) and what to do.

Type letters (the meaning is defined in part b; reading here [uncertain]): `F` = `Fehler` (error), `M` = `Meldung` (message), `W` = `Warnung` (warning). Several messages exist twice, as F and W (or F and M), with the same text.

Recurring standard phrases, translated the same way throughout the tables below:

| German | English |
|---|---|
| `Bedienung in dem Maschinenstatus unzulässig` | operation not permitted in this machine state |
| `falsche Einstellung, Parameter` | wrong setting, parameter |
| `möglicher Defekt` | possible defect |
| `Überlastung` / `Überlastung im Zyklus` | overload / overload in the cycle |
| `durch Bediener` | by the operator |
| `durch Technologe, Einrichter` | by the technologist / setter |
| `durch Wartungspersonal Elektrik` / `Mechanik` | by electrical / mechanical maintenance staff |
| `Kommando unzulässig` | command not permitted |
| `Stellung undefiniert` | position undefined |
| `Überwachung angesprochen` | monitoring tripped |
| `Der angewählte Status wurde grundlos verlassen` | the selected state was left without reason |
| `Sicherungsautomat hat ausgelöst` | circuit breaker has tripped |
| `Ursache der Überlastung beseitigen` | remove the cause of the overload |
| `Motorschutzschalter hat ausgelöst` | motor protection switch has tripped |
| `Stromeinstellung prüfen` | check the current setting |
| `Fehler quittieren` | acknowledge the error |
| `Teileprogramm korrigieren` | correct the part program |
| `Näherungsschalter` | proximity switch |
| `Endschalter` / `Endlage` / `Endstellung` | limit switch / end position |
| `Steller` | (drive) power controller / converter |
| `Haube` / `Schutzhaube` | (protective) hood, i.e. the machine door/guard |
| `==> nicht verwendet` | ==> not used |

## 9.x PLC error messages (SPS-Fehlermeldungen), continued

### Tool carrier (Werkzeugträger), continued; I/O modules; machine (pages 9-85 to 9-87)

| No. | Type | Message (German) | Message (English) | Cause | Remedy |
|---|---|---|---|---|---|
| 1508 | F | `Werkzeugträger: T-Wert konnte nicht übernommen werden` | Tool carrier: T value could not be accepted | Operation not permitted in this machine state. Execution was blocked because of the machine state. | By the operator: correct the machine state (e.g. close the hood, `Haube schließen`). |
| 1509 | F | `Werkzeugträger: Vorindexierung fehlerhaft` | Tool carrier: pre-indexing faulty | Operation not permitted in this machine state. During the tool-carrier test the pre-indexing was faulty. | By the operator: swivel (index) the tool carrier with a T command. |
| 1510 | F | `Werkzeugträger: Werkzeugträger verriegelt nicht` | Tool carrier: tool carrier does not lock | Wrong setting, parameter. Initiator (proximity sensor) `Kontrolle - Verriegelung` (check - locking) does not switch. | By technologist, setter: check the initiator, readjust and test if necessary. |
| 1511 | F | `Werkzeugträger: Überlastkupplung angesprochen` | Tool carrier: overload clutch tripped | Possible defect or overload. The overload clutch has tripped. | By electrical / mechanical maintenance staff: find the cause of the fault and re-set up (realign) the tool carrier. |
| 1512 | F | `Werkzeugträger: Werkzeugträger entriegelt nicht` | Tool carrier: tool carrier does not unlock | Wrong setting, parameter or possible defect. The tool carrier does not unlock within the specified time. | By electrical / mechanical maintenance staff: find the cause and re-set up the tool carrier if necessary. |
| 1513 | F | `Werkzeugträger: Werkzeugträger Referenzfahrt fehlerhaft` | Tool carrier: tool carrier reference run faulty | Wrong setting, parameter or possible defect. The tool carrier does not complete the reference run within the specified time. | By electrical / mechanical maintenance staff: find the cause and re-set up the tool carrier if necessary. |
| 1514 | F | `Werkzeugträger: interner Werkzeugträger-Fehler` | Tool carrier: internal tool carrier error | Possible defect. The tool carrier does not report unambiguously `Scheibe ausgekuppelt` (disk declutched) or `Spindel ausgekuppelt` (spindle declutched). | By electrical / mechanical maintenance staff: find the cause and re-set up the tool carrier. |
| 1550 | F | `Ausgabemodule: Steuerspannung ausgefallen` | Output modules: control voltage failed | Possible defect. Circuit breaker has tripped. | By electrical maintenance staff: remove the cause of the overload. |
| 1551 | F | `Eingabemodule: Steuerspannung ausgefallen` | Input modules: control voltage failed | Possible defect. Circuit breaker has tripped. | By electrical maintenance staff: remove the cause of the overload. |
| 1552 | F | `Maschine: Nicht eingeschaltet` | Machine: not switched on | Operation not permitted in this machine state. Drives not switched on at the control panel. | By the operator: press the button `Antriebe Ein` (drives on); the lamp in the button must light. |
| 1553 | F | `Maschine: Not-Aus` | Machine: emergency stop | Operation not permitted in this machine state. Emergency-stop button pressed. | By the operator: release (unlatch) the emergency-stop button. |
| 1554 | F | `Störung Maschine: Siehe Fehlerliste` | Machine fault: see fault list | Possible defect. Only with certain options, see circuit diagram (`Stromlaufplan`) I 04.1. | By electrical maintenance staff: remove the fault. |
| 1555 | F | `Ventile: Steuerspannung ausgefallen` | Valves: control voltage failed | Possible defect. Circuit breaker has tripped. | By electrical maintenance staff: remove the cause of the overload. |
| 1556 | F | `Warnung Maschine: Siehe Fehlerliste` | Machine warning: see fault list | Possible defect. Only with certain options, see circuit diagram I 04.2. | By electrical maintenance staff: remove the fault. |
| 1557 | W | `Warnung Maschine: Siehe Fehlerliste` | Machine warning: see fault list | Possible defect. Only with certain options, see circuit diagram I 04.2. | By electrical maintenance staff: remove the fault. |
| 1558 | F | `Maschine: Spannungsversorgung Maschinenlampe / Lüfter ausgefallen` | Machine: power supply for machine lamp / fan failed | Possible defect. Circuit breaker has tripped. | By electrical maintenance staff: remove the cause of the overload. |
| 1559 | W | `Maschine: Spannungsversorgung Maschinenlampe / Lüfter ausgefallen` | Machine: power supply for machine lamp / fan failed | Possible defect. Circuit breaker has tripped. | By electrical maintenance staff: remove the cause of the overload. |
| 1560 | F | `Maschine: Sicherheitskreis fehlerhaft` | Machine: safety circuit faulty | Possible defect. Circuit breaker has tripped. | By electrical maintenance staff: remove the cause of the overload. |

### Loading aid, swivel chuck, unloading device (pages 9-88 to 9-91)

| No. | Type | Message (German) | Message (English) | Cause | Remedy |
|---|---|---|---|---|---|
| 1600 | F | `Einlegehilfe: Kommando unzulässig` | Loading aid: command not permitted | ==> not used (`nicht verwendet`) | |
| 1601 | M | `Einlegehilfe: Stellung undefiniert` | Loading aid: position undefined | Operation not permitted in this machine state. Loading aid is not in an end position. | By the operator: move the loading aid to the required end position by M command. |
| 1602 | F | `Einlegehilfe: Überwachung angesprochen` | Loading aid: monitoring tripped | Wrong setting, parameter or possible defect. Loading aid has left the end position without a command. | By electrical or mechanical maintenance staff: loading aid (check hydraulics and limit switches). |
| 1650 | M | `Schwenkfutter: Änderung der Anwahl unzulässig` | Swivel chuck: change of selection not permitted | ==> not used | |
| 1651 | F | `Schwenkfutter: Kommando unzulässig` | Swivel chuck: command not permitted | Operation not permitted in this machine state. Spindle turning too fast or the swivel enable from the hardware is missing (`Schwenkerlaubnis der Hardware fehlt`). | By the operator: repeat at a lower main spindle speed; if necessary check the parameter for maximum swivel speed. |
| 1652 | M | `Schwenkfutter: Stellung undefiniert` | Swivel chuck: position undefined | Swivel command aborted. The swivel chuck is in an undefined intermediate position. | By the operator: swivel the chuck into any position. |
| 1653 | F | `Schwenkfutter: Überwachung angesprochen` | Swivel chuck: monitoring tripped | Possible defect. The swivel chuck has left its current state without a command. | By technologist, setter: check swivel chuck settings (proximity switches, pressure switches). |
| 1654 | F | `Schwenkfutter: Zielposition nicht gefunden` | Swivel chuck: target position not found | Possible defect. The swivel chuck has not found the programmed position. | By technologist, setter: check swivel chuck proximity switch settings. |
| 1655 | F | `Schwenkfutter: Maximale Schwenkzeit überschritten` | Swivel chuck: maximum swivel time exceeded | Possible defect. The swivel chuck could not complete the swivel within 20 seconds. | By technologist, setter: check swivel chuck settings (pressure switches, pressure reducers) and valves. |
| 1700 | M | `Abholeinrichtung Greifen: Stellung undefiniert` | Unloading (pick-up) device, gripping: position undefined | Operation not permitted in this machine state. Gripper is in neither of its two end positions. | By the operator: command `Greifen` (grip) or `Lösen` (release) (M command or push-button). |
| 1701 | F | `Abholeinrichtung Greifen: Überwachung angesprochen` | Unloading device, gripping: monitoring tripped | Wrong setting, parameter. The selected state was left without reason. | By technologist, setter: check proximity switch `Greifen`. |
| 1702 | M | `Abholeinrichtung Klappe: Stellung undefiniert` | Unloading device, flap: position undefined | Operation not permitted in this machine state. Flap is in neither of its two end positions. | By the operator: command `Auf` (open) or `Zu` (close) (M command or push-button). |
| 1703 | F | `Abholeinrichtung Klappe: Überwachung angesprochen` | Unloading device, flap: monitoring tripped | Wrong setting, parameter. The selected state was left without reason. | By technologist, setter: check proximity switch `Klappe`. |
| 1704 | M | `Abholeinrichtung Schwenken: Stellung undefiniert` | Unloading device, swivelling: position undefined | Operation not permitted in this machine state. Swivel arm is in neither of its two end positions. | By the operator: command `Vor` (forward) or `Zurück` (back) (M command or push-button). |
| 1705 | F | `Abholeinrichtung Schwenken: Überwachung angesprochen` | Unloading device, swivelling: monitoring tripped | Wrong setting, parameter. The selected state was left without reason. | By technologist, setter: check proximity switch `Schwenken`. |
| 1706 | F | `Abholeinrichtung: Kommando unzulässig` | Unloading device: command not permitted | Operation not permitted in this machine state. The command is not permitted in this machine state. | By the operator or electrical maintenance staff: change the machine state and repeat the command. |
| 1707 | F | `Abholeinrichtung: Maximale Ausführungszeit überschritten` | Unloading device: maximum execution time exceeded | Wrong setting, parameter. Pick-up command was not executed within the specified time. | By technologist, setter: check the unloading device. |
| 1708 | F | `Abholeinrichtung Förderband: Ausführung fehlerhaft` | Unloading device conveyor belt: execution faulty | Possible defect. The workpiece did not arrive in the conveyor channel within the maximum time set by parameter. | By technologist, setter: check unloading device / parameters. |
| 1709 | F | `Abholeinrichtung Förderband: Motorüberstrom` | Unloading device conveyor belt: motor overcurrent | Possible defect. Motor protection switch has tripped. | By electrical maintenance staff: check the current setting. |
| 1710 | W | `Abholeinrichtung Förderband: Motorüberstrom` | Unloading device conveyor belt: motor overcurrent | Possible defect. Motor protection switch has tripped. | By electrical maintenance staff: check the current setting. |
| 1711 | M | `Abholeinrichtung Förderband: Werkstück im Förderkanal` | Unloading device conveyor belt: workpiece in conveyor channel | Possible defect. The workpiece is in the conveyor channel while the conveyor belt is switched off. | By the operator: switch on the conveyor belt to transport the workpiece out of the conveyor channel. |
| 1712 | M | `Abholeinrichtung Austrager: Stellung undefiniert` | Unloading device ejector (discharger): position undefined | Operation not permitted in this machine state. The ejector is in neither of its two end positions. | By the operator: command `Auf` (open/up) or `Zu` (close) (M command or push-button). |
| 1713 | F | `Abholeinrichtung Austrager: Überwachung angesprochen` | Unloading device ejector: monitoring tripped | Wrong setting, parameter. The selected state was left without reason. | By technologist, setter: check proximity switch `Austrager`. |

### Driven tools, C axis, speed monitoring, working-area partition (pages 9-91 to 9-94)

| No. | Type | Message (German) | Message (English) | Cause | Remedy |
|---|---|---|---|---|---|
| 1750 | F | `angetriebene Werkzeuge: Kommando unzulässig` | Driven tools: command not permitted | Operation not permitted in this machine state. Protective hoods are open. The spindle enable for the driven tools is missing, or the tool carrier was not in a driven-tool position. | By the operator, technologist/setter or electrical maintenance staff: swivel in a driven-tool position or clear the cause of the missing spindle enable. |
| 1751 | F | `Angetriebene Werkzeuge: Kuppelvorgang fehlerhaft` | Driven tools: coupling operation faulty | Wrong setting, parameter. Coupling operation with M103 / M104 / M105 faulty. | By technologist, setter: check proximity switches `Eingekuppelt/Ausgekuppelt` (coupled/decoupled). |
| 1752 | F | `Angetriebene Werkzeuge: Kupplungsüberwachung angesprochen` | Driven tools: clutch monitoring tripped | Wrong setting, parameter. The selected state was left without reason. | By technologist, setter: check proximity switches coupled/decoupled. |
| 1753 | F | `angetriebene Werkzeuge: nicht betriebsbereit` | Driven tools: not ready for operation | Possible defect. The drive controller (`Steller`) for the driven tools has a fault. | By electrical maintenance staff: remove the fault, see the electrical service manual (`Service-Handbuch Elektrik`). |
| 1754 | F | `angetriebene Werkzeuge: Temperaturvorwarnung` | Driven tools: temperature pre-warning | Wrong setting, parameter, or possible defect, or overload in the cycle. The controller for the driven tools reports overtemperature. | By electrical maintenance staff: let the drive cool down, check the drive, check the controller. |
| 1755 | F | `Angetriebene Werkzeuge: Übertemperatur` | Driven tools: overtemperature | Wrong setting, parameter, overload in the cycle or possible defect. The drive for the driven tools reports overtemperature. | By electrical maintenance staff: let the drive cool down, check the drive, check the controller. |
| 1755 | W | `angetriebene Werkzeuge: Temperaturvorwarnung` | Driven tools: temperature pre-warning | Wrong setting, parameter, or possible defect, or overload in the cycle. The drive for the driven tools reports overtemperature. | By electrical maintenance staff: let the drive cool down, check the drive, check the controller. (Number printed as 1755 W, same as 1755 F; probably 1756 W [uncertain].) |
| 1800 | F | `C-Achse Hauptspindel: Überwachung angesprochen` | C axis main spindle: monitoring tripped | Wrong setting, parameter or possible defect. C-axis pressure switch and limit switch misadjusted or defective. | By electrical or mechanical maintenance staff: check pressure switch and limit switch, replace if necessary. |
| 1801 | F | `C-Achse Hauptspindel: Kommando unzulässig` | C axis main spindle: command not permitted | Possible defect. The C axis was switched on in a gear stage other than 1. | By technologist, setter: switch off the C axis, shift the gearbox to stage 1 and repeat. |
| 1850 | F | `Drehzahlüberwachung: Gerät gestört oder angesprochen` | Speed monitoring: device faulty or tripped | Possible defect. The main spindle has exceeded the maximum permissible speed. The maximum speeds are 50 min⁻¹ in setup mode (`Einrichtebetrieb`) and 5000 min⁻¹ in operation with the hood closed. | By electrical maintenance staff: check main spindle drive and monitoring device. |
| 1900 | F | `Siehe SPS-Fehlerliste` | See PLC fault list | ==> not used | |
| 1901 | M | `Siehe SPS-Fehlerliste` | See PLC fault list | ==> not used | |
| 1902 | W | `Siehe SPS-Fehlerliste` | See PLC fault list | ==> not used | |
| 1950 | F | `Arbeitsraumtrennwand: Kommando unzulässig` | Working-area partition wall: command not permitted | Operation not permitted in this machine state. Protective hoods are open. | By the operator: close the protective hoods. |
| 1951 | M | `Arbeitsraumtrennwand: Stellung undefiniert` | Working-area partition wall: position undefined | Operation not permitted in this machine state. Partition wall is in neither of its two end positions. | By the operator: command `schließen` (close) or `öffnen` (open) (M command). |
| 1952 | F | `Arbeitsraumtrennwand: Überwachung angesprochen` | Working-area partition wall: monitoring tripped | Wrong setting, limit switch defective. The selected state was left without reason. | By technologist, setter: check hydraulics and limit switches. |
| 1953 | F | `Arbeitsraumtrennwand: maximale Ausführungszeit überschritten` | Working-area partition wall: maximum execution time exceeded | Wrong setting, limit switch defective. Command was not executed within the specified time. | By technologist, setter: check hydraulics and limit switches. |

### ISM cooling, control cabinet, ISM temperature control, power supply, PLC (pages 9-94 to 9-98)

ISM: built-in spindle motor (motor spindle) [interpretation; the abbreviation is not explained on these pages]. AEM module: temperature measuring module of the ISM temperature control [interpretation].

| No. | Type | Message (German) | Message (English) | Cause | Remedy |
|---|---|---|---|---|---|
| 2000 | F | `ISM-Kühlung: Lüfter Wärmetauscher Motorüberstrom` | ISM cooling: heat exchanger fan motor overcurrent | Possible defect. Motor protection switch has tripped. | By electrical maintenance staff: check the current setting. |
| 2001 | W | `ISM-Kühlung: Lüfter Wärmetauscher Motorüberstrom` | ISM cooling: heat exchanger fan motor overcurrent | Possible defect. Motor protection switch has tripped. | By electrical maintenance staff: check the current setting. |
| 2002 | F | `ISM-Kühlung: Pumpe Motorüberstrom` | ISM cooling: pump motor overcurrent | Possible defect. Motor protection switch has tripped. | By electrical maintenance staff: check the current setting. |
| 2003 | W | `ISM-Kühlung: Pumpe Motorüberstrom` | ISM cooling: pump motor overcurrent | Possible defect. Motor protection switch has tripped. | By electrical maintenance staff: check the current setting. |
| 2050 | F | `Schaltschrank: Übertemperatur` | Control cabinet: overtemperature | Possible defect. Control cabinet overtemperature. | By mechanical maintenance staff: check the cooling unit, or ambient temperature too high? |
| 2051 | W | `Schaltschrank: Übertemperatur` | Control cabinet: overtemperature | Possible defect. Control cabinet overtemperature. | By mechanical maintenance staff: check the cooling unit, or ambient temperature too high? |
| 2100 | F | `ISM-Temperaturregelung: AEM-Modul nicht betriebsbereit` | ISM temperature control: AEM module not ready | Possible defect. The AEM module is not ready for operation. | By electrical maintenance staff: check the AEM module. |
| 2101 | W | `ISM-Temperaturregelung: AEM-Modul nicht betriebsbereit` | ISM temperature control: AEM module not ready | Possible defect. The AEM module is not ready for operation. | By electrical maintenance staff: check the AEM module. |
| 2102 | F | `ISM-Temperaturregelung: max. Lagertemperatur hinten überschritten` | ISM temperature control: max. rear bearing temperature exceeded | Possible defect or wrong setting, parameter. The max. rear bearing temperature was exceeded. | By electrical / mechanical maintenance staff: let the bearing cool down, check the bearing, check parameters. |
| 2103 | W | `ISM-Temperaturregelung: max. Lagertemperatur hinten überschritten` | ISM temperature control: max. rear bearing temperature exceeded | As 2102. | As 2102. |
| 2104 | F | `ISM-Temperaturregelung: max. Lagertemperatur vorn überschritten` | ISM temperature control: max. front bearing temperature exceeded | Possible defect or wrong setting, parameter. The max. front bearing temperature was exceeded (printed "Tagertemperatur", a typo). | By electrical / mechanical maintenance staff: let the bearing cool down, check the bearing, check parameters. |
| 2105 | W | `ISM-Temperaturregelung: max. Lagertemperatur vorn überschritten` | ISM temperature control: max. front bearing temperature exceeded | As 2104. | As 2104. |
| 2106 | F | `ISM-Temperaturregelung: max. Motorwicklungstemperatur überschritten` | ISM temperature control: max. motor winding temperature exceeded | Possible defect or wrong setting, parameter, or overload in the cycle. The max. motor winding temperature was exceeded. | By electrical / mechanical maintenance staff: let the motor cool down, check the motor, check parameters. |
| 2107 | W | `ISM-Temperaturregelung: max. Motorwicklungstemperatur überschritten` | ISM temperature control: max. motor winding temperature exceeded | As 2106. | As 2106. |
| 2108 | M | `ISM-Temperaturregelung: max. Umgebungstemperatur überschritten` | ISM temperature control: max. ambient temperature exceeded | Possible defect or wrong setting, parameter. The max. ambient temperature was exceeded. | By electrical maintenance staff: check the temperature sensor, check parameters. |
| 2109 | F | `ISM-Temperaturregelung: AEM-Modul reagiert nicht` | ISM temperature control: AEM module not responding | Possible defect or wrong setting, parameter. The AEM module does not respond properly to the measurement request. | By electrical maintenance staff: check the AEM module, the temperature sensors and the parameters. |
| 2110 | M | `ISM-Temperaturregelung: AEM-Modul reagiert nicht` | ISM temperature control: AEM module not responding | As 2109 ("falsch eingestellter Parameter", wrongly set parameter). | As 2109. |
| 2111 | W | `ISM-Temperaturregelung: AEM-Modul reagiert nicht` | ISM temperature control: AEM module not responding | As 2109. | As 2109. |
| 2112 | F | `ISM-Temperaturregelung: maxmax. Lagertemperatur hinten überschritten` | ISM temperature control: max-max (upper limit) rear bearing temperature exceeded | Possible defect or wrong setting, parameter. The maxmax. rear bearing temperature was exceeded. | By electrical / mechanical maintenance staff: let the bearing cool down, check the bearing, check parameters. |
| 2113 | F | `ISM-Temperaturregelung: maxmax. Lagertemperatur vorn überschritten` | ISM temperature control: max-max front bearing temperature exceeded | As 2112, front bearing. | As 2112. |
| 2114 | F | `ISM-Temperaturregelung: maxmax. Motorwicklungstemperatur überschritten` | ISM temperature control: max-max motor winding temperature exceeded | Possible defect or wrong setting, parameter, or overload in the cycle. The maxmax. motor winding temperature was exceeded. | By electrical / mechanical maintenance staff: let the motor cool down, check the motor, check parameters. |
| 2150 | F | `Versorgung: nicht betriebsbereit` | Supply (power supply module): not ready | Possible defect. The supply has a fault. | By electrical maintenance staff: remove the fault, see the electrical service manual. |
| 2151 | F | `Versorgung: Temperaturvorwarnung` | Supply: temperature pre-warning | Possible defect or wrong setting, parameter, or overload in the cycle. The supply reports overtemperature. | By electrical maintenance staff: check the supply. |
| 2152 | W | `Versorgung: Temperaturvorwarnung` | Supply: temperature pre-warning | As 2151. | As 2151. |
| 2250 | F | `SPS: interner Softwarefehler` | PLC: internal software error | Internal PLC error. | By the setter, or Gildemeister service if necessary: check the PLC parameter settings and switch the machine off and on again; please inform Gildemeister service. |

### Counter-spindle clamping, chuck, drive, C axis, cover (pages 9-98 to 9-101)

| No. | Type | Message (German) | Message (English) | Cause | Remedy |
|---|---|---|---|---|---|
| 2450 | M | `Spanner Gegenspindel: Ausklinkhub angewählt` | Counter-spindle clamping cylinder: release (unlatching) stroke selected | Operation not permitted in this machine state. In the parameter `Bedienung` (operation) the release stroke is selected. | By the operator: correct the parameter, see parameter list. |
| 2451 | F | `Spanner Gegenspindel: Druckeinbruch` | Counter-spindle clamping: pressure drop | Wrong setting, parameter. Pressure switch `Spanndruck` (clamping pressure) reported a pressure drop. | By technologist, setter: check the pressure switch. |
| 2452 | F | `Spanner Gegenspindel: Fehlerhafte Eingabesignale` | Counter-spindle clamping: faulty input signals | Wrong setting, parameter. Wrong signals occurred during clamping or releasing. | By technologist, setter: check the settings (`Nutzhub`, working stroke; `Spanndruck`, clamping pressure). |
| 2453 | F | `Spanner Gegenspindel: Hubbegrenzung hinten fehlerhaft` | Counter-spindle clamping: rear stroke limit faulty | Possible defect. Proximity-switch test `Näherungsschalter Hinten` (rear proximity switch) faulty. | By electrical maintenance staff: replace the proximity switch, check the signal paths if necessary. |
| 2454 | M | `Spanner Gegenspindel: Hubbegrenzung Spannen erreicht` | Counter-spindle clamping: stroke limit "clamp" reached | Wrong setting, parameter. The clamping cylinder has reached the end position `Spannen` (clamp). | By technologist, setter: check the setting (`Nutzhub`, working stroke). |
| 2455 | F | `Spanner Gegenspindel: Hubbegrenzung vorn fehlerhaft` | Counter-spindle clamping: front stroke limit faulty | Possible defect. Proximity-switch test `Näherungsschalter vorn` (front proximity switch) faulty. | By electrical maintenance staff: replace the proximity switch, check the signal paths if necessary. |
| 2456 | F | `Spanner Gegenspindel: Spindellaufzeit ohne Spanndruck überschritten` | Counter-spindle clamping: spindle running time without clamping pressure exceeded | Operation not permitted in this machine state. Spindle ran with the chuck open for longer than 90 seconds. | By the operator: acknowledge the error, correct the part program if necessary. |
| 2457 | M | `Spanner Gegenspindel: Stellung undefiniert` | Counter-spindle clamping: position undefined | Operation not permitted in this machine state. Clamping cylinder is neither clamped nor fully open. | By the operator: execute command `Spannen` (clamp) or `Lösen` (release) (M command or foot switch). |
| 2458 | F | `Spanner Gegenspindel: Überwachung angesprochen` | Counter-spindle clamping: monitoring tripped | Wrong setting, parameter. The selected state was left without reason. | By technologist, setter: check the settings (working stroke, clamping pressure). |
| 2502 | F | `Spannfutter Gegenspindel: Kommando unzulässig` | Counter-spindle chuck: command not permitted | Operation not permitted in this machine state. Chuck command not allowed in this machine state. | By the operator: acknowledge the error, correct the part program if necessary. |
| 2550 | F | `Gegenspindelantrieb: Motorübertemperatur` | Counter-spindle drive: motor overtemperature | Operation not permitted in this machine state. Motor overloaded. | By electrical maintenance staff: check the controller and let the motor cool down. |
| 2551 | W | `Gegenspindelantrieb: Motorübertemperatur` | Counter-spindle drive: motor overtemperature | As 2550. | As 2550. |
| 2552 | F | `Gegenspindelantrieb: Nicht betriebsbereit` | Counter-spindle drive: not ready | Possible defect. Counter-spindle drive controller faulty. | By electrical maintenance staff: remove the fault, see the electrical service manual. |
| 2555 | F | `Gegenspindelantrieb: Temperaturvorwarnung` | Counter-spindle drive: temperature pre-warning | Operation not permitted in this machine state. Motor overloaded. | By the operator or electrical maintenance staff: acknowledge the error, check controller and motor if necessary. |
| 2556 | W | `Gegenspindelantrieb: Temperaturvorwarnung` | Counter-spindle drive: temperature pre-warning | As 2555. | As 2555. |
| 2600 | F | `C-Achse Gegenspindel: Überwachung angesprochen` | C axis counter-spindle: monitoring tripped | Wrong setting, parameter or possible defect. The selected state was left without reason. | By electrical or mechanical maintenance staff: check the limit switches, replace if necessary. |
| 2601 | F | `C-Achse Gegenspindel: Kommando unzulässig` | C axis counter-spindle: command not permitted | Operation not permitted in this machine state. C-axis command not allowed in this machine state. | By the operator: acknowledge the error, correct the part program if necessary. |
| 2650 | M | `Gegenspindel: Abdeckung geöffnet` | Counter-spindle: cover open | Operation not permitted in this machine state. The counter-spindle cover is open. | By the operator: close the cover. |

### Tool measuring, auxiliary drives (page 9-101)

| No. | Type | Message (German) | Message (English) | Cause | Remedy |
|---|---|---|---|---|---|
| 2700 | F | `Werkzeugmessen: Messtaster darf nur im Einrichtebetrieb eingeschwenkt werden` | Tool measuring: probe may only be swivelled in in setup mode | Operation not permitted in this machine state. The probe was swivelled in in an operating mode other than setup mode. | By the operator: swivel the probe out again or select setup mode. |
| 2701 | M | `Werkzeugmessen: Anwahl nur bei eingeschwenktem Messtaster erlaubt` | Tool measuring: selection only allowed with probe swivelled in | Operation not permitted in this machine state. Tool measuring was selected although the probe was not yet swivelled in. | By the operator: deselect tool measuring. |
| 2702 | M | `Werkzeugmessen: Anwahl nur bei unbetätigtem Messtaster erlaubt` | Tool measuring: selection only allowed with probe not actuated | Operation not permitted in this machine state. Tool measuring was selected while the probe was actuated. | By the operator: deselect tool measuring. |
| 2750 | F | `Nebenantriebe: Allgemeiner Fehler` | Auxiliary drives: general error | Possible defect. A line contactor of the auxiliary drives did not drop out when the hood was opened. | By electrical maintenance staff: replace the defective contactor. |

### Machining group 1 (AG1): M function input, setup mode, Eltromatic I/O, main drive gearbox (pages 9-102 to 9-104)

`AG1` = `Arbeitsgruppe 1` / machining unit 1, i.e. the first of the two lathe sides [interpretation; not expanded on these pages]. Eltromatic: the I/O module system (`E/A-Module`, I/O modules) of the PLC.

| No. | Type | Message (German) | Message (English) | Cause | Remedy |
|---|---|---|---|---|---|
| 5300 | F | `Eingabe AG1: M-Funktion undefiniert` | Input AG1: M function undefined | Operation not permitted in this machine state. M function undefined for this machine equipment. | By the operator: acknowledge the error. |
| 5301 | F | `Eingabe AG1: M-Funktion wegen Maschinenzustand unzulässig` | Input AG1: M function not permitted because of machine state | Operation not permitted in this machine state. M function not permitted because of the machine state. | By the operator: acknowledge the error. |
| 5350 | F | `Einrichtebetrieb AG1: Anwahl unzulässig` | Setup mode AG1: selection not permitted | Operation not permitted in this machine state. Protective hood is closed, or operating mode is not `Handsteuern` (manual control). | By the operator: open the protective hood and select operating mode manual control. |
| 5351 | M | `Einrichtebetrieb AG1: Unzulässig` | Setup mode AG1: not permitted | As 5350. | As 5350. |
| 5400 | F | `Eltromatic AG1: Fehler Hochlauf Phase 3` | Eltromatic AG1: start-up error phase 3 | Possible defect. Error while testing the communication. | By electrical maintenance staff: check the I/O modules and the connections to the control. |
| 5401 | F | `Eltromatic AG1: Fehler Hochlauf Phase 4` | Eltromatic AG1: start-up error phase 4 | Possible defect. General error when starting cyclic communication. | As 5400. |
| 5402 | F | `Eltromatic AG1: Fehler Hochlauf Phase 0` | Eltromatic AG1: start-up error phase 0 | Possible defect, maintenance necessary. Error in the self-test of the I/O modules. | As 5400. |
| 5403 | F | `Eltromatic AG1: Fehler Hochlauf Phase 1` | Eltromatic AG1: start-up error phase 1 | Possible defect. General error in the identification phase. | As 5400. |
| 5404 | F | `Eltromatic AG1: Fehler Hochlauf Servicekanalfehler` | Eltromatic AG1: start-up error, service channel error | Possible defect. General error while parameterising the I/O modules. | As 5400. |
| 5405 | F | `Eltromatic AG1: Fehler im zyklischen Betrieb` | Eltromatic AG1: error in cyclic operation | Possible defect. General error during cyclic communication. | As 5400. |
| 5406 | F | `Eltromatic AG1: Kein EA-Modul identifiziert` | Eltromatic AG1: no I/O module identified | Possible defect. No I/O module could be identified. | As 5400. |
| 5407 | F | `Eltromatic AG1: Nicht unterstütztes Modul gefunden` | Eltromatic AG1: unsupported module found | Possible defect. An unknown I/O module was detected. | As 5400. |
| 5408 | F | `Eltromatic AG1: Ausrüstung und Parameter stimmen nicht überein` | Eltromatic AG1: equipment and parameters do not match | Possible defect or wrong setting, parameter. The Eltromatic equipment found at machine start-up does not match the entries in the associated parameters. | By electrical maintenance staff: check the I/O modules and parameters and match them to each other. |
| 5409 | F | `Eltromatic AG1: Fehlerhafte Masseverbindung am Modul <Modulnr.>` | Eltromatic AG1: faulty ground connection at module <module no.> | Possible defect. Faulty ground connection at the indicated Eltromatic module. | By electrical maintenance staff: check the ground connections at the indicated Eltromatic module. |
| 5410 | F | `Eltromatic AG1: Allgemeiner Fehler am Modul <Modulnr.>` | Eltromatic AG1: general error at module <module no.> | Possible defect. General error at the indicated Eltromatic module. | By electrical maintenance staff: check the indicated Eltromatic module and its connections. |
| 5450 | F | `Getriebe Hauptantrieb AG1: Kommando unzulässig` | Main drive gearbox AG1: command not permitted | Operation not permitted in this machine state. The command is not permitted in this machine state. | By the operator, electrical maintenance staff: change the machine state and repeat the command. |
| 5451 | F | `Getriebe Hauptantrieb AG1: Kuppelvorgang fehlerhaft` | Main drive gearbox AG1: shifting (coupling) operation faulty | Possible defect. The required position was not reached. | By the operator, electrical maintenance staff: repeat the command, check the gearbox control. |
| 5452 | M | `Getriebe Hauptantrieb AG1: Stellung undefiniert` | Main drive gearbox AG1: position undefined | Operation not permitted in this machine state. The gearbox is in none of the permitted positions. | By the operator: trigger a command for a permitted position. |

### AG1: main drive gearbox (cont.), handling, main clamping cylinder and chuck, main spindle drive and clamp (pages 9-105 to 9-110)

Page 9-108 and 9-111 carry the revision note "REV: dab9501 (05.05.1995) zu daaab1d1 Februar 1995"; entries marked with "!" in the margin (5603 M, 5803 F) are the revised/added entries.

| No. | Type | Message (German) | Message (English) | Cause | Remedy |
|---|---|---|---|---|---|
| 5453 | F | `Getriebe Hauptantrieb AG1: Überwachung angesprochen` | Main drive gearbox AG1: monitoring tripped | Possible defect. The required position was left without reason. | By the operator, electrical maintenance staff: repeat the command, check the gearbox control. |
| 5500 | F | `Handhabung AG1: Kommando unzulässig` | Handling (loader) AG1: command not permitted | Operation not permitted in this machine state, wrong setting, parameter. Not permitted because the state of the handling unit is wrong. | By the operator, technologist/setter: acknowledge the error, switch the handling unit to the correct state. |
| 5501 | M | `Handhabung AG1: Maschine von Handhabung ausgeschaltet` | Handling AG1: machine switched off by handling unit | (no category) The machine was switched off by the gantry (`Portal`) (no blanks, `keine Rohteile`). | By technologist, setter: refill blanks, switch the gantry and machine back on. |
| 5502 | F | `Handhabung AG1: Nicht betriebsbereit` | Handling AG1: not ready | Operation not permitted in this machine state. Handling unit not switched on or not ready. | By the operator, technologist/setter: check the handling unit. |
| 5503 | W | `Handhabung AG1: Nicht betriebsbereit` | Handling AG1: not ready | As 5502. | As 5502. |
| 5504 | M | `Handhabung AG1: Handhabung ist nicht im Automatikbetrieb` | Handling AG1: handling unit is not in automatic mode | Operation not permitted in this machine state. While the handling command was being processed the handling unit was not in automatic mode. | By the operator: set the handling unit to automatic and, if necessary, press cycle start of the lathe. |
| 5550 | M | `Hauptspanner AG1: Ausklinkhub angewählt` | Main clamping cylinder AG1: release stroke selected | Operation not permitted in this machine state. In the parameter `Bedienung` the release stroke is selected. | By the operator: correct the parameter, see 9.1.2 "Parameterliste" on page 9-6. |
| 5551 | F | `Hauptspanner AG1: Druckeinbruch` | Main clamping cylinder AG1: pressure drop | Wrong setting, parameter. Pressure switch `Spanndruck` (clamping pressure) reported a pressure drop. | By technologist, setter: check the corresponding pressure switch, depending on the clamping direction. |
| 5552 | F | `Hauptspanner AG1: Fehlerhafte Eingabesignale` | Main clamping cylinder AG1: faulty input signals | Wrong setting, parameter. Wrong signals occurred during clamping or releasing. | By technologist, setter: check the settings (`Nutzhub`, `Spanndruck`). |
| 5553 | F | `Hauptspanner AG1: Hubbegrenzung hinten fehlerhaft` | Main clamping cylinder AG1: rear stroke limit faulty | Possible defect. Proximity-switch test `Näherungsschalter Hinten` faulty. | By electrical maintenance staff: replace the proximity switch or check the signal path per circuit diagram. |
| 5554 | M | `Hauptspanner AG1: Hubbegrenzung Spannen erreicht` | Main clamping cylinder AG1: stroke limit "clamp" reached | Wrong setting, parameter. The main clamping cylinder reached the end position without pressure. | By technologist, setter: check the settings (working stroke, clamping pressure). |
| 5555 | F | `Hauptspanner AG1: Hubbegrenzung vorn fehlerhaft` | Main clamping cylinder AG1: front stroke limit faulty | Possible defect. Proximity-switch test `Näherungsschalter Vorn` faulty. | By electrical maintenance staff: replace the proximity switch or check the signal path per circuit diagram. |
| 5556 | F | `Hauptspanner AG1: Spindellaufzeit ohne Spanndruck überschritten` | Main clamping cylinder AG1: spindle running time without clamping pressure exceeded | Operation not permitted in this machine state. Spindle ran with the chuck open for longer than 90 seconds. | By the operator: acknowledge the error, correct the part program if necessary. |
| 5557 | M | `Hauptspanner AG1: Stellung undefiniert` | Main clamping cylinder AG1: position undefined | Operation not permitted in this machine state. Main clamping cylinder neither clamped nor fully open. | By the operator: command `Spannen` (clamp) or `Lösen` (release) (M command or foot switch). |
| 5558 | F | `Hauptspanner AG1: Überwachung angesprochen` | Main clamping cylinder AG1: monitoring tripped | Wrong setting, parameter. End position `Gespannt` (clamped) or `Geöffnet` (open) left without reason. | By technologist, setter: check the settings (working stroke, clamping pressure). |
| 5559 | F | `Hauptspanner AG1: Maximale Ausführungszeit überschritten` | Main clamping cylinder AG1: maximum execution time exceeded | Possible defect. The module `Spanner ohne Hubbegrenzung` (clamping cylinder without stroke limit) could not complete the clamp or release within 20 seconds. | By technologist, setter: check main clamping settings (pressure switches, pressure reducers) and valves. |
| 5600 | M | `Hauptspannfutter AG1: In Wechselposition` | Main chuck AG1: in change position | Operation not permitted in this machine state. Chuck key is not inserted at the control panel. | By the operator: insert the chuck key in the enable switch (`Freigabeschalter`) at the control panel. |
| 5601 | F | `Hauptspannfutter AG1: Max. Drehzahl überschritten` | Main chuck AG1: max. speed exceeded | Wrong setting, parameter. Speed was higher than the set chuck speed. | By technologist, setter: check the set speed, correct the part program if necessary. |
| 5602 | F | `Hauptspannfutter AG1: Kommando unzulässig` | Main chuck AG1: command not permitted | Operation not permitted in this machine state. Chuck command not permitted with the spindle turning. | By the operator: stop the spindle first. |
| 5603 | M | `Hauptspannfutter AG1: Werkstück liegt nicht an` | Main chuck AG1: workpiece not seated (not against the stop) | Workpiece not seated. | By the operator: open the chuck and clamp the workpiece again. (Entry marked "!" = revision 05.05.1995.) |
| 5650 | F | `Hauptspindelantrieb AG1: Motorübertemperatur` | Main spindle drive AG1: motor overtemperature | Operation not permitted in this machine state. Motor overloaded. | By electrical maintenance staff: check the controller and let the motor cool down. |
| 5651 | W | `Hauptspindelantrieb AG1: Motorübertemperatur` | Main spindle drive AG1: motor overtemperature | As 5650. | As 5650. |
| 5652 | F | `Hauptspindelantrieb AG1: Nicht betriebsbereit` | Main spindle drive AG1: not ready | Possible defect. Main spindle drive controller faulty. | By electrical maintenance staff: remove the fault, see the electrical service manual. |
| 5653 | F | `Hauptspindelantrieb AG1: Störung Fremdlüfter` | Main spindle drive AG1: separately driven fan fault | Possible defect. Motor protection switch has tripped. | By electrical maintenance staff: check the current setting. |
| 5654 | W | `Hauptspindelantrieb AG1: Störung Fremdlüfter` | Main spindle drive AG1: separately driven fan fault | As 5653. | As 5653. |
| 5655 | F | `Hauptspindelantrieb AG1: Temperaturvorwarnung` | Main spindle drive AG1: temperature pre-warning | Wrong setting, parameter, possible defect or overload in the cycle. The main spindle drive reports overtemperature. | By electrical maintenance staff: let the motor cool down, check the motor, check the controller. |
| 5656 | W | `Hauptspindelantrieb AG1: Temperaturvorwarnung` | Main spindle drive AG1: temperature pre-warning | As 5655. | As 5655. |
| 5700 | F | `Hauptspindelklemmung AG1: Ausführung fehlerhaft` | Main spindle clamp (brake) AG1: execution faulty | Wrong setting, parameter. Proximity switches of the spindle clamp give wrong signals. | By technologist, setter: check the proximity switches. |
| 5701 | F | `Hauptspindelklemmung AG1: Kommando unzulässig` | Main spindle clamp AG1: command not permitted | Operation not permitted in this machine state. Main spindle is not within the position window (`Lage - Positionsfenster`). | By the operator: first position the spindle with the C axis. |
| 5702 | F | `Hauptspindelklemmung AG1: Überwachung angesprochen` | Main spindle clamp AG1: monitoring tripped | Wrong setting, parameter. The selected state was left without reason. | By technologist, setter: check the proximity switches. |
| 5703 | M | `Hauptspindelklemmung AG1: Hauptspindel ist geklemmt` | Main spindle clamp AG1: main spindle is clamped | Main spindle is clamped. The main spindle has been clamped manually. | By the operator: release the main spindle clamp. |

### AG1: hydraulics, coolant, loading flap, steady rests, NC, parameter checks (pages 9-110 to 9-118)

| No. | Type | Message (German) | Message (English) | Cause | Remedy |
|---|---|---|---|---|---|
| 5750 | F | `Hydraulik AG1: Filter verstopft` | Hydraulics AG1: filter clogged | Maintenance necessary. Differential pressure before/after hydraulic filter too high. | By mechanical maintenance staff: change the hydraulic filter. |
| 5751 | W | `Hydraulik AG1: Filter verstopft` | Hydraulics AG1: filter clogged | As 5750. | As 5750. |
| 5752 | F | `Hydraulik AG1: Kein Systemdruck` | Hydraulics AG1: no system pressure | Maintenance necessary. Pressure loss or pressure drop in the hydraulics. | By mechanical maintenance staff: check oil level and tightness of the hydraulics. |
| 5753 | W | `Hydraulik AG1: Minimum Niveau erreicht` | Hydraulics AG1: minimum level reached | Maintenance necessary. Oil level in the hydraulic tank too low. | By mechanical maintenance staff: check the hydraulics for leaks, top up oil. |
| 5754 | F | `Hydraulik AG1: Minimum Niveau erreicht` | Hydraulics AG1: minimum level reached | As 5753. | As 5753. |
| 5755 | F | `Hydraulik AG1: Motorüberstrom` | Hydraulics AG1: motor overcurrent | Possible defect. Motor protection switch has tripped. | By electrical maintenance staff: check the current setting. |
| 5756 | W | `Hydraulik AG1: Übertemperatur Öl` | Hydraulics AG1: oil overtemperature | Possible defect. The temperature of the hydraulic unit is too high. | By mechanical maintenance staff: check the hydraulic unit (oil level, other). |
| 5757 | F | `Hydraulik AG1: Übertemperatur Öl` | Hydraulics AG1: oil overtemperature | As 5756. | As 5756. |
| 5800 | F | `Kühlmittel AG1: Motorüberstrom` | Coolant AG1: motor overcurrent | Possible defect. Motor protection switch has tripped. | By electrical maintenance staff: check the current setting. |
| 5801 | F | `Kühlmittel AG1: Strömungsüberwachung angesprochen` | Coolant AG1: flow monitoring tripped | Possible defect or maintenance necessary. Coolant supply faulty. | By electrical or mechanical maintenance staff: check the coolant supply. |
| 5803 | F | `Kühlmittel AG1: Kühlmittel verstopft` | Coolant AG1: coolant (filter) clogged | Maintenance necessary. Differential pressure before/after coolant filter too high. | By electrical or mechanical maintenance staff: change the coolant filter. (Entry marked "!" = revision 05.05.1995.) |
| 5850 | F | `Ladeklappe AG1: Kommando unzulässig` | Loading flap AG1: command not permitted | Operation not permitted in this machine state. Protective hood is open or the gripper is in the working area. | By the operator: close the protective hood and move the gripper out of the working area. |
| 5851 | M | `Ladeklappe AG1: Stellung undefiniert` | Loading flap AG1: position undefined | Operation not permitted in this machine state. Loading flap is neither open nor closed. | By the operator: trigger command `Öffnen` (open) or `Schließen` (close). |
| 5852 | F | `Ladeklappe AG1: Überwachung angesprochen` | Loading flap AG1: monitoring tripped | Wrong setting, parameter. The selected state was left without reason. | By technologist, setter: check the loading-flap limit switches. |
| 5900 | F | `Lünetten AG1: Kommando unzulässig` | Steady rests AG1: command not permitted | Operation not permitted in this machine state. Spindle is switched on, quill position not permitted. | By the operator: stop the spindle or clamp the quill. |
| 5901 | M | `Lünette 1 AG1: Stellung undefiniert` | Steady rest 1 AG1: position undefined | Operation not permitted in this machine state. Steady rest 1 is neither clamped nor fully open. | By the operator: command `Spannen` (clamp) or `Lösen` (release) (M command or push-button). |
| 5902 | F | `Lünette 1 AG1: Überwachung angesprochen` | Steady rest 1 AG1: monitoring tripped | Wrong setting, parameter. Clamped or open state was left. | By technologist, setter: check pressure switch `Geöffnet` (open) or `Gespannt` (clamped). |
| 5903 | M | `Lünette 2 AG1: Stellung undefiniert` | Steady rest 2 AG1: position undefined | Operation not permitted in this machine state. Steady rest 2 is neither clamped nor fully open. | By the operator: command `Spannen` or `Lösen` (M command or push-button). |
| 5904 | F | `Lünette 2 AG1: Überwachung angesprochen` | Steady rest 2 AG1: monitoring tripped | Wrong setting, parameter. Clamped or open state was left. | By technologist, setter: check pressure switch `Geöffnet` or `Gespannt`. |
| 5950 | F | `NC AG1: Nicht betriebsbereit` | NC AG1: not ready | Possible defect. Following-error overflow (`Schleppfehlerüberlauf`) cross slide / C axis. | By electrical maintenance staff: check feed drives and cross-slide encoders. |
| 5951 | F | `NC AG1: Maximale Reaktionszeit der NC überschritten` | NC AG1: maximum NC reaction time exceeded | Possible defect. The reaction time of the NC to PLC signals is greater than 300 ms. | By the operator, Gildemeister service if necessary: acknowledge the error, possibly switch the machine off and on again. Please inform Gildemeister service. |
| 6000 | F | `Parameter AG1: Übernahme nur bei Maschinenhochlauf` | Parameter AG1: accepted only at machine start-up | Wrong setting, parameter. If parameters were changed, switch off the machine. | By technologist, setter: change the parameter, switch the machine off and on again. |
| 6001 | F | `Parameter Abholeinrichtungstyp AG1: Wert unzulässig` | Parameter unloading-device type AG1: value not permitted | Wrong setting, parameter. Parameter `Handhabung und Materialzuführung` (handling and material feed) wrong. | By technologist, setter: correct the parameter, see 9.1.2 "Parameterliste" on page 9-6. |
| 6002 | F | `Parameter Getriebetyp Hauptantrieb AG1: Wert unzulässig` | Parameter main drive gearbox type AG1: value not permitted | Wrong setting, parameter. Value in parameter `Getriebedaten Hauptantrieb` (main drive gearbox data) wrong. | As 6001. |
| 6003 | F | `Parameter Handhabungstyp AG1: Typ unzulässig` | Parameter handling type AG1: type not permitted | Wrong setting, parameter. Parameter `Handhabung und Materialzuführung` wrong. | As 6001. |
| 6004 | F | `Parameter Hauptspanner AG1: Arbeitsrichtung unzulässig` | Parameter main clamping cylinder AG1: working direction not permitted | Wrong setting, parameter. Wrong value in parameter `Hauptspindel` (main spindle). | As 6001. |
| 6005 | F | `Parameter Hauptspannfutter AG1: Spannart unzulässig` | Parameter main chuck AG1: clamping type not permitted | Wrong setting, parameter. Wrong value in parameter `Hauptspindel`. | As 6001. |
| 6006 | F | `Parameter Hauptspannfutter AG1: Typ unzulässig` | Parameter main chuck AG1: type not permitted | Wrong setting, parameter. Wrong main chuck type in parameter `Hauptspindel`. | As 6001. |
| 6007 | F | `Parameter Lünettenanzahl AG1: Wert zu groß` | Parameter number of steady rests AG1: value too large | Wrong setting, parameter. Value in parameter `Lünette Beschreibung` (steady rest description) too large. | By technologist, setter: correct the parameter. |
| 6008 | F | `Parameter Materialzuführungstyp AG1: Wert unzulässig` | Parameter material-feed type AG1: value not permitted | Wrong setting, parameter. Parameter `Handhabung und Materialzuführung` wrong. | As 6001. |
| 6009 | F | `Parameter Pinole AG1: Typ unzulässig` | Parameter quill AG1: type not permitted | Wrong setting, parameter. Quill type wrongly selected in `Reitstock, Pinole` (tailstock, quill). | By technologist, setter: correct the parameter. |
| 6010 | F | `Parameter Reitstocktyp AG1: Wert unzulässig` | Parameter tailstock type AG1: value not permitted | Wrong setting, parameter. Value in parameter `Reitstock, Pinole` set wrongly. | As 6001. |
| 6011 | F | `Parameter Schwenkfuttertyp AG1: Wert unzulässig` | Parameter swivel chuck type AG1: value not permitted | ==> not used | |
| 6012 | F | `Parameter Werkzeugträger AG1: Typ unzulässig` | Parameter tool carrier AG1: type not permitted | Wrong setting, parameter. Wrong tool carrier type in parameter `Werkzeugträgerdaten` (tool carrier data). | As 6001. |
| 6013 | F | `Parameter Werkzeugträger AG1: Werkzeuganzahl unzulässig` | Parameter tool carrier AG1: number of tools not permitted | Wrong setting, parameter. Parameter `Aufnahmeanzahl u. Spindel` (number of stations and spindle) set wrongly. | As 6001. |

Messages 6014 to 6029 all have: cause "wrong setting, parameter; value in parameter `Temperaturregelung Spindeln` (spindle temperature control) wrong"; remedy "by technologist, setter: correct the parameter". Message text in each case: `Parameter ISM-Temperaturregelung AG1: Wert "<name>" unzulässig` (Parameter ISM temperature control AG1: value "<name>" not permitted).

| No. | Type | Value name (German) | Value name (English) |
|---|---|---|---|
| 6014 | F | `Anzahl Werte zur Mittelwertbildung` | number of values for averaging |
| 6015 | F | `Hysterese Kühlwassertemperatur` | hysteresis cooling-water temperature |
| 6016 | F | `Hysterese Lagertemperatur hinten` | hysteresis rear bearing temperature |
| 6017 | F | `Hysterese Lagertemperatur vorn` | hysteresis front bearing temperature |
| 6018 | F | `Hysterese Wicklungstemperatur` | hysteresis winding temperature |
| 6019 | F | `MAX Lagertemperatur hinten` | MAX rear bearing temperature |
| 6020 | F | `MAX Lagertemperatur vorn` | MAX front bearing temperature |
| 6021 | F | `MAX Umgebungstemperatur` | MAX ambient temperature |
| 6022 | F | `MAX Wicklungstemperatur` | MAX winding temperature |
| 6023 | F | `MAXMAX Lagertemperatur hinten` | MAXMAX rear bearing temperature |
| 6024 | F | `MAXMAX Lagertemperatur vorn` | MAXMAX front bearing temperature |
| 6025 | F | `MAXMAX Wicklungstemperatur` | MAXMAX winding temperature |
| 6026 | F | `Sollwert Kühlwassertemperatur` | set point cooling-water temperature |
| 6027 | F | `Sollwert Lagertemperatur hinten` | set point rear bearing temperature |
| 6028 | F | `Sollwert Lagertemperatur vorn` | set point front bearing temperature |
| 6029 | F | `Sollwert Wicklungstemperatur` | set point winding temperature |

| No. | Type | Message (German) | Message (English) | Cause | Remedy |
|---|---|---|---|---|---|
| 6030 | F | `Parameter Schmierung AG1: Wert "Schmierintervallzeit" unzulässig` | Parameter lubrication AG1: value "lubrication interval time" not permitted | Wrong setting, parameter. Value in parameter `Schmierintervallzeit` wrong. | By technologist, setter: correct the parameter. |
| 6031 | F | `Parameter Schmierung AG1: Wert "Schmierunterdrückungszeit" unzulässig` | Parameter lubrication AG1: value "lubrication suppression time" not permitted | Wrong setting, parameter. Value in parameter `Schmierunterdrückungszeit` wrong. | By technologist, setter: correct the parameter. |

### AG1: quill, tailstock, cabinet cooling, lubrication, protective hood (pages 9-118 to 9-124)

| No. | Type | Message (German) | Message (English) | Cause | Remedy |
|---|---|---|---|---|---|
| 6100 | F | `Pinole AG1: Druckeinbruch` | Quill AG1: pressure drop | Wrong setting, parameter. Quill clamped and pressure drop. | By technologist, setter: check the quill pressure switch setting. |
| 6101 | M | `Pinole AG1: Endstellung vorn erreicht` | Quill AG1: front end position reached | Wrong setting, parameter. During clamping the front end position was reached without pressure. | By technologist, setter: quill stroke not sufficient, move the tailstock. |
| 6102 | F | `Pinole AG1: Fehlerhafte Eingabesignale` | Quill AG1: faulty input signals | Wrong setting, parameter. Wrong signals occurred while moving the quill. | By technologist, setter: check the quill settings (proximity switches, pressure switches). |
| 6103 | F | `Pinole AG1: Kommando unzulässig` | Quill AG1: command not permitted | Operation not permitted in this machine state. Spindle is switched on, no steady rest clamped. | By the operator: stop the spindle or clamp a steady rest. |
| 6104 | M | `Pinole AG1: Stellung undefiniert` | Quill AG1: position undefined | Operation not permitted in this machine state. Quill neither clamped nor fully retracted. | By the operator: command `Vor` (forward) or `Zurück` (back) (M command or foot switch). |
| 6105 | F | `Pinole AG1: Überwachung angesprochen` | Quill AG1: monitoring tripped | Wrong setting, parameter. Clamped or retracted state left without reason. | By technologist, setter: check the quill settings (proximity switches, pressure switches). |
| 6150 | F | `Reitstock AG1: Kommando unzulässig` | Tailstock AG1: command not permitted | Operation not permitted in this machine state. Not permitted because quill extended or main spindle turning. | By the operator: acknowledge the error, correct the part program if necessary. |
| 6151 | F | `Reitstock AG1: Nicht betriebsbereit` | Tailstock AG1: not ready | Wrong setting, parameter. Teach-in module of the automatic tailstock faulty. | By technologist, setter: check the device, reprogram if necessary. |
| 6152 | M | `Reitstock AG1: Stellung undefiniert` | Tailstock AG1: position undefined | Operation not permitted in this machine state. The movable tailstock is in neither of its 2 end positions. | By the operator: move with M command or push-button `VOR` (forward) or `ZURÜCK` (back). |
| 6153 | F | `Reitstock AG1: Überwachung angesprochen` | Tailstock AG1: monitoring tripped | Wrong setting, parameter, possible defect. Limit switch of the movable tailstock misadjusted or defective. | By electrical maintenance staff, mechanical maintenance staff: check the limit switches, replace if necessary. |
| 6200 | F | `Schaltschrankkühlung AG1: Filter verstopft` | Control cabinet cooling AG1: filter clogged | Maintenance necessary. Differential pressure before/after filter mat too high. | By mechanical maintenance staff: clean or change the filter. |
| 6201 | W | `Schaltschrankkühlung AG1: Filter verstopft` | Control cabinet cooling AG1: filter clogged | As 6200. | As 6200. |
| 6202 | F | `Schaltschrankkühlung AG1: Überstrom` | Control cabinet cooling AG1: overcurrent | Possible defect. Motor protection switch has tripped. | By electrical maintenance staff: check the current setting. |
| 6203 | W | `Schaltschrankkühlung AG1: Überstrom` | Control cabinet cooling AG1: overcurrent | As 6202. | As 6202. |
| 6204 | F | `Schaltschrankkühlung AG1: Übertemperatur` | Control cabinet cooling AG1: overtemperature | Possible defect. Control cabinet overtemperature. | By mechanical maintenance staff: check the cooling unit; ambient temperature too high? |
| 6205 | W | `Schaltschrankkühlung AG1: Übertemperatur` | Control cabinet cooling AG1: overtemperature | As 6204. | As 6204. |
| 6250 | F | `Schmierung AG1: Kein Druck` | Lubrication AG1: no pressure | Maintenance necessary. Lubrication pressure switch reports no pressure. | By mechanical maintenance staff: refill the lubricating oil reservoir, check the pressure switch. |
| 6251 | W | `Schmierung AG1: Kein Druck` | Lubrication AG1: no pressure | As 6250. | As 6250. |
| 6252 | W | `Schmierung AG1: Minimum Niveau erreicht` | Lubrication AG1: minimum level reached | Maintenance necessary. Level of fresh-oil lubrication too low. | By mechanical maintenance staff: top up fresh oil. |
| 6253 | F | `Schmierung AG1: Minimum Niveau erreicht` | Lubrication AG1: minimum level reached | As 6252. | As 6252. |
| 6254 | F | `Schmierung AG1: Motorüberstrom` | Lubrication AG1: motor overcurrent | Possible defect. Motor protection switch has tripped. | By electrical maintenance staff: check the current setting. |
| 6255 | W | `Schmierung AG1: Motorüberstrom` | Lubrication AG1: motor overcurrent | As 6254. | As 6254. |
| 6256 | M | `Schmierung AG1: Öl nachfüllen` | Lubrication AG1: top up oil | Maintenance necessary. Level of fresh-oil lubrication not sufficient. | By mechanical maintenance staff: top up fresh oil. |
| 6257 | F | `Schmierung AG1: Permanenter Druck` | Lubrication AG1: permanent pressure | Maintenance necessary. Lubrication pressure switch reports permanent pressure. | By mechanical maintenance staff: check the lubrication pressure lines and/or pressure switch. |
| 6258 | W | `Schmierung AG1: Permanenter Druck` | Lubrication AG1: permanent pressure | As 6257. | As 6257. |
| 6259 | F | `Schmierung AG1: Kein Druck Kreis 1` | Lubrication AG1: no pressure circuit 1 | Maintenance necessary. Lubrication pressure switch reports no pressure. | By mechanical maintenance staff: refill the lubricating oil reservoir, check the pressure switch. |
| 6260 | F | `Schmierung AG1: Kein Druck Kreis 2` | Lubrication AG1: no pressure circuit 2 | As 6259. | As 6259. |
| 6261 | F | `Schmierung AG1: Permanenter Druck Kreis 1` | Lubrication AG1: permanent pressure circuit 1 | Maintenance necessary. Lubrication pressure switch reports permanent pressure. | By mechanical maintenance staff: check the lubrication pressure lines and/or pressure switch. |
| 6262 | F | `Schmierung AG1: Permanenter Druck Kreis 2` | Lubrication AG1: permanent pressure circuit 2 | As 6261. | As 6261. |
| 6263 | F | `Schmierung AG1: Kommando unzulässig` | Lubrication AG1: command not permitted | ==> not used | |
| 6264 | F | `Schmierung AG1: Maximale Unterdrückungszeit überschritten` | Lubrication AG1: maximum suppression time exceeded | Wrong setting, parameter or programming error. Maximum suppression time exceeded. | By technologist, setter: check the parameters; cancel the lubrication-pulse suppression with M49, M18, M30, M99 or cycle stop (`Zyklus Stop`). |
| 6265 | W | `Schmierung AG1: Kein Druck Kreis 1` | Lubrication AG1: no pressure circuit 1 | As 6259. | As 6259. |
| 6266 | W | `Schmierung AG1: Kein Druck Kreis 2` | Lubrication AG1: no pressure circuit 2 | As 6259. | As 6259. |
| 6267 | W | `Schmierung AG1: Permanenter Druck Kreis 1` | Lubrication AG1: permanent pressure circuit 1 | As 6261. | As 6261. |
| 6268 | W | `Schmierung AG1: Permanenter Druck Kreis 2` | Lubrication AG1: permanent pressure circuit 2 | As 6261. | As 6261. |
| 6269 | W | `Schmierung AG1: Maximale Unterdrückungszeit überschritten` | Lubrication AG1: maximum suppression time exceeded | As 6264. | As 6264. |
| 6300 | F | `Schutzhaube AG1: Max. Zeit zum auf/zufahren überschritten` | Protective hood AG1: max. time to open/close exceeded | Possible defect, wrong setting, parameter. Possible defect of the automatic protective hood. | By electrical / mechanical maintenance staff: check the function, possibly correct parameter 031. |
| 6301 | M | `Schutzhaube AG1: Motorüberstrom` | Protective hood AG1: motor overcurrent | Wrong setting, parameter, possible defect. Motor protection switch has tripped. | By electrical maintenance staff: check the current setting. |

### AG1: protective hood (cont.), chip conveyor, bar operation, feed drives (pages 9-125 to 9-128)

| No. | Type | Message (German) | Message (English) | Cause | Remedy |
|---|---|---|---|---|---|
| 6302 | M | `Schutzhaube AG1: Nicht geöffnet und nicht geschlossen` | Protective hood AG1: neither open nor closed | Operation not permitted in this machine state. Protective hood not locked and not open. | By the operator: open the protective hood and then close it. |
| 6303 | M | `Schutzhaube AG1: Öffnen nicht erlaubt, da Taster betätigt` | Protective hood AG1: opening not allowed because a button is pressed | Operation not permitted in this machine state. A button was pressed while opening the protective hood. | By the operator: press no button and then open the protective hood. |
| 6304 | M | `Schutzhaube AG1: Sicherheitsleiste hat angesprochen` | Protective hood AG1: safety edge has tripped | Operation not permitted in this machine state. Closing of the automatic protective hood interrupted. | By the operator: acknowledge the error, check the safety edge if necessary. |
| 6305 | F | `Schutzhaube AG1: Störung Sicherheitskreis` | Protective hood AG1: safety circuit fault | Possible defect. Protective-hood safety circuit has a wrong signal sequence. | By electrical maintenance staff: check the safety circuit (see circuit diagram). |
| 6306 | F | `Schutzhaube AG1: Unzulässig geöffnet` | Protective hood AG1: opened impermissibly | Operation not permitted in this machine state. Protective hood was opened with the emergency release (`Notentriegelung`). | By the operator: open the protective hood and then close it. |
| 6350 | F | `Späneförderer AG1: Motorüberstrom` | Chip conveyor AG1: motor overcurrent | Operation not permitted in this machine state. Motor protection switch has tripped. | By the operator, electrical maintenance staff: run the chip conveyor empty, check the current setting. |
| 6351 | W | `Späneförderer AG1: Motorüberstrom` | Chip conveyor AG1: motor overcurrent | As 6350. | As 6350. |
| 6400 | F | `Stangenbetrieb AG1: Kommando unzulässig` | Bar operation AG1: command not permitted | Operation not permitted in this machine state. Command M94 issued without bar operation running. | By the operator: select and switch on bar operation. |
| 6401 | F | `Stangenbetrieb AG1: Nicht betriebsbereit` | Bar operation AG1: not ready | Operation not permitted in this machine state. Bar operation selected but not ready. | By electrical maintenance staff: check the bar equipment (`Stangenanlage`). |
| 6402 | M | `Stangenbetrieb AG1: Stange wechseln` | Bar operation AG1: change bar | Operation not permitted in this machine state. Bar is used up. | By the operator: insert a new bar, since there is no bar loading magazine. |
| 6450 | F | `Vorschubantriebe AG1: Motorüberstrom` | Feed drives AG1: motor overcurrent | Possible defect. Motor protection switch has tripped. | By electrical maintenance staff: check the current setting. |
| 6451 | F | `Vorschubantriebe AG1: Motorübertemperatur` | Feed drives AG1: motor overtemperature | Wrong setting, parameter, possible defect. One of the feed drives reports overtemperature. | By electrical maintenance staff: check motor and controller, let the motor cool down. |
| 6452 | F | `Vorschubantriebe AG1: Nicht betriebsbereit` | Feed drives AG1: not ready | Possible defect. Servo controller of the feed drives faulty. | By electrical maintenance staff: see the electrical service instructions: controllers, circuit diagrams. |
| 6453 | F | `Vorschubantriebe AG1: Schlitten-X nicht betriebsbereit` | Feed drives AG1: X slide not ready | Possible defect. The controller for the X slide is faulty. | By electrical maintenance staff: remove the fault, see the electrical service manual. |
| 6454 | F | `Vorschubantriebe AG1: Schlitten-X Temperaturvorwarnung` | Feed drives AG1: X slide temperature pre-warning | Wrong setting, parameter, possible defect or overload in the cycle. One of the feed drives reports overtemperature. | By electrical maintenance staff: let the drive cool down, check the drive, check the controller. |
| 6455 | W | `Vorschubantriebe AG1: Schlitten-X Temperaturvorwarnung` | Feed drives AG1: X slide temperature pre-warning | As 6454. | As 6454. |
| 6456 | F | `Vorschubantriebe AG1: Schlitten-Z nicht betriebsbereit` | Feed drives AG1: Z slide not ready | Possible defect. The controller for the Z slide is faulty. | By electrical maintenance staff: remove the fault, see the electrical service manual. |
| 6457 | F | `Vorschubantriebe AG1: Lüfter X-Achse Motorüberstrom` | Feed drives AG1: X-axis fan motor overcurrent | Possible defect. Motor protection switch has tripped. | By electrical maintenance staff: check the current setting. |
| 6458 | W | `Vorschubantriebe AG1: Schlitten-Z Temperaturvorwarnung` | Feed drives AG1: Z slide temperature pre-warning | As 6454. | As 6454. |
| 6459 | F | `Vorschubantriebe AG1: Schlitten-Z Temperaturvorwarnung` | Feed drives AG1: Z slide temperature pre-warning | As 6454. | As 6454. |
| 6460 | W | `Vorschubantriebe AG1: Lüfter X-Achse Motorüberstrom` | Feed drives AG1: X-axis fan motor overcurrent | As 6457. | As 6457. |
| 6461 | M | `Vorschubantriebe AG1: Lüfter X-Achse Motorüberstrom` | Feed drives AG1: X-axis fan motor overcurrent | As 6457. | As 6457. |

### AG1: tool carrier, I/O modules, machine, loading aid, swivel chuck, unloading device, driven tools, C axis (pages 9-128 to 9-136)

From 6500 on, the list repeats the 15xx to 18xx messages (pages 9-85 to 9-92) for AG1, with the number offset by 5000 and "AG1" added to the text. Small differences are noted.

| No. | Type | Message (German) | Message (English) | Cause | Remedy |
|---|---|---|---|---|---|
| 6500 | F | `Werkzeugträger AG1: Motorüberstrom` | Tool carrier AG1: motor overcurrent | Possible defect. Motor protection switch has tripped. | By electrical maintenance staff: check the current setting. |
| 6501 | W | `Werkzeugträger AG1: Motorüberstrom` | Tool carrier AG1: motor overcurrent | As 6500. | As 6500. |
| 6502 | F | `Werkzeugträger AG1: Motorübertemperatur` | Tool carrier AG1: motor overtemperature | Operation not permitted in this machine state. Motor temperature monitoring has tripped. | By electrical maintenance staff: let the motor cool down, press the reset button on the device. |
| 6503 | W | `Werkzeugträger AG1: Motorübertemperatur` | Tool carrier AG1: motor overtemperature | As 6502. | As 6502. |
| 6504 | F | `Werkzeugträger AG1: Nicht in Position verriegelt` | Tool carrier AG1: not locked in position | Operation not permitted in this machine state. Tool carrier in wrong position or unlocked. | By the operator: swivel the tool carrier with a T command. |
| 6505 | F | `Werkzeugträger AG1: Parity-Check fehlerhaft` | Tool carrier AG1: parity check faulty | Possible defect. Tool carrier position encoder had a parity error. | By the operator, electrical maintenance staff: swivel the tool carrier (Tx); check position encoder and cable. |
| 6506 | F | `Werkzeugträger AG1: Position nicht gefunden` | Tool carrier AG1: position not found | Wrong setting, parameter, possible defect. Position encoder faulty or value in the parameter wrong. | By technologist/setter, mechanical maintenance staff: check the position encoder or correct the parameter. |
| 6507 | F | `Werkzeugträger AG1: T-Wert für Werkzeugträger zu groß` | Tool carrier AG1: T value too large for tool carrier | Operation not permitted in this machine state, wrong setting, parameter. T value entered is greater than that entered in parameter `Aufnahmeanzahl` (number of stations). | By the operator, technologist/setter: correct the parameter or enter a smaller T value. |
| 6508 | F | `Werkzeugträger AG1: T-Wert konnte nicht übernommen werden` | Tool carrier AG1: T value could not be accepted | As 1508. | As 1508 (e.g. close the hood). |
| 6509 | F | `Werkzeugträger AG1: Vorindexierung fehlerhaft` | Tool carrier AG1: pre-indexing faulty | As 1509. | As 1509 (swivel with T command). |
| 6510 | F | `Werkzeugträger AG1: Werkzeugträger verriegelt nicht` | Tool carrier AG1: does not lock | As 1510 (initiator `Kontrolle - Verriegelung` does not switch). | As 1510. |
| 6511 | F | `Werkzeugträger AG1: Überlastkupplung angesprochen` | Tool carrier AG1: overload clutch tripped | As 1511. | As 1511. |
| 6512 | F | `Werkzeugträger AG1: Werkzeugträger entriegelt nicht` | Tool carrier AG1: does not unlock | As 1512. | As 1512. |
| 6513 | F | `Werkzeugträger AG1: Werkzeugträger Referenzfahrt fehlerhaft` | Tool carrier AG1: reference run faulty | As 1513. | As 1513. |
| 6514 | F | `Werkzeugträger AG1: Interner Werkzeugträger-Fehler` | Tool carrier AG1: internal tool carrier error | Possible defect. The tool carrier does not report unambiguously `Scheibe ausgekuppelt` (disk declutched) (1514 also names `Spindel ausgekuppelt`). | By mechanical / electrical maintenance staff: find the cause and re-set up the tool carrier. |
| 6550 | F | `Ausgabemodule AG1: Steuerspannung ausgefallen` | Output modules AG1: control voltage failed | As 1550. | As 1550. |
| 6551 | F | `Eingabemodule AG1: Steuerspannung ausgefallen` | Input modules AG1: control voltage failed | As 1551. | As 1551. |
| 6552 | F | `Maschine AG1: Nicht eingeschaltet` | Machine AG1: not switched on | As 1552 (drives not on). | Press `Antriebe Ein`; lamp must light. |
| 6553 | F | `Maschine AG1: Not-Aus` | Machine AG1: emergency stop | As 1553. | Release the emergency-stop button. |
| 6554 | F | `Störung Maschine AG1: Siehe Fehlerliste` | Machine fault AG1: see fault list | As 1554 (circuit diagram I 04.1). | As 1554. |
| 6555 | F | `Ventile AG1: Steuerspannung ausgefallen` | Valves AG1: control voltage failed | As 1555. | As 1555. |
| 6556 | F | `Warnung Maschine AG1: Siehe Fehlerliste` | Machine warning AG1: see fault list | As 1556 (circuit diagram I 04.2). | As 1556. |
| 6557 | W | `Warnung Maschine AG1: Siehe Fehlerliste` | Machine warning AG1: see fault list | As 1557. | As 1557. |
| 6600 | F | `Einlegehilfe AG1: Kommando unzulässig` | Loading aid AG1: command not permitted | ==> not used | |
| 6601 | M | `Einlegehilfe AG1: Stellung undefiniert` | Loading aid AG1: position undefined | As 1601. | As 1601 (move by M command to end position). |
| 6602 | F | `Einlegehilfe AG1: Überwachung angesprochen` | Loading aid AG1: monitoring tripped | As 1602. | As 1602. |
| 6650 | M | `Schwenkfutter AG1: Änderung der Anwahl unzulässig` | Swivel chuck AG1: change of selection not permitted | ==> not used | |
| 6651 | F | `Schwenkfutter AG1: Kommando unzulässig` | Swivel chuck AG1: command not permitted | As 1651. | As 1651. |
| 6652 | M | `Schwenkfutter AG1: Stellung undefiniert` | Swivel chuck AG1: position undefined | As 1652. | As 1652. |
| 6653 | F | `Schwenkfutter AG1: Überwachung angesprochen` | Swivel chuck AG1: monitoring tripped | As 1653. | As 1653. |
| 6654 | F | `Schwenkfutter AG1: Zielposition nicht gefunden` | Swivel chuck AG1: target position not found | As 1654. | As 1654. |
| 6655 | F | `Schwenkfutter AG1: Maximale Schwenkzeit überschritten` | Swivel chuck AG1: maximum swivel time exceeded | As 1655 (20 seconds). | As 1655. |
| 6700 | M | `Abholeinrichtung Greifen AG1: Stellung undefiniert` | Unloading device gripping AG1: position undefined | As 1700. | As 1700. |
| 6701 | F | `Abholeinrichtung Greifen AG1: Überwachung angesprochen` | Unloading device gripping AG1: monitoring tripped | As 1701. | As 1701. |
| 6702 | M | `Abholeinrichtung Klappe AG1: Stellung undefiniert` | Unloading device flap AG1: position undefined | As 1702. | As 1702. |
| 6703 | F | `Abholeinrichtung Klappe AG1: Überwachung angesprochen` | Unloading device flap AG1: monitoring tripped | As 1703. | As 1703. |
| 6704 | M | `Abholeinrichtung Schwenken AG1: Stellung undefiniert` | Unloading device swivelling AG1: position undefined | As 1704. | As 1704. |
| 6705 | F | `Abholeinrichtung Schwenken AG1: Überwachung angesprochen` | Unloading device swivelling AG1: monitoring tripped | As 1705. | As 1705. |
| 6706 | F | `Abholeinrichtung AG1: Kommando unzulässig` | Unloading device AG1: command not permitted | As 1706. | As 1706. |
| 6707 | F | `Abholeinrichtung AG1: Maximale Ausführungszeit überschritten` | Unloading device AG1: maximum execution time exceeded | As 1707. | As 1707. |
| 6708 | F | `Abholeinrichtung Förderband AG1: Ausführung fehlerhaft` | Unloading device conveyor belt AG1: execution faulty | As 1708. | As 1708. |
| 6709 | F | `Abholeinrichtung Förderband AG1: Motorüberstrom` | Unloading device conveyor belt AG1: motor overcurrent | As 1709. | As 1709. |
| 6710 | W | `Abholeinrichtung Förderband AG1: Motorüberstrom` | Unloading device conveyor belt AG1: motor overcurrent | As 1710. | As 1710. |
| 6711 | M | `Abholeinrichtung Förderband AG1: Werkstück im Förderkanal` | Unloading device conveyor belt AG1: workpiece in conveyor channel | As 1711. | As 1711. |
| 6750 | F | `Angetriebene Werkzeuge AG1: Kommando unzulässig` | Driven tools AG1: command not permitted | As 1750 (hoods open, spindle enable missing, or not in a driven-tool position). | As 1750. |
| 6751 | F | `Angetriebene Werkzeuge AG1: Kuppelvorgang fehlerhaft` | Driven tools AG1: coupling operation faulty | As 1751 (coupling with M103 / M104 / M105 faulty). | As 1751. |
| 6752 | F | `Angetriebene Werkzeuge AG1: Kupplungsüberwachung angesprochen` | Driven tools AG1: clutch monitoring tripped | As 1752. | As 1752. |
| 6753 | F | `Angetriebene Werkzeuge AG1: Nicht betriebsbereit` | Driven tools AG1: not ready | As 1753. | As 1753. |
| 6754 | F | `Angetriebene Werkzeuge AG1: Temperaturvorwarnung` | Driven tools AG1: temperature pre-warning | As 1754. | As 1754. |
| 6755 | F | `Angetriebene Werkzeuge AG1: Übertemperatur` | Driven tools AG1: overtemperature | As 1755 F. | As 1755 F. |
| 6755 | W | `Angetriebene Werkzeuge AG1: Temperaturvorwarnung` | Driven tools AG1: temperature pre-warning | As 1755 W. (Number again printed 6755, as in the 1755 pair; probably 6756 [uncertain].) | As 1755 W. |
| 6800 | F | `C-Achse Hauptspindel AG1: Überwachung angesprochen` | C axis main spindle AG1: monitoring tripped | As 1800 (C-axis pressure switch and limit switch misadjusted or defective). | As 1800. |

**The list continues in part d** (the last entry on page 9-136 is 6800 F; the next page, 9-137, is in part d).

## For the retrofit

All from the PLC message list; the pages describe what the original PLC monitors, not wiring.

- **Original control**: the messages talk of an "NC" plus a separate "SPS" (PLC) with Gildemeister PLC parameters, and of Eltromatic I/O modules (`Eltromatic AG1`, 5400-5410, p. 9-102/103) with start-up phases 0, 1, 3, 4, a service channel and cyclic communication. The NC make/model is not named on these pages. 5951: the NC must react to PLC signals within 300 ms (p. 9-113).
- **Two sides**: messages 5300-6800 carry "AG1" (machining unit 1 [interpretation]). The 15xx-18xx messages without "AG1" (p. 9-85 to 9-92) are the same set with the number 5000 lower [interpretation: these are probably the other side/AG2 or machine-wide; not stated]. 1950-1953 `Arbeitsraumtrennwand` (working-area partition wall between the two sides?) is opened/closed by M command, hydraulic, with limit switches and a time-out; not allowed with hoods open (p. 9-93/94).
- **Axes and drives**: X and Z slides per side (6453-6459), feed-drive servo controllers, X-axis fan with motor protection (6457/6460/6461), following-error overflow on cross slide / C axis (5950, p. 9-113). Main spindle C axis only in gear stage 1 (1801, p. 9-93); C axis has pressure switch and limit switch (1800/6800). Main spindle clamp only in the position window, i.e. position with C axis first (5701, p. 9-109). Main drive has a shiftable gearbox (5450-5453). Counter-spindle with its own drive, C axis, chuck, clamping cylinder and cover (2450-2650, p. 9-98 to 9-101). Speed monitor: max 50 min⁻¹ in setup mode, 5000 min⁻¹ with hood closed (1850, p. 9-93).
- **ISM spindle temperature control**: AEM module measures rear and front bearing, winding, ambient and cooling-water temperature, with max / maxmax limits, hysteresis and set points as parameters (2100-2114, 6014-6029, p. 9-95 to 9-118); ISM cooling with heat-exchanger fan and pump (2000-2003).
- **Turret/tool carrier**: T command indexing with pre-indexing, lock/unlock (initiator `Kontrolle - Verriegelung`), reference run with time-out, overload clutch, position encoder with parity check, motor protection and thermal switch with reset button, signals `Scheibe ausgekuppelt` / `Spindel ausgekuppelt` (1508-1514, 6500-6514). T value must not exceed parameter `Aufnahmeanzahl` (6507). Driven tools: coupling with M103/M104/M105 checked by proximity switches coupled/decoupled; need a driven-tool position, closed hoods and spindle enable (1750-1755, 6750-6755).
- **Chucks**: main clamping cylinder with front/rear stroke-limit proximity switches, clamping-pressure switch, working stroke, release stroke; clamp/release by M command or foot switch; time-out 20 s; spindle may run at most 90 s with chuck open; chuck commands blocked while the spindle turns; max chuck speed parameter; chuck change position needs the chuck key in the enable switch; "workpiece not seated" check (5550-5603, p. 9-106 to 9-108). Counter-spindle chuck same (2450-2458).
- **Tailstock/quill/steady rests**: quill with pressure switch and proximity switches, M command or foot switch `Vor`/`Zurück`; movable tailstock with 2 end positions and teach-in module; steady rests 1 and 2 with pressure switches `Geöffnet`/`Gespannt`; quill command needs spindle stopped or a steady rest clamped; steady-rest command needs spindle stopped or quill clamped (5900-5904, 6100-6153).
- **Hydraulics**: filter differential-pressure switch, system-pressure switch, minimum level, motor protection, oil overtemperature (5750-5757). **Lubrication**: interval and suppression times as parameters, pressure switch (no pressure / permanent pressure, circuits 1 and 2), fresh-oil level, pump motor protection; lubrication-pulse suppression cancelled by M49, M18, M30, M99 or cycle stop (6250-6269, 6030/6031, p. 9-121 to 9-124). **Coolant**: motor protection, flow monitor, filter differential pressure (5800-5803). Control-cabinet cooling: filter, overcurrent, overtemperature (6200-6205).
- **Safety/doors**: `Antriebe Ein` button with lamp, emergency stop, safety circuit monitored (1552/1553/1560). Automatic protective hood: open/close time-out (parameter 031), motor protection, safety edge, safety-circuit signal sequence, emergency release detection, no opening while a button is pressed (6300-6306, p. 9-124/125). Setup mode only with hood open and mode `Handsteuern` (5350/5351). Auxiliary-drive line contactor must drop out when the hood opens (2750). Tool probe may only be swivelled in in setup mode (2700-2702).
- **Part handling / transfer**: loading aid (`Einlegehilfe`, hydraulic, M-command end positions), swivel chuck (20 s swivel time-out, max swivel speed parameter), unloading device (`Abholeinrichtung`: gripper `Greifen`/`Lösen`, flap `Auf`/`Zu`, swivel arm `Vor`/`Zurück`, ejector `Austrager`, conveyor belt with arrival time-out, each by M command or push-button with proximity switches), loading flap (5850-5852), gantry handling unit `Portal` (5500-5504), bar operation with M94 (6400-6402), chip conveyor (6350). The specific M-code numbers for these devices are not on these pages (only M94, M103-M105, M18/M30/M49/M99).
