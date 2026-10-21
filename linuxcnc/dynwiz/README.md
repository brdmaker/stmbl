# dynwiz: measure a gantry machine's dynamics through LinuxCNC

A console wizard, in the spirit of `servoterm-cli`. It moves the machine
through LinuxCNC, records every servo cycle, and works out:

- the moving masses and friction (`conf0.j` for the drives);
- the head's vibration modes along the bridge (pcomp's model);
- the servo's lag on a circle;
- cogging;
- loops at their stability limit.

It then checks what pcomp gains and writes `[PCOMP]` and the drive
settings. Every step runs on its own and adds to a session file
(`dynwiz.json`), so a GUI can sit on the same steps later.

It is plain Python with no numpy. It runs in LinuxCNC's own Python on a
headless controller, needing only the `linuxcnc` and `hal` modules and
`halcmd`. For development it can drive the simulated machines directly
(`-b sim`, no LinuxCNC).

The theory, and what pcomp does with the results, is in
[docs/motion_control.md](../../docs/motion_control.md).

## Quick start

```sh
# the simulated portal, no LinuxCNC (about 7x real time)
./dynwiz -b sim --portal ../../host/portal/machine/portal.portal --acc 5000 --vmax 1600 run

# a retrofit machine (machine/retrofit/README.md); -y: don't ask
./dynwiz -b sim --portal ../../host/portal/machine/retrofit/cmm.portal -y run

# LinuxCNC running (any GUI, or none), the machine on and homed
./dynwiz --ini ~/linuxcnc/configs/mymachine/mymachine.ini run
./dynwiz modes          # one step; INI_FILE_NAME is set inside LinuxCNC
./dynwiz report         # what the session holds

# LinuxCNC on the simulated portal, headless, the whole wizard (~3 min)
make -C ../../hwtest/portal-sim dynwiz        # as a non-root user
```

## Usage

```
dynwiz [-b lcnc|sim] [--ini FILE] [--portal FILE] [-s SESSION] [-y] [-v] STEP [options]
```

| Step | Moves | Finds |
|---|---|---|
| `check` | nothing | drive configs: `vel_int0.mode`, `vel1.bc`, `conf0.j` against the measured mass, `vel_bw`, an oscillation found by `mass`/`cogging` |
| `mass` | each axis back and forth about the middle (up to 150 mm, 30–60 % of max speed) | moving mass, Coulomb and viscous friction and a constant push per motor; whether a loop oscillates |
| `modes` | steps of 1 and 0.37 mm in Y at five X positions, and in X at the middle | the head's modes along the bridge, fitted with pcomp's beam model; modes of the base; the bridge's twist |
| `circle` | three circles of R 20 about the middle at half the acceleration limit | the servo's steady lag `tau2` |
| `cogging` | slow passes (20 mm/s, 300 mm) of each axis, each way | force ripple: amplitude, period, phase; a loop oscillating at slow speed |
| `verify` | 12 point-to-point moves at full acceleration, pcomp off then on | settling time and residual ring |
| `write` | nothing | `[PCOMP]` into the INI (with a backup), drive configs with the suggested changes as `CONF.dynwiz` files |
| `report` | nothing | the session |
| `run` | all of the above in turn, asking before each | |

Options:

| Option | Default | Meaning |
|---|---|---|
| `-b lcnc` / `-b sim` | lcnc | a running LinuxCNC (`--ini`), or `host/portal/portal_sim` on `--portal FILE` |
| `-s FILE` | dynwiz.json | the session; a session from another machine starts afresh |
| `-y` | | don't ask before moving |
| `-v` | | `modes`: every position's spectral peaks, from the scale and from the current |
| `--step MM` | 1.0 | `modes` step size (and 0.37 of it, so one of the two doesn't null a mode) |
| `--fmin`, `--fmax` | 14, 250 Hz | `modes`: the band searched |
| `--min-snr` | 6 | `modes`: how far a peak must stand out of the spectrum |
| `--use-tool` | | `modes`: identify from a sensor on the head (`[DYNWIZ] TOOL_X/TOOL_Y`) |
| `--radius` | 20 mm | `circle` |
| `--cog-speed`, `--cog-span`, `--cog-min` | 20 mm/s, 300 mm, 2 N | `cogging` |
| `--square`, `--tol` | 100 mm, 0.01 mm | `verify`: move length, settling tolerance |
| `--force` | | `write`: also a model that `modes` doubted |
| `--conf-out DIR` | the session's | `write`: where `CONF.dynwiz` files go |
| `--acc`, `--vmax`, `--sim-line LINE` | | `sim`: the axes' limits (else the portal file's `#@dynwiz ACC_X ACC_Y VMAX` line); a `portal_sim` line before the drives are enabled, e.g. `--sim-line 'drv y1 conf0.vel_bw = 300'` to try a setting before you flash it |

## Setting up a machine (`[DYNWIZ]`)

The wizard reads the axes' limits and travel from the INI (`[AXIS_X]`,
`[AXIS_Y]`: `MIN/MAX_LIMIT`, `MAX_ACCELERATION`, `MAX_VELOCITY`). It moves
only inside the soft limits, less 5 mm. Everything else is in `[DYNWIZ]`,
as in `hwtest/portal-sim/portal.ini`:

```ini
[DYNWIZ]
SAMPLER = 1                          # the sampler instance it may use (see below)
JOINT_X = 0
JOINT_Y1 = 1
JOINT_Y2 = 2                         # empty: one Y motor (an idler rail)
CUR_X = hm2_7i96.0.stbl.0.current    # the drives' current [A] (mass, cogging, modes)
CUR_Y1 = hm2_7i96.0.stbl.1.current
CUR_Y2 = hm2_7i96.0.stbl.2.current
FB_Y2 = ...                          # the scales, if not joint.N.motor-pos-fb (behind pcomp:
                                     # the drives' own pos-fb; an idler rail's scale)
CONF_X = x.conf                      # servoterm-cli --dump of each drive (check, write, Kf)
PERIOD_X = 60.96                     # its travel per revolution [mm] (a linear motor's period)
KF_X = 82                            # or the force constant directly [N/A]
TOOL_X = ...                         # a sensor on the head, if there is one
TOOL_Y = ...                         #   (an accelerometer will do for modes)
PCOMP = pcomp.0                      # pcomp's HAL prefix, if the config loads it
RAILS = 1.2                          # rail spacing [m] and the bridge's middle [mm]
X_CENTER = 500                       #   (else [PCOMP]'s, else the middle of X)
```

**The recording.** A HAL `sampler` with 11 float pins, recorded every
servo cycle with `halsampler`. The wizard links its pins itself at the
start, joining the signals the source pins are already on.
- If the config doesn't load `sampler`, the wizard loads it.
- If it does, give the wizard an instance of its own: add a depth and
  `,fffffffffff` to the `loadrt`, `addf sampler.N servo-thread`, and set
  `SAMPLER = N`:

```
loadrt sampler depth=20000,20000 cfg=ffffffffffffffffffffs,fffffffffff
addf sampler.1 servo-thread
```

Every instance needs its own depth: with `depth=20000` alone the second
instance is not created (DEBUGNOTES).

**pcomp.** If it is loaded, the wizard switches it off for every step but
`verify`, and puts its switches back as they were when it ends.

## Safety

- Before each moving step it says what it will do and asks: `[Enter]` go,
  `s` skip, `q` quit. `-y` doesn't ask; on LinuxCNC, `run` asks once at
  the start even so.
- The moves are G-code programs in AUTO mode, within the INI's limits and
  LinuxCNC's own checks (following error, soft limits, E-stop). Ctrl-C
  aborts the program. Any LinuxCNC error stops the wizard with its text.
- The machine must be on, homed and idle. The wizard uses machine
  coordinates and turns them into program coordinates with the active G5x
  and G92 offsets. It refuses a rotated coordinate system.
- `write` doesn't touch the drives. It writes `CONF.dynwiz` files: load one
  with `servoterm-cli --load FILE` (which backs up the old config), then
  reset that drive. Back up and check as CLAUDE.md says.

## Reading the results

**`mass`** fits `i = k_a a + k_c sign(v) + k_v v + i0` as impulse: both
sides summed over each move. A head ringing on its springs then only adds
its momentum, which stays small. Fitted sample by sample, the ring had
biased the mass by up to 18 % through its phase in the short accelerations
(DEBUGNOTES). On the simulated machines it finds every mass within 3 %.
The suggested `conf0.j` is that mass times (period/2π)².

**Oscillation.** `mass` and `cogging` look for a clear, sustained line in
the current while moving (a ring after an acceleration dies away, a limit
cycle doesn't):

    -> the current oscillates, 5.77 A at 163 Hz: the loop is at its limit.

On a gantry it is usually the bridge's yaw. In the yaw, Y1 against Y2,
each motor moves only `2 J / L²` of the bridge, a quarter or less of the
mass it is tuned for. Its velocity loop is that much faster there and runs
into the current loop: raise `conf0.cur_bw` (2000), or lower
`conf0.vel_bw`. `write` proposes `vel_bw` × 0.6.

**`modes`**, one line per X position:

```
    X [mm]   head y (from the scales or the current)       twist y1 - y2 [um/mm]  tool (simulation)
    104.0     33.6 Hz  zeta 0.051  snr   57 scale        17.87 @  33.5 Hz           33.9 Hz  zeta 0.040  snr 68145
```

- The ring-down 40–380 ms after each step, from the scales' error or from
  the motors' current, whichever is clearer.
- The twist column is the ring in y1 − y2 per mm of step: the head swinging
  off the bridge's middle twists it.
- `tool` appears only where there is a head sensor (the simulation's true
  tool point): what the tool really did.

The model fits `1/f² = 1/f_end² + (1/f_mid² − 1/f_end²)(1 − u²)` through
the positions. `FX` is the X step's ring at the middle, as the servo-held
carriage sees it.

- **The base.** A mode at the same frequency, within 3 %, at every position
  (and in X) belongs to the base, such as the frame on its feet. It is
  reported and left out:

      the base: 17.3 Hz at every position (the frame on its feet?)

  pcomp doesn't shape it. Level the machine, or stiffen its feet.
- **A doubtful model.** If the positions don't fit a bending bridge, the
  model is marked doubtful and `write` leaves it out. Look at `-v`: often
  the servo's own settling (pos_bw 100 rad/s is 16 Hz) or the frame sits
  above `--fmin`.
- **A light, stiff head** doesn't move the carriage enough for the scales
  or the current to show it (the `dispenser` retrofit, 85–140 Hz). Then use
  a sensor on the head: `--use-tool` with `TOOL_X/Y`. An accelerometer's
  signal is fine; only the frequency and damping count.

**`circle`.** `tau2 = (r_cmd − r_scales) / a`. With `vel_int0.mode 1` and
`vel1.bc 1` it is about 0 (`SERVO 0`). With the stock drive settings it is
1–2e-5 s², and pcomp's servo term feeds it forward.

**`cogging`** reports the ripple's amplitude [N], its period [mm] and its
phase [deg] at LinuxCNC's position. For `pmsm_ttc` it gives
`nc = period_motor / period_cogging` and `ac`. `pc` depends on the drive's
own position at a known LinuxCNC position, so read that from the drive (or
find it by trying).

**`verify`** reports the settling time to `--tol` and the residual ring
80 ms after each move. They are judged by the head sensor if there is one,
else by the scales, which don't see the head.

## The simulated machines

`host/portal/machine/portal.portal` and four retrofit machines in
`host/portal/machine/retrofit` (their README):
- a 3 × 1.5 m laser with iron-core motors;
- a dispenser with one Y motor;
- a CMM on air bearings;
- a PCB driller at 2 g.

What dynwiz finds on each, against what their files say:

| machine | mass Y / X [kg] (true) | FX / FY_END / FY_MID [Hz] (true) | notes |
|---|---|---|---|
| portal | 42.4 / 12.1 (42 / 12) | 44 / 54 / 27.8 (45 / 55 / 28) | ring 39 → 21 um with pcomp |
| laser3015 | 144.7 / 35.5 (146 / 36) | 36 / 41 / 18.9 (38 / 42 / 19) | cogging 28 / 26 / 18 N at 22.86 mm found; ring 180 → 68 um |
| cmm | 64.6 / 18.1 (63 / 18) | 21 / 24–26 / 16.6 (22 / 26 / 17), zeta 0.011 (0.012) | settling 600 → 68 ms, ring 252 → 5 um |
| pcbdrill | 43.2 / 15.1 (43 / 15) | 71 / 85 / 51.6 (70 / 85 / 52) | the frame's 17 Hz recognised and left out |
| dispenser | 9.2 / 3.0 (9.5 / 3) | doubtful from the scales; with the head sensor 103 / 141 / 86 (110 / 140 / 85) | its ring after moves is the frame on the bench, not the head: pcomp doesn't help |

Each Y config needed one change before it ran cleanly, and the wizard
detects each problem when it is put back:
- laser3015 and pcbdrill: `cur_bw` 2000;
- cmm: `vel_bw` 300;
- dispenser: `vel_bw` 200.

## Testing

```sh
make test       # the math on synthetic signals (~5 s)
make test-sim   # the wizard on all the simulated machines against their files (~1 min)
make -C ../../hwtest/portal-sim dynwiz   # through LinuxCNC (non-root, ~3 min)
```

`tests/test_machines.py` runs the whole command line per machine, in
parallel. It checks:
- masses within 5 %, and the modes within 8–12 % of the files;
- the scales' frequency against the tool's at every position;
- cogging and the frame mode;
- the untuned variants' oscillations (`--sim-line` puts them back);
- the INI written with a backup, and a doubtful model not written.
