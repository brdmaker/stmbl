# Gildemeister CTX twin operating manual, 9. Parameter setting, M-commands, PLC error messages (Parametereinstellung, M-Befehle, SPS-Fehlermeldungen), pages 9-137 to 9-186

Source: `9_Parametereinstellung_M-Befehle_SPS-Fehlermeldungen_d.pdf` (part d of 4), 50 scanned pages, manual pages 9-137 to 9-186. "Betriebsanleitung CTX twin, Dok.-Nr.: daaab1d1, Februar 1995". Pages 9-149, 9-152 and 9-185 additionally carry "REV: dab9501 (05.05.1995) zu daaab1d1 Februar 1995"; revised lines on those pages are marked with "!" in the left margin.
Original language: German.

This part contains no parameter tables and no M-code lists (those are in parts a-c). It covers:
- the end of the PLC error message list (section 9.3 per the chapter structure; the list **starts in an earlier part and continues here mid-way** at message 6850, the last messages of the AG1 range, followed by the complete AG2 range 10300-12250), pages 9-137 to 9-183;
- 9.4 External events (`Externe Ereignisse`), page 9-184;
- 9.4.1 Assignment of the event flags (`Belegung der Ereignis Merker`), page 9-185;
- page 9-186: (blank page; header only).

Running page header: "9. Parametereinstellung, M-Befehle, SPS-Fehlermeldungen" plus, at the right, the first message number on the page (e.g. "6850 F:").

---

## PLC error messages (continued)

Every message is printed as: number, type letter, message text (bold); then `Ursache:` (cause) with a cause category and a description; then `Behebung:` (remedy) with who is responsible and what to do.

Type letters (as used in the list): **F** = fault (`Fehler`), **W** = warning (`Warnung`), **M** = message (`Meldung`). (Legend not printed on these pages; meaning inferred from the list.)

`AG1` / `AG2` = `Arbeitsgang`/machine side 1 and 2 [uncertain expansion]: AG1 messages belong to slide/side 1 (6000-7250 range), AG2 messages to slide/side 2 (10300-12250 range); the AG2 range repeats the AG1 range offset by 5000 (compare 6850/11850, 6950/11950, 7000/12000, 7250/12250). Section 9.4.1 uses "Schlitten 1 AG1" and "Schlitten 2 AG2" (slide 1 / slide 2).

Recurring phrases:

| German | English |
|---|---|
| `Bedienung in dem Maschinenstatus unzulässig` | operation not permitted in this machine status |
| `möglicher Defekt` | possible defect |
| `falsche Einstellung, Parameter` | wrong setting, parameter |
| `Wartung ist notwendig` | maintenance necessary |
| `Überlastung im Zyklus` | overload in the cycle |
| `durch Bediener` | by operator |
| `durch Technologe, Einrichter` | by technologist, setter |
| `durch Wartungspersonal Elektrik` / `Mechanik` | by electrical / mechanical maintenance staff |
| `Motorschutzschalter hat ausgelöst` | motor protection switch has tripped |
| `Stromeinstellung prüfen` | check current setting |
| `Störung beseitigen siehe Service-Handbuch Elektrik` | clear the fault, see electrical service manual |
| `Fehler quittieren` | acknowledge the error |
| `Steller` | drive controller (power amplifier) |
| `Kommando unzulässig` | command not permitted |
| `Stellung undefiniert` | position undefined |
| `Überwachung angesprochen` | monitoring tripped |
| `nicht verwendet` | not used |
| `ISM` | integrated spindle motor [uncertain expansion; not spelled out on these pages] |

### AG1 messages (end of list), pages 9-137 to 9-142

(The AG1 list continues here from part c; 6850 is the first message on page 9-137.)

| No. | Type | Message (German, as displayed) | English | Cause (`Ursache`) | Remedy (`Behebung`) |
|---|---|---|---|---|---|
| 6850 | F | `Drehzahlüberwachung AG1: Gerät gestört oder angesprochen` | Speed monitoring AG1: device faulty or tripped | Operation not permitted in this machine status, possible defect. Spindle turns > 50 min⁻¹ in setting-up mode, speed-monitoring device fault | By operator, by electrical maintenance staff. Spindle < 50 min⁻¹; check the speed-monitoring device |
| 6900 | F | `AG1: Siehe SPS-Fehlerliste` | AG1: see PLC error list | ==> `nicht verwendet` (not used) | - |
| 6901 | F | `AG1: Siehe SPS-Fehlerliste` | AG1: see PLC error list | ==> not used | - |
| 6902 | W | `AG1: Siehe SPS-Fehlerliste` | AG1: see PLC error list | ==> not used | - |
| 6950 | F | `Arbeitsraumtrennwand AG1: Kommando unzulässig` | Work-area partition wall AG1: command not permitted | Operation not permitted in this machine status. Guard covers are open | By operator. Close the guard covers |
| 6951 | M | `Arbeitsraumtrennwand AG1: Stellung undefiniert` | Work-area partition wall AG1: position undefined | Operation not permitted in this machine status. The partition wall is in neither of its two end positions | By operator. Command "close" or "open" (M command) |
| 6952 | F | `Arbeitsraumtrennwand AG1: Überwachung angesprochen` | Work-area partition wall AG1: monitoring tripped | Wrong setting, limit switch defective. The selected state was left without reason | By technologist, setter. Check the hydraulics and limit switches |
| 6953 | F | `Arbeitsraumtrennwand AG1: Maximale Ausführungszeit überschritten` | Work-area partition wall AG1: maximum execution time exceeded | Wrong setting, limit switch defective. The command was not executed in the specified time | By technologist, setter. Check the hydraulics and limit switches |
| 7000 | F | `ISM-Kühlung AG1: Lüfter Wärmetauscher Motorüberstrom` | ISM cooling AG1: heat-exchanger fan motor overcurrent | Possible defect. Motor protection switch has tripped | By electrical maintenance staff. Check the current setting |
| 7001 | W | `ISM-Kühlung AG1: Lüfter Wärmetauscher Motorüberstrom` | ISM cooling AG1: heat-exchanger fan motor overcurrent | As 7000 | As 7000 |
| 7002 | F | `ISM-Kühlung AG1: Pumpe Motorüberstrom` | ISM cooling AG1: pump motor overcurrent | Possible defect. Motor protection switch has tripped | By electrical maintenance staff. Check the current setting |
| 7003 | W | `ISM-Kühlung AG1: Pumpe Motorüberstrom` | ISM cooling AG1: pump motor overcurrent | As 7002 | As 7002 |
| 7050 | F | `Schaltschrank AG1: Übertemperatur` | Switch cabinet AG1: overtemperature | Possible defect. Switch-cabinet overtemperature | By mechanical maintenance staff. Check the cooling unit, or ambient temperature too high? |
| 7051 | W | `Schaltschrank AG1: Übertemperatur` | Switch cabinet AG1: overtemperature | As 7050 | As 7050 |
| 7100 | F | `ISM-Temperaturregelung AG1: AEM-Modul nicht betriebsbereit` | ISM temperature control AG1: AEM module not ready | Possible defect. The AEM module is not ready for operation | By electrical maintenance staff. Check the AEM module |
| 7101 | W | `ISM-Temperaturregelung AG1: AEM-Modul nicht betriebsbereit` | ISM temperature control AG1: AEM module not ready | As 7100 | As 7100 |
| 7102 | F | `ISM-Temperaturregelung AG1: max. Lagertemperatur hinten überschritten` | ISM temperature control AG1: max. rear bearing temperature exceeded | Possible defect or wrong setting, parameter. The max. rear bearing temperature was exceeded | By mechanical and electrical maintenance staff. Let the bearing cool down, check the bearing, check the parameters |
| 7103 | W | `ISM-Temperaturregelung AG1: max. Lagertemperatur hinten überschritten` | ISM temperature control AG1: max. rear bearing temperature exceeded | As 7102 | As 7102 |
| 7104 | F | `ISM-Temperaturregelung AG1: max. Lagertemperatur vorn überschritten` | ISM temperature control AG1: max. front bearing temperature exceeded | Possible defect or wrong setting, parameter. The max. front bearing temperature was exceeded | By mechanical and electrical maintenance staff. Let the bearing cool down, check the bearing, check the parameters |
| 7105 | W | `ISM-Temperaturregelung AG1: max. Lagertemperatur vorn überschritten` | ISM temperature control AG1: max. front bearing temperature exceeded | As 7104 | As 7104 |
| 7106 | F | `ISM-Temperaturregelung AG1: max. Motorwicklungstemperatur überschritten` | ISM temperature control AG1: max. motor winding temperature exceeded | Wrong setting, parameter, possible defect or overload in the cycle. The max. motor winding temperature was exceeded | By mechanical and electrical maintenance staff. Let the motor cool down, check the motor, check the parameters |
| 7107 | W | `ISM-Temperaturregelung AG1: max. Motorwicklungstemperatur überschritten` | ISM temperature control AG1: max. motor winding temperature exceeded | As 7106 | As 7106 |
| 7108 | M | `ISM-Temperaturregelung AG1: max. Umgebungstemperatur überschritten` | ISM temperature control AG1: max. ambient temperature exceeded | Possible defect or wrong setting, parameter. The max. ambient temperature was exceeded | By electrical maintenance staff. Check the temperature sensor, check the parameters |
| 7109 | F | `ISM-Temperaturregelung AG1: AEM-Modul reagiert nicht` | ISM temperature control AG1: AEM module does not respond | Possible defect or wrong setting, parameter. The AEM module does not respond properly to the measurement request | By electrical maintenance staff. Check the AEM module, the temperature sensor and the parameters |
| 7110 | M | `ISM-Temperaturregelung AG1: AEM-Modul reagiert nicht` | ISM temperature control AG1: AEM module does not respond | As 7109 (`falsch eingestellter Parameter`: wrongly set parameter) | As 7109 |
| 7111 | W | `ISM-Temperaturregelung AG1: AEM-Modul reagiert nicht` | ISM temperature control AG1: AEM module does not respond | As 7109 | As 7109 |
| 7112 | F | `ISM-Temperaturregelung AG1: maxmax. Lagertemperatur hinten überschritten` | ISM temperature control AG1: maxmax. rear bearing temperature exceeded | Possible defect or wrong setting, parameter. The maxmax. rear bearing temperature was exceeded | By mechanical and electrical maintenance staff. Let the bearing cool down, check the bearing, check the parameters |
| 7113 | F | `ISM-Temperaturregelung AG1: maxmax. Lagertemperatur vorn überschritten` | ISM temperature control AG1: maxmax. front bearing temperature exceeded | Possible defect or wrong setting, parameter. The maxmax. front bearing temperature was exceeded | As 7112 |
| 7114 | F | `ISM-Temperaturregelung AG1: maxmax. Motorwicklungstemperatur überschritten` | ISM temperature control AG1: maxmax. motor winding temperature exceeded | Wrong setting, parameter, possible defect or overload in the cycle. The maxmax. motor winding temperature was exceeded | By mechanical and electrical maintenance staff. Let the motor cool down, check the motor, check the parameters |
| 7150 | F | `Versorgung AG1: Nicht betriebsbereit` | Supply AG1: not ready | Possible defect. The supply is faulty | By electrical maintenance staff. Clear the fault, see the electrical service manual |
| 7151 | F | `Versorgung AG1: Temperaturvorwarnung` | Supply AG1: temperature pre-warning | Wrong setting, parameter, possible defect or overload in the cycle. The supply reports overtemperature | By electrical maintenance staff. Check the supply |
| 7152 | W | `Versorgung AG1: Temperaturvorwarnung` | Supply AG1: temperature pre-warning | As 7151 | As 7151 |
| 7250 | F | `SPS AG1: Interner Softwarefehler` | PLC AG1: internal software error | Internal PLC error | By technologist, setter, if necessary Gildemeister service. Check the PLC parameter settings and switch the machine off and on again. Please inform Gildemeister service |

(Rows 6850-7250 re-transcribed from PDF pages 1-6 after the first pass picked up the English edition's rows by mistake; see DEBUGNOTES.)

Page 9-142 contains only 7250; the rest of the page is empty.

### AG2 messages, pages 9-143 to 9-183

(The AG2 range starts at 10300 on page 9-143.)

| No. | Type | Message (German, as displayed) | English | Cause (`Ursache`) | Remedy (`Behebung`) |
|---|---|---|---|---|---|
| 10300 | F | `Eingabe AG2: M-Funktion undefiniert` | Input AG2: M-function undefined | Operation not permitted in this machine status. M-function undefined for this machine equipment | By operator. Acknowledge the error |
| 10301 | F | `Eingabe AG2: M-Funktion wegen Maschinenzustand unzulässig` | Input AG2: M-function not permitted due to machine state | Operation not permitted in this machine status. M-function not permitted because of the state of the machine | By operator. Acknowledge the error |
| 10350 | F | `Einrichtebetrieb AG2: Anwahl unzulässig` | Setting-up mode AG2: selection not permitted | Operation not permitted in this machine status. Guard cover is closed or operating mode is not manual control (`Handsteuern`) | By operator. Open the guard cover and select operating mode manual control |
| 10351 | M | `Einrichtebetrieb AG2: Unzulässig` | Setting-up mode AG2: not permitted | As 10350 | As 10350 |
| 10400 | F | `Eltromatic AG2: Fehler Hochlauf Phase 3` | Eltromatic AG2: start-up error phase 3 | Possible defect. Error while testing the communication | By electrical maintenance staff. Check the I/O modules and connections to the control |
| 10401 | F | `Eltromatic AG2: Fehler Hochlauf Phase 4` | Eltromatic AG2: start-up error phase 4 | Possible defect. General error when starting cyclic communication | By electrical maintenance staff. Check the I/O modules and connections to the control |
| 10402 | F | `Eltromatic AG2: Fehler Hochlauf Phase 0` | Eltromatic AG2: start-up error phase 0 | Possible defect, maintenance necessary. Error in the self-test of the I/O modules | By electrical maintenance staff. Check the I/O modules and connections to the control |
| 10403 | F | `Eltromatic AG2: Fehler Hochlauf Phase 1` | Eltromatic AG2: start-up error phase 1 | Possible defect. General error in the identification phase | By electrical maintenance staff. Check the I/O modules and connections to the control |
| 10404 | F | `Eltromatic AG2: Fehler Hochlauf Servicekanalfehler` | Eltromatic AG2: start-up error, service channel error | Possible defect. General error while parameterizing the I/O modules | By electrical maintenance staff. Check the I/O modules and connections to the control |
| 10405 | F | `Eltromatic AG2: Fehler im zyklischen Betrieb` | Eltromatic AG2: error in cyclic operation | Possible defect. General error during cyclic communication | By operator. Check the I/O modules and connections to the control |
| 10406 | F | `Eltromatic AG2: Kein EA-Modul identifiziert` | Eltromatic AG2: no I/O module identified | Possible defect. No I/O module could be identified | By electrical maintenance staff. Check the I/O modules and connections to the control |
| 10407 | F | `Eltromatic AG2: Nicht unterstütztes Modul gefunden` | Eltromatic AG2: unsupported module found | Possible defect. An unknown I/O module was detected | By electrical maintenance staff. Check the I/O modules and connections to the control |
| 10408 | F | `Eltromatic AG2: Ausrüstung und Parameter stimmen nicht überein` | Eltromatic AG2: equipment and parameters do not match | Possible defect or wrong setting, parameter. The Eltromatic equipment detected at machine start-up does not match the entry in the associated parameters | By electrical maintenance staff. Check the I/O modules and parameters or match them to each other |
| 10409 | F | `Eltromatic AG2: Fehlerhafte Masseverbindung am Modul <Modulnr.>` | Eltromatic AG2: faulty ground connection at module <module no.> | Possible defect. Faulty ground connection at the indicated Eltromatic module | By electrical maintenance staff. Check the ground connections at the indicated Eltromatic module |
| 10410 | F | `Eltromatic AG2: Allgemeiner Fehler am Modul <Modulnr.>` | Eltromatic AG2: general error at module <module no.> | Possible defect. General error at the indicated Eltromatic module | By electrical maintenance staff. Check the indicated Eltromatic module and its connections |
| 10450 | F | `Getriebe Hauptantrieb AG2: Kommando unzulässig` | Main drive gearbox AG2: command not permitted | Operation not permitted in this machine status. The command is not permitted in this machine state | By operator, by electrical maintenance staff. Change the machine state and repeat the command |
| 10451 | F | `Getriebe Hauptantrieb AG2: Kuppelvorgang fehlerhaft` | Main drive gearbox AG2: shifting (coupling) operation faulty | Possible defect. The requested position was not reached | By operator, by electrical maintenance staff. Repeat the command, check the gearbox control |
| 10452 | M | `Getriebe Hauptantrieb AG2: Stellung undefiniert` | Main drive gearbox AG2: position undefined | Operation not permitted in this machine status. The gearbox is in none of the permitted positions | By operator. Trigger the command for a permitted position |
| 10453 | F | `Getriebe Hauptantrieb AG2: Überwachung angesprochen` | Main drive gearbox AG2: monitoring tripped | Possible defect. The requested position was left without reason | By operator, by electrical maintenance staff. Repeat the command, check the gearbox control |
| 10500 | F | `Handhabung AG2: Kommando unzulässig` | Handling (loader) AG2: command not permitted | Operation not permitted in this machine status, wrong setting, parameter. Not permitted because the state of the handling unit is wrong | By operator, by technologist, setter. Acknowledge the error, switch the handling unit to the correct state |
| 10501 | M | `Handhabung AG2: Maschine von Handhabung ausgeschaltet` | Handling AG2: machine switched off by handling unit | Machine was switched off by the gantry (`Portal`) (no blanks) | By technologist, setter. Refill blanks, switch gantry and machine on again |
| 10502 | F | `Handhabung AG2: Nicht betriebsbereit` | Handling AG2: not ready | Operation not permitted in this machine status. Handling unit not switched on or not ready | By operator, by technologist, setter. Check the handling unit |
| 10503 | W | `Handhabung AG2: Nicht betriebsbereit` | Handling AG2: not ready | Operation not permitted in this machine status. Handling unit not switched on or ready [sic, "nicht" missing] | By technologist, setter. Check the handling unit |
| 10504 | M | `Handhabung AG2: Handhabung ist nicht im Automatikbetrieb` | Handling AG2: handling unit is not in automatic mode | Operation not permitted in this machine status. While the handling command is being processed, the handling unit is not set to automatic mode | By operator. Set the handling unit to automatic and, if necessary, press cycle start on the lathe |
| 10550 | M | `Hauptspanner AG2: Ausklinkhub angewählt` | Main clamping cylinder AG2: release stroke (`Ausklinkhub`) selected | Operation not permitted in this machine status. Release stroke selected in parameter "`Bedienung`" (operation) | By operator. Correct the parameter, see 9.1.2, "`Parameterliste`" (parameter list) on page 9-6 |
| 10551 | F | `Hauptspanner AG2: Druckeinbruch` | Main clamping cylinder AG2: pressure drop | Wrong setting, parameter. Pressure switch "`Spanndruck`" (clamping pressure) reported a pressure drop | By technologist, setter. Check the corresponding pressure switch, depending on clamping direction |
| 10552 | F | `Hauptspanner AG2: Fehlerhafte Eingabesignale` | Main clamping cylinder AG2: faulty input signals | Wrong setting, parameter. Wrong signals occurred during clamping or releasing | By technologist, setter. Check the setting (useful stroke, clamping pressure) |
| 10553 | F | `Hauptspanner AG2: Hubbegrenzung hinten fehlerhaft` | Main clamping cylinder AG2: rear stroke limit faulty | Possible defect. Proximity switch test "`Näherungsschalter Hinten`" (proximity switch rear) faulty | By electrical maintenance staff. Replace the proximity switch or check the signal path according to the circuit diagram |
| 10554 | M | `Hauptspanner AG2: Hubbegrenzung Spannen erreicht` | Main clamping cylinder AG2: clamping stroke limit reached | Wrong setting, parameter. Main clamping cylinder reached the end position without pressure | By technologist, setter. Check the setting (useful stroke, clamping pressure) |
| 10555 | F | `Hauptspanner AG2: Hubbegrenzung vorn fehlerhaft` | Main clamping cylinder AG2: front stroke limit faulty | Possible defect. Proximity switch test "`Näherungsschalter Vorn`" (proximity switch front) faulty | By electrical maintenance staff. Replace the proximity switch or check the signal path according to the circuit diagram |
| 10556 | F | `Hauptspanner AG2: Spindellaufzeit ohne Spanndruck überschritten` | Main clamping cylinder AG2: spindle running time without clamping pressure exceeded | Operation not permitted in this machine status. Spindle ran for longer than 90 seconds with the chuck open | By operator. Acknowledge the error, correct the part program if necessary |
| 10557 | M | `Hauptspanner AG2: Stellung undefiniert` | Main clamping cylinder AG2: position undefined | Operation not permitted in this machine status. Main clamping cylinder neither clamped nor fully open | By operator. Command "`Spannen`" (clamp) or "`Lösen`" (release) (M-command or foot switch) |
| 10558 | F | `Hauptspanner AG2: Überwachung angesprochen` | Main clamping cylinder AG2: monitoring tripped | Wrong setting, parameter. End position "`Gespannt`" (clamped) or "`Geöffnet`" (open) left without reason | By technologist, setter. Check the setting (useful stroke, clamping pressure) |
| 10559 | F | `Hauptspanner AG2: Maximale Ausführungszeit überschritten` | Main clamping cylinder AG2: maximum execution time exceeded | Possible defect. The module "`Spanner ohne Hubbegrenzung`" (clamping cylinder without stroke limitation) could not complete the clamping or releasing operation within 20 seconds | By technologist, setter. Check main clamping cylinder settings (pressure switches, pressure reducers), valves |
| 10600 | M | `Hauptspannfutter AG2: In Wechselposition` | Main chuck AG2: in change position | Operation not permitted in this machine status. Chuck key is not inserted at the control panel | By operator. Insert the chuck key into the enabling switch on the control panel |
| 10601 | F | `Hauptspannfutter AG2: Max. Drehzahl überschritten` | Main chuck AG2: max. speed exceeded | Wrong setting, parameter. Speed was higher than the set chuck speed | By technologist, setter. Check the set speed, correct the part program if necessary |
| 10602 | F | `Hauptspannfutter AG2: Kommando unzulässig` | Main chuck AG2: command not permitted | Operation not permitted in this machine status. Chuck command not permitted with spindle rotating | By operator. Stop the spindle first |
| 10603 | M | `Hauptspannfutter AG1: Werkstück liegt nicht an` (printed "AG1" although in the AG2 number range; marked "!" as revised, REV dab9501) | Main chuck AG1: workpiece not seated (not in contact with the stop) | Workpiece not seated | By operator. Open the chuck and clamp the workpiece again |
| 10650 | F | `Hauptspindelantrieb AG2: Motorübertemperatur` | Main spindle drive AG2: motor overtemperature | Operation not permitted in this machine status. Motor overloaded | By electrical maintenance staff. Check the drive controller (`Steller`) and let the motor cool down |
| 10651 | W | `Hauptspindelantrieb AG2: Motorübertemperatur` | Main spindle drive AG2: motor overtemperature | As 10650 | As 10650 |
| 10652 | F | `Hauptspindelantrieb AG2: Nicht betriebsbereit` | Main spindle drive AG2: not ready | Possible defect. Main spindle drive controller faulty | By electrical maintenance staff. Clear the fault, see electrical service manual |
| 10653 | F | `Hauptspindelantrieb AG2: Störung Fremdlüfter` | Main spindle drive AG2: separate (external) fan fault | Possible defect. Motor protection switch has tripped | By electrical maintenance staff. Check current setting |
| 10654 | W | `Hauptspindelantrieb AG2: Störung Fremdlüfter` | Main spindle drive AG2: separate fan fault | As 10653 | As 10653 |
| 10655 | F | `Hauptspindelantrieb AG2: Temperaturvorwarnung` | Main spindle drive AG2: temperature pre-warning | Wrong setting, parameter, possible defect or overload in the cycle. The main spindle drive reports overtemperature | By electrical maintenance staff. Let the motor cool down, check the motor, check the drive controller |
| 10656 | W | `Hauptspindelantrieb AG2: Temperaturvorwarnung` | Main spindle drive AG2: temperature pre-warning | As 10655 | As 10655 |
| 10700 | F | `Hauptspindelklemmung AG2: Ausführung fehlerhaft` | Main spindle clamp (brake) AG2: execution faulty | Wrong setting, parameter. Proximity switches of the spindle clamp give wrong signals | By technologist, setter. Check the proximity switches |
| 10701 | F | `Hauptspindelklemmung AG2: Kommando unzulässig` | Main spindle clamp AG2: command not permitted | Operation not permitted in this machine status. Main spindle is not within the position window | By operator. Carry out spindle positioning with the C axis first |
| 10702 | F | `Hauptspindelklemmung AG2: Überwachung angesprochen` | Main spindle clamp AG2: monitoring tripped | Wrong setting, parameter. The selected status was left without reason | By technologist, setter. Check the proximity switches |
| 10703 | M | `Hauptspindelklemmung AG2: Hauptspindel ist geklemmt` | Main spindle clamp AG2: main spindle is clamped | Main spindle is clamped. The main spindle has been clamped manually | By operator. Release the main spindle clamp |
| 10750 | F | `Hydraulik AG2: Filter verstopft` | Hydraulics AG2: filter clogged | Maintenance necessary. Differential pressure before/after hydraulic filter too high | By mechanical maintenance staff. Change the hydraulic filter |
| 10751 | W | `Hydraulik AG2: Filter verstopft` | Hydraulics AG2: filter clogged | As 10750 | As 10750 |
| 10752 | F | `Hydraulik AG2: Kein Systemdruck` | Hydraulics AG2: no system pressure | Maintenance necessary. Pressure loss or pressure drop in hydraulics | By mechanical maintenance staff. Check oil level and hydraulic tightness |
| 10753 | W | `Hydraulik AG2: Minimum Niveau erreicht` | Hydraulics AG2: minimum level reached | Maintenance necessary. Oil level in hydraulic tank too low | By mechanical maintenance staff. Check hydraulics for leaks, top up oil |
| 10754 | F | `Hydraulik AG2: Minimum Niveau erreicht` | Hydraulics AG2: minimum level reached | As 10753 | As 10753 |
| 10755 | F | `Hydraulik AG2: Motorüberstrom` | Hydraulics AG2: motor overcurrent | Possible defect. Motor protection switch has tripped | By electrical maintenance staff. Check current setting |
| 10756 | W | `Hydraulik AG2: Übertemperatur Öl` | Hydraulics AG2: oil overtemperature | Possible defect. The temperature of the hydraulic unit is too high | By mechanical maintenance staff. Check the hydraulic unit (oil level, other) |
| 10757 | F | `Hydraulik AG2: Übertemperatur Öl` | Hydraulics AG2: oil overtemperature | As 10756 | As 10756 |
| 10800 | F | `Kühlmittel AG2: Motorüberstrom` | Coolant AG2: motor overcurrent | Possible defect. Motor protection switch(es) has/have tripped | By electrical maintenance staff. Check current setting |
| 10801 | F | `Kühlmittel AG2: Strömungsüberwachung angesprochen` | Coolant AG2: flow monitoring tripped | Possible defect or maintenance necessary. Coolant supply is faulty | By electrical or mechanical maintenance staff. Check the coolant supply |
| 10803 | F | `Kühlmittel AG2: Kühlmittel verstopft` (marked "!" as revised, REV dab9501; no 10802 printed) | Coolant AG2: coolant (filter) clogged | Maintenance necessary. Differential pressure before/after coolant filter too high | By electrical or mechanical maintenance staff. Change the coolant filter |
| 10850 | F | `Ladeklappe AG2: Kommando unzulässig` | Loading flap AG2: command not permitted | Operation not permitted in this machine status. Guard cover is open or gripper is in the work area | By operator. Close the guard cover and move the gripper out of the work area |
| 10851 | M | `Ladeklappe AG2: Stellung undefiniert` | Loading flap AG2: position undefined | Operation not permitted in this machine status. Loading flap is neither open nor closed | By operator. Trigger command "`Öffnen`" (open) or "`Schließen`" (close) |
| 10852 | F | `Ladeklappe AG2: Überwachung angesprochen` | Loading flap AG2: monitoring tripped | Wrong setting, parameter. The selected status was left without reason | By technologist, setter. Check the loading flap limit switches |
| 10900 | F | `Lünetten AG2: Kommando unzulässig` | Steady rests AG2: command not permitted | Operation not permitted in this machine status. Spindle is switched on, quill position not permitted | By operator. Stop the spindle or clamp the quill |
| 10901 | M | `Lünette 1 AG2: Stellung undefiniert` | Steady rest 1 AG2: position undefined | Operation not permitted in this machine status. Steady rest 1 is neither clamped nor fully open | By operator. Command "`Spannen`" (clamp) or "`Lösen`" (release) (M-command or push-button) |
| 10902 | F | `Lünette 1 AG2: Überwachung angesprochen` | Steady rest 1 AG2: monitoring tripped | Wrong setting, parameter. Clamped or open state was left | By technologist, setter. Check pressure switch "`Geöffnet`" (open) or "`Gespannt`" (clamped) |
| 10903 | M | `Lünette 2 AG2: Stellung undefiniert` | Steady rest 2 AG2: position undefined | Operation not permitted in this machine status. Steady rest 2 is neither clamped nor fully open | By operator. Command "`Spannen`" or "`Lösen`" (M-command or push-button) |
| 10904 | F | `Lünette 2 AG2: Überwachung angesprochen` | Steady rest 2 AG2: monitoring tripped | Wrong setting, parameter. Clamped or open state was left | By technologist, setter. Check pressure switch "`Geöffnet`" or "`Gespannt`" |
| 10950 | F | `NC AG2: Nicht betriebsbereit` | NC AG2: not ready | Possible defect. Following-error overflow cross slide / C axis | By electrical maintenance staff. Check feed drives and encoders of the cross slide |
| 10951 | F | `NC AG2: Maximale Reaktionszeit der NC überschritten` | NC AG2: maximum NC response time exceeded | Possible defect. The response time of the NC to PLC signals is greater than 300 ms | By operator, if necessary Gildemeister Service. Acknowledge the error, possibly switch the machine off and on again. Please inform Gildemeister Service. |
| 11000 | F | `Parameter AG2: Übernahme nur bei Maschinenhochlauf` | Parameter AG2: accepted only at machine start-up | Wrong setting, parameter. When parameters are changed, switch the machine off | By technologist, setter. Change the parameter, switch the machine off and on again |
| 11001 | F | `Parameter Abholeinrichtungstyp AG2: Wert unzulässig` | Parameter pick-up device type AG2: value not permitted | Wrong setting, parameter. Parameter "`Handhabung und Materialzuführung`" (handling and material feed) wrong | By technologist, setter. Correct the parameter, see 9.1.2, "Parameterliste" on page 9-6 |
| 11002 | F | `Parameter Getriebetyp Hauptantrieb AG2: Wert unzulässig` | Parameter gearbox type main drive AG2: value not permitted | Wrong setting, parameter. Value in parameter "`Getriebedaten Hauptantrieb`" (main drive gearbox data) wrong | By technologist, setter. Correct the parameter, see 9.1.2, "Parameterliste" on page 9-6 |
| 11003 | F | `Parameter Handhabungstyp AG2: Typ unzulässig` | Parameter handling type AG2: type not permitted | Wrong setting, parameter. Parameter "`Handhabung und Materialzuführung`" wrong | By technologist, setter. Correct the parameter, see 9.1.2, "Parameterliste" on page 9-6 |
| 11004 | F | `Parameter Hauptspanner AG2: Arbeitsrichtung unzulässig` | Parameter main clamping cylinder AG2: working direction not permitted | Wrong setting, parameter. Wrong value in parameter "`Hauptspindel`" (main spindle) | By technologist, setter. Correct the parameter, see 9.1.2, "Parameterliste" on page 9-6 |
| 11005 | F | `Parameter Hauptspannfutter AG2: Spannart unzulässig` | Parameter main chuck AG2: clamping mode not permitted | Wrong setting, parameter. Wrong value in parameter "`Hauptspindel`" | By technologist, setter. Correct the parameter, see 9.1.2, "Parameterliste" on page 9-6 |
| 11006 | F | `Parameter Hauptspannfutter AG2: Typ unzulässig` | Parameter main chuck AG2: type not permitted | Wrong setting, parameter. Wrong main chuck type in parameter "`Hauptspindel`" | By technologist, setter. Correct the parameter, see 9.1.2, "Parameterliste" on page 9-6 |
| 11007 | F | `Parameter Lünettenanzahl AG2: Wert zu groß` | Parameter number of steady rests AG2: value too large | Wrong setting, parameter. Value in parameter "`Lünette Beschreibung`" (steady rest description) too large | By technologist, setter. Correct the parameter |
| 11008 | F | `Parameter Materialzuführungstyp AG2: Wert unzulässig` | Parameter material feed type AG2: value not permitted | Wrong setting, parameter. Parameter "`Handhabung und Materialzuführung`" wrong | By technologist, setter. Correct the parameter, see 9.1.2, "Parameterliste" on page 9-6 |
| 11009 | F | `Parameter Pinole AG2: Typ unzulässig` | Parameter quill AG2: type not permitted | Wrong setting, parameter. Quill type in "`Reitstock, Pinole`" (tailstock, quill) wrongly selected | By technologist, setter. Correct the parameter |
| 11010 | F | `Parameter Reitstocktyp AG2: Wert unzulässig` | Parameter tailstock type AG2: value not permitted | Wrong setting, parameter. Value in parameter "`Reitstock, Pinole`" wrongly set | By technologist, setter. Correct the parameter, see 9.1.2, "Parameterliste" on page 9-6 |
| 11011 | F | `Parameter Schwenkfuttertyp AG2: Wert unzulässig` ==> `nicht verwendet` | Parameter swivel chuck type AG2: value not permitted ==> not used | – | – |
| 11012 | F | `Parameter Werkzeugträger AG2: Typ unzulässig` | Parameter tool carrier AG2: type not permitted | Wrong setting, parameter. Wrong tool carrier type in parameter "`Werkzeugträgerdaten`" (tool carrier data) | By technologist, setter. Correct the parameter, see 9.1.2, "Parameterliste" on page 9-6 |
| 11013 | F | `Parameter Werkzeugträger AG2: Werkzeuganzahl unzulässig` | Parameter tool carrier AG2: number of tools not permitted | Wrong setting, parameter. Parameter "`Aufnahmeanzahl u. Spindel`" (number of holders and spindle) wrongly set | By technologist, setter. Correct the parameter, see 9.1.2, "Parameterliste" on page 9-6 |
| 11014 | F | `Parameter ISM-Temperaturregelung AG2: Wert "Anzahl Werte zur Mittelwertbildung" unzulässig` | Parameter ISM temperature control AG2: value "number of values for averaging" not permitted | Wrong setting, parameter. Value in parameter "`Temperaturregelung Spindeln`" (temperature control spindles) wrong | By technologist, setter. Correct the parameter |
| 11015 | F | `Parameter ISM-Temperaturregelung AG2: Wert "Hysterese Kühlwassertemperatur" unzulässig` | Parameter ISM temperature control AG2: value "hysteresis cooling water temperature" not permitted | Wrong setting, parameter. Value in parameter "`Temperaturregelung Spindeln`" wrong | By technologist, setter. Correct the parameter |
| 11016 | F | `Parameter ISM-Temperaturregelung AG2: Wert "Hysterese Lagertemperatur hinten" unzulässig` | ... value "hysteresis rear bearing temperature" not permitted | As 11015 | As 11015 |
| 11017 | F | `Parameter ISM-Temperaturregelung AG2: Wert "Hysterese Lagertemperatur vorn" unzulässig` | ... value "hysteresis front bearing temperature" not permitted | As 11015 | As 11015 |
| 11018 | F | `Parameter ISM-Temperaturregelung AG2: Wert "Hysterese Wicklungstemperatur" unzulässig` | ... value "hysteresis winding temperature" not permitted | As 11015 | As 11015 |
| 11019 | F | `Parameter ISM-Temperaturregelung AG2: Wert "MAX Lagertemperatur hinten" unzulässig` | ... value "MAX rear bearing temperature" not permitted | As 11015 | As 11015 |
| 11020 | F | `Parameter ISM-Temperaturregelung AG2: Wert "MAX Lagertemperatur vorn" unzulässig` | ... value "MAX front bearing temperature" not permitted | As 11015 | As 11015 |
| 11021 | F | `Parameter ISM-Temperaturregelung AG2: Wert "MAX Umgebungstemperatur" unzulässig` | ... value "MAX ambient temperature" not permitted | As 11015 | As 11015 |
| 11022 | F | `Parameter ISM-Temperaturregelung AG2: Wert "MAX Wicklungstemperatur" unzulässig` | ... value "MAX winding temperature" not permitted | As 11015 | As 11015 |
| 11023 | F | `Parameter ISM-Temperaturregelung AG2: Wert "MAXMAX Lagertemperatur hinten" unzulässig` | ... value "MAXMAX rear bearing temperature" not permitted | As 11015 | As 11015 |
| 11024 | F | `Parameter ISM-Temperaturregelung AG2: Wert "MAXMAX Lagertemperatur vorn" unzulässig` | ... value "MAXMAX front bearing temperature" not permitted | As 11015 | As 11015 |
| 11025 | F | `Parameter ISM-Temperaturregelung AG2: Wert "MAXMAX Wicklungstemperatur" unzulässig` | ... value "MAXMAX winding temperature" not permitted | As 11015 | As 11015 |
| 11026 | F | `Parameter ISM-Temperaturregelung AG2: Wert "Sollwert Kühlwassertemperatur" unzulässig` | ... value "setpoint cooling water temperature" not permitted | As 11015 | As 11015 |
| 11027 | F | `Parameter ISM-Temperaturregelung AG2: Wert "Sollwert Lagertemperatur hinten" unzulässig` | ... value "setpoint rear bearing temperature" not permitted | As 11015 | As 11015 |
| 11028 | F | `Parameter ISM-Temperaturregelung AG2: Wert "Sollwert Lagertemperatur vorn" unzulässig` | ... value "setpoint front bearing temperature" not permitted | As 11015 | As 11015 |
| 11029 | F | `Parameter ISM-Temperaturregelung AG2: Wert "Sollwert Wicklungstemperatur" unzulässig` | ... value "setpoint winding temperature" not permitted | As 11015 | As 11015 |
| 11030 | F | `Parameter Schmierung AG2: Wert "Schmierintervallzeit" unzulässig` | Parameter lubrication AG2: value "lubrication interval time" not permitted | Wrong setting, parameter. Value in parameter "`Schmierintervallzeit`" wrong | By technologist, setter. Correct the parameter |
| 11031 | F | `Parameter Schmierung AG2: Wert "Schmierunterdrückungszeit" unzulässig` | Parameter lubrication AG2: value "lubrication suppression time" not permitted | Wrong setting, parameter. Value in parameter "`Schmierunterdrückungszeit`" wrong | By technologist, setter. Correct the parameter |
| 11100 | F | `Pinole AG2: Druckeinbruch` | Quill AG2: pressure drop | Wrong setting, parameter. Quill clamped and pressure drop | By technologist, setter. Check the quill pressure switch setting |
| 11101 | M | `Pinole AG2: Endstellung vorn erreicht` | Quill AG2: front end position reached | Wrong setting, parameter. Front end position reached without pressure while clamping | By technologist, setter. Quill stroke not sufficient, move the tailstock |
| 11102 | F | `Pinole AG2: Fehlerhafte Eingabesignale` | Quill AG2: faulty input signals | Wrong setting, parameter. Wrong signals occurred while moving the quill | By technologist, setter. Check the quill setting (proximity switches, pressure switches) |
| 11103 | F | `Pinole AG2: Kommando unzulässig` | Quill AG2: command not permitted | Operation not permitted in this machine status. Spindle is switched on, no steady rest clamped | By operator. Stop the spindle or clamp a steady rest |
| 11104 | M | `Pinole AG2: Stellung undefiniert` | Quill AG2: position undefined | Operation not permitted in this machine status. Quill neither clamped nor fully retracted | By operator. Command "`Vor`" (forward) or "`Zurück`" (back) (M-command or foot switch) |
| 11105 | F | `Pinole AG2: Überwachung angesprochen` | Quill AG2: monitoring tripped | Wrong setting, parameter. Clamped or retracted state left without reason | By technologist, setter. Check the quill setting (proximity switches, pressure switches) |
| 11150 | F | `Reitstock AG2: Kommando unzulässig` | Tailstock AG2: command not permitted | Operation not permitted in this machine status. Not permitted because the quill is extended or the main spindle is turning | By operator. Acknowledge the error, correct the part program if necessary |
| 11151 | F | `Reitstock AG2: Nicht betriebsbereit` | Tailstock AG2: not ready | Wrong setting, parameter. Teach-in module of the automatic tailstock faulty | By technologist, setter. Check the device, reprogram if necessary |
| 11152 | M | `Reitstock AG2: Stellung undefiniert` | Tailstock AG2: position undefined | Operation not permitted in this machine status. The movable tailstock is in neither of its 2 end positions | By operator. Move with M-command or push-button "`VOR`" (forward) or "`ZURÜCK`" (back) |
| 11153 | F | `Reitstock AG2: Überwachung angesprochen` | Tailstock AG2: monitoring tripped | Wrong setting, parameter, possible defect. Limit switch of the movable tailstock misadjusted or defective | By electrical maintenance staff, by mechanical maintenance staff. Check limit switches, replace if necessary |
| 11200 | F | `Schaltschrankkühlung AG2: Filter verstopft` | Control cabinet cooling AG2: filter clogged | Maintenance necessary. Differential pressure before/after filter mat too high | By mechanical maintenance staff. Clean or change the filter |
| 11201 | W | `Schaltschrankkühlung AG2: Filter verstopft` | Control cabinet cooling AG2: filter clogged | As 11200 | As 11200 |
| 11202 | F | `Schaltschrankkühlung AG2: Überstrom` | Control cabinet cooling AG2: overcurrent | Possible defect. Motor protection switch has tripped | By electrical maintenance staff. Check current setting |
| 11203 | W | `Schaltschrankkühlung AG2: Überstrom` | Control cabinet cooling AG2: overcurrent | As 11202 | As 11202 |
| 11204 | F | `Schaltschrankkühlung AG2: Übertemperatur` | Control cabinet cooling AG2: overtemperature | Possible defect. Control cabinet overtemperature | By mechanical maintenance staff. Check cooling unit; ambient temperature too high? |
| 11205 | W | `Schaltschrankkühlung AG2: Übertemperatur` | Control cabinet cooling AG2: overtemperature | As 11204 | As 11204 |
| 11250 | F | `Schmierung AG2: Kein Druck` | Lubrication AG2: no pressure | Maintenance necessary. Lubrication pressure switch reports no pressure | By mechanical maintenance staff. Top up the lubricating oil reservoir, check the pressure switch |
| 11251 | W | `Schmierung AG2: Kein Druck` | Lubrication AG2: no pressure | As 11250 | As 11250 |
| 11252 | W | `Schmierung AG2: Minimum Niveau erreicht` | Lubrication AG2: minimum level reached | Maintenance necessary. Fill level of fresh-oil lubrication too low | By mechanical maintenance staff. Top up fresh oil |
| 11253 | F | `Schmierung AG2: Minimum Niveau erreicht` | Lubrication AG2: minimum level reached | As 11252 | As 11252 |
| 11254 | F | `Schmierung AG2: Motorüberstrom` | Lubrication AG2: motor overcurrent | Possible defect. Motor protection switch has tripped | By electrical maintenance staff. Check current setting |
| 11255 | W | `Schmierung AG2: Motorüberstrom` | Lubrication AG2: motor overcurrent | As 11254 | As 11254 |
| 11256 | M | `Schmierung AG2: Öl nachfüllen` | Lubrication AG2: top up oil | Maintenance necessary. Fill level of fresh-oil lubrication not sufficient | By mechanical maintenance staff. Top up fresh oil |
| 11257 | F | `Schmierung AG2: Permanenter Druck` | Lubrication AG2: permanent pressure | Maintenance necessary. Lubrication pressure switch reports permanent pressure | By mechanical maintenance staff. Check lubrication pressure lines and/or pressure switch |
| 11258 | W | `Schmierung AG2: Permanenter Druck` | Lubrication AG2: permanent pressure | As 11257 | As 11257 |
| 11259 | F | `Schmierung AG2: Kein Druck Kreis 1` | Lubrication AG2: no pressure circuit 1 | Maintenance necessary. Lubrication pressure switch reports no pressure | By mechanical maintenance staff. Top up the lubricating oil reservoir, check the pressure switch |
| 11260 | F | `Schmierung AG2: Kein Druck Kreis 2` | Lubrication AG2: no pressure circuit 2 | As 11259 | As 11259 |
| 11261 | F | `Schmierung AG2: Permanenter Druck Kreis 1` | Lubrication AG2: permanent pressure circuit 1 | Maintenance necessary. Lubrication pressure switch reports permanent pressure | By mechanical maintenance staff. Check lubrication pressure lines and/or pressure switch |
| 11262 | F | `Schmierung AG2: Permanenter Druck Kreis 2` | Lubrication AG2: permanent pressure circuit 2 | As 11261 | As 11261 |
| 11263 | F | `Schmierung AG2: Kommando unzulässig` ==> `nicht verwendet` | Lubrication AG2: command not permitted ==> not used | – | – |
| 11264 | F | `Schmierung AG2: Maximale Unterdrückungszeit überschritten` | Lubrication AG2: maximum suppression time exceeded | Wrong parameter setting or programming error. Maximum suppression time exceeded | By technologist, setter. Check parameters; cancel the lubrication pulse suppression by M49, M18, M30, M99 or cycle stop |
| 11265 | W | `Schmierung AG2: Kein Druck Kreis 1` | Lubrication AG2: no pressure circuit 1 | As 11259 | As 11259 |
| 11266 | W | `Schmierung AG2: Kein Druck Kreis 2` | Lubrication AG2: no pressure circuit 2 | As 11259 | As 11259 |
| 11267 | W | `Schmierung AG2: Permanenter Druck Kreis 1` | Lubrication AG2: permanent pressure circuit 1 | As 11261 | As 11261 |
| 11268 | W | `Schmierung AG2: Permanenter Druck Kreis 2` | Lubrication AG2: permanent pressure circuit 2 | As 11261 | As 11261 |
| 11269 | W | `Schmierung AG2: Maximale Unterdrückungszeit überschritten` | Lubrication AG2: maximum suppression time exceeded | As 11264 | As 11264 (cancel suppression by M49, M18, M30, M99 or cycle stop) |
| 11300 | F | `Schutzhaube AG2: Max. Zeit zum auf/zufahren überschritten` | Guard cover AG2: max. time for opening/closing exceeded | Possible defect, wrong setting, parameter. Possible defect at the automatic guard cover | By electrical maintenance staff, by mechanical maintenance staff. Check function, possibly correct parameter 031 |
| 11301 | M | `Schutzhaube AG2: Motorüberstrom` | Guard cover AG2: motor overcurrent | Wrong setting, parameter, possible defect. Motor protection switch has tripped | By electrical maintenance staff. Check current setting |
| 11302 | M | `Schutzhaube AG2: Nicht geöffnet und nicht geschlossen` | Guard cover AG2: neither open nor closed | Operation not permitted in this machine status. Guard cover not locked and not open | By operator. Open the guard cover and then close it |
| 11303 | M | `Schutzhaube AG2: Öffnen nicht erlaubt da Taster betätigt` | Guard cover AG2: opening not allowed because a push-button is pressed | Operation not permitted in this machine status. A push-button is pressed while opening the guard cover | By operator. Do not press any push-button and then open the guard cover |
| 11304 | M | `Schutzhaube AG2: Sicherheitsleiste hat angespochen` [sic] | Guard cover AG2: safety edge has tripped | Operation not permitted in this machine status. Closing of the automatic guard cover interrupted | By operator. Acknowledge the error, check the safety edge if necessary |
| 11305 | F | `Schutzhaube AG2: Störung Sicherheitskreis` | Guard cover AG2: safety circuit fault | Possible defect. Guard cover safety circuit has wrong signal sequence | By electrical maintenance staff. Check the safety circuit (see circuit diagram) |
| 11306 | F | `Schutzhaube AG2: Unzulässig geöffnet` | Guard cover AG2: opened impermissibly | Operation not permitted in this machine status. Guard cover was opened with the emergency release | By operator. Open the guard cover and then close it |
| 11350 | F | `Späneförderer AG2: Motorüberstrom` | Chip conveyor AG2: motor overcurrent | Operation not permitted in this machine status. Motor protection switch has tripped | By operator, by electrical maintenance staff. Run the chip conveyor empty, check current setting |
| 11351 | W | `Späneförderer AG2: Motorüberstrom` | Chip conveyor AG2: motor overcurrent | As 11350 | As 11350 |
| 11400 | F | `Stangenbetrieb AG2: Kommando unzulässig` | Bar operation AG2: command not permitted | Operation not permitted in this machine status. Command M94 triggered without bar operation running | By operator. Select and switch on bar operation |
| 11401 | F | `Stangenbetrieb AG2: Nicht betriebsbereit` | Bar operation AG2: not ready | Operation not permitted in this machine status. Bar operation selected but not ready | By electrical maintenance staff. Check the bar equipment |
| 11402 | M | `Stangenbetrieb AG2: Stange wechseln` | Bar operation AG2: change bar | Operation not permitted in this machine status. Bar is used up | By operator. Insert a new bar, since there is no bar loading magazine |
| 11450 | F | `Vorschubantriebe AG2: Motorüberstrom` | Feed drives AG2: motor overcurrent | Possible defect. Motor protection switch has tripped | By electrical maintenance staff. Check current setting |
| 11451 | F | `Vorschubantriebe AG2: Motorübertemperatur` | Feed drives AG2: motor overtemperature | Wrong setting, parameter, possible defect. One of the feed drives reports overtemperature | By electrical maintenance staff. Check motor and drive controller, let the motor cool down |
| 11452 | F | `Vorschubantriebe AG2: Nicht betriebsbereit` | Feed drives AG2: not ready | Possible defect. Servo controller of the feed drives faulty | By electrical maintenance staff. See electrical service instructions: controllers, circuit diagrams |
| 11453 | F | `Vorschubantriebe AG2: Schlitten-X nicht betriebsbereit` | Feed drives AG2: slide X not ready | Possible defect. The controller for slide X is faulty | By electrical maintenance staff. Clear the fault, see electrical service manual |
| 11454 | F | `Vorschubantriebe AG2: Schlitten-X Temperaturvorwarnung` | Feed drives AG2: slide X temperature pre-warning | Wrong setting, parameter, possible defect or overload in the cycle. One of the feed drives reports overtemperature | By electrical maintenance staff. Let the drive cool down, check drive, check controller |
| 11455 | W | `Vorschubantriebe AG2: Schlitten-X Temperaturvorwarnung` | Feed drives AG2: slide X temperature pre-warning | As 11454 | As 11454 |
| 11456 | F | `Vorschubantriebe AG2: Schlitten-Z nicht betriebsbereit` | Feed drives AG2: slide Z not ready | Possible defect. The controller for slide Z is faulty | By electrical maintenance staff. Clear the fault, see electrical service manual |
| 11457 | F | `Vorschubantriebe AG2: Lüfter X-Achse Motorüberstrom` | Feed drives AG2: X axis fan motor overcurrent | Possible defect. Motor protection switch has tripped | By electrical maintenance staff. Check current setting |
| 11458 | W | `Vorschubantriebe AG2: Schlitten-Z Temperaturvorwarnung` | Feed drives AG2: slide Z temperature pre-warning | As 11454 | As 11454 |
| 11459 | F | `Vorschubantriebe AG2: Schlitten-Z Temperaturvorwarnung` | Feed drives AG2: slide Z temperature pre-warning | As 11454 | As 11454 |
| 11460 | W | `Vorschubantriebe AG2: Lüfter X-Achse Motorüberstrom` | Feed drives AG2: X axis fan motor overcurrent | As 11457 | As 11457 |
| 11461 | M | `Vorschubantriebe AG2: Lüfter X-Achse Motorüberstrom` | Feed drives AG2: X axis fan motor overcurrent | As 11457 | As 11457 |
| 11500 | F | `Werkzeugträger AG2: Motorüberstrom` | Tool carrier (turret) AG2: motor overcurrent | Possible defect. Motor protection switch has tripped | By electrical maintenance staff. Check current setting |
| 11501 | W | `Werkzeugträger AG2: Motorüberstrom` | Tool carrier AG2: motor overcurrent | As 11500 | As 11500 |
| 11502 | F | `Werkzeugträger AG2: Motorübertemperatur` | Tool carrier AG2: motor overtemperature | Operation not permitted in this machine status. Motor temperature monitoring has tripped | By electrical maintenance staff. Let the motor cool down, press the reset button on the device |
| 11503 | W | `Werkzeugträger AG2: Motorübertemperatur` | Tool carrier AG2: motor overtemperature | As 11502 | As 11502 |
| 11504 | F | `Werkzeugträger AG2: Nicht in Position verriegelt` | Tool carrier AG2: not locked in position | Operation not permitted in this machine status. Tool carrier in wrong position or unlocked | By operator. Index the tool carrier with a T command |
| 11505 | F | `Werkzeugträger AG2: Parity-Check fehlerhaft` | Tool carrier AG2: parity check failed | Possible defect. Tool carrier position encoder had a parity error | By operator, by electrical maintenance staff. Index the tool carrier (Tx); check position encoder, cables |
| 11506 | F | `Werkzeugträger AG2: Position nicht gefunden` | Tool carrier AG2: position not found | Wrong setting, parameter, possible defect. Position encoder faulty or value in parameter wrong | By technologist, setter, by mechanical maintenance staff. Check the position encoder or correct the parameter |
| 11507 | F | `Werkzeugträger AG2: T-Wert für Werkzeugträger zu groß` | Tool carrier AG2: T value too large for tool carrier | Operation not permitted in this machine status, wrong setting, parameter. Entered T value > value entered in parameter "`Aufnahmeanzahl`" (number of holders) | By operator, by technologist, setter. Correct the parameter or enter a smaller T value |
| 11508 | F | `Werkzeugträger AG2: T-Wert konnte nicht übernommen werden` | Tool carrier AG2: T value could not be accepted | Operation not permitted in this machine status. Execution was blocked because of the machine state | By operator. Correct the machine state (e.g. close the guard) |
| 11509 | F | `Werkzeugträger AG2: Vorindexierung fehlerhaft` | Tool carrier AG2: pre-indexing faulty | Operation not permitted in this machine status. During the tool carrier test the pre-indexing was faulty | By operator. Index the tool carrier with a T command |
| 11510 | F | `Werkzeugträger AG2: Werkzeugträger verriegelt nicht` | Tool carrier AG2: tool carrier does not lock | Wrong setting, parameter. Initiator (proximity sensor) "`Kontrolle - Verriegelung`" (check - locking) does not switch | By technologist, setter. Check the initiator, readjust and test if necessary |
| 11511 | F | `Werkzeugträger AG2: Überlastkupplung angesprochen` | Tool carrier AG2: overload clutch tripped | Possible defect or overload. Overload clutch has tripped | By mechanical maintenance staff, by electrical maintenance staff. Clarify the cause of the fault and set up the tool carrier again |
| 11512 | F | `Werkzeugträger AG2: Werkzeugträger entriegelt nicht` | Tool carrier AG2: tool carrier does not unlock | Possible defect or wrong setting, parameter. The tool carrier does not unlock within the preset time | By mechanical and electrical maintenance staff. Clarify the cause and set up the tool carrier again if necessary |
| 11513 | F | `Werkzeugträger AG2: Werkzeugträger Referenzfahrt fehlerhaft` | Tool carrier AG2: tool carrier reference run faulty | Possible defect or wrong setting, parameter. The tool carrier does not complete the reference run within the preset time | By mechanical and electrical maintenance staff. Clarify the cause and set up the tool carrier again if necessary |
| 11514 | F | `Werkzeugträger AG2: Interner Werkzeugträger-Fehler` | Tool carrier AG2: internal tool carrier error | Possible defect. The tool carrier does not clearly report "`Scheibe ausgekuppelt`" (disk disengaged) or "`Spindel ausgekuppelt`" (spindle disengaged) | By mechanical and electrical maintenance staff. Clarify the cause and set up the tool carrier again |
| 11550 | F | `Ausgabemodule AG2: Steuerspannung ausgefallen` | Output modules AG2: control voltage failed | Possible defect. Miniature circuit breaker has tripped | By electrical maintenance staff. Eliminate the cause of the overload |
| 11551 | F | `Eingabemodule AG2: Steuerspannung ausgefallen` | Input modules AG2: control voltage failed | As 11550 | As 11550 |
| 11552 | F | `Maschine AG2: Nicht eingeschaltet` | Machine AG2: not switched on | Operation not permitted in this machine status. Drives not switched on at the control panel | By operator. Press push-button "`Antriebe Ein`" (drives on); the lamp in the push-button must light |
| 11553 | F | `Maschine AG2: Not-Aus` | Machine AG2: emergency stop | Operation not permitted in this machine status. Emergency stop button pressed | By operator. Unlatch the emergency stop button |
| 11554 | F | `Störung Maschine AG2: Siehe Fehlerliste` | Machine fault AG2: see error list | Possible defect. Only with certain options, see circuit diagram I 04.1 | By electrical maintenance staff. Clear the fault |
| 11555 | F | `Ventile AG2: Steuerspannung ausgefallen` | Valves AG2: control voltage failed | Possible defect. Miniature circuit breaker has tripped | By electrical maintenance staff. Eliminate the cause of the overload |
| 11556 | F | `Warnung Maschine AG2: Siehe Fehlerliste` | Machine warning AG2: see error list | Possible defect. Only with certain options, see circuit diagram I 04.2 | By electrical maintenance staff. Clear the fault |
| 11557 | W | `Warnung Maschine AG2: Siehe Fehlerliste` | Machine warning AG2: see error list | As 11556 | As 11556 |
| 11600 | F | `Einlegehilfe AG2: Kommando unzulässig` ==> `nicht verwendet` | Loading aid AG2: command not permitted ==> not used | – | – |
| 11601 | M | `Einlegehilfe AG2: Stellung undefiniert` | Loading aid AG2: position undefined | Operation not permitted in this machine status. Loading aid is not in an end position | By operator. Move the loading aid to the desired end position by M-command |
| 11602 | F | `Einlegehilfe AG2: Überwachung angesprochen` | Loading aid AG2: monitoring tripped | Wrong setting, parameter, possible defect. Loading aid left the end position without a command | By electrical and mechanical maintenance staff. Loading aid (check hydraulics and limit switches) |
| 11650 | M | `Schwenkfutter AG2: Änderung der Anwahl unzulässig` ==> `nicht verwendet` | Swivel chuck AG2: change of selection not permitted ==> not used | – | – |
| 11651 | F | `Schwenkfutter AG2: Kommando unzulässig` | Swivel chuck AG2: command not permitted | Operation not permitted in this machine status. Spindle speed too high or hardware swivel enable missing | By operator. Repeat the operation at a lower main spindle speed; check the parameter for maximum swivel speed if necessary |
| 11652 | M | `Schwenkfutter AG2: Stellung undefiniert` | Swivel chuck AG2: position undefined | Swivel command aborted. The swivel chuck is in an undefined intermediate position | By operator. Swivel the chuck into any position |
| 11653 | F | `Schwenkfutter AG2: Überwachung angesprochen` | Swivel chuck AG2: monitoring tripped | Possible defect. The swivel chuck left its current state without a command | By technologist, setter. Check swivel chuck settings (proximity switches, pressure switches) |
| 11654 | F | `Schwenkfutter AG2: Zielposition nicht gefunden` | Swivel chuck AG2: target position not found | Possible defect. The swivel chuck did not find the programmed position | By technologist, setter. Check swivel chuck proximity switch settings |
| 11655 | F | `Schwenkfutter AG2: Maximale Schwenkzeit überschritten` | Swivel chuck AG2: maximum swivel time exceeded | Possible defect. The swivel chuck could not complete the swivel operation within 20 seconds | By technologist, setter. Check swivel chuck settings (pressure switches, pressure reducers), valves |
| 11700 | M | `Abholeinrichtung Greifen AG2: Stellung undefiniert` | Pick-up device gripping AG2: position undefined | Operation not permitted in this machine status. Gripper is in neither of its two end positions | By operator. Command "`Greifen`" (grip) or "`Lösen`" (release) (M-command or push-button) |
| 11701 | F | `Abholeinrichtung Greifen AG2: Überwachung angesprochen` | Pick-up device gripping AG2: monitoring tripped | Wrong setting, parameter. The selected status was left without reason | By technologist, setter. Check proximity switch "`Greifen`" |
| 11702 | M | `Abholeinrichtung Klappe AG2: Stellung undefiniert` | Pick-up device flap AG2: position undefined | Operation not permitted in this machine status. Flap is in neither of its two end positions | By operator. Command "`Auf`" (open) or "`Zu`" (close) (M-command or push-button) |
| 11703 | F | `Abholeinrichtung Klappe AG2: Überwachung angesprochen` | Pick-up device flap AG2: monitoring tripped | Wrong setting, parameter. The selected status was left without reason | By technologist, setter. Check proximity switch "`Klappe`" (flap) |
| 11704 | M | `Abholeinrichtung Schwenken AG2: Stellung undefiniert` | Pick-up device swivel AG2: position undefined | Operation not permitted in this machine status. Swivel arm is in neither of its two end positions | By operator. Command "`Vor`" (forward) or "`Zurück`" (back) (M-command or push-button) |
| 11705 | F | `Abholeinrichtung Schwenken AG2: Überwachung angesprochen` | Pick-up device swivel AG2: monitoring tripped | Wrong setting, parameter. The selected status was left without reason | By technologist, setter. Check proximity switch "`Schwenken`" (swivel) |
| 11706 | F | `Abholeinrichtung AG2: Kommando unzulässig` | Pick-up device AG2: command not permitted | Operation not permitted in this machine status. The command is not permitted in this machine state | By operator, by electrical maintenance staff. Change the machine state and repeat the command |
| 11707 | F | `Abholeinrichtung AG2: Maximale Ausführungszeit überschritten` | Pick-up device AG2: maximum execution time exceeded | Wrong setting, parameter. Pick-up command was not executed within the preset time | By technologist, setter. Check the pick-up device |
| 11708 | F | `Abholeinrichtung Förderband AG2: Ausführung fehlerhaft` | Pick-up device conveyor belt AG2: execution faulty | Possible defect. The workpiece did not arrive in the conveyor channel within the maximum time set by parameter | By technologist, setter. Check pick-up device / parameters |
| 11709 | F | `Abholeinrichtung Förderband AG2: Motorüberstrom` | Pick-up device conveyor belt AG2: motor overcurrent | Possible defect. Motor protection switch has tripped | By electrical maintenance staff. Check current setting |
| 11710 | W | `Abholeinrichtung Förderband AG2: Motorüberstrom` | Pick-up device conveyor belt AG2: motor overcurrent | As 11709 | As 11709 |
| 11711 | M | `Abholeinrichtung Förderband AG2: Werkstück im Förderkanal` | Pick-up device conveyor belt AG2: workpiece in conveyor channel | Possible defect. The workpiece is in the conveyor channel with the conveyor belt switched off | By operator. Switch on the conveyor belt to transport the workpiece out of the conveyor channel |
| 11750 | F | `Angetriebene Werkzeuge AG2: Kommando unzulässig` | Driven tools AG2: command not permitted | Operation not permitted in this machine status. Guard covers are open. The spindle enable for the driven tools is missing, or the tool carrier was not at a driven-tool position | By operator, by technologist, setter or by electrical maintenance staff. Index to a driven-tool position or clarify the cause of the missing spindle enable |
| 11751 | F | `Angetriebene Werkzeuge AG2: Kuppelvorgang fehlerhaft` | Driven tools AG2: coupling operation faulty | Wrong setting, parameter. Coupling operation at M203 / M204 / M205 faulty | By technologist, setter. Check proximity switches engaged/disengaged (`Eingekuppelt/Ausgekuppelt`) |
| 11752 | F | `Angetriebene Werkzeuge AG2: Kupplungsüberwachung angesprochen` | Driven tools AG2: coupling monitoring tripped | Wrong setting, parameter. The selected status was left without reason | By technologist, setter. Check proximity switches engaged/disengaged |
| 11753 | F | `Angetriebene Werkzeuge AG2: Nicht betriebsbereit` | Driven tools AG2: not ready | Possible defect. The controller for driven tools is faulty | By electrical maintenance staff. Clear the fault, see electrical service manual |
| 11754 | F | `Angetriebene Werkzeuge AG2: Temperaturvorwarnung` | Driven tools AG2: temperature pre-warning | Wrong setting, parameter, possible defect or overload in the cycle. The controller for driven tools reports overtemperature | By electrical maintenance staff. Let the drive cool down, check drive, check controller |
| 11755 | F | `Angetriebene Werkzeuge AG2: Übertemperatur` | Driven tools AG2: overtemperature | Wrong setting, parameter, overload in the cycle or possible defect. The drive for driven tools reports overtemperature | By electrical maintenance staff. Let the drive cool down, check drive, check controller |
| 11755 (sic, number printed twice) | W | `Angetriebene Werkzeuge AG2: Temperaturvorwarnung` | Driven tools AG2: temperature pre-warning | Wrong setting, parameter, possible defect or overload in the cycle. The drive for driven tools reports overtemperature | As above |
| 11800 | F | `C-Achse Hauptspindel AG2: Überwachung angesprochen` | C axis main spindle AG2: monitoring tripped | Wrong setting, parameter, possible defect. C axis pressure switch and limit switch misadjusted or defective | By electrical maintenance staff, by mechanical maintenance staff. Check pressure switch and limit switch, replace if necessary |
| 11850 | F | `Drehzahlüberwachung AG2: Gerät gestört oder angesprochen` | Speed monitoring AG2: device faulty or tripped | Operation not permitted in this machine status, possible defect. Spindle turning > 50 min⁻¹ in setting-up mode, fault in speed monitoring device | By operator, by electrical maintenance staff. Spindle < 50 min⁻¹; check the speed monitoring device |
| 11900 | F | `AG2: Siehe SPS-Fehlerliste` ==> `nicht verwendet` | AG2: see PLC error list ==> not used | – | – |
| 11901 | M | `AG2: Siehe SPS-Fehlerliste` ==> `nicht verwendet` | AG2: see PLC error list ==> not used | – | – |
| 11902 | W | `AG2: Siehe SPS-Fehlerliste` ==> `nicht verwendet` | AG2: see PLC error list ==> not used | – | – |
| 11950 | F | `Arbeitsraumtrennwand AG2: Kommando unzulässig` | Work area partition wall AG2: command not permitted | Operation not permitted in this machine status. Guard covers are open | By operator. Close the guard covers |
| 11951 | M | `Arbeitsraumtrennwand AG2: Stellung undefiniert` | Work area partition wall AG2: position undefined | Operation not permitted in this machine status. Partition wall is in neither of its two end positions | By operator. Command "`Schließen`" (close) or "`Öffnen`" (open) (M-command) |
| 11952 | F | `Arbeitsraumtrennwand AG2: Überwachung angesprochen` | Work area partition wall AG2: monitoring tripped | Wrong setting, limit switch defective. The selected status was left without reason | By technologist, setter. Check hydraulics and limit switches |
| 11953 | F | `Arbeitsraumtrennwand AG2: Maximale Ausführungszeit überschritten` | Work area partition wall AG2: maximum execution time exceeded | Wrong setting, limit switch defective. Command was not executed within the preset time | By technologist, setter. Check hydraulics and limit switches |
| 12000 | F | `ISM-Kühlung AG2: Lüfter Wärmetauscher Motorüberstrom` | ISM cooling AG2: heat exchanger fan motor overcurrent | Possible defect. Motor protection switch has tripped | By electrical maintenance staff. Check current setting |
| 12001 | W | `ISM-Kühlung AG2: Lüfter Wärmetauscher Motorüberstrom` | ISM cooling AG2: heat exchanger fan motor overcurrent | As 12000 | As 12000 |
| 12002 | F | `ISM-Kühlung AG2: Pumpe Motorüberstrom` | ISM cooling AG2: pump motor overcurrent | As 12000 | As 12000 |
| 12003 | W | `ISM-Kühlung AG2: Pumpe Motorüberstrom` | ISM cooling AG2: pump motor overcurrent | As 12000 | As 12000 |
| 12050 | F | `Schaltschrank AG2: Übertemperatur` | Control cabinet AG2: overtemperature | Possible defect. Control cabinet overtemperature | By mechanical maintenance staff. Check the cooling unit, or ambient temperature too high? |
| 12051 | W | `Schaltschrank AG2: Übertemperatur` | Control cabinet AG2: overtemperature | As 12050 | As 12050 |
| 12100 | F | `ISM-Temperaturregelung AG2: AEM-Modul nicht betriebsbereit` | ISM temperature control AG2: AEM module not ready | Possible defect. The AEM module is not ready for operation | By electrical maintenance staff. Check the AEM module |
| 12101 | W | `ISM-Temperaturregelung AG2: AEM-Modul nicht betriebsbereit` | ISM temperature control AG2: AEM module not ready | As 12100 | As 12100 |
| 12102 | F | `ISM-Temperaturregelung AG2: max. Lagertemperatur hinten überschritten` | ISM temperature control AG2: max. rear bearing temperature exceeded | Possible defect or wrong setting, parameter. The max. rear bearing temperature was exceeded | By mechanical and electrical maintenance staff. Let the bearing cool down, check the bearing, check parameters |
| 12103 | W | `ISM-Temperaturregelung AG2: max. Lagertemperatur hinten überschritten` | As 12102 | As 12102 | As 12102 |
| 12104 | F | `ISM-Temperaturregelung AG2: max. Lagertemperatur vorn überschritten` | ISM temperature control AG2: max. front bearing temperature exceeded | Possible defect or wrong setting, parameter. The max. front bearing temperature was exceeded | As 12102 |
| 12105 | W | `ISM-Temperaturregelung AG2: max. Lagertemperatur vorn überschritten` | As 12104 | As 12104 | As 12102 |
| 12106 | F | `ISM-Temperaturregelung AG2: max. Motorwicklungstemperatur überschritten` | ISM temperature control AG2: max. motor winding temperature exceeded | Wrong setting, parameter, possible defect or overload in the cycle. The max. motor winding temperature was exceeded | By mechanical and electrical maintenance staff. Let the motor cool down, check the motor, check parameters |
| 12107 | W | `ISM-Temperaturregelung AG2: max. Motorwicklungstemperatur überschritten` | As 12106 | As 12106 | As 12106 |
| 12108 | M | `ISM-Temperaturregelung AG2: max. Umgebungstemperatur überschritten` | ISM temperature control AG2: max. ambient temperature exceeded | Possible defect or wrong setting, parameter. The max. ambient temperature was exceeded | By electrical maintenance staff. Check the temperature sensor, check parameters |
| 12109 | F | `ISM-Temperaturregelung AG2: AEM-Modul reagiert nicht` | ISM temperature control AG2: AEM module not responding | Possible defect or wrong setting, parameter. The AEM module does not respond properly to the measurement request | By electrical maintenance staff. Check the AEM module, check the temperature sensor, check parameters |
| 12110 | M | `ISM-Temperaturregelung AG2: AEM-Modul reagiert nicht` | As 12109 | Possible defect or wrongly set parameter. As 12109 | As 12109 |
| 12111 | W | `ISM-Temperaturregelung AG2: AEM-Modul reagiert nicht` | As 12109 | As 12109 | As 12109 |
| 12112 | F | `ISM-Temperaturregelung AG2: maxmax. Lagertemperatur hinten überschritten` | ISM temperature control AG2: max-max rear bearing temperature exceeded | Possible defect or wrong setting, parameter. The maxmax. rear bearing temperature was exceeded | As 12102 |
| 12113 | F | `ISM-Temperaturregelung AG2: maxmax. Lagertemperatur vorn überschritten` | ISM temperature control AG2: max-max front bearing temperature exceeded | Possible defect or wrong setting, parameter. The maxmax. front bearing temperature was exceeded | As 12102 |
| 12114 | F | `ISM-Temperaturregelung AG2: maxmax. Motorwicklungstemperatur überschritten` | ISM temperature control AG2: max-max motor winding temperature exceeded | Wrong setting, parameter, possible defect or overload in the cycle. The maxmax. motor winding temperature was exceeded | As 12106 |
| 12150 | F | `Versorgung AG2: Nicht betriebsbereit` | Supply AG2: not ready | Possible defect. The supply is faulty | By electrical maintenance staff. Clear the fault, see electrical service manual |
| 12151 | F | `Versorgung AG2: Temperaturvorwarnung` | Supply AG2: temperature pre-warning | Wrong setting, parameter, possible defect or overload in the cycle. The supply reports overtemperature | By electrical maintenance staff. Check the supply |
| 12152 | W | `Versorgung AG2: Temperaturvorwarnung` | Supply AG2: temperature pre-warning | As 12151 | As 12151 |
| 12250 | F | `SPS AG2: Interner Softwarefehler` | PLC AG2: internal software error | Internal error of the PLC | By technologist, setter, if necessary Gildemeister Service. Check the PLC parameter settings and switch the machine off and on again. Please inform Gildemeister Service. |

Notes on the printed list: no 10802 is printed (10801 is followed by the revised entry 10803); 11755 is printed twice (F `Übertemperatur` and W `Temperaturvorwarnung`); 10603 reads "AG1" although it sits in the AG2 range; 11304 has the typo `angespochen`. Page 9-183 contains only 12250.

---

## 9.4 External events (`Externe Ereignisse`)

(Page 9-184.)

With the event query, the part program can react to external events (signals from the machine, peripherals etc.). This makes an individual part program sequence possible. The query is made with IF, WHILE queries (see programming manual).

**Example (`Beispiel`):**

```
IF        IF {E1 [1] ==1}
THEN
:
:         wenn das Ereignis "1" ist     (if the event is "1")
:
ELSE
:
:
:         wenn das Ereignis "0" ist     (if the event is "0")
:
ENDIF
```

- `E1` = `Ereignis-Liste von Schlitten 1` (event list of slide 1)
- `E2` = `Ereignis-Liste von Schlitten 2` (event list of slide 2)
- `[1]` = `Ereignis 1 aus der Liste` (event 1 from the list)
- :
- `[16]` = `Ereignis 16 aus der Liste` (event 16 from the list)
- `==1` = `Abfrage auf "1"` (query for "1")

### 9.4.1 Assignment of the event flags (`Belegung der Ereignis Merker`)

(Page 9-185, revised REV dab9501; the rows for branch 4 are marked "!".)

Slide 1, AG1 (`Schlitten 1 AG1`):

| Branch (`Verzweigung`) | Assignment (`Belegung`) | English | CTX 200 | CTX 400 | CTX 500 | CTX 600 | CTX twin |
|---|---|---|---|---|---|---|---|
| 1 | `Materialzuführung = Stangenende erreicht` | Material feed = bar end reached | X | X | X | X | X |
| 4 | `Werkstückanlagekontrolle für Spindel H` | Workpiece seating check for spindle H | | | | | X |
| 7 | `Werkzeugüberwachung` | Tool monitoring | | X | X | X | |
| 9 | `Handhabung Eingang X auf Highsignal` | Handling input X at high signal | | X | X | X | X |
| 10 | `Handhabung Eingang Y auf Highsignal` | Handling input Y at high signal | | X | X | X | X |
| 12 | `Zustand Spannfutter der Spindel H = "Gespannt"` | State of chuck of spindle H = "clamped" | | | | | X |

Slide 2, AG2 (`Schlitten 2 AG2`):

| Branch (`Verzweigung`) | Assignment (`Belegung`) | English | CTX 200 | CTX 400 | CTX 500 | CTX 600 | CTX twin |
|---|---|---|---|---|---|---|---|
| 1 | `Materialzuführung = Stangenende erreicht` | Material feed = bar end reached | | | | | X |
| 4 | `Werkstückanlagekontrolle für Spindel 3` | Workpiece seating check for spindle 3 | | | | | X |
| 9 | `Handhabung Eingang X auf Highsignal` | Handling input X at high signal | | | | | X |
| 10 | `Handhabung Eingang Y auf Highsignal` | Handling input Y at high signal | | | | | X |
| 12 | `Zustand Spannfutter der Spindel H = "Gespannt"` | State of chuck of spindle H = "clamped" | | | | | X |

Page 9-186: (blank page; header only).

---

## For the retrofit

- Control: these pages name no CNC make/model. The PLC is the Gildemeister PLC ("SPS") with an NC; the NC must answer PLC signals within 300 ms or 10951 trips (p. 9-154). Part programs use IF/WHILE with event lists `E1[n]`/`E2[n]` (16 events per slide), i.e. a high-level-language NC (see programming manual) (p. 9-184).
- Two independent sides: AG1 = slide 1, AG2 = slide 2, each with its own full PLC message set (AG2 = AG1 number + 5000), own hydraulics, lubrication, coolant, guard, turret, spindle, supply and control-cabinet monitoring (pp. 9-137 to 9-183). Fits one LinuxCNC/stmbl controller per side.
- Axes per side: cross slide X and Z (`Schlitten-X`, `Schlitten-Z`, separate controllers, X axis has a motor fan), C axis on the main spindle with pressure switch and limit switch (clamp/engagement) (11453-11461, 11800, 10950). Following-error overflow on cross slide / C axis = "NC not ready" (10950, p. 9-154).
- Main spindle: ISM (integrated spindle motor) with water cooling (heat-exchanger fan + pump), temperature control via an "AEM module" measuring rear/front bearing and winding temperature, with setpoint, hysteresis, MAX and MAXMAX limits per value and ambient max (7000-7114, 11014-11029, 12000-12114). Main drive gearbox with shift monitoring (10450-10453). Separate fan on spindle drive (10653). Spindle clamp (brake) only inside a C-axis position window, monitored by proximity switches (10700-10703).
- Speed monitoring device: spindle must stay < 50 min⁻¹ in setting-up mode (door open) (6850, 11850). Setting-up mode only with guard open and manual mode selected (10350/10351).
- Chuck/clamping: hydraulic main clamping cylinder with pressure switch "Spanndruck", front/rear stroke-limit proximity switches, useful stroke; clamp/release must finish within 20 s; spindle may not run > 90 s with chuck open; no chuck command with spindle turning; chuck max speed parameter; chuck change position needs chuck key in enabling switch; clamp/release by M-command or foot switch (10550-10603). Workpiece seating check is an event (spindle H / spindle 3) and message 10603.
- Tailstock/quill and steady rests: quill clamp pressure switch, proximity switches, forward/back by M-command or foot switch; movable tailstock with 2 end positions, limit switches, teach-in module; steady rests 1 and 2 with open/clamped pressure switches; interlocks: quill not with spindle on unless a steady rest is clamped, tailstock not with quill extended or spindle turning (10900-10904, 11100-11153).
- Turret (`Werkzeugträger`): motor with overcurrent/overtemperature, absolute position encoder with parity check, locking initiator "Kontrolle - Verriegelung", unlock and reference-run timeouts, overload clutch, "disk disengaged"/"spindle disengaged" signals, T value limited by parameter "Aufnahmeanzahl"; indexing only with guard closed (11500-11514).
- Driven tools: coupling by M203 / M204 / M205 with engaged/disengaged proximity switches, own controller with temperature pre-warning; need guard closed, spindle enable and turret at a driven-tool position (11750-11755).
- Hydraulics: system-pressure switch, filter differential-pressure switch, oil level min, oil overtemperature, motor protection (10750-10757).
- Lubrication: interval time and suppression time parameters, pressure switches for circuit 1 and 2 (no pressure / permanent pressure), fresh-oil level, pump motor protection; lubrication pulse suppression is cancelled by M49, M18, M30, M99 or cycle stop (11250-11269, 11030/11031).
- Coolant: motor protection, flow monitor, filter differential pressure (10800-10803). Chip conveyor motor protection (11350).
- Guard and safety: automatic guard cover (`Schutzhaube`) with motor, open/close timeout (parameter 031), safety edge, safety circuit with signal-sequence check, emergency unlock detection, no opening while a push-button is pressed (11300-11306). Emergency stop and "Antriebe Ein" (drives on) push-button with lamp (11552/11553). Control voltage monitoring of input modules, output modules and valves (11550/11551/11555).
- Part transfer / interface between the sides: a hydraulic work-area partition wall (`Arbeitsraumtrennwand`) per side, opened/closed by M-command, two end-position limit switches, execution-time monitor, only with guard covers closed (6950-6953, 11950-11953). These pages do not give the M-code numbers or the transfer sequence (see parts a-c).
- Loading/unloading: loading flap (`Ladeklappe`, not with guard open or gripper in work area), pick-up device (`Abholeinrichtung`) with gripper (grip/release), flap (open/close), swivel arm (forward/back), conveyor belt with channel sensor, all by M-command or push-button with proximity switches and timeouts; handling unit/gantry (`Portal`) interface with inputs X and Y as events 9/10, automatic-mode check; loading aid (`Einlegehilfe`); swivel chuck with 20 s swivel timeout and max speed parameter; bar operation with M94 and "bar end reached" event 1 (10500-10504, 10850-10852, 11400-11402, 11600-11711, p. 9-185).
- PLC I/O: Eltromatic I/O modules on a bus with start-up phases 0, 1, 3, 4, service channel, cyclic communication, module ground check (10400-10410). Circuit diagram references I 04.1 and I 04.2 for option fault/warning inputs (11554, 11556).
- Event flags readable by the part program per slide (p. 9-185): 1 bar end reached, 4 workpiece seating check (spindle H on slide 1, spindle 3 on slide 2), 9/10 handling inputs X/Y, 12 chuck of spindle H clamped.
