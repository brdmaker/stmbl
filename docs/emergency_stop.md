# Emergency braking: stopping instead of coasting

Until this branch, every fault and every disable switched the power stage off
at once, so a moving axis coasted to a stop on friction. On the burn-in axis
(10 mm per revolution) at 300 mm/s that is 26 mm and 180 ms. On a linear axis
at speed it means hitting the end stop.

Now the drive brakes before it switches off, depending on why it is stopping:

| Cause | Default | What happens |
|---|---|---|
| Link to LinuxCNC lost (smart-serial or CAN, `CMD error`) | **quick stop** | The drive brakes along a ramp on its own feedback, then switches off: 3 mm instead of 26 |
| Enable dropped while moving (LinuxCNC machine off or estop) | **quick stop** | The same, with no fault: 1.9 mm |
| Feedback fault (`mot`/`com`/`joint FB error`) | **dynamic brake** (PMSM) | The motor is shorted through a virtual resistor; no feedback needed: 5 mm |
| Position, saturation, temperature or DC-bus error (measured on the F4) | **dynamic brake** (PMSM) | The same |
| HV board fault (overcurrent, F3 fault pin, F3 timeout, ...) | coast | The power stage can't be trusted: outputs off at once |

The distances are from the simulator (burn-in motor ECMA-C30604FS, 188 rad/s).
They are measured from the moment of the failure, so they include detecting
it. The tests are `host/tests/estop_*.test`.

**Before relying on this on a real machine:**
- This is firmware. A safety-rated stop (STO, a contactor, mechanical end
  stops) is still needed. If the drive itself fails (F4 crash, no logic power,
  no DC bus) nothing here runs.
- None of it has run on hardware yet. Try it at low speed first.

## Contents

- [Configuration](#configuration)
- [Quick stop (`qstop`)](#quick-stop-qstop)
- [Dynamic brake (`dbrake`)](#dynamic-brake-dbrake)
- [Sequencing (`fault`)](#sequencing-fault)
- [Testing in the simulator](#testing-in-the-simulator)
- [Limits and open points](#limits-and-open-points)

## Configuration

The templates set the defaults: `conf/template/pid.txt` sets `cmd_stop` and
`en_stop`, and `conf/template/pmsm.txt` sets `fb_stop` and `err_stop`. Override
them in the drive config after the `link` lines, as with any pin.

| Pin | Values | Template default |
|---|---|---|
| `fault0.cmd_stop` | 0 coast, 1 quick stop, 2 dynamic brake | 1 |
| `fault0.en_stop` | the same, for enable dropped while moving | 1 |
| `fault0.fb_stop` | 0 coast, 2 dynamic brake (1 counts as 2: a quick stop needs feedback) | 2 (PMSM), else 0 |
| `fault0.err_stop` | 0, 1, 2 | 2 (PMSM), else 0 |
| `fault0.stop_time` | the longest stop [s]; after it the outputs go off | 2 |
| `qstop0.dec` | deceleration [rad/s²]; 0 = `dec_share` × max torque / (j + j_sys) | 0 |
| `qstop0.dec_share` | share of the motor's deceleration capability | 0.3 |
| `qstop0.tj` | S-curve time: the deceleration builds up and eases off over this [s] | 0.003 |
| `dbrake0.max_cur` | current limit while braking [A] | `conf0.max_ac_cur` |

- **To get the old behaviour back** (coast on everything), set `fault0.cmd_stop = 0`
  and `fault0.en_stop = 0` (and `fb_stop` / `err_stop = 0` for PMSM configs).
- **`en_stop = 1` changes what a normal disable does.** Machine off now stops
  the motor along a ramp first, about 20 ms extra at standstill, before the
  outputs go off.
- **Holding brake:** `fault0.mot_brake` stays released during a stop. It
  engages once the motor stands still and the outputs go off.

## Quick stop (`qstop`)

`shared/comps/qstop.c` sits in the command path, between `reslimit0` / `vel0`
and `pid0`. Normally it passes the command through unchanged.

**While the link is in trouble:** the link components output garbage when the
link fails. `vel_int` zeroes the velocity after 2 ms without packets, and
`can0` zeroes the position command at its timeout. While `hold` (the link's
error) or `hold2` (`vel_int`'s watchdog) is set, `qstop` holds the last good
command instead, extrapolated at its velocity.

**When `fault0` asks for a stop** (`stop`): `qstop` plans a stop from where the
motor is (`pos_fb`, `vel_fb`) to standstill:
- at `dec`, with an S-curve of `tj`;
- passing pid the position, velocity and acceleration of that plan, so the
  position loop keeps the motor on it.

**If the motor can't follow:** the position error is limited to `max_err`
(0.5 rad), so the motor is never dragged back further than that. `fault0`
also ignores position and saturation errors during a quick stop.

**When it's done:** `stopped` is set once the plan is at 0 and the measured
speed has stayed under `stop_vel` for 20 ms. `fault0` then switches the
outputs off.

Why only 30 % of the motor's capability by default: the drive's velocity
observer (`vel1`) lags at these decelerations. Its torque feedforward is a
50 Hz high-pass, so pid over-brakes and the stop rings. On the burn-in motor
from 188 rad/s:

| `dec_share` | Stop distance | Reverse travel |
|---|---|---|
| 0.5 | 2.7 mm | 0.11 rad |
| 0.3 | 3.1 mm | 0.01 rad |
| 0.2 | 3.8 mm | none |
| coasting | 26 mm | — |

## Dynamic brake (`dbrake`)

`shared/comps/dbrake.c` sits between the motor's current command
(`pmsm_ttc0`, `fb_switch0`) and `hv0`. It is only wired in `pmsm.txt`.
Normally it passes the command through unchanged.

**When braking:**
- It switches the HV board to voltage mode (`hv0.cmd_mode = 0`, which already
  exists for `id_pmsm`), so no F3 firmware change is needed.
- It commands a virtual resistor, u = −rv · i.
- It freezes the frame (`pos` held, `vel` = 0, so the F3 doesn't turn it
  between packets).

A resistor in the dq frame is a resistor in any frame, so this needs **no
rotor angle**. That is what makes it usable after a feedback fault.

With rv = 0 it is a short circuit, and the energy goes into the windings. At
speed a short is held back by the motor's inductance (ωL ≫ R): the current
stays near ψ/L, and it brakes weakly.

When the current would exceed `max_cur`, rv rises and holds it there. A
resistor brakes harder at speed than a short, but that energy goes into the
DC bus. Above `high_dc_volt`, rv falls back towards the short. The gains are
relative to rn = `pwm_volt` / `max_cur`, so they carry over between motors:

```
rv = rn * (kp * e + ki * integral(e)),   e = (i - max_cur) / max_cur
kp = 0.1, ki = 500/s, rv limited to 2 * rn
```

`stopped` is set once the current has stayed below `stop_cur` (5 % of
`max_cur`) for `stop_time` (0.1 s).

On the burn-in motor from 188 rad/s:
- **Shorted:** 5 mm. The current is about 2 A steady, with a transient up to
  about 5 A from the instant of the short; the limit is 10 A, so rv stays 0.
- **Limited to 2 A:** 3 mm.
- **Coasting:** 26 mm.

## Sequencing (`fault`)

The stop starts when the drive leaves ENABLED, either into SOFT_FAULT or into
DISABLED. `stop_for()` maps the fault code to a mode, and `fault0.stop`
outputs the mode in use (0, 1, 2).

**While a stop runs:**
- `en_out` stays on. `en_fb` and `en_pid` stay on only for a quick stop;
  `dbrake` needs neither.
- `fault0.state` shows the fault or DISABLED as usual.
- Re-enabling waits until the stop is over.

**A stop ends when:**
- the motor stands still (`qstop_done` / `dbrake_done`);
- or an HV fault comes in;
- or `stop_time` runs out.

**Other transitions:**
- A feedback fault during a quick stop turns it into a dynamic brake, or into
  coasting if `fb_stop = 0`.
- A link that times out drops enable together with its error. Enable dropping
  while the link error is set counts as a link loss (`cmd_stop`), not as a
  disable (`en_stop`).

## Testing in the simulator

New simulator pins:
- `enc_fb0.sim_fail = 1`: the encoder fails (position frozen, error set).
- `hv0.sim_fault = <code>`: an HV board fault (the bridge goes off).
- `hv0.sim_dist`: the unwrapped position, for measuring how far a stop went.
- `lcncstop` (smart-serial) and `canplug 0` (CAN): the link goes away.

```
make -C host check      # includes host/tests/estop_*.test
```

The `estop_*` tests:

| Test | What it checks |
|---|---|
| `link`, `can`, `disable` | the quick stop for each cause |
| `coast` | the old behaviour, with the stops off |
| `fb` | the dynamic brake after an encoder failure |
| `fb_limit` | the current limit |
| `hv` | HV faults still coast |
| `escalate` | a quick stop turned into a dynamic brake |
| `timeout` | `stop_time` |

LinuxCNC's burn-in tests (`hwtest/burnin-sim`: can, console, quota) pass
unchanged.

## Limits and open points

- **Ringing:** the velocity observer's lag limits how hard a quick stop can
  brake without ringing (DEBUGNOTES).
- **Frozen-encoder window:** after an encoder fails, the fault filter takes
  5 rt cycles (1 ms). During those the drive still commutates on the frozen
  angle, and in the simulator it applied up to 114 V. Starting the dynamic
  brake on the raw error would shorten that window (DEBUGNOTES).
- **Regeneration:** a rv > 0 stop feeds energy into the DC bus. The simulator's
  bus is a fixed supply, so the over-voltage path (`high_dc_volt`) is untested.
- **Drive power gone:** if the F4 crashes or loses power, the F3 switches off
  after its own timeout and the motor coasts. Braking in that case would need
  F3 firmware (a zero vector on timeout) or an external contactor that shorts
  the phases through resistors.
- **HAL capacity:** a PMSM config over CAN now uses 28 of the 32 HAL
  component instances (`HAL_MAX_COMPS`).
