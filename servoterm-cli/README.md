# servoterm-cli

Headless command-line client for a **real** stmbl drive over its USB serial
port (`/dev/ttyACM0`, USB ID `0483:5740`). It is the hardware counterpart of
`host/servoterm` (which drives the x86 simulator) and uses the same wire
protocol: command lines in; text plus binary scope frames out.

## Build

```sh
cd servoterm-cli
make
```

Needs only a C compiler. The user must be in the `dialout` group.

## Usage

```sh
./servoterm-cli [-d DEV] [-s FILE] [-t SEC] [-w MS]
                [--dump FILE] [--load FILE [--no-backup]]
                [-C pin:gain,...] [--step N] [--log SEC]
                [--capture FILE [--no-arm] [--capture-timeout SEC]]
                [--update IMAGE [--no-apply]] [--expect-name NAME] [--force-name]
                [-c CMD]... [SCRIPT]
  -d DEV    serial device (default /dev/ttyACM0)
  -s FILE   write decoded scope samples to FILE as CSV
  -t SEC    after the last command, exit once no text arrived for SEC s (default 0.5)
  -w MS     delay between command lines in ms (default 50)
  -c CMD    send CMD (repeatable); takes precedence over SCRIPT/stdin
  --dump FILE   save the drive's flash config to FILE
  --load FILE   write FILE to the drive's flash config (see below)
  -C LIST       scope channels pin:gain,... (see Scope logging)
  --step N      scope sample interval in rt cycles; adds a time column
  --log SEC     keep recording for SEC seconds (0 = until stopped)
  SCRIPT    file of commands; '#' comments and blank lines are skipped
```

With no `-c`/SCRIPT and a terminal on stdin it runs interactively (Ctrl-D quits).

```sh
./servoterm-cli -c about                 # firmware / bootloader / hv versions
./servoterm-cli -c list                  # comp instances and their pin links
./servoterm-cli -c hal                   # rt/frt timing stats
./servoterm-cli -c pid0.pos_error        # read a pin
./servoterm-cli -s scope.csv -t 2 -c fault0.state   # also capture ~2 s of scope
```

## Drive name and motor type

A drive keeps its own name and its motor's type in its flash config, as two
comment lines (`shared/confident.h`):

```
#stmbl_name x-axis
#stmbl_motor SEW CMP40M/BP/KTY/RH1M
```

Set them on the drive with `name x-axis` and `motor SEW CMP40M/BP/KTY/RH1M`
(at most 32 characters; `name -` clears it). That rewrites the flash config
at once, so the drive must be disabled, and unsaved `appendconf` edits are
dropped. `name` / `motor` alone print them, `ident` prints both
machine-readable, `about` and the boot messages show them.

servoterm-cli asks on connecting and says which drive it is on:

```
$ ./servoterm-cli -d /dev/ttyACM1
servoterm-cli: /dev/ttyACM1: drive x-axis, motor SEW CMP40M/BP/KTY/RH1M
x-axis SEW CMP40M/BP/KTY/RH1M> 
```

Against the wrong drive:
- `--expect-name NAME` does nothing (exit 2) unless the drive is NAME. Put it
  in scripts: `./servoterm-cli -d /dev/ttyACM1 --expect-name x-axis -c ...`.
- `--load FILE` refuses a config whose `#stmbl_name` differs from a named
  drive's, a config without one included; `--force-name` loads it anyway
  (renaming the drive to the file's name). Keep the two lines in each
  drive's config file.

## Config dump / load

```sh
./servoterm-cli --dump my.conf               # flash config -> file ('-' = stdout)
./servoterm-cli --load my.conf               # file -> flash, then verify
./servoterm-cli --load my.conf -c reset      # ...and reboot the drive to apply it
```

`--load` does, in order:

1. Checks the whole file first. Every line must be at most **52 characters**
   and printable ASCII, and the whole file at most 15 KiB. If anything fails,
   the drive is not touched.
2. Reads the current flash config. If it already matches the file, stops.
   Otherwise saves it to `conf-backup-YYYYmmdd-HHMMSS.txt` (skip with
   `--no-backup`).
3. `deleteconf`, then one `appendconf <line>` per line, waiting for the
   drive's `adding ...` reply each time. If a line gets no reply, it stops
   before flash is written.
4. `flashsaveconf`, then `showconf` to check that flash matches the file.

Only the stored config changes. The running drive keeps its current setup
until it is reset (`-c reset` or a power cycle).

Lines are compared the way the drive stores them: leading and trailing blanks
and trailing empty lines are dropped, and tabs become spaces. So a dumped file
loads back unchanged.

## Scope logging

```sh
./servoterm-cli -C "pid0.pos_error:1000,vel1.vel:0.35,pid0.torque_cmd:250" \
                --step 5 -s run.csv --log 0 > run.txt
```

- `-C pin:gain,...`: link `term0.wave0..7` to these pins and set their
  gains (offsets to 0). This is a RAM setting, not saved to flash. The drive
  sends `value * gain + 128` as one byte (clamped to 1..254), so full scale
  is +-127/gain and the resolution is 1/gain. The CSV gets the pin names as
  headers and the values in the pin's own units.
- `--step N`: `term0.send_step`, i.e. sample every N rt cycles (5 kHz / N),
  and add a time column `t`.
- `--log SEC`: record for SEC seconds (0 = until Ctrl-C / SIGTERM /
  SIGHUP). The CSV is flushed every second, and drive text lines are
  prefixed with the host time.

LinuxCNC talks to the drive over smart-serial, so the USB console is free
for logging during a LinuxCNC run.

## Protocol notes

- The drive streams scope frames (`0xFF` + 8 channel bytes, value =
  `(byte-128)/128`) continuously while `term0.send_step > 0`, interleaved with
  text. servoterm-cli splits them; scope frames alone do not count as
  "activity" for the `-t` quiet timeout.
- On open, the tool waits 0.3 s and discards that input: the drive only
  accepts commands once it has seen the connection (DTR), and the stream
  usually starts mid-frame.

Commands like `start`, `stop`, `enable`, pin writes and `flashsaveconf` change
the drive's state. Make sure the machine is safe before sending them.

## Capture scope (`--capture FILE`)

The drive's `scope0` captures into its own memory and is downloaded
afterwards. That works over a slow link too: the smart-serial console
(`linuxcnc/sserialterm`), about 14 s for 2 × 2048 samples. Set it up with
`-c` (channels, trigger, level, pre-trigger); `--capture` then arms it
(unless `--no-arm`), waits (`--capture-timeout`, default 60 s),
downloads, checks the line CRCs and writes CSV: `t` (0 at the trigger) and
a column per channel. See `../docs/capture_scope.md`.

## Firmware update (`--update IMAGE`)

In-band update through the drive's `fwup` component, over USB or the
smart-serial console:
```sh
servoterm-cli -d /tmp/drive-x --dump drive-x.conf     # back up the config first
servoterm-cli -d /tmp/drive-x --update obj_app/stmbl.bin
servoterm-cli -d /tmp/drive-x -c about                # after the reset
```
How it runs:
1. **The drive must be disabled** (machine off or in estop). Otherwise
   `fwbegin` answers `fw err drive enabled` and servoterm-cli stops.
2. **Staging:** the image goes as `fwdata` lines of 30 bytes with a CRC-16
   each, 3 lines in flight. A damaged, refused or lost line is sent again,
   and `fw busy` means wait.
3. **Check:** `fwend` checks the CRC32 of the staged image.
4. **Apply:** `fwapply` makes the drive reset, and its bootloader copies the
   image into place. With `--no-apply` it stops after the check;
   `fwapply` on the console applies later.
5. **Interrupted:** run the same command again and it resumes where the
   drive is.

Speed: over USB, seconds; over smart-serial, ~350 B/s (~15 min for the
whole application). See `../docs/sserial_bootloader.md`, also for the
recovery path (`sserialboot` and LinuxCNC's `setsserial flash`).

## Setup wizard (`--wizard`)

Sets up a drive and its motor step by step: firmware check, config
backup, supply, nameplate, feedback, wiring help, then it measures the
motor on the drive (encoder resolution by hand, `id_pmsm`, `id_mot`,
`id_pid`), sets the limits and the command link, checks the final config
and saves it with a report.
```sh
servoterm-cli -d /dev/ttyACM0 --wizard                 # resumes from stmbl-setup-session.txt
servoterm-cli -d /dev/ttyACM0 --wizard --redo tuning   # repeat a step and what follows
```
The code is a library in `setup/` (a GUI can use it); `make check` runs
its unit tests. See `../docs/setup_wizard.md`.
