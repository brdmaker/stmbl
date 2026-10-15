# ARC Mate — dynamics model and collision detection: design

Two models from one code base (`shared/robot/`):

- **Plant** (simulator): the "true" arm, as realistic as practical,
  including what makes detection hard.
- **Detector** (LinuxCNC `.comp`): rigid and smooth, identified
  parameters. Never the plant's own parameters in tests, or the tests
  prove nothing.

| Effect | Plant | Detector |
|---|---|---|
| Links: mass, centre of mass, inertia tensor; Craig DH (= genserkins) | yes | identified base parameters |
| Gravity, coupling, Coriolis/centrifugal | yes | yes |
| Rotor inertia through the gears (armature, incl. wrist coupling) | yes | yes |
| Gear elasticity + backlash (two masses per joint) | yes | no; threshold window at reversals |
| Gear efficiency (direction dependent) | yes | in the friction terms |
| Friction: Coulomb, viscous, Stribeck, temperature, ripple | yes, stick-slip | smooth fit (`rb_friction`) |
| J2 balancer | if fitted | yes |
| Payload | yes | HAL pins |
| External force on any link | test input | estimated |
| Motor electrical side | `hv.c` per joint | measured current |
| Brakes, hard stops | yes | — |

## shared/robot

C99, double, no allocation, no globals: host simulator, LinuxCNC realtime
component and Python (ctypes) use the same code.

- `rb_rnea`: Craig's recursive Newton-Euler in the form linear in the
  parameters (mass, first moment m·c, inertia about the link origin),
  with external forces. Gravity alone and Coriolis alone are special
  cases.
- `rb_mass`: M(q) column by column from `rb_rnea` (6 calls; simple and
  consistent by construction, fast enough for 6 joints).
- `rb_fd`: forward dynamics, Cholesky.
- `rb_regressor`: tau = Y·pi, for identification (Phase 6).
- `rb_armature`: rotor inertias through a coupled transmission.
- `rb_observer_*`: generalised momentum observer (De Luca). Over one step
  the integral of Mdot·qd is (M_k − M_k−1)(qd_k + qd_k−1)/2 exactly enough,
  the rest by the trapezoid rule; no acceleration is needed.

Tests (`host/tests/robot/test_robot.c`, part of `make -C host check`):
against Robotics Toolbox for Python (kinematics, inverse dynamics, mass
matrix, gravity of a 6-joint arm with full inertias, to ~1e-12),
consistency (M symmetric positive definite, M·qdd + C·qd + g = rnea,
qd'(Mdot − 2C)qd = 0, regressor), physics (energy conserved in a free
swing, energy change = work, two-link arm against hand-derived
equations, external force = Jᵀf, rotor energy through a coupled
transmission), observer (residual < 0.05 Nm without a force on a
servo-held arm, = Jᵀf within 2% with one).

Observer note: a torque that changes much faster than the 1 kHz samples
(the servo's start-up jolt) shows as a transient residual (~0.5 Nm here,
gone after ~0.2 s). Thresholds need a jerk-dependent term or a hold-off
after enable.

## Next

1. Instantiable drive core; six drives in one process; the arm plant
   (two-mass joints, backlash, friction, brakes, stops) between them.
2. LinuxCNC (built from source, cached on an orphan branch) with
   genserkins driving the six simulated drives.
3. Collision `.comp` (observer + thresholds + reaction request), tests
   against the plant with perturbed parameters: false trips, smallest
   detected force per joint, detection time, stop-and-release.
