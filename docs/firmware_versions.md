# Firmware versions, and configs across them

A drive with older firmware than the rest of the system expects faults in
ways that are hard to read (a config linking pins the firmware lacks, an HV
board speaking an older protocol). So the firmware has one version number,
counted up, and everything checks it:

| what | where |
|---|---|
| **the number** | `STMBL_FW_VERSION` in `shared/version_number.h` (1: the first versioned firmware). The LV (F4) and HV (F3) images both carry it (`version_info_t.fw_version`, with `fw_magic`). |
| **HV against LV** | The HV board reports the version it runs in its state rotation (`f3_state_data_t.fw_version`); the F4 compares it with the HV image it carries. Another one, or an HV board from before versioning (0), is fault `HV_VERSION_ERROR` (19): the drive won't enable. `hv_update` flashes the carried image; `hv0.fw_check = 0` turns the check off. Pins: `hv0.fw_version` (running; -1 not known yet), `hv0.fw_image`. |
| **the config** | A config written for a version carries the line `#stmbl_conf_version N` (first line). At boot the drive warns about a config for another version. |
| **`version`** | A console command, machine-readable: `version fw=1 hv=1 hv_image=1 conf=1`, then `hv_mismatch`, `conf_old` or `conf_newer` if so. Firmware from before versioning answers `not found: version`. `about` prints the versions too. |
| **the console** | webui/mill expects the firmware of its own checkout (or `[MILLWEB] DRIVE_FW`; machine.json `stmbl.fw`). The Drives tab shows each drive's versions and what is wrong; a drive with another firmware, an HV board with another one, or a config that needs a patch keeps the machine off (`DRIVE_FW_BLOCK = no`, machine.json `stmbl.block: false`, only says so). |

## Config patches

`conf/patches/NNNN.json` says how a config for version N-1 becomes one for N,
for the versions that change what a config must say (most change nothing and
have no file). Each operation has an exact inverse, so a config also goes
back down:

| op | does | undone by |
|---|---|---|
| `{"op": "rename", "from": "a0.x", "to": "a0.y"}` | the pin, wherever it appears (refused if `a0.y` is in use already) | the rename back |
| `{"op": "set", "pin": "a0.x", "from": 1, "to": 2}` | only if it is still `from` (a value set by hand is kept); from null: a new line; to null: the line goes | the set back |
| `{"op": "line", "from": "link old", "to": "link new"}` | a whole line; from null: added; to null: removed | the line back |

`tools/stmblconf.py` applies them (`version`, `plan`, `upgrade`, `downgrade`
a config file); the console uses the same code. `host/tests/test_stmblconf.py`
(in `make -C host check`) takes every config in `conf/` and `hwtest/` up to
this version and back down and checks it comes back unchanged.

## Upgrading a drive, and going back

1. **Back up its config**: the console's Drives tab, Config…, *Back up now*
   (or `servoterm-cli --dump`). Every write below keeps one too.
2. **Flash** the firmware of this checkout with the repo's make targets
   (`make btburn`, see CLAUDE.md). The console doesn't flash.
3. If the card says the HV board runs another version: `hv_update` in the
   drive's console (machine off).
4. **Patch its config**: Config…, *Patch for vN…* shows each step, then
   writes it (DRIVE_WRITE, control, machine off), reads it back, and keeps
   the one before. The drive runs it from its next start: *Reset drive*.

**Going back:** flash the older firmware (check out its commit, `make
btburn`), then *Restore* the backup taken before the patch (exact), or
patch the config down (`tools/stmblconf.py downgrade CONF --to N`).

## When you change the firmware

- Count `STMBL_FW_VERSION` up when a change matters to a config, the HV
  board or the console (a pin renamed or added that templates link, a
  packet changed, a console command the console relies on).
- If configs must change with it, add `conf/patches/NNNN.json` for the new
  number; `make -C host check` proves it undoes on every config here.
- The console of the same commit expects the new number; drives still on
  the old one show what to do.
