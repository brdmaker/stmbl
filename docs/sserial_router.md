# An stmbl card as a smart-serial router

One stmbl v4 LV card between two (or three) LinuxCNC controllers, each with a Mesa card: the card answers both as
a smart-serial device and passes signals from one to the other in its HAL. No motor and no HV board
are needed. This is used for the handshake between the two sides of a twin-spindle lathe. The machine's
own use of it is documented in its repository.

```
LinuxCNC A ── Mesa ── CMD port ─┐
                                 stmbl LV:  sserial0 <── HAL links ──> sserial1
LinuxCNC B ── Mesa ── enc0 port ─┘
```

## Firmware

There is one sserial component (`src/comps/sserial.c`). Every instance gets its own ctx (buffers,
parser, process data, writable PTOC copy, idle interrupt state) and opens the serial port its
`port` pin names when the HAL starts. `port` defaults to the instance number. The ports are listed
in `shared/sserial_port.h`:

| port | where | receive (pair 1/2) | transmit | driver on | DMA rx, tx |
|---|---|---|---|---|---|
| 0 | CMD | USART1 PA10 | UART4 PA0, pair 3/6 | PB7 | DMA2 S5, DMA1 S4 |
| 1 | enc0 | USART3 PC11 | USART3 PC10, pair 3/6 | PD10 | DMA1 S1, DMA1 S3 |
| 2 | enc1 | USART3 PB11 | USART3 PB10, pair 4/5 | PE14 | DMA1 S1, DMA1 S3 |
| 3 | enc0 | USART6 PC7 | USART6 PC6, pair 4/5 | PD15 | DMA2 S2, DMA2 S6 |

- **Cables:** port 1 uses the CMD port's pairs, so the same cable works. enc1 has no UART TX pin on
  its 3/6 pair, so ports 2 and 3 transmit on pair 4/5. Their cable carries the Mesa's receive pair
  on 4/5. The other pairs' drivers are switched off.
- **USART sharing:** two instances can't use one USART, and ports 1 and 2 both need USART3. For
  three links (CMD, enc0, enc1), use ports 0, 3 and 2. If an instance asks for a USART that an
  earlier instance already has, it reports `error` and doesn't open its port.
- **Other components:** with an encoder port in use, no other component can be loaded on that port
  (encm uses USART6, endat and encf use SPI3 on PC10/PC12).
- **Per-port behaviour:** each USART has its own idle-line interrupt (USART1, USART3, USART6) for
  `reply_mode 1`. The console stays on port 0. The unit number carries the port (`port << 28`).

## Config: `link sserial_router`

`conf/template/sserial_router.txt` makes these connections:

| one side writes | the other side reads |
|---|---|
| `out0`..`out3` | `in0`..`in3` |
| `pos_cmd`, `vel_cmd` | `pos_fb`, `vel_fb`, unchanged (`pos_advance = 0`): any float, and integers up to 2^24 exactly |
| (its link drops) | `fault`: the partner's link is down |

A side's link timing out zeroes what it passes on, so the other side reads 0 and `fault`. If the card
itself fails, both links fault on their own. The CMD side's servo thread paces the card
(`io0.clock_scale`). The enc0 side runs free. Both ports use `reply_mode = 1`: each port's own
USART idle-line interrupt answers a packet as soon as it ends (USART1 for the CMD port, USART3 for
enc0), and the frt doesn't busy-wait for either port.

The CMD LEDs show `sserial0`'s link and the FB0 LEDs show `sserial1`'s link (green = connected,
yellow = error).

## Test

The host simulator's sserial model has all four ports and the same USART rule.
`lcncio <port> [on|off] [out <mask>] [pos <v>] [vel <v>]` plays the LinuxCNC on that port.
- `host/tests/sserial_router.test`: both directions pass through unchanged. One side's link loss
  shows as `fault`, with zeroed inputs, on the other side, and both recover.
- `host/tests/sserial_ports.test`: three links (CMD, enc0 on USART6, enc1) run at once.
- `host/tests/sserial_port_clash.test`: enc0 on USART3 next to enc1 gives the later instance an
  error, and the others keep running.

## Open

- Not yet run on hardware: scope enc0's 3/6 pair and check `sserial1.connected` against a Mesa card.
- Bit budget: the stmbl PTOC sends 13 bytes to the host (the in direction). This is above the 96 bits
  of older Mesa smart-serial firmware, as for any stmbl drive. A smaller router PTOC (only the bits it
  carries) would remove that limit.
- In `reply_mode 0`, both instances can busy-wait in the same frt tick (see DEBUGNOTES). The template
  avoids this with mode 1.
