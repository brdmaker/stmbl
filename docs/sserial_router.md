# An stmbl card as a smart-serial router

One stmbl v4 LV card between two LinuxCNC controllers, each with a Mesa card: the card answers both as
a smart-serial device and passes signals from one to the other in its HAL. No motor and no HV board
are needed. This is used for the handshake between the two sides of a twin-spindle lathe. The machine's
own use of it is documented in its repository.

```
LinuxCNC A ── Mesa ── CMD port ─┐
                                 stmbl LV:  sserial0 <── HAL links ──> sserial1
LinuxCNC B ── Mesa ── enc0 port ─┘
```

## Firmware

`src/comps/sserial.c` can be loaded twice. Each instance has its own buffers, parser, process data
and writable copy of the PTOC. The `port` pin picks the physical port when the HAL starts. It defaults
to the instance number:

| port | pairs (RJ45) | MCU (v4.1) | DMA |
|---|---|---|---|
| 0 CMD  | 1/2 receive, 3/6 transmit (CMD_36E PB7 on) | USART1 RX PA10, UART4 TX PA0 | DMA2 S5, DMA1 S4 |
| 1 enc0 | 1/2 receive (PD11 off), 3/6 transmit (PD10 on), 4/5 off (PD15) | USART3 RX PC11, TX PC10 | DMA1 S1, DMA1 S3 |

Both ports use the same pairs, so the same cable works to either Mesa port. Both ports carry the same
PTOC, so each side's LinuxCNC sees an ordinary stmbl device. The console (`sserialterm`) is on the CMD
port only. Each link's unit number has its port in the top bits (`port << 28`), which shows which side
is which. With the second instance loaded, no other enc0 component (encoders, resolver, EnDat) can be
loaded.

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

`host/tests/sserial_router.test`: the host simulator's sserial model has both ports, and
`lcncio <port> [on|off] [out <mask>] [pos <v>] [vel <v>]` plays each side's LinuxCNC. The test checks
that both directions pass through unchanged, that one side's link loss shows as `fault` with zeroed
inputs on the other side, and that both recover.

## Open

- Not yet run on hardware: scope enc0's 3/6 pair and check `sserial1.connected` against a Mesa card.
- Bit budget: the stmbl PTOC sends 13 bytes to the host (the in direction). This is above the 96 bits
  of older Mesa smart-serial firmware, as for any stmbl drive. A smaller router PTOC (only the bits it
  carries) would remove that limit.
- In `reply_mode 0`, both instances can busy-wait in the same frt tick (see DEBUGNOTES). The template
  avoids this with mode 1.
