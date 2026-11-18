# Servo thread and stmbl rates: where the time goes, what to improve

The setup is a Pi 4 with a Mesa 7C81 over SPI (hostmot2 smart-serial
firmware), stmbl drives on smart-serial at 2.5 Mbit/s, and LinuxCNC 2.9.10
with `hm2_spix`. The numbers come from the bench (`DEBUGNOTES.md`,
2026-10-14 and 2026-10-21) and from the HSM 400U simulation
(brdmot's `sim/mill/hsm_runs.py --rates`).

## Summary

- **Recommendation:** run at **2 kHz**. It fits the budget today, and the
  simulation shows it halving the error on fast, tight contours.
- **Higher than 2 kHz:** about 4 kHz needs two things. The SPI time must
  shrink, which means a different bus to the FPGA, not tuning. The drive's
  smart-serial busy-wait must go. 5 kHz is not reachable and gives little.
- **More cores don't speed up the servo thread itself.** Its work is
  one strict sequence each period, and LinuxCNC 2.9 runs every realtime
  thread on one CPU. Other cores help indirectly, by keeping everything
  else off that CPU. That lowers the worst case, and the worst case is the
  limit.
- **The FPGA already does the time-critical framing.** The worthwhile
  offload is a faster bus to it: a Zynq (FPGA and CPU on one chip) or PCIe.
  A computing offload isn't worth it, because the drive already does the
  fine interpolation at 5 kHz.

## Where the time goes

```
servo period (LinuxCNC, Pi 4)                  drive (STM32F4)
 hm2 read   ~34 us  (SPI: last period's replies)
 motion, kins, HAL ~10-14 us
 hm2 write  ~24 us  (SPI: commands + "doit")  ->  7C81 sends 12 bytes   ~53 us
                                                  drive frt receives, busy-waits <= 22 us
                                                  drive replies 14 bytes ~62 us
                                                  reply waits in the FPGA until the next hm2 read
 -------------------------------------------
 total: avg 67-72 us, max 175-192 us           smart-serial exchange ~130-150 us
```

- **The Pi's share:** about 80 % of the servo thread is SPI I/O. The
  computing (trajectory, kinematics, HAL) is about 10-14 us. Making the
  computing faster or splitting it up can't gain much.
- **The limiting number:** the worst case, not the average. A maximum of
  ~190 us against an average of ~70 us is jitter from the Pi (other cores'
  load, interrupts, the SPI driver).
- **Smart-serial:** the 7C81 handles the protocol (sslbp), and its channels
  run in parallel, so 5 drives take as long as 1. At 11 bits per byte, 26
  bytes take ~115 us on the wire, plus the drive's turnaround. The exchange
  has to finish before the next period's hm2 read, or hostmot2 reports a
  smart-serial error.
- **The drive:** its rt loop runs at 5 kHz (200 us), and its position and
  velocity loops close there. LinuxCNC's packets only set the command, which
  `vel_int` mode 1 interpolates quadratically, with acceleration
  feedforward. The rt worst case was 168.5 us of 200. About 28 us of that
  is the smart-serial frt busy-waiting for a packet's last bytes, once per
  packet.

## The budget at each rate

| servo rate | period | the Pi (max ~190 us) + smart-serial (~150 us) | the drive (one packet per period) | verdict |
|---|---|---|---|---|
| 1 kHz | 1000 us | 340 us, fits | busy-wait every 5th rt cycle | today |
| **2 kHz** | 500 us | 340 us, fits | every 2nd-3rd rt cycle | **fits now** |
| 4 kHz | 250 us | 340 us, **does not fit** | 4 of every 5 rt cycles: rt ~150-170 of 200 us | needs both fixes below |
| 5 kHz | 200 us | does not fit | every rt cycle | no |

- **4 kHz needs the Pi's worst case under ~90 us:** the write must happen
  early enough for the 150 us exchange to finish before the next read.
- **2 kHz has margin:** the exchange finishes by about 340 us of the
  500 us period.
- **Where 4 kHz could come back:** with a faster bus, the smart-serial
  exchange (~150 us) becomes the floor.

## What the rate buys (simulated HSM 400U, motor encoders)

brdmot's `python3 sim/mill/hsm_runs.py --rates` measures the largest contour error
in µm:

| scenario | 1 kHz | 2 kHz | 4 kHz | 5 kHz |
|---|---|---|---|---|
| circle R 10 at 10 m/min (0.28 g) | 16.8 | 7.2 | 2.6 | fault (sim) |
| circle R 25 at 5 m/min | 3.0 | 2.2 | 2.1 | fault (sim) |
| five-axis line (B 0->20, C 0->60) | 7.2 | 7.1 | 7.3 | fault (sim) |
| tool centre point (B 30, C 90) | 6.8 | 6.6 | 6.2 | fault (sim) |

- **1 to 2 kHz:** a clear gain on tight, fast curves. The command is
  sampled twice as often, so the drive's quadratic interpolation follows
  the curvature better.
- **2 to 4 kHz:** helps only the tightest, fastest contour. At the owner's
  8000 mm/min, curvature like the R 10 circle at 10 m/min is rare.
- **Slow and five-axis moves:** flat from 1 kHz on. The drive's own
  loops, already at 5 kHz, set those errors.
- **5 kHz in the simulation:** with one packet in every rt cycle, the
  simulated drive faults (code 1) before it syncs. This is open in
  `DEBUGNOTES.md`. The budget table rules out 5 kHz on hardware anyway.

## Optimisations, by cost

### 1. The Pi (cheap, do first)

- **Keep one core for realtime only.** Boot with `isolcpus=3 nohz_full=3
  rcu_nocbs=3 irqaffinity=0-2`. LinuxCNC puts its realtime threads on the
  last isolated CPU, or on `RTAPI_CPU_NUMBER` if that is set. Keep
  millweb, the browser, the interpreter, Python and the network stack on
  cores 0-2. Check with `taskset -p` and `/proc/interrupts`.
- **CPU frequency:** set the `performance` governor and disable deep idle
  states on the realtime core.
- **Fewer SPI words per period:** hostmot2 reads and writes every enabled
  module's registers each period. Disable what the machine doesn't use, for
  example `num_encoders=0 num_pwmgens=0 num_stepgens=0`, unused smart-serial
  channels, and GPIO reads not wired to anything. Every 32-bit word costs
  ~1 us at 35 MHz.
- **Measure what matters:** the servo thread's `tmax` over an hour of
  cutting with the console open, not the 10 s average.
  `hwtest/suite`'s "Pi side" test already records `host_*`.
- **Expected gain:** the worst case drops toward ~100 us. That secures
  2 kHz but is not enough for 4 kHz.

### 2. The drive firmware (needed above 2 kHz)

- **Remove the frt busy-wait:** reply on the next frt tick from DMA
  instead of spinning up to 22 us (`src/comps/sserial.c`).
- **Speed up adc0:** sum its packed 16-bit samples with `__UADD16`, about
  30 us less.
- **Effect:** together these take the rt worst case from ~168 to ~110 us of
  200, the margin a packet every rt cycle would need. Both are already
  listed under "rt worst case 168.5 us" in `DEBUGNOTES.md`.

### 3. More cores

- **Why it doesn't parallelise:** the servo period is read → motion and
  kinematics → write, each step needing the one before. LinuxCNC 2.9's
  uspace runs every realtime thread on one CPU (`find_rt_cpu_number` in
  `uspace_rtapi_app.cc`), so even a second HAL thread shares that core.
- **What works:** a slower second thread (for example 10 ms for watchdogs,
  powerquota or the console) for functions that don't need to run every
  servo period. That shortens the servo thread by those functions' time.
  Everything non-realtime on the other cores (item 1).
- **Pipelining:** reading period N+1 while computing period N would add a
  full period of delay, which costs more accuracy than the higher rate
  gains.

### 4. The FPGA

- **Already in the FPGA:** the smart-serial protocol, bit timing and CRC
  (sslbp in the 7C81). LinuxCNC 2.9's smart-serial driver has no DPLL
  timer, unlike the encoder and stepgen modules, so the exchange starts
  when the servo thread writes "doit".
- **What would help: a faster bus to the FPGA.** SPI costs ~58 us of each
  period.
  - **Zynq** (the EBAZ4205 in `TODO.md`, or a Zynq-based Mesa card):
    hostmot2 in the FPGA fabric and LinuxCNC on the chip's own ARM cores.
    Register access over the internal bus takes ~1 us, so the servo thread
    would be ~20-30 us, and 4 kHz fits with the drive fixes. The Cortex-A9
    computes about half as fast as the Pi 4, but the computing is only
    ~10 us. It needs a hostmot2 driver for that bus and a bitfile with
    smart-serial: the bigger job.
  - **x86 PC with a PCIe Mesa card** (5i25/6i25 class and a smart-serial
    daughter card): fast I/O with mature drivers. Bigger and needs more
    power than the Pi.
  - **Ethernet cards** (7i96S, 7i92T) are not faster. A read/write round
    trip takes ~100-200 us, more than the SPI's 58.
- **Not worth moving into the FPGA:** the trajectory interpolation. The
  drive already interpolates LinuxCNC's samples at 5 kHz, with
  acceleration feedforward. The only remaining gain from a higher rate is
  sampling tight curves (R 10 at 10 m/min), which 2 kHz mostly covers.

## The front ends on hand: measure all three

A Zynq board (the EBAZ4205) and a Pi with PCIe are both available, so the
bus question can be answered by measurement instead of estimates.
`hwtest/timing/servo_rate.py` runs the same HAL session on each and writes
`servo_rate.md`: servo thread, hm2 read and write, average and largest,
smart-serial errors, and a verdict per period. The drives are connected but
not enabled.

| | Pi 4 + 7C81 (SPI) | Pi 5 / CM4 + PCIe Mesa card | Zynq EBAZ4205 |
|---|---|---|---|
| driver | `hm2_spix` | `hm2_pci` | `hm2_soc_ol` (bitstream via `fpga_manager`, `webui/mill/server/fpga_load.py`) |
| hm2 read + write | **58 us measured** | estimate: writes posted (a few us), reads ~1 us per 32-bit word: ~10-30 us, depending on the modules enabled | estimate: the AXI bus, ~0.1-0.2 us per word: a few us |
| CPU for motion and kinematics | Cortex-A72 1.5 GHz: ~10 us | Cortex-A76 2.4 GHz: ~5 us (estimate) | Cortex-A9 667 MHz: ~25-30 us (estimate) |
| servo thread, largest | **175-192 us measured** | to measure | to measure |
| console (millweb) beside it | yes | yes | 2 small cores and little RAM: heavy suites remote (`REMOTE=board:8080`) |
| to check first | - | the Pi's PCIe link to a full-size card (adapter or HAT); `hm2_pci` on arm64 (uspace maps the card's BAR through sysfs); a bitfile with smart-serial | a PREEMPT_RT kernel and LinuxCNC (armhf) on it; a hostmot2 bitfile with smart-serial on its pins; the RS-422 interface used with the 7C81, wired to the EBAZ's 3.3 V pins |

- **The deciding number:** the largest servo time, not the average. 4 kHz
  needs it under ~90 us with the 150 us exchange; 3 kHz (333 us) under
  ~180 us.
- **Expected results:** the Zynq should have the shortest I/O but the
  slowest CPU, and its jitter on an RT kernel is unknown. The Pi 5 with
  PCIe should be strong on both, if `hm2_pci` works there.
- **The drive firmware fix (item 2) is needed either way** for anything
  above ~2.5 kHz. The front end only moves the Pi side of the budget.

```
# each front end, LinuxCNC stopped, as the machine's user:
python3 hwtest/timing/servo_rate.py                                   # Pi 4 + 7C81 (stmbl.ini's driver)
python3 hwtest/timing/servo_rate.py --driver hm2_pci \
    --config "num_encoders=0 num_pwmgens=0 num_stepgens=0 sserial_port_0=00000000"
python3 hwtest/timing/servo_rate.py --driver hm2_soc_ol \
    --config "num_encoders=0 num_pwmgens=0 num_stepgens=0 sserial_port_0=00000000"
# with and without isolcpus; --periods 1000,500,333,250 (default), --seconds 600 for a long run
```

**On the Zynq:** step-by-step in `docs/zynq-measurement-plan.md`: OS latency
(cyclictest), the card alone, idle motion, a five-axis load
(`hwtest/timing/servo_load.py`), the console, the drive side, the wire.

## Plan

1. Isolate the realtime core, trim the hostmot2 modules, and log `tmax`
   for an hour under load.
2. Run 2 kHz on the bench: `SERVO_PERIOD = 500000`, with the drives'
   `vel_int0.cmd_freq = 2000`. Check the drive rt maximum and the
   smart-serial error count.
3. Drive firmware: remove the smart-serial busy-wait and use the adc0
   `__UADD16` sum. These are needed before anything above 2 kHz.
4. Measure the three front ends with `servo_rate.py` and pick by the
   largest servo time. With the drive fix and a front end under ~90 us,
   go to 4 kHz.
