# Gildemeister CTX twin operating manual, 9. Parameter setting, M-commands, PLC error messages (Parametereinstellung, M-Befehle, SPS-Fehlermeldungen), pages 9-43 to 9-84

Source: `9_Parametereinstellung_M-Befehle_SPS-Fehlermeldungen_b.pdf` (part b of 4), 42 scanned pages = manual pages 9-43 to 9-84. "Betriebsanleitung CTX twin" (operating manual), Dok.-Nr.: daaab1d1, Februar 1995. Pages 9-43, 9-45, 9-53 and 9-65 carry the revision footer "REV: dab9501 (05.05.1995) zu daaab1d1 Februar 1995"; on these pages changed lines are marked with "!" in the left margin (M027, M800-M804, PLC messages 803/804).
Original language: German.

Contents of these pages:
- 9.2 "Beschreibung der M-Befehle" (description of the M-commands): starts on page 9-43 and is complete in this part (ends 9-54).
- 9.3 "Fehlermeldungen" (error messages): starts on page 9-55; the list runs here from message 300 to 1507 and **continues in part c** (after 1507 F on page 9-84).
- 9.1 (parameter setting, incl. 9.1.2 "Parameterliste" on page 9-6) is in part a; it is only referenced here.

Running page header: "9. Parametereinstellung, M-Befehle, SPS-Fehlermeldungen". In 9.3 the first message number on the page is repeated at the top right (e.g. "300 F:"). Footer: "Betriebsanleitung CTX twin / Dok.-Nr.: daaab1d1 Februar 1995 / Seite 9-xx".

---

## 9.2 Description of the M-commands (Beschreibung der M-Befehle)

(Page 9-43.)

The machine consists of aggregate group 1 (AG 1, `Aggregategruppe 1`) and aggregate group 2 (AG 2), where

- AG 1 contains all functions of the left half of the machine, like a normal 2-axis machine
- AG 2 likewise contains all functions of the right machine side, like a normal 2-axis machine.

When a part program is written for one machine side, the left side is always chosen. If the program is to run on the right side, this is told to the CNC control, which then changes all M-function numbers automatically.

In unsplit operation (`ungesplitteter Betrieb`) it is possible to program the M-function numbers for one AG through any one channel or through both channels.

For the following list:

- The left column always gives the M-function number of the left side (AG 1). If there is no further M-function number on the right (example M 000), the number is valid for both sides.
- If there is a number on the right (example M 303), it can be programmed directly for the right side, or it is converted automatically by the CNC control when the program is to run on the right side.

The working of the M-functions is described below.
(\*) ==> one star means the function is optional.
(\*\*) ==> two stars mean that the I/O modules (`E-/A-Module`) needed to execute this M-function are available as an option.

The numbering concept is such that the hundreds digit at the same time identifies the spindle and the slide.

- The main spindle is spindle 0, i.e. functions 003, 004, etc.
- The driven tool is spindle 1, i.e. functions 103, 104, etc.
- This is reserved up to spindle 5 and slide 5.
- The number range 600 - 699 is for workpiece transport of the left machine side (AG 1).
- The number range 700 - 799 is for workpiece transport of the right machine side (AG 2).
- The number range 900 - 999 is for user functions (`Anwenderfunktionen`).

### M-command list

(Pages 9-44 to 9-54. "AG 2" column empty = the same number is valid for both sides. Opt.: (\*) optional, (\*\*) I/O modules optional.)

| AG 1 (left) | AG 2 (right) | Opt. | German name | English | Description (translated) | Page |
|---|---|---|---|---|---|---|
| M 000 | | | `Programm Halt` | Program stop | In automatic mode the run is stopped after a block containing M 000, e.g. for manual intervention. Pressing the key `ZYKLUS START` (cycle start) continues the program at that point. | 9-44 |
| M 001 | | | `Programm wahlweise Halt` | Optional program stop | Identical to M000; the stop only takes effect if the programmed condition for it is fulfilled. | 9-44 |
| M 003 | M 303 | | `Hauptspindel Rechtslauf` | Main spindle clockwise | Starts the main spindle clockwise at the previously programmed speed. | 9-44 |
| M 004 | M 304 | | `Hauptspindel Linkslauf` | Main spindle counter-clockwise | Starts the main spindle counter-clockwise at the previously programmed speed. | 9-44 |
| M 005 | M 305 | | `Hauptspindel Stop` | Main spindle stop | Stops the main spindle. | 9-44 |
| M 009 | M 309 | | `Kühlmittel aus` | Coolant off | Switches off all coolants of the lathe and resets preselections to low pressure. | 9-44 |
| M 012 | M 312 | (\*) | `Hauptspindel klemmen` | Main spindle clamp | Switches on the main spindle brake and so locks the chuck. | 9-44 |
| M 013 | M 313 | (\*) | `Hauptspindel lösen` | Main spindle unclamp | Releases the main spindle brake. Only then is rotation of the main spindle enabled. | 9-44 |
| M 014 | M 314 | (\*) | `Hauptspindel C-Achse ein` | Main spindle C axis on | Switches on positioning mode of the main spindle. After this the main spindle can only be run with the C-axis programming functions at reduced speed. | 9-45 |
| M 015 | M 315 | (\*) | `Hauptspindel C-Achse aus` | Main spindle C axis off | Switches the C axis off and re-enables speed mode of the main spindle. | 9-45 |
| M 016 | M 316 | | `Späneförderer aus` | Chip conveyor off | Switches the chip conveyor off. | 9-45 |
| M 017 | M 317 | | `Späneförderer ein` | Chip conveyor on | Switches the chip conveyor on. | 9-45 |
| M 018 | M 318 | (\*) | `Impuls Werkstückzähler` | Workpiece counter pulse | Outputs an additional pulse for an external workpiece counter. | 9-45 |
| M 019 | M 319 | (\*) | `Hauptspindel stillsetzen in definierter Lage` | Stop main spindle in defined position | Stops the main spindle under closed-loop control at the position programmed with this command, between 0 and 360° (e.g. S180). | 9-45 |
| M 027 | M 327 | | `Kontrolle der Werkstückanlage an der Spindel H` | Check of workpiece seating at spindle H | With the clamping device open, triggers the monitoring cycle. The result of the last monitoring cycle is always written into the associated event flag (`Ereignismerker`). (Marked "!" = revised 05.05.1995.) | 9-45 |
| M 028 | M 328 | (\*) | `Hauptspannfutter öffnen und Spannart auf Innenspannen umschalten` | Open main chuck and switch clamping mode to internal clamping | Only useful with automatic loading/unloading; switches the chuck over accordingly. | 9-45 |
| M 029 | M 329 | (\*) | `Hauptspannfutter öffnen und Spannart auf Außenspannen umschalten` | Open main chuck and switch clamping mode to external clamping | Only useful with automatic loading/unloading; switches the chuck over accordingly. | 9-46 |
| M 030 | | | `Programmende mit Rücksprung zum Anfang` | Program end with return to start | Ends a part program, generates a pulse for an external workpiece counter and stops all movements of the machine. | 9-46 |
| M 033 | M 333 | (\*) | `Hauptspannfutter öffnen bei drehender Spindel` | Open main chuck with spindle rotating | Special function: opens the chuck while the main spindle is running (needed for drawing in polygonal/multi-edge bars). Likewise, switching on the main spindle (M003, M004) with the chuck open is enabled by this function. Spindle running with open chuck at a speed n > 5 min⁻¹ is aborted with an error message after 180 seconds. | 9-46 |
| M 034 | M 334 | (\*) | `Hauptspannfutter: Zweiter Spanndruck aus` | Main chuck: second clamping pressure off | Switches off the second clamping pressure of the main chuck. Can only be executed with the chuck open. | 9-46 |
| M 035 | M 335 | (\*) | `Hauptspannfutter: Zweiter Spanndruck ein` | Main chuck: second clamping pressure on | Only useful with the chuck open, since a pressure reduction in the clamped state is prevented hydraulically. | 9-46 |
| M 036 | M 336 | | `Hauptspannfutter öffnen` | Open main chuck | Opens the main chuck up to the adjustable limit switch `Spannfutter geöffnet` (chuck open). | 9-46 |
| M 037 | M 337 | | `Hauptspannfutter spannen` | Clamp main chuck | Closes the main chuck. The set clamping pressure must be reached and the stroke limit when clamping (limit switch) must not be reached. | 9-46 |
| M 046 | | (\*) | `Arbeitsraumtrennwand öffnen` | Open work-area partition wall | Opens the partition plate between the left and right machine side. This is needed when the right chuck is to collect a workpiece from the left chuck. `Achtung:` (Caution:) When opening the partition wall the Z slide of AG2 must be in the work area of AG2. | 9-47 |
| M 047 | | (\*) | `Arbeitsraumtrennwand schließen` | Close work-area partition wall | Closes the partition plate between left and right machine side. `Achtung:` (Caution:) When closing the partition wall the Z slide of AG2 must be in the work area of AG2. | 9-47 |
| M 048 | M 348 | | `Schmierimpulsunterdrückung aktivieren` | Activate lubrication pulse suppression | Suppresses the lubrication pulse for a maximum period settable by parameter. | 9-47 |
| M 049 | M 349 | | `Schmierimpuls auslösen bzw. Schmierimpulsunterdrückung aufheben` | Trigger lubrication pulse / cancel lubrication pulse suppression | Triggers an additional lubrication pulse of the central lubrication for all lubrication points. | 9-47 |
| M 050 | M 350 | (\*) | `Kühlmittel 3: ( Backenspülen ) ein` | Coolant 3 (jaw flushing) on | Switches on the coolant for the main chuck with the function `Backenspülen` (jaw flushing). | 9-47 |
| M 051 | M 351 | (\*) | `Kühlmittel 3: ( Backenspülen ) aus` | Coolant 3 (jaw flushing) off | Switches the coolant for jaw flushing off again. | 9-47 |
| M 065 | | | `Stangenbearbeitung: Abfrage auf Stangenende` | Bar work: query for bar end | Triggers a query at the bar feeder or bar loading magazine whether the bar end has been reached. Depending on the result, specific actions can then be started (see description of the GILDEMEISTER bar interface, `Stangenschnittstelle`). | 9-48 |
| M 080 | | (\*) | `Lünette 1 öffnen` | Steady rest 1 open | Opens steady rest 1 to its end position and keeps it permanently open. | 9-48 |
| M 081 | | (\*) | `Lünette 1 schließen` | Steady rest 1 close | Closes steady rest 1. The set pressure of steady rest 1 must be reached. | 9-48 |
| M 082 | | (\*) | `Lünette 2 öffnen` | Steady rest 2 open | Opens steady rest 2 to its end position and keeps it permanently open. | 9-48 |
| M 083 | | (\*) | `Lünette 2 schließen` | Steady rest 2 close | Closes steady rest 2. The set pressure of steady rest 2 must be reached. | 9-48 |
| M 091 | | | `Programm Halt ohne Spindelstop` | Program stop without spindle stop | Stops the part program, but the main spindle keeps running at the current speed. | 9-48 |
| M 097 | | | `Schlittensynchronisation` | Slide synchronisation | On multi-slide machines this function sets synchronisation points in the different part-program channels. | 9-48 |
| M 099 | | | `Programmende und automatischer Neustart` | Program end and automatic restart | Ends a part program, generates a pulse for an external workpiece counter and stops all movements of the machine. Then a new part-program run is started immediately. | 9-48 |
| M 103 | M 203 | (\*) | `Angetriebenes Werkzeug Rechtslauf` | Driven tool clockwise | Starts the (tool) spindle clockwise at the previously programmed speed. | 9-49 |
| M 104 | M 204 | (\*) | `Angetriebenes Werkzeug Linkslauf` | Driven tool counter-clockwise | Starts the (tool) spindle counter-clockwise at the previously programmed speed. | 9-49 |
| M 105 | M 205 | (\*) | `Angetriebenes Werkzeug Spindel Stop` | Driven tool spindle stop | Stops the (tool) spindle. | 9-49 |
| M 107 | M 207 | (\*) | `Kühlmittel: Kreis 2 ein` | Coolant circuit 2 on | Switches on a second coolant for the tool holder (`Werkzeugträger`). | 9-49 |
| M 108 | M 208 | | `Kühlmittel: Kreis 1 ein` | Coolant circuit 1 on | Switches on the coolant for the tool holder. | 9-49 |
| M 109 | M 209 | | `Kühlmittel: Kreis 1 und 2 aus` | Coolant circuits 1 and 2 off | Switches off all coolants for the tool holder. | 9-49 |
| M 150 | M 250 | (\*) | `Kühlmittel: Hoher Druck` | Coolant high pressure | Switches the tool-holder coolant to high pressure. | 9-49 |
| M 151 | M 251 | (\*) | `Kühlmittel: Niedriger Druck` | Coolant low pressure | Switches the tool-holder coolant back to low pressure. | 9-49 |
| M 192 | M 292 | (\*) | `Werkzeugträger entriegeln` | Unlock tool holder | Unlocks the tool holder. Useful to save program time: typically programmed when the slide moves to the tool-change point, so the unlocking happens in parallel with the slide movement, saving program run time. | 9-50 |
| M 380 | | (\*) | `Lünette 3 öffnen` | Steady rest 3 open | Opens steady rest 3 to its end position and keeps it permanently open. | 9-50 |
| M 381 | | (\*) | `Lünette 3 schließen` | Steady rest 3 close | Closes steady rest 3. The set pressure of steady rest 3 must be reached. | 9-50 |
| M 382 | | (\*) | `Lünette 4 öffnen` | Steady rest 4 open | Opens steady rest 4 to its end position and keeps it permanently open. | 9-50 |
| M 383 | | (\*) | `Lünette 4 schließen` | Steady rest 4 close | Closes steady rest 4. The set pressure of steady rest 4 must be reached. | 9-50 |
| M 600 | M 700 | (\*) | `Portalbetrieb: Start Funktion 1` | Gantry operation: start function 1 | First programmable action for an attached gantry loader (`Portal`) (see description of the gantry interface, `Portalschnittstelle`). | 9-50 |
| M 601 | M 701 | (\*) | `Portalbetrieb: Start Funktion 2` | Gantry operation: start function 2 | Second programmable action for an attached gantry (see gantry interface description). | 9-50 |
| M 602 | M 702 | (\*) | `Portalbetrieb: Start Funktion 3` | Gantry operation: start function 3 | Third programmable action for an attached gantry (see gantry interface description). | 9-50 |
| M 603 | M 703 | (\*) | `Portalbetrieb: Start Funktion 4` | Gantry operation: start function 4 | Fourth programmable action for an attached gantry (see gantry interface description). | 9-51 |
| M 661 | M 761 | (\*) | `Ladeklappe öffnen (Arbeitsstellung)` | Open loading flap (working position) | Opens the loading flap of the lathe so the gantry can move in. | 9-51 |
| M 662 | M 762 | (\*) | `Ladeklappe schließen (Grundstellung)` | Close loading flap (home position) | Closes the loading flap so that coolant and chips are not thrown out during the following turning process. | 9-51 |
| M 680 | M 780 | (\*) | `Abholeinrichtung: Schwenken Grundstellung` | Pick-off device: swivel to home position | Swivels the pick-off (part unloading) device from turning centre back to home position. | 9-51 |
| M 681 | M 781 | (\*) | `Abholeinrichtung: Schwenken Drehmitte` | Pick-off device: swivel to turning centre | Swivels the pick-off device from home position to turning centre. | 9-51 |
| M 682 | M 782 | (\*) | `Abholeinrichtung: Greifer öffnen` | Pick-off device: open gripper | Opens the gripper of the pick-off device. | 9-51 |
| M 683 | M 783 | (\*) | `Abholeinrichtung: Greifer schließen` | Pick-off device: close gripper | Closes the gripper of the pick-off device. | 9-51 |
| M 684 | M 784 | (\*) | `Abholeinrichtung: Teileabfuhr Grundstellung` | Pick-off device: part discharge home position | Pick-off device AG 1 ==> close flap (currently not available, `z.Z. nicht verfügbar`): closes the unloading flap of the pick-off device. Pick-off device AG 2 ==> conveyor belt off: switches the conveyor belt off. | 9-51 |
| M 685 | M 785 | (\*) | `Abholeinrichtung: Teileabfuhr Arbeitsstellung` | Pick-off device: part discharge working position | Pick-off device AG 1 ==> open flap (currently not available): opens the unloading flap of the pick-off device. Pick-off device AG 2 ==> conveyor belt on: switches the conveyor belt on, time-controlled or permanently (parameter no. 49 - word no. 50131). | 9-52 |
| M 686 | M 786 | (\*) | `Abholeinrichtung: Zyklus "Ablegen"` | Pick-off device: cycle "deposit" | Executes the complete pick-off cycle of the pick-off device. | 9-52 |
| M 687 | M 787 | (\*) | `Abholeinrichtung: Zyklus "Greifen"` | Pick-off device: cycle "grip" | Executes the complete move-in cycle of the pick-off device. | 9-52 |
| M 688 | M 788 | (\*) | `Abholeinrichtung: Zyklus "Ablegen" ohne Entzug der Datenfreigabe im Teileprogramm` | Pick-off device: cycle "deposit" without withdrawing the data enable in the part program | Identical to M 686, except that after this function starts the part program is immediately released to continue. It must absolutely be ensured that the parallel functions (pick-off cycle and the next activities in the part program) cannot cause a collision. | 9-52 |
| M 689 | M 789 | (\*) | `Abholeinrichtung: Zyklus "Greifen" ohne Entzug der Datenfreigabe im Teileprogramm` | Pick-off device: cycle "grip" without withdrawing the data enable | Identical to M 687, except that after this function starts the part program is immediately released to continue. It must absolutely be ensured that the parallel functions (grip cycle and the next activities in the part program) cannot cause a collision. | 9-52 |
| M 800 | | | `Spindelsynchronisation ausschalten` | Spindle synchronisation off | Cancels the spindle synchronisations (speed-synchronous or angle-synchronous) switched on with M 801, M 803 and M 804. (Revised "!".) | 9-53 |
| M 801 | | | `Spindelsynchronisation einschalten` | Spindle synchronisation on | With M 801 the main spindle AG1 leads counter-spindle I (`Gegenspindel I`) speed-synchronously. If counter-spindle I is to be led angle-synchronously, program M 801 S... . A non-zero value under S gives an angular offset in degrees relative to the leading spindle (main spindle AG1). (Revised "!".) | 9-53 |
| M 803 | | | `Spindelsynchronisation einschalten` | Spindle synchronisation on | With M 803 the main spindle of AG1 leads the main spindle of AG2 speed-synchronously. For angle-synchronous leading program M 803 S... . A non-zero S value gives an angular offset in degrees relative to the leading spindle (main spindle of AG1). (Revised "!".) | 9-53 |
| M 804 | | | `Spindelsynchronisation einschalten` | Spindle synchronisation on | With M 804 the main spindle of AG2 leads the main spindle of AG1 speed-synchronously. For angle-synchronous leading program M 804 S... . A non-zero S value gives an angular offset in degrees relative to the leading spindle (main spindle of AG2). (Revised "!".) | 9-53 |
| M 850 to M 887 | | (\*\*) | `Komfort-Anwenderrelais 01 - 16` | Comfort user relays 01 - 16 | (No further description.) | 9-53 |
| M 900 to M 931 | | (\*) | `Anwender - Relais 01 bis 16` | User relays 01 to 16 | See the separate description of the option "Beschreibung der Anwenderfunktionen" (description of the user functions). | 9-54 |

Notes on the list:
- M 802 is not listed (numbers jump 801 -> 803).
- On page 9-49 a large hand-drawn pencil loop encircles the entries M 150/M 151 (owner's mark, no text).
- M 033 says spindle running with open chuck is aborted after 180 s; PLC message 556 (page 9-61) says 90 s. [uncertain which applies; both printed as such.]
- Page 9-54: rest of page empty after M 900 - M 931.

---

## 9.3 Error messages (Fehlermeldungen)

(Page 9-55. This section continues in part c after message 1507.)

Error messages are divided into the following number ranges:

| Range | Meaning |
|---|---|
| 0 - 4 999 | messages are independent of the aggregate group |
| 5 000 - 9 999 | messages refer to aggregate group 1 (AG 1, left) |
| 10 000 - 14 999 | messages refer to aggregate group 2 (AG 2, right) |

and the following error types (`Fehlertyp`):

| Type | German | English |
|---|---|---|
| M | `Meldung, Hinweis an den Bediener` | message, note to the operator |
| W | `Warnung, Maschine stoppt bei M0/M5/M18/M30/M99` | warning, machine stops at M0/M5/M18/M30/M99 |
| F | `Fehler, Maschine stoppt sofort` | fault, machine stops immediately |

The red fault lamp (`Störungslampe`) on the control panel signals with
- steady light, that at least one fault (F) is present
- flashing light, that at least one warning (W) is present.

If the warning is cleared before the aggregate group processes a spindle-stop function or a program-stop function in the part program, a standstill of the aggregate group is avoided.

Error recovery is always acknowledged
- by selecting the error window (key symbol: lightning-bolt key, figure)
- by positioning the cursor on the corresponding error (arrow keys) and pressing the delete key (`Del` key, figure).

(Margin symbol: warning sign "!" with hand.) Every error must be acknowledged individually. This ensures that no error message is acknowledged unknowingly.

The aggregate group named in the error text must not be in automatic mode.

In some cases (e.g. tool holder) the disturbed function must be run before acknowledgement (e.g. swivel the tool holder with a T command).

### PLC message list

Format in the manual: number, type, message text, then `Ursache:` (cause: category, then detail) and `Behebung:` (remedy: who, then action). Cause categories used: `Bedienung in dem Maschinenstatus unzulässig` (operation not permitted in this machine state), `möglicher Defekt` (possible defect), `falsche Einstellung, Parameter` (wrong setting, parameter), `Wartung ist notwendig` (maintenance needed). Remedy by: `Bediener` (operator), `Technologe, Einrichter` (technologist, setter), `Wartungspersonal Elektrik` / `Mechanik` (electrical / mechanical maintenance staff). Numbers below are the AG-independent base numbers as printed; per 9.3 the AG-related messages appear in the 5000/10000 ranges [uncertain how the base numbers map].

| No. | Type | Message (German) | Message (English) | Cause | Remedy | Page |
|---|---|---|---|---|---|---|
| 300 | F | `Eingabe: M-Funktion undefiniert` | Input: M-function undefined | Operation not permitted in this machine state; M-function undefined for this machine equipment | Operator: acknowledge error | 9-56 |
| 301 | F | `Eingabe: M-Funktion wegen Maschinenzustand unzulässig` | Input: M-function not permitted due to machine state | Operation not permitted; M-function not permitted because of machine state | Operator: acknowledge error | 9-56 |
| 350 | F | `Einrichtebetrieb: Anwahl unzulässig` | Setup mode: selection not permitted | Operation not permitted; guard (`Schutzhaube`) is closed or operating mode is not manual control (`Handsteuern`) | Operator: open guard and select operating mode manual control | 9-56 |
| 351 | M | `Einrichtebetrieb: Unzulässig` | Setup mode: not permitted | Operation not permitted; guard closed or not in mode manual control | Operator: open guard and select mode manual control | 9-56 |
| 400 | F | `Eltromatic: Fehler Hochlauf Phase 3` | Eltromatic: start-up error phase 3 | Possible defect; error during communication test | Electrical maintenance: check I/O modules and connections to the control | 9-56 |
| 401 | F | `Eltromatic: Fehler Hochlauf Phase 4` | Eltromatic: start-up error phase 4 | Possible defect; general error when starting cyclic communication | Electrical maintenance: check I/O modules and connections to the control | 9-56 |
| 402 | F | `Eltromatic: Fehler Hochlauf Phase 0` | Eltromatic: start-up error phase 0 | Possible defect, maintenance needed; error during self-test of the I/O modules | Electrical maintenance: check modules and connections to the control | 9-57 |
| 403 | F | `Eltromatic: Fehler Hochlauf Phase 1` | Eltromatic: start-up error phase 1 | Possible defect; general error in identification phase | Electrical maintenance: check I/O modules and connections | 9-57 |
| 404 | F | `Eltromatic: Fehler Hochlauf Servicekanalfehler` | Eltromatic: start-up service-channel error | Possible defect; general error while parameterising the I/O modules | Electrical maintenance: check I/O modules and connections | 9-57 |
| 405 | F | `Eltromatic: Fehler im zyklischen Betrieb` | Eltromatic: error in cyclic operation | Possible defect; general error during cyclic communication | Electrical maintenance: check I/O modules and connections | 9-57 |
| 406 | F | `Eltromatic: Kein EA-Modul identifiziert` | Eltromatic: no I/O module identified | Possible defect; no I/O module could be identified | Electrical maintenance: check I/O modules and connections | 9-57 |
| 407 | F | `Eltromatic: Nicht unterstütztes Modul gefunden` | Eltromatic: unsupported module found | Possible defect; an unknown I/O module was used | Electrical maintenance: check I/O modules and connections | 9-57 |
| 408 | F | `Eltromatic: Ausrüstung und Parameter stimmen nicht überein` | Eltromatic: equipment and parameters do not match | Possible defect or wrong setting/parameter; Eltromatic equipment found at machine start-up does not match the associated parameters | Electrical maintenance: check I/O modules and parameters and match them | 9-58 |
| 409 | F | `Eltromatic: Fehlerhafte Masseverbindung am Modul <Modulnr.>` | Eltromatic: faulty ground connection at module <module no.> | Possible defect; faulty ground connection at the named Eltromatic module | Electrical maintenance: check ground connections at the named module | 9-58 |
| 410 | F | `Eltromatic: Allgemeiner Fehler am Modul <Modulnr.>` | Eltromatic: general error at module <module no.> | Possible defect; general error at the named Eltromatic module | Electrical maintenance: check the named module and its connections | 9-58 |
| 450 | F | `Getriebe Hauptantrieb: Kommando unzulässig` | Main drive gearbox: command not permitted | Operation not permitted; the command is not permitted in this machine state | Operator or electrical maintenance: change machine state and repeat command | 9-58 |
| 451 | F | `Getriebe Hauptantrieb: Kuppelvorgang fehlerhaft` | Main drive gearbox: coupling (shifting) faulty | Possible defect; the desired position was not reached | Operator or electrical maintenance: repeat command, check gearbox control | 9-58 |
| 452 | M | `Getriebe Hauptantrieb: Stellung undefiniert` | Main drive gearbox: position undefined | Operation not permitted; gearbox is in none of the permitted positions | Operator: trigger command for a permitted position | 9-58 |
| 453 | F | `Getriebe Hauptantrieb: Überwachung angesprochen` | Main drive gearbox: monitoring tripped | Possible defect; the desired position was left without reason | Operator or electrical maintenance: repeat command, check gearbox control | 9-59 |
| 500 | F | `Handhabung: Kommando unzulässig` | Handling: command not permitted | Operation not permitted or wrong setting/parameter; not permitted because the state of the handling device is wrong | Operator or setter: acknowledge, switch handling to the correct state | 9-59 |
| 501 | M | `Handhabung: Maschine von Handhabung ausgeschaltet` | Handling: machine switched off by handling | Machine was switched off by the gantry (no raw parts) | Setter: refill raw parts, switch gantry and machine on again | 9-59 |
| 502 | F | `Handhabung: Nicht betriebsbereit` | Handling: not ready | Operation not permitted; handling not switched on or ready | Operator or setter: check handling | 9-59 |
| 503 | W | `Handhabung: nicht betriebsbereit` | Handling: not ready | As 502 | As 502 | 9-59 |
| 504 | M | `Handhabung: Handhabung ist nicht im Automatikbetrieb` | Handling: handling not in automatic mode | Operation not permitted; while the handling command is processed, the handling is not in automatic mode | Operator: set handling to automatic and if needed press cycle start of the lathe | 9-59 |
| 550 | M | `Hauptspanner: Ausklinkhub angewählt` | Main clamp: release stroke selected | Operation not permitted; release stroke (`Ausklinkhub`) selected in parameter "Bedienung" (operation) | Operator: correct parameter, see 9.1.2 "Parameterliste" page 9-6 | 9-60 |
| 551 | F | `Hauptspanner: Druckeinbruch` | Main clamp: pressure drop | Wrong setting/parameter; pressure switch "Spanndruck" (clamping pressure) reported pressure drop | Setter: check the relevant pressure switch, depending on clamping direction | 9-60 |
| 552 | F | `Hauptspanner: Fehlerhafte Eingabesignale` | Main clamp: faulty input signals | Wrong setting/parameter; wrong signals occurred during clamping or unclamping | Setter: check setting (usable stroke, clamping pressure) | 9-60 |
| 553 | F | `Hauptspanner: Hubbegrenzung hinten fehlerhaft` | Main clamp: rear stroke limit faulty | Possible defect; proximity switch test "Näherungsschalter Hinten" (rear proximity switch) faulty | Electrical maintenance: replace proximity switch or check signal path per circuit diagram | 9-60 |
| 554 | M | `Hauptspanner: Hubbegrenzung Spannen erreicht` | Main clamp: clamping stroke limit reached | Wrong setting/parameter; main clamp reached the end position without pressure | Setter: check setting (usable stroke, clamping pressure) | 9-60 |
| 555 | F | `Hauptspanner: Hubbegrenzung vorn fehlerhaft` | Main clamp: front stroke limit faulty | Possible defect; proximity switch test "Näherungsschalter Vorn" (front proximity switch) faulty | Electrical maintenance: replace proximity switch or check signal path per circuit diagram | 9-60 |
| 556 | F | `Hauptspanner: Spindellaufzeit ohne Spanndruck überschritten` | Main clamp: spindle run time without clamping pressure exceeded | Operation not permitted; spindle ran with chuck open longer than 90 seconds | Operator: acknowledge, correct part program if needed | 9-61 |
| 557 | M | `Hauptspanner: Stellung undefiniert` | Main clamp: position undefined | Operation not permitted; main clamp neither clamped nor fully open | Operator: command "Spannen" (clamp) or "Lösen" (release) (M-command or foot switch) | 9-61 |
| 558 | F | `Hauptspanner: Überwachung angesprochen` | Main clamp: monitoring tripped | Wrong setting/parameter; end position "Gespannt" (clamped) or "Geöffnet" (open) left without reason | Setter: check setting (usable stroke, clamping pressure) | 9-61 |
| 559 | F | `Hauptspanner: Maximale Ausführungszeit überschritten` | Main clamp: maximum execution time exceeded | Possible defect; the module "Spanner ohne Hubbegrenzung" (clamp without stroke limit) could not finish clamping or releasing within 20 seconds | Setter: check main clamp settings (pressure switch, pressure reducer), valves | 9-61 |
| 600 | M | `Hauptspannfutter: In Wechselposition` | Main chuck: in change position | Operation not permitted; chuck key is not inserted at the control panel | Operator: insert chuck key into the enable switch on the control panel | 9-61 |
| 601 | F | `Hauptspannfutter: Max. Drehzahl überschritten` | Main chuck: max. speed exceeded | Wrong setting/parameter; speed was higher than the set chuck speed | Setter: check set speed, correct part program if needed | 9-61 |
| 602 | F | `Hauptspannfutter: Kommando unzulässig` | Main chuck: command not permitted | Operation not permitted; chuck command not permitted with spindle rotating | Operator: stop spindle first | 9-62 |
| 650 | F | `Hauptspindelantrieb: Motorübertemperatur` | Main spindle drive: motor overtemperature | Operation not permitted; motor overloaded | Electrical maintenance: check drive controller (`Steller`) and let motor cool down | 9-62 |
| 651 | W | `Hauptspindelantrieb: Motorübertemperatur` | Main spindle drive: motor overtemperature | As 650 | As 650 | 9-62 |
| 652 | F | `Hauptspindelantrieb: Nicht betriebsbereit` | Main spindle drive: not ready | Possible defect; main spindle drive controller faulted | Electrical maintenance: clear fault, see electrical service manual | 9-62 |
| 653 | F | `Hauptspindelantrieb: Störung Fremdlüfter` | Main spindle drive: separate fan fault | Possible defect; motor protection switch tripped | Electrical maintenance: check current setting | 9-62 |
| 654 | W | `Hauptspindelantrieb: Störung Fremdlüfter` | Main spindle drive: separate fan fault | As 653 | As 653 | 9-62 |
| 655 | F | `Hauptspindelantrieb: Temperaturvorwarnung` | Main spindle drive: temperature pre-warning | Wrong setting/parameter, possible defect or overload in the cycle; main spindle drive reports overtemperature | Electrical maintenance: let motor cool, check motor, check controller | 9-63 |
| 656 | W | `Hauptspindelantrieb: Temperaturvorwarnung` | Main spindle drive: temperature pre-warning | As 655 | As 655 | 9-63 |
| 700 | F | `Hauptspindelklemmung: Ausführung fehlerhaft` | Main spindle clamping: execution faulty | Wrong setting/parameter; proximity switches of the spindle clamping give wrong signals | Setter: check proximity switches | 9-63 |
| 701 | F | `Hauptspindelklemmung: Kommando unzulässig` | Main spindle clamping: command not permitted | Operation not permitted; main spindle not in the position window | Operator: first position the spindle with the C axis | 9-63 |
| 702 | F | `Hauptspindelklemmung: Überwachung angesprochen` | Main spindle clamping: monitoring tripped | Wrong setting/parameter; selected state left without reason | Setter: check proximity switches | 9-63 |
| 703 | M | `Hauptspindelklemmung: Hauptspindel ist geklemmt` | Main spindle clamping: main spindle is clamped | ==> not used (`nicht verwendet`) | | 9-63 |
| 750 | F | `Hydraulik: Filter verstopft` | Hydraulics: filter clogged | Maintenance needed; differential pressure before/after hydraulic filter too high | Mechanical maintenance: change hydraulic filter | 9-63 |
| 751 | W | `Hydraulik: Filter verstopft` | Hydraulics: filter clogged | As 750 | As 750 | 9-64 |
| 752 | F | `Hydraulik: Kein Systemdruck` | Hydraulics: no system pressure | Maintenance needed; pressure loss or pressure drop in hydraulics | Mechanical maintenance: check oil level and tightness of hydraulics | 9-64 |
| 753 | W | `Hydraulik: Minimum Niveau erreicht` | Hydraulics: minimum level reached | Maintenance needed; oil level in hydraulic tank too low | Mechanical maintenance: check hydraulics for leaks, top up oil | 9-64 |
| 754 | F | `Hydraulik: Minimum Niveau erreicht` | Hydraulics: minimum level reached | As 753 | As 753 | 9-64 |
| 755 | F | `Hydraulik: Motorüberstrom` | Hydraulics: motor overcurrent | Possible defect; motor protection switch tripped | Electrical maintenance: check current setting | 9-64 |
| 756 | W | `Hydraulik: Übertemperatur Öl` | Hydraulics: oil overtemperature | Possible defect; temperature of hydraulic unit too high | Mechanical maintenance: check hydraulic unit (oil level, other) | 9-64 |
| 757 | F | `Hydraulik: Übertemperatur Öl` | Hydraulics: oil overtemperature | As 756 | As 756 | 9-65 |
| 800 | F | `Kühlmittel: Motorüberstrom` | Coolant: motor overcurrent | Possible defect; motor protection switch(es) tripped | Electrical maintenance: check current setting | 9-65 |
| 801 | F | `Kühlmittel: Strömungsüberwachung angesprochen` | Coolant: flow monitoring tripped | Possible defect or maintenance needed; coolant supply disturbed | Electrical or mechanical maintenance: check coolant supply | 9-65 |
| 802 | W | `Kühlmittel: Strömungsüberwachung angesprochen` | Coolant: flow monitoring tripped | As 801 | As 801 | 9-65 |
| 803 | F | `Kühlmittel: Kühlmittel verstopft` | Coolant: coolant clogged | Maintenance needed; differential pressure before/after coolant (filter) too high. (Revised "!".) | Electrical or mechanical maintenance: change coolant filter | 9-65 |
| 804 | W | `Kühlmittel: Kühlmittel verstopft` | Coolant: coolant clogged | As 803 (revised "!") | As 803 | 9-65 |
| 850 | F | `Ladeklappe: Kommando unzulässig` | Loading flap: command not permitted | Operation not permitted; guard is open or gripper is in the work area | Operator: close guard and move gripper out of the work area | 9-66 |
| 851 | M | `Ladeklappe: Stellung undefiniert` | Loading flap: position undefined | Operation not permitted; loading flap neither open nor closed | Operator: trigger command "Öffnen" (open) or "Schließen" (close) | 9-66 |
| 852 | F | `Ladeklappe: Überwachung angesprochen` | Loading flap: monitoring tripped | Wrong setting/parameter; selected state left without reason | Setter: check loading flap limit switches | 9-66 |
| 900 | F | `Lünetten: Kommando unzulässig` | Steady rests: command not permitted | Operation not permitted; spindle switched on, quill position not permitted | Operator: stop spindle or clamp quill | 9-66 |
| 901 | M | `Lünette 1: Stellung undefiniert` | Steady rest 1: position undefined | Operation not permitted; steady rest 1 neither clamped nor fully open | Operator: command "Spannen" or "Lösen" (M-command or push-button) | 9-66 |
| 902 | F | `Lünette 1: Überwachung angesprochen` | Steady rest 1: monitoring tripped | Wrong setting/parameter; clamped or open state was left | Setter: check pressure switch "Geöffnet" (open) or "Gespannt" (clamped) | 9-66 |
| 903 | M | `Lünette 2: Stellung undefiniert` | Steady rest 2: position undefined | Operation not permitted; steady rest 2 neither clamped nor fully open | Operator: command "Spannen" or "Lösen" (M-command or push-button) | 9-67 |
| 904 | F | `Lünette 2: Überwachung angesprochen` | Steady rest 2: monitoring tripped | Wrong setting/parameter; clamped or open state was left | Setter: check pressure switch "Geöffnet" or "Gespannt" | 9-67 |
| 950 | F | `NC: Nicht betriebsbereit` | NC: not ready | Possible defect; following-error overflow (`Schleppfehlerüberlauf`) cross slide / C axis | Electrical maintenance: check feed drives, cross-slide encoders | 9-67 |
| 951 | F | `NC: maximale Reaktionszeit der NC überschritten` | NC: maximum NC reaction time exceeded | Possible defect; reaction time of the NC to PLC signals is greater than 300 ms | Operator, if needed Gildemeister service: acknowledge, possibly switch machine off and on. Please inform Gildemeister service. | 9-67 |
| 952 | F | `NC: Übertemperatur` | NC: overtemperature | Possible defect; temperature in the control has exceeded the permitted maximum | Electrical maintenance: let control cool down and check ventilation | 9-67 |
| 1000 | F | `Parameter: Übernahme nur bei Maschinenhochlauf` | Parameter: accepted only at machine start-up | Wrong setting/parameter; if parameters changed, switch machine off | Setter: change parameter, switch machine off and on again | 9-67 |
| 1001 | F | `Parameter Abholeinrichtungstyp: Wert unzulässig` | Parameter pick-off device type: value not permitted | Parameter "Handhabung und Materialzuführung" (handling and material feed) wrong | Setter: correct parameter, see 9.1.2 "Parameterliste" page 9-6 | 9-68 |
| 1002 | F | `Parameter Getriebetyp Hauptantrieb: Wert unzulässig` | Parameter main drive gearbox type: value not permitted | Value in parameter "Getriebedaten Hauptantrieb" (main drive gearbox data) wrong | Setter: correct parameter, see 9.1.2 page 9-6 | 9-68 |
| 1003 | F | `Parameter Handhabungstyp: Typ unzulässig` | Parameter handling type: type not permitted | Parameter "Handhabung und Materialzuführung" wrong | Setter: correct parameter, see 9.1.2 page 9-6 | 9-68 |
| 1004 | F | `Parameter Hauptspanner: Arbeitsrichtung unzulässig` | Parameter main clamp: working direction not permitted | Wrong value in parameter "Hauptspindel" (main spindle) | Setter: correct parameter, see 9.1.2 page 9-6 | 9-68 |
| 1005 | F | `Parameter Hauptspannfutter: Spannart unzulässig` | Parameter main chuck: clamping mode not permitted | Wrong value in parameter "Hauptspindel" | Setter: correct parameter, see 9.1.2 page 9-6 | 9-68 |
| 1006 | F | `Parameter Hauptspannfutter: Typ unzulässig` | Parameter main chuck: type not permitted | Wrong main chuck type in parameter "Hauptspindel" | Setter: correct parameter, see 9.1.2 page 9-6 | 9-68 |
| 1007 | F | `Parameter Lünettenanzahl: Wert zu groß` | Parameter number of steady rests: value too large | Value in parameter "Lünette Beschreibung" (steady rest description) too large | Setter: correct parameter | 9-69 |
| 1008 | F | `Parameter Materialzuführungstyp: Wert unzulässig` | Parameter material feed type: value not permitted | Parameter "Handhabung und Materialzuführung" wrong | Setter: correct parameter, see 9.1.2 page 9-6 | 9-69 |
| 1009 | F | `Parameter Pinole: Typ unzulässig` | Parameter quill: type not permitted | Quill type wrongly selected in "Reitstock,Pinole" (tailstock, quill) | Setter: correct parameter | 9-69 |
| 1010 | F | `Parameter Reitstocktyp: Wert unzulässig` | Parameter tailstock type: value not permitted | Value in parameter "Reitstock,Pinole" wrongly set | Setter: correct parameter, see 9.1.2 page 9-6 | 9-69 |
| 1011 | F | `Parameter Schwenkfuttertyp: Wert unzulässig` | Parameter swivel chuck type: value not permitted | Value in parameter Schwenkfutter (swivel chuck) wrongly set | Setter: correct parameter, see 9.1.2 page 9-6 | 9-69 |
| 1012 | F | `Parameter Werkzeugträger: Typ unzulässig` | Parameter tool holder: type not permitted | Wrong tool holder type in parameter "Werkzeugträgerdaten" (tool holder data) | Setter: correct parameter, see 9.1.2 page 9-6 | 9-69 |
| 1013 | F | `Parameter Werkzeugträger: Werkzeuganzahl unzulässig` | Parameter tool holder: number of tools not permitted | Parameter "Aufnahmeanzahl u.Spindel" (number of holders and spindle) wrongly set | Setter: correct parameter, see 9.1.2 page 9-6 | 9-70 |
| 1014 | F | `Parameter ISM-Temperaturregelung: Wert "Anzahl Werte zur Mittelwertbildung" unzulässig` | Parameter ISM temperature control: value "number of values for averaging" not permitted | Value in parameter "Temperaturregelung Spindeln" (spindle temperature control) wrong | Setter: correct parameter | 9-70 |
| 1015 | F | `Parameter ISM-Temperaturregelung: Wert "Hysterese Kühlwassertemperatur" unzulässig` | ... value "hysteresis cooling water temperature" not permitted | As 1014 | As 1014 | 9-70 |
| 1016 | F | `Parameter ISM-Temperaturregelung: Wert "Hysterese Lagertemperatur hinten" unzulässig` | ... value "hysteresis bearing temperature rear" not permitted | As 1014 | As 1014 | 9-70 |
| 1017 | F | `Parameter ISM-Temperaturregelung: Wert "Hysterese Lagertemperatur vorn" unzulässig` | ... value "hysteresis bearing temperature front" not permitted | As 1014 | As 1014 | 9-70 |
| 1018 | F | `Parameter ISM-Temperaturregelung: Wert "Hysterese Wicklungstemperatur" unzulässig` | ... value "hysteresis winding temperature" not permitted | As 1014 | As 1014 | 9-70 |
| 1019 | F | `Parameter ISM-Temperaturregelung: Wert "MAX Lagertemperatur hinten" unzulässig` | ... value "MAX bearing temperature rear" not permitted | As 1014 | As 1014 | 9-71 |
| 1020 | F | `Parameter ISM-Temperaturregelung: Wert "MAX Lagertemperatur vorn" unzulässig` | ... value "MAX bearing temperature front" not permitted | As 1014 | As 1014 | 9-71 |
| 1021 | F | `Parameter ISM-Temperaturregelung: Wert "MAX Umgebungstemperatur" unzulässig` | ... value "MAX ambient temperature" not permitted | As 1014 | As 1014 | 9-71 |
| 1022 | F | `Parameter ISM-Temperaturregelung: Wert "MAX Wicklungstemperatur" unzulässig` | ... value "MAX winding temperature" not permitted | As 1014 | As 1014 | 9-71 |
| 1023 | F | `Parameter ISM-Temperaturregelung: Wert "MAXMAX Lagertemperatur hinten" unzulässig` | ... value "MAXMAX bearing temperature rear" not permitted | As 1014 | As 1014 | 9-71 |
| 1024 | F | `Parameter ISM-Temperaturregelung: Wert "MAXMAX Lagertemperatur vorn" unzulässig` | ... value "MAXMAX bearing temperature front" not permitted | As 1014 | As 1014 | 9-71 |
| 1025 | F | `Parameter ISM-Temperaturregelung: Wert "MAXMAX Wicklungstemperatur" unzulässig` | ... value "MAXMAX winding temperature" not permitted | As 1014 | As 1014 | 9-72 |
| 1026 | F | `Parameter ISM-Temperaturregelung: Wert "Sollwert Kühlwassertemperatur" unzulässig` | ... value "setpoint cooling water temperature" not permitted | As 1014 | As 1014 | 9-72 |
| 1027 | F | `Parameter ISM-Temperaturregelung: Wert "Sollwert Lagertemperatur hinten" unzulässig` | ... value "setpoint bearing temperature rear" not permitted | As 1014 | As 1014 | 9-72 |
| 1028 | F | `Parameter ISM-Temperaturregelung: Wert "Sollwert Lagertemperatur vorn" unzulässig` | ... value "setpoint bearing temperature front" not permitted | As 1014 | As 1014 | 9-72 |
| 1029 | F | `Parameter ISM-Temperaturregelung: Wert "Sollwert Wicklungstemperatur" unzulässig` | ... value "setpoint winding temperature" not permitted | As 1014 | As 1014 | 9-72 |
| 1030 | F | `Parameter Schmierung: Wert "Schmierintervallzeit" unzulässig` | Parameter lubrication: value "lubrication interval time" not permitted | Value in parameter "Schmierintervallzeit" wrong | Setter: correct parameter | 9-72 |
| 1031 | F | `Parameter Schmierung: Wert "Schmierunterdrückungszeit" unzulässig` | Parameter lubrication: value "lubrication suppression time" not permitted | Value in parameter "Schmierunterdrückungszeit" wrong | Setter: correct parameter | 9-73 |
| 1032 | F | `Parameter Spanner Gegenspindel.: Arbeitsrichtung unzulässig` | Parameter counter-spindle clamp: working direction not permitted | Wrong value in parameter "Gegenspindel" (counter-spindle) | Setter: correct parameter, see parameter list | 9-73 |
| 1033 | F | `Parameter Spanner Gegenspindel: Spannart unzulässig` | Parameter counter-spindle clamp: clamping mode not permitted | Wrong value in parameter "Gegenspindel" | Setter: correct parameter, see parameter list | 9-73 |
| 1034 | F | `Parameter Spanner Gegenspindel: Typ unzulässig` | Parameter counter-spindle clamp: type not permitted | Wrong value in parameter "Gegenspindel" | Setter: correct parameter, see parameter list | 9-73 |
| 1100 | F | `Pinole: Druckeinbruch` | Quill: pressure drop | Wrong setting/parameter; quill clamped and pressure drop | Setter: check quill pressure switch setting | 9-73 |
| 1101 | M | `Pinole: Endstellung vorn erreicht` | Quill: front end position reached | Wrong setting/parameter; end position reached without pressure when clamping | Setter: quill stroke not sufficient, move tailstock | 9-73 |
| 1102 | F | `Pinole: Fehlerhafte Eingabesignale` | Quill: faulty input signals | Wrong setting/parameter; wrong signals while moving the quill | Setter: check quill setting (proximity switch, pressure switch) | 9-74 |
| 1103 | F | `Pinole: Kommando unzulässig` | Quill: command not permitted | Operation not permitted; spindle switched on, no steady rest clamped | Operator: stop spindle or clamp a steady rest | 9-74 |
| 1104 | M | `Pinole: Stellung undefiniert` | Quill: position undefined | Operation not permitted; quill neither clamped nor fully retracted | Operator: command "Vor" (forward) or "Zurück" (back) (M-command or foot switch) | 9-74 |
| 1105 | F | `Pinole: Überwachung angesprochen` | Quill: monitoring tripped | Wrong setting/parameter; clamped or retracted state left | Setter: check quill setting (proximity switch, pressure switch) | 9-74 |
| 1150 | F | `Reitstock: Kommando unzulässig` | Tailstock: command not permitted | Operation not permitted; not permitted because quill extended or main spindle rotating | Operator: acknowledge, correct part program if needed | 9-74 |
| 1151 | F | `Reitstock: Nicht betriebsbereit` | Tailstock: not ready | Wrong setting/parameter; teach-in module of the automatic tailstock | Setter: check device, reprogram if needed | 9-74 |
| 1152 | M | `Reitstock: Stellung undefiniert` | Tailstock: position undefined | Operation not permitted; the movable tailstock is in neither of its 2 end positions | Operator: move with M-command or push-button "VOR" (forward) or "ZURÜCK" (back) | 9-75 |
| 1153 | F | `Reitstock: Überwachung angesprochen` | Tailstock: monitoring tripped | Wrong setting/parameter or possible defect; limit switch of the movable tailstock misadjusted or defective | Electrical or mechanical maintenance: check limit switches, replace if needed | 9-75 |
| 1200 | F | `Schaltschrankkühlung: Filter verstopft` | Control cabinet cooling: filter clogged | Maintenance needed; differential pressure before/after filter mat too high | Mechanical maintenance: clean or change filter | 9-75 |
| 1201 | W | `Schaltschrankkühlung: Filter verstopft` | Control cabinet cooling: filter clogged | As 1200 | Electrical maintenance: clean or change filter | 9-75 |
| 1202 | F | `Schaltschrankkühlung: Überstrom` | Control cabinet cooling: overcurrent | Possible defect; motor protection switch tripped | Setter: check current setting | 9-75 |
| 1203 | W | `Schaltschrankkühlung: Überstrom` | Control cabinet cooling: overcurrent | As 1202 | Electrical maintenance: check current setting | 9-75 |
| 1204 | F | `Schaltschrankkühlung: Übertemperatur` | Control cabinet cooling: overtemperature | Possible defect; control cabinet overtemperature | Mechanical maintenance: check cooling unit, ambient temperature too high | 9-76 |
| 1205 | W | `Schaltschrankkühlung: Übertemperatur` | Control cabinet cooling: overtemperature | As 1204 | As 1204 ("ambient temperature too high?") | 9-76 |
| 1250 | F | `Schmierung: Kein Druck` | Lubrication: no pressure | Maintenance needed; lubrication pressure switch reports no pressure | Mechanical maintenance: refill lube oil tank, check pressure switch | 9-76 |
| 1251 | W | `Schmierung: Kein Druck` | Lubrication: no pressure | As 1250 | As 1250 | 9-76 |
| 1252 | W | `Schmierung: Minimum Niveau erreicht` | Lubrication: minimum level reached | Maintenance needed; fill level of fresh-oil lubrication too low | Mechanical maintenance: top up fresh oil | 9-76 |
| 1253 | F | `Schmierung: Minimum Niveau erreicht` | Lubrication: minimum level reached | As 1252 | As 1252 | 9-76 |
| 1254 | F | `Schmierung: Motorüberstrom` | Lubrication: motor overcurrent | Possible defect; motor protection switch tripped | Electrical maintenance: check current setting | 9-77 |
| 1255 | W | `Schmierung: Motorüberstrom` | Lubrication: motor overcurrent | As 1254 | As 1254 | 9-77 |
| 1256 | M | `Schmierung: Öl nachfüllen` | Lubrication: top up oil | Maintenance needed; fill level of fresh-oil lubrication not sufficient | Mechanical maintenance: top up fresh oil | 9-77 |
| 1257 | F | `Schmierung: Permanenter Druck` | Lubrication: permanent pressure | Maintenance needed; lubrication pressure switch reports permanent pressure | Mechanical maintenance: check lube pressure lines and/or pressure switch | 9-77 |
| 1258 | W | `Schmierung: Permanenter Druck` | Lubrication: permanent pressure | As 1257 | As 1257 | 9-77 |
| 1259 | F | `Schmierung: Kein Druck Kreis 1` | Lubrication: no pressure circuit 1 | Maintenance needed; lubrication pressure switch reports no pressure | Mechanical maintenance: refill lube oil tank, check pressure switch | 9-77 |
| 1260 | F | `Schmierung: Kein Druck Kreis 2` | Lubrication: no pressure circuit 2 | As 1259 | As 1259 | 9-78 |
| 1261 | F | `Schmierung: Permanenter Druck Kreis 1` | Lubrication: permanent pressure circuit 1 | Maintenance needed; pressure switch reports permanent pressure | Mechanical maintenance: check lube pressure lines and/or pressure switch | 9-78 |
| 1262 | F | `Schmierung: Permanenter Druck Kreis 2` | Lubrication: permanent pressure circuit 2 | As 1261 | As 1261 | 9-78 |
| 1263 | F | `Schmierung: Kommando unzulässig` | Lubrication: command not permitted | ==> not used | | 9-78 |
| 1264 | F | `Schmierung: Maximale Unterdrückungszeit überschritten` | Lubrication: maximum suppression time exceeded | Wrong setting/parameter or programming error; maximum suppression time exceeded | Setter: check parameter, cancel lubrication pulse suppression with M49, M18, M30, M99 or cycle stop | 9-78 |
| 1265 | W | `Schmierung: Kein Druck Kreis 1` | Lubrication: no pressure circuit 1 | As 1259 | As 1259 | 9-78 |
| 1266 | W | `Schmierung: Kein Druck Kreis 2` | Lubrication: no pressure circuit 2 | As 1259 | As 1259 | 9-78 |
| 1267 | W | `Schmierung: Permanenter Druck Kreis 1` | Lubrication: permanent pressure circuit 1 | As 1261 | As 1261 | 9-79 |
| 1268 | W | `Schmierung: Permanenter Druck Kreis 2` | Lubrication: permanent pressure circuit 2 | As 1261 | As 1261 | 9-79 |
| 1269 | W | `Schmierung: Maximale Unterdrückungszeit überschritten` | Lubrication: maximum suppression time exceeded | As 1264 | As 1264 | 9-79 |
| 1300 | F | `Schutzhaube: Max. Zeit zum auf/zufahren überschritten` | Guard: max. time for opening/closing exceeded | Possible defect or wrong setting/parameter; possible defect at the automatic guard | Electrical or mechanical maintenance: check function, if necessary correct parameter 031 | 9-79 |
| 1301 | M | `Schutzhaube: Motorüberstrom` | Guard: motor overcurrent | Wrong setting/parameter or possible defect; motor protection switch tripped | Electrical maintenance: check current setting | 9-79 |
| 1302 | M | `Schutzhaube: Nicht geöffnet und nicht geschlossen` | Guard: not open and not closed | Operation not permitted; guard not locked and not open | Operator: open guard and then close it | 9-79 |
| 1303 | M | `Schutzhaube: Öffnen nicht erlaubt, da Taster betätigt` | Guard: opening not allowed, since push-button pressed | Operation not permitted; a push-button is pressed when opening the guard | Operator: press no push-button and then open the guard | 9-80 |
| 1304 | M | `Schutzhaube: Sicherheitsleiste hat angespochen` [sic] | Guard: safety edge tripped | Operation not permitted; closing of the automatic guard interrupted | Operator: acknowledge, check safety edge if needed | 9-80 |
| 1305 | F | `Schutzhaube: Störung Sicherheitskreis` | Guard: safety circuit fault | Possible defect; guard safety circuit has wrong signal sequence | Electrical maintenance: check safety circuit (see circuit diagram) | 9-80 |
| 1306 | F | `Schutzhaube: Unzulässig geöffnet` | Guard: opened impermissibly | Operation not permitted; guard was opened with the emergency release (`Notentriegelung`) | Operator: open guard and then close it | 9-80 |
| 1350 | F | `Späneförderer: Motorüberstrom` | Chip conveyor: motor overcurrent | Operation not permitted; motor protection switch tripped | Operator or electrical maintenance: run chip conveyor empty, check current setting | 9-80 |
| 1351 | W | `Späneförderer: Motorüberstrom` | Chip conveyor: motor overcurrent | As 1350 | As 1350 | 9-80 |
| 1400 | F | `Stangenbetrieb: Kommando unzulässig` | Bar operation: command not permitted | Operation not permitted; command M94 triggered without bar operation running | Operator: select and switch on bar operation | 9-81 |
| 1401 | F | `Stangenbetrieb: Nicht betriebsbereit` | Bar operation: not ready | Operation not permitted; bar operation selected but not ready | Electrical maintenance: check bar equipment | 9-81 |
| 1402 | M | `Stangenbetrieb: Stange wechseln` | Bar operation: change bar | Operation not permitted; bar is used up | Operator: insert new bar, since there is no bar loading magazine | 9-81 |
| 1450 | F | `Vorschubantriebe: Motorüberstrom` | Feed drives: motor overcurrent | Possible defect; motor protection switch tripped | Electrical maintenance: check current setting | 9-81 |
| 1451 | F | `Vorschubantriebe: Motorübertemperatur` | Feed drives: motor overtemperature | Wrong setting/parameter or possible defect; one of the feed drives reports overtemperature | Electrical maintenance: check motor, controller, let motor cool | 9-81 |
| 1452 | F | `Vorschubantriebe: Nicht betriebsbereit` | Feed drives: not ready | Possible defect; servo controller of the feed drives faulted | Electrical maintenance: see electrical service instructions: controller, circuit diagrams | 9-81 |
| 1453 | F | `Vorschubantriebe: Schlitten-X nicht betriebsbereit` | Feed drives: slide X not ready | Possible defect; the controller for slide X is faulted | Electrical maintenance: clear fault, see electrical service manual | 9-82 |
| 1454 | F | `Vorschubantriebe: Schlitten-X Temperaturvorwarnung` | Feed drives: slide X temperature pre-warning | Wrong setting/parameter, possible defect or overload in the cycle; one of the feed drives reports overtemperature | Electrical maintenance: let drive cool, check drive, check controller | 9-82 |
| 1455 | W | `Vorschubantriebe: Schlitten-X Temperaturvorwarnung` | Feed drives: slide X temperature pre-warning | As 1454 | As 1454 | 9-82 |
| 1456 | F | `Vorschubantriebe: Schlitten-Z nicht betriebsbereit` | Feed drives: slide Z not ready | Possible defect; the controller for slide Z is faulted | Electrical maintenance: clear fault, see electrical service manual | 9-82 |
| 1457 | F | `Vorschubantriebe: Lüfter X-Achse Motorüberstrom` | Feed drives: X-axis fan motor overcurrent | Possible defect; motor protection switch tripped | Electrical maintenance: check current setting | 9-82 |
| 1458 | W | `Vorschubantriebe: Schlitten-Z Temperaturvorwarnung` | Feed drives: slide Z temperature pre-warning | As 1454 | As 1454 | 9-82 |
| 1459 | F | `Vorschubantriebe: Schlitten-Z Temperaturvorwarnung` | Feed drives: slide Z temperature pre-warning | As 1454 | As 1454 | 9-83 |
| 1460 | W | `Vorschubantriebe: Lüfter X-Achse Motorüberstrom` | Feed drives: X-axis fan motor overcurrent | As 1457 | As 1457 | 9-83 |
| 1461 | M | `Vorschubantriebe: Lüfter X-Achse Motorüberstrom` | Feed drives: X-axis fan motor overcurrent | As 1457 | As 1457 | 9-83 |
| 1463 | F | `Vorschubantriebe: Netzschütz fehlerhaft` | Feed drives: mains contactor faulty | Possible defect; the mains contactor in the drive-controller supply module does not switch properly | Electrical maintenance: check control of the supply module, replace mains contactor or module if needed | 9-83 |
| 1500 | F | `Werkzeugträger: Motorüberstrom` | Tool holder: motor overcurrent | Possible defect; motor protection switch tripped | Electrical maintenance: check current setting | 9-83 |
| 1501 | W | `Werkzeugträger: Motorüberstrom` | Tool holder: motor overcurrent | As 1500 | As 1500 | 9-83 |
| 1502 | F | `Werkzeugträger: Motorübertemperatur` | Tool holder: motor overtemperature | Operation not permitted; motor temperature monitoring tripped | Electrical maintenance: let motor cool, press reset button on the device | 9-84 |
| 1503 | W | `Werkzeugträger: Motorübertemperatur` | Tool holder: motor overtemperature | As 1502 | As 1502 | 9-84 |
| 1504 | F | `Werkzeugträger: Nicht in Position verriegelt` | Tool holder: not locked in position | Operation not permitted; tool holder in wrong position or unlocked | Operator: swivel tool holder with T command | 9-84 |
| 1505 | F | `Werkzeugträger: Parity-Check fehlerhaft` | Tool holder: parity check faulty | Possible defect; tool holder position encoder had a parity error | Operator or electrical maintenance: swivel tool holder (Tx), check position encoder, cable | 9-84 |
| 1506 | F | `Werkzeugträger: Position nicht gefunden` | Tool holder: position not found | Wrong setting/parameter or possible defect; position encoder faulty or value in parameter wrong | Setter or mechanical maintenance: check position encoder or correct parameter | 9-84 |
| 1507 | F | `Werkzeugträger: T-Wert für Werkzeugträger zu groß` | Tool holder: T value too large for tool holder | Operation not permitted or wrong setting/parameter; entered T value > value entered in parameter "Aufnahmeanzahl" (number of holders) | Operator or setter: correct parameter or enter a smaller T value | 9-84 |

(Message list continues in part c.) Number 1462 is not listed.

---

## For the retrofit

- **Two independent "aggregate groups"**: AG 1 = left machine half, AG 2 = right half, each "like a normal 2-axis machine" (X/Z slide + main spindle). Programs are written for the left side; the CNC renumbers M-codes for the right side. M-codes can be sent through either or both channels in unsplit mode (p. 9-43). A LinuxCNC per-side setup can mirror this: same program, side-specific M-code offset.
- **M-code numbering scheme** (p. 9-43): hundreds digit = spindle/slide; main spindle 0xx (AG2: 3xx), driven tool 1xx (AG2: 2xx); 600-699 workpiece transport AG1, 700-799 transport AG2; 900-999 user functions. Full mapping table above (pp. 9-44 to 9-54).
- **Main spindle**: M03/M04/M05 (AG2 M303/304/305); brake clamp/unclamp M12/M13 (M312/313); C axis on/off M14/M15 (M314/315, C axis runs at reduced speed); oriented stop M19 S0-360 (M319) (pp. 9-44, 9-45).
- **Driven tools**: M103/M104/M105 (AG2 M203/204/205) (p. 9-49). Tool holder unlock in advance M192 (M292) (p. 9-50); turret swivelled by T command, has position encoder with parity check, motor with overtemperature reset button, lock check (msgs 1500-1507, p. 9-83/84).
- **Chuck (hydraulic)**: open M36 / clamp M37 (M336/337); clamp must reach set pressure and must NOT reach the stroke-limit switch; open runs to adjustable "chuck open" switch. Inside/outside clamping switchover M28/M29; second clamping pressure M34/M35 (only with chuck open); M33 open while rotating (abort after 180 s at n > 5 min⁻¹ per p. 9-46, msg 556 says 90 s, p. 9-61). Chuck commands refused with spindle rotating (msg 602). Clamp/release max. 20 s (msg 559). Sensors: front/rear stroke-limit proximity switches, clamping-pressure switch (msgs 551-559, p. 9-60/61). Chuck key enable switch on panel for change position (msg 600). Max chuck speed parameter (msg 601). Foot switch for clamp/release (msg 557).
- **Part transfer between sides**: partition wall between work areas open M46 / close M47 — Caution: AG2 Z slide must be in AG2 work area when opening or closing (p. 9-47). Right chuck collects the part from the left chuck. Spindle synchronisation: M803 = AG1 main spindle leads AG2 main spindle (speed-sync; `M803 S..` angle-sync with offset in degrees), M804 = AG2 leads AG1, M801 = AG1 leads counter-spindle I, M800 = sync off (p. 9-53). M97 = slide synchronisation points between part-program channels (p. 9-48). M27 = workpiece seating check at spindle (event flag) (p. 9-45). Typical sequence implied: M46 open wall, M803 (S offset) sync, AG2 chuck clamps (M337), AG1 chuck opens (M36), M800, retract, M47 close wall — [sequence not stated explicitly in these pages].
- **Lubrication**: central lube with pulse; M48 suppress (max time by parameter), M49 extra pulse / cancel suppression; suppression also cancelled by M18, M30, M99 or cycle stop (msg 1264). Parameters "Schmierintervallzeit", "Schmierunterdrückungszeit". Sensors: lube pressure switch (no pressure / permanent pressure, circuits 1 and 2), fresh-oil minimum level, pump motor protection (msgs 1250-1269, pp. 9-76 to 9-79).
- **Hydraulics**: filter differential-pressure switch, system pressure, tank min level, pump motor overcurrent, oil overtemperature (msgs 750-757, p. 9-63 to 9-65).
- **Coolant**: M09 all off (resets to low pressure); tool-holder circuit 1 M08 (M208), circuit 2 M107 (M207), off M109 (M209), high/low pressure M150/M151 (M250/251), jaw flushing M50/M51 (M350/351). Flow monitor, filter differential pressure, motor overcurrent (msgs 800-804).
- **Chip conveyor** M16 off / M17 on (M316/317); motor protection (msgs 1350/1351).
- **Steady rests** 1/2: M80-M83, 3/4: M380-M383 (pressure must be reached; pressure switches "open"/"clamped"); tailstock/quill with proximity and pressure switches, forward/back via M or foot switch/buttons, interlocks with spindle running (msgs 900-904, 1100-1153).
- **Guard / doors**: automatic guard (`Schutzhaube`) with motor, safety edge, safety circuit, emergency release, max open/close time (parameter 031); setup mode needs guard open and manual mode (msgs 350/351, 1300-1306). Loading flap M661/M662 (M761/762) for gantry, refused with guard open or gripper in work area (msg 850).
- **Automation**: gantry functions M600-M603 (M700-M703); pick-off device M680-M689 (M780-M789), AG2 version has conveyor belt (parameter no. 49, word no. 50131) (pp. 9-50 to 9-52). Bar feed: M65 bar-end query, M94 (bar command) (msg 1400). Workpiece counter pulse M18, M30, M99.
- **Control/PLC**: I/O system is "Eltromatic" (start-up phases 0-4, service channel, cyclic communication, module numbers, ground faults; msgs 400-410). NC reaction to PLC signals must be < 300 ms (msg 951). Main spindle drive has separate fan and temperature pre-warning; feed drives X and Z with separate controllers, common supply module with mains contactor, X-axis fan (msgs 650-656, 1450-1463). Main drive has a shiftable gearbox (msgs 450-453). Spindle temperature control "ISM" with cooling water, bearing front/rear, winding, ambient temperatures (msgs 1014-1029). The control make/model is not named on these pages.
- **Error handling**: F = immediate stop, W = stop at M0/M5/M18/M30/M99, M = info; red lamp steady for F, flashing for W; each error acknowledged individually with the error window + Del (p. 9-55). Message ranges 0-4999 common, 5000-9999 AG1, 10000-14999 AG2.
- **User relays**: M850-M887 comfort user relays 01-16 (optional I/O), M900-M931 user relays 01-16 (pp. 9-53, 9-54).
