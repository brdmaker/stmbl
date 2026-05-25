# SSLBP / LBP Protocol Reference

Smart Serial Local Bus Protocol (SSLBP) is Mesa Electronics' point-to-point
serial protocol for connecting smart servo interface cards (such as STMBL) to a
Mesa FPGA host card. The base framing and command set is called LBP (Local Bus
Protocol); SSLBP adds the discovery and process-data RPCs on top.

This document is synthesised from the Mesa 7I73 product manual, the LinuxCNC
hostmot2 sserial driver source, and the STMBL implementation in
`src/comps/sserial.c` and `inc/sserial.h`.

---

## Physical Layer

| Parameter     | Value                                    |
|---------------|------------------------------------------|
| Electrical    | RS-422 (differential), half-duplex       |
| Encoding      | UART: 1 start, 8 data, 1 stop, no parity |
| Setup baud    | 115,200 bps (host detection / setup)     |
| Operate baud  | 2,500,000 bps                            |

STMBL uses two separate UARTs (UART4 TX + USART1 RX) driven by DMA, with a
GPIO TX-enable line on PB7.

---

## Framing

Framing is timing-based — there are no length fields.  The gap between the end
of one command and the start of the next signals a new command boundary.

- **Setup mode**: the host leaves at least 25.5 character-times of silence
  between commands so the slave can detect the baud rate.
- **Operate mode**: the inter-command gap is at least 4 character-times
  (~16 µs at 2.5 Mbit/s).

The slave begins transmitting its reply immediately after verifying the final
CRC byte of a command.  The host must leave room in its transmit stream for
the reply to arrive before issuing the next command.

Special local-command bytes `0xFF` (reset) and `0xFC` (reserved) are **not**
followed by a CRC byte; the slave resets its parser on receipt of `0xFF`.

---

## CRC

Every command and every reply is protected by CRC-8/MAXIM (also called
CRC-8/1-Wire or Dallas 1-Wire):

| Parameter  | Value              |
|------------|--------------------|
| Width      | 8 bits             |
| Polynomial | 0x31               |
| Init       | 0x00               |
| RefIn      | True               |
| RefOut     | True               |
| XorOut     | 0x00               |

The CRC covers all preceding bytes of the packet (command byte, optional
address bytes, optional data bytes).  STMBL uses the table-driven
implementation in `shared/crc8.h` / `shared/crc8.c`.

---

## LBP Command Byte

Every command starts with a single command byte whose bit fields select the
operation.  The `lbp_t` union in `inc/sserial.h` documents the layout:

```
bit  7..6   ct    CommandType  01=CT_RW  10=CT_RPC  11=CT_LOCAL
bit  5      wr    Write        1=write   0=read
bit  4      rid   RPCIncludesData  (CT_RPC only; ignored otherwise)
bit  3      ai    AutoIncrement   1=post-increment address by data size
bit  2      as    AddressSize     0=use current address  1=include 2-byte address
bit  1..0   ds    DataSize        00=1 B  01=2 B  10=4 B  11=8 B
```

For `CT_RPC` commands the entire lower 6 bits (`bits 5..0`) form the RPC
index rather than the individual `wr/rid/ai/as/ds` fields.

---

## CT_RW — Memory Read / Write (ct = 01b)

CT_RW gives random access to the slave's flat 2 KB address space.

### Read (wr = 0)

```
Host  → Slave:  [cmd] [addr_lo addr_hi if as=1] [CRC]
Slave → Host:   [data (1/2/4/8 bytes)] [CRC]
```

Packet sizes:

| as | Host sends      | Slave replies          |
|----|-----------------|------------------------|
| 0  | 2 bytes (cmd+CRC) | (1<<ds) + 1 bytes     |
| 1  | 4 bytes (cmd+addr+addr+CRC) | (1<<ds) + 1 bytes |

If `ai=1` the slave's internal address pointer is incremented by `(1<<ds)`
after the operation, enabling burst reads with repeated commands at `as=0`.

### Write (wr = 1)

```
Host  → Slave:  [cmd] [addr_lo addr_hi if as=1] [data (1/2/4/8 bytes)] [CRC]
Slave → Host:   [0x00] [CRC]          (per spec)
```

**STMBL deviation**: the write handler in `sserial.c` does **not** transmit a
response byte for CT_RW writes.  The host (LinuxCNC hostmot2) must account for
this silence.

---

## CT_LOCAL — Local Commands (ct = 11b)

Local commands address a small set of fixed slave properties rather than the
memory map.

### Local Read (wr = 0, range 0xC0..0xDF)

```
Host  → Slave:  [cmd] [CRC]        (2 bytes total)
Slave → Host:   [1 byte] [CRC]     (2 bytes total)
```

Defined commands:

| Command | Hex  | Slave response byte         |
|---------|------|-----------------------------|
| LBPCookieCMD   | 0xDF | 0x5A (LBPCookie)    |
| LBPStatusCMD   | 0xC1 | status byte (0x00)  |
| LBPCardName0Cmd | 0xD0 | name[0] = 's'      |
| LBPCardName1Cmd | 0xD1 | name[1] = 't'      |
| LBPCardName2Cmd | 0xD2 | name[2] = 'b'      |
| LBPCardName3Cmd | 0xD3 | name[3] = 'l'      |

The host issues LBPCookieCMD first to verify it is talking to an LBP slave.
The four CardName commands return the four-character ASCII card identifier
("stbl" for STMBL).

Unknown local-read commands return 0x00.

### Local Write (wr = 1, range 0xE0..0xFF)

```
Host  → Slave:  [cmd] [1 data byte] [CRC]     (3 bytes total)
Slave → Host:   [0x00]                         (1 byte, no CRC)
```

The special codes `0xFF` (reset) and `0xFC` (reserved) are exceptions: they
carry **no data byte and no CRC**.  On `0xFF` STMBL resets its receive parser.

---

## CT_RPC — Remote Procedure Calls (ct = 10b)

For CT_RPC the lower 6 bits of the command byte are the RPC index, so the
full command byte is `0x80 | rpc_index`.

RPC table pitch is 8 bytes; up to 64 RPCs can be defined (indices 0..63).

### General RPC framing

Most RPCs take no input data:

```
Host  → Slave:  [cmd] [CRC]                           (2 bytes)
Slave → Host:   [reply data] [CRC]
```

RPCs that carry output (host→slave) data place it between the CRC byte of the
command and the slave's reply, but the ProcessData RPC (below) is the only
example in SSLBP.

---

## SSLBP Special RPCs

Three RPC indices are reserved by the SSLBP standard.

### 0xBB — Discovery

```
Host  → Slave:  [0xBB] [CRC]
Slave → Host:   [input] [output] [ptocp_lo ptocp_hi] [gtocp_lo gtocp_hi] [CRC]
```

The reply is a 6-byte `discovery_rpc_t` structure (little-endian):

| Field  | Bytes | Meaning                                      |
|--------|-------|----------------------------------------------|
| input  | 1     | process-data input size in bytes (slave→host) |
| output | 1     | process-data output size in bytes (host→slave) |
| ptocp  | 2     | Process Table Of Contents Pointer (PTOC)      |
| gtocp  | 2     | Global/Mode Table Of Contents Pointer (GTOC)  |

STMBL values: `input=11, output=9, ptocp=0x018B, gtocp=0x01A5`.

### 0xBC — UnitNumber

```
Host  → Slave:  [0xBC] [CRC]
Slave → Host:   [unit_lo .. unit_hi (4 bytes)] [CRC]
```

Returns a 32-bit unit number unique to the device.  STMBL derives it by XORing
the three 32-bit words of the STM32 96-bit unique chip ID.

### 0xBD — ProcessData

The cyclic real-time exchange.

```
Host  → Slave:  [0xBD] [output bytes (discovery.output)] [CRC]
Slave → Host:   [fault byte] [input bytes (discovery.input − 1)] [CRC]
```

For STMBL: host sends `1 + 9 + 1 = 11` bytes; slave replies with
`1 + 10 + 1 = 12` bytes.

The fault byte is always 0x00 in the current implementation; the `discovery.input`
field already accounts for it (11 = 1 fault byte + 10 data bytes).

---

## STMBL Process Data Layout

### Output (host → STMBL, 9 bytes)

```c
typedef struct {
  float    pos_cmd;        // position command (rad)
  float    vel_cmd;        // velocity feedforward (rad/s)
  uint32_t out_0    : 1;   // digital output 0
  uint32_t out_1    : 1;   // digital output 1
  uint32_t out_2    : 1;   // digital output 2
  uint32_t out_3    : 1;   // digital output 3
  uint32_t enable   : 1;   // drive enable
  uint32_t index_enable : 1; // encoder index latch enable
  uint32_t padding  : 2;
} sserial_out_process_data_t;  // 9 bytes
```

### Input (STMBL → host, 10 bytes, preceded by 1 fault byte = 11 bytes)

```c
typedef struct {
  float    pos_fb;         // position feedback (rad)
  float    vel_fb;         // velocity feedback (rad/s)
  int8_t   current;        // motor current; scaled: ±127 ≡ ±30 A
  uint32_t in_0    : 1;    // digital input 0
  uint32_t in_1    : 1;    // digital input 1
  uint32_t in_2    : 1;    // digital input 2
  uint32_t in_3    : 1;    // digital input 3
  uint32_t fault   : 1;    // drive fault flag
  uint32_t index_enable : 1; // index-enable echo (bidirectional)
  uint32_t padding : 2;
} sserial_in_process_data_t;  // 10 bytes
```

`pos_fb` includes a velocity-advance term: `pos_fb = pos_fb_raw + vel_fb × pos_advance`.

---

## Memory Map and Process Data Descriptors

The slave exposes a flat 2 KB address space (`MEM_SIZE = 2048`) served by
CT_RW reads.  It contains:

- **Bytes 0..5**: the Discovery reply (`discovery_rpc_t`) mirrored at address 0.
- **PDDs (Process Data Descriptors)**: fixed-up descriptors describing every
  process variable; referenced by the PTOC/GTOC pointer tables.
- **PTOC** (at `ptocp = 0x018B`): array of `uint16_t` pointers to PDDs,
  terminated by `0x0000`.
- **GTOC** (at `gtocp = 0x01A5`): same format, covers global/mode descriptors.
- **Live data regions**: addresses referenced by each PDD's `data_addr` field
  hold the current process values (updated each cycle by STMBL).

### Process Data Descriptor format

```c
typedef struct {
  uint8_t  record_type;    // 0xA0 = process data, 0xB0 = mode data
  uint8_t  data_size;      // size in bits
  uint8_t  data_type;      // see data-type codes below
  uint8_t  data_direction; // 0x00=INPUT  0x40=BIDIRECTIONAL  0x80=OUTPUT
  float    param_min;      // engineering minimum
  float    param_max;      // engineering maximum
  uint16_t data_addr;      // offset into slave memory of live data
  char     names[];        // "unit\0hal_pin_name\0"  (null-terminated pair)
} process_data_descriptor_t;
```

### Data-type codes

| Code | Name              | Notes                         |
|------|-------------------|-------------------------------|
| 0x00 | PAD               | padding, no data              |
| 0x01 | BITS              | packed bit field              |
| 0x02 | UNSIGNED          | unsigned integer              |
| 0x03 | SIGNED            | signed integer                |
| 0x04 | NONVOL_UNSIGNED   | non-volatile unsigned integer |
| 0x05 | NONVOL_SIGNED     | non-volatile signed integer   |
| 0x06 | STREAM            | byte stream                   |
| 0x07 | BOOLEAN           | boolean                       |
| 0x08 | ENCODER           | encoder count                 |
| 0x10 | FLOAT             | IEEE-754 float (STMBL extension) |

`DATA_TYPE_FLOAT (0x10)` is an STMBL extension not present in the original
Mesa spec.

### STMBL PTOC entries (in order)

| PDD offset | Bits | Type    | Direction | Unit | HAL name        |
|-----------|------|---------|-----------|------|-----------------|
| 0x000C    | 32   | FLOAT   | OUTPUT    | rad  | pos_cmd         |
| 0x002C    | 32   | FLOAT   | OUTPUT    | rad  | vel_cmd         |
| 0x0048    | 4    | BITS    | OUTPUT    | —    | out (4 bits)    |
| 0x0060    | 1    | BOOLEAN | OUTPUT    | —    | enable          |
| 0x0080    | 32   | FLOAT   | INPUT     | rad  | pos_fb          |
| 0x00A0    | 32   | FLOAT   | INPUT     | rad  | vel_fb          |
| 0x00BC    | 8    | SIGNED  | INPUT     | A    | current         |
| 0x00D8    | 4    | BITS    | INPUT     | —    | in (4 bits)     |
| 0x00F0    | 1    | BOOLEAN | INPUT     | —    | fault           |
| 0x010C    | 1    | BOOLEAN | BIDIR     | —    | index_enable    |
| 0x015C    | 32   | FLOAT   | OUTPUT    | —    | scale           |
| 0x0174    | 2    | PAD     | OUTPUT    | —    | padding         |

The GTOC points to a mode descriptor and a global (non-volatile) padding entry.

---

## STMBL Deviations from Spec

| Area                  | Spec behaviour                          | STMBL behaviour                                  |
|-----------------------|-----------------------------------------|--------------------------------------------------|
| CT_RW write response  | Slave replies `[0x00] [CRC]`            | No response sent                                 |
| CT_LOCAL write response | No response                           | Slave sends `[0x00]` (1 byte, no CRC)            |
| FLOAT data type       | Not defined in base spec                | 0x10 used for 32-bit IEEE-754 floats             |
| ProcessData response  | Fault byte is part of `input` count     | Confirmed: `input=11` = 1 fault byte + 10 data  |
| `0xFF` local write    | Reset; no CRC                           | Implemented: resets RX parser, no CRC expected   |
| `0xFC` local write    | Reserved                                | Accepted (no-op); no CRC expected                |

---

## Typical Startup Sequence

1. Host detects slave at 115,200 baud (optional setup-mode phase).
2. Host switches to 2,500,000 baud and issues `LBPCookieCMD (0xDF)`.
3. Slave replies `0x5A`; host verifies the cookie.
4. Host reads card name via `0xD0..0xD3` (four single-byte reads).
5. Host issues `UnitNumberRPC (0xBC)` to get a unique device ID.
6. Host issues `DiscoveryRPC (0xBB)` to get `input`, `output`, `ptocp`, `gtocp`.
7. Host walks the PTOC/GTOC (CT_RW reads) to learn process variable layout.
8. Host enters the cyclic loop: `ProcessDataRPC (0xBD)` once per servo period.

---

## Cyclic Process-Data Loop Timing (STMBL)

At 2,500,000 baud a byte takes 4 µs.  One ProcessData exchange:

| Direction  | Bytes             | Time      |
|------------|-------------------|-----------|
| Host→Slave | 1 cmd + 9 out + 1 CRC = 11 | 44 µs |
| Slave→Host | 1 fault + 10 in + 1 CRC = 12 | 48 µs |
| Total      | 23 bytes          | ≈ 92 µs   |

STMBL's FRT runs at 20 kHz (50 µs period), so LinuxCNC must schedule
ProcessData at or below that rate.  The `timeout` pin (default 100) counts FRT
cycles without a valid ProcessData exchange before declaring a fault.
