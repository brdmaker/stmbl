# Mikron HSM 400U operating manual: summaries

Condensed English summaries of the machine's operating manual. The source
scans, `Mikron_ohjeet_s*.pdf`, are Mikron's English edition, document
222.84.80.110 of December 2004, for the HSM 400 / HSM 400U with a Heidenhain
iTNC 530.

- **Format:** each file follows the manual's own chapters. It keeps every
  concrete figure, part number, M-code, message number and procedure, and
  ends with a "For the retrofit" section.
- **Uncertainty:** unclear readings are marked [uncertain] or
  [unreadable].
- **Inconsistencies:** where the manual contradicts itself, both versions
  are noted.

| File | Manual pages | Contents |
|---|---|---|
| [01_p001-026.md](01_p001-026.md) | 1-26 | Contents, safety (ch. 2), machine design (ch. 3): axes, measuring systems, spindle, changers, hydraulics, pneumatics, safety devices |
| [02_p027-054.md](02_p027-054.md) | 27-54 | Operation (ch. 4.1-4.5): panel and keys, start-up and shut-down, E-STOP and door logic, spindle pre-lubrication and warm-up, homing, jog and handwheels, SET-UP modes |
| [03_p055-074.md](03_p055-074.md) | 55-74 | Block restart, spindle vibration monitoring (APS), thermal compensation, C-axis pivot calibration (cycle 307, MP950), the M-function list, standby, user parameters |
| [04_p075-100.md](04_p075-100.md) | 75-100 | User parameters, the PLC message table 00-125 (with input numbers), tool and pocket tables, tool changer operation and recovery, workpiece changer |
| [05_p101-122.md](05_p101-122.md) | 101-122 | Pallet changer recovery, backup, laser tool measurement, maintenance safety, lubrication, hydraulics (diagrams, rotary table clamps), spindle cooling, MQL, pneumatics |
| [06_p123-144.md](06_p123-144.md) | 123-144 | Maintenance schedule, lubricants, electrical maintenance, the iTNC 530 logic unit's connectors, serial interfaces, backup, disposal, transport |
| [07_p145-162.md](07_p145-162.md) | 145-162 | Installation (foundation, mains, transport locks), commissioning, options (chip conveyor, probes, handwheel, MQL, extraction) |
| [08_p163-177.md](08_p163-177.md) | 163-177 | Technical data (travels, speeds, spindle, supplies, dimensions), coolants, index, EC declaration |

## For the retrofit: the manual against the earlier assumptions

| Item | Assumed so far | The manual says |
|---|---|---|
| Control | iTNC 520 | **iTNC 530** throughout |
| Axis drives | Siemens servos, drives unknown | **Siemens Simodrive 611 D**, which feeds braking energy back to the mains |
| Rotary axes | A/C trunnion, geared | **B (swivel, ±110°) and C**, also numbered as axes IV/V. The naming is inconsistent: some messages say A. Clamped hydraulically. Absolute encoders "on the rotor", which may mean direct drives. |
| Travels | X 400 / Y 450 / Z 360 (guessed) | **X 400 / Y 240 / Z 350 mm** |
| Linear drives | 16 mm screws, direct | **3000 rpm AC servos, toothed belt to the ballscrews**, 40 m/min at 1 g (10 m/s²). Belt ratio and pitch not given. |
| Rotary speeds | A 85 / C 125 deg/s | **B 150 rpm and 100 rad/s², C 250 rpm and 300 rad/s²** |
| Feedback | glass scales and EnDat | Heidenhain incremental glass scales with distance-coded marks on X Y Z. Incremental encoders in the motors. The control's inputs are 1 Vpp. The manual never mentions EnDat. |
| Spindle | kept, drive unknown | Step-Tec HVC 140 (30k / 42k / 60k rpm), oil-air lubricated |

Things the new control has to take over:

- **E-STOP chain and doors:** 3 E-STOP keys, the handwheel's E-STOP and approval keys, the door locks and the key switches. In SET-UP mode the limit is 2000 mm/min, with a 3-minute time-out.
- **Rotary clamps:** for B and C, M10/M11 and M15/M16 work the valves (Y341 for C, Y351 for B). The control has to unclamp, move, clamp, and check the 35 bar and 5 bar pressure switches.
- **Spindle interlocks:**
  - Pre-lubrication: 10 min after more than 15 min off, spindle start blocked meanwhile.
  - Warm-up: 6 min at 21,000 rpm; below 20 °C coolant temperature, limited to 3000 rpm.
  - Lubrication, cooling flow and temperature, and air pressure are all monitored.
  - Orientation by M19.
  - Tool clamp: hydraulic, valve Y461, 70 bar.
- **Hydraulics, lubrication, pneumatics, coolant, conveyors, extraction:** the M-code list is in file 03, the valves in files 05 and 06.
- **Tool changer:** disk magazine with 18/36/68 HSK-E40 tools and an automatic door. The change positions are in MP960.x.
- **Pallet changer:** if fitted. Its recovery logic is in file 05.
- **Z axis:** gravity-loaded on a toothed belt. Keep the motor brake and its control.

Back up from the iTNC 530 before removing anything:

- all machine parameters, including:
  - MP950, the C pivot (also the log `TNC:\Cycle_307\List-RP.A`)
  - MP960.x, the tool change positions
  - 4210.47 and 4230.9, handwritten on the manual's cover
- the axis error compensation tables
- the PLC program

A paper copy and a backup floppy set are reported to be in the electrical
cabinet.
