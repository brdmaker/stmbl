# Firmware updates over smart-serial

Drives in tight places should be updatable through the cable they already
have: the smart-serial link to the Mesa card, without USB or SWD. This lives
on branch `sserial_bootloader`, which is built on `sserial_console`.

What is tested where:
- **Host tests:** the protocol pieces (`make -C host check`).
- **Simulator:** the drive side, end to end, in the host simulator and in
  LinuxCNC's simulation.
- **ARM builds:** the bootloader and the application compile and fit.
- **Not yet tried on a real drive with a Mesa card.**

Related: [sserial_console.md](sserial_console.md) (the console the in-band
path uses) and [capture_scope.md](capture_scope.md).

There are two paths, for two situations.

| | In-band (routine) | Recovery (`setsserial flash`) |
|---|---|---|
| when | LinuxCNC runs, the machine off | the application is broken, or no console |
| who talks | the running application (`fwup`) | the bootloader's smart-serial loader |
| how | `servoterm-cli --update app.bin` on sserialterm's pty (or USB) | Mesa's LBP flash protocol, as for the 8i20 |
| LinuxCNC | normal config | `halrun`, hm2 with `sserial_baudrate=115200`, `setsserial` |
| speed | ~350 B/s over the console (~15 min for 345 kB); USB ~4 kB/s | ~42 s of wire time per 300 kB at 115200, plus hm2's overhead |
| safety | staged, CRC32 checked, copied by the bootloader, resumes after power loss | block 0 refused; the application's CRC is checked before it starts |

## The flash: one bank, so writes stall the chip

The STM32F405 (and the G431 of brdstepper) has a single flash bank. While
it erases or programs, every instruction fetch from flash waits: the
interrupts, the control loop and the smart-serial replies all stop.

| operation | typical | max |
|---|---|---|
| 16 kB sector erase | 0.25 s | 0.5 s |
| 128 kB sector erase | 1 s | 2 s |
| program one word | 16 µs | 100 µs |

The design follows from this:
- **The application never erases.**
  - The bootloader keeps the staging area blank.
  - `fwup` programs one word per rt period (5 kHz), and only while the
    drive is disabled (`fwup0.en = fault0.en_out`).
  - After each word, `hal.flash_grace` lets the rt/frt interrupts run late
    for two periods instead of stopping HAL (RT_TOO_LONG / FRT_TOO_LONG).
- **The smart-serial loader erases first.**
  - It erases the application area before it answers anything.
  - The erase commits during `setsserial flash` then find blank blocks and
    return at once. hostmot2 waits only 1 s for the cookie after a commit.
  - Its receiver uses DMA, so bytes keep arriving while flash stalls the
    CPU.

## Layout, stmbl F4 (1 MB)

| region | sectors | addresses | size |
|---|---|---|---|
| bootloader | 0–1 | 0x08000000 | 32 kB (the loader is ~7.5 kB) |
| config | 2–3 | 0x08008000 | 2 × 16 kB |
| application | 4–7 | 0x08010000 | 448 kB; today ~353 kB |
| staging | 8–11 | 0x08080000 | 512 kB; a 16-byte header at its end |

**brdstepper G431** (128 kB): bootloader 8 kB, application 56 kB, staging
56 kB, config 8 kB. Its pages are uniform 2 kB (erase 2¹¹).

## Booting (`bootloader/src/main.c`)

The application sets a RAM word at `0x2001C000` and resets:

| trigger | set by | the bootloader |
|---|---|---|
| `0xDEADBEEF` (or the boot pin) | — | ST's ROM DFU loader (USB), as before |
| none (`0xCAFEFEED`) | `fwapply`, power on | applies a pending staged update, then starts the application if its CRC is right |
| `0x5353424C` "SSBL" | `sserialboot` | smart-serial loader, stays there until flashed or power cycled |
| — (application CRC bad) | — | smart-serial loader; if no valid LBP request comes in 30 s, ROM DFU |

The application `bootloader` command still jumps into ROM DFU directly.

**Smart-serial loader** (115200 baud, polled, RX by DMA):
- **What it answers:** name `stbl`, the unit number (as the application's),
  an empty process data table, and the flash window.
- **Shared code:** `shared/lbp_dev.c` is the byte-level LBP parser, the
  same request and reply formats as `src/comps/sserial.c`. Every request is
  CRC8-checked, and a request with a bad CRC gets no reply.
  `shared/lbp_flash.c` handles flash mode.
- **After flashing:** once setsserial has written and the line has been
  quiet for 1 s, the loader checks the application's CRC. If it is right,
  the loader resets into it.

## Recovery: Mesa's remote flash (`setsserial flash`)

LinuxCNC's `setsserial` (hostmot2) programs a remote's flash through
smart-serial LBP commands. Decoded from `setsserial.c` and `sserial.h`:
- **Flash mode:** local write `0xEC` with data `0x02` switches non-volatile
  access to flash; `0x00` switches it back. In flash mode, memory reads and
  writes with a 2-byte address go to a flash window:
  - `0x8000`: offset (4 bytes);
  - `0x8004` / `0x8005`: log₂ erase / write size (read);
  - `0x8007`: commit, `0xFE` erase or `0xFD` write;
  - `0 … write size`: the write buffer, written 8 bytes at a time (`0x67`).
- **Handshake:** after each commit, a local read `0xDF` must return `0x5A`,
  or setsserial aborts. Each command is allowed 1 s (`waitfor()`).
- **Block 0** is never touched: it belongs to the device's bootloader.
  Blocks of all zero bytes are skipped.
- **Baud:** setsserial refuses unless the link runs at 115200 baud.

**Window, stmbl F4:**
- Offset 0 is 16 kB below `0x08010000`: block 0, refused.
- Block 1 is the application's start. The window ends at the staging area,
  so 16 kB + 448 kB.
- Erase 2¹⁴, write 2⁸.
- `lbpf_erase_lazy` maps the uniform blocks onto the uneven sectors:
  - a blank block: nothing to do;
  - stale data at block 1 (a new run after a failed one): erase every
    sector;
  - stale data elsewhere: erase the block's sector.

  Such a commit can outlast hostmot2's wait. That setsserial run fails
  once, and the next one finds everything blank.

**The image:** `make ssfw` (`tools/mkssfw.py`) writes
`obj_app/stmbl_ss.bin`: one block of zeros (block 0), then the application
`.bin` padded with 0xFF. It refuses an application that contains an
all-zero erase block, because setsserial would skip that block and the CRC
would fail.

**Procedure:**
1. `sserialboot` on the console (drive disabled), or a drive whose
   application is broken.
2. Stop LinuxCNC.
3. `halrun`, then `loadrt hostmot2` and the card driver with
   `sserial_baudrate=115200`.
4. `setsserial flash hm2_<card>.0.<remote> obj_app/stmbl_ss.bin`.

With a broken application, power the drive at most 30 s before hm2 loads,
or it goes on to DFU.

## In-band: staged update over the console (`fwup`)

`shared/comps/fwup.c` (loaded by the `pid` / `mpid` templates) and
`shared/fwstage.c`. Each command answers one line (`fw ok …`, `fw busy` or
`fw err …`):
```
fwbegin SIZE CRC32          start; the same SIZE and CRC32 again resumes
fwdata OFFSET BASE64 CRC16  30 bytes a line (a console line holds 63 chars)
fwend                       all programmed and the CRC32 right? (busy: ask again)
fwapply                     write the pending mark, reset after 0.2 s
fwstat                      state, bytes, lines, bad lines
sserialboot                 reset into the smart-serial loader
```
- **Refused while the drive is enabled:** every command except `fwstat`
  answers `fw err drive enabled`.
- **Line checks:**
  - a line with a bad CRC16 is refused, and the sender repeats it;
  - a repeat of the last line is harmless;
  - a gap is refused, and the reply tells where the drive is;
  - `fw busy` means the 512-byte queue is full, so send the line again.
- **Staging must be blank:**
  - `fwbegin` checks it; the application does not erase.
  - If it is not blank (an aborted update, then a restart), reset the
    drive and the bootloader erases it.
- **The pending mark:** a 16-byte header at the end of staging holds the
  magic, size, CRC32 and a state word.
  - The state word stays all ones while the update is pending.
  - The bootloader programs it to 0 when done. Bits only go 1 → 0, so no
    erase is needed.
- **The copy (bootloader, `fwst_boot`):**
  1. Check the staged CRC32 again; a bad image is not copied, and its mark
     is cleared.
  2. Erase the application sectors, copy, verify.
  3. Clear the mark, erase staging.

  If the copy fails or the power goes, the mark stays pending, so the next
  reset copies again.
- **`servoterm-cli --update IMAGE [--no-apply]`:**
  - keeps 3 lines in flight and follows the drive's reported position;
  - resumes an interrupted transfer (the same image);
  - then sends `fwend` and `fwapply`.

## Tests

- **`host/tests/test_bootloader.c` (19 checks):**
  - setsserial's whole programming sequence, request and reply bytes with
    CRC8, through `lbp_dev` on a model of the F4's flash (bits only
    1 → 0);
  - hm2's enumeration (name, unit, discovery, empty tables, process data);
  - no stalled commit after the loader's initial erase, and an interrupted
    run then a re-run;
  - block 0 refused, and the all-zero-block caveat;
  - staging: blank check, queue, resume, damaged / repeated / gap lines,
    one word per period, the mark, the bootloader's copy, a power loss
    during the copy, a bad staged image, a wrong CRC32, size limits.
- **`host/tests/test_mkssfw.py`:** the setsserial image.
- **`host/tests/fw_update.test`:**
  - the commands over the simulated smart-serial console: refused while
    enabled, a damaged line, `fwend`, `fwapply`;
  - the simulated bootloader copies the image and prints its CRC32.
- **`make -C hwtest/burnin-sim test-console` (LinuxCNC):**
  - `servoterm-cli --update` through sserialterm's pty: refused with the
    machine on, staged and applied with it off.
- **By hand:** `servoterm-cli --update` against `stmbl_host --pty`, also
  interrupted and resumed.

## Still to do

- **Real hardware:**
  - a drive with a Mesa card: `setsserial flash` into the loader, and
    `--update` through sserialterm;
  - hm2's per-command overhead (the host model leaves it out);
  - whether a word write really fits the rt/frt slack. `hal.flash_late`
    counts the late periods.
- **brdstepper:** the G431 flash driver (2 kB pages; the same layout ideas).
- **Speed:** a binary `fwdata`, or a wider console field, would roughly
  halve the in-band time.
