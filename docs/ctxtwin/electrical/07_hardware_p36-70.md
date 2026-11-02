# Gildemeister CTX twin, electrical documentation: 7. CNC hardware modules - ELTROPILOT controls, PDF pages 36-70

Source: `d5951336-7_Hardware_20201029_0001.pdf` (scan "7 Hardware", 70 pages), these are PDF pages 36-70 = manual pages 7-36 to 7-70. Title block / footer on every page: "Electrical technical instructions CTX twin", Doc. No.: DAAAE0G0, November 1993. Running header: "7. CNC hardware modules - ELTROPILOT controls".
Original language: English (Gildemeister's English translation; no German text on these pages). Pages 1-35 are transcribed separately.

Conventions: text in quotation marks is copied as printed (including the original spelling such as "Currant", "Ambiant", "SUD-D"). [uncertain] = reading not sure, [unreadable] = cannot be read. Comments by the transcriber are in parentheses and marked as notes.

Important: these pages describe the ELTROPILOT (EP90) control-side modules only (VK030 CPU, SI188 PLC piggy-back, VIC30 interpolator, VAP4 axis periphery card, IO48 ELTROMATIC I/O, monitor, BFT90 keyboard, handwheel). They contain no Indramat drive data, no wiring diagrams with device tags (-K.., -X..) and no wire numbers.

## Page overview

| PDF page | Manual page | Content |
|---|---|---|
| 36 | 7-36 | VK030 processor board (end of 7.5.1): LEB/IPIN, expansion modules, block diagram VK030 |
| 37 | 7-37 | VK030 connector X1 (25-pole SUB-D, 20 mA / V28) pinout, X2, RESET switch, STATUS display |
| 38 | 7-38 | 7.5.2 SI188 PLC card: description, block diagram |
| 39 | 7-39 | SI188 technical features, 80C188EB features, interface functions |
| 40 | 7-40 | SI188 connector X2 (15-pole SUB-D) pinout, ELTROMATIC bus interface schematic |
| 41 | 7-41 | 7.6 VIC30 interpolator card: description, block diagram |
| 42 | 7-42 | VIC30 technical features |
| 43 | 7-43 | VIC30 configuration (component/jumper layout) |
| 44 | 7-44 | VIC30 front panel |
| 45 | 7-45 | 7.7 VAP4 axis periphery card: description (±10 V 16-bit analog outputs, incremental encoder inputs) |
| 46 | 7-46 | VAP4 description continued |
| 47 | 7-47 | VAP4 block diagram |
| 48 | 7-48 | VAP4 technical features |
| 49 | 7-49 | VAP4 connector X2 (11-pole) pinout, displays, supply data |
| 50 | 7-50 | VAP4 configuration (component layout) |
| 51 | 7-51 | VAP4 front panel |
| 52 | 7-52 | 7.8 IO48 input/output module: description, block diagram |
| 53 | 7-53 | ELTROMATIC system architecture |
| 54 | 7-54 | IO48 technical features |
| 55 | 7-55 | IO48 technical data |
| 56 | 7-56 | IO48 interface, X12/X13 (15-pole SUB-D) and X11 pinouts |
| 57 | 7-57 | IO48 configuration (board layout, terminals) |
| 58 | 7-58 | 7.9 Control panel components, 7.9.1 Monitor unit |
| 59 | 7-59 | Monitor unit electrical connection and environment |
| 60 | 7-60 | 7.9.2 14" colour monitor, VGA connector pinout |
| 61 | 7-61 | Monitor environmental data |
| 62 | 7-62 | 7.9.3 Keyboard unit (BFT 90) |
| 63 | 7-63 | Keyboard technical data, configuration, connectors X1-X7 |
| 64 | 7-64 | OK button, keyboard key layout and row/column matrix |
| 65 | 7-65 | Keyboard electrical connection, environment |
| 66 | 7-66 | Keyboard block diagram |
| 67 | 7-67 | Keyboard technical features |
| 68 | 7-68 | 7.9.4 Handwheel, 10-pole connector pinout |
| 69 | 7-69 | Handwheel timing diagram, mechanical data |
| 70 | 7-70 | blank |

## Page-by-page transcription

### PDF page 36 — manual page 7-36 (VK030 processor board, continued)

Running header: "7. CNC hardware modules - ELTROPILOT controls". Footer: "Page 7-36 / Doc. No.: DAAAE0G0 / November 1993 / Electrical technical instructions CTX twin".

Text (continuation of the processor-board feature list from page 7-35):
- "A local extension bus (LEB) is realised on the board, making flexible extension of the processor board possible. A child board can be mounted via plug connections and metal bolts to give the basis board additional ~~special~~ functions." (the word "special" is struck through by hand)
- "Several of these child boards, so-called intelligent peripheral interfaces (IPIN's) can be used (ETHERNET, Cheapernet, serial I/O, parallel I/O and other tasks)."
- "The processor board occupies one printed circuit board slot (4TE) in a VME system."

**Expansion modules**
- "To expand the functions of the processor board, different IPIN's can be connected to the above mentioned local extension bus, such as e.g. a PLC-IPIN (SI188) with a self-sufficient processor system with DUAL PORT RAM connection to the LEB and two serial channels which are available on a front connector (X2)."
- "The processor board with the expansion via a PLC-IPIN (SI188) occupies only one printed circuit board slot (4TE) in a VME system."

**Block diagram of the processor board** (caption: "Block diagram VK030")
Blocks (small hand-lettered text, partly hard to read):
- CPU (68030 [uncertain, label "68030" small]) and FPU on the local bus, connected via bidirectional buffers to the internal bus.
- ADDRESS DECODING; DRAM CONTR.; 1/4 MBYTE DRAM [uncertain: "1/4 MBYTE"].
- "4-SOCKET 32 PIN up to 8 MBIT PROM" [uncertain wording]; "EPROM EXTENSION".
- "128KB SRAM" [uncertain]; "BATTERY BACKUP 2K SRAM REAL TIME CLOCK" [uncertain].
- VIC (VMEbus interface controller), with "SYSTEM CONTROLLER" jumper/switch above it.
- "SLAVE ADDRESS DECODER" with a rotary switch symbol.
- "32 BIT ADDRESS BUS" and "32 BIT DATA BUS" to the VME connectors P1 ("VME BUS" written vertically) and P2.
- "LOCAL EXTENSION BUS" (bottom).
- Front elements (left): hex STATUS display (7-segment), RESET pushbutton with LED, rotary switch, "25 PIN SUB-D" connector [uncertain] (X1); "PARALLEL 8536" [uncertain: Z8536 CIO]; "RS232 + 20mA" with "SCC 8530" [uncertain].

### PDF page 37 — manual page 7-37

**Connector X1**
"This connector transmits the signals of a serial interface for active 20mA operation and an additional interface for V28 and 20mA connection."
Connector type: 25-pole SUB-D, jack.

Pin assignment for active 20mA operation:

| Pin | Signal |
|---|---|
| 1 [uncertain, partly cut] | TD1- |
| 6 [uncertain] | RD1- |
| 20 [uncertain] | RD1+ |
| 21 [uncertain] | TD1+ |

Pin assignment for active V28/20mA interface:

| Pin | Signal | Group |
|---|---|---|
| 2 | TD | V28 |
| 3 | RD | V28 |
| 4 | RTS | V28 |
| 5 | CTS | V28 |
| 7 | GND | V28 / 20mA (bracket boundary) |
| 8 | ITD- | 20mA |
| 9 | TD- | 20mA |
| 10 | TD+ | 20mA |
| 11 | ITD+ | 20mA |
| 12 | ICTS- | 20mA |
| 13 | CTS- | 20mA |
| 14 | CTS+ | 20mA |
| 15 | ICTS+ | 20mA |
| 16 | IRTS- | 20mA |
| 17 | RTS- | 20mA |
| 18 | RTS+ | 20mA |
| 19 | IRTS+ | 20mA |
| 22 | IRD- | 20mA |
| 23 | IRD+ | 20mA |
| 24 | RD- | 20mA |
| 25 | RD+ | 20mA |

(Pins 2-5 and 7 bracketed "V28"; pins 8-13 bracketed "20mA"; pins 14-25 bracketed "20mA". Pin 7 GND is shared.)

**Connector X2**
"The processor board VK030 is expanded by way of a PLC-IPIN (SI188) for operating I/O periphery. The connector located on the IPIN transmits the signals of the serial ELTROMATIC bus (RS485) as well as the signals for a terminal connection (V28). The power supply of the ELTROMATIC module is also possible via this connector."
Connector type: 15-pole SUB-D, pins. "For further information on this, please refer to the description 'PLC-IPIN SI188'".

**RESET switch**
"A RESET switch contact is provided for generating the RESET of the CPU module and/or the system (if SYSTEM CONTROLLER active) and is designed as a jack contact to avoid inadvertent actuation. The LED positioned adjacent to the switch jack displays the RESET status."
- LED = on --> RESET
- LED = off --> no RESET

**STATUS display**
"Status information can be given software controlled via a hexadecimal display element."

### PDF page 38 — manual page 7-38

**7.5.2 SI188 PLC card**
- "The SI188 module assumes the PLC functions of the EP90.G and/or EP90.0. The card is designed as a piggy-back IPIN (Intelligent Peripheral Interface) and is allocated to the VDK030 D/K board or VK030 K board. The PLC processor is connected to the K level CPU via the LEB (Local Extension Bus) defined by ELTEC. In addition to this special application as a PLC processor, the SI188 can also be used as a generally employable communication module."
- "Connection to the LEB (68030 CPU, e.g. VDK030) is made via a 1K x 16 bit (opt. 2K x 16 bit) organised interruptible dual port RAM. As the communication K level PLC can be executed by byte as well by text, the memory management (differences Motorola - INTEL) must be executed via the software."
- "The memories consist of EPROM base (1M, 2M- & 4M bit types) and SRAM which can be buffered with 64KByte."
- "Time-out monitoring of the µP 80C188EB is performed by communication with the K and A level."
- "In addition, an ELTROMATIC interface for external I/O's and a RS232 connection are provided on the SI188. The serial ELTROMATIC bus connection is performed according to the serial ELTROMATIC bus connection specifications by Gildemeister Automation GmbH and allows the coupling of I/O modules e.g.
  - BST IO48
  - decentral I/O's on the ELTROMATIC bus (e.g. EMBI and EAM 26)"

**Block diagram** (SI188):
- Left: LEB connector ("LEB", "68030 - CPU 25MHZ").
- Address lines A1..11 and data lines D0..15 through buffers (driver symbols) to the DPR block: "R CONTROL L", "A1..11R A1..11L", "DPR 1Kx16BIT - 2Kx16BIT", "D0..15R D0..15L", "D0..7R D0..7L".
- "LOGIC" block with CONTROL lines between LEB, DPR and CPU.
- "80C188EB" with "3x16BIT-TIMER", oscillator "QOS 16MHZ" [uncertain "QOS"; 16 MHz].
- CONTROL, A0..19, D0..7 buses.
- "SIO1" -> RS485 driver -> "15P. SUB D"; "SIO0" -> V28 driver -> same 15-pole SUB-D.
- "EPROM 128Kx8BIT - 512Kx8BIT"; "SRAM 64Kx8BIT".

### PDF page 39 — manual page 7-39

**Technical features**
Technical features of the SI188:
- ELTEC-LEB (Local Extension Bus) connection as IPIN (Intelligent Peripheral Interface)
- interruptible 1K x 16 bit (optional 2K x 16 bit) dual port RAM coupling to the LEB
- RS232 interface (e.g. terminal connection)
- RS485 interface for connection to external ELTROMATIC I/O modules
- µP80C188EB with 16 MHz
- base for 128K, 256K and 512K x 8 bit program memory/EPROM
- 64 x 8 bit SRAM, bufferable (no accumulator on the SI188) [as printed; means 64K x 8]

Technical features of the µP80C188EB:
- CPU low power
- two UART's, independant from one another, with integrated baud rate generator per UART (MCS 51 compatible)
- two 8 bit multiplexed I/O ports
- programmable interrupt controllers
- three programmable 16 bit timers/counters
- clock generator
- ten programmable chip selects with integrated wait state generator

**Periphery interfaces and front elements**
Electrical parameters: "On the SI188 two serial interfaces are designed according to the RS232C and RS485 standard."

**Interface functions**
"The K level PLC is coupled via the 100 pin LEB specified by ELTEC, expanded using a +5V standby line (UBAT buffer voltage for the SRAM in the event of power failure) on pin D20. The control signals (/DSACKx, interrupt control etc.) required for coupling the K level DPR are generated by the SI188."

### PDF page 40 — manual page 7-40

**Connecting conditions**
"A 15-pole SUB-D connector (pins) is provided for the serial ELTROMATIC bus interface connection, the RS232 connection and a (possible) line entry of the power supply for the I/O modules. The SUB-D connector must be mechanically attached to the facia of the carrier card. This design solution was chosen due to the low overall height (see section 'Mechanical Construction')."

X2 (SI188, 15-pole SUB-D pins):

| Pin | Name | Function |
|---|---|---|
| 11 | VEXin | Line entry of the supply voltage |
| 12 | VEXout | For supply - ground |
| 4 | GNDout | (VGND) |
| 5 | GNDin | |
| 13 | UBAT | External UBAT line entry |
| 2 | GNDRS232 | Signal - ground (SGND1) |
| 1 | RxD | RS232 connection |
| 9 | TxD | RS232 connection |
| 14 | /SYSok (-) | Serial ELTROMATIC bus connection interface |
| 6 | /SYSok (+) | Serial ELTROMATIC bus connection interface |
| 15 | /OUTen (-) | Serial ELTROMATIC bus connection interface |
| 8 | /OUTen (+) | Serial ELTROMATIC bus connection interface |
| 7 | GNDRS485 | Signal - ground (SGND) |
| 10 | Information (-) | Serial ELTROMATIC bus (data) |
| 3 | Information (+) | Serial ELTROMATIC bus (data) |

**Connection of external ELTROMATIC modules**
"The data and control lines, as well as the power supply of the ELTROMATIC modules are transmitted via the 15-pole SUB-D connector X2. The power supply is fed via connection X2/11 and X2/5 and looped through to the ELTROMATIC modules via X2/12 and X2/4."

Schematic (interface circuit, very small print):
- Line drivers/receivers IC1x of type "75176" [uncertain, RS485 transceivers "75176"] for TXD/RXD-side signals, /SYSOK, /OUTEN and the "Information" line; pull-up/down resistors R1xx "22k" [uncertain] to +5V.
- Signal labels on the left: TXD [uncertain], "DATA" / "TXEN" [uncertain], RXD, GND, UBAT, SYSOK [uncertain], /OUTEN [uncertain].
- To the 15-pole connector: pins labelled DATA(-) 10, DATA(+) 3 [uncertain], VEXIN 11, VEXOUT 12, GNDOUT 4, GNDIN 5, UBAT 13, /SYSOK(-) 14, /SYSOK(+) 6, /OUTEN(-) 15, /OUTEN(+) 8, GND RS485 7; RS232 via IC "75188/75189"-type [uncertain] to pins 1/9/2.
- Two 120 Ω [uncertain "12k"/"120"] termination resistors at the bus lines; capacitor and IC on the upper right "75176 3D" [uncertain].
- Note bottom right: "Identical to EUI and PCE I".
- Component values otherwise [unreadable].

### PDF page 41 — manual page 7-41

**7.6 VIC30 interpolator card**
- "The VIC30 processor subassembly serves as a computer card for the software interpolation and position control of the EP90 generation controls."
- "As bus master for the axis periphery bus, it is capable of managing the periphery for a maximum of 32 axes."
- "The VIC30 is a VME bus subassembly with a VME slave interface (dual port RAM) and a VME interrupt requester at its disposal."

Block diagram: VME-Bus (left) -> VME-Interrupt Requester, VME-Slave-Interface; Boot EPROM; "Other periphery" (BCD switch, LED1..LED4, Reset button + LED, ABP-Reset + LED [uncertain "ABP"; front shows "APB"], Jack 2 x V28 serial port); "Axis periphery bus control" -> "Axis periphery bus" (right); "Drivers for data, addresses and control signals"; Clock; TMS 320C31; Bank 1, Bank 2, Bank 3; Wait-State, CS.

### PDF page 42 — manual page 7-42

**Technical features**
Processor: TMS320C31
- maximum clock: 33MHz
- one address and a data bus
- two internal timers
- internal DMA controller
- one serial port
- 64 x 32 bit code cache
- 132 pin plastic quad flat pack casing (SMT)

Memory:
- two memory banks with 128 x 32 bit SRAM each, access time 25 - 250 ns (SMD, i.e. type must be determined during configuration)
- a memory bank for components with 600 mil dil casing and JEDEC pinning (max. 32 pins), 25 ns - 180 ns access time (RAM or EPROM)
- base for a boot prom (EPROM's with 1, 2, 4 or 8 Mbits) 120 ns - 250 ns access time

VME slave interface:
- 32K x 32 bit dual port RAM (128KB)
- Dual port RAM only supports VME LONG WORD transfers (32 bits) in VME standard address space (A24).
- does not support any read modify write cycles.

VME interrupt requester:
- makes VME interrupt request
- drives 8 bit VME interrupt vector
- ROAK behaviour (interrupt request is cancelled by IACK signal from VME bus).

Axis periphery bus control:
- local 32 bit bus with 64K x 32 bit word address space accessing axis periphery (axis periphery modules, planned: SERCOS interface and integrated drive control)
- 2 sync signals
- 1 clock signal
- local reset
- not interruptible
- can be activated by jumper and software

Other periphery:
- reset logic
- boot logic
- connection for two terminals (only for diagnosis), including the signals of the serial processor ports
- four freely programmable LED's
- BCD switch which can be read by the processor

### PDF page 43 — manual page 7-43

**Configuration** (component layout drawing of VIC30)
- Left edge: LED pairs "L1/L2", "L3/L4" [labels small], "RESAPB" [uncertain], "RESLOC" [uncertain]; "BCD" switch; "BATTERY" [uncertain]; X1 "SUD-D 25-pole jack" (as printed, = SUB-D).
- PALs/ICs with part numbers: IC (osc) "OSC1" with jumpers "SYNC1 SYNC2"; "CLK18" [uncertain]; "RESET 0.081.241"; "IVEK" [uncertain] jumper; "APBSIG 0.081.235"; "VMEINT 0.081.235" [uncertain second number]; "VMEADC"; "Z8530 SCC"; "IODEC 0.081.237"; "SELECT 0.081.230"; "WAIT 0.081.243"; "APBSTE 0.081.240"; "DPRSTE 0.081.230" [uncertain]; "DPRMAS 0.081.238"; "CLK33" jumper.
- TMS320C31 (IC304 [uncertain]) with jumpers J4x4/J4x3/J4x2 [unreadable details], "BOOT", "SPEI" [uncertain] jumper rows; IC505 "BOOT" EPROM.
- Jumpers J1x3, J2x3, J1x2, J2x2 [uncertain].
- SRAM banks: "BANK 2 IC1x4 RAM 128Kx8", "BANK 3 IC2x4 RAM 128Kx8", further BANK 2 / BANK 3 RAM 128Kx8 ICs (IC103, IC203, IC303, IC403, IC404 [uncertain], IC503).
- BANK 1 sockets: "D24..D31 IC101 J101", "D15..D23 IC201 J201" (as printed, presumably D16..D23), "D8..D15 IC301 J301", "D0..D7 IC401 J401".
- "APB" jumper near P2; VME P1 and VME P2 connectors on the right; numbers 1-8 along the bottom edge (grid).

### PDF page 44 — manual page 7-44

**Front of module** (VIC30): LEDs L1, L2, L3, L4, "RS" / "APB", "RS" / "LOC" (reset LEDs: RS APB, RS LOC [uncertain reading of "LOC"]); rotary BCD switch "BCD"; connector X1; label "VIC30".

### PDF page 45 — manual page 7-45

**7.7 VAP4 axis periphery card**
- "The VAP4 axis periphery card can be used for accessing four drive amplifiers."
- "The axis periphery card is connected to the axis periphery bus via connector P2, 96-pole according to DIN 41612; connector P1, 96-pole according to DIN 41612, for the top section of the VME bus circuit board, is additionally intended for the card voltage supply."
- "The pins of connector rows a and c which are intended for connector P2 as 'user-defined', are used for the axis periphery bus."
- "The axis periphery card is accessed by the software interpolator via the axis periphery bus."
- "Four equivalent measurement circuits are realised with the following functions:
  - The analog output value with a voltage range of -10V to +10 V is generated by a DA transducer with 16 bit resolution.
  - A watch dog timer is provided to ensure system safety, switching off the analog output value in the event of an fault, which is also possible by evaluating a common breakage signal. In the event of a failure of the 15V supply, the analog outputs are disconnected from the connector."
- "Inputs for connecting incremental measuring systems (with digital output) are provided for recording the position, the signals of which are fed to a 32 bit counter evaluation facility. The evaluation circuitry is a universally programmable 32 bit counter gate array. The signal lines of the measuring systems are monitored for cable breakage and short circuiting."
- "The supply voltage for external measuring systems is led by a separate +5V voltage on the axis periphery bus to the periphery connector and the encoder sense line is monitored by each incremental encoder and in the event of a fault, a counter RESET can be generated. The measuring system voltage can be buffered using an external accumulator, the entire measuring circuit remaining supplied when the 5V system voltage is switched off to enable buffer operation of the position recording."
- "The counter readings of all axes can be simultaneously strobed and the analog values read out via a sync signal from the axis periphery bus."
- "A common measuring probe signal for all four axes can strobe the counter reading."
- "A software and hardware controlled reference pulse enable logic controlled by the reference cam signal from the axis is available for moving to the reference point."

### PDF page 46 — manual page 7-46 (VAP4, continued)

- "Each measuring circuit is equipped with a status and command register as interface to the axis periphery bus."
- "The following common functions are available for all four measuring circuits:"
- "A bus coupling with 16 bit data capacity address decoder and HEX rotary switch for address setting is available for connecting the axis periphery bus."
- "For connecting two measuring probes, two isolated inputs are provided, only one of which can be enabled at any one time."
- "System security is ensured by an isolated input with debouncing for a 'breakage signal'."
- "A relay output is switched via the command register and can be used on the periphery connector to form a string. The WDOG signal from each axis switches the relay output." (i.e. a contact for the E-stop/ready chain)
- "Four differential analog inputs, switched via a multiplexer on a 12 bit AD transducer, are available for recording analog values in the range up to ± 10 V."
- "The axis periphery card with four measuring circuits requires 2 printed circuit board slots in the VME system and 1 printed circuit board slot for two measuring circuits."
- "A 25-pole SUB-D connector as well as an 11-pole screw-in header are provided as periphery connectors."
- "The axis periphery card is designed with SMD and conventional components."

### PDF page 47 — manual page 7-47

**Block diagram** (VAP4, drawn rotated 90°)
Common section (top of rotated drawing):
- Analog inputs -> "MUXER 4 to 1" -> "A/D transducer 12 bits" -> bus.
- Digital output <- "Relais" (relay) <- "Control logic".
- Measuring probe inputs (2) -> "Opto coupler" -> "Meas. probe eval." -> "Meas. probe signal" line.
- Breakage signal input -> "Opto coupler" -> "De-bouncing" -> "Breakage eval." -> "Breakage signal" line.
- Measuring system supply <- "Buffer mode switch-over" fed from "+5V meas. system supply" and "+5V-VME" from the axis periphery bus; output "Measuring circuit +5V".
- "Status-/Command-Register" (common).
- "Hex switch, 1 from 16" -> "Adress-Decoder" (as printed); "Bus coupling" (Control signal; Data, addresses) <-> "Axis periphery bus"; "Sync logic" <- SYNC; "15V-FAIL" line.
Per-axis section ("Axis 1"; "Axis 2 - 4" identical, dashed box):
- Analog output <- "Relay" contact <- "16Bit-DAC"; "WDOG" block controls the relay and DAC.
- Measuring system input -> "Signal receiver" -> "32Bit Counter" (latch signals [unreadable small labels]).
- Reference cam -> "De-bouncing" -> "Reference pulse logic" (CLR, Reset) -> counter; "Reference pulse" from encoder.
- Measuring system -> "Generator sense monitoring".
- "Status-/Command-Register" per axis.

### PDF page 48 — manual page 7-48

**Technical features**
The following applies to all four measuring circuits:
- Access via axis periphery bus
- 1K addresses per axis periphery card
- Address set via HEX rotary switch
- 16 bit data bus connection
- Axis periphery clock is used for the logic
- Freely switchable sync logic for SYNC signal
- Buffer operation of the measuring system and its evaluation when the 5V system voltage is switched off, is possible due to an external accumulator
- 4 differential analog inputs via multiplexer on 12 bit AD transducer
- Two isolated measuring probe inputs with LED display
- Isolated breakage signal input with debouncing and LED display
- Relay output for emergency stop chain with LED display
- Relay output is switched off by the WDOG signals of the four axes in the event of a failure

Separately realised for each measuring circuit:
- 16 bit DA transducer with successive OP amplifier and via relay contact on the periphery connector
- DA transducer output is switched to 0V via WDOG or a breakage signal and the relay contact is opened in the event of a faulty ± 15V supply
- Synchronous loading of the DA transducer with SYNC signal
- 32 bit counter (gate array) for actual value recording
- Synchronous strobing of the counter reading in latch 1 with SYNC signal
- Strobing of the counter reading by measuring probe signal in latch 2
- Measuring system signals A, B, R are monitored for short circuits and interruptions
- Monitoring of the encoder sense line for undervoltage
- Isolated reference cam input with debouncing and LED display
- Hardware and software controlled reference pulse logic
- Adjustable WDOG timer
- Status and command register
- Programmable LED's

### PDF page 49 — manual page 7-49

**Connector type and pin assignment for the digital inputs/outputs**
"The connector type for connecting the digital inputs and for the relay output is designed as an 11-pole header is provided for the screw connection terminal."

Connector X2 (VAP4, 11-pole screw header):

| Pin | Function |
|---|---|
| 1 | relay contact 1 |
| 2 | relay contact 2 |
| 3 | measuring probe 1 |
| 4 | measuring probe 2 |
| 5 | breakage signal |
| 6 | reference cam axis 1 |
| 7 | reference cam axis 2 |
| 8 | reference cam axis 3 |
| 9 | reference cam axis 4 |
| 10 | 0V potential of the 24 V |
| 11 | 0V potential of the 24 V |

**Connection conditions**
- "The cables specified by the encoder manufacturers should be used for connecting the measuring systems."
- "The GND pins on the connectors are not intended for connecting cable screening."

**Displays**
- "The front of the module is provided with measuring probes 1 and 2, reference cams 1 to 4 and breakage signal for the digital input signals, and an LED for the relay output. In addition, a software programmable LED is located on the front of the module for each axis."
- "The input signal LED's are lit when the signals with 24V are applied at the input connector." (inputs are 24 V)
- "Once the RESET is activated, the LED for the relay output extinguishes."
- "Once the RESET is activated, the programmable LED's light up."

**Technical data and module construction — Technical data**
Supply voltages:

| Voltage | Tolerance | Note |
|---|---|---|
| +5 V | ±5% | |
| +15 V | ±5% | |
| -15 V | ±5% | |
| +5.2 V | +0 / -3% | (for external measuring systems) |

Current consumption ("Currant consumption" as printed):

| Current | for |
|---|---|
| 1.8 A | +5V |
| 180 mA | +15V/-15V |
| 300 mA | +5.2V in buffer operation without incremental encoder |

### PDF page 50 — manual page 7-50

**Configuration** — "Axis periphery card in double European format" (component layout, rotated)
- Left (front) edge, top to bottom: "2 x double LED"; X1 "25-pole SUB-D pins" with X4 "25-pole pin strip" behind it; "4 x double LED"; X2 "11-pole pin strip"; X3 "25-pole SUB-D pins" with X5 "25-pole SUB-D pin strip" (as printed).
- DAC modules: "DAC 7970P"/"DAC 797KP" [uncertain type, probably Burr-Brown DAC 707/797-type 16-bit] for Axis 1, Axis 2, Axis 3, Axis 4.
- Line receivers "V23848" [uncertain, small] x5 near X1.
- "IOCTR" [uncertain]; counter gate arrays "SC5C1xxx / 8873534" [uncertain] for Axis 1, Axis 2, Axis 2 (as printed, presumably Axis 3), Axis 4.
- "REFLOG Axis 1", "REFLOG Axis 2", "REFLOG Axis 3", "REFLOG Axis 4" (reference-pulse logic PALs).
- Oscillator "20.0 MHz" [uncertain]; "RANGE" jumper; "ADCCN" [uncertain]; "ACTIVE"/"ACTIME" [uncertain]; "1.8 MHz"/"4.8 MHz" [uncertain]; jumpers "1.75 / 3.5 / 7 / 14" (WDOG time [uncertain]).
- "DACWDOG Axis 1..4" PALs; "BUSKOG"/"BUSLOG" [uncertain]; "SOCVAP", "SOC1VAP" [uncertain]; "ADCVAP".
- Hex rotary switch (address) with label "HEX" [uncertain]; "15V" test points / fuses near P1 and P2.
- Board marking: "GILDEMEISTER 0.081.851-2 VAP4" [uncertain digits].
- P1 "96-pole blade connector" (printed "90-pole" [uncertain, probably 96]) and P2 "96-pole blade connector".
- Grid numbers 1-9 along the top edge.

### PDF page 51 — manual page 7-51

**Front of module** (VAP4)
- "---- 4 axes ---- = 2 printed circuit board slots"
- "- 2 axes - = 1 printed circuit board slots"
- Front panel, left slot: LEDs L1, L2, L3, L4 (programmable per-axis LEDs); X1 (25-pole SUB-D); LEDs "BR" (breakage signal), "AOK" (relay output / axes OK [uncertain meaning]), "M1", "M2" (measuring probes), "R1", "R2", "R3", "R4" (reference cams); X2 (11-pole screw header, pins 1..11); X3 (25-pole SUB-D).
- Right slot (only for 4-axis version): X4 and X5 (25-pole SUB-D).
- Note: the pinout of the 25-pole axis connectors X1/X3 (analog command + encoder per axis) is NOT given on these pages.

### PDF page 52 — manual page 7-52

**7.8 IO48 input/output module**
- "The IO48 module is an ELTROMATIC I/O module with 32 digital inputs and 16 digital transistor outputs. It is coupled to a PLC interface via the serial ELTROMATIC bus, e.g.
  - BST EUI (mpst module)
  - BST PCE (PC board)
  - BST SI188 (IPIN with LEB according to ELTEC specification)"
- "The 80C32 microcontroller controls the inputs and outputs and operates the serial ELTROMATIC interface."
- "An external 32KByte SRAM is provided for the parameter and SERVICE channel function. The I/O power section is designed for a 24V operating voltage and is isolated from the electronic section."

**Block diagram**: "15POL SUB-D ELTROMATIC INPUT" and "15POL SUB-D ELTROMATIC OUTPUT" (bus in / bus out, looped through); "Ux / 5V" regulator; "QOS 16MHZ" [uncertain "QOS"]; "80C32" with "SIO" and "PORT"; "RS485" transceiver; "LOGIC"; "SRAM 32Kx8BIT"; "EPROM 32Kx8BIT"; buses D0..7, A0..15, CONTROL. Output path: "16 TRANSISTOR OUTPUTS" — driver -> optocoupler -> transistor switch -> plug-in terminals; "TRANSISTOR STATUS" fed back via optocoupler. Input path: "32 DIGITAL INPUTS" — plug-in terminals -> input circuit (LED) -> optocoupler -> receiver -> bus.

### PDF page 53 — manual page 7-53

**ELTROMATIC system architecture**
- "ELTROMATIC is a decentral I/O system, the individual modules of which represent complete function units which are coupled to a PLC unit via the serial ELTROMATIC bus."
- "The compact boards, almost exclusively designed in the SMD technique, are accommodated in plastic casing elements which conveniently engage onto DIN mounting rails."
- "Typical design of an ELTROMATIC module": plan view (I/O area, Controller area, ELTROMATIC-Bus in and out connectors, width b, length l) and side view (Connector top and bottom).
- "The uniform width b is 111 mm. The length l is dependent on the type of module."
- "Single lines of digital I/O signals (24V) are connected via screw or crimp plug-in terminals. SUB-D connectors are used to couple the ELTROMATIC bus connection, two identical plug-in connections being available to allow the bus connection to be routed across the module (incl. power supply)."

### PDF page 54 — manual page 7-54

**Technical features**
Inputs:
- 32 digital inputs, addressable in four groups of 8 bits each
- Isolation
- 20mA input current at 24 V ± 15%
- software debouncing (1ms)
- one status LED per input
- plug-in terminal blocks (codable)

Outputs:
- 16 transistor outputs, electrically isolated in eight blocks with 2 outputs each
- Isolation
- 1.5 A / 24 V = constant load, simultaneity factor 50%
- one status LED per output
- short circuit and overtemperature protection
- reverse readable transistor status
- no-load switching of all outputs in RESET and in the event of a failure
- suppressor elements must be located on the load
- plug-in terminal blocks (codable)

Electronic section:
- 80C32 - 16 MHz
- 32 KByte EPROM (27C256 or 27C512) and 32 KByte SRAM
- two freely programmable LED's
- serial RS485 interface (half-duplex operation)
- separate terminals for the power supply (parallel to 15-pole SUB-D connector)
- power supply by way of longitudinal voltage regulator with on-board voltage monitoring
- module address can be set using HEX switch

### PDF page 55 — manual page 7-55

**Technical data**
"The printed circuit board is integrated with a plastic interconnect support module which can be snap fitted onto a DIN EN mounting rail."

Mechanical data:

| Item | Value |
|---|---|
| Casing/fixture | Plastic interconnect support module with universal engagement foot for DIN EN mounting rail NS35/7,5 EN 50022 or NS 32 50035 |
| Electrical connections | 15-pole SUB-D connector (jack) and 2 x 2 pole, 1 x 3 pole, 1 x 4 pole and 8 x 8 pole plug-in terminal blocks |
| Dimensions (incl. casing) | 284 x 111 x 50 mm (W x D x H) |
| Weight | approx. 0.55 kg |

Electrical data:

| Item | Value |
|---|---|
| Supply voltage | VEXT 7.5 V ± 15% |
| Current consumption | IEXT 250mA (typ.) |
| Ambient temperature | 0...55°C |
| Dimensions (printed circuit board) | 282.6 x 107.6 mm (W x D) |

### PDF page 56 — manual page 7-56 (IO48, continued)

**Electrical parameters**: "A serial ELTROMATIC interface is realised on the IO48 according to the RS485 standard."

**Interface function**
- "Data exchange between the PLC and the I/O's takes place via the serial ELTROMATIC interface (RS485). The transmission speed is 500 kbit/s in half-duplex operation."
- "The I/O's are connected via codable plug in terminal blocks."

**Connecting conditions**
- "The serial ELTROMATIC bus interface and the power feed are connected via a 15-pole SUB-D connector (jack) X12. A second 15-pole SUB-D connector (jack) X13 is connected one to one with X12 and serves for coupling (continuous looping) of additional I/O modules (max. eight IO48 with **one** power feed)."
- "If more than eight I/O modules are to be accessed, the power supply must be realised in a star pattern via connection X11."

15-pole SUB-D connector (jack) X12 and X13:

| Pin | Name | Function |
|---|---|---|
| 11 | VEXT | Supply voltage line entry |
| 12 | VEXT | Supply voltage line entry |
| 4 | GNDEXT | SUPPLY - GROUND (VGND) |
| 5 | GNDEXT | SUPPLY - GROUND (VGND) |
| 1 | SCH | Cable screening |
| 13 | SYSOKOUT | SYSTEM OK (Open Drain) |
| 2 / 9 | (bridge symbol) | Short circuit if connector coupled to PLC |
| 15 | /SYSOK (-) | Serial ELTROMATIC bus connection interface |
| 8 | /SYSOK (+) | Serial ELTROMATIC bus connection interface |
| 14 | /OUTEN (-) | Serial ELTROMATIC bus connection interface |
| 6 | /OUTEN (+) | Serial ELTROMATIC bus connection interface |
| 7 | GND485 | Signal - Ground (SGND) |
| 10 | DATA (-) | Serial ELTROMATIC bus |
| 3 | DATA (+) | Serial ELTROMATIC bus |

Note: compared with the SI188 X2 table (page 7-40), /SYSOK and /OUTEN pin numbers differ (SI188: /SYSok(-)=14, /SYSok(+)=6, /OUTen(-)=15, /OUTen(+)=8; IO48: /SYSOK(-)=15, /SYSOK(+)=8, /OUTEN(-)=14, /OUTEN(+)=6). Both are transcribed as printed; verify with a meter before reusing the cable.

"Connector X11 serves as supply voltage feed, e.g. if more than eight I/O modules are to be operated (see below):"

SLA3-180 connector X11:

| Pin | Name | Function |
|---|---|---|
| 1 | GNDEXT | Supply voltage line entry (VGND) |
| 2 | VEXT | Supply voltage line entry |
| 3 | SCH | Cable screening |

### PDF page 57 — manual page 7-57

**Configuration** (IO48 board layout, rotated)
- Two 15-pole SUB-D connectors (X12, X13) at the short ends [labels tiny, uncertain which is which].
- Small terminal blocks: "DIAG / OPTO" [uncertain], "OUT 0V" / "0V" [uncertain], "SCH V GND EXT" (X11 power feed), "IN 0V" [uncertain].
- Output terminals (left long edge): 8 blocks "OUT" each with "+" and two outputs, numbered 0..15 (e.g. "OUT 0 1", "OUT 2 3", ..., "OUT 14 15"), each pair with its own "+" supply (eight isolated blocks of 2 outputs).
- Input terminals (right long edge): X1 = IN 0..7, X2 = IN 8..15, X3 = IN 16..23, X4 = IN 24..31 (8-pole plug-in blocks, with per-input LEDs).
- 80C32 (40-pin DIL), EPROM, SRAM, HEX address switch, regulator — individual IC labels [unreadable].
- Board marking at top [unreadable].

### PDF page 58 — manual page 7-58

**7.9 Control panel components — 7.9.1 Monitor unit**
"The monitor unit is a completely assembled and tested module, consisting of the following components:
- 14" colour monitor
- Facia of integral skin foam with glued-in filter glass. Facia colour version: NCS - 5502 (grey). Screen printed company logo and symbols for operation devices.
- Assembly casing
- Maximum 4 operating devices. 2 devices are provided in standard design. Predetermined breaking points are incorporated in the facia, allowing for the installation of 2 other devices."
"Once installed, the protection type attained is IP 54. The user must ensure that the front fastening is sealed. (Blind fastening holes or sealing washers.)"
"Total weight of the unit: approx. 15 kg"

### PDF page 59 — manual page 7-59

**Electrical connection**: "A terminal strip is attached to the right-hand side of the monitor housing for connecting the supply voltages and the signal lines."

Mains connection:

| Item | Value |
|---|---|
| Mains voltage | 230 V ± 10% |
| Mains frequency | 48...62 Hz |
| Power capacity | 80 W |
| Connection cross sectional area | 1.5 mm² Cu |

Video connection: "The video is connected directly to the 15-pole SUB-D connection socket of the monitor (see point 1.1.6.2)".

Environmental conditions:
- Ambient temperature: in operation 5...60°C; out of operation -25...70°C
- Air humidity according to DIN VDE 0160: relative humidity 15...85% without condensation
- Mechanical load — vibration: test conditions: monitor is operational and switched on; curve shape of the shaking motion: sinusoidal; frequency range 10...150 Hz; number of axes: 3 (vertically superimposed); max. shaking amplitude 0.15 mm; max. acceleration 2 g
- Mechanical load — shock/impact: test conditions: monitor is operational and switched on; number of knocks: 3 x 1000 (1000 each in 3 directions vertically superimposed); max. acceleration 10 g; pulse duration 16 ms

### PDF page 60 — manual page 7-60

**7.9.2 14" colour monitor**
- "It is a 14" colour industrial monitor with metal chassis for installing in customized enclosed housing."
- "The monitor is designed for connection to a VGA interface with 640 x 480 pixel resolution."
- "It is connected via a 15-pole SUB-D connector jack with VGA signal source."
- "The mains voltage is connected via screw terminals."
- "The monitor provides the supply voltage of +5V (with screw terminal connection) for supplying the control panel keyboard."
- "The operating elements: brightness control and contrast control are located on the cross strut at the back."

Technical data — Electrical data: Mains supply*

| Item | Value |
|---|---|
| Mains voltage | 230 V ± 15% |
| Mains frequency | 48...62 Hz |
| Power capacity | max. 80 W |
| Connection | 3-pole screw terminal labelled L N PE |
| Short circuit fuse | safety fuse |

Electrical data: Current supply output — "There is an external current supply output which may exclusively used for supplying the control panel keyboard! (Automation specification)"

| Item | Value |
|---|---|
| Voltage | +5 V, -0%, +5% |
| Current ("Currant") | max. 1 A |
| Connection | 2-pole screw terminal labelled +5V, 0V |
| Short circuit fuse | safety fuse |

Electrical data: Video connection and timing — Connector type and assignment: 15-pole SUB-D jack (2-row)

| Pin | Signal | Pin | Signal |
|---|---|---|---|
| 1 | Video RED | 9 | NC |
| 2 | Video GREEN | 10 | GND-VSYNC |
| 3 | Video BLUE | 11 | NC |
| 4 | NC | 12 | NC |
| 5 | GND RED | 13 | /HSYNC |
| 6 | GND GREEN | 14 | /VSYNC |
| 7 | GND BLUE | 15 | NC |
| 8 | GND HSYNC | | |

- Signal type: RGB analog according to VGA standard (0.7 V - video positive); HSYNC-, VSYNC-TTL level negative (< 0.6 V - low; > 2.4 V - high)
- Terminating resistor: Video signals with 75 Ω; SYNC signals with ≥ 330 Ω

### PDF page 61 — manual page 7-61 (14" colour monitor, continued)

"The monitor is compensated to the VGA timing of the graphics IPIN VGI. Refer to the description of the graphics IPIN VGI for the timing."

- Ambient temperature*: in operation +5 ... 65°C; out of operation -25 ... 70°C
- Air humidity*: relative humidity 10 ... 90% without condensation
- Mechanical load: vibration* — test conditions: monitor operational and switched on; curve shape of shaking motion: sinusoidal; frequency range 10 ... 150 Hz; number of axes: 3 (vertically superimposed); max shaking amplitude 0.15 mm; max. acceleration 2 g
- Mechanical load: Shock/impact* — test conditions: monitor is operational and switched on; number of knocks: 3 x 1000 (1000 each in 3 directions vertically superimposed); max. acceleration 10 g; impulse duration 16 ms

**Attention:** "<u>This data exclusively applies to the individual components of the MONITOR described here!</u> It does not provide operating values for entire control systems or subassemblies! The respective data can be found under 'Installations and operating conditions' or in the description of the individual units."

### PDF page 62 — manual page 7-62

**7.9.3 Keyboard unit**
- "The keyboard unit is a completely assembled and tested module, forming the control panel components together with the monitor unit."
- "The unit consists of the following components:
  - BFT 90 control panel keyboard
  - Handwheel
  - Keyboard frame of integral skin foam, colour version NCS 5502 (grey)"
- "A special sealing technique ensures that the keyboard is suitable for industrial use. The user must ensure that the facia is sealed (blind fastening holes or sealing washers)."
- "All actuating elements are let into the keyboard facia and thus protected from unintentional actuation."
- "Total weight of the unit: approx. 2.5 kg"

**Mechanical construction**
- "Assembly dimensions: H x W x D = 22 x 520 x 135 [mm] (see fig.)"
- "The BFT90 control panel keyboard and the handwheel are mounted to the keyboard facia."

### PDF page 63 — manual page 7-63

**Technical data** (BFT90 keyboard)

| Item | Value |
|---|---|
| Supply voltage | +5 V ± 3% |
| Current consumption ("Currant") | 300 mA maxi. |
| Ambient temperature ("Ambiant") | 0°C - 55° C |
| Max. transmission speed | 19200 baud |
| OK button | push button with concave key, Type: ZDTK BL; contact element, Type: Z02F; n/o contact 250 V/2.5 A |
| Keys | Mechanical key with integrated diode, type: RAFI RS 76 M; Sealing (IP54) by way of sealing cap and mounting plate |

**1.2.6.3.2 Configuration** (section number as printed, left over from another manual)
Keyboard rear view with connectors X1 (2-pin), X2 (4-pin [uncertain count]), X3, X4 (long), X5, X6 (2-pin), X7 (handwheel, left side, pin 1 marked), "Setting field" / "Port1" (left), "Setting field" / "PW" (right) [uncertain].
Front key layout (see page 7-64).
- X1 - voltage supply (option)
- X2 - voltage supply
- X3 - keyboard extension connector (option)
- X4 - central connector
- X5 - level transducer (option)
- X6 - signal generator (option)
- X7 - handwheel connection

### PDF page 64 — manual page 7-64

**OK button**
- "An OK button mounted on the mounting plate is offered as a safety measure for the operating personnel. Its switch contact is designed as a n/o contact."
- "The OK button can be equipped with a maximum of 3 contact elements. Connection is made via FASTON flat blade connectors."

**Line and column matrix of the machine keyboard**
- "The machine keyboard consists of 50 mechanical keys with integrated diode. Of these 50 keys, 6 keys can be lit by an LED each located above each key. An IP 54 sealing should be achieved in conjunction with a mounting plate and sealing caps (RAFI)."
- "The key caps are sprayed in two colours without status display."

Upper figure: key legends — 2 rows of 3 keys with LEDs (top left); a large square key; "[0]", "~ 0", "[I]" keys (spindle/feed symbols [uncertain]); override keys "↶ 0 ↷"; two spindle keys; jog keys "↑", "↗ +Y", "←", "∿" (rapid [uncertain]), "→", "↙ -Y", "↓"; "Esc", ">"; "Del", "Alt", "Ins"; "⇑ ▲ ⇓" and "◀ ▼ ▶" cursor keys; screen/reset/info keys "▢ ↯ (i)" [uncertain symbols]; numeric pad 7 8 9 / 4 5 6 / 1 2 3 / - 0 . ; a wide blank key.

Lower figure: key number Tn with matrix column S (Spalte) and row Z (Zeile). All readable entries follow the rule S = (n-1) mod 8, Z = (n-1) div 8 (8 columns S0..S7 [only S0..S6 used], rows Z0..Z7). Codes as printed, grouped by position:

| Position (matches upper figure) | Key codes (T / S / Z) |
|---|---|
| Top-left lit keys, row 1 (LED1, LED2, LED3) | T7 S6 Z0; T15 S6 Z1; T23 S6 Z2 |
| Top-left lit keys, row 2 (LED4, LED5, LED6) | T31 S6 Z3; T39 S6 Z4; T47 S6 Z5 |
| Large square key | no code printed [uncertain] |
| Bottom-left row (0 / ~ / I) | T2 S1 Z0; T10 S1 Z1; T18 S1 Z2 |
| Override row (↶ 0 ↷) | T26 S1 Z3; T34 S1 Z4; T42 S1 Z5 |
| Two spindle keys | T50 S1 Z6; T58 S1 Z7 |
| Jog ↑ / ↗+Y | T6 S5 Z0; T54 S5 Z6 |
| Jog ← / ∿ / → | T14 S5 Z1; T22 S5 Z2; T30 S5 Z3 |
| Jog ↙-Y / ↓ | T46 S5 Z5; T38 S5 Z4 |
| Two keys top-middle (symbol keys) | T3 S2 Z0; T11 S2 Z1 |
| Esc / > | T5 S4 Z0; T13 S4 Z1 |
| Del / Alt / Ins | T25 S0 Z3; T33 S0 Z4; T41 S0 Z5 |
| ⇑ ▲ ⇓ | T49 S0 Z6; T21 S4 Z2; T57 S0 Z7 |
| ◀ ▼ ▶ | T29 S4 Z3; T37 S4 Z4; T45 S4 Z5 |
| Top-right three keys | T1 S0 Z0; T9 S0 Z1; T17 S0 Z2 |
| 7 8 9 | T4 S3 Z0; T12 S3 Z1; T20 S3 Z2 |
| 4 5 6 | T19 S2 Z2; T27 S2 Z3 [print partly reads S3, rule gives S2]; T35 S2 Z4 [uncertain] |
| 1 2 3 | T43 S2 Z5; T51 S2 Z6; T59 S2 Z7 |
| - 0 . | T28 S3 Z3; T36 S3 Z4; T44 S3 Z5 |
| Wide key (bottom right) | T52 S3 Z6 |

### PDF page 65 — manual page 7-65

**Electrical connection** (diagram)
- Monitor unit terminal "+5V 0" -> keyboard unit X1 (2-pole) — +5 V supply.
- Keyboard unit X4 -> "Interface to microcontroller".
- "Supply voltage: +5V (the supply voltage is provided by the monitor!)"
- "Isolated contact of the OK button: 1 n/o contact 250 V; 2.5 A"

**Environmental conditions**
- "The keyboard installation site must conform to protection type IP54."
- Ambient temperature: 5 ... 50°C
- Air humidity according to DIN VDE 0160: relative humidity 15 ... 85% without condensation

### PDF page 66 — manual page 7-66

**Block diagram** (BFT90 keyboard electronics)
- "Signal generator" -> Processor.
- "Extension keyboard" <-> LED driver, LED receiver [uncertain direction], column/line lines.
- "Machine keyboard" (key matrix sketch) <- "Column driver"; -> "NUX" (as printed, = MUX) -> "Line receivers"; LEDs <- "LED driver".
- Blocks: "LED driver", "LED driver", "LED receiver", "Column driver", "Line receivers" on the internal bus -> "Processor" and "EPROM".
- "External unit 1* V.28 without control signals" (* "e.g. alpha keyboard") -> "Level transducer" (V.28 <-> 20mA).
- Processor -> "SUB-D Handwheel"; Processor and level transducer -> "Central connector" ("Control connector").
- "Voltage supply" -> "+5V".

### PDF page 67 — manual page 7-67

**Technical features** (BFT90 keyboard)

| Item | Value |
|---|---|
| Microcontroller | 80C31 |
| Bus width: Data / Addresses | 8 bits / 16 bits |
| Clock frequency | 14.7456 MHz |
| Data memory: internal / external | 128 bits (as printed) / --- |
| Program memory | maximum 64 KByte external program memory |
| Serial interface | The microcontroller 80C31 is equipped with an asynchronous serial interface, allowing it to program different transmission rates (assignment of a timer). Passive current loop interface (20mA) without isolated handshake signals |
| Transmission rate | max. 19200 baud |
| Timers | Two freely programmable 16 bit timers |
| Interrupt inputs | 2 x interrupt inputs. These interrupt inputs are not circuited. |
| Ports | 2 ports for keyboard matrix (1 x 8 bits for column outputs and 1 x 8 bits for line inputs); 14 bit programmable output ports for LED's (6 outputs are available on LED's; 8 outputs are externally available via a pin strip) |
| Keyboard | 8 x 8 keyboard matrix with 50 mechanical keys with integrated diode |
| Level transducer | 1 x serial level transducer V.28 <-> 20 mA without isolated handshake signals |
| OK button | OK button with 1 * n/o contact |
| Supply voltage | +5V ± 3% |
| Operating temperature | from 0° to 55° Celsius |
| Printed circuit board dimensions | 372 mm * 162 mm |
| Mounting plate dimensions | 388 mm * 178 mm |

### PDF page 68 — manual page 7-68

**7.9.4 Handwheel**
- "The handwheel is a universal electronic pulse generator, providing two square wave signals with 100 flank changes at 100 engagements/revolution."
- "The two square wave signals are offset to each other by 90° to allow an evaluation circuit to determine the position and the direction of rotation. The signals are generated by way of optical scanning. The mechanical construction with wear-free and vibration resistant engagement guarantees perfect functioning."
- "A 10-pole ribbon connector is provided as a signal connector pin."

Technical data — Electrical data: Signal connector pin. "Connector pin type and assignment: 10-pole ribbon connector according to DIN 41651"

| Pin | Signal |
|---|---|
| 1 | NC |
| 2 | 0V |
| 3 | anode of the photo-coupler diode |
| 4 | 0V |
| 5 | anode of the photo-coupler diode |
| 6 | 0V |
| 7 | channel A |
| 8 | 0V |
| 9 | channel B |
| 10 | handwheel status |

Electrical data: Earth connection point — "A push-on blade for tab receptacles 6.3 mm DIN 46342 T1 is provided on the housing for connection to the process unit."

Electrical data: Output signals and signal levels — "Output signals: Channels A and B as 'open collectors', square wave signals offset to each other by 90°, 2 x 25 pulses/revolution at 100 engagements." (i.e. 25 quadrature cycles = 100 edges = 100 detents per revolution)

### PDF page 69 — manual page 7-69

"Output signals for clockwise rotation via the following diagram": timing diagram Channel A / Channel B, phase offset "90° ± 30°", "min. 10°" between edges, "Engagement positions" (detents) marked at each edge.

**Mechanical data — Handwheel dimensions**

| Item | Value |
|---|---|
| Casing diameter | 63 mm |
| Casing depth | 73 mm |
| Button diameter | 90 mm |
| Button depth up to retention pin | 50 mm |

**Environmental conditions***
- Ambient temperature: 0...65°C
- Protection type according to DIN 400050 (as printed, probably DIN 40050): from front IP 65; from back IP 50

"* Note: <u>This data refers exclusively to the individual HANDWHEEL components described here!</u> It does not provide operating values for entire control system or subassemblies! The respective data can be found under 'installation and connection conditions' or in the description of the individual units."

### PDF page 70 — manual page 7-70

Blank page (only running header "7. CNC hardware modules - ELTROPILOT controls" and footer "Page 7-70 / Doc. No.: DAAAE0G0 / November 1993 / Electrical technical instructions CTX twin"). End of chapter.

## For the retrofit

What these pages establish (and what they do not):

- **Drive command interface is analog ±10 V, not SERCOS.** The axes are driven by the VAP4 axis periphery card: per axis one 16-bit DAC, output range -10 V ... +10 V, routed through a relay contact to the 25-pole axis connector. The VIC30 text explicitly lists SERCOS only as "planned" for the axis periphery bus. So the original Indramat axis drives (types not named on these pages, see pages 1-35 and the drive chapters) must be analog-velocity-command drives. LinuxCNC can drive them directly with any ±10 V analog output card (e.g. Mesa 7i77, 7i83/7i33-type) in velocity mode with the position loop in LinuxCNC (PID).
- **Position feedback is incremental digital A/B/R (TTL/RS422-type "measuring systems with digital output")**, counted by a 32-bit counter per axis, with monitoring of A, B, R for short/open circuit and of the encoder sense line for undervoltage. Encoder supply is a separate +5.2 V (+0/-3 %), buffered by an external accumulator so position is kept with the system 5 V off. The resolution (lines per rev / scale pitch) and whether the feedback comes from motor encoders or linear scales, or from the Indramat drive's encoder emulation, is not given here. Check the 25-pole SUB-D X1/X3/X4/X5 pinout on the real card or the cable diagrams. For LinuxCNC: differential encoder counter inputs (e.g. Mesa 7i77) with 5 V encoder supply; check the drive manual whether the encoder signal is an emulated output of the drive (DDS/TDM with a DAE-type module) or a direct motor/scale encoder.
- **Reference point**: separate 24 V reference cam inputs per axis (VAP4 X2 pins 6-9) gated with the encoder reference (R) pulse. For LinuxCNC: home switch inputs (24 V) plus index pulse (`index-enable`) homing.
- **Probe**: two isolated 24 V measuring probe inputs (X2 pins 3/4) that latch the counters. In LinuxCNC: `motion.probe-input`; a hardware latch needs an encoder card with probe latch, otherwise servo-period resolution.
- **Safety / E-stop / enable**: VAP4 has a relay output "for emergency stop chain" (X2 pins 1-2), switched off by the watchdog (WDOG) of any of the four axes or by the "breakage signal" input (X2 pin 5, 24 V); the DAC outputs go to 0 V on WDOG/breakage and the relay opens on a faulty ±15 V supply. This contact belongs in the drive-enable / E-stop chain. A LinuxCNC retrofit needs the same: a watchdog-controlled contact (e.g. Mesa watchdog + a relay output) in the controller-ready chain, plus drive enable and drive-ready signals, which are not documented on these pages (they are in the Indramat/machine wiring diagrams). Holding brakes are not described here.
- **Spindle drive interface**: not described on these pages. The VAP4 has four analog outputs per card (two slots) plus four 12-bit ±10 V analog inputs. The main spindles of the twin lathe are probably driven by an analog ±10 V output of a VAP4 channel too, but this must be checked in the wiring diagrams. [uncertain]
- **PLC I/O**: the machine I/O is on ELTROMATIC IO48 modules (32 inputs 24 V/20 mA, 16 isolated transistor outputs 1.5 A/24 V each), on a proprietary RS485 bus at 500 kbit/s half-duplex (15-pole SUB-D, signals DATA±, /SYSOK±, /OUTEN±, VEXT 7.5 V supply). The protocol is Gildemeister-proprietary (80C32 firmware), so under LinuxCNC the IO48 modules would have to be replaced (e.g. Mesa 7i76/7i84 or similar 24 V I/O) and rewired terminal by terminal, or the bus protocol reverse-engineered. The I/O terminals themselves are plain 24 V and can be reused as a wiring reference. Note the pin-number discrepancy for /SYSOK and /OUTEN between the SI188 (page 7-40) and IO48 (page 7-56) tables.
- **Operator panel**: 14" VGA monitor (640x480, 15-pole SUB-D, standard VGA pinout with separate grounds; 230 V; supplies +5 V/1 A to the keyboard) can be reused on any VGA output. The BFT90 keyboard is an 80C31 serial device (20 mA current loop, 19200 baud, proprietary protocol) with an 8x8 diode matrix of 50 RAFI RS 76 M keys (key Tn = column S (n-1) mod 8, row Z (n-1) div 8), 6 LEDs; for LinuxCNC the matrix can be scanned directly (e.g. Mesa 7i73 or a microcontroller keyboard) or the 80C31 protocol decoded. The OK button is a separate n/o contact (250 V/2.5 A) on FASTON tabs.
- **Handwheel**: 100 detents/rev, 25 quadrature cycles (100 edges) per rev, open-collector A/B, opto-isolated via "anode of photo-coupler diode" pins, 10-pole ribbon (DIN 41651): pin 7 A, pin 9 B, pin 10 status, 2/4/6/8 0V, 3/5 anodes. In LinuxCNC: encoder input with pull-ups, scale 100 counts/rev in x4 mode = 1 count per detent.

For replacing the drives: the existing control expects nothing more from a drive than a ±10 V velocity input, an incremental A/B/R position signal and enable/ready contacts. Any modern analog-input or step/dir servo drive can therefore replace an Indramat unit under LinuxCNC, provided the motor (and its feedback, often resolver/sin-cos on Indramat MAC/MDD motors) is replaced with it or is supported. Drive types, plug-in modules (e.g. DAE encoder emulation, ANA command module) and the 24 V enable/ready wiring must be taken from the other chapters, not from these pages.
