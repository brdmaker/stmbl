# TODO

Open work, grouped by area. Tick an item off (or delete it) when it lands, and
record any debug findings in `DEBUGNOTES.md` as usual.

## Arm (millweb arm page, `webui/mill/js/arm.js`)

- [x] Configure page: machine type "arm" (`generate_arm`: armkins, armguard,
      armcollide, armgrip, armtool + M6, powerquota, the E-STOP chain;
      `machines/arcmate_sim`, run by `make test-gui-arm-machine`).
- [x] Simulation: a program's and MDI's A B C by IK; MDI moves, M6, gripper.
- [x] Teach pendant: Go home, the points list (Go, reorder, delete, clear,
      Load as program); the teach file is a program (M2).
- [x] Hardware enabling switch (`arm.enable`: jog-inhibit / jog-stop,
      optional feed-inhibit; the console shows it and stops its own jogs).
- [x] TOOL-frame jog (simulated; live as one G53 MDI move).
- [x] Hide the mill tool 3D-model card on the arm Tools page.
- [x] Safety note in the README (the browser jog is not safety-rated).
- [ ] Real drives on an arm: the generated config (gear scaling, enabling
      switch, changer pins, estop input) is only checked by unit tests; run it
      on hardware.
- [ ] TOOL-frame turns follow LinuxCNC's linear A B C between the ends, not
      an exact turn about the tool axis (fine for small steps; a held turn
      goes 45° in one move).
- [ ] A phone-sized pendant layout (`docs/hmi/pendant.dc.html`, 390 px).
- [ ] Real dock-stand parts in the cell model (docks are placeholders).
- [ ] Check the 3D frame rate on a Raspberry Pi and a tablet.

## Mikron HSM 400U retrofit (`host/mill/HSM400U.md`, `hwtest/hsm400u-sim`)

- [x] Rough simulation: five axes (A/C trunnion, xyzac-trt-kins), the
      Siemens servos on stmbl's bus, glass scales; LinuxCNC runs five-axis
      programs on it.
- [ ] Real data: motor nameplates (kE decides the rapids), the iTNC 520's
      machine parameters (back up before removing anything), scale and EnDat
      model numbers, masses, tap tests. Confirm A/C (not B/C) and the strokes.
- [ ] EnDat 2.1 / 2.2 on the stmbl drive: check `src/comps/endat.c` against
      the encoders' order code.
- [ ] Glass scales into LinuxCNC / the drive: 1 Vpp (interpolator) or
      absolute EnDat (LC).
- [ ] The spindle: is its drive a stand-alone VFD or a module of the
      Heidenhain inverter rack (then it needs its own)?
- [ ] Console: a trunnion machine type (Configure: xyzac-trt-kins and its
      pivots; the 3D view's tilting table; A/C in the DRO).
- [ ] Material removal for five axes (TODO below: the height map is 3-axis).

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
