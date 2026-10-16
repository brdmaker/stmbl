# Firmware updates over smart-serial (sketch)

Drives in tight places should be updatable through the cable they already
have, the smart-serial link to the Mesa card, without USB or SWD. This is a
sketch, on branch `sserial_bootloader` (built on `sserial_console`):
- the protocol pieces are written and tested on the host;
- the bootloader and drive integration are still to do.

There are two paths, for two situations.

| | In-band (routine) | Recovery (`setsserial flash`) |
|---|---|---|
| when | LinuxCNC runs (estop or not) | the application is broken, or no console |
| who talks | the running application | the bootloader |
| how | console commands over `con_cmd` / `con_fb` (sserialterm's pty) | Mesa's LBP flash protocol, as for the 8i20 |
| LinuxCNC | normal config | `halrun`, hm2 with `sserial_baudrate=115200`, `setsserial` |
| speed | ~1.7 wire bytes per image byte at ~1 kB/s | ~42 s of wire time per 300 kB at 115200 baud, plus hm2 overhead |
| safety | staged, CRC32-checked, copied by the bootloader, resumes after power loss | block 0 (bootloader) refused; the application CRC is checked before jumping |

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
  or setsserial aborts.
- **Block 0** is never touched: it belongs to the device's bootloader.
  Blocks of all zero bytes are skipped.
- **Baud:** setsserial refuses unless the link runs at 115200 baud.

`shared/lbp_flash.c` is the device's side, meant to sit behind the existing
LBP parser (`src/comps/sserial.c`). It redirects memory accesses while flash
mode is on, refuses block 0 and anything out of range or unaligned, and
reports a failed commit through the handshake. `lbpf_sector_erase` presents
uniform blocks over uneven sectors: the F4's 64 / 128 kB sectors behind
16 kB blocks, each sector erased once, at the first erase request inside it.

**Window, stmbl F4:** offset 0 is 16 kB below `0x08010000` (block 0, a
placeholder for the bootloader's area). Block 1 is the application start.
Window size 16 kB + 960 kB; erase 2¹⁴, write 2⁸. **brdstepper G431:**
uniform 2 kB pages (erase 2¹¹), block 0 the bootloader's first page.

**The image** for setsserial (`tools/mkssfw.py`, to do):
- one block of zeros (block 0), then the application `.bin` padded with
  0xFF;
- **no all-zero erase block inside the application**: setsserial would skip
  it, it would read erased (0xFF), and the CRC check would keep the
  bootloader from starting the application (tested).

**Entering the bootloader:**
- the application's console command `sserialboot` sets the RAM trigger word
  and resets, as the existing `0xDEADBEEF` → DFU path does;
- or the application fails its CRC.

The bootloader then runs a minimal smart-serial slave at 115200 baud: name
`stbl`, an empty process data table, plus `lbp_flash`. hostmot2 can load
it, and `setsserial flash hm2_….stbl.… image.bin` does the rest. It jumps to
the application once its CRC (`version_info` at `0x188`, as today) checks
out.

## In-band: staged update over the console

`shared/fwstage.c`. The application receives the image into a staging area
of its own flash, while it keeps controlling the motor:
```
fwbegin SIZE CRC32          erase staging
fwdata OFFSET BASE64 CRC16  48 bytes a line, in order
fwend                       CRC32 over the whole staged image
fwapply                     mark the update pending, reset
```
- **Line checks:** a line with a bad CRC16 is refused (the sender repeats
  it), a repeat of the last line is harmless, and a gap is refused.
- **Resume:** the offsets let an interrupted transfer resume.
- **The copy:** at reset the bootloader sees the pending mark and checks the
  staged CRC32 again. A bad staged image is never copied. It erases the
  application area, copies, verifies, then clears the mark. A power loss
  during the copy leaves a good staged image and a bad application, so the
  bootloader copies again.

**Layout, stmbl F4** (1 MB):

| region | addresses | size |
|---|---|---|
| bootloader | sectors 0 | 16 kB |
| config | sectors 2–3 | 2 × 16 kB |
| application | sectors 4–7 | 448 kB; today ~345 kB |
| staging | sectors 8–11 | 512 kB |

**Layout, brdstepper G431** (128 kB): bootloader 8 kB, application 56 kB,
staging 56 kB, config 8 kB.

`servoterm-cli --update image.bin` (to do) would send the lines, check the
replies and resume after errors. It works over USB too.

## Status of this branch

Done (host-tested: `make -C host check` runs `test_bootloader`):
- `shared/lbp_flash.c`: setsserial's whole programming sequence replayed
  byte for byte through an LBP dispatcher on a model of the F4's flash (bits
  only 1 → 0 without an erase). Every byte written, never onto unerased
  flash, each touched sector erased once, block 0 never written and its
  erase refused (no handshake, so setsserial aborts), the all-zero-block
  caveat.
- `shared/fwstage.c`: staging with damaged, repeated and gap lines; the
  CRC32; the copy; a bad staged image never copied.

To do, on hardware:
- **Bootloader:** the LBP parser split out of `src/comps/sserial.c` so the
  bootloader can use it; flash drivers (F4 sectors, G4 pages); 115200 baud
  (and 2.5 Mbaud) detection; the trigger word; the pending mark.
- **Application:** the commands `fwbegin` / `fwdata` / `fwend` / `fwapply`
  / `sserialboot` (`fwdata` writes flash outside the realtime loop).
- **Tools:** `tools/mkssfw.py` (the setsserial image, checking for all-zero
  blocks) and `servoterm-cli --update`.
- **Real hardware:** a test with a Mesa card and `setsserial`, including
  hm2's per-command overhead, which the host model does not include.
