# TODO

Open work, grouped by area. Tick an item off (or delete it) when it lands, and
record any debug findings in `DEBUGNOTES.md` as usual.

## Arm (millweb arm page, `webui/mill/js/arm.js`)

- [ ] Configure page: a machine type "arm" — generator for 6 stmbl drives,
      armkins/genserkins, armguard, armcollide, armgrip, armtool, powerquota.
- [ ] Simulation IK keeps the tool orientation fixed: a program's A B C is
      ignored and MDI does nothing in the simulation.
- [ ] Teach pendant beyond "Record point" — the rest of the
      features in `docs/hmi/pendant.dc.html`.
- [ ] Hardware enabling (dead-man) switch for the pendant.
- [ ] TOOL-frame jog (only joint and world frames today).
- [ ] Real dock-stand parts in the cell model (docks are placeholders).
- [ ] Check the 3D frame rate on a Raspberry Pi and a tablet.
- [ ] Hide the mill tool 3D-model card on the arm Tools page.
- [ ] Safety: browser jog is not safety-rated. Needs a hardware enabling
      switch and safe-stop chain before any use near people; document it.

## Lathe (`linuxcnc/lathe`, Emco Compact 5)

- [ ] Material removal: compare against a finished-part model (pick the
      model, align its axis with Z).
- [ ] C axis and live tools.
- [ ] CTX TWIN: twin-lathe synchronisation.
- [ ] EBAZ4205 template (`machines/emco_compact5/machine-stmbl.json`): fill the
      placeholders — board name, stepgen/GPIO pins, X/Z screw leads.
- [ ] Run the stmbl spindle and the EBAZ4205 config in LinuxCNC (needs the
      hm2 board; untested so far).
- [ ] Measure turret offset, overtravel and lock back-off on the machine.

## Mill material removal (`webui/mill/js/mstock.js`)

- [ ] Undercuts and more than 3 axes (height map is 3-axis only).
- [ ] Spindle on/off state in the live preview.
- [ ] Model the spindle nose and the setup's rotation.
- [ ] Preview paths past 100k moves are merged; keep full detail.

## Other

- [ ] `host/arm` and `host/lcnc` keep duplicated source lists in their
      Makefiles; share one list.
- [ ] Velocity observer using the system inertia (task #57).
- [ ] Two known `@todo` host test failures:
  - `host/tests/motor_sew_cmp40m.test` — light rotor, slow L/R: reaches
    max_vel before the current settles; J 24 % high, friction half.
  - `host/tests/stepper_ipropi_worst_detent.test` — detent compensation
    stick-slips with worst-case IPROPI.
