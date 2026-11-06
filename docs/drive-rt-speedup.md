# Drive rt speed-up: bench test (branch `claude/drive-rt-speedup`)

These are two F4 firmware changes from `docs/servo-timing.md`. Each is
behind a switch, and **both are off by default**: this firmware behaves as
before until you turn one on. The smart-serial protocol, the F3 firmware and
`STMBL_FW_VERSION` (1) are unchanged, so LinuxCNC's config stays as it is.

| switch | 0 (default) | 1 |
|---|---|---|
| `sserial0.reply_mode` | the 20 kHz frt answers packets, busy-waiting up to ~22 us for a process-data packet's last bytes | the USART1 idle-line interrupt answers each packet ~4 us after its last byte; no busy-wait |
| `adc0.fast` | the original sin/cos sums and `V_DIFF` (6 float divisions per group) | both halves summed at once (`uadd16`) and the voltage as `sum * k - k0`; no divisions |

## What each change does

**`sserial0.reply_mode = 1`** (`src/comps/sserial.c`)
- **Parsing:** the frt's packet parser became `parse(pins, spin)`. In mode 0
  the frt calls it with `spin = 1`, exactly as before.
- **In mode 1:**
  - Only `USART1_IRQHandler` parses. It runs at NVIC priority 1: below the
    frt (0) and above rt (2), so the frt and the interrupt never parse at
    the same time.
  - **The frt still does:** the link timeout, and the clock lock
    (`clock_scale`, `shared/sserial_sync.h`). A packet the interrupt
    answered within the last frt period counts as "late", as a waiting
    packet did before.
  - **Missed interrupt:** if bytes sit unparsed for a whole frt period, the
    frt pends the interrupt (`NVIC_SetPendingIRQ`) instead of parsing
    itself.
  - **When hal stops** (flash save, fault), the interrupt stops answering,
    as the frt did.
- **New pin `sserial0.irq_count`:** counts packets answered from the
  interrupt. In mode 1 it should grow at the servo rate.
- **The reply's data is unchanged:** the last rt cycle's `pos_fb`, with
  `pos_advance`. Only when it is sent changes.

**`adc0.fast = 1`** (`src/comps/adc.c`, `shared/adc_sum.h`)
- **Sums:** the integer sums are bit-exact. With at most 16 samples a
  group, the 16-bit halves can't carry into each other (a static assert
  checks the hardware's 9 or 10).
- **Voltage:** the conversion is within 2e-6 V of the old one
  (`host/tests/test_adc_sum.c` checks every possible sum on both boards'
  op-amp values). `make -C host/m4 check` runs the same test as Cortex-M4F
  code in QEMU (`mps2-an386`): the real `uadd16` and the M4's single-precision
  FPU; it is part of `make -C host check`.
- **Expected saving:** about 10-15 us per rt cycle (from the compiled code:
  6 `vdiv` of 14 cycles and a shorter inner loop, x24 groups at 168 MHz).
  That is less than the ~30 us guessed earlier.

## Test

Before flashing:
- Back up the config: `servoterm-cli --dump > backup.txt`.
- Make sure the motor is safe to move.

Flash with the repo's targets only: `make all` (on the bench Pi, binutils
2.40), then `make btburn`. Afterwards, check `about` and that the config is
unchanged.

1. **Baseline:** both switches 0. Run the hwtest suite as usual. Run `hal`
   in the drive console: note rt time and max (whole rt and `adc0`) and
   the `sserial0` frt time, under LinuxCNC motion (soak).
2. **`adc0.fast = 1`:**
   - `adc0`'s rt time should drop by ~10-15 us.
   - Compare `adc0.sin0` / `cos0` / `amp0` at rest (same within noise).
   - The phasing and encoder tests and a soak must pass unchanged.
3. **`sserial0.reply_mode = 1`:**
   - `sserial0.irq_count` counts.
   - `sserial0.crc_error` stays put.
   - LinuxCNC shows no smart-serial errors.
   - The rt max under motion drops. That was 168.5 us of 200, with ~28 us
     of busy-wait.
   - `sserial0.clock_scale` and `phase` still toggle (the clock lock);
     `vel_int0.real_cmd_freq` stays at the servo rate.
   - Run the soak and the following-error test.
4. **Both on, then the servo rate:**
   - `hwtest/timing/servo_rate.py` at 1000, 500, 333 and 250 us, with the
     drives' `vel_int0.cmd_freq` matching.
   - Then a short LinuxCNC soak at 2 kHz.
5. **If all is well:** set the switches in the drive config. A later
   firmware can make them the default.

If anything misbehaves, set the switch back to 0 at runtime; no reflash
is needed. Record findings in `DEBUGNOTES.md` (entries "reply_mode" and
"adc0.fast").
