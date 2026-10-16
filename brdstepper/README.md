# brdstepper firmware

Closed-loop NEMA11 stepper drive on an STM32G473CE. It runs stmbl's HAL
and shared components, with brdstepper's own drivers. Hardware, pin plan
and parts: [docs/brdstepper_hw.md](../docs/brdstepper_hw.md).

**Status:** builds (`make -C brdstepper`); not yet run on a board. The
control code is tested in the host simulator (below). The drivers follow
RM0440 and ST's pin data; lines marked "check" need the reference manual
at the bench.

## Layout

```
brdstepper/
  Makefile, g473.ld     build (arm-none-eabi, newlib-nano), linker script
  inc/brd.h             pins, analog scaling, the hardware API
  inc/flash.h           flash layout (dual bank)
  src/main.c            clocks, SysTick, the frt / rt interrupts, main loop, about / reset / bootloader
  src/hw.c              GPIO, TIM1 + TIM8 PWM, ADC1 + ADC2, DAC (VREF), SPI1 (AS5047P)
  src/flash.c           bank 2 erase / program (64-bit double words)
  src/config.c          the config text in bank 2 (the F4's commands: showconf, flashsaveconf, ...)
  src/fwup_g4.c         in-band update staging in bank 2 (shared/comps/fwup.c's platform)
  src/syscalls.c        printf to the smart-serial console
  src/cdc.c             USB CDC (to do: a stub)
  comps/hv.c            power stage: the two-coil current loop (shared/bip_loop.c) on the DRV8876s
  comps/mag.c           AS5047P magnetic encoder (absolute, 14 bit)
  comps/io.c            status LED; the F4 board's I/O pins, accepted
  comps/adc.c           hardware version ID
  conf/                 board templates: brdstepper (the default), mag_fb0, nema11
  tools/brd.gdb         bring-up over SWD before there is a console: brd, pin, hal, enc
```

From the shared code it uses:
- the HAL, the commands and the config templates;
- the components `pid`, `vel`, `rev`, `fb_switch`, `fault`, `reslimit`,
  `iit`, `pe`, `sim`, `scope`, `fwup`, `term`, and `stp_ttc` (the
  stepper's torque constant and detent compensation);
- `sscon`, `fwstage`, `bip_loop`;
- `src/comps/conf.c`.

The component `hv` keeps stmbl's pin names, so the stock `pid` template
links unchanged.

## Timing

| loop | rate | where |
|---|---|---|
| PWM | 20 kHz, centre aligned | TIM1 (CH1..3) + TIM8 (CH1), started together |
| current samples | 40 kHz: drive and brake window of each period | TIM1 TRGO2 → ADC1 + ADC2 injected, simultaneous |
| frt (current loop) | 20 kHz | ADC1_2 interrupt, after the brake-window sample |
| rt (position, velocity) | 5 kHz | every 4th frt, in TIM7's vector (pended by software, lower priority) |
| nrt | main loop | console, config |

**Current sensing:** the DRV8876's IPROPI sees only the low-side MOSFETs,
and only current flowing drain to source.
- **Two samples:** each PWM period gives a drive-window sample and a
  brake-window sample. `bip_sense` (shared/bip_loop.c) makes a signed
  current of the two.
- **Windows:** `hv0.sense_window` 0.12 and `hv0.max_duty` 0.88 keep both
  windows long enough for the DRV8876's delays.
- **Offset calibration:** at each enable the bridges brake for
  `hv0.sense_cal` periods (64, 3.2 ms) to measure the IPROPI offsets.
- **More:** see `DEBUGNOTES.md`, and the simulation in
  `host/tests/stepper_ipropi.test`.

## Flash

Dual bank (DBANK = 1), 2 kB pages:

| bank | address | content |
|---|---|---|
| 1 | 0x08000000 | bootloader 16 kB (to do) |
| 1 | 0x08004000 | application 240 kB |
| 2 | 0x08040000 | update staging 240 kB |
| 2 | 0x0807C000 | config 16 kB |

The code runs from bank 1, so writing bank 2 doesn't stall it. Saving the
config and staging an update (`servoterm-cli --update`) both work while the
drive runs.

Until brdstepper's bootloader exists, `make` links the application at
0x08000000 (`BOOT=0`); `make BOOT=1` links it behind the bootloader.

Once per new chip, `make option_bytes` (STM32CubeProgrammer, SWD) sets the
option bytes:
- nSWBOOT0 = 0, nBOOT0 = 1: PB8 is CAN RX as well as BOOT0;
- DBANK = 1.

Bring-up, connections and parts: [docs/brdstepper_hw.md](../docs/brdstepper_hw.md).

## Tested in the simulator

`make -C host check`:
- **`test_bip`:** the current loop against an exact coil model, and the
  IPROPI reconstruction.
- **`stepper.test`, `stepper_ipropi.test`, `stepper_ipropi_worst.test`:**
  a NEMA11 in closed loop, with ideal sensing, with IPROPI sensing, and
  with IPROPI at the datasheet's error limits.
- **`test_bootloader`:** update staging in 64-bit units, each programmed
  once, as on the G4.

## Not done yet

- **Smart-serial:** port `src/comps/sserial.c` to USART1 + DMA (DMAMUX).
  With it come the console over smart-serial and LinuxCNC.
- **USB CDC** (console, `servoterm-cli`), and **CAN** (FDCAN1, the CAN
  variant; protocol to decide).
- **The bootloader** (smart-serial loader, USB DFU entry, applying staged
  updates), IWDG, and setting the option bytes (nSWBOOT0 = 0, nBOOT0 = 1,
  DBANK = 1) in the flash target.
- **On the board:** check the marked register details, IPROPI against a
  current probe, and the encoder's offset calibration (phasing).
