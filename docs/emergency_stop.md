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
| HV board fault (overcurrent, F3 fault pin, ...) | coast | The power stage can't be trusted: outputs off at once |
| The F4 stops (LV side lost, F4 crash or reset while enabled) | **dynamic brake on the F3** (PMSM) | The HV side runs from the DC bus and brakes on its own: 6 mm |

The distances are from the simulator (burn-in motor ECMA-C30604FS, 188 rad/s).
They are measured from the moment of the failure, so they include detecting
it. The tests are `host/tests/estop_*.test`.

**Before relying on this on a real machine:**
- This is firmware. A safety-rated stop (STO, a contactor, mechanical end
  stops) is still needed. Losing the F4 or the LV supply is covered by the F3
  (below), but with no DC bus, or the F3 itself failing, nothing here runs.
- None of it has run on hardware yet. Try it at low speed first.

## Contents

- [Configuration](#configuration)
- [Quick stop (`qstop`)](#quick-stop-qstop)
- [Dynamic brake (`dbrake`)](#dynamic-brake-dbrake)
- [Sequencing (`fault`)](#sequencing-fault)
- [Protecting the power stage](#protecting-the-power-stage)
- [Braking on the HV side when the F4 is lost](#braking-on-the-hv-side-when-the-f4-is-lost)
- [Hardware: a brake resistor on the supply card](#hardware-a-brake-resistor-on-the-supply-card)
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
| `dbrake0.max_cur` | the motor's current limit [A]; the brake holds 90 % of it (`cur_share`) | `conf0.max_ac_cur` |
| `conf0.max_hv_power` | the HV board's rating [W]: the most a quick stop feeds back into the DC bus | 1000 |
| `hv0.brake_on_loss` | 1: the F3 brakes on its own if the F4 stops | 1 (PMSM) |

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

## Protecting the power stage

**Current.** Both stops keep the phase current within limits:
- **Quick stop:** pid's torque limit (`max_force`) and the F3's current
  clamp at `max_cur` bound it. The tests check it stays under `max_ac_cur`.
- **Dynamic brake:** it holds 90 % of `max_ac_cur`. The 90 % keeps it below
  the F3's own voltage-mode clamp. That clamp scales the commanded voltage
  down above `max_cur`, which for a resistor means less resistance and *more*
  current, so the two would fight.
- **The F3's brake:** it also stays below the stage's `ABS_MAX_CURRENT`
  (30 A).
- **Hardware backstop:** the comparators that cut the PWM on overcurrent
  remain behind all of this.

**Power.** `conf0.max_hv_power` is the board's rating (set it per board,
500 W to 2 kW).
- **Quick stop:** it limits the regenerated power, J·|a|·|ω|, so the stop
  decelerates more gently at high speed (`qstop0.power` shows it).
  `estop_power.test`: at 30 W, 7 mm instead of 3.
- **Dynamic brake:** it does not limit it, and can't. A short (rv = 0) puts
  nothing into the bus: the energy goes into the motor's windings. rv only
  rises to hold the current at its limit, and then the bus gets 1.5·rv·i²
  (`dbrake0.power`). Capping that as well would mean letting the current
  rise, and overcurrent destroys the stage at once. The bus has its own
  protection: rv backs off above `high_dc_volt`, the drive faults at
  `max_dc_volt`, and the F3 at 410 V. A brake resistor on the supply
  (below) is what makes room for that energy.

## Braking on the HV side when the F4 is lost

The HV board powers itself from the DC bus: `hv_supply.sch` has an LNK304
off-line regulator from VPP to +15 V, then an ACT4088 to 3.3 V for the F3.
So if the F4 crashes, resets, or loses its 24 V, the F3 and the gate drivers
keep running as long as the bus does.

1. **The F4 asks for it.** It sets `brake_on_loss` in every packet
   (`hv0.brake_on_loss`, a spare bit of the flag word). An older F3 ignores
   the bit, and an older F4 sends 0.
2. **The packets stop.** If the last packet enabled the drive, the F3
   (`stm32f303/src/comps/ls.c`) runs the dynamic brake itself, on its
   timeout (6 rt cycles), instead of switching off.
3. **How it brakes.** It uses the same law as the F4's `dbrake`
   (`shared/vbrake.h`), on its own current measurement and in its own frozen
   frame. The current target is 90 % of `max_cur`, capped by the stage's
   `ABS_MAX_CURRENT`.
4. **When it ends.** Once the current has stayed under 5 % for 0.1 s (or after
   2 s at most), the F3 switches off. If the F4 comes back, its packets take
   over again.

`estop_f4dead.test` (`hv0.sim_f4_dead`): 6 mm. `estop_f4dead_coast.test`
(without the flag): coasts 25 mm.

**Limits:**
- The F3 doesn't know `max_hv_power`. It has no power rating, so its brake
  is bounded by current only, like `dbrake`.
- The LNK304 has a minimum input voltage. At low bus voltages (a 48 V
  system) the HV side may drop out before the motor has stopped. Check the
  datasheet against your bus.
- **This changes the F3 firmware too:** flash it with `make f3_btburn` (after
  the F4). An F3 without it still coasts on loss, as before.

## Hardware: a brake resistor on the supply card

A shared brake resistor on the supply card for the stmbl pack is a good idea.
Every drive dumps its braking energy into the common bus, and one resistor
takes it. **Don't switch it with a relay, though:**

- **Too slow.** A relay takes 5–15 ms to close. The bus capacitance is
  small: `pe0.cap` is 540 µF, and a 1 kW stop raises a 320 V bus by
  1000 / (540 µ × 320) ≈ 5.8 V per ms. That's 30–90 V before the relay
  contacts close, enough to reach the drive's 380 V fault, or the F3's
  410 V, during one hard stop.
- **DC arcs.** At 300–400 V DC, a relay that opens under current draws an
  arc. Most relays are rated to break only a fraction of an amp at that
  voltage. The contacts burn or weld, and a welded relay keeps the resistor
  on the bus.
- **Wear.** It would switch on every hard stop.

**What works instead: a brake chopper**, entirely in hardware, so no firmware
or LV supply is needed.
- **Parts:** a 600 V MOSFET or IGBT in series with the resistor across the
  bus, driven by a comparator with hysteresis on the bus voltage.
- **Example thresholds:** on at about 375 V, off at about 365 V.
  - Set them above the highest rectified mains (230 V +10 % is about 360 V)
    and below `max_dc_volt`.
  - Then raise the drives' `high_dc_volt` above the chopper's "on"
    threshold, so the drives don't throttle braking current first.
  - Check the values against your capacitors' rating.
- **Resistor:**
  - At 375 V, 100 Ω takes 1.4 kW while on.
  - Size its average rating from the energy per stop, ½Jω² plus the load,
    times how often the machine stops.
  - Give it a thermal switch, and a fuse in the chopper path.
- **Monitoring:** the F3 already has a `BRK` output with detection of a brake
  circuit (`io0.brk`, `brk_present`, not used by any template yet). It could
  report or test the chopper. The protection itself should not depend on it.

**Where a relay is right:** in places where it never has to break current.
- **A bleed relay:** a normally closed relay that ties the bus to a bleed
  resistor while the supply is off, so a stopped machine doesn't keep a
  charged bus. It opens when the supply comes on, before the bus charges.
- **A phase-shorting contactor:** a normally closed contactor that shorts the
  motor phases through resistors. It covers the case where nothing on the
  drive can act, with no bus and no F3. The drive must open it before it
  enables.

## Testing in the simulator

New simulator pins:
- `enc_fb0.sim_fail = 1`: the encoder fails (position frozen, error set).
- `hv0.sim_fault = <code>`: an HV board fault (the bridge goes off).
- `hv0.sim_dist`: the unwrapped position, for measuring how far a stop went.
- `hv0.sim_f4_dead = 1`: the F4 stops talking. The simulated F3 keeps the
  last packet's frame and brakes as `ls.c` does (`hv0.sim_f3_brake`). The
  simulated F3 now also models curpid's voltage-mode current clamp.
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
| `power` | the quick stop's power limit, and the current within `max_ac_cur` |
| `f4dead`, `f4dead_coast` | the F3 braking on its own when the F4 is lost, and coasting without the flag |

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
- **F3 untested on hardware:** the F3 brake has only run against the
  simulated F3. The real F3 runs it in `ls.c` at its own rt rate.
- **HAL capacity:** a PMSM config over CAN now uses 28 of the 32 HAL
  component instances (`HAL_MAX_COMPS`).
