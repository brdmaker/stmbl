# stman KiCad project

The schematic of **stman**, the stmbl with an analog output
([docs/stman.md](../../docs/stman.md)). It is stmbl v4.0's LV board
(`hw/kicad/v4.0`) with the stman changes, as a KiCad 7 project. The
schematic is in [doc/stman-schematic.pdf](doc/stman-schematic.pdf).

**stmbl's sheets keep their drawing.** KiCad 7 can't read the KiCad 5
sheets, so `gen/build.py` converts them with `hardware/kigen/legacy.py`.
Every symbol, wire, junction, label and note stays where stmbl has it. Then
it edits them in place.

| sheet | what |
|---|---|
| `stman.kicad_sch` | root, stmbl's `ctrl.sch`: P14 (the HV board link) removed; the new sheets added |
| `supply.kicad_sch` | stmbl's: P3 24 V in (+UI / 0VI), D200 reverse polarity in the wire up from P3, bucks 5 V / 12 V / port 5 V, LDO 3.3 V |
| `f4.kicad_sch` | stmbl's MCU sheet. U22 (ESD on the CMD lines) removed. The MCU pins stman uses carry their new nets as global labels where stmbl's labels were, or at the pin when it was tied to something else (`build.MCU`) |
| `cmd.kicad_sch` | stmbl's CMD port, isolated. U8/U9 stay on the cable side (SS_5V, SS_GND, SS_SHIELD); U10, U11 and their parts are removed; U200 ISO7331FC joins them to the MCU; JP200/JP201 pick the cable's +5 V and GND pairs |
| `fb.kicad_sch` (FB0, FB1) | stmbl's, with a polyfuse (F200 / F201) between the port supply and the connector's pin 7 |
| `io.kicad_sch` | stmbl's IO sheet with the outputs, analog inputs, fan and 2x6 header removed; the status LEDs kept |
| `dac.kicad_sch` | new: AD5761R ×2, OPA2197 Sallen-Key low-pass, TVS, J5; NMA1215 ±15 V |
| `ain.kicad_sch` | new: AD7606 monitor ADC, input resistors, rail dividers, J6 |
| `drvio.kicad_sch` | new: ITS724G ×2 (OUT1-8, J7), 8 inputs (J8), MAX6369 watchdog and the 74LVC2G08 gate on OUT1/OUT2 |

The new sheets are drawn the way `hardware/kigen` draws: parts in framed
groups, each pin joined to its net by a label. Global labels connect them
to the MCU sheet.

Refs from 200 up are stman's. The others are stmbl v4.0's, with the same
values and footprints (stmbl's footprint library, `fp-lib-table` ->
`hw/kicad/lib/stmbl.pretty`).

## Make

| target | does |
|---|---|
| `make sch` | `gen/build.py`: convert, edit, add the new sheets |
| `make check` | `gen/check.py`: the netlist kicad-cli exports against two references (below) |
| `make doc` | the PDF |

`gen/check.py` checks:
- **stmbl's own nets:** every net of stmbl's 2018 netlist
  (`hw/kicad/v4.0/stmbl_4.0.net`) among the parts kept is still one net,
  except the ones stman changes on purpose;
- **stman's nets:** every pin of the new parts, the MCU pins and the
  isolator is on the net the design gives it, and no two of those nets are
  joined.

At the time of writing it reports 229 nets, 335 parts, OK.

Edit the generator (`gen/build.py`, `gen/newsheets.py`) rather than the
sheets while the design is moving. Once layout starts, edit in KiCad.

## To check before layout (✱)

- **AD5761R:** the pin numbers (its symbol is drawn from the datasheet's pin
  names).
- **AD7606:** in serial mode, check that DOUT_A is tri-stated with /CS high
  (it shares MISO with the DACs).
- **MAX6369:** the SET0-2 timeout table.
- **ITS724G:** the status logic.
- **Mesa's smart-serial RJ45 pinout:** which pairs carry +5 V and GND
  (JP200/JP201).
