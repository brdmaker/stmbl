# armgrip: electric finger gripper and payload check

A LinuxCNC realtime module (uspace) for a parallel gripper whose fingers
are moved by their own stmbl drive (one motor, a screw per finger; the
drive's position is the finger gap in mm), and for the payload it picks
up. Machine protection, not a safety function. The code is
`shared/robot/gripper.c`, the same `host/arm/arm_sim` runs in its tests
(`arm_grip_*.test`, soak `grip_*`); this file is the HAL wrapper.

```
loadrt armgrip model=/path/model.arm     # armcollide's model file
addf armgrip.0 servo-thread              # after armcollide.0
```

Wiring and G-code: `hwtest/arcmate-sim/grip.hal`; all pins are listed at
the top of `armgrip.c`.

- **Fingers.** Closing is a position move (`speed`, 40 mm/s) with the
  drive's current limit as the grip force. More than 1 mm of lag means
  contact: the gap is then the part's width (`width`), the command goes
  1.2 mm inside it and the state is GRIPPED (`width-expect` +-1 mm, or any
  width if 0), else WRONG; closed to 0 is EMPTY; a gripped part slipping
  out lets the fingers close on: LOST. The drive's config needs
  `max_sat = 0` (it holds at its current limit) and `max_pos_error` over
  the squeeze (stock 1.5 rad = 1.9 mm of gap at 4 mm/turn per finger).
- **Payload.** On GRIPPED, armcollide's model takes `mass-expect` at
  (`cx`, `cy`, `cz`) at once; detection is muted 0.3 s while the fingers
  squeeze. The lift that follows checks the mass from armcollide's
  residuals, fitted over the joints moving faster than 0.15 rad/s (at
  standstill static friction carries an unknown part of the weight).
  Off by more than 1 kg (or 15 %): mismatch. `mass-expect` 0: the mass is
  learnt on the lift instead, with detection muted until then. A release
  empties the model at once.
- **Faults** (latched until `reset`, `reason`): 6 payload mismatch, 7 part
  lost, 8 wrong width or nothing gripped (with a width expected).
  `chain-out` = `chain-in` and no fault, for the estop chain.

The check's accuracy is the dynamic model's: its residual floor on the
lift reads as mass. With the simulated ARC Mate it is +0.3 kg with the
exact model and +0.8 kg with identification-sized errors, the same for
any part mass. That sets the 1 kg tolerance; a smaller error stays under
the collision thresholds anyway.
