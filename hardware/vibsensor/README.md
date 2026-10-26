# vibsensor KiCad project

The vibration and temperature sensor of [docs/vibsensor.md](../../docs/vibsensor.md),
for the firmware in [vibsensor/](../../vibsensor/README.md): a KiCad 7
project with the schematic, a placed (not routed) round 26 mm board, and a
BOM.

![top and bottom (seen from below)](doc/vibsensor-placement.png)

**State:**
- **Schematic:** complete, one sheet, checked against the part list
  (`make sch`: 50 parts, 33 nets).
- **Board:**
  - 26 mm round, four layers, a GND plane on In1.Cu (not filled: press B in
    the editor).
  - Every footprint placed with its nets; DRC clean but for the unrouted
    connections.
  - **Not routed:** the placement is a starting point for the layout.
- **To check against datasheets** (✱ in each part's *Note* and the BOM):
  - the IIS3DWB's pinout and decoupling (DS12569);
  - the TMP117's pinout and exposed pad;
  - the TCAN1044V, drawn with the TJA1042T-3 symbol (same pinout);
  - the buck converter's inductor;
  - the crystal's load capacitors;
  - the M8 socket's pin order (no CiA standard for M8 4-pin).

## The circuit

| section | parts |
|---|---|
| supply | J1, the M8 socket's four leads (1 V+, 2 CAN_H, 3 GND, 4 CAN_L); D1 reverse polarity; D2 TVS; U2 TPS560430 buck to 5 V (the transceiver); U3 TLV75533 to 3.3 V; ferrite-filtered VDDA and the accelerometer's own 3V3_ACC; R3 / R4, the bus voltage to PA0 |
| MCU | U1 STM32G473CEU6, pins as `vibsensor/inc/vs.h`; 8 MHz crystal; LED; J2 Tag-Connect TC2030 pads for SWD (programmed before potting) |
| sensors | U4 IIS3DWB on SPI1 (PA4-7, INT1 / INT2 on PB0 / PB1); U5 TMP117 on I2C2 (PA8 / PA9, ALERT on PA10), address 0x48 |
| CAN | U6 TCAN1044V (VCC 5 V, VIO 3.3 V, STB on PB13); 120 Ω through JP1, closed on the sensor at a bus end |

**On the board:**
- **Top:** the M8 leads' pads (J1), the supply, SWD.
- **Bottom** (facing the housing's base, over the M6 stud):
  - the IIS3DWB at the centre;
  - the TMP117 against the base;
  - the MCU and the transceiver.
- **Mounting:** two M2 holes hold the board to the housing before it is
  potted.

## Generated: edit `gen/parts.py` and `gen/board.py`

`make` runs `hardware/kigen` (see `hardware/brdstepper/README.md`):
- `sch` and `check`: the schematic, and its netlist against the part list;
- `pcb`: placement (redone: hand edits to the board are lost);
- `drc`, `bom`, `doc`: DRC, the BOM, the PDF and the render.
