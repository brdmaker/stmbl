# vibsensor firmware

The vibration and temperature sensor of [docs/vibsensor.md](../docs/vibsensor.md):
an STM32G473CE with an IIS3DWB accelerometer and a TMP117, on a CAN FD
measurement bus. It runs stmbl's HAL with the shared components `vib` (the
processing) and `vnode` (the bus), and its own drivers.

**Status:**
- **Builds** (`make -C vibsensor`): 63 KB flash, 108 KB of 128 KB RAM.
- **Not run on a board yet.**
  - The drivers follow RM0440 and the IIS3DWB / TMP117 datasheets as
    remembered; lines marked "check" need the documents at the bench.
  - The pin plan in `inc/vs.h` is a proposal for the board.
- **Tested in the host simulator:** the processing and the bus logic, with
  `vibsim` in place of the IIS3DWB (`host/tests/vib/`; `node.test` runs
  this board's template).

## Layout

```
vibsensor/
  Makefile, g473.ld     build (arm-none-eabi, newlib-nano), linker script (as brdstepper's)
  inc/vs.h              pins, the hardware API
  inc/flash.h           flash layout: config 16 kB, the identity page, update staging
  src/main.c            clocks, SysTick, rt (TIM6, 5 kHz), the main loop, the LED, about / reset / bootloader
  src/hw.c              clocks, GPIO, SPI1 (IIS3DWB), I2C2 (TMP117), TIM6
  src/fdcan.c           FDCAN1 1 / 5 Mbit/s, interrupt-driven rings; vnode's platform
                        (unique ID, node number and name in flash, console output)
  src/config.c          the config text (showconf, appendconf, flashsaveconf, ...)
  src/syscalls.c        printf into the console ring (out on the bus)
  src/flash.c, cdc.c    bank 2 erase / program (brdstepper's); USB: to do
  comps/iis.c           IIS3DWB: setup, FIFO into vibbuf in rt, range, die temperature
  comps/thermo.c        TMP117: the mounting face's temperature, once a second
  conf/vibsensor.txt    the default config: iis, thermo, then vnode.txt
  conf/vnode.txt        vib and vnode and their links (the host test uses it too)
```

## Timing

| loop | rate | what |
|---|---|---|
| rt | 5 kHz, TIM6 | `iis`: the FIFO's ~5.3 words (polled SPI at 5.3 MHz, ~60 µs); `vib`: filters, sums, crash, FFT blocks |
| nrt | main loop | `vib`: FFTs (4096 points ≈ 2.5 ms ✱); `vnode`: frames in and out, console; `thermo`; the LED |

**Improvement for later:** SPI with DMA, started by the IIS3DWB's INT1 at a
FIFO watermark, would free the 60 µs that rt spends in SPI now.

## LED

| LED | meaning |
|---|---|
| fast blink | no node number yet (the PC's discovery will give one) |
| slow blink | numbered |
| on | `vib0.crash` latched |

## The console

It runs on the bus (0x600 + node in, 0x580 + node out) once the sensor has
a number. The usual commands work: `show`, `vib0.fft_n = 4096`, `vnid`,
`about`, and `vnspec` (send the spectrum).

A parameter is kept like this:
1. `appendconf vib0.fft_n = 4096`
2. `flashsaveconf`

The node number and name are kept apart, in the identity page, by the
PC's numbering (§3.1 of the document).

## Flashing

As for every target here, see `CLAUDE.md`:
- **SWD:** `make flash`.
- **Once per new chip:** `make option_bytes` (BOOT0 pin ignored: PB8 is
  CAN RX).
