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
- Tested on the bench drive (ECMA-C30604FS, burn-in axis) at 50, 150 and
  300 mm/s for every cause but an HV fault: [On hardware](#on-hardware).
  On another machine, try it at low speed first.

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
  exists for `id_pmsm`).
- It commands a virtual resistor, u = −rv · i.
- It freezes the frame (`pos` held, `vel` = 0, so the F3 doesn't turn it
  between packets).
- It sets the packet flag `brake` (`hv0.brake`). An F3 with this branch's
  firmware then skips curpid's voltage-mode current clamp. That clamp scales
  the voltage down above `max_cur`, which for a resistor means less resistance
  and more current, down to a plain short. An older F3 ignores the flag and
  clamps.

**Before the fault is declared:** the fault filter takes 1 ms before a
feedback error becomes a fault. During that time commutation runs on a
frozen or wrong angle, and even a zero current command can't be held in that
frame at speed. So `dbrake` switches the bridge off (`en_out` → `hv0.en`)
while the raw error (`fb_err0` / `fb_err1`) is set. A glitch that clears just
resumes; a real fault starts the brake.

A resistor in the dq frame is a resistor in any frame, so this needs **no
rotor angle**. That is what makes it usable after a feedback fault.

With rv = 0 it is a short circuit, and the energy goes into the windings. At
speed a short is held back by the motor's inductance (ωL ≫ R): the current
stays near ψ/L, and it brakes weakly.

rv holds the current at 90 % of `max_cur`. A resistor brakes harder at speed
than a short, but that energy goes into the DC bus. Above `high_dc_volt`, rv
falls back towards the short. The gains are relative to rn = `pwm_volt` /
`max_cur`, so they carry over between motors:

```
rv = rn * (kp * e + ki * integral(e)),   e = (i - target) / target
kp = 0.1, ki = 500/s, rv limited to min(2 * rn, l / delay)
```

- **It starts resistive** (rv = rn, about the target current at full back-EMF)
  and relaxes towards the short. Starting as a short gave a transient of up
  to twice the short-circuit current before rv could rise: 9–15 A on 5–11 A
  motors.
- **Stability:** the voltage follows the current a delay later (2 rt periods
  on the F4, 1.5 on the F3). When the winding's L / R is not much longer than
  that delay, an rv much above l / delay oscillates: the voltage flips sign
  every sample. On the Yaskawa r2aa06020f (0.7 mH) this went to 15 A against
  5.6 A. So rv stays below l / delay (`dbrake0.l = conf0.l`).
- **Giving up:** if the current then can't be held, i.e. it stays above
  1.3 × the target for 2 ms, the brake gives up (`dbrake0.gave_up`,
  `hv0.sim_f3_gave_up` in the simulator). The outputs go off and the motor
  coasts. That is safe for the power stage: losing a stop beats losing the
  stage.

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
- **Dynamic brake:** it holds 90 % of `max_ac_cur`. It starts resistive, so
  there's no short-circuit transient. It switches off the F3's voltage-mode
  clamp while braking (the `brake` flag). If the current can't be held
  (above 1.3 × the target for 2 ms) it gives up, and the motor coasts.
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
  protection: rv backs off towards the short above `high_dc_volt` (but not
  past the current target), the drive faults at `max_dc_volt`, and the F3
  at 410 V. A brake resistor on the supply (below) is what makes room for
  that energy.

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

## On hardware

`hwtest/suite/run_hwtest.sh hw --phase-only ebrake` (the axis moves; see
`hwtest/suite/README.md`) runs each cause at each speed (`EBRAKE_SPEEDS`,
default 50,150,300 mm/s; `EBRAKE_KINDS` off,link,fb,f3) during a G1 move
under LinuxCNC. The drive's scope records the encoder's own angle
(`enc_fb0.pos`, which keeps counting whatever the drive's state), the q
current, the DC bus and `fault0.stop` around the stop; then the drive must
come back into service (machine on, home, a move, no crash record).

| Cause | How it is made | 50 mm/s | 150 mm/s | 300 mm/s (188 rad/s) |
|---|---|---|---|---|
| Enable dropped (machine off) | LinuxCNC machine off | 0.06 mm, 16 ms | 0.37 mm, 44 ms | 1.46 mm, 52 ms, 3.1 A |
| Link lost | `hm2_7c81.0.sserial.port-0.run = 0` (no more packets, as a pulled cable) | 0.02 mm, 14 ms | 0.38 mm, 44 ms | 1.49 mm, 53 ms, 3.0 A |
| Feedback fault | `fault0.mot_fb_error = 1` (dynamic brake) | 0.08 mm, 11 ms | 0.23 mm, 16 ms | 0.55 mm, 17 ms, 2.6 A |
| The F4 gone | `hv0.drop_link = 1.5`: the F4 sends the F3 nothing for 1.5 s and keeps reading the encoder | 0.10 mm, 11 ms | 0.27 mm, 16 ms | 0.75 mm, 18 ms |
| (coasting, friction only) | | ~0.9 mm, 36 ms | ~8 mm | ~30 mm, ~220 ms |

Bench drive, v0.9.18-348, 200 V bus (peak 202 V), max_ac_cur 10 A,
2026-10-20. The F3's own current isn't reported while its link is
dropped. Not run on hardware: an HV fault (coasts by design).

- `hv0.drop_link` is a test pin: it counts down, so the link always comes
  back; the F4 then sends "disabled" and the F3 stops braking.
- Stopping LinuxCNC's threads (`halcmd stop`) instead of the port makes the
  card's watchdog bite and hostmot2 stop the port ("too many errors"); the
  test did not get the machine back on after that (clearing
  `watchdog.has-bit` and toggling the port's `run` was not enough).

## Testing in the simulator

### The `ebrake` suite

`make -C host ebrake` runs:
- **The hand-written tests** in `host/tests/ebrake/` (22 tests). They are also
  part of `make check`.
- **Every motor in `tests/motors.py`** (35 motors), generated by
  `tests/gen_ebrake_tests.py` into `gen/ebrake/`. Each motor goes through the
  three failures: link lost (quick stop), encoder fails (dynamic brake), and
  F4 lost (the F3 brakes). That's 105 tests, all passing, about 6 s.

**What every generated test checks:**
- **Current:** from the failure on, within the power stage (0.95 × the F3's
  30 A) and from 5 ms on within 1.3 × `max_ac_cur`.
- **End state:** the motor stands still, the outputs are off (or the F3's
  brake is over), and no stop was cut off by `stop_time`.
- **Distance:** shorter than coasting would be (counting the time until the
  failure is noticed). At most 1 rad of reverse travel: the decaying winding
  current pushes past standstill, which on light motors can outweigh the
  short way forward. For the quick stop, also within 1.5 × the planned
  S-curve + 0.5 rad.
- **Give-up:** the expected outcome per motor. Only manutec_r15_1 with the
  encoder fault gives up: its short draws ~39 A against a 4.5 A target, which
  needs ~11 Ω, and the F4 can only use up to 5 Ω stably. With the F4 lost,
  the F3's faster loop holds it.

What the suite found and fixed: the F3 clamp turning the brake into a short,
the instability on low-L motors, the start transient, and the bridge driving
on a frozen angle before the fault filter trips.

### The DC bus

The simulated bus is 230 V 50 Hz mains, rectified by a single-phase diode
bridge straight into the drive's own 540 µF (no capacitors on the supply),
with a brake chopper standing in for the supply card (47 Ω from 345 V,
off below 335 V; `host/README.md`, "DC bus"). So regeneration is now
tested: the energy a stop sends back charges the capacitors, and only the
chopper, the windings and the motor's losses take it away.

`host/tests/bus/` (part of `make check`):

| Test | What it checks |
|---|---|
| `ripple` | 323 V at rest; ~17 V of 100 Hz ripple at ~300 W, the speed holding |
| `mains_loss` | the mains gone at speed: the caps carry the axis ~40 ms, then a following error and the dynamic brake |
| `regen_chopper` | the 829 J spindle's quick stop: the chopper takes it, the bus stays below `high_dc_volt` |
| `regen_nochop` | the same without a chopper: `max_dc_volt` trips, the drive coasts, and the bus has overshot the F3's 410 V |
| `ov_f3` | the F3's own cut-off at 410 V (`hv0.sim_hv_fault`) with the F4's limits out of the way |
| `mains_high` | 253 V mains (+10 %): the simulator's 345 V chopper clips every peak (~460 W); the recommended 375 / 365 V with `high_dc_volt` 385 idles |

What it showed:
- **A heavy motor needs the chopper.** 540 µF hold about 20 J between
  323 V and 410 V; the spindle has 829 J at speed. Without a chopper the
  bus climbs ~40 V/ms while braking, so by the time `max_dc_volt` (380 V)
  has tripped and the bridge is off it is at ~427 V, past the F3's 410 V.
  The burn-in motor (0.5 J at speed, 2 Ω windings) never gets near.
- **The dynamic brake gave up the current for the bus.** With the bus above
  `high_dc_volt` rv went to a plain short whatever the current; on the
  spindle that drew 64 A against a 52 A limit. Now it backs off only while
  the current is below its target; if the bus still rises, the F3 cuts off
  at 410 V and the motor coasts (`estop_bus_high`).
- **Thresholds against the mains tolerance.** At 230 V +10 % the rectified
  peak is 358 V: a chopper below that burns power on every peak, and a
  `high_dc_volt` below it throttles the drive at rest. The card's chopper
  at 375 / 365 V needs the drives' `high_dc_volt` / `max_dc_volt` raised to
  about 385 / 400 V (`bus_mains_high`); the simulator keeps 345 V so the
  template's 350 / 380 V work.

### Simulator pins

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

The hand-written `estop_*` tests (`host/tests/ebrake/`):

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
| `reverse`, `standstill`, `reenable` | the quick stop moving backwards, at standstill, and re-enabling after it (enable is ignored while a stop runs) |
| `holding_brake` | the holding brake stays released while braking, grips at standstill, and then holds a hanging load |
| `bus_high`, `giveup` | rv backs off towards a short when the bus is high, but not past the current target; the brake gives up when the current can't be held |
| `f4dead_standstill`, `f4_returns` | the F3 brake at standstill; the F4 coming back after it |
| `hv_during` | an HV fault during a quick stop ends it at once |
| `stepper` | the quick stop on the brdstepper NEMA11 (stepper template) |

LinuxCNC's burn-in tests (`hwtest/burnin-sim`: can, console, quota) pass
unchanged.

## Limits and open points

- **Ringing:** the velocity observer's lag limits how hard a quick stop can
  brake without ringing (DEBUGNOTES).
- **Old F3 firmware:** without this branch's F3 firmware, the F3 keeps its
  voltage-mode clamp during the F4's brake. Motors whose short-circuit current
  is far above `max_ac_cur` then brake as a plain short. Flash the F3 too.
- **Regeneration:** the simulator's bus is the rectified mains into the
  drive's own capacitors, so over-voltage is tested (above). Not modelled:
  the bridge's freewheel diodes charging the bus from back-EMF above it
  while the bridge is off (a motor driven above its voltage-limited speed).
- **F3 on hardware:** tested on the bench with `hv0.drop_link` (above);
  before that the F3 brake had only run against the
  simulated F3. The simulated F3 loop runs at 20 kHz, the real F3's rt at
  15 kHz. The F3 code uses its own period for the delay, so its rv limit
  will be a little lower than in the simulator.
- **HAL capacity:** a PMSM config over CAN now uses 28 of the 32 HAL
  component instances (`HAL_MAX_COMPS`).
