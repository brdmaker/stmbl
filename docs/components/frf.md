# frf

*F4 and F3 (shared), `shared/comps/frf.c`*

## Brief
`frf` measures the frequency response of the mechanics with the position loop running (stepped sine) and fits a two-mass model to it, mainly to find the coupling anti-resonance for `conf0.j_lpf`. It is loaded by `conf/template/id_frf.txt`, which replaces `id_lpf`'s ring down and is run before `id_pid` (`j_lpf` changes the plant the gains are tuned against). It sits between `pid0` and the torque controller (after `biquad0` if that is loaded):
- `frf0.in = pid0.torque_cmd`, `pmsm_ttc0.torque = frf0.out`
- `frf0.pos = fb_switch0.vel_fb`
- `vel1.torque = frf0.out` and `vel2.torque = frf0.out`: the observers must see the injection
- `frf0.rt_prio = 8.6`
- creep: `frf0.en = fault0.en_out`, `frf0.start = fault0.en_pid`, `frf0.pos_fb = fb_switch0.pos_fb`, `pid0.pos_ext_cmd = frf0.pos_cmd`, `pid0.vel_ext_cmd = frf0.vel_cmd`, `pid0.acc_ext_cmd = frf0.acc_cmd`
- `frf0.amp = 1`, `frf0.creep_vel = 3`, `frf0.creep_acc = 20`, `frf0.min_pos = -10`, `frf0.max_pos = 10`, `conf0.max_pos_error = 0`; `term0` shows `frf0.freq` and `frf0.inj`.

The template links `start` to `fault0.en_pid`, so enabling the drive starts the sweep once the position loop runs and disabling stops it (the result pins keep the sweep). Unlinked, start with `frf0.start = 1`; `frf0.start = 0` aborts a running sweep (with creep it first drives back to the start position), or clears the state after a sweep; the result pins keep the last sweep. The table, the fit and a `conf0.j_lpf` line print in the terminal.

## Component Explanation
The sweep and the creep run in `rt`, printing and the fit in `nrt`.

1. **Stepped sine**:
- A torque sine of `amp` is added to the torque command, `out = in + inj`, at `points` log-spaced frequencies from `f_start` to `f_end`.
- At each frequency it waits `settle` cycles, then correlates over whole periods (`cycles`), so the correlation rejects DC and the other harmonics. The signals are `u = out` (torque into the current loop, injection included), `c = in` (the controller's torque) and `vel = minus(pos, last pos) / period`.
- `vel` is the raw motor-side velocity: an observer such as `vel1` would mix its own model into the result. The half sample delay of the difference is taken out of the phase.
- Plant `G = vel / u` (rad/s per Nm). Open loop `L = -c / u` at the injection frequency: the loop gain seen at the torque command.
- `gj = abs(G) * w` is 1 / inertia: the total inertia below the coupling ring, the motor inertia above it, a dip at the anti-resonance and a peak at the resonance.
- Each point prints as it comes in: `f`, `abs(G)`, G phase, `gj`, `abs(L)`, L phase.

2. **Creep**:
- Friction sticks the motor at standstill and the low frequencies then read wrong, so the axis has to move. With `creep_vel > 0` `frf` drives it itself through `pid0`'s ext commands (as `ids` does): trapezoid moves back and forth at `creep_vel` and `creep_acc` between start + `min_pos` and start + `max_pos`.
- It measures only while it cruises; after each reversal the current point starts over. When the sweep is done it drives back to the start, then `state` goes to 2.
- While `en <= 0` the profile follows `pos_fb`, so enabling never jumps. Losing `en` during a sweep stops it and its injection where it is (`state` 3), with or without creep.
- The lowest point has to fit in one cruise: `(max_pos - min_pos) / creep_vel - creep_vel / creep_acc` must exceed its settle plus measure time (the template: about 3 s at `f_start` 5 Hz). Otherwise, or with `max_pos <= 0`, `min_pos >= 0` or the drive disabled at the start, it stops at once (`state` 3).
- `creep_vel` 3 rad/s keeps the load sliding through the sine near the anti-resonance, where it swings most (1 rad/s read the mode 10 % low in the simulation).
- `creep_vel` 0 leaves the motion to someone else (a LinuxCNC jog) and measures all the time.

3. **Crossover**: after the sweep `f_cross` is the last point where `abs(L)` falls below 1, interpolated in log (`abs(L)` can dip under 1 around the anti-resonance and come back), and `pm = 180 + L phase` there. If `abs(L)` is still above 1 at the end of the band there is none (0).

4. **Two-mass fit**:
- The complex inertia `J(jw) = 1 / (jw G)` of the points up to `fit_max` is fitted with `J = j_mot + j_load * (2 z wa s + wa^2) / (s^2 + 2 z wa s + wa^2) + b / s` (motor and load inertia, anti-resonance `wa`, its damping `z`, viscous `b`), after taking out a pure delay (current loop and feedback).
- The model is linear in `j_mot`, `j_load` and `b`, so each (`wa`, `z`, delay) on a grid is one small weighted least squares (weight `1 / abs(J)`, so every point counts in proportion); the best one with positive inertias wins. Grid: 160 anti-resonances log-spaced from 0.7 times the lowest fitted frequency to the highest, 16 dampings from 0.02 to 0.8, 8 delays from 0 to 1.5 ms. One grid row runs per `nrt` pass, so `nrt` stays responsive.
- It needs at least 6 points. The fit replaces picking the extremes of `gj`, which a second mode or noise moves. `f_anti` is the corner for `pid0.j_lpf` (what `ids`' ring down measured).
- Verdict printed: no coupling resonance in the band (`f_anti` above 95 % of the band or `j_load < 0.1 j_mot`) gives `conf0.j_lpf = 0`; `f_anti` at the low end asks for a lower `f_start`; `fit_err > 0.15` is a poor fit (more than one mode or noise); otherwise it prints `conf0.j_lpf = f_anti`.

5. **Results** (commit message, Y axis bench): crossover 45 Hz, phase margin 47 deg, anti-resonance about 14 Hz, peak 26 Hz.

{{% hint warning %}}
- `amp` defaults to 0 (no injection): set it (the template uses 1 Nm).
- Only one `frf` per drive: the fit arrays are shared (file statics), so a second instance stops at once (`state` 3) and prints that only the first frf instance can run.
{{% /hint %}}

**Config templates:** `filters`, `id_frf`

## Pins (33, 33 described)

| pin | description |
|---|---|
| `in` | *input*, Controller torque command (Nm), pid0.torque_cmd |
| `out` | *output*, Torque command with the injection (Nm), to the torque controller |
| `pos` | *input*, Motor-side position for the velocity (rad), fb_switch0.vel_fb |
| `inj` | *output*, Injected torque sine (Nm) |
| `start` | *input*, 1 = start the sweep, 0 = abort or clear |
| `amp` | *parameter*, Injection amplitude (Nm, default 0) |
| `f_start` | *parameter*, First frequency (Hz, default 5, min 0.5) |
| `f_end` | *parameter*, Last frequency (Hz, default 60) |
| `points` | *parameter*, Number of log-spaced frequencies (default 40, 1 to 48) |
| `settle` | *parameter*, Cycles before measuring (default 5, at least 0.05 s) |
| `cycles` | *parameter*, Cycles measured, whole periods (default 10, at least 0.1 s) |
| `freq` | *output*, Current frequency (Hz), 0 when idle |
| `state` | *output*, 0 idle, 1 running, 2 done, 3 stopped (en lost, stroke too short or not the first frf), start = 0 clears it |
| `en` | *input*, Drive enable, fault0.en_out: the profile follows pos_fb while disabled (default 1 when unlinked) |
| `pos_fb` | *input*, Position feedback (rad), fb_switch0.pos_fb |
| `pos_cmd` | *output*, Creep position command (rad), to pid0.pos_ext_cmd |
| `vel_cmd` | *output*, Creep velocity command (rad/s), to pid0.vel_ext_cmd |
| `acc_cmd` | *output*, Creep acceleration command (rad/s^2), to pid0.acc_ext_cmd |
| `creep_vel` | *parameter*, Creep velocity (rad/s, default 0 = no motion of its own) |
| `creep_acc` | *parameter*, Creep acceleration (rad/s^2, default 20, min 0.1) |
| `min_pos` | *parameter*, Lower end of the stroke about the start position (rad, default -10, must be below 0) |
| `max_pos` | *parameter*, Upper end of the stroke about the start position (rad, default 10, must be above 0) |
| `fit_max` | *parameter*, Fit the points up to this frequency (Hz, default 0 = all) |
| `f_anti` | *output*, Fitted anti-resonance (Hz), the pid0.j_lpf corner |
| `f_res` | *output*, Fitted resonance f_anti * sqrt(1 + j_load / j_mot) (Hz) |
| `zeta` | *output*, Fitted damping of the anti-resonance |
| `j_mot` | *output*, Fitted inertia above the ring (kgm^2) |
| `j_load` | *output*, Fitted inertia behind the coupling (kgm^2) |
| `b` | *output*, Fitted viscous friction (Nm s/rad) |
| `delay` | *output*, Fitted delay (s) |
| `fit_err` | *output*, RMS fit error, fraction of abs(J) |
| `f_cross` | *output*, Crossover frequency where abs(L) = 1 (Hz), 0 = none in the band |
| `pm` | *output*, Phase margin at f_cross (deg) |

*Generated by `tools/comp_docs.py` from the source; edit the source, not this page.*
