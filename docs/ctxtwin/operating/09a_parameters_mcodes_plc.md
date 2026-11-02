# Gildemeister CTX twin operating manual, 9. Parameter setting, M-commands, PLC error messages (Parametereinstellung, M-Befehle, SPS-Fehlermeldungen), pages 9-1 to 9-42

Source: `9_Parametereinstellung_M-Befehle_SPS-Fehlermeldungen_a.pdf` (part a of 4), 42 scanned pages = manual pages 9-1 to 9-42. "Betriebsanleitung CTX twin, Dok.-Nr.: daaab1d1, Februar 1995"; the parameter-list pages (from 9-6) carry the revision note "REV: dab9501 (05.05.1995) zu daaab1d1 Februar 1995".
Original language: German.

Running page header on every page: "9. Parametereinstellung, M-Befehle, SPS-Fehlermeldungen". Footer: "Betriebsanleitung CTX twin, Dok.-Nr.: daaab1d1, Februar 1995, Seite 9-xx".

This part covers the chapter contents, 9.1.1 parameter setting and the complete 9.1.2 parameter list (ends on page 9-42). It contains no M-codes or PLC messages; those follow in parts b-d (9.2 description of the M-commands from page 9-43, 9.3 error messages from 9-55, 9.4 external events from 9-184).

Note on handwriting: the scanned copy has handwritten numbers written next to many parameters (mostly in or near the "CTX twin" column). They look like the values actually set on this machine and are reproduced in a separate column "Handwritten". Printed "x" marks in the CTX 200 / CTX 400-500-600 / CTX twin columns mean the parameter applies to that machine type.

---

## Contents of chapter 9 (page 9-1)

| Section | German | English | Page |
|---|---|---|---|
| 9.1 | Parameter | Parameters | 9-3 |
| 9.1.1 | Parametereinstellung | Parameter setting | 9-3 |
| 9.1.2 | Parameterliste | Parameter list | 9-6 |
| 9.1.2.1 | Schutzklassen / Schutzkriterien | Protection classes / protection criteria | 9-6 |
| 9.1.2.2 | Bereich | Range | 9-6 |
| 9.2 | Beschreibung der M-Befehle | Description of the M-commands | 9-43 |
| 9.3 | Fehlermeldungen | Error messages | 9-55 |
| 9.4 | Externe Ereignisse | External events | 9-184 |
| 9.4.1 | Belegung der Ereignis Merker | Assignment of the event flags | 9-185 |

Page 9-2: (blank page)

---

## 9.1 Parameter (Parameters)

### 9.1.1 Parametereinstellung (Parameter setting)

(Page 9-3)

This lathe has presettings (= parameters) for certain functions, e.g. for the NC zero point or the graphics display.

You have to set some of these parameters in your daily work. Some others you should under no circumstances change.

The parameters that are important for your daily work are explained below in ascending order.

A complete list of the parameters is in the programming handbook (`Programmier-Handbuch`).

For each parameter you find the menu items you must select one after another to set the respective value.

**`NC-Nullpunkt` (NC zero point)**
With this parameter you can shift the NC zero point (= workpiece zero point referred to the machine zero point). The X value always stays the same, and the Z value changes depending on the workpiece length.
- Operating mode `PARAMETER`
- Menu item `AKT. PARAMETER` (current parameters)
- Menu item `EINRICHTE (MENÜ)` (setup (menu))
- Menu item `NC-NULLPUNKT` (NC zero point)

**`Vorschub Hand` (manual feed)**
With this parameter you set the feed rate (mm/min.) for manual operation.
- Operating mode `PARAMETER`
- Menu item `AKT. PARAMETER`
- Menu item `MASCHINENPARAMETER` (machine parameters)
- Menu item `VORSCHÜBE` (feeds)

**`Eilgang Hand` (manual rapid traverse)**
With this parameter you set the rapid traverse speed (mm/min.) for manual operation.
- Operating mode `PARAMETER`
- Menu item `AKT. PARAMETER`
- Menu item `MASCHINENPARAMETER`
- Menu item `VORSCHÜBE`

(Page 9-4)

**`Eilgang Auto` (automatic rapid traverse)**
With this parameter you set the rapid traverse speed (mm/min.) for automatic operation (= automatic execution of a program). The speed can be set individually for each axis.
- Operating mode `PARAMETER`
- Menu item `AKT. PARAMETER`
- Menu item `MASCHINENPARAMETER`

**`Spannart` (clamping mode)**
In this parameter you set the clamping mode.
- Operating mode `PARAMETER`
- Menu item `AKT. PARAMETER`
- Menu item `SPS` (PLC)
- Menu item `SPANNART` (clamping mode)

**`Anzeige Sprache` (display language)**
is still to be implemented (`wird noch implementiert`).

**`Bedienung` (operation)**
- Operating mode `PARAMETER`
- Menu item `AKT. PARAMETER`
- Menu item `SPS-PARAMETER` (PLC parameters)
- Menu item `HYDRAULIK,SCHMIERUNG` (hydraulics, lubrication) (additional messages about hydraulics and lubrication)

or
- Menu item `HANDHABUNG` (handling) (for mounting the loading flap, `Ladeklappe`)

**`Maschinenausrüstung` (machine equipment)**
- Operating mode `PARAMETER`
- Menu item `AKT. PARAMETER`
- Menu item `SPS-PARAMETER`
- Menu item `SPANNART`

or
- Menu item `REITSTOCK` (tailstock)

or
- Menu item `HANDHABUNG` (handling)

or
- Menu item `AUTOM. HAUBE` (automatic hood/cover)

or
- Menu item `MEßSYSTEM` (measuring system)

(Page 9-5)

**`Eilgang C-Achse` (C-axis rapid traverse)**
In this parameter you set the maximum angular velocity in degrees/min for spindle positioning in rapid traverse of C axis 1.
- Operating mode `PARAMETER`
- Menu item `AKT. PARAMETER`
- Menu item `MASCHINENPARAMETER`
- Menu item `VORSCHÜBE`

**`Nullpunkt C-Achse` (C-axis zero point)**
In this parameter you set the zero offset for C axis 1.
- Operating mode `PARAMETER`
- Menu item `AKT. PARAMETER`
- Menu item `MASCHINENPARAMETER`
- Menu item `EINRICHTE (MENÜ)`
- Menu item `NP-VERSCHIEBUNG C-ACHSE` (zero offset C axis)

**`Drehzahlbegrenzung Hauptspindel` (main spindle speed limit)**
In this parameter you set the maximum speed of the main spindle.
- Operating mode `PARAMETER`
- Menu item `AKT. PARAMETER`
- Menu item `MASCHINENPARAMETER`
- Menu item `DREHZAHLEN` (speeds)
- Menu item `DREHZAHL SPINDEL 1` (speed spindle 1)

**`Meßart` (measuring mode)**
In this parameter you must enter whether the dimensions of a tool are set manually, with an optical measuring device or with a probe (`Meßtaster`).
- Operating mode `PARAMETER`
- Menu item `AKT. PARAMETER`
- Menu item `NC-SCHALTER` (NC switches)
- Menu item `WKZ MESSART` (tool measuring mode)

**`Betriebsartensplitting` (operating-mode splitting)**
In this parameter you must enter whether the operating-mode splitting mode is switched on or off.
- Operating mode `PARAMETER`
- Menu item `KONFIG` (configuration)
- Menu item `MASCHINEN LISTE` (machine list)
- `Prameter 8: Splittingbetrieb EIN oder AUS` (parameter 8: splitting operation ON or OFF) [sic, "Prameter" in the original]

---

### 9.1.2 Parameterliste (Parameter list)

(Page 9-6)

This parameter list describes all parameters that concern the equipment of a specific machine.

All other parameters are to be taken from the programming manual ELTROPILOT (`Programmieranleitung ELTROPILOT`), since they have the same meaning across all machine types.

#### 9.1.2.1 Schutzklassen / Schutzkriterien (Protection classes / protection criteria)

| Class | German | English |
|---|---|---|
| A | Der Parameter ist nicht geschützt. Jeder Bediener darf den Parameter ändern. | The parameter is not protected. Every operator may change it. |
| B | Der Bediener mit der Benutzerklasse NC - Programmierer darf den Parameter ändern. | The operator with user class NC programmer may change the parameter. |
| C | Der Parameter ist durch ein Paßwort geschützt und darf nicht geändert werden, wenn die Betriebsart Maschine Automatik aktiv ist. | The parameter is password-protected and must not be changed while operating mode machine automatic is active. |
| D | Der Parameter ist nur für den Gildemeister Service zugänglich. Der Benutzer kann sich den Parameter ansehen, ihn aber nicht verändern. | The parameter is accessible only to Gildemeister service. The user can view it but not change it. |

"A /Akt" in the tables below = class A, set under `AKT. PARAMETER` [uncertain reading of the meaning of "Akt"].

#### 9.1.2.2 Bereich (Range)

Valid range of the respective parameter settings.
UNS32 = 0-4294967295

Table columns in the original: `Schutzklasse` (protection class), parameter name, `Wertebereich` (value range), `K-Nr.` (K number), applicability `CTX 200`, `CTX 400 500 600`, `CTX twin`.

#### Parameter 020: `Steuerparameter` (control parameters) (page 9-6)

| Class | Dialog box | Parameter (German) | English | Range | K-Nr. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|---|---|
| D | 1 | `Maschinentypnummer` | machine type number | UNS32 | 50000 | | | |
| D | 1 | `Serienversion` | series version | UNS32 | 50001 | | | |
| D | 1 | `Steuerungstypnummer` | control type number | UNS32 | 50002 | | | |

#### Parameter 022: `Spindel H, Spannfutter H, Getriebe H` (spindle H, chuck H, gearbox H) (pages 9-7, 9-8)

Handwritten at top right of page 9-7: "0.37" and "2089" [uncertain; meaning not stated].

| Class | Dialog box | Parameter (German) / values | English | Range | K-Nr. | CTX 200 | CTX 400/500/600 | CTX twin | Handwritten |
|---|---|---|---|---|---|---|---|---|---|
| A /Akt | 1 | `Spannart`: 1 = `Innenspannen`, 2 = `Aussenspannen`, 3 = `Ausklinkhub` | clamping mode: 1 = internal clamping (ID), 2 = external clamping (OD), 3 = release stroke | 1-3 | 50010 | x | x | x | 2 |
| A /Akt | 1 | `Spannfuttertyp`: 1 = `Handbetätigt`, 2 = `Kraftbetätigt` | chuck type: 1 = manually operated, 2 = power operated | 1-2 | 50011 | x | x | x | 2 |
| A /Akt | 1 | `Spannzylinder`: 0 = `kein Spannzylinder`, 1 = `zurück = Backen zufahren`, 2 = `zurück = Backen auffahren` | clamping cylinder: 0 = no clamping cylinder, 1 = retract = jaws close, 2 = retract = jaws open | 0-2 | 50012 | x | x | x | 1 |
| A /Akt | 1 | `Spannfutter: Nachspannzeit [20 msec]` | chuck: re-clamping time [units of 20 ms] | 0-100 | 50013 | x | | x | 1 |
| C | 2 | `Spannertyp`: 0 = `Hydraulisch, Hubbegrenzung, Sicherheitsberos, Mittelstellungsventile (Standard)`; 1 = `Hydraulisch, ohne Hubbegrenzung, Rastventile` | clamp type: 0 = hydraulic, stroke limiting, safety proximity switches (Beros), centre-position valves (standard); 1 = hydraulic, without stroke limiting, detent valves | 0-10 | 50014 | | x | | 0 |
| C | 2 | `Schwenkfuttertyp`: 0 = `kein Schwenkfutter montiert`; 1 = `AXN-Schwenkfuttertyp der Fa. Autoblock mit 4 Positionen` | swivel chuck type: 0 = no swivel chuck fitted; 1 = AXN swivel chuck from Autoblock with 4 positions | 0-10 | 50015 | | x | | 0 |
| C | 2 | `Maximale Spannfutterschwenkdrehzahl`: Wert = `Maximaldrehzahl bis zu der das Schwenken des Schwenkfutters erlaubt ist [Upm]` | maximum chuck swivel speed: value = max. speed up to which swivelling the swivel chuck is allowed [rpm] | 0-65535 | 50016 | | x | | 0 |
| D | 3 | `Spindel montiert` | spindle fitted | 0-1 | 50017 | | | x | 1 |
| D | 3 | `Spindelklemmung montiert` | spindle clamp (brake) fitted | 0-1 | 50018 | x | x | x | 0 |
| D | 3 | `Klemmung lösen Zeit [1/50sec]` | clamp release time [1/50 s] | 0-255 | 50019 | x | x | x | 0 |
| D | 3 | `Klemmung klemmen Zeit [1/50sec]` | clamp engage time [1/50 s] | 0-255 | 50020 | x | x | x | 0 |
| D | 3 | `Typ C-Achse`: 0 = `keine C-Achse montiert`, 1 = `C - Achse mit Andruckrolle`, 2 = `C - Achse ohne Andruckrolle` | C-axis type: 0 = no C axis fitted, 1 = C axis with pressure roller, 2 = C axis without pressure roller | 0-10 | 50021 | x | x | x | 2 |
| D | 3 | `Spindel: Verzögerungszeit 1 [100 msec]` (`HW-Spindelfreigabe --> Reglerfreigabe Steller EIN`) | spindle: delay time 1 [units of 100 ms] (hardware spindle enable -> controller enable drive ON) | 0-255 | 50022 | | | x | 3 |
| D | 4 | `Getriebetyp`: 0 = `kein Getriebe montiert`, 1 = `ZF: 2-Stufen elektrisch betätigt` | gearbox type: 0 = no gearbox fitted, 1 = ZF 2-stage, electrically shifted | 0-10 | 50023 | | x | x | 0 |

#### Parameter 023: `Spindel 1, Spannfutter 1, Getriebe 1` (spindle 1, chuck 1, gearbox 1) (pages 9-8, 9-9)

No printed "x" marks for most rows of this table; the handwritten values are written between the CTX 400 and CTX twin columns.

| Class | Dialog box | Parameter (German) / values | English | Range | K-Nr. | Printed applicability | Handwritten |
|---|---|---|---|---|---|---|---|
| A /Akt | 1 | `Spannart`: 1 = `Innenspannen`, 2 = `Aussenspannen`, 3 = `Ausklinkhub` | clamping mode: 1 = internal, 2 = external, 3 = release stroke | 1-3 | 50024 | - | 1 |
| A /Akt | 1 | `Spannfuttertyp`: 1 = `Handbetätigt`, 2 = `Kraftbetätigt` | chuck type: 1 = manual, 2 = power operated | 1-2 | 50025 | - | 2 |
| A /Akt | 1 | `Spannzylinder`: 0 = `kein Spannzylinder`, 1 = `zurück = Backen zufahren`, 2 = `zurück = Backen auffahren` | clamping cylinder: 0 = none, 1 = retract = jaws close, 2 = retract = jaws open | 0-2 | 50026 | - | 1 |
| A /Akt | 1 | `Spannfutter: Nachspannzeit [20 msec]` | chuck: re-clamping time [20 ms] | 0-100 | 50027 | - | 1 |
| C | 2 | `Spannertyp`: 0 = `Hydraulisch, Hubbegrenzung, Sicherheitsberos, Mittelstellungsventile (Standard)`; 1 = `Hydraulisch, ohne Hubbegrenzung, Rastventile` | clamp type: 0 = hydraulic, stroke limit, safety Beros, centre-position valves (standard); 1 = hydraulic, no stroke limit, detent valves | 0-10 | 50028 | - | 0 |
| C | 2 | `Schwenkfuttertyp`: 0 = `kein Schwenkfutter montiert`; 1 = `AXN-Schwenkfuttertyp der Fa. Autoblock mit 4 Positionen` | swivel chuck type: 0 = none; 1 = Autoblock AXN, 4 positions | 0-10 | 50029 | - | 0 |
| C | 2 | `Maximale Spannfutterschwenkdrehzahl` [Upm] | maximum chuck swivel speed [rpm] | 0-65535 | 50030 | - | 0 |
| D | 3 | `Spindel montiert` | spindle fitted | 0-1 | 50031 | x / x / x | 1 (row circled; a "0" written and struck through [uncertain]) |
| D | 3 | `Spindelklemmung montiert` | spindle clamp fitted | 0-1 | 50032 | - | 0 |
| D | 3 | `Klemmung lösen Zeit [1/50sec]` | clamp release time [1/50 s] | 0-255 | 50033 | - | 0 |
| D | 3 | `Klemmung klemmen Zeit [1/50sec]` | clamp engage time [1/50 s] | 0-255 | 50034 | - | 0 |
| D | 3 | `Typ C-Achse`: 0 = `keine C-Achse montiert`, 1 = `C - Achse mit Andruckrolle`, 2 = `C - Achse ohne Andruckrolle` | C-axis type: 0 = none, 1 = with pressure roller, 2 = without pressure roller | 0-10 | 50035 | - | 0 [uncertain, could be 6] |
| D | 3 | `Spindel: Verzögerungszeit 1 [100 msec]` (`HW-Spindelfreigabe --> Reglerfreigabe Steller EIN`) | spindle: delay time 1 [100 ms] (HW spindle enable -> controller enable drive ON) | 0-255 | 50036 | CTX twin x | 1 |
| D | 4 | `Getriebetyp`: 0 = `kein Getriebe montiert`, 1 = `ZF: 2-Stufen elektrisch betätigt` | gearbox type: 0 = none, 1 = ZF 2-stage electrical | 0-10 | 50037 | - | 0 |

#### Parameter 024: `Spindel 2, Spannfutter 2, Getriebe 2` (spindle 2, chuck 2, gearbox 2) (pages 9-10, 9-11)

Handwritten values written in the CTX twin column area; no printed "x" marks except as noted.

| Class | Dialog box | Parameter (German) / values | English | Range | K-Nr. | Handwritten |
|---|---|---|---|---|---|---|
| A /Akt | 1 | `Spannart`: 1 = `Innenspannen`, 2 = `Aussenspannen`, 3 = `Ausklinkhub` | clamping mode: 1 = internal, 2 = external, 3 = release stroke | 1-3 | 50038 | 1 |
| A /Akt | 1 | `Spannfuttertyp`: 1 = `Handbetätigt`, 2 = `Kraftbetätigt` | chuck type: 1 = manual, 2 = power operated | 1-2 | 50039 | 1 [uncertain, written as "("] |
| A /Akt | 1 | `Spannzylinder`: 0 = `kein Spannzylinder`, 1 = `zurück = Backen zufahren`, 2 = `zurück = Backen auffahren` | clamping cylinder: 0 = none, 1 = retract = jaws close, 2 = retract = jaws open | 0-2 | 50040 | 0 |
| A /Akt | 1 | `Spannfutter: Nachspannzeit [20 msec]` | chuck: re-clamping time [20 ms] | 0-100 | 50041 | 0 |
| C | 2 | `Spannertyp`: 0 = `Hydraulisch, Hubbegrenzung, Sicherheitsberos, Mittelstellungsventile (Standard)`; 1 = `Hydraulisch, ohne Hubbegrenzung, Rastventile` | clamp type: 0 = hydraulic standard; 1 = hydraulic, no stroke limit, detent valves | 0-10 | 50042 | 0 |
| C | 2 | `Schwenkfuttertyp`: 0 = `kein Schwenkfutter montiert`; 1 = `AXN-Schwenkfuttertyp der Fa. Autoblock mit 4 Positionen` | swivel chuck type: 0 = none; 1 = Autoblock AXN, 4 positions | 0-10 | 50043 | 0 |
| C | 2 | `Maximale Spannfutterschwenkdrehzahl` [Upm] | maximum chuck swivel speed [rpm] | 0-65535 | 50044 | 0 |
| D | 3 | `Spindel montiert` | spindle fitted | 0-1 | 50045 | 1 (printed x in CTX twin column) |
| D | 3 | `Spindelklemmung montiert` | spindle clamp fitted | 0-1 | 50046 | 0 |
| D | 3 | `Klemmung lösen Zeit [1/50sec]` | clamp release time [1/50 s] | 0-255 | 50047 | 0 |
| D | 3 | `Klemmung klemmen Zeit [1/50sec]` | clamp engage time [1/50 s] | 0-255 | 50048 | 0 |
| D | 3 | `Typ C-Achse`: 0 = `keine C-Achse montiert`, 1 = `C - Achse mit Andruckrolle`, 2 = `C - Achse ohne Andruckrolle` | C-axis type: 0 = none, 1 = with pressure roller, 2 = without pressure roller | 0-10 | 50049 | 0 |
| D | 3 | `Spindel: Verzögerungszeit 1 [100 msec]` (`HW-Spindelfreigabe --> Reglerfreigabe Steller EIN`) | spindle: delay time 1 [100 ms] (HW spindle enable -> controller enable drive ON) | 0-255 | 50050 | 1 (printed x in CTX twin column) |
| D | 4 | `Getriebetyp`: 0 = `kein Getriebe montiert`, 1 = `ZF: 2-Stufen elektrisch betätigt` | gearbox type: 0 = none, 1 = ZF 2-stage electrical | 0-10 | 50051 | 0 |

#### Parameter 025: `Spindel 3, Spannfutter 3, Getriebe 3` (spindle 3, chuck 3, gearbox 3) (pages 9-11, 9-12)

Handwritten values here are written in the CTX 400/500/600 column, between the printed CTX 200 and CTX twin "x" marks.

| Class | Dialog box | Parameter (German) / values | English | Range | K-Nr. | CTX 200 | CTX 400/500/600 | CTX twin | Handwritten |
|---|---|---|---|---|---|---|---|---|---|
| A /Akt | 1 | `Spannart`: 1 = `Innenspannen`, 2 = `Aussenspannen`, 3 = `Ausklinkhub` | clamping mode: 1 = internal, 2 = external, 3 = release stroke | 1-3 | 50052 | x | | x | 2 |
| A /Akt | 1 | `Spannfuttertyp`: 1 = `Handbetätigt`, 2 = `Kraftbetätigt` | chuck type: 1 = manual, 2 = power operated | 1-2 | 50053 | x | | x | 2 |
| A /Akt | 1 | `Spannzylinder`: 0 = `kein Spannzylinder`, 1 = `zurück = Backen zufahren`, 2 = `zurück = Backen auffahren` | clamping cylinder: 0 = none, 1 = retract = jaws close, 2 = retract = jaws open | 0-2 | 50054 | x | | x | 1 |
| A /Akt | 1 | `Spannfutter: Nachspannzeit [20 msec]` | chuck: re-clamping time [20 ms] | 0-100 | 50055 | | | x | 1 |
| C | 2 | `Spannertyp`: 0 = `Hydraulisch, Hubbegrenzung, Sicherheitsberos, Mittelstellungsventile (Standard)`; 1 = `Hydraulisch, ohne Hubbegrenzung, Rastventile` | clamp type: 0 = hydraulic standard; 1 = hydraulic, no stroke limit, detent valves | 0-10 | 50056 | | | | 0 [uncertain, could be 6] |
| C | 2 | `Schwenkfuttertyp`: 0 = `kein Schwenkfutter montiert`; 1 = `AXN-Schwenkfuttertyp der Fa. Autoblock mit 4 Positionen` | swivel chuck type: 0 = none; 1 = Autoblock AXN, 4 positions | 0-10 | 50057 | | | | 0 |
| C | 2 | `Maximale Spannfutterschwenkdrehzahl` [Upm] | maximum chuck swivel speed [rpm] | 0-65535 | 50058 | | | | 0 |
| D | 3 | `Spindel montiert` | spindle fitted | 0-1 | 50059 | x | | | 1 |
| D | 3 | `Spindelklemmung montiert` | spindle clamp fitted | 0-1 | 50060 | | | x | 0 |
| D | 3 | `Klemmung lösen Zeit [1/50sec]` | clamp release time [1/50 s] | 0-255 | 50061 | | | x | 0 |
| D | 3 | `Klemmung klemmen Zeit [1/50sec]` | clamp engage time [1/50 s] | 0-255 | 50062 | | | x | 0 |
| D | 3 | `Typ C-Achse`: 0 = `keine C-Achse montiert`, 1 = `C - Achse mit Andruckrolle`, 2 = `C - Achse ohne Andruckrolle` | C-axis type: 0 = none, 1 = with pressure roller, 2 = without pressure roller | 0-10 | 50063 | x | | x | 2 |
| D | 3 | `Spindel: Verzögerungszeit 1 [100 msec]` (`HW-Spindelfreigabe --> Reglerfreigabe Steller EIN`) | spindle: delay time 1 [100 ms] | 0-255 | 50064 | | | x | 3 |
| D | 4 | `Getriebetyp`: 0 = `kein Getriebe montiert`, 1 = `ZF: 2-Stufen elektrisch betätigt` | gearbox type: 0 = none, 1 = ZF 2-stage electrical | 0-10 | 50065 | | | | 0 |

#### Parameters 026 to 030: `Steuer- u. Werkzeugträgerpara. Schlitten1` ... `Schlitten5` (control and tool-carrier parameters, slide 1 ... slide 5) (pages 9-13 to 9-17)

The five tables are identical in content; only the K numbers and the marks differ. Parameter 026 = slide 1 (page 9-13), 027 = slide 2 (9-14), 028 = slide 3 (9-15), 029 = slide 4 (9-16), 030 = slide 5 (9-17).

Value lists used in these tables:
- `Vorschub: Überrastkupplungstyp` (feed: overload/slip-clutch type): 0 = `keine Rastkupplung` (no detent clutch), 1 = `Typ 1 (Fa. Meyer)` (type 1, Meyer).
- `Revolvertyp` (turret type): 1 = `Multifix`; 2 = `Sauter, Kurvenrollengest. Baureihe 200` (Sauter, cam-roller indexed, series 200); 3 = `Sauter, Kurvenrollengest. Baureihe 500` (Sauter, cam-roller, series 500); 4 = `Duplomatic Revolver Baureihe BSV-N` (Duplomatic turret, series BSV-N); 5 = `Sauter-Servo-Revolver` (Sauter servo turret); 6 = `Sauter, Kurvenrollengest. Baureihe 500 mit Dahlandermotor` (Sauter cam-roller series 500 with Dahlander (pole-changing) motor).

| Class | Dialog box | Parameter (German) | English | Range | K-Nr. slide 1 (026) | slide 2 (027) | slide 3 (028) | slide 4 (029) | slide 5 (030) |
|---|---|---|---|---|---|---|---|---|---|
| D | 1 | `Schlitten montiert` | slide fitted | 0-1 | 50066 | 50073 | 50080 | 50087 | 50094 |
| D | 1 | `Vorschub: Verzögerungszeit 1 [20 msec]` (`Reglerfreigabe Steller EIN --> Bremse lösen`) | feed: delay time 1 [20 ms] (controller enable drive ON -> release brake) | 0-255 | 50067 | 50074 | 50081 | 50088 | 50095 |
| D | 1 | `Vorschub: Verzögerungszeit 2 [20 msec]` (`Bremse ein --> Reglerfreigabe Steller AUS`) | feed: delay time 2 [20 ms] (brake on -> controller enable drive OFF) | 0-255 | 50068 | 50075 | 50082 | 50089 | 50096 |
| D | 1 | `Vorschub: Verzögerungszeit 3 [100 msec]` (`HW-Vorschubfreigabe --> Reglerfreigabe Steller EIN`) | feed: delay time 3 [100 ms] (hardware feed enable -> controller enable drive ON) | 0-255 | 50069 | 50076 | 50083 | 50090 | 50097 |
| C | 1 | `Vorschub: Überrastkupplungstyp` (0/1, see above) | feed: slip-clutch type | 0-10 | 50070 | 50077 | 50084 | 50091 | 50098 |
| D | 2 | `Revolvertyp` (1-6, see above) | turret type | 1-20 | 50071 | 50078 | 50085 | 50092 | 50099 |
| D | 2 | `Anzahl Werkzeugplätze` | number of tool stations | 1-16 | 50072 | 50079 | 50086 | 50093 | 50100 |

Applicability marks and handwritten values:

| K-Nr. | Parameter | CTX 200 | CTX 400/500/600 | CTX twin | Handwritten |
|---|---|---|---|---|---|
| 50066 | slide 1 fitted | | | | 1 |
| 50067 | slide 1 delay 1 | | x | | 0 |
| 50068 | slide 1 delay 2 | | x | | 0 |
| 50069 | slide 1 delay 3 | | | x | 1 |
| 50070 | slide 1 slip-clutch type | | | x | 1 |
| 50071 | slide 1 turret type | x | x | x | 5 (Sauter servo turret); a "0" written below it [uncertain] |
| 50072 | slide 1 number of tool stations | x | x | x | 12 |
| 50073 | slide 2 fitted | x | | | - |
| 50076 | slide 2 delay 3 | | | x | - |
| 50077 | slide 2 slip-clutch type | | | x | - |
| 50078 | slide 2 turret type | | | x | - |
| 50079 | slide 2 number of tool stations | | | x | - |

No marks or handwriting for 50074, 50075 and the slide 3 to 5 tables (50080-50100).

Note: the K-numbers 50101-50107 (a parameter 031) do not appear on these pages; the list jumps from parameter 030 (page 9-17) to parameter 032 (page 9-18).

#### Parameter 032: `Eltromatic - Module` (Eltromatic modules) (page 9-18)

All rows: class D, range 0-1, printed "x" for CTX 200, CTX 400/500/600 and CTX twin. `E/A - Modul nn installiert` = I/O module nn installed.

| Dialog box | Parameter (German) | English | K-Nr. |
|---|---|---|---|
| 1 | `E/A - Modul 01 installiert` | I/O module 01 installed | 50108 |
| 1 | `E/A - Modul 02 installiert` | I/O module 02 installed | 50109 |
| 1 | `E/A - Modul 03 installiert` | I/O module 03 installed | 50110 |
| 1 | `E/A - Modul 04 installiert` | I/O module 04 installed | 50111 |
| 1 | `E/A - Modul 05 installiert` | I/O module 05 installed | 50112 |
| 1 | `E/A - Modul 06 installiert` | I/O module 06 installed | 50113 |
| 2 | `E/A - Modul 07 installiert` | I/O module 07 installed | 50114 |
| 2 | `E/A - Modul 08 installiert` | I/O module 08 installed | 50115 |
| 2 | `E/A - Modul 09 installiert` | I/O module 09 installed | 50116 |
| 2 | `E/A - Modul 10 installiert` | I/O module 10 installed | 50117 |
| 2 | `E/A - Modul 11 installiert` | I/O module 11 installed | 50118 |
| 2 | `E/A - Modul 12 installiert` | I/O module 12 installed | 50119 |
| 3 | `E/A - Modul 13 installiert` | I/O module 13 installed | 50120 |
| 3 | `E/A - Modul 14 installiert` | I/O module 14 installed | 50121 |
| 3 | `E/A - Modul 15 installiert` | I/O module 15 installed | 50122 |
| 3 | `E/A - Modul 16 installiert` | I/O module 16 installed | 50123 |

#### Parameter 033: `WKZ-Überwachung` (tool monitoring) (page 9-19)

| Class | Dialog box | Parameter (German) | English | Range | K-Nr. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|---|---|
| D | 1 | `WKZ-Überwachung I: Gerätetyp` | tool monitoring I: device type | 0-5 | 50124 | x | x | x |
| D | 1 | `WKZ-Überwachung I: Ausgabeformat` | tool monitoring I: output format | 0-17 | 50125 | x | x | x |
| D | 1 | `WKZ-Überwachung I: Strategie` | tool monitoring I: strategy | 0-15 | 50126 | x | x | x |
| D | 2 | `WKZ-Überwachung II: Gerätetyp` | tool monitoring II: device type | 0-5 | 50127 | | | x |
| D | 2 | `WKZ-Überwachung II: Ausgabeformat` | tool monitoring II: output format | 0-17 | 50128 | | | x |
| D | 2 | `WKZ-Überwachung II: Strategie` | tool monitoring II: strategy | 0-15 | 50129 | | | x |

#### Parameter 034: `Schottblech` (partition plate / bulkhead) (page 9-19)

| Class | Dialog box | Parameter (German) | English | Range | K-Nr. | CTX twin |
|---|---|---|---|---|---|---|
| D | 1 | `Schottblech montiert` | partition plate fitted | 0-1 | 50130 | x |
| C | 1 | `Schottblech: Maximalzeit [sec]` (`Maximalzeit für Funktionsausführung`) | partition plate: maximum time [s] (maximum time for executing the function) | 0-255 | 50131 | x |

#### Parameter 035: `Mengenimpuls` (quantity / count pulse) (page 9-19)

| Class | Dialog box | Parameter (German) / values | English | Range | K-Nr. | CTX twin |
|---|---|---|---|---|---|---|
| C | 1 | `Mengenimpulsausgabe`: 0 = `Impulse von AG1 und AG2 direkt`; 1 = `Impulse nur von AG1`; 2 = `Impulse nur von AG2`; 3 = `Impulse von AG1 und AG2 zeitlich versetzt` | count-pulse output: 0 = pulses from AG1 and AG2 directly; 1 = pulses from AG1 only; 2 = pulses from AG2 only; 3 = pulses from AG1 and AG2 offset in time | 0-10 | 50132 | x |

(AG1/AG2 = `Automatikgruppe`/machining group 1 and 2, i.e. the two sides of the twin [uncertain expansion; the abbreviation is not explained on these pages].)

#### Parameter 036: `Lünetten` (steady rests) (page 9-20)

All rows: class C/Akt, range 0-10, printed x for CTX 200, CTX 400/500/600 and CTX twin. Values: 0 = `nicht montiert` (not fitted), 1 = `fest auf Bett` (fixed on bed), 2 = `mittschleppend auf Bett` (dragged along on bed, i.e. travelling steady).

| Dialog box | Parameter (German) | English | K-Nr. |
|---|---|---|---|
| 1 | `Typ Lünette 1` | type steady rest 1 | 50133 |
| 1 | `Typ Lünette 2` | type steady rest 2 | 50134 |
| 1 | `Typ Lünette 3` | type steady rest 3 | 50135 |
| 1 | `Typ Lünette 4` | type steady rest 4 | 50136 |

#### Parameter 037: `ISM - Temperaturregelung` (ISM temperature control) (pages 9-20, 9-21)

All rows: class D, printed x for CTX twin only. (ISM = integrated spindle motor [uncertain expansion].)

| Dialog box | Parameter (German) | English | Range | K-Nr. |
|---|---|---|---|---|
| 1 | `Temperaturregelung montiert` | temperature control fitted | 0-1 | 50137 |
| 1 | `Anzahl Werte zur Mittelwertbildung` | number of values for averaging | 0-255 | 50138 |
| 1 | `Max. Umgebungstemperatur in °C` | max. ambient temperature in °C | 0-255 | 50139 |
| 1 | `Sollwert Kühlwassertemperatur in °C` | set point cooling-water temperature in °C | 0-255 | 50140 |
| 1 | `Hysterese Kühlwassertemperatur in °C` | hysteresis cooling-water temperature in °C | 0-255 | 50141 |
| 2 | `Sollwert Wicklungstemperatur in °C` | set point winding temperature in °C | 0-255 | 50142 |
| 2 | `Hysterese Wicklungstemperatur in °C` | hysteresis winding temperature in °C | 0-255 | 50143 |
| 2 | `Sollwert Lagertemperatur vorn in °C` | set point front bearing temperature in °C | 0-255 | 50144 |
| 2 | `Hysterese Lagetemperatur vorn in °C` [sic] | hysteresis front bearing temperature in °C | 0-255 | 50145 |
| 2 | `Sollwert Lagertemperatur hinten in °C` | set point rear bearing temperature in °C | 0-255 | 50146 |
| 2 | `Hysterese Lagertemperatur hinten in °C` | hysteresis rear bearing temperature in °C | 0-255 | 50147 |
| 3 | `MAX Wicklungstemperatur in °C` | MAX winding temperature in °C | 0-255 | 50148 |
| 3 | `MAXMAX Wicklungstemperatur in °C` | MAXMAX winding temperature in °C | 0-255 | 50149 |
| 3 | `MAX Lagertemperatur vorn in °C` | MAX front bearing temperature in °C | 0-255 | 50150 |
| 3 | `MAXMAX Lagertemperatur vorn in °C` | MAXMAX front bearing temperature in °C | 0-255 | 50151 |
| 3 | `MAX Lagertemperatur hinten in °C` | MAX rear bearing temperature in °C | 0-255 | 50152 |
| 3 | `MAXMAX Lagertemperatur hinten in °C` | MAXMAX rear bearing temperature in °C | 0-255 | 50153 |

#### Parameter 038: `Reitstock, Pinole` (tailstock, quill) (page 9-21)

| Class | Dialog box | Parameter (German) / values | English | Range | K-Nr. | CTX 200 | CTX 400/500/600 | CTX twin | Handwritten |
|---|---|---|---|---|---|---|---|---|---|
| D | 1 | `Reitstocktyp`: 0 = `Feststehender Reitstock`; 1 = `Hydraulischer Reitstock Typ 1 (über Zylinder verfahrbar)`; 2 = `Hydraulischer Reitstock Typ 2 (Festanschlag)`; 3 = `Hydraulischer Reitstock Typ 3 (Teach-In)` | tailstock type: 0 = fixed tailstock; 1 = hydraulic tailstock type 1 (movable by cylinder); 2 = hydraulic tailstock type 2 (fixed stop); 3 = hydraulic tailstock type 3 (teach-in) | 0-10 | 50154 | x | x | x | 0 [uncertain, faint] |
| D | 1 | `Typ Pinole`: 0 = `keine Pinole montiert`; 1 = `Hydraulische Pinole` | quill type: 0 = no quill fitted; 1 = hydraulic quill | 0-10 | 50155 | x | x | x | 1 [uncertain, faint] |

#### Parameter 039: `Materialzuführungen / -abführungen` (material feeding / removal) (pages 9-22 to 9-25)

Handwritten at top right of page 9-23 again: "037" / "2089" [uncertain].

Value list `Typ Abholeinrichtung` (type of unloading/pick-up device): 0 = `nicht montiert` (not fitted); 1 = `Abholeinrichtung Typ 1 (CTX xxx)`; 2 = `Abholeinrichtung Typ 2 (CTX twin)`; 3 = `Abholeinrichtung Typ 3 (CTX500 4A)`; 4 = `Abholeinrichtung Typ 4 (CTX200)`.

Value list `Typ Handhabung` (handling type, HH = `Handhabung`): 0 = `nicht montiert`; 1 = `HH = Promot-Portal` (Promot gantry); 2 = `HH = Roboter einfach` (simple robot); 3 = `HH = Felsomat-Portal` (Felsomat gantry); 4 = `HH = Roboter einfach für TWIN` (simple robot for TWIN). (On page 9-23 the value-4 line is underlined by hand.)

Value list `Typ Ladeklappe` (loading flap type): 0 = `nicht montiert`; 1 = `Ladeklappe Typ 1`.

Value list `Typ Materialzuführung` (material feed type): 0 = `nicht montiert`; 1 = `Stangenlader` (bar loader); 2 = `Stangenvorschub` (bar feed); 3 = `Stangenlader mit CE` (bar loader with CE); 4 = `Stangenvorschub mit CE` (bar feed with CE); 5 = `Kurzteilllademagazin (Sameca, Monosam)` (short-part loading magazine).

Bit list `Handhabung I/II: Achssperren bei HH im Arbeitsraum` (handling I/II: axis interlocks while the handling device is in the working area), UNS32, one bit per axis, each `<axis>-Achse sperren` (lock <axis> axis):

| Bit | Axis | Bit | Axis | Bit | Axis |
|---|---|---|---|---|---|
| 0 | X1 | 10 | B2 | 20 | Y4 |
| 1 | Z1 | 11 | C2 | 21 | A4 |
| 2 | Y1 | 12 | X3 | 22 | B4 |
| 3 | A1 | 13 | Z3 | 23 | C4 |
| 4 | B1 | 14 | Y3 | 24 | X5 |
| 5 | C1 | 15 | A3 | 25 | Z5 |
| 6 | X2 | 16 | B3 | 26 | Y5 |
| 7 | Z2 | 17 | C3 | 27 | A5 |
| 8 | Y2 | 18 | X4 | 28 | B5 |
| 9 | A2 | 19 | Z4 | 29 | C5 |

| Class | Dialog box | Parameter (German) | English | Range | K-Nr. | CTX 200 | CTX 400/500/600 | CTX twin | Handwritten |
|---|---|---|---|---|---|---|---|---|---|
| D | 1 | `Typ Abholeinrichtung I` (list above) | type of unloading device I | 0-10 | 50156 | x | x | x | 0 |
| C | 1 | `Abholeinrichtung I: Zeit 1 [sec]` (`Maximalzeit für Funktionsausführung`) | unloading device I: time 1 [s] (max. time for executing the function) | 0-25111 [sic, probably 0-255] | 50157 | x | x | x | 0 |
| A/Akt | 1 | `Abholeinrichtung I: Zeit 2 [100 msec]` (`Zeit für Klappe geöffnet`) | unloading device I: time 2 [100 ms] (time flap open) | 0-255 | 50158 | x | x | x | 0 |
| A/Akt | 1 | `Abholeinrichtung I: Zeit 3 [sec]` (`Zeit Förderband Ein`; `Wert = 0 ==> keine Zeitbegrenzung`) | unloading device I: time 3 [s] (conveyor-belt on time; value 0 => no time limit) | 0-255 | 50159 | x | | x | 0 |
| D | 2 | `Typ Abholeinrichtung II` (list above) | type of unloading device II | 0-10 | 50160 | | | x | 0 |
| C | 2 | `Abholeinrichtung II: Zeit 1 [sec]` (`Maximalzeit für Funktionsausführung`) | unloading device II: time 1 [s] | 0-255 | 50161 | | | x | 0 |
| A/Akt | 2 | `Abholeinrichtung II: Zeit 2 [100 msec]` (`Zeit für Klappe geöffnet`) | unloading device II: time 2 [100 ms] (flap open) | 0-255 | 50162 | | | x | 6 [uncertain, could be 0] |
| A/Akt | 2 | `Abholeinrichtung II: Zeit 3 [sec]` (`Zeit Förderband Ein`; 0 ==> no time limit) | unloading device II: time 3 [s] (conveyor on) | 0-255 | 50163 | | | x | 0 |
| D | 3 | `Typ Handhabung I` (list above) | handling type I | 0-10 | 50164 | x | x | x | 4 (simple robot for TWIN) |
| D | 3 | `Typ Ladeklappe I` (list above) | loading flap type I | 0-10 | 50165 | x | x | x | 0 |
| D | 3 | `Handhabung I: Achssperren bei HH im Arbeitsraum` (bit list above) | handling I: axis interlocks while HH in working area | UNS32 | 50166 | | | x | 0 |
| D | 3 | `Typ Handhabung II` (list above) | handling type II | 0-10 | 50167 | | | x | 4 |
| D | 3 | `Typ Ladeklappe II` (list above) | loading flap type II | 0-10 | 50168 | | | x | 0 [uncertain, could be 6] |
| D | 3 | `Handhabung II: Achssperren bei HH im Arbeitsraum` (bit list above) | handling II: axis interlocks while HH in working area | UNS32 | 50169 | | | x | 0 |
| C | 4 | `Typ Materialzuführung I` (list above) | material feed type I | 0-10 | 50170 | x | x | x | 0 |
| C | 4 | `Typ Materialzuführung II` (list above) | material feed type II | 0-10 | 50171 | | | x | 0 |

#### Parameter 040: `Schutzhauben` (protective hoods / guard doors) (pages 9-25, 9-26)

(On page 9-25 the heading "Parameter 040: Schutzhauben" is crossed through by a hand-drawn line.)

Value list `Typ Schutzhaube` (guard type): 0 = `Typ 1 (Hand bedient)` (type 1, hand operated); 1 = `Typ 2 (autom., elektr., mit Sicherheitsleiste)` (type 2, automatic, electric, with safety edge); 2 = `Typ 3 (autom., elektr., ohne Sicherheitsleiste)` (type 3, automatic, electric, without safety edge); 3 = `Typ 4 (autom., pneum., mit Sicherheitsleiste)` (type 4, automatic, pneumatic, with safety edge); 4 = `Typ 5 (autom., pneum., ohne Sicherheitsleiste)` (type 5, automatic, pneumatic, without safety edge); 5 = `Typ 6 (autom., pneum. Typ 2 mit Sicherheitsleiste)` (type 6, automatic, pneumatic type 2, with safety edge); 6 = `Typ 7 (autom., pneum. Typ 2 ohne Sicherheitsleiste)` (type 7, automatic, pneumatic type 2, without safety edge).

| Class | Dialog box | Parameter (German) | English | Range | K-Nr. | CTX 200 | CTX 400/500/600 | CTX twin | Handwritten |
|---|---|---|---|---|---|---|---|---|---|
| D | 1 | `Typ Schutzhaube I` (list above) | guard type I | 0-255 | 50172 | | x | x | 6 (type 7, automatic pneumatic type 2 without safety edge) |
| A /Akt | 1 | `Schutzhaube I automatisch entriegeln` | guard I unlock automatically | 0-1 | 50173 | x | x | x | 1 |
| A /Akt | 1 | `Schutzhaube I Auto-Zyklus-Start aktiv` | guard I auto cycle start active | 0-1 | 50174 | x | x | x | "1 ->" (changed from 1, new value cut off at page edge; probably 0 as for 50179) |
| D | 1 | `Zeitüberwachung Schutzhaube I` (`Wert = 0 ==> keine Zeitbegrenzung`; `Wert # 0 ==> Zeitüberwachung in Sekunden`) | time monitoring guard I (0 => no time limit; not 0 => monitoring time in seconds) | 0-2000 | 50175 | x | x | x | 0 |
| D | 1 | `Schutzhaube I: Verzögerungszeit 1 [100 msec]` (`Reglerfreigaben Steller AUS --> Haube entriegeln`) | guard I: delay time 1 [100 ms] (controller enables drives OFF -> unlock guard) | 0-255 | 50176 | | | x | 6 |
| D | 2 | `Typ Schutzhaube II` (list above) | guard type II | 0-255 | 50177 | | | x | 6 |
| A /Akt | 2 | `Schutzhaube II automatisch entriegeln` | guard II unlock automatically | 0-1 | 50178 | | | x | 1 |
| A /Akt | 2 | `Schutzhaube II Auto-Zyklus-Start aktiv` | guard II auto cycle start active | 0-1 | 50179 | | | x | 1 -> 0 (changed by hand) |
| D | 2 | `Zeitüberwachung Schutzhaube II` (0 ==> no time limit; # 0 ==> seconds) | time monitoring guard II | 0-2000 | 50180 | | | x | 0 |
| D | 2 | `Schutzhaube II: Verzögerungszeit 1 [100 msec]` (`Reglerfreigaben Steller AUS --> Haube entriegeln`) | guard II: delay time 1 [100 ms] (drives OFF -> unlock guard) | 0-255 | 50181 | | | x | 6 |

Handwritten note at the bottom of page 9-26, in Finnish: "Ovi muutettu, että ei anna zyklus startia. 20.6-95" [uncertain reading] = "Door changed so that it does not give cycle start. 20.6.95" (matches the 1 -> 0 change of 50174/50179: closing the guard no longer starts the cycle automatically).

#### Parameter 041: `Meß - Systeme` (measuring systems) (page 9-27)

Values: 0 = `nicht vorhanden` (not present), 1 = `Typ 1` (for workpiece measuring: `Typ 1 (MARPOSS)`).

| Class | Dialog box | Parameter (German) | English | Range | K-Nr. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|---|---|
| C | 1 | `Typ Postprozeßmessen I` | type post-process measuring I | 0-10 | 50182 | x | x | |
| C | 1 | `Typ Werkzeugmessen I` | type tool measuring I | 0-10 | 50183 | x | x | x |
| C | 1 | `Typ Werkstückmessen I` (1 = Typ 1 (MARPOSS)) | type workpiece measuring I (1 = type 1, Marposs) | 0-10 | 50184 | | x | |
| C | 2 | `Typ Postprozeßmessen II` | type post-process measuring II | 0-10 | 50185 | | | |
| C | 2 | `Typ Werkzeugmessen II` | type tool measuring II | 0-10 | 50186 | | | x |
| C | 2 | `Typ Werkstückmessen II` (1 = Typ 1 (MARPOSS)) | type workpiece measuring II (Marposs) | 1-10 [sic] | 50187 | | | |

#### Parameter 042: `Schmierungen` (lubrication systems) (page 9-28)

| Class | Dialog box | Parameter (German) | English | Range | K-Nr. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|---|---|
| D | 1 | `Zusätzliche Meldungen für Schmierung I` | additional messages for lubrication I | 0-10 | 50188 | x | x | x |
| C | 1 | `Intervallzeit für Schmierimpuls I [min]` | interval time for lubrication pulse I [min] | 0-255 | 50189 | | | x |
| C | 1 | `Max. Unterdrückungszeit für Schmierimpuls I [min]` | max. suppression time for lubrication pulse I [min] | 0-255 | 50190 | | | x |
| D | 2 | `Zusätzliche Meldungen für Schmierung II` | additional messages for lubrication II | 0-10 | 50191 | | | x |
| C | 2 | `Intervallzeit für Schmierimpuls II [min]` | interval time for lubrication pulse II [min] | 0-255 | 50192 | | | x |
| C | 2 | `Max. Unterdrückungszeit für Schmierimpuls II [min]` | max. suppression time for lubrication pulse II [min] | 0-255 | 50193 | | | x |

#### Parameter 043: `Hydrauliken` (hydraulic systems) (page 9-29)

| Class | Dialog box | Parameter (German) | English | Range | K-Nr. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|---|---|
| D | 1 | `Zusätzliche Meldungen für Hydraulik I` | additional messages for hydraulics I | 0-10 | 50194 | x | x | x |
| D | 1 | `Hydraulik I mit Antrieben abschalten` | switch off hydraulics I together with the drives | 0-10 | 50195 | x | x | x |
| D | 2 | `Zusätzliche Meldungen für Hydraulik II` | additional messages for hydraulics II | 0-10 | 50196 | | | x |
| D | 2 | `Hydraulik II mit Antrieben abschalten` | switch off hydraulics II together with the drives | 0-10 | 50197 | | | x |

#### Parameter 044: `Späneförderer` (chip conveyor) (page 9-29)

| Class | Dialog box | Parameter (German) | English | Range | K-Nr. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|---|---|
| D | 1 | `Späneförderer I montiert` | chip conveyor I fitted | 0-1 | 50198 | x | x | x |
| A/Akt | 1 | `Späneförderer I: Intervallzeit EIN [sec]` (`Wert = 0 ==> kein Intervallbetrieb`) | chip conveyor I: interval time ON [s] (0 => no interval operation) | 0-1200 | 50199 | | | |
| A/Akt | 1 | `Späneförderer I: Intervallzeit AUS [sec]` (`Wert = 0 ==> kein Intervallbetrieb`) | chip conveyor I: interval time OFF [s] (0 => no interval operation) | 0-1200 | 50200 | | | |
| D | 2 | `Späneförderer II montiert` | chip conveyor II fitted | 0-1 | 50201 | | | |
| A/Akt | 2 | `Späneförderer II: Intervallzeit EIN [sec]` (0 ==> no interval operation) | chip conveyor II: interval time ON [s] | 0-1200 | 50202 | | | |
| A/Akt | 2 | `Späneförderer II: Intervallzeit AUS [sec]` (0 ==> no interval operation) | chip conveyor II: interval time OFF [s] | 0-1200 | 50203 | | | |

#### Parameter 045: `Komfort-Anwenderrelais` (comfort user relays) (pages 9-30, 9-31)

All rows class C, range 0-3. Values: 0 = `nicht montiert` (not fitted); 1 = `montiert, keine automatische Abschaltung` (fitted, no automatic switch-off); 2 = `montiert, Abschaltung bei Not-Aus` (fitted, switch-off at emergency stop); 3 = `montiert, Abschaltung bei "Schnellstop" (schwerer Fehler, Betriebsartenwechsel, Not-Aus)` (fitted, switch-off at "quick stop" (serious fault, change of operating mode, emergency stop)).

| Dialog box | Parameter (German) | English | K-Nr. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|
| 1 | `Typ Komfort Anwenderrelais 1 bis 4` | type comfort user relays 1 to 4 | 50204 | x | x | x |
| 1 | `Typ Komfort Anwenderrelais 5 bis 8` | type comfort user relays 5 to 8 | 50205 | x | x | x |
| 1 | `Typ Komfort Anwenderrelais 9 bis 12` | type comfort user relays 9 to 12 | 50206 | x | x | x |
| 1 | `Typ Komfort Anwenderrelais 13 bis 16` | type comfort user relays 13 to 16 | 50207 | x | x | x |
| 2 | `Typ Komfort Anwenderrelais 17 bis 20` | type comfort user relays 17 to 20 | 50208 | | | |
| 2 | `Typ Komfort Anwenderrelais 21 bis 24` | type comfort user relays 21 to 24 | 50209 | | | |
| 2 | `Typ Komfort Anwenderrelais 25 bis 28` | type comfort user relays 25 to 28 | 50210 | | | |
| 2 | `Typ Komfort Anwenderrelais 29 bis 32` | type comfort user relays 29 to 32 | 50211 | | | |

#### Parameter 046: `Testfunktion` (test function) (page 9-32)

All rows class D.

| Dialog box | Parameter (German) | English | Range | K-Nr. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|---|
| 1 | `Emulatorbetrieb` | emulator operation | 0-1 | 50212 | x | x | x |
| 1 | `Fehlertexte testen` | test error texts | 0-1 | 50213 | x | x | x |
| 1 | `Test 3 (Reserve)` | test 3 (reserve) | 0-255 | 50214 | | | |
| 1 | `Test 4 (Reserve)` | test 4 (reserve) | 0-255 | 50215 | | | |
| 1 | `Test 5 (Reserve)` | test 5 (reserve) | 0-255 | 50216 | | | |
| 1 | `Test 6 (Reserve)` | test 6 (reserve) | 0-255 | 50217 | | | |
| 2 | `Test 7 (Reserve)` | test 7 (reserve) | 0-255 | 50218 | | | |
| 2 | `Test 8 (Reserve)` | test 8 (reserve) | 0-255 | 50219 | | | |
| 2 | `Test 9 (Reserve)` | test 9 (reserve) | 0-255 | 50220 | | | |
| 2 | `Test 10 (Reserve)` | test 10 (reserve) | 0-255 | 50221 | | | |
| 2 | `Test 11 (Reserve)` | test 11 (reserve) | 0-255 | 50222 | | | |
| 2 | `Test 12 (Reserve)` | test 12 (reserve) | 0-255 | 50223 | | | |

#### Parameter 047: `Reseve - Parameter` [sic] (reserve parameters) (pages 9-33 to 9-42)

All rows class C (one printed as "C :"), range UNS32 unless noted. "unsigned long Nummer nnnnn" = unsigned long number nnnnn.

| Dialog box | Parameter (German) | English | Range | K-Nr. | CTX 200 | CTX 400/500/600 | CTX twin |
|---|---|---|---|---|---|---|---|
| 1 | `unsigned long Nummer 50225` [sic, number printed as 50225] `reserviert für: Handling der D-Freigaben` | reserved for: handling of the D releases (class-D enables) | UNS32 | 50224 | x | x | x |
| 1 | `unsigned long Nummer 50225` `reserviert für: Massebruchüberwachung aktiv` | reserved for: ground-break (broken earth) monitoring active | UNS32 | 50225 | x | x | x |
| 1 | `unsigned long Nummer 50226` `Kühlmittel: Strömungsüberwachung montiert` | coolant: flow monitoring fitted | UNS32, 0-1 | 50226 | | | x |
| 1 | `unsigned long Nummer 50227` `Kühlmittel: Strömungsüberwachung Entprellzeit Überwachungsstart / -ende [100 msec]` | coolant: flow monitoring debounce time at monitoring start / end [100 ms] | UNS32, 0-65535 | 50227 | | | x |
| 1 | `unsigned long Nummer 50228` `Kühlmittel: Strömungsüberwachung Entprellzeit während der Überwachung [100 msec]` | coolant: flow monitoring debounce time during monitoring [100 ms] | UNS32, 0-65535 | 50228 | | | x |
| 1 | `unsigned long Nummer 50229` `Werkstückanlagekontrolle 1` | workpiece seating control 1 | UNS32, 0-1 | 50229 | | | x |
| 2 | `unsigned long Nummer 50230` `Werkstückanlagekontrolle 2` | workpiece seating control 2 | UNS32, 0-1 | 50230 | | | x |
| 2 | `unsigned long Nummer 50231` | unsigned long number 50231 (reserve) | UNS32 | 50231 | | | |
| 2 | `unsigned long Nummer 50232` | unsigned long number 50232 (reserve) | UNS32 | 50232 | | | |
| 2 | `unsigned long Nummer 50233` | unsigned long number 50233 (reserve) | UNS32 | 50233 | | | |
| 2 | `unsigned long Nummer 50234` | unsigned long number 50234 (reserve) | UNS32 | 50234 | | | |
| 2 | `unsigned long Nummer 50235` | unsigned long number 50235 (reserve) | UNS32 | 50235 | | | |
| 3 | `unsigned long Nummer 50236` | unsigned long number 50236 (reserve) | UNS32 | 50236 | | | |
| 3 | `unsigned long Nummer 50237` | unsigned long number 50237 (reserve) | UNS32 | 50237 | | | |
| 3 | `unsigned long Nummer 50238` | unsigned long number 50238 (reserve) | UNS32 | 50238 | | | |
| 3 | `unsigned long Nummer 50239` | unsigned long number 50239 (reserve) | UNS32 | 50239 | | | |
| 3 | `unsigned long Nummer 50240` | unsigned long number 50240 (reserve) | UNS32 | 50240 | | | |
| 3 | `unsigned long Nummer 50241` | unsigned long number 50241 (reserve) | UNS32 | 50241 | | | |
| 4 | `unsigned long Nummer 50242` | unsigned long number 50242 (reserve) | UNS32 | 50242 | | | |
| 4 | `unsigned long Nummer 50243` | unsigned long number 50243 (reserve) | UNS32 | 50243 | | | |
| 4 | `unsigned long Nummer 50244` | unsigned long number 50244 (reserve) | UNS32 | 50244 | | | |
| 4 | `unsigned long Nummer 50245` | unsigned long number 50245 (reserve) | UNS32 | 50245 | | | |
| 4 | `unsigned long Nummer 50246` | unsigned long number 50246 (reserve) | UNS32 | 50246 | | | |
| 4 | `unsigned long Nummer 50247` | unsigned long number 50247 (reserve) | UNS32 | 50247 | | | |
| 5 | `unsigned long Nummer 50248` | unsigned long number 50248 (reserve) | UNS32 | 50248 | | | |
| 5 | `unsigned long Nummer 50249` | unsigned long number 50249 (reserve) | UNS32 | 50249 | | | |
| 5 | `unsigned long Nummer 50250` | unsigned long number 50250 (reserve) | UNS32 | 50250 | | | |
| 5 | `unsigned long Nummer 50251` | unsigned long number 50251 (reserve) | UNS32 | 50251 | | | |
| 5 | `unsigned long Nummer 50252` | unsigned long number 50252 (reserve) | UNS32 | 50252 | | | |
| 5 | `unsigned long Nummer 50253` | unsigned long number 50253 (reserve) | UNS32 | 50253 | | | |
| 6 | `unsigned long Nummer 50254` | unsigned long number 50254 (reserve) | UNS32 | 50254 | | | |
| 6 | `unsigned long Nummer 50255` | unsigned long number 50255 (reserve) | UNS32 | 50255 | | | |
| 6 | `unsigned long Nummer 50256` | unsigned long number 50256 (reserve) | UNS32 | 50256 | | | |
| 6 | `unsigned long Nummer 50257` | unsigned long number 50257 (reserve) | UNS32 | 50257 | | | |
| 6 | `unsigned long Nummer 50258` | unsigned long number 50258 (reserve) | UNS32 | 50258 | | | |
| 6 | `unsigned long Nummer 50259` | unsigned long number 50259 (reserve) | UNS32 | 50259 | | | |
| 7 | `unsigned long Nummer 50260` | unsigned long number 50260 (reserve) | UNS32 | 50260 | | | |
| 7 | `unsigned long Nummer 50261` | unsigned long number 50261 (reserve) | UNS32 | 50261 | | | |
| 7 | `unsigned long Nummer 50262` | unsigned long number 50262 (reserve) | UNS32 | 50262 | | | |
| 7 | `unsigned long Nummer 50263` | unsigned long number 50263 (reserve) | UNS32 | 50263 | | | |
| 7 | `unsigned long Nummer 50264` | unsigned long number 50264 (reserve) | UNS32 | 50264 | | | |
| 7 | `unsigned long Nummer 50265` | unsigned long number 50265 (reserve) | UNS32 | 50265 | | | |
| 8 | `unsigned long Nummer 50266` | unsigned long number 50266 (reserve) | UNS32 | 50266 | | | |
| 8 | `unsigned long Nummer 50267` | unsigned long number 50267 (reserve) | UNS32 | 50267 | | | |
| 8 | `unsigned long Nummer 50268` | unsigned long number 50268 (reserve) | UNS32 | 50268 | | | |
| 8 | `unsigned long Nummer 50269` | unsigned long number 50269 (reserve) | UNS32 | 50269 | | | |
| 8 | `unsigned long Nummer 50270` | unsigned long number 50270 (reserve) | UNS32 | 50270 | | | |
| 8 | `unsigned long Nummer 50271` | unsigned long number 50271 (reserve) | UNS32 | 50271 | | | |
| 9 | `unsigned long Nummer 50272` | unsigned long number 50272 (reserve) | UNS32 | 50272 | | | |
| 9 | `unsigned long Nummer 50273` | unsigned long number 50273 (reserve) | UNS32 | 50273 | | | |
| 9 | `unsigned long Nummer 50274` | unsigned long number 50274 (reserve) | UNS32 | 50274 | | | |
| 9 | `unsigned long Nummer 50275` | unsigned long number 50275 (reserve) | UNS32 | 50275 | | | |
| 9 | `unsigned long Nummer 50276` | unsigned long number 50276 (reserve) | UNS32 | 50276 | | | |
| 9 | `unsigned long Nummer 50277` | unsigned long number 50277 (reserve) | UNS32 | 50277 | | | |
| 10 | `unsigned long Nummer 50278` | unsigned long number 50278 (reserve) | UNS32 | 50278 | | | |
| 10 | `unsigned long Nummer 50279` | unsigned long number 50279 (reserve) | UNS32 | 50279 | | | |
| 10 | `unsigned long Nummer 50280` | unsigned long number 50280 (reserve) | UNS32 | 50280 | | | |
| 10 | `unsigned long Nummer 50281` | unsigned long number 50281 (reserve) | UNS32 | 50281 | | | |
| 10 | `unsigned long Nummer 50282` | unsigned long number 50282 (reserve) | UNS32 | 50282 | | | |
| 10 | `unsigned long Nummer 50283` | unsigned long number 50283 (reserve) | UNS32 | 50283 | | | |
| 11 | `unsigned long Nummer 50284` | unsigned long number 50284 (reserve) | UNS32 | 50284 | | | |
| 11 | `unsigned long Nummer 50285` | unsigned long number 50285 (reserve) | UNS32 | 50285 | | | |
| 11 | `unsigned long Nummer 50286` | unsigned long number 50286 (reserve) | UNS32 | 50286 | | | |
| 11 | `unsigned long Nummer 50287` | unsigned long number 50287 (reserve) | UNS32 | 50287 | | | |
| 11 | `unsigned long Nummer 50288` | unsigned long number 50288 (reserve) | UNS32 | 50288 | | | |
| 11 | `unsigned long Nummer 50289` | unsigned long number 50289 (reserve) | UNS32 | 50289 | | | |
| 12 | `unsigned long Nummer 50290` | unsigned long number 50290 (reserve) | UNS32 | 50290 | | | |
| 12 | `unsigned long Nummer 50291` | unsigned long number 50291 (reserve) | UNS32 | 50291 | | | |
| 12 | `unsigned long Nummer 50292` | unsigned long number 50292 (reserve) | UNS32 | 50292 | | | |
| 12 | `unsigned long Nummer 50293` | unsigned long number 50293 (reserve) | UNS32 | 50293 | | | |
| 12 | `unsigned long Nummer 50294` | unsigned long number 50294 (reserve) | UNS32 | 50294 | | | |
| 12 | `unsigned long Nummer 50295` | unsigned long number 50295 (reserve) | UNS32 | 50295 | | | |
| 13 | `unsigned long Nummer 50296` | unsigned long number 50296 (reserve) | UNS32 | 50296 | | | |
| 13 | `unsigned long Nummer 50297` | unsigned long number 50297 (reserve) | UNS32 | 50297 | | | |
| 13 | `unsigned long Nummer 50298` | unsigned long number 50298 (reserve) | UNS32 | 50298 | | | |
| 13 | `unsigned long Nummer 50299` | unsigned long number 50299 (reserve) | UNS32 | 50299 | | | |
| 13 | `unsigned long Nummer 50300` | unsigned long number 50300 (reserve) | UNS32 | 50300 | | | |
| 13 | `unsigned long Nummer 50301` | unsigned long number 50301 (reserve) | UNS32 | 50301 | | | |
| 14 | `unsigned long Nummer 50302` | unsigned long number 50302 (reserve) | UNS32 | 50302 | | | |
| 14 | `unsigned long Nummer 50303` | unsigned long number 50303 (reserve) | UNS32 | 50303 | | | |
| 14 | `unsigned long Nummer 50304` | unsigned long number 50304 (reserve) | UNS32 | 50304 | | | |
| 14 | `unsigned long Nummer 50305` | unsigned long number 50305 (reserve) | UNS32 | 50305 | | | |
| 14 | `unsigned long Nummer 50306` | unsigned long number 50306 (reserve) | UNS32 | 50306 | | | |
| 14 | `unsigned long Nummer 50307` | unsigned long number 50307 (reserve) | UNS32 | 50307 | | | |
| 15 | `unsigned long Nummer 50308` | unsigned long number 50308 (reserve) | UNS32 | 50308 | | | |
| 15 | `unsigned long Nummer 50309` | unsigned long number 50309 (reserve) | UNS32 | 50309 | | | |
| 15 | `unsigned long Nummer 50310` | unsigned long number 50310 (reserve) | UNS32 | 50310 | | | |
| 15 | `unsigned long Nummer 50311` | unsigned long number 50311 (reserve) | UNS32 | 50311 | | | |
| 15 | `unsigned long Nummer 50312` | unsigned long number 50312 (reserve) | UNS32 | 50312 | | | |
| 15 | `unsigned long Nummer 50313` | unsigned long number 50313 (reserve) | UNS32 | 50313 | | | |
| 16 | `unsigned long Nummer 50314` | unsigned long number 50314 (reserve) | UNS32 | 50314 | | | |
| 16 | `unsigned long Nummer 50315` | unsigned long number 50315 (reserve) | UNS32 | 50315 | | | |
| 16 | `unsigned long Nummer 50316` | unsigned long number 50316 (reserve) | UNS32 | 50316 | | | |
| 16 | `unsigned long Nummer 50317` | unsigned long number 50317 (reserve) | UNS32 | 50317 | | | |
| 16 | `unsigned long Nummer 50318` | unsigned long number 50318 (reserve) | UNS32 | 50318 | | | |
| 16 | `unsigned long Nummer 50319` | unsigned long number 50319 (reserve) | UNS32 | 50319 | | | |
| 17 | `unsigned long Nummer 50320` | unsigned long number 50320 (reserve) | UNS32 | 50320 | | | |
| 17 | `unsigned long Nummer 50321` | unsigned long number 50321 (reserve) | UNS32 | 50321 | | | |
| 17 | `unsigned long Nummer 50322` | unsigned long number 50322 (reserve) | UNS32 | 50322 | | | |
| 17 | `unsigned long Nummer 50323` | unsigned long number 50323 (reserve) | UNS32 | 50323 | | | |
| 17 | `unsigned long Nummer 50324` | unsigned long number 50324 (reserve) | UNS32 | 50324 | | | |
| 17 | `unsigned long Nummer 50325` | unsigned long number 50325 (reserve) | UNS32 | 50325 | | | |
| 18 | `unsigned long Nummer 50326` | unsigned long number 50326 (reserve) | UNS32 | 50326 | | | |
| 18 | `unsigned long Nummer 50327` | unsigned long number 50327 (reserve) | UNS32 | 50327 | | | |
| 18 | `unsigned long Nummer 50328` | unsigned long number 50328 (reserve) | UNS32 | 50328 | | | |
| 18 | `unsigned long Nummer 50329` | unsigned long number 50329 (reserve) | UNS32 | 50329 | | | |
| 18 | `unsigned long Nummer 50330` | unsigned long number 50330 (reserve) | UNS32 | 50330 | | | |
| 18 | `unsigned long Nummer 50331` | unsigned long number 50331 (reserve) | UNS32 | 50331 | | | |
| 19 | `unsigned long Nummer 50332` | unsigned long number 50332 (reserve) | UNS32 | 50332 | | | |
| 19 | `unsigned long Nummer 50333` | unsigned long number 50333 (reserve) | UNS32 | 50333 | | | |
| 19 | `unsigned long Nummer 50334` | unsigned long number 50334 (reserve) | UNS32 | 50334 | | | |
| 19 | `unsigned long Nummer 50335` | unsigned long number 50335 (reserve) | UNS32 | 50335 | | | |
| 19 | `unsigned long Nummer 50336` | unsigned long number 50336 (reserve) | UNS32 | 50336 | | | |
| 19 | `unsigned long Nummer 50337` | unsigned long number 50337 (reserve) | UNS32 | 50337 | | | |
| 20 | `unsigned long Nummer 50338` | unsigned long number 50338 (reserve) | UNS32 | 50338 | | | |
| 20 | `unsigned long Nummer 50339` | unsigned long number 50339 (reserve) | UNS32 | 50339 | | | |
| 20 | `unsigned long Nummer 50340` | unsigned long number 50340 (reserve) | UNS32 | 50340 | | | |
| 20 | `unsigned long Nummer 50341` | unsigned long number 50341 (reserve) | UNS32 | 50341 | | | |
| 20 | `unsigned long Nummer 50342` | unsigned long number 50342 (reserve) | UNS32 | 50342 | | | |
| 20 | `unsigned long Nummer 50343` | unsigned long number 50343 (reserve) | UNS32 | 50343 | | | |
| 21 | `unsigned long Nummer 50344` | unsigned long number 50344 (reserve) | UNS32 | 50344 | | | |
| 21 | `unsigned long Nummer 50345` | unsigned long number 50345 (reserve) | UNS32 | 50345 | | | |
| 21 | `unsigned long Nummer 50346` | unsigned long number 50346 (reserve) | UNS32 | 50346 | | | |
| 21 | `unsigned long Nummer 50347` | unsigned long number 50347 (reserve) | UNS32 | 50347 | | | |
| 21 | `unsigned long Nummer 50348` | unsigned long number 50348 (reserve) | UNS32 | 50348 | | | |
| 21 | `unsigned long Nummer 50349` | unsigned long number 50349 (reserve) | UNS32 | 50349 | | | |
| 22 | `unsigned long Nummer 50350` | unsigned long number 50350 (reserve) | UNS32 | 50350 | | | |
| 22 | `unsigned long Nummer 50351` | unsigned long number 50351 (reserve) | UNS32 | 50351 | | | |
| 22 | `unsigned long Nummer 50352` | unsigned long number 50352 (reserve) | UNS32 | 50352 | | | |
| 22 | `unsigned long Nummer 50353` | unsigned long number 50353 (reserve) | UNS32 | 50353 | | | |
| 22 | `unsigned long Nummer 50354` | unsigned long number 50354 (reserve) | UNS32 | 50354 | | | |
| 22 | `unsigned long Nummer 50355` | unsigned long number 50355 (reserve) | UNS32 | 50355 | | | |
| 23 | `unsigned long Nummer 50356` | unsigned long number 50356 (reserve) | UNS32 | 50356 | | | |
| 23 | `unsigned long Nummer 50357` | unsigned long number 50357 (reserve) | UNS32 | 50357 | | | |
| 23 | `unsigned long Nummer 50358` | unsigned long number 50358 (reserve) | UNS32 | 50358 | | | |
| 23 | `unsigned long Nummer 50359` | unsigned long number 50359 (reserve) | UNS32 | 50359 | | | |
| 23 | `unsigned long Nummer 50360` | unsigned long number 50360 (reserve) | UNS32 | 50360 | | | |
| 23 | `unsigned long Nummer 50361` | unsigned long number 50361 (reserve) | UNS32 | 50361 | | | |
| 24 | `unsigned long Nummer 50362` | unsigned long number 50362 (reserve) | UNS32 | 50362 | | | |
| 24 | `unsigned long Nummer 50363` | unsigned long number 50363 (reserve) | UNS32 | 50363 | | | |
| 24 | `unsigned long Nummer 50364` | unsigned long number 50364 (reserve) | UNS32 | 50364 | | | |
| 24 | `unsigned long Nummer 50365` | unsigned long number 50365 (reserve) | UNS32 | 50365 | | | |
| 24 | `unsigned long Nummer 50366` | unsigned long number 50366 (reserve) | UNS32 | 50366 | | | |
| 24 | `unsigned long Nummer 50367` | unsigned long number 50367 (reserve) | UNS32 | 50367 | | | |
| 25 | `unsigned long Nummer 50368` | unsigned long number 50368 (reserve) | UNS32 | 50368 | | | |
| 25 | `unsigned long Nummer 50369` | unsigned long number 50369 (reserve) | UNS32 | 50369 | | | |
| 25 | `unsigned long Nummer 50370` | unsigned long number 50370 (reserve) | UNS32 | 50370 | | | |
| 25 | `unsigned long Nummer 50371` | unsigned long number 50371 (reserve) | UNS32 | 50371 | | | |
| 25 | `unsigned long Nummer 50372` | unsigned long number 50372 (reserve) | UNS32 | 50372 | | | |
| 25 | `unsigned long Nummer 50373` | unsigned long number 50373 (reserve) | UNS32 | 50373 | | | |
| 26 | `unsigned long Nummer 50374` | unsigned long number 50374 (reserve) | UNS32 | 50374 | | | |
| 26 | `unsigned long Nummer 50375` | unsigned long number 50375 (reserve) | UNS32 | 50375 | | | |
| 26 | `unsigned long Nummer 50376` | unsigned long number 50376 (reserve) | UNS32 | 50376 | | | |
| 26 | `unsigned long Nummer 50377` | unsigned long number 50377 (reserve) | UNS32 | 50377 | | | |
| 26 | `unsigned long Nummer 50378` | unsigned long number 50378 (reserve) | UNS32 | 50378 | | | |
| 26 | `unsigned long Nummer 50379` | unsigned long number 50379 (reserve) | UNS32 | 50379 | | | |
| 27 | `unsigned long Nummer 50380` | unsigned long number 50380 (reserve) | UNS32 | 50380 | | | |
| 27 | `unsigned long Nummer 50381` | unsigned long number 50381 (reserve) | UNS32 | 50381 | | | |
| 27 | `unsigned long Nummer 50382` | unsigned long number 50382 (reserve) | UNS32 | 50382 | | | |
| 27 | `unsigned long Nummer 50383` | unsigned long number 50383 (reserve) | UNS32 | 50383 | | | |
| 27 | `unsigned long Nummer 50384` | unsigned long number 50384 (reserve) | UNS32 | 50384 | | | |
| 27 | `unsigned long Nummer 50385` | unsigned long number 50385 (reserve) | UNS32 | 50385 | | | |
| 28 | `unsigned long Nummer 50386` | unsigned long number 50386 (reserve) | UNS32 | 50386 | | | |
| 28 | `unsigned long Nummer 50387` | unsigned long number 50387 (reserve) | UNS32 | 50387 | | | |
| 28 | `unsigned long Nummer 50388` | unsigned long number 50388 (reserve) | UNS32 | 50388 | | | |
| 28 | `unsigned long Nummer 50389` | unsigned long number 50389 (reserve) | UNS32 | 50389 | | | |
| 28 | `unsigned long Nummer 50390` | unsigned long number 50390 (reserve) | UNS32 | 50390 | | | |
| 28 | `unsigned long Nummer 50391` | unsigned long number 50391 (reserve) | UNS32 | 50391 | | | |
| 29 | `unsigned long Nummer 50392` | unsigned long number 50392 (reserve) | UNS32 | 50392 | | | |
| 29 | `unsigned long Nummer 50393` | unsigned long number 50393 (reserve) | UNS32 | 50393 | | | |
| 29 | `unsigned long Nummer 50394` | unsigned long number 50394 (reserve) | UNS32 | 50394 | | | |
| 29 | `unsigned long Nummer 50395` | unsigned long number 50395 (reserve) | UNS32 | 50395 | | | |
| 29 | `unsigned long Nummer 50396` | unsigned long number 50396 (reserve) | UNS32 | 50396 | | | |
| 29 | `unsigned long Nummer 50397` | unsigned long number 50397 (reserve) | UNS32 | 50397 | | | |
| 30 | `unsigned long Nummer 50398` | unsigned long number 50398 (reserve) | UNS32 | 50398 | | | |
| 30 | `unsigned long Nummer 50399` | unsigned long number 50399 (reserve) | UNS32 | 50399 | | | |
| 30 | `unsigned long Nummer 50400` | unsigned long number 50400 (reserve) | UNS32 | 50400 | | | |
| 30 | `unsigned long Nummer 50401` | unsigned long number 50401 (reserve) | UNS32 | 50401 | | | |
| 30 | `unsigned long Nummer 50402` | unsigned long number 50402 (reserve) | UNS32 | 50402 | | | |
| 30 | `unsigned long Nummer 50403` | unsigned long number 50403 (reserve) | UNS32 | 50403 | | | |

Rows 50231 to 50403 are all bare reserve entries ("unsigned long Nummer nnnnn", class C, UNS32) without description or applicability marks, spread over dialog boxes 2 to 30 (six entries per box; box 30 holds 50398-50403). Pages: 50231-50238 on 9-33, 50239-50258 on 9-34, 50259-50278 on 9-35, 50279-50299 on 9-36, 50300-50319 on 9-37, 50320-50339 on 9-38, 50340-50360 on 9-39, 50361-50380 on 9-40, 50381-50400 on 9-41, 50401-50403 on 9-42. The rest of page 9-42 is empty; this is the end of the parameter list.

**Continues in part b:** section 9.2 `Beschreibung der M-Befehle` (description of the M-commands) starts on page 9-43, followed by 9.3 `Fehlermeldungen` (error messages, from 9-55) and 9.4 `Externe Ereignisse` (external events, from 9-184). No M-code or PLC-message tables are on pages 9-1 to 9-42.

---

## For the retrofit

- Control: Gildemeister's own control, programming manual "ELTROPILOT" (p. 9-6); PLC I/O is on "Eltromatic" I/O modules, up to 16 modules (`E/A - Modul 01..16 installiert`, K 50108-50123, all applicable to CTX twin) (p. 9-18). Parameters are "K numbers" 50000-50403 in parameter groups 020-047, protection classes A-D (p. 9-6).
- Two sides are handled as two channels/automatic groups: almost every twin-specific parameter exists as "I" and "II" (tool monitoring, hoods, unloading devices, handling, lubrication, hydraulics, chip conveyor, material feed, measuring) (pp. 9-19 to 9-29). `Mengenimpulsausgabe` K 50132 sets whether part-count pulses come from AG1 and AG2 directly, only AG1, only AG2, or time-offset (p. 9-19). Operating-mode splitting (`Betriebsartensplitting`, KONFIG / MASCHINEN LISTE parameter 8, ON/OFF) lets the two sides run in different modes (p. 9-5).
- Axis naming (from the handling axis-interlock bit list): X, Z, Y, A, B, C per slide/channel 1-5, i.e. X1 Z1 Y1 A1 B1 C1, X2 Z2 ... (p. 9-23/9-24). The twin uses X1/Z1/C1 and X2/Z2/C2 [inference from "I/II" structure, not stated explicitly].
- Spindles: parameter groups for spindle H, 1, 2, 3 (pp. 9-7 to 9-12). Spindle drive sequence: `HW-Spindelfreigabe --> Reglerfreigabe Steller EIN` delay in 100 ms (K 50022/50036/50050/50064; handwritten values 3, 1, 1, 3). Spindle clamp (brake) not fitted (handwritten 0). Gearbox: none (0). C axis type: 2 = C axis without pressure roller (handwritten for spindle H and 3) (pp. 9-7 to 9-12). C-axis rapid in degrees/min and C zero offset are settable parameters (p. 9-5).
- Chucks: hydraulic power chucks; `Spannart` 1/2 = internal/external clamping, `Spannzylinder` direction (retract = jaws close/open), re-clamp time in 20 ms units; clamp type 0 = hydraulic with stroke limit, safety proximity switches and centre-position valves (pp. 9-7 to 9-12). The control must handle ID/OD clamping selection and the stroke-limit switches.
- Feed axes sequence per slide: drive enable ON -> release brake (delay 1, 20 ms units), brake ON -> drive enable OFF (delay 2), HW feed enable -> drive enable ON (delay 3, 100 ms units); slip clutch type 1 (Meyer) on CTX twin (K 50066-50070, handwritten 1/0/0/1/1) (p. 9-13). These are the brake/enable timings to reproduce with stmbl enable + brake outputs.
- Turret: `Revolvertyp` 5 = Sauter servo turret, 12 tool stations (handwritten for slide 1, K 50071/50072) (p. 9-13).
- Tailstock: fixed / hydraulic types; quill hydraulic (handwritten 1, faint) (p. 9-21).
- Guards: guard type 6 = "Typ 7, automatic pneumatic type 2 without safety edge" for both sides; automatic unlocking on (1); guard unlocking only after drives off (`Reglerfreigaben Steller AUS --> Haube entriegeln`, delay 6 x 100 ms = 0.6 s); time monitoring off. "Auto cycle start on closing the guard" changed by hand from 1 to 0 on 20.6.95 (pp. 9-25, 9-26).
- Part handling / transfer: `Typ Handhabung` I and II = 4, "simple robot for TWIN"; with a bit mask (K 50166/50169) to lock axes while the handler is in the working area (set to 0 here). Unloading device type 2 "CTX twin" exists but is set 0; loading flap not fitted; no bar feed (pp. 9-22 to 9-25).
- Partition plate (`Schottblech`) between the sides with a max. function time (K 50130/50131, CTX twin) (p. 9-19).
- ISM temperature control for the CTX twin: cooling-water, winding and front/rear bearing temperature set points, hysteresis and MAX/MAXMAX limits (pp. 9-20, 9-21).
- Lubrication: interval pulse lubrication per side with interval [min] and max. suppression time [min] (p. 9-28). Hydraulics I/II can be switched off together with the drives (p. 9-29). Chip conveyor with interval ON/OFF times (p. 9-29).
- Coolant flow monitoring with debounce times and workpiece seating control (`Werkstückanlagekontrolle`) 1 and 2 on CTX twin (p. 9-33). Ground-break monitoring (p. 9-33).
- Comfort user relays 1-16 with selectable switch-off at E-stop or "quick stop" (serious fault, mode change, E-stop) (p. 9-30).
- Tool measurement: tool measuring I and II (type 1) applicable to CTX twin (p. 9-27).
