# Zynq (EBAZ4205) servo-rate measurement plan

**Goal:** find the servo rate (2, 3 or 4 kHz) that LinuxCNC on the EBAZ4205
runs reliably with the stmbl drives on smart-serial. Every step produces a
markdown table; send them back for the comparison.

**What the estimate says** (`docs/servo-timing.md`, unmeasured):
- servo thread ~40-60 us average, ~100-140 us worst
- 3 kHz realistic, 4 kHz only if the worst case comes in low
- reference, Pi 4 + 7C81 over SPI (measured): 67-72 us average, 175-192 us
  worst

**The rule used throughout:**
- **Budget:** the worst servo time plus the smart-serial exchange (~150 us
  today, ~125 us estimated with the drive's `reply_mode = 1`) must stay under
  80 % of the period.
- **Clean runs:** no smart-serial errors and no "unexpected realtime delay".

| rate | period | worst servo time allowed (exchange 150 / 125 us) |
|---|---|---|
| 2 kHz | 500 us | 250 / 275 us |
| 3 kHz | 333 us | 116 / 141 us |
| 4 kHz | 250 us | 50 / 75 us |

## 0. Setup

**Board:**
- EBAZ4205 with a PREEMPT_RT kernel.
- LinuxCNC 2.9 uspace (armhf).
- `hm2_soc_ol` and a hostmot2 bitstream with smart-serial on the pins
  wired to the drives.
- The RS-422 interface used with the 7C81, on the EBAZ's 3.3 V pins.

**Drives:**
- One or more stmbl drives, logic power only. Motor power can stay off: the
  drives exchange packets but are never enabled. The tools leave every
  enable pin at 0, and `servo_load.py` loops the joints back inside
  LinuxCNC.
- For step 6: the drive firmware from branch `claude/drive-rt-speedup`
  (`docs/drive-rt-speedup.md`).

**Software:**
- This repo (branch `claude/mill-webui`): `hwtest/timing/servo_rate.py` and
  `servo_load.py`.
- `rt-tests` (cyclictest) and `stress-ng`: `apt install rt-tests stress-ng`.
- A PC on the network for the console and iperf3 load.

**Record once** (paste into the results):
```
uname -a; cat /sys/kernel/realtime; cat /proc/cmdline; nproc; free -m
linuxcnc_var LINUXCNCVERSION || dpkg -l linuxcnc-uspace | tail -1
cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor 2>/dev/null
cat /sys/class/fpga_manager/fpga0/state; sha256sum <bitstream>
cat /proc/interrupts
```

**Two board configurations to compare.** Every step from 1 on runs in both,
because the 7010 has only two cores.
- **A, plain:** the kernel as it boots.
- **B, isolated:**
  - Boot with `isolcpus=1 nohz_full=1 rcu_nocbs=1 irqaffinity=0`.
  - Set the `performance` governor (or fix the clock at 667 MHz).
  - If cpuidle exists, boot with `cpuidle.off=1`.
  - LinuxCNC puts its realtime thread on the last isolated CPU (1).
    `RTAPI_CPU_NUMBER=1` forces it.
  - Check with `cat /sys/devices/system/cpu/isolated` and `ps -eLo
    psr,comm | grep rtapi`.

## 1. Operating-system latency (no LinuxCNC), ~25 min

```
# idle, 10 min, on the RT core, at the 3 kHz period
sudo cyclictest -m -p 95 -a 1 -t 1 -i 333 -h 400 -q -D 10m > cyclic_idle.txt
# under load on core 0: CPU, memory, I/O, network (iperf3 -s on the board, iperf3 -c <board> -t 600 on the PC)
taskset -c 0 stress-ng --cpu 1 --vm 1 --vm-bytes 64M --io 1 --timeout 11m &
sudo cyclictest -m -p 95 -a 1 -t 1 -i 333 -h 400 -q -D 10m > cyclic_load.txt
```

- **Record:** the max latency, and the 99.99 % point from the histogram.
- **Pass:** max under 80 us under load (configuration B).
  - Over 150 us: fix the system before going on. Typical causes are the
    network driver's interrupt on core 1, cpuidle, and SD-card I/O; check
    `/proc/interrupts`.
- **Why:** this jitter is the floor under everything below.

## 2. The card alone: hostmot2 read/write over the Zynq's bus, ~10 min

Load the bitstream first:
```
sudo python3 webui/mill/server/fpga_load.py bitstreams/<file>.bit.bin
```

Then:
```
python3 hwtest/timing/servo_rate.py --driver hm2_soc_ol --no-motion --seconds 120 \
    --config "num_encoders=0 num_pwmgens=0 num_stepgens=0 sserial_port_0=00000000" --out step2_trim.md
python3 hwtest/timing/servo_rate.py --driver hm2_soc_ol --no-motion --seconds 120 \
    --config "sserial_port_0=00000000" --out step2_full.md
```

- **Record:** the read and write time, average and largest, at 1000, 500,
  333 and 250 us.
- **Expected:** a few us each, against 34 and 24 us over SPI.
- **Pass:** smart-serial errors 0 at 1000, 500 and 333 us.
  - Errors at 250 us mean the exchange doesn't finish within the period.
    That is the first measured hint of the real exchange time.
- **Compare trim and full:** the difference is what each enabled module
  costs per period.

## 3. Idle motion: the thread LinuxCNC runs, no program, ~5 min

```
python3 hwtest/timing/servo_rate.py --driver hm2_soc_ol --seconds 60 \
    --config "num_encoders=0 num_pwmgens=0 num_stepgens=0 sserial_port_0=00000000" --out step3.md
```

- **Record:** the servo thread average and worst.
- **What it shows:** step 3 minus step 2 is the A9's cost for motion when
  idle.

## 4. Five-axis load, ~45 min per configuration

`servo_load.py` runs LinuxCNC with the HSM 400U's kinematics
(`xyzbc-trt-kins`), five joints looped back and the card in the thread. The
programs loop:
- tool centre point (B 30, C 90)
- the five-axis line
- the circle at B 20 / C 45
- a circle of 0.05 mm lines at 8000 mm/min with line joining off: the
  planner's worst case

```
# quiet board, then with the step-1 load on core 0 (stress-ng + iperf3)
python3 hwtest/timing/servo_load.py --driver hm2_soc_ol --seconds 600 \
    --config "num_encoders=0 num_pwmgens=0 num_stepgens=0 sserial_port_0=00000000" --out step4_quiet.md
python3 hwtest/timing/servo_load.py --driver hm2_soc_ol --seconds 600 \
    --config "num_encoders=0 num_pwmgens=0 num_stepgens=0 sserial_port_0=00000000" --out step4_load.md
```

- **Record:** servo average and worst, read and write, programs completed
  (should be > 0 at every period), realtime delays, smart-serial errors.
- **The deciding number:** the worst servo time in `step4_load.md` at
  333 us.
  - Compare it with the table at the top: 116 us (today's drive firmware)
    or 141 us (with `reply_mode = 1`).
- **Reference:** on an x86 PC the same load averages 24-30 us. Expect the
  A9 at 2-4x that.

## 5. With the web console

millweb on the board is the realistic second load: websocket, preview,
Python. Run the HSM simulation config with millweb as its display, open the
console from the PC, run a program in it, and keep it open.
- Meanwhile watch the servo thread with
  `halcmd show thread servo-thread` (Time, Max-Time) over 10 min.
- Or repeat step 4 with `make -C webui/mill serve-test` running and a
  browser attached.

**Pass:** the worst servo time stays within 10 % of `step4_load.md`.

## 6. The drive side at the chosen rate, ~30 min

Firmware from `claude/drive-rt-speedup`. Back up and flash as in
`docs/drive-rt-speedup.md`, only with the repo's make targets. Set the
drives' `vel_int0.cmd_freq` to the servo rate. Run `servo_load.py` at that
rate.

In the drive console (servoterm over USB), run `hal`. Note the rt time and
max (whole loop and `adc0`) and `sserial0`'s frt time for:

| `sserial0.reply_mode` | `adc0.fast` | rt max [us] (limit 200) |
|---|---|---|
| 0 | 0 | |
| 0 | 1 | |
| 1 | 0 | |
| 1 | 1 | |

**Pass at 3 kHz:**
- rt max under ~170 us
- `sserial0.crc_error` steady
- `sserial0.irq_count` counting (mode 1)
- no smart-serial errors in LinuxCNC

## 7. Optional: the exchange on the wire, ~20 min

With a scope or logic analyser on the RS-422 request and reply lines,
measure from the request's first edge to the reply's last edge, with
`reply_mode` 0 and 1. This replaces the 150 / 125 us estimate. Rerun the
verdicts with
`servo_rate.py ... --exchange-us <measured>`.

## 8. Then motion (motor safe to move)

At the chosen rate:
- the drive's usual hwtest suite on one axis
- a soak
- a following-error comparison against 1 kHz on the same moves

Only after this goes the rate into the machine config: `SERVO_PERIOD`, and
the drives' `vel_int0.cmd_freq`.

## Decision

**Pick the highest rate where, in configuration B:**
- step 4 (with load) has worst servo time + exchange < 80 % of the period
- smart-serial errors 0, realtime delays 0
- step 5 within 10 % of step 4
- step 6 drive rt max < ~170 us
- step 8 clean

The HSM simulation says 3 kHz gets almost all of 4 kHz's gain (circle
R 10 at 10 m/min: 7.2 / 3.1 / 2.6 um at 2 / 3 / 4 kHz). There's no need
to push for 4 kHz unless 3 kHz has a large margin.

## Results to send back

```
setup.txt (the "record once" block)   cyclic_idle.txt  cyclic_load.txt
step2_trim.md  step2_full.md  step3.md  step4_quiet.md  step4_load.md  (A and B)
step 5 and 6 notes, step 7 scope numbers if done
```
