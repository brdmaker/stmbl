# Gildemeister CTX twin operating manual, front matter, safety regulations and table of contents (Inhaltsverzeichnis), pages i-xvi

Source: `Inhaltsverzeichnis.pdf`, 17 scanned pages: cover, copyright page, chapter register (tab divider), "Sicherheitsvorschriften und Vorsichtsmaßnahmen" (safety regulations and precautions) pages iii-x, "Inhaltsverzeichnis" (table of contents) pages xi-xvi. "Betriebsanleitung CTX twin, Dok.-Nr.: daaab1d1, Februar 1995".
Original language: German.

Running footer on every numbered page: "Betriebsanleitung / CTX twin", "Dok.-Nr.: daaab1d1 / Februar 1995", "Seite <n>" (operating manual, CTX twin, doc. no. daaab1d1, February 1995, page n). The safety pages (iii-x) are printed on orange paper.

---

## Cover (PDF page 1)

- Logo "GILDEMEISTER" / "Drehmaschinen" (lathes); product name "CTX twin".
- Title: **Betriebsanleitung** (operating manual).
- Subtitle: "Anleitung zur Bedienung und Wartung von Maschine und Steuerung für den CNC-Zerspaner" (instructions for operation and maintenance of machine and control for the CNC machinist).
- Photo: a turned part in a chuck being machined with coolant spray; a turning tool on the turret at the right.
- Address block: GILDEMEISTER Drehmaschinen GmbH, Gildemeisterstraße 60, D-33689 Bielefeld. Telefon (05205) 74-0; Telefax Service (05205) 74-3964; Telex 932222; Telex Service 932224.
- Footer: "Betriebsanleitung", "Dok.-Nr.: daaab1d1", "Ausgabe: Februar 1995" (edition February 1995).

## Copyright (PDF page 2, unnumbered)

**Urheberrecht** (copyright)

This document is intended for operating, maintenance and assembly personnel. Passing on or copying this document, in full or in part, and using or communicating its content are not permitted unless expressly granted. Gildemeister reserves all rights in the event of a patent grant or registration of a utility model.

Infringements oblige to compensation for damages and may have criminal consequences.

## Chapter register (PDF page 3, tab divider)

Left: "Betriebsanleitung" (operating manual). Right, tab list:

| Tab | German | English |
|---|---|---|
| 1 | Zu dieser Unterlage | About this document |
| 2 | Technische Daten | Technical data |
| 3 | Allgemeine Beschreibung der Maschine | General description of the machine |
| 4 | Kurzanweisung | Short instructions |
| 5 | Bedienung der Maschine und Steuerung | Operation of the machine and control |
| 6 | Einrichten | Setting up |
| 7 | Programme eingeben, aufrufen und bearbeiten | Entering, calling and editing programs |
| 8 | Wartung | Maintenance |
| 9 | Parameterreinstellung, M-Befehle, SPS - Fehlermeldungen | Parameter setting, M commands, PLC error messages |
| 10 | Stichwortverzeichnis | Index |

---

## Sicherheitsvorschriften und Vorsichtsmaßnahmen (Safety regulations and precautions), pages iii-x

(Page iii.)

Gildemeister lathes are built to the latest state of the art and are operationally safe. Faultless and safe operation of this machine nevertheless requires proper transport, professional installation and commissioning, and careful operation and maintenance.

To this end the information in the planning documentation for preparatory measures, the transport instructions, the installation instructions, the operating manual, the programming manual and the technical documentation for mechanics and electrics must be carefully observed.

### 1. Bestimmungsgemäßer Gebrauch (Intended use)

This CNC lathe including its options is intended for

- machining predominantly rotating workpieces of various metallic materials,
- handling workpieces, blanks, bar material and tools, as made possible by the peripheral equipment connected to and supplied with the machine.

Operation and control are designed for operation by a trained and authorized operator.

**Not permitted are:**

- any intervention by hand or with aids into running machine movements, or over or under rotating clamping devices and workpieces
- speeds higher than the stated maximum speed of the clamping devices
- modifying or disabling safety devices such as position switches, interlocks, covers or similar
- painting over or removing signs, warning symbols etc.
- operation of the machine by unauthorized and uninstructed persons
- turning ceramic parts

### 2. Symbol- und Hinweiserklärung (Explanation of symbols and notes)

(Page iv.)

- Warning triangle with "!" -- **Warnung, Vorsicht** (warning, caution): This symbol is used to warn of a possible danger of injury or to health, regardless of the level of danger.
- Symbol of a box with a broken part -- used to point out the danger of possible damage to property.
- Pointing hand -- marks advantageous applications (tips).
- Telephone with "Service" -- marks situations in which the in-house service should be notified.

### 3. Allgemeine Arbeitssicherheitshinweise (General work safety notes)

The machine can be a source of danger of physical injury, to health or of damage to property if it is operated or maintained improperly, not as intended, or by untrained persons.

The operator (company) must ensure that only suitably qualified and authorized personnel work on this machine.

**Warning: Trennende Schutzeinrichtungen, fest oder beweglich** (separating guards, fixed or movable). All movable guard hoods and doors must be closed before the machine is put into operation and must not be opened during operation. Built-in safety devices must not be disabled.

**Warning: Sicherheitsscheibe** (safety window). The safety windows are made of polycarbonate glass, which owing to its elasticity can bulge when struck by a part flying off. A safety distance of **at least 200 mm** from the window must be kept.

**Warning: Druckbehälterverordnung** (pressure vessel regulations). Pressure media are a possible danger of physical injury. If pressure vessels are installed, they are subject to regular inspections under the pressure vessel regulations and, where applicable, to an acceptance test at the machine's installation site. All certificates of pressure vessel inspections must be kept carefully.

(Page v.)

**Warning: Öle, Schmier- und Kühlschmierstoffe** (oils, lubricants and coolants). Due to their constituents (additives) these substances are a possible danger to health and the environment.

Selection and use are solely the responsibility of the operating company. An operating instruction, to be drawn up by the operating company, governing the handling of these substances should contain:

- name of the substances
- dangers to people and the environment
- protective measures and rules of conduct, e.g.
  - wear protective gloves of resistant plastic
  - avoid contact with skin and eyes
  - do not inhale vapours and mist
  - fire, naked flames and smoking are prohibited
- conduct in case of danger, first aid
- proper disposal

**Warning: Drehzahlbegrenzung** (speed limitation). When using clamping devices with a prescribed maximum speed, this speed must under no circumstances be exceeded. This value must be set on the machine before it is put into operation with this clamping device.

If the limit speed of the chuck is lower than the maximum speed of the machine, this value must be entered as a parameter. In addition this value must be set on the separate speed limiter for power chucks (option). Access to these settings is only possible via key switch or password.

- The operating company must ensure that operating, maintenance and servicing personnel have been thoroughly familiarized with all safety notes and that they observe them.
- Any working method that impairs safety on the machine must be avoided.
- The operator is obliged to report immediately any changes to the machine that impair safety.
- Where necessary, the operating company must oblige the operating personnel to wear personal protective equipment and close-fitting clothing.
- The safety of the installation can be impaired if

(Page vi.)

  - conversions or modifications are made to the machine
  - additional equipment (options) other than that provided by the manufacturer or approved by him is used on the machine
  - spare and wearing parts other than those provided by the manufacturer are used

### 4. Bedienung (Operation)

For all work concerning operation, installation, maintenance and repair, the switch-off procedures given in the operating manual must be observed, these are:

**Abschalten der Maschine** (switching off the machine)
- stop at end of cycle
- open the guard hood
- push-button: `Maschine - Aus` (machine off)
- main switch: `Aus` (off)

**Hauptschalter** (main switch). Do **not** switch off the main switch while a machining program is running; machine damage is the result.

**Not-Aus** (emergency stop). Press `Not-Aus` only in case of danger. All movements are switched off; tools in the cut can be destroyed. The machine does not move to its home or initial position.

- Do not reach into the running chip conveyor or tamper with it.
  - Operation of the chip conveyor is only permitted with the chip bin properly placed underneath, so that it is impossible to reach into the ejection chute of the chip conveyor. Chips must not be removed by hand. The machine must be switched off to clean the chip ejection opening.
- Caution when taking over existing clamping devices:
  - check critically whether they are still suitable for this modern lathe.
- After collisions in which chuck parts were damaged, consult the chuck manufacturer about further use of the chuck.
- Clamping devices and the clamping process are particularly important because of possible dangers:

(Page vii.)

  - After every change of clamping device, readjust the switch monitoring for clamping stroke and clamping pressure as described.
  - Use clamping devices only up to their permitted maximum speed. Set the speed limitation accordingly.
  - Follow the instructions for setting the clamping pressure and monitoring; this also applies to tailstock and steady rest.
- Load blanks correctly, in particular ensure clean seating.
- Working with chuck and tailstock: observe the sequence when releasing the workpiece -- risk of injury:
  - 1. release tailstock clamping
  - 2. secure the workpiece
  - 3. open the chuck
- Before switching the machine on, check fill levels and leakage.
- Keep the workplace at the machine clean and clear.
- Caution when removing chips: use only chip hook and hand brush.
- Do not throw waste into the coolant.
- Fix the workpiece before switching off the clamping energy. After the clamping energy is switched off, the hydraulics are without pressure. There is then a danger that the workpiece comes loose from the clamping devices (chuck, tailstock, steady rests).
- Do not let tools protrude further than specified, otherwise collisions with the work area cladding are possible.
- Before longer standstill periods (from 2 days), clean the guideways and move the axes to their end positions. Trigger additional lubrication pulses while doing so.
- Observe the limit values in the technical data.
- Watch for the increase in feed force as the cutting tool wears.
- In bar machining note in particular:
  - Do not process bar material without a concentricity (runout) check.
  - When machining bar sections, the bar end must not protrude from the main spindle.

(Page viii.)

  - When machining bar sections, spindle reducing tubes must be inserted into the draw tube and connecting tube. Inner diameter of the reducing tube = bar diameter + 1 mm.

### 5. Wartung und Reparatur (Maintenance and repair)

(Page ix.)

**Warning:** Before maintenance or repair work, set the main switch to `AUS` (off) and secure it against being switched on again with a padlock. This also applies to work on the chip conveyor and the control cabinet, and on the peripheral equipment.

If the machine must be ready for operation for certain repair work, special caution is required:
- make sure that under no circumstances are persons in the danger zone.
- the danger zone must be secured against unauthorized persons by additional barriers or fences.

- For repairs on electrical equipment the relevant regulations must be observed (IEC ..., EN ..., country-specific accident prevention regulations).
- Maintenance and servicing may only be carried out by specially trained personnel.
- Repairs may only be carried out by personnel trained and authorized by Gildemeister.
- On hydraulic systems with pressure accumulators, depressurize the system at the accumulator drain valve.
- On pneumatic systems, depressurize the system and close the shut-off cock.
- When dismantling safety devices for repair or maintenance, put the machine out of operation as described in the operating manual. Immediately after completing the repair or maintenance work, refit the safety devices.
- Damaged safety windows must be replaced. After damage, check the function of the guard hood and replace damaged parts.
- Use only mutually compatible lubricants and coolants according to the lubricant table.
- Fill hydraulic oil only filtered (filter fineness 10 µm), otherwise danger of damage to the hydraulic pump.
- Always observe the notes on care and maintenance of the clamping devices.

(Page x.)

- Never clean the machine with wool or cleaning wool. Use only lint-free cleaning material (cloths or paper).
- Never clean the machine with compressed air. Chips could get into guideways, spindle bearings or the turret.
- Clean the safety window with a mild soap solution or a mild household cleaner, never with strongly alkaline cleaning agents.

### 6. Garantie (Warranty)

Gildemeister accepts neither liability nor warranty
- if the instructions of the operating and programming manuals, and of the planning, transport and installation instructions are not observed,
- if the machine including additional equipment is operated improperly,
- if the machine including additional equipment is not properly maintained and serviced,
- if safety devices are not used or are disabled,
- if any technical or functional modifications are made that are not approved by Gildemeister.

### 7. Konformitätserklärung (Declaration of conformity)

If the points listed under 6 are not observed, the declaration of conformity supplied with the machine according to the EC Machinery Directive 89/392/EWG (EEC) becomes invalid.

---

## Inhaltsverzeichnis (Table of contents), pages xi-xvi

Page numbers are the manual's own ("chapter-page").

| No. | German | English | Page |
|---|---|---|---|
| **1.** | **Zu dieser Anleitung** | **About this manual** | 1-1 |
| 1.1 | An wen wendet sich diese Anleitung? | Who is this manual for? | 1-3 |
| 1.2 | Was steht in dieser Anleitung? | What is in this manual? | 1-3 |
| 1.3 | Was steht nicht in dieser Anleitung? | What is not in this manual? | 1-4 |
| 1.4 | Hinweise | Notes | 1-4 |
| **2.** | **Technische Daten** | **Technical data** | 2-1 |
| 2.1 | Technische Daten | Technical data | 2-3 |
| 2.2 | Leistungsdiagramm Hauptantrieb 202 Nm | Power diagram main drive 202 Nm | 2-4 |
| 2.3 | Leistungsdiagramm angetr. Werkzeuge | Power diagram driven tools | 2-5 |
| 2.4 | Arbeitsraum Blatt 1 | Work area sheet 1 | 2-6 |
| 2.5 | Arbeitsraum Blatt 2 | Work area sheet 2 | 2-7 |
| 2.6 | Vorschubantriebe | Feed drives | 2-8 |
| **3.** | **Allgemeine Beschreibung der Maschine** | **General description of the machine** | 3-1 |
| 3.1 | Konzept der CTX twin | Concept of the CTX twin | 3-3 |
| 3.1.1 | Aggregategruppen | Unit groups (aggregate groups) | 3-3 |
| 3.2 | Baugruppen der CTX twin | Assemblies of the CTX twin | 3-4 |
| 3.3 | Optionale Baugruppen | Optional assemblies | 3-6 |
| 3.4 | Bestimmungsgemäßer Gebrauch der Maschine | Intended use of the machine | 3-8 |
| 3.5 | Warnung vor Mißbrauch | Warning against misuse | 3-9 |
| 3.6 | Sicherheitseinrichtungen | Safety devices | 3-10 |
| 3.6.1 | Schlüsselschalter | Key switch | 3-10 |
| 3.6.2 | Not-Aus Abschaltung | Emergency stop shutdown | 3-10 |
| 3.6.3 | Zustimmtaste | Enabling (consent) button | 3-10 |
| 3.6.4 | Trennende Schutzeinrichtungen um den Bearbeitungsraum | Separating guards around the machining area | 3-11 |
| 3.6.4.1 | Automatische Arbeitsraumtrennwand (Betriebsartensplitting) | Automatic work area partition wall (operating mode splitting) | 3-11 |
| 3.6.5 | Drehzahlkontrolle | Speed monitoring | 3-12 |
| 3.6.6 | Werkstückspannung | Workpiece clamping | 3-12 |
| 3.6.7 | Hydraulik | Hydraulics | 3-13 |
| 3.6.8 | Pneumatik (wenn vorhanden) | Pneumatics (if present) | 3-13 |
| 3.6.9 | Trennende Schutzeinrichtungen für Einstell- und Wartungsräume | Separating guards for setting and maintenance areas | 3-13 |
| 3.6.10 | Lärmschutz | Noise protection | 3-13 |
| 3.6.11 | Emissionen | Emissions | 3-13 |
| 3.6.12 | Späneförderer | Chip conveyor | 3-14 |
| 3.7 | Schmierstoffe und Kühlschmiermittel | Lubricants and coolants | 3-15 |
| 3.7.1 | Füllmengen | Fill quantities | 3-15 |
| 3.7.2 | Schmierstofftabelle | Lubricant table | 3-16 |
| 3.7.3 | Kühlschmiermittel | Cutting coolant | 3-17 |
| 3.7.4 | Kühlmittel für die integrierten Spindelmotore | Coolant for the integrated spindle motors | 3-17 |
| 3.7.5 | Umgang mit Schmierstoffen und Kühlschmiermitteln | Handling lubricants and coolants | 3-18 |
| **4.** | **Kurzanweisung für Einleger** | **Short instructions for loaders (part loaders)** | 4-1 |
| 4.1 | Kurzanweisung für Einleger | Short instructions for loaders | 4-3 |
| 4.1.1 | Maschine starten | Starting the machine | 4-3 |
| 4.1.2 | Maschine anhalten | Stopping the machine | 4-4 |
| 4.1.3 | Werkstück wechseln | Changing the workpiece | 4-4 |
| **5.** | **Bedienung von Maschine und Steuerung** | **Operation of machine and control** | 5-1 |
| 5.1 | Allgemeine Hinweise | General notes | 5-4 |
| 5.1.1 | Aggregategruppen | Unit groups | 5-4 |
| 5.1.2 | Betriebsartensplitting | Operating mode splitting | 5-4 |
| 5.2 | Bedienelemente und Tastatur | Controls and keyboard | 5-5 |
| 5.2.1 | Bedienelemente an der rechten Seite | Controls on the right-hand side | 5-5 |
| 5.2.2 | Bedienfeld | Control panel | 5-6 |
| 5.2.3 | Weitere Bedienelemente | Further controls | 5-20 |
| 5.3 | Menütechnik und Softkeytasten | Menu technique and soft keys | 5-21 |
| 5.3.1 | Bildschirm | Screen | 5-22 |
| 5.3.2 | Allgemeines | General | 5-22 |
| 5.3.3 | Fenster - Aufteilung des Bildschirms | Windows - screen layout | 5-24 |
| 5.3.4 | Anzeigenbereich | Display area | 5-26 |
| 5.4 | Befehlseingaben | Command input | 5-28 |
| 5.4.1 | Pull-down-Menüs | Pull-down menus | 5-28 |
| 5.4.2 | Dialogboxen | Dialog boxes | 5-29 |
| 5.5 | INFO-System | INFO system | 5-32 |
| 5.5.1 | Allgemeines | General | 5-32 |
| 5.5.2 | Funktionen und Tastaturbelegung im INFO-System | Functions and key assignment in the INFO system | 5-33 |
| 5.5.3 | Fehler - INFO-System | Errors - INFO system | 5-33 |
| 5.6 | Grafische Bediener-Unterstützung | Graphical operator support | 5-34 |
| 5.7 | Ein- und Ausschalten der Maschine | Switching the machine on and off | 5-35 |
| 5.7.1 | Maschine einschalten | Switching the machine on | 5-35 |
| 5.7.2 | Referenzpunkt anfahren | Approaching the reference point (homing) | 5-36 |
| 5.7.3 | Fehleranzeigen und Meldungen quittieren | Acknowledging error displays and messages | 5-37 |
| 5.7.4 | Arbeitsraum-Positionsschalter freifahren | Moving clear of the work area position switches | 5-38 |
| 5.7.5 | Maschine ausschalten | Switching the machine off | 5-39 |
| 5.7.6 | Not-Aus-Abschaltung | Emergency stop shutdown | 5-39 |
| **6.** | **Einrichten** | **Setting up** | 6-1 |
| 6.1 | Allgemeine Hinweise | General notes | 6-3 |
| 6.1.1 | Koordinatensysteme | Coordinate systems | 6-3 |
| 6.1.2 | Aggregategruppen | Unit groups | 6-4 |
| 6.1.3 | Betriebsartensplitting | Operating mode splitting | 6-4 |
| 6.1.4 | Bearbeitungsmöglichkeiten | Machining options | 6-5 |
| 6.2 | Voraussetzungen | Prerequisites | 6-7 |
| 6.3 | Die Arbeitsschritte im Überblick | Overview of the work steps | 6-7 |
| 6.4 | Die Spanneinrichtung im einzelnen | The clamping equipment in detail | 6-9 |
| 6.5 | Spannmittel aus- und einbauen | Removing and fitting clamping devices | 6-12 |
| 6.5.1 | Spannmittel ausbauen | Removing a clamping device | 6-12 |
| 6.5.2 | Spannmittel einbauen | Fitting a clamping device | 6-13 |
| 6.5.3 | Spannhub einstellen | Setting the clamping stroke | 6-14 |
| 6.5.4 | Parametereingabe Spannmittel | Parameter input for clamping devices | 6-17 |
| 6.5.5 | Drehzahlbegrenzung einstellen | Setting the speed limitation | 6-18 |
| 6.6 | Rohteil einspannen | Clamping the blank | 6-19 |
| 6.6.1 | Spanndruck überprüfen und ggf. einstellen | Checking and if necessary setting the clamping pressure | 6-19 |
| 6.6.2 | Werkzeugträger bestücken | Loading the tool carrier (turret) | 6-20 |
| 6.7 | Nullpunkt in Z-Achse verschieben mit verrechnetem Werkzeug | Shifting the Z-axis zero point with a measured tool | 6-21 |
| 6.8 | Folgewerkzeuge (noch nicht verrechnete Werkzeuge) verrechnen | Measuring subsequent tools (tools not yet measured) | 6-23 |
| 6.9 | Schutzzonen setzen oder deaktivieren | Setting or deactivating protection zones | 6-25 |
| 6.10 | Zusätzliche Bearbeitungsmöglichkeiten in der Betriebsart MASCHINE | Additional machining options in the operating mode `MASCHINE` (machine/manual) | 6-26 |
| 6.10.1 | Dauervorschubdrehen langer Werkstücke ohne Programm | Continuous-feed turning of long workpieces without a program | 6-26 |
| 6.10.2 | Parallelbearbeitung | Parallel machining | 6-27 |
| 6.11 | Option C-Achse | Option C axis | 6-28 |
| 6.11.1 | Die Arbeitsschritte im Überblick | Overview of the work steps | 6-28 |
| 6.11.2 | Drehzahl eingeben und angetriebene Werkzeuge starten | Entering the speed and starting driven tools | 6-29 |
| 6.11.3 | Werkzeuge verrechnen | Measuring tools | 6-30 |
| 6.11.4 | Nullpunkt der C-Achse verschieben | Shifting the C-axis zero point | 6-31 |
| 6.12 | Option Abholeinrichtung | Option part pick-up (unloading) device | 6-32 |
| 6.12.1 | Abholeinrichtung einrichten | Setting up the pick-up device | 6-33 |
| 6.12.2 | Abholeinrichtung Werkstückausstoßer | Pick-up device workpiece ejector | 6-34 |
| 6.13 | Option Zwei-Druck-Spanneinrichtung | Option two-pressure clamping equipment | 6-36 |
| 6.14 | Option Werkzeugmeßeinrichtung | Option tool measuring device | 6-37 |
| 6.15 | Option Stangenbearbeitung | Option bar machining | 6-39 |
| 6.15.1 | Stangenbearbeitung nur mit der Drehmaschine | Bar machining with the lathe only | 6-39 |
| 6.15.2 | Automatische Material Zu- und Abführung | Automatic material feed and removal | 6-41 |
| 6.15.2.1 | Allgemeine Hinweise | General notes | 6-41 |
| 6.15.2.2 | Bearbeitung mit Stangenvorschub | Machining with bar feeder | 6-44 |
| 6.15.2.3 | Bearbeitung mit Stangenlademagazin | Machining with bar loading magazine | 6-46 |
| 6.16 | Option Kontrolleuchte | Option signal lamp | 6-48 |
| 6.17 | Option Überlastkupplung X- und Z-Achse | Option overload clutch X and Z axis | 6-49 |
| 6.18 | Werkstückanlagekontrolle | Workpiece seating check | 6-50 |
| **7.** | **Programme eingeben, aufrufen und abarbeiten** | **Entering, calling and running programs** | 7-1 |
| 7.1 | Programme eingeben, aufrufen und abarbeiten | Entering, calling and running programs | 7-3 |
| 7.2 | Fertiges Programm aufrufen | Calling a finished program | 7-3 |
| 7.3 | Werkstück im Einzelsatz abarbeiten | Machining a workpiece in single block | 7-6 |
| 7.4 | Werkzeug-Korrekturen eingeben | Entering tool offsets | 7-7 |
| 7.5 | Werkstücke automatisch abarbeiten | Machining workpieces automatically | 7-8 |
| 7.6 | Werkstück wechseln im Automatik-Betrieb | Changing the workpiece in automatic mode | 7-9 |
| 7.7 | Programm von externem Datenträger einlesen | Reading in a program from an external data medium | 7-10 |
| 7.8 | Programm auf externen Datenträger senden / sichern | Sending / saving a program to an external data medium | 7-11 |
| 7.9 | Programmablauf ab- oder unterbrechen | Aborting or interrupting program execution | 7-12 |
| 7.10 | Programm abbrechen und von Anfang an neu starten | Aborting a program and restarting from the beginning | 7-12 |
| 7.11 | Expertenprogramme "%XTWI..NCS" | Expert programs "%XTWI..NCS" | 7-13 |
| 7.11.1 | Allgemeines | General | 7-13 |
| 7.11.2 | Expertenprogramme der Version %XTWI... | Expert programs of version %XTWI... | 7-14 |
| 7.11.2.1 | Vorhandene Programme | Existing programs | 7-15 |
| 7.11.2.2 | Beschreibung der Programme | Description of the programs | 7-16 |
| | Programm %XTWIFAHR.NCS | Program %XTWIFAHR.NCS | 7-16 |
| | Programm %XTWIFEST.NCS | Program %XTWIFEST.NCS | 7-18 |
| | Programm %XTWI1U2.NCS | Program %XTWI1U2.NCS | 7-20 |
| | Programm %XTWI1U2A.NCS | Program %XTWI1U2A.NCS | 7-23 |
| 7.11.2.3 | Übersicht der Programme mit ihren Variablen | Overview of the programs with their variables | 7-27 |
| 7.11.3 | Expertenprogramme der Version %X.... | Expert programs of version %X.... | 7-28 |
| 7.11.3.1 | Vorhandene Programme | Existing programs | 7-29 |
| | Programm %X27.NCS | Program %X27.NCS | 7-30 |
| **8.** | **Wartung** | **Maintenance** | 8-1 |
| 8.1 | Wartung | Maintenance | 8-3 |
| 8.2 | Füllstände, Kontrolle | Fill levels, check | 8-3 |
| 8.3 | Arbeitsraum, Kontrolle und Reinigung | Work area, check and cleaning | 8-4 |
| 8.4 | Wartungsplan nach Baugruppen und Betriebsstunden | Maintenance schedule by assembly and operating hours | 8-5 |
| **9.** | **Parametereinstellung, M-Befehle, SPS-Fehlermeldungen** | **Parameter setting, M commands, PLC error messages** | 9-1 |
| 9.1 | Parameter | Parameters | 9-3 |
| 9.1.1 | Parametereinstellung | Parameter setting | 9-3 |
| 9.1.2 | Parameterliste | Parameter list | 9-6 |
| 9.1.2.1 | Schutzklassen / Schutzkriterien | Protection classes / protection criteria | 9-6 |
| 9.1.2.2 | Bereich | Range | 9-6 |
| 9.2 | Beschreibung der M-Befehle | Description of the M commands | 9-43 |
| 9.3 | Fehlermeldungen | Error messages | 9-55 |
| 9.4 | Externe Ereignisse | External events | 9-184 |
| 9.4.1 | Belegung der Ereignis Merker | Assignment of the event flags (markers) | 9-185 |
| | **Index** | **Index** | X-1 |

Notes: page xi has the heading "Inhaltsverzeichnis"; page xvi (PDF page 17) is blank apart from header/footer (show-through of page xv only). In 6.18 a hand-written "!" mark is next to "Werkstückanlagekontrolle" on page xiv. 6.12.2 reads "Abholeinrichtung Werkstückausstoßer" as printed.

---

## For the retrofit

- Manual identification: Gildemeister CTX twin operating manual, Dok.-Nr. daaab1d1, February 1995 (cover). The control type is not named on these pages; the contents show a menu/soft-key screen with pull-down menus, dialog boxes and an "INFO-System" (5.3-5.5, p. 5-21 to 5-33) and expert programs named `%XTWI...NCS` (7.11, p. 7-13), i.e. a PC-style CNC front end [control make to be confirmed from chapter 5/7].
- Two-side architecture: "Aggregategruppen" (unit groups) and "Betriebsartensplitting" (operating mode splitting, separate modes per side) with an "automatische Arbeitsraumtrennwand" (automatic work area partition wall) (3.1.1 p. 3-3, 3.6.4.1 p. 3-11, 5.1.1/5.1.2 p. 5-4, 6.1.2/6.1.3 p. 6-4). The partition wall is a control-driven, interlocked actuator.
- Part transfer between the sides: expert program `%XTWI1U2.NCS` (p. 7-20) and `%XTWI1U2A.NCS` (p. 7-23) ("1U2" = 1 and 2, likely the side-1-to-side-2 transfer [uncertain]), plus `%XTWIFAHR.NCS` (p. 7-16), `%XTWIFEST.NCS` (p. 7-18), `%X27.NCS` (p. 7-30); "Parallelbearbeitung" (parallel machining) in manual mode (6.10.2, p. 6-27).
- Safety functions the new control must provide: key switch, emergency stop, enabling button `Zustimmtaste`, guard-door interlocks, speed monitoring/limitation per chuck (parameter plus separate speed limiter for power chucks, access via key switch or password), clamping stroke and clamping pressure monitoring switches, work area position switches (3.6.1-3.6.6 p. 3-10 to 3-12; safety pages v, vii; 6.5.3-6.5.5 p. 6-14 to 6-18; 5.7.4 p. 5-38).
- E-stop behaviour per manual: all movements switched off, machine does not move to home position (p. vi). Do not switch off main switch during a program (p. vi).
- Hydraulic clamping: after removing clamping energy the hydraulics are pressureless and the workpiece can come loose (p. vii); accumulator must be drained at the accumulator drain valve before work (p. ix). Release sequence with tailstock: tailstock first, secure part, then chuck (p. vii).
- Lubrication: "Schmierimpulse" (lubrication pulses) are triggerable; extra pulses recommended before standstill of 2 days or more (p. vii).
- Options to be handled: C axis with driven tools (6.11, p. 6-28), part pick-up device with ejector (6.12, p. 6-32), two-pressure clamping (6.13), tool measuring device (6.14), bar feeder / bar loading magazine (6.15), signal lamp (6.16), overload clutch X/Z (6.17), workpiece seating check (6.18, p. 6-50).
- M-code list p. 9-43, PLC error messages p. 9-55 to 9-183, external events and event flags p. 9-184/9-185, parameter list p. 9-6 (chapter 9) -- key sources for I/O mapping.
