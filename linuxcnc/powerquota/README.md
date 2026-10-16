# powerquota: slow the machine before a fuse blows

The supply of a machine with several stmbl drives is usually smaller than
the drives together: six 2 kW drives behind a 3 kW supply, two drives per
phase on a three-phase supply. `powerquota` is a LinuxCNC realtime module
(uspace). It sums the drives' input power into quotas, one per fuse, and
follows each fuse's heating against its trip profile. When a fuse gets
warm, it slows the motion through `motion.adaptive-feed`. If even the
lowest feed cannot save the fuse (a load that does not fall with speed,
such as holding against gravity), it raises an alarm for the estop chain
first.

The drives measure their own input power: `pe0.p_in` in the drive is the
motor's electrical input 3/2 (ud id + uq iq), over the inverter's
efficiency, plus its idle draw. It goes to LinuxCNC as the smart-serial
process value `power`: hm2 pin `hm2_<board>.0.stbl.<port>.<chan>.power`,
or `stmbl-sim.0.power` with the simulated drive.

```
loadrt powerquota config=/path/quota.txt
addf powerquota.0 servo-thread
net d0-power hm2_7i96s.0.stbl.0.0.power => powerquota.0.power-0
net d1-power hm2_7i96s.0.stbl.0.1.power => powerquota.0.power-1
...
net feed-scale powerquota.0.feed-scale => motion.adaptive-feed
# the estop chain: ... => powerquota.0.chain-in, powerquota.0.chain-out => ...
net estop-reset iocontrol.0.user-request-enable => powerquota.0.reset
```

Adaptive feed must be on, either in the program (`M52 P1`) or in the ini:
`[RS274NGC] RS274NGC_STARTUP_CODE = M52 P1`. It slows coordinated moves
along their path. Jogs and homing do not follow it, so the alarm is their
only protection.

## The config file

```
# three-phase 400 V supply, C 16 A per phase; six single-phase drives,
# two per phase; a spindle drive on all three phases
quota L1 C 16 230 0.65
quota L2 C 16 230 0.65
quota L3 C 16 230 0.65
load L1 0 1
load L1 1 1
load L2 2 1
load L2 3 1
load L3 4 1
load L3 5 1
load L1 6 0.333
load L2 6 0.333
load L3 6 0.333
```

- `quota NAME PROFILE IN VOLT [PF]`: a fuse. IN is its rated current [A];
  VOLT is the voltage its load sees (phase-neutral for one phase); PF is the
  power factor of what it feeds. A rectifier with a bus capacitor draws
  peaky current, so its PF is 0.6–0.9: the fuse's current is
  P / (VOLT * PF).
- `load QUOTA DRIVE SHARE`: drive DRIVE (its `power-N` pin) puts SHARE of
  its power through this fuse. Use 1 for a single-phase drive on that
  phase and 1/3 on each phase for a three-phase drive. One fuse for
  everything works too.
- Fuse profiles: `B`, `C` and `D` breakers (IEC 60898-1: one thermal curve,
  magnetic at 3 / 5 / 10 In) and `gG` fuses (IEC 60269) are built in, at
  the fast edge of the standard bands. Your own:
  - `profile NAME M1 T1 M2 T2 ...` is the thermal curve: at M x In it
    trips after T s (log-log between the points; never below M1).
  - `magnetic NAME M` is the instant trip at M x In.
  - `cool NAME TAU` is the cooling time constant [s].

  Take the points from the fuse's data sheet (the fast edge of its band).
- `regen R`: the share of regenerated (negative) power that offsets the
  others. Use 0 for separate rectifiers (default; braking resistors burn
  it) and 1 for a common DC bus.
- `start`, `margin`, `trip` (0.2 / 0.5 / 0.9): the fuse's damage where
  slowing starts, where the feed reaches `fmin` (0.05), and where the
  alarm goes. `magmargin` (0.7) keeps the current under 0.7 x the magnetic
  trip. `rates TDOWN TUP` (0.02 / 1 s) are how fast the feed falls and
  recovers.

The model: a fuse's heating is followed as damage, d' = 1 / t(I / In)
while over the curve's first point (1 = blown), decaying with the cooling
time constant below it.

## Pins

- `powerquota.0.power-N` (in): drive N's input power [W].
- `.feed-scale`, `.limiting`, `.total-power`.
- `.alarm`, `.alarm-quota`: latched; an estop reset (`.reset`) clears it
  once the fuse model has cooled below `trip`.
- `.chain-in` / `.chain-out`: the estop chain.
- Per quota `NAME`: `.power`, `.current`, `.load` (I / In), `.damage` and
  `.scale` (the feed it asks for).

## Limits

- **It reacts, it does not predict.** A single peak big enough to blow the
  fuse before the feed can follow (a breaker's magnetic trip on an
  acceleration peak) is not caught: size the fuse for the peaks, or lower
  the accelerations.
- **The power is the drives' estimate**, not a measured mains current:
  their efficiency and idle draw are configured (`pe0.eff`, `pe0.idle`).
  The power factor turns it into current.

## Tests

`make -C ../../host check` runs `test_quota`, the model's unit tests:
- the profiles' trip times and cooling;
- six 2 kW drives on a 3 kW C 16 A supply: kept under the margin, feed
  about 0.75;
- two drives per phase;
- a three-phase drive's thirds;
- a static overload raising the alarm before the fuse;
- regeneration;
- the magnetic margin;
- the config.

`make -C ../../hwtest/burnin-sim test-quota` runs LinuxCNC with the
simulated drive and a quota below what its moves draw. With M52 P1 the
feed falls to 0.55 and the fuse stays at damage 0.34. Without it, the
alarm stops the machine at 0.9.
